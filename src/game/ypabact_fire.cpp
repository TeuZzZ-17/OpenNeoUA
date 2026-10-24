// Firing: missile launch, minigun, weapon direction patterns and fire visuals.
#include "game/ypabact_internal.h"

float ypabact_GetMinigunRange()
{
    static const float range = []()
    {
        float parsed = ypabact_ReadNonNegativeFloatIni(System::IniConf::GameMgunRange, 1000.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return 1000.0f;

        // Avoid unbounded continuous-fire world scans from extreme values.
        return std::min(parsed, 6000.0f);
    }();

    return range;
}

float ypabact_GetMinigunAiFireAlignment()
{
    static const float alignment = []()
    {
        const float dflt = 0.85f;
        std::string value = System::IniConf::GameMgunAiFireAlignment.Get<std::string>();
        if ( value.empty() || value.find(',') != std::string::npos )
            return dflt;

        try
        {
            size_t pos = 0;
            float parsed = std::stof(value, &pos);
            if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
                 !isfinite(parsed) )
                return dflt;

            return std::max(-1.0f, std::min(parsed, 1.0f));
        }
        catch (...)
        {
            return dflt;
        }
    }();

    return alignment;
}

bool NC_STACK_ypabact::HasMinigun() const
{
    if ( _mgun_set )
        return _mgun != -1;

    return _mgun_shot_time > 0;
}

float NC_STACK_ypabact::GetMinigunRange() const
{
    return ypabact_GetMinigunRange();
}

int NC_STACK_ypabact::GetMinigunShotTime(int frameDeltaMs) const
{
    // Vehicle-level MGUN cadence mirrors Weapon shot_time_user semantics:
    // player control may opt into its own timing, while AI and old data keep
    // using mgun_shot_time. A zero/unset override is a strict fallback.
    int baseShotTime = _mgun_shot_time;
    if ( getBACT_inputting() && _mgun_shot_time_user > 0 )
        baseShotTime = _mgun_shot_time_user;

    int shotTime = GetEffectiveShotTime(baseShotTime, true);

    if ( shotTime < frameDeltaMs )
        shotTime = frameDeltaMs;

    return shotTime;
}

NC_STACK_ypabact *ypabact_GetDistributedMissileTarget(const std::vector<NC_STACK_ypabact *> &targets, int shotIndex)
{
    if ( targets.empty() )
        return nullptr;

    return targets[shotIndex % targets.size()];
}

bool ypabact_IsLiveProximityMissileOwner(NC_STACK_ypabact *candidate, NC_STACK_ypabact *unit)
{
    if ( !candidate || candidate == unit )
        return false;

    if ( candidate->_status == BACT_STATUS_DEAD )
        return false;

    if ( candidate->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN) )
        return false;

    return true;
}

void ypabact_SpawnWeaponImpactVisual(NC_STACK_ypaworld *world,
                                             const World::TWeapProto &wproto,
                                             bool preferMegadeth,
                                             const vec3d &pos,
                                             const mat3x3 &rot,
                                             int32_t lifeTime)
{
    if ( !world )
        return;

    const bool hasDead = wproto.vp_dead > 0 || !wproto.visual_3ds.dead.empty() ||
                         !wproto.visual_base.dead.empty();
    const bool hasMegadeth = wproto.vp_megadeth > 0 || !wproto.visual_3ds.megadeth.empty() ||
                             !wproto.visual_base.megadeth.empty();

    if ( preferMegadeth && hasMegadeth )
        world->SpawnTransientVisual(wproto.vp_megadeth, wproto.visual_3ds.megadeth,
                                    wproto.visual_base.megadeth, pos, rot, lifeTime,
                                    1.0f, wproto.visual_tint);
    else if ( hasDead )
        world->SpawnTransientVisual(wproto.vp_dead, wproto.visual_3ds.dead,
                                    wproto.visual_base.dead, pos, rot, lifeTime,
                                    1.0f, wproto.visual_tint);
    else if ( hasMegadeth )
        world->SpawnTransientVisual(wproto.vp_megadeth, wproto.visual_3ds.megadeth,
                                    wproto.visual_base.megadeth, pos, rot, lifeTime,
                                    1.0f, wproto.visual_tint);
}

void ypabact_StartVehicleFireVP(NC_STACK_ypabact *bact, int now)
{
    if ( !bact || (bact->_status != BACT_STATUS_NORMAL && bact->_status != BACT_STATUS_IDLE) )
        return;

    bact->_vehicle_fire_vp_end_time = now + 180;

    if ( bact->_vp_active != 7 )
    {
        bact->_vp_active = 7;
        bact->SetVP(bact->_vp_fire);
    }
}

void ypabact_StartVehicleFireVPIfEnabled(NC_STACK_ypabact *bact, int now)
{
    if ( !bact )
        return;

    // An attached Unit Gun shows the fire visual on its carrier Vehicle, so only
    // the carrier's weapon_use_vehicle_fire counts for it.
    NC_STACK_ypabact *vehicle = (bact->_isUnitGunChild && bact->_parent) ? bact->_parent : bact;
    if ( vehicle->_weapon_use_vehicle_fire )
        ypabact_StartVehicleFireVP(vehicle, now);
}

