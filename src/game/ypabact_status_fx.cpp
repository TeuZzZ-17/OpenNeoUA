// Debuffs, damaged/decoration/energy status effects and their sound carriers.
#include "game/ypabact_internal.h"

void ypabact_ResetDamagedFX(NC_STACK_ypabact *bact)
{
    bact->_damaged_fx = World::TDamagedFXConfig();
    bact->_damaged_fx_next_time = 0;
}

bool ypabact_IsDamagedFXSystemDisabled(const NC_STACK_ypabact *bact)
{
    return !bact || !bact->_damaged_fx.threshold.defined ||
           bact->_damaged_fx.threshold.value <= 0.0f;
}

bool ypabact_CanSpawnDecorationFX(NC_STACK_ypabact *bact)
{
    // OpenNeoUA invisible: cloaked stealth units spawn no visible/audible decoration FX.
    return bact &&
           !bact->IsInvisibleUnrevealed() &&
           bact->getBACT_pWorld() &&
           bact->_energy > 0 &&
           bact->_status != BACT_STATUS_DEAD &&
           bact->_status != BACT_STATUS_CREATE &&
           bact->_status != BACT_STATUS_BEAM &&
           !(bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER));
}

bool ypabact_IsDamagedStateActive(const NC_STACK_ypabact *bact)
{
    if ( ypabact_IsDamagedFXSystemDisabled(bact) || bact->_energy <= 0 || bact->_energy_max <= 0 )
        return false;

    const float thresholdEnergy = ypabact_GetDamagedThresholdEnergy(bact);
    return thresholdEnergy > 0.0f && (float)bact->_energy <= thresholdEnergy;
}

float ypabact_DebuffMalusToMult(float malus)
{
    if ( malus < 0.0 )
        malus = 0.0;
    else if ( malus > 1.0 )
        malus = 1.0;

    return 1.0 - malus;
}

vec3d ypabact_BuildLegacyAttachedFXOffset(float overeof)
{
    vec3d localOffset(0.0, 0.0, 0.0);
    float heightOffset = overeof * 0.25;
    if ( heightOffset < 5.0 )
        heightOffset = 5.0;
    else if ( heightOffset > 60.0 )
        heightOffset = 60.0;

    localOffset.y -= heightOffset;
    return localOffset;
}

vec3d ypabact_BuildUniformStatusFXScale(float scale)
{
    float safeScale = scale > 0.0f ? scale : 1.0f;
    return vec3d(safeScale, safeScale, safeScale);
}

bool ypabact_BuildAttachedFXOffset(NC_STACK_ypabact *bact,
                                          const World::TAbsoluteOrPercent &randomMaxOffset,
                                          vec3d *localOffset)
{
    if ( !bact || !localOffset )
        return false;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( world && world->SampleAttachedFXLocalPosition(bact, randomMaxOffset, localOffset) )
        return true;

    *localOffset = ypabact_BuildLegacyAttachedFXOffset(bact->_overeof);
    return false;
}

int ypabact_GetDebuffFXLifetime(const TActiveDebuffState &debuff)
{
    int lifetime = debuff.tick_time + 100;

    if ( lifetime < 1000 )
        lifetime = 1000;
    else if ( lifetime > 5000 )
        lifetime = 5000;

    return lifetime;
}

void ypabact_SpawnDebuffFXEvent(NC_STACK_ypabact *bact, const TActiveDebuffState &debuff, int lifetime)
{
    if ( !bact || !debuff.active )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    const vec3d effectScale = ypabact_BuildUniformStatusFXScale(debuff.visual_scale);
    const World::TVisualTint *trailTint = debuff.has_vp_trail_tint
        ? &debuff.vp_trail_tint : nullptr;

    for (int16_t fxVp : debuff.vps)
    {
        if ( fxVp <= 0 )
            continue;

        vec3d localOffset;
        bool rotateOffset = ypabact_BuildAttachedFXOffset(
            bact, debuff.random_max_offset, &localOffset);
        world->SpawnAttachedStatusTransientVP(fxVp, bact, localOffset, lifetime,
                                              false, rotateOffset, effectScale,
                                              debuff.tint, trailTint);
    }

    if ( !debuff.mesh3ds.empty() )
    {
        vec3d localOffset;
        bool rotateOffset = ypabact_BuildAttachedFXOffset(
            bact, debuff.random_max_offset, &localOffset);
        world->SpawnAttachedStatusTransientMesh(
            debuff.mesh3ds, bact, localOffset, lifetime,
            rotateOffset, effectScale, debuff.tint);
    }
}

void ypabact_ApplyDamagedRuntime(NC_STACK_ypabact *bact, bool active)
{
    bact->_damaged_fx_active = active;

    float forceMult = active ? ypabact_DebuffMalusToMult(bact->_damaged_force_malus) : 1.0;
    float maxrotMult = active ? ypabact_DebuffMalusToMult(bact->_damaged_maxrot_malus) : 1.0;

    if ( bact->_active_debuff.active )
    {
        forceMult *= ypabact_DebuffMalusToMult(bact->_active_debuff.force_malus);
        maxrotMult *= ypabact_DebuffMalusToMult(bact->_active_debuff.maxrot_malus);
    }

    // Elite medal bonuses use the same non-compounding runtime multiplier chain
    // as damaged/debuff effects. The immutable per-instance bases remain intact.
    const float eliteStatMult = bact->GetEliteStatMultiplier();
    forceMult *= eliteStatMult;
    maxrotMult *= eliteStatMult;

    bact->_force = bact->_base_force * forceMult;
    bact->_maxrot = bact->_base_maxrot * maxrotMult;
}

int ypabact_ScaledPitch(TSoundSource &snd, int basePitch, float mult)
{
    if ( mult == 1.0 )
        return basePitch;

    if ( snd.PSample && snd.PSample->SampleRate > 0 )
    {
        float rate = (float)(snd.PSample->SampleRate + basePitch) * mult;

        // Very low or invalid playback rates can make some looping vehicle sounds
        // effectively disappear, especially heli/hover idle loops. Keep the
        // effective rate valid while still allowing obvious pitch reduction.
        float minRate = (float)snd.PSample->SampleRate * 0.10f;
        if ( !isnormal(rate) || rate < minRate )
            rate = minRate;

        return (int)rate - snd.PSample->SampleRate;
    }

    return (int)(basePitch * mult);
}

