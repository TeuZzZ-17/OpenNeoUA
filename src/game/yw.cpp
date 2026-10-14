// ypaworld core: construction, Init/Deinit, the per-frame Process, level teardown, sector access and the ypaworld_funcNNN
// entry points. The other areas live next to it:
//   yw_shell.cpp ........ game shell (menus), settings, input bindings, save/load entry points
//   yw_sprint.cpp ....... player sprint
//   yw_gem.cpp .......... gem notifications and their sounds
//   yw_plasma.cpp ....... plasma currency
//   yw_ambient.cpp ...... ambient level sound and atmospheric FX
//   yw_protos.cpp ....... prototype scripts, super item profiles and display names
//   yw_attached_fx.cpp .. attached FX volumes and sampling
// Shared helpers are declared in yw_impl.h.
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

int vertMenuSpace;
int dword_5A50B2;
int dword_5A50B2_h;
int word_5A50AE;
int word_5A50BC;
int word_5A50BA;
int word_5A50BE;
int buttonsSpace;
int checkBoxWidth;
int dword_5A50B6;
int dword_5A50B6_h;
int scaledFontHeight;
int bottomButtonsY;
int button1LineWidth;
int bottomCenteredFirstBtnPosX;
int bottomCenteredSecondBtnPosX;
int bottomSecondBtnPosX;
int bottomThirdBtnPosX;
GuiList stru_5C91D0;
uint32_t bact_id = 0x10000;
uint32_t dword_5A7A80;
int PrevMouseX = 0;
int PrevMouseY = 0;

// OpenNeoUA: when a Vehicle does not author snd_genesis_pitch, derive a fixed
// Genesis pitch from its energy. The calibration follows the supplied reference
// points exactly and stays monotonic between and beyond them.
int yw_GetAutomaticGenesisPitch(int energy)
{
    struct GenesisPitchPoint
    {
        int energy;
        int pitch;
    };

    static constexpr GenesisPitchPoint points[] = {
        {  9500,  -100 },
        { 18000,  -800 },
        { 28000, -2000 },
    };

    const GenesisPitchPoint *low = &points[0];
    const GenesisPitchPoint *high = &points[1];

    if ( energy > points[1].energy )
    {
        low = &points[1];
        high = &points[2];
    }

    const double span = (double)(high->energy - low->energy);
    const double t = ((double)energy - (double)low->energy) / span;
    const double pitch = (double)low->pitch +
                         t * (double)(high->pitch - low->pitch);

    return (int)std::lround(pitch);
}

float yw_ReadGameplayTimeScale(Common::Ini::Key &key, float fallback)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return fallback;

    try
    {
        size_t pos = 0;
        const float scale = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !isfinite(scale) || scale <= 0.0f )
            return fallback;

        return std::max(GAMEPLAY_MIN_TIME_SCALE, std::min(scale, 1.0f));
    }
    catch (...)
    {
        return fallback;
    }
}

uint32_t yw_ReadNonNegativeDuration(Common::Ini::Key &key,
                                           uint32_t fallback)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return fallback;

    try
    {
        size_t pos = 0;
        const long long duration = std::stoll(value, &pos, 0);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             duration < 0 )
            return fallback;

        return (uint32_t)std::min<long long>(duration,
                                             GAMEPLAY_TIME_SCALE_MAX_DURATION_MS);
    }
    catch (...)
    {
        return fallback;
    }
}

float yw_ReadNonNegativeDistance(Common::Ini::Key &key, float fallback)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return fallback;

    try
    {
        size_t pos = 0;
        const float distance = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !isfinite(distance) || distance < 0.0f )
            return fallback;

        return std::min(distance, ROBO_DEATH_TIME_SCALE_MAX_DISTANCE_LIMIT);
    }
    catch (...)
    {
        return fallback;
    }
}

TimedGameplayScaleProfile yw_GetRoboDeathScaleProfile()
{
    TimedGameplayScaleProfile profile;
    profile.scale = yw_ReadGameplayTimeScale(
        System::IniConf::GameRoboDeathTimeScale, 1.0f);
    profile.durationMs = yw_ReadNonNegativeDuration(
        System::IniConf::GameRoboDeathTimeScaleDuration, 0);
    return profile;
}

bool yw_IsKamikazeFireHeld(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    return yw && inpt &&
           (inpt->Buttons.Is(0) ||
            (yw->_mouseGrabbed &&
             World::IsFixedInputShortcutHeld(World::INPUT_BIND_FIRE)));
}

void yw_UpdateKamikazeFireLatch(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( !yw )
        return;

    if ( yw->_isNetGame || !inpt )
    {
        yw->_kamikazeFireInputWasHeld = false;
        yw->_kamikazeFireTimeScaleDrainGid = 0;
        yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
        return;
    }

    const bool fireHeld = yw_IsKamikazeFireHeld(yw, inpt);
    const bool firePressed = fireHeld && !yw->_kamikazeFireInputWasHeld;
    yw->_kamikazeFireInputWasHeld = fireHeld;

    if ( yw->_kamikazeFireTimeScaleDrainGid > 0 )
    {
        NC_STACK_ypabact *latchedUnit =
            yw->FindLiveBactByGid(yw->_kamikazeFireTimeScaleDrainGid);
        float scale = 1.0f;
        float drain = 0.0f;
        if ( latchedUnit && latchedUnit->GetKamikazeFireTimeScale(&scale, &drain) )
            return;

        yw->_kamikazeFireTimeScaleDrainGid = 0;
        yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
    }

    if ( !firePressed || !yw->_userUnit || !yw->_userUnit->getBACT_inputting() )
        return;

    float scale = 1.0f;
    float drain = 0.0f;
    if ( !yw->_userUnit->GetKamikazeFireTimeScale(&scale, &drain) )
        return;

    // One valid player FIRE press permanently arms this runtime sequence. The
    // same input is intentionally left untouched so any normally fireable
    // Weapon/MGUN can still react to the click in its existing User_layer.
    yw->_kamikazeFireTimeScaleDrainGid = yw->_userUnit->_gid;
    yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
}

bool yw_GetActiveKamikazeFireTimeScale(NC_STACK_ypaworld *yw,
                                               float *outScale,
                                               float *outHpDrainPerSecond)
{
    if ( outScale )
        *outScale = 1.0f;
    if ( outHpDrainPerSecond )
        *outHpDrainPerSecond = 0.0f;

    if ( !yw || yw->_isNetGame || yw->_kamikazeFireTimeScaleDrainGid <= 0 )
        return false;

    NC_STACK_ypabact *unit = yw->FindLiveBactByGid(yw->_kamikazeFireTimeScaleDrainGid);
    return unit && unit->GetKamikazeFireTimeScale(outScale, outHpDrainPerSecond);
}

float yw_GetActiveGameplayTimeScale(NC_STACK_ypaworld *yw)
{
    if ( !yw || yw->_isNetGame )
        return 1.0f;

    float scale = 1.0f;
    if ( yw->HasActiveRoboDeathTimeScale() )
        scale = std::min(scale, yw_GetRoboDeathScaleProfile().scale);

    float fireScale = 1.0f;
    if ( yw_GetActiveKamikazeFireTimeScale(yw, &fireScale, nullptr) )
        scale = std::min(scale, fireScale);

    // Debug acceleration uses the same clock. Authored slow-motion effects
    // still take priority when they are active.
    if ( yw->IsDebugGameplayFastMotionEnabled() && scale >= 1.0f )
        scale = DEBUG_GAMEPLAY_FAST_TIME_SCALE;
    if ( yw->IsDebugGameplaySlowMotionEnabled() )
        scale = std::min(scale, DEBUG_GAMEPLAY_TIME_SCALE);

    return scale;
}

void yw_ApplyDebugHostStationResources(NC_STACK_ypaworld *yw)
{
    if ( !yw || !yw->IsDebugHostStationCheatEnabled() )
        return;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->getYW_userHostStation());
    if ( !robo || robo->_energy_max <= 0 )
        return;

    // These are the three player Host Station HUD batteries: main Energy,
    // Genesis/Creation and Move/Teleport. Keep them at their canonical max;
    // no shadow resource pool is introduced for the debug cheat.
    robo->_energy = robo->_energy_max;
    robo->setROBO_battVehicle(robo->_energy_max);
    robo->setROBO_battBeam(robo->_energy_max);
}

void yw_UpdateKamikazeFireTimeScaleHpDrain(NC_STACK_ypaworld *yw,
                                                   int32_t frameTime)
{
    if ( !yw )
        return;

    float scale = 1.0f;
    float drainPerSecond = 0.0f;
    if ( !yw_GetActiveKamikazeFireTimeScale(yw, &scale, &drainPerSecond) ||
         drainPerSecond <= 0.0f )
    {
        yw->_kamikazeFireTimeScaleDrainGid = 0;
        yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
        return;
    }

    // HP drain belongs to the same master game clock as the Kamikaze motion
    // and FX. F4 slows it and F6 pauses it; releasing FIRE still has no effect
    // after activation.
    if ( frameTime <= 0 )
        return;

    NC_STACK_ypabact *unit = yw->FindLiveBactByGid(yw->_kamikazeFireTimeScaleDrainGid);
    if ( !unit || unit->_energy <= 0 || unit->_energy_max <= 0 )
    {
        yw->_kamikazeFireTimeScaleDrainGid = 0;
        yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
        return;
    }

    const double rawDrain = (double)drainPerSecond *
                            ((double)frameTime / 1000.0);
    yw->_kamikazeFireTimeScaleHpDrainRemainder += rawDrain;

    const int energyDrain = (int)yw->_kamikazeFireTimeScaleHpDrainRemainder;
    if ( energyDrain <= 0 )
        return;

    yw->_kamikazeFireTimeScaleHpDrainRemainder -= energyDrain;

    if ( energyDrain >= unit->_energy )
    {
        // Detonate while the carrier is still alive so the shared Kamikaze
        // payload path runs before the generic zero-energy death lifecycle.
        if ( unit->TriggerKamikazeDetonation(nullptr) )
        {
            yw->_kamikazeFireTimeScaleDrainGid = 0;
            yw->_kamikazeFireTimeScaleHpDrainRemainder = 0.0;
        }
        return;
    }

    unit->_energy -= energyDrain;
}

int yw_SelectMimicVehicleID(const std::vector<World::TVhclProto> &protos, int shellVehicleId)
{
    if ( shellVehicleId <= 0 || (size_t)shellVehicleId >= protos.size() )
        return 0;

    const World::TVhclProto &shell = protos[shellVehicleId];
    if ( !shell.is_mimic )
        return 0;

    // OpenNeoUA: Mimic candidates are derived from the current level's canonical
    // vehicle availability masks, removing the need for a manually authored list.
    // A Mimic can therefore only disguise itself as a real vehicle
    // enabled somewhere in this level. Host Stations, Guns/modules and other
    // Mimics are shells/technical units, so they are never disguise candidates.
    std::vector<int16_t> validIds;
    validIds.reserve(protos.size());

    for (size_t vehicleId = 1; vehicleId < protos.size(); ++vehicleId)
    {
        if ((int)vehicleId == shellVehicleId)
            continue;

        const World::TVhclProto &candidate = protos[vehicleId];
        if ( candidate.disable_enable_bitmask == 0 ||
             candidate.is_mimic ||
             candidate.model_id == BACT_TYPES_NOPE ||
             candidate.model_id == BACT_TYPES_ROBO ||
             candidate.model_id == BACT_TYPES_GUN )
        {
            continue;
        }

        validIds.push_back((int16_t)vehicleId);
    }

    if ( validIds.empty() )
        return 0;

    size_t randomIndex = (size_t)(rand() % (int)validIds.size());
    return validIds[randomIndex];
}

int32_t yw_ClampRenderSectors(int32_t sectors)
{
    if (sectors < 3)
        sectors = 3;
    if (sectors > YW_RENDER_SECTORS_MAX)
        sectors = YW_RENDER_SECTORS_MAX;
    // The render window is centered on the viewer and must have an odd side
    // count. Even values are rounded down to the nearest valid window.
    if ((sectors & 1) == 0)
        sectors -= 1;

    return sectors;
}

int32_t yw_RenderSectorsToNormalVizLimit(int32_t sectors)
{
    const int32_t normalized = yw_ClampRenderSectors(sectors);

    // _renderSectors is an internal centered sector-window width derived from
    // Sky Horizon Distance. Keep a conservative camera-space visibility budget
    // so outer safety sectors are prepared correctly even with camera offsets,
    // tall assets and large bounds.
    const int32_t requested =
        (int32_t)((normalized + 1) * World::CVSectorLength);
    const int32_t safeMaximum =
        (int32_t)GFX::WORLD_FAR_CLIP - (int32_t)World::CVSectorLength;

    return std::min(requested, safeMaximum);
}

void yw_ApplyNucleusViewDistanceOverrides(NC_STACK_ypaworld *yw)
{
    int32_t v = 0;
    bool b = false;

    if (yw_ParseOptionalInt(System::IniConf::GfxSkyHeight.Get<std::string>(), &v))
        yw->_skyHeight = v;
    if (yw_ParseOptionalBool(System::IniConf::GfxSkyRender.Get<std::string>(), &b))
        yw->_skyRender = b;

    // Sky Horizon Distance is the single public draw-distance control.
    yw->UpdateSkyHorizonRenderSectors();

    yw->SetHideMapBorderWalls(System::IniConf::GfxHideMapBorderWalls.Get<bool>());
}

void NC_STACK_ypaworld::ApplyNucleusViewDistanceOverrides()
{
    yw_ApplyNucleusViewDistanceOverrides(this);
}

NC_STACK_ypaworld::NC_STACK_ypaworld()
: _unitsList(this, NC_STACK_ypabact::GetKidRefNode, World::BLIST_UNITS)
, _deadCacheList(this, NC_STACK_ypabact::GetKidRefNode, World::BLIST_CACHE)
, _history(4096)
{
}

namespace World
{
int AssignParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
	if ( !StriCmp(p1, "end") )
	{
		return ScriptParser::RESULT_SCOPE_END;
	}
	else if ( !p1.empty() && !p2.empty() )
	{
		Common::Env.SetPrefix(p1, p2);
		ypa_log_out("parsing assign.txt: set assign %s to %s\n", p1.c_str(), p2.c_str());
		return ScriptParser::RESULT_OK;
	}
	else
	{
		return ScriptParser::RESULT_BAD_DATA;
	}

    return ScriptParser::RESULT_UNKNOWN;
}

bool ParseAssignFile(const std::string &file)
{
	ScriptParser::HandlersList hndls{
		AssignParser::MakeParser()
	};

	return ScriptParser::ParseFile(file, hndls, 0);
}

}

int yw_InitSceneRecorder(NC_STACK_ypaworld *yw)
{
    yw->_replayRecorder = new TGameRecorder();
    return yw->_replayRecorder != nullptr;
}

void yw_setInitScriptLoc(NC_STACK_ypaworld *yw)
{
    bool ok = false;
    FSMgr::FileHandle *fil = nullptr;

    // Optional legacy override. Modern layouts use Data/Scripts/Startup.cfg.
    if ( uaFileExist("env:startup.def") )
        fil = uaOpenFileAlloc("env:startup.def", "r");

    if (fil)
    {
        std::string line;
        if ( fil->ReadLine(&line) )
        {
            size_t en = line.find_first_of("\n;");
            if (en != std::string::npos)
                line.erase(en);

            yw->_initScriptFilePath = line;
            ok = true;
        }


        delete fil;
    }

    if (!ok)
    {
        // Prefer the modern Game Config, but keep original vanilla Data folders usable.
        if ( uaFileExist("data:scripts/Startup.cfg") )
            yw->_initScriptFilePath = "data:scripts/Startup.cfg";
        else
            yw->_initScriptFilePath = "data:scripts/startup.scr";
    }
}

size_t NC_STACK_ypaworld::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
    {
        ypa_log_out("yw_main.c/OM_NEW: _supermethoda() failed!\n");
        return 0;
    }

    Common::Env.SetPrefix("rsrc", "mc2res");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("save", "Save");
    Common::Env.SetPrefix("help", "Help");
    Common::Env.SetPrefix("mov", "Data:mov");
    Common::Env.SetPrefix("levels", "Levels");
    Common::Env.SetPrefix("mbpix", "levels:mbpix");
    Common::Env.SetPrefix("locale", "locale");
    Common::Env.SetPrefix("scripts", "data:scripts");

    if ( !World::ParseAssignFile("env:assign.txt") )
        ypa_log_out("Warning, no env:assign.txt script.\n");

    yw_setInitScriptLoc(this);

    System::IniConf::ReadFromNucleusIni();
    Locale::Text::SetLangDefault();
    if (!ypaworld_func166("language")) // Auto-detect an original vanilla language catalogue.
    {
        ypa_log_out("ERROR: no readable original language DLL was found in Locale/.\n");
        Deinit();
        return 0;
    }


    _vhclModels.clear();
    _attachedFXGeometryCache.clear();
    ClearOverrideModels();

    if ( !ProtosInit() )
    {
        ypa_log_out("ERROR: couldn't initialize prototypes.\n");
        Deinit();
        return 0;
    }


    _screenSize = GFX::Engine.GetScreenSize();

    _unitsList.clear();
    _guiActive.clear();
    _deadCacheList.clear();
    _transientVPs.clear();
    _nextTransientVPId = 1;
    ClearMinigunTracers();
    _mgunTracers.reserve(512);
    _proceduralEnergyFX.reserve(512);
    ClearGroundDecals();
    _damageHoverTargets.clear();
    _fxLimit = 16;
    _renderSectors = yw_ClampRenderSectors(stak.Get<int32_t>(YW_ATT_VISSECTORS, 9));
    _normalVizLimit = stak.Get<int32_t>(YW_ATT_NORMVISLIMIT, 3100);
    _normalFadeLength = stak.Get<int32_t>(YW_ATT_FADELENGTH, 2100);
    _skyVizLimit = stak.Get<int32_t>(YW_ATT_SKYVISLIMIT, 4200);
    _skyFadeLength = stak.Get<int32_t>(YW_ATT_SKYFADELENGTH, 2400);
    _mapSize.x = stak.Get<int32_t>(YW_ATT_MAPMAX_X, 64);
    _mapSize.y = stak.Get<int32_t>(YW_ATT_MAPMAX_Y, 64);
    _skyHeight = stak.Get<int32_t>(YW_ATT_SKYHEIGHT, -550);
    _skyRender = stak.Get<bool>(YW_ATT_SKYRENDER, true);
    ApplyNucleusViewDistanceOverrides();
    _doEnergyRecalc = stak.Get<bool>(YW_ATT_DOENERGYRECALC, true);

    _buildDate = stak.Get<std::string>(YW_ATT_BUILD_DATE, "");

    for (char &chr : _buildDate)
    {
        chr = toupper(chr);
        if ( chr < ' ' || chr > 'Z' )
            chr = '*';
    }



    _buildHealthModelId[0] = 3;
    for (int i = 1; i <= 100; i++ )
        _buildHealthModelId[i] = 2;
    for (int i = 101; i <= 200; i++ )
        _buildHealthModelId[i] = 1;
    for (int i = 201; i < 256; i++ )
        _buildHealthModelId[i] = 0;

    for (int j = 0; j < 64; j++)
    {
        for (int i = 0; i < 64; i++)
        {
            _sqrtTable(j,i) = sqrt(j * j + i * i);
        }
    }

    _cells.Clear();

    if ( !yw_InitSceneRecorder(this) )
    {
        ypa_log_out("yw_main.c/OM_NEW: init scene recorder failed!\n");
        Deinit();
        return 0;
    }

    _shellGfxMode = Common::Point( GFX::DEFAULT_WIDTH, GFX::DEFAULT_HEIGHT );
    _gfxMode = Common::Point( GFX::DEFAULT_WIDTH, GFX::DEFAULT_HEIGHT );



    _shellDefaultRes = Common::Point(640, 480);
    _gameDefaultRes = Common::Point(640, 480);

    if ( !InitMapRegionsNet() )
    {
        ypa_log_out("yw_main.c/OM_NEW: yw_InitLevelNet() failed!\n");
        Deinit();
        return 0;
    }

    if ( !yw_InitNetwork(this) )
    {
        ypa_log_out("yw_main.c/OM_NEW: yw_InitNetwork() failed!\n");
        Deinit();
        return 0;
    }

    _doNotRender = false;

    UpdateGuiSettings();

    return 1;
}

