// Super items: custom waves, detonation push, front and runtime restore.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include "nucleus/loaders.h"
#include "game/yparobo.h"
#include "gui/font.h"
#include "gui/uacommon.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "world/spin.h"
#include "common/crashdiag.h"
#include "game/yw_game_impl.h"

void NC_STACK_ypaworld::InitSuperItems()
{
    ClearSuperItemRuntime();

    // The level owns copies of the SuperItem definitions it uses: each begin_item names
    // its definition with superitem_id (see SuperItems.cfg).
    for (World::TSuperItemProfile &profile : _superItemProfiles)
    {
        profile.debuff.tick_snd.ClearSounds();
        profile.detonate_snd.ClearSounds();
        profile.wave_snd.ClearSounds();
    }
    _superItemProfiles.clear();

    _superItemSoundCarriers.resize(_levelInfo.SuperItems.size());
    _superItemWaveSoundCarriers.resize(_levelInfo.SuperItems.size());
    _superItemFalloutAtmosphericFXProfiles.resize(_levelInfo.SuperItems.size());
    _superItemFalloutSoundCarriers.resize(_levelInfo.SuperItems.size());
    _superItemFalloutSoundSamples.assign(_levelInfo.SuperItems.size(), nullptr);
    _superItemFalloutActive.assign(_levelInfo.SuperItems.size(), 0);

    std::map<int32_t, int32_t> loadedProfileIds;

    for ( size_t i = 0; i < _levelInfo.SuperItems.size(); i++ )
    {
        TMapSuperItem &sitem = _levelInfo.SuperItems[i];

        sitem.PCell = &_cells(sitem.CellId);

        ypaworld_arg148 arg148;
        arg148.owner = sitem.PCell->owner;
        arg148.blg_ID = sitem.InactiveBldID;
        arg148.field_C = 1;
        arg148.CellId = sitem.CellId;
        arg148.field_18 = 0;

        ypaworld_func148(&arg148);

        sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
        sitem.PCell->PurposeIndex = i;

        for ( TMapKeySector &ks : sitem.KeySectors )
        {
            if ( IsGamePlaySector( ks.CellId ) )
                ks.PCell = &_cells(ks.CellId);
        }

        sitem.ActiveTime = 0;
        sitem.TriggerTime = 0;
        sitem.ActivateOwner = 0;
        sitem.State = TMapSuperItem::STATE_INACTIVE;
        sitem.CurrentRadius = 0;
        sitem.LastRadius = 0;
        sitem.CustomProfileIndex = -1;
        sitem.WaveTransientVPId = 0;
        sitem.WavePalEffectStarted = false;
        sitem.WaveShakeEffectStarted = false;
        sitem.CustomHitUnitGids.clear();
        sitem.CustomHitBuildingSlots.clear();

        if ( _isNetGame || sitem.Type != TMapSuperItem::TYPE_BOMB || sitem.ProfileId < 0 )
            continue;

        int32_t resolvedProfile = -1;
        const auto loadedIt = loadedProfileIds.find(sitem.ProfileId);
        if ( loadedIt != loadedProfileIds.end() )
            resolvedProfile = loadedIt->second;
        else
        {
            World::TSuperItemProfile loadedProfile;
            if ( !PrepareSuperItemProfile(sitem.ProfileId, loadedProfile) )
            {
                ypa_log_out("WARNING: SuperItem #%u superitem_id %d could not be used; using vanilla fallback.\n",
                            (unsigned)i, sitem.ProfileId);
                continue;
            }

            _superItemProfiles.push_back(std::move(loadedProfile));
            resolvedProfile = (int32_t)_superItemProfiles.size() - 1;
            loadedProfileIds[sitem.ProfileId] = resolvedProfile;
        }

        const World::TSuperItemProfile &profile = _superItemProfiles[resolvedProfile];

        NC_STACK_base *waveVisual = nullptr;
        if ( !profile.wave_3ds.empty() )
            waveVisual = GetSharedExternalMesh(profile.wave_3ds);
        if ( !waveVisual && !profile.wave_base.empty() )
            waveVisual = GetSharedExternalBase(profile.wave_base);
        if ( !waveVisual && profile.wave_vp > 0 &&
             (size_t)profile.wave_vp < _vhclModels.size() )
            waveVisual = _vhclModels[profile.wave_vp];
        if ( !waveVisual )
        {
            ypa_log_out("WARNING: SuperItem #%u profile '%s' wave visual is unavailable; using vanilla fallback.\n",
                        (unsigned)i, profile.name.c_str());
            continue;
        }

        sitem.CustomProfileIndex = resolvedProfile;
        sitem.CustomHitBuildingSlots.assign(_cells.size() * 9, 0);
        _superItemSoundCarriers[i].reset(new TSndCarrier());
        _superItemWaveSoundCarriers[i].reset(new TSndCarrier());

        if ( profile.fallout_cloud_id >= 0 )
        {
            World::TAtmosphericFXProfile falloutProfile;
            if ( FindCloudProfile(profile.fallout_cloud_id, falloutProfile) )
            {
                _superItemFalloutAtmosphericFXProfiles[i] = std::move(falloutProfile);
                _superItemFalloutSoundCarriers[i].reset(new TSndCarrier());
                ypa_log_out("OpenNeoUA: SuperItem #%u loaded fallout cloud %d.\n",
                            (unsigned)i, profile.fallout_cloud_id);
            }
            else
            {
                ypa_log_out("WARNING: SuperItem #%u fallout cloud %d is unavailable; fallout visuals disabled for this bomb.\n",
                            (unsigned)i, profile.fallout_cloud_id);
            }
        }
    }

    // Load profile-owned sounds only after _superItemProfiles has reached its
    // final layout. TVhclSound owns raw sample pointers and must not be moved
    // after LoadSamples(), otherwise temporary/vector moves can duplicate the
    // owning pointer and cause a use-after-free or double delete.
    for (World::TSuperItemProfile &profile : _superItemProfiles)
    {
        if ( !profile.valid )
            continue;
        if ( profile.debuff.valid )
            profile.debuff.tick_snd.LoadSamples();
        profile.detonate_snd.LoadSamples();
        profile.wave_snd.LoadSamples();
    }
}