void ypabact_ApplyDamagedSoundPitch(NC_STACK_ypabact *bact)
{
    if ( bact->_soundcarrier.Sounds.size() <= World::TVhclProto::SND_WAIT )
        return;

    float pitchMult = 1.0;
    float firePitchMult = 1.0;

    if ( bact->_damaged_fx_active )
        pitchMult *= ypabact_SafeDamageMult(bact->_damaged_snd_pitch_multiplier);

    if ( bact->_active_debuff.active )
    {
        float debuffPitchMult = ypabact_SafeDamageMult(bact->_active_debuff.snd_pitch_multiplier);
        pitchMult *= debuffPitchMult;
        firePitchMult *= debuffPitchMult;
    }

    TSoundSource &normal = bact->_soundcarrier.Sounds[World::TVhclProto::SND_NORMAL];
    TSoundSource &fire = bact->_soundcarrier.Sounds[World::TVhclProto::SND_FIRE];
    TSoundSource &wait = bact->_soundcarrier.Sounds[World::TVhclProto::SND_WAIT];

    if ( pitchMult != 1.0 || firePitchMult != 1.0 )
    {
        normal.Pitch = ypabact_ScaledPitch(normal, normal.Pitch, pitchMult);
        fire.Pitch = ypabact_ScaledPitch(fire, fire.PitchBase, firePitchMult);

        // SND_WAIT can be the active loop for heli hover/idle. Unlike SND_NORMAL,
        // SND_FIRE/SND_WAIT are not always rebuilt by Move(), so scale them from
        // the prototype base pitch instead of repeatedly scaling the previous
        // frame's pitch.
        wait.Pitch = ypabact_ScaledPitch(wait, wait.PitchBase, pitchMult);
    }
}

void ypabact_StartStatusSoundIfIdle(NC_STACK_ypabact *bact, TSndCarrier *carrier, int volume, int pitch)
{
    if ( !bact || !carrier || carrier->Sounds.empty() )
        return;

    carrier->Position = bact->_position;
    carrier->Vector = bact->_fly_dir * bact->_fly_dir_length;

    TSoundSource &snd = carrier->Sounds[0];
    snd.Volume = volume;
    snd.Pitch = pitch;
    snd.PriorityBias = 0;

    if ( !snd.IsEnabled() && !snd.IsPFxEnabled() && !snd.IsShkEnabled() )
    {
        SFXEngine::SFXe.startSound(carrier, 0);
        SFXEngine::SFXe.UpdateSoundCarrier(carrier);
    }
}

void NC_STACK_ypabact::ClearActiveDebuff()
{
    ResetDebuffState();
    SFXEngine::SFXe.StopCarrier(&_debuff_soundcarrier);
}

// Combined view of every Debuff on the unit: each malus is multiplied (several Debuffs never
// pass 100%), the strongest stun wins, the newest tint wins. The stun runtime (phase, timers)
// lives only here and survives a rebuild until the stun really starts or ends.
void NC_STACK_ypabact::RebuildActiveDebuffAggregate()
{
    if ( _debuff_list.empty() )
    {
        _active_debuff.Clear();
        return;
    }

    static float TActiveDebuffState::* const malusFields[] =
    {
        &TActiveDebuffState::force_malus, &TActiveDebuffState::maxrot_malus,
        &TActiveDebuffState::shield_malus, &TActiveDebuffState::mgun_shot_time_malus,
        &TActiveDebuffState::shot_time_malus, &TActiveDebuffState::damage_malus,
        &TActiveDebuffState::regen_malus
    };
    constexpr int malusCount = sizeof(malusFields) / sizeof(malusFields[0]);

    float mult[malusCount];
    std::fill(mult, mult + malusCount, 1.0f);

    float pitch = 1.0f;
    bool stun = false;
    bool stunUnitFire = true;
    float stunLevel = 0.0f;
    int expireTime = 0;
    World::TVisualTint targetTint;

    for (const TActiveDebuffState &entry : _debuff_list)
    {
        for (int i = 0; i < malusCount; i++)
            mult[i] *= ypabact_DebuffMalusToMult(entry.*malusFields[i]);

        pitch *= entry.snd_pitch_multiplier;
        expireTime = std::max(expireTime, entry.expire_time);

        if ( entry.stun )
        {
            stun = true;
            stunLevel = std::max(stunLevel, entry.stun_motion_level);
            stunUnitFire = stunUnitFire && entry.stun_unit_fire;
        }

        if ( !entry.target_tint.IsNeutral() )
            targetTint = entry.target_tint;
    }

    TActiveDebuffState &agg = _active_debuff;
    const bool wasStunMoving = agg.active && agg.stun && agg.stun_motion_level > 0.0f;

    agg.active = true;
    agg.expire_time = expireTime;
    for (int i = 0; i < malusCount; i++)
        agg.*malusFields[i] = 1.0f - mult[i];
    agg.snd_pitch_multiplier = pitch;
    agg.target_tint = targetTint;
    agg.stun = stun;
    agg.stun_unit_fire = stunUnitFire;
    agg.stun_motion_level = stunLevel;

    if ( stunLevel <= 0.0f || (stun && !wasStunMoving) )
    {
        agg.stun_move_phase = 0;
        agg.stun_next_move_time = 0;
        agg.stun_floor_close = false;
        agg.stun_next_floor_check_time = 0;
    }
}

