// Energy and damage, death, plasma currency, elite medals, progressive fire rate and stun phases.
#include "game/ypabact_internal.h"

float ypabact_GetPlasmaDeathDurationMultiplier()
{
    static const float multiplier = []()
    {
        float parsed = ypabact_ReadNonNegativeFloatIni(
            System::IniConf::GamePlasmaDeathDurationMult, 1.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return 1.0f;

        return parsed;
    }();

    return multiplier;
}

float ypabact_GetPlasmaDeathMagnetRadius()
{
    static const float radius = []()
    {
        const float parsed = ypabact_ReadNonNegativeFloatIni(
            System::IniConf::GamePlasmaDeathMagnetRadius, 0.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return 0.0f;

        return parsed;
    }();

    return radius;
}

float ypabact_GetPlasmaDeathMagnetSpeed()
{
    static const float speed = []()
    {
        const float parsed = ypabact_ReadNonNegativeFloatIni(
            System::IniConf::GamePlasmaDeathMagnetSpeed, 0.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return 0.0f;

        return parsed;
    }();

    return speed;
}

const ypabact_TEliteUnitConfig &ypabact_GetEliteUnitConfig()
{
    static const ypabact_TEliteUnitConfig config = []()
    {
        ypabact_TEliteUnitConfig result;

        World::TAuthoredScalar statBonus;
        if ( World::ParseAuthoredScalar(
                 System::IniConf::GameEliteUnitMaxStatBonus.Get<std::string>(), statBonus) &&
             statBonus.percent && std::isfinite(statBonus.value) && statBonus.value > 0.0f )
        {
            result.maxStatBonusPercent = std::min(statBonus.value, 100.0f);
        }

        const std::string medalsText =
            System::IniConf::GameEliteUnitMaxMedals.Get<std::string>();
        char *medalsTail = nullptr;
        const long medals = std::strtol(medalsText.c_str(), &medalsTail, 10);
        if ( medalsTail != medalsText.c_str() )
        {
            while ( *medalsTail == ' ' || *medalsTail == '\t' )
                ++medalsTail;
            if ( *medalsTail == '\0' && medals > 0 )
                result.maxMedals = (uint8_t)std::min(medals, 255L);
        }

        const std::string killsText = System::IniConf::GameEliteUnitKillsPerMedal.Get<std::string>();
        char *killsTail = nullptr;
        const long kills = std::strtol(killsText.c_str(), &killsTail, 10);
        if ( killsTail != killsText.c_str() )
        {
            while ( *killsTail == ' ' || *killsTail == '\t' ) ++killsTail;
            if ( *killsTail == '\0' && kills > 0 )
                result.killsPerMedal = (uint32_t)std::min(kills, 100000L);
        }

        char *glowTail = nullptr;
        const std::string glowText =
            System::IniConf::GameEliteUnitMaxGlowIntensity.Get<std::string>();
        const float glow = std::strtof(glowText.c_str(), &glowTail);
        if ( glowTail != glowText.c_str() && std::isfinite(glow) )
        {
            while ( *glowTail == ' ' || *glowTail == '\t' )
                ++glowTail;
            if ( *glowTail == '\0' )
                result.maxGlowIntensity = std::max(0.0f, std::min(glow, 1.0f));
        }

        World::TVisualTint tint;
        if ( World::ParseVisualTint(
                 System::IniConf::GameEliteUnitGlowTint.Get<std::string>(), &tint) )
        {
            result.glowTint = tint;
        }

        char *pulseTail = nullptr;
        const std::string pulseText =
            System::IniConf::GameEliteUnitGlowPulseSeconds.Get<std::string>();
        const float pulse = std::strtof(pulseText.c_str(), &pulseTail);
        if ( pulseTail != pulseText.c_str() && std::isfinite(pulse) )
        {
            while ( *pulseTail == ' ' || *pulseTail == '\t' )
                ++pulseTail;
            if ( *pulseTail == '\0' && pulse > 0.0f )
                result.glowPulseSeconds = std::min(pulse, 60.0f);
        }

        return result;
    }();

    return config;
}

float ypabact_SafeDamageMult(float mult)
{
    return mult >= 0.0 ? mult : 1.0;
}

bool ypabact_IsConfiguredAreaEffectTarget(const NC_STACK_ypabact *source,
                                                  const NC_STACK_ypabact *target)
{
    return source && target && source != target &&
           target->_bact_type != BACT_TYPES_MISSLE &&
           target->_energy > 0 && target->_energy_max > 0 &&
           target->_status != BACT_STATUS_DEAD &&
           target->_status != BACT_STATUS_CREATE &&
           target->_status != BACT_STATUS_BEAM &&
           !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 |
                                    BACT_STFLAG_CLEAN | BACT_STFLAG_NORENDER));
}

void ypabact_ApplyConfiguredDamage(NC_STACK_ypabact *source,
                                          NC_STACK_ypabact *target,
                                          int rawDamage)
{
    if ( !source || !target || rawDamage <= 0 ||
         !ypabact_IsConfiguredAreaEffectTarget(source, target) )
        return;

    const int damage = target->CalcShieldedCustomDamage(rawDamage);
    if ( damage <= 0 )
        return;

    bact_arg84 damageArg;
    damageArg.energy = -damage;
    damageArg.unit = source;
    damageArg.killerOwner = source->_owner;
    damageArg.bypassAttackerDamageModifiers = true;
    target->ModifyEnergy(&damageArg);
}

void ypabact_ApplyInlinePhysicalImpactDamage(NC_STACK_ypabact *fragment,
                                                     NC_STACK_ypabact *directTarget)
{
    NC_STACK_ypaworld *world = fragment ? fragment->getBACT_pWorld() : nullptr;
    if ( !fragment || !directTarget || !world ||
         world->_isNetGame || !fragment->_inlinePhysicalFX ||
         fragment->_inlinePhysicalImpactDamageSpent ||
         fragment->_inlinePhysicalFX->impact_energy <= 0 ||
         !ypabact_IsConfiguredAreaEffectTarget(fragment, directTarget) )
    {
        return;
    }

    // A physical fragment can deal gameplay damage only once. It may continue
    // bouncing visually afterwards, but repeated contact cannot become a damage loop.
    fragment->_inlinePhysicalImpactDamageSpent = true;

    const int rawDamage = fragment->_inlinePhysicalFX->impact_energy;
    ypabact_ApplyConfiguredDamage(fragment, directTarget, rawDamage);

    const float radius = fragment->_inlinePhysicalFX->impact_energy_radius;
    if ( !(radius > 0.0f) || !std::isfinite(radius) )
        return;

    const vec3d origin = fragment->_position;
    const float radiusSq = radius * radius;
    if ( !std::isfinite(radiusSq) )
        return;

    const int sectorRadius = (int)(radius / World::CVSectorLength) + 2;
    const Common::Point center = World::PositionToSectorID(origin);
    std::unordered_set<NC_STACK_ypabact *> visited;
    visited.insert(directTarget);

    for ( int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++ )
    {
        for ( int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++ )
        {
            const Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            for ( NC_STACK_ypabact *target :
                  world->SectorAt(cellId).unitsList.safe_iter() )
            {
                if ( !ypabact_IsConfiguredAreaEffectTarget(fragment, target) ||
                     !visited.insert(target).second )
                {
                    continue;
                }

                const vec3d delta = target->_position - origin;
                const float distanceSq = delta.dot(delta);
                if ( !std::isfinite(distanceSq) || distanceSq > radiusSq )
                    continue;

                ypabact_ApplyConfiguredDamage(fragment, target, rawDamage);
            }
        }
    }
}

void ypabact_ApplyConfiguredAtDeathEffects(NC_STACK_ypabact *source)
{
    NC_STACK_ypaworld *world = source ? source->getBACT_pWorld() : nullptr;
    const bool hasPush = source && source->_at_death_push_force > 0.0f &&
                         source->_at_death_push_radius > 0.0f;
    const bool hasDamage = source && source->_at_death_energy > 0 &&
                           source->_at_death_energy_radius > 0.0f;
    if ( !world || world->_isNetGame || (!hasPush && !hasDamage) )
        return;

    const vec3d origin = source->_position;
    const float pushRadius = hasPush ? source->_at_death_push_radius : 0.0f;
    const float damageRadius = hasDamage ? source->_at_death_energy_radius : 0.0f;
    const float searchRadius = std::max(pushRadius, damageRadius);
    const float searchRadiusSq = searchRadius * searchRadius;
    if ( !isfinite(searchRadiusSq) )
        return;

    // F10 debug: preserve each configured at-death effect volume after the source disappears.
    if ( hasPush )
        world->DebugAddAtDeathSphere(origin, pushRadius);
    if ( hasDamage )
        world->DebugAddAtDeathSphere(origin, damageRadius);

    const float pushRadiusSq = pushRadius * pushRadius;
    const float damageRadiusSq = damageRadius * damageRadius;
    const int sectorRadius = (int)(searchRadius / World::CVSectorLength) + 2;
    const Common::Point center = World::PositionToSectorID(origin);
    std::unordered_set<NC_STACK_ypabact *> visited;

    for ( int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++ )
    {
        for ( int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++ )
        {
            const Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            for ( NC_STACK_ypabact *target : world->SectorAt(cellId).unitsList.safe_iter() )
            {
                if ( !ypabact_IsConfiguredAreaEffectTarget(source, target) ||
                     !visited.insert(target).second )
                {
                    continue;
                }

                const vec3d delta = target->_position - origin;
                const float distanceSq = delta.dot(delta);
                if ( !isfinite(distanceSq) || distanceSq > searchRadiusSq )
                    continue;

                const float distance = distanceSq > 0.0f ? sqrtf(distanceSq) : 0.0f;

                if ( hasDamage && distanceSq <= damageRadiusSq )
                {
                    const float damageFalloff = World::BlastPushFalloffFactor(
                        distance, damageRadius, source->_at_death_energy_falloff != 0);
                    const int rawDamage = (int)ceilf(
                        (float)source->_at_death_energy * damageFalloff);
                    if ( rawDamage > 0 )
                        ypabact_ApplyConfiguredDamage(source, target, rawDamage);
                }

                if ( target->_energy <= 0 || target->_status == BACT_STATUS_DEAD )
                    continue;

                if ( hasPush && distanceSq <= pushRadiusSq &&
                     target->CanReceiveConfiguredPush() && distance > 0.001f )
                {
                    const float pushFalloff = World::BlastPushFalloffFactor(
                        distance, pushRadius, source->_at_death_push_falloff != 0);
                    const float appliedForce =
                        source->_at_death_push_force *
                        pushFalloff *
                        target->GetPushResistanceMultiplier();

                    if ( appliedForce > 0.0f )
                        target->ApplyConfiguredPush(delta / distance, appliedForce);
                }
            }
        }
    }
}

NC_STACK_ypabact *ypabact_ResolveSessionKillCreditedUnit(NC_STACK_ypabact *victim)
{
    if ( !victim )
        return nullptr;

    NC_STACK_ypabact *candidate = victim->_killer;
    std::unordered_set<NC_STACK_ypabact *> visited;

    // Normalize attached components and Host Station Robo-Guns, then follow a
    // bounded chain through dying damage sources. This preserves the original
    // attacker for simultaneous chain kills instead of awarding only the first
    // directly destroyed unit.
    while ( candidate && candidate != victim && visited.insert(candidate).second )
    {
        if ( (candidate->_isUnitGunChild || candidate->_isDummy) &&
             candidate->_parent && candidate->_parent != candidate )
        {
            candidate = candidate->_parent;
            continue;
        }

        // Robo-Guns are weapons of the Host Station, not independent Elite
        // units. Normalize their kills to the owning Robo so Host Stations can
        // never gain medals, glow or stat bonuses through attached turrets.
        if ( candidate->_bact_type == BACT_TYPES_GUN )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(candidate);
            if ( gun && gun->IsRoboGun() && candidate->_host_station &&
                 candidate->_host_station != candidate )
            {
                candidate = candidate->_host_station;
                continue;
            }
        }

        const bool candidateIsDying =
            candidate->_status == BACT_STATUS_DEAD ||
            (candidate->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));

        if ( candidateIsDying && candidate->_killer &&
             candidate->_killer != candidate )
        {
            candidate = candidate->_killer;
            continue;
        }

        return candidate;
    }

    return nullptr;
}

