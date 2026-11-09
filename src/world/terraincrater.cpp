// Terrain craters made by weapons: spawning, the dug copies of sector pieces,
// seams and collision surfaces, and saving them with the game.
#include <algorithm>
#include <chrono>
#include <cmath>

#include "terraincrater.h"
#include "common/common.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "../system/inivals.h"

namespace
{

namespace Cr = World::Crater;

// Pieces of a 3x3 sector are 300 wide; a one-piece sector model covers 900.
// Both with a margin: some building models reach a little past their square.
constexpr double PIECE_HALF = 225.0;
constexpr double SINGLE_HALF = 525.0;
// A seam strip is 300 wide and as long as a sector; generous on purpose.
constexpr double SEAM_HALF = 750.0;

// Same slope limit the shape uses to decide what counts as standing ground.
constexpr double MIN_UP = 0.64;

// Pieces rebuilt in one frame: always at least one, then only while the frame's
// time lasts. A volley of bombs then spreads its work over a few frames
// instead of freezing one; the others show their old shape a little longer.
constexpr int REBUILD_MIN_PER_FRAME = 1;
constexpr double REBUILD_BUDGET_MS = 2.0;

bool RebuildAllowed(const World::Crater::State &state)
{
    return state.rebuiltThisFrame < REBUILD_MIN_PER_FRAME || state.rebuildMs < REBUILD_BUDGET_MS;
}

double MsSince(const std::chrono::steady_clock::time_point &start)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

int CraterLimit()
{
    const int value = System::IniConf::GameDeformationMaxCraters.Get<int32_t>();
    if ( value < 0 )
        return 128;
    return std::min(value, 2000);
}

uint32_t Mix(uint32_t seed, uint32_t value)
{
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

// Visual-only random in [0, 1): never consumes the gameplay random generator.
float Roll(uint32_t *seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return (float)(*seed & 0x00ffffffu) / 16777216.0f;
}

float Between(float minimum, float maximum, uint32_t *seed)
{
    if ( !(maximum > minimum) )
        return minimum;
    return minimum + (maximum - minimum) * Roll(seed);
}

// Does a crater reach the square (or rectangle) centred on a point?
bool Reaches(const Cr::Shape &shape, double centreX, double centreZ, double halfX, double halfZ)
{
    const double reach = Cr::Reach(shape);
    const double nearX = std::max(centreX - halfX, std::min(shape.x, centreX + halfX));
    const double nearZ = std::max(centreZ - halfZ, std::min(shape.z, centreZ + halfZ));
    const double dx = nearX - shape.x, dz = nearZ - shape.z;
    return dx * dx + dz * dz < reach * reach;
}

// Craters that can change something inside a square.
std::vector<Cr::Shape> Nearby(const std::vector<Cr::Shape> &craters, double centreX,
                              double centreZ, double halfX, double halfZ)
{
    std::vector<Cr::Shape> out;
    for (const Cr::Shape &shape : craters)
        if ( Reaches(shape, centreX, centreZ, halfX, halfZ) )
            out.push_back(shape);
    return out;
}

// Colour channels stay in 0..1 after shading.
float Unit(double value)
{
    return (float)std::max(0.0, std::min(1.0, value));
}

Cr::Point PointOf(const GFX::TVertex &v, const vec3d &origin)
{
    Cr::Point p;
    p.x = origin.x + v.Pos.x;
    p.y = origin.y + v.Pos.y;
    p.z = origin.z + v.Pos.z;
    p.u = v.TexCoord.tu;
    p.v = v.TexCoord.tv;
    p.color[0] = v.Color.r; p.color[1] = v.Color.g; p.color[2] = v.Color.b; p.color[3] = v.Color.a;
    p.computed[0] = v.ComputedColor.r; p.computed[1] = v.ComputedColor.g;
    p.computed[2] = v.ComputedColor.b; p.computed[3] = v.ComputedColor.a;
    return p;
}

// One mesh of a surface, cut and dug where craters reach it, dented where dents
// reach it. Triangles nothing reaches are copied untouched. Returns whether
// anything was reached.
bool DigMesh(const GFX::TMesh &source, const vec3d &origin,
             const std::vector<Cr::Shape> &craters, const Cr::DeformationLimits &limits, GFX::TMesh *out)
{
    bool changed = false;
    out->Mat = source.Mat;
    out->Vertexes.clear();
    out->Indixes.clear();

    Cr::PointIndex index;
    uint32_t texCoordId = 0;
    // Only the shapes reaching the triangle being added move its points: a
    // shape that moves a point reaches every triangle holding it.
    std::vector<Cr::Shape> local;

    const auto add = [&](const Cr::Point &p) -> uint32_t
    {
        GFX::TVertex vertex;
        const Cr::Sample sample = limits.Apply(Cr::SampleAt(local, p.x, p.z, p.y), p.x, p.z);
        const double shade = Cr::ShadeOf(sample);
        vertex.Pos = vec3f(p.x + sample.OffsetX() - origin.x, p.y + sample.OffsetY() - origin.y,
                           p.z + sample.OffsetZ() - origin.z);
        vertex.TexCoord = tUtV(p.u, p.v);
        vertex.Color = GFX::TGLColor(Unit(p.color[0] * shade), Unit(p.color[1] * shade),
                                     Unit(p.color[2] * shade), p.color[3]);
        vertex.ComputedColor = GFX::TGLColor(Unit(p.computed[0] * shade), Unit(p.computed[1] * shade),
                                             Unit(p.computed[2] * shade), p.computed[3]);
        vertex.TexCoordId = texCoordId;
        out->Vertexes.push_back(vertex);
        return (uint32_t)(out->Vertexes.size() - 1);
    };

    std::vector<Cr::Polygon> pieces;
    for (size_t i = 0; i + 2 < source.Indixes.size(); i += 3)
    {
        const GFX::TVertex &a = source.Vertexes[source.Indixes[i]];
        const GFX::TVertex &b = source.Vertexes[source.Indixes[i + 1]];
        const GFX::TVertex &c = source.Vertexes[source.Indixes[i + 2]];
        texCoordId = a.TexCoordId;

        const Cr::Polygon triangle = {PointOf(a, origin), PointOf(b, origin), PointOf(c, origin)};
        if ( Cr::Touching(craters, triangle, &local) )
        {
            double heightFrom = 1.0, heightTo = 0.0;
            Cr::DentHeights(local, triangle, &heightFrom, &heightTo);
            Cr::CutOnLattice(triangle, Cr::VISUAL_STEP, &pieces, heightFrom, heightTo);
            changed = true;
        }
        else
            pieces.assign(1, triangle);

        for (const Cr::Polygon &piece : pieces)
        {
            if ( piece.size() < 3 )
                continue;
            const uint32_t first = index.Find(piece[0], add);
            for (size_t k = 1; k + 1 < piece.size(); ++k)
            {
                out->Indixes.push_back((GFX::IndexType)first);
                out->Indixes.push_back((GFX::IndexType)index.Find(piece[k], add));
                out->Indixes.push_back((GFX::IndexType)index.Find(piece[k + 1], add));
            }
        }
    }

    out->RecalcBoundBox();
    return changed;
}

// Is the visible surface under a point sector ground (BODEN textures)? A crater
// is dug only there; a roof or a wall takes a dent instead. The ground counts
// as it is now, with the craters already in it.
bool VisibleGroundAt(const std::list<GFX::TMesh> &meshes, const vec3d &origin, const vec3d &point,
                     double tolerance, const std::vector<Cr::Shape> &craters,
                     const Cr::DeformationLimits &limits)
{
    const double x = point.x - origin.x;
    const double z = point.z - origin.z;
    for (const GFX::TMesh &mesh : meshes)
    {
        if ( !NC_STACK_base::IsSectorTerrainMesh(mesh) )
            continue;
        for (size_t i = 0; i + 2 < mesh.Indixes.size(); i += 3)
        {
            const vec3f &a = mesh.Vertexes[mesh.Indixes[i]].Pos;
            const vec3f &b = mesh.Vertexes[mesh.Indixes[i + 1]].Pos;
            const vec3f &c = mesh.Vertexes[mesh.Indixes[i + 2]].Pos;
            const double area = (b.x - a.x) * (c.z - a.z) - (b.z - a.z) * (c.x - a.x);
            if ( std::fabs(area) < 1e-6 )
                continue;
            const double w1 = ((b.x - x) * (c.z - z) - (b.z - z) * (c.x - x)) / area;
            const double w2 = ((c.x - x) * (a.z - z) - (c.z - z) * (a.x - x)) / area;
            const double w3 = 1.0 - w1 - w2;
            if ( w1 < -1e-4 || w2 < -1e-4 || w3 < -1e-4 )
                continue;
            const double height = origin.y + a.y * w1 + b.y * w2 + c.y * w3;
            const double now = height + limits.Apply(Cr::SampleAt(craters, point.x, point.z, height), point.x, point.z).OffsetY();
            if ( std::fabs(now - point.y) <= tolerance )
                return true;
        }
    }
    return false;
}

}

namespace World
{
namespace Crater
{

PieceMeshes::~PieceMeshes()
{
    for (GFX::TMesh &mesh : meshes)
        GFX::Engine.MeshFreeVBO(&mesh);
}

}
}

namespace
{

// "a|b|c..." with exactly `count` numbers.
bool SavedNumbers(const std::string &value, size_t count, std::vector<double> *numbers)
{
    numbers->clear();
    size_t start = 0;
    while ( start <= value.size() )
    {
        const size_t end = value.find('|', start);
        const std::string part = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        char *stop = nullptr;
        const double number = std::strtod(part.c_str(), &stop);
        if ( stop == part.c_str() )
            return false;
        numbers->push_back(number);
        if ( end == std::string::npos )
            break;
        start = end + 1;
    }
    return numbers->size() == count;
}

}

// A single world-space footprint rule prevents material boundaries and sector
// boundaries from giving a shared point two different displacements.
World::Crater::DeformationLimits NC_STACK_ypaworld::TerrainCraterLimits(const vec3d &position)
{
    Cr::DeformationLimits limits;
    const auto read = [](Common::Ini::Key &key) -> double
    {
        const std::string raw = key.Get<std::string>();
        try {
            size_t used = 0;
            const double value = std::stod(raw, &used);
            if (raw.find_first_not_of(" \t\r\n", used) != std::string::npos ||
                !std::isfinite(value) || value < 0.0 || value > 10.0)
                return -1.0;
            return value * 100.0;
        } catch (...) { return -1.0; }
    };
    limits.ground = read(System::IniConf::GameDeformationGroundLimit);
    limits.building = read(System::IniConf::GameDeformationBuildingLimit);
    if (limits.building < 0.0)
        return limits;
    const Common::Point centre = World::PositionToSectorID(position);
    for (int y = centre.y - 1; y <= centre.y + 1; ++y)
        for (int x = centre.x - 1; x <= centre.x + 1; ++x)
        {
            if (!IsGamePlaySector(Common::Point(x, y))) continue;
            const cellArea &cell = _cells(x, y);
            const bool single = cell.SectorType == 1;
            const int count = single ? 1 : 3;
            for (int zz = 0; zz < count; ++zz)
                for (int xx = 0; xx < count; ++xx)
                {
                    const int id = GetLegoBld(&cell, xx, zz);
                    if (id < 0 || (size_t)id >= _legoArray.size() || !_legoArray[id].Base) continue;
                    const vec3d origin = cell.CenterPos + (single ? vec3d() : vec3d((xx - 1) * 300.0, 0, (zz - 1) * 300.0));
                    Cr::BuildingFootprint box{1e30, 1e30, -1e30, -1e30};
                    for (const auto &mesh : _legoArray[id].Base->Meshes)
                    {
                        if (NC_STACK_base::IsSectorTerrainMesh(mesh)) continue;
                        for (const auto index : mesh.Indixes)
                        {
                            const auto &p = mesh.Vertexes[index].Pos;
                            box.minX = std::min(box.minX, origin.x + p.x);
                            box.maxX = std::max(box.maxX, origin.x + p.x);
                            box.minZ = std::min(box.minZ, origin.z + p.z);
                            box.maxZ = std::max(box.maxZ, origin.z + p.z);
                        }
                    }
                    if (box.minX <= box.maxX) limits.buildings.push_back(box);
                }
        }
    return limits;
}

// ------------------------------------------------------------- spawning ---

bool NC_STACK_ypaworld::SpawnTerrainCrater(const World::TCraterConfig &config,
                                           const ypaworld_arg136 &hit)
{
    const int limit = CraterLimit();
    if ( _isNetGame || limit <= 0 || !config.defined || !hit.isect || !hit.skel ||
         hit.polyID < 0 || (size_t)hit.polyID >= hit.skel->polygons.size() ||
         !std::isfinite(hit.isectPos.x) || !std::isfinite(hit.isectPos.y) ||
         !std::isfinite(hit.isectPos.z) || !IsGamePlaySector(hit.hitCell) )
        return false;

    // The polygon normal points into the surface that was hit (down for the
    // ground, into the building for a wall or a roof).
    vec3d normal = hit.skel->polygons[hit.polyID].Normal();
    if ( normal.normalise() <= 0.001 )
        return false;

    uint32_t seed = 0x43524154u;
    seed = Mix(seed, (uint32_t)_timeStamp);
    seed = Mix(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.x));
    seed = Mix(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.z));
    const float radius = Between(config.radius_min, config.radius_max, &seed);
    const float depth = Between(config.depth_min, config.depth_max, &seed);
    const float rim = Between(config.rim_min, config.rim_max, &seed) / 100.0f;

    if ( hit.hitCollisionType < 1 || hit.hitCollisionType > 4 )
        return false;

    // Standing ground (a slope up to about 50 degrees) takes a crater: seams
    // between sectors are ground by definition, inside a sector the visible
    // model must have ground there. Anything else (roof, wall, steep side)
    // takes a dent pushed into it.
    bool ground = normal.y >= MIN_UP;
    if ( ground && hit.hitCollisionType == 1 )
    {
        const cellArea &cell = _cells(hit.hitCell);
        int bldX = 0, bldY = 0;
        if ( cell.SectorType != 1 )
        {
            const int dxx = (int)std::lround(hit.hitSkelPos.x / 300.0);
            const int dzz = (int)std::lround(-hit.hitSkelPos.z / 300.0);
            bldX = dxx % 4 - 1;
            bldY = 2 - (dzz % 4 - 1);
        }
        const int legoId = GetLegoBld(&cell, bldX, bldY);
        if ( legoId < 0 || (size_t)legoId >= _legoArray.size() || !_legoArray[legoId].Base )
            return false;
        ground = VisibleGroundAt(_legoArray[legoId].Base->Meshes, hit.hitSkelPos, hit.isectPos,
                                 12.0, _terrainCraters.craters, TerrainCraterLimits(hit.hitSkelPos));
    }

    const World::Crater::Shape shape = ground
        ? World::Crater::Make(hit.isectPos.x, hit.isectPos.y, hit.isectPos.z, radius, depth, rim)
        : World::Crater::MakeDent(hit.isectPos.x, hit.isectPos.y, hit.isectPos.z, radius, depth,
                                  normal.x, normal.y, normal.z);
    if ( !World::Crater::Valid(shape) )
        return false;

    AddTerrainCrater(shape);
    return true;
}