size_t NC_STACK_ypaworld::Deinit()
{
    StopAmbientLevelSound();
    StopAtmosphericFXLoopSound();
    ClearAtmosphericFXRuntime();
    ClearMobilePowerFXRuntime();
    _atmosphericFXProfile = World::TAtmosphericFXProfile();
    ClearSuperItemRuntime();
    for (World::TSuperItemProfile &profile : _superItemProfiles)
    {
        profile.debuff.tick_snd.ClearSounds();
        profile.detonate_snd.ClearSounds();
        profile.wave_snd.ClearSounds();
    }
    _superItemProfiles.clear();
    _buffProfiles.clear();
    _debuffProfiles.clear();
    _debugBlastRings.clear();
    ClearMinigunTracers();
    ClearProceduralEnergyFX();
    ClearGroundDecals();
    // Transient instances may reference BASE objects owned by either external
    // visual cache. Destroy the instances before releasing those caches.
    _transientVPs.clear();
    _nextTransientVPId = 1;
    ClearSharedExternalBases();
    ClearSharedExternalMeshes();
    FreeGameDataCursors();
    dprintf("MAKE ME %s\n","ypaworld_func1");
    return NC_STACK_nucleus::Deinit();
}

void sub_445230(NC_STACK_ypaworld *yw)
{
    GFX::Engine.setViewZoom(1.0f);

    if ( yw->UpdateSpectatorFollowCamera(nullptr) )
        return;

    if ( yw->_viewerBact->getBACT_extraViewer() )
    {
        NC_STACK_ypabact *v4 = yw->_viewerBact;

        if ( v4->IsCockpitCameraActive() )
            yw->_viewerPosition = v4->GetCockpitCameraViewPosition();
        else
            yw->_viewerPosition = v4->_position + v4->_rotation.Transpose().Transform(v4->_viewer_position);

        yw->_viewerRotation = yw->_viewerBact->_viewer_rotation;
    }
    else
    {
        if ( yw->_viewerBact->IsCockpitCameraActive() )
            yw->_viewerPosition = yw->_viewerBact->GetCockpitCameraViewPosition();
        else
            yw->_viewerPosition = yw->_viewerBact->_position;

        yw->_viewerRotation = yw->_viewerBact->_rotation;
    }

    if ( yw->_viewerBact == yw->_userUnit && yw->_userUnit &&
         yw->_userUnit->IsAlternativeViewActive() )
    {
        yw->_viewerRotation = yw->_userUnit->GetAlternativeViewRotation();
    }

    if (yw->_viewerBact == yw->_userUnit && yw->_userUnit
            && yw->_userUnit->_bact_type == BACT_TYPES_UFO
            && yw->_userUnit->_status != BACT_STATUS_DEAD
            && !(yw->_userUnit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2))
            && yw->_userUnit->getBACT_inputting())
    {
        GFX::Engine.setViewZoom(yw->_userUnit->GetPlayerViewZoom());
    }
}

