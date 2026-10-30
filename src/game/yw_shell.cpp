// Game shell core: input binding order, shell init/deinit, background, open/close,
// per-frame shell processing, info messages, GUI settings and fonts.
// Screen controls are created in yw_shell_ui_options.cpp and yw_shell_ui_screens.cpp;
// level start, replays, save/load and settings files live in yw_shell_session.cpp.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/framepacer.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"
#include "game/database_session.h"

bool UserData::IsInputBindingRetired(int binding)
{
    switch ( binding )
    {
        case World::INPUT_BIND_GUN_HEIGHT:
        case World::INPUT_BIND_WAPOINT:
        case World::INPUT_BIND_LANDLAYER:
        case World::INPUT_BIND_OWNER:
        case World::INPUT_BIND_HEIGHT:
        case World::INPUT_BIND_MINIMAP:
        case World::INPUT_BIND_LOCKVIEW:
        case World::INPUT_BIND_HELP:
        case World::INPUT_BIND_COCKPIT_CAMERA:
            return true;

        default:
            return false;
    }
}

void UserData::RetireInputBindings(bool clearDefaults)
{
    for ( int binding = 1; binding < World::INPUT_BIND_MAX; ++binding )
    {
        if ( !IsInputBindingRetired(binding) )
            continue;

        TInputConf &cfg = InputConfig[binding];
        cfg.PKeyCode = Input::KC_NONE;
        cfg.NKeyCode = Input::KC_NONE;
        cfg.PKeyCodeBkp = Input::KC_NONE;
        cfg.NKeyCodeBkp = Input::KC_NONE;
        cfg.SetFlags = 0;

        if ( clearDefaults )
        {
            cfg.PKeyCodeDef = Input::KC_NONE;
            cfg.NKeyCodeDef = Input::KC_NONE;
        }

        if ( p_YW )
            p_YW->ReloadInput(binding);
    }
}

void UserData::RebuildInputDisplayOrder()
{
    inputDisplayOrder.clear();
    inputDisplayOrder.reserve(World::INPUT_BIND_MAX - 1);

    for ( int binding = 1; binding < World::INPUT_BIND_MAX; ++binding )
    {
        if ( IsInputBindingRetired(binding) || InputConfigTitle[binding].empty() )
            continue;

        inputDisplayOrder.push_back(binding);
    }

    auto sortKey = [this](int binding)
    {
        std::string key = InputConfigTitle[binding];
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch)
        {
            return (char)std::tolower(ch);
        });
        return key;
    };

    std::stable_sort(inputDisplayOrder.begin(), inputDisplayOrder.end(),
                     [&sortKey](int lhs, int rhs)
    {
        const std::string lhsKey = sortKey(lhs);
        const std::string rhsKey = sortKey(rhs);
        if ( lhsKey == rhsKey )
            return lhs < rhs;
        return lhsKey < rhsKey;
    });
}

int UserData::InputBindingFromDisplayIndex(int displayIndex) const
{
    if ( displayIndex < 0 || displayIndex >= (int)inputDisplayOrder.size() )
        return 0;

    return inputDisplayOrder[displayIndex];
}

int UserData::InputDisplayIndexFromBinding(int binding) const
{
    const auto it = std::find(inputDisplayOrder.begin(), inputDisplayOrder.end(), binding);
    if ( it == inputDisplayOrder.end() )
        return -1;

    return (int)std::distance(inputDisplayOrder.begin(), it);
}

int UserData::InputDisplayCount() const
{
    return (int)inputDisplayOrder.size();
}

void UserData::sub_46D2B4()
{
    int v10 = inpListActiveElement;

    for (int i = 0; i < World::INPUT_BIND_MAX; i++)
        Input::Engine.SetHotKey(i, "nop");

    for (int i = 1; i < World::INPUT_BIND_MAX; i++)
    {
        inpListActiveElement = i;
        p_YW->ReloadInput(i);
    }

    inpListActiveElement = v10;
}

