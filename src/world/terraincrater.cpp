// Terrain craters made by weapons: spawning, the dug copies of sector pieces,
// seams and collision surfaces, and saving them with the game.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

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
    if ( value == std::numeric_limits<int32_t>::max() )
        return value;
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

// Is the square (or rectangle) centred on a point within reach of a place?
bool Reaches(double x, double z, double reach, double centreX, double centreZ, double halfX, double halfZ)
{
    const double nearX = std::max(centreX - halfX, std::min(x, centreX + halfX));
    const double nearZ = std::max(centreZ - halfZ, std::min(z, centreZ + halfZ));
    const double dx = nearX - x, dz = nearZ - z;
    return dx * dx + dz * dz < reach * reach;
}

// Does a crater reach it?
bool Reaches(const Cr::Shape &shape, double centreX, double centreZ, double halfX, double halfZ)
{
    return Reaches(shape.x, shape.z, Cr::Reach(shape), centreX, centreZ, halfX, halfZ);
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
    out->Rest.clear();

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
        // Where the point was: the decals are mapped there (decalshader.h).
        out->Rest.push_back(vec3f(p.x - origin.x, p.y - origin.y, p.z - origin.z));
        return (uint32_t)(out->Vertexes.size() - 1);
    };

    std::vector<Cr::Polygon> pieces;
    Cr::HeightRanges heights;
    for (size_t i = 0; i + 2 < source.Indixes.size(); i += 3)
    {
        const GFX::TVertex &a = source.Vertexes[source.Indixes[i]];
        const GFX::TVertex &b = source.Vertexes[source.Indixes[i + 1]];
        const GFX::TVertex &c = source.Vertexes[source.Indixes[i + 2]];
        texCoordId = a.TexCoordId;

        const Cr::Polygon triangle = {PointOf(a, origin), PointOf(b, origin), PointOf(c, origin)};
        if ( Cr::Touching(craters, triangle, &local) )
        {
            Cr::WallHeights(local, triangle, &heights);
            Cr::CutOnLattice(triangle, Cr::VISUAL_STEP, &pieces, heights);
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

    // Nothing reached: an ordinary mesh, with no rest places to draw.
    if ( !changed )
        out->Rest.clear();
    out->RecalcBoundBox();
    return changed;
}

// Is the visible surface at a point sector ground (BODEN textures)? A crater
// is dug only there; a roof or a wall takes a dent instead. The point is where
// the hit was before any crater or dent (TerrainCraterRestHit), so the model
// is looked at as it was made.
bool VisibleGroundAt(const std::list<GFX::TMesh> &meshes, const vec3d &origin, const vec3d &point,
                     const vec3d &normal)
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
            if ( std::fabs(height - point.y) <= 12.0 )
            {
                // A wall can meet the ground at this point. Match the plane too.
                vec3d groundNormal = vec3d(b - a) * vec3d(c - a);
                if ( groundNormal.normalise() > 0.0 && std::fabs(groundNormal.dot(normal)) >= 0.7 )
                    return true;
            }
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

// game.deformation_*_limit in world units, or -1 when not authored (no limit).
static double AuthoredLimit(Common::Ini::Key &key)
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
}

// A single world-space footprint rule prevents material boundaries and sector
// boundaries from giving a shared point two different displacements.
World::Crater::DeformationLimits NC_STACK_ypaworld::TerrainCraterLimits(const vec3d &position, int ring)
{
    Cr::DeformationLimits limits;
    limits.ground = AuthoredLimit(System::IniConf::GameDeformationGroundLimit);
    limits.building = AuthoredLimit(System::IniConf::GameDeformationBuildingLimit);

    // XZ box of one lego piece: its buildings only, or everything on it.
    const auto pieceBox = [this](const cellArea &cell, int xx, int zz, bool withTerrain)
    {
        Cr::BuildingFootprint box{1e30, 1e30, -1e30, -1e30};
        const int id = GetLegoBld(&cell, xx, zz);
        if (id < 0 || (size_t)id >= _legoArray.size() || !_legoArray[id].Base) return box;
        const bool single = cell.SectorType == 1;
        const vec3d origin = cell.CenterPos + (single ? vec3d() : vec3d((xx - 1) * 300.0, 0, (zz - 1) * 300.0));
        for (const auto &mesh : _legoArray[id].Base->Meshes)
        {
            if (!withTerrain && NC_STACK_base::IsSectorTerrainMesh(mesh)) continue;
            for (const auto index : mesh.Indixes)
            {
                const auto &p = mesh.Vertexes[index].Pos;
                box.minX = std::min(box.minX, origin.x + p.x);
                box.maxX = std::max(box.maxX, origin.x + p.x);
                box.minZ = std::min(box.minZ, origin.z + p.z);
                box.maxZ = std::max(box.maxZ, origin.z + p.z);
            }
        }
        return box;
    };

    const Common::Point centre = World::PositionToSectorID(position);
    for (int y = centre.y - ring; y <= centre.y + ring; ++y)
        for (int x = centre.x - ring; x <= centre.x + ring; ++x)
        {
            if (x < 0 || y < 0 || x >= _mapSize.x || y >= _mapSize.y) continue;
            const cellArea &cell = _cells(x, y);
            const bool border = !IsGamePlaySector(Common::Point(x, y));
            if (!border && limits.building < 0.0) continue;
            const int count = cell.SectorType == 1 ? 1 : 3;
            for (int zz = 0; zz < count; ++zz)
                for (int xx = 0; xx < count; ++xx)
                {
                    const Cr::BuildingFootprint box = pieceBox(cell, xx, zz, border);
                    if (box.minX <= box.maxX)
                        (border ? limits.frozen : limits.buildings).push_back(box);
                }
        }
    return limits;
}