bool ypabact_HasGroundDecalSupport(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
        return false;

    // A crash Ground Decal is valid only when terrain is immediately below the
    // vehicle body. This also rejects death-state transitions that reach the
    // crash trigger while the vehicle is still airborne.
    const float bodyToGround =
        unit->getBACT_viewer() ? unit->_viewer_overeof : unit->_overeof;
    const float supportReach =
        std::max(100.0f, std::fabs(bodyToGround) + 50.0f);

    ypaworld_arg136 support;
    support.stPos = unit->_position;
    support.vect = vec3d::OY(supportReach);
    support.flags = 0;
    unit->getBACT_pWorld()->ypaworld_func136(&support);

    return support.isect &&
           std::isfinite(support.isectPos.y) &&
           support.isectPos.y >= unit->_position.y - 0.25f;
}

void NC_STACK_ypabact::Die()
{
    if ( _status_flg & BACT_STFLAG_DEATH1 )
        return;

    ResetAlternativeView();

    CrashDiag::Breadcrumb("BACT_DEATH",
                          "begin ptr=%p gid=%d type=%d owner=%d status=%d flags=0x%x energy=%d killer_gid=%d killer_owner=%d parent_gid=%d host_gid=%d kids=%zu",
                          this, _gid, _bact_type, _owner, _status, _status_flg,
                          _energy, _killer ? _killer->_gid : 0, _killer_owner,
                          _parent ? _parent->_gid : 0,
                          _host_station ? _host_station->_gid : 0,
                          _kidList.size());

    // OpenNeoUA custom: lightweight single-player kill marks. Each victim death is
    // credited independently, including simultaneous chain kills, while attached
    // guns/dummies are normalized to their carrying unit. Friendly/self kills,
    // components and missiles never award marks.
    NC_STACK_ypabact *creditedKiller = ypabact_ResolveSessionKillCreditedUnit(this);
    const uint8_t creditedOwner = creditedKiller ? creditedKiller->_owner : World::OWNER_0;

    if ( _world && !_world->_isNetGame && creditedKiller &&
         _owner != World::OWNER_0 && creditedOwner != World::OWNER_0 &&
         _owner != creditedOwner && creditedKiller != this &&
         _bact_type != BACT_TYPES_MISSLE && !_isUnitGunChild && !_isDummy &&
         creditedKiller->_bact_type != BACT_TYPES_MISSLE &&
         creditedKiller->CanUseSessionKillMarks() )
    {
        creditedKiller->AwardSessionKill(_bact_type == BACT_TYPES_ROBO);
    }

    if ( _isUnitGunChild )
        ypabact_SafeDetachControlFrom(this, _parent);

    ClearActiveDebuff();
    _carrier_spawned_gids.clear();
    CleanupUnitGuns(true, true);

    int maxy = _world->getYW_mapSizeY();
    int maxx = _world->getYW_mapSizeX();

    uamessage_dead deadMsg;
    deadMsg.msgID = UAMSG_DEAD;
    deadMsg.owner = _owner;
    deadMsg.id = _gid;
    deadMsg.newParent = 0;
    deadMsg.landed = 0;
    deadMsg.classID = _bact_type;

    if ( _killer )
        deadMsg.killer = _killer->_gid;
    else
        deadMsg.killer = 0;

    deadMsg.killerOwner = _killer_owner;

    NC_STACK_ypabact *deputy = nullptr;

    if (!_kidList.empty())
    {

        for (World::RefBactList::iterator it = _kidList.begin(); it != _kidList.end(); )
        {
            // Forward dereference iterator and next
            NC_STACK_ypabact *kid = *it;
            it++;

            if ( kid->_status == BACT_STATUS_DEAD )
            {
                if ( _parent )
                    _parent->AddSubject(kid);
                else
                    _world->ypaworld_func134(kid);

                kid->_status_flg |= BACT_STFLAG_NOMSG;
            }
            else
            {
                float kidLen = (kid->_position.XZ() - _position.XZ()).square();

                float deputyLen;

                if ( deputy )
                    deputyLen = (deputy->_position.XZ() - _position.XZ()).square();
                else
                    deputyLen = (POW2(maxx) + POW2(maxy)) * World::CVSectorLength * World::CVSectorLength;

                if ( kid->_bact_type == BACT_TYPES_UFO )
                    kidLen = (POW2(maxx) + POW2(maxy)) * World::CVSectorLength * World::CVSectorLength - 1000.0;

                if ( kidLen <= deputyLen )
                    deputy = kid;
            }
        }

        if ( deputy )
        {
            if ( _parent )
                _parent->AddSubject(deputy);
            else
                _world->ypaworld_func134(deputy);

            while ( !_kidList.empty() )
                deputy->AddSubject(_kidList.front());

            setTarget_msg arg67;
            arg67.tgt_pos = _primTpos;
            arg67.tgt.pbact = _primT.pbact;
            arg67.tgt_type = _primTtype;
            arg67.priority = 0;

            deputy->SetTarget(&arg67);

            deputy->CopyWaypointsStuff(this);

            deputy->_commandID = _commandID;
            deputy->_aggr = _aggr;

            if ( _world->_isNetGame )
            {
                if (_owner)
                    deadMsg.newParent = deputy->_gid;
            }
        }
        else
        {
            for(World::RefBactList::iterator kidXit = _kidList.begin(); kidXit != _kidList.end(); )
            {
                NC_STACK_ypabact *kidX = *kidXit;
                kidXit++;

                for ( World::RefBactList::iterator kidYit = kidX->_kidList.begin(); kidYit != kidX->_kidList.end(); )
                {
                    NC_STACK_ypabact *kidY = *kidYit;
                    kidYit++;

                    _world->ypaworld_func134(kidY);

                    if ( kidY->_status != BACT_STATUS_DEAD )
                        ypa_log_out("Scheisse, da hфngt noch ein Lebendiger unter der Leiche! owner %d, state %d, class %d\n", kidY->_owner, kidY->_status, _bact_type);
                }
                _world->ypaworld_func134(kidX);
            }
        }
    }

    NC_STACK_ypabact *v76 = _world->getYW_userHostStation();

    if ( !deputy && IsParentMyRobo()&& !(_status_flg & BACT_STFLAG_NOMSG) )
    {
        robo_arg134 v53;

        if ( v76 == _host_station )
        {
            if ( _bact_type == BACT_TYPES_GUN )
            {
                if ( _weapon != -1 || HasMinigun() )
                {
                    v53.field_14 = 80;
                    v53.field_4 = 31;
                }
                else
                {
                    v53.field_14 = 80;
                    v53.field_4 = 32;
                }

                v53.field_10 = 0;
                v53.field_C = 0;
                v53.field_8 = 0;
                v53.unit = this;

                _host_station->placeMessage(&v53);
            }
            else
            {
                if ( !(_status_flg & BACT_STFLAG_CLEAN) )
                {
                    v53.field_8 = _commandID;
                    v53.unit = this;
                    v53.field_10 = 0;
                    v53.field_14 = 44;
                    v53.field_C = 0;
                    v53.field_4 = 8;

                    _host_station->placeMessage(&v53);
                }
            }
        }
        else
        {
            if ( _killer && v76 == _killer->_host_station )
            {
                v53.field_4 = 5;
                v53.unit = _killer;
                v53.field_8 = _primT_cmdID;
                v53.field_10 = 0;
                v53.field_C = 0;
                v53.field_14 = 36;

                _host_station->placeMessage(&v53);
            }
        }

    }

    CleanAttackersTarget();

    if ( _parent )
    {
        for (World::MissileList::iterator it = _missiles_list.begin(); it != _missiles_list.end(); it = _missiles_list.erase(it))
        {
            NC_STACK_ypamissile *miss = *it;

            _parent->_missiles_list.push_back(miss);
            miss->SetLauncherBact( _parent );
        }
    }
    else
    {
        for (World::MissileList::iterator it = _missiles_list.begin(); it != _missiles_list.end(); it = _missiles_list.erase(it))
        {
            NC_STACK_ypamissile *miss = *it;

            miss->ResetViewing();

            setState_msg arg119;
            arg119.newStatus = BACT_STATUS_DEAD;
            arg119.unsetFlags = 0;
            arg119.setFlags = 0;
            miss->SetStateInternal(&arg119);

            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;
            miss->SetTarget(&arg67);

            miss->_parent = nullptr;

            _world->ypaworld_func144(miss);
        }
    }


    if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        _secndT.pbact->DeleteAttacker(this, 1);

    if ( _primTtype == BACT_TGT_TYPE_UNIT )
        _primT.pbact->DeleteAttacker(this, 0);


    _secndTtype = BACT_TGT_TYPE_NONE;
    _primTtype = BACT_TGT_TYPE_NONE;

    ypabact_ApplyConfiguredAtDeathEffects(this);
    ypabact_FireProximityDefenseAtDeath(this);
    ypabact_TrySpawnAtDeath(this);

    _status = BACT_STATUS_DEAD;
    _commandID = 0;
    _status_flg |= BACT_STFLAG_DEATH1;
    _dead_time = _clock;

    if ( _status_flg & BACT_STFLAG_LAND )
    {
        if ( _vp_active == 1 || _vp_active == 6 )
        {
            setState_msg arg119;
            arg119.unsetFlags = 0;
            arg119.newStatus = BACT_STATUS_NOPE;
            arg119.setFlags = BACT_STFLAG_DEATH2;

            SetStateInternal(&arg119);

            if ( _world->_isNetGame )
            {
                if (_owner)
                    deadMsg.landed = 1;
            }
        }
    }

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( !(_oflags & BACT_OFLAG_VIEWER) )
        {
            if ( _parent )
                setBACT_inputting(false);
        }
    }

    if ( _world->_isNetGame )
    {
        if ( _owner )
        {
            if ( _bact_type != BACT_TYPES_ROBO )
                _world->NetBroadcastMessage(&deadMsg, sizeof(deadMsg), true);
        }
    }

    if ( _owner )
    {
        if ( !(_status_flg & BACT_STFLAG_CLEAN) )
            _world->HistoryAktKill(this);
    }

    CrashDiag::Breadcrumb("BACT_DEATH",
                          "end ptr=%p gid=%d type=%d owner=%d status=%d flags=0x%x",
                          this, _gid, _bact_type, _owner, _status, _status_flg);
}

