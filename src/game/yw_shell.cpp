// Game shell: menu controls and screens, settings files, input bindings, video mode and language.
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
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"

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

NC_STACK_bitmap *yw_LoadDatabaseBackground()
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
        const std::string fileName = std::string("database.") + ext;
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
            menu_map  = yw->_globalMapRegions.menu_map[v38].PicName;
            ilbm_rollover_map = yw_LoadDatabaseBackground();
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

//Controls creation methods
bool NC_STACK_ypaworld::CreateTitleControls(){
    _GameShell->titel_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y} } );
    if ( !_GameShell->titel_button )
    {
        ypa_log_out("Unable to create Titel-Button-Object\n");
        return false;
    }

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_up = 18;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.field_3A = 30;
    btn_64arg.xpos = _screenSize.x * 0.3328125;
    btn_64arg.ypos = _screenSize.y * 0.2291666666666666;
    btn_64arg.width = _screenSize.x / 3;
    btn_64arg.caption = Locale::Text::Title(Locale::TITLE_GAME);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
    btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SINGLE_PLAYER_UP;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->titel_button->Add(&btn_64arg) )
    {
        btn_64arg.ypos = _screenSize.y * 0.3083333333333334;
        btn_64arg.caption = Locale::Text::Title(Locale::TITLE_NETWORK);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_MULTIPLAYER_UP;
        btn_64arg.pressedCode = 0;
        btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
        btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;

        if ( _GameShell->titel_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = _screenSize.x * 0.3328125;
            btn_64arg.ypos = _screenSize.y * 0.4333333333333334;
            btn_64arg.width = _screenSize.x / 3;
            btn_64arg.caption = Locale::Text::Title(Locale::TITLE_INPUT);
            btn_64arg.caption2.clear();
            btn_64arg.pressedCode = 0;
            btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
            btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
            btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_INPUT_SETTINGS_UP;

            if ( _GameShell->titel_button->Add(&btn_64arg) )
            {
                btn_64arg.ypos = _screenSize.y * 0.5125;
                btn_64arg.caption = Locale::Text::Title(Locale::TITLE_SETTINGS);
                btn_64arg.caption2.clear();
                btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_OPTIONS_UP;
                btn_64arg.pressedCode = 0;
                btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;

                if ( _GameShell->titel_button->Add(&btn_64arg) )
                {
                    btn_64arg.ypos = _screenSize.y * 0.5916666666666667;
                    btn_64arg.caption = Locale::Text::Title(Locale::TITLE_PROFILE);
                    btn_64arg.caption2.clear();
                    btn_64arg.pressedCode = 0;
                    btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                    btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SAVE_LOAD_UP;
                    btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD;

                    if ( _GameShell->titel_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = _screenSize.x * 0.890625;
                        btn_64arg.ypos = _screenSize.y * 0.9583333333333334;
                        btn_64arg.width = _screenSize.x * 0.1;
                        btn_64arg.caption = Locale::Text::Title(Locale::TITLE_LOCALE);
                        btn_64arg.caption2.clear();
                        btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_LANGUAGE_UP;
                        btn_64arg.pressedCode = 0;
                        btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                        btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;

                        if ( _GameShell->titel_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = _screenSize.x * 0.3328125;
                            btn_64arg.ypos = _screenSize.y * 0.7166666666666667;
                            btn_64arg.width = _screenSize.x / 3;
                            btn_64arg.caption = Locale::Text::Title(Locale::TITLE_DATABASE);
                            btn_64arg.caption2.clear();
                            btn_64arg.pressedCode = 0;
                            btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                            btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
                            btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_HELP_UP;

                            if ( _GameShell->titel_button->Add(&btn_64arg) )
                            {
                                btn_64arg.ypos = _screenSize.y * 0.7958333333333333;
                                btn_64arg.caption = Locale::Text::Title(Locale::TITLE_QUIT);
                                btn_64arg.caption2.clear();
                                btn_64arg.upCode = UIWidgets::MAIN_MENU_EVENT_IDS::BTN_QUIT_UP;
                                btn_64arg.pressedCode = 0;
                                btn_64arg.downCode = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
                                btn_64arg.button_id = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;

                                if ( _GameShell->titel_button->Add(&btn_64arg) )
                                    v70 = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    if ( !v70 )
    {
        ypa_log_out("Unable to add button to Titel\n");
        return false;
    }

    NC_STACK_button::button_66arg v228;

    if ( _GameShell->lang_dlls.size() <= 1 )
    {
        v228.field_4 = 0;
        v228.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
        _GameShell->titel_button->Disable(&v228);
    }

    _GameShell->titel_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateSubBarControls(){
    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;
    dword_5A50B6_h = _screenSize.x / 4 - 20;

    _GameShell->sub_bar_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)(_screenSize.y - _fontH)},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_fontH}});

    if ( !_GameShell->sub_bar_button )
    {
        ypa_log_out("Unable to create Button-Object\n");
        return false;
    }

    v70 = 0;

    btn_64arg.tileset_down = 19;
    btn_64arg.field_3A = 30;
    btn_64arg.ypos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.tileset_up = 18;
    btn_64arg.xpos = dword_5A50B6_h + buttonsSpace;
    btn_64arg.width = dword_5A50B6_h;
    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_REWIND);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 1251;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1011;
    btn_64arg.upCode = 1016;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 2 * (buttonsSpace + dword_5A50B6_h);
        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_STEPFWD);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1020;
        btn_64arg.pressedCode = 1018;
        btn_64arg.button_id = 1013;

        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_START);
            btn_64arg.caption2.clear();
            btn_64arg.upCode = 1019;
            btn_64arg.pressedCode = 0;
            btn_64arg.downCode = 1251;
            btn_64arg.button_id = 1014;

            if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = (_screenSize.x - 3 * dword_5A50B6_h - 2 * buttonsSpace);
                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_GOTOLDSV);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.downCode = 1251;
                btn_64arg.button_id = 1020;
                btn_64arg.upCode = 1026;

                if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                {
                    btn_64arg.xpos = (_screenSize.x - 2 * dword_5A50B6_h - buttonsSpace);
                    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_LOAD);
                    btn_64arg.caption2.clear();
                    btn_64arg.upCode = 1021;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.downCode = 1251;
                    btn_64arg.button_id = 1015;

                    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = _screenSize.x - dword_5A50B6_h;
                        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_GOBACK);
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.downCode = 1251;
                        btn_64arg.button_id = 1019;
                        btn_64arg.upCode = 1013;

                        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                        {
                            int playAsWidth = _screenSize.x * 0.36;

                            btn_64arg.xpos = (_screenSize.x - playAsWidth) / 2;
                            btn_64arg.width = playAsWidth;
                            btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_PLAY_AS);
                            btn_64arg.caption2.clear();
                            btn_64arg.pressedCode = 0;
                            btn_64arg.downCode = 1251;
                            btn_64arg.button_id = 1027;
                            btn_64arg.upCode = 1027;

                            if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                                v70 = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add button to sub-bar\n");
        return false;
    }
    NC_STACK_button::button_66arg v228;
    if ( _GameShell->sgmSaveExist != 1 )
    {
        v228.butID = 1015;
        v228.field_4 = 0;
        _GameShell->sub_bar_button->Disable(&v228);
    }

    v228.field_4 = 0;
    v228.butID = 1014;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1013;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1011;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1027;
    _GameShell->sub_bar_button->Disable(&v228);

    _GameShell->sub_bar_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateConfirmControls()
{

    _GameShell->confirm_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y}} );
    if ( !_GameShell->confirm_button )
    {
        ypa_log_out("Unable to create Confirm-Button-Object\n");
        return false;
    }

    NC_STACK_button::button_64_arg btn_64arg;
    const SDL_Color boxTextColor = GetFactionBoxTextColor();
    btn_64arg.tileset_up = 18;
    btn_64arg.tileset_down = 19;
    btn_64arg.field_3A = 30;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.xpos = _screenSize.x * 0.25;
    btn_64arg.ypos = _screenSize.y * 0.53125;
    btn_64arg.width = _screenSize.x * 0.125;
    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
    btn_64arg.caption2.clear();
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.upCode = 1350;
    btn_64arg.downCode = 1251;
    btn_64arg.button_id = 1300;
    btn_64arg.txt_r = boxTextColor.r;
    btn_64arg.txt_g = boxTextColor.g;
    btn_64arg.txt_b = boxTextColor.b;

    if ( _GameShell->confirm_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = _screenSize.x * 0.625;
        btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
        btn_64arg.upCode = 1351;
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1301;
        btn_64arg.downCode = 1251;
        btn_64arg.pressedCode = 0;

        if ( _GameShell->confirm_button->Add(&btn_64arg) )
        {
            btn_64arg.tileset_down = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_up = 16;
            btn_64arg.field_3A = 16;
            btn_64arg.xpos = _screenSize.x * 0.25;
            btn_64arg.ypos = _screenSize.y * 0.4375;
            btn_64arg.caption = " ";
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 1302;
            btn_64arg.width = _screenSize.x * 0.5;
            btn_64arg.txt_r = boxTextColor.r;
            btn_64arg.txt_g = boxTextColor.g;
            btn_64arg.txt_b = boxTextColor.b;

            if ( _GameShell->confirm_button->Add(&btn_64arg) )
            {
                btn_64arg.button_id = 1303;
                btn_64arg.ypos = _screenSize.y * 0.46875;
                btn_64arg.caption = " ";
                btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                btn_64arg.caption2.clear();

                _GameShell->confirm_button->Add(&btn_64arg);
            }
        }
    }
    NC_STACK_button::button_66arg v228;
    v228.field_4 = 0;
    v228.butID = 1300;
    _GameShell->confirm_button->Disable(&v228);

    v228.butID = 1301;
    _GameShell->confirm_button->Disable(&v228);

    _GameShell->confirm_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateInputControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    const int inputActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
    const int inputActionButtonStep = inputActionButtonWidth + buttonsSpace;

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = _GameShell->InputDisplayCount();
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 8;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = dword_5A50B2_h;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;
    args.wheelScroll = true;

    if ( !_GameShell->input_listview.Init(this, args) )
    {
        ypa_log_out("Unable to create Input-ListView\n");
        return false;
    }

    _GameShell->input_listview.x = posLeftPaddingX;
    _GameShell->input_listview.y = scaledFontHeight + (vertMenuSpace + _fontH) * 4;

    _GameShell->button_input_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});
    if ( !_GameShell->button_input_button )
    {
        ypa_log_out("Unable to create Input-Button\n");
        return false;
    }
    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.xpos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TITLE);
    btn_64arg.downCode = 0;
    btn_64arg.caption2.clear();
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = 1057;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->button_input_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1058;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->button_input_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.pressedCode = 0;
            btn_64arg.button_id = 1059;

            if ( _GameShell->button_input_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.button_id = 1060;

                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.field_3A = 30;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                    btn_64arg.xpos = menuWidth / 6;
                    btn_64arg.caption = "g";
                    btn_64arg.caption2 = "g";
                    btn_64arg.upCode = 1051;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.flags = 0;
                    btn_64arg.ypos = 6 * vertMenuSpace + 14 * _fontH;
                    btn_64arg.width = checkBoxWidth;
                    btn_64arg.downCode = 1050;
                    btn_64arg.button_id = 1050;

                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 16;
                        btn_64arg.tileset_up = 16;
                        btn_64arg.field_3A = 16;
                        btn_64arg.xpos = (checkBoxWidth + buttonsSpace + menuWidth / 6);
                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                        btn_64arg.width = (menuWidth / 2 - buttonsSpace);
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_JOYSTICK);
                        btn_64arg.button_id = 2;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 0;
                        btn_64arg.pressedCode = 0;
                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[60].r;
                        btn_64arg.txt_g = _iniColors[60].g;
                        btn_64arg.txt_b = _iniColors[60].b;

                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                        {
                            btn_64arg.tileset_down = 19;
                            btn_64arg.field_3A = 30;
                            btn_64arg.tileset_up = 18;
                            btn_64arg.caption = "g";
                            btn_64arg.caption2 = "g";
                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                            btn_64arg.xpos = buttonsSpace + (menuWidth / 2);
                            btn_64arg.width = checkBoxWidth;
                            btn_64arg.downCode = 1058;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.button_id = 1061;
                            btn_64arg.upCode = 1059;
                            btn_64arg.flags = 0;

                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 16;
                                btn_64arg.tileset_up = 16;
                                btn_64arg.field_3A = 16;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                btn_64arg.xpos = (checkBoxWidth + (menuWidth / 2) + 2 * buttonsSpace);
                                btn_64arg.width = ((menuWidth / 2) - buttonsSpace);
                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_ALTJOYMODEL);
                                btn_64arg.caption2.clear();
                                btn_64arg.downCode = 0;
                                btn_64arg.upCode = 0;
                                btn_64arg.pressedCode = 0;
                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                btn_64arg.button_id = 2;
                                btn_64arg.txt_r = _iniColors[60].r;
                                btn_64arg.txt_g = _iniColors[60].g;
                                btn_64arg.txt_b = _iniColors[60].b;

                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.field_3A = 30;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.xpos = menuWidth / 3;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.upCode = 1055;
                                    btn_64arg.button_id = 1055;
                                    btn_64arg.ypos = 7 * vertMenuSpace + (15 * _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.flags = 0;
                                    btn_64arg.downCode = 1056;

                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.field_3A = 16;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.xpos = (checkBoxWidth + (menuWidth / 3) + buttonsSpace);
                                        btn_64arg.width = menuWidth / 2;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_FF);
                                        btn_64arg.button_id = 2;
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;

                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                            btn_64arg.field_3A = 30;
                                            btn_64arg.ypos = bottomButtonsY;
                                            btn_64arg.width = inputActionButtonWidth;
                                            btn_64arg.xpos = 2 * inputActionButtonStep;
                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_DELETE);
                                            btn_64arg.downCode = 1251;
                                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.upCode = 1057;
                                            btn_64arg.button_id = 1056;
                                            btn_64arg.txt_r = _iniColors[68].r;
                                            btn_64arg.txt_g = _iniColors[68].g;
                                            btn_64arg.txt_b = _iniColors[68].b;

                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                            {
                                                // OpenNeoUA: keep all Input Settings actions aligned in one
                                                // compact bottom row: OK / Back / Delete / Reset Defaults.
                                                btn_64arg.xpos = 3 * inputActionButtonStep;
                                                btn_64arg.ypos = bottomButtonsY;
                                                btn_64arg.width = inputActionButtonWidth;
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_RESETDEF);
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.upCode = 1053;
                                                btn_64arg.button_id = 1053;

                                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.ypos = bottomButtonsY;
                                                    btn_64arg.width = inputActionButtonWidth;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1051;
                                                    btn_64arg.upCode = 1052;
                                                    btn_64arg.downCode = 1251;

                                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.button_id = 1052;
                                                        btn_64arg.pressedCode = 0;

                                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = inputActionButtonStep;
                                                            btn_64arg.ypos = bottomButtonsY;
                                                            btn_64arg.width = inputActionButtonWidth;
                                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                            btn_64arg.upCode = 1054;
                                                            btn_64arg.button_id = 1054;
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.pressedCode = 0;

                                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                            {
                                                                v70 = 1;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add input-button\n");
        return false;
    }

        // OpenNeoUA: remove deprecated online-help button from Input Settings.
    _GameShell->button_input_button->Remove(1052);

