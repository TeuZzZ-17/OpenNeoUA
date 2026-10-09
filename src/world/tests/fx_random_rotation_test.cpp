// Focused probe linked to the production parser and FX spawn paths.
#include "../../yw.h"
#include "../parsers.h"
#include "../tools.h"
#include <cstdio>
#include <cstdlib>
#undef main
namespace {
int checks=0, failures=0;
void Check(bool ok,const char *name) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",name); } }
bool Same(const mat3x3 &a,const mat3x3 &b) {
    return (a.AxisX()-b.AxisX()).length()<1e-10 &&
           (a.AxisY()-b.AxisY()).length()<1e-10 && (a.AxisZ()-b.AxisZ()).length()<1e-10;
}
struct Fragment : NC_STACK_ypabact { void Bind(NC_STACK_ypaworld *world) { _world=world; } void Renew() override {} size_t SetPosition(bact_arg80 *arg) override { _position=arg->pos; return 1; } size_t SetStateInternal(setState_msg *) override { return 1; } };
struct FXWorld : NC_STACK_ypaworld {
    Fragment fragment; vec3d spawnPos;
    NC_STACK_ypabact *ypaworld_func146(ypaworld_arg146 *arg) override {
        spawnPos=arg->pos; fragment._rotation=mat3x3::Ident(); return &fragment;
    }
    void ypaworld_func134(NC_STACK_ypabact *) override {}
};
void ParserChecks() {
    for(int context=0;context<3;++context) for(const char *value : {"", "0", "1", "2", "-1", "1.0", "invalid"}) {
        NC_STACK_ypaworld world; world._vhclProtos.resize(2); world._weaponProtos.resize(2);
        std::vector<World::TSuperItemProfile> profiles;
        ScriptParser::HandlersList handlers;
        handlers.push_back(context==0 ? (ScriptParser::DataHandler*)new World::Parsers::VhclProtoParser(&world) :
            context==1 ? (ScriptParser::DataHandler*)new World::Parsers::WeaponProtoParser(&world) :
            (ScriptParser::DataHandler*)new World::Parsers::SuperItemProfileParser(&profiles));
        Engine::StringList lines{context==0 ? "new_vehicle 1" : context==1 ? "new_weapon 1" : "begin_superitem_profile"};
        for(const char *mode : {"visual","physical","legacy","ground_decal"}) {
            if(context==2 && std::string(mode)!="visual") continue;
            lines.push_back("begin_fx");
            // Deliberately put the shared parameter before mode.
            if(*value) lines.push_back(std::string("random_rotation = ")+value);
            lines.push_back(std::string("trigger = ")+(context==0 ? "destroyed" : context==1 && std::string(mode)=="ground_decal" ? "impact_world" : "detonate"));
            lines.push_back(std::string("mode = ")+(std::string(mode)=="legacy" ? "physical" : mode));
            if(std::string(mode)=="visual") { lines.push_back("vp_model = 1"); lines.push_back("duration = 1000"); }
            else if(std::string(mode)=="physical") { lines.push_back("vp_model = 1"); lines.push_back("mass = 40"); lines.push_back("radius = 2"); }
            else if(std::string(mode)=="legacy") lines.push_back("physical_vehicle = 1");
            else { lines.push_back("texture = Data/Interface/GroundDecals/impact_01.png"); lines.push_back("size = 50"); lines.push_back("duration = 1000"); }
            lines.push_back("end");
        }
        lines.push_back("end");
        Check(ScriptParser::ParseStringList(lines,handlers,ScriptParser::FLAG_NO_SCOPE_SKIP),"all applicable modes parse");
        const auto &fx=context==0 ? world._vhclProtos[1].chain_fx : context==1 ? world._weaponProtos[1].chain_fx : profiles.back().detonate_chain_fx;
        Check(fx.size()==(context==2 ? 1u : 4u),"all applicable blocks retained");
        for(const auto &config:fx) {
            Check(config.random_rotation==(std::string(value)=="1"),"only exact 1 enables random rotation");
            if(config.mode==World::TChainFXConfig::MODE_GROUND_DECAL)
                Check(config.ground_decal_random_rotation==config.random_rotation,"decal rotation retains shared parameter");
        }
    }
}
}
int main() {
    ParserChecks();
    const mat3x3 source=mat3x3::RotateY(.7);
    srand(77); const int expected=rand(); srand(77);
    Check(Same(World::RandomFXRotation(source,false),source),"disabled orientation unchanged");
    Check(rand()==expected,"disabled orientation consumes no RNG");
    for(int i=0;i<32;++i) {
        const mat3x3 rotation=World::RandomFXRotation(source,true);
        Check(!Same(rotation,source),"enabled orientation changes");
        Check(Same(rotation*rotation.Transpose(),mat3x3::Ident()),"random orientation remains orthonormal");
    }
    NC_STACK_base model; NC_STACK_ypaworld world; world._vhclModels.resize(2); world._vhclModels[1]=&model;
    World::TChainFXConfig config; config.duration=1000; config.visuals.emplace_back(); config.visuals.back().vp=1;
    config.offset_min=config.offset_max=vec3d(10,20,30);
    const vec3d pos(100,200,300);
    world.SpawnChainFX(config,pos,source);
    Check(Same(world._transientVPs.back().rot,source),"visual disabled inherits orientation");
    const vec3d spawn=world._transientVPs.back().pos;
    config.random_rotation=true; world.SpawnChainFX(config,pos,source); world.SpawnChainFX(config,pos,source);
    Check((world._transientVPs.back().pos-spawn).length()<1e-10,"visual rotation preserves offset position");
    Check(!Same(std::next(world._transientVPs.begin())->rot,world._transientVPs.back().rot),"visual instances roll independent orientations");
    Check(world._transientVPs.back().age==0,"random rotation applied at spawn");
    world._transientVPs.clear(); world._vhclModels[1]=nullptr;
    FXWorld physicalWorld; Fragment actor; actor.Bind(&physicalWorld); actor._position=pos; actor._rotation=source;
    World::DestFX physical; physical.ModelID=1; physical.Pos=vec3d(10,0,5);
    actor.StartDestFX(physical); const vec3d velocity=physicalWorld.fragment._fly_dir*physicalWorld.fragment._fly_dir_length;
    const vec3d physicalPos=physicalWorld.spawnPos;
    Check(Same(physicalWorld.fragment._rotation,mat3x3::Ident()),"legacy physical default unchanged");
    actor.StartDestFX(physical,nullptr,nullptr,true);
    Check(!Same(physicalWorld.fragment._rotation,mat3x3::Ident()),"legacy physical random orientation applied");
    Check((physicalWorld.spawnPos-physicalPos).length()<1e-10,"legacy physical spawn position unchanged");
    Check((physicalWorld.fragment._fly_dir*physicalWorld.fragment._fly_dir_length-velocity).length()<1e-10,"legacy physical launch unchanged");
    // Reuse a cached test actor while exercising the production inline spawn.
    NC_STACK_ypaworld inlineWorld; Fragment cached;
    cached.Bind(&inlineWorld); cached._bact_type=BACT_TYPES_FLYER;
    auto cacheRef=inlineWorld._deadCacheList.push_back(&cached);
    inlineWorld._vhclModels.resize(2); inlineWorld._vhclModels[1]=&model;
    World::TChainFXConfig inlineConfig;
    inlineConfig.physical_inline.reset(new World::TChainFXPhysical());
    inlineConfig.physical_inline->vp_models={1};
    inlineConfig.physical_inline->mass=40; inlineConfig.physical_inline->radius=2;
    inlineConfig.physical_inline->force=0;
    const vec3d launch(10,20,30);
    auto *fragment=inlineWorld.SpawnInlinePhysicalFX(inlineConfig,pos,source,launch,1,nullptr);
    Check(fragment==&cached && Same(cached._rotation,source),"inline physical default inherits orientation");
    const vec3d inlineVelocity=cached._fly_dir*cached._fly_dir_length;
    inlineConfig.random_rotation=true;
    fragment=inlineWorld.SpawnInlinePhysicalFX(inlineConfig,pos,source,launch,1,nullptr);
    Check(fragment==&cached && !Same(cached._rotation,source),"inline physical random orientation applied");
    Check((cached._position-pos).length()<1e-10,"inline physical position unchanged");
    Check((cached._fly_dir*cached._fly_dir_length-inlineVelocity).length()<1e-10,"inline physical launch unchanged");

    cacheRef.Detach(); inlineWorld._vhclModels[1]=nullptr;
    NC_STACK_ypaworld superWorld; superWorld._vhclModels.resize(2); superWorld._vhclModels[1]=&model;
    superWorld._superItemProfiles.emplace_back();
    auto &profile=superWorld._superItemProfiles.back(); profile.valid=true;
    config.trigger=World::TChainFXConfig::TRIGGER_DETONATE; profile.detonate_chain_fx.push_back(config);
    superWorld._levelInfo.SuperItems.emplace_back();
    auto &item=superWorld._levelInfo.SuperItems.back(); item.Type=TMapSuperItem::TYPE_BOMB; item.CustomProfileIndex=0;
    waldev silentDriver; auto *savedDriver=SFXEngine::SFXe.digDriver;
    SFXEngine::SFXe.digDriver=&silentDriver;
    superWorld.StartCustomSuperItemDetonation(0);
    SFXEngine::SFXe.digDriver=savedDriver;
    Check(superWorld._transientVPs.size()==1 && !Same(superWorld._transientVPs.back().rot,mat3x3::Ident()),"SuperItem detonation forwards random orientation");
    superWorld._transientVPs.clear(); superWorld._vhclModels[1]=nullptr;
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures); return failures?1:0;
}


