// Integration test linked with the production world and actor objects.
#include "game/yw.h"
#include "game/ypabact.h"
#include "system/inivals.h"
#include <cstdio>
#undef main

struct ProductionWorld : NC_STACK_ypaworld
{
    ProductionWorld()
    {
        _vhclProtos.resize(6);
        _vhclProtos[5].max_active_at_once = 1;
    }
    void Cheat(bool enabled) { _debugHostStationCheat = enabled; }
    int AuthoredLimit() const { return _vhclProtos[5].max_active_at_once; }
};

int main()
{
    ProductionWorld world;
    NC_STACK_ypabact host, produced;
    world._userRobo = &host;
    host._owner = 1;
    produced._owner = 1;
    produced._vehicleID = 5;
    produced._status = BACT_STATUS_NORMAL;
    produced._isGenesisProduced = true;
    produced._kidRef = world._unitsList.push_back(&produced);
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *label) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
    };
    System::IniConf::GameNewDebug.Value = std::string("yes");
    check(world.IsVehicleProductionLimitReached(1, 5), "normal production respects the limit");
    world.Cheat(true);
    check(world.GetVehicleProductionLimitRemaining(1, 5) == -1, "Host cheat permits unlimited player production");
    check(!world.IsVehicleProductionLimitReached(1, 5), "runtime creation and UI see the same bypass");
    check(world.AuthoredLimit() == 1, "cheat does not alter prototype or GEM progression");
    world.Cheat(false);
    check(world.IsVehicleProductionLimitReached(1, 5), "switching cheat off restores the limit immediately");
    check(produced._status == BACT_STATUS_NORMAL, "existing units survive when the cheat ends");
    check(world.GetVehicleProductionLimitRemaining(2, 5) == -1, "AI production remains unrestricted");
    world._isNetGame = true;
    check(world.GetVehicleProductionLimitRemaining(1, 5) == -1, "network behavior stays unchanged");
    world._isNetGame = false;
    check(world.GetVehicleProductionLimitRemaining(1, 9999) == -1, "invalid prototype stays safe");
    world._userRobo = nullptr;
    System::IniConf::GameNewDebug.Reset();
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