bool NC_STACK_ypaworld::InitGameShell(UserData *usr)
{
    _GameShell = usr;
    usr->p_YW = this;

    _levelInfo.State = TLevelInfo::STATE_MENU;
    usr->EnvMode = ENVMODE_TITLE;

    System::IniConf::ReadFromNucleusIni();

    // OpenNeoUA: Nucleus.ini supplies the default Retro Interface state for new
    // users and profiles that predate the profile-side interface_style key.
    // A saved profile may still override this value when its video block loads.
    const GFX::VirtualUIStyle nucleusInterfaceStyle =
        System::IniConf::UiRetroInterface.Get<bool>()
            ? GFX::VirtualUIStyle::RETRO
            : GFX::VirtualUIStyle::SMOOTH;
    usr->interfaceStyle = nucleusInterfaceStyle;
    usr->confInterfaceStyle = nucleusInterfaceStyle;
    GFX::Engine.SetVirtualUIStyle(nucleusInterfaceStyle);

    usr->RefreshPaletteThemes();
    usr->RefreshMenuFonts();

    _netExclusiveGem = System::IniConf::NetGameExclusiveGem.Get<bool>();

    usr->profiles.clear();
    usr->lang_dlls.clear();

    LoadKeyNames();

    listSaveDir("save:");
    listLocaleDir(usr, "locale:");

    usr->userNameDir = "NEWUSER";
    usr->userNameDirCursor = usr->userNameDir.size();

    usr->IgnoreScoreSaving = true;
    usr->diskListActiveElement = 0;
    usr->inpListActiveElement = 1;

    usr->samples1_info.Clear();
    usr->samples1_info.Resize(World::SOUND_ID_MAX);

    if ( !usr->ShellSoundsLoad() )
    {
        ypa_log_out("Error: Unable to load from Shell.ini\n");
        return false;
    }

    usr->_gfxMode = _gfxMode;
    usr->_gfxModeIndex = GFX::GFXEngine::Instance.GetGfxModeIndex(_gfxMode);

    if (usr->_gfxModeIndex < 0)
        usr->_gfxModeIndex = 0;

    usr->InputConfig[World::INPUT_BIND_DRIVE_DIR]   = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 3,  Input::KC_D, Input::KC_A);
    usr->InputConfig[World::INPUT_BIND_DRIVE_SPEED] = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 4,  Input::KC_W, Input::KC_S);
    usr->InputConfig[World::INPUT_BIND_FLY_DIR]     = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 0,  Input::KC_D, Input::KC_A);
    usr->InputConfig[World::INPUT_BIND_FLY_HEIGHT]  = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 1,  Input::KC_W, Input::KC_S);
    usr->InputConfig[World::INPUT_BIND_FLY_SPEED]   = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 2,  Input::KC_Q, Input::KC_Z);
    usr->InputConfig[World::INPUT_BIND_GUN_HEIGHT]  = UserData::TInputConf(World::INPUT_BIND_TYPE_SLIDER, 5,  Input::KC_2, Input::KC_1);
    usr->InputConfig[World::INPUT_BIND_FIRE]          = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 0, Input::KC_RETURN);
    usr->InputConfig[World::INPUT_BIND_SWITCH_WEAPON] = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 1, Input::KC_CTRL);
    usr->InputConfig[World::INPUT_BIND_CYCLE_TARGET]  = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 6, Input::KC_TAB);
    usr->InputConfig[World::INPUT_BIND_ALTERNATIVE_VIEW]    = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 7, Input::KC_F);
    usr->InputConfig[World::INPUT_BIND_GUN]           = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 2, Input::KC_X);
    usr->InputConfig[World::INPUT_BIND_BRAKE]         = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 3, Input::KC_SPACE);
    usr->InputConfig[World::INPUT_BIND_CAMFIRE]       = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 5, Input::KC_EXTRA7);
    usr->InputConfig[World::INPUT_BIND_HUD]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 25, Input::KC_H);
    usr->InputConfig[World::INPUT_BIND_NEW]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 2,  Input::KC_3);
    usr->InputConfig[World::INPUT_BIND_ADD]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 3,  Input::KC_Q);
    usr->InputConfig[World::INPUT_BIND_ORDER]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 0,  Input::KC_O);
    usr->InputConfig[World::INPUT_BIND_ATTACK]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 1,  Input::KC_SPACE);
    usr->InputConfig[World::INPUT_BIND_CONTROL]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 4,  Input::KC_J);
    usr->InputConfig[World::INPUT_BIND_AUTOPILOT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 7,  Input::KC_B);
    usr->InputConfig[World::INPUT_BIND_MAP]         = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 8,  Input::KC_1);
    usr->InputConfig[World::INPUT_BIND_SQ_MANAGE]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 9,  Input::KC_2);
    usr->InputConfig[World::INPUT_BIND_LANDLAYER]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 10, Input::KC_NUM1);
    usr->InputConfig[World::INPUT_BIND_OWNER]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 11, Input::KC_NUM2);
    usr->InputConfig[World::INPUT_BIND_HEIGHT]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 12, Input::KC_NUM3);
    usr->InputConfig[World::INPUT_BIND_LOCKVIEW]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 14, Input::KC_NUM5);
    usr->InputConfig[World::INPUT_BIND_ZOOMIN]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 16, Input::KC_NUMPLUS);
    usr->InputConfig[World::INPUT_BIND_ZOOMOUT]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 17, Input::KC_NUMMINUS);
    usr->InputConfig[World::INPUT_BIND_MINIMAP]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 18, Input::KC_NUM4);
    usr->InputConfig[World::INPUT_BIND_NEXT_COMM]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 20, Input::KC_F7);
    usr->InputConfig[World::INPUT_BIND_TO_HOST]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 21, Input::KC_F4);
    usr->InputConfig[World::INPUT_BIND_NEXT_UNIT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 22, Input::KC_F6);
    usr->InputConfig[World::INPUT_BIND_TO_COMM]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 23, Input::KC_F5);
    usr->InputConfig[World::INPUT_BIND_QUIT]        = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 24, Input::KC_ESCAPE);
    usr->InputConfig[World::INPUT_BIND_LOG_WND]     = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 27, Input::KC_I);
    usr->InputConfig[World::INPUT_BIND_LAST_MSG]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 31, Input::KC_F8);
    usr->InputConfig[World::INPUT_BIND_PAUSE]       = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 32, Input::KC_P);
    usr->InputConfig[World::INPUT_BIND_TO_ALL]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 37, Input::KC_SHIFT);
    usr->InputConfig[World::INPUT_BIND_AGGR_1]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 38, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_AGGR_2]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 39, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_AGGR_3]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 40, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_AGGR_4]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 41, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_AGGR_5]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 42, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_WAPOINT]     = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 4,  Input::KC_SHIFT);
    usr->InputConfig[World::INPUT_BIND_HELP]        = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 43, Input::KC_F1);
    usr->InputConfig[World::INPUT_BIND_LAST_SEAT]   = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 44, Input::KC_BACKSPACE);
    usr->InputConfig[World::INPUT_BIND_SET_COMM]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 45, Input::KC_L);
    usr->InputConfig[World::INPUT_BIND_ANALYZER]    = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 46, Input::KC_F2);
    usr->InputConfig[World::INPUT_BIND_COCKPIT_CAMERA] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 47, Input::KC_NONE);
    usr->InputConfig[World::INPUT_BIND_SPRINT]      = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 48, Input::KC_LSHIFT);
    usr->InputConfig[World::INPUT_BIND_PLACE_MAP_MARKER] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 49, Input::KC_R);
    usr->InputConfig[World::INPUT_BIND_TOGGLE_UFO_SPY_UI] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 52, Input::KC_SPACE);
    usr->InputConfig[World::INPUT_BIND_MAP_FOCUS] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 53, Input::KC_E);
    usr->InputConfig[World::INPUT_BIND_HIDE_VEHICLE] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 54, Input::KC_C);

    // OpenNeoUA: keep the legacy IDs/type slots reserved for compatibility, but
    // these retired controls are no longer bindable or active.
    usr->RetireInputBindings(true);
    usr->sub_46D2B4();

    windp_arg87 v67;

    if (!_netDriver->GetRemoteStart(&v67) )
    {
        ypa_log_out("Error while remote start check\n");
        return  false;
    }

    if ( v67.isClient )
    {
        _GameShell->netPlayerName = v67.callSIGN;

        if ( v67.isHoster )
            _GameShell->isHost = true;
        else
            _GameShell->isHost = false;

        _GameShell->remoteMode = true;

        usr->netLevelID = 0;
        usr->EnvMode = ENVMODE_NETPLAY;

        if ( usr->isHost )
        {
            usr->lobbyPlayers[ _netDriver->GetMyIndex() ].Ready = true;
            usr->rdyStart = true;
            usr->netSelMode = UserData::NETSCREEN_CHOOSE_MAP;
        }
        else
        {
            usr->netSelMode = UserData::NETSCREEN_INSESSION;
        }

        for(uint32_t i = 0; i < _netDriver->GetPlayerCount(); i++)
            _GameShell->lobbyPlayers[i].Name = _netDriver->GetPlayerName(i);

        usr->update_time_norm = 400;
        usr->flush_time_norm = 400;
    }
    else
    {
        _GameShell->remoteMode = false;
    }

    return true;
}