void NC_STACK_ypaworld::ClearSuperItemRuntime()
{
    for (std::unique_ptr<TSndCarrier> &carrier : _superItemSoundCarriers)
    {
        if ( carrier )
            SFXEngine::SFXe.StopCarrier(carrier.get());
    }
    _superItemSoundCarriers.clear();

    for (std::unique_ptr<TSndCarrier> &carrier : _superItemWaveSoundCarriers)
    {
        if ( carrier )
            SFXEngine::SFXe.StopCarrier(carrier.get());
    }
    _superItemWaveSoundCarriers.clear();

    for (size_t i = 0; i < _superItemFalloutSoundSamples.size(); ++i)
    {
        NC_STACK_sample *&sample = _superItemFalloutSoundSamples[i];
        if ( i < _superItemFalloutSoundCarriers.size() && _superItemFalloutSoundCarriers[i] )
            StopAtmosphericFXLoopSoundInstance(sample, *_superItemFalloutSoundCarriers[i]);
        else if ( sample )
        {
            sample->Delete();
            sample = nullptr;
        }
    }
    _superItemFalloutSoundSamples.clear();
    _superItemFalloutSoundCarriers.clear();
    _superItemFalloutAtmosphericFXProfiles.clear();
    _superItemFalloutActive.clear();

    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( it->atmosphericFX && it->atmosphericSuperItemIndex >= 0 )
            it = _transientVPs.erase(it);
        else
            ++it;
    }

    for (TMapSuperItem &sitem : _levelInfo.SuperItems)
    {
        if ( sitem.WaveTransientVPId > 0 )
            RemoveTransientVP(sitem.WaveTransientVPId);
        sitem.WaveTransientVPId = 0;
        sitem.CustomProfileIndex = -1;
        sitem.WavePalEffectStarted = false;
        sitem.WaveShakeEffectStarted = false;
        sitem.CustomHitUnitGids.clear();
        sitem.CustomHitBuildingSlots.clear();
    }
}

const World::TSuperItemProfile *NC_STACK_ypaworld::GetSuperItemProfile(const TMapSuperItem &sitem) const
{
    if ( sitem.CustomProfileIndex < 0 ||
         (size_t)sitem.CustomProfileIndex >= _superItemProfiles.size() )
        return nullptr;

    const World::TSuperItemProfile &profile = _superItemProfiles[sitem.CustomProfileIndex];
    return profile.valid ? &profile : nullptr;
}

bool NC_STACK_ypaworld::IsCustomSuperItem(const TMapSuperItem &sitem) const
{
    return !_isNetGame &&
           sitem.Type == TMapSuperItem::TYPE_BOMB &&
           GetSuperItemProfile(sitem) != nullptr;
}

std::string NC_STACK_ypaworld::GetSuperItemDisplayName(const TMapSuperItem &sitem) const
{
    const World::TSuperItemProfile *profile = GetSuperItemProfile(sitem);
    if ( IsCustomSuperItem(sitem) && profile )
    {
        std::string name = profile->name;
        std::replace(name.begin(), name.end(), '_', ' ');
        return name;
    }

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
        return Locale::Text::Common(Locale::CMN_BOMBNAME);
    if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
        return Locale::Text::Common(Locale::CMN_WAVENAME);
    return "SUPER ITEM";
}

void NC_STACK_ypaworld::RenderSuperWave(vec2d pos, vec2d fromPos, baseRender_msg *arg)
{
    if ( !_stoudsonWaveVehicleId )
        return;

    if ( pos.x > 0.0 && pos.y < 0.0 && pos.x < _mapLength.x && -_mapLength.y < pos.y )
    {
        if ( IsVisibleMapPos(pos) )
        {
            const World::TVhclProto &waveProto = _vhclProtos[_stoudsonWaveVehicleId];
            NC_STACK_base *wall_base = ResolveVisualModel(waveProto.vp_normal,
                                                          waveProto.visual_3ds.normal,
                                                          waveProto.visual_base.normal);

            if ( wall_base )
            {
                float v28 = 0.0;

                int v23 = (pos.x + 150) / 300;
                int v26 = (-pos.y + 150) / 300;

                if ( (v23 & 3) && (v26 & 3) )
                {
                    v28 = _cells((v23 / 4), (v26 / 4)).height;
                }
                else
                {
                    ypaworld_arg136 v22;
                    v22.vect = vec3d::OY(50000.0);
                    v22.stPos.x = pos.x;
                    v22.stPos.y = -25000.0;
                    v22.stPos.z = pos.y;
                    v22.flags = 0;

                    ypaworld_func136(&v22);

                    if ( v22.isect )
                    {
                        v28 = v22.isectPos.y;
                    }
                }


                wall_base->TForm().Pos = vec3d(pos.x, v28, pos.y);

                vec2d delt = pos - fromPos;
                delt.normalise();

                wall_base->TForm().SclRot =  mat3x3(delt.y,   0, -delt.x,
                                                      0, 1.0,    0,
                                                    delt.x, 0.0,  delt.y);

                wall_base->Render(arg, nullptr);
            }
        }
    }
}

void NC_STACK_ypaworld::RenderSuperItems(baseRender_msg *arg)
{
    // Render super items
    for ( const TMapSuperItem &sitem : _levelInfo.SuperItems )
    {
        if ( IsCustomSuperItem(sitem) )
            continue;

        if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
        {
            vec2d pos = World::SectorIDToCenterPos2( sitem.CellId );

            float v14 = sqrt( POW2(_mapLength.x) + POW2(_mapLength.y) );

            if ( sitem.CurrentRadius > 300 && sitem.CurrentRadius < v14 )
            {
                float v17 = (2 * sitem.CurrentRadius) * C_PI / 300.0;

                if ( v17 > 2.0 )
                {
                    float v9 = 6.283 / v17;

                    for (float j = 0.0; j < 6.283; j = j + v9 )
                    {
                        vec2d wallpos = vec2d(cos(j), sin(j)) * sitem.CurrentRadius + pos;
                        RenderSuperWave(wallpos, pos, arg);
                    }
                }
            }
        }
    }
}

bool yw_IsInsideSuperItemWaveXZ(const vec3d &pos,
                                        const vec3d &waveCenter,
                                        float waveRadius)
{
    if ( !std::isfinite(waveRadius) || waveRadius <= 0.0f )
        return false;

    const double dx = pos.x - waveCenter.x;
    const double dz = pos.z - waveCenter.z;
    const double radius = waveRadius;
    return dx * dx + dz * dz <= radius * radius;
}