float NC_STACK_ypaworld::GetUfoSpyUiRadius() const
{
    if ( !_userUnit || _userUnit->_bact_type != BACT_TYPES_UFO )
        return 0.0f;

    if ( _userUnit->_status == BACT_STATUS_DEAD ||
         (_userUnit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
        return 0.0f;

    const size_t vehicleId = _userUnit->_vehicleID;
    if ( vehicleId >= _vhclProtos.size() )
        return 0.0f;

    const World::TVhclProto &proto = _vhclProtos[vehicleId];
    if ( proto.model_id != BACT_TYPES_UFO || proto.spy_ui_radius <= 0.0f )
        return 0.0f;

    return proto.spy_ui_radius;
}

bool NC_STACK_ypaworld::IsUfoSpyUiControlContext() const
{
    return _GameShell &&
           _userRobo &&
           _userUnit &&
           _viewerBact == _userUnit &&
           !IsRoboMapOpen() &&
           _userUnit->_bact_type == BACT_TYPES_UFO &&
           _userUnit->getBACT_inputting() &&
           GetUfoSpyUiRadius() > 0.0f;
}

void yw_UpdateUfoSpyUiToggle(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( !yw || !inpt )
        return;

    const UserData::TInputConf &bind =
        yw->_GameShell->InputConfig[World::INPUT_BIND_TOGGLE_UFO_SPY_UI];
    const bool primaryDown =
        bind.PKeyCode > Input::KC_NONE && bind.PKeyCode < Input::KC_MAX &&
        Input::Engine.GetKeyState(bind.PKeyCode);
    const bool primaryPressed = primaryDown && !yw->_ufoSpyBindingIsDown;
    yw->_ufoSpyBindingIsDown = primaryDown;

    if ( !yw->IsUfoSpyUiControlContext() )
        return;

    const bool fixedShortcutPressed =
        yw->_mouseGrabbed &&
        World::IsFixedInputShortcutPressed(inpt, World::INPUT_BIND_TOGGLE_UFO_SPY_UI);

    if ( !primaryPressed && !fixedShortcutPressed )
        return;

    yw->_ufoSpyUiEnabled = !yw->_ufoSpyUiEnabled;

    // The optional authored sound is shared by ON and OFF so one parameter
    // describes the complete toggle interaction. Empty/missing = silent.
    const size_t soundId = World::TVhclProto::SND_SPY_UI;
    if ( yw->_userUnit && yw->_userUnit->_soundcarrier.Sounds.size() > soundId )
    {
        TSoundSource &sound = yw->_userUnit->_soundcarrier.Sounds[soundId];
        if ( sound.PSample )
            SFXEngine::SFXe.startSound(&yw->_userUnit->_soundcarrier, soundId);
    }

    if ( primaryPressed )
        inpt->HotKeyID = -1;

    if ( fixedShortcutPressed )
    {
        // The UFO owns MMB while its Spy UI control is available. Consume the
        // physical edge so map/UI handlers later in the frame cannot observe
        // the same click. The toggle controls only the detailed HP/status layer;
        // lightweight faction arrows remain part of the default UFO sensor view.
        inpt->ClickInf.flag &= ~TClickBoxInf::FLAG_MM_DOWN;
    }
}

// OpenNeoUA: cockpit camera vehicle silhouette toggle. The binding is polled
// directly like Sprint/UFO Spy Mode, so it never claims a global HotKeyID slot
// and cannot fire a second hotkey through the shared input expression system.
void yw_UpdateHideVehicleToggle(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( !yw || !inpt || !yw->_GameShell )
        return;

    const UserData::TInputConf &bind =
        yw->_GameShell->InputConfig[World::INPUT_BIND_HIDE_VEHICLE];
    const bool keyDown =
        bind.Type == World::INPUT_BIND_TYPE_HOTKEY &&
        bind.PKeyCode > Input::KC_NONE && bind.PKeyCode < Input::KC_MAX &&
        Input::Engine.GetKeyState(bind.PKeyCode);
    const bool keyPressed = keyDown && !yw->_hideVehicleBindingIsDown;
    yw->_hideVehicleBindingIsDown = keyDown;

    if ( !keyPressed )
        return;

    // The silhouette is only drawn in cockpit camera view, so the toggle is only
    // meaningful there. Anywhere else the key stays inert instead of silently
    // changing a state the player cannot see.
    if ( !yw->_userUnit || !yw->_userUnit->IsCockpitCameraActive() )
        return;

    // Flip the live copy on the unit the player is flying only. The authored
    // Vehicles.cfg value is untouched, so a restart restores it, and taking a
    // new vehicle starts again from that vehicle's own setting.
    yw->_userUnit->_cockpit_camera_hide_vehicle = !yw->_userUnit->_cockpit_camera_hide_vehicle;
}

bool NC_STACK_ypaworld::DebugSwitchToFactionUnit(NC_STACK_ypabact *target)
{
    if ( _isNetGame || !System::IniConf::IsGameNewDebugEnabled() ||
         IsSpectatorControlled() || !target || !_userRobo || !_userUnit ||
         target->_owner == World::OWNER_0 ||
         target->_owner == _userRobo->_owner ||
         target->_energy <= 0 ||
         (target->_status != BACT_STATUS_NORMAL && target->_status != BACT_STATUS_IDLE) ||
         (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) ||
         target->IsArtilleryShellPlatform() )
        return false;

    NC_STACK_yparobo *newHost = target->_bact_type == BACT_TYPES_ROBO ?
        dynamic_cast<NC_STACK_yparobo *>(target) : target->_host_station;
    if ( !newHost )
    {
        // Some scripted vehicles are unassigned but their faction still has
        // a Host Station. Reuse that station rather than fabricating a new one.
        for ( NC_STACK_ypabact *unit : _unitsList )
        {
            if ( unit->_owner == target->_owner && unit->_bact_type == BACT_TYPES_ROBO )
            {
                newHost = dynamic_cast<NC_STACK_yparobo *>(unit);
                if ( newHost && newHost->_energy > 0 &&
                     (newHost->_status == BACT_STATUS_NORMAL || newHost->_status == BACT_STATUS_IDLE) )
                    break;
            }
            newHost = nullptr;
        }
    }
    if ( !newHost || newHost->_owner != target->_owner ||
         newHost->_energy <= 0 ||
         (newHost->_status != BACT_STATUS_NORMAL && newHost->_status != BACT_STATUS_IDLE) ||
         (newHost->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) )
        return false;

    // The enemy must own a real Host Station: reusing that station also changes
    // the strategic commands, squad list and owner-dependent UI to this faction.
    NC_STACK_yparobo *oldHost = dynamic_cast<NC_STACK_yparobo *>(_userRobo);
    if ( _viewerBact )
    {
        _viewerBact->setBACT_viewer(false);
        _viewerBact->setBACT_inputting(false);
    }
    if ( _userUnit && _userUnit != _viewerBact )
        _userUnit->setBACT_inputting(false);
    if ( oldHost )
    {
        oldHost->setBACT_inputting(false);
        oldHost->_roboState &= ~NC_STACK_yparobo::ROBOSTATE_PLAYERROBO;
    }

    newHost->setBACT_inputting(true); // Sets the authoritative user Host and owner.
    target->setBACT_viewer(true);
    if ( target != newHost )
    {
        newHost->setBACT_inputting(false);
        target->setBACT_inputting(true);
    }

    _extraViewEnable = false;
    _extraViewNumber = -1;
    _playerInHSGun = false;
    _activeCmdrID = 0;
    _cmdrIdToSelect = -1;
    _activeCmdrRemapIndex = -1;
    _lastMsgSender = nullptr;
    _bactOnMouse = nullptr;
    _guiActFlags &= ~0x20;
    ResetPlayerSprint();
    // Close any stale squad/Genesis selection belonging to the previous side.
    extern GuiList gui_lstvw;
    extern GuiList lstvw2;
    GuiWinClose(&gui_lstvw);
    GuiWinClose(&lstvw2);
    sub_4C40AC();
    return _userUnit == target && _userRobo == newHost;
}

void NC_STACK_ypaworld::HandleDebugTimeHotkeys(TInputState *inpt, bool openUADebug)
{
    if ( !inpt || !openUADebug || _isNetGame )
        return;

    // All three time controls run before the first gameplay update.
    // In netplay the original hotkeys remain available unchanged.
    const int16_t key = inpt->KbdLastHit;
    if ( key != Input::KC_F2 && key != Input::KC_F3 && key != Input::KC_F4 )
        return;

    inpt->HotKeyID = -1;
    const char *message = nullptr;
    if ( key == Input::KC_F2 )
    {
        _debugGameplaySlowMotion = !_debugGameplaySlowMotion;
        if ( _debugGameplaySlowMotion )
            _debugGameplayFastMotion = false;
        message = _debugGameplaySlowMotion ? "Slow Motion 20%" : "Slow Motion OFF";
    }
    else if ( key == Input::KC_F3 )
    {
        _debugGameplayFastMotion = !_debugGameplayFastMotion;
        if ( _debugGameplayFastMotion )
            _debugGameplaySlowMotion = false;
        message = _debugGameplayFastMotion ? "Fast Motion 200%" : "Fast Motion OFF";
    }
    else
    {
        _debugGameplayFrozen = !_debugGameplayFrozen;
        message = _debugGameplayFrozen ? "Game Time Frozen" : "Game Time Resumed";
    }

    yw_arg159 infoMsg;
    infoMsg.txt = message;
    infoMsg.unit = nullptr;
    infoMsg.Priority = 100;
    infoMsg.MsgID = 0;
    ypaworld_func159(&infoMsg);
}

size_t NC_STACK_ypaworld::Process(base_64arg *arg)
{
    CrashDiag::SetPhase("WorldPreprocess");
    extern GuiList gui_lstvw; //In yw_game_ui.cpp
    extern GuiList lstvw2; //In yw_game_ui.cpp
    extern bool SPEED_DOWN_NET; //In yw_net.cpp

    if ( (gui_lstvw.IsClosed() && lstvw2.IsClosed())
            || (arg->field_8->KbdLastHit != Input::KC_RETURN && arg->field_8->KbdLastHit != Input::KC_ESCAPE) )
    {
        _kbdLastKeyHit = Input::KC_NONE;
    }
    else
    {
        _kbdLastKeyHit = arg->field_8->KbdLastHit;
        arg->field_8->KbdLastHit = Input::KC_NONE;
        arg->field_8->KbdLastDown = Input::KC_NONE;
        arg->field_8->HotKeyID = -1;
    }

    if ( !ypaworld_func64__sub4(this, arg) )
    {
        uint32_t v92 = profiler_begin();

        _netChatSystem = false;

        if ( _screenShotSeq )
        {
            arg->TimeStamp -= arg->DTime;
            arg->field_8->Period = 40;
            arg->DTime = 40;
            arg->TimeStamp += arg->DTime;
        }

        bool openUADebug = System::IniConf::IsGameNewDebugEnabled();
        if ( !openUADebug )
        {
            _debugGameplaySlowMotion = false;
            _debugGameplayFastMotion = false;
            _debugGameplayFrozen = false;
            _debugHostStationCheat = false;
            _debugGlobalInvulnerability = false;
            _debugDpsEnabled = false;
            DebugResetDpsMeter();
        }

        HandleDebugTimeHotkeys(arg->field_8, openUADebug);

        const int32_t unscaledFrameTime = arg->DTime;
        const int32_t realFrameTimeStamp = arg->TimeStamp;

        yw_UpdateKamikazeFireLatch(this, arg->field_8);
        const float gameplayTimeScale = yw_GetActiveGameplayTimeScale(this);
        const bool gameplayFrozen = !_isNetGame && openUADebug && _debugGameplayFrozen;
        const bool forceAllTimeScaled = !_isNetGame && openUADebug &&
                                        (_debugGameplaySlowMotion || _debugGameplayFastMotion || _debugGameplayFrozen);

        // One master clock now owns every local in-game time consumer.  From
        // this point on, DTime, input Period and TimeStamp are the virtual game
        // domain; platform/network time stays outside this scope.
        arg->DTime = System::GameClock.BeginFrame(realFrameTimeStamp, unscaledFrameTime,
                                                   _timeStamp, gameplayTimeScale,
                                                   gameplayFrozen, forceAllTimeScaled);
        _timeStamp = System::GameClock.Time();
        arg->TimeStamp = System::GameClock.VisualTime();
        if ( arg->field_8 )
            arg->field_8->Period = arg->DTime;

        Gui::Root::Instance.TimersUpdate(arg->DTime);

        // Audio pitch and sound-effect clocks consume the same master time.
        // Debug F2/F3/F4 also override per-source opt-outs so nothing audible can
        // silently continue at real speed while the world is globally dilated.
        SFXEngine::SFXe.SetTimeScale(gameplayFrozen ? 0.0f : gameplayTimeScale,
                                     forceAllTimeScaled);
        yw_UpdateKamikazeFireTimeScaleHpDrain(this, arg->DTime);

        if ( _userUnit )
        {
            if ( _framesElapsed == 1 )
            {
                _viewerPosition = _userUnit->_position;
                _viewerRotation = _userUnit->_rotation;
            }

            vec3d a3 = _userUnit->_fly_dir * _userUnit->_fly_dir_length;

            SFXEngine::SFXe.sub_423EFC(arg->DTime, _viewerPosition, a3, _viewerRotation);
        }

        if ( _framesElapsed == 1 )
        {
            yw_arg159 arg159;
            arg159.unit = _userRobo;
            arg159.Priority = 128;
            arg159.MsgID = 41;

            ypaworld_func159(&arg159);
        }

        ypaworld_func64__sub6(this);
        ypaworld_func64__sub2(this);

        if ( !_doNotRender )
        {
            ypaworld_func64__sub1(arg->field_8); //Precompute input (add mouse turn)

            if ( HasActiveNewGemNotification() && !yw_IsPriorityGameplayUiOpen() )
            {
                const bool escRequested =
                    _kbdLastKeyHit == Input::KC_ESCAPE ||
                    arg->field_8->KbdLastHit == Input::KC_ESCAPE;
                const bool fireRequested =
                    arg->field_8->Buttons.IsAny({0, 2, 5});

                if ( escRequested || fireRequested )
                {
                    DismissNewGemNotification();

                    // FIRE is an action, not a physical key, so this works with
                    // the player's current bindings. Consume it in the same frame
                    // so dismissing the popup never also fires a weapon.
                    if ( fireRequested )
                        arg->field_8->Buttons.UnSet({0, 2, 5});

                    if ( escRequested )
                    {
                        // ESC belongs exclusively to the GEM popup while it is
                        // active. Do not let the original menu/window handlers
                        // observe the same key in this frame.
                        _kbdLastKeyHit = Input::KC_NONE;
                        arg->field_8->KbdLastHit = Input::KC_NONE;
                        arg->field_8->KbdLastDown = Input::KC_NONE;
                        arg->field_8->HotKeyID = -1;
                    }
                }
            }

            yw_UpdateUfoSpyUiToggle(this, arg->field_8);
            yw_UpdateHideVehicleToggle(this, arg->field_8);

            TClickBoxInf *winp = &arg->field_8->ClickInf;

            if ( _mouseCursorHidden )
            {
                if ( winp->move.ScreenPos != _mouseCursorHidePos )
                    _mouseCursorHidden = false;
            }
            else
            {
                if ( arg->field_8->KbdLastHit != Input::KC_NONE )
                {
                    if ( arg->field_8->KbdLastHit != Input::KC_LMB &&
                         arg->field_8->KbdLastHit != Input::KC_RMB &&
                         arg->field_8->KbdLastHit != Input::KC_MMB &&
                         arg->field_8->KbdLastHit != Input::KC_CTRL &&
                         !(arg->field_8->Buttons.Is(4)) )
                    {
                        _mouseCursorHidden = true;
                        _mouseCursorHidePos = winp->move.ScreenPos;
                    }
                }
            }
        }

        if ( openUADebug && arg->field_8 )
        {
            // Overlay toggles run during drawing; consume their bindings before gameplay UI.
            if ( arg->field_8->KbdLastHit == Input::KC_F9 ||
                 arg->field_8->KbdLastHit == Input::KC_F10 )
                arg->field_8->HotKeyID = -1;

            // F1: rolling DPS meter for the currently controlled player unit.
            // Consume the binding only while game.new.debug is enabled.
            if ( arg->field_8->KbdLastHit == Input::KC_F1 )
            {
                arg->field_8->HotKeyID = -1;

                if ( _isNetGame )
                {
                    _debugDpsEnabled = false;
                    DebugResetDpsMeter();

                    yw_arg159 infoMsg;
                    infoMsg.txt = "DPS Debug unavailable in netgame";
                    infoMsg.unit = nullptr;
                    infoMsg.Priority = 100;
                    infoMsg.MsgID = 0;
                    ypaworld_func159(&infoMsg);
                }
                else
                {
                    _debugDpsEnabled = !_debugDpsEnabled;
                    DebugResetDpsMeter();
                    DebugUpdateDpsSource();

                    yw_arg159 infoMsg;
                    infoMsg.txt = _debugDpsEnabled ? "DPS Debug ON" : "DPS Debug OFF";
                    infoMsg.unit = nullptr;
                    infoMsg.Priority = 100;
                    infoMsg.MsgID = 0;
                    ypaworld_func159(&infoMsg);
                }
            }

            // F5 destroys vehicles and guns; F6/F7 only handle vehicles.
            // Attached non-vehicle, non-gun objects resolve to their carrier,
            // matching the existing debug behavior without introducing a
            // second selection path.
            auto isDebugVehicleTarget = [](const NC_STACK_ypabact *unit)
            {
                if ( !unit ||
                     (unit->_status != BACT_STATUS_NORMAL && unit->_status != BACT_STATUS_IDLE) ||
                     unit->_energy <= 0 ||
                     (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) )
                    return false;

                switch ( unit->_bact_type )
                {
                case BACT_TYPES_BACT:
                case BACT_TYPES_TANK:
                case BACT_TYPES_ZEPP:
                case BACT_TYPES_FLYER:
                case BACT_TYPES_UFO:
                case BACT_TYPES_CAR:
                case BACT_TYPES_ROBO:
                    return true;

                default:
                    return false;
                }
            };

            auto resolveDebugSelectedVehicle = [&]() -> NC_STACK_ypabact *
            {
                NC_STACK_ypabact *selectedVehicle = _bactOnMouse;
                if ( selectedVehicle && !isDebugVehicleTarget(selectedVehicle) &&
                     selectedVehicle->_parent && selectedVehicle->_parent != selectedVehicle->_host_station )
                    selectedVehicle = selectedVehicle->_parent;

                return isDebugVehicleTarget(selectedVehicle) ? selectedVehicle : nullptr;
            };

            // F5 target: any live vehicle plus any live gun, of any gun kind.
            // A gun under the mouse is destroyed directly instead of promoting
            // to its carrier, otherwise guns on buildings or vehicles could
            // never be selected on their own.
            auto isDebugDestroyTarget = [&](const NC_STACK_ypabact *unit)
            {
                if ( isDebugVehicleTarget(unit) )
                    return true;

                if ( !unit ||
                     (unit->_status != BACT_STATUS_NORMAL && unit->_status != BACT_STATUS_IDLE) ||
                     unit->_energy <= 0 ||
                     (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) )
                    return false;

                return unit->_bact_type == BACT_TYPES_GUN;
            };

            auto resolveDebugDestroyTarget = [&]() -> NC_STACK_ypabact *
            {
                NC_STACK_ypabact *selectedTarget = _bactOnMouse;
                if ( selectedTarget && !isDebugDestroyTarget(selectedTarget) &&
                     selectedTarget->_parent && selectedTarget->_parent != selectedTarget->_host_station )
                    selectedTarget = selectedTarget->_parent;

                return isDebugDestroyTarget(selectedTarget) ? selectedTarget : nullptr;
            };

            // F6/F7: toggle invulnerability or AI exclusion on an allied vehicle.
            // When the mouse is not currently pointing at another unit, use the
            // vehicle directly controlled by the player instead.
            // Consume the vanilla binding only while New Debug owns the key.
            if ( arg->field_8->KbdLastHit == Input::KC_F6 ||
                 arg->field_8->KbdLastHit == Input::KC_F7 )
            {
                arg->field_8->HotKeyID = -1;
                const bool ignoreAI = arg->field_8->KbdLastHit == Input::KC_F7;

                NC_STACK_ypabact *selectedVehicle = nullptr;
                if ( _guiActFlags & 0x20 )
                    selectedVehicle = resolveDebugSelectedVehicle();
                else if ( isDebugVehicleTarget(_userUnit) )
                    selectedVehicle = _userUnit;

                const bool validAlliedVehicle = selectedVehicle && _userRobo &&
                                                selectedVehicle->_owner == _userRobo->_owner;
                if ( validAlliedVehicle )
                {
                    if ( ignoreAI )
                        selectedVehicle->_debugIgnoredByAI = !selectedVehicle->_debugIgnoredByAI;
                    else
                        selectedVehicle->_invulnerable = !selectedVehicle->_invulnerable;
                }

                yw_arg159 infoMsg;
                infoMsg.txt = validAlliedVehicle ?
                              (ignoreAI ?
                               (selectedVehicle->_debugIgnoredByAI ? "Vehicle Ignored by Enemies" : "Vehicle Visible to Enemies") :
                               (selectedVehicle->_invulnerable ?
                               "Vehicle Invulnerable" :
                               "Vehicle Vulnerable")) :
                              "No Allied Vehicle Selected";
                infoMsg.unit = nullptr;
                infoMsg.Priority = 100;
                infoMsg.MsgID = 0;
                ypaworld_func159(&infoMsg);
            }

            // F8: runtime-only global unit invulnerability. Handle it before
            // simulation so the toggle applies to damage in the same frame.
            if ( arg->field_8->KbdLastHit == Input::KC_F8 )
            {
                _debugGlobalInvulnerability = !_debugGlobalInvulnerability;

                yw_arg159 infoMsg;
                infoMsg.txt = _debugGlobalInvulnerability ?
                              "Global Invulnerability ON" :
                              "Global Invulnerability OFF";
                infoMsg.unit = nullptr;
                infoMsg.Priority = 100;
                infoMsg.MsgID = 0;
                ypaworld_func159(&infoMsg);
            }

            // F5: one-shot destruction of the selected vehicle or gun. Consume
            // the vanilla binding only
            // while the OpenNeoUA debug mode is active.
            if ( arg->field_8->KbdLastHit == Input::KC_F5 )
            {
                arg->field_8->HotKeyID = -1;

                NC_STACK_ypabact *selectedVehicle = resolveDebugDestroyTarget();
                const bool destroyed = selectedVehicle != nullptr;
                if ( destroyed )
                {
                    selectedVehicle->_killer = nullptr;
                    selectedVehicle->_killer_owner = 0;
                    selectedVehicle->_energy = 0;

                    setState_msg deathState;
                    deathState.unsetFlags = 0;

                    // Match the normal lethal-damage transition for tanks and
                    // cars: their death state is DEATH2, which emits the
                    // begin_fx trigger "destroyed". Vehicles and guns of any
                    // other kind use DEAD, same as normal lethal damage.
                    // Keep this as a direct internal transition so F5 remains
                    // a forced debug kill even while global invulnerability
                    // (F8) is enabled.
                    if ( selectedVehicle->_bact_type == BACT_TYPES_TANK ||
                         selectedVehicle->_bact_type == BACT_TYPES_CAR )
                    {
                        deathState.newStatus = BACT_STATUS_NOPE;
                        deathState.setFlags = BACT_STFLAG_DEATH2;
                    }
                    else
                    {
                        deathState.newStatus = BACT_STATUS_DEAD;
                        deathState.setFlags = 0;
                    }

                    selectedVehicle->SetStateInternal(&deathState);
                    selectedVehicle->Die();
                    _bactOnMouse = nullptr;
                }

                yw_arg159 infoMsg;
                infoMsg.txt = destroyed ?
                              "Target Destroyed" :
                              "No Vehicle or Gun Selected";
                infoMsg.unit = nullptr;
                infoMsg.Priority = 100;
                infoMsg.MsgID = 0;
                ypaworld_func159(&infoMsg);
            }

            // F12: take control of the hovered enemy's faction and unit.
            // A missing enemy Host Station leaves the player's side unchanged.
            if ( arg->field_8->KbdLastHit == Input::KC_F12 )
            {
                arg->field_8->HotKeyID = -1;
                NC_STACK_ypabact *target = (_guiActFlags & 0x20) ? resolveDebugDestroyTarget() : nullptr;
                const bool changed = DebugSwitchToFactionUnit(target);

                yw_arg159 infoMsg;
                infoMsg.txt = changed ? "Enemy Faction Control Acquired" :
                              "Select Enemy Unit With Living Host Station";
                infoMsg.unit = nullptr;
                infoMsg.Priority = 100;
                infoMsg.MsgID = 0;
                ypaworld_func159(&infoMsg);
            }

            // F11: player Host Station economy cheat. The three Host batteries
            // stay full and user-issued Host actions are charged zero energy.
            if ( arg->field_8->KbdLastHit == Input::KC_F11 )
            {
                arg->field_8->HotKeyID = -1;
                _debugHostStationCheat = !_debugHostStationCheat;
                yw_ApplyDebugHostStationResources(this);

                yw_arg159 infoMsg;
                infoMsg.txt = _debugHostStationCheat ?
                              "Host Station Cheat ON" :
                              "Host Station Cheat OFF";
                infoMsg.unit = nullptr;
                infoMsg.Priority = 100;
                infoMsg.MsgID = 0;
                ypaworld_func159(&infoMsg);
            }
        }

        yw_ApplyDebugHostStationResources(this);
        if ( _debugDpsEnabled )
            DebugUpdateDpsSource();

        _frameTime = arg->DTime;
        _framesElapsed++;

        _updateMessage.user_action = World::DOACTION_0;
        _updateMessage.gTime = _timeStamp;
        _updateMessage.frameTime = arg->DTime;
        _updateMessage.units_count = 0;
        _updateMessage.inpt = arg->field_8;
        // Sprint consumes the same already-scaled gameplay delta as every
        // other update path. New systems should use _updateMessage.frameTime
        // or _timeStamp rather than adding event-specific scaling.
        UpdatePlayerSprint(_updateMessage.inpt, gameplayFrozen ? 0 : _updateMessage.frameTime);
        _FPS = 1024 / unscaledFrameTime;
        _profileVals[PFID_FPS] = _FPS;

        if ( !gameplayFrozen )
            HistoryEventAdd(World::History::Frame(_timeStamp));

        uint32_t v22 = profiler_begin();

        if ( _isNetGame && !gameplayFrozen )
            yw_NetMsgHndlLoop(this);

        if ( !_isNetGame || _levelInfo.State != TLevelInfo::STATE_ABORTED )
        {
            _profileVals[PFID_NETTIME] = profiler_end(v22);

            uint32_t v23 = profiler_begin();

            CrashDiag::SetPhase("WorldVisibility");
            for (cellArea &cell : _cells)
            {
                cell.view_mask = cellArea::ViewMask(cell.owner);
                cell.UnhideMask = 0;
            }

            for (NC_STACK_ypabact* &unit : _unitsList)
                unit->MarkSectorsForView();

            _profileVals[PFID_MARKTIME] = profiler_end(v23);

            if ( !_doNotRender )
            {
                GFX::Engine.BeginFrame();
                /*_win3d->setRSTR_BGpen(0);
                _win3d->raster_func192(nullptr);*/
            }

            // Spectator Follow input is handled during the GUI input pass,
            // immediately after the tactical map. This lets an open map consume
            // the shared wheel/+/- controls first while the controlled Spectator
            // UFO keeps its normal class-level input for the later User_layer.

            if ( !gameplayFrozen )
            {
                CrashDiag::SetPhase("WorldSimulationPre");
                ypaworld_func64__sub15(this);
                ypaworld_func64__sub16(this);
                ypaworld_func64__sub17(this);
            }

            sub_4C40AC();

            if ( !gameplayFrozen )
            {
                ypaworld_func64__sub9(this);
                ypaworld_func64__sub19();
                BuildingConstructUpdate(arg->DTime);
                BuildingDecorationFXUpdate();
                UpdateAtmosphericFX();
            }

            if ( !_doNotRender )
            {
                CrashDiag::SetPhase("WorldGuiPrecompute");
                uint32_t v33 = profiler_begin();

                ypaworld_func64__sub8(this);
                ypaworld_func64__sub7(arg->field_8);

                sub_445230(this);

                ypaworld_func64__sub14(this);
                ypaworld_func64__sub21(arg->field_8);

                _profileVals[PFID_GUITIME] = profiler_end(v33);
            }

            if ( _doEnergyRecalc && !gameplayFrozen )
                DoSectorsEnergyRecalc();

            if ( _replayRecorder->do_record && !gameplayFrozen )
                recorder_update_time(this, arg->DTime);

            _guiVisor.field_0 = 0;
            _guiVisor.field_4 = 0;

            _ownerOldCellUserUnit = _userUnit->_pSector->owner;

            uint32_t v37 = profiler_begin();

            // Do user commands before any unit state can be changed. Spectator Mode
            // keeps the original faction under AI control, so do not consume the
            // observer UFO's class-level hotkeys as Robo/squad commands here.
            if (!gameplayFrozen && _userRobo && !IsSpectatorControlled())
            {
                if (_userRobo->_bact_type == BACT_TYPES_ROBO)
                {
                    if ( _debugHostStationCheat )
                        _updateMessage.energy = 0;
                    ((NC_STACK_yparobo *)_userRobo)->HandleUserCommands(&_updateMessage);
                }
            }

            CrashDiag::UpdateWorldState(_levelInfo.LevelID,
                                        _updateMessage.units_count,
                                        _userRobo ? _userRobo->_gid : 0,
                                        _userUnit ? _userUnit->_gid : 0,
                                        _viewerBact ? _viewerBact->_gid : 0);
            CrashDiag::SetPhase("WorldUnitUpdate");

            if ( !gameplayFrozen )
            {
                for ( NC_STACK_ypabact *unit : SnapshotBacts(_unitsList) )
                {
                    CrashDiag::SetActiveBact(unit, unit->_gid, unit->_bact_type,
                                                unit->_owner, unit->_status,
                                                unit->_status_flg, unit->_energy);
                    if (_isNetGame && unit != _userRobo && unit->_bact_type == BACT_TYPES_ROBO)
                        unit->NetUpdate(&_updateMessage);
                    else
                        unit->Update(&_updateMessage);

                    CrashDiag::ClearActiveBact();
                    _updateMessage.units_count++;
                }
            }

            CrashDiag::ClearActiveBact();
            yw_ApplyDebugHostStationResources(this);
            CrashDiag::UpdateWorldState(_levelInfo.LevelID,
                                        _updateMessage.units_count,
                                        _userRobo ? _userRobo->_gid : 0,
                                        _userUnit ? _userUnit->_gid : 0,
                                        _viewerBact ? _viewerBact->_gid : 0);
            _profileVals[PFID_UPDATETIME] = profiler_end(v37);

            CrashDiag::SetPhase("WorldPostUpdate");
            sub_445230(this);

            uint32_t v41 = profiler_begin();

            if ( _isNetGame && !gameplayFrozen )
            {
                if ( arg->DTime == 1 )
                    _netFlushTimer -= 20;
                else
                    _netFlushTimer -= arg->DTime;

                if ( _netFlushTimer <= 0 )
                {

                    uint32_t v44 = _netDriver->FlushBroadcastBuffer();

                    _GameShell->netsend_count += v44;

                    if ( !_GameShell->net_packet_min || v44 < _GameShell->net_packet_min )
                        _GameShell->net_packet_min = v44;

                    if ( v44 > _GameShell->net_packet_max )
                        _GameShell->net_packet_max = v44;

                    if ( v44 )
                        _GameShell->net_packet_cnt++;

                    if ( SPEED_DOWN_NET )
                        _netFlushTimer = 1500;
                    else
                        _netFlushTimer = _GameShell->flush_time_norm;
                }
            }
            _profileVals[PFID_NETTIME] += profiler_end(v41);

            if ( _userUnit )
            {
                if ( _GameShell )
                {
                    _GameShell->samples1_info.Position = _userUnit->_position;
                    _GameShell->samples1_info.Vector = _userUnit->_fly_dir * _userUnit->_fly_dir_length;
                    SFXEngine::SFXe.UpdateSoundCarrier(&_GameShell->samples1_info);
                }
            }

            UpdateAmbientLevelSound();

            //ypaworld_func64__sub22(this); // scene events

            if (_script && !gameplayFrozen)
            {
                CrashDiag::SetPhase("WorldScriptUpdate");
                _script->CallUpdate(_timeStamp, arg->DTime);
            }

            if ( !gameplayFrozen )
            {
                CrashDiag::SetPhase("WorldVoiceUpdate");
                VoiceMessageUpdate(); // update sound messages
            }

            const mat3x3 &v57 = SFXEngine::SFXe.sb_0x424c74();
            TF::TForm3D *v58 = TF::Engine.GetViewPoint();

            v58->SclRot = v57 * v58->SclRot;

            if ( _replayRecorder->do_record && !gameplayFrozen )
                recorder_write_frame();

            ypaworld_func64__sub3(this);

            if ( !_doNotRender )
            {
                uint32_t v62 = profiler_begin();

                if ( _userUnit->_cellId ) // if cell is not 0,0
                {
                    CrashDiag::SetPhase("WorldRender3D");
                    RenderGame(arg, 0);

                    GFX::Engine.BeginVirtualUI(_screenSize);

                    CrashDiag::SetPhase("WorldRenderUI");
                    uint32_t uiRenderStart = profiler_begin();
                    sb_0x4d7c08__sub0(this);
                    _profileVals[PFID_NEWGUITIME] = profiler_end(uiRenderStart);

                    if ( _isNetGame )
                        yw_NetDrawStats(this);
                }
                else
                {
                    GFX::Engine.BeginVirtualUI(_screenSize);
                }

                debug_info_draw(arg->field_8);

                // Map/Squadron Manager only occlude gameplay UI inside their
                // own rectangles. Finalize them after every ordinary overlay
                // so the overlap can be cleared without globally hiding HUD.
                yw_FinalizePriorityGameplayUi(this);

                GFX::Engine.EndVirtualUI();

                GFX::Engine.EndFrame();

                _profileVals[PFID_RENDERTIME] = profiler_end(v62);
            }

            CrashDiag::SetPhase("WorldFrameFinalize");
            FFeedback_Update(); // Do vibrate joystick

            sb_0x447720(this, arg->field_8); // Snaps/ start/stop recording

            _profileVals[PFID_FRAMETIME] = profiler_end(v92);
            _profileVals[PFID_POLYGONS] = _polysDraw;

            ProfileCalcValues();

            if ( !_doNotRender )
            {
                if ( _netChatSystem )
                {
                    _doNotRender = true;

                    dprintf("MAKE ME %s (multiplayer part Messaging)\n", "ypaworld_func64");

//
//
//
//
//
                    _doNotRender = false;
//
                }
            }

            // OpenNeoUA: legacy online help pages are obsolete/dead.
            // Clear pending help URLs instead of launching an external browser.
            if ( !_helpURL.empty() )
                _helpURL.clear();

            // ProcessNextFrame owns the platform timestamp. All in-game code
            // above saw the virtual timestamp; restore the external clock now.
            arg->TimeStamp = realFrameTimeStamp;

        }
    }

    return 1;
}

int32_t NC_STACK_ypaworld::GetGameplayRenderTimeStamp() const
{
    return System::GameClock.IsActive() ? System::GameClock.VisualTime() : _timeStamp;
}

void NC_STACK_ypaworld::StartRoboDeathTimeScale(const NC_STACK_ypabact *destroyedRobo)
{
    if ( _isNetGame || !destroyedRobo )
        return;

    const TimedGameplayScaleProfile profile = yw_GetRoboDeathScaleProfile();
    if ( profile.scale >= 1.0f || profile.durationMs == 0 )
        return;

    const float maxDistance = yw_ReadNonNegativeDistance(
        System::IniConf::GameRoboDeathTimeScaleMaxDistance, 0.0f);
    if ( maxDistance > 0.0f )
    {
        if ( !_userUnit )
            return;

        const float dx = destroyedRobo->_position.x - _userUnit->_position.x;
        const float dz = destroyedRobo->_position.z - _userUnit->_position.z;
        if ( dx * dx + dz * dz > maxDistance * maxDistance )
            return;
    }

    const int32_t now = _timeStamp;
    const int32_t requestedEnd = now + (int32_t)profile.durationMs;

    // Repeated qualifying Host Station deaths refresh/extend the one shared
    // event instead of creating parallel timers. The controller itself lives
    // in the master gameplay-time domain too: F4 stretches its duration and
    // F4 freezes it together with the world instead of letting a wall clock
    // expire the effect behind a paused/slowed simulation.
    if ( !HasActiveRoboDeathTimeScale() || requestedEnd > _roboDeathTimeScaleEndTime )
        _roboDeathTimeScaleEndTime = requestedEnd;
}

bool NC_STACK_ypaworld::HasActiveRoboDeathTimeScale() const
{
    return _roboDeathTimeScaleEndTime != 0 &&
           _roboDeathTimeScaleEndTime > _timeStamp;
}

void sub_47C1EC(NC_STACK_ypaworld *yw, TMapGem *gemProt, int *a3, int *a4)
{
    switch ( yw->_GameShell->netPlayerOwner )
    {
    case 1:
        *a3 = gemProt->NwVprotoNum1;
        *a4 = gemProt->NwBprotoNum1;
        break;

    case 6:
        *a3 = gemProt->NwVprotoNum2;
        *a4 = gemProt->NwBprotoNum2;
        break;

    case 3:
        *a3 = gemProt->NwVprotoNum3;
        *a4 = gemProt->NwBprotoNum3;
        break;

    case 4:
        *a3 = gemProt->NwVprotoNum4;
        *a4 = gemProt->NwBprotoNum4;
        break;

    default:
        *a3 = 0;
        *a4 = 0;
        break;
    }
}

void sub_47C29C(NC_STACK_ypaworld *yw, cellArea *cell, int a3)
{
    TMapGem &gem = yw->_techUpgrades[a3];

    int a3a, a4;
    sub_47C1EC(yw, &gem, &a3a, &a4);

    bool newUiCapture = System::IniConf::GameGemUnlockNewUI.Get<bool>();

    yw->_upgradeId = a3;
    yw->_upgradeTimeStamp = yw->_timeStamp;
    yw->_upgradeVehicleId = a3a;
    yw->_upgradeWeaponId = 0;
    yw->_upgradeBuildId = a4;

    if ( newUiCapture )
        yw->BeginGemNotificationCapture();
    else
        yw->ClearGemNotificationCapture();

    if ( a3a )
    {
        bool wasEnabled = (yw->_vhclProtos[a3a].disable_enable_bitmask &
                           (1 << yw->_GameShell->netPlayerOwner)) != 0;
        yw->_vhclProtos[a3a].disable_enable_bitmask = 0;
        yw->_vhclProtos[a3a].disable_enable_bitmask |= 1 << yw->_GameShell->netPlayerOwner;
        yw->RecordGemNotificationChange(TGemNotificationEntry::TARGET_VEHICLE, a3a,
                                        TGemNotificationEntry::CHANGE_ENABLE,
                                        wasEnabled ? 1 : 0, 1, !wasEnabled);
    }

    if ( a4 )
    {
        bool wasEnabled = (yw->_buildProtos[a4].EnableMask &
                           (1 << yw->_GameShell->netPlayerOwner)) != 0;
        yw->_buildProtos[a4].EnableMask = 0;
        yw->_buildProtos[a4].EnableMask |= 1 << yw->_GameShell->netPlayerOwner;
        yw->RecordGemNotificationChange(TGemNotificationEntry::TARGET_BUILDING, a4,
                                        TGemNotificationEntry::CHANGE_ENABLE,
                                        wasEnabled ? 1 : 0, 1, !wasEnabled);
    }

    if ( newUiCapture )
        yw->FinishGemNotificationCapture(true);

    yw_arg159 v14;
    if ( newUiCapture )
        v14.txt = yw->BuildNewGemNotificationLogText();
    if ( v14.txt.empty() )
        v14.txt = Locale::Text::Feedback(Locale::FEEDBACK_TECHUP);
    v14.unit = 0;
    v14.Priority = 48;

    if ( gem.Type )
        v14.MsgID = World::Log::GetUpgradeLogID(gem.Type);
    else
        v14.MsgID = 0;

    v14.IgnoreAudioTimeScale = true;
    yw->ypaworld_func159(&v14);
    yw->PlayConfiguredGemUnlockSound();

    if ( yw->_isNetGame && yw->_netExclusiveGem )
    {
        uamessage_upgrade upMsg;
        upMsg.msgID = UAMSG_UPGRADE;
        upMsg.owner = yw->_GameShell->netPlayerOwner;
        upMsg.enable = 1;
        upMsg.upgradeID = a3;

        yw->NetBroadcastMessage(&upMsg, sizeof(upMsg), true);
    }

    cell->PurposeType = cellArea::PT_TECHDEACTIVE;
}

void ypaworld_func129__sub1(NC_STACK_ypaworld *yw, cellArea *cell, int a3)
{
    TMapGem &gem = yw->_techUpgrades[a3];

    int a3a;
    int a4;

    sub_47C1EC(yw, &gem, &a3a, &a4);

    if ( a3a )
        yw->_vhclProtos[a3a].disable_enable_bitmask = 0;

    if ( a4 )
        yw->_buildProtos[a4].EnableMask = 0;

    std::string v13 = Locale::Text::Feedback(Locale::FEEDBACK_TECHDOWN) + gem.MsgDefault;

    yw_arg159 arg159;
    arg159.unit = 0;
    arg159.Priority = 80;
    arg159.txt = v13;
    arg159.MsgID = 29;

    yw->ypaworld_func159(&arg159);

    if ( yw->_isNetGame )
    {
        uamessage_upgrade upMsg;
        upMsg.msgID = UAMSG_UPGRADE;
        upMsg.owner = yw->_GameShell->netPlayerOwner;
        upMsg.enable = 0;
        upMsg.upgradeID = a3;

        yw->NetBroadcastMessage(&upMsg, sizeof(upMsg), true);
    }

    cell->PurposeIndex = 0;
    cell->PurposeType = cellArea::PT_NONE;
}

void NC_STACK_ypaworld::yw_ActivateWunderstein(cellArea *cell, int gemid)
{
    _upgradeVehicleId = 0;
    _upgradeBuildId = 0;
    _upgradeWeaponId = 0;

    bool newUiCapture = System::IniConf::GameGemUnlockNewUI.Get<bool>();

    _upgradeId = gemid;
    _upgradeTimeStamp = _timeStamp;

    if ( newUiCapture )
        BeginGemNotificationCapture();
    else
        ClearGemNotificationCapture();

    TMapGem &gem = _techUpgrades[gemid];
    bool parseSuccessful = false;

    if ( !gem.ScriptFile.empty() )
    {
        parseSuccessful = LoadProtosScript(gem.ScriptFile);
        if ( !parseSuccessful )
            ypa_log_out("yw_ActivateWunderstein: ERROR parsing script %s.\n", gem.ScriptFile.c_str());
    }
    else
    {
        std::string tmp = Common::Env.SetPrefix("rsrc", "data:");

        ScriptParser::HandlersList parsers {
            new World::Parsers::BuffProfileParser(&_buffProfiles),
            new World::Parsers::DebuffProfileParser(&_debuffProfiles),
            new World::Parsers::VhclProtoParser(this),
            new World::Parsers::WeaponProtoParser(this),
            new World::Parsers::BuildProtoParser(this)
        };

        parseSuccessful = ScriptParser::ParseStringList(gem.ActionsList, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);
        Common::Env.SetPrefix("rsrc", tmp);
        if ( parseSuccessful )
            ResolveStatusProfileLinks();
    }

    if ( newUiCapture )
        FinishGemNotificationCapture(parseSuccessful);

    yw_arg159 arg159;
    arg159.unit = nullptr;
    arg159.Priority = 48;
    if ( newUiCapture )
        arg159.txt = BuildNewGemNotificationLogText();
    if ( arg159.txt.empty() )
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_TECHUP);

    if ( gem.Type )
        arg159.MsgID = World::Log::GetUpgradeLogID(gem.Type);
    else
        arg159.MsgID = 0;

    arg159.IgnoreAudioTimeScale = true;
    ypaworld_func159(&arg159);

    if ( parseSuccessful )
        PlayConfiguredGemUnlockSound();

    cell->PurposeType = cellArea::PT_TECHDEACTIVE;
}

void sub_44FD6C(NC_STACK_ypaworld *yw, const cellArea &cell, int bldX, int bldY)
{
    Common::Point dist = yw->_viewerBact->_cellId.AbsDistance( cell.CellId );

    if ( dist.x + dist.y <= (yw->_renderSectors - 1) / 2 )
    {
        const TLego &v10 = yw->_legoArray[  yw->GetLegoBld(&cell, bldX, bldY) ];

        vec3d ttt = World::SectorIDToCenterPos3( cell.CellId );
        ttt.y = cell.height;

        if ( cell.SectorType != 1 )
        {
            ttt.x += (bldX - 1) * 300.0;
            ttt.z += (bldY - 1) * 300.0;
        }

        for (const TLego::ExFX &fx : v10.Explosions)
        {
            if ( fx.Index >= yw->_fxLimit )
                break;

            ypaworld_arg146 arg146;
            arg146.vehicle_id = fx.ObjectID;
            arg146.pos = fx.Position + ttt;

            NC_STACK_ypabact *boom = yw->ypaworld_func146(&arg146);

            if ( boom )
            {
                boom->_owner = 0;

                setState_msg arg78;
                arg78.newStatus = BACT_STATUS_DEAD;
                arg78.setFlags = BACT_STFLAG_DEATH1;
                arg78.unsetFlags = 0;
                boom->SetState(&arg78);

                // Building LEGO explosion objects are spawned directly in the
                // going-down state, which only starts SND_GODOWN. The dedicated
                // building sound objects define SND_EXPLODE instead, so trigger
                // that existing slot at creation time. Objects without an
                // explode sample remain silent through the normal sound fallback.
                SFXEngine::SFXe.startSound(&boom->_soundcarrier,
                                           World::TVhclProto::SND_EXPLODE);

                yw->ypaworld_func134(boom);

                bact_arg83 arg83;
                arg83.pos = arg146.pos;
                arg83.energ = 40000;
                arg83.pos2.x = fx.Position.x;
                arg83.pos2.y = -150.0;
                arg83.pos2.z = fx.Position.z;

                float tmp = arg83.pos2.length();

                if ( tmp > 0.1 )
                    arg83.pos2 /= tmp;

                arg83.force = 30.0;
                arg83.mass = 50.0;

                boom->ApplyImpulse(&arg83);
            }
        }
    }
}

void ypaworld_func129__sub0(NC_STACK_ypaworld *yw, const cellArea &cell, yw_arg129 *arg)
{
    if ( cell.PurposeType == cellArea::PT_POWERSTATION )
    {
        if ( cell.owner == yw->_userRobo->_owner )
        {
            if ( arg->unit )
            {
                if ( yw->_userRobo->_owner != arg->unit->_owner && yw->_timeStamp - yw->_msgTimestampPSUnderAtk > 5000 )
                {
                    yw->_msgTimestampPSUnderAtk = yw->_timeStamp;

                    yw_arg159 arg159;
                    arg159.unit = nullptr;
                    arg159.Priority = 77;
                    arg159.MsgID = 33;

                    yw->ypaworld_func159(&arg159);
                }
            }
        }
    }
}

void NC_STACK_ypaworld::ApplyBuildingHealthChange(cellArea *cell, int bldX, int bldY,
                                                   int targetHealth, yw_arg129 *arg)
{
    if ( !cell || !arg )
        return;

    targetHealth = std::max(0, std::min(targetHealth, 255));
    int currentHealth = cell->buildings_health.At(bldX, bldY);
    int currentModel = _buildHealthModelId[currentHealth];
    int targetModel = _buildHealthModelId[targetHealth];

    if ( currentModel > targetModel )
    {
        while ( currentModel > targetModel )
        {
            sub_44FD6C(this, *cell, bldX, bldY);
            --currentModel;
        }
    }
    else if ( currentModel < targetModel )
    {
        while ( currentModel < targetModel )
        {
            sub_44FD6C(this, *cell, bldX, bldY);
            ++currentModel;
        }
    }

    cell->buildings_health.At(bldX, bldY) = targetHealth;
    ypaworld_func129__sub0(this, *cell, arg);
    CellCheckHealth(cell, arg->OwnerID, arg->unit);

    if ( cell->PurposeType == cellArea::PT_TECHUPGRADE )
    {
        if ( _userRobo && _userRobo->_owner == cell->owner )
        {
            if ( _isNetGame )
                sub_47C29C(this, cell, cell->PurposeIndex);
            else
                yw_ActivateWunderstein(cell, cell->PurposeIndex);

            HistoryEventAdd(World::History::Upgrade(cell->CellId.x, cell->CellId.y,
                                                    cell->owner, _techUpgrades[_upgradeId].Type,
                                                    _upgradeVehicleId, _upgradeWeaponId,
                                                    _upgradeBuildId));
        }
    }
    else if ( cell->PurposeType == cellArea::PT_TECHDEACTIVE && _isNetGame )
    {
        int totalHealth = 0;
        for (auto health : cell->buildings_health)
            totalHealth += health;

        if ( !totalHealth )
            ypaworld_func129__sub1(this, cell, cell->PurposeIndex);
    }
}

void NC_STACK_ypaworld::ypaworld_func129(yw_arg129 *arg)
{
    if ( arg->unit && IsSpectatorBact(arg->unit) )
        return;

    Common::Point sec = World::PositionToSectorID( arg->pos );
    cellArea &cell = _cells.At(sec);

    if ( cell.IsGamePlaySector() && cell.PurposeType != cellArea::PT_CONSTRUCTING )
    {
        int v8 = (int)(arg->pos.x / 150.0) % 8;

        int v10;

        if ( v8 < 3 )
            v10 = 1;
        else if ( v8 < 5)
            v10 = 2;
        else
            v10 = 3;

        v8 = (int)(-arg->pos.z / 150.0) % 8;

        int v14;

        if ( v8 < 3 )
            v14 = 1;
        else if ( v8 < 5)
            v14 = 2;
        else
            v14 = 3;

        if ( v10 && v14 )
        {
            int bldY;
            int bldX;

            if ( cell.SectorType == 1 )
            {
                bldY = 0;
                bldX = 0;
            }
            else
            {
                bldX = v10 - 1;
                bldY = 2 - (v14 - 1);
            }

            int currentHealth = cell.buildings_health.At(bldX, bldY);

            int targetHealth = currentHealth - arg->field_10 * (100 - _legoArray[GetLegoBld(&cell, bldX, bldY)].Shield) / 100 / 400;

            ApplyBuildingHealthChange(&cell, bldX, bldY, targetHealth, arg);
        }
    }
}

size_t NC_STACK_ypaworld::GetSectorInfo(yw_130arg *arg)
{
    arg->CellId = World::PositionToSectorID( arg->pos_x, arg->pos_z );

    if ( !IsSector( arg->CellId ) )
    {
        ypa_log_out("YWM_GETSECTORINFO %d %d max: %d %d\n", arg->CellId.x, arg->CellId.y, _mapSize.x, _mapSize.y);
        ypa_log_out("YWM_GETSECTORINFO ausserhalb!!!\n");

        arg->pcell = nullptr;

        return 0;
    }

    arg->pcell = &_cells(arg->CellId);
    return 1;
}

void NC_STACK_ypaworld::ypaworld_func131(NC_STACK_ypabact *bact)
{
    _viewerBact = bact;

    setYW_userVehicle(bact);
}

void NC_STACK_ypaworld::ypaworld_func132(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func132");
}

void NC_STACK_ypaworld::ypaworld_func133(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func133");
}

void NC_STACK_ypaworld::ypaworld_func134(NC_STACK_ypabact *bact)
{
    newMaster_msg arg73;

    arg73.bact = nullptr;
    arg73.list = &_unitsList;

    bact->SetNewMaster(&arg73);
}

void NC_STACK_ypaworld::ypaworld_func135(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func135");
}

void NC_STACK_ypaworld::ypaworld_func136(ypaworld_arg136 *arg)
{
    arg->tVal = 2.0;
    arg->isect = 0;
    arg->hitCell = Common::Point(-1, -1);
    arg->hitCollisionType = 0;
    arg->hitSkelPos = vec3d(0.0, 0.0, 0.0);

    vec3d stpos = arg->stPos;

    float pos_xx = stpos.x + arg->vect.x;
    float pos_zz = stpos.z + arg->vect.z;

    int dx = (stpos.x + 150) / 300;
    int dxx = (pos_xx + 150) / 300;

    int dz = (-stpos.z + 150) / 300;

    int dzz = (-pos_zz + 150) / 300;

    int elems = 0;
    TSectorCollision a6[4];

    if ( dx == dxx && dz == dzz )
    {
        elems = 1;
        a6[0] = sub_44DBF8(dx, dz, dx, dz, arg->flags);
    }
    else if ( dx == dxx || dz == dzz )
    {
        elems = 2;
        a6[0] = sub_44DBF8(dx, dz, dx,  dz,  arg->flags);
        a6[1] = sub_44DBF8(dx, dz, dxx, dzz, arg->flags);
    }
    else
    {
        elems = 4;
        a6[0] = sub_44DBF8(dx, dz, dx,  dz,  arg->flags);
        a6[1] = sub_44DBF8(dx, dz, dx,  dzz, arg->flags);
        a6[2] = sub_44DBF8(dx, dz, dxx, dz,  arg->flags);
        a6[3] = sub_44DBF8(dx, dz, dxx, dzz, arg->flags);
    }

    for (int i = 0; i < elems; i++)
    {
        if ( a6[i].CollisionType )
        {
            if ( a6[i].CollisionType != 1)
                sub_44E07C(a6[i]);

            arg->stPos = stpos - a6[i].pos;

            sub_44D8B8(arg, a6[i]);

            if ( arg->isect )
                break;
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func137(ypaworld_arg137 *arg)
{
    arg->coll_count = 0;

    vec3d pos = arg->pos;

    int dxx = (pos.x + 150) / 300;
    int dzz = (-pos.z + 150) / 300;
    int xxmr = (pos.x - arg->radius + 150) / 300;
    int zzmr = (-(pos.z - arg->radius) + 150) / 300;
    int xxpr = (pos.x + arg->radius + 150) / 300;
    int zzpr = (-(pos.z + arg->radius) + 150) / 300;

    TSectorCollision a6;

    for (int i = 0; i < 9; i++)
    {
        a6.CollisionType = 0;

        switch ( i )
        {
        case 0:
            a6 = sub_44DBF8(dxx, dzz, dxx, dzz, arg->field_30);
            break;

        case 1:
            if ( dxx != xxmr )
                a6 = sub_44DBF8(dxx, dzz, xxmr, dzz, arg->field_30);
            break;

        case 2:
            if ( dxx != xxpr )
                a6 = sub_44DBF8(dxx, dzz, xxpr, dzz, arg->field_30);
            break;

        case 3:
            if ( dzz != zzmr )
                a6 = sub_44DBF8(dxx, dzz, dxx, zzmr, arg->field_30);
            break;

        case 4:
            if ( dzz != zzpr )
                a6 = sub_44DBF8(dxx, dzz, dxx, zzpr, arg->field_30);
            break;

        case 5:
            if ( dxx != xxmr && dzz != zzmr )
                a6 = sub_44DBF8(dxx, dzz, xxmr, zzmr, arg->field_30);
            break;

        case 6:
            if ( dxx != xxpr && dzz != zzmr )
                a6 = sub_44DBF8(dxx, dzz, xxpr, zzmr, arg->field_30);
            break;

        case 7:
            if ( dxx != xxpr && dzz != zzpr )
                a6 = sub_44DBF8(dxx, dzz, xxpr, zzpr, arg->field_30);
            break;

        case 8:
            if ( dxx != xxmr && dzz != zzpr )
                a6 = sub_44DBF8(dxx, dzz, xxmr, zzpr, arg->field_30);
            break;
        }

        if ( a6.CollisionType )
        {
            if ( a6.CollisionType != 1 )
                sub_44E07C(a6);

            arg->pos = pos - a6.pos;

            ypaworld_func137__sub0(arg, a6);
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func138(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func138");
}

void NC_STACK_ypaworld::ypaworld_func139(GuiBase *lstvw)
{
    if ( !(lstvw->flags & GuiBase::FLAG_WITH_ICON) )
        lstvw->flags &= ~GuiBase::FLAG_ICONIFED;

    if ( lstvw->flags & GuiBase::FLAG_IN_LIST )
        ypaworld_func140(lstvw);

    lstvw->Attach(_guiActive);

    lstvw->flags |= GuiBase::FLAG_IN_LIST;

    if ( lstvw->flags & GuiBase::FLAG_WITH_ICON )
        lstvw->iconBox.pobject = lstvw;

    lstvw->pobject = lstvw;

    if ( lstvw->flags & GuiBase::FLAG_ICONIFED )
        Input::Engine.AddClickBoxFront(&lstvw->iconBox);
    else if ( lstvw->IsOpen() )
        Input::Engine.AddClickBoxFront(lstvw);
}

void NC_STACK_ypaworld::ypaworld_func140(GuiBase *lstvw)
{
    if ( lstvw->flags & GuiBase::FLAG_IN_LIST )
    {
        lstvw->Detach();

        lstvw->flags &= ~GuiBase::FLAG_IN_LIST;

        if ( lstvw->flags & GuiBase::FLAG_ICONIFED )
            Input::Engine.RemClickBox(&lstvw->iconBox);
        else if ( lstvw->IsOpen() )
            Input::Engine.RemClickBox(lstvw);
    }
}

void NC_STACK_ypaworld::ypaworld_func143(void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func143");
}

void NC_STACK_ypaworld::ypaworld_func144(NC_STACK_ypabact *bacto)
{
    if ( bacto->_bact_type == BACT_TYPES_MISSLE )
    {
        if ( bacto->_primTtype )
            ypa_log_out("OH NO! The DEATH CACHE BUG is back!\n");
    }

    SFXEngine::SFXe.StopCarrier(&bacto->_soundcarrier);

    newMaster_msg cache;
    cache.bact = nullptr;
    cache.list = &_deadCacheList;

    bacto->SetNewMaster(&cache);

    bact_arg80 v6;
    v6.pos.x = 600.0;
    v6.pos.y = -50000.0;
    v6.pos.z = -600.0;
    v6.field_C = 2;

    bacto->SetPosition(&v6);

    bacto->_status_flg |= BACT_STFLAG_NORENDER;
}

size_t NC_STACK_ypaworld::ypaworld_func145(NC_STACK_ypabact *bact)
{
    if ( _viewerBact )
    {
        Common::Point dist = _viewerBact->_cellId.AbsDistance( bact->_cellId );
        if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
            return 1;
    }

    for ( NC_STACK_ypabact* &station : _unitsList ) //Robos
    {
        if ( station->_status_flg & BACT_STFLAG_ISVIEW )
        {
            Common::Point dist = station->_cellId.AbsDistance( bact->_cellId );
            if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                return 1;
        }


        for ( NC_STACK_ypabact* &comm : station->_kidList ) // Squad comms
        {
            if ( comm->_status_flg & BACT_STFLAG_ISVIEW )
            {
                Common::Point dist = comm->_cellId.AbsDistance( bact->_cellId );
                if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                    return 1;
            }


            for ( NC_STACK_ypabact* &unit : comm->_kidList ) // Squad units
            {
                if ( unit->_status_flg & BACT_STFLAG_ISVIEW )
                {
                    Common::Point dist = unit->_cellId.AbsDistance( bact->_cellId );
                    if ( dist.x + dist.y <= (_renderSectors - 1) / 2 )
                        return 1;
                }
            }
        }
    }

    return 0;
}

NC_STACK_ypabact *NC_STACK_ypaworld::SpawnInlinePhysicalFX(
    const World::TChainFXConfig &config, const vec3d &pos,
    const mat3x3 &rot, const vec3d &launch, int16_t owner,
    NC_STACK_ypabact *deadWreck)
{
    const std::shared_ptr<World::TChainFXPhysical> &physical = config.physical_inline;
    if ( !physical )
        return nullptr;

    vec3d velocity = rot.Transform(launch);
    const double initialSpeed = velocity.length();
    if ( !std::isfinite(initialSpeed) )
        return nullptr;
    if ( initialSpeed > 0.001 )
    {
        const double impulse = (double)physical->force / (double)physical->mass;
        if ( !std::isfinite(impulse) )
            return nullptr;
        velocity += velocity / initialSpeed * impulse;
    }
    const double finalSpeed = velocity.length();
    if ( !std::isfinite(finalSpeed) )
        return nullptr;

    int16_t selectedVP = 0;
    int16_t selectedImpactVP = 0;
    if ( !physical->vp_models.empty() )
    {
        const int visualIndex = World::RandomIntRangeInclusive(
            0, (int)physical->vp_models.size() - 1);
        selectedVP = physical->vp_models[visualIndex];
        if ( physical->vp_impacts.size() == physical->vp_models.size() )
            selectedImpactVP = physical->vp_impacts[visualIndex];
    }
    else if ( physical->vp_impacts.size() == 1 )
        selectedImpactVP = physical->vp_impacts[0];

    NC_STACK_base *model = nullptr;
    if ( !physical->mesh3ds.empty() )
        model = GetSharedExternalMesh(physical->mesh3ds);
    if ( !model && !physical->base_model.empty() )
        model = GetSharedExternalBase(physical->base_model);
    if ( !model && selectedVP > 0 &&
         (size_t)selectedVP < _vhclModels.size() )
        model = _vhclModels[selectedVP];
    if ( !model )
        return nullptr;

    NC_STACK_ypabact *fragment = yw_createUnit(BACT_TYPES_FLYER);
    if ( !fragment )
        return nullptr;

    fragment->_inlinePhysicalFX = physical;
    fragment->_inlinePhysicalImpactDamageSpent = false;
    fragment->_inlinePhysicalLifetimeMs = World::RandomIntRangeInclusive(
        physical->lifetime_min, physical->lifetime_max);
    fragment->_owner = owner;
    fragment->_vehicleID = 0;
    fragment->_mass = physical->mass;
    fragment->_base_force = physical->force;
    fragment->_force = physical->force;
    fragment->_base_maxrot = physical->maxrot;
    fragment->_maxrot = physical->maxrot;
    fragment->_airconst = physical->airconst;
    fragment->_airconst_static = physical->airconst;
    fragment->_radius = physical->radius;
    fragment->_viewer_radius = physical->radius;
    // Dead-flight terrain contact still uses the legacy vertical clearance. Keep
    // its old minimum, but let larger authored fragment radii increase it.
    fragment->_overeof = std::max(fragment->_overeof, physical->radius);
    // Inline fragments remain visual/world-physics only unless gameplay impact
    // damage is explicitly authored. This keeps existing fragments unchanged.
    fragment->setBACT_bactCollisions(physical->impact_energy > 0);
    fragment->_vp_normal = model;
    fragment->_vp_dead = model;
    fragment->_vp_megadeth = selectedImpactVP > 0 &&
                             (size_t)selectedImpactVP < _vhclModels.size() ?
        _vhclModels[selectedImpactVP] : nullptr;
    fragment->_vp_genesis = nullptr;
    fragment->_vp_scale = vec3d(physical->visual_scale,
                                 physical->visual_scale,
                                 physical->visual_scale);
    fragment->_vp_tint = physical->tint;
    fragment->_rotation = World::RandomFXRotation(rot, config.random_rotation);

    SFXEngine::SFXe.StopCarrier(&fragment->_soundcarrier);
    fragment->_soundcarrier.Clear();
    fragment->_soundcarrier.Resize(World::TVhclProto::SND_MAX);
    fragment->_soundFlags = 0;
    const int events[] = {World::TVhclProto::SND_NORMAL,
                          World::TVhclProto::SND_GODOWN,
                          World::TVhclProto::SND_EXPLODE};
    for (int event : events)
    {
        World::TVhclSound &definition = physical->sounds[event];
        definition.LoadSamples();
        TSoundSource &sound = fragment->_soundcarrier.Sounds[event];
        sound.Volume = definition.volume;
        definition.ConfigureSoundSourcePitch(sound);
        sound.Radius = definition.radius;
        sound.SetLoop(World::TVhclProto::IsLoopingSnd(event));
        sound.PSample = definition.MainSample.Sample ?
            definition.MainSample.Sample->GetSampleData() : nullptr;
        sound.PPFx = definition.sndPrm.slot ? &definition.sndPrm : nullptr;
        sound.SetPFx(sound.PPFx != nullptr);
        sound.PShkFx = definition.sndPrm_shk.slot ? &definition.sndPrm_shk : nullptr;
        sound.SetShk(sound.PShkFx != nullptr);
        sound.PFragments = definition.extS.empty() ? nullptr : &definition.extS;
        sound.SetFragmented(sound.PFragments != nullptr);
    }
    fragment->_volume = fragment->_soundcarrier.Sounds[World::TVhclProto::SND_NORMAL].Volume;

    bact_arg80 position;
    position.pos = pos;

    // A ground-destruction fragment can inherit a source position that already
    // touches the terrain. Give it only the clearance needed to start its authored
    // launch instead of letting the first dead-flight collision end it immediately.
    if ( config.trigger == World::TChainFXConfig::TRIGGER_DESTROYED )
    {
        const float clearance = std::max(fragment->_overeof, fragment->_radius);
        const float probeDistance = std::max(50.0f, clearance * 2.0f);

        ypaworld_arg136 ground = {};
        ground.stPos = position.pos - vec3d::OY(probeDistance);
        ground.vect = vec3d::OY(probeDistance * 2.0f);
        ground.flags = 0;
        ypaworld_func136(&ground);

        if ( ground.isect )
        {
            const float safeY = ground.isectPos.y - clearance - 1.0f;
            if ( position.pos.y > safeY &&
                 fabs(position.pos.y - ground.isectPos.y) <= probeDistance )
                position.pos.y = safeY;
        }
    }

    position.field_C = 0;
    fragment->SetPosition(&position);
    ypaworld_func134(fragment);

    setState_msg state;
    state.newStatus = BACT_STATUS_DEAD;
    state.setFlags = 0;
    state.unsetFlags = 0;
    fragment->SetStateInternal(&state);
    fragment->_status_flg |= BACT_STFLAG_DEATH1;
    fragment->_dead_time = fragment->_clock;
    SFXEngine::SFXe.startSound(&fragment->_soundcarrier,
                               World::TVhclProto::SND_NORMAL);
    fragment->_soundFlags |= 1;

    if ( finalSpeed > 0.001 )
    {
        fragment->_fly_dir_length = finalSpeed;
        fragment->_fly_dir = velocity / finalSpeed;
    }

    // OpenNeoUA follow_vehicle: bind the dying unit's wreck to this fragment so
    // both share one exact trajectory until the fragment impacts. The guards keep
    // live shooters (weapon impact/detonate triggers) and already-landed wrecks
    // (DEATH2 phase) unbound, and with count > 1 only the first fragment binds.
    if ( physical->follow_vehicle && deadWreck &&
         deadWreck != fragment && !deadWreck->_deadFollowLeader &&
         deadWreck->_status == BACT_STATUS_DEAD &&
         !(deadWreck->_status_flg & BACT_STFLAG_DEATH2) )
    {
        fragment->_deadFollowWreck = deadWreck;
        deadWreck->_deadFollowLeader = fragment;
    }

    return fragment;
}

NC_STACK_ypabact * NC_STACK_ypaworld::ypaworld_func146(ypaworld_arg146 *vhcl_id)
{
    if ( vhcl_id->vehicle_id <= 0 || (size_t)vhcl_id->vehicle_id >= _vhclProtos.size() )
        return nullptr;

    World::TVhclProto &requestedVhcl = _vhclProtos[vhcl_id->vehicle_id];
    int mimicDisguiseVehicleId = requestedVhcl.is_mimic ?
        yw_SelectMimicVehicleID(_vhclProtos, vhcl_id->vehicle_id) :
        0;
    World::TVhclProto *spawnProto = mimicDisguiseVehicleId > 0 ?
        &_vhclProtos[mimicDisguiseVehicleId] :
        (requestedVhcl.is_mimic ? nullptr : &requestedVhcl);

    if ( !spawnProto )
        return nullptr;

    World::TVhclProto &vhcl = *spawnProto;
    World::TVhclProto &deathProto = requestedVhcl.is_mimic ? requestedVhcl : vhcl;

    NC_STACK_ypabact *bacto = yw_createUnit(vhcl.model_id);

    if ( bacto )
    {
        bacto->_energy = vhcl.energy;
        bacto->_energy_max = vhcl.energy;
        bacto->_buff = vhcl.buff;
        bacto->_buff_deflect_charges_max = vhcl.buff.valid ? vhcl.buff.deflect_charges : 0;
        bacto->_invulnerable = vhcl.buff.valid && vhcl.buff.invulnerable;
        bacto->_shield = vhcl.shield;
        bacto->_mass = vhcl.mass;
        bacto->_base_force = vhcl.force;
        bacto->_base_maxrot = vhcl.maxrot;
        bacto->_force = vhcl.force;
        bacto->_maxrot = vhcl.maxrot;
        bacto->_height = vhcl.height;
        bacto->_radius = vhcl.radius;
        bacto->_viewer_radius = vhcl.vwr_radius;

        // OpenNeoUA: universal compound collision spheres. If the prototype defines
        // coll_* spheres, copy them so getBACT_collNodes() exposes them to the
        // existing narrow-phase collision/hit tests (same path Robo uses).
        //
        // Presence matters: coll_* with no explicit radius uses compound-only
        // collision, while an authored radius keeps the vanilla sphere active
        // alongside the compound set. The prototype default radius remains
        // available to non-collision systems without silently becoming a hitbox.
        bacto->ApplyCompoundCollision(vhcl.coll, vhcl.radius_defined);

        bacto->_overeof = vhcl.overeof;
        bacto->_viewer_overeof = vhcl.vwr_overeof;
        bacto->_airconst = vhcl.airconst;
        bacto->_airconst_static = vhcl.airconst;
        bacto->_adist_sector = vhcl.adist_sector;
        bacto->_adist_bact = vhcl.adist_bact;
        bacto->_sdist_sector = vhcl.sdist_sector;
        bacto->_sdist_bact = vhcl.sdist_bact;
        bacto->_ai_engage_range = vhcl.ai_engage_range;
        bacto->_ai_retreat_range = vhcl.ai_retreat_range;
        bacto->_ai_reengage_range = vhcl.ai_reengage_range;
        bacto->_unifiedAICombatDistance = vhcl.HasValidUnifiedAICombatDistance();
        bacto->_radar = vhcl.radar;
        bacto->_gun_radius = vhcl.gun_radius;
        bacto->_gun_power = vhcl.gun_power;
        bacto->_pitch_max = vhcl.max_pitch;
        bacto->_vehicleID = vhcl_id->vehicle_id;
        bacto->_mimic_disguise_vehicleID = mimicDisguiseVehicleId > 0 ? mimicDisguiseVehicleId : 0;
        bacto->_isDummy = false; // set by gun_type for BACT_TYPES_GUN
        bacto->_weapon = vhcl.weapon;
        bacto->_extra_weapons = vhcl.extra_weapons;
        bacto->_weapon_player_switch_mode = vhcl.weapon_player_switch_mode;
        bacto->_weapon_ai_switch_mode = vhcl.weapon_ai_switch_mode;
        bacto->_weapon_slot_index = 0;
        bacto->_current_weapon_id = vhcl.weapon;
        bacto->_current_weapon_source_slot = 0;

        if ( vhcl.weapon == -1 )
            bacto->_weapon_flags = 0;
        else
            bacto->_weapon_flags = _weaponProtos.at( vhcl.weapon )._weaponFlags;

        bacto->_mgun = vhcl.mgun;
        bacto->_mgun_set = vhcl.mgun_set;
        bacto->_mgun_shot_time = vhcl.mgun_shot_time;
        bacto->_mgun_shot_time_user = vhcl.mgun_shot_time_user;
        bacto->_mgun_tracer = vhcl.mgun_tracer;
        bacto->_mgun_decal_enable = vhcl.mgun_decal_enable;
        bacto->_mgun_decal = vhcl.mgun_decal;
        bacto->_mgun_vp_dead = vhcl.mgun_vp_dead;
        bacto->_mgun_vp_megadeth = vhcl.mgun_vp_megadeth;
        bacto->_mgun_3ds_dead = vhcl.mgun_3ds_dead;
        bacto->_mgun_3ds_megadeth = vhcl.mgun_3ds_megadeth;
        bacto->_mgun_base_dead = vhcl.mgun_base_dead;
        bacto->_mgun_base_megadeth = vhcl.mgun_base_megadeth;
        bacto->_mgun_energy = vhcl.mgun_energy;
        bacto->_mgun_angle = vhcl.mgun_angle;
        bacto->_mgun_energy_set = vhcl.mgun_energy_set;
        bacto->_mgun_recoil_player = vhcl.mgun_recoil_player;
        bacto->_mgun_angle_set = vhcl.mgun_angle_set;
        bacto->_mgun_sector_damage_accum = 0.0;
        bacto->_weapon_spread_x = vhcl.weapon_spread_x;
        bacto->_weapon_spread_y = vhcl.weapon_spread_y;
        bacto->_weapon_arc_x = vhcl.weapon_arc_x;
        bacto->_weapon_arc_y = vhcl.weapon_arc_y;
        bacto->_weapon_cone_xy = vhcl.weapon_cone_xy;
        bacto->_mgun_spread_x = vhcl.mgun_spread_x;
        bacto->_mgun_spread_y = vhcl.mgun_spread_y;
        bacto->_fire_pos.x = vhcl.fire_x;
        bacto->_fire_pos.y = vhcl.fire_y;
        bacto->_fire_pos.z = vhcl.fire_z;
        bacto->_fire_x_mode = vhcl.fire_x_mode;
        bacto->_fire_x_start = vhcl.fire_x_start;
        bacto->_fire_x_step = vhcl.fire_x_step;
        bacto->_fire_x_slots = vhcl.fire_x_slots;
        bacto->_fire_x_advanced = vhcl.fire_x_advanced;
        bacto->_cockpit_camera_offset = vhcl.cockpit_camera_offset;
        bacto->_cockpit_gun_camera_recoil = vhcl.cockpit_gun_camera_recoil;
        bacto->_cockpit_camera_hide_vehicle = vhcl.cockpit_camera_hide_vehicle;
        bacto->_gun_angle = vhcl.gun_angle;
        bacto->_gun_angle_user = vhcl.gun_angle;
        int primaryMin = 1;
        int primaryMax = 1;
        vhcl.GetWeaponProjectileCountRange(0, primaryMin, primaryMax);
        bacto->_num_weapons = (uint8_t)primaryMin;
        bacto->_mgun_fire_energy_cost = vhcl.mgun_fire_energy_cost;
        bacto->_mgun_fire_energy_cost_defined = vhcl.mgun_fire_energy_cost_defined;
        bacto->_num_weapons_snd_events = (uint8_t)vhcl.num_weapons_snd_events;
        bacto->_weapon_projectile_counts[0] = (uint8_t)primaryMin;
        bacto->_weapon_projectile_count_maxs[0] = (uint8_t)primaryMax;
        for (size_t weaponSlot = 0; weaponSlot < vhcl.extra_num_weapons.size(); weaponSlot++)
        {
            int slotMin = 1;
            int slotMax = 1;
            vhcl.GetWeaponProjectileCountRange((int)weaponSlot + 1,
                                               slotMin, slotMax);
            bacto->_weapon_projectile_counts[weaponSlot + 1] = (uint8_t)slotMin;
            bacto->_weapon_projectile_count_maxs[weaponSlot + 1] = (uint8_t)slotMax;
        }
        bacto->_kill_after_shot = vhcl.kill_after_shot;
        bacto->_vp_normal = ResolveVisualModel(vhcl.vp_normal, vhcl.visual_3ds.normal, vhcl.visual_base.normal);
        bacto->_vp_fire = ResolveVisualModel(vhcl.vp_fire, vhcl.visual_3ds.fire, vhcl.visual_base.fire);

        // vp_dead is used exactly as authored: effect and invisible dead VPs
        // must not be replaced by the normal model.
        bacto->_vp_dead = ResolveVisualModel(vhcl.vp_dead, vhcl.visual_3ds.dead, vhcl.visual_base.dead);
        bacto->_vp_wait = ResolveVisualModel(vhcl.vp_wait, vhcl.visual_3ds.wait, vhcl.visual_base.wait);
        bacto->_vp_megadeth = ResolveVisualModel(vhcl.vp_megadeth, vhcl.visual_3ds.megadeth, vhcl.visual_base.megadeth);
        bacto->_vp_genesis = ResolveVisualModel(vhcl.vp_genesis, vhcl.visual_3ds.genesis, vhcl.visual_base.genesis);
        bacto->_vp_scale = vhcl.visual_scale;
        bacto->_vp_tint = vhcl.visual_tint;
        if ( requestedVhcl.is_mimic && !requestedVhcl.mimic_tint.IsNeutral() )
        {
            // Tint parameters are target-hue colorizers: a Mimic RGB tint must not
            // be contaminated by the copied vehicle's authored color/tint. Alpha
            // remains multiplicative so translucency/fade composition is preserved.
            if ( requestedVhcl.mimic_tint.ColorizesRGB() )
            {
                bacto->_vp_tint.r = requestedVhcl.mimic_tint.r;
                bacto->_vp_tint.g = requestedVhcl.mimic_tint.g;
                bacto->_vp_tint.b = requestedVhcl.mimic_tint.b;
            }
            bacto->_vp_tint.a *= requestedVhcl.mimic_tint.a;
            bacto->_vp_tint.Clamp();
        }
        bacto->_vp_rotation = vhcl.visual_rotation;
        if (!vhcl.collision_shape.empty() &&
            (bacto->_bact_type == BACT_TYPES_TANK || bacto->_bact_type == BACT_TYPES_CAR ||
             bacto->_bact_type == BACT_TYPES_BACT || bacto->_bact_type == BACT_TYPES_FLYER ||
             bacto->_bact_type == BACT_TYPES_UFO || bacto->_bact_type == BACT_TYPES_GUN ||
             bacto->_bact_type == BACT_TYPES_ROBO))
        {
            if (!_collisionScene) _collisionScene.reset(new Collision::Scene(*this));
            auto shape = _collisionScene->LoadShared(vhcl.collision_shape);
            if (shape &&
                (shape->visualScale - vhcl.visual_scale).length() < 1e-5 &&
                (shape->visualRotation - vhcl.visual_rotation).length() < 1e-5)
                bacto->_collisionShape = shape;
            else if (shape)
                ypa_log_out("WARNING: collision_shape %s does not match the visual scale/rotation; regenerate in Studio. Existing collision retained.\n", vhcl.collision_shape.c_str());
        }
        bacto->_vp_spin_strength = vhcl.visual_spin;
        bacto->_vp_trail_scale = vec3d(1.0, 1.0, 1.0);
        bacto->_vp_trail_tint = World::TVisualTint();
        bacto->_vp_trail_spin_strength = vec3d(0.0, 0.0, 0.0);
        bacto->_damaged_fx = vhcl.damaged_fx;
        bacto->_damaged_fx_next_time = 0;
        bacto->_decoration_fx = vhcl.decoration_fx;
        bacto->_decoration_fx_next_time = 0;
        bacto->_damaged_force_malus = vhcl.damaged_force_malus;
        bacto->_damaged_maxrot_malus = vhcl.damaged_maxrot_malus;
        bacto->_damaged_mgun_shot_time_malus = vhcl.damaged_mgun_shot_time_malus;
        bacto->_damaged_shot_time_malus = vhcl.damaged_shot_time_malus;
        bacto->_damaged_snd_pitch_multiplier = vhcl.damaged_snd_pitch_multiplier;
        bacto->_damaged_fx_active = false;
        bacto->_spawn_units = vhcl.spawn_units;
        bacto->_spawn_vehicle = vhcl.spawn_vehicle;
        bacto->_spawn_interval = vhcl.spawn_interval > 0 ? vhcl.spawn_interval : 5000;
        bacto->_spawn_trigger_radius = vhcl.spawn_trigger_radius > 0.0 ? vhcl.spawn_trigger_radius : 0.0;
        bacto->_spawn_random_pos = vhcl.spawn_random_pos > 0.0 ? vhcl.spawn_random_pos : 0.0;
        bacto->_spawn_offset = vhcl.spawn_offset;
        bacto->_spawn_max_active = vhcl.spawn_max_active > 0 ? vhcl.spawn_max_active : 0;
        bacto->_spawn_count = vhcl.spawn_count > 0 ? vhcl.spawn_count : 1;
        bacto->_spawn_instant = vhcl.spawn_instant ? 1 : 0;
        bacto->_spawn_last_time = 0;
        bacto->_at_death_spawn_units = deathProto.at_death_spawn_units;
        bacto->_at_death_spawn_vehicle = deathProto.at_death_spawn_vehicle;
        bacto->_at_death_spawn_count = deathProto.at_death_spawn_count > 0 ? deathProto.at_death_spawn_count : 1;
        if ( bacto->_at_death_spawn_count > 8 )
            bacto->_at_death_spawn_count = 8;
        bacto->_at_death_spawn_random_pos = deathProto.at_death_spawn_random_pos > 0.0 ? deathProto.at_death_spawn_random_pos : 0.0;
        bacto->_at_death_spawn_instant = deathProto.at_death_spawn_instant ? 1 : 0;
        bacto->_at_death_spawn_immunity_time = deathProto.at_death_spawn_immunity_time > 0 ? deathProto.at_death_spawn_immunity_time : 0;
        bacto->_at_death_spawn_done = false;
        bacto->_at_death_spawn_protection_end_time = 0;
        bacto->_at_death_spawn_restore_vulnerable = false;
        bacto->_at_death_push_force = deathProto.at_death_push_force > 0.0f
            ? deathProto.at_death_push_force : 0.0f;
        bacto->_at_death_push_radius = deathProto.at_death_push_radius > 0.0f
            ? deathProto.at_death_push_radius : 0.0f;
        bacto->_at_death_push_falloff = deathProto.at_death_push_falloff ? 1 : 0;
        bacto->_at_death_energy = deathProto.at_death_energy > 0
            ? deathProto.at_death_energy : 0;
        bacto->_at_death_energy_radius = deathProto.at_death_energy_radius > 0.0f
            ? deathProto.at_death_energy_radius : 0.0f;
        bacto->_at_death_energy_falloff = deathProto.at_death_energy_falloff ? 1 : 0;
        bacto->_carrier_spawn_root_gid = 0;
        bacto->_carrier_spawn_root_vehicle = 0;
        bacto->_carrier_spawned_gids.clear();
        bacto->_proximity_defense_enable = vhcl.proximity_defense_enable;
        bacto->_proximity_defense_weapon = vhcl.proximity_defense_weapon;
        bacto->_proximity_defense_trigger_radius = vhcl.proximity_defense_trigger_radius > 0.0 ? vhcl.proximity_defense_trigger_radius : 0.0;
        bacto->_proximity_defense_interval = vhcl.proximity_defense_interval > 0 ? vhcl.proximity_defense_interval : 1000;
        bacto->_proximity_defense_shots = vhcl.proximity_defense_shots > 0 ? vhcl.proximity_defense_shots : 1;
        bacto->_proximity_defense_vp_launch = vhcl.proximity_defense_vp_launch;
        bacto->_proximity_defense_3ds_launch = vhcl.proximity_defense_3ds_launch;
        bacto->_proximity_defense_base_launch = vhcl.proximity_defense_base_launch;
        bacto->_proximity_defense_fire_mode = vhcl.proximity_defense_fire_mode;
        bacto->_proximity_defense_sequence_delay = vhcl.proximity_defense_sequence_delay > 0 ? vhcl.proximity_defense_sequence_delay : 100;
        bacto->_proximity_defense_mode = vhcl.proximity_defense_mode;
        bacto->_proximity_defense_horizontal_angle_set = vhcl.proximity_defense_horizontal_angle_set;
        bacto->_proximity_defense_horizontal_angle_min = vhcl.proximity_defense_horizontal_angle_min;
        bacto->_proximity_defense_horizontal_angle_max = vhcl.proximity_defense_horizontal_angle_max;
        if ( bacto->_proximity_defense_horizontal_angle_min > bacto->_proximity_defense_horizontal_angle_max )
            std::swap(bacto->_proximity_defense_horizontal_angle_min, bacto->_proximity_defense_horizontal_angle_max);
        bacto->_proximity_defense_vertical_angle_set = vhcl.proximity_defense_vertical_angle_set;
        bacto->_proximity_defense_vertical_angle_min = vhcl.proximity_defense_vertical_angle_min;
        bacto->_proximity_defense_vertical_angle_max = vhcl.proximity_defense_vertical_angle_max;
        if ( bacto->_proximity_defense_vertical_angle_min > bacto->_proximity_defense_vertical_angle_max )
            std::swap(bacto->_proximity_defense_vertical_angle_min, bacto->_proximity_defense_vertical_angle_max);
        bacto->_proximity_defense_sequence_active = false;
        bacto->_proximity_defense_sequence_shots_fired = 0;
        bacto->_proximity_defense_next_shot_time = 0;
        bacto->_proximity_defense_next_activation_time = 0;
        bacto->_proximity_defense_at_death_done = false;
        bacto->_kamikaze_triggered = false;
        bacto->_kamikaze_ground_decals_handled = false;
        bacto->SetUnitGuns(vhcl_id->skip_unit_guns ? std::vector<World::TRoboGun>() : vhcl.unit_guns);

        bacto->_destroyFX = vhcl.dest_fx;
        bacto->_extDestroyFX = vhcl.ExtDestroyFX;
        bacto->_chainFX = vhcl.chain_fx;

        for (NC_STACK_base *& vp_fx : bacto->_vp_fx_models)
            vp_fx = nullptr;

        bacto->_scale_start = vhcl.scale_fx_p0;
        bacto->_scale_speed = vhcl.scale_fx_p1;
        bacto->_scale_accel = vhcl.scale_fx_p2;
        bacto->_scale_duration = vhcl.scale_fx_p3;
        bacto->_scale_pos = 0;

        bacto->_hidden = vhcl.hidden;
        bacto->_unhideRadar = vhcl.unhideRadar;

        // Buff invisibility is seeded once at spawn and remains active until the
        // first real attack reveals the unit. Missing/disabled Buff stays vanilla.
        bacto->_invisibleUnrevealed = vhcl.buff.valid && vhcl.buff.invisible;
        bacto->_buff_invisible_reveal_vp = vhcl.buff.invisible_reveal_vp;
        bacto->_buff_invisible_reveal_3ds = vhcl.buff.invisible_reveal_3ds;
        bacto->_buff_invisible_reveal_base = vhcl.buff.invisible_reveal_base;

        for (int i = 0; vhcl.scale_fx_pXX[ i ]; i++ )
        {
            bacto->_vp_fx_models[i] = _vhclModels.at( vhcl.scale_fx_pXX[ i ] );

            bacto->_status_flg |= BACT_STFLAG_SEFFECT;
        }

        bacto->_soundcarrier.Resize(vhcl.sndFX.size());

        for (World::TVhclSound &sfx : vhcl.sndFX)
            sfx.LoadSamples();

        for (size_t i = 0; i < vhcl.sndFX.size(); i++)
        {
            TSoundSource *smpl_inf = &bacto->_soundcarrier.Sounds[ i ];

            smpl_inf->Volume = vhcl.sndFX[i].volume;

            if ( i == World::TVhclProto::SND_GENESIS && vhcl.sndFX[i].auto_pitch )
            {
                const int genesisPitch = yw_GetAutomaticGenesisPitch(vhcl.energy);
                smpl_inf->ConfigurePitchRange(genesisPitch, genesisPitch);
            }
            else
            {
                vhcl.sndFX[i].ConfigureSoundSourcePitch(*smpl_inf);
            }

            smpl_inf->Radius = vhcl.sndFX[i].radius;
            smpl_inf->PriorityBias = (i == World::TVhclProto::SND_COCKPIT) ? 4096 : 0;

            if ( World::TVhclProto::IsLoopingSnd(i) )
                smpl_inf->SetLoop(true);

            if ( vhcl.sndFX[i].MainSample.Sample )
                smpl_inf->PSample = vhcl.sndFX[i].MainSample.Sample->GetSampleData();
            else
                smpl_inf->PSample = 0;

            if ( vhcl.sndFX[i].sndPrm.slot )
            {
                smpl_inf->PPFx = &vhcl.sndFX[i].sndPrm;
                smpl_inf->SetPFx(true);
            }
            else
            {
                smpl_inf->SetPFx(false);
            }

            if ( vhcl.sndFX[i].sndPrm_shk.slot )
            {
                smpl_inf->PShkFx = &vhcl.sndFX[i].sndPrm_shk;
                smpl_inf->SetShk(true);
            }
            else
            {
                smpl_inf->SetShk(false);
            }

            if ( !vhcl.sndFX[i].extS.empty() )
            {
                smpl_inf->PFragments = &vhcl.sndFX[i].extS; //CHECK IT
                smpl_inf->SetFragmented(true);
            }
            else
            {
                smpl_inf->SetFragmented(false);
            }
        }

        // Pickup audio stays positional on the collecting unit. Nucleus.ini can
        // override its sample globally; empty or unloadable paths fall back to
        // the vanilla World.ini plasma sample.
        TSoundSource &pickup = bacto->_soundcarrier.Sounds[World::TVhclProto::SND_PICKUP];
        if ( !pickup.PSample && _GameShell &&
             World::SOUND_ID_PLASMA < _GameShell->samples1_info.Sounds.size() )
        {
            TSoundSource &legacyPickup =
                _GameShell->samples1_info.Sounds[World::SOUND_ID_PLASMA];
            pickup.PSample = legacyPickup.PSample;
        }

        bacto->_volume = bacto->_soundcarrier.Sounds[0].Volume;

        bacto->_mimic_soundcarrier.Clear();
        if ( requestedVhcl.is_mimic )
        {
            requestedVhcl.snd_mimic.LoadSamples();
            if ( requestedVhcl.snd_mimic.MainSample.Sample )
            {
                bacto->_mimic_soundcarrier.Resize(1);

                TSoundSource &snd = bacto->_mimic_soundcarrier.Sounds[0];
                snd.PSample = requestedVhcl.snd_mimic.MainSample.Sample->GetSampleData();
                snd.Volume = requestedVhcl.snd_mimic.volume;
                requestedVhcl.snd_mimic.ConfigureSoundSourcePitch(snd);
                snd.PriorityBias = 0;
                snd.SetLoop(true);
                snd.SetPFx(false);
                snd.SetShk(false);
                snd.SetFragmented(false);
            }
        }

        bacto->SetParameters(vhcl.initParams);

        bact_arg80 arg80;
        arg80.pos = vhcl_id->pos;
        arg80.field_C = 0;
        bacto->SetPosition(&arg80);

        setState_msg arg119;
        arg119.newStatus = BACT_STATUS_NORMAL;
        arg119.setFlags = 0;
        arg119.unsetFlags = 0;
        bacto->SetStateInternal(&arg119);
    }

    if (_collisionScene) _collisionScene->UpdateActor(bacto);
    return bacto;
}

NC_STACK_ypamissile * NC_STACK_ypaworld::ypaworld_func147(ypaworld_arg146 *arg)
{
    if ( !arg || arg->vehicle_id >= _weaponProtos.size() )
        return nullptr;

    World::TWeapProto &wproto = _weaponProtos.at(arg->vehicle_id);

    if ( !(wproto._weaponFlags & 1) )
        return nullptr;

    NC_STACK_ypamissile *wobj = dynamic_cast<NC_STACK_ypamissile *>( yw_createUnit(wproto.unitID) );

    if ( !wobj )
        return nullptr;

    wobj->_energy = wproto.energy;
    wobj->_energy_max = wproto.energy;
    wobj->_shield = 0;
    wobj->_mass = wproto.mass;
    wobj->_force = wproto.force;
    wobj->_base_force = wproto.force;    // OpenNeoUA fix: ypabact_ApplyDamagedRuntime() recomputes
    wobj->_maxrot = wproto.maxrot;       // _force = _base_force * mult every frame, so missiles must seed
    wobj->_base_maxrot = wproto.maxrot;  // _base_force/_base_maxrot too, or they crawl at the 5000/0.5 defaults.
    wobj->_height = wproto.heightStd;
    wobj->_radius = wproto.radius;
    wobj->ApplyCompoundCollision(wproto.coll, wproto.radius_defined);
    wobj->_viewer_radius = wproto.vwr_radius;
    wobj->_overeof = wproto.overeof;
    wobj->_viewer_overeof = wproto.vwr_overeof;
    wobj->_airconst = wproto.airconst;
    wobj->_airconst_static = wproto.airconst;
    wobj->_adist_sector = wproto.adistSector;
    wobj->_adist_bact = wproto.adistBact;
    wobj->_vehicleID = arg->vehicle_id;
    wobj->_weapon = 0;

    wobj->_vp_normal = ResolveVisualModel(wproto.vp_normal, wproto.visual_3ds.normal, wproto.visual_base.normal);
    wobj->_vp_fire = ResolveVisualModel(wproto.vp_fire, wproto.visual_3ds.fire, wproto.visual_base.fire);
    wobj->_vp_dead = ResolveVisualModel(wproto.vp_dead, wproto.visual_3ds.dead, wproto.visual_base.dead);
    wobj->_vp_wait = ResolveVisualModel(wproto.vp_wait, wproto.visual_3ds.wait, wproto.visual_base.wait);
    wobj->_vp_megadeth = ResolveVisualModel(wproto.vp_megadeth, wproto.visual_3ds.megadeth, wproto.visual_base.megadeth);
    wobj->_vp_genesis = ResolveVisualModel(wproto.vp_genesis, wproto.visual_3ds.genesis, wproto.visual_base.genesis);
    wobj->_vp_scale = wproto.visual_scale;
    wobj->_vp_tint = wproto.visual_tint;
    wobj->_vp_rotation = wproto.visual_rotation;
    wobj->_vp_spin_strength = wproto.visual_spin;
    if (!wproto.collision_shape.empty())
    {
        if (!_collisionScene) _collisionScene.reset(new Collision::Scene(*this));
        auto shape = _collisionScene->LoadShared(wproto.collision_shape);
        if (shape &&
            (shape->visualScale - wproto.visual_scale).length() < 1e-5 &&
            (shape->visualRotation - wproto.visual_rotation).length() < 1e-5)
            wobj->_collisionShape = shape;
        else if (shape)
            ypa_log_out("WARNING: collision_shape %s does not match the visual scale/rotation; regenerate in Studio. Existing collision retained.\n", wproto.collision_shape.c_str());
    }
    const bool supportsProjectileVisualMotion = wproto.SupportsProjectileVisualMotion();
    wobj->_spiral_speed = supportsProjectileVisualMotion ? wproto.spiral_speed : 0.0f;
    wobj->_spiral_radius = supportsProjectileVisualMotion ? wproto.spiral_radius : 0.0f;
    wobj->_chaos_factor = supportsProjectileVisualMotion ? wproto.chaos_factor : 0.0f;
    wobj->_chaos_radius = supportsProjectileVisualMotion ? wproto.chaos_radius : 0.0f;
    wobj->ResetProjectileVisualMotionFreeze();
    wobj->_vp_trail_scale = wproto.vp_trail_scale;
    wobj->_vp_trail_tint = wproto.vp_trail_tint;
    wobj->_vp_trail_spin_strength = wproto.vp_trail_spin;
    wobj->ConfigureWeaponTracer(wproto.tracer,
                                wproto.SupportsProjectileTracer());

    wobj->_destroyFX = wproto.dfx;
    wobj->_extDestroyFX = wproto.ExtDestroyFX;
    wobj->_chainFX = wproto.chain_fx;
    wobj->_decoration_fx = wproto.decoration_fx;
    wobj->_decoration_fx_next_time = 0;

    int missileType;

    switch(wproto._weaponFlags)
    {
    case World::TWeapProto::WEAPON_FLAGS_BOMB:
    case World::TWeapProto::WEAPON_FLAGS_HOMING_BOMB:
        missileType = NC_STACK_ypamissile::MISL_BOMB;
        break;

    case World::TWeapProto::WEAPON_FLAGS_MISSILE:
        missileType = NC_STACK_ypamissile::MISL_TARGETED;
        break;

    case World::TWeapProto::WEAPON_FLAGS_OBSAVOID:
        missileType = NC_STACK_ypamissile::MISL_OBSAVOID;
        break;

    case World::TWeapProto::WEAPON_FLAGS_GRENADE:
        missileType = NC_STACK_ypamissile::MISL_GRENADE;
        break;

    case World::TWeapProto::WEAPON_FLAGS_ARC_GRENADE:
        missileType = NC_STACK_ypamissile::MISL_ARC_GRENADE;
        break;

    default:
        missileType = NC_STACK_ypamissile::MISL_DIRECT;
        break;
    }

    // Resolve an authored fixed lifetime or min_max range once per physical
    // projectile. The prototype keeps the range; the missile owns the result.
    wobj->SetLifeTime(wproto.RollLifeTime());
    wobj->SetDelay(wproto.delay_time);
    wobj->SetDriveTime(wproto.drive_time);
    wobj->SetHomingTime(
        wproto._weaponFlags == World::TWeapProto::WEAPON_FLAGS_MISSILE || wproto.IsHomingBomb()
            ? wproto.homing_time : 0);
    wobj->SetMissileType(missileType);
    wobj->SetPowerHeli(wproto.energy_heli * 1000.0);
    wobj->SetPowerTank(wproto.energy_tank * 1000.0);
    wobj->SetPowerFlyer(wproto.energy_flyer * 1000.0);
    wobj->SetPowerRobo(wproto.energy_robo * 1000.0);
    wobj->ConfigureSpecificEnergyMultipliers(wproto);
    wobj->SetRadiusHeli(wproto.radius_heli);
    wobj->SetRadiusTank(wproto.radius_tank);
    wobj->SetRadiusFlyer(wproto.radius_flyer);
    wobj->SetRadiusRobo(wproto.radius_robo);
    wobj->SetBlast(wproto.blast_radius, wproto.blast_energy, wproto.blast_falloff);
    wobj->SetBlastPush(wproto.blast_push);
    wobj->SetDirectPush(wproto.push);
    wobj->SetArmorPenetrationTargets(wproto.armor_penetration_targets);

    wobj->SetWeaponSoundEventsEnabled(arg->weapon_sound_events_enabled);
    wobj->_soundcarrier.Resize(wproto.sndFXes.size());

    if ( wobj->WeaponSoundEventsEnabled() )
    {
        for (World::TVhclSound &sfx : wproto.sndFXes)
            sfx.LoadSamples();

        for (size_t i = 0; i < wproto.sndFXes.size(); i++)
        {
            TSoundSource *v25 = &wobj->_soundcarrier.Sounds[i];

            v25->Volume = wproto.sndFXes[i].volume;
            wproto.sndFXes[i].ConfigureSoundSourcePitch(*v25);
            v25->Radius = wproto.sndFXes[i].radius;

            if ( i == 0 )
                v25->SetLoop(true);

            if ( wproto.sndFXes[i].MainSample.Sample )
                v25->PSample =
                    wproto.sndFXes[i].MainSample.Sample->GetSampleData();
            else
                v25->PSample = 0;

            if ( wproto.sndFXes[i].sndPrm.slot )
            {
                v25->SetPFx(true);
                v25->PPFx = &wproto.sndFXes[i].sndPrm;
            }
            else
            {
                v25->SetPFx(false);
            }

            if ( wproto.sndFXes[i].sndPrm_shk.slot )
            {
                v25->SetShk(true);
                v25->PShkFx = &wproto.sndFXes[i].sndPrm_shk;
            }
            else
            {
                v25->SetShk(false);
            }

            if ( !wproto.sndFXes[i].extS.empty() )
            {
                v25->SetFragmented(true);
                v25->PFragments = &wproto.sndFXes[i].extS;
            }
            else
            {
                v25->SetFragmented(false);
            }
        }
    }

    // Debuff tick audio is a separate status-effect system, not one of the
    // projectile Weapon SND_NORMAL/SND_LAUNCH/SND_HIT packages.
    if ( wproto.debuff.valid )
        wproto.debuff.tick_snd.LoadSamples();
    if ( wproto.delay_time_debuff.valid )
        wproto.delay_time_debuff.tick_snd.LoadSamples();

    wobj->SetParameters(wproto.initParams);

    bact_arg80 arg80;

    arg80.pos = arg->pos;
    arg80.field_C = 1;

    wobj->SetPosition(&arg80);

    setState_msg arg119;

    arg119.setFlags = 0;
    arg119.unsetFlags = 0;
    arg119.newStatus = BACT_STATUS_NORMAL;

    wobj->SetStateInternal(&arg119);

    return wobj;
}

size_t NC_STACK_ypaworld::ypaworld_func148(ypaworld_arg148 *arg)
{
    if (  !arg->field_C
       && !_allowMultiBuildLevel
       && IsAnyBuildingProcess(arg->owner))
            return false;

    cellArea &cell = _cells(arg->CellId);

    bool UserInSec = false;

    for ( NC_STACK_ypabact* &node : cell.unitsList )
    {

        if ( _userUnit == node || node->_bact_type == BACT_TYPES_ROBO)
        {
            UserInSec = true;
            break;
        }
    }

    if ( _userUnit  &&  &cell == _userUnit->_pSector )
        UserInSec = true;

    if (cell.IsBorder())
        return 0;

    if ( cell.PurposeType == cellArea::PT_CONSTRUCTING )
        return 0;
    else if ( UserInSec  && !arg->field_C )
        return 0;
    else if ( cell.PurposeType == cellArea::PT_POWERSTATION )
        PowerStationErase(&cell);
    else if ( (cell.PurposeType == cellArea::PT_TECHUPGRADE || cell.PurposeType == cellArea::PT_GATECLOSED || cell.PurposeType == cellArea::PT_GATEOPENED) && !arg->field_C )
        return 0;
    else if ( cell.PurposeType == cellArea::PT_TECHDEACTIVE && _isNetGame )
        return 0;

    if ( arg->field_C )
    {
        sb_0x456384(arg->CellId, arg->owner, arg->blg_ID, arg->field_18 & 1);
    }
    else
    {
        DestroyAllGunsInSector(&cell);

        if ( !BuildingConstructBegin(&cell, arg->blg_ID, arg->owner, World::CVBuildConstructTime) )
            return 0;
    }

    return 1;
}

void NC_STACK_ypaworld::ypaworld_func149(ypaworld_arg136 *arg)
{
    arg->tVal = 2.0;
    arg->isect = 0;

    vec3d stpos = arg->stPos;

    int v33 = (stpos.x + 150.0) / 75.0 * 16384.0;
    int v34 = (stpos.z - 150.0) / 75.0 * 16384.0;

    float v36 = fabs(arg->vect.x);
    float v39 = fabs(arg->vect.z);

    int v31;
    int v32;
    float v37;

    if ( v36 != 0.0 || v39 != 0.0 )
    {
        if ( v36 <= v39 )
        {
            v32 = (v36 / v39 * 16384.0);

            if ( arg->vect.x < 0.0 )
                v32 = -v32;

            if ( arg->vect.z >= 0.0 )
                v31 = 16384;
            else
                v31 = -16384;

            v37 = v39;
        }
        else
        {
            if ( arg->vect.x >= 0.0 )
                v32 = 16384;
            else
                v32 = -16384;

            v31 = (v39 / v36 * 16384.0);

            if ( arg->vect.z < 0.0 )
                v31 = -v31;

            v37 = v36;
        }
    }
    else
    {
        v31 = 0;
        v37 = 0.0;
        v32 = 0;
    }

    int a2a = v33 >> 16;
    int a3a = -v34 >> 16;

    TSectorCollision a6;
    int v10, v11;

    do // Don't like this :E
    {
        v10 = v33 >> 16;
        v11 = -v34 >> 16;

        a6 = sub_44DBF8(a2a, a3a, v10, v11, arg->flags);

        if ( a6.CollisionType )
        {
            if ( a6.CollisionType != 1 )
                sub_44E07C(a6);

            arg->stPos = stpos - a6.pos;

            sub_44D8B8(arg, a6);

            if ( arg->isect )
                return;
        }

        do // Don't like this :E
        {
            v37 -= 75.0;
            v33 += v32;
            v34 += v31;
        }
        while ( v33 >> 16 == v10 && -v34 >> 16 == v11 && v37 > 0.0 );

    }
    while( v37 > 0.0 );

    int v24 = ((int)((arg->vect.x + stpos.x + 150.0) / 75.0 * 16384.0)) >> 16;
    int v27 = ((int)((arg->vect.z + stpos.z - 150.0) / 75.0 * 16384.0)) >> 16;

    if ( v24 != v10 || -v27 != v11 )
    {
        a6 = sub_44DBF8(a2a, a3a, v24, -v27, arg->flags);

        if ( a6.CollisionType )
        {
            if ( a6.CollisionType != 1 )
                sub_44E07C(a6);

            arg->stPos = stpos - a6.pos;

            sub_44D8B8(arg, a6);
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func150(yw_arg150 *arg)
{
    arg->field_24 = nullptr;

    int v6 = arg->pos.x / 300.0 * 16384.0;
    int v7 = arg->pos.z / 300.0 * 16384.0;

    float v47 = fabs(arg->field_18.x);
    float v46 = fabs(arg->field_18.z);

    arg->field_28 = arg->field_18.length();

    int v27, v28, v35;

    if ( v47 != 0.0 || v46 != 0.0 )
    {
        if ( v47 <= v46 )
        {
            v27 = (v47 * 16384.0 / v46);

            if ( arg->field_18.x < 0.0 )
                v27 = -v27;

            if ( arg->field_18.z >= 0.0 )
                v28 = 16384;
            else
                v28 = -16384;

            v35 = v46;
        }
        else
        {
            if ( arg->field_18.x >= 0.0 )
                v27 = 16384;
            else
                v27 = -16384;

            v28 = (v46 * 16384.0 / v47);

            if ( arg->field_18.z < 0.0 )
                v28 = -v28;

            v35 = v47;
        }
    }
    else
    {
        v28 = 0;
        v35 = 0;
        v27 = 0;
    }

    vec3d v41 = arg->field_18 / arg->field_28;

    while ( v35 >= 0 )
    {
        int v12 = -v7 >> 16;
        int v29 = v6 >> 16;

        if ( IsSector( {v29, v12} ) )
        {
            for ( NC_STACK_ypabact* &sect_bacts : _cells(v29, v12).unitsList )
            {
                if ( sect_bacts != arg->unit && sect_bacts->_status != BACT_STATUS_DEAD && !sect_bacts->_isDummy )
                {
                    if ( !(arg->unit == _userUnit && _playerInHSGun) || sect_bacts != _userRobo )
                    {
                        vec3d v36 = sect_bacts->_position - arg->pos;
                        vec3d v16 = v41 * v36;

                        if ( v16.length() < sect_bacts->_radius )
                        {
                            float v30 = v36.length();

                            if ( v41.dot(v36) / v30 > 0.0 )
                            {
                                if ( arg->field_28 > v30 - sect_bacts->_radius )
                                {
                                    arg->field_24 = sect_bacts;
                                    arg->field_28 = v30 - sect_bacts->_radius;
                                }
                            }
                        }
                    }
                }
            }
        }

        if ( arg->field_24 )
            break;

        do
        {
            v6 += v27;
            v7 += v28;
            v35 -= 300;
        }
        while ( (v6 >> 16) == v29 && (-v7 >> 16) == v12 && v35 >= 0 );
    }
}

void NC_STACK_ypaworld::DeleteLevel()
{
    _collisionScene.reset();
    // Only victory banks this run, before the existing profile save and teardown.
    CommitPlasmaCurrency();
    _missionMapStatusSnapshotValid = false;
    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED && !_isNetGame )
    {
        for ( size_t i = 0; i < _globalMapRegions.MapRegions.size(); i++ )
            _missionMapStatusSnapshot[i] = _globalMapRegions.MapRegions[i].Status;

        _missionMapStatusSnapshotLevel = _levelInfo.LevelID;
        _missionMapStatusSnapshotValid = true;
    }

    EnableLevelPasses();

    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        _prepareDebrief = true;
        if ( _firstContactFaction )
            _levelInfo.JodieFoster[ _firstContactFaction ] = 1;
    }
    else
    {
        _prepareDebrief = false;
    }

    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        if ( _GameShell )
        {
            FSMgr::FileHandle *fil = uaOpenFileAlloc(fmt::sprintf("save:%s/sgisold.txt", _GameShell->UserName), "w");

            if ( fil )
                delete fil;

            SaveSettings(_GameShell, fmt::sprintf("%s/user.txt", _GameShell->UserName), World::SDF_ALL);

            fil = uaOpenFileAlloc("env:user.def", "w");

            if ( fil )
            {
                fil->printf("%s", _GameShell->UserName.c_str());
                delete fil;
            }
        }
    }

    if ( _isNetGame )
    {
        if ( !_GameShell->sentAQ )
            NetSendExitMsg(0);

        _GameShell->ypaworld_func151__sub7();
        _GameShell->yw_netcleanup();

        _prepareDebrief = true;
        _gameWasNetGame = true;

        if ( _userUnit )
            _userOwnerIdWasInNetGame = _userUnit->_owner;
    }
    else
    {
        _userOwnerIdWasInNetGame = 0;
        _gameWasNetGame = false;
    }

    if ( _replayRecorder->do_record )
        recorder_stoprec(this);

    _screenShotSeq = false;
    _replayRecorder->do_record = 0;

    NC_STACK_bitmap *disk = loadDisk_screen(this);

    if ( disk )
    {
        draw_splashScreen(this, disk);
        disk->Delete();
    }

    //ypaworld_func151__sub5(this); Free map events
    _voiceMessage.Reset();

    SFXEngine::SFXe.setMasterVolume(audio_volume);

    GUI_Close();

    if ( _skyObject )
    {
        _skyObject->Delete();
        _skyObject = nullptr;
    }

    int plowner;
    if ( _userRobo )
        plowner = _userRobo->_owner;
    else
        plowner = 0;

    BeginLevelTeardown();

    while ( !_deadCacheList.empty() )
    {
        NC_STACK_ypabact *bct = _deadCacheList.front();

        if ( _gameWasNetGame )
        {
            if ( plowner != bct->_owner && bct->_bact_type == BACT_TYPES_ROBO )
                NetRemove(bct);
        }

        bct->Delete();
    }

    while ( !_unitsList.empty() )
    {
        NC_STACK_ypabact *bct = _unitsList.front();

        if ( _gameWasNetGame )
        {
            if ( plowner != bct->_owner && bct->_bact_type == BACT_TYPES_ROBO )
                NetRemove(bct);
        }

        bct->Delete();
    }

    // NetRemove can fill deadcache, so clean it again
    while ( !_deadCacheList.empty() )
        _deadCacheList.front()->Delete();

    _transientVPs.clear();
    _nextTransientVPId = 1;
    _damageHoverTargets.clear();
    ProtosFreeSounds();

    sb_0x44ac24(this);

    _powerStations.clear();
    _energyAccumMap.Clear();

    _lvlTypeMap.Clear();
    _lvlOwnMap.Clear();
    _lvlBuildingsMap.Clear();
    _lvlHeightMap.Clear();

    if ( _GameShell )
        _GameShell->samples1_info.Position = vec3d();

    if ( _gameWasNetGame )
    {
        LoadProtosScript(_initScriptFilePath);
    }

    EndLevelTeardown();
}

void NC_STACK_ypaworld::BeginLevelTeardown()
{
    _levelTeardownInProgress = true;
    _debugGameplaySlowMotion = false;
    _debugGameplayFastMotion = false;
    _debugGameplayFrozen = false;
    _debugHostStationCheat = false;
    _debugGlobalInvulnerability = false;
    ResetPlasmaCurrencyRuntime();
    StopAmbientLevelSound();
    StopAtmosphericFXLoopSound();
    ClearAtmosphericFXRuntime();
    ClearMobilePowerFXRuntime();
    _atmosphericFXProfile = World::TAtmosphericFXProfile();
    ClearSuperItemRuntime();

    // NC_STACK_ypaworld is reused by restart/load/menu transitions. Invalidate
    // every mission-owned reference before the first BACT is destroyed so UI,
    // debug and delayed runtime state cannot observe the outgoing hierarchy.
    _particles.Clear();
    Common::DeleteAndNull(&_script);
    _transientVPs.clear();
    _nextTransientVPId = 1;
    ClearMinigunTracers();
    ClearProceduralEnergyFX();
    ClearGroundDecals();
    _damageHoverTargets.clear();
    _inBuildProcess.clear();
    _debugBlastRings.clear();
    ClearArtilleryShellMarkers();
    _artilleryShellManualGid = 0;
    _artilleryShellManualRadius = 0.0f;

    _hudMissileMultiLockTargets.clear();
    _cmdrsRemap.clear();
    _spectatorFollowTarget = nullptr;
    _lastMsgSender = nullptr;
    _viewerBact = nullptr;
    _userRobo = nullptr;
    _userUnit = nullptr;
    ResetPlayerSprint();
    _bactOnMouse = nullptr;
    _bactPrevClicked = nullptr;
    _cellOnMouse = nullptr;
    _guiDragElement = nullptr;
    _guiDragging = false;
    _guiDragDefaultMouse = false;
    _mouseGrabbed = false;
    _guiActFlags = 0;
    _doAction = World::DOACTION_0;
    _guiVisor = bact_hudi();
    _updateMessage = update_msg();

    _vehicleTakenControlTimestamp = 0;
    _vehicleTakenCommandId = 0;
    _activeCmdrID = 0;
    _activeCmdrRemapIndex = 0;
    _activeCmdrKidsCount = 0;
    _cmdrIdToSelect = -1;
    _makingWaypointsMode = false;
    _waypointCount = 0;
    _playerInHSGun = false;
    _upgradeId = 0;
    _upgradeTimeStamp = 0;
    _roboDeathTimeScaleEndTime = 0;
    _kamikazeFireTimeScaleDrainGid = 0;
    _kamikazeFireTimeScaleHpDrainRemainder = 0.0;
    _kamikazeFireInputWasHeld = false;
    System::GameClock.Reset(_timeStamp);
    _upgradeVehicleId = 0;
    _upgradeWeaponId = 0;
    _upgradeBuildId = 0;
    ClearGemNotificationCapture();
    _gemUnlockSoundAttemptedPath.clear();
}

void NC_STACK_ypaworld::EndLevelTeardown()
{
    _levelTeardownInProgress = false;
}

bool NC_STACK_ypaworld::IsLevelTeardownInProgress() const
{
    return _levelTeardownInProgress;
}

void NC_STACK_ypaworld::ypaworld_func153(bact_hudi *arg)
{
    if ( !arg->field_18 )
        _hudMissileMultiLockTargets.clear();

    _guiVisor = *arg;
}

void NC_STACK_ypaworld::NoteUserDamageHover(NC_STACK_ypabact *attacker, NC_STACK_ypabact *target)
{
    if ( !attacker || !target || attacker != _userUnit || attacker != _viewerBact || !attacker->getBACT_inputting() )
        return;

    if ( target == attacker ||
         target->_bact_type == BACT_TYPES_MISSLE ||
         target->_status == BACT_STATUS_DEAD ||
         target->_status == BACT_STATUS_CREATE ||
         target->_status == BACT_STATUS_BEAM ||
         target->ShouldHideFromStrategicUI() )
        return;

    int32_t hoverUntil = _timeStamp + 800;
    for (TDamageHoverTarget &hover : _damageHoverTargets)
    {
        if ( hover.target == target )
        {
            hover.until = hoverUntil;
            return;
        }
    }

    TDamageHoverTarget hover;
    hover.target = target;
    hover.until = hoverUntil;
    _damageHoverTargets.push_back(hover);
}

std::vector<NC_STACK_ypabact *> NC_STACK_ypaworld::GetUserDamageHoverTargets()
{
    std::vector<NC_STACK_ypabact *> targets;

    for (auto it = _damageHoverTargets.begin(); it != _damageHoverTargets.end(); )
    {
        NC_STACK_ypabact *target = it->target;
        if ( !target ||
             _timeStamp > it->until ||
             target->_status == BACT_STATUS_DEAD ||
             target->_status == BACT_STATUS_CREATE ||
             target->_status == BACT_STATUS_BEAM ||
             target->ShouldHideFromStrategicUI() )
        {
            it = _damageHoverTargets.erase(it);
            continue;
        }

        targets.push_back(target);
        ++it;
    }

    return targets;
}

void NC_STACK_ypaworld::ClearUserDamageHoverTarget(NC_STACK_ypabact *target)
{
    if ( !target )
        return;

    for (auto it = _damageHoverTargets.begin(); it != _damageHoverTargets.end(); )
    {
        if ( it->target == target )
            it = _damageHoverTargets.erase(it);
        else
            ++it;
    }
}

void NC_STACK_ypaworld::ypaworld_func176(yw_arg176 *arg)
{
    arg->field_4 = _reloadRatioClamped[arg->owner];
    arg->field_8 = _reloadRatioPositive[arg->owner];
}

void NC_STACK_ypaworld::ypaworld_func177(yw_arg177 *arg)
{
    //Reown sectors for new owner

    if ( !arg->field_4 ) //New owner
        return;

    if ( arg->bact == _userRobo )
        _playerHSDestroyed = true;

    if ( _userRobo )
    {
        if ( _userRobo->_owner == arg->field_4 )
            _beamEnergyCapacity += _beamEnergyAdd;
    }

    for ( NC_STACK_ypabact* &unit : _unitsList )
    {
        if ( unit &&
             unit->_bact_type == BACT_TYPES_ROBO &&
             unit != arg->bact &&
             arg->bact->_owner == unit->_owner &&
             unit->_energy > 0 &&
             unit->_status != BACT_STATUS_DEAD &&
             !(unit->_status_flg &
               (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) )
            return;
    }

    for (int i = 0; i < _mapSize.y; i++)
    {
        for (int j = 0; j < _mapSize.x; j++)
        {
            cellArea &v9 = _cells(j, i);

            if ( v9.owner == arg->bact->_owner )
                CellSetOwner(&v9, arg->field_4);
        }
    }

    if ( !_userRobo )
        return;

    if ( _userRobo->_owner != arg->field_4 )
        return;

    for (int i = 0; i < _mapSize.y; i++)
    {
        for (int j = 0; j < _mapSize.x; j++)
        {
            cellArea &v15 = _cells(j, i);

            if ( v15.PurposeType == cellArea::PT_TECHUPGRADE && v15.owner == _userRobo->_owner )
            {

                if ( _isNetGame )
                    sub_47C29C(this, &v15, v15.PurposeIndex);
                else
                    yw_ActivateWunderstein(&v15, v15.PurposeIndex);

                HistoryEventAdd( World::History::Upgrade(j, i, v15.owner, _techUpgrades[ _upgradeId ].Type, _upgradeVehicleId, _upgradeWeaponId, _upgradeBuildId) );
            }

        }
    }
}

//179 method in yw_net


void NC_STACK_ypaworld::ypaworld_func180(yw_arg180 *arg)
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }

    switch ( arg->effects_type )
    {
    case 0:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_MISSILEFIRE);
        break;

    case 1:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_GRENADEFIRE);
        break;

    case 2:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_BOMBFIRE);
        break;

    case 3:
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_MINIGUN);
        break;

    case 4:
        Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_MINIGUN);
        break;

    case 5:
    {
        NC_STACK_ypabact *bct = _userUnit;
        Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_COLLISION,
            arg->field_4, 0.0,
            (arg->field_C - bct->_position.z) * bct->_rotation.m02 + (arg->field_8 - bct->_position.x) * bct->_rotation.m00,
            -((arg->field_8 - bct->_position.x) * bct->_rotation.m20 + (arg->field_C - bct->_position.z) * bct->_rotation.m22));

    }
    break;

    default:
        break;
    }
}

