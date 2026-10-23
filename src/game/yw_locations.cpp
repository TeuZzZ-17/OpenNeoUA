// Level-local entry notices. Vanilla levels do not allocate fonts or sound samples.
#include "game/yw.h"
#include "game/ypabact.h"
#include "common/env.h"
#include "common/log.h"
#include "common/utils.h"
#include "nucleus/loaders.h"
#include "platform/wav.h"
#include "system/gfx.h"

void NC_STACK_ypaworld::ResetLocations()
{
    SFXEngine::SFXe.StopCarrier(&_locationCarrier);
    _locationCarrier.Clear();
    for (auto *sample:_locationSamples) if (sample) sample->Delete();
    _locationSamples.clear();
    _locationSoundAttempted.clear();
    _locations.clear();
    _locationEntry=World::LocationEntryState();
}

void NC_STACK_ypaworld::UpdateLocations()
{
    if (_locations.empty() || !_userUnit) return;
    const int next=World::FindLocation(_locations,_userUnit->_position.x,_userUnit->_position.z);
    if (_locationEntry.update(next,_timeStamp)) {
        SFXEngine::SFXe.StopCarrier(&_locationCarrier);
        _locationCarrier.Clear();
        const auto &area=_locations[next];
        if (!area.sound.empty()) {
            _locationSamples.resize(_locations.size(),nullptr);
            _locationSoundAttempted.resize(_locations.size(),false);
            if (!_locationSoundAttempted[next]) {
                _locationSoundAttempted[next]=true;
                std::string path;
                if (uaNormalizeDataAssetPath(area.sound,&path,false)) {
                    const std::string previous=Common::Env.SetPrefix("rsrc","data:");
                    auto *wav=Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME,path.substr(5)}});
                    Common::Env.SetPrefix("rsrc",previous);
                    if (wav && wav->GetSampleData()) _locationSamples[next]=wav;
                    else {
                        if (wav) wav->Delete();
                        ypa_log_out("WARNING: could not load location sound '%s'.\n",path.c_str());
                    }
                }
            }
            if (_locationSamples[next]) {
                _locationCarrier.Resize(1);
                _locationCarrier.Position=_viewerPosition;
                _locationCarrier.Vector=vec3d(0,0,0);
                auto &sound=_locationCarrier.Sounds[0];
                sound.PSample=_locationSamples[next]->GetSampleData();
                sound.Volume=100; sound.Pitch=0; sound.Radius=0; sound.FadeDuration=0;
                sound.PPFx=nullptr; sound.PShkFx=nullptr;
                sound.SetPFx(false); sound.SetShk(false);
                sound.SetLoop(false); sound.SetFragmented(false);
                SFXEngine::SFXe.startSound(&_locationCarrier,0);
            }
        }
    }
    if (!_locationCarrier.Sounds.empty()) {
        _locationCarrier.Position=_viewerPosition;
        SFXEngine::SFXe.UpdateSoundCarrier(&_locationCarrier);
    }
}

void NC_STACK_ypaworld::RenderLocationTitle()
{
    const int index=_locationEntry.notice;
    if (index<0 || index>=int(_locations.size())) return;
    const auto &area=_locations[index];
    const float opacity=World::LocationOpacity(area,int64_t(_timeStamp)-_locationEntry.started);
    if (opacity>0) {
        const World::LocationColorRgb color=World::LocationTextColor(area.color);
        GFX::Engine.DrawLocationTitle(area.name,SDL_Color{color.r,color.g,color.b,255},opacity);
    }
}

void NC_STACK_ypaworld::WriteLocations(FSMgr::FileHandle *file)
{
    for (const auto &area:_locations) {
        file->printf("\nbegin_location %d\n   name = %s\n",area.id,area.name.c_str());
        if (area.color!="white") file->printf("   color = %s\n",area.color.c_str());
        if (!area.sound.empty()) file->printf("   sound = %s\n",area.sound.c_str());
        file->printf("   fade_in_ms = %d\n   hold_ms = %d\n   fade_out_ms = %d\n",
                     area.fadeIn,area.hold,area.fadeOut);
        for (const auto &p:area.points) file->printf("   point = %.5f_%.5f\n",p.x,p.z);
        file->printf("end\n");
    }
}