// ------------------------------------------------------------- spawning ---

static int BuildingSlot(const cellArea &cell, const Common::Point &cellId, const vec3d &position)
{
    if ( cell.SectorType == 1 )
        return 0;

    const vec3d centre = World::SectorIDToCenterPos3(cellId);
    const int x = 1 + (int)std::round(std::max(-1.0, std::min(1.0, (position.x - centre.x) / 300.0)));
    const int y = 1 + (int)std::round(std::max(-1.0, std::min(1.0, (position.z - centre.z) / 300.0)));
    return x + 3 * y;
}

World::BuildingPiece NC_STACK_ypaworld::ImpactBuildingPiece(const ypaworld_arg136 &hit,
                                                            const vec3d &restPos, const vec3d &normal,
                                                            bool *onGround)
{
    // A sector seam is terrain. In a LEGO piece, terrain and building can share
    // the same collision skeleton, so check the visible ground at the hit.
    if ( onGround )
        *onGround = true;
    if ( hit.hitCollisionType != 1 )
        return {};

    const cellArea &cell = _cells(hit.hitCell);
    const int slot = BuildingSlot(cell, hit.hitCell, hit.hitSkelPos);
    const int x = slot % 3, y = slot / 3;
    const bool alive = cell.buildings_health.Get(x, y) > 0;
    if ( !onGround && !alive )
        return {};
    const int legoId = GetLegoBld(&cell, x, y);
    const NC_STACK_base *base = legoId >= 0 && (size_t)legoId < _legoArray.size()
        ? _legoArray[legoId].Base : nullptr;
    const bool ground = base && VisibleGroundAt(base->Meshes, hit.hitSkelPos, restPos, normal);
    if ( onGround )
        *onGround = ground;
    if ( !ground && alive )
        return {hit.hitCell.x, hit.hitCell.y, slot};
    return {};
}

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
    // The slope is the one the surface had before any hit: the walls of a pit
    // dug by many bombs get steeper than that, but they are still ground. A
    // dent there would push the ground sideways into flaps, and off the square
    // its collision is looked up in.
    vec3d restPos = hit.isectPos, restNormal = normal;
    TerrainCraterRestHit(hit, &restPos, &restNormal);
    bool onGround;
    const World::BuildingPiece building = ImpactBuildingPiece(hit, restPos, restNormal, &onGround);
    const bool ground = onGround && restNormal.y >= MIN_UP;

    World::Crater::Shape shape = ground
        ? World::Crater::Make(hit.isectPos.x, hit.isectPos.y, hit.isectPos.z, radius, depth, rim)
        : World::Crater::MakeDent(hit.isectPos.x, hit.isectPos.y, hit.isectPos.z, radius, depth,
                                  normal.x, normal.y, normal.z);
    if ( !World::Crater::Valid(shape) )
        return false;

    shape.building = building;
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

void NC_STACK_ypaworld::ClearBuildingDents(const Common::Point &cellId, int slot)
{
    auto &craters = _terrainCraters.craters;
    const auto removed = std::remove_if(craters.begin(), craters.end(), [&](const Cr::Shape &shape)
    {
        if ( !shape.building.Matches(cellId.x, cellId.y, slot) )
            return false;
        RetireTerrainCraterCopies(shape);
        return true;
    });
    craters.erase(removed, craters.end());
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
    _terrainCraters.version++;
    RetireTerrainCraterCopiesNear(shape.x, shape.z, Cr::Reach(shape));
}