void NC_STACK_ypaworld::DeinitGameShell()
{
    _GameShell->yw_netcleanup();

    _GameShell->profiles.clear();

    _GameShell->lang_dlls.clear();

    SFXEngine::SFXe.StopPlayingSounds();

    for (NC_STACK_sample * &smpl : _GameShell->samples1)
    {
        if (smpl)
        {
            smpl->Delete();
            smpl = nullptr;
        }
    }

    SFXEngine::SFXe.StopCarrier(&_GameShell->samples1_info);
}

void TMapRegionsNet::UnloadImages()
{
    if ( MenuImage )
    {
        MenuImage->Delete();
        MenuImage = nullptr;
    }
    if ( RolloverImage )
    {
        RolloverImage->Delete();
        RolloverImage = nullptr;
    }
    if ( FinishedImage )
    {
        FinishedImage->Delete();
        FinishedImage = nullptr;
    }
    if ( EnabledImage )
    {
        EnabledImage->Delete();
        EnabledImage = nullptr;
    }
    if ( MaskImage )
    {
        MaskImage->Delete();
        MaskImage = nullptr;
    }
}

NC_STACK_bitmap *yw_LoadDatabaseBackground(bool workshop)
{
    // Database backgrounds are loose Data assets so modders can replace them
    // without editing world.ini. Use the same image loader already used by the
    // shell, preserving all currently supported image formats.
    static const char *extensions[] = {
        "png", "PNG",
        "ilbm", "ILBM",
        "jpg", "JPG",
        "jpeg", "JPEG",
        "webp", "WEBP",
        "bmp", "BMP",
        "tiff", "TIFF",
        "gif", "GIF",
        "pcx", "PCX",
        "svg", "SVG"
    };

    const std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:levels/bg");

    for ( const char *ext : extensions )
    {
        const std::string fileName = std::string(workshop ? "Workshop." : "database.") + ext;
        const std::string dataPath = std::string("data:levels/bg/") + fileName;
        if ( !uaFileExist(dataPath) )
            continue;

        NC_STACK_bitmap *image = Utils::ProxyLoadImage({
            {NC_STACK_rsrc::RSRC_ATT_NAME, fileName},
            {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}
        });

        if ( image )
        {
            Common::Env.SetPrefix("rsrc", oldRsrc);
            ypa_log_out("OpenNeoUA Database background: %s\n", dataPath.c_str());
            return image;
        }

        ypa_log_out("Warning: Could not load Database background %s; trying another supported format.\n", dataPath.c_str());
    }

    Common::Env.SetPrefix("rsrc", oldRsrc);
    return nullptr;
}

void sb_0x4e75e8__sub1(NC_STACK_ypaworld *yw, int mode)
{
    int v37 = 1;

    if ( yw->_globalMapRegions.NumSets )
    {
        std::string oldRsrc = Common::Env.SetPrefix("rsrc", "levels:");

        int v38 = 0;
        int v39 = 65535;
        for (int i = 0; i < yw->_globalMapRegions.NumSets; i++)
        {

            int xx = (yw->_globalMapRegions.background_map[i].Size.x - yw->_screenSize.x);
            int yy = (yw->_globalMapRegions.background_map[i].Size.y - yw->_screenSize.y);

            int sq = sqrt(xx * xx + yy * yy);

            if (sq < v39)
            {
                v38 = i;
                v39 = sq;
            }
        }

        std::string menu_map;
        std::string rollover_map;
        std::string mask_map;
        std::string finished_map;
        std::string enabled_map;

        NC_STACK_bitmap *ilbm_menu_map  = nullptr;
        NC_STACK_bitmap *ilbm_rollover_map = nullptr;
        NC_STACK_bitmap *ilbm_mask_map = nullptr;
        NC_STACK_bitmap *ilbm_finished_map = nullptr;
        NC_STACK_bitmap *ilbm_enabled_map = nullptr;

        switch ( mode )
        {
        case ENVMODE_TITLE:
        case ENVMODE_INPUT:
        case ENVMODE_SETTINGS:
        case ENVMODE_NETPLAY:
        case ENVMODE_SELLOCALE:
        case ENVMODE_ABOUT:
        case ENVMODE_SELPLAYER:
        case ENVMODE_HELP:
            menu_map  = yw->_globalMapRegions.menu_map[v38].PicName;
            rollover_map = yw->_globalMapRegions.settings_map[v38].PicName;
            break;
        case ENVMODE_DATABASE:
        case ENVMODE_WORKSHOP:
            menu_map  = yw->_globalMapRegions.menu_map[v38].PicName;
            ilbm_rollover_map = yw_LoadDatabaseBackground(mode == ENVMODE_WORKSHOP);
            if ( !ilbm_rollover_map )
                rollover_map = yw->_globalMapRegions.settings_map[v38].PicName;
            break;
        case ENVMODE_TUTORIAL:
            menu_map  = yw->_globalMapRegions.tut_background_map[v38].PicName;
            mask_map = yw->_globalMapRegions.tut_mask_map[v38].PicName;
            rollover_map = yw->_globalMapRegions.tut_rollover_map[v38].PicName;
            break;
        case ENVMODE_SINGLEPLAY:
            menu_map  = yw->_globalMapRegions.background_map[v38].PicName;
            rollover_map = yw->_globalMapRegions.rollover_map[v38].PicName;
            finished_map = yw->_globalMapRegions.finished_map[v38].PicName;
            mask_map = yw->_globalMapRegions.mask_map[v38].PicName;
            enabled_map = yw->_globalMapRegions.enabled_map[v38].PicName;
            break;
        default:
            break;
        }

        if ( !menu_map.empty() )
        {
            ilbm_menu_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, menu_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} } );
            if ( !ilbm_menu_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", menu_map.c_str());
                v37 = 0;
            }
        }

        if ( !rollover_map.empty() && !ilbm_rollover_map )
        {
            ilbm_rollover_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, rollover_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_rollover_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", rollover_map.c_str());
                v37 = 0;
            }
        }

        if ( !finished_map.empty() )
        {
            ilbm_finished_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, finished_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_finished_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", finished_map.c_str());
                v37 = 0;
            }
        }

        if ( !enabled_map.empty() )
        {
            ilbm_enabled_map = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, enabled_map},
                {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1} });
            if ( !ilbm_enabled_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", enabled_map.c_str());
                v37 = 0;
            }
        }
        if ( !mask_map.empty() )
        {
            ilbm_mask_map = Utils::ProxyLoadImage({{NC_STACK_rsrc::RSRC_ATT_NAME, mask_map}});
            if ( !ilbm_mask_map )
            {
                ypa_log_out("world.ini: Could not load %s\n", mask_map.c_str());
                v37 = 0;
            }
        }

        Common::Env.SetPrefix("rsrc", oldRsrc);

        if ( !v37 )
        {
            if ( ilbm_menu_map )
            {
                ilbm_menu_map->Delete();
                ilbm_menu_map = nullptr;
            }
            if ( ilbm_rollover_map )
            {
                ilbm_rollover_map->Delete();
                ilbm_rollover_map = nullptr;
            }
            if ( ilbm_finished_map )
            {
                ilbm_finished_map->Delete();
                ilbm_finished_map = nullptr;
            }
            if ( ilbm_enabled_map )
            {
                ilbm_enabled_map->Delete();
                ilbm_enabled_map = nullptr;
            }
            if ( ilbm_mask_map )
            {
                ilbm_mask_map->Delete();
                ilbm_mask_map = nullptr;
            }
        }
        yw->_globalMapRegions.UnloadImages();
        yw->_globalMapRegions.MenuImage = ilbm_menu_map;
        yw->_globalMapRegions.MaskImage = ilbm_mask_map;
        yw->_globalMapRegions.RolloverImage = ilbm_rollover_map;
        yw->_globalMapRegions.FinishedImage = ilbm_finished_map;
        yw->_globalMapRegions.EnabledImage = ilbm_enabled_map;
    }
}

