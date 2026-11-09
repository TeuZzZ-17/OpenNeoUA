// Attached FX silhouettes: voxel body from the collision profile or the visual model, plus random sampling.
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

bool yw_IsFinitePoint(const vec3d &point)
{
    return isfinite(point.x) && isfinite(point.y) && isfinite(point.z);
}

static void yw_PushAttachedFXTriangle(std::vector<yw_AttachedFXTriangle> *triangles,
                                      const vec3d &a, const vec3d &b, const vec3d &c)
{
    if ( yw_IsFinitePoint(a) && yw_IsFinitePoint(b) && yw_IsFinitePoint(c) )
        triangles->push_back(yw_AttachedFXTriangle{a, b, c});
}

// Visual model triangles (all kids), in actor-local coordinates.
void yw_CollectAttachedFXTriangles(NC_STACK_base *base, const mat3x3 &rot, const vec3d &pos,
                                   std::vector<yw_AttachedFXTriangle> *triangles)
{
    if ( !base || !triangles )
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

            yw_PushAttachedFXTriangle(triangles,
                pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ia].Pos)),
                pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ib].Pos)),
                pos + rot.Transform(static_cast<vec3d>(mesh.Vertexes[ic].Pos)));
        }
    }

    for (NC_STACK_base *kid : base->GetKidList())
    {
        TF::TForm3D &transform = kid->TForm();
        mat3x3 kidRot = rot * transform.SclRot;
        vec3d kidPos = pos + rot.Transform(transform.Pos);
        yw_CollectAttachedFXTriangles(kid, kidRot, kidPos, triangles);
    }
}

// Collision profile hull faces. Vertices are already actor-local, with the
// visual scale/rotation baked in, so no transform is applied here.
void yw_CollectAttachedFXShapeTriangles(const Collision::Shape &shape,
                                        std::vector<yw_AttachedFXTriangle> *triangles)
{
    if ( !triangles )
        return;

    for (const Collision::Part &part : shape.parts)
    {
        for (const std::array<int, 3> &face : part.faces)
        {
            if ( face[0] < 0 || face[1] < 0 || face[2] < 0 ||
                 (size_t)face[0] >= part.vertices.size() ||
                 (size_t)face[1] >= part.vertices.size() ||
                 (size_t)face[2] >= part.vertices.size() )
                continue;

            yw_PushAttachedFXTriangle(triangles, part.vertices[face[0]],
                                      part.vertices[face[1]], part.vertices[face[2]]);
        }
    }
}

