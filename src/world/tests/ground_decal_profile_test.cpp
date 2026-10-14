// Probe for new_decal profiles: parser, size rules and the links from vehicles/weapons.
#include "game/yw.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#undef main

namespace
{
int checks = 0, failures = 0;
void Check(bool ok, const char *name)
{
    ++checks;
    if ( !ok ) { ++failures; std::printf("FAIL %s\n", name); }
}

bool Near(float a, float b) { return std::fabs(a - b) < 0.01f; }

bool ParseDecals(std::map<int32_t, World::TGroundDecalProfile> &profiles, const Engine::StringList &lines)
{
    ScriptParser::HandlersList handlers;
    handlers.push_back(new World::Parsers::DecalProfileParser(&profiles));
    return ScriptParser::ParseStringList(lines, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
}

const World::TChainFXConfig *FindDecal(const std::vector<World::TChainFXConfig> &chain)
{
    for (const World::TChainFXConfig &fx : chain)
    {
        if ( fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL )
            return &fx;
    }
    return nullptr;
}
}

int main()
{
    // ---- size rules ---------------------------------------------------------------------
    World::TGroundDecalProfile rule;
    float lo = 0.0f, hi = 0.0f;
    World::ResolveGroundDecalSize(rule, 50.0f, 200.0f, &lo, &hi);
    Check(Near(lo, 140.0f) && Near(hi, 160.0f), "default: 70-80% of the explosion diameter");
    World::ResolveGroundDecalSize(rule, 50.0f, 0.0f, &lo, &hi);
    Check(Near(lo, 75.0f) && Near(hi, 100.0f), "no explosion: 150-200% of the silhouette");
    World::ResolveGroundDecalSize(rule, 0.0f, 0.0f, &lo, &hi);
    Check(Near(lo, 30.0f) && Near(hi, 30.0f), "nothing to measure: the floor");
    World::ResolveGroundDecalSize(rule, 4.0f, 10.0f, &lo, &hi);
    Check(Near(lo, 30.0f) && Near(hi, 30.0f), "tiny explosion never goes below the floor");
    World::ResolveGroundDecalSize(rule, std::nanf(""), std::nanf(""), &lo, &hi);
    Check(Near(lo, 30.0f) && Near(hi, 30.0f), "invalid diameters are ignored");
    rule.has_absolute_size = true;
    rule.size_min = 200.0f;
    rule.size_max = 210.0f;
    World::ResolveGroundDecalSize(rule, 50.0f, 900.0f, &lo, &hi);
    Check(Near(lo, 200.0f) && Near(hi, 210.0f), "size = min_max wins over the percentage rules");

    // ---- parser -------------------------------------------------------------------------
    std::map<int32_t, World::TGroundDecalProfile> profiles;
    const bool parsed = ParseDecals(profiles, Engine::StringList{
        "new_decal 1",
        "name = destroyed",
        "texture = Interface/GroundDecals/impact_01.png",
        "size_explosion = 60_70",
        "size_silhouette = 120",
        "size_min = 12",
        "stretch_x = 0.8_1.2",
        "stretch_z = 0.9",
        "tint = 255_255_255_180",
        "random_rotation = 1",
        "edge_fade = 5_10",
        "duration = permanent",
        "fade_in = 100",
        "fade_out = 1000",
        "smoke_duration = 20000_25000",
        "smoke_fade_in = 3000",
        "smoke_fade_out = 20000",
        "smoke_count = 30_10",
        "smoke_size = 35_50",
        "smoke_intensity = 120",
        "smoke_tint = 70_70_70_150",
        "end",
        "new_decal 2",
        "size = 200_210",
        "duration = 5000",
        "fade_in = 9000",
        "fade_out = 9000",
        "end",
        "new_decal 3",
        "texture = x.png",
        "end",
        "new_decal",
        "duration = permanent",
        "end"});
    Check(parsed, "decal profiles parse");
    Check(profiles.size() == 2 && profiles.count(1) && profiles.count(2), "invalid profiles are not stored");

    const World::TGroundDecalProfile &one = profiles[1];
    Check(one.valid && one.name == "destroyed" && !one.has_absolute_size, "profile 1 uses the percentage rules");
    Check(Near(one.explosion_percent_min, 60.0f) && Near(one.explosion_percent_max, 70.0f) &&
          Near(one.silhouette_percent_min, 120.0f) && Near(one.silhouette_percent_max, 120.0f) &&
          Near(one.floor_size, 12.0f), "size rule keys");
    Check(one.fx.ground_decal_permanent && one.fx.fade_in == 100 && one.fx.fade_out == 0,
          "permanent decal: no fade-out, fade-in kept");
    Check(one.fx.ground_decal_stretch_x_min == 0.8f && one.fx.ground_decal_stretch_z_min == 0.9f &&
          one.fx.ground_decal_stretch_z_max == 0.9f, "stretch");
    Check(one.fx.ground_decal_edge_fade_min == 5 && one.fx.ground_decal_edge_fade_max == 10, "edge fade range");
    Check(one.fx.ground_decal_random_rotation && one.fx.ground_decal_texture == "Interface/GroundDecals/impact_01.png",
          "rotation and texture");
    const World::TGroundDecalSmoke &smoke = one.fx.ground_decal_smoke;
    Check(smoke.enabled && smoke.count_min == 10 && smoke.count_max == 30 &&
          smoke.size_min == 35.0f && smoke.size_max == 50.0f && smoke.intensity == 120.0f &&
          smoke.duration_min == 20000 && smoke.duration_max == 25000,
          "any smoke_* key switches the smoke on, smoke_count accepts min_max");

    const World::TGroundDecalProfile &two = profiles[2];
    Check(two.has_absolute_size && two.size_min == 200.0f && two.size_max == 210.0f, "absolute size");
    Check(!two.fx.ground_decal_smoke.enabled, "no smoke key: no smoke");
    Check(!two.fx.ground_decal_permanent && two.fx.duration == 5000 && two.fx.fade_out == 5000 && two.fx.fade_in == 0,
          "fades never exceed the duration");

    // A later definition of the same id replaces the previous one.
    ParseDecals(profiles, Engine::StringList{"new_decal 2", "size = 10", "duration = 100", "end"});
    Check(profiles[2].size_min == 10.0f, "redefinition replaces the profile");

    // smoke_count = 0 keeps the block but produces no puffs.
    std::map<int32_t, World::TGroundDecalProfile> off;
    ParseDecals(off, Engine::StringList{"new_decal 1", "duration = permanent", "smoke_count = 0", "end"});
    Check(off.count(1) && off[1].fx.ground_decal_smoke.count_max == 0, "smoke_count = 0");

    // The old syntax is gone: a ground_decal Chain FX block is ignored, and unknown keys are errors.
    {
        NC_STACK_ypaworld world;
        world._vhclProtos.resize(2);
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::VhclProtoParser(&world));
        ScriptParser::ParseStringList(Engine::StringList{
            "new_vehicle 1", "begin_fx", "trigger = destroyed", "mode = ground_decal", "size = 50",
            "duration = 1000", "end", "end"}, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        Check(!FindDecal(world._vhclProtos[1].chain_fx), "begin_fx mode = ground_decal no longer exists");
    }

    // ---- links from vehicles and weapons --------------------------------------------------
    NC_STACK_ypaworld world;
    world._vhclProtos.resize(3);
    world._weaponProtos.resize(3);
    world._decalProfiles = profiles;
    {
        ScriptParser::HandlersList handlers;
        handlers.push_back(new World::Parsers::VhclProtoParser(&world));
        handlers.push_back(new World::Parsers::WeaponProtoParser(&world));
        const bool linked = ScriptParser::ParseStringList(Engine::StringList{
            "new_vehicle 1", "decal_id = 1", "radius = 40", "at_death_energy_radius = 100", "mgun_decal_id = 2", "end",
            "new_vehicle 2", "decal_id = 7", "radius = 40", "end",
            "new_weapon 1", "decal_id = 1", "blast_radius = 200", "end",
            "new_weapon 2", "blast_radius = 200", "end"},
            handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        Check(linked, "decal_id and mgun_decal_id are known keys");
    }
    world.ResolveDecalProfileLinks();

    const World::TChainFXConfig *vehicleDecal = FindDecal(world._vhclProtos[1].chain_fx);
    Check(vehicleDecal && vehicleDecal->trigger == World::TChainFXConfig::TRIGGER_DESTROYED &&
          Near(vehicleDecal->ground_decal_size_min, 120.0f) && Near(vehicleDecal->ground_decal_size_max, 140.0f),
          "vehicle: destroyed decal, 60-70% of the at-death explosion diameter");
    Check(world._vhclProtos[1].mgun_decal_enable &&
          world._vhclProtos[1].mgun_decal.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
          world._vhclProtos[1].mgun_decal.ground_decal_size_min == 10.0f, "mgun_decal_id links profile 2");
    Check(!FindDecal(world._vhclProtos[2].chain_fx), "missing decal id: no decal");

    const World::TChainFXConfig *weaponDecal = FindDecal(world._weaponProtos[1].chain_fx);
    Check(weaponDecal && weaponDecal->trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
          Near(weaponDecal->ground_decal_size_min, 240.0f) && Near(weaponDecal->ground_decal_size_max, 280.0f),
          "weapon: impact decal, 60-70% of the blast diameter");
    Check(!FindDecal(world._weaponProtos[2].chain_fx), "no decal_id: no decal");

    // Resolving again after a profile change replaces the decal instead of stacking another.
    world._decalProfiles[1].explosion_percent_min = 50.0f;
    world._decalProfiles[1].explosion_percent_max = 50.0f;
    world.ResolveDecalProfileLinks();
    size_t decals = 0;
    for (const World::TChainFXConfig &fx : world._weaponProtos[1].chain_fx)
        decals += fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL ? 1u : 0u;
    Check(decals == 1 && Near(FindDecal(world._weaponProtos[1].chain_fx)->ground_decal_size_min, 200.0f),
          "re-resolving rebuilds the decal");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
