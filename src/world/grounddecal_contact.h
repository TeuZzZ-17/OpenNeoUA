#ifndef OPENNEOUA_GROUNDDECAL_CONTACT_H
#define OPENNEOUA_GROUNDDECAL_CONTACT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "common/vectors.h"

namespace World
{
// Find the nearest point on the filled triangle, including edges and vertices.
inline vec3d GroundDecalClosestTrianglePoint(const vec3d &point,
                                            const vec3d &a,
                                            const vec3d &b,
                                            const vec3d &c)
{
    const vec3d ab = b - a;
    const vec3d ac = c - a;
    const vec3d normal = ab * ac;
    const double normalSquared = normal.dot(normal);
    if ( normalSquared > 0.0 )
    {
        const vec3d projected = point - normal * ((point - a).dot(normal) / normalSquared);
        const vec3d offset = projected - a;
        const double u = (offset * ac).dot(normal) / normalSquared;
        const double v = (ab * offset).dot(normal) / normalSquared;
        if ( u >= 0.0 && v >= 0.0 && u + v <= 1.0 )
            return projected;
    }

    // Edges also handle collapsed triangles without dividing by zero.
    const auto closestOnEdge = [&point](const vec3d &start, const vec3d &end)
    {
        const vec3d edge = end - start;
        const double squared = edge.dot(edge);
        const double amount = squared > 0.0
            ? std::max(0.0, std::min(1.0, (point - start).dot(edge) / squared)) : 0.0;
        return start + edge * amount;
    };
    vec3d closest = closestOnEdge(a, b);
    double best = (point - closest).dot(point - closest);
    for (const vec3d &candidate : {closestOnEdge(b, c), closestOnEdge(c, a)})
    {
        const double squared = (point - candidate).dot(point - candidate);
        if ( squared < best )
        {
            best = squared;
            closest = candidate;
        }
    }
    return closest;
}

inline bool GroundDecalSphereTouchesPoint(const vec3d &center, double radius,
                                          const vec3d &point)
{
    const vec3d offset = point - center;
    const double squared = offset.dot(offset);
    return std::isfinite(radius) && radius > 0.0 &&
           std::isfinite(squared) && squared <= radius * radius;
}
}

#endif