void sb_0x4e75e8__sub0(NC_STACK_ypaworld *yw)
{
    std::array<Common::Rect, 256> regions;

    if ( yw->_globalMapRegions.MaskImage )
    {
        for (int i = 0; i < 256; i++)
        {
            regions[i].left = 10000;
            regions[i].top = 10000;
            regions[i].right = -10000;
            regions[i].bottom = -10000;
        }

        ResBitmap *bitm = yw->_globalMapRegions.MaskImage->GetBitmap();

        SDL_LockSurface(bitm->swTex);
        for (int y = 0; y < bitm->height; y++ )
        {
            uint8_t *ln = ((uint8_t *)bitm->swTex->pixels + y * bitm->swTex->pitch);

            for (int x = 0; x < bitm->width; x++)
            {
                Common::Rect &rgn = regions.at( ln[x] );

                if ( x < rgn.left )
                    rgn.left = x;

                if ( x > rgn.right )
                    rgn.right = x;

                if ( y < rgn.top )
                    rgn.top = y;

                if ( y > rgn.bottom )
                    rgn.bottom = y;
            }
        }

        for (int i = 0; i < 256; i++)
        {
            TMapRegionInfo &minf = yw->_globalMapRegions.MapRegions[i];

            if ( minf.Status != TMapRegionInfo::STATUS_NONE && minf.Status != TMapRegionInfo::STATUS_NETWORK && regions.at(i).IsValid() )
            {
                minf.Rect.left = 2.0 * ((float)(regions[i].left) / (float)bitm->width) + -1.0;
                minf.Rect.right = 2.0 * ((float)(regions[i].right) / (float)bitm->width) + -1.0;
                minf.Rect.top = 2.0 * ((float)(regions[i].top) / (float)bitm->height) + -1.0;
                minf.Rect.bottom = 2.0 * ((float)(regions[i].bottom) / (float)bitm->height) + -1.0;
            }
            else
                minf.Rect = Common::FRect();
        }

        SDL_UnlockSurface(bitm->swTex);
    }
}

void NC_STACK_ypaworld::GameShellInitBkgMode(int mode)
{
    sb_0x4e75e8__sub1(this, mode);
    if ( mode == ENVMODE_TUTORIAL || mode == ENVMODE_SINGLEPLAY )
    {
        _firstContactFaction = 0;
        _briefScreen.Stage = TBriefengScreen::STAGE_NONE;
        _globalMapRegions.SelectedRegion = 0;

        sb_0x4e75e8__sub0(this);

        _tipOfDayId = loadTOD(this, "tod.def");

        int v6 = _tipOfDayId + 1;

        if ( (v6 + 2490) > 2512 )
            v6 = 0;
        writeTOD(this, "tod.def", v6);
    }
}

bool NC_STACK_ypaworld::GameShellInitBkg()
{
    GFX::Engine.raster_func211(Common::Rect (-(_screenSize.x / 2), -(_screenSize.y / 2), _screenSize.x / 2, _screenSize.y / 2) );
    GameShellInitBkgMode(_GameShell->EnvMode);
    return true;
}