void NC_STACK_ypaworld::AddTerrainCrater(const World::Crater::Shape &shape)
{
    const int limit = CraterLimit();
    if ( limit <= 0 || !World::Crater::Valid(shape) )
        return;

    // The oldest crater goes first; what it reached is rebuilt without it.
    while ( (int)_terrainCraters.craters.size() >= limit )
    {
        const World::Crater::Shape oldest = _terrainCraters.craters.front();
        _terrainCraters.craters.erase(_terrainCraters.craters.begin());
        RetireTerrainCraterCopies(oldest);
    }

    _terrainCraters.craters.push_back(shape);
    RetireTerrainCraterCopies(shape);
}

void NC_STACK_ypaworld::ClearTerrainCratersInCell(const Common::Point &cellId)
{
    const vec2d centre = World::SectorIDToCenterPos2(cellId);
    std::vector<World::Crater::Shape> &craters = _terrainCraters.craters;
    for (size_t i = 0; i < craters.size(); )
    {
        const World::Crater::Shape shape = craters[i];
        if ( std::fabs(shape.x - centre.x) <= 600.0 && std::fabs(shape.z - centre.y) <= 600.0 )
        {
            craters.erase(craters.begin() + i);
            RetireTerrainCraterCopies(shape);
        }
        else
            ++i;
    }
}

