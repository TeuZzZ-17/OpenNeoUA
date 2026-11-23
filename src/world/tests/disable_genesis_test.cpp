// Vehicle disable_genesis: parser, factory copy and the instant creation phase.
#include "game/yw.h"
#include "game/ypatank.h"
#include "game/ypaflyer.h"
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

    // Parser: 1 disables creation+death, at_death disables death only; missing, 0 and junk keep vanilla.
    Check(Parse(w, {"new_vehicle 3", "model = tank", "end"}) &&
          w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_NONE,
          "absent keeps vanilla");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = 1", "end"}) &&
          w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_ALL,
          "1 disables all");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = at_death", "end"}) &&
          w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_AT_DEATH,
          "at_death disables death only");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = 0", "end"}) &&
          w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_NONE,
          "0 restores");
    Check(Parse(w, {"modify_vehicle 3", "disable_genesis = AT_DEATH", "end"}) &&
          w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_AT_DEATH,
          "AT_DEATH is case-insensitive");
    for (const char *value : {"2", "-1", "abc", ""}) {
        Check(Parse(w, {"modify_vehicle 3", std::string("disable_genesis = ") + value, "end"}) &&
              w._vhclProtos[3].disable_genesis == World::TVhclProto::DISABLE_GENESIS_NONE,
              "invalid value keeps vanilla");
    }

    // air_dead parser: only explicit 1 enables.
    Check(!w._vhclProtos[3].air_dead, "air_dead absent keeps vanilla");
    Check(Parse(w, {"modify_vehicle 3", "air_dead = 1", "end"}) && w._vhclProtos[3].air_dead,
          "air_dead = 1 enables");
    Check(Parse(w, {"modify_vehicle 3", "air_dead = 0", "end"}) && !w._vhclProtos[3].air_dead,
          "air_dead = 0 restores");

    // Factory: the live unit gets the switch.
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypatank>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypaflyer>());
    NC_STACK_base visual;
    w._vhclModels.resize(256, &visual);
    for (uint8_t mode : {World::TVhclProto::DISABLE_GENESIS_NONE,
                         World::TVhclProto::DISABLE_GENESIS_ALL,
                         World::TVhclProto::DISABLE_GENESIS_AT_DEATH}) {
        auto &proto = w._vhclProtos[2]; proto.model_id = BACT_TYPES_TANK; proto.weapon = -1;
        proto.scale_fx_pXX.fill(0);
        proto.disable_genesis = mode;
        proto.disable_collision = true;   // the probe world has no terrain to collide with
        ypaworld_arg146 request; request.vehicle_id = 2; request.pos = vec3d(2400, -100, -2400);
        auto *spawned = w.ypaworld_func146(&request);
        Check(spawned && spawned->_disable_genesis == mode, "factory copies disable_genesis");
        if (spawned) {
            const bool expectNoCreation = (mode == World::TVhclProto::DISABLE_GENESIS_ALL);
            const bool expectNoDeath = (mode != World::TVhclProto::DISABLE_GENESIS_NONE);
            Check(spawned->DisableCreationGenesis() == expectNoCreation, "creation genesis helper");
            Check(spawned->DisableDeathGenesis() == expectNoDeath, "death genesis helper");

            // Creation: only mode 1 ends creation on the first update; at_death and 0 keep it running.
            spawned->_status = BACT_STATUS_CREATE;
            spawned->_scale_time = 5000;
            update_msg tick; tick.frameTime = 20; tick.gTime = 20;
            spawned->CreationTimeUpdate(&tick);
            Check((spawned->_status == BACT_STATUS_NORMAL) == expectNoCreation,
                  "creation ends at once only when creation genesis is disabled");

            // Death: mode 1 and at_death skip death plasma; mode 0 starts death plasma.
            spawned->_status = BACT_STATUS_DEAD;
            spawned->_status_flg |= (BACT_STFLAG_LAND | BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2);
            spawned->_owner = 1;
            spawned->_vp_genesis = &visual;
            spawned->_yls_time = 0;
            spawned->DeadTimeUpdate(&tick);
            const bool plasmaStarted = (spawned->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) != 0;
            Check(plasmaStarted == !expectNoDeath,
                  "death plasma starts only when death genesis is enabled");
            if (plasmaStarted)
                spawned->Delete();
        }
    }

    // air_dead: flying unit enters DEATH2 in mid-air without CrashOrLand; ground tank ignores air_dead.
    for (int modelId : {BACT_TYPES_FLYER, BACT_TYPES_TANK}) {
        auto &proto = w._vhclProtos[4]; proto.model_id = modelId; proto.weapon = -1;
        proto.scale_fx_pXX.fill(0);
        proto.air_dead = true;
        proto.disable_genesis = World::TVhclProto::DISABLE_GENESIS_AT_DEATH;
        proto.disable_collision = true;
        ypaworld_arg146 request; request.vehicle_id = 4; request.pos = vec3d(2400, -500, -2400);
        auto *spawned = w.ypaworld_func146(&request);
        Check(spawned && spawned->_air_dead, "factory copies air_dead");
        if (spawned) {
            spawned->_status = BACT_STATUS_DEAD;
            spawned->_status_flg = BACT_STFLAG_DEATH1;
            spawned->_owner = 1;
            spawned->_vp_genesis = &visual;
            spawned->_yls_time = 3000;
            update_msg tick; tick.frameTime = 20; tick.gTime = 20;
            spawned->DeadTimeUpdate(&tick);
            const bool isFlyer = (modelId == BACT_TYPES_FLYER);
            Check(((spawned->_status_flg & BACT_STFLAG_DEATH2) != 0) == isFlyer,
                  "air_dead triggers DEATH2 in mid-air only for flying units");
            Check(!isFlyer || (spawned->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) == 0,
                  "air_dead + disable_genesis = at_death skips mid-air death plasma");
            spawned->Delete();
        }
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