int ypabact_GetStunPhaseDuration(const NC_STACK_ypabact *bact, int phase)
{
    const bool ufo = bact->_bact_type == BACT_TYPES_UFO;
    const bool turning = phase == 0 || phase == 1;
    float baseDuration;

    if ( turning )
    {
        const float effectiveMaxRot = fabs(bact->_maxrot);
        baseDuration = effectiveMaxRot > 0.001f
                           ? C_2PI * 1000.0f / effectiveMaxRot
                           : 12000.0f;
    }
    else if ( ufo )
    {
        static const int ufoDurations[] = {0, 0, 1200, 1500, 1400, 1400};
        baseDuration = (float)ufoDurations[phase % 6];
    }
    else
    {
        static const int durations[] = {0, 0, 1200, 1500};
        baseDuration = (float)durations[phase & 3];
    }

    const float motionLevel =
        std::max(0.0f, std::min(bact->_active_debuff.stun_motion_level, 1.0f));
    return std::max(120, (int)(baseDuration * motionLevel + 0.5f));
}

int ypabact_GetNextStunPhase(const NC_STACK_ypabact *bact, int currentPhase)
{
    if ( bact->_bact_type != BACT_TYPES_UFO )
        return (currentPhase + 1) & 3;

    int nextPhase = rand() % 6;
    if ( nextPhase == currentPhase )
        nextPhase = (nextPhase + 1) % 6;
    return nextPhase;
}

void ypabact_AdvanceStunPhase(NC_STACK_ypabact *bact, int clock)
{
    TActiveDebuffState &debuff = bact->_active_debuff;

    if ( debuff.stun_next_move_time <= 0 )
    {
        if ( bact->_bact_type == BACT_TYPES_UFO )
            debuff.stun_move_phase = rand() % 6;
        else
            debuff.stun_move_phase = 0;

        debuff.stun_next_move_time =
            clock + ypabact_GetStunPhaseDuration(bact, debuff.stun_move_phase);
        return;
    }

    while ( clock >= debuff.stun_next_move_time )
    {
        debuff.stun_move_phase =
            ypabact_GetNextStunPhase(bact, debuff.stun_move_phase);
        debuff.stun_next_move_time +=
            ypabact_GetStunPhaseDuration(bact, debuff.stun_move_phase);
    }
}

vec3d ypabact_BuildStunIntent(const mat3x3 &rotation, bool verticalUnit,
                                          bool ufo, int phase)
{
    float forward = 0.0f;
    float side = 0.0f;
    float vertical = 0.0f;

    if ( ufo )
    {
        switch ( phase % 6 )
        {
        case 0: side = 1.0f; break;
        case 1: side = -1.0f; break;
        case 2: forward = -1.0f; break;
        case 3: forward = 1.0f; break;
        case 4: forward = 0.15f; vertical = -1.0f; break;
        default: forward = 0.15f; vertical = 1.0f; break;
        }
    }
    else
    {
        switch ( phase & 3 )
        {
        case 0: forward = 0.55f; side = 1.0f; vertical = verticalUnit ? -0.7f : 0.0f; break;
        case 1: forward = -0.55f; side = -1.0f; vertical = verticalUnit ? 0.7f : 0.0f; break;
        case 2: forward = -1.0f; side = 0.55f; vertical = verticalUnit ? 0.45f : 0.0f; break;
        default: forward = 1.0f; side = -0.55f; vertical = verticalUnit ? -0.45f : 0.0f; break;
        }
    }

    vec3d intent =
        rotation.AxisZ() * forward +
        rotation.AxisX() * side +
        vec3d::OY(vertical);
    if ( intent.normalise() <= 0.001f )
        intent = rotation.AxisZ();

    return intent;
}

void ypabact_UpdateStunFloorSafety(NC_STACK_ypabact *bact)
{
    NC_STACK_ypaworld *world = bact ? bact->getBACT_pWorld() : nullptr;
    if ( !bact || !ypabact_IsAirVehicle(bact) || !world )
        return;

    TActiveDebuffState &debuff = bact->_active_debuff;

    if ( bact->_status_flg & BACT_STFLAG_LAND )
    {
        debuff.stun_floor_close = true;
    }
    else if ( debuff.stun_next_floor_check_time <= 0 ||
              bact->_clock >= debuff.stun_next_floor_check_time )
    {
        debuff.stun_next_floor_check_time = bact->_clock + 100;

        const float probeDistance =
            std::max(150.0f, std::max(bact->_height, bact->_radius * 2.0f));

        ypaworld_arg136 floorProbe = {};
        floorProbe.stPos = bact->_position;
        floorProbe.vect = vec3d::OY(probeDistance);
        floorProbe.flags = 0;

        world->ypaworld_func136(&floorProbe);

        const float safeClearance = std::max(50.0f, bact->_radius * 1.25f);
        debuff.stun_floor_close =
            floorProbe.isect && floorProbe.tVal * probeDistance <= safeClearance;
    }

    if ( debuff.stun_floor_close && bact->_target_dir.y > -0.35f )
    {
        bact->_target_dir.y = -0.35f;
        bact->_target_dir.normalise();
    }
}

void NC_STACK_ypabact::GetSummary(bact_arg81 *arg)
{
    for ( NC_STACK_ypabact* &node : _kidList )
        node->GetSummary(arg);

    if ( _status != BACT_STATUS_DEAD )
    {
        switch ( arg->enrg_type )
        {
        case 1:
            arg->enrg_sum += _energy;
            break;

        case 3:
            arg->enrg_sum++;
            break;

        case 2:
            arg->enrg_sum += _shield;
            break;

        case 4:
            arg->enrg_sum += _energy_max;
            break;

        case 5:
        {
            arg->enrg_sum += _attackersList.size();
        }
        break;

        default:
            break;
        }
    }
}

