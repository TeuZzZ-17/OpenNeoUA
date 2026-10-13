// Attached FX volumes: model triangles, closed-mesh tests and random position sampling.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"

mat3x3 yw_BuildVPRotationMatrix(const vec3d &degrees)
{
    vec3d angle = degrees * C_PI_180;
    mat3x3 rot = mat3x3::Ident();

    if ( angle.x != 0.0 )
        rot *= mat3x3::RotateX(angle.x);
    if ( angle.y != 0.0 )
        rot *= mat3x3::RotateY(angle.y);
    if ( angle.z != 0.0 )
        rot *= mat3x3::RotateZ(angle.z);

    return rot;
}

vec3d yw_SafeVPScale(const vec3d &scale)
{
    return vec3d(scale.x > 0.001 ? scale.x : 1.0,
                 scale.y > 0.001 ? scale.y : 1.0,
                 scale.z > 0.001 ? scale.z : 1.0);
}

void yw_AddModelPoint(yw_ModelPointCloud *cloud, const vec3d &point)
{
    if ( !cloud->valid )
    {
        cloud->min = point;
        cloud->max = point;
        cloud->valid = true;
    }
    else
    {
        cloud->min.x = std::min(cloud->min.x, point.x);
        cloud->min.y = std::min(cloud->min.y, point.y);
        cloud->min.z = std::min(cloud->min.z, point.z);
        cloud->max.x = std::max(cloud->max.x, point.x);
        cloud->max.y = std::max(cloud->max.y, point.y);
        cloud->max.z = std::max(cloud->max.z, point.z);
    }

    cloud->points.push_back(point);
}

vec3d yw_Cross3(const vec3d &a, const vec3d &b)
{
    return vec3d(a.y * b.z - a.z * b.y,
                 a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x);
}

bool yw_IsFinitePoint(const vec3d &point)
{
    return isfinite(point.x) && isfinite(point.y) && isfinite(point.z);
}

void yw_AddAttachedFXTriangle(NC_STACK_ypaworld::TAttachedFXGeometryCache *cache,
                                     yw_ModelPointCloud *bounds,
                                     const vec3d &a, const vec3d &b, const vec3d &c)
{
    if ( !cache || !bounds || !yw_IsFinitePoint(a) || !yw_IsFinitePoint(b) || !yw_IsFinitePoint(c) )
        return;

    float area = (float)yw_Cross3(b - a, c - a).length() * 0.5f;
    if ( !isfinite(area) || area <= 0.0001f )
        return;

    cache->totalArea += area;
    NC_STACK_ypaworld::TAttachedFXTriangle triangle;
    triangle.a = a;
    triangle.b = b;
    triangle.c = c;
    triangle.cumulativeArea = cache->totalArea;
    cache->triangles.push_back(triangle);

    yw_AddModelPoint(bounds, a);
    yw_AddModelPoint(bounds, b);
    yw_AddModelPoint(bounds, c);
}

void yw_CollectAttachedFXTriangles(NC_STACK_base *base, const mat3x3 &rot, const vec3d &pos,
                                          NC_STACK_ypaworld::TAttachedFXGeometryCache *cache,
                                          yw_ModelPointCloud *bounds)
{
    if ( !base || !cache || !bounds )
        return;

    for (const GFX::TMesh &mesh : base->Meshes)
    {
        for (size_t index = 0; index + 2 < mesh.Indixes.size(); index += 3)
        {
            size_t ia = mesh.Indixes[index];
            size_t ib = mesh.Indixes[index + 1];
            size_t ic = mesh.Indixes[index + 2];
            if ( ia >= mesh.Vertexes.size() || ib >= mesh.Vertexes.size() || ic >= mesh.Vertexes.size() )
                continue;

            vec3d a = pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ia].Pos));
            vec3d b = pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ib].Pos));
            vec3d c = pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ic].Pos));
            yw_AddAttachedFXTriangle(cache, bounds, a, b, c);
        }
    }

    for (NC_STACK_base *kid : base->GetKidList())
    {
        TF::TForm3D &transform = kid->TForm();
        mat3x3 kidRot = rot * transform.SclRot;
        vec3d kidPos = pos + rot.Transform(transform.Pos);
        yw_CollectAttachedFXTriangles(kid, kidRot, kidPos, cache, bounds);
    }
}