void NC_STACK_ypaworld::UpdateSuperItemFalloutAtmosphericFX()
{
    const size_t superItemCount = _levelInfo.SuperItems.size();
    if ( _superItemFalloutAtmosphericFXProfiles.size() != superItemCount )
        return;

    std::vector<uint8_t> active(superItemCount, 0);
    std::vector<vec3d> waveCenters(superItemCount, vec3d(0.0, 0.0, 0.0));
    std::vector<float> waveRadii(superItemCount, 0.0f);
    std::vector<int32_t> activeCounts(superItemCount, 0);

    for (size_t i = 0; i < superItemCount; ++i)
    {
        TMapSuperItem &sitem = _levelInfo.SuperItems[i];
        const World::TAtmosphericFXProfile &profile = _superItemFalloutAtmosphericFXProfiles[i];

        if ( !profile.valid ||
             sitem.State != TMapSuperItem::STATE_TRIGGED ||
             !IsCustomSuperItem(sitem) || sitem.CurrentRadius <= 0 )
            continue;

        vec3d waveCenter = World::SectorIDToCenterPos3(sitem.CellId);
        if ( sitem.PCell )
            waveCenter.y = sitem.PCell->height;

        const float waveRadius = (float)sitem.CurrentRadius;
        waveCenters[i] = waveCenter;
        waveRadii[i] = waveRadius;

        // Fallout is only an illusion around the viewer. The full map is never
        // populated: the local Atmospheric FX volume turns on only after the
        // Stoudson wave has already crossed the viewer's X/Z position.
        if ( yw_IsInsideSuperItemWaveXZ(_viewerPosition, waveCenter, waveRadius) )
            active[i] = 1;
    }

    // Sound follows the same viewer-local rule as the visuals. Start/stop only
    // on boundary transitions so a missing authored sample is not retried every frame.
    if ( _superItemFalloutActive.size() != superItemCount )
        _superItemFalloutActive.assign(superItemCount, 0);

    for (size_t i = 0; i < superItemCount; ++i)
    {
        const bool wasActive = _superItemFalloutActive[i] != 0;
        const bool isActive = active[i] != 0;

        if ( isActive && !wasActive &&
             i < _superItemFalloutSoundCarriers.size() &&
             i < _superItemFalloutSoundSamples.size() &&
             _superItemFalloutSoundCarriers[i] )
        {
            StartAtmosphericFXLoopSoundInstance(_superItemFalloutAtmosphericFXProfiles[i],
                                                _superItemFalloutSoundSamples[i],
                                                *_superItemFalloutSoundCarriers[i]);
        }
        else if ( !isActive && wasActive &&
                  i < _superItemFalloutSoundCarriers.size() &&
                  i < _superItemFalloutSoundSamples.size() &&
                  _superItemFalloutSoundCarriers[i] )
        {
            StopAtmosphericFXLoopSoundInstance(_superItemFalloutSoundSamples[i],
                                               *_superItemFalloutSoundCarriers[i]);
        }

        _superItemFalloutActive[i] = isActive ? 1 : 0;

        if ( isActive &&
             i < _superItemFalloutSoundCarriers.size() &&
             i < _superItemFalloutSoundSamples.size() &&
             _superItemFalloutSoundSamples[i] &&
             _superItemFalloutSoundCarriers[i] )
        {
            _superItemFalloutSoundCarriers[i]->Position = _viewerPosition;
            _superItemFalloutSoundCarriers[i]->Vector = vec3d(0.0, 0.0, 0.0);
            SFXEngine::SFXe.UpdateSoundCarrier(_superItemFalloutSoundCarriers[i].get());
        }
    }

    // Cull or count only fallout transients. Normal level Atmospheric FX are
    // intentionally left to UpdateAtmosphericFX().
    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( !it->atmosphericFX || it->atmosphericSuperItemIndex < 0 )
        {
            ++it;
            continue;
        }

        const int32_t superItemIndex = it->atmosphericSuperItemIndex;
        if ( superItemIndex < 0 || (size_t)superItemIndex >= superItemCount ||
             !active[superItemIndex] )
        {
            it = _transientVPs.erase(it);
            continue;
        }

        const World::TAtmosphericFXProfile &profile =
            _superItemFalloutAtmosphericFXProfiles[superItemIndex];
        if ( !yw_IsAtmosphericFXInsideVolume(*it, _viewerPosition, profile) ||
             !yw_IsInsideSuperItemWaveXZ(it->pos, waveCenters[superItemIndex],
                                         waveRadii[superItemIndex]) )
        {
            it = _transientVPs.erase(it);
            continue;
        }

        ++activeCounts[superItemIndex];
        ++it;
    }

    for (size_t superItemIndex = 0; superItemIndex < superItemCount; ++superItemIndex)
    {
        if ( !active[superItemIndex] )
            continue;

        const World::TAtmosphericFXProfile &profile =
            _superItemFalloutAtmosphericFXProfiles[superItemIndex];
        int32_t spawnNeeded = profile.count - activeCounts[superItemIndex];
        if ( spawnNeeded <= 0 )
            continue;

        const vec3d localCenter = _viewerPosition + profile.spawn_offset;

        // Near the wave front only part of the viewer-local spawn box is
        // contaminated. Rejection sampling keeps particles behind the wave
        // without creating a persistent per-sector contamination map.
        int32_t attemptsRemaining = spawnNeeded * 8;
        while ( spawnNeeded > 0 && attemptsRemaining-- > 0 )
        {
            vec3d pos = localCenter;
            pos.x += yw_RandomFloatSigned(profile.spawn_radius.x);
            pos.y += yw_RandomFloatSigned(profile.spawn_radius.y);
            pos.z += yw_RandomFloatSigned(profile.spawn_radius.z);

            if ( !yw_IsInsideSuperItemWaveXZ(pos, waveCenters[superItemIndex],
                                             waveRadii[superItemIndex]) )
                continue;

            vec3d velocity = profile.velocity;
            velocity.x += yw_RandomFloatSigned(profile.velocity_random.x);
            velocity.y += yw_RandomFloatSigned(profile.velocity_random.y);
            velocity.z += yw_RandomFloatSigned(profile.velocity_random.z);

            const int32_t lifeTime = yw_RandomInRange(profile.lifetime_min,
                                                      profile.lifetime_max);
            const int32_t id = SpawnTransientVisual(0, profile.mesh3ds, pos,
                                                    mat3x3::Ident(), lifeTime,
                                                    1.0, profile.tint,
                                                    profile.scale, profile.spin,
                                                    TTransientVPParticleControls(),
                                                    profile.fade_in, profile.fade_out);
            if ( id > 0 && !_transientVPs.empty() && _transientVPs.back().id == id )
            {
                TTransientVP &fx = _transientVPs.back();
                fx.velocity = velocity;
                fx.atmosphericFX = true;
                fx.atmosphericSuperItemIndex = (int32_t)superItemIndex;
                --spawnNeeded;
            }
        }
    }
}