void NC_STACK_ypabact::ApplyDebuff(World::TWeaponDebuffConfig &debuff, NC_STACK_ypabact *source, int16_t sourceOwner)
{
    if ( !debuff.valid || debuff.duration <= 0 )
        return;

    const bool hostStation = _bact_type == BACT_TYPES_ROBO;
    if ( hostStation && !debuff.allow_on_host_station )
        return;

    if ( !ypabact_CanUseGameplayStatusMechanics(this) )
        return;

    // One entry per profile: another profile never replaces or cancels it. The same profile
    // re-applied is a refresh. It must never delay or interrupt what the Debuff is already
    // doing: damage ticks and sound keep running even while the target is under continuous fire.
    size_t foundIndex = _debuff_list.size();
    for (size_t i = 0; i < _debuff_list.size(); i++)
    {
        if ( _debuff_list[i].profile_id == debuff.profile_id )
        {
            foundIndex = i;
            break;
        }
    }

    const bool refreshActive = foundIndex < _debuff_list.size();
    if ( refreshActive )
    {
        // The list is ordered by last application ("newest tint" and the sound owner follow it).
        std::rotate(_debuff_list.begin() + foundIndex, _debuff_list.begin() + foundIndex + 1, _debuff_list.end());
    }
    else
        _debuff_list.emplace_back();

    TActiveDebuffState &entry = _debuff_list.back();
    const int pendingTickTime = entry.next_tick_time;

    // stack_max: a hit by the same profile on a unit already carrying it adds one layer.
    // Every malus is its per-layer value x layers, still capped at 100%.
    int stackLayers = 1;
    if ( debuff.stack_max > 0 && refreshActive )
        stackLayers = std::min(entry.stack_count + 1, debuff.stack_max);
    const auto stackedMalus = [stackLayers](float perLayer)
    {
        return std::max(0.0f, std::min(perLayer * (float)stackLayers, 1.0f));
    };

    const bool canMindcontrol = !hostStation && debuff.mindcontrol &&
                               ypabact_CanBeMindcontrolled(this, source);
    const float stunMotionLevel = hostStation ? 0.0f :
        std::max(0.0f, std::min(debuff.stun_motion_level, 1.0f));

    entry.active = true;
    entry.allow_on_children = debuff.allow_on_children;
    entry.name = debuff.name.empty() ? "debuff" : debuff.name;
    entry.icon = debuff.icon;
    entry.energy = debuff.energy;
    entry.tick_time = debuff.tick_time > 0 ? debuff.tick_time : 1000;
    entry.expire_time = _clock + debuff.duration;
    entry.next_tick_time = _clock + entry.tick_time;

    // A refresh must never postpone a tick already scheduled: fast repeated
    // hits would keep pushing the damage tick away and the Debuff would never
    // deal its defined damage while the target is being hit. Keep the earliest
    // pending tick so the defined cadence always runs.
    if ( refreshActive && pendingTickTime < entry.next_tick_time )
        entry.next_tick_time = pendingTickTime;

    entry.stun = !hostStation && debuff.stun;
    entry.stun_motion_level = stunMotionLevel;
    entry.stun_unit_fire = hostStation ? true : debuff.stun_unit_fire;
    entry.force_malus = stackedMalus(debuff.force_malus);
    entry.maxrot_malus = stackedMalus(debuff.maxrot_malus);
    entry.shield_malus = stackedMalus(debuff.shield_malus);
    entry.mgun_shot_time_malus = stackedMalus(debuff.mgun_shot_time_malus);
    entry.shot_time_malus = stackedMalus(debuff.shot_time_malus);
    entry.damage_malus = stackedMalus(debuff.damage_malus);
    entry.regen_malus = stackedMalus(debuff.regen_malus);
    entry.profile_id = debuff.profile_id;
    entry.stack_max = debuff.stack_max;
    entry.stack_count = debuff.stack_max > 0 ? stackLayers : 0;
    entry.snd_pitch_multiplier = ypabact_SafeDamageMult(debuff.snd_pitch_multiplier);
    entry.target_tint = debuff.target_tint;
    entry.vps = debuff.vps;
    entry.mesh3ds = debuff.mesh3ds;
    entry.visual_scale = debuff.visual_scale;
    entry.tint = debuff.tint;
    entry.random_max_offset = debuff.random_max_offset;
    entry.vp_trail_tint = debuff.vp_trail_tint;
    entry.has_vp_trail_tint = debuff.has_vp_trail_tint;
    entry.source_gid = source ? source->_gid : 0;
    int16_t resolvedSourceOwner = source ? source->_owner : sourceOwner;
    if ( resolvedSourceOwner < World::OWNER_0 || resolvedSourceOwner > World::OWNER_7 )
        resolvedSourceOwner = World::OWNER_0;
    entry.source_owner = resolvedSourceOwner;
    entry.snd_sample = nullptr;
    entry.snd_volume = debuff.tick_snd.volume ? debuff.tick_snd.volume : 120;
    entry.snd_pitch = debuff.tick_snd.pitch_min;

    if ( debuff.tick_snd.MainSample.Sample )
        entry.snd_sample = debuff.tick_snd.MainSample.Sample->GetSampleData();

    RebuildActiveDebuffAggregate();

    // All Debuffs share one sound channel, owned by the last one applied with a sound.
    // A silent Debuff leaves the channel alone, so it can never cut another Debuff's sound.
    TSndFXParam *sndPFx = debuff.tick_snd.sndPrm.slot ? &debuff.tick_snd.sndPrm : nullptr;
    TSndFxPosParam *sndShkFx = debuff.tick_snd.sndPrm_shk.slot ? &debuff.tick_snd.sndPrm_shk : nullptr;
    const bool loopDebuffSound = !debuff.has_tick_time && entry.snd_sample;

    if ( entry.snd_sample || sndPFx || sndShkFx )
    {
        if ( _debuff_soundcarrier.Sounds.empty() )
            _debuff_soundcarrier.Resize(1);

        TSoundSource &snd = _debuff_soundcarrier.Sounds[0];

        // A refresh must not interrupt a Debuff sound already playing with the same
        // setup: repeated hits would restart it every time and its defined sound
        // would never really play. Only a real setup change rebuilds the carrier.
        const bool soundRunning = snd.IsEnabled() || snd.IsPFxEnabled() || snd.IsShkEnabled();
        const bool keepRunningSound = refreshActive && soundRunning &&
                                      _debuff_sound_owner == entry.profile_id &&
                                      snd.PSample == entry.snd_sample &&
                                      snd.IsLoop() == loopDebuffSound &&
                                      snd.PPFx == sndPFx && snd.PShkFx == sndShkFx;

        if ( !keepRunningSound )
        {
            SFXEngine::SFXe.StopCarrier(&_debuff_soundcarrier);

            snd.PSample = entry.snd_sample;
            snd.Volume = entry.snd_volume;
            debuff.tick_snd.ConfigureSoundSource(snd);
            snd.Radius = debuff.tick_snd.radius;
            snd.PriorityBias = 0;
            snd.SetLoop(loopDebuffSound);
            snd.SetFragmented(false);

            snd.PPFx = sndPFx;
            snd.SetPFx(sndPFx != nullptr);

            snd.PShkFx = sndShkFx;
            snd.SetShk(sndShkFx != nullptr);
        }

        _debuff_sound_owner = entry.profile_id;
    }

    if ( loopDebuffSound )
        ypabact_StartStatusSoundIfIdle(this, &_debuff_soundcarrier,
                                       entry.snd_volume,
                                       entry.snd_pitch);

    // Last use of the entry reference: mind control reshuffles ownership and unit lists.
    ypabact_SpawnDebuffFXEvent(this, entry, ypabact_GetDebuffFXLifetime(entry));

    if ( canMindcontrol )
        ypabact_ApplyMindcontrol(this, source);
}