// Update bact energy
void NC_STACK_ypabact::EnergyInteract(update_msg *arg)
{
    if ( _status != BACT_STATUS_DEAD )
    {
        int v16 = _clock - _energy_time;

        if ( v16 >= 1500 )
        {
            _energy_time = _clock;

            yw_arg176 arg176;
            arg176.owner = _pSector->owner;

            _world->ypaworld_func176(&arg176);

            float v14 = v16 / 1000.0;

            const float powerEnergyMultiplier = ReadPowerStationEnergyMultiplier();
            float denerg = powerEnergyMultiplier * _energy_max * v14 * _pSector->energy_power * arg176.field_4 / 7000.0;

            if ( _owner == _pSector->owner )
                _energy += denerg;
            else if ( !IsInvulnerableToDamage() )
                _energy -= denerg;

            TMobilePowerInfluence mobilePower = _world->FindMobilePowerInfluenceForUnit(this);
            float mobileDelta = powerEnergyMultiplier * 2.0 * _energy_max * v14 * (mobilePower.AlliedEnergyPower - mobilePower.EnemyEnergyPower) / 7000.0;

            if ( mobileDelta >= 0.0 || !IsInvulnerableToDamage() )
                _energy += mobileDelta;

            if ( _energy < 0 )
                _energy = 0;

            if ( _energy > _energy_max )
                _energy = _energy_max;
        }
    }
}

bool NC_STACK_ypabact::CanUseSessionKillMarks() const
{
    // UFO and Host Station classes are explicitly outside the unit medal system.
    // They may never acquire, display or benefit from session kill marks.
    return _bact_type != BACT_TYPES_UFO &&
           _bact_type != BACT_TYPES_ROBO;
}

uint8_t NC_STACK_ypabact::GetSessionKillMarks() const
{
    if ( !_world || _world->_isNetGame || !CanUseSessionKillMarks() )
        return 0;

    return std::min<uint8_t>(_sessionKillMarks, ypabact_GetEliteUnitConfig().maxMedals);
}

void NC_STACK_ypabact::AwardSessionKill(bool hostStation)
{
    if ( !_world || _world->_isNetGame || !CanUseSessionKillMarks() )
        return;
    const auto &config = ypabact_GetEliteUnitConfig();
    if ( hostStation )
    {
        _sessionKillMarks = config.maxMedals;
        _sessionKillsTowardMedal = 0;
    }
    else if ( _sessionKillMarks < config.maxMedals )
    {
        if ( ++_sessionKillsTowardMedal >= config.killsPerMedal )
        {
            ++_sessionKillMarks;
            _sessionKillsTowardMedal = 0;
        }
    }
}

float NC_STACK_ypabact::GetEliteMedalProgress() const
{
    const ypabact_TEliteUnitConfig &config = ypabact_GetEliteUnitConfig();
    const uint8_t medals = GetSessionKillMarks();
    if ( medals == 0 || config.maxMedals == 0 )
        return 0.0f;

    return std::min(1.0f, (float)medals / (float)config.maxMedals);
}

float NC_STACK_ypabact::GetEliteStatBonusPercent() const
{
    return ypabact_GetEliteUnitConfig().maxStatBonusPercent * GetEliteMedalProgress();
}

float NC_STACK_ypabact::GetEliteStatMultiplier() const
{
    return 1.0f + GetEliteStatBonusPercent() / 100.0f;
}

float NC_STACK_ypabact::GetEliteGlowIntensity() const
{
    return ypabact_GetEliteUnitConfig().maxGlowIntensity * GetEliteMedalProgress();
}

int ypabact_GetProgressiveWeaponBaseShotTime(const NC_STACK_ypabact *bact,
                                                       const World::TWeapProto &proto)
{
    if ( !bact )
        return 0;

    return (bact->_oflags & BACT_OFLAG_USERINPT) ? proto.shot_time_user : proto.shot_time;
}

bool ypabact_HasProgressiveWeaponFireRate(const NC_STACK_ypabact *bact,
                                                  const World::TWeapProto &proto)
{
    const int baseShotTime = ypabact_GetProgressiveWeaponBaseShotTime(bact, proto);
    return proto.ramp_up_time > 0 &&
           proto.ramp_up_max_shot_time > 0 &&
           baseShotTime > proto.ramp_up_max_shot_time;
}

void NC_STACK_ypabact::ResetProgressiveWeaponFireRate()
{
    _progressive_weapon_id = -1;
    _progressive_weapon_level = 0.0f;
    _progressive_weapon_requested = false;
    _progressive_weapon_overheat_active = false;
    _progressive_weapon_overheat_elapsed_ms = 0.0;
    _progressive_weapon_overheat_hp_drain_remainder = 0.0;
}

void NC_STACK_ypabact::RegisterProgressiveWeaponFireRequest(int weaponId)
{
    if ( !_world || !ypabact_IsValidWeaponId(this, weaponId) )
        return;

    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(weaponId);
    if ( !ypabact_HasProgressiveWeaponFireRate(this, proto) )
        return;

    if ( _progressive_weapon_id != weaponId )
    {
        _progressive_weapon_id = weaponId;
        _progressive_weapon_level = 0.0f;
    }

    _progressive_weapon_requested = true;
}

int NC_STACK_ypabact::GetProgressiveWeaponShotTime(const World::TWeapProto &proto,
                                                    int fallbackShotTime)
{
    if ( !ypabact_HasProgressiveWeaponFireRate(this, proto) ||
         _progressive_weapon_id < 0 || !_world ||
         !ypabact_IsValidWeaponId(this, _progressive_weapon_id) ||
         &_world->GetWeaponsProtos().at(_progressive_weapon_id) != &proto )
    {
        return fallbackShotTime;
    }

    if ( fallbackShotTime <= proto.ramp_up_max_shot_time )
        return fallbackShotTime;

    const float level = std::max(0.0f, std::min(_progressive_weapon_level, 1.0f));
    const double slow = (double)fallbackShotTime;
    const double fast = (double)proto.ramp_up_max_shot_time;
    const double interpolated = slow + (fast - slow) * (double)level;

    if ( !std::isfinite(interpolated) )
        return fallbackShotTime;

    return std::max(1, (int)floor(interpolated + 0.5));
}

const std::string &NC_STACK_ypabact::GetProgressiveWeaponOverheatIcon()
{
    static const std::string emptyIcon;

    if ( !_progressive_weapon_overheat_active || _progressive_weapon_id < 0 ||
         !_world || !ypabact_IsValidWeaponId(this, _progressive_weapon_id) )
    {
        return emptyIcon;
    }

    return _world->GetWeaponsProtos().at(_progressive_weapon_id).ramp_up_overheat_icon;
}

void NC_STACK_ypabact::UpdateProgressiveWeaponFireRate(update_msg *arg)
{
    if ( !arg || arg->frameTime <= 0 || _progressive_weapon_id < 0 || !_world ||
         !ypabact_IsValidWeaponId(this, _progressive_weapon_id) )
    {
        if ( _progressive_weapon_id >= 0 && (!_world || !ypabact_IsValidWeaponId(this, _progressive_weapon_id)) )
            ResetProgressiveWeaponFireRate();
        else
            _progressive_weapon_requested = false;
        return;
    }

    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(_progressive_weapon_id);
    if ( !ypabact_HasProgressiveWeaponFireRate(this, proto) )
    {
        ResetProgressiveWeaponFireRate();
        return;
    }

    if ( !_progressive_weapon_requested )
    {
        // Releasing FIRE immediately restores the authored shot_time/shot_time_user.
        ResetProgressiveWeaponFireRate();
        return;
    }

    _progressive_weapon_overheat_active = false;

    const float previousLevel = std::max(0.0f, std::min(_progressive_weapon_level, 1.0f));
    _progressive_weapon_level = std::min(1.0f,
        previousLevel + (float)arg->frameTime / (float)proto.ramp_up_time);

    // Count only the part of this frame actually spent at the authored maximum
    // cadence. This keeps the overheat delay independent of frame rate.
    double timeAtMaxMs = 0.0;
    if ( previousLevel >= 1.0f )
    {
        timeAtMaxMs = (double)arg->frameTime;
    }
    else if ( _progressive_weapon_level >= 1.0f )
    {
        const double timeToMaxMs =
            (1.0 - (double)previousLevel) * (double)proto.ramp_up_time;
        timeAtMaxMs = std::max(0.0, (double)arg->frameTime - timeToMaxMs);
    }

    const World::TAbsoluteOrPercent &overheatDrain = proto.ramp_up_overheat_hp_drain;
    const bool overheatConfigured =
        proto.ramp_up_overheat_time > 0 &&
        overheatDrain.defined && std::isfinite(overheatDrain.value) &&
        overheatDrain.value > 0.0f;

    if ( overheatConfigured && timeAtMaxMs > 0.0 )
    {
        const double delayMs = (double)proto.ramp_up_overheat_time;
        double damageTimeMs = 0.0;

        if ( _progressive_weapon_overheat_elapsed_ms >= delayMs )
        {
            damageTimeMs = timeAtMaxMs;
        }
        else
        {
            const double remainingDelayMs =
                delayMs - _progressive_weapon_overheat_elapsed_ms;
            if ( timeAtMaxMs > remainingDelayMs )
                damageTimeMs = timeAtMaxMs - remainingDelayMs;

            _progressive_weapon_overheat_elapsed_ms = std::min(
                delayMs, _progressive_weapon_overheat_elapsed_ms + timeAtMaxMs);
        }

        // Show the icon only while this frame is actually inside the
        // self-damage phase, not while the overheat delay is still counting.
        _progressive_weapon_overheat_active = damageTimeMs > 0.0;

        if ( damageTimeMs > 0.0 && _energy > 0 && _energy_max > 0 )
        {
            double drainPerSecond = overheatDrain.percent
                ? (double)_energy_max * (double)overheatDrain.value / 100.0
                : (double)overheatDrain.value;

            if ( std::isfinite(drainPerSecond) && drainPerSecond > 0.0 )
            {
                _progressive_weapon_overheat_hp_drain_remainder +=
                    drainPerSecond * damageTimeMs / 1000.0;

                const int energyDrain = (int)std::min(
                    _progressive_weapon_overheat_hp_drain_remainder,
                    (double)std::numeric_limits<int>::max());
                if ( energyDrain > 0 )
                {
                    _progressive_weapon_overheat_hp_drain_remainder -= energyDrain;
                    _energy = energyDrain >= _energy ? 0 : _energy - energyDrain;
                }
            }
        }
    }

    _progressive_weapon_requested = false;
}