bool yw_IsCustomSuperItemPushTarget(const NC_STACK_ypabact *target)
{
    return target &&
           target->_energy > 0 && target->_energy_max > 0 &&
           target->CanReceiveConfiguredPush() &&
           target->_status != BACT_STATUS_DEAD &&
           target->_status != BACT_STATUS_CREATE &&
           target->_status != BACT_STATUS_BEAM &&
           !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 |
                                    BACT_STFLAG_CLEAN | BACT_STFLAG_NORENDER));
}

void NC_STACK_ypaworld::ApplyCustomSuperItemDetonationPush(int id)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    const World::TSuperItemProfile *profile = GetSuperItemProfile(sitem);
    if ( !profile || !IsCustomSuperItem(sitem) ||
         profile->push_force <= 0.0f || profile->push_radius <= 0.0f )
    {
        return;
    }

    vec3d center = World::SectorIDToCenterPos3(sitem.CellId);
    if ( sitem.PCell )
        center.y = sitem.PCell->height;

    const float radiusSq = profile->push_radius * profile->push_radius;
    if ( !std::isfinite(radiusSq) )
        return;

    std::set<NC_STACK_ypabact *> processedTargets;
    const auto applyPush = [&](NC_STACK_ypabact *target)
    {
        if ( !yw_IsCustomSuperItemPushTarget(target) ||
             target->_owner == World::OWNER_0 ||
             !processedTargets.insert(target).second )
        {
            return;
        }

        vec3d delta(target->_position.x - center.x,
                    0.0f,
                    target->_position.z - center.z);
        const float distanceSq = delta.dot(delta);
        if ( !std::isfinite(distanceSq) || distanceSq > radiusSq )
            return;

        const float distance = sqrtf(std::max(distanceSq, 0.0f));
        const float falloff = World::BlastPushFalloffFactor(
            distance, profile->push_radius, profile->push_falloff != 0);
        const float appliedForce = profile->push_force *
                                   falloff *
                                   target->GetPushResistanceMultiplier();
        if ( appliedForce <= 0.0f )
            return;

        if ( distance <= 0.001f )
            delta = vec3d(1.0f, 0.0f, 0.0f);

        target->ApplyConfiguredPush(delta, appliedForce);
    };

    // Root units and sector occupants are both visited. The set keeps the
    // detonation impulse one-shot even when the same BACT appears in both lists.
    for (NC_STACK_ypabact *target : _unitsList)
        applyPush(target);

    for (cellArea &cell : _cells)
    {
        for (NC_STACK_ypabact *target : cell.unitsList.safe_iter())
            applyPush(target);
    }
}

double yw_SuperItemMapRadius(const vec2d &mapLength)
{
    return sqrt((double)mapLength.x * mapLength.x +
                (double)mapLength.y * mapLength.y);
}

void NC_STACK_ypaworld::StartCustomSuperItemDetonation(int id)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    if ( !IsCustomSuperItem(sitem) )
        return;

    World::TSuperItemProfile &profile = _superItemProfiles[sitem.CustomProfileIndex];
    vec3d center = World::SectorIDToCenterPos3(sitem.CellId);
    if ( sitem.PCell )
        center.y = sitem.PCell->height;

    // Countdown reached 0:00. This profile-level blast is independent from
    // the propagating wave and affects every live allied or enemy unit once.
    ApplyCustomSuperItemDetonationPush(id);

    for (const World::TChainFXConfig &chain : profile.detonate_chain_fx)
    {
        if ( chain.trigger == World::TChainFXConfig::TRIGGER_DETONATE &&
             chain.mode == World::TChainFXConfig::MODE_VISUAL )
            SpawnChainFX(chain, center, mat3x3::Ident());
    }

    if ( (size_t)id >= _superItemSoundCarriers.size() )
        _superItemSoundCarriers.resize(_levelInfo.SuperItems.size());
    if ( !_superItemSoundCarriers[id] )
        _superItemSoundCarriers[id].reset(new TSndCarrier());

    profile.detonate_snd.LoadSamples();
    TSndCarrier *carrier = _superItemSoundCarriers[id].get();
    SFXEngine::SFXe.StopCarrier(carrier);
    carrier->Resize(1);
    carrier->Position = center;
    carrier->Vector = vec3d(0.0, 0.0, 0.0);

    TSoundSource &sound = carrier->Sounds[0];
    sound.PSample = profile.detonate_snd.MainSample.Sample
                  ? profile.detonate_snd.MainSample.Sample->GetSampleData()
                  : nullptr;
    sound.Volume = profile.detonate_snd.volume;
    profile.detonate_snd.ConfigureSoundSourcePitch(sound);
    sound.Radius = profile.detonate_snd.radius;
    sound.SetLoop(false);
    sound.SetFragmented(false);

    if ( profile.detonate_snd.sndPrm.slot )
    {
        sound.PPFx = &profile.detonate_snd.sndPrm;
        sound.SetPFx(true);
    }
    else
    {
        sound.PPFx = nullptr;
        sound.SetPFx(false);
    }

    if ( profile.detonate_snd.sndPrm_shk.slot )
    {
        sound.PShkFx = &profile.detonate_snd.sndPrm_shk;
        sound.SetShk(true);
    }
    else
    {
        sound.PShkFx = nullptr;
        sound.SetShk(false);
    }

    SFXEngine::SFXe.startSound(carrier, 0);
    SFXEngine::SFXe.UpdateSoundCarrier(carrier);

    StartCustomSuperItemWaveEffects(id);
}