int yw_GetWeldedVertexID(const vec3d &point, double weld,
                                std::map<std::tuple<int64_t, int64_t, int64_t>, int> *vertices)
{
    std::tuple<int64_t, int64_t, int64_t> key(
        (int64_t)llround(point.x / weld),
        (int64_t)llround(point.y / weld),
        (int64_t)llround(point.z / weld));

    auto found = vertices->find(key);
    if ( found != vertices->end() )
        return found->second;

    int id = (int)vertices->size();
    (*vertices)[key] = id;
    return id;
}

bool yw_IsClosedAttachedFXMesh(const NC_STACK_ypaworld::TAttachedFXGeometryCache &cache,
                                      const yw_ModelPointCloud &bounds)
{
    if ( !bounds.valid || cache.triangles.size() < 4 )
        return false;

    double weld = std::max(0.001, (double)(bounds.max - bounds.min).length() * 0.00001);
    std::map<std::tuple<int64_t, int64_t, int64_t>, int> vertices;
    std::map<std::pair<int, int>, int> edges;

    for (const NC_STACK_ypaworld::TAttachedFXTriangle &triangle : cache.triangles)
    {
        int ids[3] = {
            yw_GetWeldedVertexID(triangle.a, weld, &vertices),
            yw_GetWeldedVertexID(triangle.b, weld, &vertices),
            yw_GetWeldedVertexID(triangle.c, weld, &vertices)
        };

        for (int edge = 0; edge < 3; edge++)
        {
            int first = ids[edge];
            int second = ids[(edge + 1) % 3];
            if ( first == second )
                return false;
            if ( first > second )
                std::swap(first, second);
            edges[std::make_pair(first, second)]++;
        }
    }

    if ( edges.empty() )
        return false;

    for (const auto &edge : edges)
    {
        if ( edge.second != 2 )
            return false;
    }

    return true;
}

bool yw_RayIntersectsAttachedFXTriangle(const vec3d &origin, const vec3d &direction,
                                                const NC_STACK_ypaworld::TAttachedFXTriangle &triangle)
{
    const double epsilon = 0.000001;
    vec3d edge1 = triangle.b - triangle.a;
    vec3d edge2 = triangle.c - triangle.a;
    vec3d p = yw_Cross3(direction, edge2);
    double determinant = edge1.dot(p);
    if ( fabs(determinant) <= epsilon )
        return false;

    double inverse = 1.0 / determinant;
    vec3d t = origin - triangle.a;
    double u = t.dot(p) * inverse;
    if ( u < 0.0 || u > 1.0 )
        return false;

    vec3d q = yw_Cross3(t, edge1);
    double v = direction.dot(q) * inverse;
    if ( v < 0.0 || u + v > 1.0 )
        return false;

    return edge2.dot(q) * inverse > epsilon;
}

bool yw_IsPointInsideAttachedFXMesh(const vec3d &point,
                                           const NC_STACK_ypaworld::TAttachedFXGeometryCache &cache)
{
    const vec3d rayDirection(0.922766, 0.342094, 0.174731);
    int intersections = 0;
    for (const NC_STACK_ypaworld::TAttachedFXTriangle &triangle : cache.triangles)
    {
        if ( yw_RayIntersectsAttachedFXTriangle(point, rayDirection, triangle) )
            intersections++;
    }
    return (intersections & 1) != 0;
}

float yw_NextAttachedFXCacheRandom(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (float)((*state >> 8) & 0x00ffffffu) / 16777216.0f;
}

void yw_BuildAttachedFXVolumeCache(NC_STACK_ypaworld::TAttachedFXGeometryCache *cache,
                                          const yw_ModelPointCloud &bounds)
{
    if ( !cache || !yw_IsClosedAttachedFXMesh(*cache, bounds) )
        return;

    vec3d size = bounds.max - bounds.min;
    if ( size.x <= 0.001 || size.y <= 0.001 || size.z <= 0.001 )
        return;

    const size_t targetPoints = 2048;
    const int maxAttempts = 65536;
    uint32_t randomState = 0x9e3779b9u ^ (uint32_t)cache->triangles.size();
    cache->volumePoints.reserve(targetPoints);

    for (int attempt = 0; attempt < maxAttempts && cache->volumePoints.size() < targetPoints; attempt++)
    {
        vec3d point(bounds.min.x + size.x * yw_NextAttachedFXCacheRandom(&randomState),
                    bounds.min.y + size.y * yw_NextAttachedFXCacheRandom(&randomState),
                    bounds.min.z + size.z * yw_NextAttachedFXCacheRandom(&randomState));
        if ( yw_IsPointInsideAttachedFXMesh(point, *cache) )
            cache->volumePoints.push_back(point);
    }

    // Sparse or anomalous results are not a trustworthy occupied-volume cache.
    if ( cache->volumePoints.size() < 64 )
        cache->volumePoints.clear();
}

