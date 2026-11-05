#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "common/utils.h"
#include "world/parsers.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#undef main

int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *label) {
        ++checks;
        if ( !ok ) { ++failures; std::printf("FAIL: %s\n", label); }
    };
    const char *fixtureDir = std::getenv("OPENNEOUA_RANGE_TEST_DIR");
    if ( !fixtureDir ) { std::puts("Set OPENNEOUA_RANGE_TEST_DIR to a disposable directory."); return 2; }
    FSMgr::iDir::setBaseDir("");
    Common::Env.SetPrefix("save", fixtureDir);

    NC_STACK_ypaworld world;
    world._vhclProtos.resize(3);
    auto &vehicle = world._vhclProtos[1];
    vehicle.model_id = BACT_TYPES_TANK;
    vehicle.num_weapons_min = 1;
    vehicle.num_weapons_max = 2;
    vehicle.mimic_energy_cost_min = 20000;
    vehicle.mimic_energy_cost_max = 30000;

    std::ofstream legacy(std::string(fixtureDir) + "/range-legacy.txt");
    legacy << "modify_vehicle 1\nnum_weapons = 7_11\nmimic_energy_cost = 20000_30000\nend\n";
    legacy.close();
    auto *file = uaOpenFileAlloc("save:range-test.txt", "w");
    check(file != nullptr, "open disposable output");
    if ( !file ) return 1;
    check(yw_write_item_modifers(&world, file), "write vehicle modifiers");
    delete file;
    std::ifstream written(std::string(fixtureDir) + "/range-test.txt");
    const std::string text((std::istreambuf_iterator<char>(written)), {});
    check(text.find("num_weapons    = 1~2") != std::string::npos, "write weapon range using tilde");
    check(text.find("mimic_energy_cost = 20000~30000") != std::string::npos, "write Mimic range using tilde");

    check(world.ParseSettingsFile("save:range-legacy.txt", World::SDF_PROTO), "load old profile");
    check(vehicle.num_weapons_min == 7 && vehicle.num_weapons_max == 11, "recover old weapon range");
    check(vehicle.mimic_energy_cost_min == 20000 && vehicle.mimic_energy_cost_max == 30000, "recover old Mimic range");

    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
    check(ScriptParser::ParseStringList({"modify_vehicle 1", "num_weapons = 1_2", "end"}, handlers, 0), "normal script parses");
    check(vehicle.num_weapons_min == 7 && vehicle.num_weapons_max == 11, "normal scripts still reject old separator");
    check(world.ParseSettingsFile("save:range-test.txt", World::SDF_PROTO), "load newly written profile");
    check(vehicle.num_weapons_min == 1 && vehicle.num_weapons_max == 2, "new weapon range round trip");
    check(vehicle.mimic_energy_cost_min == 20000 && vehicle.mimic_energy_cost_max == 30000, "new Mimic range round trip");
    ScriptParser::HandlersList saveHandlers{new World::Parsers::VhclProtoParser(&world, -1, true)};
    check(ScriptParser::ParseStringList({"modify_vehicle 1", "num_weapons = 2_1", "end"}, saveHandlers, 0), "old save block parses");
    check(vehicle.num_weapons_min == 1 && vehicle.num_weapons_max == 2, "old save range orders endpoints");
    ScriptParser::HandlersList invalidHandlers{new World::Parsers::VhclProtoParser(&world, -1, true)};
    check(ScriptParser::ParseStringList({"modify_vehicle 1", "num_weapons = 1__2", "end"}, invalidHandlers, 0), "invalid old range handled");
    check(vehicle.num_weapons_min == 1 && vehicle.num_weapons_max == 2, "invalid old range preserves previous value");
    std::printf("CHECKS=%d FAILED=%d\n", checks, failures);
    return failures ? 1 : 0;
}
