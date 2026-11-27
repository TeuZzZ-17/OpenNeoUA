// OpenNeoUA Orbiter: orbiter_* parsing rules.
// Run from Data: python src/world/tests/run_native_probe.py src/world/tests/orbiter_parse_test.cpp
#include "game/yw.h"
#include "common/env.h"
#include "../parsers_internal.h"
#include <cstdio>
#undef main

namespace
{
int failures = 0;
int checks = 0;

void Check(bool condition, const char *name)
{
    ++checks;
    if ( !condition )
    {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}
}

int main()
{
    using World::Parsers::ParseOrbiterParam;

    // Defaults are vanilla-safe: nothing happens unless a vehicle links the weapon.
    {
        World::TVhclProto vehicle;
        Check(vehicle.orbiter_weapon == 0, "vehicle orbiter link off by default");
        World::TWeaponOrbiterConfig o;
        Check(o.count_min == 1 && o.strike_count_min == 3 && o.strike_energy_mult == 1.0f, "orbiter defaults");
        Check(o.strike_first_delay_max == 0 && o.strike_mode == World::TWeaponOrbiterConfig::STRIKE_ALL_AT_ONCE,
              "launch at once, no delay by default");
        Check(!o.HasRingGuard(), "classic free orbit by default");
    }

    // Ranges, fixed values, reversed ends and invalid values.
    {
        World::TWeaponOrbiterConfig o;
        Check(ParseOrbiterParam("orbiter_count", "3~2", o) && o.count_min == 2 && o.count_max == 3, "count range (reversed ends)");
        Check(ParseOrbiterParam("orbiter_count", "16", o) && o.count_max == 16, "count up to 16");
        Check(ParseOrbiterParam("orbiter_count", "17", o) && o.count_max == 16, "count above 16 rejected");
        Check(ParseOrbiterParam("orbiter_count", "0", o) && o.count_min == 16, "count 0 rejected");
        Check(ParseOrbiterParam("orbiter_strike_count", "5", o) && o.strike_count_min == 5 && o.strike_count_max == 5, "strike count fixed");
        Check(ParseOrbiterParam("orbiter_strike_count", "abc", o) && o.strike_count_min == 5, "strike count garbage rejected");
        Check(ParseOrbiterParam("orbiter_respawn_time", "4000~6000", o) && o.respawn_time_max == 6000, "respawn range");
        Check(ParseOrbiterParam("orbiter_strike_first_delay", "300~500", o) && o.strike_first_delay_max == 500, "first delay range");
        Check(ParseOrbiterParam("orbiter_strike_interval", "50", o) && o.strike_interval_min == 900, "interval below 100 rejected");
        Check(ParseOrbiterParam("orbiter_strike_mode", "one_by_one", o) &&
              o.strike_mode == World::TWeaponOrbiterConfig::STRIKE_ONE_BY_ONE, "strike mode one_by_one");
        Check(ParseOrbiterParam("orbiter_strike_mode", "sometimes", o) &&
              o.strike_mode == World::TWeaponOrbiterConfig::STRIKE_ONE_BY_ONE, "strike mode garbage rejected");
        Check(ParseOrbiterParam("orbiter_guard_radius", "60~90", o) && o.guard_radius_min == 60.0f && o.guard_radius_max == 90.0f, "guard radius range");
        Check(ParseOrbiterParam("orbiter_guard_radius", "0", o) && o.guard_radius_max == 0.0f, "guard radius 0 = automatic");
        Check(ParseOrbiterParam("orbiter_siege_radius", "-5", o) && o.siege_radius_max == 0.0f, "negative siege radius rejected");
        Check(ParseOrbiterParam("orbiter_fade_in", "500~900", o) && o.fade_in == 800, "fade_in is a single value");
        Check(ParseOrbiterParam("orbiter_guard_speed", "0.4", o) && o.guard_speed == 0.4f && o.HasRingGuard(), "guard speed decimals");
        Check(ParseOrbiterParam("orbiter_guard_chaos_factor", "11", o) && o.guard_chaos < 0.0f, "guard chaos above 10 rejected");
        Check(ParseOrbiterParam("orbiter_guard_chaos_factor", "0", o) && o.guard_chaos == 0.0f, "guard chaos 0 = ordered");
        Check(ParseOrbiterParam("orbiter_strike_energy_mult", "1.5", o) && o.strike_energy_mult == 1.5f, "energy mult increasing cascade");
        Check(ParseOrbiterParam("orbiter_strike_energy_mult", "0", o) && o.strike_energy_mult == 1.5f, "energy mult 0 rejected");
        Check(!ParseOrbiterParam("orbiter_strikes", "3", o), "retired orbiter_strikes not consumed");
        Check(!ParseOrbiterParam("orbiter_unknown", "1", o), "unknown key not consumed");
    }

    std::printf("orbiter_parse_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