void NC_STACK_ypaworld::ClearTerrainCraters()
{
    // Seams keep the dug shape in their cache: mark them for a flat rebuild.
    for (const World::Crater::Shape &shape : _terrainCraters.craters)
        RetireTerrainCraterCopies(shape);

    _terrainCraters.craters.clear();
    _terrainCraters.pieces.clear();
    _terrainCraters.collision.clear();
    _terrainCraters.retiredPieces.clear();
    _terrainCraters.retiredCollision.clear();
}

bool NC_STACK_ypaworld::TerrainCraterRebuildAllowed() const
{
    return RebuildAllowed(_terrainCraters);
}

void NC_STACK_ypaworld::ReleaseRetiredTerrainCraters()
{
    _terrainCraters.retiredPieces.clear();
    _terrainCraters.retiredCollision.clear();
    _terrainCraters.rebuiltThisFrame = 0;
    _terrainCraters.rebuildMs = 0.0;
}

// Throws away every copy a crater can change, so the next use rebuilds it.
void NC_STACK_ypaworld::RetireTerrainCraterCopies(const World::Crater::Shape &shape)
{
    World::Crater::State &state = _terrainCraters;

    // Decals this crater reaches make their shader field again.
    for (TGroundDecal &decal : _groundDecals)
    {
        const double decalReach = 0.5 * std::sqrt(decal.sizeT * decal.sizeT + decal.sizeB * decal.sizeB);
        const double dx = decal.pos.x - shape.x, dz = decal.pos.z - shape.z;
        const double limit = decalReach + Cr::Reach(shape) + 150.0;
        if ( dx * dx + dz * dz < limit * limit )
            decal.fieldDirty = true;
    }

    for (auto it = state.pieces.begin(); it != state.pieces.end(); )
    {
        const int cellX = std::get<0>(it->first);
        const int cellY = std::get<1>(it->first);
        const int piece = std::get<2>(it->first);
        const vec2d centre = World::SectorIDToCenterPos2(Common::Point(cellX, cellY));
        double x = centre.x, z = centre.y, half = SINGLE_HALF;
        if ( piece < 9 )
        {
            x += (piece % 3 - 1) * 300.0;
            z += (piece / 3 - 1) * 300.0;
            half = PIECE_HALF;
        }
        // Kept and drawn until its new copy is made.
        if ( Reaches(shape, x, z, half, half) )
            it->second->stale = true;
        ++it;
    }

    for (auto it = state.collision.begin(); it != state.collision.end(); )
    {
        const double x = (double)std::get<0>(it->first);
        const double z = (double)std::get<1>(it->first);
        if ( Reaches(shape, x, z, SINGLE_HALF, SINGLE_HALF) )
        {
            state.retiredCollision.push_back(std::move(it->second));
            it = state.collision.erase(it);
        }
        else
            ++it;
    }

    // Seams are rebuilt from scratch by PrepareFiller when their cache is reset.
    const int minX = std::max(0, (int)std::floor((shape.x - Cr::Reach(shape)) / 1200.0) - 1);
    const int maxX = (int)std::floor((shape.x + Cr::Reach(shape)) / 1200.0) + 1;
    const int minY = std::max(0, (int)std::floor((-shape.z - Cr::Reach(shape)) / 1200.0) - 1);
    const int maxY = (int)std::floor((-shape.z + Cr::Reach(shape)) / 1200.0) + 1;
    for (Common::PlaneVector<TCellFillerCh> *caches : {&_cellsHFCache, &_cellsVFCache})
    {
        for (int y = minY; y <= maxY && y < (int)caches->Height(); ++y)
            for (int x = minX; x <= maxX && x < (int)caches->Width(); ++x)
                (*caches)(x, y).craterStale = true;
    }
}

