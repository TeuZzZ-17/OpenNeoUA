// Terrain craters: the shape of a crater and how a surface is cut to take it.
//
// The ground of Urban Assault is not a height map: a 300x300 sector piece is one
// flat polygon with four corners, so there is nothing to push down. A crater
// therefore cuts every surface it touches on one world lattice and then moves
// the new points. Because the lattice belongs to the world and not to a model,
// two pieces that share an edge get the very same points on it, so the crater
// can never open a crack between them.
//
// The height change is a function of the world position only. Every point of
// every model gets the same answer for the same place, which is what keeps
// ground, sector seams and the bottom of walls joined after the crater.
//
// A hit on a roof or a wall makes a dent instead: everything inside a sphere
// around the impact is pushed into the building, with folds, so it looks
// crumpled. It is also a function of the world position only, for the same
// reason; walls are then cut in height too, near the dent.
//
// No engine dependency: the tests build this file on its own.
#ifndef WORLD_TERRAINCRATER_CORE_H_INCLUDED
#define WORLD_TERRAINCRATER_CORE_H_INCLUDED

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "building_piece.h"

namespace World
{
namespace Crater
{

// Lattice steps: both divide the 300 unit sector piece exactly, so lattice lines
// fall on every piece border and seam.
constexpr double VISUAL_STEP = 300.0 / 16.0;
constexpr double COLLISION_STEP = 300.0 / 8.0;

// The rim can reach past the crater radius; nothing changes beyond this.
constexpr double REACH = 1.6;

// Deepest crater relative to its radius: keeps the walls at about 37 degrees, so
// ground vehicles can still drive in and out.
constexpr double MAX_DEPTH_RATIO = 0.5;

// How uneven a dent is: the push into the building varies by this much, and
// the surface also folds sideways by CRUMPLE_SIDE of the push.
constexpr double CRUMPLE = 0.6;
constexpr double CRUMPLE_SIDE = 0.3;
// Folds are about a third of the dent radius wide, rounded to a size of
// FOLD_SIZE times a power of two. Dents of similar size then share the very
// same fold pattern, worked out once per point.
constexpr double FOLD_SIZE = 45.0;

struct Shape
{
    enum Kind
    {
        GROUND = 0,     // Crater dug in the ground.
        DENT = 1        // Dent in a roof or wall.
    };