void NC_STACK_ypabact::InheritActiveDebuffFromParent(NC_STACK_ypabact *parent)
{
    NC_STACK_ypaworld *world = getBACT_pWorld();

    if ( !parent || parent == this ||
         !world ||
         parent->getBACT_pWorld() != world ||
         world->_isNetGame ||
         parent->_debuff_list.empty() ||
         parent->_energy <= 0 ||
         parent->_status == BACT_STATUS_DEAD ||
         (parent->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) ||
         !ypabact_CanUseGameplayStatusMechanics(this, true) )
        return;

    std::vector<TActiveDebuffState> inherited;
    for (const TActiveDebuffState &parentEntry : parent->_debuff_list)
    {
        const int remainingDuration = parentEntry.expire_time - parent->_clock;
        if ( !parentEntry.allow_on_children || remainingDuration <= 0 )
            continue;

        const int remainingTickDelay = std::max(0, parentEntry.next_tick_time - parent->_clock);
        inherited.push_back(parentEntry);
        inherited.back().expire_time = _clock + remainingDuration;
        inherited.back().next_tick_time = _clock + remainingTickDelay;
    }

    if ( inherited.empty() )
        return;

    TSoundSource *sourceSound = parent->_debuff_soundcarrier.Sounds.empty()
        ? nullptr : &parent->_debuff_soundcarrier.Sounds[0];

    ClearActiveDebuff();
    _debuff_list = inherited;
    RebuildActiveDebuffAggregate();

    // The shared sound is copied only when the parent's channel owner is one of the inherited Debuffs.
    const TActiveDebuffState *soundEntry = nullptr;
    for (const TActiveDebuffState &entry : _debuff_list)
    {
        if ( entry.profile_id == parent->_debuff_sound_owner )
            soundEntry = &entry;
    }

    if ( soundEntry )
    {
        _debuff_sound_owner = soundEntry->profile_id;

        if ( _debuff_soundcarrier.Sounds.empty() )
            _debuff_soundcarrier.Resize(1);
        else
            SFXEngine::SFXe.StopCarrier(&_debuff_soundcarrier);

        TSoundSource &sound = _debuff_soundcarrier.Sounds[0];
        sound.Flags = 0;
        sound.PSample = sourceSound ? sourceSound->PSample : soundEntry->snd_sample;
        sound.Volume = sourceSound ? sourceSound->Volume : soundEntry->snd_volume;
        if ( sourceSound )
            sound.CopyConfig(*sourceSound);
        else
            sound.ConfigurePitchRange(soundEntry->snd_pitch, soundEntry->snd_pitch);
        sound.Radius = sourceSound ? sourceSound->Radius : 0.0f;
        sound.FadeDuration = sourceSound ? sourceSound->FadeDuration : 0.0;
        sound.AllowExtendedRate = sourceSound ? sourceSound->AllowExtendedRate : false;
        sound.PriorityBias = sourceSound ? sourceSound->PriorityBias : 0;
        sound.IgnoreTimeScale = sourceSound ? sourceSound->IgnoreTimeScale : false;
        sound.SetLoop(sourceSound && sourceSound->IsLoop());
        sound.SetFragmented(sourceSound && sourceSound->IsFragmented());
        sound.PPFx = sourceSound ? sourceSound->PPFx : nullptr;
        sound.SetPFx(sourceSound && sourceSound->IsPFx());
        sound.PShkFx = sourceSound ? sourceSound->PShkFx : nullptr;
        sound.SetShk(sourceSound && sourceSound->IsShk());

        if ( sound.IsLoop() && sound.PSample )
            ypabact_StartStatusSoundIfIdle(this, &_debuff_soundcarrier,
                                           soundEntry->snd_volume,
                                           soundEntry->snd_pitch);
    }

    for (const TActiveDebuffState &entry : _debuff_list)
        ypabact_SpawnDebuffFXEvent(this, entry, ypabact_GetDebuffFXLifetime(entry));
}

