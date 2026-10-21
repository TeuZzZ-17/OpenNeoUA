// Checks that begin_fx trigger = launch is accepted for weapons only and keeps its authored values.
#include "game/yw.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#undef main
int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what); } };

    const auto fxLines = [](const char *header)
    {
        return Engine::StringList{header,
            "begin_fx", "trigger = launch", "mode = visual", "offset_xyz = 0_-20_0",
            "start_size = 0.2", "end_size = 0.2", "duration = 1000", "vp_model = 130",
            "end", "end"};
    };

    // Weapon: the block is kept as a launch trigger with its offset and sizes.
    {
        NC_STACK_ypaworld world;
        world._weaponProtos.resize(2);
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::WeaponProtoParser(&world));
        check(ScriptParser::ParseStringList(fxLines("new_weapon 1"), handlers, ScriptParser::FLAG_NO_SCOPE_SKIP), "weapon parse");
        const auto &fx = world._weaponProtos[1].chain_fx;
        check(fx.size() == 1, "weapon block kept");
        if (!fx.empty())
        {
            check(fx[0].trigger == World::TChainFXConfig::TRIGGER_LAUNCH, "weapon trigger is launch");
            check(fx[0].duration == 1000, "weapon duration");
            check(std::abs(fx[0].offset_min.y + 20.0f) < 0.0001f, "weapon offset");
            check(std::abs(fx[0].start_size - 0.2f) < 0.0001f && std::abs(fx[0].end_size - 0.2f) < 0.0001f, "weapon sizes");
            check(fx[0].visuals.size() == 1 && fx[0].visuals[0].vp == 130, "weapon model");
        }
    }

    // Vehicle: launch is not a vehicle trigger, so the block is ignored.
    {
        NC_STACK_ypaworld world;
        world._vhclProtos.resize(2);
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::VhclProtoParser(&world));
        ScriptParser::ParseStringList(fxLines("new_vehicle 1"), handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        check(world._vhclProtos[1].chain_fx.empty(), "vehicle ignores launch");
    }

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