int NC_STACK_ypabact::GetEffectiveShotTime(int baseShotTime, bool minigun) const
{
    if ( baseShotTime <= 0 )
        return baseShotTime;

    float multiplier = 1.0f;

    // The Elite bonus increases fire rate, therefore its linear stat multiplier
    // divides the selected cooldown. This one helper is used after choosing the
    // exact shot_time/user or shared mgun_shot_time field and never mutates a proto.
    const float eliteStatMultiplier = GetEliteStatMultiplier();
    if ( eliteStatMultiplier > 0.0f )
        multiplier /= eliteStatMultiplier;

    // Damaged fire-rate maluses reuse the same damaged-state gate as the
    // existing force/maxrot/sound penalties. Authoring follows the established
    // negative-percent malus convention: e.g. -50% halves fire rate, therefore
    // doubles the selected cooldown. MGUN and normal Weapon timing remain
    // independently configurable and never mutate prototype shot_time values.
    if ( _damaged_fx_active )
    {
        const float malus = minigun
            ? _damaged_mgun_shot_time_malus
            : _damaged_shot_time_malus;
        const float fireRateMultiplier = ypabact_DebuffMalusToMult(malus);

        if ( fireRateMultiplier <= 0.0f )
            return std::numeric_limits<int>::max();

        multiplier /= fireRateMultiplier;
    }

    // Debuff cadence penalties deliberately reuse this same multiplier path.
    // Damaged + Debuff therefore compose deterministically instead of one
    // overwriting the other, while MGUN and normal Weapon timing stay separate.
    if ( _active_debuff.active )
    {
        const float malus = minigun
            ? _active_debuff.mgun_shot_time_malus
            : _active_debuff.shot_time_malus;
        const float fireRateMultiplier = ypabact_DebuffMalusToMult(malus);

        if ( fireRateMultiplier <= 0.0f )
            return std::numeric_limits<int>::max();

        multiplier /= fireRateMultiplier;
    }

    const double scaled = (double)baseShotTime * (double)multiplier;
    if ( !std::isfinite(scaled) ||
         scaled >= (double)std::numeric_limits<int>::max() )
    {
        return std::numeric_limits<int>::max();
    }

    return std::max(1, (int)floor(scaled + 0.5));
}

int NC_STACK_ypabact::GetEffectiveOutgoingEnergy(int baseDamage) const
{
    if ( baseDamage == 0 )
        return 0;

    float multiplier = GetEliteStatMultiplier();

    if ( multiplier == 1.0f )
        return baseDamage;

    int scaled = (int)((float)baseDamage * multiplier);
    // Never let truncation turn a real hit into zero damage. This helper accepts
    // both the negative ModifyEnergy convention and positive damage magnitudes.
    if ( scaled == 0 )
        scaled = baseDamage < 0 ? -1 : 1;
    return scaled;
}

float NC_STACK_ypabact::GetEffectiveShieldWithAdditionalMalus(float additionalMalus) const
{
    float shield = (float)_shield;
    float mult = 1.0f;

    if ( _active_debuff.active )
        mult *= ypabact_DebuffMalusToMult(_active_debuff.shield_malus);

    mult *= ypabact_DebuffMalusToMult(additionalMalus);

    if ( mult < 0.0f )
        mult = 0.0f;

    const float eliteBonusPercent = GetEliteStatBonusPercent();
    const float baseEffectiveShield = shield * mult;
    float effectiveShield = baseEffectiveShield * GetEliteStatMultiplier();

    // Shield 100 is complete immunity in the existing damage formula. An Elite
    // bonus may improve defense but must not turn a previously vulnerable unit
    // invulnerable. Units already at 100+ before the medal remain unchanged.
    if ( eliteBonusPercent > 0.0f && baseEffectiveShield < 100.0f &&
         effectiveShield >= 100.0f )
        effectiveShield = 99.0f;

    return effectiveShield;
}

float NC_STACK_ypabact::GetEffectiveShield() const
{
    return GetEffectiveShieldWithAdditionalMalus(0.0f);
}

float NC_STACK_ypabact::CalcShieldedActionEnergyCost(float rawCost) const
{
    if ( !isfinite(rawCost) || rawCost <= 0.0f )
        return 0.0f;

    float shield = GetEffectiveShield();
    if ( !isfinite(shield) || shield <= 0.0f )
        shield = 0.0f;
    else if ( shield >= 100.0f )
        return 0.0f;

    return rawCost * (100.0f - shield) / 100.0f;
}

int32_t NC_STACK_ypabact::GetEnergyDrainIntervalMs(Common::Ini::Key &key)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return 0;

    try
    {
        size_t pos = 0;
        const long long parsed = std::stoll(value, &pos, 0);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos || parsed < 0 )
            return 0;

        return (int32_t)std::min<long long>(parsed, 600000);
    }
    catch (...)
    {
        return 0;
    }
}

int NC_STACK_ypabact::CalcShieldedCustomDamage(int rawDamage) const
{
    if ( rawDamage <= 0 )
        return 0;

    float shield = GetEffectiveShield();
    if ( shield < 0.0f )
        shield = 0.0f;

    if ( shield >= 100.0f )
        return 0;

    int damage = (int)ceil((float)rawDamage * (100.0f - shield) / 100.0f);
    return damage > 0 ? damage : 0;
}

bool NC_STACK_ypabact::ConsumeBuffDeflectCharge()
{
    if ( !HasDeflectBuff() )
        return false;

    --_buff.deflect_charges;
    return true;
}

void NC_STACK_ypabact::SpawnBuffDeflectVisual(const vec3d &pos, const mat3x3 &rot)
{
    if ( !_world || !_buff.valid )
        return;

    // Deflect event visual uses the shared transient loader: 3DS -> BASE -> VP.
    _world->SpawnTransientVisual(_buff.deflect_vp, _buff.deflect_3ds, _buff.deflect_base,
                                 pos, rot, 1000);
}

bool NC_STACK_ypabact::IsInvulnerableToDamage() const
{
    if ( _invulnerable )
        return true;

    // Projectiles must retain their normal lifetime and impact/destruction
    // path. The F9 debug feature protects gameplay units, not missiles.
    if ( _bact_type == BACT_TYPES_MISSLE )
        return false;

    return _world && _world->IsDebugGlobalInvulnerabilityEnabled();
}

void NC_STACK_ypabact::ModifyEnergy(bact_arg84 *arg)
{
    // OpenNeoUA attacker modifiers share this single final-damage choke point.
    // Clone malus and the per-instance kill-mark bonus are folded into one value,
    // applied exactly once to direct weapons, missiles, lasers, MGUN and blast.
    // Healing, environmental bypasses and shared weapon prototypes stay untouched.
    if ( arg->energy < 0 && !arg->bypassAttackerDamageModifiers && arg->unit )
        arg->energy = arg->unit->GetEffectiveOutgoingEnergy(arg->energy);

    if ( IsInvulnerableToDamage() && arg->energy < 0 )
    {
        // F3 debug uses invulnerable friendly units as repeatable test dummies.
        // At this point the normal weapon path has already applied target shield/
        // debuff handling and the attacker modifier above. Record the effective
        // damage the hit would have caused, then keep invulnerability unchanged.
        if ( _world && arg->unit )
        {
            const int attemptedDamage = arg->energy == std::numeric_limits<int>::min()
                ? std::numeric_limits<int>::max()
                : -arg->energy;
            if ( attemptedDamage > 0 )
                _world->DebugRecordDpsDamage(arg->unit, this, attemptedDamage);
        }
        return;
    }

    if (_world && (_oflags & BACT_OFLAG_VIEWER))
    {
        if (_world->getYW_invulnerable() && arg->energy > -1000000)
            return;
    }

    bool isNetGame = false;
    if (_world && _world->_isNetGame)
        isNetGame = true;

    // ---- OpenNeoUA: protective Unit Gun/module damage absorption (single-player only) ----
    // Route incoming damage to an active protective attachment before it
    // reaches the parent. If the module survives, the parent takes nothing; if
    // the hit destroys the module, only the leftover passes through. Net games
    // keep vanilla routing to avoid desync.
    if ( arg->energy < 0 && !_isDummy && !isNetGame )
    {
        NC_STACK_ypabact *prot = SelectProtectiveUnitGun(arg->unit);
        if ( prot && prot != this )
        {
            int incoming = -arg->energy;   // positive damage amount
            int moduleHP = prot->_energy; // remaining module health

            if ( incoming <= moduleHP )
            {
                // Module absorbs the whole hit; parent untouched.
                bact_arg84 dmgArg;
                dmgArg.energy = arg->energy;
                dmgArg.unit   = arg->unit;
                dmgArg.killerOwner = arg->killerOwner;
                dmgArg.bypassAttackerDamageModifiers = arg->bypassAttackerDamageModifiers;
                prot->ModifyEnergy(&dmgArg);
                return;
            }

            // Module is destroyed; only the leftover damage passes to the parent.
            bact_arg84 dmgArg;
            dmgArg.energy = -moduleHP;
            dmgArg.unit   = arg->unit;
            dmgArg.killerOwner = arg->killerOwner;
            dmgArg.bypassAttackerDamageModifiers = arg->bypassAttackerDamageModifiers;
            prot->ModifyEnergy(&dmgArg);

            arg->energy += moduleHP;       // reduce parent damage by absorbed part
            if ( arg->energy >= 0 )
                return;
        }
    }

    bool friendlyFire = false;
    if (!arg->unit || _owner == arg->unit->_owner)
        friendlyFire = true;

    if ( isNetGame && friendlyFire == false )
    {
        uamessage_vhclEnergy vhclEnrgy;
        vhclEnrgy.msgID = UAMSG_VHCLENERGY;
        vhclEnrgy.owner = _owner;
        vhclEnrgy.id = _gid;
        vhclEnrgy.energy = arg->energy;

        if ( arg->unit )
        {
            vhclEnrgy.killer = arg->unit->_gid;
            vhclEnrgy.killerOwner = arg->unit->_owner;
        }
        else
        {
            vhclEnrgy.killer = 0;
            vhclEnrgy.killerOwner = 0;
        }

        _world->NetBroadcastMessage(&vhclEnrgy, sizeof(vhclEnrgy), true);
    }
    else
    {
        // Keep the last real enemy faction that damaged this unit. A later
        // lethal hit from an ally (for example a friendly collision) must not
        // transfer a Host Station's sectors back to its defeated faction.
        const int16_t damageOwner = arg->killerOwner
                                  ? arg->killerOwner
                                  : (arg->unit ? arg->unit->_owner : World::OWNER_0);
        if ( arg->energy < 0 && damageOwner > World::OWNER_0 && damageOwner != _owner )
            _killer_owner = damageOwner;

        if ( arg->energy < 0 && _world )
        {
            _world->NoteUserDamageHover(arg->unit, this);

            // F3 DPS debug records real HP loss on vulnerable targets. Invulnerable
            // debug dummies are recorded earlier as effective attempted damage, so
            // they can be fired on indefinitely without forcing the meter to zero.
            const int incomingDamage = arg->energy == std::numeric_limits<int>::min()
                ? std::numeric_limits<int>::max()
                : -arg->energy;
            const int actualDamage = std::min(incomingDamage, std::max(_energy, 0));
            if ( actualDamage > 0 )
                _world->DebugRecordDpsDamage(arg->unit, this, actualDamage);
        }

        _energy += arg->energy;

        if ( _energy <= 0 )
        {
            // Hostile damage has already refreshed _killer_owner above. Keep
            // that attribution when the final hit is friendly or ownerless.
            if ( _killer_owner == _owner )
                _killer_owner = World::OWNER_0;

            _killer = arg->unit;
            _status_flg &= ~BACT_STFLAG_LAND;

            setState_msg v16;
            v16.newStatus = BACT_STATUS_DEAD;
            v16.unsetFlags = 0;
            v16.setFlags = 0;

            SetState(&v16);

            Die();
        }
    }
}