bool NC_STACK_ypaworld::NetSendMessage(uamessage_base *data, size_t dataSize, const std::string &recvID, bool garantee)
{
    if (_GameShell->noSent)
        return false;

    return _netDriver->Send(data, dataSize, recvID, garantee);
}

bool NC_STACK_ypaworld::NetBroadcastMessage(uamessage_base *data, size_t dataSize, bool garantee)
{
    if (_GameShell->noSent)
        return false;

    if ( _GameShell->netPlayerOwner != 0 && data->msgID != UAMSG_VHCLENERGY )
        _GameShell->msgcount++;

    return _netDriver->Broadcast(data, dataSize, garantee);
}

int ypaworld_func183__sub0(int lvlID, const char *userName)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc( fmt::sprintf("save:%s/%d.fin", userName, lvlID) , "r");

    if ( !fil )
        return 0;

    delete fil;
    return 1;
}

// Advanced Create Level
size_t NC_STACK_ypaworld::ypaworld_func183(yw_arg161 *arg)
{
    if ( !arg || arg->lvlID < 0 ||
            (size_t)arg->lvlID >= _globalMapRegions.MapRegions.size() )
        return 0;

    int v6;

    if ( !arg->freshStart &&
         _globalMapRegions.MapRegions[ arg->lvlID ].Status == TMapRegionInfo::STATUS_COMPLETED &&
         ypaworld_func183__sub0(arg->lvlID, _GameShell->UserName.c_str()) )
    {
        std::string savename = fmt::sprintf("save:%s/%d.fin", _GameShell->UserName, arg->lvlID);
        v6 = LoadGame(savename);

        if ( !v6 )
        {
            ypa_log_out("Warning: in YWM_ADVANCEDCREATELEVEL: YWM_LOADGAME of %s failed!\n", savename.c_str());
            return 0;
        }

        if ( _userRobo )
            _userRobo->_energy = _userRobo->_energy_max;
    }
    else
    {
        v6 = ypaworld_func161(arg);

        if ( !v6 )
            ypa_log_out("Warning: in YWM_ADVANCEDCREATELEVEL: YWM_CREATELEVEL %d failed!\n", arg->lvlID);
    }

    if ( v6 )
    {
        // Every fresh mission start, including replaying a completed mission,
        // begins at the second-closest tactical-map zoom.
        SetFreshLevelTacticalMapZoom();

        if ( !SaveGame(fmt::sprintf("save:%s/%d.rst", _GameShell->UserName, _levelInfo.LevelID)) )
            ypa_log_out("Warning: could not create restart file for level %d, user %s.\n", _levelInfo.LevelID, _GameShell->UserName.c_str());
    }

    _lvlPrimevalTypeMap = _lvlTypeMap;
    _lvlPrimevalOwnMap = _lvlOwnMap;

    return v6;
}