    int kind = GROUND;
    // World position of the impact. World Y points down.
    double x = 0.0, y = 0.0, z = 0.0;
    double radius = 0.0;
    double depth = 0.0;     // How far the centre goes down (or in, for a dent).
    double rim = 0.0;       // Raised border, as a fraction of the depth (craters only).
    double nx = 0.0, ny = 1.0, nz = 0.0;   // Dent: unit direction into the building.
    // Dent: two directions along the surface, square to nx/ny/nz (for the folds).
    double t1x = 1.0, t1y = 0.0, t1z = 0.0;
    double t2x = 0.0, t2y = 0.0, t2z = 1.0;
    BuildingPiece building;
};

// An authored crater made safe: no negative or non-finite value, depth limited.
inline Shape Make(double x, double y, double z, double radius, double depth, double rim)
{
    Shape shape;
    if ( !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
         !std::isfinite(radius) || !std::isfinite(depth) || !std::isfinite(rim) ||
         radius <= 0.0 || depth <= 0.0 )
        return shape;

    shape.x = x;
    shape.y = y;
    shape.z = z;
    shape.radius = radius;
    shape.depth = std::min(depth, radius * MAX_DEPTH_RATIO);
    shape.rim = std::max(0.0, std::min(rim, 1.0));
    return shape;
}

// A dent made safe the same way; the direction must be a real direction.
inline Shape MakeDent(double x, double y, double z, double radius, double depth,
                      double nx, double ny, double nz)
{
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    if ( !std::isfinite(length) || length < 1e-6 )
        return Shape();

    Shape shape = Make(x, y, z, radius, depth, 0.0);
    if ( shape.radius <= 0.0 )
        return shape;
    shape.kind = Shape::DENT;
    shape.nx = nx / length;
    shape.ny = ny / length;
    shape.nz = nz / length;

    double ax = 0.0, ay = 1.0, az = 0.0;
    if ( std::fabs(shape.ny) > 0.9 )
    {
        ax = 1.0;
        ay = 0.0;
    }
    shape.t1x = shape.ny * az - shape.nz * ay;
    shape.t1y = shape.nz * ax - shape.nx * az;
    shape.t1z = shape.nx * ay - shape.ny * ax;
    const double t1 = std::sqrt(shape.t1x * shape.t1x + shape.t1y * shape.t1y + shape.t1z * shape.t1z);
    shape.t1x /= t1; shape.t1y /= t1; shape.t1z /= t1;
    shape.t2x = shape.ny * shape.t1z - shape.nz * shape.t1y;
    shape.t2y = shape.nz * shape.t1x - shape.nx * shape.t1z;
    shape.t2z = shape.nx * shape.t1y - shape.ny * shape.t1x;
    return shape;
}

inline bool Valid(const Shape &shape)
{
    return shape.radius > 0.0 && shape.depth > 0.0;
}

inline double Reach(const Shape &shape)
{
    return shape.radius * (shape.kind == Shape::GROUND && shape.rim > 0.0 ? REACH : 1.0);
}

// How far above or below the impact a surface still belongs to the ground the
// crater is dug in. Higher up is a roof or another floor and stays as it is.
inline double Band(const Shape &shape)
{
    return shape.radius * 0.6 + shape.depth;
}

inline double Smooth(double edge0, double edge1, double value)
{
    double t = (value - edge0) / (edge1 - edge0);
    t = std::max(0.0, std::min(t, 1.0));
    return t * t * (3.0 - 2.0 * t);
}

// Slope of Smooth() along its value.
inline double SmoothSlope(double edge0, double edge1, double value)
{
    const double t = (value - edge0) / (edge1 - edge0);
    if ( t <= 0.0 || t >= 1.0 )
        return 0.0;
    return 6.0 * t * (1.0 - t) / (edge1 - edge0);
}

// Signed change of height for one crater, in world Y: positive goes down. Also
// gives its slope (change of the drop along x and z), worked out exactly, so
// the shading needs no extra samples.
inline double ShapeSample(const Shape &shape, double x, double z, double y,
                          double *slopeX, double *slopeZ)
{
    *slopeX = 0.0;
    *slopeZ = 0.0;
    if ( !Valid(shape) || shape.kind != Shape::GROUND )
        return 0.0;

    const double dx = x - shape.x;
    const double dz = z - shape.z;
    const double reach = Reach(shape);
    const double distanceSquared = dx * dx + dz * dz;
    if ( distanceSquared >= reach * reach )
        return 0.0;

    // Points far above or below the impact are not the ground it hit.
    const double band = Band(shape);
    const double vertical = std::fabs(y - shape.y);
    const double keep = 1.0 - Smooth(band, band * 1.25, vertical);
    if ( keep <= 0.0 )
        return 0.0;

    const double distance = std::sqrt(distanceSquared);
    const double d = distance / shape.radius;
    double drop = 0.0;
    double change = 0.0;    // Change of the drop along d.
    if ( d < 1.0 )
    {
        const double inside = 1.0 - d * d;
        drop = shape.depth * inside * inside;
        change = -4.0 * shape.depth * d * inside;
    }
    if ( shape.rim > 0.0 )
    {
        // A soft ridge on the edge, fading to nothing before the reach. Inside
        // the bowl it is far below a thousandth of a unit: skipped there.
        const double ridge = (d - 1.0) / 0.22;
        if ( ridge > -3.5 )
        {
            const double bump = shape.depth * shape.rim * std::exp(-ridge * ridge);
            const double fade = 1.0 - Smooth(1.3, REACH, d);
            drop -= bump * fade;
            change -= bump * (-2.0 * ridge / 0.22) * fade - bump * SmoothSlope(1.3, REACH, d);
        }
    }

    if ( distance > 1e-9 )
    {
        const double along = change * keep / (distance * shape.radius);
        *slopeX = along * dx;
        *slopeZ = along * dz;
    }
    return drop * keep;
}

inline double ShapeDrop(const Shape &shape, double x, double z, double y)
{
    double slopeX, slopeZ;
    return ShapeSample(shape, x, z, y, &slopeX, &slopeZ);
}

// Smooth noise in -1..1 that depends on the position only: the folds of a dent
// are the same on every model, every frame and after loading a saved game.
inline double Lattice(long x, long y, long z)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (double)(h & 0xffffffu) / 8388607.5 - 1.0;
}

inline double Noise(double x, double y, double z)
{
    const double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const long ix = (long)fx, iy = (long)fy, iz = (long)fz;
    const double tx = Smooth(0.0, 1.0, x - fx), ty = Smooth(0.0, 1.0, y - fy), tz = Smooth(0.0, 1.0, z - fz);
    const auto mix = [](double a, double b, double t) { return a + (b - a) * t; };
    const double x00 = mix(Lattice(ix, iy, iz), Lattice(ix + 1, iy, iz), tx);
    const double x10 = mix(Lattice(ix, iy + 1, iz), Lattice(ix + 1, iy + 1, iz), tx);
    const double x01 = mix(Lattice(ix, iy, iz + 1), Lattice(ix + 1, iy, iz + 1), tx);
    const double x11 = mix(Lattice(ix, iy + 1, iz + 1), Lattice(ix + 1, iy + 1, iz + 1), tx);
    return mix(mix(x00, x10, ty), mix(x01, x11, ty), tz);
}

// The folds at a point: three independent noises in -1..1.
struct Folds
{
    double n1 = 0.0, n2 = 0.0, n3 = 0.0;
};

// Fold size step of a dent: FOLD_SIZE * 2^step.
inline int FoldStep(const Shape &shape)
{
    const double wanted = std::max(VISUAL_STEP, shape.radius * 0.3) / FOLD_SIZE;
    return (int)std::lround(std::log2(wanted));
}

// The folds of each size used at one point, worked out when first needed.
class FoldCache
{
public:
    const Folds &At(double x, double y, double z, int step)
    {
        for (int i = 0; i < _count; ++i)
            if ( _step[i] == step )
                return _folds[i];
        const int slot = _count < 4 ? _count++ : 3;
        const double scale = 1.0 / (FOLD_SIZE * std::ldexp(1.0, step));
        _step[slot] = step;
        Folds &folds = _folds[slot];
        folds.n1 = Noise(x * scale, y * scale, z * scale);
        folds.n2 = Noise(x * scale + 31.7, y * scale + 11.3, z * scale + 5.9);
        folds.n3 = Noise(x * scale + 57.1, y * scale + 43.9, z * scale + 23.3);
        return folds;
    }

private:
    int _count = 0;
    int _step[4] = {0, 0, 0, 0};
    Folds _folds[4];
};

// How far one dent moves a point, as a world vector. *strength is 0..1: how
// much of the dent reaches the point (for the shading); *fold is its main fold.
inline bool DentMove(const Shape &shape, double x, double y, double z, FoldCache *cache,
                     double *moveX, double *moveY, double *moveZ, double *strength, double *fold)
{
    if ( !Valid(shape) || shape.kind != Shape::DENT )
        return false;
    const double dx = x - shape.x, dy = y - shape.y, dz = z - shape.z;
    const double distanceSquared = dx * dx + dy * dy + dz * dz;
    if ( distanceSquared >= shape.radius * shape.radius )
        return false;

    const double d = std::sqrt(distanceSquared) / shape.radius;
    const double inside = 1.0 - d * d;
    const double push = shape.depth * inside * inside;

    const Folds &folds = cache->At(x, y, z, FoldStep(shape));
    const double n1 = folds.n1, n2 = folds.n2, n3 = folds.n3;

    const double along = push * (1.0 + CRUMPLE * n1);
    const double side = push * CRUMPLE_SIDE;
    *moveX = shape.nx * along + (shape.t1x * n2 + shape.t2x * n3) * side;
    *moveY = shape.ny * along + (shape.t1y * n2 + shape.t2y * n3) * side;
    *moveZ = shape.nz * along + (shape.t1z * n2 + shape.t2z * n3) * side;
    *strength = inside * inside;
    *fold = n1;
    return true;
}

// What all craters and dents do at one point.
struct Sample
{
    double drop = 0.0;      // Craters: positive = lower ground.
    double slopeX = 0.0;
    double slopeZ = 0.0;
    double deepest = 0.0;   // Depth of the deepest crater reaching the point.