// --------------------------------------------------------------- drawing ---

// The craters and dents a decal's shader needs to take its pixels back to
// where they were before the hits: the strongest ones reaching it, the
// deformation limits and the building footprints next to it, all from the
// decal origin. Same field as SampleAt + DeformationLimits::Apply.
// Where a point on the surface was before the craters and dents moved it:
// fixed point of the same field they dug with. *normal (optional) loses the
// crater slope too, so a decal on dug ground lies as on the ground it was.
vec3d NC_STACK_ypaworld::TerrainCraterRestore(const vec3d &pos, vec3d *normal)
{
    // Shapes that can have moved a point to here: the old place is at most a
    // few depths away (1.75 cap), well inside this margin.
    constexpr double MARGIN = 1000.0;
    std::vector<Cr::Shape> nearby;
    for (const Cr::Shape &shape : _terrainCraters.craters)
        if ( Cr::ShapeReachesBox(shape, pos.x - MARGIN, pos.y - MARGIN, pos.z - MARGIN,
                                 pos.x + MARGIN, pos.y + MARGIN, pos.z + MARGIN) )
            nearby.push_back(shape);
    if ( nearby.empty() )
        return pos;

    const Cr::DeformationLimits limits = TerrainCraterLimits(pos);
    vec3d old = pos;
    Cr::Sample sample;
    for (int i = 0; i < 4; ++i)
    {
        sample = limits.Apply(Cr::SampleAt(nearby, old.x, old.z, old.y), old.x, old.z);
        old = vec3d(pos.x - sample.OffsetX(), pos.y - sample.OffsetY(), pos.z - sample.OffsetZ());
    }

    // Ground facing up (World Y points down) dug by craters only: y = f(x, z),
    // normal (fx, -1, fz); take the crater slope out of fx and fz.
    if ( normal && normal->y < -0.3 && sample.moveX == 0.0 && sample.moveY == 0.0 && sample.moveZ == 0.0 )
    {
        const double fx = -normal->x / normal->y - sample.slopeX;
        const double fz = -normal->z / normal->y - sample.slopeZ;
        *normal = vec3d::Normalise(vec3d(fx, -1.0, fz));
    }
    return old;
}