size_t NC_STACK_ypabact::LaunchMissile(bact_arg79 *arg)
{
    // Gate before laser requests, cadence, energy, visuals or projectile creation.
    // Player aim and the actual target must both be inside the same scope.
    if ( (_oflags & BACT_OFLAG_USERINPT) && !IsPlayerPrimaryWeaponElevationAllowed() )
        return 0;
    const vec3d scopeAim = arg->tgType == BACT_TGT_TYPE_UNIT && arg->target.pbact
        ? arg->target.pbact->_position - _position
        : (arg->tgType == BACT_TGT_TYPE_CELL ? arg->tgt_pos - _position : arg->direction);
    if ( !IsPrimaryWeaponElevationAllowed(scopeAim) )
        return 0;

    if ( IsActiveDebuffStunFireBlocked() )
        return 0;

    if ( _world && _world->IsSpectatorBact(this) )
        return 0;

    NC_STACK_ypamissile *wobj = nullptr;

    int slots[4];
    int sourceSlots[4];
    int slotCount = ypabact_GetPrimaryWeaponSlots(this, slots, sourceSlots);
    const bool usePlayerRandomSlots =
        arg->weapon == _weapon &&
        arg->weapon_source_slot < 0 &&
        (_oflags & BACT_OFLAG_USERINPT) &&
        _weapon_player_switch_mode == World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_RANDOM &&
        slotCount > 1;

    int selectedWeapon = -1;
    int selectedWeaponSourceSlot = 0;
    int cooldownWeapon = -1;
    NC_STACK_ypabact *weaponTarget =
        arg->tgType == BACT_TGT_TYPE_UNIT ? arg->target.pbact : nullptr;

    if ( usePlayerRandomSlots )
    {
        // Preserve the existing player Random timing semantics: the current
        // slot supplies the cooldown gate, then the shot chooses a random slot.
        int currentWeapon = ypabact_GetWeaponIdForSourceSlot(this, _current_weapon_source_slot);
        cooldownWeapon = currentWeapon == _current_weapon_id &&
                         ypabact_IsValidFireWeaponId(this, currentWeapon)
                             ? currentWeapon
                             : slots[0];
    }
    else
    {
        selectedWeapon = ypabact_SelectPrimaryWeaponSlot(
            this, arg->weapon, arg->weapon_source_slot, weaponTarget, &selectedWeaponSourceSlot);
        cooldownWeapon = selectedWeapon;
    }

    if ( cooldownWeapon == -1 || !ypabact_IsValidWeaponId(this, cooldownWeapon) )
        return 0;

    World::TWeapProto &cooldownProto = _world->GetWeaponsProtos().at(cooldownWeapon);

    if ( !getBACT_inputting() && cooldownProto.HasAIFireTriggerArea() && !cooldownProto.IsBombLike() &&
         !IsPrimaryWeaponElevationAllowed(scopeAim, true) )
        return 0;

    // Central safety gate: a Kamikaze Weapon is a mounted carrier payload,
    // never a normally launchable projectile, even from secondary call sites.
    if ( cooldownProto.IsKamikaze() )
        return 0;

    const bool cockpitDirectAim =
        arg && arg->tgType == BACT_TGT_TYPE_DRCT && IsCockpitCameraActive();
    const vec3d cockpitRequestedViewDir = cockpitDirectAim ? arg->direction : vec3d();

    bact_arg79 cockpitAimArg;
    if ( cockpitDirectAim )
    {
        cockpitAimArg = *arg;
        vec3d origin = GetBodyPosition() + _rotation.Transpose().Transform(arg->start_point);
        cockpitAimArg.direction = ypabact_GetCockpitAimDirection(this, origin, arg->direction, _rotation.AxisZ(), 1400.0);
        cockpitAimArg.tgt_pos = cockpitAimArg.direction;
        arg = &cockpitAimArg;
    }

    // OpenNeoUA custom: a laser weapon does not spawn projectiles and is not rate-limited
    // by shot_time. It registers a per-frame fire request that UpdateLaser() turns into
    // a continuous beam (static tick damage + VP beam visual + serialized snd_normal). Bail out here,
    // before the cooldown gate and the normal missile-spawn path.
    if ( cooldownProto.IsVerticalLaser() )
    {
        RequestVerticalLaserFire(cooldownWeapon, arg);
        return 0;
    }

    if ( cooldownProto.IsLaser() )
    {
        RequestLaserFire(cooldownWeapon, arg);
        return 0;
    }

    const bool salveCooldown =
        cooldownProto.salve_shots > 0 && cooldownProto.salve_delay > 0 &&
        cooldownProto.salve_shots <= _salve_counter;

    // Progressive cadence belongs only to the uninterrupted shot_time chain.
    // A vanilla structural pause (currently salve_delay) ends that chain, so
    // holding FIRE through the pause must not spool the next burst in advance.
    if ( salveCooldown )
        ResetProgressiveWeaponFireRate();
    else
        RegisterProgressiveWeaponFireRequest(cooldownWeapon);

    if ( _weapon_time )
    {
        int v4;

        if ( _oflags & BACT_OFLAG_USERINPT )
            v4 = cooldownProto.shot_time_user;
        else
            v4 = cooldownProto.shot_time;

        if ( salveCooldown )
        {
            // salve_delay is a separate burst-pattern cooldown. Entering it
            // resets Progressive Fire Rate to the authored base shot time.
            v4 = cooldownProto.salve_delay;
        }
        else
        {
            v4 = GetProgressiveWeaponShotTime(cooldownProto, v4);
            v4 = GetEffectiveShotTime(v4, false);
        }

        if ( arg->g_time - _weapon_time < v4 )
            return 0;
    }

    // The salve pause has just completed. Start a fresh ramp sequence from the
    // base cadence on the first shot of the new burst, never during the delay.
    if ( salveCooldown )
        RegisterProgressiveWeaponFireRequest(cooldownWeapon);

    if ( usePlayerRandomSlots )
    {
        int randomSlot = rand() % slotCount;
        selectedWeapon = slots[randomSlot];
        selectedWeaponSourceSlot = sourceSlots[randomSlot];
    }

    if ( selectedWeapon == -1 || !ypabact_IsValidWeaponId(this, selectedWeapon) )
    {
        ResetProgressiveWeaponFireRate();
        return 0;
    }

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(selectedWeapon);
    if ( wproto.IsKamikaze() )
    {
        ResetProgressiveWeaponFireRate();
        return 0;
    }

    const bool usePlayerLaunchShake = ypabact_ShouldUsePlayerLaunchShake(this, wproto);
    bool weaponEnergyCostApplied = false;

    // OpenNeoUA custom: artillery shell weapons are driven exclusively by UpdateArtilleryShell()'s
    // barrage AI. Never fire them through the normal direct/missile path.
    if ( wproto.IsArtilleryShell() )
    {
        ResetProgressiveWeaponFireRate();
        return 0;
    }

    // OpenNeoUA invisible: committed to firing the primary weapon this frame -> reveal now
    // (before the projectile spawns) so a cloaked unit never launches an invisible shot.
    RevealInvisibleOnAttack();

    if ( _salve_counter < wproto.salve_shots )
        _salve_counter += 1;
    else
        _salve_counter = 1;

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        yw_arg180 v26;

        if ( wproto._weaponFlags & 2 )
            v26.effects_type = 0;
        else if ( wproto._weaponFlags & 0x10 )
            v26.effects_type = 1;
        else
            v26.effects_type = 2;

        _world->ypaworld_func180(&v26);
    }

    // Resolve Random Num Weapons exactly once per successful firing event. All
    // later salvo logic (spawn spacing, multi-target distribution, recoil
    // and tracer count) consumes this same rolled value.
    int v13 = ypabact_RollWeaponProjectileCountForSourceSlot(
        this, selectedWeaponSourceSlot);

    const int soundEventProjectileLimit =
        _num_weapons_snd_events > 0
            ? std::min((int)_num_weapons_snd_events, v13)
            : v13;
    int soundEnabledProjectiles = 0;

    // Cockpit aiming preserves the vanilla base direction for a multi-projectile salvo.
    // fire_x still distributes the physical spawn points left/right, but it must
    // not steer the whole salvo from the legacy alternating signed muzzle offset
    // or make each projectile converge independently on the same aim point.
    if ( cockpitDirectAim && v13 > 1 )
    {
        vec3d commonOrigin = GetBodyPosition() + _rotation.Transpose().Transform(
            vec3d(0.0, cockpitAimArg.start_point.y, cockpitAimArg.start_point.z));
        cockpitAimArg.direction = ypabact_GetCockpitAimDirection(
            this, commonOrigin, cockpitRequestedViewDir, _rotation.AxisZ(), 1400.0);
        cockpitAimArg.tgt_pos = cockpitAimArg.direction;
        arg = &cockpitAimArg;
    }

    bool homingBomb = ypabact_IsHomingBombWeapon(wproto);
    bool missileTargeting = ypabact_UsesMissileTargeting(wproto);
    int maxTargets = ypabact_GetMultiTargetLimit(wproto, v13);
    bool missileMultiTarget = maxTargets > 1 && missileTargeting;
    bool bombMultiTarget = maxTargets > 1 && homingBomb;
    std::vector<NC_STACK_ypabact *> weaponTargets;

    if ( !(arg->flags & BACT_ARG79_FLAG_NO_AUTO_TARGETS) )
    {
        if ( missileMultiTarget )
            weaponTargets = ypabact_CollectMissileMultiTargets(this, arg, wproto, maxTargets);
        else if ( bombMultiTarget )
            weaponTargets = ypabact_CollectHomingBombTargets(this, arg, wproto, maxTargets);
        else if ( homingBomb )
            weaponTargets = ypabact_CollectHomingBombTargets(this, arg, wproto, 1);
    }

    // Multi-lock HUD and manual target cycling use the shared missile targeting
    // path. Homing bombs remain automatic and do not enter that HUD path.
    ypabact_StoreHUDMissileMultiLockTargets(
        this, missileMultiTarget ? weaponTargets : std::vector<NC_STACK_ypabact *>());

    vec3d recoilDirSum(0.0, 0.0, 0.0);
    int recoilShotCount = 0;

    const bool suicideControlledAtFire =
        _kill_after_shot && _world &&
        (_world->_userUnit == this || _world->_viewerBact == this);
    std::vector<int32_t> suicideReturnGids;
    int32_t suicideHostGid = 0;
    bool suicideViewerStarted = false;
    if ( suicideControlledAtFire )
    {
        for ( NC_STACK_ypabact *candidate : ypabact_GetSquadControlFallbacks(this) )
            suicideReturnGids.push_back(candidate->_gid);
        if ( _world->_userRobo )
            suicideHostGid = _world->_userRobo->_gid;
    }

    for (int i = 0; i < v13; i++)
    {
        float v37;
        bact_arg79 missileArg = *arg;
        if ( suicideControlledAtFire && i == 0 )
            missileArg.flags |= 1; // kill_after_shot always owns a Weapon Cam sequence.

        bool distributeTargets = missileMultiTarget || bombMultiTarget || homingBomb;
        NC_STACK_ypabact *multiTarget = distributeTargets ? ypabact_GetDistributedMissileTarget(weaponTargets, i) : nullptr;

        if ( v13 == 1 )
            v37 = arg->start_point.x;
        else
        {
            float v14 = fabs(arg->start_point.x);
            v37 = (i * 2) * v14 / (v13 - 1) - v14;
        }

        v37 = ypabact_GetProjectileFireX(this, v37, i, v13);

        ypaworld_arg146 arg147;
        arg147.vehicle_id = selectedWeapon;
        arg147.pos = GetBodyPosition() + _rotation.Transpose().Transform( vec3d(v37, arg->start_point.y, arg->start_point.z) );
        arg147.weapon_sound_events_enabled =
            soundEnabledProjectiles < soundEventProjectileLimit;

        if ( multiTarget )
        {
            missileArg.tgType = BACT_TGT_TYPE_UNIT;
            missileArg.target.pbact = multiTarget;
            missileArg.tgt_pos = multiTarget->_position;

            vec3d targetDir = multiTarget->_position - arg147.pos;
            float targetDirLen = targetDir.length();
            if ( targetDirLen > 0.001 )
                missileArg.direction = targetDir / targetDirLen;
        }
        else if ( missileMultiTarget && missileArg.tgType == BACT_TGT_TYPE_UNIT &&
                  !ypabact_IsValidMissileMultiTarget(this, missileArg.target.pbact) )
        {
            missileArg.tgType = BACT_TGT_TYPE_DRCT;
        }

        if ( v13 == 1 && missileArg.tgType == BACT_TGT_TYPE_DRCT && IsCockpitCameraActive() )
            missileArg.direction = ypabact_GetCockpitAimDirection(this, arg147.pos, missileArg.direction, _rotation.AxisZ(), 1400.0);

        vec3d aiArcDirection;
        const bool useAiArcDirection =
            wproto.IsArcGrenade() &&
            (arg->flags & BACT_ARG79_FLAG_AI_BALLISTIC_AIM);
        if ( useAiArcDirection )
        {
            vec3d ballisticTarget;
            bool hasBallisticTarget = false;

            if ( missileArg.tgType == BACT_TGT_TYPE_UNIT && missileArg.target.pbact )
            {
                ballisticTarget = missileArg.target.pbact->_position;
                hasBallisticTarget = true;
            }
            else if ( missileArg.tgType == BACT_TGT_TYPE_CELL )
            {
                ballisticTarget = missileArg.tgt_pos;
                hasBallisticTarget = true;
            }

            if ( !hasBallisticTarget ||
                 !ypabact_TrySolveArcGrenadeDirection(
                     arg147.pos, ballisticTarget, wproto, &aiArcDirection) )
            {
                // Do not manufacture speed/range for an unreachable target.
                // A later projectile in a multi-target salvo may be outside the
                // ballistic envelope even though the primary AI target passed
                // CheckFireAI(), so end the salvo without spawning that shot.
                if ( i == 0 )
                {
                    ResetProgressiveWeaponFireRate();
                    return 0;
                }
                break;
            }
        }

        wobj = _world->ypaworld_func147(&arg147);

        if ( !wobj )
        {
            ResetProgressiveWeaponFireRate();
            return 0;
        }

        if ( wobj->WeaponSoundEventsEnabled() )
            soundEnabledProjectiles++;

        if ( i == 0 )
            ypabact_StartVehicleFireVPIfEnabled(this, arg->g_time);

        ypabact_ConsumeProjectileFireX(this);

        wobj->SetLauncherBact(this);

        wobj->SetStartHeight(arg147.pos.y);

        wobj->_owner = _owner;

        if ( wproto.weapon_energy_cost_defined )
        {
            // Per-Weapon authored cost: charge once after the first projectile
            // of this firing event was successfully created. num_weapons, random
            // projectile count, multi-target distribution and extra weapon slots
            // never multiply the authored cost.
            if ( !weaponEnergyCostApplied )
            {
                _energy -= ypabact_GetAuthoredPercentEnergyCost(
                    _energy_max, wproto.weapon_energy_cost);
                weaponEnergyCostApplied = true;
            }
        }
        else if ( _bact_type != BACT_TYPES_GUN )
        {
            // Missing/invalid weapon_energy_cost preserves the exact vanilla
            // projectile rule: each successfully created projectile costs energy/300.
            _energy -= wobj->_energy / 300;
        }

        if ( missileArg.direction.x != 0.0 || missileArg.direction.y != 0.0 || missileArg.direction.z != 0.0 )
        {
            wobj->_fly_dir = missileArg.direction;
        }
        else
        {
            wobj->_fly_dir = _rotation.AxisZ();
        }

        if ( wproto.IsArcGrenade() )
        {
            if ( useAiArcDirection )
            {
                // The solver supplies the unmodified ballistic intercept.
                // Pattern/cone/spread below intentionally perturb that base
                // vector just like they do for every other projectile Weapon.
                wobj->_fly_dir = aiArcDirection;
            }
            else
            {
                // Player/manual fire first establishes the Arc Grenade elevation.
                // The common directional modifiers below then apply their
                // authored horizontal and vertical offsets (weapon_spread_xyz).
                vec3d groundAimDir;
                if ( getBACT_inputting() &&
                     ypabact_ComputeArcGrenadeGroundAim(this, wproto, arg147.pos,
                                                        wobj->_fly_dir, &groundAimDir) )
                {
                    // Player aim: the shell lands where the crosshair meets the
                    // terrain, so aiming up/down changes the range (mortar-style).
                    wobj->_fly_dir = groundAimDir;
                }
                else
                {
                    wobj->SetupArcGrenadeLaunch(wproto.grenade_arc_angle,
                                                wproto.grenade_arc_gravity,
                                                wproto.start_speed);
                }
            }
        }

        wobj->_fly_dir = ypabact_ApplyWeaponDirectionPattern(_rotation, wobj->_fly_dir,
                                                               i, v13,
                                                               _weapon_arc_x, _weapon_arc_y,
                                                               _weapon_cone_xy);

        // Hand Brake affects only the random component. Arc and cone were already
        // applied above and intentionally keep their authored pattern unchanged.
        const float handBrakeSpreadScale = ypabact_GetHandBrakeRandomSpreadScale(this);
        float spreadX = _weapon_spread_x;
        float spreadY = _weapon_spread_y;
        if ( _progressive_weapon_id == selectedWeapon )
        {
            const float level = std::max(0.0f, std::min(_progressive_weapon_level, 1.0f));
            if ( wproto.ramp_up_max_weapon_spread_x > 0.0f &&
                 spreadX > wproto.ramp_up_max_weapon_spread_x )
                spreadX += (wproto.ramp_up_max_weapon_spread_x - spreadX) * level;
            if ( wproto.ramp_up_max_weapon_spread_y > 0.0f &&
                 spreadY > wproto.ramp_up_max_weapon_spread_y )
                spreadY += (wproto.ramp_up_max_weapon_spread_y - spreadY) * level;
        }
        const float effectiveSpreadX = spreadX * handBrakeSpreadScale;
        const float effectiveSpreadY = spreadY * handBrakeSpreadScale;
        // Bombs ignore the aim-based X/Y rotation: their spread is the ground-plane
        // push (X and Z of weapon_spread_xyz) applied with the launch speed below.
        const bool bombSpread = wproto.IsBombLike();
        if ( !bombSpread && (effectiveSpreadX > 0.0f || effectiveSpreadY > 0.0f) )
            wobj->_fly_dir = World::ApplyDirectionalSpread(_rotation, wobj->_fly_dir,
                                                           effectiveSpreadX, effectiveSpreadY);

        if ( wproto.IsArcGrenade() )
        {
            // SetupArcGrenadeLaunch()/the AI solver establish the base vector
            // above. Commit the direction only after the shared pattern and
            // X/Y path of weapon_spread_xyz has run, otherwise Arc
            // Grenade would overwrite the vertical spread (and all AI spread).
            wobj->SetupArcGrenadeVelocity(wobj->_fly_dir * wproto.start_speed,
                                          wproto.grenade_arc_gravity);
        }
        else
        {
            wobj->_fly_dir_length = _fly_dir_length + wproto.start_speed;

            if ( !(wproto._weaponFlags & 0x12) )
                wobj->_fly_dir_length *= 0.2;

            const float effectiveSpreadZ = _weapon_spread_z * handBrakeSpreadScale;
            if ( bombSpread && (effectiveSpreadX > 0.0f || effectiveSpreadZ > 0.0f) )
            {
                // A bomb's launch speed is tiny (or zero), so rotating it barely
                // moves the bomb. Add a fixed-speed push on the ground plane instead.
                const vec3d bombVelocity = World::ApplyBombSpreadKick(
                    wobj->_fly_dir * wobj->_fly_dir_length, _rotation,
                    effectiveSpreadX, effectiveSpreadZ);
                const float bombSpeed = bombVelocity.length();
                if ( bombSpeed > 0.001f )
                {
                    wobj->_fly_dir = bombVelocity / bombSpeed;
                    wobj->_fly_dir_length = bombSpeed;
                }
            }

            wobj->_rotation.SetZ( wobj->_fly_dir );
            wobj->_rotation.SetX( _rotation.AxisX() );
            wobj->_rotation.SetY( wobj->_rotation.AxisZ() * wobj->_rotation.AxisX() );
        }

        if ( wproto.recoil > 0.0f )
        {
            recoilDirSum -= wobj->_fly_dir;
            recoilShotCount++;
        }

        if ( i == 0 )
        {
            if ( missileArg.flags & 1 )
                wobj->_position = wobj->_position - wobj->_rotation.AxisZ() * 30.0;
        }

        // One physical projectile means one visual tracer. This runs after the
        // final launch point and orientation are known, so num_weapons naturally
        // produces the same number of correctly positioned tracers.
        wobj->StartWeaponTracer();

        // A launch flash belongs to the vehicle fire event, not each projectile.
        // The first successful projectile supplies the final muzzle transform.
        if ( i == 0 )
            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_LAUNCH, nullptr,
                                  &wproto.chain_fx, &wobj->_position, &wobj->_rotation);

        /** Missiles will be stored in another list
         *  so kidref will be not attached to anything.
         *  Looks it's somehow related to mentioned problem with dead cache.
        **/

        wobj->_kidRef.Detach();
        wobj->_parent = nullptr;

        _missiles_list.push_back(wobj);

        int v42 = wobj->GetMissileType();
        if ( v42 == NC_STACK_ypamissile::MISL_TARGETED || homingBomb )
        {
            setTarget_msg arg67;

            arg67.tgt = missileArg.target;
            arg67.tgt_type = missileArg.tgType;
            arg67.priority = 0;
            arg67.tgt_pos = missileArg.tgt_pos;

            wobj->SetTarget(&arg67);

            if ( missileArg.flags & 2 )
            {
                if ( missileArg.tgType == BACT_TGT_TYPE_CELL )
                    wobj->_primTpos.y = missileArg.tgt_pos.y;
            }
        }

        uamessage_newWeapon wpnMsg;
        wpnMsg.targetPos = missileArg.tgt_pos;

        if ( v42 == 2 )
        {
            wobj->_primTtype = BACT_TGT_TYPE_DRCT;
            wobj->_target_dir = wobj->_fly_dir;
        }

        wobj->_host_station = _host_station;
        _weapon_time = arg->g_time;

        if ( usePlayerLaunchShake &&
             wobj->_soundcarrier.Sounds.size() > World::TWeapProto::SND_LAUNCH )
        {
            // The player-specific event replaces only the generic launch shake on
            // this local projectile. Launch sound and palette FX remain untouched.
            TSoundSource &launchSource =
                wobj->_soundcarrier.Sounds[World::TWeapProto::SND_LAUNCH];
            launchSource.PShkFx = nullptr;
            launchSource.SetShk(false);
            launchSource.SetShkEnable(false);
            launchSource.SetShkPlay(false);
        }

        if ( wproto.snd_launch_at_muzzle &&
             wobj->_soundcarrier.Sounds.size() > World::TWeapProto::SND_LAUNCH )
        {
            // Launch sound comes from the shooter, not from the fast projectile that would
            // Doppler-shift and pan it. Silent projectiles (snd event limit) have no PSample.
            ypabact_PlayMuzzleSound(this, wobj->_soundcarrier.Sounds[World::TWeapProto::SND_LAUNCH]);
        }
        else
            SFXEngine::SFXe.startSound(&wobj->_soundcarrier, World::TWeapProto::SND_LAUNCH);

        if ( _world->_isNetGame )
        {
            wobj->_gid |= _owner << 24;

            wpnMsg.msgID = UAMSG_NEWWEAPON;
            wpnMsg.owner = _owner;
            wpnMsg.id = wobj->_gid;
            wpnMsg.launcher = _gid;
            wpnMsg.type = selectedWeapon;
            wpnMsg.pos = arg147.pos;
            wpnMsg.flags = wobj->WeaponSoundEventsEnabled()
                ? 0
                : UAMSG_NEWWEAPON_FLAG_SILENT_SOUND_EVENTS;
            wpnMsg.dir = wobj->_fly_dir * wobj->_fly_dir_length;
            wpnMsg.targetType = wobj->_primTtype;

            if ( wobj->_primTtype == BACT_TGT_TYPE_UNIT )
            {
                wpnMsg.target = wobj->_primT.pbact->_gid;
                wpnMsg.targetOwner = wobj->_primT.pbact->_owner;
            }

            _world->NetBroadcastMessage(&wpnMsg, sizeof(wpnMsg), true);
        }

        if ( missileArg.flags & 1 )
        {
            if ( i == 0 )
            {
                if ( _oflags & BACT_OFLAG_VIEWER )
                {
                    if ( suicideControlledAtFire )
                    {
                        wobj->ConfigureSuicideViewerReturn(suicideReturnGids, suicideHostGid);
                        suicideViewerStarted = true;
                    }
                    setBACT_viewer(false);
                    wobj->setBACT_viewer(true);
                }
            }
        }

        if ( missileArg.flags & 4 )
            wobj->SetIgnoreBuilds(1);


        if ( missileArg.tgType != BACT_TGT_TYPE_UNIT )
        {
            int life_time_nt = wproto.life_time_nt;

            if ( life_time_nt )
                wobj->SetLifeTime(life_time_nt);
        }
    }

    if ( _fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE )
        _fire_x_slot_index = (_fire_x_slot_index + 1) & 1;

    if ( wproto.recoil > 0.0f && recoilShotCount > 0 )
    {
        float recoilAmount = wproto.recoil * (float)recoilShotCount;

        // OpenNeoUA custom: game.handbrake_power controls both braking strength
        // and recoil reduction.  The recoil part is applied only when the
        // input path marks an explicit brake press, not joystick auto-brake.
        // AI/third-person behavior stays independent from push_resistance.
        if ( arg->flags & BACT_ARG79_FLAG_RECOIL_BRAKE_HELD )
            recoilAmount *= 1.0f - ypabact_ReadHandBrakeRecoilReduction();

        if ( recoilAmount > 0.0f )
            ApplyRecoil(recoilDirSum, recoilAmount);
    }

    ypabact_TriggerPlayerLaunchShake(this, wproto);

    if ( _kill_after_shot )
    {
        if ( suicideControlledAtFire )
        {
            if ( suicideViewerStarted )
            {
                // The missile is already the viewer/_userUnit. Drop only the
                // dying launcher's stale input flag; ResetViewing() performs the
                // same-squad handoff after the camera sequence actually ends.
                setBACT_inputting(false);
            }
            else
            {
                // A controlled kill_after_shot normally forces the first projectile
                // into Weapon Cam above. Keep the existing immediate fallback only
                // for unusual cases where no projectile viewer could take ownership.
                PrepareSuicideControlHandoff();
            }
        }

        bact_arg84 arg84;
        arg84.unit = _parent;
        arg84.energy = -2 * _energy_max;

        ModifyEnergy(&arg84);
    }

    ypabact_AdvancePrimaryWeaponSlot(this, arg->weapon);

    const bool keepResolvedSlot =
        (_oflags & BACT_OFLAG_USERINPT)
            ? _weapon_player_switch_mode == World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_RANDOM
            : (_weapon_ai_switch_mode == World::TVhclProto::WEAPON_AI_SWITCH_MODE_RANDOM ||
               _weapon_ai_switch_mode == World::TVhclProto::WEAPON_AI_SWITCH_MODE_SMART);

    if ( keepResolvedSlot )
    {
        _current_weapon_id = selectedWeapon;
        _current_weapon_source_slot = selectedWeaponSourceSlot;
    }
    else
    {
        _current_weapon_source_slot = ypabact_GetCurrentPrimaryWeaponSourceSlot(this);
        _current_weapon_id = GetCurrentWeaponId();
    }

    if ( _progressive_weapon_id >= 0 && _current_weapon_id != _progressive_weapon_id )
        ResetProgressiveWeaponFireRate();

    return 1;
}

