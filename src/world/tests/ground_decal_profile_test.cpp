// Probe for new_decal profiles: parser, size rules and the links from vehicles/weapons.
#include "game/yw.h"
#include "../parsers.h"
#include <cmath>
#include "nucleus/skeleton.h"
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
    // ---- parser -------------------------------------------------------------------------
    std::map<int32_t, World::TGroundDecalProfile> profiles;
    const bool parsed = ParseDecals(profiles, Engine::StringList{
        "new_decal 1",
        "name = destroyed",
        "texture = Interface/GroundDecals/impact_01.png",
        "size_by_model_% = 150%_200",
        "stretch_xz = 0.8~1.2_0.9",
        "tint = 255_255_255_180",
        "random_rotation = 1",
        "edge_fade = 5_10",
        "duration = permanent",
        "fade_inout = 100_1000",
        "smoke_duration = 20000_25000",
        "smoke_fade_inout = 3000_20000",
        "smoke_count = 30_10",
        "smoke_size = 35_50",
        "smoke_intensity = 120",
        "smoke_tint = 70_70_70_150",
        "end",
        "new_decal 2",
        "size_by_model_% = abc",
        "duration = 5000",
        "fade_inout = 9000_9000",
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
    Check(one.valid && one.name == "destroyed", "profile 1 is valid");
    Check(Near(one.fx.ground_decal_model_percent_min, 150.0f) && Near(one.fx.ground_decal_model_percent_max, 200.0f),
          "size_by_model_% accepts optional percent signs");
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
    Check(Near(two.fx.ground_decal_model_percent_min, 100.0f) && Near(two.fx.ground_decal_model_percent_max, 100.0f),
          "invalid size_by_model_% keeps the 100_100 default");
    Check(!two.fx.ground_decal_smoke.enabled, "no smoke key: no smoke");
    Check(!two.fx.ground_decal_permanent && two.fx.duration == 5000 && two.fx.fade_out == 5000 && two.fx.fade_in == 0,
          "fades never exceed the duration");

    // A later definition of the same id replaces the previous one.
    ParseDecals(profiles, Engine::StringList{"new_decal 2", "size_by_model_% = 10", "duration = 100", "end"});
    Check(Near(profiles[2].fx.ground_decal_model_percent_min, 10.0f), "redefinition replaces the profile");

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

    // The previous size keys no longer exist.
    for (const char *oldKey : {"size = 50", "size_explosion_% = 50", "size_silhouette = 150", "size_min = 30"})
    {
        std::map<int32_t, World::TGroundDecalProfile> oldProfiles;
        const bool accepted = ParseDecals(oldProfiles, Engine::StringList{"new_decal 1", "duration = permanent", oldKey, "end"});
        Check(!accepted, oldKey);
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
            "new_vehicle 1", "ground_decal_id = 1", "ground_decal_size_by_model_% = 400%_500", "radius = 40", "at_death_energy_radius = 100",
            "mgun_ground_decal_id = 2", "mgun_ground_decal_size_by_model_% = 7", "end",
            "new_vehicle 2", "ground_decal_id = 7", "radius = 40", "end",
            "new_weapon 1", "ground_decal_id = 1", "ground_decal_size_by_model_% = abc", "blast_radius = 200", "end",
            "new_weapon 2", "blast_radius = 200", "end"},
            handlers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        Check(linked, "ground_decal_id and mgun_ground_decal_id are known keys");
    }
    world.ResolveDecalProfileLinks();

    const World::TChainFXConfig *vehicleDecal = FindDecal(world._vhclProtos[1].chain_fx);
    Check(vehicleDecal && vehicleDecal->trigger == World::TChainFXConfig::TRIGGER_DESTROYED &&
          Near(vehicleDecal->ground_decal_model_percent_min, 400.0f) &&
          Near(vehicleDecal->ground_decal_model_percent_max, 500.0f),
          "vehicle: the unit's ground_decal_size_by_model_% replaces the profile size");
    Check(world._vhclProtos[1].mgun_decal_enable &&
          world._vhclProtos[1].mgun_decal.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
          Near(world._vhclProtos[1].mgun_decal.ground_decal_model_percent_min, 7.0f),
          "mgun_ground_decal_id links profile 2");
    Check(!FindDecal(world._vhclProtos[2].chain_fx), "missing decal id: no decal");

    const World::TChainFXConfig *weaponDecal = FindDecal(world._weaponProtos[1].chain_fx);
    Check(weaponDecal && weaponDecal->trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
          Near(weaponDecal->ground_decal_model_percent_max, 200.0f),
          "weapon: a bad override is ignored, the profile size is kept");
    Check(!FindDecal(world._weaponProtos[2].chain_fx), "no ground_decal_id: no decal");

    // Resolving again after a profile change replaces the decal instead of stacking another.
    world._decalProfiles[1].fx.ground_decal_model_percent_min = 50.0f;
    world.ResolveDecalProfileLinks();
    size_t decals = 0;
    for (const World::TChainFXConfig &fx : world._weaponProtos[1].chain_fx)
        decals += fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL ? 1u : 0u;
    Check(decals == 1 && Near(FindDecal(world._weaponProtos[1].chain_fx)->ground_decal_model_percent_min, 50.0f),
          "re-resolving rebuilds the decal");

    // ---- model sphere diameter ---------------------------------------------------------------
    {
        NC_STACK_skeleton skeleton;
        UAskeleton::Data data;
        skeleton._resData = &data;
        data.POO = {vec3d(-10, -5, -2), vec3d(10, 5, 2)};
        NC_STACK_base *model = Nucleus::CInit<NC_STACK_base>();
        model->_skeleton = &skeleton;

        Check(World::GroundDecalModelDiameter(nullptr, vec3d(1, 1, 1)) == 0.0f, "no model: no diameter");
        Check(Near(World::GroundDecalModelDiameter(model, vec3d(1, 1, 1)), 2.0f * std::sqrt(129.0f)),
              "sphere reaches the farthest vertex from the bounding box center");
        Check(Near(World::GroundDecalModelDiameter(model, vec3d(2, 1, 1)), 2.0f * std::sqrt(429.0f)),
              "visual scale is applied per axis");

        // A child model offset along X widens the sphere.
        NC_STACK_skeleton kidSkeleton;
        UAskeleton::Data kidData;
        kidSkeleton._resData = &kidData;
        kidData.POO = {vec3d(0, 0, 0)};
        NC_STACK_base *kid = Nucleus::CInit<NC_STACK_base>();
        kid->_skeleton = &kidSkeleton;
        kid->_transform.Pos = vec3d(100, 0, 0);
        kid->_transform.SclRot = mat3x3::Ident();
        model->_KIDS.push_back(kid);
        Check(World::GroundDecalModelDiameter(model, vec3d(1, 1, 1)) > 100.0f, "child models are included");

        // Empty geometry: nothing to measure.
        model->_KIDS.clear();
        data.POO.clear();
        Check(World::GroundDecalModelDiameter(model, vec3d(1, 1, 1)) == 0.0f, "model without vertices: no diameter");

        // The stack skeletons are not owned by the bases.
        kid->_skeleton = nullptr;
        model->_skeleton = nullptr;
    }

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
