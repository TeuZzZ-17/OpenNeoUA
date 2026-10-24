// Hidden OpenGL validation of the production menu, real assets and UI composition.
#include <bits/stdc++.h>
#define protected public
#define private public
#include "game/yw.h"
#include "game/database_session.h"
#include "gui/button.h"
#include "common/env.h"
#include "common/utils.h"
#include "system/system.h"
#include "system/inivals.h"
#include "nucleus/loaders.h"
#include "locale/locale.h"
#include "world/parsers.h"
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <direct.h>
#undef protected
#undef private
#undef main

extern int init_classesLists_and_variables();
extern int yw_write_user(FSMgr::FileHandle *,UserData *);
extern std::vector<std::string> db_wrap_text_columns(const std::string &,int,int);


static void Capture(const std::string &path,int width,int height)
{
    auto *surface=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
    std::vector<uint8_t> pixels(width*height*4);
    glReadBuffer(GL_FRONT);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glReadBuffer(GL_BACK);
    for(int y=0;y<height;++y)
        std::memcpy((char*)surface->pixels+y*surface->pitch,pixels.data()+(height-y-1)*width*4,width*4);
    const auto filename=path.substr(0,path.size()-4)+"_"+std::to_string(width)+"x"+std::to_string(height)+".png";
    IMG_SavePNG(surface,filename.c_str());SDL_FreeSurface(surface);
}

int main(int argc,char **argv)
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
    if(argc>3)
    {
        System::IniConf::GfxColorEffects.Value=(int32_t)std::atoi(argv[3]);
        System::IniConf::GfxColorEffects.WasSet=true;
    }
    // GFX initialization persists its selected mode. Keep the user's exact file.
    init_classesLists_and_variables();System::RescanFonts();TF::Engine.Init();GFX::Engine.Init();
    if(hadVideoFile)std::ofstream("Data/Env/vid.def",std::ios::binary|std::ios::trunc)<<videoMode;
    else std::remove("Data/Env/vid.def");
    const Common::Point resolution(argc>1?std::atoi(argv[1]):640,argc>2?std::atoi(argv[2]):480);
    // Exercise arbitrary drawable sizes without depending on the monitor's
    // advertised video modes or writing the user's env/vid.def.
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
    if(!world->ProtosInit()||!world->yw_LoadSet(46)){std::puts("Cold shell load failed");return 3;}
    std::printf("Shell loaded, font %d\n",world->_fontH);
    System::IniConf::GamePlasmaCurrencyEnable.Value=true;
    System::IniConf::GamePlasmaCurrencyEnable.WasSet=true;
    if(!world->CreateTitleControls()||!world->CreateDatabaseControls()||!world->CreateAboutControls())return 4;

    int failures=0,checks=0;
    auto check=[&](bool ok,const char *label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}};
    user.ShowDatabaseMenu();
    check(user.db_session&&user.db_session->loaded,"real model resources load");
    check(!user.db_session->catalog.Entries(0).empty(),"loose scripts produce units");
    check(user.db_session->catalog.Entries(1).empty(),"no weapons tab/catalog");
    check(!user.db_session->catalog.Entries(2).empty(),"loose scripts produce buildings");
    auto draw=[&](const char *name){
        user.DTime=40;user.GlobalTime+=40;
        GFX::Engine.BeginFrame();GFX::Engine.BeginVirtualUI(world->_screenSize);
        user.database_button->Draw();user.RenderDatabaseEntryMedia();
        GFX::Engine.EndVirtualUI();GFX::Engine.EndFrame();
        const std::string path=std::string(std::getenv("OPENNEOUA_PROBE_OUTPUT")?std::getenv("OPENNEOUA_PROBE_OUTPUT"):".")+"/"+name+".png";
        Capture(path,resolution.x,resolution.y);
    };
    draw("database-units");
    check(user.db_session->preview.Available(),"normal-state unit preview");
    user.db_tab=2;user.db_page=0;user.db_selected=0;user.PopulateDatabasePage();draw("database-buildings");
    check(user.db_session->card.valid,"building card binds to shared definitions");
    user.CloseDatabaseMenu();
    // All transactions use a disposable profile under the test workspace.
    const char *savePath=std::getenv("OPENNEOUA_PROBE_SAVE");
    if(!savePath){std::puts("Set OPENNEOUA_PROBE_SAVE to a disposable directory containing probe/");return 9;}
    FSMgr::iDir::replacePath("Data/Save",savePath);
    Common::Env.SetPrefix("save","data:Save");
    user.UserName="probe";
    world->_plasmaCurrencyBank=100;
    world->_vhclProtos[1].purchase_cost=50;
    check(world->WorkshopPurchase(1,50,false),"purchase succeeds and saves profile");
    check(world->GetPlasmaCurrencyBank()==50&&world->GetWorkshopOwned().count(1),"purchase debits once and records ownership");
    check(!world->WorkshopPurchase(1,50,false)&&world->GetPlasmaCurrencyBank()==50,"duplicate purchase cannot debit");
    world->_vhclProtos[2].purchase_cost=100;
    check(!world->WorkshopPurchase(2,100,false)&&world->GetPlasmaCurrencyBank()==50,"insufficient funds cannot debit");
    int host=-1;
    for(size_t id=0;id<world->_vhclProtos.size();++id)
        if(world->_vhclProtos[id].Index>=0&&world->_vhclProtos[id].model_id==BACT_TYPES_ROBO){host=id;break;}
    check(host>=0,"real host definition exists");
    if(host>=0)
    {
        world->_workshopOwned.insert(host);
        check(world->WorkshopEquip(host)&&world->GetWorkshopHost()==host,"owned host equips and saves");
    }
    auto saved=uaOpenFile("save:probe/user.txt","r");std::string data,line;
    while(saved.ReadLine(&line))data+=line;
    check(data.find("workshop_owned = 1")!=std::string::npos,"ownership persisted in user profile");
    check(host<0||data.find("workshop_host = "+std::to_string(host))!=std::string::npos,"equipped host persisted");
    // Explicit fixture costs exercise the real workshop layout without editing game scripts.
    user.ShowWorkshopMenu();
    auto &defs=*user.db_session->definitions;
    for(int id:user.db_session->catalog.Entries(0))defs._vhclProtos[id].purchase_cost=250+id;
    user.db_session->LoadOffers(defs);user.PopulateDatabasePage();draw("workshop");
    for(int id:{UIWidgets::DB_BTN_UNITS,UIWidgets::DB_BTN_BUILDINGS})
    {const auto &b=user.database_button->field_d8[user.database_button->GetIndexByID(id)];check(!(b.flags&NC_STACK_button::FLAG_DRAW),"workshop has no fake header buttons");}
    check(user.db_session->card.valid,"workshop uses the shared card");
    user.CloseDatabaseMenu();
    std::printf("DATABASE_RENDER CHECKS %d FAILURES %d\n",checks,failures);
    return failures?1:0;
}