int NC_STACK_ypabact::GetPlasmaDurationMs() const
{
    int vanillaDuration = (int)((float)_energy_max * 0.7f);
    if ( vanillaDuration < 10000 )
        vanillaDuration = 10000;
    if ( vanillaDuration > 25000 )
        vanillaDuration = 25000;

    if ( !_world || _world->_isNetGame )
        return vanillaDuration;

    const double scaledDuration =
        (double)vanillaDuration * (double)ypabact_GetPlasmaDeathDurationMultiplier();
    if ( !std::isfinite(scaledDuration) || scaledDuration <= 0.0 )
        return vanillaDuration;
    if ( scaledDuration >= (double)std::numeric_limits<int>::max() )
        return std::numeric_limits<int>::max();

    return std::max(1, (int)scaledDuration);
}

void NC_STACK_ypabact::UpdateDeathPlasmaMagnet(int frameTime)
{
    if ( !_world || _world->_isNetGame || frameTime <= 0 ||
         !(_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) || _scale_time <= 0 )
        return;

    NC_STACK_ypabact *player = _world->getYW_userVehicle();
    if ( !player || player == this || !player->getBACT_inputting() )
        return;

    const float radius = ypabact_GetPlasmaDeathMagnetRadius();
    const float speed = ypabact_GetPlasmaDeathMagnetSpeed();
    if ( radius <= 0.0f || speed <= 0.0f ||
         !player->CanCollectPlasmaFrom(this) )
        return;

    const vec3d plasmaPos = _vp_extra[0].pos;
    const vec3d offset = player->_position - plasmaPos;
    const float distance = offset.length();
    if ( !isfinite(distance) || distance > radius )
        return;

    // Magnet pickup is gameplay logic, not physical collision. A precise
    // collision shape can leave the vehicle origin inside empty hull space, so
    // plasma that reaches the player centre must be collected directly instead
    // of waiting for a shape contact that may never exist.
    if ( distance <= 0.001f )
    {
        player->CollectPlasmaFrom(this);
        return;
    }

    const float step = speed * (float)frameTime * 0.001f;
    if ( !isfinite(step) || step <= 0.0f )
        return;

    const float travel = std::min(step, distance);
    const vec3d target = plasmaPos + offset * (travel / distance);

    bact_arg80 move;
    move.pos = target;
    move.field_C = 2;
    if ( NC_STACK_ypabact::SetPosition(&move) )
    {
        _vp_extra[0].pos = _position;

        // Complete the magnet interaction in the same frame that the plasma
        // reaches the player. This keeps pickup independent from legacy,
        // compound and collision_shape geometry alike.
        if ( travel >= distance )
            player->CollectPlasmaFrom(this);
    }
}

World::TAbsoluteOrPercent ypabact_GetPlasmaCurrencyGain()
{
    static const World::TAbsoluteOrPercent gain = []()
    {
        World::TAbsoluteOrPercent result;
        result.defined = true;
        result.percent = true;
        result.value = 100.0f;

        World::TAuthoredScalar parsed;
        if ( !World::ParseAuthoredScalar(System::IniConf::GamePlasmaCurrencyGain.Get<std::string>(), parsed) ||
             !std::isfinite(parsed.value) || parsed.value < 0.0f )
        {
            return result;
        }

        result.percent = parsed.percent;
        result.value = parsed.percent ? std::min(parsed.value, 100.0f) : parsed.value;
        return result;
    }();

    return gain;
}

uint64_t ypabact_CalculatePlasmaCurrencyValue(const NC_STACK_ypabact *source)
{
    if ( !source )
        return 0;

    const int energyMax = source->_energy_max;
    const int remainingTime = source->_scale_time;
    const int duration = source->GetPlasmaDurationMs();
    if ( energyMax <= 0 || remainingTime <= 0 || duration <= 0 )
        return 0;

    const double remainingFraction = (double)remainingTime / (double)duration;
    const double rawValue = (double)energyMax * remainingFraction;
    if ( !std::isfinite(remainingFraction) || !std::isfinite(rawValue) || rawValue <= 0.0 )
        return 0;

    const double clampedValue = std::min(rawValue, (double)energyMax);
    if ( clampedValue <= 0.0 )
        return 0;

    const World::TAbsoluteOrPercent gain = ypabact_GetPlasmaCurrencyGain();
    if ( gain.value <= 0.0f )
        return 0;

    const double creditedValue = gain.percent
        ? clampedValue * (double)gain.value / 100.0
        : (double)gain.value;
    if ( !std::isfinite(creditedValue) || creditedValue <= 0.0 )
        return 0;

    // Any still-valid residue with a positive gain percentage is worth at
    // least one currency point.  This prevents low percentages from rounding
    // a legitimate late pickup down to zero.
    return std::max<uint64_t>(1, (uint64_t)std::floor(creditedValue));
}

bool NC_STACK_ypabact::CanRecoverPlasmaEnergyFrom(const NC_STACK_ypabact *source) const
{
    if ( !source || source == this || !_world ||
         _status == BACT_STATUS_DEAD ||
         (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) ||
         _energy >= _energy_max ||
         source->_status != BACT_STATUS_DEAD ||
         !(source->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) ||
         source->_scale_time <= 0 )
        return false;

    if ( _oflags & BACT_OFLAG_USERINPT )
        return true;

    if ( _world->_isNetGame || source->_bact_type == BACT_TYPES_ROBO )
        return false;

    NC_STACK_ypabact *userHost = _world->getYW_userHostStation();
    return userHost && _owner > World::OWNER_0 && _owner == userHost->_owner;
}

bool NC_STACK_ypabact::CanCreditPlasmaCurrencyFrom(const NC_STACK_ypabact *source) const
{
    if ( !source || source == this || !_world || !_world->IsPlasmaCurrencyEnabled() ||
         _status == BACT_STATUS_DEAD ||
         (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) ||
         source->_status != BACT_STATUS_DEAD ||
         !(source->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE) ||
         source->_scale_time <= 0 ||
         ypabact_GetPlasmaCurrencyGain().value <= 0.0f )
        return false;

    const int playerOwner = _world->GetPlayerOwner();
    if ( playerOwner <= World::OWNER_0 || _owner != playerOwner )
        return false;

    // Currency is a property of collecting a valid enemy Plasma residue, not
    // of who delivered the killing blow.  Friendly/player-faction casualties
    // never generate Currency, but third-party and environment deaths remain
    // collectible when the residue belongs to an enemy faction.
    return source->_owner > World::OWNER_0 && source->_owner != playerOwner;
}

bool NC_STACK_ypabact::CanCollectPlasmaFrom(const NC_STACK_ypabact *source) const
{
    // Keep vanilla energy eligibility and Plasma-currency eligibility separate.
    // A full-energy allied unit may still collect any valid enemy residue for
    // the global currency, without gaining/overflowing vanilla energy.
    return CanRecoverPlasmaEnergyFrom(source) || CanCreditPlasmaCurrencyFrom(source);
}

