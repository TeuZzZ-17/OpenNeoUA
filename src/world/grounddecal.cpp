// Ground decals (burn marks, craters) placed by weapons and wrecks.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <set>
#include <tuple>
#include <vector>

#include "grounddecal_contact.h"
#include "grounddecal_limit.h"
#include "common/env.h"
#include "common/includes.h"
#include "nucleus/loaders.h"
#include "nucleus/nucleas.h"
#include "game/yw_internal.h"
#include "../system/inivals.h"

namespace
{

constexpr int GROUND_DECAL_SHAPE_POINTS = 12;
constexpr float GROUND_DECAL_SHAPE_JAGGEDNESS = 0.35f;
constexpr float GROUND_DECAL_PI2 = 6.28318530717958647692f;
// Same limit of the shader: above this height a level surface is another surface.
constexpr double GROUND_DECAL_SAME_SURFACE_MAX = 40.0;
// The drawn image ends at about 60% of the decal radius: past that height the
// ground would receive nothing visible.
constexpr double GROUND_DECAL_GROUND_REACH = 0.6;

struct TGroundDecalShapePoint
{
    float u = 0.5f;
    float v = 0.5f;
};

static bool GroundDecalFinitePoint(const vec3d &point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

static bool GroundDecalWorldCell(NC_STACK_ypaworld *world, const Common::Point &cellId)
{
    return world->IsGamePlaySector(cellId);
}

// The hit must come from the map itself. Which surface it is does not matter:
// what really receives the decal is decided per pixel from what is drawn there.
static bool GroundDecalWorldLocation(NC_STACK_ypaworld *world,
                                     const Common::Point &cellId,
                                     int collisionType)
{
    if ( !GroundDecalWorldCell(world, cellId) )
        return false;

    if ( collisionType == 1 )
        return true;
    if ( collisionType == 2 )
        return GroundDecalWorldCell(world, cellId + Common::Point(-1, 0));
    if ( collisionType == 3 )
        return GroundDecalWorldCell(world, cellId + Common::Point(0, -1));
    if ( collisionType == 4 )
    {
        return GroundDecalWorldCell(world, cellId + Common::Point(-1, 0)) &&
               GroundDecalWorldCell(world, cellId + Common::Point(0, -1)) &&
               GroundDecalWorldCell(world, cellId + Common::Point(-1, -1));
    }

    return false;
}

// Normal of the hit polygon, turned toward the side the impact came from.
static bool GroundDecalHitNormal(const ypaworld_arg136 &hit, vec3d *normal)
{
    if ( !hit.skel || hit.polyID < 0 || (size_t)hit.polyID >= hit.skel->polygons.size() )
        return false;

    const UAskeleton::Polygon &polygon = hit.skel->polygons[hit.polyID];
    if ( polygon.num_vertices < 3 || (size_t)polygon.num_vertices > polygon.v.size() )
        return false;

    vec3d candidate = polygon.Normal();
    if ( !GroundDecalFinitePoint(candidate) || candidate.normalise() <= 0.001f ||
         !GroundDecalFinitePoint(candidate) )
        return false;

    // No direction known: keep the side facing up (world Y points down).
    const double incoming = candidate.dot(hit.vect);
    if ( incoming > 0.0 || (incoming == 0.0 && candidate.y > 0.0) )
        candidate = candidate * -1.0;

    *normal = candidate;
    return true;
}

// Decal plane axes. On level ground they are world X/Z, like the authored
// rotation and stretch always were.
static void GroundDecalAxes(const vec3d &normal, float angle, vec3d *axisT, vec3d *axisB)
{
    vec3d baseT = std::fabs(normal.x) < 0.9 ? vec3d(1.0, 0.0, 0.0) : vec3d(0.0, 0.0, 1.0);
    baseT = baseT - normal * baseT.dot(normal);
    baseT.normalise();
    const vec3d baseB = normal * baseT;

    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    *axisT = baseT * cosine + baseB * sine;
    *axisB = baseT * -sine + baseB * cosine;
}

static uint32_t GroundDecalMixSeed(uint32_t seed, uint32_t value)
{
    seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
}

static float GroundDecalRotation(const NC_STACK_ypaworld *world,
                                 const ypaworld_arg136 &hit,
                                 bool randomRotation)
{
    if ( !randomRotation )
        return 0.0f;

    uint32_t seed = 0x811c9dc5u;
    seed = GroundDecalMixSeed(seed, (uint32_t)world->_timeStamp);
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.x));
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.z));
    seed = GroundDecalMixSeed(seed, (uint32_t)hit.polyID);
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;

    return (float)(seed & 0x00ffffffu) * (GROUND_DECAL_PI2 / 16777216.0f);
}