bool NC_STACK_ypaworld::OpenGameShell()
{
    printf("OpenGameShell\n");
    SetGameShellVideoMode( _GameShell->IsWindowedFlag() );

    if ( !yw_LoadSet(46) )
    {
        ypa_log_out("Unable to load set for shell\n");
        return false;
    }

    _GameShell->sgmSaveExist = 0;
    _GameShell->confFirstKey = true;
    _GameShell->lastInputEvent = 0;
    _GameShell->p_YW->_upScreenBorder = 0;
    _GameShell->keyCatchMode = false;
    _GameShell->p_YW->_helpURL.clear();
    _GameShell->blocked = false;

    if ( _GameShell->default_lang_dll )
    {
        if (!ypaworld_func166(*_GameShell->default_lang_dll))
        {
            ypa_log_out("ERROR: selected language catalogue could not be loaded.\n");
            return false;
        }
    }
    else
    {
        ypa_log_out("Warning: No Language selected, use default set\n");
    }

    ypaworld_func156__sub1(_GameShell);

    if ( !GameShellInitBkg() )
    {
        ypa_log_out("Could not init level select stuff!\n");
        return false;
    }

    GFX::displ_arg263 v233;

    if (_mousePointers[0])
        v233.bitm = _mousePointers[0]->GetBitmap();
    v233.pointer_id = 1;

    GFX::Engine.SetCursor(v233.pointer_id, 0);

    if ( _GameShell->GFXFlags & World::GFX_FLAG_SOFTMOUSE )
    {
        GFX::Engine.setWDD_cursor(1);
    }
    else
    {
        GFX::Engine.setWDD_cursor(0);
    }

    LoadKeyNames();

    _GameShell->InputConfigTitle[World::INPUT_BIND_PAUSE]       = Locale::Text::Inputs(Locale::INPUTS_PAUSE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_QUIT]        = Locale::Text::Inputs(Locale::INPUTS_QUIT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_DRIVE_DIR]   = Locale::Text::Inputs(Locale::INPUTS_DRIVEDIR);
    _GameShell->InputConfigTitle[World::INPUT_BIND_DRIVE_SPEED] = Locale::Text::Inputs(Locale::INPUTS_DRIVESPD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_GUN_HEIGHT]  = Locale::Text::Inputs(Locale::INPUTS_GUNHGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_HEIGHT]  = Locale::Text::Inputs(Locale::INPUTS_FLYHGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_SPEED]   = Locale::Text::Inputs(Locale::INPUTS_FLYSPD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FLY_DIR]     = Locale::Text::Inputs(Locale::INPUTS_FLYDIR);
    _GameShell->InputConfigTitle[World::INPUT_BIND_BRAKE]       = Locale::Text::Inputs(Locale::INPUTS_STOP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_FIRE]          = Locale::Text::Inputs(Locale::INPUTS_FIRE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SWITCH_WEAPON] = Locale::Text::OpenUA(Locale::OUA_SWITCH_WEAPON);
    _GameShell->InputConfigTitle[World::INPUT_BIND_CYCLE_TARGET]  = Locale::Text::OpenUA(Locale::OUA_CYCLE_TARGET);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ALTERNATIVE_VIEW]    = Locale::Text::OpenUA(Locale::OUA_ALTERNATIVE_VIEW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_GUN]           = Locale::Text::Inputs(Locale::INPUTS_FIREGUN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SET_COMM]    = Locale::Text::Inputs(Locale::INPUTS_MAKECOMM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HUD]         = Locale::Text::Inputs(Locale::INPUTS_HEADUPDISP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AUTOPILOT]   = Locale::Text::Inputs(Locale::INPUTS_AUTOPILOT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ORDER]       = Locale::Text::Inputs(Locale::INPUTS_ORDER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEW]         = Locale::Text::Inputs(Locale::INPUTS_NEW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ADD]         = Locale::Text::Inputs(Locale::INPUTS_ADD);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SQ_MANAGE]   = Locale::Text::Inputs(Locale::INPUTS_FINDER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_1]      = Locale::Text::Inputs(Locale::INPUTS_AGGR1);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_2]      = Locale::Text::Inputs(Locale::INPUTS_AGGR2);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_3]      = Locale::Text::Inputs(Locale::INPUTS_AGGR3);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_4]      = Locale::Text::Inputs(Locale::INPUTS_AGGR4);
    _GameShell->InputConfigTitle[World::INPUT_BIND_AGGR_5]      = Locale::Text::Inputs(Locale::INPUTS_AGGR5);
    _GameShell->InputConfigTitle[World::INPUT_BIND_MAP]         = Locale::Text::Inputs(Locale::INPUTS_MAP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_WAPOINT]     = Locale::Text::Inputs(Locale::INPUTS_SELWAYPT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LANDLAYER]   = Locale::Text::Inputs(Locale::INPUTS_LANDSCAPE);
    _GameShell->InputConfigTitle[World::INPUT_BIND_OWNER]       = Locale::Text::Inputs(Locale::INPUTS_OWNER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HEIGHT]      = Locale::Text::Inputs(Locale::INPUTS_HEIGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_MINIMAP]     = Locale::Text::Inputs(Locale::INPUTS_MAPMINI);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LOCKVIEW]    = Locale::Text::Inputs(Locale::INPUTS_LOCKVW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ZOOMIN]      = Locale::Text::OpenUA(Locale::OUA_ZOOM_IN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ZOOMOUT]     = Locale::Text::OpenUA(Locale::OUA_ZOOM_OUT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LOG_WND]     = Locale::Text::Inputs(Locale::INPUTS_LOGWIN);
    _GameShell->InputConfigTitle[World::INPUT_BIND_CONTROL]     = Locale::Text::Inputs(Locale::INPUTS_CONTROL);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LAST_SEAT]   = Locale::Text::Inputs(Locale::INPUTS_TOLASTOCCUP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ATTACK]      = Locale::Text::Inputs(Locale::INPUTS_FIGHT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_HOST]     = Locale::Text::Inputs(Locale::INPUTS_TOROBO);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_COMM]     = Locale::Text::Inputs(Locale::INPUTS_TOCOMM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEXT_UNIT]   = Locale::Text::Inputs(Locale::INPUTS_NEXTUNIT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_NEXT_COMM]   = Locale::Text::Inputs(Locale::INPUTS_NEXTCOM);
    _GameShell->InputConfigTitle[World::INPUT_BIND_LAST_MSG]    = Locale::Text::Inputs(Locale::INPUTS_JUMPLASTMSNG);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TO_ALL]      = Locale::Text::Inputs(Locale::INPUTS_MSGTOALL);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HELP]        = Locale::Text::Inputs(Locale::INPUTS_HELP);
    _GameShell->InputConfigTitle[World::INPUT_BIND_ANALYZER]    = Locale::Text::Inputs(Locale::INPUTS_ANALYZER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_SPRINT]      = Locale::Text::OpenUA(Locale::OUA_SPRINT);
    _GameShell->InputConfigTitle[World::INPUT_BIND_CAMFIRE]     = Locale::Text::Inputs(Locale::INPUTS_FIREVW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_PLACE_MAP_MARKER] = Locale::Text::OpenUA(Locale::OUA_PLACE_MAP_MARKER);
    _GameShell->InputConfigTitle[World::INPUT_BIND_TOGGLE_UFO_SPY_UI] = Locale::Text::OpenUA(Locale::OUA_TOGGLE_UFO_SPY_UI);
    _GameShell->InputConfigTitle[World::INPUT_BIND_MAP_FOCUS] = Locale::Text::Inputs(Locale::INPUTS_LOCKVW);
    _GameShell->InputConfigTitle[World::INPUT_BIND_HIDE_VEHICLE] = Locale::Text::OpenUA(Locale::OUA_HIDE_VEHICLE_SILHOUETTE);

    // Display only active bindings and sort them by their localized title.
    // Runtime IDs remain unchanged for profile compatibility.
    _GameShell->RebuildInputDisplayOrder();

    if ( _screenSize.x < 512 )
    {
        buttonsSpace = 2;
        vertMenuSpace = 2;
        checkBoxWidth = 8;
        dword_5A50B2 = 210;
        word_5A50AE = 200;
        word_5A50BC = 220;
        word_5A50BA = 300;
        word_5A50BE = 270;
    }
    else
    {
        buttonsSpace = 3;
        vertMenuSpace = 3;
        checkBoxWidth = 16;
        dword_5A50B2 = 380;
        word_5A50AE = 280;
        word_5A50BC = 390;
        word_5A50BA = 500;
        word_5A50BE = 480;
    }

    int menuWidth = _screenSize.x * 0.7;
    int menuHeight = _screenSize.y * 0.8;

    scaledFontHeight = _fontH;
    if ( _screenSize.x >= 512 )
        scaledFontHeight += (_screenSize.y - 384) / 2;

    if ( _screenSize.x < 512 )
        bottomButtonsY = menuHeight - _fontH;
    else
        bottomButtonsY = menuHeight - _fontH - (_screenSize.y - 384) / 2;

    button1LineWidth = ( menuWidth - 2 * buttonsSpace )/ 3;

    bottomCenteredFirstBtnPosX =
        ( menuWidth - 2 * button1LineWidth - buttonsSpace ) / 2;
    bottomCenteredSecondBtnPosX =
        bottomCenteredFirstBtnPosX + button1LineWidth + buttonsSpace;
    bottomSecondBtnPosX = buttonsSpace + button1LineWidth;
    bottomThirdBtnPosX = 2 * buttonsSpace + 2 * button1LineWidth;

    printf("Creating CreateTitleControls\n");
    if (!this->CreateTitleControls()) return false;
    printf("Creating CreateSubBarControls\n");
    if (!this->CreateSubBarControls()) return false;
    printf("Creating CreateConfirmControls\n");
    if (!this->CreateConfirmControls()) return false;

    dword_5A50B2_h = menuWidth - _fontVBScrollW;

    printf("Creating CreateInputControls\n");
    if (!this->CreateInputControls()) return false;
    printf("Creating CreateVideoControls\n");
    if (!this->CreateVideoControls()) return false;
    printf("Creating CreateAtmosphereControls\n");
    if (!this->CreateAtmosphereControls()) return false;
    printf("Creating CreateDiskControls\n");
    if (!this->CreateDiskControls()) return false;
    printf("Creating CreateLocaleControls\n");
    if (!this->CreateLocaleControls()) return false;
    printf("Creating CreateAboutControls\n");
    if (!this->CreateAboutControls()) return false;
    printf("Creating CreateDatabaseControls\n");
    if (!this->CreateDatabaseControls()) return false;

    dword_5A50B6 = menuWidth - _fontVBScrollW;

    if (!this->CreateNetworkControls()) return false;

    switch (_GameShell->EnvMode)
    {
    default:
        _GameShell->titel_button->ShowScreen();
        break;
    case ENVMODE_TUTORIAL:
    case ENVMODE_SINGLEPLAY:
        _GameShell->sub_bar_button->ShowScreen();

        if ( _GameShell->GameIsOver )
        {
            NC_STACK_button::button_66arg v231;
            v231.field_4 = 0;
            v231.butID = 1014;

            _GameShell->sub_bar_button->Disable(&v231);

            v231.butID = 1013;
            _GameShell->sub_bar_button->Disable(&v231);
        }
        break;
    case ENVMODE_NETPLAY:
        _GameShell->network_button->ShowScreen();
        break;
    }

    UpdateGameShell();

    SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 6);

    _GameShell->HasInited = true;

    if ( _GameShell->remoteMode )
    {
        _GameShell->GameShellUiOpenNetwork();
        _GameShell->p_YW->_isNetGame = true;
        _GameShell->FreeFraction = (World::OWNER_GHOR_BIT | World::OWNER_MYKO_BIT | World::OWNER_TAER_BIT);
        _GameShell->SelectedFraction = World::OWNER_RESIST_BIT;
    }
    else
    {
        _GameShell->yw_netcleanup();
        _GameShell->netSelMode = UserData::NETSCREEN_MODE_SELECT;
    }

    _GameShell->netSel = -1;

    if ( _GameShell->p_YW->_preferences & World::PREF_CDMUSICDISABLE )
    {
        SFXEngine::SFXe.StopMusicTrack();
        SFXEngine::SFXe.SetMusicTrack(_GameShell->shelltrack, _GameShell->shelltrack__adv.min_delay, _GameShell->shelltrack__adv.max_delay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    return true;
}

void NC_STACK_ypaworld::CloseGameShell()
{
    if ( _GameShell->HasInited )
    {
        if ( _GameShell->confirm_button )
        {
            _GameShell->confirm_button->HideScreen();
            _GameShell->confirm_button->Delete();
        }
        _GameShell->confirm_button = nullptr;

        if ( _GameShell->sub_bar_button )
        {
            _GameShell->sub_bar_button->HideScreen();
            _GameShell->sub_bar_button->Delete();
        }
        _GameShell->sub_bar_button = nullptr;

        if ( _GameShell->titel_button )
        {
            _GameShell->titel_button->HideScreen();
            _GameShell->titel_button->Delete();
        }
        _GameShell->titel_button = nullptr;

        if ( _GameShell->button_input_button )
        {
            if ( _GameShell->input_listview.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->input_listview );
            _GameShell->input_listview.Free();

            _GameShell->button_input_button->HideScreen();
            _GameShell->button_input_button->Delete();
            _GameShell->button_input_button = nullptr;
        }

        if ( _GameShell->video_button )
        {
            if ( _GameShell->video_listvw.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->video_listvw );
            _GameShell->video_listvw.Free();

            if ( _GameShell->d3d_listvw.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->d3d_listvw );
            _GameShell->d3d_listvw.Free();

            _GameShell->video_button->HideScreen();
            _GameShell->video_button->Delete();
            _GameShell->video_button = nullptr;
        }

        if ( _GameShell->atmosphere_button )
        {
            _GameShell->atmosphere_button->HideScreen();
            _GameShell->atmosphere_button->Delete();
            _GameShell->atmosphere_button = nullptr;
        }

        if ( _GameShell->disk_button )
        {
            if ( _GameShell->disk_listvw.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->disk_listvw );
            _GameShell->disk_listvw.Free();

            _GameShell->disk_button->HideScreen();
            _GameShell->disk_button->Delete();
            _GameShell->disk_button = nullptr;
        }

        if ( _GameShell->locale_button )
        {
            if ( _GameShell->local_listvw.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->local_listvw );
            _GameShell->local_listvw.Free();

            _GameShell->locale_button->HideScreen();
            _GameShell->locale_button->Delete();
            _GameShell->locale_button = nullptr;
        }

        if ( _GameShell->about_button )
        {
            _GameShell->about_button->HideScreen();
            _GameShell->about_button->Delete();
            _GameShell->about_button = nullptr;
        }

        if ( _GameShell->network_button )
        {
            if ( _GameShell->network_listvw.IsOpen() )
                _GameShell->p_YW->GuiWinClose( &_GameShell->network_listvw );
            _GameShell->network_listvw.Free();

            _GameShell->network_button->HideScreen();
            _GameShell->network_button->Delete();
            _GameShell->network_button = nullptr;
        }

        _GameShell->ReleaseDatabasePreview();
        if(_GameShell->database_button)
        {
            _GameShell->database_button->HideScreen();_GameShell->database_button->Delete();
            _GameShell->database_button=nullptr;
        }
        SFXEngine::SFXe.StopPlayingSounds();

        _globalMapRegions.UnloadImages();

        if ( _GameShell->EnvMode == ENVMODE_TUTORIAL || _GameShell->EnvMode == ENVMODE_SINGLEPLAY )
        {
            if ( _levelInfo.State == TLevelInfo::STATE_BRIEFING )
            {
                FreeBriefing();
            }
            else if ( _levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
            {
                FreeDebrief();
            }
        }

        sb_0x44ac24(this);

        _GameShell->HasInited = false;
    }
}

//Draw bkg or briefing
void NC_STACK_ypaworld::GameShellBkgProcess()
{
    if ( _GameShell->EnvModeChanged )
        GameShellInitBkgMode(_GameShell->EnvMode);

    switch(_GameShell->EnvMode)
    {
    case ENVMODE_TUTORIAL:
    case ENVMODE_SINGLEPLAY:
        ypaworld_func158__sub4__sub1();
        break;

    case ENVMODE_TITLE:
        GameShellBlitBkg(_globalMapRegions.MenuImage);
        break;

    default:
        GameShellBlitBkg(_globalMapRegions.RolloverImage);
        break;
    }
}

void draw_tooltip(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent)
{
    if ( yw->_mouseGrabbed || (yw->_toolTipId && !yw->_mouseCursorHidden) )
    {
        int v15 = -(yw->_fontH + yw->_downScreenBorder + yw->_fontH / 4);
        std::string v2;

        if ( yw->_toolTipHotKeyId != -1 )
        {
            int16_t keycode = Input::Engine.GetHotKey(yw->_toolTipHotKeyId);

            if ( keycode != Input::KC_NONE )
            {
                if ( yw->_GameShell && !Input::Engine.KeyTitle.at(keycode).empty())
                {
                    v2 = fmt::sprintf("[%s]", Input::Engine.KeyTitle.at(keycode));
                    v15 = -(yw->_downScreenBorder + 2 * yw->_fontH + yw->_fontH / 4);
                }
            }
        }

        CmdStream buf;
        buf.reserve(1024);

        FontUA::select_tileset(&buf, 15);
        FontUA::set_xpos(&buf, 0);
        FontUA::set_ypos(&buf, v15);

        if ( !v2.empty() )
        {
            FontUA::set_txtColor(&buf, yw->_iniColors[61].r, yw->_iniColors[61].g, yw->_iniColors[61].b);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, v2.c_str(), yw->_screenSize.x);

            FontUA::next_line(&buf);
        }

        FontUA::set_txtColor(&buf, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);

        FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf,  yw->GetTooltipString() , yw->_screenSize.x);

        FontUA::set_end(&buf);

        GFX::Engine.ProcessDrawSeq(buf, nullptr, uiAccent);
    }

    yw->_toolTipHotKeyId = -1;
    yw->_toolTipId = 0;
}