void NC_STACK_ypaworld::TerrainCraterDecalField(const vec3d &centre, float reach, GFX::TDecalField *field)
{
    *field = GFX::TDecalField();
    if ( _terrainCraters.craters.empty() || !(reach > 0.0f) )
        return;

    // Shapes reaching the decal; the very same hit repeated (a burst on one
    // spot) is one shape with its number of copies, as they simply add up.
    std::vector<std::pair<const Cr::Shape *, int>> reaching;
    const auto same = [](const Cr::Shape &a, const Cr::Shape &b)
    {
        const double close = 0.01 * a.radius;
        return a.kind == b.kind && std::fabs(a.x - b.x) <= close && std::fabs(a.y - b.y) <= close &&
               std::fabs(a.z - b.z) <= close && std::fabs(a.radius - b.radius) <= close &&
               std::fabs(a.depth - b.depth) <= close && std::fabs(a.rim - b.rim) <= 0.01 &&
               a.nx * b.nx + a.ny * b.ny + a.nz * b.nz >= 0.9999;
    };
    for (const Cr::Shape &shape : _terrainCraters.craters)
    {
        if ( !Cr::ShapeReachesBox(shape, centre.x - reach, centre.y - reach, centre.z - reach,
                                  centre.x + reach, centre.y + reach, centre.z + reach) )
            continue;
        bool merged = false;
        for (auto &entry : reaching)
            if ( same(*entry.first, shape) )
            {
                entry.second++;
                merged = true;
                break;
            }
        if ( !merged )
            reaching.push_back(std::make_pair(&shape, 1));
    }
    if ( reaching.empty() )
        return;
    // Too many: the ones moving the surface most shape it (copies past the
    // 1.75 cap add nothing more).
    const auto weight = [](const std::pair<const Cr::Shape *, int> &entry)
    {
        return entry.first->depth * std::min(entry.second, 4);
    };
    std::stable_sort(reaching.begin(), reaching.end(),
                     [&](const std::pair<const Cr::Shape *, int> &a, const std::pair<const Cr::Shape *, int> &b)
                     { return weight(a) > weight(b); });
    if ( (int)reaching.size() > GFX::DECAL_FIELD_SHAPES )
        reaching.resize(GFX::DECAL_FIELD_SHAPES);

    for (const auto &entry : reaching)
    {
        const Cr::Shape *shape = entry.first;
        const int i = field->shapes++;
        const bool dent = shape->kind == Cr::Shape::DENT;
        field->pos[i][0] = (float)(shape->x - centre.x);
        field->pos[i][1] = (float)(shape->y - centre.y);
        field->pos[i][2] = (float)(shape->z - centre.z);
        field->pos[i][3] = dent ? -(float)entry.second : (float)entry.second;
        field->size[i][0] = (float)shape->radius;
        field->size[i][1] = (float)shape->depth;
        field->size[i][2] = (float)shape->rim;
        field->size[i][3] = (float)Cr::Band(*shape);
        field->dir[i][0] = (float)shape->nx;
        field->dir[i][1] = (float)shape->ny;
        field->dir[i][2] = (float)shape->nz;
        field->largest = std::max(field->largest, (float)(shape->depth * 1.75 * (1.0 + Cr::CRUMPLE)));
    }

    static_assert(GFX::DECAL_FIELD_STEP == Cr::VISUAL_STEP, "decal lattice must be the visual one");
    const auto phase = [](double value)
    {
        const double rest = std::fmod(value, Cr::VISUAL_STEP);
        return (float)(rest < 0.0 ? rest + Cr::VISUAL_STEP : rest);
    };
    field->lattice[0] = phase(centre.x);
    field->lattice[1] = phase(centre.z);

    const Cr::DeformationLimits limits = TerrainCraterLimits(centre);
    field->limits[0] = (float)limits.ground;
    field->limits[1] = (float)limits.building;
    // Footprints that change a cap where the decal is and a shape moves the
    // surface (150 = the blend), closest first.
    const auto boxDistance = [](const Cr::BuildingFootprint &box, double x, double z)
    {
        const double dx = std::max(0.0, std::max(box.minX - x, x - box.maxX));
        const double dz = std::max(0.0, std::max(box.minZ - z, z - box.maxZ));
        return std::sqrt(dx * dx + dz * dz);
    };
    std::vector<std::pair<double, const Cr::BuildingFootprint *>> boxes;
    for (const Cr::BuildingFootprint &box : limits.buildings)
    {
        const double dx = std::max(0.0, std::max(box.minX - centre.x - reach, centre.x - reach - box.maxX));
        const double dz = std::max(0.0, std::max(box.minZ - centre.z - reach, centre.z - reach - box.maxZ));
        if ( dx * dx + dz * dz >= 150.0 * 150.0 )
            continue;
        bool moved = false;
        for (const auto &entry : reaching)
            if ( boxDistance(box, entry.first->x, entry.first->z) < Cr::Reach(*entry.first) + 150.0 )
            {
                moved = true;
                break;
            }
        if ( moved )
            boxes.push_back(std::make_pair(boxDistance(box, centre.x, centre.z), &box));
    }
    std::stable_sort(boxes.begin(), boxes.end(),
                     [](const std::pair<double, const Cr::BuildingFootprint *> &a,
                        const std::pair<double, const Cr::BuildingFootprint *> &b) { return a.first < b.first; });
    for (const auto &entry : boxes)
    {
        if ( field->footprints >= GFX::DECAL_FIELD_FOOTPRINTS )
            break;
        const int i = field->footprints++;
        field->footprint[i][0] = (float)(entry.second->minX - centre.x);
        field->footprint[i][1] = (float)(entry.second->minZ - centre.z);
        field->footprint[i][2] = (float)(entry.second->maxX - centre.x);
        field->footprint[i][3] = (float)(entry.second->maxZ - centre.z);
    }
}