    // Dents: how far the point moves, and the light they leave on it.
    double moveX = 0.0, moveY = 0.0, moveZ = 0.0;
    double dentLight = 1.0;

    // Where the point ends up, from where it was.
    double OffsetX() const { return moveX; }
    double OffsetY() const { return drop + moveY; }
    double OffsetZ() const { return moveZ; }
};

// Overlapping craters add up, so repeated hits make a deeper hole, but never
// deeper than 1.75 times the deepest one (nor a rim above the highest rim).
inline Sample SampleAt(const std::vector<Shape> &craters, double x, double z, double y)
{
    Sample sample;
    double highestRim = 0.0;
    for (const Shape &shape : craters)
    {
        double slopeX, slopeZ;
        const double one = ShapeSample(shape, x, z, y, &slopeX, &slopeZ);
        if ( one == 0.0 && slopeX == 0.0 && slopeZ == 0.0 )
            continue;
        sample.drop += one;
        sample.slopeX += slopeX;
        sample.slopeZ += slopeZ;
        sample.deepest = std::max(sample.deepest, shape.depth);
        highestRim = std::max(highestRim, shape.depth * shape.rim);
    }

    // Where the limit holds, the ground is flat.
    const double lowest = sample.deepest * 1.75;
    if ( sample.drop > lowest || sample.drop < -highestRim )
    {
        sample.drop = std::max(-highestRim, std::min(sample.drop, lowest));
        sample.slopeX = sample.slopeZ = 0.0;
    }

    // Dents add up too, never more than 1.75 times the strongest one.
    double strongest = 0.0;
    double shadow = 0.0;
    FoldCache folds;
    for (const Shape &shape : craters)
    {
        double mx, my, mz, strength, fold;
        if ( !DentMove(shape, x, y, z, &folds, &mx, &my, &mz, &strength, &fold) )
            continue;
        sample.moveX += mx;
        sample.moveY += my;
        sample.moveZ += mz;
        strongest = std::max(strongest, shape.depth * (1.0 + CRUMPLE));
        // Darker inside the dent, with the folds catching or losing light.
        shadow += strength * (0.30 - 0.15 * fold);
    }
    const double moved = std::sqrt(sample.moveX * sample.moveX + sample.moveY * sample.moveY +
                                   sample.moveZ * sample.moveZ);
    if ( moved > strongest * 1.75 && moved > 0.0 )
    {
        const double keep = strongest * 1.75 / moved;
        sample.moveX *= keep;
        sample.moveY *= keep;
        sample.moveZ *= keep;
    }
    sample.dentLight = std::max(0.45, 1.0 - shadow);
    return sample;
}

// Shared by rendering and collision. Building footprints also constrain the
// ground at their base; a smooth 150-unit band joins it to open terrain.
struct BuildingFootprint { double minX, minZ, maxX, maxZ; };
constexpr double FOOTPRINT_BLEND = 150.0;
struct DeformationLimits
{
    double ground = -1.0, building = -1.0; // Negative means the old, unlimited path.
    std::vector<BuildingFootprint> buildings;
    // Map border pieces: never dug (their walls are scaled), so nothing moves
    // on them and the ground next to them fades back over the same band.
    std::vector<BuildingFootprint> frozen;

