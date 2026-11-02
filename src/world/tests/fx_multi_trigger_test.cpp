// Checks that begin_fx trigger = a|b stores one config per trigger and rejects invalid lists.
#include "game/yw.h"
#include "../parsers.h"
#include <cstdio>
#undef main
int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what); } };

    const auto fxLines = [](const char *header, const char *trigger)
    {
        return Engine::StringList{header,
            "begin_fx", trigger, "mode = visual", "start_size = 0",
            "end_size = 1.5", "duration = 300", "vp_model = 2", "vp_model = 8",
            "end", "end"};
    };

    const auto weaponFX = [&](const char *trigger)
    {
        NC_STACK_ypaworld world;
        world._weaponProtos.resize(2);
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::WeaponProtoParser(&world));
        ScriptParser::ParseStringList(fxLines("new_weapon 1", trigger), handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        return world._weaponProtos[1].chain_fx;
    };

    const auto vehicleFX = [&](const char *trigger)
    {
        NC_STACK_ypaworld world;
        world._vhclProtos.resize(2);
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::VhclProtoParser(&world));
        ScriptParser::ParseStringList(fxLines("new_vehicle 1", trigger), handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        return world._vhclProtos[1].chain_fx;
    };

    // Single trigger keeps the old behaviour: one config.
    {
        const auto fx = weaponFX("trigger = detonate");
        check(fx.size() == 1 && fx[0].trigger == World::TChainFXConfig::TRIGGER_DETONATE, "single trigger");
    }

    // Two triggers: two configs with the same content, in the written order.
    {
        const auto fx = weaponFX("trigger = detonate|impact_world");
        check(fx.size() == 2, "weapon pair count");
        if (fx.size() == 2)
        {
            check(fx[0].trigger == World::TChainFXConfig::TRIGGER_DETONATE, "weapon pair first");
            check(fx[1].trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD, "weapon pair second");
            check(fx[0].duration == 300 && fx[1].duration == 300, "weapon pair duration");
            check(fx[0].visuals.size() == 2 && fx[1].visuals.size() == 2, "weapon pair models");
        }
    }

    // Three weapon triggers.
    {
        const auto fx = weaponFX("trigger = launch|detonate|impact_world");
        check(fx.size() == 3, "weapon triple count");
    }

    // Repeated names count once.
    {
        const auto fx = weaponFX("trigger = detonate|detonate");
        check(fx.size() == 1, "duplicate name counted once");
    }

    // Vehicle pair.
    {
        const auto fx = vehicleFX("trigger = destroyed|air_destroyed");
        check(fx.size() == 2, "vehicle pair count");
        if (fx.size() == 2)
        {
            check(fx[0].trigger == World::TChainFXConfig::TRIGGER_DESTROYED, "vehicle pair first");
            check(fx[1].trigger == World::TChainFXConfig::TRIGGER_AIR_DESTROYED, "vehicle pair second");
        }
    }

    // Invalid lists reject the whole block.
    check(weaponFX("trigger = detonate|").empty(), "trailing separator rejected");
    check(weaponFX("trigger = |detonate").empty(), "leading separator rejected");
    check(weaponFX("trigger = detonate||impact_world").empty(), "empty name rejected");
    check(weaponFX("trigger = detonate|nonsense").empty(), "unknown name rejected");
    check(weaponFX("trigger = detonate|destroyed").empty(), "vehicle trigger in weapon rejected");
    check(vehicleFX("trigger = destroyed|detonate").empty(), "weapon trigger in vehicle rejected");
    check(weaponFX("trigger = detonate&impact_world").empty(), "old ampersand separator rejected");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
