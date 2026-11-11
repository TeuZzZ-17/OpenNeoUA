// Real-map regression: authored roof placement, idle stability and roof descent.
// Run from the game root with the installed Test_Map-TeuZzZ assets.
#include <bits/stdc++.h>
#define protected public
#define private public
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "common/utils.h"
#include "system/system.h"
#include "system/inivals.h"
#include "nucleus/loaders.h"
#include "world/parsers.h"
#include "world/collision_shape.h"
#include "game/ypatank.h"
#include "game/yparobo.h"
#undef protected
#undef private
#undef main

extern int RegisterNucleusClasses();


int main(int argc, char **argv)
{
    std::setvbuf(stdout,nullptr,_IONBF,0);
    // System initialization can also persist a mode, before GFX::Init runs.
    std::ifstream videoInput("Data/Env/vid.def",std::ios::binary);
    const bool hadVideoFile=videoInput.good();
    const std::string videoMode((std::istreambuf_iterator<char>(videoInput)),std::istreambuf_iterator<char>());
    videoInput.close();
    struct RestoreVideo
    {
        bool existed;const std::string &contents;
        ~RestoreVideo()
        {
            if(existed)std::ofstream("Data/Env/vid.def",std::ios::binary|std::ios::trunc)<<contents;
            else std::remove("Data/Env/vid.def");
        }
    } restoreVideo{hadVideoFile,videoMode};
    System::Init();auto *window=SDL_GL_GetCurrentWindow();
    if(!window){std::printf("No GL context: %s\n",SDL_GetError());return 2;}
    SDL_HideWindow(window);
    FSMgr::iDir::setBaseDir("");
    Common::Env.SetPrefix("data","Data");Common::Env.SetPrefix("env","Data/Env");
    Common::Env.SetPrefix("levels","data:levels");Common::Env.SetPrefix("locale","data:locale");
    Common::Env.SetPrefix("rsrc","data:mc2res");
    // GFX initialization persists its selected mode. Keep the user's exact file.
    RegisterNucleusClasses();System::RescanFonts();TF::Engine.Init();GFX::Engine.Init();
    if(hadVideoFile)std::ofstream("Data/Env/vid.def",std::ios::binary|std::ios::trunc)<<videoMode;
    else std::remove("Data/Env/vid.def");
    const Common::Point resolution(640,480);
    // Keep the native probe hidden at a small fixed drawable size.
    System::SetVideoMode(resolution,0,nullptr);
    GFX::Engine.SetResVariables(resolution);
    GFX::Engine.ApplyResolution();GFX::Engine.RecreateScreenSurface();GFX::Engine.UpdateFBOSizes();
    SDL_PumpEvents();System::ProcessEvents();
    auto actual=System::GetResolution();
    int windowWidth=0,windowHeight=0;SDL_GetWindowSize(window,&windowWidth,&windowHeight);
    std::printf("Requested %dx%d; drawable %dx%d; window %dx%d\n",resolution.x,resolution.y,actual.x,actual.y,windowWidth,windowHeight);
    if(actual!=resolution)return 8;
    _putenv_s("ALSOFT_DRIVERS","null");SFXEngine::SFXe.init();SFXEngine::SFXe.setMasterVolume(0);
    std::printf("GL initialized\n");
    auto world=std::unique_ptr<NC_STACK_ypaworld>(new NC_STACK_ypaworld);
    UserData user;user.clear();user.p_YW=world.get();world->_GameShell=&user;
    world->_screenSize=GFX::Engine.GetVirtualUIResolution();world->_initScriptFilePath="data:scripts/Startup.cfg";
    world->_iniColors[68]=SDL_Color{180,205,209,255};
    if(!world->ypaworld_func166("language"))return 7;
    if(!world->ProtosInit()||!world->yw_LoadSet(3)){std::puts("Cold shell load failed");return 3;}
    std::printf("Shell loaded, font %d\n",world->_fontH);



    int checks=0,failures=0;
    auto check=[&](bool ok,const char *label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}};
    std::printf("VBO enabled: %d\n",GFX::Engine._vbo);
    check(GFX::Engine._vbo,"production VBO renderer active");
    // Replay the reported options in memory; do not edit the user's building script.
    ScriptParser::HandlersList fixture{new World::Parsers::BuildProtoParser(world.get())};
    check(ScriptParser::ParseStringList({"modify_building 78", "building_time = 10",
        "snd_genesis_sample = sounds/lowapp.wav", "snd_genesis_volume = 120", "snd_genesis_pitch = auto",
        "pal_genesis_slot = 1", "pal_genesis_mag0 = 4", "pal_genesis_mag1 = 6",
        "shk_genesis_slot = 1", "shk_genesis_mag0 = 4", "shk_genesis_mag1 = 5", "end"},fixture,0),
        "reported construction options parse");
    auto &prototype=world->_buildProtos[78];
    check(prototype.building_time==10 && prototype.GenesisFX.MainSample.Name=="sounds/lowapp.wav",
          "user building78 construction settings loaded");
    check(prototype.genesis_sound_defined && prototype.GenesisFX.sndPrm.slot==1 && prototype.GenesisFX.sndPrm_shk.slot==1,
          "user sound palette and shake settings loaded");
    world->_mapSize=Common::Point(8,8);world->_cells.Resize(world->_mapSize);
    auto &cell=world->_cells(2,2);cell.Id=18;cell.CellId=Common::Point(2,2);cell.CenterPos=vec3d(2400,0,-2400);
    check(world->BuildingConstructBegin(&cell,78,1,3000),"exact building construction starts");
    world->BuildingConstructUpdate(1);
    check(cell.PurposeType==cellArea::PT_CONSTRUCTING,"first audio and FX update does not crash");
    {
        auto carrier=world->_inBuildProcess[cell.Id].GenesisCarrier;
        check(carrier && carrier->Sounds.size()==2 && carrier->Sounds[0].PSample && carrier->Sounds[0].IsLoop(),"real lowapp sample loops");
        check(carrier && carrier->Sounds[1].PPFx->time==10000 && carrier->Sounds[1].PShkFx->time==10000,"real FX span the 10 s construction");
        world->BuildingConstructUpdate(9000);
        check(cell.PurposeType==cellArea::PT_CONSTRUCTING && carrier->Sounds[1].IsShkEnabled(),"shake still active near the end");
    }
    const int plainGround=world->FindPlainGroundSecType(world->_secTypeArray[prototype.SecType].SurfaceType);
    std::printf("Plain ground sector type for building 78: %d\n",plainGround);
    check(plainGround>=0 && world->_secTypeArray[plainGround].SurfaceType==world->_secTypeArray[prototype.SecType].SurfaceType,
          "Set3 has plain ground of the power station surface");
    std::function<const GFX::TMesh*(NC_STACK_base*)> findReference=[&](NC_STACK_base *base)->const GFX::TMesh* {
        for(const auto &mesh:base->Meshes)
            if((mesh.Mat.Flags&GFX::RFLAGS_TEXTURED)&&(mesh.Mat.Flags&GFX::RFLAGS_DYNAMIC_TEXTURE)&&mesh.Mat.TexSource)return &mesh;
        for(auto *kid:base->GetKidList())if(auto *found=findReference(kid))return found;
        return nullptr;
    };
    auto *reference=findReference(world->_vhclModels[5]);
    check(reference!=nullptr,"real animated Genesis material available");
    if(!reference)return 4;
    std::printf("Genesis frames: %zu\n",reference->CoordsCache.size());
    check(reference->CoordsCache.size()>1,"real Genesis has several frames");
    auto &sector=world->_secTypeArray[prototype.SecType];
    int count=sector.SectorType==1?1:3;
    for(int y=0;y<count;++y)for(int x=0;x<count;++x) {
        auto *base=world->_legoArray[sector.SubSectors.At(x,y)->HPModels[0]].Base;
        auto *instance=base->GenRenderInstance();
        std::vector<std::pair<GLuint,GLuint>> originalBuffers;
        for(const auto &mesh:base->Meshes) originalBuffers.emplace_back(mesh.glDataBuf,mesh.glIndexBuf);
        auto &variants=base->GetRenderMeshes(instance,reference,true);
        auto &again=base->GetRenderMeshes(instance,reference,true);
        check(&variants==&again && variants.front().glDataBuf==again.front().glDataBuf,"GPU variant reused across frames");
        size_t sourceIndex=0;
        for(const auto &mesh:base->Meshes) {
            check(originalBuffers[sourceIndex++]==std::make_pair(mesh.glDataBuf,mesh.glIndexBuf),"original building GPU buffers remain unchanged");
        }
        for(auto &mesh:variants)if(!mesh.Vertexes.empty()&&!mesh.Indixes.empty()) {
            std::printf("LEGO[%d,%d] verts=%zu buffer=%u index=%u\n",x,y,mesh.Vertexes.size(),mesh.glDataBuf,mesh.glIndexBuf);
            check(mesh.glDataBuf!=0 && mesh.glIndexBuf!=0,"construction mesh has GPU buffers before rendering");
            if(!NC_STACK_base::IsSectorTerrainMesh(mesh))
                check(mesh.CoordsCache.size()==reference->CoordsCache.size() && mesh.Mat.TexSource==reference->Mat.TexSource,"construction mesh animated like the vehicle Genesis");
            if(argc>1 && std::string(argv[1])=="--draw") {
                GFX::TRenderNode node(GFX::TRenderNode::TYPE_MESH);
                node.Mesh=&mesh;node.Flags=mesh.Mat.Flags;node.Color=mesh.Mat.Color;node.Tex=mesh.Mat.Tex;
                if(!mesh.CoordsCache.empty()){node.coordsID=(int)mesh.CoordsCache.size()-1;node.Tex=mesh.CoordsCache.back().Tex;}
                node.TForm=mat4x4(vec3d(0,0,-3000));
                GFX::Engine.BeginFrame();GFX::Engine.BeginScene();
                while(glGetError()!=GL_NO_ERROR) {}
                std::puts("Drawing exact construction mesh...");
                GFX::Engine.RenderNode(&node);
                glFinish();
                check(glGetError()==GL_NO_ERROR,"real construction draw completes without GL error");
                GFX::Engine.EndScene();GFX::Engine.EndFrame();
            }
        }
        delete instance;
    }
    world->_inBuildProcess.clear();
    // Like mission_roof_test, this probe initializes loading/rendering rather than the full shell lifecycle.
    world.release();
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
