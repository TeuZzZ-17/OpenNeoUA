// Gem unlock popup (visibility, duration), unlock sound and map marker sound.
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

uint32_t yw_GetGemUnlockDuration()
{
    return yw_ReadNonNegativeDuration(System::IniConf::GameGemUnlockDuration,
                                      GEM_UNLOCK_DEFAULT_DURATION_MS);
}

uint32_t NC_STACK_ypaworld::GetGemUnlockDuration() const
{
    return yw_GetGemUnlockDuration();
}

float NC_STACK_ypaworld::GetGemPopupY() const
{
    // Missing, non numeric or non finite values keep the vanilla position.
    constexpr float vanillaY = -0.5f;
    const std::string value = System::IniConf::GameGemUnlockPopupY.Get<std::string>();
    try
    {
        size_t pos = 0;
        const float y = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos || !std::isfinite(y) )
            return vanillaY;

        return std::max(-1.0f, std::min(y, 1.0f));
    }
    catch (...)
    {
        return vanillaY;
    }
}

bool NC_STACK_ypaworld::IsGemPopupVisible(int32_t *outVehicleId) const
{
    if ( _upgradeId == -1 )
        return false;

    // With the optional slow motion the game clock runs slower, so the popup
    // lifetime shrinks by the same factor to last the configured real time.
    uint32_t duration = GetGemUnlockDuration();
    if ( yw_IsGemUnlockSlowMotionEnabled() )
        duration = (uint32_t)(duration * DEBUG_GAMEPLAY_TIME_SCALE);

    if ( (uint32_t)_timeStamp - _upgradeTimeStamp >= duration )
        return false;

    int32_t vehicleId = _upgradeVehicleId;
    if ( !vehicleId && _upgradeWeaponId )
    {
        for (size_t i = 0; i < _vhclProtos.size(); i++)
        {
            if ( _vhclProtos[i].weapon == _upgradeWeaponId )
            {
                vehicleId = (int32_t)i;
                break;
            }
        }

        if ( !vehicleId )
            return false;
    }

    if ( !vehicleId && !_upgradeBuildId )
        return false;

    if ( outVehicleId )
        *outVehicleId = vehicleId;

    return true;
}

bool yw_IsGemUnlockSlowMotionEnabled()
{
    return System::IniConf::GameGemUnlockSlowMotion.Get<bool>();
}

void NC_STACK_ypaworld::PlayConfiguredGemUnlockSound()
{
    if ( !_GameShell )
        return;

    const std::string path = System::IniConf::GameGemUnlockSound.Get<std::string>();
    if ( path.empty() )
        return;

    const size_t soundId = World::SOUND_ID_GEM_UNLOCK;
    if ( soundId >= _GameShell->samples1.size() || soundId >= _GameShell->samples1_info.Sounds.size() )
        return;

    NC_STACK_sample *&sample = _GameShell->samples1[soundId];
    TSoundSource &source = _GameShell->samples1_info.Sounds[soundId];

    // Cache the attempted path so an unloadable file logs only once.
    if ( _gemUnlockSoundAttemptedPath != path )
    {
        if ( sample )
        {
            SFXEngine::SFXe.sub_424000(&_GameShell->samples1_info, soundId);
            SFXEngine::SFXe.ForceStopSource(&_GameShell->samples1_info, soundId);
            sample->Delete();
            sample = nullptr;
            source.PSample = nullptr;
        }

        _gemUnlockSoundAttemptedPath = path;

        // Canonical value is Data/...; the loader root is data:, so drop the Data/ prefix.
        std::string normalized;
        if ( !uaNormalizeDataAssetPath(path, &normalized, true) )
        {
            ypa_log_out("Warning: Invalid GEM unlock sample path %s. GEM unlock sound disabled.\n", path.c_str());
            return;
        }

        std::string previousRsrc = Common::Env.SetPrefix("rsrc", "data:");
        NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, normalized.substr(5)}});
        Common::Env.SetPrefix("rsrc", previousRsrc);

        if ( !wav )
        {
            ypa_log_out("Warning: Could not load GEM unlock sample %s. GEM unlock sound disabled.\n", path.c_str());
            return;
        }

        sample = wav;
        source.PSample = wav->GetSampleData();
    }

    if ( !sample )
        return;

    source.PSample = sample->GetSampleData();
    source.Volume = 100;
    source.Pitch = 0;
    source.IgnoreTimeScale = true;
    SFXEngine::SFXe.startSound(&_GameShell->samples1_info, soundId);
}

void NC_STACK_ypaworld::PlayConfiguredMapMarkerSound()
{
    if ( !_GameShell )
        return;

    const std::string path = System::IniConf::UiMapMarkerSound.Get<std::string>();
    const size_t soundId = World::SOUND_ID_MAP_MARKER;
    if ( soundId >= _GameShell->samples1.size() || soundId >= _GameShell->samples1_info.Sounds.size() )
        return;

    NC_STACK_sample *&sample = _GameShell->samples1[soundId];
    TSoundSource &source = _GameShell->samples1_info.Sounds[soundId];

    // Map Marker audio is fully data-driven. An empty path means silent markers.
    // Cache the attempted path so an unloadable file logs only once instead of
    // retrying on every marker placement.
    if ( path.empty() )
    {
        if ( sample && !_mapMarkerSoundAttemptedPath.empty() )
        {
            SFXEngine::SFXe.sub_424000(&_GameShell->samples1_info, soundId);
            SFXEngine::SFXe.ForceStopSource(&_GameShell->samples1_info, soundId);
            sample->Delete();
            sample = nullptr;
            source.PSample = nullptr;
        }

        _mapMarkerSoundAttemptedPath.clear();
        return;
    }

    if ( _mapMarkerSoundAttemptedPath != path )
    {
        if ( sample )
        {
            SFXEngine::SFXe.sub_424000(&_GameShell->samples1_info, soundId);
            SFXEngine::SFXe.ForceStopSource(&_GameShell->samples1_info, soundId);
            sample->Delete();
            sample = nullptr;
            source.PSample = nullptr;
        }

        _mapMarkerSoundAttemptedPath = path;

        std::string previousRsrc = Common::Env.SetPrefix("rsrc", "data:");
        NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, path}});
        Common::Env.SetPrefix("rsrc", previousRsrc);

        if ( !wav )
        {
            ypa_log_out("Warning: Could not load map marker sample %s. Marker audio disabled.\n", path.c_str());
            return;
        }

        sample = wav;
        source.PSample = sample->GetSampleData();
    }

    if ( !sample )
        return;

    source.PSample = sample->GetSampleData();
    source.Volume = 100;
    source.Pitch = 0;
    source.IgnoreTimeScale = false;
    SFXEngine::SFXe.startSound(&_GameShell->samples1_info, soundId);
}
