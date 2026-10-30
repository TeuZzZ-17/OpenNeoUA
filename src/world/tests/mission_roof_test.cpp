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


int main()
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
    auto check=[&](bool ok,const char *label){++checks;if(!ok){++failures;printf("FAIL %s\n",label);}};
    TLevelDescription map;
    check(world->sub_4DA41C(&map,"levels:single/L0101.LDF"),"real Test_Map-TeuZzZ parses");
    if(failures)return 9;
    check(world->LoadTypeMap(map.TypStr)&&world->LoadOwnerMap(map.OwnStr)&&world->LoadHightMap(map.HgtStr),"real level terrain maps load");
    check(world->yw_createRobos(map.Robos)&&world->LoadBlgMap(map.BlgStr),"real hosts and buildings load");
    if(failures)return 10;
    world->_collisionScene.reset(new Collision::Scene(*world));
    std::vector<MapSquad> squads;
    for(auto squad:map.Squads)if(squad.VhclID==1)squads.push_back(squad);
    world->yw_InitSquads(squads);
    for(int y=0;y<world->_mapSize.y;++y)for(int x=0;x<world->_mapSize.x;++x)
        world->CellCheckHealth(&world->_cells(x,y),World::OWNER_RECALC,nullptr);
    world->yw_InitTechUpgradeBuildings();world->InitGates();world->InitSuperItems();world->UpdatePowerEnergy();world->PrepareAllFillers();
    std::vector<NC_STACK_ypabact*> tanks;
    const auto collect=[&](const auto &visit,NC_STACK_ypabact *unit)->void {
        if(unit->_vehicleID==1)tanks.push_back(unit);
        for(auto *child:unit->GetKidList())visit(visit,child);
    };
    for(auto *unit:world->_unitsList)collect(collect,unit);
    check(tanks.size()==10,"the ten authored Weasel squad members are reached, including followers");
    int supportedRoofs=0,descentCases=0;
    for(auto *unit:tanks) {
        ypaworld_arg136 ray;ray.stPos=vec3d(unit->_position.x,-50000,unit->_position.z);ray.vect=vec3d::OY(100000);ray.flags=0;
        world->ypaworld_func136(&ray);
        check(ray.isect&&unit->GetBodyPosition().y<ray.isectPos.y,"authored Weasel starts above the collision surface");
        if(ray.isect&&ray.isectPos.y<unit->_pSector->height-50&&(unit->_status_flg&BACT_STFLAG_LAND))++supportedRoofs;
    }
    check(supportedRoofs>=3,"raised roof units start supported by the native ground controller");
    // Mission start with no vehicle selection, no driving and live squad collisions.
    // Updating the commander also updates all authored followers through AI_layer1.
    std::vector<int> settledEnergy;
    TInputState initialInput{};initialInput.Period=20;user.Input=&initialInput;
    for(int frame=0;frame<1900&&!tanks.empty();++frame) {
        world->_timeStamp+=20;update_msg update{};update.frameTime=20;update.gTime=world->_timeStamp;update.inpt=&initialInput;
        tanks.front()->Update(&update);
        if(frame==99)for(auto *unit:tanks)settledEnergy.push_back(unit->_energy);
    }
    for(size_t i=0;i<tanks.size();++i) {
        printf("AI_START_IDLE gid=%d hp=%d/%d\n",tanks[i]->_gid,settledEnergy[i],tanks[i]->_energy);
        check(tanks[i]->_energy>0&&tanks[i]->_energy==settledEnergy[i],"untouched initial AI squad stays alive with stable energy and live unit collisions");
    }
    user.cockpitCameraRuntimeMode=true;
    for(auto *unit:tanks) {
        unit->setBACT_viewer(true);unit->setBACT_inputting(true);
        // Isolate terrain acceptance from optional squad-to-squad collisions.
        const bool unitCollisions=unit->getBACT_bactCollisions();unit->setBACT_bactCollisions(false);
        TInputState input{};input.Period=20;user.Input=&input;
        auto tick=[&](){world->_timeStamp+=20;update_msg update{};update.frameTime=20;update.gTime=world->_timeStamp;update.inpt=&input;unit->Update(&update);};
        for(int frame=0;frame<100;++frame)tick();
        const int settledEnergy=unit->_energy;int maxRebounds=0;
        for(int frame=0;frame<1800;++frame){tick();maxRebounds=std::max(maxRebounds,(int)unit->_reb_count);}
        check(unit->_energy==settledEnergy&&unit->_energy>0&&maxRebounds<50,"idle Weasel does not keep losing energy or enter a fatal rebound loop");
        printf("ROOF_IDLE gid=%d hp=%d/%d rebounds=%d\n",unit->_gid,settledEnergy,unit->_energy,maxRebounds);
        ypaworld_arg136 roof;roof.stPos=unit->_position;roof.vect=vec3d::OY(100);roof.flags=0;world->ypaworld_func136(&roof);
        if(unit->_position.x>4100&&unit->_position.x<4300&&roof.isect&&roof.isectPos.y<unit->_pSector->height-100) {
            ++descentCases;const auto start=unit->_position;bool airborne=false;double furthest=start.z;
            input.Sliders[4]=1;
            for(int frame=0;frame<500;++frame){tick();airborne|=!(unit->_status_flg&BACT_STFLAG_LAND);furthest=std::max(furthest,unit->_position.z);}
            printf("ROOF_DESCENT gid=%d z=%.3f->%.3f airborne=%d hp=%d\n",unit->_gid,start.z,furthest,airborne,unit->_energy);
            check(airborne&&furthest>start.z+200&&unit->_energy>0,"real sloping-roof edge is crossed and the Weasel falls alive");
        }
        unit->setBACT_bactCollisions(unitCollisions);unit->setBACT_viewer(false);unit->setBACT_inputting(false);
        world->_viewerBact=nullptr;
    }
    check(descentCases>=2,"both real high-edge regression cases were exercised");
    user.Input=nullptr;
    printf("MISSION_ROOF CHECKS=%d FAILED=%d\n",checks,failures);
    // The fixture intentionally initialized only the loading/physics services.
    // The process owns their teardown rather than the interactive shell lifecycle.
    world.release();return failures?1:0;
}
