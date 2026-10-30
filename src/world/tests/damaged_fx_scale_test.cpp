#include "game/ypabact_internal.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }
struct Unit : NC_STACK_ypabact {
    void Bind(NC_STACK_ypaworld &world) { _world = &world; }
};
bool Parse(NC_STACK_ypaworld &world, const char *scale)
{
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
    Engine::StringList lines{"new_vehicle 1", "damaged_fx_vp = 138_138_130"};
    if (scale) lines.push_back(std::string("damaged_fx_scale = ") + scale);
    lines.push_back("end");
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main()
{
    NC_STACK_ypaworld world;
    world._vhclProtos.resize(2);
    Check(Parse(world, "1.0~1.7_1.0~1.7_0.2~0.3"), "parse per-VP ranges");
    const auto &scales = world._vhclProtos[1].damaged_fx.scales;
    Check(scales.size() == 3, "three VP scale ranges");
    if (scales.size() == 3) {
        Check(Near(scales[0].first, 1.0f) && Near(scales[0].second, 1.7f), "first range");
        Check(Near(scales[1].first, 1.0f) && Near(scales[1].second, 1.7f), "second range");
        Check(Near(scales[2].first, 0.2f) && Near(scales[2].second, 0.3f), "smoke range");
    }
    Check(Parse(world, "2_0_0.4~0.2"), "parse fixed and invalid values");
    const auto &mixed = world._vhclProtos[1].damaged_fx.scales;
    Check(mixed.size() == 3 && Near(mixed[0].first, 2.0f) && Near(mixed[0].second, 2.0f) &&
          Near(mixed[1].first, 1.0f) && Near(mixed[1].second, 1.0f) &&
          Near(mixed[2].first, 0.2f) && Near(mixed[2].second, 0.4f), "fixed, invalid and reversed ranges");

    NC_STACK_base fire, smoke;
    world._vhclModels.resize(139, nullptr);
    world._vhclModels[138] = &fire;
    world._vhclModels[130] = &smoke;
    Unit unit;
    unit.Bind(world);
    unit._damaged_fx = world._vhclProtos[1].damaged_fx;
    ypabact_SpawnDamagedFXEvent(&unit);
    Check(world._transientVPs.size() == 3, "one effect per configured VP");
    for (const auto &fx : world._transientVPs) {
        if (fx.vp && fx.vp->Bas == &smoke)
            Check(fx.axisScale.x >= 0.2f && fx.axisScale.x <= 0.4f, "smoke uses its range");
        else
            Check(Near(fx.axisScale.x, 1.0f) || Near(fx.axisScale.x, 2.0f), "fire uses its own scale");
    }
    world._transientVPs.clear();
    unit._damaged_fx.count_min = unit._damaged_fx.count_max = 3;
    for (int i = 0; i < 20; ++i) {
        ypabact_SpawnDamagedFXEvent(&unit);
        for (const auto &fx : world._transientVPs) {
            if (fx.vp && fx.vp->Bas == &smoke)
                Check(fx.axisScale.x >= 0.2f && fx.axisScale.x <= 0.4f, "smoke keeps scale after shuffle");
        }
        world._transientVPs.clear();
    }
    Check(Parse(world, "0.5~0.6"), "parse shared range");
    unit._damaged_fx = world._vhclProtos[1].damaged_fx;
    ypabact_SpawnDamagedFXEvent(&unit);
    Check(world._transientVPs.size() == 3, "shared range creates three effects");
    for (const auto &fx : world._transientVPs)
        Check(fx.axisScale.x >= 0.5f && fx.axisScale.x <= 0.6f, "shared range applies to every VP");
    world._transientVPs.clear();
    Check(Parse(world, "2_nan"), "parse missing and invalid scale");
    unit._damaged_fx = world._vhclProtos[1].damaged_fx;
    ypabact_SpawnDamagedFXEvent(&unit);
    auto it = world._transientVPs.begin();
    if (world._transientVPs.size() == 3) {
        Check(Near((it++)->axisScale.x, 2.0f), "first fixed scale");
        Check(Near((it++)->axisScale.x, 1.0f), "invalid scale defaults to one");
        Check(Near(it->axisScale.x, 1.0f), "missing scale defaults to one");
    } else Check(false, "effects created for missing scale test");
    world._transientVPs.clear();
    Check(Parse(world, nullptr), "parse absent scale");
    unit._damaged_fx = world._vhclProtos[1].damaged_fx;
    ypabact_SpawnDamagedFXEvent(&unit);
    Check(world._transientVPs.size() == 3, "absent scale creates three effects");
    for (const auto &fx : world._transientVPs)
        Check(Near(fx.axisScale.x, 1.0f), "absent scale defaults to one");
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
