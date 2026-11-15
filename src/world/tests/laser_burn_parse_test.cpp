// laser_burn_decal_frequency: parsing limits. Missing / zero / negative = off (vanilla-safe).
#include "game/ypabact_internal.h"
#include "game/yw_internal.h"
#include "../parsers.h"
#include <cstdio>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

struct WorldProbe : NC_STACK_ypaworld {
    NC_STACK_base visual;
    WorldProbe() {
        _vhclModels.resize(256, &visual);
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    void ypaworld_func180(yw_arg180 *) override {}
    void ypaworld_func149(ypaworld_arg136 *arg) override { arg->isect = false; }
};
bool ParseWeapons(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::WeaponProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main() {
    WorldProbe w;
    const char *box = "ai_fire_trigger_box_forward_xyz = 80|80|1000";
    Check(ParseWeapons(w, {
        "new_weapon 1", "model = laser", "laser_burn_decal_frequency = 5", box, "end",
        "new_weapon 2", "model = laser", "laser_burn_decal_frequency = 0.5", box, "end",
        "new_weapon 3", "model = laser", "laser_burn_decal_frequency = 0.001", box, "end",
        "new_weapon 4", "model = laser", "laser_burn_decal_frequency = 99999", box, "end",
        "new_weapon 5", "model = laser", box, "end",
        "new_weapon 6", "model = laser", "laser_burn_decal_frequency = abc", box, "end",
        "new_weapon 7", "model = laser", "laser_burn_decal_frequency = 0", box, "end",
        "new_weapon 8", "model = laser", "laser_burn_decal_frequency = -5", box, "end"
    }), "fixtures parsed");
    Check(w._weaponProtos[1].laser_burn_decal_frequency == 5, "integer frequency parsed");
    Check(w._weaponProtos[2].laser_burn_decal_frequency == 0.5f, "fractional frequency parsed");
    Check(w._weaponProtos[3].laser_burn_decal_frequency == 0.05f, "floor of 0.05");
    Check(w._weaponProtos[4].laser_burn_decal_frequency == 10, "ceiling of 10");
    Check(w._weaponProtos[5].laser_burn_decal_frequency == 0, "missing = off");
    Check(w._weaponProtos[6].laser_burn_decal_frequency == 0, "not a number = off");
    Check(w._weaponProtos[7].laser_burn_decal_frequency == 0, "zero = off");
    Check(w._weaponProtos[8].laser_burn_decal_frequency == 0, "negative = off");
    std::printf("LASER BURN PARSE: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
