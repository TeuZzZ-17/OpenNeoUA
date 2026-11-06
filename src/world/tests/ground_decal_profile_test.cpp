#include "game/yw.h"
#include "world/parsers.h"
#include "world/grounddecal_smoke.h"
#include <cstdio>
#include <memory>
#undef main

int main()
{
    int checks=0,failures=0;
    auto check=[&](bool ok,const char *label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}};
    auto world=std::unique_ptr<NC_STACK_ypaworld>(new NC_STACK_ypaworld);
    world->_vhclProtos.resize(8);world->_weaponProtos.resize(8);world->_buildProtos.resize(8);
    auto parse=[&](Engine::StringList lines){
        ScriptParser::HandlersList handlers{
            new World::Parsers::DecalProfileParser(&world->_decalProfiles),
            new World::Parsers::VhclProtoParser(world.get()),new World::Parsers::WeaponProtoParser(world.get())};
        return ScriptParser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP);
    };
    check(parse({"new_decal 1","texture = Graphics/GroundDecals/impact_01.png","duration = permanent","end"}),"plain decal parse");
    const auto &fx=world->_decalProfiles[1].fx;
    check(!fx.ground_decal_smoke.enabled,"decal without smoke keys emits no smoke");
    check(fx.ground_decal_size_min==0&&fx.ground_decal_size_max==0,"profile has no implicit size");
    check(parse({"new_decal 2","duration = permanent","effect_count = 2~4","effect_fade_inout = 100|400","end"}),"smoke settings parse");
    const auto &smoky=world->_decalProfiles[2].fx;
    check(smoky.ground_decal_smoke.enabled&&smoky.ground_decal_smoke.count_min==2&&smoky.ground_decal_smoke.count_max==4,"smoke keys enable smoke");
    check(parse({"new_vehicle 1","ground_decal_id = 2","ground_decal_size = 120~80","mgun_ground_decal_id = 2","mgun_ground_decal_size = 4~8","end",
        "new_weapon 2","ground_decal_id = 2","ground_decal_size = 25~40","end"}),"manual source sizes parse");
    const auto &vehicle=world->_vhclProtos[1];
    check(vehicle.ground_decal_size.defined&&vehicle.ground_decal_size.size_min==80&&vehicle.ground_decal_size.size_max==120,"manual size range normalizes");
    check(vehicle.mgun_ground_decal_size.size_min==4&&world->_weaponProtos[2].ground_decal_size.size_max==40,"MGUN and weapon retain independent manual sizes");
    const auto resolved=World::ResolveGroundDecal(world->_decalProfiles[2],World::TChainFXConfig::TRIGGER_DESTROYED,vehicle.ground_decal_size);
    check(resolved.ground_decal_size_min==80&&resolved.ground_decal_size_max==120&&resolved.ground_decal_smoke.enabled,"shared profile resolution retains size and smoke");
    check(parse({"modify_vehicle 1","ground_decal_size = nan","end"})&&!vehicle.ground_decal_size.defined,"invalid size safely disables decal");
    check(parse({"modify_vehicle 1","ground_decal_size = 0","end"})&&vehicle.ground_decal_size.defined&&vehicle.ground_decal_size.size_max==0,"zero explicit size suppresses decal");
    World::TGroundDecalSmoke smoke;
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"disabled smoke cannot spawn");
    smoke.enabled=true;
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"effect without a model emits nothing");
    smoke.mesh3ds="Data/Models/3ds/Smoke.3ds";
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"effect without effect_mode emits nothing");
    smoke.mode=World::TGroundDecalSmoke::MODE_RISE;
    smoke.mesh3ds.clear();smoke.base_model="x.BASE";
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"rise without a 3DS emits nothing (BASE is ignored)");
    smoke.mode=World::TGroundDecalSmoke::MODE_GROUND_PULSE;
    smoke.count_min=smoke.count_max=0;
    check(world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"ground_pulse accepts a BASE and ignores effect_count");
    world->ClearGroundDecals();
    smoke.count_min=10;smoke.count_max=30;
    smoke.mode=World::TGroundDecalSmoke::MODE_RISE;
    smoke.mesh3ds="Data/Models/3ds/Smoke.3ds";
    check(world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"enabled smoke spawns");
    smoke.chance=0;
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"zero probability suppresses smoke");
    auto &emitter=world->_groundDecalSmoke.back();World::GroundDecalSmoke::Puff puff;
    check(World::GroundDecalSmoke::Evaluate(emitter,0,1000,&puff)&&std::isfinite(puff.pos.x)&&puff.size>0,"smoke puff geometry finite");
    check(!World::GroundDecalSmoke::Evaluate(emitter,0,emitter.duration,&puff),"smoke stops at authored lifetime");
    world->ClearGroundDecals();
    check(world->_groundDecalSmoke.empty(),"level cleanup releases smoke");
    check(parse({"new_decal 3", "duration = permanent", "effect_3ds = Data/Models/3ds/Spark.3ds",
                 "effect_mode = ground_pulse", "effect_pulse_interval = 900", "effect_pulse_time = 120", "effect_scale = 0.12", "end"}), "mesh pulse profile parses");
    const auto &pulse=world->_decalProfiles[3].fx.ground_decal_smoke;
    check(pulse.enabled && pulse.pulse_interval_min==900 && pulse.pulse_interval_max==900 && pulse.pulse_time==120 &&
          !pulse.mesh3ds.empty() && std::fabs(pulse.mesh_scale-0.12f)<0.0001f, "mesh pulse settings retained");
    check(parse({"new_decal 4", "duration = permanent", "effect_3ds = 0", "end"}) &&
          world->_decalProfiles[4].fx.ground_decal_smoke.mesh3ds.empty(), "zero mesh preserves smoke mode");
    check(parse({"new_decal 5", "duration = permanent", "effect_vp = 2", "end"}) &&
          world->_decalProfiles[5].fx.ground_decal_smoke.pulse_vp==2, "original VP pulse parses");
    check(parse({"new_decal 6", "duration = permanent", "effect_vp = 0", "end"}) &&
          world->_decalProfiles[6].fx.ground_decal_smoke.pulse_vp==0, "zero VP keeps smoke mode");
    check(parse({"new_decal 7", "duration = permanent", "effect_base = Data/Models/Base/New/Effect_Smoke/Effect_Smoke.BASE", "end"}) &&
          !world->_decalProfiles[7].fx.ground_decal_smoke.base_model.empty(), "external BASE effect parses");
    check(parse({"new_decal 8", "duration = permanent", "effect_3ds = Data/Models/3ds/Smoke.3ds", "effect_mode = rise", "effect_size = 30~45", "end"}) &&
          world->_decalProfiles[8].fx.ground_decal_smoke.mode==World::TGroundDecalSmoke::MODE_RISE &&
          world->_decalProfiles[8].fx.ground_decal_smoke.size_min==30.0f &&
          world->_decalProfiles[8].fx.ground_decal_smoke.size_max==45.0f,
          "external rise settings parse (size only, no effect_scale)");
    check(parse({"new_decal 9", "duration = permanent", "effect_3ds = Data/Models/3ds/Spark.3ds", "effect_mode = ground_pulse", "effect_pulse_interval = 1500~700", "end"}) &&
          world->_decalProfiles[9].fx.ground_decal_smoke.pulse_interval_min==700 &&
          world->_decalProfiles[9].fx.ground_decal_smoke.pulse_interval_max==1500, "effect_pulse_interval accepts a reversed min~max");
    check(parse({"new_decal 10", "duration = permanent", "effect_3ds = Data/Models/3ds/Smoke.3ds", "end"}) &&
          world->_decalProfiles[10].fx.ground_decal_smoke.mode==World::TGroundDecalSmoke::MODE_NONE,
          "profile without effect_mode keeps the effect disabled");
    check(parse({"new_decal 11", "duration = permanent", "effect_3ds = Data/Models/3ds/Smoke.3ds", "effect_mode = plume", "end"}) &&
          world->_decalProfiles[11].fx.ground_decal_smoke.mode==World::TGroundDecalSmoke::MODE_NONE,
          "old effect_mode names are rejected");
    {
        World::TGroundDecalSmoke ranged;
        ranged.pulse_interval_min=700;ranged.pulse_interval_max=1500;
        bool inside=true,varies=false;int first=World::GroundDecalSmoke::PulseInterval(ranged,77,0);
        for(int cycle=0;cycle<50;++cycle){const int v=World::GroundDecalSmoke::PulseInterval(ranged,77,cycle);
            inside=inside&&v>=700&&v<=1500;varies=varies||v!=first;}
        check(inside&&varies,"pulse interval rolls inside min~max per pulse");
        ranged.pulse_interval_max=700;
        check(World::GroundDecalSmoke::PulseInterval(ranged,77,3)==700,"fixed pulse interval stays constant");
    }
    std::printf("DECAL PROFILE CHECKS %d FAILURES %d\n",checks,failures);
    return failures?1:0;
}