//Make screenshot
void sub_4476AC(NC_STACK_ypaworld *yw)
{
    GFX::Engine.SaveScreenshot(fmt::sprintf("env:snaps/f_%04d", yw->_screenShotCount));

    yw->_screenShotCount++;
}

bool NC_STACK_ypaworld::IsAnyInput(TInputState *struc)
{
    bool click = (struc->ClickInf.flag & ~TClickBoxInf::FLAG_OK);
    bool mousemove = struc->ClickInf.move.ScreenPos.x != PrevMouseX || PrevMouseY != struc->ClickInf.move.ScreenPos.y;

    PrevMouseX = struc->ClickInf.move.ScreenPos.x;
    PrevMouseY = struc->ClickInf.move.ScreenPos.y;

    return struc->KbdLastDown != Input::KC_NONE || struc->KbdLastHit != Input::KC_NONE || struc->HotKeyID >= 0 || click || mousemove;
}

void NC_STACK_ypaworld::ProcessGameShell()
{
    _GameShell->envAction.action = EnvAction::ACTION_NONE;

    SFXEngine::SFXe.SetTimeScale(1.0f);
    SFXEngine::SFXe.sub_423EFC(_GameShell->DTime, vec3d(0.0), vec3d(0.0), mat3x3::Ident());

    GFX::Engine.BeginFrame();
    GFX::Engine.BeginVirtualUI(_screenSize);

    int oldMode = _GameShell->EnvMode;
    _GameShell->EnvModeChanged = false;

    _GameShell->GameShellUiHandleInput();

    if ( oldMode != _GameShell->EnvMode )
        _GameShell->EnvModeChanged = true;

    GameShellBkgProcess();

    draw_tooltip(this);

    ypaworld_func158__sub3(this, _GameShell);

    if ( _isNetGame )
    {
        _netFlushTimer -= _GameShell->DTime;
        if ( _netFlushTimer <= 0 )
        {
            _netDriver->FlushBroadcastBuffer();

            _netFlushTimer = 100;
        }
    }

    SFXEngine::SFXe.UpdateSoundCarrier(&_GameShell->samples1_info);

    SFXEngine::SFXe.sb_0x424c74();

    GFX::Engine.EndVirtualUI();
    GFX::Engine.EndFrame();

    if ( sub_449678(_GameShell->Input, Input::KC_NUMMUL) )
        sub_4476AC(this);

    if ( _GameShell->netSelMode == UserData::NETSCREEN_INSESSION )
    {
        yw_CheckCRCs(this);
    }

    if ( IsAnyInput(_GameShell->Input) )
        _GameShell->lastInputEvent = _GameShell->GlobalTime;

    if ( (_GameShell->GlobalTime - _GameShell->lastInputEvent) > World::CVDemoWait && _GameShell->EnvMode == ENVMODE_TITLE )
        _GameShell->envAction.action = EnvAction::ACTION_REPLAY;

    _GameShell->GameIsOver = false;

    // OpenNeoUA: legacy online help pages are obsolete/dead.
    // Clear pending help URLs instead of launching an external browser.
    if ( !_helpURL.empty() )
        _helpURL.clear();
}