void NC_STACK_ypaworld::HistoryEventAdd(const World::History::Record &arg)
{
    switch ( arg.type )
    {
    case World::History::TYPE_FRAME: // Do not write timestamp every frame, wait for any another data
        _historyLastIsTimeStamp = true;
        _historyLastFrame = static_cast<const World::History::Frame&>(arg);
        break;

    case World::History::TYPE_CONQ:
    case World::History::TYPE_VHCLKILL:
    case World::History::TYPE_VHCLCREATE:
    case World::History::TYPE_PLASMA:
    case World::History::TYPE_POWERST:
    case World::History::TYPE_UPGRADE:

        if (_historyLastIsTimeStamp) // If
            _history.Write(_historyLastFrame.MakeByteArray());

        _history.Write(arg.MakeByteArray());

        _historyLastIsTimeStamp = false;

        if (_GameShell && _GameShell->isHost )
            arg.AddScore(&_gameplayStats);
        break;

    default:
        break;
    }


}

void NC_STACK_ypaworld::ypaworld_func185(const void *arg)
{
    dprintf("MAKE ME %s\n","ypaworld_func185");
}

void NC_STACK_ypaworld::setYW_normVisLimit(int limit)
{
    _normalVizLimit = limit;
}

void NC_STACK_ypaworld::setYW_fadeLength(int len)
{
    _normalFadeLength = len;
}