std::list<GFX::TMesh> *NC_STACK_ypaworld::TerrainCraterPieceMeshes(const cellArea &cell, int bldX,
                                                                  int bldY, int legoId,
                                                                  const vec3d &position)
{
    if ( _terrainCraters.craters.empty() || legoId < 0 || (size_t)legoId >= _legoArray.size() ||
         !_legoArray[legoId].Base )
        return nullptr;

    World::Crater::State &state = _terrainCraters;
    const bool single = cell.SectorType == 1;
    const World::Crater::PieceKey key(cell.CellId.x, cell.CellId.y, single ? 9 : bldX + bldY * 3);
    auto found = state.pieces.find(key);

    // The piece changed model (a building was destroyed or built): the old copy
    // belongs to another model and must not be drawn again.
    if ( found != state.pieces.end() && found->second->legoId != legoId )
    {
        state.retiredPieces.push_back(std::move(found->second));
        state.pieces.erase(found);
        found = state.pieces.end();
    }

    World::Crater::PieceMeshes *current = found != state.pieces.end() ? found->second.get() : nullptr;
    if ( current && !current->stale )
        return current->empty ? nullptr : &current->meshes;

    // Over this frame's budget: the old copy (or the flat model) one frame more.
    if ( !RebuildAllowed(state) )
        return current && !current->empty ? &current->meshes : nullptr;

    const auto start = std::chrono::steady_clock::now();
    std::unique_ptr<World::Crater::PieceMeshes> fresh(new World::Crater::PieceMeshes());
    fresh->legoId = legoId;

    const Cr::DeformationLimits limits = TerrainCraterLimits(position);
    const double half = single ? SINGLE_HALF : PIECE_HALF;
    const std::vector<Cr::Shape> nearby = Nearby(state.craters, position.x, position.z, half, half);
    if ( !nearby.empty() )
    {
        bool changed = false;
        for (const GFX::TMesh &source : _legoArray[legoId].Base->Meshes)
        {
            fresh->meshes.emplace_back();
            GFX::TMesh &mesh = fresh->meshes.back();
            // Animated surfaces keep their frames as they are.
            if ( source.Mat.Flags & GFX::RFLAGS_DYNAMIC_TEXTURE )
                mesh = source;
            else
                changed = DigMesh(source, position, nearby, limits, &mesh) || changed;
        }
        // Nothing reached after all: the shared model is drawn, no copy kept.
        if ( changed )
        {
            for (GFX::TMesh &mesh : fresh->meshes)
                GFX::Engine.MeshMakeVBO(&mesh);
            fresh->empty = false;
        }
        else
            fresh->meshes.clear();
    }

    state.rebuiltThisFrame++;
    state.rebuildMs += MsSince(start);

    std::unique_ptr<World::Crater::PieceMeshes> &slot = state.pieces[key];
    if ( slot )
        state.retiredPieces.push_back(std::move(slot));
    slot = std::move(fresh);
    return slot->empty ? nullptr : &slot->meshes;
}