void NC_STACK_ypaworld::ypaworld_func159(yw_arg159 *arg)
{
    if ( arg->MsgID )
        VoiceMessagePlayMsg(arg->unit, arg->Priority, arg->MsgID, arg->IgnoreAudioTimeScale);

    if ( IsSpectatorControlled() && (arg->MsgID || arg->unit) )
        return;

    if ( arg->unit )
        info_log.field_255C = arg->unit->_gid;
    else
        info_log.field_255C = 0;

    info_log.field_2560 = _timeStamp;
    info_log.field_2564 = arg->MsgID;

    if ( !arg->txt.empty() )
    {
        inflog_msg *v6;

        if ( info_log.field_250 >= 5 )
        {
            info_log.msg_count++;

            if ( info_log.msg_count >= 64 )
                info_log.msg_count = 0;

            if ( info_log.field_254 == info_log.msg_count )
            {
                info_log.field_254++;

                if ( info_log.field_254 >= 64 )
                    info_log.field_254 = 0;
            }

            info_log.numEntries++;;

            if ( info_log.numEntries > 64 )
                info_log.numEntries = 64;

            v6 = &info_log.msgs[info_log.msg_count];

            info_log.field_24C = info_log.msg_count;
        }
        else
        {
            info_log.msg_count = info_log.field_24C;

            v6 = &info_log.msgs[info_log.field_24C];
        }

        info_log.field_256C = 5000;
        info_log.field_2568 = 1;
        info_log.field_250 = arg->Priority;

        if ( arg->unit )
            v6->id = arg->unit->_gid;
        else
            v6->id = 0;

        v6->field_8 = 7000;
        v6->field_4 = _timeStamp;

        const char *v5 = arg->txt.c_str();

        int v10 = 0;

        while ( *v5 )
        {
            if ( *v5 == '\n' )
            {
                v6->txt[v10] = 0;

                v10 = 0;

                info_log.msg_count++;

                if ( info_log.msg_count >= 64 )
                    info_log.msg_count = 0;

                if ( info_log.field_254 == info_log.msg_count )
                {
                    info_log.field_254++;

                    if ( info_log.field_254 >= 64 )
                        info_log.field_254 = 0;
                }

                info_log.numEntries++;

                if ( info_log.numEntries > 64 )
                    info_log.numEntries = 64;

                info_log.field_256C += 5000;
                info_log.field_2568++;

                v6 = &info_log.msgs[ info_log.msg_count ];

                if ( arg->unit )
                    v6->id = arg->unit->_gid;
                else
                    v6->id = 0;

                v6->field_8 = 7000;
                v6->field_4 = 0;
            }
            else if ( v10 < 125 )
            {
                v6->txt[v10] = *v5;
                v10++;
            }

            v5++;
        }

        v6->txt[v10] = 0;

        info_log.firstShownEntries = info_log.numEntries - info_log.shownEntries;

        if ( info_log.firstShownEntries < 0 )
            info_log.firstShownEntries = 0;
    }
}