void NC_STACK_ypaworld::StartCustomSuperItemWaveEffects(int id)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    if ( !IsCustomSuperItem(sitem) )
        return;

    World::TSuperItemProfile &profile = _superItemProfiles[sitem.CustomProfileIndex];
    if ( (size_t)id >= _superItemWaveSoundCarriers.size() )
        _superItemWaveSoundCarriers.resize(_levelInfo.SuperItems.size());
    if ( !_superItemWaveSoundCarriers[id] )
        _superItemWaveSoundCarriers[id].reset(new TSndCarrier());

    profile.wave_snd.LoadSamples();
    TSndCarrier *carrier = _superItemWaveSoundCarriers[id].get();
    SFXEngine::SFXe.StopCarrier(carrier);
    carrier->Resize(3);
    carrier->Vector = vec3d(0.0, 0.0, 0.0);

    TSoundSource &audio = carrier->Sounds[0];
    audio.PSample = profile.wave_snd.MainSample.Sample
                  ? profile.wave_snd.MainSample.Sample->GetSampleData()
                  : nullptr;
    audio.Volume = profile.wave_snd.volume;
    profile.wave_snd.ConfigureSoundSourcePitch(audio);
    audio.Radius = profile.wave_snd.radius;
    const double maximumAudioRadius = yw_SuperItemMapRadius(_mapLength);
    audio.FadeDuration = yw_SuperItemWaveTravelTimeMs(profile, maximumAudioRadius);
    audio.PPFx = nullptr;
    audio.PShkFx = nullptr;
    audio.SetPFx(false);
    audio.SetShk(false);
    audio.SetLoop(true);
    audio.SetFragmented(false);

    TSoundSource &palette = carrier->Sounds[1];
    palette.PSample = nullptr;
    palette.Volume = 0;
    palette.Pitch = 0;
    palette.Radius = 0.0f;
    palette.PShkFx = nullptr;
    palette.SetShk(false);
    palette.SetLoop(false);
    palette.SetFragmented(false);

    if ( profile.wave_snd.sndPrm.slot && profile.wave_snd.sndPrm.time > 0 )
    {
        palette.PPFx = &profile.wave_snd.sndPrm;
        palette.SetPFx(true);
    }
    else
    {
        palette.PPFx = nullptr;
        palette.SetPFx(false);
    }

    TSoundSource &shake = carrier->Sounds[2];
    shake.PSample = nullptr;
    shake.Volume = 0;
    shake.Pitch = 0;
    shake.Radius = 0.0f;
    shake.PPFx = nullptr;
    shake.SetPFx(false);
    shake.SetLoop(false);
    shake.SetFragmented(false);

    if ( profile.wave_snd.sndPrm_shk.slot && profile.wave_snd.sndPrm_shk.time > 0 )
    {
        shake.PShkFx = &profile.wave_snd.sndPrm_shk;
        shake.SetShk(true);
    }
    else
    {
        shake.PShkFx = nullptr;
        shake.SetShk(false);
    }

    UpdateCustomSuperItemWaveEffects(id);

    // Wave audio is a managed loop tied to the moving front. On load it resumes
    // at the current front instead of replaying the initial detonation sound.
    if ( audio.PSample )
        SFXEngine::SFXe.startSound(carrier, 0);

    SFXEngine::SFXe.UpdateSoundCarrier(carrier);
}

void NC_STACK_ypaworld::UpdateCustomSuperItemWaveEffects(int id)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() ||
         (size_t)id >= _superItemWaveSoundCarriers.size() ||
         !_superItemWaveSoundCarriers[id] )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    if ( !IsCustomSuperItem(sitem) )
        return;

    TSndCarrier *carrier = _superItemWaveSoundCarriers[id].get();
    if ( carrier->Sounds.empty() )
        return;

    vec3d center = World::SectorIDToCenterPos3(sitem.CellId);
    if ( sitem.PCell )
        center.y = sitem.PCell->height;

    const vec3d listener = SFXEngine::SFXe.stru_547018;
    const float dx = listener.x - center.x;
    const float dz = listener.z - center.z;
    const float distance = sqrtf(dx * dx + dz * dz);
    const float radius = (float)std::max(sitem.CurrentRadius, 0);

    if ( distance > 0.001f )
    {
        carrier->Position.x = center.x + dx * (radius / distance);
        carrier->Position.z = center.z + dz * (radius / distance);
    }
    else
    {
        carrier->Position.x = center.x + radius;
        carrier->Position.z = center.z;
    }

    // The gameplay wall has no Y limit. Matching the listener height keeps
    // SND/PAL/SHK attenuation tied only to distance from the XZ wavefront.
    carrier->Position.y = listener.y;
    carrier->Vector = vec3d(0.0, 0.0, 0.0);

    const float frontDistance = fabsf(distance - radius);
    if ( carrier->Sounds.size() >= 3 )
    {
        TSoundSource &palette = carrier->Sounds[1];
        const float paletteRadius = palette.PPFx && palette.PPFx->radius > 0.0f
                                  ? palette.PPFx->radius
                                  : 2400.0f;
        if ( !sitem.WavePalEffectStarted && radius <= distance + 0.001f && palette.IsPFx() &&
             frontDistance < paletteRadius )
        {
            SFXEngine::SFXe.startSound(carrier, 1);
            sitem.WavePalEffectStarted = true;
        }

        TSoundSource &shake = carrier->Sounds[2];
        const float shakeRadius = shake.PShkFx && shake.PShkFx->radius > 0.0f
                                ? shake.PShkFx->radius
                                : 2400.0f;
        if ( !sitem.WaveShakeEffectStarted && radius <= distance + 0.001f && shake.IsShk() &&
             frontDistance < shakeRadius )
        {
            SFXEngine::SFXe.startSound(carrier, 2);
            sitem.WaveShakeEffectStarted = true;
        }
    }

    if ( !carrier->Sounds.empty() )
    {
        const World::TSuperItemProfile &profile = _superItemProfiles[sitem.CustomProfileIndex];
        const double maximumRadius = yw_SuperItemMapRadius(_mapLength);
        const double propagationEnd = yw_SuperItemWaveTravelTimeMs(profile, maximumRadius);
        int64_t elapsed = (int64_t)_timeStamp - (int64_t)sitem.TriggerTime;
        if ( elapsed < 0 )
            elapsed = 0;

        TSoundSource &audio = carrier->Sounds[0];
        audio.Volume = profile.wave_snd.volume;

        if ( propagationEnd > 0.0 && (double)elapsed >= propagationEnd && audio.IsEnabled() )
            SFXEngine::SFXe.ForceStopSource(carrier, 0);
    }
}