bool NC_STACK_ypabact::CollectPlasmaFrom(NC_STACK_ypabact *source)
{
    const bool recoverEnergy = CanRecoverPlasmaEnergyFrom(source);
    const bool creditCurrency = CanCreditPlasmaCurrencyFrom(source);
    if ( !recoverEnergy && !creditCurrency )
        return false;

    const uint64_t plasmaCurrencyValue =
        creditCurrency ? ypabact_CalculatePlasmaCurrencyValue(source) : 0;
    const vec3d plasmaPosition = source->_vp_extra[0].pos;

    if ( recoverEnergy )
        CollisionWithBact__sub0(this, source);
    if ( plasmaCurrencyValue > 0 )
        _world->AddPlasmaCurrency(plasmaCurrencyValue, plasmaPosition);
    source->_scale_time = -1;

    if ( _soundcarrier.Sounds.size() > World::TVhclProto::SND_PICKUP )
        SFXEngine::SFXe.startSound(&_soundcarrier, World::TVhclProto::SND_PICKUP);

    if ( _world->_isNetGame )
    {
        uamessage_endPlasma epMsg;
        epMsg.msgID = UAMSG_ENDPLASMA;
        epMsg.owner = source->_owner;
        epMsg.id = source->_gid;
        _world->NetBroadcastMessage(&epMsg, sizeof(epMsg), true);

        if ( source->_owner != _owner )
        {
            source->_vp_extra[0].flags = 0;
            source->_vp_extra[0].SetVP((NC_STACK_base::Instance *)nullptr);
        }
    }

    return true;
}

size_t NC_STACK_ypabact::IsDestroyed()
{
    return (GetVP() == _vp_dead || GetVP() == _vp_genesis || GetVP() == _vp_megadeth) && _status == BACT_STATUS_DEAD;
}

bool ypabact_GetCompoundFXGeometry(NC_STACK_ypabact *bact,
                                           vec3d *center,
                                           float *radius)
{
    if ( !bact || !bact->HasManualCompoundCollision() || bact->UsesLegacyRadiusCollision() )
        return false;

    World::rbcolls *colls = bact->getBACT_collNodes();
    if ( !colls )
        return false;

    vec3d weightedCenter(0.0, 0.0, 0.0);
    double totalWeight = 0.0;

    for (const World::TRoboColl &sphere : colls->roboColls)
    {
        if ( sphere.robo_coll_radius <= 0.01f )
            continue;

        double sphereRadius = sphere.robo_coll_radius;
        double weight = sphereRadius * sphereRadius * sphereRadius;
        weightedCenter += sphere.coll_pos * weight;
        totalWeight += weight;
    }

    if ( totalWeight <= 0.0 )
        return false;

    weightedCenter /= totalWeight;

    if ( center )
        *center = weightedCenter;

    if ( radius )
    {
        float broadRadius = 0.0f;
        float padding = bact->getBACT_collPadding();

        for (const World::TRoboColl &sphere : colls->roboColls)
        {
            if ( sphere.robo_coll_radius <= 0.01f )
                continue;

            float extent = (sphere.coll_pos - weightedCenter).length() +
                           sphere.robo_coll_radius + padding;
            if ( extent > broadRadius )
                broadRadius = extent;
        }

        *radius = broadRadius;
    }

    return true;
}

void NC_STACK_ypabact::StartDestFX(const World::DestFX &fx,
                                  const vec3d *impactPos, const mat3x3 *impactRot,
                                  bool randomRotation)
{
    ypaworld_arg146 arg146;

    vec3d compoundCenter;
    float compoundRadius = 0.0f;
    const bool hasCompoundFXGeometry =
        !impactPos && ypabact_GetCompoundFXGeometry(this, &compoundCenter, &compoundRadius);

    arg146.pos = impactPos ? *impactPos : _position;
    float effectRadius = _radius;

    if ( hasCompoundFXGeometry )
    {
        arg146.pos += _rotation.Transpose().Transform(compoundCenter);
        effectRadius = compoundRadius;
    }

    arg146.vehicle_id = fx.ModelID;

    if ( impactPos && impactRot )
        arg146.pos += impactRot->Transform(fx.Pos);
    else if ( (hasCompoundFXGeometry && effectRadius > 0.01f) ||
         (!hasCompoundFXGeometry && _radius > 31.0) )
    {
        float len = fx.Pos.length();

        if ( len > 0.1 )
        {
            vec3d pos = fx.Pos / len * effectRadius;

            arg146.pos += _rotation.Transform(pos);
        }
    }

    NC_STACK_ypabact *bah = _world->ypaworld_func146(&arg146);

    if ( bah )
    {
        if ( randomRotation )
            bah->_rotation = World::RandomFXRotation(bah->_rotation, true);

        _world->ypaworld_func134(bah);

        setState_msg v18;
        v18.newStatus = BACT_STATUS_DEAD;
        v18.setFlags = 0;
        v18.unsetFlags = 0;

        bah->SetStateInternal(&v18);

        bah->_fly_dir = (impactRot ? *impactRot : _rotation).Transform(fx.Pos);

        if ( fx.Accel )
            bah->_fly_dir += _fly_dir * _fly_dir_length;

        float len = bah->_fly_dir.length();

        if ( len > 0.001 )
        {
            bah->_fly_dir /= len;
            bah->_fly_dir_length = len;
        }

    }
}

void NC_STACK_ypabact::StartDestFXByType(uint8_t type)
{
    if ( _world->ypaworld_func145(this) )
    {
        size_t a4 = _world->getYW_destroyFX();

        if (a4 > _destroyFX.size())
            a4 = _destroyFX.size();

        for (size_t i = 0; i < a4; i++)
        {
            if ( _destroyFX[i].ModelID )
            {
                const World::DestFX &fx = _destroyFX[i];

                if ( fx.Type == type )
                    StartDestFX(fx);
            }
        }

        for (const World::DestFX &x : _extDestroyFX)
        {
            if (x.ModelID != 0 && x.Type == type)
                StartDestFX(x);
        }
    }
}

bool NC_STACK_ypabact::StartChainFXByTrigger(
    uint8_t trigger, const ypaworld_arg136 *worldHit,
    const std::vector<World::TChainFXConfig> *fxOverride,
    const vec3d *impactPos, const mat3x3 *impactRot)
{
    const std::vector<World::TChainFXConfig> &chainFX = fxOverride ? *fxOverride : _chainFX;
    if ( !_world || chainFX.empty() )
        return false;

    bool hasGroundDecal = false;
    for (const World::TChainFXConfig &fx : chainFX)
    {
        if ( fx.trigger == trigger && fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL )
        {
            hasGroundDecal = true;
            break;
        }
    }

    // Ground decals are sized from the visual model of this unit or projectile.
    const float decalModelDiameter =
        hasGroundDecal ? World::GroundDecalModelDiameter(_vp_normal, _vp_scale) : 0.0f;

    // Persistent terrain decals are generated even when the source is outside
    // the normal FX visibility range. Existing visual/physical Chain FX keep
    // their historical visibility gate.
    const bool sourceVisible = impactPos || worldHit || _world->ypaworld_func145(this);
    if ( !worldHit && !hasGroundDecal && !sourceVisible )
        return false;

    bool spawned = false;
    for (const World::TChainFXConfig &fx : chainFX)
    {
        if ( fx.trigger != trigger )
            continue;

        if ( fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL )
        {
            if ( worldHit && trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD )
            {
                if ( _world->SpawnGroundDecal(fx, *worldHit, decalModelDiameter) )
                    spawned = true;
            }
            else if ( !worldHit &&
                      trigger == World::TChainFXConfig::TRIGGER_DESTROYED )
            {
                if ( _kamikaze_ground_decals_handled || !ypabact_HasGroundDecalSupport(this) )
                    continue;

                // Vehicle crash decals are anchored to the real terrain below
                // the vehicle. Reacquire the hit for each decal because decal
                // geometry preparation can reuse temporary collision data.
                ypaworld_arg136 projectedGroundHit;
                projectedGroundHit.stPos = vec3d(_position.x, -30000.0, _position.z);
                projectedGroundHit.vect = vec3d(0.0, 50000.0, 0.0);
                projectedGroundHit.flags = 0;
                _world->ypaworld_func136(&projectedGroundHit);

                if ( projectedGroundHit.isect &&
                     _world->SpawnGroundDecal(fx, projectedGroundHit, decalModelDiameter) )
                    spawned = true;
            }

            continue;
        }

        // A real world collision is supplied in an immediate first pass so the
        // temporary collision skeleton can be copied safely. Existing visual
        // and physical Chain FX remain on their normal SetState pass and keep
        // the normal visibility check.
        if ( (worldHit && !impactPos) || !sourceVisible )
            continue;

        if ( fx.mode == World::TChainFXConfig::MODE_VISUAL )
        {
            vec3d visualPos = impactPos ? *impactPos : _position;
            mat3x3 visualRot = impactRot ? *impactRot : _rotation;
            vec3d compoundCenter;
            if ( !impactPos && ypabact_GetCompoundFXGeometry(this, &compoundCenter, nullptr) )
                visualPos += _rotation.Transpose().Transform(compoundCenter);

            vec3d visualOffset;
            mat3x3 visualRotationDelta;
            if ( !impactPos && GetProjectileVisualMotionDelta(&visualOffset, &visualRotationDelta) )
            {
                visualPos += visualOffset;
                visualRot = (_rotation.Transpose() * visualRotationDelta).Transpose();
            }

            const int count = World::RandomIntRangeInclusive(fx.count_min, fx.count_max);
            for (int i = 0; i < count; ++i)
                _world->SpawnChainFX(fx, visualPos, visualRot);
        }
        else if ( fx.mode == World::TChainFXConfig::MODE_PHYSICAL )
        {
            const int count = World::RandomIntRangeInclusive(fx.count_min, fx.count_max);
            for (int i = 0; i < count; ++i)
            {
                if ( fx.physical_inline )
                {
                    const vec3d launch = World::RandomVec3RangeInclusive(fx.launch_min,
                                                                          fx.launch_max);
                    const vec3d pos = impactPos ? *impactPos : _position;
                    const mat3x3 &rot = impactRot ? *impactRot : _rotation;
                    _world->SpawnInlinePhysicalFX(fx, pos, rot, launch, _owner, this);
                    continue;
                }

                World::DestFX tempFx;
                tempFx.ModelID = fx.physical_vehicle;
                // Offset min_max is rolled independently for every generated instance.
                tempFx.Pos = World::RandomVec3RangeInclusive(fx.offset_min, fx.offset_max);
                StartDestFX(tempFx, impactPos, impactRot, fx.random_rotation);
            }
        }

        spawned = true;
    }

    return spawned;
}