vec3d ypabact_ApplyWeaponDirectionPattern(const mat3x3 &rotation, const vec3d &direction,
                                                   int shotIndex, int weaponCount,
                                                   float arcX, float arcY, float coneXY)
{
    if ( weaponCount <= 1 || shotIndex < 0 || shotIndex >= weaponCount )
        return direction;

    const bool arcXActive = std::isfinite(arcX) && arcX > 0.0f;
    const bool arcYActive = std::isfinite(arcY) && arcY > 0.0f;
    const bool coneActive = std::isfinite(coneXY) && coneXY > 0.0f;

    if ( (!(arcXActive || arcYActive) && !coneActive) ||
         ((arcXActive || arcYActive) && coneActive) )
        return direction;

    if ( arcXActive && arcYActive && weaponCount % 4 != 0 && weaponCount % 4 != 1 )
        return direction;

    vec3d forward = direction;
    if ( forward.normalise() <= 0.001 )
        return direction;

    vec3d right = rotation.AxisX();
    right -= forward * right.dot(forward);

    if ( right.normalise() <= 0.001 )
    {
        vec3d refAxis = fabs(forward.y) < 0.99 ? vec3d::OY(1.0) : vec3d::OX(1.0);
        right = refAxis * forward;
    }

    if ( right.normalise() <= 0.001 )
        return forward;

    vec3d up = forward * right;
    if ( up.normalise() <= 0.001 )
        return forward;

    if ( coneActive )
    {
        const double opening = (double)coneXY * C_PI_180;
        const double around = C_2PI * (double)shotIndex / (double)weaponCount;
        vec3d result = forward * cos(opening) +
                       right * (sin(opening) * cos(around)) +
                       up * (sin(opening) * sin(around));

        if ( result.normalise() > 0.001 )
            return result;

        return direction;
    }

    float horizontalAngle = 0.0f;
    float verticalAngle = 0.0f;

    if ( arcXActive && arcYActive )
    {
        const bool hasCenter = weaponCount % 4 == 1;
        if ( hasCenter && shotIndex == 0 )
            return forward;

        const int armShots = (weaponCount - (hasCenter ? 1 : 0)) / 4;
        const int patternIndex = shotIndex - (hasCenter ? 1 : 0);
        const int arm = patternIndex % 4;
        const int level = patternIndex / 4 + 1;
        const float fraction = (float)level / (float)armShots;

        switch ( arm )
        {
        case 0:
            horizontalAngle = -arcX * fraction;
            break;
        case 1:
            horizontalAngle = arcX * fraction;
            break;
        case 2:
            verticalAngle = -arcY * fraction;
            break;
        default:
            verticalAngle = arcY * fraction;
            break;
        }
    }
    else
    {
        const float fraction = (float)shotIndex / (float)(weaponCount - 1);
        if ( arcXActive )
            horizontalAngle = -arcX + 2.0f * arcX * fraction;
        else
            verticalAngle = -arcY + 2.0f * arcY * fraction;
    }

    const double horizontal = (double)horizontalAngle * C_PI_180;
    const double vertical = (double)verticalAngle * C_PI_180;
    vec3d result = forward * (cos(horizontal) * cos(vertical)) +
                   right * (sin(horizontal) * cos(vertical)) +
                   up * sin(vertical);

    if ( result.normalise() > 0.001 )
        return result;

    return direction;
}