static uint32_t GroundDecalNextRandom(uint32_t *seed)
{
    *seed ^= *seed << 13;
    *seed ^= *seed >> 17;
    *seed ^= *seed << 5;
    return *seed;
}

// Visual-only deterministic random. Separate salts keep each authored range
// independent without consuming the global gameplay RNG.
static float GroundDecalRandomUnit(const NC_STACK_ypaworld *world,
                                   const ypaworld_arg136 &hit,
                                   uint32_t salt)
{
    uint32_t seed = 0x9e3779b9u;
    seed = GroundDecalMixSeed(seed, salt);
    seed = GroundDecalMixSeed(seed, (uint32_t)world->_timeStamp);
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.x));
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.y));
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.z));
    seed = GroundDecalMixSeed(seed, (uint32_t)hit.polyID);
    return (float)(GroundDecalNextRandom(&seed) & 0x00ffffffu) / 16777216.0f;
}

static int GroundDecalResolvedIntegerRange(const NC_STACK_ypaworld *world,
                                            const ypaworld_arg136 &hit,
                                            int fallback,
                                            int minValue,
                                            int maxValue,
                                            int allowedMin,
                                            int allowedMax,
                                            uint32_t salt)
{
    if ( minValue < allowedMin || maxValue > allowedMax || maxValue < minValue )
        return fallback;

    if ( minValue == maxValue )
        return minValue;

    const float random = GroundDecalRandomUnit(world, hit, salt);
    const int span = maxValue - minValue + 1;
    return minValue + std::min((int)(random * (float)span), span - 1);
}

static float GroundDecalResolvedFloatRange(const NC_STACK_ypaworld *world,
                                            const ypaworld_arg136 &hit,
                                            float fallback,
                                            float minValue,
                                            float maxValue,
                                            uint32_t salt)
{
    if ( !std::isfinite(minValue) || !std::isfinite(maxValue) ||
         minValue <= 0.0f || maxValue <= 0.0f )
    {
        minValue = fallback;
        maxValue = fallback;
    }

    if ( !std::isfinite(minValue) || !std::isfinite(maxValue) ||
         minValue <= 0.0f || maxValue <= 0.0f )
        return 0.0f;

    if ( maxValue < minValue )
        std::swap(minValue, maxValue);
    if ( minValue == maxValue )
        return minValue;

    const float random = GroundDecalRandomUnit(world, hit, salt);
    return minValue + (maxValue - minValue) * random;
}

static float GroundDecalResolvedSize(const NC_STACK_ypaworld *world,
                                     const ypaworld_arg136 &hit,
                                     const World::TChainFXConfig &config)
{
    const float lo=config.ground_decal_size_min,hi=config.ground_decal_size_max;
    if(!std::isfinite(lo)||!std::isfinite(hi)||lo<0||hi<lo||hi>12000)return 0;
    return lo+(hi-lo)*GroundDecalRandomUnit(world,hit,0x53495a45u);
}

static float GroundDecalResolvedStretchX(const NC_STACK_ypaworld *world,
                                         const ypaworld_arg136 &hit,
                                         const World::TChainFXConfig &config)
{
    return GroundDecalResolvedFloatRange(world, hit,
                                         config.ground_decal_stretch_x,
                                         config.ground_decal_stretch_x_min,
                                         config.ground_decal_stretch_x_max,
                                         0x53545258u);
}

static float GroundDecalResolvedStretchZ(const NC_STACK_ypaworld *world,
                                         const ypaworld_arg136 &hit,
                                         const World::TChainFXConfig &config)
{
    return GroundDecalResolvedFloatRange(world, hit,
                                         config.ground_decal_stretch_z,
                                         config.ground_decal_stretch_z_min,
                                         config.ground_decal_stretch_z_max,
                                         0x5354525au);
}

