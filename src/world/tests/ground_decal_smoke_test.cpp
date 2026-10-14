// Probe for the ground decal smoke math (stateless puffs) and its default parameters.
#include "../grounddecal_smoke.h"
#include <cmath>
#include <cstdio>

// SDL renames main to SDL_main; the probe needs its own entry point.
#undef main

namespace
{
int checks = 0, failures = 0;
void Check(bool result, const char *label)
{
    ++checks;
    if ( !result ) { ++failures; std::printf("FAIL: %s\n", label); }
}

namespace GDS = World::GroundDecalSmoke;

bool FinitePuff(const GDS::Puff &p)
{
    return std::isfinite(p.pos.x) && std::isfinite(p.pos.y) && std::isfinite(p.pos.z) &&
           std::isfinite(p.size) && std::isfinite(p.alpha) && p.alpha >= 0.0f && p.alpha <= 1.0f &&
           p.size >= GDS::kMinParticleSize;
}
}

int main()
{
    // A block without smoke keys must stay vanilla; with smoke = 1 the defaults give a dense plume.
    const World::TGroundDecalSmoke defaults;
    World::TChainFXConfig plainBlock;
    Check(!defaults.enabled && !plainBlock.ground_decal_smoke.enabled, "smoke is disabled unless a block enables it");
    Check(defaults.count_min == 10 && defaults.count_max == 30 &&
          defaults.duration_min == 20000 && defaults.duration_max == 25000 && defaults.intensity == 120.0f && defaults.size_min == 35.0f && defaults.size_max == 50.0f,
          "standard smoke defaults");
    Check(defaults.tint.a > 0.5f && defaults.tint.a < 0.7f && defaults.tint.r < 0.3f, "default tint is semi-transparent dark gray");

    // Life shape.
    Check(GDS::Intensity(-1, 1000, 100, 100) == 0.0f && GDS::Intensity(1000, 1000, 100, 100) == 0.0f &&
          GDS::Intensity(0, 0, 0, 0) == 0.0f, "no intensity outside the lifetime");
    Check(GDS::Intensity(0, 10000, 600, 5000) == 0.0f &&
          std::fabs(GDS::Intensity(300, 10000, 600, 5000) - 0.5f) < 1e-5f, "linear fade-in");
    Check(GDS::Intensity(5000, 10000, 600, 5000) == 1.0f, "full strength between the fades");
    Check(std::fabs(GDS::Intensity(9000, 10000, 600, 5000) - 0.2f) < 1e-5f, "linear fade-out");
    Check(GDS::Intensity(9999, 10000, 600, 5000) < 0.001f, "smoke has died at the end");
    Check(GDS::Intensity(0, 10000, 0, 0) == 1.0f && GDS::Intensity(9999, 10000, 0, 0) == 1.0f,
          "zero fades mean instant start and instant stop");
    Check(GDS::Intensity(0, 1000, 5000, 5000) == 0.0f && GDS::Intensity(500, 1000, 5000, 5000) < 0.11f,
          "fades longer than the lifetime never exceed full strength");

    // Puff count scales with decal radius but stays bounded.
    Check(GDS::ScaledCount(6, 50.0f, 24) == 6, "reference radius keeps the count");
    Check(GDS::ScaledCount(6, 5.0f, 24) == 3, "small decal halves the count at most");
    Check(GDS::ScaledCount(6, 500.0f, 24) == 18, "big decal triples the count at most");
    Check(GDS::ScaledCount(20, 500.0f, 24) == 24, "cap is respected");
    Check(GDS::ScaledCount(0, 100.0f, 24) == 0 && GDS::ScaledCount(6, 0.0f, 24) == 0 &&
          GDS::ScaledCount(6, std::nanf(""), 24) == 0, "zero or invalid input gives no puffs");

    // Smoke is finite, deterministic, above the ground and keeps going for the whole lifetime.
    GDS::Emitter e;
    e.params = defaults;
    e.params.enabled = true;
    e.pos = vec3d(1000.0, -50.0, -2000.0);
    e.radius = 100.0f;
    e.seed = 0x1234567u;
    e.duration = 60000;
    e.count = GDS::ScaledCount(e.params.count_max, e.radius, GDS::kMaxPuffs);

    int seen = 0, seenLate = 0;
    bool rises = true, finite = true, deterministic = true, inArea = true;
    for (int age = 0; age < e.duration; age += 97)
    {
        for (int slot = 0; slot < e.count; ++slot)
        {
            GDS::Puff a, b;
            const bool on = GDS::Evaluate(e, slot, age, &a);
            if ( on != GDS::Evaluate(e, slot, age, &b) ) deterministic = false;
            if ( !on ) continue;
            ++seen;
            if ( age > 45000 ) ++seenLate;
            if ( a.pos.x != b.pos.x || a.pos.y != b.pos.y || a.pos.z != b.pos.z ) deterministic = false;
            if ( !FinitePuff(a) ) finite = false;
            if ( !(a.pos.y < e.pos.y) ) rises = false;
            // The start area is `spread` percent of the radius; wind drift adds sideways travel.
            const double dx = a.pos.x - e.pos.x, dz = a.pos.z - e.pos.z;
            const double reach = e.params.spread * 0.01 * e.radius +
                                 e.params.rise_speed * 0.01 * e.radius * (e.params.puff_time_max * 0.001) *
                                 e.params.drift * 0.01 + 0.01;
            if ( std::sqrt(dx * dx + dz * dz) > reach ) inArea = false;
        }
    }
    Check(seen > 0 && seenLate > 0, "smoke appears and is still there late in its life");
    Check(finite, "puff values are finite, alpha in 0..1, size above minimum");
    Check(deterministic, "same age gives the same puff");
    Check(rises, "smoke is above the ground");
    Check(inArea, "smoke stays inside its start area plus the wind drift");

    GDS::Puff none;
    Check(!GDS::Evaluate(e, 0, e.duration, &none) && !GDS::Evaluate(e, 0, -5, &none), "no smoke outside the lifetime");
    Check(!GDS::Evaluate(e, e.count, 20000, &none), "slot above the count is off");

    // Puffs switch off one by one during the fade-out.
    int activeEarly = 0, activeLate = 0;
    for (int slot = 0; slot < e.count; ++slot)
    {
        GDS::Puff p;
        if ( GDS::Evaluate(e, slot, 30000, &p) ) ++activeEarly;
        if ( GDS::Evaluate(e, slot, e.duration - 2000, &p) ) ++activeLate;
    }
    Check(activeEarly == e.count && activeLate > 0 && activeLate < activeEarly,
          "fewer puffs near the end of the smoke");

    // Authored values really change the result: intensity-independent geometry follows the radius.
    GDS::Emitter big = e;
    big.radius = 300.0f;
    double maxRiseSmall = 0.0, maxRiseBig = 0.0;
    for (int age = 5000; age < 50000; age += 131)
    {
        GDS::Puff a, b;
        if ( GDS::Evaluate(e, 0, age, &a) ) maxRiseSmall = std::max(maxRiseSmall, e.pos.y - a.pos.y);
        if ( GDS::Evaluate(big, 0, age, &b) ) maxRiseBig = std::max(maxRiseBig, big.pos.y - b.pos.y);
    }
    Check(maxRiseBig > maxRiseSmall * 2.0, "bigger decals send smoke higher");

    GDS::Emitter noRise = e;
    noRise.params.rise_speed = 0.0f;
    noRise.params.drift = 0.0f;
    bool still = true;
    for (int age = 5000; age < 50000; age += 131)
    {
        GDS::Puff p;
        if ( GDS::Evaluate(noRise, 0, age, &p) && std::fabs((e.pos.y - p.pos.y) - (GDS::kGroundLift + 0.05 * e.radius)) > 1e-3 )
            still = false;
    }
    Check(still, "rise_speed 0 keeps the smoke at its starting height");

    // Same slot, same cycle: later phase means higher and bigger.
    const uint32_t slotHash = GDS::Hash(e.seed, 0u, 0x534d4f4bu);
    const float period = GDS::RangeFrom(GDS::Hash(slotHash, 1u, 0u),
                                        (float)e.params.puff_time_min, (float)e.params.puff_time_max);
    const float offset = period * GDS::HashUnit(GDS::Hash(slotHash, 2u, 0u));
    const int cycleStart = (int)std::ceil(std::floor((30000.0 + offset) / period + 1.0) * period - offset);
    GDS::Puff early, late;
    const bool grew = GDS::Evaluate(e, 0, cycleStart + 200, &early) &&
                      GDS::Evaluate(e, 0, cycleStart + (int)(period * 0.6f), &late) &&
                      late.size > early.size && late.pos.y < early.pos.y;
    Check(grew, "smoke grows and rises within one cycle");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
