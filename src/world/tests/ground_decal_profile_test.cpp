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
    check(parse({"new_decal 1","texture = Interface/GroundDecals/impact_01.png","duration = permanent","end"}),"plain decal parse");
    const auto &fx=world->_decalProfiles[1].fx;
    check(!fx.ground_decal_smoke.enabled,"decal without smoke keys emits no smoke");
    check(fx.ground_decal_size_min==0&&fx.ground_decal_size_max==0,"profile has no implicit size");
    check(parse({"new_decal 2","duration = permanent","smoke_count = 2_4","smoke_fade_inout = 100_400","end"}),"smoke settings parse");
    const auto &smoky=world->_decalProfiles[2].fx;
    check(smoky.ground_decal_smoke.enabled&&smoky.ground_decal_smoke.count_min==2&&smoky.ground_decal_smoke.count_max==4,"smoke keys enable smoke");
    check(parse({"new_vehicle 1","ground_decal_id = 2","ground_decal_size = 120_80","mgun_ground_decal_id = 2","mgun_ground_decal_size = 4_8","end",
        "new_weapon 2","ground_decal_id = 2","ground_decal_size = 25_40","end"}),"manual source sizes parse");
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
    check(world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"enabled smoke spawns");
    smoke.chance=0;
    check(!world->SpawnGroundDecalSmoke(smoke,vec3d(),50,123),"zero probability suppresses smoke");
    auto &emitter=world->_groundDecalSmoke.back();World::GroundDecalSmoke::Puff puff;
    check(World::GroundDecalSmoke::Evaluate(emitter,0,1000,&puff)&&std::isfinite(puff.pos.x)&&puff.size>0,"smoke puff geometry finite");
    check(!World::GroundDecalSmoke::Evaluate(emitter,0,emitter.duration,&puff),"smoke stops at authored lifetime");
    world->ClearGroundDecals();
    check(world->_groundDecalSmoke.empty(),"level cleanup releases smoke");
    std::printf("DECAL PROFILE CHECKS %d FAILURES %d\n",checks,failures);
    return failures?1:0;
}