void NC_STACK_ypaworld::StopCustomSuperItemWaveEffects(int id)
{
    if ( id < 0 || (size_t)id >= _superItemWaveSoundCarriers.size() ||
         !_superItemWaveSoundCarriers[id] )
        return;

    SFXEngine::SFXe.StopCarrier(_superItemWaveSoundCarriers[id].get());
    _superItemWaveSoundCarriers[id]->Clear();
    if ( (size_t)id < _levelInfo.SuperItems.size() )
    {
        _levelInfo.SuperItems[id].WavePalEffectStarted = false;
        _levelInfo.SuperItems[id].WaveShakeEffectStarted = false;
    }
}

double yw_SuperItemWaveRadiusAtElapsed(const World::TSuperItemProfile &profile,
                                               double elapsedMilliseconds)
{
    if ( !std::isfinite(elapsedMilliseconds) || elapsedMilliseconds <= 0.0 )
        return 0.0;

    const double elapsed = elapsedMilliseconds / 1000.0;
    const double startSpeed = profile.wave_start_speed;
    const double endSpeed = profile.wave_end_speed;
    const double rampTime = (double)profile.wave_speed_ramp_time / 1000.0;

    if ( rampTime <= 0.0 )
        return elapsed * endSpeed;

    if ( elapsed <= rampTime )
    {
        const double acceleration = (endSpeed - startSpeed) / rampTime;
        return startSpeed * elapsed + 0.5 * acceleration * elapsed * elapsed;
    }

    const double rampDistance = 0.5 * (startSpeed + endSpeed) * rampTime;
    return rampDistance + endSpeed * (elapsed - rampTime);
}

double yw_SuperItemWaveTravelTimeMs(const World::TSuperItemProfile &profile,
                                            double radius)
{
    if ( !std::isfinite(radius) || radius <= 0.0 )
        return 0.0;

    const double startSpeed = profile.wave_start_speed;
    const double endSpeed = profile.wave_end_speed;
    const double rampTime = (double)profile.wave_speed_ramp_time / 1000.0;
    if ( rampTime <= 0.0 )
        return endSpeed > 0.0 ? radius * 1000.0 / endSpeed : 0.0;

    const double rampDistance = 0.5 * (startSpeed + endSpeed) * rampTime;
    if ( radius >= rampDistance )
        return (rampTime + (radius - rampDistance) / endSpeed) * 1000.0;

    const double acceleration = (endSpeed - startSpeed) / rampTime;
    double time = 0.0;
    if ( fabs(acceleration) < 0.0000001 )
    {
        if ( startSpeed <= 0.0 )
            return 0.0;
        time = radius / startSpeed;
    }
    else
    {
        const double discriminant = std::max(0.0,
            startSpeed * startSpeed + 2.0 * acceleration * radius);
        const double root = sqrt(discriminant);
        const double stableDenominator = startSpeed + root;

        if ( stableDenominator > 0.0 )
            time = 2.0 * radius / stableDenominator;
        else
            time = (-startSpeed + root) / acceleration;
    }

    if ( !std::isfinite(time) || time < 0.0 )
        return 0.0;
    if ( time > rampTime )
        time = rampTime;
    return time * 1000.0;
}

void NC_STACK_ypaworld::UpdateCustomSuperItemWaveVisual(TMapSuperItem &sitem,
                                                     const World::TSuperItemProfile &profile,
                                                     double fadeElapsed,
                                                     double fadeDuration)
{
    if ( sitem.CurrentRadius <= 0 )
        return;

    vec3d center = World::SectorIDToCenterPos3(sitem.CellId);
    if ( sitem.PCell )
        center.y = sitem.PCell->height;
    static constexpr float WAVE_VP_BASE_RADIUS = 300.0f;
    float factor = (float)sitem.CurrentRadius / WAVE_VP_BASE_RADIUS;
    vec3d runtimeScale(profile.wave_axis_scale.x * factor,
                       profile.wave_axis_scale.y,
                       profile.wave_axis_scale.z * factor);

    World::TVisualTint runtimeTint = profile.wave_tint;

    if ( sitem.WaveTransientVPId > 0 )
    {
        for (TTransientVP &effect : _transientVPs)
        {
            if ( effect.id == sitem.WaveTransientVPId )
            {
                effect.pos = center;
                effect.axisScale = runtimeScale;
                effect.tint = runtimeTint;
                yw_ConfigureTransientVPFade(effect,
                                            profile.fade_in,
                                            profile.fade_out,
                                            fadeDuration,
                                            fadeElapsed);
                return;
            }
        }
        sitem.WaveTransientVPId = 0;
    }

    sitem.WaveTransientVPId = SpawnTransientVisual(profile.wave_vp, profile.wave_3ds,
                                                   profile.wave_base, center,
                                                   mat3x3::Ident(), 0, 1.0f,
                                                   runtimeTint, runtimeScale);
    if ( sitem.WaveTransientVPId > 0 && !_transientVPs.empty() &&
         _transientVPs.back().id == sitem.WaveTransientVPId )
    {
        yw_ConfigureTransientVPFade(_transientVPs.back(),
                                    profile.fade_in,
                                    profile.fade_out,
                                    fadeDuration,
                                    fadeElapsed);
    }
}