// Builds the body silhouette as a voxel grid (longest side = 24 cells):
// 1. every cell touched by a triangle is a "surface" cell;
// 2. an empty cell is "inside" when surface cells surround it in at least 5 of
//    the 6 axis directions. This fills vanilla models too, which are often open
//    (no bottom, single-sided faces, small gaps), where a strict closed-mesh
//    test would fail;
// 3. optionally (visual models only) cells with few solid neighbours are
//    dropped: rotor blades, antennas, flat fins and other one-cell-thin parts.
//    If that would leave too little of the body, the full set is kept.
// The result is a list of cell centres plus their mean (the body centre).
void yw_BuildAttachedFXSilhouette(const std::vector<yw_AttachedFXTriangle> &triangles,
                                  bool dropThinParts,
                                  NC_STACK_ypaworld::TAttachedFXGeometryCache *cache)
{
    if ( !cache )
        return;

    cache->cells.clear();
    cache->centre = vec3d(0.0, 0.0, 0.0);
    cache->cellSize = 0.0f;
    if ( triangles.empty() )
        return;

    vec3d boundsMin = triangles[0][0];
    vec3d boundsMax = triangles[0][0];
    for (const yw_AttachedFXTriangle &triangle : triangles)
    {
        for (const vec3d &point : triangle)
        {
            boundsMin.x = std::min(boundsMin.x, point.x);
            boundsMin.y = std::min(boundsMin.y, point.y);
            boundsMin.z = std::min(boundsMin.z, point.z);
            boundsMax.x = std::max(boundsMax.x, point.x);
            boundsMax.y = std::max(boundsMax.y, point.y);
            boundsMax.z = std::max(boundsMax.z, point.z);
        }
    }

    const int gridCells = 24;
    const vec3d size = boundsMax - boundsMin;
    const double extent = std::max(size.x, std::max(size.y, size.z));
    if ( !isfinite(extent) || extent <= 0.001 )
        return;

    // One empty padding cell on every side keeps the outside reachable.
    const double cell = extent / gridCells;
    const int nx = std::max(1, std::min(gridCells, (int)ceil(size.x / cell))) + 2;
    const int ny = std::max(1, std::min(gridCells, (int)ceil(size.y / cell))) + 2;
    const int nz = std::max(1, std::min(gridCells, (int)ceil(size.z / cell))) + 2;
    const vec3d origin = boundsMin - vec3d(cell, cell, cell);
    auto cellIndex = [&](int x, int y, int z) { return (size_t)((z * ny + y) * nx + x); };

    std::vector<uint8_t> surface((size_t)nx * ny * nz, 0);
    for (const yw_AttachedFXTriangle &triangle : triangles)
    {
        const vec3d &a = triangle[0];
        const vec3d ab = triangle[1] - triangle[0];
        const vec3d ac = triangle[2] - triangle[0];
        const double longestEdge = std::max(ab.length(), std::max(ac.length(), (triangle[2] - triangle[1]).length()));
        // Sample the triangle every half cell, so no touched cell is skipped.
        const int steps = std::max(1, std::min(96, (int)ceil(longestEdge / (cell * 0.5))));
        for (int i = 0; i <= steps; i++)
        {
            for (int j = 0; i + j <= steps; j++)
            {
                const vec3d point = a + ab * ((double)i / steps) + ac * ((double)j / steps);
                const int x = std::max(0, std::min(nx - 1, (int)floor((point.x - origin.x) / cell)));
                const int y = std::max(0, std::min(ny - 1, (int)floor((point.y - origin.y) / cell)));
                const int z = std::max(0, std::min(nz - 1, (int)floor((point.z - origin.z) / cell)));
                surface[cellIndex(x, y, z)] = 1;
            }
        }
    }

    // Count, for every cell, the axis directions in which a surface cell lies.
    std::vector<uint8_t> votes(surface.size(), 0);
    const int dims[3] = {nx, ny, nz};
    for (int axis = 0; axis < 3; axis++)
    {
        const int u = (axis + 1) % 3;
        const int v = (axis + 2) % 3;
        for (int iu = 0; iu < dims[u]; iu++)
        {
            for (int iv = 0; iv < dims[v]; iv++)
            {
                auto lineIndex = [&](int along) {
                    int coords[3];
                    coords[axis] = along;
                    coords[u] = iu;
                    coords[v] = iv;
                    return cellIndex(coords[0], coords[1], coords[2]);
                };

                bool seen = false;
                for (int along = 0; along < dims[axis]; along++)
                {
                    const size_t index = lineIndex(along);
                    if ( seen )
                        votes[index]++;
                    if ( surface[index] )
                        seen = true;
                }

                seen = false;
                for (int along = dims[axis] - 1; along >= 0; along--)
                {
                    const size_t index = lineIndex(along);
                    if ( seen )
                        votes[index]++;
                    if ( surface[index] )
                        seen = true;
                }
            }
        }
    }

    std::vector<uint8_t> solid(surface.size(), 0);
    size_t solidCount = 0;
    for (size_t index = 0; index < surface.size(); index++)
    {
        solid[index] = surface[index] || votes[index] >= 5;
        solidCount += solid[index];
    }

    std::vector<uint8_t> keep = solid;
    if ( dropThinParts )
    {
        // A cell inside a one-cell-thin plate has at most 8 solid neighbours
        // out of 26; body surfaces have 11 or more.
        const int minNeighbours = 12;
        size_t keptCount = 0;
        for (int z = 1; z < nz - 1; z++)
        {
            for (int y = 1; y < ny - 1; y++)
            {
                for (int x = 1; x < nx - 1; x++)
                {
                    const size_t index = cellIndex(x, y, z);
                    if ( !solid[index] )
                        continue;

                    int neighbours = 0;
                    for (int dz = -1; dz <= 1; dz++)
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                                neighbours += solid[cellIndex(x + dx, y + dy, z + dz)];
                    neighbours -= 1; // The cell itself.

                    keep[index] = neighbours >= minNeighbours;
                    keptCount += keep[index];
                }
            }
        }

        // Thin-only models (or very small bodies): keep the whole silhouette.
        if ( keptCount < std::max<size_t>(8, solidCount / 4) )
            keep = solid;
    }

    vec3d centreSum(0.0, 0.0, 0.0);
    for (int z = 0; z < nz; z++)
    {
        for (int y = 0; y < ny; y++)
        {
            for (int x = 0; x < nx; x++)
            {
                if ( !keep[cellIndex(x, y, z)] )
                    continue;

                const vec3d centre = origin + vec3d((x + 0.5) * cell, (y + 0.5) * cell, (z + 0.5) * cell);
                cache->cells.push_back(centre);
                centreSum += centre;
            }
        }
    }

    if ( cache->cells.empty() )
        return;

    cache->centre = centreSum * (1.0 / (double)cache->cells.size());
    cache->cellSize = (float)cell;
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

    NC_STACK_base *visual = owner->_vp_normal;
    if ( !visual ) visual = owner->_vp_wait;
    if ( !visual ) visual = owner->_vp_fire;
    if ( !visual ) visual = owner->_vp_genesis;

    // Finds or builds the silhouette of one source. A valid collision profile
    // wins (structural hulls, already actor-local); the visual model is the
    // fallback that works for every unit, vanilla scripts included.
    auto findOrBuild = [&](const void *source, bool fromProfile) -> TAttachedFXGeometryCache * {
        for (TAttachedFXGeometryCache &candidate : _attachedFXGeometryCache)
        {
            if ( candidate.source == source &&
                 (fromProfile ||
                  (yw_SameAttachedFXTransform(candidate.scale, owner->_vp_scale) &&
                   yw_SameAttachedFXTransform(candidate.orientation, owner->_vp_rotation))) )
                return &candidate;
        }

        _attachedFXGeometryCache.emplace_back();
        TAttachedFXGeometryCache *cache = &_attachedFXGeometryCache.back();
        cache->source = source;
        cache->scale = owner->_vp_scale;
        cache->orientation = owner->_vp_rotation;

        std::vector<yw_AttachedFXTriangle> triangles;
        if ( fromProfile )
        {
            yw_CollectAttachedFXShapeTriangles(*static_cast<const Collision::Shape *>(source), &triangles);
        }
        else
        {
            mat3x3 transform = yw_BuildVPRotationMatrix(owner->_vp_rotation);
            transform *= mat3x3::Scale(yw_SafeVPScale(owner->_vp_scale));
            yw_CollectAttachedFXTriangles(visual, transform, vec3d(0.0, 0.0, 0.0), &triangles);
        }
        yw_BuildAttachedFXSilhouette(triangles, !fromProfile, cache);
        return cache;
    };

    TAttachedFXGeometryCache *cache = nullptr;
    if ( owner->_collisionShape && !owner->_collisionShape->parts.empty() )
        cache = findOrBuild(owner->_collisionShape.get(), true);
    if ( (!cache || cache->cells.empty()) && visual )
        cache = findOrBuild(visual, false);
    if ( !cache || cache->cells.empty() )
        return false;

    // Random point inside a random body cell, then pulled toward the body
    // centre: 100% = whole body, 25% = only its central core.
    const vec3d &cellCentre = cache->cells[(size_t)rand() % cache->cells.size()];
    vec3d jitter;
    jitter.x = ((float)rand() / ((float)RAND_MAX + 1.0f) - 0.5f) * cache->cellSize;
    jitter.y = ((float)rand() / ((float)RAND_MAX + 1.0f) - 0.5f) * cache->cellSize;
    jitter.z = ((float)rand() / ((float)RAND_MAX + 1.0f) - 0.5f) * cache->cellSize;
    const vec3d point = cellCentre + jitter;
    *localPosition = cache->centre + (point - cache->centre) * (randomOffsetPercent / 100.0f);

    return true;
}