// Buildings limit how far the ground moves under and next to them
// (TerrainCraterLimits), so a copy also depends on the models standing near
// it. When a piece changes model (a building is damaged, destroyed or built),
// every copy its footprint can reach is made again: otherwise the piece made
// with the new footprint and its neighbours made with the old one would no
// longer meet, and the gap between them shows what is behind.
void NC_STACK_ypaworld::RefreshTerrainCraterFootprints()
{
    World::Crater::State &state = _terrainCraters;
    if ( state.craters.empty() || AuthoredLimit(System::IniConf::GameDeformationBuildingLimit) < 0.0 )
    {
        state.cellModels.clear();
        return;
    }

    // Only the sectors a crater can have a copy in.
    Common::Point low(_mapSize.x, _mapSize.y), high(-1, -1);
    for (const Cr::Shape &shape : state.craters)
    {
        const Common::Point id = World::PositionToSectorID(vec3d(shape.x, shape.y, shape.z));
        low.x = std::min(low.x, id.x - 2); low.y = std::min(low.y, id.y - 2);
        high.x = std::max(high.x, id.x + 2); high.y = std::max(high.y, id.y + 2);
    }
    low.x = std::max(low.x, 0); low.y = std::max(low.y, 0);
    high.x = std::min(high.x, _mapSize.x - 1); high.y = std::min(high.y, _mapSize.y - 1);

    const size_t count = (size_t)_mapSize.x * _mapSize.y;
    if ( state.cellModels.size() != count )
        state.cellModels.assign(count, 0);

    for (int y = low.y; y <= high.y; ++y)
        for (int x = low.x; x <= high.x; ++x)
        {
            const cellArea &cell = _cells(x, y);
            uint32_t models = Mix(0x4c45474fu, (uint32_t)cell.type_id);
            const int pieces = cell.SectorType == 1 ? 1 : 3;
            for (int zz = 0; zz < pieces; ++zz)
                for (int xx = 0; xx < pieces; ++xx)
                    models = Mix(models, (uint32_t)GetLegoBld(&cell, xx, zz));
            if ( models == 0 )
                models = 1;

            uint32_t &kept = state.cellModels[(size_t)y * _mapSize.x + x];
            // 0: not seen yet, its copies (if any) were made with these models.
            if ( kept != 0 && kept != models )
                // The whole sector square, a model reaching a little past it,
                // and the band where its footprint still changes the limit.
                RetireTerrainCraterCopiesNear(cell.CenterPos.x, cell.CenterPos.z,
                                              600.0 * 1.4143 + (SINGLE_HALF - 450.0) + Cr::FOOTPRINT_BLEND);
            kept = models;
        }
}

// Throws away every copy within reach of a place.
void NC_STACK_ypaworld::RetireTerrainCraterCopiesNear(double placeX, double placeZ, double reach)
{
    World::Crater::State &state = _terrainCraters;

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
        if ( Reaches(placeX, placeZ, reach, x, z, half, half) )
            it->second->stale = true;
        ++it;
    }

    for (auto it = state.collision.begin(); it != state.collision.end(); )
    {
        const double x = (double)std::get<0>(it->first);
        const double z = (double)std::get<1>(it->first);
        if ( Reaches(placeX, placeZ, reach, x, z, SINGLE_HALF, SINGLE_HALF) )
        {
            state.retiredCollision.push_back(std::move(it->second));
            it = state.collision.erase(it);
        }
        else
            ++it;
    }

    // Seams are rebuilt from scratch by PrepareFiller when their cache is reset.
    const int minX = std::max(0, (int)std::floor((placeX - reach) / 1200.0) - 1);
    const int maxX = (int)std::floor((placeX + reach) / 1200.0) + 1;
    const int minY = std::max(0, (int)std::floor((-placeZ - reach) / 1200.0) - 1);
    const int maxY = (int)std::floor((-placeZ + reach) / 1200.0) + 1;
    for (Common::PlaneVector<TCellFillerCh> *caches : {&_cellsHFCache, &_cellsVFCache})
    {
        for (int y = minY; y <= maxY && y < (int)caches->Height(); ++y)
            for (int x = minX; x <= maxX && x < (int)caches->Width(); ++x)
                (*caches)(x, y).craterStale = true;
    }
}

// --------------------------------------------------------------- drawing ---