void NC_STACK_ypaworld::RestoreCustomSuperItemRuntimeAfterLoad()
{
    const double mapRadius = yw_SuperItemMapRadius(_mapLength);

    for (size_t id = 0; id < _levelInfo.SuperItems.size(); ++id)
    {
        TMapSuperItem &sitem = _levelInfo.SuperItems[id];
        if ( sitem.State != TMapSuperItem::STATE_TRIGGED || !IsCustomSuperItem(sitem) )
            continue;

        const World::TSuperItemProfile *profile = GetSuperItemProfile(sitem);
        if ( !profile )
            continue;

        const double maximumRadius = mapRadius;
        const int32_t maximum = (int32_t)ceil(std::min(maximumRadius,
                                                       (double)std::numeric_limits<int32_t>::max()));
        int64_t elapsed = (int64_t)_timeStamp - (int64_t)sitem.TriggerTime;
        if ( elapsed < 0 )
            elapsed = 0;

        const double propagationEnd = yw_SuperItemWaveTravelTimeMs(*profile, maximumRadius);
        const double waveVisualEnd = propagationEnd + profile->fade_out;
        const bool propagationActive = sitem.CurrentRadius < maximum &&
                                       (double)elapsed < propagationEnd;
        const bool fadeOutActive = sitem.CurrentRadius >= maximum &&
                                   profile->fade_out > 0 &&
                                   (double)elapsed < waveVisualEnd;

        if ( propagationActive || fadeOutActive )
            UpdateCustomSuperItemWaveVisual(sitem, *profile, (double)elapsed, waveVisualEnd);

        if ( propagationActive )
            StartCustomSuperItemWaveEffects((int)id);
    }
}

uint32_t yw_SuperItemBuildingHash(const World::TSuperItemProfile &profile,
                                         const TMapSuperItem &sitem,
                                         const cellArea &cell,
                                         int bldX, int bldY)
{
    uint32_t hash = 2166136261u;
    auto mixByte = [&hash](uint8_t value)
    {
        hash ^= value;
        hash *= 16777619u;
    };
    auto mixInt = [&mixByte](int value)
    {
        uint32_t raw = (uint32_t)value;
        mixByte((uint8_t)(raw & 0xff));
        mixByte((uint8_t)((raw >> 8) & 0xff));
        mixByte((uint8_t)((raw >> 16) & 0xff));
        mixByte((uint8_t)((raw >> 24) & 0xff));
    };

    for (unsigned char ch : profile.name)
        mixByte(ch);
    mixInt(sitem.CellId.x);
    mixInt(sitem.CellId.y);
    mixInt(cell.CellId.x);
    mixInt(cell.CellId.y);
    mixInt(bldX);
    mixInt(bldY);
    return hash;
}