static void GroundDecalBuildShape(int points,
                                  float jaggedness,
                                  uint32_t seed,
                                  std::vector<TGroundDecalShapePoint> *shape)
{
    shape->clear();
    shape->reserve((size_t)points);

    const float center = 0.5f;
    const float baseRadius = 0.42f;
    jaggedness = std::max(0.0f, std::min(jaggedness, 1.0f));

    std::vector<float> variations((size_t)points);
    for (float &variation : variations)
    {
        const float random = (float)(GroundDecalNextRandom(&seed) & 0x00ffffffu) /
                             16777216.0f;
        variation = random * 2.0f - 1.0f;
    }

    // Correlate neighbouring radii so jaggedness creates an irregular stain
    // instead of a saw-tooth star. Two passes retain broad variation while
    // removing isolated spikes.
    std::vector<float> smoothed(variations.size());
    for (int pass = 0; pass < 2; ++pass)
    {
        for (int i = 0; i < points; ++i)
        {
            const int previous = (i + points - 1) % points;
            const int next = (i + 1) % points;
            smoothed[(size_t)i] = variations[(size_t)previous] * 0.25f +
                                  variations[(size_t)i] * 0.50f +
                                  variations[(size_t)next] * 0.25f;
        }
        variations.swap(smoothed);
    }

    // Smoothing also shrinks the requested variation. Recenter and restore its
    // range so jaggedness remains visible as broad lobes, not sharp spikes.
    float variationAverage = 0.0f;
    for (float variation : variations)
        variationAverage += variation;
    variationAverage /= (float)points;

    float variationExtent = 0.0f;
    for (float variation : variations)
        variationExtent = std::max(variationExtent,
                                   std::fabs(variation - variationAverage));

    for (int i = 0; i < points; ++i)
    {
        const float angleRandom = (float)(GroundDecalNextRandom(&seed) & 0x00ffffffu) /
                                  16777216.0f;
        const float angleStep = GROUND_DECAL_PI2 / (float)points;
        const float angleJitter = (angleRandom * 2.0f - 1.0f) *
                                  angleStep * jaggedness * 0.20f;
        const float variation = variationExtent > 0.0001f
                              ? (variations[(size_t)i] - variationAverage) /
                                    variationExtent
                              : 0.0f;
        const float radius = std::max(0.20f,
                                      std::min(baseRadius *
                                                   (1.0f + variation *
                                                               jaggedness * 0.75f),
                                               0.47f));
        const float angle = angleStep * (float)i + angleJitter;

        TGroundDecalShapePoint point;
        point.u = center + std::cos(angle) * radius;
        point.v = center + std::sin(angle) * radius;
        shape->push_back(point);
    }
}

static uint32_t GroundDecalShapeSeed(const ypaworld_arg136 &hit, int points)
{
    uint32_t seed = 0x6d2b79f5u;
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.x));
    seed = GroundDecalMixSeed(seed, (uint32_t)(int32_t)std::lround(hit.isectPos.z));
    seed = GroundDecalMixSeed(seed, (uint32_t)hit.polyID);
    seed = GroundDecalMixSeed(seed, (uint32_t)points);
    return seed ? seed : 0x1u;
}

static float GroundDecalCross2D(float ax, float ay, float bx, float by)
{
    return ax * by - ay * bx;
}

static float GroundDecalShapeBoundaryRadius(
    const std::vector<TGroundDecalShapePoint> &shape,
    float u,
    float v)
{
    const float center = 0.5f;
    const float dx = u - center;
    const float dy = v - center;
    const float radius = std::sqrt(dx * dx + dy * dy);
    if ( radius <= 0.0001f )
        return 1.0f;

    float boundary = 1.0f;
    for (size_t i = 0; i < shape.size(); ++i)
    {
        const TGroundDecalShapePoint &a = shape[i];
        const TGroundDecalShapePoint &b = shape[(i + 1) % shape.size()];
        const float edgeX = b.u - a.u;
        const float edgeY = b.v - a.v;
        const float denominator = GroundDecalCross2D(dx, dy, edgeX, edgeY);
        if ( std::fabs(denominator) <= 0.000001f )
            continue;

        const float offsetX = a.u - center;
        const float offsetY = a.v - center;
        const float alongRay = GroundDecalCross2D(offsetX, offsetY, edgeX, edgeY) /
                               denominator;
        const float alongEdge = GroundDecalCross2D(offsetX, offsetY, dx, dy) /
                                denominator;
        if ( alongRay >= 0.0f && alongEdge >= -0.0001f && alongEdge <= 1.0001f )
            boundary = std::min(boundary, alongRay);
    }

    return std::max(boundary, radius);
}

