// Script parsers for the player profile (user.txt): user, input, video,
// sound, level status, buddies and shell windows.
#include "parsers.h"
#include "../fmtlib/core.h"
#include "../fmtlib/printf.h"
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/ypaflyer.h"
#include "game/ypacar.h"
#include "common/glyph_key.h"
#include "common/log.h"
#include "common/utils.h"
#include "../system/inivals.h"
#include "spin.h"
#include "tools.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include "parsers_internal.h"

namespace World
{
namespace Parsers
{

bool UserParser::ReadUserNameFile(const std::string &filename)
{
    if ( !_o._GameShell->UserName.empty() )
        return false;

    std::string buf = fmt::sprintf("save:%s/%s", _o._GameShell->UserName, filename);

    // Optional legacy callsign storage. It is created on save when needed.
    if ( !uaFileExist(buf) )
        return false;

    FSMgr::FileHandle *signFile = uaOpenFileAlloc(buf, "r");

    if ( !signFile )
        return false;

    bool res = signFile->ReadLine(&_o._GameShell->netPlayerName);

    delete signFile;
    return res;
}

bool UserParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (StriCmp(word, "new_user"))
        return false;

    // Profiles and older saves without this field start with an empty reserve.
    _o._plasmaCurrencyBank = 0;
    _o._workshopOwned.clear();
    _o._workshopHost=-1;