void NC_STACK_ypabact::ChangeSectorEnergy(yw_arg129 *arg)
{
    if ( _world && _world->IsSpectatorBact(this) )
        return;

    // Sector/building damage bypasses ModifyEnergy, so reuse the same attacker-side
    // damage helper used by units, missiles, lasers, MGUN and blast.
    if ( arg->field_10 > 0 && arg->unit )
        arg->field_10 = arg->unit->GetEffectiveOutgoingEnergy(arg->field_10);

    arg->OwnerID = World::OWNER_RECALC;

    _world->ypaworld_func129(arg);

    yw_130arg arg130;
    arg130.pos_x = arg->pos.x;
    arg130.pos_z = arg->pos.z;

    int v5;

    if ( _world->GetSectorInfo(&arg130) )
        v5 = arg130.pcell->owner;
    else
        v5 = 0;

    if ( _world->_isNetGame )
    {
        uamessage_sectorEnergy seMsg;
        seMsg.msgID = UAMSG_SECTORENERGY;
        seMsg.owner = _owner;
        seMsg.pos = arg->pos;
        seMsg.energy = arg->field_10;
        seMsg.sectOwner = v5;

        if ( arg->unit )
            seMsg.whoHit = arg->unit->_gid;
        else
            seMsg.whoHit = 0;

        _world->NetBroadcastMessage(&seMsg, sizeof(seMsg), true);
    }
}

void sb_0x4874c4(NC_STACK_ypabact *bact, int a2, int a3, float a4)
{
    if (a2 == 0)
        a2 = 1;

    bact->_vp_extra[0].scale = sqrt( (float)bact->_scale_time / (float)a2 ) * a4;

    if ( bact->_vp_extra[0].scale < 0.0 )
        bact->_vp_extra[0].scale = 0;

    bact->_vp_extra[0].rotate = mat3x3::RotateY(bact->_maxrot * 2.0 * (float)a3 * 0.001) * bact->_vp_extra[0].rotate;
}

// OpenNeoUA follow_vehicle: mirrors the guiding fragment's trajectory onto its
// wreck. The render transform is copied too, so the wreck stays in sync even
// when unit update order puts the fragment last in the frame.
void ypabact_CopyDeadFollowTransform(NC_STACK_ypabact *dst, NC_STACK_ypabact *src)
{
    dst->_old_pos = dst->_position;
    dst->_position = src->_position;
    dst->_rotation = src->_rotation;
    dst->_fly_dir = src->_fly_dir;
    dst->_fly_dir_length = src->_fly_dir_length;

    dst->_tForm.Pos = dst->GetBodyPosition();
    if ( dst->_status_flg & BACT_STFLAG_SCALE )
        dst->_tForm.SclRot = dst->_rotation.Transpose() * mat3x3::Scale(dst->_scale);
    else
        dst->_tForm.SclRot = dst->_rotation.Transpose();
}

void NC_STACK_ypabact::ClearDeadFollowLink()
{
    if ( _deadFollowWreck )
    {
        _deadFollowWreck->_deadFollowLeader = nullptr;
        _deadFollowWreck = nullptr;
    }

    if ( _deadFollowLeader )
    {
        _deadFollowLeader->_deadFollowWreck = nullptr;
        _deadFollowLeader = nullptr;
    }
}

void NC_STACK_ypabact::DeadTimeUpdate(update_msg *arg)
{
    // OpenNeoUA follow_vehicle: a wreck bound to a guiding fragment only mirrors
    // that fragment's trajectory. Its own fall physics and death timeout stay
    // frozen until the link breaks (fragment impact/DEATH2 or removal).
    if ( _deadFollowLeader )
    {
        if ( !(_deadFollowLeader->_status_flg & (BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
            return;

        // The fragment ends here: leave the wreck at this exact spot and let its
        // own death finish (DEATH2 -> plasma) run from the impact point.
        ClearDeadFollowLink();
        _status_flg |= BACT_STFLAG_LAND;
    }

    const int deadLifetimeMs = _inlinePhysicalFX ?
        (_inlinePhysicalLifetimeMs > 0 ? _inlinePhysicalLifetimeMs : 30000) : 5000;

    if ( _status_flg & BACT_STFLAG_LAND ||
         (_clock - _dead_time > deadLifetimeMs && _status_flg & BACT_STFLAG_DEATH1 ) )
    {
        if ( !(_status_flg & BACT_STFLAG_DEATH2) )
        {
            setState_msg arg78;
            arg78.newStatus = BACT_STATUS_NOPE;
            arg78.unsetFlags = 0;
            arg78.setFlags = BACT_STFLAG_DEATH2;

            SetState(&arg78);
        }

        _status_flg |= BACT_STFLAG_LAND;

        // disable_genesis: the wreck falls as usual but leaves no death plasma.
        if ( _owner && _bact_type != BACT_TYPES_MISSLE && _vp_genesis && !_disable_genesis )
        {
            int a2 = GetPlasmaDurationMs();

            if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
            {
                UpdateDeathPlasmaMagnet(arg->frameTime);
                _scale_time -= arg->frameTime;

                if ( _scale_time <= 0 )
                {
                    _vp_extra[0].SetVP((NC_STACK_base::Instance *)nullptr);

                    if ( _yls_time <= 0 )
                    {

                        if ( _oflags & BACT_OFLAG_USERINPT )
                            _status_flg |= BACT_STFLAG_NORENDER;
                        else
                            Release();

                    }
                }
                else
                {
                    sb_0x4874c4(this, a2, arg->frameTime, 0.75);

                    if ( _yls_time <= 0 )
                        _status_flg |= BACT_STFLAG_NORENDER;
                }
            }
            else
            {
                _scale_time = a2;
                _vp_extra[0].scale = 0.75;
                _vp_extra[0].pos = _position;
                _vp_extra[0].rotate = _rotation;
                _vp_extra[0].SetVP(_vp_genesis);
                _vp_extra[0].flags |= (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);

                if ( _world->_isNetGame )
                {
                    uamessage_startPlasma splMsg;
                    splMsg.msgID = UAMSG_STARTPLASMA;
                    splMsg.owner = _owner;
                    splMsg.scale = 0.75;
                    splMsg.time = a2;
                    splMsg.id = _gid;
                    splMsg.pos = _position;
                    splMsg.dir = _rotation;

                    _world->NetBroadcastMessage(&splMsg, sizeof(splMsg), true);
                }
            }
        }
        else if ( _yls_time <= 0 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
                _status_flg |= BACT_STFLAG_NORENDER;
            else
                Release();
        }
    }
    else
    {
        if ( _inlinePhysicalFX && _maxrot > 0.0f )
            _rotation = mat3x3::RotateY(_maxrot * arg->frameTime * 0.001f) * _rotation;

        bact_arg86 arg86;
        arg86.field_one = 3;
        arg86.field_two = arg->frameTime;

        CrashOrLand(&arg86);

        // OpenNeoUA follow_vehicle: after the fragment's own fall step, drag the
        // bound wreck onto the same trajectory. Skipped after Release() moves the
        // fragment out of the world.
        if ( _deadFollowWreck && !(_status_flg & BACT_STFLAG_NORENDER) )
            ypabact_CopyDeadFollowTransform(_deadFollowWreck, this);
    }
}

// OpenNeoUA Buff: permanently reveal a buff_invisible unit when it makes a
// real attack. Attached unit-gun children fire on behalf of their carrier, so a
// child attack reveals the carrier (which in turn reveals all its attached children).
// No-op for units without active Buff invisibility or units already revealed.
void NC_STACK_ypabact::RevealInvisibleOnAttack()
{
    // Attached unit-guns/modules: redirect the reveal to the carrying unit.
    if ( (_isUnitGunChild || _isDummy) && _parent && _parent != this )
    {
        _parent->RevealInvisibleOnAttack();
        return;
    }

    if ( !_invisibleUnrevealed )
        return;

    _invisibleUnrevealed = false;

    if ( _buff_invisible_reveal_vp > 0 || !_buff_invisible_reveal_3ds.empty() ||
         !_buff_invisible_reveal_base.empty() )
    {
        NC_STACK_ypaworld *world = getBACT_pWorld();
        if ( world )
            world->SpawnTransientVisual(_buff_invisible_reveal_vp,
                                        _buff_invisible_reveal_3ds,
                                        _buff_invisible_reveal_base,
                                        _position, _rotation, 1000);
    }

    // Reveal attached unit-guns/modules together with their carrier so the whole unit
    // becomes visible/targettable in the same frame.
    for ( World::TRoboGun &gun : _unitGuns )
    {
        if ( gun.gun_obj )
            gun.gun_obj->_invisibleUnrevealed = false;
    }

}