// The jagged outline as equally spaced radii, the form the shader reads.
static void GroundDecalSampleShape(const std::vector<TGroundDecalShapePoint> &shape,
                                   std::array<float, GFX::DECAL_SHAPE_SAMPLES> *radii)
{
    const float step = GROUND_DECAL_PI2 / (float)GFX::DECAL_SHAPE_SAMPLES;
    for (int i = 0; i < GFX::DECAL_SHAPE_SAMPLES; ++i)
    {
        const float angle = step * (float)i;
        (*radii)[(size_t)i] = GroundDecalShapeBoundaryRadius(
            shape, 0.5f + std::cos(angle) * 0.05f, 0.5f + std::sin(angle) * 0.05f);
    }
}

static NC_STACK_bitmap *GroundDecalTexture(NC_STACK_ypaworld *world,
                                           const std::string &path)
{
    auto found = world->_groundDecalTextures.find(path);
    if ( found != world->_groundDecalTextures.end() )
        return found->second;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");
    NC_STACK_bitmap *texture = Utils::ProxyLoadImage({
        {NC_STACK_rsrc::RSRC_ATT_NAME, path},
        {NC_STACK_rsrc::RSRC_ATT_TRYSHARED, (int32_t)1},
        {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});
    Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( texture && texture->GetBitmap() )
        texture->PrepareTexture(false);
    else
        Common::DeleteAndNull(&texture);

    world->_groundDecalTextures[path] = texture;
    return texture;
}

}

bool NC_STACK_ypaworld::SpawnGroundDecalInSphere(const World::TChainFXConfig &config,
                                                const vec3d &center, float radius)
{
    if ( _isNetGame || World::GroundDecalLimit() <= 0 ||
         config.mode != World::TChainFXConfig::MODE_GROUND_DECAL ||
         config.trigger != World::TChainFXConfig::TRIGGER_DESTROYED )
        return false;
    ypaworld_arg136 hit;
    return FindWorldSurfaceInSphere(center, radius, hit) && SpawnGroundDecal(config, hit);
}

