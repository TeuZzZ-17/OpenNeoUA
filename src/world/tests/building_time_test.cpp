// Construction timing: real parser and runtime regression checks.
#include "game/yw.h"
#include "game/ypatank.h"
#include "nucleus/base.h"
#include "../parsers.h"
#include <cstdio>
#undef main
extern int yw_GetAutomaticGenesisPitch(int energy);
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
bool Parse(WorldProbe &w, const std::string &kind, const std::string &value) {
    std::printf("Parse %s = %s\n", kind.c_str(), value.c_str());
    ScriptParser::HandlersList handlers;
    if (kind == "vehicle") handlers.push_back(new World::Parsers::VhclProtoParser(&w));
    else handlers.push_back(new World::Parsers::BuildProtoParser(&w));
    return ScriptParser::ParseStringList({"new_" + kind + " 3", "model = " + (kind == "vehicle" ? std::string("tank") : std::string("building")), "building_time = " + value, "end"}, handlers, 0);
}
}
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("Starting construction probe");
    WorldProbe w;
    std::puts("World ready");
    Check(Parse(w, "vehicle", "4.4"), "vehicle accepts building_time");
    Check(Parse(w, "building", "0.4"), "building accepts building_time");

    Check(w._vhclProtos[3].building_time == 4.4f, "vehicle stores seconds");
    Check(w._buildProtos[3].building_time == 0.4f, "building stores seconds");
    for (const auto &kind : {"vehicle", "building"}) {
        for (const auto &value : {"-1", "101", "nan", "inf", "junk", "4.4junk", "0,4"}) {
            Check(Parse(w, kind, value), "invalid time does not reject entire prototype");
            const float time = std::string(kind) == "vehicle" ? w._vhclProtos[3].building_time : w._buildProtos[3].building_time;
            Check(time < 0.0f, "invalid time restores vanilla");
        }
        for (const auto &value : {"0", "0.4", "4.4", "10", "11", "50", "100"})
            Check(Parse(w, kind, value), "valid range parses");
    }
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypatank>());
    NC_STACK_base visual;
    w._vhclModels.resize(256, &visual);
    auto &proto = w._vhclProtos[2];
    proto.model_id = BACT_TYPES_TANK; proto.weapon = -1;
    proto.scale_fx_pXX.fill(0); proto.disable_collision = true;
    proto.energy = 9500; proto.sndFX[3].auto_pitch = true;
    for (float seconds : {-1.0f, 0.0f, 0.4f, 4.4f, 10.0f, 11.0f, 50.0f, 100.0f}) {
        std::printf("Vehicle duration %g\n", seconds);
        proto.building_time = seconds;
        ypaworld_arg146 request; request.vehicle_id = 2; request.pos = vec3d(2400, -100, -2400);
        auto *unit = w.ypaworld_func146(&request);
        Check(unit && unit->_building_time == seconds, "factory copies time");
        if (!unit) continue;
        const int expected = seconds < 0 ? 1900 : (int)std::lround(seconds * 1000);
        Check(unit->GetCreationTime(1900) == expected, "duration independent of energy");
        Check(unit->GetCreationTime(0) == (seconds < 0 ? 0 : expected), "network creation uses explicit time and preserves legacy zero fallback");
        const int reference = seconds < 0 ? 9500 : (int)std::lround(std::min(seconds, 10.0f) * 5000);
        Check(unit->_soundcarrier.Sounds[3].Pitch == yw_GetAutomaticGenesisPitch(reference), "pitch matches legacy cost and caps at ten");
        unit->_status = BACT_STATUS_CREATE; unit->_scale_time = expected;
        update_msg tick; tick.frameTime = expected > 0 ? expected - 1 : 1; tick.gTime = tick.frameTime;
        unit->CreationTimeUpdate(&tick);
        Check((unit->_status == BACT_STATUS_CREATE) == (expected > 0), "unit waits until deadline, zero finishes first update");
        if (expected > 0) {
            tick.frameTime = 1; ++tick.gTime; unit->CreationTimeUpdate(&tick);
            Check(unit->_status == BACT_STATUS_NORMAL, "unit operational at deadline");
        }
        unit->_disable_genesis = true;
        Check(unit->GetCreationTime(1900) == (seconds < 0 ? 1900 : 0), "disable_genesis retains priority and absent time preserves legacy dock wait");
        unit->Delete();
    }
    // Authored sound pitch remains vanilla without building_time; an explicit time owns Genesis pitch.
    proto.sndFX[3].auto_pitch = false;
    proto.sndFX[3].SetPitchRange(123, 123);
    for (float seconds : {-1.0f, 4.4f}) {
        std::printf("Vehicle duration %g\n", seconds);
        proto.building_time = seconds;
        ypaworld_arg146 request; request.vehicle_id = 2; request.pos = vec3d(2400, -100, -2400);
        auto *unit = w.ypaworld_func146(&request);
        Check(unit && unit->_soundcarrier.Sounds[3].Pitch == (seconds < 0 ? 123 : yw_GetAutomaticGenesisPitch(22000)), "explicit time controls sound pitch");
        if (unit) unit->Delete();
    }
    std::puts("Vehicle checks complete; testing building deadlines");
    cellArea &cell = w._cells(2, 2);
    cell.CellId = Common::Point(2, 2); cell.Id = 18;
    for (float seconds : {-1.0f, 0.0f, 0.4f, 10.0f, 50.0f, 100.0f}) {
        std::printf("Building duration %g\n", seconds);
        w._inBuildProcess.clear();
        w._buildProtos[3].building_time = seconds;
        Check(w.BuildingConstructBegin(&cell, 3, 1, 3000), "building starts construction");
        const int expected = seconds < 0 ? 3000 : (int)std::lround(seconds * 1000);
        Check(w._inBuildProcess[cell.Id].EndTime == expected, "building deadline follows explicit seconds or vanilla");
        Check(cell.PurposeType == cellArea::PT_CONSTRUCTING, "building starts non-operational");
        if (expected > 0) {
            w.BuildingConstructUpdate(expected - 1);
            Check(cell.PurposeType == cellArea::PT_CONSTRUCTING && w._inBuildProcess[cell.Id].Time == expected - 1,
                  "building stays non-operational before its deadline");
        }
    }
    w._inBuildProcess.clear();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}


