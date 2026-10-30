// Game shell session: level start, replay playback, loading screen, saved games,
// settings files, input/video/language reload.
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
    _FPS = System::Frames.MeasuredFps();

    _profileVals[PFID_FPS] = _FPS;

    GFX::Engine.BeginFrame();

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

    _GameShell->confRetroCursors = System::IniConf::UiRetroCursors.Get<bool>();
    v16.butID = 1196; // Retro Cursors checkbox
    v16.field_4 = _GameShell->confRetroCursors ? 1 : 2;
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
        new World::Parsers::LevelLocationParser(&_locations, true),
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
        _locationEntry=World::LocationEntryState();
        _locationEntry.suppressInitial=true;
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

bool NC_STACK_ypaworld::RestoreCompletedMissionHostStart(const std::vector<MapRobo> &robos)
{
    auto *host = dynamic_cast<NC_STACK_yparobo *>(_userRobo);
    if ( !host )
        return false;

    const Common::Point cell = World::PositionToSectorID(host->_position);
    const bool onGate = std::any_of(_levelInfo.Gates.begin(), _levelInfo.Gates.end(),
        [&](const TMapGate &gate) { return gate.CellId == cell; });
    if ( !onGate )
        return false;

    for ( const MapRobo &start : robos )
    {
        if ( start.Owner != host->_owner )
            continue;

        // Use the same authored, terrain-relative position as a fresh mission.
        // The completed territory and its units remain in the final-save state.
        ypaworld_arg136 ground;
        ground.stPos = start.Pos.X0Z() - vec3d::OY(30000.0);
        ground.vect = vec3d::OY(50000.0);
        ground.flags = 0;
        ypaworld_func136(&ground);

        bact_arg80 placement;
        placement.pos = start.Pos;
        if ( ground.isect )
            placement.pos.y += ground.isectPos.y;
        placement.field_C = 0;
        if ( !host->SetPosition(&placement) )
            return false;

        host->_old_pos = host->_position;
        host->_beam_time = 0;
        host->_fly_dir_length = 0;
        host->_thraction = 0;
        host->_roboState &= ~NC_STACK_yparobo::ROBOSTATE_MOVE;
        return true;
    }
    return false;
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
    {
        RestoreCompletedMissionHostStart(mapp.Robos);
        InitBuddies();
    }

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

    WriteLocations(fil);

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