void NC_STACK_ypaworld::ApplyTerrainCratersToFiller(TCellFillerCh *filler)
{
    if ( !filler || _terrainCraters.craters.empty() )
        return;

    const vec3d origin = filler->Transform.Pos;
    const Cr::DeformationLimits limits = TerrainCraterLimits(origin);
    const std::vector<Cr::Shape> nearby =
        Nearby(_terrainCraters.craters, origin.x, origin.z, SEAM_HALF, SEAM_HALF);
    if ( nearby.empty() )
        return;

    const auto start = std::chrono::steady_clock::now();
    for (GFX::TMesh &mesh : filler->Meshes)
    {
        if ( mesh.Mat.Flags & GFX::RFLAGS_DYNAMIC_TEXTURE )
            continue;
        GFX::TMesh dug;
        DigMesh(mesh, origin, nearby, limits, &dug);
        mesh.Vertexes = std::move(dug.Vertexes);
        mesh.Indixes = std::move(dug.Indixes);
        mesh.RecalcBoundBox();
    }
    _terrainCraters.rebuiltThisFrame++;
    _terrainCraters.rebuildMs += MsSince(start);
}

// ------------------------------------------------------------- collision ---

void NC_STACK_ypaworld::ApplyTerrainCraterCollision(TSectorCollision &location)
{
    if ( _terrainCraters.craters.empty() || !location.CollisionType || !location.sklt )
        return;

    UAskeleton::Data *source = location.sklt->GetSkelet();
    if ( !source )
        return;

    const World::Crater::CollisionKey key((int64_t)std::llround(location.pos.x),
                                          (int64_t)std::llround(location.pos.z),
                                          location.sklt, location.CollisionType);
    std::unique_ptr<World::Crater::CollisionCopy> &entry = _terrainCraters.collision[key];

    if ( !entry )
    {
        const Cr::DeformationLimits limits = TerrainCraterLimits(location.pos);
        entry.reset(new World::Crater::CollisionCopy());
        const std::vector<Cr::Shape> nearby =
            Nearby(_terrainCraters.craters, location.pos.x, location.pos.z, SINGLE_HALF, SINGLE_HALF);

        if ( !nearby.empty() )
        {
            UAskeleton::Data &out = entry->data;
            out.SEN = source->SEN;
            Cr::PointIndex index;
            bool changed = false;
            std::vector<Cr::Shape> local;   // Shapes reaching the polygon being added.

            const auto add = [&](const Cr::Point &p) -> uint32_t
            {
                UAskeleton::Vertex vertex;
                const Cr::Sample sample = limits.Apply(Cr::SampleAt(local, p.x, p.z, p.y), p.x, p.z);
                vertex.x = p.x + sample.OffsetX() - location.pos.x;
                vertex.y = p.y + sample.OffsetY() - location.pos.y;
                vertex.z = p.z + sample.OffsetZ() - location.pos.z;
                out.POO.push_back(vertex);
                return (uint32_t)(out.POO.size() - 1);
            };

            std::vector<Cr::Polygon> pieces;
            for (const UAskeleton::Polygon &polygon : source->polygons)
            {
                Cr::Polygon points;
                for (int i = 0; i < polygon.num_vertices; ++i)
                {
                    const UAskeleton::Vertex &v = source->POO[polygon.v[i]];
                    Cr::Point p;
                    p.x = location.pos.x + v.x;
                    p.y = location.pos.y + v.y;
                    p.z = location.pos.z + v.z;
                    points.push_back(p);
                }

                if ( !Cr::Touching(nearby, points, &local) )
                {
                    // Untouched: same polygon and plane, new point numbers.
                    UAskeleton::Polygon copy = polygon;
                    for (int i = 0; i < polygon.num_vertices; ++i)
                        copy.v[i] = (int32_t)index.Find(points[i], add);
                    out.polygons.push_back(copy);
                    continue;
                }

                changed = true;
                double heightFrom = 1.0, heightTo = 0.0;
                Cr::DentHeights(local, points, &heightFrom, &heightTo);
                Cr::CutOnLattice(points, Cr::COLLISION_STEP, &pieces, heightFrom, heightTo);
                for (const Cr::Polygon &piece : pieces)
                {
                    const uint32_t first = index.Find(piece[0], add);
                    for (size_t k = 1; k + 1 < piece.size(); ++k)
                    {
                        UAskeleton::Polygon triangle;
                        triangle.num_vertices = 3;
                        triangle.v[0] = (int32_t)first;
                        triangle.v[1] = (int32_t)index.Find(piece[k], add);
                        triangle.v[2] = (int32_t)index.Find(piece[k + 1], add);

                        // Same plane rule as the engine (sub_44DF60).
                        const vec3d &p0 = out.POO[triangle.v[0]];
                        const vec3d &p1 = out.POO[triangle.v[1]];
                        const vec3d &p2 = out.POO[triangle.v[2]];
                        vec3d normal = (p1 - p0) * (p2 - p1);
                        if ( normal.normalise() <= 1e-9 )
                            continue;
                        triangle.A = normal.x;
                        triangle.B = normal.y;
                        triangle.C = normal.z;
                        triangle.D = -normal.dot(p0);
                        out.polygons.push_back(triangle);
                    }
                }
            }

            out.tformedVertex.resize(out.POO.size());
            entry->empty = !changed;

            if ( changed )
            {
                std::vector<Cr::PolygonGrid::Box> boxes;
                boxes.reserve(out.polygons.size());
                for (const UAskeleton::Polygon &polygon : out.polygons)
                {
                    Cr::PolygonGrid::Box box = {1e30, 1e30, -1e30, -1e30};
                    for (int i = 0; i < polygon.num_vertices; ++i)
                    {
                        const UAskeleton::Vertex &v = out.POO[polygon.v[i]];
                        box.minX = std::min(box.minX, v.x); box.maxX = std::max(box.maxX, v.x);
                        box.minZ = std::min(box.minZ, v.z); box.maxZ = std::max(box.maxZ, v.z);
                    }
                    boxes.push_back(box);
                }
                entry->grid.Build(boxes, Cr::COLLISION_STEP * 2.0);
            }
        }
    }

    if ( !entry->empty )
    {
        location.sklt = &entry->skeleton;
        location.craterGrid = &entry->grid;
    }
}