// Plays a one-shot copy of src from the shooter's fixed sound pool (no projectile Doppler/pan drift).
void ypabact_PlayMuzzleSound(NC_STACK_ypabact *bact, const TSoundSource &src)
{
    static const size_t MGUN_PULSE_SOUND_SLOTS = 8;

    if ( !bact || !src.PSample )
        return;

    if ( bact->_mgun_soundcarrier.Sounds.empty() )
    {
        bact->_mgun_soundcarrier.Resize(MGUN_PULSE_SOUND_SLOTS);
        bact->_mgun_sound_index = 0;
    }

    size_t soundCount = bact->_mgun_soundcarrier.Sounds.size();
    size_t soundIndex = (size_t)bact->_mgun_sound_index % soundCount;

    for (size_t i = 0; i < soundCount; i++)
    {
        size_t candidate = (soundIndex + i) % soundCount;
        if ( !bact->_mgun_soundcarrier.Sounds[candidate].IsEnabled() )
        {
            soundIndex = candidate;
            break;
        }
    }

    TSoundSource &snd = bact->_mgun_soundcarrier.Sounds[soundIndex];
    snd.PSample = src.PSample;
    snd.PFragments = nullptr;
    snd.PPFx = nullptr;
    snd.PShkFx = nullptr;
    snd.Volume = src.Volume;
    snd.CopyPitchConfig(src);
    snd.PriorityBias = src.PriorityBias;
    snd.SetLoop(false);
    snd.SetFragmented(false);
    snd.SetPFx(false);
    snd.SetPFxEnable(false);
    snd.SetShk(false);
    snd.SetShkEnable(false);

    bact->_mgun_soundcarrier.Position = bact->_soundcarrier.Position;
    bact->_mgun_soundcarrier.Vector = bact->_soundcarrier.Vector;
    SFXEngine::SFXe.startSound(&bact->_mgun_soundcarrier, soundIndex);
    SFXEngine::SFXe.UpdateSoundCarrier(&bact->_mgun_soundcarrier);
    bact->_mgun_sound_index = (int)((soundIndex + 1) % soundCount);
}