bool NC_STACK_ypaworld::FindWorldSurfaceInSphere(const vec3d &center, float radius,
                                                 ypaworld_arg136 &surfaceHit)
{
    if ( !GroundDecalFinitePoint(center) || !std::isfinite(radius) || radius <= 0.0f ||
         _mapSize.x <= 0 || _mapSize.y <= 0 )
        return false;

    // Visit the world polygons throughout the sphere's horizontal bounds. A
    // vertical ray alone would miss a wall touched by the side of the sphere.
    const auto microIndex = [](double coordinate, int maximum)
    {
        return (int)std::max(1.0, std::min((double)maximum, coordinate));
    };
    const int maxX = 4 * _mapSize.x - 2;
    const int maxZ = 4 * _mapSize.y - 2;
    const int minMicroX = microIndex(std::floor((center.x - radius + 150.0) / 300.0) - 1, maxX);
    const int maxMicroX = microIndex(std::floor((center.x + radius + 150.0) / 300.0) + 1, maxX);
    const int minMicroZ = microIndex(std::floor((-center.z - radius + 150.0) / 300.0) - 1, maxZ);
    const int maxMicroZ = microIndex(std::floor((-center.z + radius + 150.0) / 300.0) + 1, maxZ);
    const int centerMicroX = microIndex((center.x + 150.0) / 300.0, maxX);
    const int centerMicroZ = microIndex((-center.z + 150.0) / 300.0, maxZ);
    typedef std::tuple<int, int, int, int, const NC_STACK_skeleton *> TLocationKey;
    std::set<TLocationKey> visited;
    double bestSquared = (double)radius * radius;
    ypaworld_arg136 bestHit;
    TSectorCollision bestLocation;

    for (int microZ = minMicroZ; microZ <= maxMicroZ; ++microZ)
    {
        for (int microX = minMicroX; microX <= maxMicroX; ++microX)
        {
            TSectorCollision location = sub_44DBF8(centerMicroX, centerMicroZ, microX, microZ, 0);
            if ( !location.CollisionType || !location.sklt ||
                 !GroundDecalWorldLocation(this, location.Cell, location.CollisionType) )
                continue;

            TLocationKey key(location.CollisionType,
                             (int)std::lround(location.pos.x),
                             (int)std::lround(location.pos.y),
                             (int)std::lround(location.pos.z), location.sklt);
            if ( !visited.insert(key).second )
                continue;
            if ( location.CollisionType != 1 )
                sub_44E07C(location);
            UAskeleton::Data *skeleton = location.sklt ? location.sklt->GetSkelet() : nullptr;
            if ( !skeleton )
                continue;

            for (size_t polygonId = 0; polygonId < skeleton->polygons.size(); ++polygonId)
            {
                const UAskeleton::Polygon &polygon = skeleton->polygons[polygonId];
                if ( polygon.num_vertices < 3 ||
                     (size_t)polygon.num_vertices > polygon.v.size() )
                    continue;
                vec3d normal = polygon.Normal();
                if ( !GroundDecalFinitePoint(normal) || normal.normalise() <= 0.001f )
                    continue;

                std::vector<vec3d> points;
                for (int i = 0; i < polygon.num_vertices; ++i)
                {
                    const int vertexId = polygon.v[i];
                    if ( vertexId < 0 || (size_t)vertexId >= skeleton->POO.size() )
                        break;
                    const vec3d point = location.pos + skeleton->POO[vertexId];
                    if ( !GroundDecalFinitePoint(point) )
                        break;
                    points.push_back(point);
                }
                if ( points.size() != (size_t)polygon.num_vertices )
                    continue;

                ypaworld_arg136 hit;
                hit.hitCell = location.Cell;
                hit.hitCollisionType = location.CollisionType;

                for (size_t i = 1; i + 1 < points.size(); ++i)
                {
                    const vec3d contact = World::GroundDecalClosestTrianglePoint(
                        center, points[0], points[i], points[i + 1]);
                    const vec3d offset = contact - center;
                    const double squared = offset.dot(offset);
                    if ( squared > bestSquared ||
                         !World::GroundDecalSphereTouchesPoint(center, radius, contact) )
                        continue;
                    bestSquared = squared;
                    bestLocation = location;
                    bestHit = hit;
                    bestHit.isect = true;
                    bestHit.isectPos = contact;
                    bestHit.stPos = center;
                    bestHit.vect = offset;
                    bestHit.polyID = (int)polygonId;
                    bestHit.hitSkelPos = location.pos;
                }
            }
        }
    }

    if ( !bestHit.isect )
        return false;
    // Filler skeletons are shared scratch data. Restore the selected receiver
    // before the existing decal builder copies its central polygon.
    if ( bestLocation.CollisionType != 1 )
        sub_44E07C(bestLocation);
    bestHit.skel = bestLocation.sklt ? bestLocation.sklt->GetSkelet() : nullptr;
    surfaceHit = bestHit;
    return surfaceHit.skel != nullptr;
}


