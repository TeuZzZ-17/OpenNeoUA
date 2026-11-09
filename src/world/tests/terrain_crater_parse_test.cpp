// Terrain crater keys, saved lines and the crater limit, on the real parser and
// the real world object (no level, no window). Run with run_runtime_probe.py.
#include "game/yw.h"
#include "system/inivals.h"
#include "world/parsers.h"
#include <cmath>
#include <cstdio>
#include <memory>
#undef main

int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *label)
    {
        ++checks;
        if ( !ok )
        {
            ++failures;
            std::printf("FAIL %s\n", label);
        }
    };

    auto world = std::unique_ptr<NC_STACK_ypaworld>(new NC_STACK_ypaworld);
    world->_vhclProtos.resize(8);
    world->_weaponProtos.resize(8);
    world->_buildProtos.resize(8);
    auto parse = [&](Engine::StringList lines)
    {
        ScriptParser::HandlersList handlers{new World::Parsers::WeaponProtoParser(world.get())};
        return ScriptParser::ParseStringList(lines, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
    };
    const auto &weapons = world->_weaponProtos;

    System::IniConf::GameDeformationGroundLimit.Value = std::string("10");
    System::IniConf::GameDeformationBuildingLimit.Value = std::string("0.4");
    const auto limits = world->TerrainCraterLimits(vec3d());
    check(limits.ground == 1000 && limits.building == 40, "decimal INI limits use 100 world units per value");
    for (const char *raw : {"", "-1", "10.1", "nan", "inf", "0.4junk", "0,4"}) {
        System::IniConf::GameDeformationGroundLimit.Value = std::string(raw);
        check(world->TerrainCraterLimits(vec3d()).ground < 0, "invalid INI cap keeps legacy behavior");
    }
    System::IniConf::GameDeformationGroundLimit.Value = std::string("0");
    check(world->TerrainCraterLimits(vec3d()).ground == 0, "INI zero disables ground displacement");
    System::IniConf::GameDeformationGroundLimit.Value = std::string("");
    System::IniConf::GameDeformationBuildingLimit.Value = std::string("");

    // ----------------------------------------------------------- weapon keys ---
    check(parse({"new_weapon 1", "crater_radius = 120~160", "crater_depth = 40~25", "crater_rim = 20", "end"}),
          "crater keys parse");
    const World::TCraterConfig &full = weapons[1].crater;
    check(full.defined && full.radius_min == 120 && full.radius_max == 160, "radius range read");
    check(full.depth_min == 25 && full.depth_max == 40, "depth range read in any order");
    check(full.rim_min == 20 && full.rim_max == 20, "rim read");

    check(parse({"new_weapon 2", "crater_radius = 200", "end"}) && !weapons[2].crater.defined,
          "radius without depth makes no crater");
    check(parse({"new_weapon 3", "crater_rim = abc", "crater_radius = 100", "crater_depth = 30", "end"}) &&
          !weapons[3].crater.defined, "an invalid key written first still disables the crater");
    check(parse({"new_weapon 4", "crater_radius = -5", "crater_depth = 30", "end"}) && !weapons[4].crater.defined,
          "negative radius disables the crater");
    check(parse({"new_weapon 5", "crater_radius = 100", "crater_depth = 30", "crater_rim = 150", "end"}) &&
          !weapons[5].crater.defined, "rim over 100 disables the crater");
    check(parse({"new_weapon 6", "end"}) && !weapons[6].crater.defined &&
          weapons[6].crater.rim_min == 20 && weapons[6].crater.rim_max == 20,
          "no keys: no crater (vanilla), rim default 20");
    check(parse({"new_weapon 7", "crater_radius = 100", "crater_depth = 30", "end"}) &&
          weapons[7].crater.defined && weapons[7].crater.rim_max == 20, "rim is optional");
    check(parse({"modify_weapon 1", "crater_depth = 0", "end"}) && !weapons[1].crater.defined,
          "modify_weapon with depth 0 switches the crater off");
    check(parse({"new_weapon 1", "end"}) && !weapons[1].crater.defined, "new_weapon starts from no crater");

    // ----------------------------------------------------------- saved lines ---
    System::IniConf::GameDeformationMaxCraters.Value = (int32_t)128;
    world->ClearTerrainCraters();
    check(world->ReadTerrainCrater("100.00|-50.00|200.00|150.00|40.00|0.2000") &&
          world->TerrainCraterCount() == 1, "saved crater line read");
    const World::Crater::Shape &read = world->_terrainCraters.craters.back();
    check(read.x == 100 && read.y == -50 && read.z == 200 && read.radius == 150 && read.depth == 40 &&
          std::fabs(read.rim - 0.2) < 1e-6, "saved values kept");
    check(!world->ReadTerrainCrater("1|2|3"), "short line refused");
    check(!world->ReadTerrainCrater("a|b|c|d|e|f"), "text refused");
    check(!world->ReadTerrainCrater("0|0|0|0|30|0"), "zero radius refused");
    check(world->ReadTerrainCrater("0|0|0|100|500|0") &&
          world->_terrainCraters.craters.back().depth == 50, "saved depth limited to half the radius");
    check(world->TerrainCraterCount() == 2, "only valid lines were added");

    check(world->ReadTerrainDent("10.00|-300.00|20.00|120.00|35.00|0.0000|2.0000|0.0000") &&
          world->TerrainCraterCount() == 3, "saved dent line read");
    const World::Crater::Shape &dent = world->_terrainCraters.craters.back();
    check(dent.kind == World::Crater::Shape::DENT && dent.ny == 1.0 && dent.radius == 120 && dent.depth == 35,
          "saved dent kept, direction made unit length");
    check(!world->ReadTerrainDent("10|20|30|100|30|0|0|0"), "dent without a direction refused");
    check(!world->ReadTerrainDent("10|20|30|100|30|0.2"), "crater numbers are not a dent");
    check(!world->ReadTerrainCrater("10|20|30|100|30|0|1|0"), "dent numbers are not a crater");
    check(world->TerrainCraterCount() == 3, "only valid dent lines were added");

    // ----------------------------------------------------------------- limit ---
    world->ClearTerrainCraters();
    System::IniConf::GameDeformationMaxCraters.Value = (int32_t)3;
    for (int i = 0; i < 5; ++i)
        world->AddTerrainCrater(World::Crater::Make(i * 1000.0, 0, 0, 100, 30, 0.2));
    check(world->TerrainCraterCount() == 3 && world->_terrainCraters.craters.front().x == 2000.0,
          "past the limit the oldest crater goes first");
    System::IniConf::GameDeformationMaxCraters.Value = (int32_t)0;
    world->ClearTerrainCraters();
    world->AddTerrainCrater(World::Crater::Make(0, 0, 0, 100, 30, 0.2));
    check(world->TerrainCraterCount() == 0, "limit 0 disables craters");
    System::IniConf::GameDeformationMaxCraters.Value = (int32_t)-7;
    world->AddTerrainCrater(World::Crater::Make(0, 0, 0, 100, 30, 0.2));
    check(world->TerrainCraterCount() == 1, "negative limit falls back to the default");

    // ------------------------------------------------------------ multiplayer ---
    System::IniConf::GameDeformationMaxCraters.Value = (int32_t)128;
    World::TCraterConfig config;
    config.defined = true;
    config.radius_min = config.radius_max = 100;
    config.depth_min = config.depth_max = 30;
    ypaworld_arg136 hit;
    hit.isect = true;
    world->_isNetGame = true;
    check(!world->SpawnTerrainCrater(config, hit), "no craters in multiplayer");
    world->_isNetGame = false;
    config.defined = false;
    check(!world->SpawnTerrainCrater(config, hit), "a weapon without crater keys digs nothing");

    world->ClearTerrainCraters();
    check(world->TerrainCraterCount() == 0, "level cleanup removes every crater");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