void NC_STACK_ypaworld::setYW_skyVisLimit(int limit)
{
    _skyVizLimit = limit;
}

void NC_STACK_ypaworld::setYW_skyFadeLength(int len)
{
    _skyFadeLength = len;
}

void NC_STACK_ypaworld::setYW_skyHeight(int hght)
{
    _skyHeight = hght;
}

void NC_STACK_ypaworld::setYW_skyRender(int dorender)
{
    _skyRender = dorender;
    UpdateSkyHorizonRenderSectors();
}

void NC_STACK_ypaworld::setYW_doEnergyRecalc(int doRecalc)
{
    _doEnergyRecalc = doRecalc;
}

int32_t NC_STACK_ypaworld::GetSkyHorizonDistance() const
{
    const int32_t configured = System::IniConf::GfxSkyDistance.Get<int32_t>();
    return std::max<int32_t>(YW_SKY_HORIZON_DISTANCE_MIN,
                             std::min<int32_t>(YW_SKY_HORIZON_DISTANCE_MAX, configured));
}

void NC_STACK_ypaworld::UpdateSkyHorizonRenderSectors()
{
    if ( !_skyRender )
    {
        setYW_visSectors(YW_RENDER_SECTORS_MAX);
        return;
    }

    const float sectorLength = World::CVSectorLength;

    // RenderGame uses a diamond-shaped (Manhattan-distance) sector window.
    // Convert the circular sky radius to the worst-case diagonal Manhattan
    // radius, then keep two additional hidden sector layers behind the dome.
    const int32_t horizonRadius = (int32_t)std::ceil(
        ((float)GetSkyHorizonDistance() / sectorLength) * 1.41421356237f);
    const int32_t safeRadius =
        horizonRadius + YW_SKY_HORIZON_SECTOR_MARGIN;

    setYW_visSectors(safeRadius * 2 + 1);
}

