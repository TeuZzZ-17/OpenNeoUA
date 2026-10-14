// Native probe for the public prototype loader and shipped Startup.cfg scripts.
#include "game/yw.h"
#include "common/env.h"
#include <algorithm>
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

        // Every shipped Ground Decal block carries the smoke_* keys of the script.
        int decals = 0;
        int smoking = 0;
        const World::TGroundDecalSmoke *sample = nullptr;
        const auto scan = [&](const std::vector<World::TChainFXConfig> &chain)
        {
            for (const World::TChainFXConfig &fx : chain)
            {
                if (fx.mode != World::TChainFXConfig::MODE_GROUND_DECAL)
                    continue;
                ++decals;
                if (fx.ground_decal_smoke.enabled)
                {
                    ++smoking;
                    sample = &fx.ground_decal_smoke;
                }
            }
        };
        for (const auto &proto : world.GetWeaponsProtos())
            scan(proto.chain_fx);
        for (const auto &proto : world.GetVhclProtos())
            scan(proto.chain_fx);
        Check(decals > 0 && decals == smoking, "live Ground Decals (ground_decal_id = 1) all have smoke");
        const auto isDecal = [](const World::TChainFXConfig &fx) { return fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL; };
        const auto &decalVehicle = world.GetVhclProtos()[1];
        const auto &decalWeapon = world.GetWeaponsProtos()[3];
        Check(decalVehicle.ground_decal_id == 1 &&
              std::count_if(decalVehicle.chain_fx.begin(), decalVehicle.chain_fx.end(), isDecal) == 1,
              "live vehicle links decal 1");
        Check(decalWeapon.ground_decal_id == 1 && decalWeapon.blast_radius > 0.0f &&
              std::count_if(decalWeapon.chain_fx.begin(), decalWeapon.chain_fx.end(), isDecal) == 1,
              "live weapon links decal 1");
        Check(sample && sample->count_min == 10 && sample->count_max == 30 &&
              sample->duration_min == 20000 && sample->duration_max == 25000 &&
              sample->fade_out == 20000 &&
              sample->size_min == 35.0f && sample->size_max == 50.0f &&
              sample->puff_time_min == 4000 && sample->puff_time_max == 7500 &&
              sample->intensity == 120.0f && sample->tint.a > 0.58f && sample->tint.a < 0.59f,
              "live smoke_* values parsed");

        const auto debuff = world._debuffProfiles.find(2);
        Check(debuff != world._debuffProfiles.end() && debuff->second.allow_on_children,
              "live generated-child Debuff propagation");
    }

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
