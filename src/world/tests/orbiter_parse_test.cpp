// OpenNeoUA Orbiter: orbiter_* parsing rules and the shipped Slime Lord spores.
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
        Check(o.count_min == 1 && o.strikes_min == 3 && o.energy_mult == 1.0f, "orbiter defaults");
    }

    // Ranges, fixed values, reversed ends and invalid values.
    {
        World::TWeaponOrbiterConfig o;
        Check(ParseOrbiterParam("orbiter_count", "3~2", o) && o.count_min == 2 && o.count_max == 3, "count range (reversed ends)");
        Check(ParseOrbiterParam("orbiter_count", "9", o) && o.count_min == 2 && o.count_max == 3, "count above 8 rejected");
        Check(ParseOrbiterParam("orbiter_count", "0", o) && o.count_min == 2, "count 0 rejected");
        Check(ParseOrbiterParam("orbiter_strikes", "5", o) && o.strikes_min == 5 && o.strikes_max == 5, "strikes fixed");
        Check(ParseOrbiterParam("orbiter_strikes", "abc", o) && o.strikes_min == 5, "strikes garbage rejected");
        Check(ParseOrbiterParam("orbiter_respawn_time", "4000~6000", o) && o.respawn_time_max == 6000, "respawn range");
        Check(ParseOrbiterParam("orbiter_first_strike_delay", "0", o) && o.first_strike_delay_min == 0, "first strike delay 0 allowed");
        Check(ParseOrbiterParam("orbiter_strike_interval", "50", o) && o.strike_interval_min == 900, "interval below 100 rejected");
        Check(ParseOrbiterParam("orbiter_guard_radius", "60~90", o) && o.guard_radius_min == 60.0f && o.guard_radius_max == 90.0f, "guard radius range");
        Check(ParseOrbiterParam("orbiter_guard_radius", "0", o) && o.guard_radius_max == 0.0f, "guard radius 0 = automatic");
        Check(ParseOrbiterParam("orbiter_siege_radius", "-5", o) && o.siege_radius_max == 0.0f, "negative siege radius rejected");
        Check(ParseOrbiterParam("orbiter_fade_in", "500~900", o) && o.fade_in == 800, "fade_in is a single value");
        Check(ParseOrbiterParam("orbiter_energy_mult", "1.5", o) && o.energy_mult == 1.5f, "energy_mult increasing cascade");
        Check(ParseOrbiterParam("orbiter_energy_mult", "0.4", o) && o.energy_mult == 0.4f, "energy_mult decreasing cascade");
        Check(ParseOrbiterParam("orbiter_energy_mult", "0", o) && o.energy_mult == 0.4f, "energy_mult 0 rejected");
        Check(!ParseOrbiterParam("orbiter_unknown", "1", o), "unknown key not consumed");
    }

    // Shipped scripts: weapon 11 Slime_Spore carried by the Slime Lord (vehicle 72).
    {
        FSMgr::iDir::setBaseDir(".");
        Common::Env.SetPrefix("data", "Data");
        Common::Env.SetPrefix("rsrc", "data:");
        Common::Env.SetPrefix("scripts", "data:scripts");

        NC_STACK_ypaworld world;
        world._vhclProtos.resize(256);
        world._weaponProtos.resize(256);
        world._buildProtos.resize(256);
        world._roboProtos.resize(1);

        const bool loaded = world.LoadProtosScript("data:scripts/Startup.cfg");
        Check(loaded, "live Startup.cfg parsed");
        if ( loaded )
        {
            const World::TWeapProto &spore = world.GetWeaponsProtos()[11];
            Check(spore.IsOrbiter(), "weapon 11 is model = orbiter");
            Check(spore.orbiter.count_min == 2 && spore.orbiter.count_max == 3, "spore count 2~3");
            Check(spore.orbiter.strikes_min == 3 && spore.orbiter.energy_mult == 1.25f, "spore strikes and cascade");
            Check(world.GetVhclProtos()[72].orbiter_weapon == 11, "Slime Lord carries the spores");
            Check(world.GetVhclProtos()[72].weapon == 72, "Slime Lord primary weapon unchanged");
        }
    }

    std::printf("orbiter_parse_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