void NC_STACK_ypaworld::setYW_visSectors(int visSectors)
{
    _renderSectors = yw_ClampRenderSectors(visSectors);
    _normalVizLimit = yw_RenderSectorsToNormalVizLimit(_renderSectors);
}

void NC_STACK_ypaworld::setYW_userHostStation(NC_STACK_ypabact *host)
{
    _userRobo = host;
    _playerOwner = host->_owner;
}

void NC_STACK_ypaworld::setYW_userVehicle(NC_STACK_ypabact *bact)
{
    if ( !CanControlUnitInSpectatorMode(bact) )
        return;

    // OpenNeoUA custom: never make an artillery shell platform the player-controlled vehicle;
    // artillery shells are commanded only from the 2D strategic map.
    if ( bact && bact->IsArtilleryShellPlatform() )
        return;

    if ( bact != _userUnit )
    {
        NC_STACK_ypabact *oldpBact = _userUnit;

        if ( oldpBact )
            _prevUnitId = oldpBact->_gid;

        _userUnit = bact;

        _vehicleTakenControlTimestamp = _timeStamp;
        _vehicleTakenCommandId = _userUnit->_commandID;
        _guiDragDefaultMouse = false;

        if ( _userUnit->_bact_type == BACT_TYPES_ROBO )
        {
            _joyIgnoreY = 1;
            _joyIgnoreX = 1;
        }

        FFeedback_VehicleChanged();

        if ( oldpBact )
            ypaworld_func2__sub0__sub1(this, oldpBact, _userUnit);
    }
}

