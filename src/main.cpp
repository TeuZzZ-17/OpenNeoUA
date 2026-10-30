// Program entry point: engine start-up, the main loop and the switch between
// menu (game shell), gameplay and replay screens.
#define GLOBAL
#include <fstream>
#include "system/fsmgr.h"
#include "common/utils.h"
#include "system/system.h"
#include "common/env.h"

#include "common/includes.h"
#include "system/gfx.h"
#include "system/inpt.h"
#include "system/framepacer.h"
#include "platform/winp.h"
#include "platform/wintimer.h"

#include "nucleus/ade.h"
#include "nucleus/area.h"
#include "nucleus/amesh.h"
#include "nucleus/bitmap.h"
#include "nucleus/bmpAnm.h"
#include "nucleus/base.h"
#include "nucleus/ilbm.h"
#include "nucleus/particle.h"
#include "nucleus/embed.h"
#include "nucleus/3ds.h"
#include "nucleus/image.h"
#include "nucleus/obj3d.h"
#include "platform/network.h"
#include "platform/windp.h"
#include "game/ypabact.h"
#include "game/ypatank.h"
#include "game/ypacar.h"
#include "game/ypaflyer.h"
#include "game/yparobo.h"
#include "game/ypaufo.h"
#include "game/yw.h"
#include "gui/font.h"
#include "gui/button.h"
#include "gui/widget.h"
#include "gui/uawidgets.h"
#include "gui/uamsgbox.h"
#include "gui/uaempty.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "world/blacksecttint.h"
#include "world/energyfx.h"
#include "common/crashdiag.h"

