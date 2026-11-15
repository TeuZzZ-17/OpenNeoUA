// Visual source keys without the word "mesh": new_cloud 3ds / base / vp, tracer_3ds.
// The old names (mesh_3ds, mesh_tracer_*) are gone and must not be read any more.
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
bool ParseClouds(std::map<int32_t, World::TAtmosphericFXProfile> &profiles, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::AtmosphericFXProfileParser(&profiles)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main() {
    std::map<int32_t, World::TAtmosphericFXProfile> clouds;
    ParseClouds(clouds, {
        "new_cloud 1", "3ds = Data/Models/3ds/Spore.3ds", "count = 10", "end",
        "new_cloud 2", "base = Data/Models/Base/New/X/X.BASE", "count = 10", "end",
        "new_cloud 3", "vp = 12", "count = 10", "end",
        "new_cloud 4", "mesh_3ds = Data/Models/3ds/Spore.3ds", "count = 10", "end",
        "new_cloud 5", "3ds = Data/Models/3ds/Spore.3ds", "end"
    });
    Check(clouds.count(1) && clouds[1].mesh3ds == "Data/Models/3ds/Spore.3ds", "cloud 3ds key");
    Check(clouds.count(2) && clouds[2].basePath == "Data/Models/Base/New/X/X.BASE", "cloud base key");
    Check(clouds.count(3) && clouds[3].vp == 12, "cloud vp key");
    Check(!clouds.count(4), "old mesh_3ds is not read (cloud without a source is ignored)");
    Check(!clouds.count(5), "cloud without count is ignored");

    WorldProbe w;
    const char *box = "ai_fire_trigger_box_forward_xyz = 80|80|1000";
    Check(ParseWeapons(w, {
        "new_weapon 1", "model = laser", "tracer_3ds = Data/Models/3ds/Tracer.3ds", "tracer_size_xyz = 1|1|80", box, "end"
    }), "tracer weapon parsed");
    // The old key is an unknown key now: a script error, and no tracer.
    Check(!ParseWeapons(w, {
        "new_weapon 2", "model = laser", "mesh_tracer_path = Data/Models/3ds/Tracer.3ds", box, "end"
    }), "old mesh_tracer_path is a script error");
    Check(w._weaponProtos[1].tracer.enabled && w._weaponProtos[1].tracer.path_3ds == "Data/Models/3ds/Tracer.3ds",
          "tracer_3ds key");
    Check(!w._weaponProtos[2].tracer.enabled && w._weaponProtos[2].tracer.path_3ds.empty(),
          "old mesh_tracer_path is not read");
    std::printf("VISUAL SOURCE KEYS: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