void NC_STACK_ypabact::UpdateActiveDebuff(update_msg *)
{
    if ( _debuff_list.empty() )
        return;

    bool invalid = !_world ||
                   _energy <= 0 ||
                   _bact_type == BACT_TYPES_MISSLE ||
                   _status == BACT_STATUS_DEAD ||
                   (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));

    if ( invalid )
    {
        ClearActiveDebuff();
        return;
    }

    bool expired = false;

    for (size_t i = 0; i < _debuff_list.size(); )
    {
        TActiveDebuffState &entry = _debuff_list[i];

        if ( _clock >= entry.expire_time )
        {
            if ( entry.profile_id == _debuff_sound_owner )
            {
                SFXEngine::SFXe.StopCarrier(&_debuff_soundcarrier);
                _debuff_sound_owner = -1;
            }

            _debuff_list.erase(_debuff_list.begin() + i);
            expired = true;
            continue;
        }

        if ( _clock >= entry.next_tick_time )
        {
            entry.next_tick_time += entry.tick_time;
            if ( entry.next_tick_time <= _clock )
                entry.next_tick_time = _clock + entry.tick_time;

            NC_STACK_ypabact *source = nullptr;
            if ( entry.source_gid )
                source = ypabact_FindLiveBactByGid(_world->_unitsList, entry.source_gid);

            int tickDamage = 0;
            if ( entry.energy.defined && entry.energy.value > 0.0f )
            {
                double rawDamage = entry.energy.percent
                                 ? (double)_energy_max * ((double)entry.energy.value / 100.0)
                                 : (double)entry.energy.value;
                if ( std::isfinite(rawDamage) && rawDamage > 0.0 )
                {
                    rawDamage = std::min(rawDamage, (double)std::numeric_limits<int>::max());
                    tickDamage = CalcShieldedCustomDamage((int)std::ceil(rawDamage));
                }
            }

            if ( tickDamage > 0 )
            {
                const int32_t tickProfile = entry.profile_id;

                bact_arg84 arg84;
                arg84.energy = -tickDamage;
                arg84.unit = source;
                arg84.killerOwner = entry.source_owner;
                // The tick deals exactly the damage defined by the Debuff profile:
                // attacker-side damage multipliers belong to hits, not to status ticks.
                arg84.bypassAttackerDamageModifiers = true;
                ModifyEnergy(&arg84);

                // A killing tick clears every Debuff: the list (and this entry) is gone.
                if ( _energy <= 0 || _status == BACT_STATUS_DEAD )
                {
                    ClearActiveDebuff();
                    return;
                }

                // The list must not change under a living unit; if anything did, drop the
                // stale reference and let the next frame continue with the new list.
                if ( i >= _debuff_list.size() || _debuff_list[i].profile_id != tickProfile )
                    break;
            }

            TActiveDebuffState &current = _debuff_list[i];
            ypabact_SpawnDebuffFXEvent(this, current, ypabact_GetDebuffFXLifetime(current));

            // The shared channel plays the tick sound of its owner only.
            if ( !_debuff_soundcarrier.Sounds.empty() && current.profile_id == _debuff_sound_owner )
            {
                TSoundSource &snd = _debuff_soundcarrier.Sounds[0];
                snd.PSample = current.snd_sample;

                ypabact_StartStatusSoundIfIdle(this, &_debuff_soundcarrier, current.snd_volume, current.snd_pitch);
            }
        }

        ++i;
    }

    if ( expired )
        RebuildActiveDebuffAggregate();
}

// Combined view of every Buff: Deflect charges add up, the Deflect parameters come from the one in
// use (the first with charges left, or the last Deflect Buff once all are spent, so its visual still
// plays on the final hit), invisible/invulnerable are on when any Buff provides them. The unit flags
// follow the view only when it changes, so other users of those flags are never overridden.
void NC_STACK_ypabact::RebuildActiveBuffAggregate()
{
    const bool wasInvisible = _buff.valid && _buff.invisible;
    const bool wasInvulnerable = _buff.valid && _buff.invulnerable;

    World::TVehicleBuffConfig combined;
    int deflectMax = 0;
    const TActiveBuffState *deflectEntry = nullptr;
    const TActiveBuffState *invisibleEntry = nullptr;

    for (const TActiveBuffState &entry : _buff_list)
    {
        const World::TVehicleBuffConfig &config = entry.config;

        combined.valid = true;
        combined.deflect_charges += config.deflect_charges;
        combined.invulnerable = combined.invulnerable || config.invulnerable;
        deflectMax += entry.deflect_max;

        if ( config.invisible && !invisibleEntry )
            invisibleEntry = &entry;

        if ( entry.deflect_max > 0 && (!deflectEntry || deflectEntry->config.deflect_charges <= 0) )
            deflectEntry = &entry;
    }

    if ( deflectEntry )
    {
        const World::TVehicleBuffConfig &config = deflectEntry->config;
        combined.deflect_max_energy = config.deflect_max_energy;
        combined.deflect_end_energy_reduction = config.deflect_end_energy_reduction;
        combined.deflect_vp = config.deflect_vp;
        combined.deflect_3ds = config.deflect_3ds;
        combined.deflect_base = config.deflect_base;
    }

    combined.invisible = invisibleEntry != nullptr;
    _buff = combined;
    _buff_deflect_charges_max = deflectMax;

    if ( combined.invulnerable != wasInvulnerable )
        _invulnerable = combined.invulnerable;

    if ( combined.invisible != wasInvisible )
        _invisibleUnrevealed = combined.invisible;

    if ( invisibleEntry )
    {
        _buff_invisible_reveal_vp = invisibleEntry->config.invisible_reveal_vp;
        _buff_invisible_reveal_3ds = invisibleEntry->config.invisible_reveal_3ds;
        _buff_invisible_reveal_base = invisibleEntry->config.invisible_reveal_base;
    }
}

// One entry per profile; another profile never replaces it. duration 0 = permanent (vehicle-born
// Buffs). A permanent entry stays permanent; re-applying refills its Deflect charges.
void NC_STACK_ypabact::ApplyBuff(const World::TVehicleBuffConfig &buff, int duration)
{
    if ( !buff.valid )
        return;

    TActiveBuffState *found = nullptr;
    for (TActiveBuffState &entry : _buff_list)
    {
        if ( entry.config.profile_id == buff.profile_id )
        {
            found = &entry;
            break;
        }
    }

    const bool refresh = found != nullptr;
    if ( !found )
    {
        _buff_list.emplace_back();
        found = &_buff_list.back();
    }

    found->config = buff;
    found->deflect_max = std::max(0, buff.deflect_charges);

    if ( duration <= 0 )
        found->expire_time = 0;
    else if ( !refresh || found->expire_time != 0 )
        found->expire_time = _clock + duration;

    RebuildActiveBuffAggregate();
}

void NC_STACK_ypabact::UpdateActiveBuff(update_msg *)
{
    bool expired = false;

    for (size_t i = 0; i < _buff_list.size(); )
    {
        const int expireTime = _buff_list[i].expire_time;
        if ( expireTime != 0 && _clock >= expireTime )
        {
            _buff_list.erase(_buff_list.begin() + i);
            expired = true;
            continue;
        }

        ++i;
    }

    if ( expired )
        RebuildActiveBuffAggregate();
}

// Savegame restore: the saved total goes back to the Deflects in reverse order, because they
// are spent front to back.
void NC_STACK_ypabact::SetBuffDeflectChargesRemaining(int remaining)
{
    remaining = std::max(0, std::min(remaining, _buff_deflect_charges_max));

    for (auto it = _buff_list.rbegin(); it != _buff_list.rend(); ++it)
    {
        const int charges = std::min(remaining, it->deflect_max);
        it->config.deflect_charges = charges;
        remaining -= charges;
    }

    RebuildActiveBuffAggregate();
}

