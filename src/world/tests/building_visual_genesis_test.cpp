// Optional building Genesis: configuration, animated materials, cache, ground, FX and faction colors.
#include "game/yw.h"
#include "nucleus/base.h"
#include "nucleus/bitmap.h"
#include "system/inivals.h"
#include "world/parsers.h"
#include <cstdio>
#undef main
namespace {
struct TerrainBitmap : NC_STACK_bitmap {
    const std::string getRsrc_name() const override { return "BODEN1.ILBM"; }
};
struct AnimBitmap : NC_STACK_bitmap {
    const std::string getRsrc_name() const override { return "GENES.ANM"; }
};
struct CheckingChild : NC_STACK_base {
    bool excluded=false;
    size_t Render(baseRender_msg *arg, Instance *, bool) override { excluded=arg->excludeSectorTerrainFromTint;return 0; }
    size_t RenderImmediately(baseRender_msg *arg, Instance *) override { excluded=arg->excludeSectorTerrainFromTint;return 0; }
};
int checks=0, failures=0;
void Check(bool ok,const char* label) { ++checks; if(!ok){++failures;std::printf("FAIL %s\n",label);} }
}
int main() {
    auto &key=System::IniConf::GameBuildingVisualGenesis;
    Check(!key.Get<bool>(),"missing setting keeps vanilla");
    Common::Ini::PKeyList keys{&key};
    for (const auto &value : {"1","0","junk","2"}) {
        key.Reset();
        Common::Ini::ParseIniTextOverlay(std::string("game.building_visual_genesis = ")+value+"\n",&keys);
        Check(key.Get<bool>() == (std::string(value)=="1"),"strict boolean setting with invalid fallback");
    }
    NC_STACK_base base;
    GFX::TMesh source;
    source.Mat.Color=GFX::TGLColor(0.4f,0.2f,0.1f,1);
    source.Vertexes.emplace_back(vec3f(0,0,0),tUtV(0,0),source.Mat.Color);
    source.Vertexes.emplace_back(vec3f(1,0,0),tUtV(1,0),source.Mat.Color);
    source.Vertexes.emplace_back(vec3f(0,1,0),tUtV(0,1),source.Mat.Color);
    source.Indixes={0,1,2};
    base.Meshes.push_back(source);
    GFX::TMesh reference;
    reference.Mat.Flags=GFX::RFLAGS_TEXTURED|GFX::RFLAGS_DYNAMIC_TEXTURE|GFX::RFLAGS_LUMTRACY;
    reference.Mat.Color=GFX::TGLColor(0,1,1,1);
    reference.Vertexes=source.Vertexes;
    for(auto &v:reference.Vertexes) v.Color=reference.Mat.Color;
    ResBitmap frame;
    GFX::TCoordsCache cache;
    cache.Tex=&frame;cache.Coords={tUtV(.25f,.5f),tUtV(.5f,.5f),tUtV(.25f,.75f)};
    reference.CoordsCache.push_back(cache);
    NC_STACK_base::Instance instance(&base);
    auto &meshes=base.GetRenderMeshes(&instance,&reference,true);
    Check(&meshes != &base.Meshes && meshes.size()==1,"per-instance plasma geometry");
    auto &plasma=meshes.front();
    Check(!(plasma.Mat.Flags&GFX::RFLAGS_DYNAMIC_TEXTURE) && plasma.CoordsCache.empty(),"reference without frame source stays static");
    Check(plasma.Mat.Tex==&frame,"uses first Genesis atlas frame");
    Check(plasma.Vertexes[1].Pos==source.Vertexes[1].Pos && plasma.Indixes==source.Indixes,"geometry unchanged");
    Check(plasma.Vertexes[0].Color.b==1 && base.Meshes.front().Vertexes[0].Color.b==.1f,"source materials unchanged");
    Check(plasma.Vertexes[1].TexCoord.tu<=.5f && plasma.Vertexes[1].TexCoord.tv>=.5f,"UV stays inside static atlas region");
    Check(&base.GetRenderMeshes(&instance,&reference,true).front()==&plasma,"cache reused across frames");
    Check(&base.GetRenderMeshes(&instance,nullptr,true)==&base.Meshes && instance.genesisMeshes.empty(),"completion returns original and frees variant");
    // Animated reference: every frame of the vehicle Genesis reaches the building.
    AnimBitmap anim;
    ResBitmap frame2;
    GFX::TMesh animated=reference;
    animated.Mat.TexSource=&anim;
    GFX::TCoordsCache cache2;
    cache2.Tex=&frame2;cache2.Coords={tUtV(.5f,0),tUtV(.75f,0),tUtV(.5f,.25f)};
    animated.CoordsCache.push_back(cache2);
    {
        NC_STACK_base::Instance animInstance(&base);
        auto &moving=base.GetRenderMeshes(&animInstance,&animated,true).front();
        Check((moving.Mat.Flags&GFX::RFLAGS_DYNAMIC_TEXTURE) && moving.Mat.TexSource==&anim,"plasma keeps the animated frame source");
        Check(moving.CoordsCache.size()==2 && moving.CoordsCache[0].Tex==&frame && moving.CoordsCache[1].Tex==&frame2,"one UV set per Genesis frame");
        Check(moving.CoordsCache[1].Coords.size()==moving.Vertexes.size(),"frame UVs cover every vertex");
        const tUtV &uv=moving.CoordsCache[1].Coords[1];
        Check(uv.tu>=.5f && uv.tu<=.75f && uv.tv>=0 && uv.tv<=.25f,"frame UVs stay inside that frame region");
        base.GetRenderMeshes(&animInstance,nullptr,true);
    }
    NC_STACK_base root;
    CheckingChild child;
    root.GetKidList().push_back(&child);
    baseRender_msg message;message.excludeSectorTerrainFromTint=true;message.genesisReference=&reference;
    root.Render(&message);
    Check(child.excluded,"queued child terrain remains excluded from plasma");
    child.excluded=false;root.RenderImmediately(&message);
    Check(child.excluded,"immediate child terrain remains excluded from plasma");
    message.genesisReference=nullptr;root.Render(&message);
    Check(!child.excluded,"ordinary building tint keeps legacy child handling");
    root.GetKidList().clear();
    TerrainBitmap terrain;
    GFX::TMesh ground=source;ground.Mat.TexSource=&terrain;
    base.Meshes.push_back(ground);
    auto &withGround=base.GetRenderMeshes(&instance,&reference,true);
    Check(withGround.back().Mat.TexSource==&terrain && withGround.back().Vertexes[0].Color.b==.1f,"sector terrain retains original material and colors");
    base.GetRenderMeshes(&instance,nullptr,true);
    NC_STACK_ypaworld world;
    for (int owner=0;owner<=6;++owner) world._iniColors[owner]=SDL_Color{(Uint8)(owner*31),(Uint8)(owner*17),(Uint8)(owner*9),255};
    world._buildProtos.resize(8);world._mapSize=Common::Point(8,8);world._cells.Resize(world._mapSize);
    ScriptParser::HandlersList handlers{new World::Parsers::BuildProtoParser(&world)};
    Check(ScriptParser::ParseStringList({"new_building 3", "model = kraftwerk", "building_time = 10",
        "snd_genesis_sample = 0", "snd_genesis_volume = 125", "snd_genesis_pitch = -300",
        "pal_genesis_slot = 1", "pal_genesis_mag0 = 0.25", "pal_genesis_mag1 = 0",
        "shk_genesis_slot = 1", "shk_genesis_mag0 = 0.5", "shk_genesis_mag1 = 0", "end",
        "new_building 4", "model = building", "pal_genesis_slot = 2", "pal_genesis_mag0 = 0.75", "end"},handlers,0),"all individual building Genesis keys parse");
    for (const char *removed : {"pal_genesis_time = 1000", "shk_genesis_time = 1000"}) {
        ScriptParser::HandlersList timed{new World::Parsers::BuildProtoParser(&world)};
        Check(!ScriptParser::ParseStringList({"new_building 6", "model = building", removed, "end"},timed,0),"palette/shake time keys removed");
    }
    ScriptParser::HandlersList invalid{new World::Parsers::BuildProtoParser(&world)};
    Check(ScriptParser::ParseStringList({"new_building 5", "model = building", "snd_genesis_volume = junk",
        "pal_genesis_slot = -1", "pal_genesis_mag0 = nan",
        "shk_genesis_slot = junk", "shk_genesis_mag0 = inf", "end"},invalid,0),"invalid optional numeric effects never block loading");
    Check(!world._buildProtos[5].genesis_sound_defined,"invalid sound value retains host fallback");
    auto &prototype=world._buildProtos[3];
    Check(prototype.genesis_sound_defined && prototype.genesis_effects_defined,"explicit building sound and effects tracked");
    Check(prototype.GenesisFX.volume==125 && prototype.GenesisFX.pitch_min==-300,"per-building audio settings stored");
    Check(prototype.GenesisFX.sndPrm_shk.mute==0.02f && prototype.GenesisFX.sndPrm_shk.pos.x==0.2 &&
          prototype.GenesisFX.sndPrm_shk.pos.y==0.2 && prototype.GenesisFX.sndPrm_shk.pos.z==0.2,"shake has vehicle default axes");
    auto &cell=world._cells(2,2);cell.Id=18;cell.CellId=Common::Point(2,2);cell.CenterPos=vec3d(2400,0,-2400);
    Check(world.BuildingConstructBegin(&cell,3,1,3000),"custom building construction starts");
    auto carrier=world._inBuildProcess[cell.Id].GenesisCarrier;
    Check(carrier && carrier->Sounds.size()==2,"construction owns independent sound carrier");
    if(carrier) {
        auto &sound=carrier->Sounds[0];
        auto &effects=carrier->Sounds[1];
        Check(sound.IsLoop() && !sound.IsPFx() && !sound.IsShk(),"sample loops until construction completes");
        Check(!effects.IsLoop() && effects.IsPFx() && effects.IsShk(),"palette and shake work even without audio sample");
        Check(effects.PPFx && effects.PShkFx && effects.PPFx!=&prototype.GenesisFX.sndPrm,"effects owned by this construction");
        Check(effects.PPFx->time==10000 && effects.PShkFx->time==10000,"palette and shake last the whole construction");
        Check(effects.PPFx->mag0==0.25f && effects.PShkFx->mag0==0.5f && effects.PShkFx->pos.x==0.2,"building magnitudes kept");
        Check(effects.PFxMag==0.25f && effects.ShkMag==0.5f,"effects start at mag0");
        Check(sound.Pitch==-4640,"10-second pitch matches legacy vehicle cost1000");
        Check(carrier->Position==cell.CenterPos,"sound is emitted at the building");
    }
    auto &other=world._cells(3,2);other.Id=19;other.CellId=Common::Point(3,2);
    Check(world.BuildingConstructBegin(&other,4,2,3000),"second building starts independently");
    auto second=world._inBuildProcess[other.Id].GenesisCarrier;
    Check(second && second!=carrier && second->Sounds[1].PPFx->mag0==0.75f && second->Sounds[1].PPFx->time==3000,"simultaneous buildings retain separate effects");
    Check(world._inBuildProcess[cell.Id].GroundSecType==-1,"no emulated ground while visual Genesis is off");
    Check(!world._buildProtos[4].genesis_sound_defined,"palette-only definition retains legacy build audio fallback");
    world._inBuildProcess.clear();carrier.reset();second.reset();
    // Emulated ground: only a full 3x3 sector made of BODEN meshes qualifies.
    {
        NC_STACK_base plainGround, house;
        plainGround.Meshes.push_back(ground);
        house.Meshes.push_back(source);
        world._legoArray[2].Base=&plainGround;
        world._legoArray[3].Base=&house;
        world._subSectorArray[1].HPModels[0]=2;
        world._subSectorArray[2].HPModels[0]=3;
        auto fill=[&](int type,int sub,uint8_t layout,uint8_t surface) {
            auto &desc=world._secTypeArray[type];desc.SectorType=layout;desc.SurfaceType=surface;
            for(int z=0;z<3;++z)for(int x=0;x<3;++x)desc.SubSectors.At(x,z)=&world._subSectorArray[sub];
        };
        fill(10,2,0,1);  // buildings: never ground
        fill(11,1,1,1);  // single-cell layout: skipped
        fill(12,1,0,3);  // plain ground of another surface
        fill(13,1,0,1);  // plain ground, matching surface
        Check(world.FindPlainGroundSecType(1)==13,"plain ground of the same surface preferred");
        Check(world.FindPlainGroundSecType(4)==12,"any plain ground as fallback");
        world._legoArray[2].Base=nullptr;world._legoArray[3].Base=nullptr;
        Check(world.FindPlainGroundSecType(1)==-1,"no plain ground keeps legacy look");
        for(int type:{10,11,12,13}) world._secTypeArray[type]=TSectorDesc();
    }
    GFX::TGLColor color;
    Check(!world.GetPlasmaFactionTint(1,&color),"Resistance keeps original cyan");
    Check(!world.GetPlasmaFactionTint(0,&color),"neutral keeps source plasma");
    for(int owner=2;owner<=6;++owner) {
        Check(world.GetPlasmaFactionTint(owner,&color),"other factions use shared owner tint");
        auto expected=world.GetColor(owner);
        Check(std::abs(color.r-expected.r/255.f)<.0001f && std::abs(color.g-expected.g/255.f)<.0001f,"matches loaded faction palette");
    }
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
