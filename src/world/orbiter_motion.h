// OpenNeoUA Orbiter motion: pure math for "model = orbiter" projectiles.
// No engine state is touched here, so the whole flight can be unit tested.
//
//  Guard: a slow, irregular orbit around the carrier. The offset is always at
//         least minRadius away from the carrier centre (it never passes through it).
//  Siege: a steering "dance" around the target. While dancing the orbiter flies
//         between random waypoints on a shell around the target; when a strike is
//         due it dives through the target centre (strike event), comes out on the
//         far side and dances again until the next strike.
//
// Coordinates follow Urban Assault: +y is DOWN, so "above" means negative y.
#ifndef WORLD_ORBITER_MOTION_H_INCLUDED
#define WORLD_ORBITER_MOTION_H_INCLUDED

#include <algorithm>
#include <cmath>
#include <cstdint>
#include "common/vectors.h"

namespace World
{
namespace Orbiter
{

// Small deterministic generator: every orbiter owns one, so its dance is
// different from its siblings but reproducible in tests.
struct Rng
{
    uint32_t state = 0x9E3779B9u;

    explicit Rng(uint32_t seed = 0x9E3779B9u) : state(seed ? seed : 0x9E3779B9u) {}

    uint32_t NextU()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float Next01() { return (float)(NextU() & 0xFFFFFFu) / 16777216.0f; }
    float Range(float a, float b) { return a + (b - a) * Next01(); }
    int RangeInt(int a, int b)
    {
        if ( b <= a )
            return a;
        return a + (int)(NextU() % (uint32_t)(b - a + 1));
    }
};

// Rotates unit vector dir toward desired by at most maxAngle radians.
inline vec3d SteerToward(const vec3d &dir, const vec3d &desired, double maxAngle)
{
    vec3d from = dir;
    vec3d to = desired;
    if ( to.normalise() <= 1e-6 )
        return dir;
    if ( from.normalise() <= 1e-6 )
        return to;

    const double cosA = std::max(-1.0, std::min(1.0, from.dot(to)));
    const double angle = std::acos(cosA);
    if ( angle <= maxAngle )
        return to;

    vec3d axis = from * to;
    if ( axis.normalise() <= 1e-6 )
    {
        // Exactly opposite: turn around any axis perpendicular to dir.
        axis = from * (std::fabs(from.y) < 0.9 ? vec3d(0.0, 1.0, 0.0) : vec3d(1.0, 0.0, 0.0));
        axis.normalise();
    }

    // Rodrigues rotation of "from" around "axis" by maxAngle.
    const double c = std::cos(maxAngle);
    const double s = std::sin(maxAngle);
    vec3d out = from * c + (axis * from) * s + axis * (axis.dot(from) * (1.0 - c));
    out.normalise();
    return out;
}

// ---- Guard ---------------------------------------------------------------

struct Guard
{
    float radius = 60.0f;      // mean distance from the carrier centre
    float minRadius = 30.0f;   // hard limit: never closer than this
    float phase = 0.0f;
    float angularSpeed = 0.8f; // rad/s, slow and monotone
    float seedA = 0.0f;
    float seedB = 0.0f;
    float seedC = 0.0f;
    float time = 0.0f;         // seconds since the guard started
};

inline Guard MakeGuard(Rng &rng, float radius, float minRadius)
{
    Guard g;
    g.minRadius = std::max(minRadius, 1.0f);
    g.radius = std::max(radius, g.minRadius);
    g.phase = rng.Range(0.0f, 6.2831853f);
    g.angularSpeed = rng.Range(0.55f, 1.0f) * (rng.Next01() < 0.5f ? -1.0f : 1.0f);
    g.seedA = rng.Range(0.0f, 6.2831853f);
    g.seedB = rng.Range(0.0f, 6.2831853f);
    g.seedC = rng.Range(0.0f, 6.2831853f);
    return g;
}

// Offset from the carrier centre at the current guard time. The horizontal
// distance alone is already >= minRadius, so the orbiter never enters the carrier.
inline vec3d GuardOffset(const Guard &g)
{
    const float t = g.time;
    const float angle = g.phase + g.angularSpeed * t + 0.45f * std::sin(0.37f * t + g.seedA);
    const float r = std::max(g.minRadius, g.radius * (1.0f + 0.15f * std::sin(0.53f * t + g.seedB)));
    // Mostly at or above the carrier: y in [-0.55r, +0.05r] (negative = up).
    const float h = g.radius * (-0.25f + 0.30f * std::sin(0.71f * t + g.seedC));
    return vec3d(r * std::cos(angle), h, r * std::sin(angle));
}

// ---- Siege ---------------------------------------------------------------

enum SiegeEvent
{
    SIEGE_NONE = 0,
    SIEGE_STRIKE,       // passed through the target, more strikes left
    SIEGE_FINAL_STRIKE  // last pass: the projectile must detonate now
};

struct Siege
{
    vec3d pos;
    vec3d dir = vec3d(0.0, 0.0, 1.0);
    float speed = 400.0f;        // world units per second
    float radius = 120.0f;       // dance shell radius around the target centre
    int strikesLeft = 1;
    bool diving = false;
    float danceLeft = 0.0f;      // seconds of dance before the next dive
    float intervalMin = 0.9f;    // seconds of dance between two strikes
    float intervalMax = 0.9f;
    vec3d waypoint;              // dance waypoint, relative to the target centre
    Rng rng;
};

// Random dance waypoint on the shell, mostly above the target, turned 60..120
// degrees from the previous one: the path loops around without crossing the
// centre (chord midpoint stays >= 0.5 * radius from it).
inline vec3d SiegeWaypoint(Rng &rng, const vec3d &previous, float radius)
{
    vec3d prev = previous;
    const bool hasPrev = prev.normalise() > 1e-6;
    vec3d pick(0.0, -1.0, 0.0);
    for (int attempt = 0; attempt < 12; attempt++)
    {
        const float yaw = rng.Range(0.0f, 6.2831853f);
        const float y = rng.Range(-1.0f, 0.25f);
        const float horiz = std::sqrt(std::max(0.0f, 1.0f - y * y));
        pick = vec3d(horiz * std::cos(yaw), y, horiz * std::sin(yaw));
        if ( !hasPrev )
            break;
        const double d = pick.dot(prev);
        if ( d <= 0.5 && d >= -0.5 ) // 60..120 degrees
            break;
    }
    return pick * (radius * rng.Range(0.85f, 1.15f));
}

inline void BeginSiege(Siege &s, Rng rng, const vec3d &pos, const vec3d &dir,
                       const vec3d &target, float speed, float radius,
                       float firstDelaySec, int strikes,
                       float intervalMinSec, float intervalMaxSec)
{
    s.rng = rng;
    s.pos = pos;
    s.dir = dir;
    if ( s.dir.normalise() <= 1e-6 )
        s.dir = vec3d(0.0, 0.0, 1.0);
    s.speed = std::max(speed, 1.0f);
    s.radius = std::max(radius, 1.0f);
    s.strikesLeft = std::max(strikes, 1);
    s.diving = false;
    s.danceLeft = std::max(firstDelaySec, 0.0f);
    s.intervalMin = std::max(intervalMinSec, 0.0f);
    s.intervalMax = std::max(intervalMaxSec, s.intervalMin);
    // First waypoint: the shell point on our side of the target.
    vec3d side = pos - target;
    if ( side.normalise() <= 1e-6 )
        side = vec3d(0.0, -1.0, 0.0);
    s.waypoint = side * s.radius;
}

// One simulation step. target / targetRadius are the live target centre and
// collision radius. Returns a strike event when the orbiter passes the centre.
inline SiegeEvent StepSiege(Siege &s, float dt, const vec3d &target, float targetRadius)
{
    if ( !(dt > 0.0f) )
        return SIEGE_NONE;

    const float step = s.speed * dt;
    // Loops of roughly the shell size: tighter shell = faster turning.
    const double turnRate = std::max(1.5, std::min(14.0, 2.6 * (double)s.speed / (double)s.radius));
    SiegeEvent event = SIEGE_NONE;
    vec3d desired;

    if ( s.diving )
    {
        desired = target - s.pos;
        const double hitDistance = std::max({(double)targetRadius * 0.5, (double)step, 4.0});
        if ( desired.length() <= hitDistance )
        {
            s.pos = target; // the strike happens inside the target
            s.strikesLeft--;
            if ( s.strikesLeft <= 0 )
                return SIEGE_FINAL_STRIKE;

            event = SIEGE_STRIKE;
            s.diving = false;
            s.danceLeft = s.rng.Range(s.intervalMin, s.intervalMax);
            // Exit through the far side, kept mostly above the target.
            vec3d exitDir = s.dir;
            exitDir.y = std::min(exitDir.y, 0.25);
            if ( exitDir.normalise() <= 1e-6 )
                exitDir = vec3d(0.0, -1.0, 0.0);
            s.waypoint = exitDir * (s.radius * s.rng.Range(0.9f, 1.2f));
            desired = s.waypoint;
        }
    }
    else
    {
        s.danceLeft -= dt;
        vec3d toWaypoint = (target + s.waypoint) - s.pos;
        if ( toWaypoint.length() <= std::max((double)step, (double)s.radius * 0.25) )
        {
            s.waypoint = SiegeWaypoint(s.rng, s.waypoint, s.radius);
            toWaypoint = (target + s.waypoint) - s.pos;
        }

        if ( s.danceLeft <= 0.0f )
        {
            s.diving = true;
            desired = target - s.pos;
        }
        else
        {
            desired = toWaypoint;
            // Keep the dance off the target body: push outward when too close.
            vec3d out = s.pos - target;
            const double dist = out.normalise();
            const double keepOut = std::max((double)targetRadius * 1.5, (double)s.radius * 0.45);
            if ( dist > 1e-6 && dist < keepOut )
            {
                vec3d d = desired;
                d.normalise();
                desired = d + out * (2.0 * (1.0 - dist / keepOut));
            }
        }
    }

    // Diving homes harder so a moving target cannot be orbited forever.
    s.dir = SteerToward(s.dir, desired, turnRate * (s.diving ? 1.6 : 1.0) * dt);
    s.pos += s.dir * step;
    return event;
}

} // namespace Orbiter
} // namespace World

#endif // WORLD_ORBITER_MOTION_H_INCLUDED