void NC_STACK_ypaworld::ypaworld_func160(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func160");
}

void NC_STACK_ypaworld::UpdateGuiSettings()
{
    Gui::UA::_UATextColor = _iniColors[60];
    Gui::UA::_UAButtonTextColor = _iniColors[68];
}

void NC_STACK_ypaworld::LoadGuiFonts()
{
    std::string old = Common::Env.SetPrefix("rsrc", "data:set46");

    Gui::UA::_UATiles[Gui::UA::TILESET_46DEFAULT] = yw_LoadFont("default.font"); //0
    Gui::UA::_UATiles[Gui::UA::TILESET_46MAPC16] = yw_LoadFont("mapcur16.font"); //18
    Gui::UA::_UATiles[Gui::UA::TILESET_46MAPC32] = yw_LoadFont("mapcur32.font"); //19
    Gui::UA::_UATiles[Gui::UA::TILESET_46ENERGY] = yw_LoadFont("energy.font"); //30

    Common::Env.SetPrefix("rsrc", "data:fonts");
    Gui::UA::_UATiles[Gui::UA::TILESET_DEFAULT]     = yw_LoadFont("default.font"); //0
    Gui::UA::_UATiles[Gui::UA::TILESET_MENUGRAY]    = yw_LoadFont("menugray.font"); //6
    Gui::UA::_UATiles[Gui::UA::TILESET_ICONNS]      = yw_LoadFont("icon_ns.font"); //24
    Gui::UA::_UATiles[Gui::UA::TILESET_ICONPS]      = yw_LoadFont("icon_ps.font"); //25
    Gui::UA::_UATiles[Gui::UA::TILESET_MAPHORZ]     = yw_LoadFont("maphorz.font"); //11
    Gui::UA::_UATiles[Gui::UA::TILESET_MAPVERT]     = yw_LoadFont("mapvert.font"); //12
    Gui::UA::_UATiles[Gui::UA::TILESET_MAPVERT1]    = yw_LoadFont("mapvert1.font"); //13

    Common::Env.SetPrefix("rsrc", old);
}

void NC_STACK_ypaworld::CreateNewGuiElements()
{
    _GameShell->_menuMsgBox = new Gui::UABlockMsgBox(Gui::UAMessageBox::TYPE_INMENU);
    _GameShell->_menuMsgBox->SetEnable(false);
    Gui::Root::Instance.AddWidget(_GameShell->_menuMsgBox);
}

void NC_STACK_ypaworld::DeleteNewGuiElements()
{
    delete _GameShell->_menuMsgBox;
    _GameShell->_menuMsgBox = nullptr;
}
