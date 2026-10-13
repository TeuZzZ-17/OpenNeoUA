// Gem notifications, gem unlock and map marker sounds.
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
                                      GEM_NEW_UI_DEFAULT_DURATION_MS);
}

void NC_STACK_ypaworld::BeginGemNotificationCapture()
{
    _gemNotificationEntries.clear();
    _gemNotificationActionOrder = 0;
    _gemNotificationCaptureActive = true;
}

void NC_STACK_ypaworld::ClearGemNotificationCapture()
{
    _gemNotificationCaptureActive = false;
    _gemNotificationActionOrder = 0;
    _gemNotificationEntries.clear();
}

bool NC_STACK_ypaworld::IsGemNotificationCaptureActive() const
{
    return _gemNotificationCaptureActive;
}

uint32_t NC_STACK_ypaworld::GetGemUnlockDuration() const
{
    return yw_GetGemUnlockDuration();
}

bool NC_STACK_ypaworld::HasActiveNewGemNotification() const
{
    const uint32_t durationMs = GetGemUnlockDuration();
    return durationMs > 0 &&
           System::IniConf::GameGemUnlockNewUI.Get<bool>() &&
           _upgradeId != -1 &&
           GetNewGemNotificationElapsedTime() < durationMs;
}

uint32_t NC_STACK_ypaworld::GetNewGemNotificationElapsedTime() const
{
    return (uint32_t)_timeStamp - _upgradeTimeStamp;
}

void NC_STACK_ypaworld::DismissNewGemNotification()
{
    if ( !HasActiveNewGemNotification() )
        return;

    _upgradeTimeStamp = (uint32_t)_timeStamp - GetGemUnlockDuration();
}

bool NC_STACK_ypaworld::IsNewGemNotificationBlockingPlayerWeapons(const NC_STACK_ypabact *bact) const
{
    return bact && bact == _userUnit && bact->_bact_type != BACT_TYPES_UFO &&
           HasActiveNewGemNotification();
}

void NC_STACK_ypaworld::RecordGemNotificationChange(uint8_t targetKind, int32_t targetProtoId,
                                                     uint8_t changeKind, int32_t previousRawValue,
                                                     int32_t newRawValue, bool newlyEnabled)
{
    if ( !_gemNotificationCaptureActive )
        return;

    if ( (targetKind == TGemNotificationEntry::TARGET_VEHICLE &&
          (targetProtoId < 0 || (size_t)targetProtoId >= _vhclProtos.size())) ||
         (targetKind == TGemNotificationEntry::TARGET_WEAPON &&
          (targetProtoId < 0 || (size_t)targetProtoId >= _weaponProtos.size())) ||
         (targetKind == TGemNotificationEntry::TARGET_BUILDING &&
          (targetProtoId < 0 || (size_t)targetProtoId >= _buildProtos.size())) )
        return;

    TGemNotificationEntry entry;
    entry.TargetKind = targetKind;
    entry.ChangeKind = changeKind;
    entry.TargetProtoId = targetProtoId;
    entry.PreviousRawValue = previousRawValue;
    entry.NewRawValue = newRawValue;
    entry.ActionOrder = _gemNotificationActionOrder++;
    entry.NewlyEnabled = changeKind == TGemNotificationEntry::CHANGE_ENABLE && newlyEnabled;
    entry.AlreadyUnlocked = (changeKind == TGemNotificationEntry::CHANGE_ENABLE &&
                             previousRawValue != 0 && newRawValue != 0);
    _gemNotificationEntries.push_back(entry);
}

void NC_STACK_ypaworld::FinishGemNotificationCapture(bool successful)
{
    _gemNotificationCaptureActive = false;

    if ( !successful )
    {
        _gemNotificationEntries.clear();
        return;
    }

    for (TGemNotificationEntry &entry : _gemNotificationEntries)
    {
        if ( entry.TargetKind != TGemNotificationEntry::TARGET_WEAPON )
            continue;

        std::vector<int32_t> candidates;
        for (const TGemNotificationEntry &other : _gemNotificationEntries)
        {
            if ( other.TargetKind != TGemNotificationEntry::TARGET_VEHICLE ||
                 other.TargetProtoId < 0 ||
                 (size_t)other.TargetProtoId >= _vhclProtos.size() ||
                 !yw_VehicleReferencesWeapon(_vhclProtos[other.TargetProtoId], entry.TargetProtoId) )
                continue;

            if ( std::find(candidates.begin(), candidates.end(), other.TargetProtoId) == candidates.end() )
                candidates.push_back(other.TargetProtoId);
        }

        if ( candidates.size() == 1 )
        {
            entry.RelatedVehicleId = candidates.front();
            continue;
        }

        if ( !candidates.empty() )
            continue;

        for (size_t vehicleId = 0; vehicleId < _vhclProtos.size(); ++vehicleId)
        {
            if ( yw_VehicleReferencesWeapon(_vhclProtos[vehicleId], entry.TargetProtoId) )
                candidates.push_back(vehicleId);

            if ( candidates.size() > 1 )
                break;
        }

        if ( candidates.size() == 1 )
            entry.RelatedVehicleId = candidates.front();
    }
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
        std::string previousRsrc = Common::Env.SetPrefix("rsrc", "data:");
        NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, path}});
        Common::Env.SetPrefix("rsrc", previousRsrc);

        if ( !wav )
        {
            ypa_log_out("Warning: Could not load GEM unlock sample %s. Using vanilla GEM audio only.\n", path.c_str());
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
