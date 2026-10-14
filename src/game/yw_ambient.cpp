// Ambient level sound and atmospheric FX profiles and loops.
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

// Copies one cloud profile of Clouds.cfg. The parser already validated it.
bool NC_STACK_ypaworld::FindCloudProfile(int32_t id, World::TAtmosphericFXProfile &outProfile)
{
    outProfile = World::TAtmosphericFXProfile();

    const auto it = _cloudProfiles.find(id);
    if ( it == _cloudProfiles.end() )
    {
        ypa_log_out("WARNING: cloud profile %d is not defined in the scripts.\n", id);
        return false;
    }

    outProfile = it->second;
    return true;
}

bool NC_STACK_ypaworld::LoadAtmosphericFXProfile(const TLevelDescription &mapp)
{
    ClearAtmosphericFXRuntime();
    StopAtmosphericFXLoopSound();
    _atmosphericFXProfile = World::TAtmosphericFXProfile();

    if ( mapp.CloudId < 0 )
        return false;

    World::TAtmosphericFXProfile parsedProfile;
    if ( !FindCloudProfile(mapp.CloudId, parsedProfile) )
    {
        ypa_log_out("WARNING: cloud disabled for this level.\n");
        return false;
    }

    _atmosphericFXProfile = std::move(parsedProfile);
    StartAtmosphericFXLoopSound();
    ypa_log_out("OpenNeoUA: loaded cloud %d for this level.\n", mapp.CloudId);
    return true;
}

void NC_STACK_ypaworld::ClearAtmosphericFXRuntime()
{
    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( it->atmosphericFX )
            it = _transientVPs.erase(it);
        else
            ++it;
    }
}

int32_t NC_STACK_ypaworld::GetAmbientSoundGlobalVolume() const
{
    const std::string value = System::IniConf::GameAmbientSoundVolume.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return 100;

    try
    {
        size_t pos = 0;
        const long parsed = std::stol(value, &pos, 0);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos || parsed < 0 )
            return 100;

        return (int32_t)std::min<long>(parsed, 127);
    }
    catch (...)
    {
        return 100;
    }
}

bool yw_IsAmbientAudioFile(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    if ( dot == std::string::npos )
        return false;

    const std::string ext = name.substr(dot);
    return !StriCmp(ext, ".wav") || !StriCmp(ext, ".ogg") ||
           !StriCmp(ext, ".mp3") || !StriCmp(ext, ".flac") ||
           !StriCmp(ext, ".opus") || !StriCmp(ext, ".m4a") ||
           !StriCmp(ext, ".aac") || !StriCmp(ext, ".wma");
}

std::string yw_NormalizeAmbientPath(const std::string &path)
{
    std::string normalized = path;
    while ( !normalized.empty() &&
            (normalized.back() == '/' || normalized.back() == '\\') )
    {
        normalized.pop_back();
    }

    return normalized;
}

std::vector<std::string> yw_GetAmbientSoundCandidates(const std::string &configuredPath)
{
    std::vector<std::string> candidates;
    const std::string path = yw_NormalizeAmbientPath(configuredPath);
    if ( path.empty() )
        return candidates;

    FSMgr::DirIter dir = uaOpenDir("data:" + path);
    if ( !dir )
    {
        candidates.push_back(path);
        return candidates;
    }

    FSMgr::iNode *node = nullptr;
    while ( dir.getNext(&node) )
    {
        if ( !node || node->getType() != FSMgr::iNode::NTYPE_FILE )
            continue;

        const std::string name = node->getName();
        if ( !yw_IsAmbientAudioFile(name) )
            continue;

        candidates.push_back(path + "/" + name);
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const std::string &a, const std::string &b) { return StriCmp(a, b) < 0; });

    return candidates;
}

void NC_STACK_ypaworld::StopAmbientLevelSound()
{
    SFXEngine::SFXe.StopCarrier(&_ambientSoundCarrier);
    _ambientSoundCarrier.Clear();

    if ( _ambientSoundSample )
    {
        _ambientSoundSample->Delete();
        _ambientSoundSample = nullptr;
    }

}

void NC_STACK_ypaworld::StopAtmosphericFXLoopSoundInstance(NC_STACK_sample *&sample,
                                                            TSndCarrier &carrier)
{
    SFXEngine::SFXe.StopCarrier(&carrier);
    carrier.Clear();

    if ( sample )
    {
        sample->Delete();
        sample = nullptr;
    }
}

bool NC_STACK_ypaworld::StartAtmosphericFXLoopSoundInstance(const World::TAtmosphericFXProfile &profile,
                                                            NC_STACK_sample *&sample,
                                                            TSndCarrier &carrier)
{
    StopAtmosphericFXLoopSoundInstance(sample, carrier);

    if ( !profile.valid || profile.loop_sound.empty() || profile.loop_sound_volume <= 0 )
        return false;

    const std::string previousRsrc = Common::Env.SetPrefix("rsrc", "data:");
    NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME,
                                                       profile.loop_sound}});
    Common::Env.SetPrefix("rsrc", previousRsrc);

    if ( !wav || !wav->GetSampleData() )
    {
        if ( wav )
            wav->Delete();
        ypa_log_out("WARNING: could not load Atmospheric FX loop sound '%s'.\n",
                    profile.loop_sound.c_str());
        return false;
    }

    sample = wav;
    carrier.Resize(1);
    carrier.Position = _viewerPosition;
    carrier.Vector = vec3d(0.0, 0.0, 0.0);

    TSoundSource &sound = carrier.Sounds[0];
    sound.PSample = wav->GetSampleData();
    sound.Volume = (int16_t)std::max(0, std::min(profile.loop_sound_volume, 127));
    sound.Pitch = 0;
    // FX profile loop sounds are viewer-local by default. Context-specific
    // bindings (for example Mobile Power) may override this with their own
    // existing gameplay radius instead of duplicating a profile radius.
    sound.Radius = 0.0f;
    sound.FadeDuration = 0.0;
    sound.PPFx = nullptr;
    sound.PShkFx = nullptr;
    sound.SetPFx(false);
    sound.SetShk(false);
    sound.SetLoop(true);
    sound.SetFragmented(false);

    SFXEngine::SFXe.startSound(&carrier, 0);
    return true;
}

