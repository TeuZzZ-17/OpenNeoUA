// Integration probe linked against production actor/world objects.
#include "../../yw.h"
#include "../../ypabact.h"
#include "../../yparobo.h"
#include "../../system/inivals.h"
#include <cstdio>
#undef main

namespace {
struct TestWorld : NC_STACK_ypaworld {
    void ypaworld_func159(yw_arg159 *) override {}
    void SpectatorProto(int id) { _spectatorVehicleProtoID = id; }
    bool Frozen() const { return _debugGameplayFrozen; }
    using NC_STACK_ypaworld::HandleDebugTimeHotkeys;
};
struct Actor : NC_STACK_ypabact {
    void Bind(NC_STACK_ypaworld &world) { _world = &world; }
};
struct Host : NC_STACK_yparobo {
    void Bind(NC_STACK_ypaworld &world) { _world = &world; }
    using NC_STACK_yparobo::sub_4F4C6C;
    using NC_STACK_yparobo::yparobo_func70__sub6__sub5;
};
int checks = 0, failures = 0;
void Check(bool ok, const char *label) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
}
}

int main() {
    TestWorld world;
    Actor vehicle, child, hostGun, squad, enemy;
    for (Actor *unit : {&vehicle, &child, &hostGun, &squad, &enemy})
        unit->Bind(world);
    vehicle._owner = 1;
    enemy._owner = 2;
    child._parent = &vehicle;
    child._isUnitGunChild = true;
    hostGun._parent = &vehicle;
    hostGun._bact_type = BACT_TYPES_GUN;
    squad._parent = &vehicle;
    squad._bact_type = BACT_TYPES_TANK;

    System::IniConf::GameNewDebug.Value = std::string("no");
    Check(!vehicle.IsIgnoredByAI() && vehicle.CanBeSeenByAIOrRadar(), "default vanilla visible");
    vehicle._debugIgnoredByAI = true;
    Check(!vehicle.IsIgnoredByAI(), "debug off disables exclusion");
    System::IniConf::GameNewDebug.Value = std::string("yes");
    Check(vehicle.IsIgnoredByAI() && !vehicle.CanBeSeenByAIOrRadar(), "debug on excludes vehicle");
    Check(child.IsIgnoredByAI() && !child.CanBeSeenByAIOrRadar(), "mounted gun inherits exclusion");
    Check(hostGun.IsIgnoredByAI(), "Host turret inherits exclusion");
    Check(!squad.IsIgnoredByAI(), "squad does not inherit Host exclusion");
    Check(!vehicle.IsInvisibleUnrevealed(), "debug does not enable visual stealth");
    Check(!vehicle._invulnerable, "debug does not enable invulnerability");
    vehicle.RevealInvisibleOnAttack();
    Check(vehicle.IsIgnoredByAI(), "attacking does not reveal debug vehicle");

    enemy._primTtype = BACT_TGT_TYPE_UNIT;
    enemy._primT.pbact = &vehicle;
    enemy._secndTtype = BACT_TGT_TYPE_UNIT;
    enemy._secndT.pbact = &child;
    bact_arg110 target{BACT_TGT_TYPE_UNIT, 0};
    Check(enemy.TargetAssess(&target) == NC_STACK_ypabact::TA_CANCEL, "primary enemy lock cancelled");
    target.priority = 1;
    Check(enemy.TargetAssess(&target) == NC_STACK_ypabact::TA_CANCEL, "secondary attached-gun lock cancelled");
    cellArea visibleCell;
    visibleCell.AddToViewMask(vehicle._owner);
    vehicle._pSector = &visibleCell;
    enemy._owner = vehicle._owner;
    enemy._aggr = 50;
    target.priority = 0;
    Check(enemy.TargetAssess(&target) == NC_STACK_ypabact::TA_IGNORE, "allied follow target remains valid");
    enemy._owner = 2;

    Host enemyHost;
    enemyHost.Bind(world);
    enemyHost._owner = 2;
    enemyHost._kidRef = world._unitsList.push_back(&enemyHost);
    vehicle._kidRef = world._unitsList.push_back(&vehicle);
    vehicle._commandID = 123;
    Check(enemyHost.sub_4F4C6C(&vehicle) < 0, "Host strategic score rejects ignored vehicle");
    int command = 0;
    Common::Point cell;
    Check(enemyHost.yparobo_func70__sub6__sub5(&command, &cell) < 0, "Host nearby scan rejects ignored vehicle");
    setTarget_msg lookup;
    lookup.priority = vehicle._commandID;
    Check(!enemyHost.yparobo_func132(&lookup), "Host cannot resolve ignored enemy command");
    enemyHost._owner = vehicle._owner;
    lookup.priority = vehicle._commandID;
    Check(enemyHost.yparobo_func132(&lookup) && lookup.tgt.pbact == &vehicle, "allied commands remain usable");
    enemyHost._owner = 2;
    vehicle._debugIgnoredByAI = false;
    command = 0;
    Check(enemyHost.yparobo_func70__sub6__sub5(&command, &cell) == 500 && command == 123, "toggle off restores Host nearby targeting");
    vehicle._kidRef.Detach();
    enemyHost._kidRef.Detach();

    vehicle._debugIgnoredByAI = false;
    Check(vehicle.CanBeSeenByAIOrRadar() && !child.IsIgnoredByAI(), "toggle off restores vehicle and attachment");
    vehicle._invisibleUnrevealed = true;
    Check(!vehicle.CanBeSeenByAIOrRadar(), "authored stealth remains independent");
    vehicle._invisibleUnrevealed = false;
    vehicle._debugIgnoredByAI = true;
    vehicle.Renew();
    Check(!vehicle._debugIgnoredByAI && vehicle.CanBeSeenByAIOrRadar(), "recycled vehicle clears debug state");

    world.SpectatorProto(250);
    vehicle._vehicleID = 250;
    System::IniConf::GameSpectatorMode.Value = true;
    Check(vehicle.IsIgnoredByAI() && !vehicle.CanBeSeenByAIOrRadar(), "Spectator shares AI exclusion without debug flag");
    System::IniConf::GameSpectatorMode.Value = false;
    Check(!vehicle.IsIgnoredByAI(), "disabled Spectator retains vanilla behavior");
    System::IniConf::GameSpectatorMode.Reset();

    TInputState input;
    input.KbdLastHit = Input::KC_F2;
    input.HotKeyID = 21;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(world.IsDebugGameplaySlowMotionEnabled() && input.HotKeyID == -1, "F2 slow motion consumes vanilla binding");
    world.HandleDebugTimeHotkeys(&input, true);
    Check(!world.IsDebugGameplaySlowMotionEnabled(), "F2 toggles slow motion off");
    input.KbdLastHit = Input::KC_F3;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(world.IsDebugGameplayFastMotionEnabled() && !world.IsDebugGameplaySlowMotionEnabled(), "F3 enables +80 percent time");
    input.KbdLastHit = Input::KC_F2;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(world.IsDebugGameplaySlowMotionEnabled() && !world.IsDebugGameplayFastMotionEnabled(), "slow and fast modes are exclusive");
    input.KbdLastHit = Input::KC_F4;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(world.Frozen(), "F4 freezes game time");
    world.HandleDebugTimeHotkeys(&input, true);
    Check(!world.Frozen(), "F4 unfreezes game time");
    input.KbdLastHit = Input::KC_F2;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(!world.IsDebugGameplaySlowMotionEnabled(), "F2 slow motion off before vanilla test");
    input.KbdLastHit = Input::KC_F2;
    input.HotKeyID = 21;
    world.HandleDebugTimeHotkeys(&input, false);
    Check(!world.IsDebugGameplaySlowMotionEnabled() && input.HotKeyID == 21, "debug off preserves vanilla F2");
    world._isNetGame = true;
    world.HandleDebugTimeHotkeys(&input, true);
    Check(!world.IsDebugGameplaySlowMotionEnabled() && input.HotKeyID == 21, "netgame time controls preserve vanilla binding");
    world._isNetGame = false;
    System::IniConf::GameNewDebug.Reset();
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