_GameShell->button_input_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateVideoControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;

    int v261 = 0;
    int v3 = 0;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();

    for ( const GFX::TGFXDeviceInfo &dev : devices )
    {
        if ( dev.isCurrent )
        {
            _GameShell->win3d_guid = dev.guid ;

            if ( !StriCmp(dev.name, "software") )
                _GameShell->win3d_name = Locale::Text::Advanced(Locale::ADV_SOFTWARE);
            else
                _GameShell->win3d_name = dev.name;

            v3 = v261;
            break;
        }
        v261++;
    }

    int v294 = menuWidth - 3 * buttonsSpace - _fontVBScrollW;
    int v94 = (menuWidth - 3 * buttonsSpace - _fontVBScrollW) * 0.6;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = GFX::GFXEngine::Instance.GetAvailableModes().size();
    args.shownEntries = 4;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 4;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = v94;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->video_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create Game-Video-Menu\n");
        return false;
    }

    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = devices.size();
    args.shownEntries = 4;
    args.firstShownEntry = 0;
    args.selectedEntry = v3;
    args.maxShownEntries = 4;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = v94;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->d3d_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create D3D-Menu\n");
        return false;
    }

    _GameShell->video_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});

    if ( !_GameShell->video_button )
    {
        ypa_log_out("Unable to create Video-Button\n");
        return false;
    }

    int v98 = v294 * 0.4;
    int v99 = posLeftPaddingX + buttonsSpace + v98;

    _GameShell->video_listvw.x = v99;
    _GameShell->video_listvw.y = 4 * vertMenuSpace + 4 * _fontH + scaledFontHeight; // OpenNeoUA: opens just below Display Resolution (row 3)

    _GameShell->d3d_listvw.x = v99;
    _GameShell->d3d_listvw.y = 7 * vertMenuSpace + 7 * _fontH + scaledFontHeight;

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;

    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1168;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( true ) // OpenNeoUA: legacy Options title hidden; the screen content is self-explanatory.
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.width = menuWidth;
        btn_64arg.caption.clear();
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1169;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( true ) // OpenNeoUA: legacy Options helper line hidden; Help button is removed.
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.width = menuWidth;
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TXT3);
            btn_64arg.button_id = 1170;
            btn_64arg.caption2.clear();

            if ( true ) // OpenNeoUA: legacy instruction line 2 hidden (mentioned the removed Direct3D selector)
            {
                btn_64arg.xpos = 0;
                btn_64arg.width = menuWidth;
                btn_64arg.ypos = 3 * (_fontH + vertMenuSpace);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1171;

                if ( true ) // OpenNeoUA: legacy instruction line 3 hidden
                {
                    btn_64arg.tileset_down = 16;
                    btn_64arg.tileset_up = 16;
                    btn_64arg.field_3A = 16;
                    btn_64arg.xpos = 0;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                    btn_64arg.ypos = 3 * (_fontH + vertMenuSpace); // OpenNeoUA: Display Resolution moved above Atmosphere
                    btn_64arg.width = v98;
                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_RES);
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.upCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.button_id = 2;
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.txt_r = _iniColors[60].r;
                    btn_64arg.txt_g = _iniColors[60].g;
                    btn_64arg.txt_b = _iniColors[60].b;

                    if ( _GameShell->video_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 19;
                        btn_64arg.field_3A = 30;
                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                        btn_64arg.caption = _gfxMode.name;
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.tileset_up = 18;
                        btn_64arg.downCode = 1100;
                        btn_64arg.button_id = 1156;
                        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
                        btn_64arg.upCode = 1101;
                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                        btn_64arg.width = v294 * 0.6;
                        btn_64arg.txt_r = _iniColors[68].r;
                        btn_64arg.txt_g = _iniColors[68].g;
                        btn_64arg.txt_b = _iniColors[68].b;

                        if ( _GameShell->video_button->Add(&btn_64arg) )
                        {
                            btn_64arg.tileset_down = 16;
                            btn_64arg.tileset_up = 16;
                            btn_64arg.field_3A = 16;
                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                            btn_64arg.xpos = 0;
                            btn_64arg.ypos = 2 * (3 * (vertMenuSpace + _fontH));
                            btn_64arg.width = v294 * 0.4;
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SEL3D);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.upCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_TEXT;
                            btn_64arg.button_id = 2;
                            btn_64arg.txt_r = _iniColors[60].r;
                            btn_64arg.txt_g = _iniColors[60].g;
                            btn_64arg.txt_b = _iniColors[60].b;

                            if ( true ) // OpenNeoUA: 3D device label hidden from Options UI (device still chosen from NUCLEUS.INI / auto)
                            {
                                btn_64arg.width = v294 * 0.6;
                                btn_64arg.tileset_down = 19;
                                btn_64arg.field_3A = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                btn_64arg.downCode = 1134;
                                btn_64arg.upCode = 1135;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.xpos = buttonsSpace + v294 * 0.4;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.caption = _GameShell->win3d_name;
                                btn_64arg.button_id = 1172;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( true ) // OpenNeoUA: 3D device selector hidden from Options UI (renderer = current OpenNeoUA/OpenGL device)
                                {
                                    int v117 = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;

                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.field_3A = 30;
                                    btn_64arg.xpos = 0;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.downCode = 1102;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.upCode = 1103;
                                    btn_64arg.ypos = 6 * (vertMenuSpace + _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = 0;
                                    btn_64arg.button_id = 1157;

                                    if ( true ) // OpenNeoUA: Horizon Depth checkbox hidden (gfx FARVIEW still read from NUCLEUS.INI)
                                    {
                                        int v120 = v117 / 2;

                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.field_3A = 16;
                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.width = v120;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_FARVIEW);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                        btn_64arg.button_id = 2;
                                        btn_64arg.txt_r = _iniColors[60].r;
                                        btn_64arg.txt_g = _iniColors[60].g;
                                        btn_64arg.txt_b = _iniColors[60].b;

                                        if ( true ) // OpenNeoUA: Horizon Depth label hidden
                                        {
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.width = checkBoxWidth;
                                            btn_64arg.caption = "g";
                                            btn_64arg.caption2 = "g";
                                            btn_64arg.field_3A = 30;
                                            btn_64arg.upCode = 1107;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                            btn_64arg.downCode = 1106;
                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Sky -> left column, row 10
                                            btn_64arg.ypos = 10 * (vertMenuSpace + _fontH);
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.flags = 0;
                                            btn_64arg.button_id = 1160;

                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 16;
                                                btn_64arg.tileset_up = 16;
                                                btn_64arg.field_3A = 16;
                                                btn_64arg.width = v120;
                                                btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Sky label -> left column
                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SKY);
                                                btn_64arg.caption2.clear();
                                                btn_64arg.downCode = 0;
                                                btn_64arg.upCode = 0;
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.button_id = 2;
                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.tileset_down = 19;
                                                    btn_64arg.tileset_up = 18;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.field_3A = 30;
                                                    btn_64arg.width = checkBoxWidth;
                                                    btn_64arg.caption = "g";
                                                    btn_64arg.caption2 = "g";
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
                                                    btn_64arg.downCode = 1132;
                                                    btn_64arg.upCode = 1133;
                                                    btn_64arg.button_id = 1165;
                                                    btn_64arg.flags = 0;

                                                    if ( true ) // OpenNeoUA: Software Cursor checkbox hidden (gfx SOFTMOUSE still read from NUCLEUS.INI)
                                                    {
                                                        btn_64arg.tileset_down = 16;
                                                        btn_64arg.tileset_up = 16;
                                                        btn_64arg.field_3A = 16;
                                                        btn_64arg.width = v120;
                                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SWMOUSE);
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.downCode = 0;
                                                        btn_64arg.upCode = 0;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.button_id = 2;
                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                        if ( true ) // OpenNeoUA: Software Cursor label hidden
                                                        {
                                                            btn_64arg.width = checkBoxWidth;
                                                            btn_64arg.tileset_down = 19;
                                                            btn_64arg.tileset_up = 18;
                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                            btn_64arg.field_3A = 30;
                                                            btn_64arg.downCode = 1130;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.flags = 0;
                                                            btn_64arg.caption = "g";
                                                            btn_64arg.caption2 = "g";
                                                            btn_64arg.upCode = 1131;
                                                            btn_64arg.button_id = 1166;
                                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Windowed -> former VHS Filter position
                                                            btn_64arg.ypos = 9 * (vertMenuSpace + _fontH);

                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                            {
                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.field_3A = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                btn_64arg.ypos = 9 * (vertMenuSpace + _fontH);
                                                                btn_64arg.width = v120;
                                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_WINDOWEDMODE);
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 2;

                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                    btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                    btn_64arg.ypos = 7 * (vertMenuSpace + _fontH);
                                                                    btn_64arg.width = v120;
                                                                    btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_USE16BIT);
                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                    btn_64arg.caption2.clear();
                                                                    btn_64arg.downCode = 0;
                                                                    btn_64arg.upCode = 0;
                                                                    btn_64arg.pressedCode = 0;
                                                                    btn_64arg.button_id = 0;

                                                                    if ( true ) // OpenNeoUA: 16-Bit Textures label hidden (gfx 16BITTEXTURE still read from NUCLEUS.INI)
                                                                    {
                                                                        btn_64arg.width = checkBoxWidth;
                                                                        btn_64arg.tileset_down = 19;
                                                                        btn_64arg.tileset_up = 18;
                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                        btn_64arg.pressedCode = 0;
                                                                        btn_64arg.flags = 0;
                                                                        btn_64arg.field_3A = 30;
                                                                        btn_64arg.xpos = 0;
                                                                        btn_64arg.button_id = 1150;
                                                                        btn_64arg.caption = "g";
                                                                        btn_64arg.caption2 = "g";
                                                                        btn_64arg.downCode = 1113;
                                                                        btn_64arg.upCode = 1114;

                                                                        if ( true ) // OpenNeoUA: 16-Bit Textures checkbox hidden
                                                                        {
                                                                            btn_64arg.tileset_down = 16;
                                                                            btn_64arg.tileset_up = 16;
                                                                            btn_64arg.field_3A = 16;
                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                            btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Music label -> left column, row 11
                                                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                                                            btn_64arg.width = v120;
                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENCDAUD);
                                                                            btn_64arg.caption2.clear();
                                                                            btn_64arg.downCode = 0;
                                                                            btn_64arg.upCode = 0;
                                                                            btn_64arg.pressedCode = 0;
                                                                            btn_64arg.button_id = 0;
                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                            {
                                                                                btn_64arg.width = checkBoxWidth;
                                                                                btn_64arg.tileset_down = 19;
                                                                                btn_64arg.tileset_up = 18;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                btn_64arg.field_3A = 30;
                                                                                btn_64arg.downCode = 1128;
                                                                                btn_64arg.pressedCode = 0;
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.caption = "g";
                                                                                btn_64arg.caption2 = "g";
                                                                                btn_64arg.upCode = 1129;
                                                                                btn_64arg.button_id = 1164;
                                                                                btn_64arg.xpos = 0; // OpenNeoUA repack: Music checkbox -> left column, row 11

                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.tileset_down = 19;
                                                                                    btn_64arg.field_3A = 30;
                                                                                    btn_64arg.tileset_up = 18;
                                                                                    btn_64arg.xpos = 0;
                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                    btn_64arg.width = checkBoxWidth;
                                                                                    btn_64arg.caption = "g";
                                                                                    btn_64arg.caption2 = "g";
                                                                                    btn_64arg.ypos = 8 * (vertMenuSpace + _fontH);
                                                                                    btn_64arg.downCode = 1126;
                                                                                    btn_64arg.pressedCode = 0;
                                                                                    btn_64arg.button_id = 1163;
                                                                                    btn_64arg.upCode = 1127;
                                                                                    btn_64arg.flags = 0;

                                                                                    if ( true ) // OpenNeoUA: Enemy Indicators checkbox hidden (enemyIndicator still read from NUCLEUS.INI)
                                                                                    {
                                                                                        btn_64arg.tileset_down = 16;
                                                                                        btn_64arg.tileset_up = 16;
                                                                                        btn_64arg.field_3A = 16;
                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                                        btn_64arg.width = v120 - checkBoxWidth;
                                                                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENMYINDIC);
                                                                                        btn_64arg.caption2.clear();
                                                                                        btn_64arg.downCode = 0;
                                                                                        btn_64arg.upCode = 0;
                                                                                        btn_64arg.pressedCode = 0;
                                                                                        btn_64arg.button_id = 0;
                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                        if ( true ) // OpenNeoUA: Enemy Indicators label hidden
                                                                                        {
                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                            btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120; // OpenNeoUA repack: Host Station AI -> right column, row 11
                                                                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1137;
                                                                                            btn_64arg.upCode = 1138;
                                                                                            btn_64arg.button_id = 1174;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add player Host Station AI behavior checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = 4 * buttonsSpace + v120 + 2 * checkBoxWidth; // OpenNeoUA repack: Host Station AI label -> right column
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_HOSTSTATIONAI);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add player Host Station AI behavior label\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Spectator Mode -> left column, row 12
                                                                                            btn_64arg.ypos = 12 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1139;
                                                                                            btn_64arg.upCode = 1140;
                                                                                            btn_64arg.button_id = 1175;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add spectator mode checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Spectator Mode label -> left column
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SPECTATORMODE);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 1175;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add spectator mode label\n");
                                                                                                return false;
                                                                                            }

                                                                                            // OpenNeoUA: profile-saved Retro Interface checkbox.
                                                                                            // Checked = Retro/nearest; unchecked = Smooth/linear.
                                                                                            // It shares row 13 with Spectator Mode and uses the free right column.
                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                                    btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120;
                                                                                                    btn_64arg.ypos = 12 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1314;
                                                                                            btn_64arg.upCode = 1315;
                                                                                            btn_64arg.button_id = 1189;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add Retro Interface checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = 4 * buttonsSpace + v120 + 2 * checkBoxWidth;
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_RETRO_INTERFACE);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                            btn_64arg.txt_r = _iniColors[60].r;
                                                                                            btn_64arg.txt_g = _iniColors[60].g;
                                                                                            btn_64arg.txt_b = _iniColors[60].b;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add Retro Interface label\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = v120 + 2 * checkBoxWidth + 4 * buttonsSpace;
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_INVERT);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( true ) // OpenNeoUA: Stereo Reverse label hidden (SF_INVERTLR still read from NUCLEUS.INI)
                                                                                            {
                                                                                                btn_64arg.width = checkBoxWidth;
                                                                                                btn_64arg.tileset_down = 19;
                                                                                                btn_64arg.tileset_up = 18;
                                                                                                btn_64arg.pressedCode = 0;
                                                                                                btn_64arg.flags = 0;
                                                                                                btn_64arg.caption = "g";
                                                                                                btn_64arg.caption2 = "g";
                                                                                                btn_64arg.field_3A = 30;
                                                                                                btn_64arg.button_id = 1151;
                                                                                                btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120;
                                                                                                btn_64arg.downCode = 1112;
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                                btn_64arg.upCode = 1111;

                                                                                                if ( true ) // OpenNeoUA: Stereo Reverse checkbox hidden
                                                                                                {
                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                    btn_64arg.field_3A = 16;
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos = 0;
                                                                                                btn_64arg.ypos = 13 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Explosion Effects row 13
                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_DESTRFX);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.button_id = 2;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        NC_STACK_button::Slider v225;

                                                                                                        v225.value = 8;
                                                                                                        v225.max = 16;
                                                                                                        v225.min = 0;

                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.tileset_down = 18;
                                                                                                        btn_64arg.tileset_up = 18;
                                                                                                        btn_64arg.field_3A = 30;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                        btn_64arg.pressedCode = 1110;
                                                                                                        btn_64arg.button_id = 1159;
                                                                                                        btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                        btn_64arg.caption = " ";
                                                                                                        btn_64arg.downCode = 1108;
                                                                                                        btn_64arg.flags = 0;
                                                                                                        btn_64arg.field_34 = &v225;
                                                                                                        btn_64arg.upCode = 1109;
                                                                                                        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;

                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.tileset_down = 16;
                                                                                                            btn_64arg.tileset_up = 16;
                                                                                                            btn_64arg.field_3A = 16;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.caption2.clear();
                                                                                                            btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                            btn_64arg.downCode = 0;
                                                                                                            btn_64arg.upCode = 0;
                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                            btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                            btn_64arg.button_id = 1158;
                                                                                                            btn_64arg.caption = " 4";
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = 14 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Sound Volume row 14
                                                                                                                btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_FXVOL);
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.upCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.button_id = 2;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                {
                                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;

                                                                                                                    v225.min = 1;
                                                                                                                    v225.max = 127;
                                                                                                                    v225.value = 100;

                                                                                                                    btn_64arg.field_3A = 30;
                                                                                                                    btn_64arg.tileset_down = 18;
                                                                                                                    btn_64arg.tileset_up = 18;
                                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                                    btn_64arg.caption2.clear();
                                                                                                                    btn_64arg.button_id = 1152;
                                                                                                                    btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                    btn_64arg.caption = " ";
                                                                                                                    btn_64arg.downCode = 1115;
                                                                                                                    btn_64arg.upCode = 1117;
                                                                                                                    btn_64arg.field_34 = &v225;
                                                                                                                    btn_64arg.pressedCode = 1116;
                                                                                                                    btn_64arg.flags = 0;

                                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                    {
                                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                        btn_64arg.tileset_down = 16;
                                                                                                                        btn_64arg.tileset_up = 16;
                                                                                                                        btn_64arg.field_3A = 16;
                                                                                                                        btn_64arg.caption = "4";
                                                                                                                        btn_64arg.button_id = 1153;
                                                                                                                        btn_64arg.caption2.clear();
                                                                                                                        btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                                        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                                        btn_64arg.downCode = 0;
                                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                                        btn_64arg.upCode = 0;
                                                                                                                        btn_64arg.pressedCode = 0;

                                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                        {
                                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                            btn_64arg.xpos = 0;
                                                                                                                            btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                            btn_64arg.ypos = 15 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Music Volume row 15
                                                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_CDVOL);
                                                                                                                            btn_64arg.caption2.clear();
                                                                                                                            btn_64arg.downCode = 0;
                                                                                                                            btn_64arg.upCode = 0;
                                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                                            btn_64arg.button_id = 2;

                                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                            {
                                                                                                                                btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;
                                                                                                                                v225.min = 1;
                                                                                                                                v225.max = 127;
                                                                                                                                v225.value = 100;

                                                                                                                                btn_64arg.tileset_down = 18;
                                                                                                                                btn_64arg.tileset_up = 18;
                                                                                                                                btn_64arg.upCode = 1120;
                                                                                                                                btn_64arg.field_3A = 30;
                                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                                                btn_64arg.caption2.clear();
                                                                                                                                btn_64arg.downCode = 1118;
                                                                                                                                btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                                btn_64arg.caption = " ";
                                                                                                                                btn_64arg.pressedCode = 1119;
                                                                                                                                btn_64arg.field_34 = &v225;
                                                                                                                                btn_64arg.flags = 0;
                                                                                                                                btn_64arg.button_id = 1154;

                                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                {
                                                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                                                    btn_64arg.field_3A = 16;
                                                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                                    btn_64arg.caption = "4";
                                                                                                                                    btn_64arg.caption2.clear();
                                                                                                                                    btn_64arg.downCode = 0;
                                                                                                                                    btn_64arg.upCode = 0;
                                                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                                                    btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                                                    btn_64arg.button_id = 1155;
                                                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                    {
                                                                                                                                        btn_64arg.tileset_up = 18;
                                                                                                                                        btn_64arg.field_3A = 30;
                                                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                                                                                                        const int optionsActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
                                                                                                                                        const int optionsActionButtonStep = optionsActionButtonWidth + buttonsSpace;

                                                                                                                                        btn_64arg.xpos = 0;
                                                                                                                                        btn_64arg.ypos = bottomButtonsY;
                                                                                                                                        btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                        btn_64arg.tileset_down = 19;
                                                                                                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                                                                                                        btn_64arg.upCode = 1124;
                                                                                                                                        btn_64arg.caption2.clear();
                                                                                                                                        btn_64arg.downCode = 0;
                                                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                                                        btn_64arg.button_id = 1161;
                                                                                                                                        btn_64arg.txt_r = _iniColors[68].r;
                                                                                                                                        btn_64arg.txt_g = _iniColors[68].g;
                                                                                                                                        btn_64arg.txt_b = _iniColors[68].b;

                                                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                        {
                                                                                                                                            btn_64arg.xpos = 2 * optionsActionButtonStep;
                                                                                                                                            btn_64arg.ypos = bottomButtonsY;
                                                                                                                                            btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_RESETDEF);
                                                                                                                                            btn_64arg.upCode = 1321;
                                                                                                                                            btn_64arg.caption2.clear();
                                                                                                                                            btn_64arg.downCode = 0;
                                                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                                                            btn_64arg.button_id = 1194;

                                                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                            {
                                                                                                                                                btn_64arg.xpos = optionsActionButtonStep;
                                                                                                                                                btn_64arg.ypos = bottomButtonsY;
                                                                                                                                                btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                                                                                                                btn_64arg.upCode = 1125;
                                                                                                                                                btn_64arg.caption2.clear();
                                                                                                                                                btn_64arg.downCode = 0;
                                                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                                                btn_64arg.button_id = 1162;

                                                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                                {
                                                                                                                                                    v70 = 1;
                                                                                                                                                }
                                                                                                                                            }
                                                                                                                                        }
                                                                                                                                    }
                                                                                                                                }
                                                                                                                            }
                                                                                                                        }
                                                                                                                    }
                                                                                                                }
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add video-button\n");
        return false;
    }

    // OpenNeoUA: global mission ambience volume. This is intentionally placed
    // directly under the existing Music Volume row and persists to
    // game.ambient_sound_volume in Nucleus.ini.
    {
        NC_STACK_button::Slider ambientSlider;
        ambientSlider.min = 0;
        ambientSlider.max = 127;
        ambientSlider.value = 100;

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 16 * (vertMenuSpace + _fontH);
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_AMBIENT_VOLUME);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume label\n");
            return false;
        }

        btn_64arg.tileset_down = 18;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
        btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;
        btn_64arg.caption = " ";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 1141;
        btn_64arg.pressedCode = 1142;
        btn_64arg.upCode = 1143;
        btn_64arg.button_id = 1191;
        btn_64arg.flags = 0;
        btn_64arg.field_34 = &ambientSlider;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume slider\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
        btn_64arg.caption = "100";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1192;
        btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume value\n");
            return false;
        }
    }

    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_up = 18;
    btn_64arg.field_3A = 30;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    // OpenNeoUA: fourth compact bottom action after OK / Back / Reset Defaults.
    const int optionsActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
    const int optionsActionButtonStep = optionsActionButtonWidth + buttonsSpace;
    btn_64arg.xpos = 3 * optionsActionButtonStep;
    btn_64arg.ypos = bottomButtonsY;
    btn_64arg.width = optionsActionButtonWidth;
    btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_MORE_OPTIONS);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 1320;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 2;
    btn_64arg.txt_r = _iniColors[60].r;
    btn_64arg.txt_g = _iniColors[60].g;
    btn_64arg.txt_b = _iniColors[60].b;

    if ( !_GameShell->video_button->Add(&btn_64arg) )
    {
        ypa_log_out("Unable to add More Options button\n");
        return false;
    }

    // --- Enable Play As checkbox (row 9, free right column) ---
    // Disabled by default: the briefing keeps the original Resistance-only
    // presentation until the player explicitly enables the optional selector.
    {
        int playAsAvailableWidth = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        int playAsColumnWidth = playAsAvailableWidth / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + playAsColumnWidth;
        btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1316;
        btn_64arg.upCode = 1317;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1190;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Enable Play As checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 4 * buttonsSpace + playAsColumnWidth + 2 * checkBoxWidth;
        btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
        btn_64arg.width = playAsColumnWidth;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_ENABLE_PLAY_AS);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 0;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Enable Play As label\n");
            return false;
        }
    }

    // ===== OpenNeoUA: modern graphics options =====================================
    // Blending, menu font, frame limit and the remaining main-page controls.
    {
        // --- Blending label + cycle-button (row 4, left column) ---
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_BLENDING);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Blending label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_DEFAULT);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = 1306;
        btn_64arg.button_id = 1183;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Blending button\n");
            return false;
        }

        // --- Menu Font cycle-button (row 5, left column) ---
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 5 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_MENU_FONT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Menu Font label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 5 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_DEFAULT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1311;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1186;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Menu Font button\n");
            return false;
        }

    }

        // --- Intro Movies checkbox (row 9, right column) ---
    {
        int gv117 = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        int gv120 = gv117 / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + gv120;
        btn_64arg.ypos = 10 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1307;
        btn_64arg.upCode = 1308;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1184;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Intro Movies checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 4 * buttonsSpace + gv120 + 2 * checkBoxWidth;
        btn_64arg.width = gv120;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_INTRO_MOVIES);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Intro Movies label\n");
            return false;
        }
    }

    // --- FPS Limit cycle-button (row 6, below Menu Font) ---
    {
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_FPS_LIMIT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add FPS Limit label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = "60";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1312;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1187;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add FPS Limit button\n");
            return false;
        }
    }

    // --- Genesis List Order cycle-button (row 7, below FPS Limit) ---
    {
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_GENESIS_LIST_ORDER);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Genesis List Order label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_GENESIS_ORDER_ENERGY);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1313;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1188;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Genesis List Order button\n");
            return false;
        }
    }

    // ===== end OpenNeoUA modern graphics options ==================================

    // OpenNeoUA: visual-only map boundary wall toggle (left column, row 8).
    // Unchecked by default: vanilla RAND boundary walls remain visible.
    {
        const int availableWidth = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        const int columnWidth = availableWidth / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1318;
        btn_64arg.upCode = 1319;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1193;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Hide Map Border Walls checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
        btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
        btn_64arg.width = columnWidth;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_HIDE_MAP_BORDER_WALLS);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 0;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Hide Map Border Walls label\n");
            return false;
        }
    }

    _GameShell->UpdatePaletteThemeText();
    _GameShell->UpdateGfxOptionTexts();
    _GameShell->UpdateMenuFontText();

    NC_STACK_button::button_66arg v229;
    v229.butID = 1151;
    v229.field_4 = ((_GameShell->soundFlags & World::SF_INVERTLR) == 0) + 1;

    _GameShell->video_button->SetState(&v229);