void NC_STACK_ypaworld::StopAtmosphericFXLoopSound()
{
    StopAtmosphericFXLoopSoundInstance(_atmosphericFXLoopSoundSample,
                                       _atmosphericFXLoopSoundCarrier);
}

void NC_STACK_ypaworld::StartAtmosphericFXLoopSound()
{
    StartAtmosphericFXLoopSoundInstance(_atmosphericFXProfile,
                                        _atmosphericFXLoopSoundSample,
                                        _atmosphericFXLoopSoundCarrier);
}

void NC_STACK_ypaworld::StartAmbientLevelSound(const TLevelDescription &mapp)
{
    StopAmbientLevelSound();
    StopAtmosphericFXLoopSound();

    const std::string globalPath = System::IniConf::GameAmbientSound.Get<std::string>();
    const int32_t globalVolume = GetAmbientSoundGlobalVolume();

    auto tryStartFile = [this](const std::string &path, int32_t volume) -> bool
    {
        if ( path.empty() )
            return false;

        // Volume 0 is an explicit mute. Treat it as a valid configuration so
        // a muted level override does not unexpectedly fall back to Nucleus.
        if ( volume <= 0 )
            return true;

        const std::string previousRsrc = Common::Env.SetPrefix("rsrc", "data:");
        NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, path}});
        Common::Env.SetPrefix("rsrc", previousRsrc);

        if ( !wav || !wav->GetSampleData() )
        {
            if ( wav )
                wav->Delete();
            return false;
        }

        _ambientSoundSample = wav;
        _ambientSoundCarrier.Resize(1);
        _ambientSoundCarrier.Position = _viewerPosition;
        _ambientSoundCarrier.Vector = vec3d(0.0, 0.0, 0.0);

        TSoundSource &sound = _ambientSoundCarrier.Sounds[0];
        sound.PSample = wav->GetSampleData();
        sound.Volume = (int16_t)volume;
        sound.Pitch = 0;
        sound.Radius = 0.0f;
        sound.FadeDuration = 0.0;
        sound.PPFx = nullptr;
        sound.PShkFx = nullptr;
        sound.SetPFx(false);
        sound.SetShk(false);
        sound.SetLoop(true);
        sound.SetFragmented(false);

        SFXEngine::SFXe.startSound(&_ambientSoundCarrier, 0);
        return true;
    };

    auto tryStart = [&tryStartFile](const std::string &configuredPath, int32_t volume) -> bool
    {
        if ( configuredPath.empty() )
            return false;

        // Volume 0 is an explicit mute. Do not require a file/folder to exist
        // just to preserve the configured priority semantics.
        if ( volume <= 0 )
            return true;

        const std::vector<std::string> candidates = yw_GetAmbientSoundCandidates(configuredPath);
        if ( candidates.empty() )
            return false;

        if ( candidates.size() == 1 )
            return tryStartFile(candidates.front(), volume);

        // A directory chooses a different random starting candidate for every
        // level start. If that file is invalid, try the remaining candidates
        // before falling back from LDF to Nucleus.
        const size_t first = (size_t)(rand() % (int)candidates.size());
        for ( size_t i = 0; i < candidates.size(); ++i )
        {
            const std::string &candidate = candidates[(first + i) % candidates.size()];
            if ( tryStartFile(candidate, volume) )
            {
                ypa_log_out("OpenNeoUA: ambient sound selected '%s' from '%s'.\n",
                            candidate.c_str(), configuredPath.c_str());
                return true;
            }
        }

        return false;
    };

    if ( !mapp.AmbientSoundStr.empty() )
    {
        // LDF controls only which ambience is used. Volume is always the
        // global game.ambient_sound_volume value, including the Options slider.
        if ( tryStart(mapp.AmbientSoundStr, globalVolume) )
            return;

        ypa_log_out("Warning: could not load level ambient sound '%s'; trying Nucleus fallback.\n",
                    mapp.AmbientSoundStr.c_str());
    }

    if ( !globalPath.empty() && !tryStart(globalPath, globalVolume) )
    {
        ypa_log_out("Warning: could not load Nucleus ambient sound '%s'; ambience disabled for this level.\n",
                    globalPath.c_str());
    }
}

void NC_STACK_ypaworld::UpdateAmbientLevelSound()
{
    if ( _ambientSoundSample && !_ambientSoundCarrier.Sounds.empty() )
    {
        // Keep the carrier on the active listener so ambience is non-positional
        // and remains at the configured volume anywhere in the level.
        _ambientSoundCarrier.Position = _viewerPosition;
        _ambientSoundCarrier.Vector = vec3d(0.0, 0.0, 0.0);
        SFXEngine::SFXe.UpdateSoundCarrier(&_ambientSoundCarrier);
    }

    if ( _atmosphericFXLoopSoundSample && !_atmosphericFXLoopSoundCarrier.Sounds.empty() )
    {
        // Atmospheric FX loop audio is also screen/listener anchored.
        _atmosphericFXLoopSoundCarrier.Position = _viewerPosition;
        _atmosphericFXLoopSoundCarrier.Vector = vec3d(0.0, 0.0, 0.0);
        SFXEngine::SFXe.UpdateSoundCarrier(&_atmosphericFXLoopSoundCarrier);
    }
}
