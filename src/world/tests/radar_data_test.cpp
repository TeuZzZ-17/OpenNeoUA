// Loads the shipped manifest with the production parser and checks radar stations.
#include "game/yw.h"
#include "game/ypagun.h"
#include "common/env.h"
#include <cmath>
#include <cstdio>
#undef main

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
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *label) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
    };
    const bool loaded = world.LoadProtosScript("data:scripts/Startup.cfg");
    check(loaded, "production loader parses Startup.cfg");
    if (loaded)
    {
        struct Case { int building, vehicle; float speed; };
        for (const auto &c : {Case{54,98,0.35f}, Case{71,168,0.30f},
                              Case{72,169,0.55f}, Case{73,170,0.20f}})
        {
            const auto &b = world.GetBuildProtos()[c.building];
            auto &v = world.GetVhclProtos()[c.vehicle];
            check(!b.Guns.empty() && b.Guns[0].VhclID == c.vehicle,
                  "station uses the faction radar prototype");
            check(v.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_FIRETYPE,-1) ==
                  NC_STACK_ypagun::GUN_TYPE_RADAR, "radar gun type parsed");
            check(std::fabs(v.maxrot - c.speed) < 0.00001f,
                  "faction rotation speed parsed");
            check(v.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_UPANGLE,-1) == 0 &&
                  v.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_DOWNANGLE,-1) == 0 &&
                  v.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_SIDEANGLE,-1) == 3200,
                  "no pitch and full rotation data parsed");
            check(v.radar == 15 && v.energy == 40000 && v.shield == 98,
                  "radar detection and durability preserved");
        }
        check(world.GetVhclProtos()[170].visual_base.normal.find("Taer_Radar_Normal") != std::string::npos,
              "Taerkasten module uses its dedicated model");
        check(world.GetVhclProtos()[63].visual_base.dead ==
              R"(Data\Models\Base\Original_Refined\Hourglass\Hourglass_Dead\Hourglass_Dead.BASE)" &&
              world.GetVhclProtos()[63].vp_normal == 271,
              "Hourglass overrides only the dead model");
        check(world.GetVhclProtos()[66].visual_base.dead ==
              "Data/Models/Base/New/Static/Static_Dead/Static_Dead.BASE" &&
              world.GetVhclProtos()[66].vp_normal == 184,
              "Static overrides only the dead model");
    }
    std::printf("RADAR DATA CHECKS=%d FAILURES=%d\n",checks,failures);
    return failures ? 1 : 0;
}