void ypabact_PlayVehicleMinigunPulse(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_soundcarrier.Sounds.size() <= World::TVhclProto::SND_FIRE )
        return;

    ypabact_PlayMuzzleSound(bact, bact->_soundcarrier.Sounds[World::TVhclProto::SND_FIRE]);
}

vec3d ypabact_GetMinigunFireDir(NC_STACK_ypabact *bact, const vec3d &requestedDir)
{
    if ( !bact || !bact->_mgun_angle_set )
        return requestedDir;

    vec3d dir = bact->_rotation.AxisZ() - bact->_rotation.AxisY() * bact->GetMinigunAngle();

    if ( dir.normalise() > 0.001 )
        return dir;

    return requestedDir;
}

bool ypabact_SpawnVehicleMinigunImpact(NC_STACK_ypabact *bact, const vec3d &pos, const vec3d &dir, bool worldHit)
{
    if ( !bact || !bact->getBACT_pWorld() )
        return false;

    const int preferredVP = worldHit ? bact->_mgun_vp_megadeth : bact->_mgun_vp_dead;
    const int fallbackVP = worldHit ? bact->_mgun_vp_dead : bact->_mgun_vp_megadeth;
    const std::string &preferred3DS = worldHit ? bact->_mgun_3ds_megadeth : bact->_mgun_3ds_dead;
    const std::string &fallback3DS = worldHit ? bact->_mgun_3ds_dead : bact->_mgun_3ds_megadeth;
    const std::string &preferredBase = worldHit ? bact->_mgun_base_megadeth : bact->_mgun_base_dead;
    const std::string &fallbackBase = worldHit ? bact->_mgun_base_dead : bact->_mgun_base_megadeth;

    int vp = preferredVP > 0 ? preferredVP : fallbackVP;
    const std::string &mesh3ds = !preferred3DS.empty() ? preferred3DS : fallback3DS;
    const std::string &basePath = !preferredBase.empty() ? preferredBase : fallbackBase;
    if ( vp <= 0 && mesh3ds.empty() && basePath.empty() )
        return false;

    bact->getBACT_pWorld()->SpawnTransientVisual(
        vp, mesh3ds, basePath, pos,
        ypabact_LaserRotationFromDir(dir, bact->_rotation), 90);
    return true;
}