// Where a hit was before the craters and dents moved the surface it landed
// on, and the normal the surface had there: a decal is placed there, where its
// shader maps the dug pixels (decalshader.h). The hit polygon of a dug
// collision copy carries the answer: the rest places of its points. False,
// changing nothing, when the hit is not on a dug polygon.
bool NC_STACK_ypaworld::TerrainCraterRestHit(const ypaworld_arg136 &hit, vec3d *pos, vec3d *normal)
{
    if ( !hit.skel || hit.polyID < 0 )
        return false;

    const World::Crater::CollisionCopy *copy = nullptr;
    for (const auto &entry : _terrainCraters.collision)
        if ( entry.second && &entry.second->data == hit.skel )
            copy = entry.second.get();
    // A copy thrown away in this frame may still be the one that was hit.
    for (const auto &entry : _terrainCraters.retiredCollision)
        if ( entry && &entry->data == hit.skel )
            copy = entry.get();
    if ( !copy || (size_t)hit.polyID >= copy->data.polygons.size() )
        return false;

    // Dug polygons are triangles; the others were copied as they were.
    const UAskeleton::Polygon &polygon = copy->data.polygons[hit.polyID];
    if ( polygon.num_vertices != 3 )
        return false;
    for (int i = 0; i < 3; ++i)
        if ( polygon.v[i] < 0 || (size_t)polygon.v[i] >= copy->rest.size() ||
             (size_t)polygon.v[i] >= copy->data.POO.size() )
            return false;

    const vec3d p0 = copy->data.POO[polygon.v[0]], p1 = copy->data.POO[polygon.v[1]];
    const vec3d p2 = copy->data.POO[polygon.v[2]];
    const vec3d &r0 = copy->rest[polygon.v[0]], &r1 = copy->rest[polygon.v[1]], &r2 = copy->rest[polygon.v[2]];

    // The hit inside the triangle as it is now, as weights of its points.
    const vec3d e1 = p1 - p0, e2 = p2 - p0, at = hit.isectPos - hit.hitSkelPos - p0;
    const double d11 = e1.dot(e1), d12 = e1.dot(e2), d22 = e2.dot(e2);
    const double den = d11 * d22 - d12 * d12;
    if ( !(std::fabs(den) > 1e-12) )
        return false;
    const double w1 = (d22 * at.dot(e1) - d12 * at.dot(e2)) / den;
    const double w2 = (d11 * at.dot(e2) - d12 * at.dot(e1)) / den;
    if ( w1 < -0.05 || w2 < -0.05 || w1 + w2 > 1.05 )
        return false;

    // Same winding as the plane of the dug triangle (ApplyTerrainCraterCollision):
    // the rest normal is on the side the given normal is on now.
    vec3d rest = (r1 - r0) * (r2 - r1);
    vec3d now = (p1 - p0) * (p2 - p1);
    if ( rest.normalise() <= 1e-9 || now.normalise() <= 1e-9 )
        return false;
    if ( now.dot(*normal) < 0.0 )
        rest = rest * -1.0;

    *pos = hit.hitSkelPos + r0 + (r1 - r0) * w1 + (r2 - r0) * w2;
    *normal = rest;
    return true;
}

// How far craters and dents can have moved a surface within reach of a point
// (0 = none there): the decal passes this to its screen rectangle.
float NC_STACK_ypaworld::TerrainCraterMoveNear(const vec3d &centre, float reach) const
{
    double largest = 0.0;
    for (const Cr::Shape &shape : _terrainCraters.craters)
        if ( Cr::ShapeReachesBox(shape, centre.x - reach, centre.y - reach, centre.z - reach,
                                 centre.x + reach, centre.y + reach, centre.z + reach) )
            // Overlap limit of SampleAt, with the dent folds.
            largest = std::max(largest, shape.depth * 1.75 * (1.0 + Cr::CRUMPLE));
    return (float)largest;
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
        mesh.Rest = std::move(dug.Rest);
        mesh.RecalcBoundBox();
    }
    _terrainCraters.rebuiltThisFrame++;
    _terrainCraters.rebuildMs += MsSince(start);
}

// ------------------------------------------------------------- collision ---