void NC_STACK_ypaworld::ApplyCustomSuperItemFront(int id, float lastRadius, float currentRadius)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() || currentRadius <= lastRadius )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    if ( !IsCustomSuperItem(sitem) )
        return;

    World::TSuperItemProfile &profile = _superItemProfiles[sitem.CustomProfileIndex];
    vec2d center = World::SectorIDToCenterPos2(sitem.CellId);
    const float lastRadiusSq = lastRadius * lastRadius;
    const float currentRadiusSq = currentRadius * currentRadius;

    NC_STACK_ypabact *source = yw_getHostByOwner((uint8_t)sitem.ActivateOwner);
    if ( source && (source->_energy <= 0 || source->_status == BACT_STATUS_DEAD ||
                    (source->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2))) )
        source = nullptr;

    // De-duplicate the sector lists within this update. Damage and debuff may
    // still be applied again if a displaced unit crosses a later front, but
    // wave_push_force is persistently limited to one impulse per unit.
    std::set<NC_STACK_ypabact *> processedTargets;

    for (cellArea &cell : _cells)
    {
        for (NC_STACK_ypabact *target : cell.unitsList.safe_iter())
        {
            if ( !target || !processedTargets.insert(target).second )
                continue;

            float dx = center.x - target->_position.x;
            float dz = center.y - target->_position.z;
            float distanceSq = dx * dx + dz * dz;
            if ( distanceSq <= lastRadiusSq || distanceSq > currentRadiusSq )
                continue;

            if ( target->_energy <= 0 || target->_status == BACT_STATUS_DEAD ||
                 (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
                continue;

            const bool canReceiveDamageAndDebuff =
                target->_owner != World::OWNER_0 &&
                target->_owner != sitem.ActivateOwner &&
                !target->IsInvulnerableToDamage();

            const int32_t targetGid = (int32_t)target->_gid;
            const bool alreadyWavePushed =
                targetGid <= 0 ||
                std::find(sitem.CustomHitUnitGids.begin(),
                          sitem.CustomHitUnitGids.end(),
                          targetGid) != sitem.CustomHitUnitGids.end();

            // Push eligibility is owner-agnostic and separate from damage and
            // debuff eligibility. Allies and invulnerable units receive the
            // same configured mechanical push; only GUN actors are excluded by
            // type. Contact with this front is already the activation test.
            // A valid GID is required so the one-push protection can be saved.
            float appliedPushForce = 0.0f;
            vec3d pushDir(0.0f, 0.0f, 0.0f);
            if ( target->_owner != World::OWNER_0 &&
                  target->CanReceiveConfiguredPush() &&
                  !alreadyWavePushed &&
                  profile.wave_push_force > 0.0f )
            {
                appliedPushForce = profile.wave_push_force *
                                   target->GetPushResistanceMultiplier();
                if ( appliedPushForce > 0.0f )
                {
                    pushDir = vec3d(target->_position.x - center.x,
                                    0.0f,
                                    target->_position.z - center.y);
                    if ( pushDir.length() <= 0.001f )
                        pushDir = vec3d(1.0f, 0.0f, 0.0f);
                }
            }

            if ( canReceiveDamageAndDebuff )
            {
                int damage = target->CalcShieldedCustomDamage(profile.wave_unit_energy);
                if ( damage > 0 )
                {
                    bact_arg84 damageArg;
                    damageArg.energy = -damage;
                    damageArg.unit = source;
                    damageArg.bypassAttackerDamageModifiers = true;
                    target->ModifyEnergy(&damageArg);
                }
            }

            // Keep the weapon push ordering: a valid contact still receives the
            // impulse when the same wave hit is lethal. Friendly damage remains
            // disabled independently from this owner-agnostic push.
            if ( appliedPushForce > 0.0f )
            {
                target->ApplyConfiguredPush(pushDir, appliedPushForce);
                sitem.CustomHitUnitGids.push_back(targetGid);
            }

            if ( canReceiveDamageAndDebuff && profile.debuff.valid &&
                 target->_energy > 0 && target->_status != BACT_STATUS_DEAD &&
                 !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
                target->ApplyDebuff(profile.debuff, source, (int16_t)sitem.ActivateOwner);
        }
    }

    for (cellArea &cell : _cells)
    {
        if ( !cell.IsGamePlaySector() || cell.PurposeType == cellArea::PT_CONSTRUCTING ||
             &cell == sitem.PCell )
            continue;

        int slotCount = cell.SectorType == 1 ? 1 : 3;
        for (int bldY = 0; bldY < slotCount; ++bldY)
        {
            for (int bldX = 0; bldX < slotCount; ++bldX)
            {
                int buildingKey = cell.Id * 9 + bldY * 3 + bldX;
                if ( buildingKey < 0 || (size_t)buildingKey >= sitem.CustomHitBuildingSlots.size() ||
                     sitem.CustomHitBuildingSlots[buildingKey] )
                    continue;

                vec3d buildingPos = World::SectorIDToCenterPos3(cell.CellId);
                buildingPos.y = cell.height;
                if ( cell.SectorType != 1 )
                {
                    buildingPos.x += (bldX - 1) * 300.0f;
                    buildingPos.z += (bldY - 1) * 300.0f;
                }

                float dx = center.x - buildingPos.x;
                float dz = center.y - buildingPos.z;
                float distanceSq = dx * dx + dz * dz;
                if ( distanceSq <= lastRadiusSq || distanceSq > currentRadiusSq )
                    continue;

                sitem.CustomHitBuildingSlots[buildingKey] = 1;

                int currentHealth = cell.buildings_health.At(bldX, bldY);
                if ( currentHealth <= 0 )
                    continue;

                TSubSectorDesc *subSector = _secTypeArray[cell.type_id].SubSectors.At(bldX, bldY);
                if ( !subSector )
                    continue;

                int currentLego = GetLegoBld(&cell, bldX, bldY);
                if ( currentLego < 0 || currentLego >= (int)_legoArray.size() ||
                     _legoArray[currentLego].Shield >= 100 )
                    continue;

                uint8_t currentVisual = subSector->HPModels[_buildHealthModelId[currentHealth]];
                bool visuallyPerfect = currentVisual == subSector->HPModels[0];
                int targetHealth = 0;

                if ( visuallyPerfect )
                {
                    bool destroy = profile.wave_building_total_destruction >= 100;
                    if ( profile.wave_building_total_destruction > 0 &&
                         profile.wave_building_total_destruction < 100 )
                    {
                        destroy = (yw_SuperItemBuildingHash(profile, sitem, cell, bldX, bldY) % 100u) <
                                  (uint32_t)profile.wave_building_total_destruction;
                    }

                    if ( !destroy )
                    {
                        uint8_t destroyedVisual = subSector->HPModels[3];
                        uint8_t perfectVisual = subSector->HPModels[0];
                        if ( subSector->HPModels[2] != destroyedVisual &&
                             subSector->HPModels[2] != perfectVisual )
                            targetHealth = 100;
                        else if ( subSector->HPModels[1] != destroyedVisual &&
                                  subSector->HPModels[1] != perfectVisual )
                            targetHealth = 200;
                    }
                }

                yw_arg129 buildingArg;
                buildingArg.field_0 = 0;
                buildingArg.pos = buildingPos;
                buildingArg.field_10 = 0;
                buildingArg.OwnerID = sitem.ActivateOwner;
                buildingArg.unit = source;

                int oldFxLimit = _fxLimit;
                _fxLimit = 2;
                ApplyBuildingHealthChange(&cell, bldX, bldY, targetHealth, &buildingArg);
                _fxLimit = oldFxLimit;
            }
        }
    }
}

void NC_STACK_ypaworld::UpdateCustomSuperItem(int id)
{
    if ( id < 0 || (size_t)id >= _levelInfo.SuperItems.size() )
        return;

    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    const World::TSuperItemProfile *profile = GetSuperItemProfile(sitem);
    if ( !profile || !IsCustomSuperItem(sitem) )
        return;

    int64_t elapsed = (int64_t)_timeStamp - (int64_t)sitem.TriggerTime;
    if ( elapsed < 0 )
        elapsed = 0;

    double calculatedRadius = yw_SuperItemWaveRadiusAtElapsed(*profile, (double)elapsed);
    const double maximumRadius = yw_SuperItemMapRadius(_mapLength);
    bool reachedMaximum = calculatedRadius >= maximumRadius;
    if ( calculatedRadius > maximumRadius )
        calculatedRadius = maximumRadius;
    if ( calculatedRadius > (double)std::numeric_limits<int32_t>::max() )
        calculatedRadius = (double)std::numeric_limits<int32_t>::max();

    int32_t maximum = (int32_t)ceil(std::min(maximumRadius,
                                             (double)std::numeric_limits<int32_t>::max()));
    int32_t computed = reachedMaximum ? maximum : (int32_t)calculatedRadius;
    sitem.CurrentRadius = std::max(sitem.CurrentRadius, computed);
    sitem.CurrentRadius = std::max(0, std::min(sitem.CurrentRadius, maximum));
    sitem.LastRadius = std::max(0, std::min(sitem.LastRadius, sitem.CurrentRadius));

    if ( sitem.CurrentRadius > sitem.LastRadius )
        ApplyCustomSuperItemFront(id, (float)sitem.LastRadius, (float)sitem.CurrentRadius);

    UpdateCustomSuperItemWaveEffects(id);

    const double propagationEnd = yw_SuperItemWaveTravelTimeMs(*profile, maximumRadius);
    const double waveVisualEnd = propagationEnd + profile->fade_out;

    if ( sitem.CurrentRadius >= maximum )
    {
        const bool fadeOutActive = profile->fade_out > 0 &&
                                   (double)elapsed < waveVisualEnd;
        if ( fadeOutActive )
            UpdateCustomSuperItemWaveVisual(sitem, *profile, (double)elapsed, waveVisualEnd);
        else
        {
            if ( sitem.WaveTransientVPId > 0 )
                RemoveTransientVP(sitem.WaveTransientVPId);
            sitem.WaveTransientVPId = 0;
        }
    }
    else
        UpdateCustomSuperItemWaveVisual(sitem, *profile, (double)elapsed, waveVisualEnd);
    sitem.LastRadius = sitem.CurrentRadius;
}