void ypabact_ShuffleDamagedVisuals(std::vector<TDamagedVisualRef> &visuals)
{
    for (size_t remaining = visuals.size(); remaining > 1; --remaining)
    {
        size_t randomIndex = (size_t)World::RandomIntRangeInclusive(0, (int)remaining - 1);
        std::swap(visuals[remaining - 1], visuals[randomIndex]);
    }
}

void ypabact_SpawnSingleDamagedFX(NC_STACK_ypabact *bact,
                                         NC_STACK_ypaworld *world,
                                         const TDamagedVisualRef &visual)
{
    if ( visual.vp <= 0 && visual.mesh3ds.empty() )
        return;

    // Damaged FX are status effects that visually belong to the damaged unit.
    // They must follow the owner while they live; otherwise moving units leave
    // smoke/fire stuck in world-space behind them.
    vec3d localOffset;
    bool rotateOffset = ypabact_BuildAttachedFXOffset(
        bact, bact->_damaged_fx.random_max_offset, &localOffset);
    const float randomScale = (float)World::RandomFloatRangeInclusive(
        visual.scale_min, visual.scale_max);
    const vec3d effectScale = ypabact_BuildUniformStatusFXScale(randomScale);

    if ( !visual.mesh3ds.empty() )
        world->SpawnAttachedStatusTransientMesh(visual.mesh3ds, bact, localOffset, 1000,
                                                rotateOffset, effectScale, World::TVisualTint());
    else
        world->SpawnAttachedStatusTransientVP(visual.vp, bact, localOffset, 1000,
                                              bact->_damaged_fx.trail_only, rotateOffset, effectScale);
}

void ypabact_SpawnDamagedFXEvent(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    std::vector<TDamagedVisualRef> validVisuals;
    validVisuals.reserve(bact->_damaged_fx.vps.size() + bact->_damaged_fx.meshes3ds.size());
    for (size_t i = 0; i < bact->_damaged_fx.vps.size(); ++i)
    {
        const int16_t vp = bact->_damaged_fx.vps[i];
        if ( vp > 0 )
        {
            TDamagedVisualRef visual;
            visual.vp = vp;
            const auto &scales = bact->_damaged_fx.scales;
            if ( !scales.empty() && (scales.size() == 1 || i < scales.size()) )
            {
                const auto &scale = scales[scales.size() == 1 ? 0 : i];
                visual.scale_min = scale.first;
                visual.scale_max = scale.second;
            }
            validVisuals.push_back(visual);
        }
    }
    for (const std::string &mesh3ds : bact->_damaged_fx.meshes3ds)
    {
        if ( !mesh3ds.empty() )
        {
            TDamagedVisualRef visual;
            visual.mesh3ds = mesh3ds;
            if ( bact->_damaged_fx.scales.size() == 1 )
            {
                visual.scale_min = bact->_damaged_fx.scales[0].first;
                visual.scale_max = bact->_damaged_fx.scales[0].second;
            }
            validVisuals.push_back(visual);
        }
    }

    if ( validVisuals.empty() )
        return;

    int spawnCount = 0;

    // No explicit count keeps the pre-count behaviour: every configured VP is
    // emitted once per interval trigger. This preserves existing scripts.
    if ( !ypabact_GetRandomFXSpawnCount(bact->_damaged_fx.count_min,
                                         bact->_damaged_fx.count_max,
                                         spawnCount) )
    {
        for (const TDamagedVisualRef &visual : validVisuals)
            ypabact_SpawnSingleDamagedFX(bact, world, visual);
        return;
    }

    // Randomize the configured VPs as a small shuffle bag. This avoids choosing
    // the same VP twice while another configured VP has not been used yet. If
    // the requested count exceeds the number of configured VPs, reshuffle and
    // continue until the requested amount has been emitted.
    size_t visualIndex = validVisuals.size();
    for (int i = 0; i < spawnCount; i++)
    {
        if ( visualIndex >= validVisuals.size() )
        {
            ypabact_ShuffleDamagedVisuals(validVisuals);
            visualIndex = 0;
        }

        ypabact_SpawnSingleDamagedFX(bact, world, validVisuals[visualIndex++]);
    }
}