bool yw_SameAttachedFXTransform(const vec3d &a, const vec3d &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool NC_STACK_ypaworld::SampleAttachedFXLocalPosition(NC_STACK_ypabact *owner,
                                                       const World::TAbsoluteOrPercent &randomMaxOffset,
                                                       vec3d *localPosition)
{
    if ( !owner || !localPosition || !randomMaxOffset.defined )
        return false;

    if ( randomMaxOffset.value <= 0.0f )
    {
        *localPosition = vec3d(0.0, 0.0, 0.0);
        return true;
    }

    if ( !randomMaxOffset.percent )
    {
        // Absolute authoring uses a true radial maximum in world/model units.
        // Rejection sampling keeps the distribution inside the sphere instead
        // of allowing a cube corner to exceed the configured maximum distance.
        vec3d sample;
        for (int attempt = 0; attempt < 32; ++attempt)
        {
            sample.x = ((float)rand() / ((float)RAND_MAX + 1.0f)) * 2.0f - 1.0f;
            sample.y = ((float)rand() / ((float)RAND_MAX + 1.0f)) * 2.0f - 1.0f;
            sample.z = ((float)rand() / ((float)RAND_MAX + 1.0f)) * 2.0f - 1.0f;
            if ( sample.length() <= 1.0f )
            {
                *localPosition = sample * randomMaxOffset.value;
                return true;
            }
        }

        *localPosition = vec3d(0.0, 0.0, 0.0);
        return true;
    }

    float randomOffsetPercent = std::min(randomMaxOffset.value, 100.0f);

    NC_STACK_base *source = owner->_vp_normal;
    if ( !source ) source = owner->_vp_wait;
    if ( !source ) source = owner->_vp_fire;
    if ( !source ) source = owner->_vp_genesis;
    if ( !source )
        return false;

    TAttachedFXGeometryCache *cache = nullptr;
    for (TAttachedFXGeometryCache &candidate : _attachedFXGeometryCache)
    {
        if ( candidate.source == source &&
             yw_SameAttachedFXTransform(candidate.scale, owner->_vp_scale) &&
             yw_SameAttachedFXTransform(candidate.orientation, owner->_vp_rotation) )
        {
            cache = &candidate;
            break;
        }
    }

    if ( !cache )
    {
        _attachedFXGeometryCache.emplace_back();
        cache = &_attachedFXGeometryCache.back();
        cache->source = source;
        cache->scale = owner->_vp_scale;
        cache->orientation = owner->_vp_rotation;

        yw_ModelPointCloud bounds;
        mat3x3 transform = yw_BuildVPRotationMatrix(owner->_vp_rotation);
        transform *= mat3x3::Scale(yw_SafeVPScale(owner->_vp_scale));
        yw_CollectAttachedFXTriangles(source, transform, vec3d(0.0, 0.0, 0.0), cache, &bounds);
        if ( !cache->triangles.empty() )
            yw_BuildAttachedFXVolumeCache(cache, bounds);
    }

    if ( !cache->volumePoints.empty() )
    {
        size_t index = (size_t)(rand() % (int)cache->volumePoints.size());
        *localPosition = cache->volumePoints[index];
    }
    else
    {
        if ( cache->triangles.empty() || cache->totalArea <= 0.0f )
            return false;

        float selectedArea = ((float)rand() / ((float)RAND_MAX + 1.0f)) * cache->totalArea;
        const TAttachedFXTriangle *selected = &cache->triangles.back();
        for (const TAttachedFXTriangle &triangle : cache->triangles)
        {
            if ( selectedArea < triangle.cumulativeArea )
            {
                selected = &triangle;
                break;
            }
        }

        float r1 = (float)rand() / ((float)RAND_MAX + 1.0f);
        float r2 = (float)rand() / ((float)RAND_MAX + 1.0f);
        float root = sqrt(r1);
        *localPosition = selected->a * (1.0f - root) +
                         selected->b * (root * (1.0f - r2)) +
                         selected->c * (root * r2);
    }

    *localPosition = *localPosition * (randomOffsetPercent / 100.0f);

    return true;
}