bool ypabact_GetMinigunSpreadImpactPoint(const vec3d &origin, const vec3d &dir,
                                                const vec3d &center, float radius,
                                                vec3d *outPos, float *outDistance)
{
    if ( radius <= 0.0f )
        return false;

    vec3d rayDir = dir;
    if ( rayDir.normalise() <= 0.001f )
        return false;

    vec3d toCenter = center - origin;
    float along = toCenter.dot(rayDir);

    if ( along < 0.0f )
        return false;

    vec3d closest = origin + rayDir * along;
    vec3d offset = center - closest;
    float visualRadius = radius * 0.7f;
    float impactDistance = along;

    if ( visualRadius > 0.0f )
    {
        float offsetSq = offset.dot(offset);
        float radiusSq = visualRadius * visualRadius;

        if ( offsetSq < radiusSq )
        {
            impactDistance = along - sqrt(radiusSq - offsetSq);
            if ( impactDistance < 0.0f )
                impactDistance = along;
        }
    }

    if ( outPos )
        *outPos = origin + rayDir * impactDistance;

    if ( outDistance )
        *outDistance = impactDistance;

    return true;
}

bool ypabact_GetRaySphereEntryDistance(const vec3d &origin, const vec3d &dir,
                                               const vec3d &center, float radius,
                                               float *outDistance)
{
    if ( radius <= 0.0f )
        return false;

    vec3d rayDir = dir;
    if ( rayDir.normalise() <= 0.001f )
        return false;

    vec3d toCenter = center - origin;
    float along = toCenter.dot(rayDir);
    if ( along < 0.0f )
        return false;

    float perpendicularSq = toCenter.dot(toCenter) - along * along;
    float radiusSq = radius * radius;
    if ( perpendicularSq > radiusSq )
        return false;

    float entryDistance = along - sqrt(std::max(0.0f, radiusSq - perpendicularSq));
    if ( entryDistance < 0.0f )
        entryDistance = 0.0f;

    if ( outDistance )
        *outDistance = entryDistance;

    return true;
}