vec3d ypabact_BuildRandomLocalDecorationFXOffset(float radius)
{
    if ( radius <= 0.0 )
        return vec3d(0.0, 0.0, 0.0);

    vec3d localOffset;
    localOffset.x = (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * radius;
    localOffset.y = (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * radius;
    localOffset.z = (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * radius;
    return localOffset;
}

bool ypabact_GetDecorationFXSpawnCount(const World::TDecorationFXConfig &config, int &spawnCount)
{
    return ypabact_GetRandomFXSpawnCount(config.count_min, config.count_max, spawnCount);
}

void ypabact_SpawnDecorationFXEvent(NC_STACK_ypabact *bact)
{
    if ( !bact || (bact->_decoration_fx.vp <= 0 &&
                    bact->_decoration_fx.mesh3ds.empty() &&
                    bact->_decoration_fx.basePath.empty()) )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    int spawnCount = 0;
    if ( !ypabact_GetDecorationFXSpawnCount(bact->_decoration_fx, spawnCount) )
        return;

    for (int i = 0; i < spawnCount; i++)
    {
        // Vehicle decoration FX visually belong to the moving owner.
        // Spawn them through the attached transient VP path so smoke/spores/glitches
        // follow mobile vehicles instead of being left behind in world-space.
        //
        // This render path also avoids clamping to the coarse sector height, so it
        // preserves the old slope fix used by damaged/debuff FX and prevents random
        // decoration VPs from popping above the unit on hills.
        vec3d localOffset = bact->_decoration_fx.offset + ypabact_BuildRandomLocalDecorationFXOffset(bact->_decoration_fx.random_pos);
        world->SpawnAttachedTransientVP(bact->_decoration_fx.vp,
                                        bact,
                                        localOffset,
                                        bact->_decoration_fx.duration > 0 ? bact->_decoration_fx.duration : 1000,
                                        1.0,
                                        true,
                                        bact->_decoration_fx.tint,
                                        bact->_decoration_fx.scale,
                                        bact->_decoration_fx.spin,
                                        false,
                                        vec3d(0.0, 0.0, 0.0),
                                        true,
                                        NC_STACK_ypaworld::TTransientVPParticleControls(bact->_decoration_fx),
                                        true,
                                        bact->_decoration_fx.fade_in,
                                        bact->_decoration_fx.fade_out,
                                        bact->_decoration_fx.mesh3ds,
                                        bact->_decoration_fx.basePath);
    }
}

void NC_STACK_ypabact::UpdateDamageFX(update_msg *)
{
    bool canUseDamaged = ypabact_CanUseGameplayStatusMechanics(this);
    bool damaged = canUseDamaged && ypabact_IsDamagedStateActive(this);

    ypabact_ApplyDamagedRuntime(this, damaged);

    if ( !canUseDamaged || !damaged )
    {
        _damaged_fx_next_time = 0;
        return;
    }

    if ( !_world->UpdateRandomFXTimer(_damaged_fx.interval_min, _damaged_fx.interval_max, _damaged_fx_next_time) )
        return;

    ypabact_SpawnDamagedFXEvent(this);
}

void ypabact_SpawnEnergyStatusFXEvent(NC_STACK_ypabact *bact,
                                             const World::EnergyFX::Config &config,
                                             bool plusSymbol)
{
    if ( !bact || !config.IsEnabled() )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    // Regen/Drain marks float around the unit, so in the player's own first-person
    // view (classic or cockpit camera) they would sit right in front of the lens.
    // They stay visible on every other unit and in every other view.
    if ( bact->IsPlayerFirstPersonCameraActive() || bact->IsCockpitCameraActive() )
        return;

    int spawnCount = World::RandomIntRangeInclusive(config.count_min, config.count_max);
    const vec3d axisScale(config.scale, config.scale, config.scale);

    for (int i = 0; i < spawnCount; i++)
    {
        // Reuse the same geometry-aware sampler/fallback path as damaged and
        // debuff FX so both VP and procedural Energy FX originate from the unit.
        vec3d localOffset;
        ypabact_BuildAttachedFXOffset(bact, config.random_max_offset,
                                      &localOffset);

        if ( config.IsProcedural() )
        {
            // Procedural symbols become independent world-space particles once
            // spawned: they originate on the moving unit, then rise vertically
            // instead of being dragged around by later unit rotations/movement.
            const vec3d spawnPos = bact->_position +
                                   bact->_rotation.Transpose().Transform(localOffset);
            world->SpawnProceduralEnergyFX(spawnPos,
                                            plusSymbol,
                                            config.duration,
                                            config.size,
                                            config.thickness,
                                            config.rise_speed,
                                            config.fade_in,
                                            config.fade_out,
                                            config.tint);
            continue;
        }

        // External visual priority matches the shared runtime rule:
        // valid 3DS -> valid BASE -> legacy VP. Same independent, rising,
        // fading world-space particle as the procedural +/- symbol above,
        // instead of an attached VP glued to (and rotating with) the unit.
        const vec3d spawnPos = bact->_position +
                               bact->_rotation.Transpose().Transform(localOffset);
        world->SpawnTransientVisual(config.vp,
                                    config.mesh3ds,
                                    config.basePath,
                                    spawnPos,
                                    bact->_rotation,
                                    config.duration,
                                    1.0,
                                    config.tint,
                                    axisScale,
                                    config.spin,
                                    NC_STACK_ypaworld::TTransientVPParticleControls(),
                                    config.fade_in,
                                    config.fade_out,
                                    vec3d(0.0, -(double)config.rise_speed, 0.0));
    }
}

void ypabact_UpdateEnergyStatusFXProfile(NC_STACK_ypabact *bact,
                                                bool active,
                                                const World::EnergyFX::Config &config,
                                                bool plusSymbol,
                                                int32_t &nextTime)
{
    if ( !active || !config.IsEnabled() || !ypabact_CanSpawnDecorationFX(bact) )
    {
        nextTime = 0;
        return;
    }

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world || !world->UpdateRandomFXTimer(config.interval_min, config.interval_max, nextTime) )
        return;

    ypabact_SpawnEnergyStatusFXEvent(bact, config, plusSymbol);
}

void NC_STACK_ypabact::UpdateEnergyStatusFX(update_msg *)
{
    const World::EnergyFX::Config &regenConfig = World::EnergyFX::Regen();
    const World::EnergyFX::Config &drainConfig = World::EnergyFX::Drain();

    // Default vanilla-safe fast path: with both profiles absent/incomplete, do
    // not perform any additional power-source scan for visual effects.
    if ( !_world || (!regenConfig.IsEnabled() && !drainConfig.IsEnabled()) )
    {
        _regen_fx_next_time = 0;
        _drain_fx_next_time = 0;
        return;
    }

    const uint8_t state = _world->GetUnitEnergyVisualState(this);

    ypabact_UpdateEnergyStatusFXProfile(this,
                                        (state & UNIT_ENERGY_VISUAL_REGEN) != 0,
                                        regenConfig,
                                        true,
                                        _regen_fx_next_time);
    ypabact_UpdateEnergyStatusFXProfile(this,
                                        (state & UNIT_ENERGY_VISUAL_DRAIN) != 0,
                                        drainConfig,
                                        false,
                                        _drain_fx_next_time);
}

void NC_STACK_ypabact::UpdateDecorationFX(update_msg *)
{
    if ( !ypabact_CanSpawnDecorationFX(this) ||
         (_decoration_fx.vp <= 0 && _decoration_fx.mesh3ds.empty() &&
          _decoration_fx.basePath.empty()) )
    {
        _decoration_fx_next_time = 0;
        if ( _world )
            _world->RemoveTransientVP(_decoration_fx_persistent_id,
                                      _decoration_fx.fade_out,
                                      _decoration_fx.vp_trail_fade_out);
        _decoration_fx_persistent_id = 0;
        return;
    }

    if ( _decoration_fx.mode == World::DECORATION_FX_PERSISTENT )
    {
        _decoration_fx_next_time = 0;

        if ( !_world->HasTransientVP(_decoration_fx_persistent_id) )
        {
            _decoration_fx_persistent_id =
                _world->SpawnAttachedTransientVP(_decoration_fx.vp,
                                                 this,
                                                 _decoration_fx.offset,
                                                 0,
                                                 1.0,
                                                 true,
                                                 _decoration_fx.tint,
                                                 _decoration_fx.scale,
                                                 _decoration_fx.spin,
                                                 false,
                                                 vec3d(0.0, 0.0, 0.0),
                                                 true,
                                                 NC_STACK_ypaworld::TTransientVPParticleControls(_decoration_fx),
                                                 true,
                                                 _decoration_fx.fade_in,
                                                 _decoration_fx.fade_out,
                                                 _decoration_fx.mesh3ds,
                                                 _decoration_fx.basePath);
        }

        return;
    }

    _world->RemoveTransientVP(_decoration_fx_persistent_id,
                              _decoration_fx.fade_out,
                              _decoration_fx.vp_trail_fade_out);
    _decoration_fx_persistent_id = 0;

    if ( !_world->UpdateRandomFXTimer(_decoration_fx.interval_min, _decoration_fx.interval_max, _decoration_fx_next_time) )
        return;

    ypabact_SpawnDecorationFXEvent(this);
}

bool ypabact_GetPlasmaFactionTint(NC_STACK_ypabact *bact,
                                            GFX::TGLColor *outTint)
{
    return bact && bact->getBACT_pWorld() &&
           bact->getBACT_pWorld()->GetPlasmaFactionTint(bact->_owner, outTint);
}

// The Genesis plasma has its own colour: the script `tint` of the unit never applies to it
// (a red tint would make the Resistance Genesis red). Only the faction colour does, and the
// Resistance keeps the original cyan plasma.
void ypabact_ApplyGenesisPlasmaTint(NC_STACK_ypabact *bact, GFX::TGLColor *tint, GFX::TGLColor *particleTint,
                                    bool *colorize, bool *particleColorize)
{
    const GFX::TGLColor neutral(1.0f, 1.0f, 1.0f, 1.0f);
    *tint = neutral;
    *particleTint = neutral;
    *colorize = false;
    *particleColorize = false;

    GFX::TGLColor plasmaTint;
    if ( ypabact_GetPlasmaFactionTint(bact, &plasmaTint) )
    {
        *tint = plasmaTint;
        *particleTint = plasmaTint;
        *colorize = true;
        *particleColorize = true;
    }
}

bool NC_STACK_ypabact::IsActiveDebuffStunning(bool requireMovementLevel) const
{
    if ( !_active_debuff.active ||
         !_active_debuff.stun ||
         !_world ||
         _isDummy ||
         _energy <= 0 ||
         _status == BACT_STATUS_DEAD ||
         (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
        return false;

    if ( requireMovementLevel && _active_debuff.stun_motion_level <= 0.0f )
        return false;

    switch ( _bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_ZEPP:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
    case BACT_TYPES_GUN:
        return true;

    default:
        return false;
    }
}

bool NC_STACK_ypabact::IsActiveDebuffStunFireBlocked() const
{
    return _active_debuff.active &&
           _active_debuff.stun &&
           !_active_debuff.stun_unit_fire;
}

void NC_STACK_ypabact::UpdateActiveDebuffStunMoveIntent()
{
    if ( !IsActiveDebuffStunning() )
        return;

    const bool ufo = _bact_type == BACT_TYPES_UFO;
    const bool verticalUnit = ypabact_IsAirVehicle(this) ||
                              _bact_type == BACT_TYPES_ZEPP ||
                              _bact_type == BACT_TYPES_GUN;

    ypabact_AdvanceStunPhase(this, _clock);
    _target_dir = ypabact_BuildStunIntent(
        _rotation, verticalUnit, ufo, _active_debuff.stun_move_phase);
    ypabact_UpdateStunFloorSafety(this);
}

float NC_STACK_ypabact::GetActiveDebuffStunTraction(float currentTraction,
                                                          bool supportsReverse) const
{
    if ( !IsActiveDebuffStunning() )
        return currentTraction;

    const int phase = _active_debuff.stun_move_phase;
    if ( _active_debuff.stun_floor_close && ypabact_IsAirVehicle(this) )
        return currentTraction;

    if ( _bact_type == BACT_TYPES_UFO )
    {
        if ( phase == 0 || phase == 1 )
            return currentTraction * 0.2f;

        if ( phase == 2 )
            return supportsReverse ? -_force * 0.55f : currentTraction * 0.05f;

        return currentTraction;
    }

    if ( phase == 1 || phase == 2 )
        return supportsReverse ? -_force * 0.55f : currentTraction * 0.05f;

    return currentTraction;
}

void NC_STACK_ypabact::UpdateActiveDebuffStunFire(update_msg *arg)
{
    if ( !arg ||
         IsActiveDebuffStunFireBlocked() ||
         !IsActiveDebuffStunning(false) )
        return;

    vec3d fireDirection = _rotation.AxisZ();
    if ( fireDirection.normalise() <= 0.001f )
        return;

    bool firedPrimary = false;
    if ( ypabact_IsValidFireWeaponId(this, _weapon) )
    {
        World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_weapon);
        if ( !wproto.IsArtilleryShell() )
        {
            bact_arg79 fireArg = {};
            fireArg.direction = fireDirection;
            fireArg.start_point = _fire_pos;
            fireArg.tgType = BACT_TGT_TYPE_DRCT;
            fireArg.tgt_pos = fireArg.direction;
            fireArg.weapon = _weapon;
            fireArg.g_time = _clock;
            fireArg.flags = BACT_ARG79_FLAG_NO_AUTO_TARGETS;
            LaunchMissile(&fireArg);
            firedPrimary = true;
        }
    }

    if ( !firedPrimary && HasMinigun() )
    {
        bact_arg105 fireArg;
        fireArg.field_0 = fireDirection;
        fireArg.field_C = arg->frameTime / 1000.0f;
        fireArg.field_10 = _clock;
        FireMinigun(&fireArg);
    }
}

void NC_STACK_ypabact::RunAIWithActiveDebuffStun(update_msg *arg)
{
    if ( IsActiveDebuffStunning() )
    {
        if ( _status == BACT_STATUS_IDLE )
        {
            setState_msg state;
            state.newStatus = BACT_STATUS_NORMAL;
            SetState(&state);
        }
    }

    UpdateActiveDebuffStunFire(arg);
    AI_layer3(arg);
}