bool NC_STACK_ypaworld::SpawnGroundDecal(const World::TChainFXConfig &config,
                                         const ypaworld_arg136 &hit)
{
    const int limit = World::GroundDecalLimit();
    const bool supportedTrigger =
        config.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD ||
        config.trigger == World::TChainFXConfig::TRIGGER_DESTROYED;
    if ( _isNetGame || limit <= 0 ||
         config.mode != World::TChainFXConfig::MODE_GROUND_DECAL ||
         !supportedTrigger ||
         (!config.ground_decal_permanent && config.duration <= 0) ||
         config.ground_decal_tint.a <= 0.0f ||
         !hit.isect || !GroundDecalFinitePoint(hit.isectPos) )
        return false;

    vec3d normal;
    if ( !GroundDecalWorldLocation(this, hit.hitCell, hit.hitCollisionType) ||
         !GroundDecalHitNormal(hit, &normal) )
        return false;

    const int decalPoints = GROUND_DECAL_SHAPE_POINTS;
    const float decalJaggedness = GROUND_DECAL_SHAPE_JAGGEDNESS;

    const float decalSize = GroundDecalResolvedSize(this, hit, config);
    const float decalStretchX = GroundDecalResolvedStretchX(this, hit, config);
    const float decalStretchZ = GroundDecalResolvedStretchZ(this, hit, config);
    const float decalSizeX = decalSize * decalStretchX;
    const float decalSizeZ = decalSize * decalStretchZ;

    float edgeFade = std::isfinite(config.ground_decal_edge_fade)
                       ? std::max(0.0f, std::min(config.ground_decal_edge_fade, 10.0f))
                       : 0.0f;
    if ( config.ground_decal_edge_fade_min >= 0 &&
         config.ground_decal_edge_fade_max >= 0 )
    {
        edgeFade = (float)GroundDecalResolvedIntegerRange(
            this, hit, (int)std::lround(edgeFade),
            config.ground_decal_edge_fade_min,
            config.ground_decal_edge_fade_max,
            0, 10, 0x45444745u);
    }
    if ( !std::isfinite(decalSize) || decalSize <= 0.0f ||
         !std::isfinite(decalStretchX) || decalStretchX <= 0.0f ||
         !std::isfinite(decalStretchZ) || decalStretchZ <= 0.0f ||
         !std::isfinite(decalSizeX) || decalSizeX <= 0.0f ||
         !std::isfinite(decalSizeZ) || decalSizeZ <= 0.0f ||
         !std::isfinite(edgeFade) || edgeFade < 0.0f || edgeFade > 10.0f )
        return false;

    NC_STACK_bitmap *texture = nullptr;
    if ( !config.ground_decal_texture.empty() )
    {
        texture = GroundDecalTexture(this, config.ground_decal_texture);
        if ( !texture || !texture->GetBitmap() )
            return false;
    }

    std::vector<TGroundDecalShapePoint> shape;
    GroundDecalBuildShape(decalPoints, decalJaggedness,
                          GroundDecalShapeSeed(hit, decalPoints), &shape);

    const float angle = GroundDecalRotation(this, hit, config.ground_decal_random_rotation);
    const float decalRadius = 0.5f * std::sqrt(decalSizeX * decalSizeX +
                                               decalSizeZ * decalSizeZ);

    const auto addDecal = [&](const vec3d &position, const vec3d &surfaceNormal)
    {
        while ( _groundDecals.size() >= (size_t)limit )
            _groundDecals.pop_front();

        _groundDecals.emplace_back();
        TGroundDecal &decal = _groundDecals.back();
        decal.pos = position;
        decal.axisN = surfaceNormal;
        GroundDecalAxes(surfaceNormal, angle, &decal.axisT, &decal.axisB);
        decal.sizeT = decalSizeX;
        decal.sizeB = decalSizeZ;
        decal.edgeFade = edgeFade / 10.0f;
        GroundDecalSampleShape(shape, &decal.shape);
        decal.texture = texture ? texture->GetBitmap() : nullptr;
        decal.radius = decalRadius;
        decal.startTime = _timeStamp;
        decal.duration = config.duration;
        decal.permanent = config.ground_decal_permanent;
        if ( decal.permanent )
        {
            decal.fadeOut = 0;
            decal.fadeIn = std::max(config.fade_in, 0);
        }
        else
        {
            decal.fadeOut = std::min(std::max(config.fade_out, 0), decal.duration);
            decal.fadeIn = std::min(std::max(config.fade_in, 0), decal.duration - decal.fadeOut);
        }
        decal.tint = config.ground_decal_tint;
        decal.tint.Clamp();
    };

    // Craters and dents already there moved the surface: the decal is placed
    // where the hit point was before them, the same place its shader maps the
    // dug pixels at (decalshader.h).
    vec3d placed = hit.isectPos;
    TerrainCraterRestHit(hit, &placed, &normal);
    addDecal(placed, normal);

    // A decal follows the surfaces, so from a roof it would use up its size going
    // down the walls. When the ground is well within reach, a big impact also
    // marks it, with a second decal that climbs the same walls from below.
    const double aboveGround = _cells(hit.hitCell).height - placed.y;
    if ( normal.y < -0.7 && aboveGround > GROUND_DECAL_SAME_SURFACE_MAX &&
         aboveGround < decalRadius * GROUND_DECAL_GROUND_REACH )
        addDecal(vec3d(placed.x, _cells(hit.hitCell).height, placed.z), vec3d(0.0, -1.0, 0.0));

    // Optional smoke rising from the fresh decal (off unless the block has smoke = 1).
    // A retriggered effect stops with the decal, not at the end of one activation.
    int32_t decalEndTime = 0;
    if ( !config.ground_decal_permanent )
    {
        const long long end = (long long)_timeStamp + (long long)config.duration;
        decalEndTime = end > std::numeric_limits<int32_t>::max()
                           ? std::numeric_limits<int32_t>::max()
                           : (int32_t)end;
    }
    SpawnGroundDecalSmoke(config.ground_decal_smoke, hit.isectPos, decalRadius,
                          (uint32_t)(GroundDecalRandomUnit(this, hit, 0x534d4f4bu) * 16777216.0f),
                          decalEndTime);
    return true;
}

