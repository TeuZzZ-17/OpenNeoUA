#ifndef WORLD_TOOLS_H
#define WORLD_TOOLS_H

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "common/common.h"
#include "common/plane.h"
#include "../vectors.h"
#include "../bitmap.h"
#include "consts.h"

namespace World {

// Shared authored scalar syntax used by modern OpenNeoUA parameters:
//   2000  -> absolute value
//   5%    -> percentage value
//   +20%  -> positive percentage adjustment
//   -20%  -> negative percentage adjustment
// The parser is deliberately strict and does not assign semantics beyond
// identifying whether the author supplied a percent sign. Callers keep their
// own range/default/fallback rules.
struct TAuthoredScalar
{
    float value = 0.0f;
    bool percent = false;
};

inline bool ParseAuthoredScalar(const std::string &text, TAuthoredScalar &out)
{
    if ( text.empty() )
        return false;

    const char *begin = text.c_str();
    while ( *begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n' )
        ++begin;
    if ( !*begin )
        return false;

    errno = 0;
    char *end = NULL;
    float parsed = std::strtof(begin, &end);
    if ( end == begin || errno == ERANGE || !std::isfinite(parsed) )
        return false;

    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
        ++end;

    bool isPercent = false;
    if ( *end == '%' )
    {
        isPercent = true;
        ++end;
        while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
            ++end;
    }

    if ( *end != '\0' )
        return false;

    out.value = parsed;
    out.percent = isPercent;
    return true;
}

// Smart list syntax shared by VP-style parameters: "123_456_789". A single
// positive integer is also valid. Zero/negative/invalid entries are rejected
// instead of silently creating holes; maxItems <= 0 means no explicit limit.
inline bool ParsePositiveInt16ListValue(const std::string &text,
                                        std::vector<int16_t> &out,
                                        int maxItems = 0)
{
    out.clear();
    if ( text.empty() )
        return false;

    size_t start = 0;
    while ( start <= text.size() )
    {
        const size_t separator = text.find('_', start);
        const size_t end = separator == std::string::npos ? text.size() : separator;
        const std::string part = text.substr(start, end - start);
        if ( part.empty() )
        {
            out.clear();
            return false;
        }

        errno = 0;
        char *parseEnd = NULL;
        const long value = std::strtol(part.c_str(), &parseEnd, 0);
        if ( parseEnd == part.c_str() || errno == ERANGE || *parseEnd != '\0' ||
             value <= 0 || value > std::numeric_limits<int16_t>::max() )
        {
            out.clear();
            return false;
        }

        out.push_back((int16_t)value);
        if ( maxItems > 0 && (int)out.size() > maxItems )
        {
            out.clear();
            return false;
        }

        if ( separator == std::string::npos )
            break;
        start = separator + 1;
    }

    return !out.empty();
}

// OpenNeoUA range syntax shared by script/config parameters authored as either
// a fixed "value" or an inclusive "min_max" range. The generic tokenizer is
// intentionally untouched; only callers that opt in get this syntax. Reversed
// endpoints are normalized automatically.
inline bool ParseIntRangeValue(const std::string &value, int &minValue, int &maxValue)
{
    auto parsePart = [](const std::string &part, int &out) -> bool
    {
        if ( part.empty() )
            return false;

        errno = 0;
        char *end = NULL;
        const char *begin = part.c_str();
        long long parsed = std::strtoll(begin, &end, 0);
        if ( end == begin || errno == ERANGE || *end != '\0' ||
             parsed < std::numeric_limits<int>::min() ||
             parsed > std::numeric_limits<int>::max() )
            return false;

        out = (int)parsed;
        return true;
    };

    const size_t separator = value.find('_');
    if ( separator == std::string::npos )
    {
        int fixed = 0;
        if ( !parsePart(value, fixed) )
            return false;
        minValue = fixed;
        maxValue = fixed;
        return true;
    }

    if ( value.find('_', separator + 1) != std::string::npos )
        return false;

    int first = 0;
    int second = 0;
    if ( !parsePart(value.substr(0, separator), first) ||
         !parsePart(value.substr(separator + 1), second) )
        return false;

    minValue = std::min(first, second);
    maxValue = std::max(first, second);
    return true;
}

// Positive bounded specialization for authored count ranges. It deliberately
// delegates syntax and reversed-endpoint handling to ParseIntRangeValue().
inline bool ParsePositiveIntRangeValue(const std::string &value,
                                       int maxAllowed,
                                       int &minValue,
                                       int &maxValue)
{
    int parsedMin = 0;
    int parsedMax = 0;
    if ( maxAllowed <= 0 ||
         !ParseIntRangeValue(value, parsedMin, parsedMax) ||
         parsedMin <= 0 || parsedMax <= 0 )
    {
        minValue = 0;
        maxValue = 0;
        return false;
    }

    minValue = std::min(parsedMin, maxAllowed);
    maxValue = std::min(parsedMax, maxAllowed);
    return true;
}

inline bool ParseFloatRangeValue(const std::string &value, float &minValue, float &maxValue)
{
    auto parsePart = [](const std::string &part, float &out) -> bool
    {
        if ( part.empty() )
            return false;

        errno = 0;
        char *end = NULL;
        const char *begin = part.c_str();
        float parsed = std::strtof(begin, &end);
        if ( end == begin || errno == ERANGE || *end != '\0' || !std::isfinite(parsed) )
            return false;

        out = parsed;
        return true;
    };

    const size_t separator = value.find('_');
    if ( separator == std::string::npos )
    {
        float fixed = 0.0f;
        if ( !parsePart(value, fixed) )
            return false;
        minValue = fixed;
        maxValue = fixed;
        return true;
    }

    if ( value.find('_', separator + 1) != std::string::npos )
        return false;

    float first = 0.0f;
    float second = 0.0f;
    if ( !parsePart(value.substr(0, separator), first) ||
         !parsePart(value.substr(separator + 1), second) )
        return false;

    minValue = std::min(first, second);
    maxValue = std::max(first, second);
    return true;
}

// Shared inclusive integer roll for authored value/min_max ranges. Keeping the
// calculation here prevents each runtime feature from growing its own subtly
// different rand() endpoint handling.
inline int RandomIntRangeInclusive(int minValue, int maxValue)
{
    if ( maxValue < minValue )
        std::swap(minValue, maxValue);

    if ( minValue == maxValue )
        return minValue;

    const double randomPart = (double)rand() / ((double)RAND_MAX + 1.0);
    const int64_t range = (int64_t)maxValue - minValue;
    return minValue + (int)((range + 1) * randomPart);
}

// Shared yaw/pitch direction used by Proximity Defense and Weapon Cluster.
// Authored angle ranges are sampled uniformly. Callers may instead request the
// existing evenly-spaced full-circle distribution by supplying a valid shot
// index/count and evenlyDistributeYaw=true.
inline vec3d ResolveYawPitchDirection(bool horizontalAngleSet,
                                      float horizontalMin,
                                      float horizontalMax,
                                      bool verticalAngleSet,
                                      float verticalMin,
                                      float verticalMax,
                                      int shotIndex = 0,
                                      int totalShots = 0,
                                      bool evenlyDistributeYaw = false)
{
    if ( horizontalMax < horizontalMin )
        std::swap(horizontalMin, horizontalMax);
    if ( verticalMax < verticalMin )
        std::swap(verticalMin, verticalMax);

    double yaw = 0.0;
    double pitch = 0.0;

    if ( evenlyDistributeYaw && totalShots > 0 )
    {
        yaw = (double)horizontalMin +
              ((double)shotIndex / (double)totalShots) * 360.0;
    }
    else if ( horizontalAngleSet )
    {
        const double randomPart = (double)rand() / (double)RAND_MAX;
        yaw = (double)horizontalMin +
              randomPart * ((double)horizontalMax - horizontalMin);
    }

    if ( verticalAngleSet )
    {
        const double randomPart = (double)rand() / (double)RAND_MAX;
        pitch = (double)verticalMin +
                randomPart * ((double)verticalMax - verticalMin);
    }

    const double degreesToRadians = 0.01745329251994329577;
    const double yawRad = yaw * degreesToRadians;
    const double pitchRad = pitch * degreesToRadians;
    const double horizontal = std::cos(pitchRad);

    return vec3d(std::sin(yawRad) * horizontal,
                 std::sin(pitchRad),
                 std::cos(yawRad) * horizontal);
}

inline vec3d ApplyDirectionalSpread(const mat3x3 &rotation,
                                    const vec3d &direction,
                                    float spreadX,
                                    float spreadY)
{
    if ( spreadX <= 0.0f && spreadY <= 0.0f )
        return direction;

    vec3d aimDir = direction;
    if ( aimDir.normalise() <= 0.001f )
        return direction;

    vec3d right = rotation.AxisX();
    right -= aimDir * right.dot(aimDir);

    if ( right.normalise() <= 0.001f )
    {
        const vec3d refAxis = std::fabs(aimDir.y) < 0.99f
                                  ? vec3d::OY(1.0f)
                                  : vec3d::OX(1.0f);
        right = refAxis * aimDir;
    }

    if ( right.normalise() <= 0.001f )
        return aimDir;

    vec3d up = aimDir * right;
    if ( up.normalise() <= 0.001f )
        return aimDir;

    float randomX = 0.0f;
    float randomY = 0.0f;

    if ( spreadX > 0.0f )
        randomX = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) *
                  std::tan(spreadX * 0.01745329251994329577);
    if ( spreadY > 0.0f )
        randomY = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) *
                  std::tan(spreadY * 0.01745329251994329577);

    aimDir += right * randomX + up * randomY;
    if ( aimDir.normalise() > 0.001f )
        return aimDir;

    return direction;
}

inline bool IsWeaponClusterTriggerDue(int triggerTime,
                                      int elapsedTime,
                                      bool impactOrDetonation)
{
    if ( triggerTime <= 0 )
        return impactOrDetonation;

    return !impactOrDetonation && elapsedTime >= triggerTime;
}

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

// Single VP fade path shared by transient visual effects. Callers provide the
// effect-local timeline; ordinary materials use the factor as alpha, while
// additive LUMTRACY materials use it as RGB intensity because GL_ONE/GL_ONE
// ignores source alpha. Scale, lifetime and gameplay state remain unchanged.
inline float ComputeVPFadeEnvelope(double elapsed, double totalDuration,
                                   double fadeIn, double fadeOut)
{
    return ComputeTemporalEnvelope(elapsed, totalDuration, fadeIn, fadeOut);
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
