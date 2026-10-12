#ifndef OPENNEOUA_KAMIKAZE_CONTACT_H
#define OPENNEOUA_KAMIKAZE_CONTACT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../vectors.h"

namespace World
{
// Test the complete movement segment against a sphere, including tangency.
inline bool KamikazeSphereSweep(const vec3d &from, const vec3d &to,
                               const vec3d &center, double radius, double *fraction)
{
    const vec3d offset = from - center;
    const vec3d movement = to - from;
    const double outside = offset.square() - radius * radius;
    if ( !std::isfinite(radius) || radius <= 0.0 || !std::isfinite(outside) ||
         !std::isfinite(movement.square()) )
        return false;
    if ( outside <= 0.0 )
    {
        if ( fraction ) *fraction = 0.0;
        return true;
    }
    const double lengthSquared = movement.square();
    if ( lengthSquared <= 0.0 ) return false;
    const double projection = offset.dot(movement);
    if ( projection >= 0.0 ) return false;
    const double discriminant = projection * projection - lengthSquared * outside;
    if ( discriminant < 0.0 ) return false;
    const double entry = (-projection - std::sqrt(discriminant)) / lengthSquared;
    if ( entry < 0.0 || entry > 1.0 ) return false;
    if ( fraction ) *fraction = entry;
    return true;
}
}
#endif