    if (!_o._GameShell->remoteMode)
    {
        if ( !ReadUserNameFile("callsign.def") )
            _o._GameShell->netPlayerName =  Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
    }
    return true;
}

int UserParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( !StriCmp(p1, "username") )
    {
    }
    else if ( !StriCmp(p1, "netname") )
    {
    }
    else if ( !StriCmp(p1, "plasma_bank") )
    {
        _o._plasmaCurrencyBank = NC_STACK_ypaworld::ParsePlasmaCurrencyAmount(p2);
    }
    else if(!StriCmp(p1,"workshop_owned")||!StriCmp(p1,"workshop_host"))
    {
        try {
            size_t end=0;const int id=std::stoi(p2,&end);
            if(end==p2.size()&&id>=0&&id<(int)_o._vhclProtos.size()) {
                if(!StriCmp(p1,"workshop_owned"))_o._workshopOwned.insert(id);
                else _o._workshopHost=id;
            }
        } catch(const std::exception &) {}
    }
    else if ( !StriCmp(p1, "maxroboenergy") )
    {
        _o._maxRoboEnergy = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "maxreloadconst") )
    {
        _o._maxReloadConst = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "numbuddies") )
    {
    }
    else if ( !StriCmp(p1, "beamenergy") )
    {
        _o._beamEnergyCapacity = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "playerstatus") )
    {
        Stok stok(p2, "_ \t");
        std::string val;
        if ( stok.GetNext(&val) )
        {
            int plid = parser.stol(val, nullptr, 0);
            if ( stok.GetNext(&val) )
            {
                _o._playersStats[plid].DestroyedUnits = parser.stol(val, nullptr, 0);
                if ( stok.GetNext(&val) )
                {
                    _o._playersStats[plid].DestroyedByUser = parser.stol(val, nullptr, 0);
                    if ( stok.GetNext(&val) )
                    {
                        _o._playersStats[plid].ElapsedTime = parser.stol(val, nullptr, 0);
                        if ( stok.GetNext(&val) )
                        {
                            _o._playersStats[plid].SectorsTaked = parser.stol(val, nullptr, 0);
                            if ( stok.GetNext(&val) )
                            {
                                _o._playersStats[plid].Score = parser.stol(val, nullptr, 0);
                                if ( stok.GetNext(&val) )
                                {
                                    _o._playersStats[plid].Power = parser.stol(val, nullptr, 0);
                                    if ( stok.GetNext(&val) )
                                    {
                                        _o._playersStats[plid].Upgrades = parser.stol(val, nullptr, 0);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else if ( !StriCmp(p1, "jodiefoster") )
    {
        Stok stok(p2, "_ \t");
        std::string val;
        if ( stok.GetNext(&val) )
        {
            _o._levelInfo.JodieFoster[0] = parser.stol(val, nullptr, 0);
            if ( stok.GetNext(&val) )
            {
                _o._levelInfo.JodieFoster[1] = parser.stol(val, nullptr, 0);
                if ( stok.GetNext(&val) )
                {
                    _o._levelInfo.JodieFoster[2] = parser.stol(val, nullptr, 0);
                    if ( stok.GetNext(&val) )
                    {
                        _o._levelInfo.JodieFoster[3] = parser.stol(val, nullptr, 0);
                        if ( stok.GetNext(&val) )
                        {
                            _o._levelInfo.JodieFoster[4] = parser.stol(val, nullptr, 0);
                            if ( stok.GetNext(&val) )
                            {
                                _o._levelInfo.JodieFoster[5] = parser.stol(val, nullptr, 0);
                                if ( stok.GetNext(&val) )
                                {
                                    _o._levelInfo.JodieFoster[6] = parser.stol(val, nullptr, 0);
                                    if ( stok.GetNext(&val) )
                                        _o._levelInfo.JodieFoster[7] = parser.stol(val, nullptr, 0);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool InputParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( !StriCmp(word, "new_input") )
    {
        _isNewInputScope = true;
        _legacyCameraZoomInSeen = false;
        _legacyCameraZoomOutSeen = false;
        _mapFocusSeen = false;
        _legacyCameraZoomInKey = Input::KC_NONE;
        _legacyCameraZoomOutKey = Input::KC_NONE;

        for (size_t i = 0; i < _o._GameShell->InputConfig.size(); i++)
        {
            // Preserve defaults for bindings introduced after older user.txt files.
            // Explicit entries, including nop, still override these defaults below.
            if ( i == World::INPUT_BIND_SPRINT ||
                 i == World::INPUT_BIND_CYCLE_TARGET ||
                 i == World::INPUT_BIND_ALTERNATIVE_VIEW ||
                 i == World::INPUT_BIND_TOGGLE_UFO_SPY_UI ||
                 i == World::INPUT_BIND_MAP_FOCUS )
                continue;

            UserData::TInputConf &k = _o._GameShell->InputConfig[i];
            k.PKeyCode = 0;
            k.NKeyCode = 0;
        }
        return true;
    }
    else if ( !StriCmp(word, "modify_input") )
    {
        _isNewInputScope = false;
        return true;
    }

    return false;
}

int InputParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( _isNewInputScope )
        {
            bool migrated = _legacyCameraZoomInSeen || _legacyCameraZoomOutSeen;

            UserData::TInputConf &zoomIn =
                _o._GameShell->InputConfig[World::INPUT_BIND_ZOOMIN];
            UserData::TInputConf &zoomOut =
                _o._GameShell->InputConfig[World::INPUT_BIND_ZOOMOUT];

            // Import the short-lived split Camera Zoom bindings only when the
            // surviving map binding still has that build's known Up/Down default
            // (or is empty). Custom map bindings remain authoritative.
            if ( zoomIn.PKeyCode == Input::KC_UP ||
                 (zoomIn.PKeyCode == Input::KC_NONE && _legacyCameraZoomInSeen) )
            {
                zoomIn.PKeyCode =
                    (_legacyCameraZoomInSeen && _legacyCameraZoomInKey != Input::KC_NONE)
                    ? _legacyCameraZoomInKey : Input::KC_NUMPLUS;
                _o.ReloadInput(World::INPUT_BIND_ZOOMIN);
                migrated = true;
            }

            if ( zoomOut.PKeyCode == Input::KC_DOWN ||
                 (zoomOut.PKeyCode == Input::KC_NONE && _legacyCameraZoomOutSeen) )
            {
                zoomOut.PKeyCode =
                    (_legacyCameraZoomOutSeen && _legacyCameraZoomOutKey != Input::KC_NONE)
                    ? _legacyCameraZoomOutKey : Input::KC_NUMMINUS;
                _o.ReloadInput(World::INPUT_BIND_ZOOMOUT);
                migrated = true;
            }

            // The legacy slots must never remain active: otherwise a key used
            // only by an old Camera Zoom entry could still produce HotKeyID 50/51.
            Input::Engine.SetHotKey(50, "nop");
            Input::Engine.SetHotKey(51, "nop");

            // OpenNeoUA input-default migration. Older builds used ALT for Switch
            // Weapon (or the earlier TAB/TAB pair) and CTRL for Launch Missile
            // Cam. Move only those known default combinations to the current
            // defaults: Switch Weapon = CTRL, Launch Missile Cam = Grave Accent.
            // Custom bindings remain authoritative and are never stolen.
            UserData::TInputConf &switchWeapon =
                _o._GameShell->InputConfig[World::INPUT_BIND_SWITCH_WEAPON];
            UserData::TInputConf &cycleTarget =
                _o._GameShell->InputConfig[World::INPUT_BIND_CYCLE_TARGET];
            UserData::TInputConf &missileCam =
                _o._GameShell->InputConfig[World::INPUT_BIND_CAMFIRE];

            const bool knownOldSwitchDefault =
                switchWeapon.PKeyCode == Input::KC_ALT ||
                (switchWeapon.PKeyCode == Input::KC_TAB &&
                 cycleTarget.PKeyCode == Input::KC_TAB);
            const bool knownOldMissileCamDefault =
                missileCam.PKeyCode == Input::KC_CTRL;

            if ( knownOldSwitchDefault && knownOldMissileCamDefault )
            {
                bool ctrlUsedElsewhere = false;
                bool graveUsedElsewhere = false;

                for ( size_t i = 1; i < _o._GameShell->InputConfig.size(); ++i )
                {
                    if ( i == World::INPUT_BIND_SWITCH_WEAPON ||
                         i == World::INPUT_BIND_CYCLE_TARGET ||
                         i == World::INPUT_BIND_CAMFIRE ||
                         UserData::IsInputBindingRetired((int)i) )
                        continue;

                    const UserData::TInputConf &cfg = _o._GameShell->InputConfig[i];
                    ctrlUsedElsewhere |=
                        cfg.PKeyCode == Input::KC_CTRL || cfg.NKeyCode == Input::KC_CTRL;
                    graveUsedElsewhere |=
                        cfg.PKeyCode == Input::KC_EXTRA7 || cfg.NKeyCode == Input::KC_EXTRA7;
                }

                if ( !ctrlUsedElsewhere && !graveUsedElsewhere )
                {
                    switchWeapon.PKeyCode = Input::KC_CTRL;
                    missileCam.PKeyCode = Input::KC_EXTRA7;
                    _o.ReloadInput(World::INPUT_BIND_SWITCH_WEAPON);
                    _o.ReloadInput(World::INPUT_BIND_CAMFIRE);
                    migrated = true;
                }
            }

            // UFO Spy Mode used U as its provisional default in older OpenNeoUA
            // profiles. Space is now the contextual default shown in Input Settings.
            // Only the known old U default is migrated; every other custom key stays
            // authoritative. The binding is polled directly, so sharing Space with
            // Host Station / normal-unit actions is intentional and safe.
            UserData::TInputConf &ufoSpyToggle =
                _o._GameShell->InputConfig[World::INPUT_BIND_TOGGLE_UFO_SPY_UI];
            if ( ufoSpyToggle.PKeyCode == Input::KC_U )
            {
                ufoSpyToggle.PKeyCode = Input::KC_SPACE;
                _o.ReloadInput(World::INPUT_BIND_TOGGLE_UFO_SPY_UI);
                migrated = true;
            }

            // Map Focus is newer than older user.txt profiles. Preserve the
            // new E default only when E is still free. An explicit slot 53
            // entry, including nop, is always authoritative.
            if ( !_mapFocusSeen )
            {
                UserData::TInputConf &mapFocus =
                    _o._GameShell->InputConfig[World::INPUT_BIND_MAP_FOCUS];
                bool eAlreadyUsed = false;

                for ( size_t i = 1; i < _o._GameShell->InputConfig.size(); ++i )
                {
                    if ( i == World::INPUT_BIND_MAP_FOCUS ||
                         UserData::IsInputBindingRetired((int)i) )
                        continue;

                    const UserData::TInputConf &cfg = _o._GameShell->InputConfig[i];
                    if ( cfg.PKeyCode == Input::KC_E || cfg.NKeyCode == Input::KC_E )
                    {
                        eAlreadyUsed = true;
                        break;
                    }
                }

                if ( eAlreadyUsed && mapFocus.PKeyCode == Input::KC_E )
                {
                    mapFocus.PKeyCode = Input::KC_NONE;
                    Input::Engine.SetHotKey(mapFocus.KeyID, "nop");
                    migrated = true;
                }
            }

            // Alternative View replaces the provisional Bomb Sight binding. New
            // profiles default to F. Profiles that still contain the old T default
            // are migrated to F only when F is free; custom bindings are preserved.
            // If an older profile has no entry for this newer action, its preserved
            // F default is cleared rather than stealing an already-used F key.
            UserData::TInputConf &alternativeView =
                _o._GameShell->InputConfig[World::INPUT_BIND_ALTERNATIVE_VIEW];

            bool fAlreadyUsed = false;
            bool currentAlternativeKeyAlreadyUsed = false;
            for ( size_t i = 1; i < _o._GameShell->InputConfig.size(); ++i )
            {
                if ( i == World::INPUT_BIND_ALTERNATIVE_VIEW ||
                     UserData::IsInputBindingRetired((int)i) )
                    continue;

                const UserData::TInputConf &cfg = _o._GameShell->InputConfig[i];
                if ( cfg.PKeyCode == Input::KC_F || cfg.NKeyCode == Input::KC_F )
                    fAlreadyUsed = true;

                if ( alternativeView.PKeyCode != Input::KC_NONE &&
                     (cfg.PKeyCode == alternativeView.PKeyCode ||
                      cfg.NKeyCode == alternativeView.PKeyCode) )
                    currentAlternativeKeyAlreadyUsed = true;
            }

            if ( alternativeView.PKeyCode == Input::KC_T && !fAlreadyUsed )
            {
                alternativeView.PKeyCode = Input::KC_F;
                _o.ReloadInput(World::INPUT_BIND_ALTERNATIVE_VIEW);
                migrated = true;
            }
            else if ( (alternativeView.PKeyCode == Input::KC_F && fAlreadyUsed) ||
                      (alternativeView.PKeyCode == Input::KC_T && currentAlternativeKeyAlreadyUsed) )
            {
                alternativeView.PKeyCode = Input::KC_NONE;
                // ReloadInput() rejects KC_NONE before updating the expression.
                Input::Engine.SetInputExpression(false, alternativeView.KeyID, "nop");
                migrated = true;
            }

            bool retiredBindingFound = false;
            for ( int binding = 1; binding < World::INPUT_BIND_MAX; ++binding )
            {
                if ( !UserData::IsInputBindingRetired(binding) )
                    continue;

                const UserData::TInputConf &cfg = _o._GameShell->InputConfig[binding];
                if ( cfg.PKeyCode != Input::KC_NONE || cfg.NKeyCode != Input::KC_NONE )
                {
                    retiredBindingFound = true;
                    break;
                }
            }

            if ( retiredBindingFound )
            {
                _o._GameShell->RetireInputBindings(false);
                migrated = true;
            }

            if ( migrated )
                _o._GameShell->inputDefaultsMigrated = true;
        }

        return ScriptParser::RESULT_SCOPE_END;
    }

    _o._GameShell->savedDataFlags |= World::SDF_INPUT;

    if ( !StriCmp(p1, "qualmode") )
    {
    }
    else if ( !StriCmp(p1, "joystick") )
    {
        if ( StrGetBool(p2) )
        {
             _o._GameShell->joystickEnabled = true;
             _o._preferences &= ~World::PREF_JOYDISABLE;
        }
        else
        {
            _o._GameShell->joystickEnabled = false;
            _o._preferences |= World::PREF_JOYDISABLE;
        }
    }
    else if ( !StriCmp(p1, "altjoystick") )
    {
        if ( StrGetBool(p2) )
        {
            _o._GameShell->altJoystickEnabled = true;
            _o._preferences |= World::PREF_ALTJOYSTICK;
        }
        else
        {
            _o._GameShell->altJoystickEnabled = false;
            _o._preferences &= ~World::PREF_ALTJOYSTICK;
        }
    }
    else if ( !StriCmp(p1, "forcefeedback") )
    {
        if ( StrGetBool(p2) )
            _o._preferences &= ~World::PREF_FFDISABLE;
        else
            _o._preferences |= World::PREF_FFDISABLE;
    }
    else
    {
        std::string buf;
        for (std::string::const_iterator it = p2.cbegin(); it != p2.cend(); it++)
        {
            if (*it == '_')
                buf += ' ';
            else if (*it == '$')
                buf += "winp:";
            else
                buf += *it;
        }

        if ( !StriCmp(p1.substr(0,13), "input.slider[") )
        {
            bool ok = false;
            int cfgIdex = parser.stoi( Stok::Fast(p1.substr(13), "] \t=\n") );

            if ( !Input::Engine.SetInputExpression(true, cfgIdex, buf) )
            {
                ypa_log_out("WARNING: cannot set slider %d with %s\n", cfgIdex, buf.c_str());
                return ScriptParser::RESULT_BAD_DATA;
            }

            int gsIndex = UserData::InputIndexFromConfig(World::INPUT_BIND_TYPE_SLIDER, cfgIdex);
            if ( gsIndex == -1 )
            {
                ypa_log_out("Unknown number in slider-declaration (%d)\n", cfgIdex);
                return ScriptParser::RESULT_BAD_DATA;
            }
            _o._GameShell->InputConfig[ gsIndex ].Type = World::INPUT_BIND_TYPE_SLIDER;
            _o._GameShell->InputConfig[ gsIndex ].KeyID = cfgIdex;

            Stok stok(buf, " :\t\n");
            std::string tmp;
            if ( stok.GetNext(&tmp) && stok.GetNext(&tmp) ) // skip drivername before ':'
            {
                _o._GameShell->InputConfig[ gsIndex ].NKeyCode = Input::Engine.GetKeyIDByName(tmp);

                if ( _o._GameShell->InputConfig[ gsIndex ].NKeyCode == -1 )
                {
                    ypa_log_out("Unknown keyword for slider %s\n", tmp.c_str());
                    return ScriptParser::RESULT_BAD_DATA;
                }

                if ( stok.GetNext(&tmp) && stok.GetNext(&tmp) ) // skip drivername before ':'
                {
                    _o._GameShell->InputConfig[ gsIndex ].PKeyCode = Input::Engine.GetKeyIDByName(tmp);

                    if ( _o._GameShell->InputConfig[ gsIndex ].PKeyCode == -1 )
                    {
                        ypa_log_out("Unknown keyword for slider %s\n", tmp.c_str());
                        return ScriptParser::RESULT_BAD_DATA;
                    }
                    ok = 1;
                }
            }

            if ( !ok )
            {
                ypa_log_out("Wrong input expression for slider %d\n", cfgIdex);
                return ScriptParser::RESULT_BAD_DATA;
            }
        }
        else if ( !StriCmp(p1.substr(0,13), "input.button[") )
        {
            bool ok = false;

            int cfgIdex = parser.stoi( Stok::Fast(p1.substr(13), "] \t=\n") );

            if ( !Input::Engine.SetInputExpression(false, cfgIdex, buf) )
            {
                ypa_log_out("WARNING: cannot set button %d with %s\n", cfgIdex, buf.c_str());
                return ScriptParser::RESULT_BAD_DATA;
            }

            int gsIndex = UserData::InputIndexFromConfig(World::INPUT_BIND_TYPE_BUTTON, cfgIdex);
            if ( gsIndex == -1 )
            {
                ypa_log_out("Unknown number in button-declaration (%d)\n", cfgIdex);
                return ScriptParser::RESULT_BAD_DATA;
            }
            _o._GameShell->InputConfig[ gsIndex ].Type = World::INPUT_BIND_TYPE_BUTTON;
            _o._GameShell->InputConfig[ gsIndex ].KeyID = cfgIdex;

            Stok stok(buf, " :\t\n");
            std::string tmp;
            if ( stok.GetNext(&tmp) && stok.GetNext(&tmp) ) // skip drivername before ':'
            {
                _o._GameShell->InputConfig[ gsIndex ].PKeyCode = Input::Engine.GetKeyIDByName(tmp);

                if ( _o._GameShell->InputConfig[ gsIndex ].PKeyCode == -1 )
                {
                    ypa_log_out("Unknown keyword for button %s\n", tmp.c_str());
                    return ScriptParser::RESULT_BAD_DATA;
                }
                ok = true;
            }

            if ( !ok )
            {
                ypa_log_out("Wrong input expression for button %d\n", cfgIdex);
                return ScriptParser::RESULT_BAD_DATA;
            }
        }
        else if ( !StriCmp(p1.substr(0,13), "input.hotkey[") )
        {
            bool ok = false;

            int cfgIdex = parser.stoi( Stok::Fast(p1.substr(13), "] \t=\n") );

            if ( !Input::Engine.SetHotKey(cfgIdex, buf) )
            {
                ypa_log_out("WARNING: cannot set hotkey %d with %s\n", cfgIdex, buf.c_str());
                return ScriptParser::RESULT_OK;
            }

            if ( cfgIdex == 50 || cfgIdex == 51 )
            {
                const std::string legacyKeyName = Stok::Fast(buf, " :\t\n");
                const int legacyKey = Input::Engine.GetKeyIDByName(legacyKeyName);

                if ( legacyKey == -1 )
                {
                    ypa_log_out("Unknown keyword for legacy camera zoom hotkey: %s\n",
                                legacyKeyName.c_str());
                    return ScriptParser::RESULT_OK;
                }

                if ( cfgIdex == 50 )
                {
                    _legacyCameraZoomInSeen = true;
                    _legacyCameraZoomInKey = legacyKey;
                }
                else
                {
                    _legacyCameraZoomOutSeen = true;
                    _legacyCameraZoomOutKey = legacyKey;
                }

                // Accept and immediately neutralize transient slots 50/51.
                Input::Engine.SetHotKey(cfgIdex, "nop");
                return ScriptParser::RESULT_OK;
            }

            if ( cfgIdex == 53 )
                _mapFocusSeen = true;

            int gsIndex = UserData::InputIndexFromConfig(World::INPUT_BIND_TYPE_HOTKEY, cfgIdex);
            if ( gsIndex == -1 )
            {
                ypa_log_out("Unknown number in hotkey-declaration (%d)\n", cfgIdex);
                return ScriptParser::RESULT_OK;
            }

            _o._GameShell->InputConfig[ gsIndex ].Type = World::INPUT_BIND_TYPE_HOTKEY;
            _o._GameShell->InputConfig[ gsIndex ].KeyID = cfgIdex;

            std::string tmp = Stok::Fast(buf, " :\t\n");
            if ( !tmp.empty() )
            {
                _o._GameShell->InputConfig[ gsIndex ].PKeyCode = Input::Engine.GetKeyIDByName(tmp);
                if ( _o._GameShell->InputConfig[ gsIndex ].PKeyCode == -1 )
                {
                    ypa_log_out("Unknown keyword for hotkey: %s\n", tmp.c_str());
                    return ScriptParser::RESULT_OK;
                }
                ok = true;
            }

            if ( !ok )
            {
                ypa_log_out("Wrong input expression for hotkey %d\n", cfgIdex);
                return ScriptParser::RESULT_BAD_DATA;
            }
        }
        else
        {
            ypa_log_out("Unknown keyword %s in InputExpression\n", p1.c_str());
            return ScriptParser::RESULT_UNKNOWN;
        }
    }
    return ScriptParser::RESULT_OK;
}

int VideoParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if (!_initialized)
    {
        // Existing profiles predate interface_style. Reset each parsed video
        // block to the Nucleus.ini default so loading another profile cannot
        // inherit the previous profile's choice. A profile-side value, when
        // present, is parsed below and remains authoritative for that profile.
        const GFX::VirtualUIStyle defaultStyle =
            System::IniConf::UiRetroInterface.Get<bool>()
                ? GFX::VirtualUIStyle::RETRO
                : GFX::VirtualUIStyle::SMOOTH;
        _o._GameShell->interfaceStyle = defaultStyle;
        _o._GameShell->confInterfaceStyle = defaultStyle;
        GFX::Engine.SetVirtualUIStyle(defaultStyle);
        _initialized = true;
    }

    if ( !StriCmp(p1, "end") )
    {
        if ( GFX::Engine.getWDD_drawPrim() )
            _o._GameShell->GFXFlags |= World::GFX_FLAG_DRAWPRIMITIVES;
        else
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_DRAWPRIMITIVES;

        if ( GFX::Engine.getWDD_16bitTex() )
            _o._GameShell->GFXFlags |= World::GFX_FLAG_16BITTEXTURE;
        else
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_16BITTEXTURE;

        return ScriptParser::RESULT_SCOPE_END;
    }

    _o._GameShell->savedDataFlags |= World::SDF_VIDEO;

    if ( !StriCmp(p1, "videomode") )
    {
        int modeid = parser.stoi(p2);
        _o._gameDefaultRes = Common::Point((modeid >> 12 & 0xFFF), (modeid & 0xFFF));
        _o._GameShell->game_default_res = modeid;
    }
    else if ( !StriCmp(p1, "farview") )
    {
        if ( StrGetBool(p2) )
        {
            _o._GameShell->GFXFlags |= World::GFX_FLAG_FARVIEW;
            _o.SetFarView(true);
        }
        else
        {
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_FARVIEW;
            _o.SetFarView(false);
        }
    }
    else if ( !StriCmp(p1, "filtering") )
    {
    }
    else if ( !StriCmp(p1, "drawprimitive") )
    {
        if ( StrGetBool(p2) )
            _o._GameShell->GFXFlags |= World::GFX_FLAG_DRAWPRIMITIVES;
        else
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_DRAWPRIMITIVES;
    }
    else if ( !StriCmp(p1, "16bittexture") )
    {
        if ( StrGetBool(p2) )
            _o._GameShell->GFXFlags |= World::GFX_FLAG_16BITTEXTURE;
        else
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_16BITTEXTURE;
    }
    else if ( !StriCmp(p1, "softmouse") )
    {
        if ( StrGetBool(p2) )
        {
            _o._GameShell->GFXFlags |= World::GFX_FLAG_SOFTMOUSE;
            _o._preferences |= World::PREF_SOFTMOUSE;

            GFX::Engine.setWDD_cursor(1);
        }
        else
        {
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_SOFTMOUSE;
            _o._preferences &= ~World::PREF_SOFTMOUSE;

            GFX::Engine.setWDD_cursor(0);
        }
    }
    else if ( !StriCmp(p1, "palettefx") )
    {
    }
    else if ( !StriCmp(p1, "heaven") )
    {
        if ( StrGetBool(p2) )
        {
            _o._GameShell->GFXFlags |= World::GFX_FLAG_SKYRENDER;
            _o.setYW_skyRender(true);
        }
        else
        {
            _o._GameShell->GFXFlags &= ~World::GFX_FLAG_SKYRENDER;
            _o.setYW_skyRender(false);
        }
    }
    else if ( !StriCmp(p1, "fxnumber") )
    {
        _o._fxLimit = parser.stoi(p2);
        _o._GameShell->fxnumber = _o._fxLimit;
    }
    else if ( !StriCmp(p1, "default_view") )
    {
        // Retired profile key: keep old user.txt files loadable without letting
        // profile data override the global Nucleus.ini cockpit setting.
        _o._GameShell->cockpitCameraRuntimeMode = System::IniConf::GameCockpitCamera.Get<bool>();
    }
    else if ( !StriCmp(p1, "interface_style") )
    {
        _o._GameShell->hasModernSettings = true;
        const GFX::VirtualUIStyle style = !StriCmp(p2, "smooth")
                                              ? GFX::VirtualUIStyle::SMOOTH
                                              : GFX::VirtualUIStyle::RETRO;
        _o._GameShell->interfaceStyle = style;
        _o._GameShell->confInterfaceStyle = style;
        GFX::Engine.SetVirtualUIStyle(style);
    }
    else if ( !StriCmp(p1, "genesis_list_order") )
    {
        // Retired profile key: keep old user.txt files loadable, but do not let
        // profile data override the global ui.genesis_list_order setting.
    }
    else if ( !StriCmp(p1, "palette_theme") )
    {
        _o._GameShell->hasModernSettings = true;
        std::string theme = p2;
        if (!StriCmp(theme, "Original"))
            theme.clear();

        _o._GameShell->paletteTheme = theme;
        _o._GameShell->confPaletteTheme = theme;
    }
    else if ( !StriCmp(p1, "enemyindicator") )
    {
        if ( StrGetBool(p2) )
        {
            _o._preferences |= World::PREF_ENEMYINDICATOR;
            _o._GameShell->enemyIndicator = true;
        }
        else
        {
            _o._preferences &= ~World::PREF_ENEMYINDICATOR;
            _o._GameShell->enemyIndicator = false;
        }
    }
    else if ( !StriCmp(p1, "gfxmode") )
    {
        _o._GameShell->hasModernSettings = true;
        Stok stok(p2, " _");
        std::string resW, resH, resWin;

        if ( stok.GetNext(&resW) && stok.GetNext(&resH) && stok.GetNext(&resWin))
        {
            int w = parser.stoi(resW);
            int h = parser.stoi(resH);
            int win = parser.stoi(resWin);

            const std::vector<GFX::GfxMode> &pModes = GFX::GFXEngine::Instance.GetAvailableModes();
            for (size_t i = 0; i < pModes.size(); ++i)
            {
                const GFX::GfxMode &mode = pModes.at(i);
                if (mode.w == w && mode.h == h)
                {
                    _o._GameShell->_gfxModeIndex = i;
                    _o._gfxMode = mode;

                    if (win)
                        _o._GameShell->GFXFlags |= World::GFX_FLAG_WINDOWED;
                    else
                        _o._GameShell->GFXFlags &= ~World::GFX_FLAG_WINDOWED;

                    _o._GameShell->_gfxMode = mode;
                    break;
                }
            }
        }
    }
    else
        return ScriptParser::RESULT_UNKNOWN;
    return ScriptParser::RESULT_OK;
}

int SoundParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    _o._GameShell->savedDataFlags |= World::SDF_SOUND;

    if ( !StriCmp(p1, "channels") )
    {
    }
    else if ( !StriCmp(p1, "volume") )
    {
        _o._GameShell->soundVolume = parser.stoi(p2);
        SFXEngine::SFXe.setMasterVolume(_o._GameShell->soundVolume);
    }
    else if ( !StriCmp(p1, "cdvolume") )
    {
        _o._GameShell->musicVolume = parser.stoi(p2);
        SFXEngine::SFXe.SetMusicVolume(_o._GameShell->musicVolume);
    }
    else if ( !StriCmp(p1, "invertlr") )
    {
        if ( !StriCmp(p2, "yes") )
        {
            _o._GameShell->soundFlags |= World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(true);
        }
        else
        {
            _o._GameShell->soundFlags &= ~World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(false);
        }
    }
    else if ( !StriCmp(p1, "sound") )
    {
    }
    else if ( !StriCmp(p1, "cdsound") )
    {
        if ( !StriCmp(p2, "yes") )
        {
            _o._GameShell->soundFlags |= World::SF_CDSOUND;
            _o._preferences |= World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(true);
        }
        else
        {
            _o._GameShell->soundFlags &= ~World::SF_CDSOUND;
            _o._preferences &= ~World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(false);
        }
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelStatusParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_levelstatus") )
        return false;

    _levelId = parser.stoi(opt);
    return true;
}

int LevelStatusParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( _setFlag )
        _o._GameShell->savedDataFlags |= World::SDF_SCORE;

    if ( !StriCmp(p1, "status") )
    {
        if ( _o._globalMapRegions.MapRegions[_levelId].Status != TMapRegionInfo::STATUS_NONE )
            _o._globalMapRegions.MapRegions[_levelId].Status = parser.stoi(p2);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool BuddyParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (!StriCmp(word, "begin_buddy"))
    {
        _o._levelInfo.Buddies.emplace_back();
        return true;
    }
    return false;
}

int BuddyParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "commandid") )
    {
        _o._levelInfo.Buddies.back().CommandID = parser.stoi(p2);
    }
    else if ( !StriCmp(p1, "type") )
    {
        _o._levelInfo.Buddies.back().Type = parser.stoi(p2);
    }
    else if ( !StriCmp(p1, "energy") )
    {
        _o._levelInfo.Buddies.back().Energy = parser.stoi(p2);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

void ShellParser::ParseStatus(ScriptParser::Parser &parser, TMFWinStatus *status, const std::string &p2)
{
    Stok stok(p2, " _");
    std::string val;

    if ( stok.GetNext(&val) )
        status->Valid = parser.stoi(val) != 0;

    if ( stok.GetNext(&val) )
        status->IsOpen = parser.stoi(val) != 0;

    if ( stok.GetNext(&val) )
        status->Rect.x = parser.stoi(val);

    if ( stok.GetNext(&val) )
        status->Rect.y = parser.stoi(val);

    if ( stok.GetNext(&val) )
        status->Rect.w = parser.stoi(val);

    if ( stok.GetNext(&val) )
        status->Rect.h = parser.stoi(val);

    for (auto &x : status->Data)
    {
        if ( !stok.GetNext(&val) )
            break;

        x = parser.stoi(val);
    }
}

int ShellParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        _o._shellConfIsParsed = true;
        return ScriptParser::RESULT_SCOPE_END;
    }

    _o._GameShell->savedDataFlags |= World::SDF_SHELL;

    if ( !StriCmp(p1, "LANGUAGE") )
    {
        std::string * deflt = nullptr;
        std::string * slct = nullptr;

        for(std::string &s : _o._GameShell->lang_dlls)
        {
            if ( !StriCmp(s, p2) )
                slct = &s;
            // Retail profiles use LANGUAGE; the supported catalogue is listed as ENGLISH.
            if ( !StriCmp(s, "english") )
                deflt = &s;
        }

        if ( slct )
            _o._GameShell->default_lang_dll = slct;
        else
            _o._GameShell->default_lang_dll = deflt;

        _o._GameShell->prev_lang = _o._GameShell->default_lang_dll;

        if ( !_o.ReloadLanguage() )
        {
            ypa_log_out("Unable to set new language\n");
        }
    }
    else if ( !StriCmp(p1, "SOUND") || !StriCmp(p1, "VIDEO") ||
              !StriCmp(p1, "INPUT") || !StriCmp(p1, "DISK") ||
              !StriCmp(p1, "LOCALE") || !StriCmp(p1, "NET") ||
              !StriCmp(p1, "FINDER") || !StriCmp(p1, "LOG") ||
              !StriCmp(p1, "ENERGY") || !StriCmp(p1, "MESSAGE") ||
              !StriCmp(p1, "MAP") )
    {

    }
    else if ( !StriCmp(p1, "robo_map_status") )
    {
        ParseStatus(parser, &_o._roboMapStatus, p2);
    }
    else if ( !StriCmp(p1, "robo_finder_status") )
    {
        ParseStatus(parser, &_o._roboFinderStatus, p2);
    }
    else if ( !StriCmp(p1, "vhcl_map_status") )
    {
        ParseStatus(parser, &_o._vhclMapStatus, p2);
    }
    else if ( !StriCmp(p1, "vhcl_finder_status") )
    {
        ParseStatus(parser, &_o._vhclFinderStatus, p2);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

}
}
