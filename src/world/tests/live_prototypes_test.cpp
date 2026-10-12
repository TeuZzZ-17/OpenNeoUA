// Native probe for the public prototype loader and shipped Startup.cfg scripts.
#include "../../yw.h"
#include "../../env.h"
#include <cstdio>
#undef main

namespace
{
int failures = 0;
int checks = 0;

void Check(bool condition, const char *name)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}
}

int main()
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
    if (loaded)
    {
        const auto &weapon = world.GetWeaponsProtos()[3];
        Check(weapon.blast_radius == 160.0f && weapon.blast_energy == 20000 &&
              weapon.blast_push == 4.0f && weapon.blast_falloff == 1,
              "live Weapon blast fields");

        const auto &vehicle = world.GetVhclProtos()[1];
        Check(vehicle.speech_voicepack ==
              "Data/Sounds/Speech/Voicepacks/resistance-tank-light",
              "live direct voicepack path");
        const auto &mgunVehicle = world.GetVhclProtos()[3];
        Check(mgunVehicle.mgun_energy_set && mgunVehicle.mgun_energy == 1440.0f &&
              mgunVehicle.mgun_shot_time == 100,
              "live MGUN shot energy and shot time");

        const auto debuff = world._debuffProfiles.find(2);
        Check(debuff != world._debuffProfiles.end() && debuff->second.allow_on_children,
              "live generated-child Debuff propagation");
    }

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