    // 1 on a box, 0 from FOOTPRINT_BLEND away, smooth in between.
    static double Influence(const std::vector<BuildingFootprint> &boxes, double x, double z)
    {
        double influence = 0.0;
        for (const auto &box : boxes)
        {
            const double dx = std::max(0.0, std::max(box.minX - x, x - box.maxX));
            const double dz = std::max(0.0, std::max(box.minZ - z, z - box.maxZ));
            const double t = std::min(1.0, std::sqrt(dx * dx + dz * dz) / FOOTPRINT_BLEND);
            influence = std::max(influence, 1.0 - t * t * (3.0 - 2.0 * t));
        }
        return influence;
    }

    double At(double x, double z, double uncapped) const
    {
        const double g = ground < 0.0 ? uncapped : ground;
        const double b = building < 0.0 ? uncapped : building;
        const double influence = Influence(buildings, x, z);
        // The building cap applies at the shared foundation.
        const double base = b;
        const double cap = influence == 0.0 ? g : (influence == 1.0 ? base : g + (base - g) * influence);
        return frozen.empty() ? cap : cap * (1.0 - Influence(frozen, x, z));
    }
    Sample Apply(Sample sample, double x, double z) const
    {
        if (ground >= 0.0) sample.drop = std::max(-ground, std::min(ground, sample.drop));
        const double ox = sample.OffsetX(), oy = sample.OffsetY(), oz = sample.OffsetZ();
        const double length = std::sqrt(ox * ox + oy * oy + oz * oz);
        const double cap = At(x, z, length);
        if (length > cap && length > 0.0)
        {
            const double ratio = cap / length;
            sample.drop *= ratio;
            sample.moveX *= ratio; sample.moveY *= ratio; sample.moveZ *= ratio;
            sample.slopeX *= ratio; sample.slopeZ *= ratio;
            sample.dentLight = 1.0 + (sample.dentLight - 1.0) * ratio;
        }
        return sample;
    }
};

inline double Drop(const std::vector<Shape> &craters, double x, double z, double y)
{
    return SampleAt(craters, x, z, y).drop;
}

// Light for the crater surface. The engine has no dynamic lights, so the slope
// is baked in the vertex colours: the side facing the light gets brighter, the
// other side and the bottom darker. 1 = unchanged.
inline double ShadeOf(const Sample &sample)
{
    if ( sample.drop == 0.0 && sample.slopeX == 0.0 && sample.slopeZ == 0.0 )
        return sample.dentLight;

    // Up is -Y. Surface normal pointing up, light from the north-west and above.
    double nx = sample.slopeX, ny = -1.0, nz = sample.slopeZ;
    const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
    nx /= length; ny /= length; nz /= length;
    const double lx = -0.45, ly = -0.80, lz = 0.40;
    const double lighting = nx * lx + ny * ly + nz * lz;    // 0.943 on flat ground

    // Darker the deeper the point, and normalised so flat ground stays at 1.
    const double depthShade = sample.deepest > 0.0
        ? 1.0 - 0.30 * std::max(0.0, sample.drop) / sample.deepest
        : 1.0;
    const double light = lighting / 0.943;
    return std::max(0.35, std::min(1.25, light * depthShade * sample.dentLight));
}

inline double Shade(const std::vector<Shape> &craters, double x, double z, double y)
{
    return ShadeOf(SampleAt(craters, x, z, y));
}

// --------------------------------------------------------------- cutting ---

// A point of a surface and what it carries: texture position and the two
// colours the renderer uses. Collision uses the position only.
struct Point
{
    double x = 0.0, y = 0.0, z = 0.0;
    float u = 0.0f, v = 0.0f;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float computed[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

inline Point Lerp(const Point &a, const Point &b, double t)
{
    Point out;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.u = (float)(a.u + (b.u - a.u) * t);
    out.v = (float)(a.v + (b.v - a.v) * t);
    for (int i = 0; i < 4; ++i)
    {
        out.color[i] = (float)(a.color[i] + (b.color[i] - a.color[i]) * t);
        out.computed[i] = (float)(a.computed[i] + (b.computed[i] - a.computed[i]) * t);
    }
    return out;
}

typedef std::vector<Point> Polygon;

// One side of a lattice line. Lower lines are kept (x >= line), upper lines are
// left out (x < line): a point on a line belongs to exactly one cell, so a wall
// standing exactly on a line is not cut into two copies of itself.
inline void ClipAxis(const Polygon &in, int axis, double line, bool keepAbove, Polygon *out)
{
    out->clear();
    if ( in.empty() )
        return;

    const auto coordinate = [axis](const Point &p) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; };
    const auto inside = [&](const Point &p)
    {
        return keepAbove ? coordinate(p) >= line : coordinate(p) < line;
    };

    Point previous = in.back();
    bool previousInside = inside(previous);
    for (const Point &current : in)
    {
        const bool currentInside = inside(current);
        if ( currentInside != previousInside )
        {
            const double a = coordinate(previous);
            const double b = coordinate(current);
            const double t = b != a ? (line - a) / (b - a) : 0.0;
            out->push_back(Lerp(previous, current, std::max(0.0, std::min(1.0, t))));
        }
        if ( currentInside )
            out->push_back(current);
        previous = current;
        previousInside = currentInside;
    }
}

// Twice the 3D area of a polygon (Newell): also right for walls, whose area
// seen from above is zero.
inline double Area(const Polygon &polygon)
{
    double nx = 0.0, ny = 0.0, nz = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        const Point &a = polygon[i];
        const Point &b = polygon[(i + 1) % polygon.size()];
        nx += (a.y - b.y) * (a.z + b.z);
        ny += (a.z - b.z) * (a.x + b.x);
        nz += (a.x - b.x) * (a.y + b.y);
    }
    return std::sqrt(nx * nx + ny * ny + nz * nz);
}

// A stretch of height where a shape bends what stands upright (see WallHeights).
struct HeightRange
{
    double from, to;
};
typedef std::vector<HeightRange> HeightRanges;

// Cut a convex polygon on the world lattice. Each piece is convex and keeps the
// winding of the original, so the triangles made from it face the same way.
// Lines across the height are added only in the given ranges: elsewhere a
// surface moves evenly along its height and needs none. Each range also gets
// the first line past each of its ends: two surfaces sharing an upright edge
// then have a common point where the move is even again, whatever other
// points each of them has further on (no gap between them).
inline void CutOnLattice(const Polygon &polygon, double step, std::vector<Polygon> *pieces,
                         const HeightRanges &ranges = HeightRanges())
{
    pieces->clear();
    if ( polygon.size() < 3 || !(step > 0.0) )
        return;

    // Height lines inside the polygon and inside the asked ranges.
    std::vector<double> heights;
    if ( !ranges.empty() )
    {
        double minY = polygon[0].y, maxY = minY;
        for (const Point &p : polygon)
        {
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        // Twice the step across the height: dents are wide and soft, and walls
        // would get too many pieces. Every surface uses the same lines.
        const double rise = step * 2.0;
        std::vector<long> rows;
        for (const HeightRange &range : ranges)
        {
            const long first = (long)std::floor(range.from / rise);
            const long last = (long)std::ceil(range.to / rise);
            for (long row = first; row <= last; ++row)
                if ( row * rise > minY && row * rise < maxY )
                    rows.push_back(row);
        }
        std::sort(rows.begin(), rows.end());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
        for (long row : rows)
            heights.push_back(row * rise);
    }

    double minX = polygon[0].x, maxX = minX, minZ = polygon[0].z, maxZ = minZ;
    for (const Point &p : polygon)
    {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }

    const long firstColumn = (long)std::floor(minX / step);
    const long lastColumn = (long)std::floor(maxX / step);
    const long firstRow = (long)std::floor(minZ / step);
    const long lastRow = (long)std::floor(maxZ / step);

    Polygon strip, temp, cell;
    for (long column = firstColumn; column <= lastColumn; ++column)
    {
        ClipAxis(polygon, 0, column * step, true, &temp);
        ClipAxis(temp, 0, (column + 1) * step, false, &strip);
        if ( strip.size() < 3 )
            continue;

        for (long row = firstRow; row <= lastRow; ++row)
        {
            ClipAxis(strip, 2, row * step, true, &temp);
            ClipAxis(temp, 2, (row + 1) * step, false, &cell);
            if ( cell.size() < 3 || !(Area(cell) > 1e-6) )
                continue;
            if ( heights.empty() )
            {
                pieces->push_back(cell);
                continue;
            }

            // Slice the cell across the height, bottom (smaller y) first.
            Polygon rest = cell, below;
            for (double line : heights)
            {
                ClipAxis(rest, 1, line, false, &below);
                if ( below.size() >= 3 && Area(below) > 1e-6 )
                    pieces->push_back(below);
                ClipAxis(rest, 1, line, true, &temp);
                rest.swap(temp);
                if ( rest.size() < 3 )
                    break;
            }
            if ( rest.size() >= 3 && Area(rest) > 1e-6 )
                pieces->push_back(rest);
        }
    }
}

// Whether a polygon can be changed by any crater: it reaches into a crater from
// the side, and it is not entirely above or below the ground the crater hit.
// Whether one crater or dent can change something inside a box. Exact for the
// points: a shape that moves a point reaches every polygon holding that point.
inline bool ShapeReachesBox(const Shape &shape, double minX, double minY, double minZ,
                            double maxX, double maxY, double maxZ)
{
    if ( shape.kind == Shape::DENT )
    {
        // The box meets the sphere of the dent.
        const double ex = std::max(minX, std::min(shape.x, maxX)) - shape.x;
        const double ey = std::max(minY, std::min(shape.y, maxY)) - shape.y;
        const double ez = std::max(minZ, std::min(shape.z, maxZ)) - shape.z;
        return ex * ex + ey * ey + ez * ez < shape.radius * shape.radius;
    }

    const double reach = Reach(shape);
    const double nearX = std::max(minX, std::min(shape.x, maxX));
    const double nearZ = std::max(minZ, std::min(shape.z, maxZ));
    const double dx = nearX - shape.x, dz = nearZ - shape.z;
    if ( dx * dx + dz * dz >= reach * reach )
        return false;

    const double band = Band(shape) * 1.25;
    return !(maxY < shape.y - band || minY > shape.y + band);
}

// The heights where the shapes touching a polygon need it cut, for
// CutOnLattice. False when there is none.
//  - A dent bends everything inside its sphere.
//  - A crater moves what stands on its ground by the same amount at every
//    height (no cut needed), except where its pull fades out above and below
//    (Band): there an upright surface bends.
inline bool WallHeights(const std::vector<Shape> &craters, const Polygon &polygon, HeightRanges *ranges)
{
    ranges->clear();
    if ( polygon.empty() )
        return false;
    double minX = polygon[0].x, maxX = minX, minY = polygon[0].y, maxY = minY;
    double minZ = polygon[0].z, maxZ = minZ;
    for (const Point &p : polygon)
    {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }

    for (const Shape &shape : craters)
    {
        if ( !ShapeReachesBox(shape, minX, minY, minZ, maxX, maxY, maxZ) )
            continue;
        if ( shape.kind == Shape::DENT )
        {
            ranges->push_back({shape.y - shape.radius, shape.y + shape.radius});
            continue;
        }
        const double band = Band(shape);
        if ( minY < shape.y - band )
            ranges->push_back({shape.y - band * 1.25, shape.y - band});
        if ( maxY > shape.y + band )
            ranges->push_back({shape.y + band, shape.y + band * 1.25});
    }
    return !ranges->empty();
}

// The craters and dents that reach a polygon, into *out. False if none.
inline bool Touching(const std::vector<Shape> &craters, const Polygon &polygon, std::vector<Shape> *out)
{
    out->clear();
    if ( polygon.empty() )
        return false;

    double minX = polygon[0].x, maxX = minX, minY = polygon[0].y, maxY = minY;
    double minZ = polygon[0].z, maxZ = minZ;
    for (const Point &p : polygon)
    {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }
    for (const Shape &shape : craters)
        if ( ShapeReachesBox(shape, minX, minY, minZ, maxX, maxY, maxZ) )
            out->push_back(shape);
    return !out->empty();
}

inline bool Touches(const std::vector<Shape> &craters, const Polygon &polygon)
{
    if ( polygon.empty() )
        return false;

    double minX = polygon[0].x, maxX = minX, minY = polygon[0].y, maxY = minY;
    double minZ = polygon[0].z, maxZ = minZ;
    for (const Point &p : polygon)
    {
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }

    for (const Shape &shape : craters)
        if ( ShapeReachesBox(shape, minX, minY, minZ, maxX, maxY, maxZ) )
            return true;
    return false;
}

// Points shared by the triangles of one surface: same place and same texture
// position means the same point.
class PointIndex
{
public:
    // Returns the index of an existing equal point, or of the one just added.
    template <typename AddFunction>
    uint32_t Find(const Point &p, AddFunction add)
    {
        const Key key = MakeKey(p);
        auto found = _index.find(key);
        if ( found != _index.end() )
            return found->second;
        const uint32_t id = add(p);
        _index.emplace(key, id);
        return id;
    }

private:
    struct Key
    {
        int64_t x, y, z;
        int32_t u, v;
        bool operator==(const Key &o) const
        {
            return x == o.x && y == o.y && z == o.z && u == o.u && v == o.v;
        }
    };
    struct KeyHash
    {
        size_t operator()(const Key &k) const
        {
            uint64_t h = (uint64_t)k.x * 73856093ULL;
            h ^= (uint64_t)k.y * 19349663ULL;
            h ^= (uint64_t)k.z * 83492791ULL;
            h ^= (uint64_t)(uint32_t)k.u * 2654435761ULL;
            h ^= (uint64_t)(uint32_t)k.v * 40503ULL;
            return (size_t)h;
        }
    };

    static Key MakeKey(const Point &p)
    {
        Key key;
        key.x = (int64_t)std::llround(p.x * 256.0);
        key.y = (int64_t)std::llround(p.y * 256.0);
        key.z = (int64_t)std::llround(p.z * 256.0);
        key.u = (int32_t)std::lround(p.u * 65536.0f);
        key.v = (int32_t)std::lround(p.v * 65536.0f);
        return key;
    }

    std::unordered_map<Key, uint32_t, KeyHash> _index;
};

// A dug collision surface has hundreds of small polygons where the original had
// one. The grid sorts them into squares (top view), so a short ray or a small
// sphere only tests the few polygons under it instead of all of them.
class PolygonGrid
{
public:
    // A polygon's top view box: min x, min z, max x, max z.
    struct Box
    {
        double minX, minZ, maxX, maxZ;
    };

    void Build(const std::vector<Box> &boxes, double cell)
    {
        _cell = cell;
        _start.clear();
        _items.clear();
        _stamp.assign(boxes.size(), 0);
        _tick = 0;
        _w = _h = 0;
        if ( boxes.empty() || !(cell > 0.0) )
            return;

        double minX = boxes[0].minX, minZ = boxes[0].minZ, maxX = boxes[0].maxX, maxZ = boxes[0].maxZ;
        for (const Box &b : boxes)
        {
            minX = std::min(minX, b.minX); minZ = std::min(minZ, b.minZ);
            maxX = std::max(maxX, b.maxX); maxZ = std::max(maxZ, b.maxZ);
        }
        _x0 = minX - EDGE;
        _z0 = minZ - EDGE;
        _w = std::max(1, (int)std::ceil((maxX + EDGE - _x0) / cell));
        _h = std::max(1, (int)std::ceil((maxZ + EDGE - _z0) / cell));

        // Two passes: count per square, then fill (one flat array, no lists).
        std::vector<uint32_t> count((size_t)_w * _h + 1, 0);
        for (int pass = 0; pass < 2; ++pass)
        {
            if ( pass == 1 )
            {
                _start.assign(count.size(), 0);
                for (size_t i = 1; i < count.size(); ++i)
                    _start[i] = _start[i - 1] + count[i - 1];
                _items.assign(_start.back(), 0);
                std::fill(count.begin(), count.end(), 0);
            }
            for (size_t id = 0; id < boxes.size(); ++id)
            {
                int x0, z0, x1, z1;
                Squares(boxes[id].minX, boxes[id].minZ, boxes[id].maxX, boxes[id].maxZ, &x0, &z0, &x1, &z1);
                for (int z = z0; z <= z1; ++z)
                    for (int x = x0; x <= x1; ++x)
                    {
                        const size_t square = (size_t)z * _w + x;
                        if ( pass == 1 )
                            _items[_start[square] + count[square]] = (uint32_t)id;
                        ++count[square];
                    }
            }
        }
    }

    // Calls visit(id) once for each polygon whose box meets the area. Returns
    // false, visiting nothing, when the area covers too many squares: the
    // caller then tests every polygon, as the game always did.
    template <typename Visit>
    bool Query(double minX, double minZ, double maxX, double maxZ, Visit visit) const
    {
        if ( _w <= 0 || !(minX <= maxX) || !(minZ <= maxZ) )
            return false;
        int x0, z0, x1, z1;
        Squares(minX, minZ, maxX, maxZ, &x0, &z0, &x1, &z1);
        if ( (x1 - x0 + 1) * (z1 - z0 + 1) > MAX_SQUARES )
            return false;

        if ( ++_tick == 0 )
        {
            std::fill(_stamp.begin(), _stamp.end(), 0);
            _tick = 1;
        }
        for (int z = z0; z <= z1; ++z)
            for (int x = x0; x <= x1; ++x)
            {
                const size_t square = (size_t)z * _w + x;
                for (uint32_t i = _start[square]; i < _start[square + 1]; ++i)
                {
                    const uint32_t id = _items[i];
                    if ( _stamp[id] != _tick )
                    {
                        _stamp[id] = _tick;
                        visit(id);
                    }
                }
            }
        return true;
    }

    bool Empty() const { return _w <= 0; }

private:
    static constexpr double EDGE = 0.01;   // Points on a box border count as inside.
    static constexpr int MAX_SQUARES = 256;

    void Squares(double minX, double minZ, double maxX, double maxZ,
                 int *x0, int *z0, int *x1, int *z1) const
    {
        const auto clampX = [this](double v) { return std::max(0, std::min(_w - 1, (int)std::floor(v))); };
        const auto clampZ = [this](double v) { return std::max(0, std::min(_h - 1, (int)std::floor(v))); };
        *x0 = clampX((minX - EDGE - _x0) / _cell);
        *x1 = clampX((maxX + EDGE - _x0) / _cell);
        *z0 = clampZ((minZ - EDGE - _z0) / _cell);
        *z1 = clampZ((maxZ + EDGE - _z0) / _cell);
    }

    double _cell = 0.0, _x0 = 0.0, _z0 = 0.0;
    int _w = 0, _h = 0;
    std::vector<uint32_t> _start;
    std::vector<uint32_t> _items;
    mutable std::vector<uint32_t> _stamp;
    mutable uint32_t _tick = 0;
};

}
}

#endif // WORLD_TERRAINCRATER_CORE_H_INCLUDED