size_t NC_STACK_ypabact::FireMinigun(bact_arg105 *arg)
{
    if ( IsActiveDebuffStunFireBlocked() )
        return 0;

    if ( _world && _world->IsSpectatorBact(this) )
        return 0;

    int a5 = 0;

    if ( _world->_isNetGame )
        a5 = 1;

    if ( !HasMinigun() )
        return 0;

    World::TWeapProto *mgunProto = _mgun != -1 ? &_world->GetWeaponsProtos().at(_mgun) : nullptr;
    bool vehicleTimedMgun = UsesVehicleMinigunTiming();
    // With mgun_energy the MGUN fires discrete shots: one hit scan and one
    // energy hit per mgun_shot_time. Without it the vanilla MGUN keeps its
    // continuous gun_power damage on every frame.
    const bool shotEnergyMode = _mgun_energy_set;
    const float mgunDamage = shotEnergyMode ? _mgun_energy : _gun_power * arg->field_C;
    RevealInvisibleOnAttack();

    int v107 = 0;
    if ( _bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( this );
        if ( gun )
            v107 = gun->IsRoboGun() ? 1 : 0;
    }

    int v88 = getBACT_inputting();
    const bool emitLocalMgunPulse =
        (v88 || _world->ypaworld_func145(this)) && !a5;
    const int frameDeltaMs = (int)(arg->field_C * 1000.0);
    int mgunPulseInterval = 0;

    // Preserve the vanilla path exactly when the authored MGUN cost is absent
    // and no local pulse timing is needed. The new parameter reuses the same
    // effective cadence as the existing MGUN pulse timing.
    if ( _mgun_fire_energy_cost_defined || emitLocalMgunPulse || shotEnergyMode )
    {
        if ( vehicleTimedMgun )
        {
            mgunPulseInterval = GetMinigunShotTime(frameDeltaMs);
        }
        else
        {
            const int configuredShotTime = v88
                ? mgunProto->shot_time_user
                : mgunProto->shot_time;
            mgunPulseInterval = std::max(frameDeltaMs,
                                         GetEffectiveShotTime(configuredShotTime, true));
        }
    }

    if ( _mgun_fire_energy_cost_defined )
    {
        // Authored Vehicle-level MGUN cost: one percentage charge for each
        // effective MGUN pulse (the same cadence used by MGUN shot timing),
        // never per frame, per second or per simultaneous MGUN barrel/ray.
        if ( arg->field_10 - _mgun_energy_cost_time > mgunPulseInterval )
        {
            _mgun_energy_cost_time = arg->field_10;
            _energy -= ypabact_GetAuthoredPercentEnergyCost(
                _energy_max, _mgun_fire_energy_cost);
        }
    }
    else if ( !shotEnergyMode && _bact_type != BACT_TYPES_GUN )
    {
        // Missing/invalid mgun_fire_energy_cost preserves the exact vanilla
        // continuous Vehicle MGUN drain. Vanilla mounted gun units remain exempt.
        _energy -= mgunDamage / 300.0f;
    }

    bool spawnVisual = false;
    bool shotDue = !shotEnergyMode;

    if ( emitLocalMgunPulse || shotEnergyMode )
    {
        if ( arg->field_10 - _mgun_time > mgunPulseInterval )
        {
            _mgun_time = arg->field_10;
            shotDue = true;
            spawnVisual = emitLocalMgunPulse;

            // Keep the Vehicle FIRE visual state on the exact same effective
            // cadence that emitted this real MGUN pulse. This prevents damaged
            // shot-time malus from slowing gameplay while the FIRE VP runs at
            // the old undamaged request rate.
            if ( emitLocalMgunPulse && vehicleTimedMgun )
                ypabact_StartVehicleFireVP(this, arg->field_10);

            // weapon_use_vehicle_fire: an attached MGUN gun pulse also shows its carrier's fire visual.
            if ( emitLocalMgunPulse )
                ypabact_StartVehicleFireVPIfEnabled(this, arg->field_10);

            // One cockpit shake per shot, so the shake follows the fire rate.
            if ( emitLocalMgunPulse )
                ypabact_TriggerPlayerMgunRecoilShake(this);

            if ( emitLocalMgunPulse && vehicleTimedMgun )
                ypabact_PlayVehicleMinigunPulse(this);
        }
    }

    // In shot mode the hit scan, the energy hit and the default energy drain
    // all happen only when a shot is due.
    if ( !shotDue )
        return 1;

    if ( shotEnergyMode && !_mgun_fire_energy_cost_defined && _bact_type != BACT_TYPES_GUN )
        _energy -= mgunDamage / 300.0f;

    // One spread ray per shot; the scope keeps the ray variables local.
    {
        bool cockpitAim = IsCockpitCameraActive();
        vec3d shotPos = cockpitAim ? GetCockpitCameraPosition() : GetBodyPosition();
        vec3d shotOldPos = cockpitAim ? shotPos : _old_pos + (GetBodyPosition() - _position);
        float spreadX = _mgun_spread_x;
        float spreadY = _mgun_spread_y;
        const float handBrakeSpreadScale = ypabact_GetHandBrakeRandomSpreadScale(this);
        spreadX *= handBrakeSpreadScale;
        spreadY *= handBrakeSpreadScale;

        vec3d fireDir = ypabact_GetMinigunFireDir(this, arg->field_0);
        if ( cockpitAim )
            fireDir = ypabact_GetCockpitViewDirection(this, fireDir);
        vec3d shotDir = World::ApplyDirectionalSpread(_rotation, fireDir, spreadX, spreadY);
        float minigunTraceRange = ypabact_GetMinigunRange();

        // Resolve the nearest world obstruction before applying unit damage.
        // The legacy order damaged every intersected unit first and traced the
        // world only when no unit was found, so MGUN hits could pass through a
        // power station, wall or terrain ridge. ypaworld_func149 is the
        // existing full stepped terrain/building trace used by projectiles.
        ypaworld_arg136 v59;
        v59.stPos = shotPos;
        v59.vect = shotDir * minigunTraceRange;
        v59.flags = 0;

        _world->ypaworld_func149(&v59);

        vec3d v80;
        bool minigunWorldHit = v59.isect;
        float minigunWorldHitDistance = minigunTraceRange + 1.0f;
        if ( minigunWorldHit )
        {
            v80 = v59.isectPos;
            minigunWorldHitDistance = std::max(0.0f, v59.tVal * minigunTraceRange);
        }

        NC_STACK_ypabact *v108 = nullptr;
        float v123 = 0.0;
        float v121 = 0.0;
        vec3d v66;
        bool minigunSpreadImpactPoint = false;

        yw_130arg arg130;
        arg130.pos_x = shotPos.x;
        arg130.pos_z = shotPos.z;

        vec2d tmp = shotPos.XZ() + shotDir.XZ() * minigunTraceRange;

        if ( !_world->GetSectorInfo(&arg130) )
            return 1;

        arg130.pos_x = tmp.x;
        arg130.pos_z = tmp.y;

        if ( !_world->GetSectorInfo(&arg130) )
            return 1;

        // Longer configurable traces may cross more than the vanilla
        // start/middle/end sample. Include every sector in the segment's
        // bounding rectangle, then retain the existing exact ray/body test.
        Common::Point startCellId = World::PositionToSectorID(shotPos);
        Common::Point endCellId = World::PositionToSectorID(tmp);
        int minCellX = std::min(startCellId.x, endCellId.x);
        int maxCellX = std::max(startCellId.x, endCellId.x);
        int minCellY = std::min(startCellId.y, endCellId.y);
        int maxCellY = std::max(startCellId.y, endCellId.y);
        std::vector<cellArea *> pCells;

        for (int cellY = minCellY; cellY <= maxCellY; cellY++)
        {
            for (int cellX = minCellX; cellX <= maxCellX; cellX++)
            {
                Common::Point cellId(cellX, cellY);
                if ( _world->IsSector(cellId) )
                    pCells.push_back(&_world->SectorAt(cellId));
            }
        }

        std::vector<NC_STACK_ypabact *> minigunTargets;
        for (auto *cell : pCells)
            for (auto *actor : cell->unitsList) minigunTargets.push_back(actor);
        if (_world->_collisionScene)
            for (auto *actor : _world->_collisionScene->ShapeTargets(shotPos, shotPos + shotDir * minigunTraceRange, std::max(0.0f, _gun_radius)))
                if (std::find(minigunTargets.begin(), minigunTargets.end(), actor) == minigunTargets.end())
                    minigunTargets.push_back(actor);
        for (NC_STACK_ypabact *cellUnit : minigunTargets)
        {
            if (!cellUnit || cellUnit->CollisionsDisabled()) continue;
            // A mounted Unit Gun's hit scan never touches its carrier or sibling guns.
            if (IsMountedFamilyWith(cellUnit)) continue;
            if ( cellUnit != this && cellUnit->_bact_type != BACT_TYPES_MISSLE && cellUnit->_status != BACT_STATUS_DEAD )
            {
                int v89 = 0;
                if (cellUnit->_bact_type == BACT_TYPES_GUN)
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( cellUnit );
                    v89 = gun->IsRoboGun();
                }

                if ( cellUnit->_bact_type != BACT_TYPES_GUN || !v89 || cellUnit->GetEffectiveShield() < 100.0f )
                {
                    if ( (_oflags & BACT_OFLAG_USERINPT || cellUnit->_owner != _owner) && (!v107 || cellUnit != _host_station) )
                    {

                        World::rbcolls *v93 = cellUnit->getBACT_collNodes();
                        const bool targetManualCompound = cellUnit->HasManualCompoundCollision();
                        const int targetLegacySlots =
                            targetManualCompound && cellUnit->UsesLegacyRadiusCollision() ? 1 : 0;
                        const mat3x3 targetRotationT =
                            v93 ? cellUnit->_rotation.Transpose() : mat3x3::Ident();

                        int v109;
                        if ( v93 )
                            v109 = targetLegacySlots + v93->roboColls.size();
                        else
                            v109 = 1;

                        Collision::Contact shapeHit;
                        const bool usesShape = cellUnit->HasCollisionShape();
                        if (usesShape)
                        {
                            if (!_world->_collisionScene->Trace(cellUnit, shotPos,
                                  shotPos + shotDir * minigunTraceRange, std::max(0.0f, _gun_radius), &shapeHit)) continue;
                            v109 = 1;
                        }
                        int v22 = 0;

                        for (int j = v109 - 1; j >= 0; j-- )
                        {
                            vec3d v77;
                            float v27;

                            if (usesShape)
                            { v77 = shotPos + shotDir * (shapeHit.fraction * minigunTraceRange); v27 = std::max(0.001f, _gun_radius); }
                            else if ( v93 && (!targetManualCompound || j >= targetLegacySlots) )
                            {
                                int sphereIndex = targetManualCompound ? j - targetLegacySlots : j;
                                v77 = cellUnit->_position + targetRotationT.Transform( v93->roboColls[sphereIndex].coll_pos );

                                v27 = v93->roboColls[sphereIndex].robo_coll_radius;
                            }
                            else
                            {
                                v77 = cellUnit->_position;

                                v27 = cellUnit->_radius;
                            }

                            if ( !v93 || v27 >= 0.01 )
                            {
                                v121 = v27;

                                vec3d v63 = v77 - shotOldPos;

                                if ( usesShape || v63.dot( shotDir ) >= 0.3 )
                                {
                                    vec3d v33 = shotDir * v63;
                                    const float v111Sq = v63.square();
                                    const float v110Sq = v33.square();
                                    const float v37 = v27 + _gun_radius;

                                    if ( usesShape || (v37 > 0.0f && v37 * v37 > v110Sq) )
                                    {
                                        if ( usesShape || v110Sq + POW2(minigunTraceRange) > v111Sq )
                                        {
                                            // The exact distance is only needed after the cheap
                                            // squared tests accept this collision sphere.
                                            const float v111 = sqrtf(v111Sq);
                                            float unitEntryDistance = v111;
                                            if (!usesShape && !ypabact_GetRaySphereEntryDistance(
                                                     shotPos, shotDir, v77, v37,
                                                     &unitEntryDistance) )
                                                continue;

                                            if (usesShape) unitEntryDistance = shapeHit.fraction * minigunTraceRange;

                                            // A solid world surface wins when it is reached
                                            // before the target collision sphere. Preserve the
                                            // existing MGUN multi-hit behavior for units that are
                                            // all genuinely in front of that surface.
                                            if ( minigunWorldHit &&
                                                 unitEntryDistance >= minigunWorldHitDistance - 0.01f )
                                                continue;

                                            if ( !v22 )
                                            {
                                                int energ;
                                                if ( cellUnit->getBACT_inputting() || cellUnit->getBACT_viewer() )
                                                {
                                                    float v39 = mgunDamage * (100.0 - cellUnit->GetEffectiveShield());
                                                    energ = (v39 * 0.004);
                                                }
                                                else
                                                {

                                                    float v41 = mgunDamage * (100.0 - cellUnit->GetEffectiveShield());
                                                    energ = v41 / 100;
                                                }

                                                bact_arg84 v86;
                                                v86.unit = this;
                                                v86.energy = -energ;

                                                // The Deflect Buff stops MGUN damage without
                                                // spending a charge. The ray still hits normally,
                                                // so tracer/impact presentation stays unchanged.
                                                if ( energ && !cellUnit->HasDeflectBuff() )
                                                    cellUnit->ModifyEnergy(&v86);
                                            }

                                            v22 = 1;

                                            vec3d minigunImpactPoint = cellUnit->_position;
                                            float minigunImpactDistance = v111;
                                            bool hasMinigunSpreadImpactPoint = false;

                                            if (usesShape)
                                            {
                                                minigunImpactPoint = shapeHit.point;
                                                minigunImpactDistance = unitEntryDistance;
                                                hasMinigunSpreadImpactPoint = true;
                                            }
                                            else if ( spreadX > 0.0f || spreadY > 0.0f )
                                            {
                                                if ( ypabact_GetMinigunSpreadImpactPoint(shotPos, shotDir, v77, v27, &minigunImpactPoint, &minigunImpactDistance) )
                                                    hasMinigunSpreadImpactPoint = true;
                                            }

                                            if ( !v108 || v123 > minigunImpactDistance )
                                            {
                                                v108 = cellUnit;
                                                v123 = minigunImpactDistance;
                                                v121 = v27;
                                                minigunSpreadImpactPoint = hasMinigunSpreadImpactPoint;
                                                v66 = minigunImpactPoint;
                                            }
                                        }
                                    }
                                }

                            }

                        }


                    }
                }
            }
        }

        int v55 = 0;
        int v96 = minigunWorldHit && !v108 ? 1 : 0;

        if ( spawnVisual )
        {
            if ( v108 )
            {
                v55 = 1;
                v96 = 0;

                if ( minigunSpreadImpactPoint )
                    v80 = v66;
                else if (isnormal(v123)) // Not nullptr, NAN, INF
                    v80 = v66 - (v66 - shotPos) * (v121 * 0.7) / v123;
                else
                    v80 = v66;
            }
            else if ( minigunWorldHit )
            {
                v55 = 1;
                if ( _mgun_decal_enable )
                {
                    _world->SpawnGroundDecal(_mgun_decal, v59);
                }
            }

            bool spawnedVehicleImpact = false;
            if ( v55 && vehicleTimedMgun )
                spawnedVehicleImpact = ypabact_SpawnVehicleMinigunImpact(this, v80, shotDir, v96 != 0);

            if ( v55 && !spawnedVehicleImpact && _mgun != -1 )
            {
                ypaworld_arg146 arg147;
                arg147.pos = v80;
                arg147.vehicle_id = _mgun;

                NC_STACK_ypamissile *gunFireBact = _world->ypaworld_func147(&arg147);

                if ( gunFireBact )
                {
                    gunFireBact->_owner = _owner;

                    gunFireBact->_kidRef.Detach();
                    gunFireBact->_parent = nullptr;

                    _missiles_list.push_back(gunFireBact);

                    setState_msg v69;
                    v69.newStatus = BACT_STATUS_DEAD;
                    v69.setFlags = 0;
                    v69.unsetFlags = 0;

                    gunFireBact->SetStateInternal(&v69);

                    if ( v96 )
                    {
                        v69.setFlags = BACT_STFLAG_DEATH2;
                        v69.newStatus = BACT_STATUS_NOPE;
                        v69.unsetFlags = 0;
                        gunFireBact->SetStateInternal(&v69);

                        if ( v59.skel && v59.polyID >= 0 && (size_t)v59.polyID < v59.skel->polygons.size() )
                            gunFireBact->AlignMissileByNormal( v59.skel->polygons[ v59.polyID ].Normal() );
                    }
                }
            }
        }
    }

    return 1;
}