// ---------------------------------------------------------------- saving ---

void NC_STACK_ypaworld::WriteTerrainCraters(FSMgr::FileHandle *file)
{
    if ( !file || _terrainCraters.craters.empty() )
        return;

    // Behind ;#! so the original game reads the save as if they were comments.
    file->printf("\n;#!begin_craters\n");
    for (const World::Crater::Shape &shape : _terrainCraters.craters)
    {
        if ( shape.kind == World::Crater::Shape::DENT )
            file->printf(";#!    dent = %.2f|%.2f|%.2f|%.2f|%.2f|%.4f|%.4f|%.4f\n", shape.x, shape.y,
                         shape.z, shape.radius, shape.depth, shape.nx, shape.ny, shape.nz);
        else
            file->printf(";#!    crater = %.2f|%.2f|%.2f|%.2f|%.2f|%.4f\n", shape.x, shape.y, shape.z,
                         shape.radius, shape.depth, shape.rim);
    }
    file->printf(";#!end\n");
}

bool NC_STACK_ypaworld::ReadTerrainCrater(const std::string &value)
{
    std::vector<double> numbers;
    if ( !SavedNumbers(value, 6, &numbers) )
        return false;

    const World::Crater::Shape shape = World::Crater::Make(numbers[0], numbers[1], numbers[2],
                                                          numbers[3], numbers[4], numbers[5]);
    if ( !World::Crater::Valid(shape) )
        return false;
    AddTerrainCrater(shape);
    return true;
}

bool NC_STACK_ypaworld::ReadTerrainDent(const std::string &value)
{
    std::vector<double> numbers;
    if ( !SavedNumbers(value, 8, &numbers) )
        return false;

    const World::Crater::Shape shape = World::Crater::MakeDent(numbers[0], numbers[1], numbers[2],
                                                              numbers[3], numbers[4], numbers[5],
                                                              numbers[6], numbers[7]);
    if ( !World::Crater::Valid(shape) )
        return false;
    AddTerrainCrater(shape);
    return true;
}
