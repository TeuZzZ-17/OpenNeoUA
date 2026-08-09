#ifndef WORLD_TOOLS_H
#define WORLD_TOOLS_H

#include <algorithm>
#include <cmath>
#include <string>

#include "common/common.h"
#include "common/plane.h"
#include "../vectors.h"
#include "../bitmap.h"
#include "consts.h"

namespace World {

// Shared optional effect envelope. Fade-in and fade-out are part of the total
// duration. If their sum exceeds the total, both are reduced proportionally so
// their authored ratio remains deterministic. Invalid/negative fade values are
// treated as zero. A non-positive total supports fade-in only (useful for
// open-ended effects); fade-out requires a real end time.
inline float ComputeTemporalEnvelope(double elapsed, double totalDuration,
                                     double fadeIn, double fadeOut)
{
    if ( !std::isfinite(elapsed) || elapsed < 0.0 )
        elapsed = 0.0;

    if ( !std::isfinite(fadeIn) || fadeIn < 0.0 )
        fadeIn = 0.0;
    if ( !std::isfinite(fadeOut) || fadeOut < 0.0 )
        fadeOut = 0.0;

    if ( !std::isfinite(totalDuration) || totalDuration <= 0.0 )
    {
        if ( fadeIn <= 0.0 )
            return 1.0f;

        double factor = elapsed / fadeIn;
        if ( factor < 0.0 )
            return 0.0f;
        if ( factor > 1.0 )
            return 1.0f;
        return (float)factor;
    }

    const double fadeSum = fadeIn + fadeOut;
    if ( fadeSum > totalDuration && fadeSum > 0.0 )
    {
        const double scale = totalDuration / fadeSum;
        fadeIn *= scale;
        fadeOut *= scale;
    }

    double factor = 1.0;
    if ( fadeIn > 0.0 && elapsed < fadeIn )
        factor = elapsed / fadeIn;

    if ( fadeOut > 0.0 )
    {
        const double fadeOutStart = totalDuration - fadeOut;
        if ( elapsed > fadeOutStart )
            factor = std::min(factor, (totalDuration - elapsed) / fadeOut);
    }

    if ( factor < 0.0 )
        return 0.0f;
    if ( factor > 1.0 )
        return 1.0f;
    return (float)factor;
}

// Shared normalized linear spatial attenuation. Optional callers keep their
// exact legacy path when radius is absent/non-positive; configured callers get
// 100% at the source and exactly zero at/after the radius.
inline float ComputeSpatialFalloff(float distance, float radius)
{
    if ( !std::isfinite(radius) || radius <= 0.0f )
        return 1.0f;
    if ( !std::isfinite(distance) || distance >= radius )
        return 0.0f;
    if ( distance <= 0.0f )
        return 1.0f;

    const float factor = 1.0f - distance / radius;
    return factor < 0.0f ? 0.0f : (factor > 1.0f ? 1.0f : factor);
}

inline Common::Point PositionToSectorID(const vec3d &pos)
{ return Common::Point(pos.x / CVSectorLength, -pos.z / CVSectorLength); };

inline Common::Point PositionToSectorID(const vec2d &pos)
{ return Common::Point(pos.x / CVSectorLength, -pos.y / CVSectorLength); };

inline Common::Point PositionToSectorID(float x, float z)
{ return Common::Point(x / CVSectorLength, -z / CVSectorLength); };


inline vec3d SectorIDToPos3(const Common::Point &sec)
{ return vec3d((float)sec.x * CVSectorLength, 0.0, -((float)sec.y * CVSectorLength)); };

inline vec2d SectorIDToPos2(const Common::Point &sec)
{ return vec2d((float)sec.x * CVSectorLength, -((float)sec.y * CVSectorLength)); };


inline vec3d SectorIDToCenterPos3(const Common::Point &sec)
{ return vec3d((float)sec.x * CVSectorLength + CVSectorHalfLength, 0.0, -((float)sec.y * CVSectorLength + CVSectorHalfLength)); };

inline vec2d SectorIDToCenterPos2(const Common::Point &sec)
{ return vec2d((float)sec.x * CVSectorLength + CVSectorHalfLength, -((float)sec.y * CVSectorLength + CVSectorHalfLength)); };

// Shared radial push attenuation. This is the same linear shape historically
// used by aoe_unit_push when aoe_falloff is enabled.
inline float AoePushFalloffFactor(float distance, float radius, bool falloff)
{
    if ( radius <= 0.0f || distance > radius )
        return 0.0f;

    if ( !falloff )
        return 1.0f;

    return ComputeSpatialFalloff(distance, radius);
}


Common::PlaneBytes GetPlaneBytesFromBitmap(NC_STACK_bitmap *bitmap);
Common::PlaneBytes LoadMapDataFromImage(const std::string &fileName);


}

#endif
