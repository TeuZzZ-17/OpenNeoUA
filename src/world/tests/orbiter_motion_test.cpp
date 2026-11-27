// OpenNeoUA Orbiter motion: guard never enters the carrier, the siege dance
// strikes exactly N times through the target and never clips it while dancing.
// Header-only module: build with  g++ -std=c++17 -I src src/world/tests/orbiter_motion_test.cpp
#include "world/orbiter_motion.h"
#include <cstdio>

using namespace World::Orbiter;

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label)
{
    ++checks;
    if ( !value ) { ++failures; std::printf("FAIL %s\n", label); }
}

// Simulates one besieged target (optionally moving) and counts events.
struct SiegeRun { int strikes = 0; int finals = 0; bool clippedWhileDancing = false; bool finished = false; };

SiegeRun RunSiege(uint32_t seed, int strikes, float radius, float targetRadius, vec3d targetVel)
{
    SiegeRun run;
    Rng rng(seed);
    vec3d target(0.0, 0.0, 0.0);
    Siege s;
    BeginSiege(s, rng, vec3d(600.0, -200.0, 0.0), vec3d(-1.0, 0.0, 0.0), target,
               420.0f, radius, 1.5f, strikes, 0.7f, 1.2f);
    const float dt = 1.0f / 60.0f;
    bool exiting = false; // after a strike the orbiter is still inside the body
    for (int frame = 0; frame < 60 * 60 && !run.finished; frame++)
    {
        target += targetVel * dt;
        const bool wasDiving = s.diving;
        SiegeEvent ev = StepSiege(s, dt, target, targetRadius);
        if ( ev == SIEGE_STRIKE ) { run.strikes++; exiting = true; continue; }
        if ( ev == SIEGE_FINAL_STRIKE ) { run.finals++; run.finished = true; }
        const double dist = (s.pos - target).length();
        if ( exiting && dist >= targetRadius )
            exiting = false;
        if ( !exiting && ev == SIEGE_NONE && !wasDiving && !s.diving && dist < targetRadius )
            run.clippedWhileDancing = true;
    }
    return run;
}
}

int main()
{
    // Guard: distance from the carrier is always >= minRadius.
    {
        bool inside = false;
        for (uint32_t seed = 1; seed < 200; seed++)
        {
            Rng rng(seed);
            Guard g = MakeGuard(rng, rng.Range(10.0f, 200.0f), 40.0f);
            for (int i = 0; i < 2000; i++)
            {
                g.time = i * 0.05f;
                if ( GuardOffset(g).length() < g.minRadius - 1e-3 )
                    inside = true;
            }
        }
        Check(!inside, "guard never enters the carrier");
        Rng rng(7);
        Guard g = MakeGuard(rng, 5.0f, 40.0f);
        Check(g.radius >= 40.0f, "guard radius below the hard limit is raised");
    }

    // Ring guard: chaos 0 = evenly spaced at the exact radius; speed 0 = still;
    // with chaos the wander never enters the carrier.
    {
        Rng rng(3);
        Guard g = MakeGuard(rng, 100.0f, 40.0f);
        bool spaced = true;
        for (int slot = 0; slot < 6; slot++)
        {
            const vec3d a = RingGuardOffset(g, slot, 6, 0.0f, 0.0f, 0.0f);
            const vec3d b = RingGuardOffset(g, (slot + 1) % 6, 6, 0.0f, 0.0f, 0.0f);
            const double horiz = std::sqrt(a.x * a.x + a.z * a.z);
            const double gap = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
            if ( std::fabs(horiz - 100.0) > 1e-3 || std::fabs(gap - 100.0) > 1e-3 ) // hexagon side = radius
                spaced = false;
        }
        Check(spaced, "ordered ring: exact radius and equal spacing");

        g.time = 7.0f;
        const vec3d still0 = RingGuardOffset(g, 2, 6, 0.0f, 0.0f, 0.0f);
        const vec3d still1 = RingGuardOffset(g, 2, 6, 0.0f, 0.0f, 9.0f);
        Check((still0 - still1).length() < 1e-6, "speed 0 and chaos 0: still");
        const vec3d turned = RingGuardOffset(g, 2, 6, 5.0f, 0.0f, 1.0f);
        Check(std::fabs(std::sqrt(turned.x * turned.x + turned.z * turned.z) - 100.0) < 1e-3 &&
              (turned - still0).length() > 1.0, "speed turns the ring at the same radius");

        bool inside = false;
        for (uint32_t seed = 1; seed < 100; seed++)
        {
            Rng r(seed);
            Guard wander = MakeGuard(r, r.Range(10.0f, 200.0f), 40.0f);
            for (int i = 0; i < 1000; i++)
            {
                wander.time = i * 0.05f;
                const vec3d o = RingGuardOffset(wander, (int)(seed % 8), 8, 3.0f, 10.0f, i * 0.05f);
                if ( std::sqrt(o.x * o.x + o.z * o.z) < wander.minRadius - 1e-3 )
                    inside = true;
            }
        }
        Check(!inside, "chaotic ring never enters the carrier");
    }

    // Steering never turns more than allowed and keeps unit length.
    {
        vec3d out = SteerToward(vec3d(1.0, 0.0, 0.0), vec3d(-1.0, 0.0, 0.0), 0.1);
        Check(std::fabs(out.length() - 1.0) < 1e-6, "steer keeps unit length");
        Check(std::acos(std::max(-1.0, std::min(1.0, out.dot(vec3d(1.0, 0.0, 0.0))))) <= 0.1 + 1e-6,
              "steer limited by max angle (opposite direction)");
        vec3d exact = SteerToward(vec3d(1.0, 0.0, 0.0), vec3d(1.0, 0.0, 0.05), 0.5);
        Check(std::fabs(exact.z) > 0.0, "small turn reaches desired direction");
    }

    // Siege: exactly N passes, the last one final; no clipping while dancing.
    for (int strikes = 1; strikes <= 5; strikes++)
    {
        int bad = 0, clip = 0;
        for (uint32_t seed = 1; seed <= 50; seed++)
        {
            SiegeRun r = RunSiege(seed, strikes, 140.0f, 30.0f, vec3d(0.0, 0.0, 0.0));
            if ( !r.finished || r.finals != 1 || r.strikes != strikes - 1 ) bad++;
            if ( r.clippedWhileDancing ) clip++;
        }
        char label[96];
        std::snprintf(label, sizeof(label), "static target: %d strikes exactly", strikes);
        Check(bad == 0, label);
        std::snprintf(label, sizeof(label), "static target: no clipping while dancing (%d strikes)", strikes);
        Check(clip == 0, label);
    }

    // Moving target (tank speed): still reached and finished.
    {
        int bad = 0;
        for (uint32_t seed = 1; seed <= 50; seed++)
        {
            SiegeRun r = RunSiege(seed, 3, 140.0f, 30.0f, vec3d(120.0, 0.0, 60.0));
            if ( !r.finished || r.strikes != 2 ) bad++;
        }
        Check(bad == 0, "moving target: all strikes land");
    }

    // Same seed = same dance (reproducible).
    {
        SiegeRun a = RunSiege(42, 3, 140.0f, 30.0f, vec3d(0.0, 0.0, 0.0));
        SiegeRun b = RunSiege(42, 3, 140.0f, 30.0f, vec3d(0.0, 0.0, 0.0));
        Check(a.strikes == b.strikes && a.finished == b.finished, "deterministic per seed");
    }

    std::printf("orbiter_motion_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
