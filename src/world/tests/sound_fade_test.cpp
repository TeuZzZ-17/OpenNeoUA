// Native probe: snd_/pal_/shk_<event>_fade_inout of the shipped scripts and the parser rules.
// Run from Data: python src/world/tests/run_native_probe.py src/world/tests/sound_fade_test.cpp
#include "game/yw.h"
#include "common/env.h"
#include <cstdio>
#undef main

namespace
{
int failures = 0;
int checks = 0;
int soundsSeen = 0;
int palSeen = 0;
int shkSeen = 0;

void Check(bool condition, const char *name)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL %s\n", name);
    }
}

// Every active package of an event must carry the shipped 150|300 fade.
void CheckEvent(const World::TVhclSound &fx, const char *owner, int id, int event)
{
    char label[160];
    if ( fx.MainSample.Name.size() && fx.MainSample.Name != "0" )
    {
        ++soundsSeen;
        std::snprintf(label, sizeof(label), "%s %d event %d snd fade", owner, id, event);
        Check(fx.fade_in == 20 && fx.fade_out == 100, label);
    }
    if ( fx.sndPrm.slot > 0 )
    {
        ++palSeen;
        std::snprintf(label, sizeof(label), "%s %d event %d pal fade", owner, id, event);
        Check(fx.sndPrm.fade_in == 150 && fx.sndPrm.fade_out == 300, label);
    }
    if ( fx.sndPrm_shk.slot > 0 )
    {
        ++shkSeen;
        std::snprintf(label, sizeof(label), "%s %d event %d shk fade", owner, id, event);
        Check(fx.sndPrm_shk.fade_in == 150 && fx.sndPrm_shk.fade_out == 300, label);
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
        const auto &vehicles = world.GetVhclProtos();
        for (size_t id = 0; id < vehicles.size(); ++id)
            // Events 13+ (handbrake, pickup, zoom step) come from Nucleus/ini, not from fx keys.
            for (size_t event = 0; event < vehicles[id].sndFX.size() && event < 13; ++event)
                CheckEvent(vehicles[id].sndFX[event], "vehicle", (int)id, (int)event);

        const auto &weapons = world.GetWeaponsProtos();
        for (size_t id = 0; id < weapons.size(); ++id)
        {
            for (size_t event = 0; event < weapons[id].sndFXes.size(); ++event)
                CheckEvent(weapons[id].sndFXes[event], "weapon", (int)id, (int)event);
            if ( weapons[id].shk_launch_player.slot > 0 )
            {
                ++shkSeen;
                Check(weapons[id].shk_launch_player.fade_in == 150 &&
                      weapons[id].shk_launch_player.fade_out == 300, "player launch shake fade");
            }
        }

        for (size_t id = 0; id < world._buildProtos.size(); ++id)
            if ( world._buildProtos[id].genesis_effects_defined )
                CheckEvent(world._buildProtos[id].GenesisFX, "building", (int)id, 3);

        std::printf("events with fade: snd %d, pal %d, shk %d\n", soundsSeen, palSeen, shkSeen);
        Check(soundsSeen > 100 && palSeen > 50 && shkSeen > 50, "the shipped scripts define many fades");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