void NC_STACK_ypaworld::setYW_screenW(int w)
{
    _screenSize.x = w;
}

void NC_STACK_ypaworld::setYW_screenH(int h)
{
    _screenSize.y = h;
}

void NC_STACK_ypaworld::setYW_dontRender(bool drndr)
{
    _doNotRender = drndr;
}

int NC_STACK_ypaworld::getYW_mapSizeX()
{
    return _mapSize.x;
}

int NC_STACK_ypaworld::getYW_mapSizeY()
{
    return _mapSize.y;
}

Common::Point NC_STACK_ypaworld::GetMapSize()
{
    return _mapSize;
}

int NC_STACK_ypaworld::getYW_normVisLimit()
{
    return _normalVizLimit;
}

int NC_STACK_ypaworld::getYW_fadeLength()
{
    return _normalFadeLength;
}

int NC_STACK_ypaworld::getYW_skyHeight()
{
    return _skyHeight;
}

int NC_STACK_ypaworld::getYW_skyRender()
{
    return _skyRender;
}

int NC_STACK_ypaworld::getYW_doEnergyRecalc()
{
    return _doEnergyRecalc;
}

int NC_STACK_ypaworld::getYW_visSectors()
{
    return _renderSectors;
}

NC_STACK_ypabact *NC_STACK_ypaworld::getYW_userHostStation()
{
    return _userRobo;
}

NC_STACK_ypabact *NC_STACK_ypaworld::getYW_userVehicle()
{
    return _userUnit;
}

int NC_STACK_ypaworld::getYW_lvlFinished()
{
    if ( _levelInfo.State != TLevelInfo::STATE_COMPLETED && _levelInfo.State != TLevelInfo::STATE_ABORTED )
        return 0;

    return 1;
}

int NC_STACK_ypaworld::getYW_screenW()
{
    return _screenSize.x;
}

int NC_STACK_ypaworld::getYW_screenH()
{
    return _screenSize.y;
}

TLevelInfo &NC_STACK_ypaworld::GetLevelInfo()
{
    return _levelInfo;
}

int NC_STACK_ypaworld::getYW_destroyFX()
{
    return _fxLimit;
}

NC_STACK_windp *NC_STACK_ypaworld::getYW_pNET()
{
    return _netDriver;
}

int NC_STACK_ypaworld::getYW_invulnerable()
{
    return _invulnerable;
}

int NC_STACK_ypaworld::TestVehicle(int protoID, int job, NC_STACK_ypabact *target)
{
    const World::TVhclProto &proto = _vhclProtos[ protoID ];

    int legacyValue = -1;

    if ( proto.is_mimic )
    {
        switch ( job )
        {
        case 1: legacyValue = proto.job_fightrobo; break;
        case 2: legacyValue = proto.job_fighttank; break;
        case 4: legacyValue = proto.job_fighthelicopter; break;
        case 3: legacyValue = proto.job_fightflyer; break;
        case 5: return proto.job_reconnoitre;
        case 6: return proto.job_conquer;
        default:return -1;
        }
    }
    else
    {
        World::TWeapProto *wpn = proto.weapon == -1
                               ? nullptr
                               : &_weaponProtos.at(proto.weapon);

        switch ( job )
        {
        case 1:
        case 2:
        case 3:
        case 4:
            if ( (((proto.mgun_set && proto.mgun == -1) ||
                   (!proto.mgun_set && proto.mgun_shot_time <= 0)) && !wpn) ||
                 proto.model_id == BACT_TYPES_UFO )
                return -1;

            if ( job == 1 ) legacyValue = proto.job_fightrobo;
            if ( job == 2 ) legacyValue = proto.job_fighttank;
            if ( job == 3 ) legacyValue = proto.job_fightflyer;
            if ( job == 4 ) legacyValue = proto.job_fighthelicopter;
            break;

        case 5:
            return proto.job_reconnoitre;

        case 6:
            if ( !wpn || proto.model_id == BACT_TYPES_UFO )
                return -1;
            return proto.job_conquer;

        default:
            return -1;
        }
    }

    // OpenNeoUA: when AllocForce knows the concrete enemy, allow a new
    // fine-grained job_fight* to override the old coarse strategic group.
    // If the specific value is absent, legacyValue is returned unchanged.
    if ( target )
    {
        int specificValue = 0;
        if ( World::TryGetSpecificFightJob(
                 proto, World::ResolveVehicleCombatClass(target), &specificValue) )
            return specificValue;
    }

    return legacyValue;
}

SDL_Color NC_STACK_ypaworld::GetColor(int colorID)
{
    return _iniColors.at(colorID);
}

SDL_Color NC_STACK_ypaworld::GetFactionBoxTextColor() const
{
    if ( _userRobo && _userRobo->_owner == World::OWNER_RESIST )
        return _iniColors[World::COLOR_TEXT_TOOLTIP];

    return GFX::Engine.Color(255, 255, 255);
}

cellArea *NC_STACK_ypaworld::GetSector(int32_t x, int32_t y)
{
    if (_cells.empty())
        return nullptr;

    if (x >= 0 && x < _mapSize.x
    &&  y >= 0 && y < _mapSize.y)
        return &_cells(x, y);
    return nullptr;
}

cellArea *NC_STACK_ypaworld::GetSector(const Common::Point &sec)
{
    if (_cells.empty())
        return nullptr;

    if (sec.x >= 0 && sec.x < _mapSize.x
    &&  sec.y >= 0 && sec.y < _mapSize.y)
        return &_cells(sec.x, sec.y);
    return nullptr;
}

cellArea *NC_STACK_ypaworld::GetSector(size_t id)
{
    if (_cells.empty())
        return nullptr;

    if (id >= 0 && id < _cells.size())
        return &_cells.At(id);
    return nullptr;
}

cellArea &NC_STACK_ypaworld::SectorAt(int32_t x, int32_t y)
{
    return _cells(x, y);
}

cellArea &NC_STACK_ypaworld::SectorAt(const Common::Point &sec)
{
    return _cells.At(sec);
}

cellArea &NC_STACK_ypaworld::SectorAt(size_t id)
{
    return _cells.At(id);
}

void NC_STACK_ypaworld::SetMapSize(const Common::Point &sz)
{
    _mapSize = sz;

    _mapLength = vec2d( _mapSize.x * World::CVSectorLength, _mapSize.y * World::CVSectorLength );

    _cells.Clear();
    _cells.Resize(sz);

    int32_t id = 0;
    for (int y = 0; y < sz.y; y++)
    {
        for (int x = 0; x < sz.x; x++)
        {
            cellArea &cell = _cells(x, y);
            cell.Id = id;
            cell.CellId = Common::Point(x, y);

            if (x == 0 || y == 0 || x == sz.x - 1 || y == sz.y - 1)
                cell.Flags |= cellArea::CF_BORDER;

            id++;
        }
    }

    _cellsVFCache.Clear();
    _cellsVFCache.Resize(sz.x, sz.y);

    _cellsHFCache.Clear();
    _cellsHFCache.Resize(sz.x, sz.y);

    _energyAccumMap.Clear();
    _energyAccumMap.Resize(sz);
}

bool NC_STACK_ypaworld::IsGamePlaySector(const Common::Point &sz) const
{
    return sz.x > 0 && sz.x < (_mapSize.x - 1) && sz.y > 0 && sz.y < (_mapSize.y - 1);
}

bool NC_STACK_ypaworld::IsSectorBorder(const Common::Point &sz, int border) const
{
    return sz.x < border || sz.x >= (_mapSize.x - border) || sz.y < border || sz.y >= (_mapSize.y - border);
}

bool NC_STACK_ypaworld::IsSector(const Common::Point &sz) const
{
    return sz.x >= 0 && sz.x < _mapSize.x  && sz.y >= 0 && sz.y < _mapSize.y;
}

Common::PlaneVector<cellArea> &NC_STACK_ypaworld::Sectors()
{
    return _cells;
}

int32_t NC_STACK_ypaworld::GetLegoBld(const cellArea *cell, int bldX, int bldY)
{
    TSubSectorDesc *ssec = _secTypeArray[ cell->type_id ].SubSectors.At(bldX, bldY);
    int32_t hlth = cell->buildings_health.Get(bldX, bldY);
    return ssec->HPModels[ _buildHealthModelId[ hlth ] ];
}

int32_t NC_STACK_ypaworld::GetLegoBld(const Common::Point &cell, int bldX, int bldY)
{
    return GetLegoBld(&SectorAt(cell), bldX, bldY);
}

TCellFillerCh::~TCellFillerCh()
{
    FreeVBO();
}

void TCellFillerCh::FreeVBO()
{
    for (GFX::TMesh &msh : Meshes)
        GFX::Engine.MeshFreeVBO( &msh );
}

void TCellFillerCh::MakeVBO()
{
    for (GFX::TMesh &msh : Meshes)
        GFX::Engine.MeshMakeVBO( &msh );
}