_GameShell->video_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateAtmosphereControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    int usableWidth = menuWidth - 3 * buttonsSpace - _fontVBScrollW;
    int labelWidth = (int)(usableWidth * 0.36f);
    int sliderWidth = (int)(usableWidth * 0.49f);
    int valueWidth = usableWidth - labelWidth - sliderWidth;
    int rowHeight = _fontH + vertMenuSpace;
    const int pageHeight = _screenSize.y - scaledFontHeight;
    const int maxButtonY = pageHeight - _fontH - buttonsSpace;

    // This page is taller than the legacy video page. Compact row spacing only
    // when needed, then move the action buttons just far enough down to clear
    // the final slider while keeping the familiar legacy placement when it fits.
    const int maxRowHeight =
        (maxButtonY - buttonsSpace - _fontH) / (UserData::ATMOPT_COUNT + 2);
    if (maxRowHeight >= _fontH)
        rowHeight = std::min(rowHeight, maxRowHeight);

    const int lastOptionBottom =
        (UserData::ATMOPT_COUNT + 2) * rowHeight + _fontH;
    const int buttonY = std::min(
        maxButtonY,
        std::max(bottomButtonsY, lastOptionBottom + buttonsSpace));

    _GameShell->atmosphere_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});

    if (!_GameShell->atmosphere_button)
    {
        ypa_log_out("Unable to create Atmosphere-Button\n");
        return false;
    }

    const std::array<std::string, UserData::ATMOPT_COUNT> labels =
    {{
        Locale::Text::OpenUA(Locale::OUA_VISUAL_FILTER_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_ATMOSPHERE_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_EXPOSURE),
        Locale::Text::OpenUA(Locale::OUA_CONTRAST),
        Locale::Text::OpenUA(Locale::OUA_SATURATION),
        Locale::Text::OpenUA(Locale::OUA_VIGNETTE),
        Locale::Text::OpenUA(Locale::OUA_SKY_HORIZON_DISTANCE),
        Locale::Text::OpenUA(Locale::OUA_SKY_HEIGHT),
        Locale::Text::OpenUA(Locale::OUA_FOG_START),
        Locale::Text::OpenUA(Locale::OUA_FOG_LENGTH),
        Locale::Text::OpenUA(Locale::OUA_FOG_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_DARK_START),
        Locale::Text::OpenUA(Locale::OUA_DARK_LENGTH),
        Locale::Text::OpenUA(Locale::OUA_DARK_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_WORLD_UI_MAX_DISTANCE),
        Locale::Text::OpenUA(Locale::OUA_VIDEO_FILTER_INTENSITY),
        Locale::Text::OpenUA(Locale::OUA_PARTICLE_LIMIT),
        Locale::Text::OpenUA(Locale::OUA_VIDEO_FILTER_MODE),
        Locale::Text::OpenUA(Locale::OUA_HORIZON_FADE_MODE)
    }};

    const std::array<int, UserData::ATMOPT_COUNT> mins =
    {{0, 0, 25, 50, 0, 0,
      YW_SKY_HORIZON_DISTANCE_MIN / YW_SKY_HORIZON_DISTANCE_UI_STEP,
      YW_SKY_HEIGHT_MIN,
      0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0}};
    const std::array<int, UserData::ATMOPT_COUNT> maxs =
    {{100, 100, 200, 200, 200, 100,
      YW_SKY_HORIZON_DISTANCE_MAX / YW_SKY_HORIZON_DISTANCE_UI_STEP,
      YW_SKY_HEIGHT_MAX,
      10000, 10000, 100, 10000, 10000, 100, 20000, 100,
      YW_PARTICLE_LIMIT_UI_MAX, 1, 1}};

    NC_STACK_button::button_64_arg btn;
    btn.caption2.clear();
    btn.pressedCode = 0;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;

    // Page title.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = 0;
    btn.width = usableWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_ADVANCED_GRAPHICS_SETTINGS);
    btn.downCode = 0;
    btn.upCode = 0;
    btn.button_id = 1390;
    btn.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    // Data-driven graphics preset selector. Profile names come from
    // Data/Scripts/Graphic_Profiles/*.cfg; .txt remains a compatibility fallback; Custom is an in-memory UI state.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = rowHeight;
    btn.width = labelWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_GRAPHIC_PRESET);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 0;
    btn.pressedCode = 0;
    btn.button_id = 1388;
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.xpos = labelWidth + buttonsSpace;
    btn.ypos = rowHeight;
    btn.width = sliderWidth + valueWidth + buttonsSpace;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_CUSTOM);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 1449;
    btn.pressedCode = 0;
    btn.button_id = 1389;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    // Global visual-filter selector, persisted with the sliders in OpenNeoUA.ini.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = 2 * rowHeight;
    btn.width = labelWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_VISUAL_FILTER);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 0;
    btn.pressedCode = 0;
    btn.button_id = 1391;
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.xpos = labelWidth + buttonsSpace;
    btn.ypos = 2 * rowHeight;
    btn.width = sliderWidth + valueWidth + buttonsSpace;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_STANDARD);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 1136;
    btn.pressedCode = 0;
    btn.button_id = 1392;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    for (int i = 0; i < UserData::ATMOPT_COUNT; ++i)
    {
        int y = (i + 3) * rowHeight;

        btn.tileset_down = 16;
        btn.tileset_up = 16;
        btn.field_3A = 16;
        btn.button_type = NC_STACK_button::TYPE_CAPTION;
        btn.xpos = 0;
        btn.ypos = y;
        btn.width = labelWidth;
        btn.caption = labels[i];
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1500 + i;
        btn.flags = NC_STACK_button::FLAG_TEXT;
        btn.txt_r = _iniColors[60].r;
        btn.txt_g = _iniColors[60].g;
        btn.txt_b = _iniColors[60].b;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;

        NC_STACK_button::Slider slider;
        slider.value = mins[i];
        slider.min = mins[i];
        slider.max = maxs[i];
        slider.field_6_ = 0;
        slider.field_8_ = 0;
        slider.pressPart = 0;
        slider.oldValue = slider.value;
        slider.scrDownX = 0;

        btn.tileset_down = 18;
        btn.tileset_up = 18;
        btn.field_3A = 30;
        btn.button_type = NC_STACK_button::TYPE_SLIDER;
        btn.xpos = labelWidth + buttonsSpace;
        btn.ypos = y;
        btn.width = sliderWidth;
        btn.caption = " ";
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1400 + i;
        btn.flags = 0;
        btn.field_34 = &slider;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;

        btn.tileset_down = 16;
        btn.tileset_up = 16;
        btn.field_3A = 16;
        btn.button_type = NC_STACK_button::TYPE_CAPTION;
        btn.xpos = labelWidth + sliderWidth + 2 * buttonsSpace;
        btn.ypos = y;
        btn.width = valueWidth;
        btn.caption = "0";
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1420 + i;
        btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn.txt_r = _iniColors[60].r;
        btn.txt_g = _iniColors[60].g;
        btn.txt_b = _iniColors[60].b;
        btn.field_34 = nullptr;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;
    }

    int buttonWidth = (menuWidth - buttonsSpace) / 2;
    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.ypos = buttonY;
    btn.width = buttonWidth;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;

    btn.xpos = 0;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_SAVE_SETTINGS);
    btn.upCode = 1450;
    btn.button_id = 1450;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.xpos = buttonWidth + buttonsSpace;
    btn.caption = Locale::Text::Common(Locale::CMN_CANCEL);
    btn.upCode = 1452;
    btn.button_id = 1452;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    _GameShell->atmosphere_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateDiskControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    // OpenNeoUA: keep all Save/Load actions on one row. Back sits immediately
    // to the right of Save; Delete keeps its own final slot when enabled.
    const int diskActionButtonWidth = (menuWidth - 4 * buttonsSpace) / 5;
    const int diskActionButtonY = 7 * buttonsSpace + 15 * _fontH;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = _GameShell->profiles.size();
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->disk_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create disk-listview\n");
        return false;
    }

    _GameShell->disk_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->disk_button )
    {
        ypa_log_out("Unable to create disk-buttonobject\n");
        return false;
    }

    _GameShell->disk_listvw.x = posLeftPaddingX;
    _GameShell->disk_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    _GameShell->userNameDir = _GameShell->UserName;

    _GameShell->userNameDirCursor = _GameShell->userNameDir.size();

    std::string v223 = _GameShell->userNameDir;

    if ( _GameShell->diskScreenMode )
        v223 += "h";

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;

    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;

    btn_64arg.xpos = 0;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.button_id = 1108;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->disk_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = buttonsSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT2);
        btn_64arg.button_id = 1109;
        btn_64arg.caption2.clear();
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->disk_button->Add(&btn_64arg))
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1110;

            if ( _GameShell->disk_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (buttonsSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1111;

                if ( _GameShell->disk_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 17;
                    btn_64arg.tileset_up = 17;
                    btn_64arg.field_3A = 17;
                    btn_64arg.xpos = 0;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                    btn_64arg.width = menuWidth;
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.upCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.caption = v223.c_str();
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1100;
                    btn_64arg.ypos = 6 * buttonsSpace + 14 * _fontH;

                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 19;
                        btn_64arg.tileset_up = 18;
                        btn_64arg.field_3A = 30;
                        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                        btn_64arg.xpos = buttonsSpace + diskActionButtonWidth;
                        btn_64arg.ypos = diskActionButtonY;
                        btn_64arg.width = diskActionButtonWidth;
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_LOAD);
                        btn_64arg.downCode = 1251;
                        btn_64arg.upCode = 1160;
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.button_id = 1101;
                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[68].r;
                        btn_64arg.txt_g = _iniColors[68].g;
                        btn_64arg.txt_b = _iniColors[68].b;

                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = 4 * (buttonsSpace + diskActionButtonWidth);
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_DELETE);;
                            btn_64arg.caption2.clear();
                            btn_64arg.upCode = 1161;
                            btn_64arg.button_id = 1102;

                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                            {
                                btn_64arg.xpos = 0;
                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_NEW);
                                btn_64arg.button_id = 1103;
                                btn_64arg.caption2.clear();
                                btn_64arg.upCode = 1162;

                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.xpos = 2 * (buttonsSpace + diskActionButtonWidth);
                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_SAVE);
                                    btn_64arg.button_id = 1104;
                                    btn_64arg.caption2.clear();
                                    btn_64arg.upCode = 1163;

                                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = bottomButtonsY;
                                        btn_64arg.width = button1LineWidth;
                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 1251;
                                        btn_64arg.button_id = 1105;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.upCode = 1164;

                                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.ypos = bottomButtonsY;
                                            btn_64arg.width = button1LineWidth;
                                            btn_64arg.xpos = bottomThirdBtnPosX;
                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                            btn_64arg.button_id = 1107;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.upCode = 1250;

                                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.ypos = diskActionButtonY;
                                                btn_64arg.width = diskActionButtonWidth;
                                                btn_64arg.xpos = 3 * (buttonsSpace + diskActionButtonWidth);
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                btn_64arg.button_id = 1106;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.upCode = 1165;

                                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                                {
                                                    v70 = 1;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if ( !v70 )
    {
        ypa_log_out("Unable to add button to disk-buttonobject\n");
        return false;
    }


        // OpenNeoUA: remove deprecated online-help button from Save/Load.
    _GameShell->disk_button->Remove(1107);

_GameShell->disk_button->HideScreen();
    NC_STACK_button::button_66arg v228;
    v228.field_4 = 0;
    v228.butID = 1105;
    _GameShell->disk_button->Disable(&v228);
    return true;
}

