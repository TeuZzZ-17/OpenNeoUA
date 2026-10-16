// Vehicle disable_genesis: parser, factory copy and the instant creation phase.
#include "game/yw.h"
#include "game/ypatank.h"
#include "game/ypabact_internal.h"
#include "nucleus/base.h"
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
    WorldProbe() {
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    void ypaworld_func180(yw_arg180 *) override {}
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 0; }
};
bool Parse(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main() {
    WorldProbe w;

    // Parser: only an explicit 1 disables; missing, 0 and junk keep the vanilla Genesis.
    Check(Parse(w, {"new_vehicle 3", "model = tank", "end"}) && !w._vhclProtos[3].disable_genesis,
          "absent keeps vanilla");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = 1", "end"}) && w._vhclProtos[3].disable_genesis,
          "1 disables");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = 0", "end"}) && !w._vhclProtos[3].disable_genesis,
          "0 restores");
    for (const char *value : {"2", "-1", "abc", ""}) {
        Check(Parse(w, {"modify_vehicle 3", std::string("disable_genesis = ") + value, "end"}) &&
              !w._vhclProtos[3].disable_genesis, "invalid value keeps vanilla");
    }

    // Factory: the live unit gets the switch.
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypatank>());
    NC_STACK_base visual;
    w._vhclModels.resize(256, &visual);
    for (bool disabled : {true, false}) {
        auto &proto = w._vhclProtos[2]; proto.model_id = BACT_TYPES_TANK; proto.weapon = -1;
        proto.scale_fx_pXX.fill(0);
        proto.disable_genesis = disabled;
        proto.disable_collision = true;   // the probe world has no terrain to collide with
        ypaworld_arg146 request; request.vehicle_id = 2; request.pos = vec3d(2400, -100, -2400);
        auto *spawned = w.ypaworld_func146(&request);
        Check(spawned && spawned->_disable_genesis == disabled, "factory copies disable_genesis");
        if (spawned) {
            // Creation: with the switch the phase ends on the first update, without it it lasts.
            spawned->_status = BACT_STATUS_CREATE;
            spawned->_scale_time = 5000;
            update_msg tick; tick.frameTime = 20; tick.gTime = 20;
            spawned->CreationTimeUpdate(&tick);
            Check((spawned->_status == BACT_STATUS_NORMAL) == disabled,
                  "creation ends at once only when genesis is disabled");
            spawned->Delete();
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