// The collision of the world is looked up by squares of 300 units: a ray only
// meets the surfaces of the piece standing on its square. A crater moves the
// ground straight down, so every square keeps its own ground. A dent pushes
// what it reaches sideways too: near one, the ground of a square can move on
// over the next square, and a ray coming down there would find nothing (units
// would fall through the world). For a square (as sub_44DBF8 numbers them)
// this tells how many squares around it a ray that found nothing must also
// try: 0 where no dent is near.
int NC_STACK_ypaworld::TerrainCraterSpill(int microX, int microZ)
{
    World::Crater::State &state = _terrainCraters;
    if ( state.craters.empty() )
        return 0;

    const int width = 4 * _mapSize.x, height = 4 * _mapSize.y;
    if ( state.spillVersion != state.version || state.spillWidth != width )
    {
        state.spillVersion = state.version;
        state.spillWidth = width;
        state.spill.assign((size_t)width * height, 0);
        for (const Cr::Shape &shape : state.craters)
        {
            if ( shape.kind != Cr::Shape::DENT )
                continue;
            // The furthest a dent moves a point (overlap limit of SampleAt, with the folds).
            const double move = shape.depth * 1.75 * (1.0 + Cr::CRUMPLE);
            const uint8_t ring = (uint8_t)std::max(1, std::min(2, (int)std::ceil(move / 300.0)));
            const double reach = shape.radius + move;
            const int x0 = std::max(0, (int)std::floor((shape.x - reach + 150.0) / 300.0));
            const int x1 = std::min(width - 1, (int)std::floor((shape.x + reach + 150.0) / 300.0));
            const int z0 = std::max(0, (int)std::floor((-shape.z - reach + 150.0) / 300.0));
            const int z1 = std::min(height - 1, (int)std::floor((-shape.z + reach + 150.0) / 300.0));
            for (int z = z0; z <= z1; ++z)
                for (int x = x0; x <= x1; ++x)
                {
                    uint8_t &square = state.spill[(size_t)z * width + x];
                    square = std::max(square, ring);
                }
        }
    }

    if ( microX < 0 || microZ < 0 || microX >= width || microZ >= height )
        return 0;
    return state.spill[(size_t)microZ * width + microX];
}

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
                entry->rest.push_back(vec3d(p.x - location.pos.x, p.y - location.pos.y, p.z - location.pos.z));
                return (uint32_t)(out.POO.size() - 1);
            };

            std::vector<Cr::Polygon> pieces;
            Cr::HeightRanges heights;
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
                Cr::WallHeights(local, points, &heights);
                Cr::CutOnLattice(points, Cr::COLLISION_STEP, &pieces, heights);
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
            // Older engines read only the first token, which still holds the whole dent.
            file->printf(";#!    dent = %.2f|%.2f|%.2f|%.2f|%.2f|%.4f|%.4f|%.4f |%d|%d|%d\n",
                         shape.x, shape.y, shape.z, shape.radius, shape.depth, shape.nx, shape.ny,
                         shape.nz, shape.building.cellX, shape.building.cellY, shape.building.slot);
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
    const bool hasOwner = SavedNumbers(value, 11, &numbers);
    if ( !hasOwner && !SavedNumbers(value, 8, &numbers) )
        return false;

    World::Crater::Shape shape = World::Crater::MakeDent(numbers[0], numbers[1], numbers[2],
                                                        numbers[3], numbers[4], numbers[5],
                                                        numbers[6], numbers[7]);
    if ( !World::Crater::Valid(shape) )
        return false;

    if ( hasOwner && (numbers[8] != -1 || numbers[9] != -1 || numbers[10] != -1) )
    {
        const double x = numbers[8], y = numbers[9], slot = numbers[10];
        if ( !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(slot) ||
             x != std::floor(x) || y != std::floor(y) || slot != std::floor(slot) ||
             x < 1 || x >= _mapSize.x - 1 || y < 1 || y >= _mapSize.y - 1 || slot < 0 || slot > 8 )
            return false;
        const cellArea &cell = _cells((int)x, (int)y);
        if ( cell.SectorType == 1 && slot != 0 )
            return false;
        if ( cell.buildings_health.Get((int)slot % 3, (int)slot / 3) <= 0 )
            return true; // The saved owner has already been destroyed.
        shape.building = {(int)x, (int)y, (int)slot};
    }
    else if ( !hasOwner && shape.x >= 0.0 && shape.x < _mapSize.x * World::CVSectorLength &&
              shape.z <= 0.0 && -shape.z < _mapSize.y * World::CVSectorLength )
    {
        // Old saves stored only the impact point. Use the piece under it
        // when the slot belongs to a destructible building.
        const vec3d pos(shape.x, shape.y, shape.z);
        const Common::Point id = World::PositionToSectorID(pos);
        if ( IsGamePlaySector(id) )
        {
            const cellArea &cell = _cells(id);
            const int slot = BuildingSlot(cell, id, pos);
            const int x = slot % 3, y = slot / 3;
            const TSubSectorDesc *piece = _secTypeArray[cell.type_id].SubSectors.Get(x, y);
            if ( cell.buildings_health.Get(x, y) > 0 )
                shape.building = {id.x, id.y, slot};
            else if ( piece && piece->StartHealth > 0 )
                return true;
        }
    }
    AddTerrainCrater(shape);
    return true;
}
