// Checks the production Chain FX parser and spawn path without opening a window.
#include "game/yw.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#undef main
int main()
{
    int checks = 0, failures = 0;
    for (int context = 0; context < 3; ++context)
    for (int variant = 0; variant < 7; ++variant)
    {
        NC_STACK_ypaworld world;
        world._vhclProtos.resize(2);
        world._weaponProtos.resize(2);
        std::map<int32_t, World::TSuperItemProfile> profiles;
        ScriptParser::HandlersList handlers;
        handlers.push_back(context == 0 ? (ScriptParser::DataHandler*)new World::Parsers::VhclProtoParser(&world) :
            context == 1 ? (ScriptParser::DataHandler*)new World::Parsers::WeaponProtoParser(&world) :
            (ScriptParser::DataHandler*)new World::Parsers::SuperItemProfileParser(&profiles));
        Engine::StringList lines{context == 0 ? "new_vehicle 1" : context == 1 ? "new_weapon 1" : "new_superitem 1",
            "begin_fx", context == 0 ? "trigger = air_destroyed" : "trigger = detonate"};
        if (variant == 0) lines.push_back("tint = 255_231_8_255");
        lines.push_back("mode = visual");
        lines.push_back("duration = 1600");
        lines.push_back("vp_model = 1");
        if (variant == 1 || variant == 3) lines.push_back("tint = 255_231_8_255");
        lines.push_back("vp_model = 1");
        lines.push_back("vp_model = 1");
        if (variant == 2) lines.push_back("tint = 255_231_8_255");
        if (variant == 3) lines.push_back("tint = 8_231_255_128");
        if (variant == 5) lines.push_back("tint = invalid");
        if (variant == 6) lines.push_back("tint = 999_-3_8_255");
        lines.push_back("end");
        lines.push_back("end");
        auto check = [&](bool ok) { ++checks; if (!ok) { ++failures; std::printf("FAIL context=%d variant=%d check=%d\n", context, variant, checks); } };
        check(ScriptParser::ParseStringList(lines, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP));
        const auto &fx = context == 0 ? world._vhclProtos[1].chain_fx : context == 1 ? world._weaponProtos[1].chain_fx : profiles.rbegin()->second.detonate_chain_fx;
        check(fx.size() == 1);
        if (fx.empty()) continue;
        check(fx[0].visuals.size() == 3);
        for (const auto &visual : fx[0].visuals)
        {
            const auto &t = visual.tint;
            check(visual.has_tint == (variant != 4));
            if (variant == 4 || variant == 5) check(t.IsNeutral());
            else check(std::abs(t.r - (variant == 3 ? 8.f/255 : 1.f)) < 0.0001f &&
                       std::abs(t.g - (variant == 6 ? 0.f : 231.f/255)) < 0.0001f &&
                       std::abs(t.b - (variant == 3 ? 1.f : 8.f/255)) < 0.0001f &&
                       std::abs(t.a - (variant == 3 ? 128.f/255 : 1.f)) < 0.0001f);
        }
        NC_STACK_base model;
        world._vhclModels.resize(2);
        world._vhclModels[1] = &model;
        world.SpawnChainFX(fx[0], vec3d(), mat3x3::Ident());
        check(world._transientVPs.size() == 1);
        if (!world._transientVPs.empty())
        {
            const auto &spawned = world._transientVPs.back();
            check(spawned.chainTints.size() == 3);
            for (const auto &t : spawned.chainTints)
                check(t.r == spawned.tint.r && t.g == spawned.tint.g && t.b == spawned.tint.b && t.a == spawned.tint.a);
        }
        world._transientVPs.clear();
        world._vhclModels[1] = nullptr;
    }
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