namespace
{

enum GAME_SCREEN_MODE {
    GAME_SCREEN_MODE_UNKNOWN = 0,
    GAME_SCREEN_MODE_MENU = 1,
    GAME_SCREEN_MODE_GAME = 2,
    GAME_SCREEN_MODE_REPLAY = 3
};

GAME_SCREEN_MODE GameScreenMode;
UserData userdata;

// Netplay and replays change the player settings; when they end, the copy
// saved in settings.tmp is restored.
bool restoreSettingsOnExit = false;
// The menu demo loop chains the next demo replay when the current one ends.
bool chainDemoReplays = false;
// Index of the demo replay shown last, so the loop does not repeat it.
int lastDemoIndex = 0;

bool tformInited = false;
bool audioInited = false;
bool inputInited = false;

int DiagnosticLevelId()
{
    return ypaworld ? ypaworld->GetLevelInfo().LevelID : -1;
}

bool RestoreSavedSettings()
{
    if ( !restoreSettingsOnExit )
        return true;

    if ( !ypaworld->LoadSettings("settings.tmp", userdata.UserName,
                                 World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER,
                                 false) )
        return false;

    restoreSettingsOnExit = false;
    return true;
}

// Opens the game shell again after a level or a replay. Returns false when the
// shell cannot be opened (fatal).
bool ReturnToMenu()
{
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
    world_update_arg.TimeStamp = 0;
    userdata.lastInputEvent = 0;

    if ( !ypaworld->OpenGameShell() )
    {
        ypa_log_out("GameShell-Error!!!\n");
        ypaworld->DeinitGameShell();
        return false;
    }

    GameScreenMode = GAME_SCREEN_MODE_MENU;
    return true;
}

// Picks a demo replay from env:snaps, avoiding the one shown last time.
std::string PickDemoReplay(uint32_t seed)
{
    if ( userdata.snaps.empty() )
        return std::string();

    int index = seed % userdata.snaps.size();

    if ( index == lastDemoIndex )
    {
        if ( index )
            index--;
        else if ( userdata.snaps.size() > 1 )
            index = 1;
    }

    lastDemoIndex = index;
    return userdata.snaps[index];
}

int ProcessGameplayFrame()
{
    CrashDiag::SetPhase("GameplayWorldProcess");
    ypaworld->Process(&world_update_arg);
    CrashDiag::SetPhase("GameplayStateTransition");

    const TLevelInfo &levelInfo = ypaworld->GetLevelInfo();

    switch ( levelInfo.State )
    {
    case TLevelInfo::STATE_QUIT_GAME:
        CrashDiag::Breadcrumb("LEVEL", "quit requested level=%d", levelInfo.LevelID);
        return 0;

    case TLevelInfo::STATE_COMPLETED:
    case TLevelInfo::STATE_ABORTED:
    {
        CrashDiag::Breadcrumb("LEVEL", "leave level=%d state=%d", levelInfo.LevelID, levelInfo.State);
        CrashDiag::SetPhase("DeleteLevel");
        const bool aborted = levelInfo.State == TLevelInfo::STATE_ABORTED;
        ypaworld->DeleteLevel();

        if ( aborted )
            restoreSettingsOnExit = true;
        if ( !RestoreSavedSettings() )
            return 0;

        // A remote-mode session quits after its level instead of returning to the menu.
        const bool keepRunning = !userdata.remoteMode;

        if ( userdata.EnvMode == ENVMODE_NETPLAY )
        {
            userdata.returnToTitle = true;
            userdata.EnvMode = ENVMODE_SINGLEPLAY;
        }
        else
        {
            userdata.returnToTitle = false;
        }

        userdata.GameIsOver = true;

        if ( !ReturnToMenu() )
            return 0;

        Input::Engine.QueryInput(&input_states);
        if ( !keepRunning )
            return 0;
    }
    break;

    case TLevelInfo::STATE_RESTART:
    {
        CrashDiag::Breadcrumb("LEVEL", "restart level=%d", levelInfo.LevelID);
        const int levelId = levelInfo.LevelID;
        const int tacticalMapZoom = ypaworld->GetTacticalMapZoomLevel();

        CrashDiag::SetPhase("RestartDeleteLevel");
        ypaworld->DeleteLevel();

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.rst", userdata.UserName, levelId) ) )
            ypa_log_out("Warning, load error\n");
        else
            ypaworld->SetTacticalMapZoomLevel(tacticalMapZoom); // Restart keeps the player's zoom.

        Input::Engine.QueryInput(&input_states);
    }
    break;

    case TLevelInfo::STATE_SAVE:
        if ( !ypaworld->SaveGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
            ypa_log_out("Warning, Save error\n");

        Input::Engine.QueryInput(&input_states);
        break;

    case TLevelInfo::STATE_LOAD:
        CrashDiag::Breadcrumb("LEVEL", "load save from level=%d", levelInfo.LevelID);
        CrashDiag::SetPhase("LoadDeleteLevel");
        ypaworld->DeleteLevel();

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
            ypa_log_out("Warning, load error\n");

        Input::Engine.QueryInput(&input_states);
        break;

    default:
        break;
    }

    return 1;
}

// Advances replay playback and handles its keys. Returns false when the
// player leaves the replay (or the next demo cannot be loaded).
bool UpdateReplayPlayback(base_64arg *arg)
{
    TGameRecorder *player = userdata.p_YW->_replayPlayer;

    if ( chainDemoReplays && player->field_74 - 3 <= player->frame_id )
    {
        ypaworld->ypaworld_func164();

        if ( !ypaworld->ypaworld_func162( PickDemoReplay(arg->TimeStamp) ) )
        {
            GameScreenMode = GAME_SCREEN_MODE_MENU;
            Input::Engine.QueryInput(&input_states);
            return false;
        }

        GameScreenMode = GAME_SCREEN_MODE_REPLAY;
    }

    ypaworld->ypaworld_func163(arg);

    // Replay control keys: field_0 is the replay command, frame the step size.
    yw_arg165 control;
    control.field_0 = 0;
    control.frame = 0;

    switch ( arg->field_8->KbdLastHit )
    {
    case Input::KC_S: control.field_0 = 1; break;
    case Input::KC_P: control.field_0 = 2; break;
    case Input::KC_R: control.field_0 = 3; break;
    case Input::KC_N: control.field_0 = 4; break;
    case Input::KC_B: control.field_0 = 5; break;
    case Input::KC_V: control.field_0 = 7; control.frame = -10; break;
    case Input::KC_M: control.field_0 = 7; control.frame = 10; break;
    case Input::KC_SPACE:
    case Input::KC_ESCAPE:
        return false;
    default:
        break;
    }

    if ( control.field_0 )
        ypaworld->ypaworld_func165(&control);

    return true;
}

int ProcessReplayFrame()
{
    if ( UpdateReplayPlayback(&world_update_arg) )
        return 1;

    ypaworld->ypaworld_func164();

    if ( !RestoreSavedSettings() )
        return 0;

    if ( !ReturnToMenu() )
        return 0;

    userdata.p_YW->_levelInfo.State = TLevelInfo::STATE_MENU;
    input_states = TInputState();
    Input::Engine.QueryInput(&input_states);
    return 1;
}

// Closes the shell before a level, a saved game or a replay starts.
bool LeaveMenu()
{
    userdata.SaveSettings();
    ypaworld->CloseGameShell();
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
    return userdata.SaveBuildProtoState();
}

int ProcessMenuFrame()
{
    userdata.GlobalTime = world_update_arg.TimeStamp;
    userdata.DTime = world_update_arg.DTime;
    userdata.Input = &input_states;

    ypaworld->ProcessGameShell();

    switch ( userdata.envAction.action )
    {
    case EnvAction::ACTION_QUIT:
        return 0;

    case EnvAction::ACTION_PLAY:
    case EnvAction::ACTION_RESTART_MISSION:
    {
        CrashDiag::Breadcrumb("LEVEL", "start requested level=%d", userdata.envAction.params[0]);
        CrashDiag::SetPhase("LevelStart");
        if ( !LeaveMenu() )
            return 0;

        yw_arg161 level;
        level.lvlID = userdata.envAction.params[0];
        level.field_4 = 0;
        level.freshStart = userdata.envAction.action == EnvAction::ACTION_RESTART_MISSION;

        if ( !ypaworld->ypaworld_func183(&level) )
        {
            ypa_log_out("Sorry, unable to init this level!\n");
            ypaworld->DeinitGameShell();
            return 0;
        }
        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    break;

    case EnvAction::ACTION_LOAD:
    {
        CrashDiag::Breadcrumb("LEVEL", "load save requested");
        CrashDiag::SetPhase("LevelLoad");
        const int levelId = ypaworld->GetLevelInfo().LevelID;
        if ( !LeaveMenu() )
            return 0;

        if ( !ypaworld->LoadGame( fmt::sprintf("save:%s/%d.sgm", userdata.UserName, 0) ) )
        {
            ypa_log_out("Error while loading level (level %d, User %s\n", levelId, userdata.UserName.c_str());
            ypaworld->DeinitGameShell();
            return 0;
        }
        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    break;

    case EnvAction::ACTION_NETPLAY:
    {
        CrashDiag::Breadcrumb("LEVEL", "netplay start requested level=%d", userdata.envAction.params[0]);
        CrashDiag::SetPhase("NetLevelStart");
        if ( !LeaveMenu() )
            return 0;

        restoreSettingsOnExit = true;

        yw_arg161 level;
        level.lvlID = userdata.envAction.params[0];
        level.field_4 = 0;

        if ( !ypaworld->ypaworld_func179(&level) )
        {
            ypa_log_out("Sorry, unable to init this level for network!\n");
            ypaworld->DeinitGameShell();
            return 0;
        }

        GameScreenMode = GAME_SCREEN_MODE_GAME;
        Input::Engine.QueryInput(&input_states);
    }
    break;

    case EnvAction::ACTION_REPLAY:
    {
        std::string replayName = PickDemoReplay(world_update_arg.TimeStamp);
        if ( replayName.empty() )
        {
            userdata.lastInputEvent = world_update_arg.TimeStamp;
            return 1;
        }

        if ( !userdata.SaveBuildProtoState() )
            return 0;

        restoreSettingsOnExit = true;
        ypaworld->CloseGameShell();
        GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;

        if ( ypaworld->ypaworld_func162(replayName) )
        {
            chainDemoReplays = true;
            GameScreenMode = GAME_SCREEN_MODE_REPLAY;
        }
        else
        {
            ypa_log_out("Sorry, unable to init player!\n");
            if ( !ReturnToMenu() )
                return 0;
        }

        Input::Engine.QueryInput(&input_states);
    }
    break;

    default:
        break;
    }

    return 1;
}

// Game clock units (1024 per second) that this frame advances.
//
// The clock always follows real time: the simulation steps once per rendered
// frame with the measured delta, so the game runs at the same speed at 60 or
// 240 FPS and every extra frame only adds fluidity. The delta is at least one
// unit (some code divides by it) and at most 256 units (1/4 s), so a stall
// such as alt-tab cannot become one giant physics step.
//
// Netplay keeps the original +1 bias per frame: the network flush and sync
// code was written around it.
int32_t FrameClockDelta(int32_t measured)
{
    const bool netGame = GameScreenMode == GAME_SCREEN_MODE_GAME && ypaworld && ypaworld->_isNetGame;
    if ( netGame )
        return measured + 1;

    return std::max(1, std::min(measured, 256));
}

} // namespace

int ProcessNextFrame()
{
    CrashDiag::FrameBegin(world_update_arg.TimeStamp, GameScreenMode, DiagnosticLevelId());
    CrashDiag::SetPhase("InputQuery");

    input_states = TInputState();
    Input::Engine.QueryInput(&input_states);

    if ( userdata.ResetInputPeriod )
    {
        input_states.Period = 1;
        userdata.ResetInputPeriod = false;
    }

    input_states.Period = FrameClockDelta(input_states.Period);

    world_update_arg.DTime = input_states.Period;
    world_update_arg.field_8 = &input_states;
    world_update_arg.TimeStamp += input_states.Period;

    System::SetReleativeMouse(ypaworld->_mouseGrabbed);

    CrashDiag::SetPhase("GuiTimers");
    // In-game and replay GUI timers follow the game clock (with time scale)
    // and are advanced inside the world frame; menus use platform time.
    if ( GameScreenMode != GAME_SCREEN_MODE_GAME &&
         GameScreenMode != GAME_SCREEN_MODE_REPLAY )
        Gui::Root::Instance.TimersUpdate(input_states.Period);

    int result = 1;
    switch ( GameScreenMode )
    {
    case GAME_SCREEN_MODE_MENU:
        CrashDiag::SetPhase("MenuFrame");
        result = ProcessMenuFrame();
        break;

    case GAME_SCREEN_MODE_GAME:
        CrashDiag::SetPhase("GameplayFrame");
        result = ProcessGameplayFrame();
        break;

    case GAME_SCREEN_MODE_REPLAY:
        CrashDiag::SetPhase("ReplayFrame");
        result = ProcessReplayFrame();
        break;

    default:
        break;
    }

    CrashDiag::SetPhase("FrameComplete");
    CrashDiag::FrameEnd(world_update_arg.TimeStamp, GameScreenMode, DiagnosticLevelId());
    return result;
}

int RegisterNucleusClasses()
{
    ypa_log__ypa_general_log();
    init_d3dlog();
    init_dinputlog();

    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_nucleus>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_rsrc>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_bitmap>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_skeleton>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ilbm>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_sklt>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ade>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_area>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_base>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_bmpanim>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_amesh>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_particle>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_embed>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_idev>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_itimer>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_sample>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_wav>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_button>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_network>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_winp>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_wintimer>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_windp>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaworld>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypabact>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypatank>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_yparobo>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypamissile>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaflyer>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypacar>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypaufo>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_ypagun>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_3ds>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_image>() );
    Nucleus::ClassList.push_back( Nucleus::MakeClassDescr<NC_STACK_Obj3D>() );

    return 1;
}

namespace
{

void DeinitEngines()
{
    if ( tformInited )
        TF::Engine.Deinit();
    if ( inputInited )
        Input::Engine.Deinit();
    if ( audioInited )
        SFXEngine::SFXe.deinit();

    ypa_log_out("Nucleus shutdown:\n");
}

// Registers the Nucleus classes and starts the audio, input and transform engines.
bool InitEngines()
{
    ypaworld = nullptr;
    GameScreenMode = GAME_SCREEN_MODE_UNKNOWN;
    userdata.clear();
    input_states = TInputState();
    world_update_arg = base_64arg();

    RegisterNucleusClasses();

    Common::Env.AddGlobalIniKey("gfx.display  = win3d.class");
    Common::Env.AddGlobalIniKey("gfx.display2 = windd.class");
    Common::Env.AddGlobalIniKey("gfx.engine     = gfx.engine");
    Common::Env.AddGlobalIniKey("tform.engine   = tform_ng.engine");
    Common::Env.AddGlobalIniKey("input.engine   = input.engine");
    Common::Env.AddGlobalIniKey("input.wimp     = winp");
    Common::Env.AddGlobalIniKey("input.keyboard = winp");
    Common::Env.AddGlobalIniKey("input.slider[10] = winp:mousex");
    Common::Env.AddGlobalIniKey("input.slider[11] = winp:mousey");
    Common::Env.AddGlobalIniKey("input.slider[12] = winp:joyx winp:joyrudder");
    Common::Env.AddGlobalIniKey("input.slider[13] = winp:joyy");
    Common::Env.AddGlobalIniKey("input.slider[14] = winp:joythrottle");
    Common::Env.AddGlobalIniKey("input.slider[15] = winp:joyhatx");
    Common::Env.AddGlobalIniKey("input.slider[16] = winp:joyhaty");
    Common::Env.AddGlobalIniKey("input.button[16] = winp:joyb0");
    Common::Env.AddGlobalIniKey("input.button[17] = winp:joyb1");
    Common::Env.AddGlobalIniKey("input.button[18] = winp:joyb2");
    Common::Env.AddGlobalIniKey("input.button[19] = winp:joyb3");
    Common::Env.AddGlobalIniKey("input.button[20] = winp:joyb4");
    Common::Env.AddGlobalIniKey("input.button[21] = winp:joyb5");
    Common::Env.AddGlobalIniKey("input.button[22] = winp:joyb6");
    Common::Env.AddGlobalIniKey("input.button[23] = winp:joyb7");

    audioInited = SFXEngine::SFXe.init();
    inputInited = Input::Engine.Init();
    tformInited = TF::Engine.Init();

    const char *failed = !audioInited ? "audio" : !tformInited ? "tform" : !inputInited ? "input" : nullptr;
    if ( failed )
    {
        ypa_log_out("Couldn't open %s engine!\n", failed);
        DeinitEngines();
        return false;
    }

    return true;
}

// Reads the default player from env:user.def and loads that player's settings
// (SDU7 when the file or the player is missing).
bool LoadDefaultUserSettings()
{
    std::string settingsFileName = "sdu7/user.txt";
    userdata.UserName = "SDU7";

    if ( FSMgr::FileHandle *userDef = uaOpenFileAlloc("env:user.def", "r") )
    {
        std::string line;
        userDef->ReadLine(&line);
        delete userDef;

        if ( FSMgr::FileHandle *userTxt = uaOpenFileAlloc(fmt::sprintf("save:%s/user.txt", line), "r") )
        {
            delete userTxt;
            userdata.UserName = line;
            settingsFileName = fmt::sprintf("%s/user.txt", line);
        }
        else
        {
            ypa_log_out("Warning: default user file doesn't exist (save:%s/user.txt)\n", line.c_str());
        }
    }
    else
    {
        ypa_log_out("Warning: No default user set\n");
    }

    userdata.diskListActiveElement = -1;

    int element = 1;
    for ( const auto &profile : userdata.profiles )
    {
        if ( !StriCmp(profile.name, userdata.UserName) )
        {
            userdata.diskListActiveElement = element;
            break;
        }
        element++;
    }

    return ypaworld->LoadSettings(settingsFileName, userdata.UserName, World::SDF_ALL, true, true) != 0;
}

void ReadSnapsDir()
{
    FSMgr::DirIter dir = uaOpenDir("env:snaps/");
    if ( !dir )
        return;

    FSMgr::iNode *entry;
    while ( dir.getNext(&entry) )
    {
        if ( entry->getType() == FSMgr::iNode::NTYPE_FILE && !StriCmp(entry->getName().substr(0, 4), "demo") )
            userdata.snaps.push_back( fmt::sprintf("env:snaps/%s", entry->getName()) );
    }
}

// Creates the world object and opens the game shell on the default player.
bool InitWorld()
{
    // The original 1998 build date: the world class and save files expect it.
    const std::string buildDate = "Jul 09 1998  23:52:47";

    ypaworld = Nucleus::CInit<NC_STACK_ypaworld>( { {NC_STACK_ypaworld::YW_ATT_BUILD_DATE, buildDate} } );
    Gui::UA::yw = ypaworld;

    if ( !ypaworld )
    {
        ypa_log_out("Unable to init ypaworld.class\n");
        return false;
    }
    if ( !ypaworld->InitGameShell(&userdata) )
    {
        ypa_log_out("Unable to init shell structure\n");
        return false;
    }
    if ( !LoadDefaultUserSettings() )
    {
        ypa_log_out("Unable to init game with default settings\n");
        return false;
    }
    if ( !userdata.HasInited && !ypaworld->OpenGameShell() )
    {
        ypa_log_out("Error: Unable to open Gameshell\n");
        return false;
    }

    GameScreenMode = GAME_SCREEN_MODE_MENU;
    ReadSnapsDir();
    return true;
}

bool InitGame()
{
    if ( !InitEngines() )
        return false;

    std::vector<std::string> &cmdl = System::GetCmdLineArray();
    int32_t i = System::FindCmdLineArg("-env");
    if ( i >= 0 && i + 1 < (int32_t)cmdl.size() )
        Common::Env.SetPrefix("env", cmdl[i + 1]);

    if ( InitWorld() )
        return true;

    DeinitEngines();
    return false;
}

void ShutdownWorld()
{
    if ( ypaworld )
    {
        if ( GameScreenMode == GAME_SCREEN_MODE_GAME )
        {
            ypaworld->DeleteLevel();
            ypaworld->DeinitGameShell();
        }
        else if ( GameScreenMode == GAME_SCREEN_MODE_MENU )
        {
            userdata.SaveSettings();
            ypaworld->CloseGameShell();
            ypaworld->DeinitGameShell();
        }

        ypaworld->Delete();
    }

    DeinitEngines();
}

} // namespace

int main(int argc, char *argv[])
{
    for ( int i = 0; i < argc; ++i )
        System::AddCmdLine( std::string(argv[i]) );

    System::IniConf::Init();
    FSMgr::iDir::setBaseDir("");

    // OpenNeoUA.ini is required: it carries the shipped configuration of the game.
    {
        const std::string openNeoUAIni = uaDataFirstOpenNeoUAIniPath();
        if ( !std::ifstream(openNeoUAIni).good() )
        {
            const std::string message = "OpenNeoUA.ini was not found (" + openNeoUAIni +
                                        ").\nPut it in the Data folder and start the game again.";
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenNeoUA", message.c_str(), nullptr);
            return 1;
        }
    }

    System::IniConf::ReadFromNucleusIni();
    System::Init(!System::IniConf::GfxVBO.Get<bool>());

    GFX::Engine.Init();
    System::Movie.Init();
    Gui::UA::Init();

    if ( !InitGame() )
        return 0;

    System::IniConf::ReadFromNucleusIni();

    CrashDiag::Init(System::IniConf::GameCrashDiagnostics.Get<bool>(),
                    std::string("OpenNeoUA ") + __DATE__ + " " + __TIME__);
    CrashDiag::Breadcrumb("STARTUP", "runtime initialized");

    // Cache global runtime configuration after Nucleus.ini is parsed.
    World::BlackSectTint::Init();
    World::EnergyFX::Init();

    Gui::Root::Instance.SetHwCompose(true);
    ypaworld->LoadGuiFonts();
    ypaworld->CreateNewGuiElements();

    while ( ProcessNextFrame() && !System::ProcessEvents() )
        System::Frames.EndFrame();

    CrashDiag::DisarmWatchdog();
    CrashDiag::SetPhase("Shutdown");
    ypaworld->DeleteNewGuiElements();

    ShutdownWorld();
    Gui::UA::Deinit();

    CrashDiag::Shutdown();
    System::Deinit();

    return 0;
}