void NC_STACK_ypaworld::RenderGroundDecals(baseRender_msg *arg)
{
    const int limit = World::GroundDecalLimit();
    if ( limit <= 0 )
    {
        _groundDecals.clear();
        return;
    }

    while ( _groundDecals.size() > (size_t)limit )
        _groundDecals.pop_front();

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    // Same visible-world limit as the other effects: decals vanish into the fog at the horizon.
    const float drawLimit = GetEffectDrawDistance();
    GFX::Engine.SetDecalView(view->CalcSclRot, arg->flags,
                             drawLimit - (float)_normalFadeLength,
                             (float)_normalFadeLength);

    for (auto it = _groundDecals.begin(); it != _groundDecals.end(); )
    {
        const int32_t age = std::max(0, _timeStamp - it->startTime);
        if ( !it->permanent && age >= it->duration )
        {
            it = _groundDecals.erase(it);
            continue;
        }

        const vec3d relative = it->pos - view->CalcPos;
        const float distance = view->CalcSclRot.Transform(relative).length();

        if ( distance <= drawLimit + it->radius )
        {
            const float fade = it->permanent
                ? (it->fadeIn > 0
                   ? std::min(1.0f, (float)age / (float)it->fadeIn)
                   : 1.0f)
                : World::ComputeVPFadeEnvelope((double)age,
                                               (double)it->duration,
                                               (double)it->fadeIn,
                                               (double)it->fadeOut);

            if ( it->movedVersion != _terrainCraters.version )
            {
                it->moved = TerrainCraterMoveNear(it->pos, it->radius);
                it->movedVersion = _terrainCraters.version;
            }

            GFX::TDecalDraw draw;
            draw.Origin = vec3f(relative);
            draw.AxisT = vec3f(it->axisT);
            draw.AxisB = vec3f(it->axisB);
            draw.AxisN = vec3f(it->axisN);
            draw.SizeT = it->sizeT;
            draw.SizeB = it->sizeB;
            draw.EdgeFade = it->edgeFade;
            draw.Moved = it->moved;
            draw.Shape = it->shape.data();
            draw.Tex = it->texture;
            draw.Tint = GFX::TGLColor(it->tint.r, it->tint.g, it->tint.b, it->tint.a * fade);
            draw.Colorize = it->tint.ColorizesRGB();
            GFX::Engine.QueueRenderDecal(draw);
        }

        ++it;
    }
}

void NC_STACK_ypaworld::ClearGroundDecals()
{
    _groundDecals.clear();
    ClearGroundDecalSmoke();

    for (auto &entry : _groundDecalTextures)
    {
        if ( entry.second )
            entry.second->Delete();
    }
    _groundDecalTextures.clear();
}