bool NC_STACK_ypaworld::CreateLocaleControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 10;
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth - _fontVBScrollW;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;


    args.wheelScroll = true;

    if ( !_GameShell->local_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create local-listview\n");
        return false;
    }

    _GameShell->locale_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->locale_button )
    {
        ypa_log_out("Unable to create locale-buttonobject\n");
        return false;
    }

    _GameShell->local_listvw.x = posLeftPaddingX;
    _GameShell->local_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.field_3A = 30;
    btn_64arg.ypos = 0;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1253;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->locale_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1254;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->locale_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1255;

            if ( _GameShell->locale_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1256;

                if ( _GameShell->locale_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                    btn_64arg.field_3A = 30;
                    btn_64arg.xpos = bottomCenteredFirstBtnPosX;
                    btn_64arg.ypos = bottomButtonsY;
                    btn_64arg.width = button1LineWidth;
                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.upCode = 1300;
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1250;
                    btn_64arg.txt_r = _iniColors[68].r;
                    btn_64arg.txt_g = _iniColors[68].g;
                    btn_64arg.txt_b = _iniColors[68].b;

                    if ( _GameShell->locale_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = bottomThirdBtnPosX;
                        btn_64arg.ypos = bottomButtonsY;
                        btn_64arg.width = button1LineWidth;
                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                        btn_64arg.button_id = 1252;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 1250;
                        btn_64arg.pressedCode = 0;

                        if ( _GameShell->locale_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = bottomCenteredSecondBtnPosX;
                            btn_64arg.ypos = bottomButtonsY;
                            btn_64arg.width = button1LineWidth;
                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.upCode = 1301;
                            btn_64arg.button_id = 1251;

                            if ( _GameShell->locale_button->Add(&btn_64arg) )
                                v70 = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add locale-button\n");
        return false;
    }

        // OpenNeoUA: remove deprecated online-help button from Language.
    _GameShell->locale_button->Remove(1252);

_GameShell->locale_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateAboutControls(){
    _GameShell->about_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - 0)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->about_button )
    {
        ypa_log_out("Unable to create sound-buttonobject\n");
        return false;
    }
    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;
    v70 = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.width = (_screenSize.x - 4 * buttonsSpace);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = 2;
    btn_64arg.ypos = vertMenuSpace + _fontH;
    btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.caption = "Fuer den Kauf dieses erzgebirgischen Qualitaetsspielzeuges bedanken sich";
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->about_button->Add(&btn_64arg) )
    {
        btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
        btn_64arg.caption = "Bernd Beyreuther,";

        if ( _GameShell->about_button->Add(&btn_64arg) )
        {
            btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
            btn_64arg.caption = "Andre 'Floh' Weissflog, Andreas Flemming,";

            if ( _GameShell->about_button->Add(&btn_64arg) )
            {
                btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
                btn_64arg.caption = "Stefan 'Metzel Hetzel' Karau, Sylvius Lack,";

                if ( _GameShell->about_button->Add(&btn_64arg) )
                {
                    btn_64arg.ypos = 5 * (vertMenuSpace + _fontH);
                    btn_64arg.caption = "Dietmar 'Didi' Koebelin, Nico Nitsch, Steffen Priebus, ";

                    if ( _GameShell->about_button->Add(&btn_64arg) )
                    {
                        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
                        btn_64arg.caption = "Stefan Warias, Henrik Volkening und";

                        if ( _GameShell->about_button->Add(&btn_64arg) )
                        {
                            btn_64arg.ypos = 7 * (vertMenuSpace + _fontH);
                            btn_64arg.caption = "Uta Kapp";

                            if ( _GameShell->about_button->Add(&btn_64arg) )
                            {
                                btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
                                btn_64arg.caption = " ";

                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
                                    btn_64arg.caption = "Unser Dank gilt:";

                                    if ( _GameShell->about_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.ypos = 10 * (_fontH + vertMenuSpace);
                                        btn_64arg.caption = "dem gesamten Microsoft Team, besonders";

                                        if ( _GameShell->about_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                            btn_64arg.caption = "Michael Lyons, Jonathan Sposato und Earnest Yuen";

                                            if ( _GameShell->about_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.ypos = 12 * (_fontH + vertMenuSpace);
                                                btn_64arg.caption = "weiterhin";

                                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.ypos = 13 * (_fontH + vertMenuSpace);
                                                    btn_64arg.caption = "Robert Birker, Andre 'Goetz' Blechschmidt, Jan Blechschmidt, Stephan Bludau,";

                                                    if ( _GameShell->about_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.ypos = 14 * (_fontH + vertMenuSpace);
                                                        btn_64arg.caption = "Andre Kunth, Markus Lorenz, Dirk Mansbart";

                                                        if ( _GameShell->about_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.ypos = 15 * (vertMenuSpace + _fontH);
                                                            btn_64arg.caption = "und natuerlich";

                                                            if ( _GameShell->about_button->Add(&btn_64arg) )
                                                            {
                                                                btn_64arg.ypos = 16 * (_fontH + vertMenuSpace);
                                                                btn_64arg.caption = "        GoldEd - dPaint - SAS/C";

                                                                if ( _GameShell->about_button->Add(&btn_64arg) )
                                                                {
                                                                    v70 = 1;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add about-button\n");
        return false;
    }

    _GameShell->about_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateDatabaseControls()
{
    _GameShell->database_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        // OpenNeoUA Database: use the full shell height. The previous panel started at
        // scaledFontHeight, leaving a large empty strip above the tabs and wasting
        // vertical space.
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y} });

    if ( !_GameShell->database_button )
    {
        ypa_log_out("Unable to create database-button\n");
        return false;
    }

    // Database V2: list rows now show only ID + name, so the left bars can be
    // shorter and the lore/image pane can claim more of the screen.
    const int lw   = (_screenSize.x * 34) / 100;   // list panel width
    const int rx   = (_screenSize.x * 36) / 100;   // right panel x origin
    const int rw   = _screenSize.x - rx;            // right panel width
    const int lh   = _fontH + vertMenuSpace;        // line height
    const int DB_VISIBLE_LINES = 22;                // fill the vertical space above the bottom buttons without overlapping navigation
    const int DB_DETAIL_LINES = 14;
    const int panelH = _screenSize.y;
    // OpenNeoUA Database: anchor navigation buttons to the lowest safe row of the
    // full-screen database panel. The previous formula subtracted an extra
    // vertMenuSpace, leaving a visible gap under Prev/Back/Next.
    // Button height is roughly _fontH; subtracting the full line height (lh)
    // leaves one vertMenuSpace gap at the bottom. Subtract only _fontH to
    // place Prev/Back/Next on the lowest safe row without clipping.
    int nav_y = panelH - _fontH;
    if (nav_y < 10 * lh) nav_y = 10 * lh;   // safety floor on tiny resolutions

    int imgW = std::max(120, (rw * 70) / 100);
    const int textW = imgW;
    const int textX = rx;
    const int statsX = rx + (rw * 52) / 100;
    int statsW = _screenSize.x - statsX - vertMenuSpace;
    if ( statsW < 80 ) statsW = 80;
    const int imgX = rx + (rw - imgW) / 2;
    const int imgY = (DB_DETAIL_LINES + 3) * lh;
    int imgH = nav_y - imgY - vertMenuSpace;
    if (imgH < lh * 3) imgH = lh * 3;
    if (imgX + imgW > _screenSize.x) imgW = _screenSize.x - imgX;
    if (imgH < 1) imgH = 1;

    bool ok = true;
    NC_STACK_button::button_64_arg btn;
    btn.caption2.clear();
    btn.pressedCode = 0;

    // --- TAB BUTTONS (full width, top row) ---
    btn.tileset_down = 19;
    btn.tileset_up   = 18;
    btn.field_3A     = 30;
    btn.button_type  = NC_STACK_button::TYPE_BUTTON;
    btn.downCode     = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
    btn.flags        = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r        = _iniColors[68].r;
    btn.txt_g        = _iniColors[68].g;
    btn.txt_b        = _iniColors[68].b;
    btn.ypos         = 0;
    btn.width        = _screenSize.x / 3;

    btn.xpos = 0;                        btn.caption = Locale::Text::OpenUA(Locale::OUA_DB_UNITS);     btn.button_id = UIWidgets::DB_BTN_UNITS;     btn.upCode = UIWidgets::DB_UP_UNITS;
    if (!_GameShell->database_button->Add(&btn)) ok = false;
    btn.xpos = _screenSize.x / 3;       btn.caption = Locale::Text::OpenUA(Locale::OUA_DB_WEAPONS);   btn.button_id = UIWidgets::DB_BTN_WEAPONS;   btn.upCode = UIWidgets::DB_UP_WEAPONS;
    if (!_GameShell->database_button->Add(&btn)) ok = false;
    btn.xpos = (2 * _screenSize.x) / 3; btn.caption = Locale::Text::OpenUA(Locale::OUA_DB_BUILDINGS); btn.button_id = UIWidgets::DB_BTN_BUILDINGS; btn.upCode = UIWidgets::DB_UP_BUILDINGS;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    // --- LEFT PANEL: page label (TYPE_CAPTION, left-aligned) ---
    btn.tileset_down = 16;
    btn.tileset_up   = 16;
    btn.field_3A     = 16;
    btn.button_type  = NC_STACK_button::TYPE_CAPTION;
    btn.downCode     = 0;
    btn.upCode       = 0;
    btn.xpos         = 0;
    btn.width        = lw;
    btn.flags        = NC_STACK_button::FLAG_TEXT;
    btn.ypos         = lh;
    btn.caption      = " ";
    btn.button_id    = UIWidgets::DB_LABEL_PAGE;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    // --- LEFT PANEL: compact list rows (TYPE_BUTTON, clickable, no border) ---
    btn.tileset_down = 19;
    btn.tileset_up   = 18;
    btn.field_3A     = 30;
    btn.button_type  = NC_STACK_button::TYPE_BUTTON;
    btn.downCode     = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
    btn.flags        = NC_STACK_button::FLAG_TEXT;   // no border, just text+click
    btn.xpos         = 0;
    btn.width        = lw;
    for (int k = 0; k < DB_VISIBLE_LINES; k++)
    {
        btn.ypos      = (k + 2) * lh;
        btn.caption   = " ";
        btn.button_id = UIWidgets::DB_LINE_0 + k;
        btn.upCode    = UIWidgets::DB_UP_LINE_BASE + k;
        if (!_GameShell->database_button->Add(&btn)) ok = false;
    }

    // --- RIGHT PANEL: detail header (TYPE_CAPTION) ---
    btn.tileset_down = 16;
    btn.tileset_up   = 16;
    btn.field_3A     = 16;
    btn.button_type  = NC_STACK_button::TYPE_CAPTION;
    btn.downCode     = 0;
    btn.upCode       = 0;
    btn.xpos         = textX;
    btn.width        = textW;
    btn.flags        = NC_STACK_button::FLAG_TEXT;
    btn.ypos         = lh;
    btn.caption      = std::string("-- ") + Locale::Text::OpenUA(Locale::OUA_DB_DETAILS) + " --";
    btn.button_id    = UIWidgets::DB_DETAIL_HEADER;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    // --- RIGHT PANEL: player-facing detail/description lines (TYPE_CAPTION, left-aligned) ---
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.xpos = textX;
    btn.width = textW;
    for (int k = 0; k < DB_DETAIL_LINES; k++)
    {
        btn.ypos      = (k + 2) * lh;
        btn.caption   = " ";
        btn.button_id = UIWidgets::DB_DETAIL_0 + k;
        if (!_GameShell->database_button->Add(&btn)) ok = false;
    }

    // --- RIGHT PANEL: compact runtime stats block, using the empty upper-right space. ---
    btn.flags     = NC_STACK_button::FLAG_TEXT;
    btn.xpos      = statsX;
    btn.width     = statsW;
    btn.ypos      = 2 * lh;
    btn.caption   = " ";
    btn.button_id = UIWidgets::DB_STATS_HEADER;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    for (int k = 0; k < 12; k++)
    {
        btn.ypos      = (k + 3) * lh;
        btn.caption   = " ";
        btn.button_id = UIWidgets::DB_STATS_0 + k;
        if (!_GameShell->database_button->Add(&btn)) ok = false;
    }

    // --- RIGHT PANEL: image placeholder text, centered inside the custom preview box ---
    btn.xpos      = imgX;
    btn.width     = imgW;
    btn.ypos      = imgY + (imgH / 2) - (lh / 2);
    btn.flags     = NC_STACK_button::FLAG_TEXT | NC_STACK_button::FLAG_CENTER;
    btn.caption   = " ";
    btn.button_id = UIWidgets::DB_IMAGE_TEXT;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    // --- NAV BUTTONS (full width, anchored near the bottom of the panel) ---
    // The database panel starts at screen top, so child Y is relative to the full screen.
    btn.tileset_down = 19;
    btn.tileset_up   = 18;
    btn.field_3A     = 30;
    btn.button_type  = NC_STACK_button::TYPE_BUTTON;
    btn.downCode     = UIWidgets::MAIN_MENU_EVENT_IDS::ALL_DOWN;
    btn.flags        = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.ypos         = nav_y;
    btn.width        = _screenSize.x / 3;

    btn.xpos = 0;                        btn.caption = std::string("< ") + Locale::Text::OpenUA(Locale::OUA_DB_PREV); btn.button_id = UIWidgets::DB_BTN_PREV; btn.upCode = UIWidgets::DB_UP_PREV;
    if (!_GameShell->database_button->Add(&btn)) ok = false;
    btn.xpos = _screenSize.x / 3;       btn.caption = Locale::Text::OpenUA(Locale::OUA_DB_BACK);   btn.button_id = UIWidgets::DB_BTN_BACK; btn.upCode = UIWidgets::DB_UP_BACK;
    if (!_GameShell->database_button->Add(&btn)) ok = false;
    btn.xpos = (2 * _screenSize.x) / 3; btn.caption = Locale::Text::OpenUA(Locale::OUA_DB_NEXT) + " >"; btn.button_id = UIWidgets::DB_BTN_NEXT; btn.upCode = UIWidgets::DB_UP_NEXT;
    if (!_GameShell->database_button->Add(&btn)) ok = false;

    if ( !ok )
    {
        ypa_log_out("Unable to add buttons to database panel\n");
        return false;
    }

    _GameShell->database_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateNetworkControls()
{
    int posLeftPaddingX = (_screenSize.x * 0.3) / 2;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 12;
    args.shownEntries = 12;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 12;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = dword_5A50B2_h;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->network_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create network-listview\n");
        return false;
    }

    int nypos = scaledFontHeight - _fontH;

    _GameShell->network_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)nypos},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - nypos)}});
    if ( !_GameShell->network_button )
    {
        ypa_log_out("Unable to create network-buttonobject\n");
        return false;
    }

    _GameShell->netListY = 3 * (vertMenuSpace + _fontH) + nypos;

    _GameShell->network_listvw.x = posLeftPaddingX;
    _GameShell->network_listvw.y = _GameShell->netListY;

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    btn_64arg.tileset_down = 17;
    btn_64arg.tileset_up = 17;
    btn_64arg.field_3A = 17;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.caption = "???";
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.ypos = 14 * (vertMenuSpace + _fontH);
    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.width = dword_5A50B6 * 0.8;
    btn_64arg.txt_r = _iniColors[60].r;
    btn_64arg.txt_g = _iniColors[60].g;
    btn_64arg.txt_b = _iniColors[60].b;


    v70 = 0;

    if ( _GameShell->network_button->Add(&btn_64arg) )
    {
        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.xpos = buttonsSpace + dword_5A50B6 * 0.8;
        btn_64arg.field_3A = 30;
        btn_64arg.width = dword_5A50B6 * 0.2 - buttonsSpace;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SEND);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = 1210;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1225;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( _GameShell->network_button->Add(&btn_64arg) )
        {
            int v284 = ((dword_5A50B6 - 3 * buttonsSpace) * 0.25 - 3 * buttonsSpace) * 0.25;

            TileMap *v198 = GFX::Engine.GetTileset(8);

            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.field_3A = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.ypos = (15 * (vertMenuSpace + _fontH));
            btn_64arg.xpos = 0;
            btn_64arg.width = dword_5A50B6 * 0.4 - 2 * buttonsSpace;
            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELRACE);
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT | NC_STACK_button::FLAG_RALIGN;
            btn_64arg.button_id = 1220;
            btn_64arg.txt_r = _iniColors[60].r;
            btn_64arg.txt_g = _iniColors[60].g;
            btn_64arg.txt_b = _iniColors[60].b;

            if ( _GameShell->network_button->Add(&btn_64arg) )
            {
                btn_64arg.tileset_down = 8;
                btn_64arg.tileset_up = 8;
                btn_64arg.field_3A = 30;
                btn_64arg.button_type = 4;
                btn_64arg.xpos += btn_64arg.width + 2 * buttonsSpace;
                btn_64arg.width = v198->map[65].w;
                btn_64arg.caption = "A";
                btn_64arg.caption2 = "B";
                btn_64arg.button_id = 1206;
                btn_64arg.downCode = 1204;
                btn_64arg.flags = 0;

                if ( _GameShell->network_button->Add(&btn_64arg) )
                {
                    btn_64arg.caption = "C";
                    btn_64arg.caption2 = "D";
                    btn_64arg.downCode = 1205;
                    btn_64arg.button_id = 1207;
                    btn_64arg.xpos += v284 + buttonsSpace;

                    if ( _GameShell->network_button->Add(&btn_64arg) )
                    {
                        btn_64arg.caption = "E";
                        btn_64arg.caption2 = "F";
                        btn_64arg.downCode = 1206;
                        btn_64arg.button_id = 1208;
                        btn_64arg.xpos += v284 + buttonsSpace;

                        if ( _GameShell->network_button->Add(&btn_64arg) )
                        {
                            btn_64arg.caption = "G";
                            btn_64arg.caption2 = "H";
                            btn_64arg.downCode = 1207;
                            btn_64arg.button_id = 1209;
                            btn_64arg.xpos += v284 + buttonsSpace;

                            if ( _GameShell->network_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 19;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.field_3A = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                btn_64arg.xpos += v284 + 2 * buttonsSpace;
                                btn_64arg.width = dword_5A50B2_h - btn_64arg.xpos;
                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_BACK);
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.button_id = 1205;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.upCode = 1203;
                                btn_64arg.downCode = 1251;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 16;
                                    btn_64arg.xpos = 0;
                                    btn_64arg.ypos = 0;
                                    btn_64arg.tileset_up = 16;
                                    btn_64arg.field_3A = 16;
                                    btn_64arg.width = dword_5A50B2_h;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELPROV);
                                    btn_64arg.caption2.clear();
                                    btn_64arg.downCode = 0;
                                    btn_64arg.upCode = 0;
                                    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE;
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = buttonsSpace + _fontH;
                                        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT2);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1;
                                        btn_64arg.txt_r = _iniColors[60].r;
                                        btn_64arg.txt_g = _iniColors[60].g;
                                        btn_64arg.txt_b = _iniColors[60].b;

                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.xpos = 0;
                                            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT3);
                                            btn_64arg.caption2.clear();
                                            btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2;

                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 19;
                                                btn_64arg.tileset_up = 18;
                                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                btn_64arg.field_3A = 30;
                                                btn_64arg.xpos = dword_5A50B6 * 0.3;
                                                btn_64arg.ypos = (buttonsSpace + _fontH) * 15.2;
                                                btn_64arg.width = dword_5A50B6 * 0.4;
                                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEW);
                                                btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
                                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                btn_64arg.downCode = 1251;
                                                btn_64arg.upCode = 1201;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.txt_r = _iniColors[68].r;
                                                btn_64arg.txt_g = _iniColors[68].g;
                                                btn_64arg.txt_b = _iniColors[68].b;

                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = bottomCenteredFirstBtnPosX;
                                                    btn_64arg.ypos = bottomButtonsY + _fontH;
                                                    btn_64arg.width = button1LineWidth;
                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEXT);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1201;
                                                    btn_64arg.upCode = 1200;

                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY + _fontH;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.button_id = 1218;

                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = bottomCenteredSecondBtnPosX;
                                                            btn_64arg.ypos = bottomButtonsY + _fontH;
                                                            btn_64arg.width = button1LineWidth;
                                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_BACK);
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.upCode = 1202;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.button_id = 1203;

                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                            {
                                                                int v204;

                                                                if ( _screenSize.x < 512 )
                                                                    v204 = 6 * checkBoxWidth;
                                                                else
                                                                    v204 = 4 * checkBoxWidth;

                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.field_3A = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = v204 + checkBoxWidth;
                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                btn_64arg.caption = " ";
                                                                btn_64arg.width = dword_5A50B2_h - v204 - checkBoxWidth;
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 1210;
                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.ypos = 5 * (buttonsSpace + _fontH);
                                                                    btn_64arg.button_id = 1211;

                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                    {
                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                        btn_64arg.button_id = 1212;

                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                        {
                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                            btn_64arg.button_id = 1213;

                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                            {
                                                                                btn_64arg.tileset_down = 8;
                                                                                btn_64arg.tileset_up = 8;
                                                                                btn_64arg.field_3A = 8;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                btn_64arg.xpos = 0;
                                                                                btn_64arg.width = v204;
                                                                                btn_64arg.caption2.clear();
                                                                                btn_64arg.downCode = 0;
                                                                                btn_64arg.upCode = 0;
                                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                                btn_64arg.pressedCode = 0;
                                                                                btn_64arg.caption = " ";
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.button_id = 1214;
                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.ypos = 5 * (_fontH + buttonsSpace);
                                                                                    btn_64arg.button_id = 1215;

                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                    {
                                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                                        btn_64arg.button_id = 1216;

                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                        {
                                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                                            btn_64arg.button_id = 1217;

                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                btn_64arg.tileset_down = 19;
                                                                                                btn_64arg.tileset_up = 18;
                                                                                                btn_64arg.xpos = 0;
                                                                                                btn_64arg.field_3A = 30;
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                                btn_64arg.width = checkBoxWidth;
                                                                                                btn_64arg.caption = "g";
                                                                                                btn_64arg.caption2 = "g";
                                                                                                btn_64arg.pressedCode = 0;
                                                                                                btn_64arg.button_id = 1219;
                                                                                                btn_64arg.ypos = bottomButtonsY + _fontH;
                                                                                                btn_64arg.downCode = 1208;
                                                                                                btn_64arg.flags = 0;
                                                                                                btn_64arg.upCode = 1209;

                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                {
                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                    btn_64arg.field_3A = 16;
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos += buttonsSpace + checkBoxWidth;
                                                                                                    btn_64arg.width = button1LineWidth - checkBoxWidth - buttonsSpace;
                                                                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_READY);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                    btn_64arg.button_id = 1221;

                                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        btn_64arg.xpos = 0;
                                                                                                        btn_64arg.tileset_down = 16;
                                                                                                        btn_64arg.tileset_up = 16;
                                                                                                        btn_64arg.field_3A = 16;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                        btn_64arg.ypos = 3 * (_fontH + buttonsSpace);
                                                                                                        btn_64arg.width = dword_5A50B6 * 0.3;
                                                                                                        btn_64arg.caption = Locale::Text::Get(Locale::LBL_YOUPLAY, Locale::DefaultStrings::YouPlay);
                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.downCode = 0;
                                                                                                        btn_64arg.upCode = 0;
                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                        btn_64arg.button_id = 1227;
                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                        btn_64arg.txt_r = _iniColors[68].r;
                                                                                                        btn_64arg.txt_g = _iniColors[68].g;
                                                                                                        btn_64arg.txt_b = _iniColors[68].b;

                                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.xpos = dword_5A50B6 * 0.3;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.width = dword_5A50B6 * 0.7;
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                            btn_64arg.button_id = 1226;
                                                                                                            btn_64arg.caption = "...";

                                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.tileset_down = 16;
                                                                                                                btn_64arg.tileset_up = 16;
                                                                                                                btn_64arg.field_3A = 16;
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = (14 * (vertMenuSpace + _fontH));
                                                                                                                btn_64arg.width = dword_5A50B2_h;
                                                                                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_REFRESHSESS);
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.upCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.button_id = 1228;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                                    v70 = 1;
                                                                                                            }
                                                                                                        }
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add network-button\n");
        return false;
    }
    NC_STACK_button::button_66arg v228;
    v228.butID = 1210;
    v228.field_4 = 0;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1211;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1212;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1213;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1214;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1215;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1216;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1217;
    _GameShell->network_button->Disable(&v228);

        // OpenNeoUA: remove deprecated online-help button from Network.
    _GameShell->network_button->Remove(1218);

_GameShell->network_button->HideScreen();
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

// Load Level
size_t NC_STACK_ypaworld::ypaworld_func161(yw_arg161 *arg)
{
    int ok = 0;
    TLevelDescription mapp;

    _particles.Clear();

    if ( LevelCommonLoader(&mapp, arg->lvlID, arg->field_4) )
    {
        if ( LoadTypeMap(mapp.TypStr) )
        {
            if ( LoadOwnerMap( mapp.OwnStr) )
            {
                if ( LoadHightMap(mapp.HgtStr) )
                {
                    std::vector<MapRobo> playAsRobos = BriefingReorderRobosForPlayAs(mapp.Robos);

                    if ( yw_createRobos(playAsRobos) )
                    {
                        if ( LoadBlgMap(mapp.BlgStr) )
                        {
                            if ( _levelInfo.Mode != 1 )
                            {
                                yw_InitSquads(mapp.Squads);
                                InitBuddies();

                                for (int yy = 0; yy < _mapSize.y; yy++)
                                {
                                    for (int xx = 0; xx < _mapSize.x; xx++)
                                    {
                                        CellCheckHealth(&_cells(xx, yy), World::OWNER_RECALC, nullptr);
                                    }
                                }

                                yw_InitTechUpgradeBuildings();
                                InitGates();
                                InitSuperItems();
                                UpdatePowerEnergy();
                                TryActivateSpectatorMode();
                            }

                            PrepareAllFillers();

                            if ( sb_0x451034(this) )
                                ok = 1;
                        }
                    }
                }
            }
        }
    }

    if ( !ok )
    {
        printf("Load level not OK\n");
        DeleteLevel();
    }

    return ok;
}

size_t NC_STACK_ypaworld::ypaworld_func162(const std::string &fname)
{
    _replayPlayer = new TGameRecorder();

    if ( !_replayPlayer )
        return 0;

    TGameRecorder *repl = _replayPlayer;

    repl->filename = fname;
    _debugBlastRings.clear();
    _timeStamp = 0;
    System::GameClock.Reset(_timeStamp);

    if ( !recorder_open_replay(repl) )
        return 0;

    while ( repl->mfile.parse() != IFFile::IFF_ERR_EOC )
    {
        const IFFile::Context &v13 = repl->mfile.GetCurrentChunk();

        if ( v13.Is(TAG_SINF) )
        {
            repl->seqn = repl->mfile.readU16L();
            repl->level_id = repl->mfile.readU16L();
            repl->mfile.parse();
        }
        else if ( v13.Is(TAG_FORM, TAG_FRAM) )
        {
            repl->field_74++;
            repl->mfile.skipChunk();
        }
        else
        {
            repl->mfile.skipChunk();
        }
    }


    repl->mfile.close();

    yw_arg161 arg161;
    arg161.field_4 = 1;
    arg161.lvlID = repl->level_id;

    if ( !ypaworld_func161(&arg161) )
        return 0;

    if ( !recorder_create_camera() )
    {
        ypa_log_out("PLAYER ERROR: could not create virtual camera!\n");
        ypaworld_func164();

        return 0;
    }

    repl->field_44 = vec3d(0.0, 0.0, 0.0);
    repl->rotation_matrix = mat3x3::Ident();

    if ( !recorder_go_to_frame(repl, 0) )
    {
        ypa_log_out("PLAYER ERROR: could not position on 1st frame!\n");
        ypaworld_func164();
        return 0;
    }

    yw_arg165 arg165;

    arg165.field_0 = 2;
    arg165.frame = 0;
    ypaworld_func165(&arg165);

    arg165.field_0 = 20;
    arg165.frame = 0;
    ypaworld_func165(&arg165);

    return 1;
}

void NC_STACK_ypaworld::ypaworld_func163(base_64arg *arg)
{
    TGameRecorder *repl = _replayPlayer;
    uint32_t v33 = profiler_begin();

    const int32_t unscaledFrameTime = arg->DTime;
    const int32_t realFrameTimeStamp = arg->TimeStamp;
    const bool openUADebug = System::IniConf::IsGameNewDebugEnabled();
    if ( !openUADebug )
    {
        _debugGameplaySlowMotion = false;
        _debugGameplayFrozen = false;
    }

    HandleDebugTimeHotkeys(arg->field_8, openUADebug);

    const float gameplayTimeScale = yw_GetActiveGameplayTimeScale(this);
    const bool gameplayFrozen = !_isNetGame && openUADebug && _debugGameplayFrozen;
    const bool forceAllTimeScaled = !_isNetGame && openUADebug &&
                                    (_debugGameplaySlowMotion || _debugGameplayFastMotion || _debugGameplayFrozen);

    arg->DTime = System::GameClock.BeginFrame(realFrameTimeStamp, unscaledFrameTime,
                                               _timeStamp, gameplayTimeScale,
                                               gameplayFrozen, forceAllTimeScaled);
    _timeStamp = System::GameClock.Time();
    arg->TimeStamp = System::GameClock.VisualTime();
    if ( arg->field_8 )
        arg->field_8->Period = arg->DTime;

    Gui::Root::Instance.TimersUpdate(arg->DTime);
    SFXEngine::SFXe.SetTimeScale(gameplayFrozen ? 0.0f : gameplayTimeScale,
                                 forceAllTimeScaled);

    _framesElapsed++;
    _updateMessage.user_action = World::DOACTION_0;
    _updateMessage.gTime = _timeStamp;
    _updateMessage.frameTime = arg->DTime;
    _updateMessage.units_count = 0;
    _updateMessage.inpt = arg->field_8;
    _FPS = unscaledFrameTime > 0 ? 1024 / unscaledFrameTime : 0;

    _profileVals[PFID_FPS] = _FPS;

    GFX::Engine.BeginFrame();

    /*_win3d->setRSTR_BGpen(0);

    _win3d->raster_func192(nullptr);*/

    sub_4C40AC();

    _guiVisor.field_0 = 0;
    _guiVisor.field_4 = 0;

    if ( repl->field_7C != 1 && !gameplayFrozen )
    {
        ypaworld_func163__sub1(repl, arg->DTime);
        // Replay interpolation may snap the canonical recorded timestamp to a
        // frame boundary. Keep the master clock synchronized without changing
        // its scaled visual phase.
        System::GameClock.SyncGameTime(_timeStamp);
    }

    CameraPrepareRender(repl, _userUnit, arg->field_8);

    vec3d a3a = _userUnit->_fly_dir * _userUnit->_fly_dir_length;

    SFXEngine::SFXe.sub_423EFC(arg->DTime, _userUnit->_position, a3a, _userUnit->_rotation);
    UpdateAmbientLevelSound();

    for ( NC_STACK_ypabact* &bct : _userUnit->_kidList )
    {
        bct->_tForm.Pos = bct->_position;

        bct->_tForm.SclRot = bct->_rotation.Transpose();

        bct->_soundcarrier.Position = bct->_position;

        bct->_soundcarrier.Vector = bct->_fly_dir * bct->_fly_dir_length;

        SFXEngine::SFXe.UpdateSoundCarrier(&bct->_soundcarrier);
    }

    const mat3x3 &v25 = SFXEngine::SFXe.sb_0x424c74();
    TF::TForm3D *v26 = TF::Engine.GetViewPoint();

    v26->SclRot = v25 * v26->SclRot;

    uint32_t v28 = profiler_begin();

    RenderGame(arg, 0);

    GFX::Engine.BeginVirtualUI(_screenSize);
    debug_info_draw(arg->field_8);
    GFX::Engine.EndVirtualUI();

    GFX::Engine.EndFrame();

    _profileVals[PFID_UPDATETIME] = profiler_end(v28);

    sb_0x447720(this, arg->field_8);

    _profileVals[PFID_FRAMETIME] = profiler_end(v33);
    _profileVals[PFID_POLYGONS] = _polysDraw;

    ProfileCalcValues();

    arg->TimeStamp = realFrameTimeStamp;
}

void NC_STACK_ypaworld::ypaworld_func164()
{
    if ( _replayPlayer )
    {
        _replayPlayer->mfile.close();

        DeleteLevel();

        delete _replayPlayer;
        _replayPlayer = nullptr;
    }
}

void NC_STACK_ypaworld::ypaworld_func165(yw_arg165 *arg)
{
    TGameRecorder *repl = _replayPlayer;

    if ( (repl->field_80 == 18 || repl->field_80 == 19 || repl->field_80 == 20) && (arg->field_0 == 16 || arg->field_0 == 17) )
    {
        repl->field_44 = _userUnit->_position;
        repl->rotation_matrix = _userUnit->_rotation;
    }

    if ( arg->field_0 == 1 || arg->field_0 == 2 )
    {
        repl->field_7C = arg->field_0;
    }
    else if ( arg->field_0 == 3 )
    {
        recorder_go_to_frame(repl, arg->frame);
    }
    else if ( arg->field_0 == 4 )
    {
        recorder_go_to_frame(repl, repl->frame_id + 1);
    }
    else if ( arg->field_0 == 5 )
    {
        recorder_go_to_frame(repl, repl->frame_id - 1);
    }
    else if ( arg->field_0 == 7 )
    {
        recorder_go_to_frame(repl, repl->frame_id + arg->frame);
    }
    else if ( arg->field_0 == 16 || arg->field_0 == 17 )
    {
        repl->field_84 = 0;
        repl->field_80 = arg->field_0;
    }
    else if ( arg->field_0 == 18 || arg->field_0 == 19 || arg->field_0 == 20 )
    {
        repl->field_80 = arg->field_0;
        repl->field_84 = arg->frame;

        repl->field_44 = vec3d(0.0, 0.0, 0.0);
        repl->rotation_matrix = mat3x3::Ident();
    }
    else
    {
        repl->field_7C = 1;
    }
}

size_t NC_STACK_ypaworld::ypaworld_func166(const std::string &langname)
{
    Locale::Text::SetLangDefault();

    // OpenNeoUA currently supports the English vanilla catalogue only. Depending
    // on the retail layout it may be named LANGUAGE.DLL or ENGLISH.DLL. Other
    // locale catalogues are intentionally ignored instead of being selected as
    // an automatic fallback.
    struct LocaleCatalogueCandidate
    {
        std::string path;
        int priority = 0; // LANGUAGE.DLL before ENGLISH.DLL.
    };

    std::vector<LocaleCatalogueCandidate> candidates;
    FSMgr::DirIter localeDir = uaOpenDir("locale:");
    if (localeDir)
    {
        FSMgr::iNode *node = nullptr;
        while (localeDir.getNext(&node))
        {
            if (!node || node->getType() != FSMgr::iNode::NTYPE_FILE)
                continue;

            const std::string filename = node->getName();
            std::string lower = filename;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            int priority = 0;
            if (lower == "language.dll")
                priority = 2;
            else if (lower == "english.dll")
                priority = 1;
            else
                continue;

            candidates.push_back({fmt::sprintf("locale:%s", filename), priority});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const LocaleCatalogueCandidate &a, const LocaleCatalogueCandidate &b)
              {
                  if (a.priority != b.priority)
                      return a.priority > b.priority;
                  return a.path < b.path;
              });

    std::set<std::string> attemptedPaths;
    const auto tryPath = [&](const std::string &path) -> bool
    {
        if (!attemptedPaths.insert(path).second)
            return false;
        return Locale::Text::DllFileLoad(path);
    };

    bool baseLoaded = false;
    for (const LocaleCatalogueCandidate &candidate : candidates)
    {
        if (tryPath(candidate.path))
        {
            baseLoaded = true;
            break;
        }
    }

    // Preserve direct lookups when directory enumeration is unavailable or the
    // filesystem is case-sensitive. Both accepted names contain the English
    // vanilla catalogue; no other language is probed.
    if (!baseLoaded)
    {
        const std::array<std::string, 6> directCandidates{{
            "locale:LANGUAGE.DLL",
            "locale:language.dll",
            "locale:Language.dll",
            "locale:ENGLISH.DLL",
            "locale:english.dll",
            "locale:English.dll"
        }};

        for (const std::string &candidate : directCandidates)
        {
            if (tryPath(candidate))
            {
                baseLoaded = true;
                break;
            }
        }
    }

    if (StriCmp(langname, "language") && StriCmp(langname, "english"))
    {
        ypa_log_out("Locale: '%s' is unsupported; OpenNeoUA uses English only.\n",
                    langname.c_str());
    }

    if (baseLoaded)
    {
        // Keep the historical runtime locale key. Existing level titles,
        // messages and speech assets use title_language/msg_language and the
        // sounds/speech/language directory even when the catalogue file itself
        // is named ENGLISH.DLL.
        Locale::Text::SetLocaleName("language");
    }

    // OpenNeoUA-owned strings are always a separate overlay and cannot overwrite
    // IDs belonging to the original game catalogue.
    const bool openUALoaded =
        Locale::Text::OpenUALngFileLoad("locale:OpenNeoUA.lng") ||
        Locale::Text::OpenUALngFileLoad("locale:openeoua.lng");

    if (!openUALoaded)
        ypa_log_out("Warning: Locale/OpenNeoUA.lng not found or contains no valid OpenNeoUA strings.\n");

    if (!baseLoaded)
    {
        ypa_log_out("ERROR: English vanilla catalogue not found. Expected Locale/LANGUAGE.DLL or Locale/ENGLISH.DLL.\n");
        Locale::Text::SetLangDefault();
        return 0;
    }

    std::string fontStr = _screenSize.x >= 512
                              ? Locale::Text::Font()
                              : Locale::Text::SmallFont();

    fontStr = System::ResolveMenuFontDescr(fontStr);
    GFX::Engine.LoadFontByDescr(fontStr);
    Gui::UA::LoadFont(fontStr);

    return 1;
}

// Update menu values
void NC_STACK_ypaworld::UpdateGameShell()
{
    if ( _GameShell->diskListActiveElement )
    {
        _GameShell->disk_listvw.PosOnSelected(_GameShell->diskListActiveElement - 1);

        NC_STACK_button::button_66arg v18;
        v18.field_4 = 1;
        v18.butID = 1101;
        _GameShell->disk_button->Enable(&v18);

        v18.butID = 1102;
        _GameShell->disk_button->Enable(&v18);

        v18.butID = 1103;
        _GameShell->disk_button->Enable(&v18);
    }
    else
    {
        NC_STACK_button::button_66arg v18;
        v18.field_4 = 1;
        v18.butID = 1101;
        _GameShell->disk_button->Disable(&v18);

        v18.butID = 1102;
        _GameShell->disk_button->Disable(&v18);

        v18.butID = 1103;
        _GameShell->disk_button->Disable(&v18);
    }

    NC_STACK_button::button_66arg v16;
    v16.butID = 1151;
    v16.field_4 = ((_GameShell->soundFlags & World::SF_INVERTLR) == 0) + 1;

    _GameShell->video_button->SetState(&v16);


    v16.butID = 1150;
    v16.field_4 = ((_GameShell->GFXFlags & World::GFX_FLAG_16BITTEXTURE) == 0) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.field_4 = ((_GameShell->soundFlags & World::SF_CDSOUND) == 0) + 1;
    v16.butID = 1164;
    _GameShell->video_button->SetState(&v16);

    NC_STACK_button::Slider *tmp = _GameShell->video_button->GetSliderData(1152);
    tmp->value = _GameShell->soundVolume;

    _GameShell->video_button->Refresh(1152);

    tmp = _GameShell->video_button->GetSliderData(1154);
    tmp->value = _GameShell->musicVolume;

    _GameShell->video_button->Refresh(1154);

    v16.butID = 1163;
    v16.field_4 = (_GameShell->enemyIndicator == 0) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1174;
    v16.field_4 = (!System::IniConf::GameRoboPlayerAIBehavior.Get<bool>()) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1175;
    v16.field_4 = (!System::IniConf::GameSpectatorMode.Get<bool>()) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1190;
    v16.field_4 = (!System::IniConf::GamePlayAsOtherFactions.Get<bool>()) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1157;
    v16.field_4 = ((_GameShell->GFXFlags & World::GFX_FLAG_FARVIEW) == 0) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.field_4 = ((_GameShell->GFXFlags & World::GFX_FLAG_SKYRENDER) == 0) + 1;
    v16.butID = 1160;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1165;
    v16.field_4 = ((_GameShell->GFXFlags & World::GFX_FLAG_SOFTMOUSE) == 0) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1166;
    v16.field_4 = (!_GameShell->IsWindowedFlag()) + 1;
    _GameShell->video_button->SetState(&v16);

    // OpenNeoUA: modern graphics options initial state (read from config)
    _GameShell->confBlending = System::IniConf::GfxBlending.Get<int32_t>();
    _GameShell->confMaxFps = System::IniConf::GfxMaxFps.Get<int32_t>();
    _GameShell->confMoviePlayer = System::IniConf::GfxMoviePlayer.Get<bool>();
    _GameShell->confMenuFont = _GameShell->menuFont;
    // Nucleus.ini controls whether first-person play uses the modern cockpit
    // camera. Missing/disabled keeps the original Urban Assault viewer.
    _GameShell->cockpitCameraRuntimeMode = System::IniConf::GameCockpitCamera.Get<bool>();
    _GameShell->confInterfaceStyle = _GameShell->interfaceStyle;
    GFX::Engine.SetVirtualUIStyle(_GameShell->interfaceStyle);

    v16.butID = 1184; // Intro Movies checkbox
    v16.field_4 = (!_GameShell->confMoviePlayer) + 1;
    _GameShell->video_button->SetState(&v16);

    v16.butID = 1189; // Retro Interface checkbox
    v16.field_4 = (_GameShell->confInterfaceStyle == GFX::VirtualUIStyle::RETRO) ? 1 : 2;
    _GameShell->video_button->SetState(&v16);

    _GameShell->confHideMapBorderWalls = System::IniConf::GfxHideMapBorderWalls.Get<bool>();
    v16.butID = 1193; // Hide Map Border Walls checkbox
    v16.field_4 = _GameShell->confHideMapBorderWalls ? 1 : 2;
    _GameShell->video_button->SetState(&v16);

    _GameShell->confGenesisListOrder = System::IniConf::UiGenesisListOrder.Get<std::string>();

    _GameShell->video_button->SetText(1156, _GameShell->p_YW->_gfxMode.name);
    _GameShell->UpdatePaletteThemeText();
    _GameShell->UpdateGfxOptionTexts();
    _GameShell->UpdateMenuFontText();
    _GameShell->UpdateGenesisListOrderText();

    tmp = _GameShell->video_button->GetSliderData(1159);
    tmp->value = _GameShell->fxnumber;

    _GameShell->video_button->Refresh(1159);


    NC_STACK_button::button_66arg v9;
    v9.butID = 1050;
    v9.field_4 = ((_GameShell->p_YW->_preferences & World::PREF_JOYDISABLE) != 0) + 1;

    _GameShell->button_input_button->SetState(&v9);

    v9.butID = 1061;
    v9.field_4 = (_GameShell->altJoystickEnabled == false) + 1;
    _GameShell->button_input_button->SetState(&v9);

    v9.butID = 1055;
    v9.field_4 = ((_GameShell->p_YW->_preferences & World::PREF_FFDISABLE) != 0) + 1;
    _GameShell->button_input_button->SetState(&v9);

    if ( _GameShell->inpListActiveElement )
    {
        int v7 = _GameShell->InputDisplayIndexFromBinding(_GameShell->inpListActiveElement);
        if ( v7 < 0 )
            v7 = 0;

        int v8 = _GameShell->input_listview.maxShownEntries + _GameShell->input_listview.firstShownEntries;

        if ( v7 >= _GameShell->input_listview.firstShownEntries && v7 < v8 )
        {
            if ( v8 > _GameShell->input_listview.numEntries )
                _GameShell->input_listview.firstShownEntries = _GameShell->input_listview.numEntries - _GameShell->input_listview.maxShownEntries;

            if ( _GameShell->input_listview.firstShownEntries < 0 )
                _GameShell->input_listview.firstShownEntries = 0;
        }
        else if ( _GameShell->input_listview.numEntries - v7 <= _GameShell->input_listview.maxShownEntries )
        {
            _GameShell->input_listview.firstShownEntries = _GameShell->input_listview.numEntries - _GameShell->input_listview.maxShownEntries;
        }
        else
        {
            _GameShell->input_listview.firstShownEntries = v7;
        }
    }
}

size_t NC_STACK_ypaworld::ypaworld_func168(NC_STACK_ypabact *bact)
{
    if ( bact->_bact_type == BACT_TYPES_GUN || bact->_bact_type == BACT_TYPES_MISSLE )
        return 1;

    if ( bact->_owner == _userRobo->_owner )
    {
        if ( bact->_pSector->PurposeType == cellArea::PT_GATEOPENED )
        {
            if ( _userRobo == bact )
            {
                _levelInfo.State = TLevelInfo::STATE_COMPLETED;
                _levelInfo.GateCompleteID = bact->_pSector->PurposeIndex;
            }
            else
            {
                _beamEnergyCurrent += (bact->_energy_max + 99) / 100;

                if ( _beamEnergyCurrent <= _beamEnergyCapacity )
                    _levelInfo.Buddies.push_back( TMapBuddy( bact->_commandID, bact->_vehicleID, bact->_energy ) );
                else
                    return 0;
            }
        }
    }
    return 1;
}

int NC_STACK_ypaworld::LoadingParseSaveFile(const std::string &filename)
{
    World::Parsers::SaveBact::ResetHierarchyState();

    int lvlnum;
    ScriptParser::HandlersList parsers
    {
        new World::Parsers::UserParser(this),
        new World::Parsers::SaveRoboParser(this),
        new World::Parsers::SaveSquadParser(this), // commander and units
        new World::Parsers::SaveGemParser(this),
        new World::Parsers::VhclProtoParser(this),
        new World::Parsers::WeaponProtoParser(this),
        new World::Parsers::BuildProtoParser(this),
        new World::Parsers::SaveExtraViewParser(this),
        new World::Parsers::SaveKwFactorParser(this),
        new World::Parsers::SaveGlobalsParser(this),
        new World::Parsers::SaveOwnerMapParser(this),
        new World::Parsers::SaveBuildingMapParser(this),
        new World::Parsers::SaveEnergyMapParser(this),
        new World::Parsers::SaveLevelNumParser(this, &lvlnum),
        new World::Parsers::LevelStatusParser(this, true),
        new World::Parsers::SaveHistoryParser(this),
        new World::Parsers::SaveMasksParser(this),
        new World::Parsers::SaveSuperBombParser(this),
    };

    parsers.push_back( new World::Parsers::SaveLuaScriptParser(this) );

    bool parsed = ScriptParser::ParseFile(filename, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);
    World::Parsers::SaveBact::ResetHierarchyState();

    if ( parsed )
    {
        ResolveStatusProfileLinks();
        RestoreCustomSuperItemRuntimeAfterLoad();
    }
    return parsed;
}

void NC_STACK_ypaworld::LoadingUnitsRefresh()
{
    for ( NC_STACK_ypabact* &station : _unitsList )
    {
        RefreshUnitPRT(station, station, true);

        for ( NC_STACK_ypabact* &commander : station->_kidList )
        {
            RefreshUnitPRT(commander, station, false);

            for ( NC_STACK_ypabact* &slave : commander->_kidList )
                RefreshUnitPRT(slave, station, false);
        }
    }

    if ( _extraViewEnable )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);
        if ( robo && _extraViewNumber >= 0 &&
                (size_t)_extraViewNumber < robo->_roboGuns.size() &&
                robo->_roboGuns[_extraViewNumber].gun_obj )
        {
            NC_STACK_ypabact *gun = robo->_roboGuns[_extraViewNumber].gun_obj;
            gun->setBACT_viewer(true);
            gun->setBACT_inputting(true);
        }
        else
        {
            _extraViewEnable = false;
            _extraViewNumber = -1;
        }
    }
}

size_t NC_STACK_ypaworld::LoadGame(const std::string &saveFile)
{
    bool loadOK = false;

    if ( !uaFileExist(saveFile) )
        return 0;

    int lvlnum = -1;

    ScriptParser::HandlersList parsers
    {
        new World::Parsers::SaveLevelNumParser(this, &lvlnum),
    };

    if ( !ScriptParser::ParseFile(saveFile, parsers, 0) ||
            lvlnum < 0 || (size_t)lvlnum >= _globalMapRegions.MapRegions.size() )
        return 0;

    if ( saveFile.find(".sgm") != std::string::npos || saveFile.find(".SGM") != std::string::npos )
    {
        _maxReloadConst = 0;
        _maxRoboEnergy = 0;
    }

    _extraViewNumber = -1;
    _extraViewEnable = false;

    TLevelDescription mapp;

    _particles.Clear();

    if ( LevelCommonLoader(&mapp, lvlnum, 0) )
    {
        if ( LoadTypeMap(mapp.TypStr) )
        {
            if ( LoadOwnerMap(mapp.OwnStr) )
            {
                if ( LoadHightMap(mapp.HgtStr) )
                {
                    if ( LoadBlgMap(mapp.BlgStr) )
                        loadOK = true;
                }
            }
        }
    }

    if ( !loadOK )
    {
        DeleteLevel();
        return 0;
    }

    _levelInfo.OwnerMask = 0;
    _levelInfo.UserMask = 0;

    bact_id = 0x10000;
    dword_5A7A80 = 0;

    InitSuperItems();

    _lvlPrimevalTypeMap = _lvlTypeMap;
    _lvlPrimevalOwnMap = _lvlOwnMap;

    if ( !LoadingParseSaveFile(saveFile) )
    {
        DeleteLevel();
        return 0;
    }

    dword_5A7A80++;
    bact_id++;

    if ( NC_STACK_yparobo *userRobo = dynamic_cast<NC_STACK_yparobo *>(_userRobo) )
        userRobo->setROBO_commCount(dword_5A7A80);

    LoadingUnitsRefresh();

    if ( saveFile.find(".fin") != std::string::npos || saveFile.find(".FIN") != std::string::npos )
        InitBuddies();

    for(int y = 0; y < _mapSize.y; y++)
    {
        for(int x = 0; x < _mapSize.x; x++)
        {
            CellCheckHealth(&_cells(x, y), World::OWNER_NOCHANGE, nullptr);
        }
    }

    InitGates();
    UpdatePowerEnergy();

    PrepareAllFillers();

    if ( !sb_0x451034(this) )
    {
        DeleteLevel();
        return 0;
    }

    return 1;
}

size_t NC_STACK_ypaworld::SaveGame(const std::string &saveFile)
{
    bool write_ok = true;

    if ( saveFile.find(".sgm") != std::string::npos || saveFile.find(".SGM") != std::string::npos )
        uaDeleteFile( fmt::sprintf("save:%s/sgisold.txt", _GameShell->UserName) );

    int write_modifers;
    int write_user;
    int write_level_statuses;

    bool isfin_save = saveFile.find(".fin") != std::string::npos || saveFile.find(".FIN") != std::string::npos;

    if ( isfin_save )
    {
        _maxRoboEnergy = _userRobo->_energy_max;
        write_modifers = 0;
        write_user = 0;
        write_level_statuses = 0;
        _maxReloadConst = _userRobo->_reload_const;
    }
    else
    {
        write_level_statuses = 1;
        write_modifers = 1;
        write_user = 1;
    }

    // Force to write last frame timestamp into history
    if (_historyLastIsTimeStamp)
        _history.Write( _historyLastFrame.MakeByteArray() );

    FSMgr::FileHandle *fil = uaOpenFileAlloc( saveFile, "w");

    if ( !fil )
    {
        ypa_log_out("Unable to open savegame file\n");
        return 0;
    }

    if ( write_modifers )
    {
        if ( !yw_write_item_modifers(this, fil) )
            write_ok = false;
    }

    if ( write_user )
    {
        if ( !yw_write_user(fil, _GameShell) )
            write_ok = false;
    }

    if ( write_ok )
    {
        if ( yw_write_levelnum(this, fil) )
        {
            yw_write_ownermap(this, fil);
            yw_write_buildmap(this, fil);
            yw_write_energymap(this, fil);

            if ( yw_write_units(fil) )
            {
                if ( yw_write_wunderinfo(this, fil) )
                {
                    if ( yw_write_kwfactor(this, fil) )
                    {
                        if ( yw_write_globals(this, fil, isfin_save) )
                        {
                            if ( yw_write_superbomb(this, fil) )
                                write_ok = true;
                        }
                    }
                }
            }
        }
    }

    if ( write_ok && write_level_statuses )
    {
        for (int i = 0; i < 256; i++)
        {
            if ( _globalMapRegions.MapRegions[i].Status != TMapRegionInfo::STATUS_NONE )
            {
                if ( !yw_write_level_status(fil, this, i) )
                {
                    write_ok = false;
                    break;
                }
            }
        }
    }

    if ( write_ok )
    {
        if ( !isfin_save )
        {
            yw_write_history(this, fil);
            yw_write_masks(this, fil);
        }
    }

    if (_script)
    {
        std::string buf = _script->GetSaveString();

        if (!buf.empty())
        {
            fil->printf("\nbegin_luascript\n");
            fil->printf("%s\n", buf.c_str());
            fil->printf("EOF\n");
            fil->printf("end\n");
        }
    }

    delete fil;
    return write_ok;
}

// Saving game
bool NC_STACK_ypaworld::SaveSettings(UserData *usr, const std::string &fileName, uint32_t sdfMask)
{
    if ( fileName.empty() )
    {
        ypa_log_out("No names for save action\n");
        return false;
    }

    FSMgr::FileHandle *sfil = uaOpenFileAlloc(fmt::sprintf("save:%s", fileName), "w");

    if ( !sfil )
        return false;

    if ( (sdfMask & World::SDF_USER) && !yw_write_user(sfil, usr) )
    {
        ypa_log_out("Unable to write user data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_INPUT) && !yw_write_input(sfil, usr) )
    {
        ypa_log_out("Unable to write input data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SOUND) && !yw_write_sound(sfil, usr) )
    {
        ypa_log_out("Unable to write sound data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_VIDEO) && !yw_write_video(sfil, usr) )
    {
        ypa_log_out("Unable to write video data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SCORE) && !yw_write_levels_statuses(sfil, usr->p_YW) )
    {
        ypa_log_out("Unable to write score data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_BUDDY) && !yw_write_buddies(sfil, usr->p_YW) )
    {
        ypa_log_out("Unable to write buddies to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_SHELL) && !yw_write_shell(sfil, usr) )
    {
        ypa_log_out("Unable to write shell data to file\n");
        delete sfil;
        return false;
    }

    if ( (sdfMask & World::SDF_PROTO) && !yw_write_item_modifers(usr->p_YW, sfil) )
    {
        ypa_log_out("Unable to write build info to file\n");
        delete sfil;
        return false;
    }

    delete sfil;
    return true;
}

int NC_STACK_ypaworld::ParseSettingsFile(const std::string &fname, uint32_t sdfMask)
{
    ScriptParser::HandlersList parsers;
    if ( sdfMask & World::SDF_USER )
        parsers += new World::Parsers::UserParser(this);

    if ( sdfMask & World::SDF_INPUT )
        parsers += new World::Parsers::InputParser(this);

    if ( sdfMask & World::SDF_VIDEO )
        parsers += new World::Parsers::VideoParser(this);

    if ( sdfMask & World::SDF_SOUND )
        parsers += new World::Parsers::SoundParser(this);

    if ( sdfMask & World::SDF_SCORE )
        parsers += new World::Parsers::LevelStatusParser(this, true);

    if ( sdfMask & World::SDF_BUDDY )
        parsers += new World::Parsers::BuddyParser(this);

    if ( sdfMask & World::SDF_SHELL )
        parsers += new World::Parsers::ShellParser(this);

    if ( sdfMask & World::SDF_PROTO )
    {
        parsers += new World::Parsers::BuffProfileParser(&_buffProfiles);
        parsers += new World::Parsers::DebuffProfileParser(&_debuffProfiles);
        parsers += new World::Parsers::VhclProtoParser(this);
        parsers += new World::Parsers::WeaponProtoParser(this);
        parsers += new World::Parsers::BuildProtoParser(this);
    }

    const bool parsed = ScriptParser::ParseFile(fname, parsers, 0);
    if ( parsed && (sdfMask & World::SDF_PROTO) )
        ResolveStatusProfileLinks();
    return parsed;
}

// Load user settings (Global save)
size_t NC_STACK_ypaworld::LoadSettings(const std::string &fileName, const std::string &userName, uint32_t sdfMask, bool updateGameShell, bool playIntro)
{
    if ( sdfMask & World::SDF_SCORE )
    {
        if ( _GameShell->IgnoreScoreSaving )
        {
            _GameShell->IgnoreScoreSaving = false;
        }
        else
        {
            SaveSettings(_GameShell, fmt::sprintf("%s/user.txt", _GameShell->UserName), World::SDF_ALL);
        }
    }

    _GameShell->savedDataFlags = 0;

    if ( (sdfMask & World::SDF_ALL) == World::SDF_ALL )
        _GameShell->hasModernSettings = false;

    if ( sdfMask & World::SDF_BUDDY )
        _levelInfo.Buddies.clear();
    if ( !ParseSettingsFile(fmt::sprintf("save:%s", fileName), sdfMask) )
    {
        ypa_log_out("Error while loading information from %s\n", fileName.c_str());
        return 0;
    }

    if (playIntro && !_GameShell->remoteMode)
        SetGameShellVideoMode( _GameShell->IsWindowedFlag() );

    if ( updateGameShell && !_GameShell->HasInited && !OpenGameShell() ) // Init menus
    {
        ypa_log_out("Unable to open GameShell\n");
        return 0;
    }


    if ( (sdfMask & World::SDF_SCORE) && (_GameShell->savedDataFlags & World::SDF_SCORE) )
    {
        _GameShell->UserName = userName;
    }

    // Retail profiles have no engine-written video keys. Reuse the menu resets
    // once; their normal save writes interface_style, preserving future edits.
    if ( updateGameShell && (sdfMask & World::SDF_ALL) == World::SDF_ALL &&
         !_GameShell->hasModernSettings )
    {
        ypa_log_out("OpenNeoUA: initializing current Options and input defaults for original profile %s.\n", userName.c_str());
        _GameShell->InputConfigRestoreDefault();
        _GameShell->sub_46D2B4();
        _GameShell->ShowOptionsMenu();
        _GameShell->ResetOptionsToDefaults();
        _GameShell->sb_0x46aa8c();
        _GameShell->hasModernSettings = true;
    }

    if (playIntro && !_GameShell->remoteMode)
    {
        PlayIntroMovie();
    }

    const bool persistInputMigration = _GameShell->inputDefaultsMigrated;
    _GameShell->inputDefaultsMigrated = false;

    if ( persistInputMigration && (sdfMask & World::SDF_ALL) == World::SDF_ALL )
    {
        if ( !SaveSettings(_GameShell, fileName, World::SDF_ALL) )
            ypa_log_out("Warning: unable to persist migrated input defaults in %s\n", fileName.c_str());
    }

    if ( sdfMask & World::SDF_INPUT )
        _GameShell->InputConfCopyToBackup();

    if ( updateGameShell )
        UpdateGameShell(); // Update menu values

    return 1;
}

bool NC_STACK_ypaworld::ReloadInput(size_t id)
{
    std::string keyConfStr;

    if ( id < 1 || id >= _GameShell->InputConfig.size() )
        return false;

    UserData::TInputConf &kconf = _GameShell->InputConfig.at(id);

    if ( UserData::IsInputBindingRetired((int)id) )
    {
        kconf.PKeyCode = Input::KC_NONE;
        kconf.NKeyCode = Input::KC_NONE;
        kconf.PKeyCodeBkp = Input::KC_NONE;
        kconf.NKeyCodeBkp = Input::KC_NONE;
        kconf.SetFlags = 0;

        if ( kconf.Type == World::INPUT_BIND_TYPE_HOTKEY )
            Input::Engine.SetHotKey(kconf.KeyID, "nop");
        else if ( kconf.Type == World::INPUT_BIND_TYPE_BUTTON )
            Input::Engine.SetInputExpression(false, kconf.KeyID, "nop");
        else if ( kconf.Type == World::INPUT_BIND_TYPE_SLIDER )
            Input::Engine.SetInputExpression(true, kconf.KeyID, "nop");

        return true;
    }

    if ( Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name.empty() )
        return false;

    if ( kconf.Type == World::INPUT_BIND_TYPE_SLIDER && Input::Engine.KeyNamesTable.at(kconf.NKeyCode).Name.empty() )
        return false;

    if ( kconf.Type == World::INPUT_BIND_TYPE_SLIDER )
    {
        keyConfStr += "~#";
        keyConfStr += "winp:";
        keyConfStr += Input::Engine.KeyNamesTable.at(kconf.NKeyCode).Name;
        keyConfStr += " #";
        keyConfStr += "winp:";
    }
    else if ( kconf.Type == World::INPUT_BIND_TYPE_BUTTON )
    {
        keyConfStr += "winp:";
    }

    if ( Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name.empty() )
        return false;

    keyConfStr += Input::Engine.KeyNamesTable.at(kconf.PKeyCode).Name;

    if ( kconf.Type == World::INPUT_BIND_TYPE_HOTKEY )
    {
        // Sprint and the runtime toggles are polled directly. This allows their
        // contextual defaults to coexist with legacy actions that use the same
        // physical key without stealing a global HotKeyID slot.
        if ( id == World::INPUT_BIND_SPRINT ||
             id == World::INPUT_BIND_TOGGLE_UFO_SPY_UI ||
             id == World::INPUT_BIND_HIDE_VEHICLE )
        {
            // The toggles keep their HotKeyID slot free so the key cannot fire a
            // second, unhandled hotkey while the direct poll already owns it.
            if ( id == World::INPUT_BIND_TOGGLE_UFO_SPY_UI ||
                 id == World::INPUT_BIND_HIDE_VEHICLE )
                Input::Engine.SetHotKey(kconf.KeyID, "nop");
            return true;
        }

        if ( !Input::Engine.SetHotKey(kconf.KeyID, keyConfStr) )
            ypa_log_out("input.engine: WARNING: Hotkey[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
    }
    else
    {
        if ( kconf.Type == World::INPUT_BIND_TYPE_BUTTON )
        {
            if ( !Input::Engine.SetInputExpression(false, kconf.KeyID, keyConfStr) )
                ypa_log_out("input.engine: WARNING: Button[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
        }
        else
        {
            if ( !Input::Engine.SetInputExpression(true, kconf.KeyID, keyConfStr) )
                ypa_log_out("input.engine: WARNING: Slider[%d] (%s) not accepted.\n", kconf.KeyID, keyConfStr.c_str());
        }
    }
    kconf.NKeyCodeBkp = kconf.NKeyCode;
    kconf.PKeyCodeBkp = kconf.PKeyCode;
    return true;
}

size_t NC_STACK_ypaworld::SetGameShellVideoMode(bool windowed)
{
    UserData *usr = _GameShell;

    if (System::IniConf::MenuWindowed.Get<bool>())
        windowed = true;

    if ( _shellGfxMode == GFX::Engine.GetGfxMode() && windowed == GFX::Engine.GetGfxMode().windowed )
    {
        const Common::Point physicalSize = GFX::Engine.GetScreenSize();
        _screenSize = GFX::Engine.GetVirtualUIResolution();
        Input::Engine.SetPointerResolution(physicalSize, _screenSize);
        Gui::Root::Instance.SetScreenSize(_screenSize);
        return 1;
    }

    int v6;

    if ( usr->HasInited )
    {
        CloseGameShell();
        v6 = 1;
    }
    else
    {
        v6 = 0;
    }

    GFX::Engine.SetResolution( _shellGfxMode, windowed );

    const Common::Point physicalSize = GFX::Engine.GetScreenSize();
    _screenSize = GFX::Engine.GetVirtualUIResolution();
    Input::Engine.SetPointerResolution(physicalSize, _screenSize);
    Gui::Root::Instance.SetScreenSize(_screenSize);

    if ( v6 && !OpenGameShell())
    {
        ypa_log_out("Error: Unable to open GameShell with mode %d x %d\n", _shellGfxMode.x, _shellGfxMode.y);

        return 0;
    }

    if ( usr->GFXFlags & World::GFX_FLAG_SOFTMOUSE )
    {
        GFX::Engine.setWDD_cursor(1);
    }
    else
    {
        GFX::Engine.setWDD_cursor(0);
    }

    std::string fontStr = _screenSize.x >= 512 ? Locale::Text::Font() : Locale::Text::SmallFont();
    fontStr = System::ResolveMenuFontDescr(fontStr);
    GFX::Engine.LoadFontByDescr(fontStr);
    Gui::UA::LoadFont(fontStr);

    return 1;
}

size_t NC_STACK_ypaworld::ReloadLanguage()
{
    if ( !_GameShell->default_lang_dll )
    {
        ypa_log_out("Set Language, but no language selected\n");
        return 0;
    }

    int v6;

    if ( _GameShell->HasInited )
    {
        CloseGameShell();
        v6 = 1;
    }
    else
    {
        v6 = 0;
    }

    if (!ypaworld_func166(*_GameShell->default_lang_dll))
    {
        ypa_log_out("ERROR: SETLANGUAGE failed.\n");
        return 0;
    }

    if ( v6 && !OpenGameShell() )
    {
        ypa_log_out("Unable to open GameShell\n");
        return 0;
    }

    return 1;
}

void NC_STACK_ypaworld::UpdateGuiSettings()
{
    Gui::UA::_UATextColor = _iniColors[60];
    Gui::UA::_UAButtonTextColor = _iniColors[68];



    /*for (uint8_t i = 0; i < ypaworld.tiles.size(); i++)
        Gui::UA::_UATiles[i] = ypaworld.tiles[i];*/
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
