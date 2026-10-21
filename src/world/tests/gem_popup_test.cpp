// Checks when the classic GEM popup is visible: vanilla 10 s lifetime, vehicle/weapon/building
// selection, and the shorter game-clock lifetime used by the optional slow motion.
#include "game/yw.h"
#include "system/inivals.h"
#include <cstdio>
#undef main
int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what); } };

    NC_STACK_ypaworld world;
    world._vhclProtos.resize(4);
    world._vhclProtos[2].weapon = 3;

    int32_t vehicle = -1;

    world._upgradeId = 0;
    world._upgradeTimeStamp = 1000;
    world._upgradeVehicleId = 1;
    world._upgradeWeaponId = 0;
    world._upgradeBuildId = 0;

    world._timeStamp = 10999;
    check(world.IsGemPopupVisible(&vehicle) && vehicle == 1, "vehicle popup visible inside the vanilla 10000 ms");
    world._timeStamp = 11000;
    check(!world.IsGemPopupVisible(), "popup hidden after 10000 ms");

    world._timeStamp = 2000;
    world._upgradeId = -1;
    check(!world.IsGemPopupVisible(), "no popup without an unlocked GEM");
    world._upgradeId = 0;

    world._upgradeVehicleId = 0;
    check(!world.IsGemPopupVisible(), "nothing to show");

    world._upgradeWeaponId = 3;
    check(world.IsGemPopupVisible(&vehicle) && vehicle == 2, "weapon GEM shows the vehicle that fires it");
    world._upgradeWeaponId = 9;
    check(!world.IsGemPopupVisible(), "weapon without a vehicle");

    world._upgradeWeaponId = 0;
    world._upgradeBuildId = 5;
    check(world.IsGemPopupVisible(&vehicle) && vehicle == 0, "building GEM visible without vehicle");

    // Slow motion: the game clock runs at 20%, so the popup lasts 2000 game ms (10000 real ms).
    System::IniConf::GameGemUnlockSlowMotion.Value = true;
    world._timeStamp = 2999;
    check(world.IsGemPopupVisible(), "slow motion popup still visible");
    world._timeStamp = 3000;
    check(!world.IsGemPopupVisible(), "slow motion popup ends on the scaled lifetime");
    System::IniConf::GameGemUnlockSlowMotion.Value = false;


    // Popup anchor: vanilla -0.5 by default, valid values are clamped, invalid ones keep the default.
    check(world.GetGemPopupY() == -0.5f, "default popup position is vanilla");
    System::IniConf::GameGemUnlockPopupY.Value = std::string("-0.04");
    check(world.GetGemPopupY() == -0.04f, "authored popup position");
    System::IniConf::GameGemUnlockPopupY.Value = std::string("7");
    check(world.GetGemPopupY() == 1.0f, "popup position is clamped");
    System::IniConf::GameGemUnlockPopupY.Value = std::string("abc");
    check(world.GetGemPopupY() == -0.5f, "invalid popup position keeps vanilla");
    System::IniConf::GameGemUnlockPopupY.Value = std::string("-0.5");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
