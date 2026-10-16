// Core of NC_STACK_ypabact: construction, update/render loop, state changes, waypoints and network updates.
// The rest of the class lives in feature files next to this one:
//   AI and combat decisions ......... ypabact_ai.cpp
//   player control .................. ypabact_user.cpp
//   movement, collisions, crashes ... ypabact_physics.cpp
//   damage, death, plasma, medals ... ypabact_damage.cpp
//   weapons (missiles, minigun) ..... ypabact_weapons.cpp
//   laser / artillery / kamikaze / carrier / proximity / fire trigger area / projectile / recoil / status FX / camera / control:
//   ypabact_<feature>.cpp
// Shared helpers and constants are declared in ypabact_internal.h.
#include "game/ypabact_internal.h"

int ypabact_id = 1;

double ypabact_GetArcGrenadeGravity(const World::TWeapProto &wproto)
{
    return std::isfinite(wproto.grenade_arc_gravity) &&
           wproto.grenade_arc_gravity > 0.0f
               ? std::min((double)wproto.grenade_arc_gravity, 1000.0)
               : 9.80665;
}

bool ypabact_TrySolveArcGrenadeDirection(
    const vec3d &launchPos, const vec3d &targetPos,
    const World::TWeapProto &wproto, vec3d *outDirection)
{
    if ( !outDirection ||
         !std::isfinite(launchPos.x) || !std::isfinite(launchPos.y) ||
         !std::isfinite(launchPos.z) || !std::isfinite(targetPos.x) ||
         !std::isfinite(targetPos.y) || !std::isfinite(targetPos.z) ||
         !std::isfinite(wproto.start_speed) || wproto.start_speed <= 0.0f )
    {
        return false;
    }

    const double speed = (double)wproto.start_speed;
    const double gravity = ypabact_GetArcGrenadeGravity(wproto);
    const double deltaX = (double)targetPos.x - launchPos.x;
    const double deltaZ = (double)targetPos.z - launchPos.z;
    const double horizontalWorld = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
    const double verticalUp = ((double)launchPos.y - targetPos.y) /
                              YPABACT_MISSILE_DISTANCE_SCALE;

    if ( horizontalWorld <= 0.001 )
    {
        if ( std::fabs(verticalUp) <= 0.001 )
            return false;

        const double maxUp = speed * speed / (2.0 * gravity);
        if ( verticalUp > maxUp )
            return false;

        *outDirection = vec3d::OY(-1.0f);
        return true;
    }

    const double horizontal = horizontalWorld /
                              YPABACT_MISSILE_DISTANCE_SCALE;
    const double speedSquared = speed * speed;
    const double discriminant =
        speedSquared * speedSquared -
        gravity * (gravity * horizontal * horizontal +
                   2.0 * verticalUp * speedSquared);
    const double tolerance =
        std::max(1.0, speedSquared * speedSquared) * 1.0e-10;

    if ( discriminant < -tolerance )
        return false;

    const double root = std::sqrt(std::max(0.0, discriminant));
    const double denominator = gravity * horizontal;
    if ( denominator <= 0.0 || !std::isfinite(denominator) )
        return false;

    const double lowAngle = std::atan((speedSquared - root) / denominator);
    const double highAngle = std::atan((speedSquared + root) / denominator);
    const double preferredDegrees =
        std::isfinite(wproto.grenade_arc_angle) &&
        wproto.grenade_arc_angle > 0.0f
            ? std::min((double)wproto.grenade_arc_angle, 89.0)
            : 0.0;
    const double preferred = preferredDegrees * C_PI_180;
    const double launchAngle =
        std::fabs(highAngle - preferred) < std::fabs(lowAngle - preferred)
            ? highAngle : lowAngle;

    const double horizontalCos = std::cos(launchAngle);
    const double horizontalSin = std::sin(launchAngle);
    vec3d direction((float)(deltaX / horizontalWorld * horizontalCos),
                    (float)-horizontalSin,
                    (float)(deltaZ / horizontalWorld * horizontalCos));

    if ( direction.normalise() <= 0.001f )
        return false;

    *outDirection = direction;
    return true;
}

float NC_STACK_ypabact::ReadPowerStationEnergyMultiplier()
{
    std::string value = System::IniConf::GamePowerStationEnergyMultiplier.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return 1.0f;

    try
    {
        size_t pos = 0;
        float multiplier = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !isfinite(multiplier) || multiplier < 0.0f )
            return 1.0f;

        return multiplier;
    }
    catch (...)
    {
        return 1.0f;
    }
}

float ypabact_ReadNonNegativeFloatIni(Common::Ini::Key &key, float dflt)
{
    std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return dflt;

    try
    {
        size_t pos = 0;
        float parsed = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos || parsed < 0.0f )
            return dflt;

        return parsed;
    }
    catch (...)
    {
        return dflt;
    }
}

bool ypabact_ReadAbsoluteOrPercentIni(Common::Ini::Key &key,
                                                World::TAbsoluteOrPercent *out,
                                                float maxPercent)
{
    if ( !out || !key.WasSet )
        return false;

    World::TAuthoredScalar parsed;
    if ( !World::ParseAuthoredScalar(key.Get<std::string>(), parsed) ||
         !std::isfinite(parsed.value) || parsed.value < 0.0f )
    {
        return false;
    }

    out->defined = true;
    out->percent = parsed.percent;
    out->value = parsed.percent ? std::min(parsed.value, maxPercent) : parsed.value;
    return true;
}

bool ypabact_IsMainVPBase(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    if ( bact->_bact_type == BACT_TYPES_MISSLE )
        return base == bact->_vp_normal || base == bact->_vp_fire || base == bact->_vp_wait;

    return base == bact->_vp_normal || base == bact->_vp_fire || base == bact->_vp_wait || base == bact->_vp_genesis;
}

bool ypabact_IsTintableVisualBase(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    return base == bact->_vp_normal || base == bact->_vp_fire || base == bact->_vp_wait ||
           base == bact->_vp_dead || base == bact->_vp_megadeth || base == bact->_vp_genesis;
}

mat3x3 ypabact_BuildProjectileVisualTangent(const NC_STACK_ypabact *bact,
                                                   const vec3d &visualVelocityLocal)
{
    if ( !bact )
        return mat3x3::Ident();

    // Missile integration advances the physical center by speed * dt * 6.
    // Convert that world-space center velocity back into projectile-local space
    // and add the derivative of the render-only motion. The resulting vector is
    // the tangent of the rendered curve, never a gameplay velocity.
    const vec3d centerVelocityWorld = bact->_fly_dir * (bact->_fly_dir_length * 6.0);
    const vec3d centerVelocityLocal = bact->_rotation.Transform(centerVelocityWorld);
    vec3d tangent = centerVelocityLocal + visualVelocityLocal;

    const double tangentLength = tangent.length();
    if ( tangentLength <= 0.000001 )
        return mat3x3::Ident();
    tangent /= tangentLength;

    const vec3d forwardAxis(0.0, 0.0, 1.0);
    double alignment = forwardAxis.dot(tangent);
    alignment = std::max(-1.0, std::min(alignment, 1.0));

    vec3d axis = forwardAxis * tangent;
    const double axisLength = axis.length();
    if ( axisLength <= 0.000001 )
        return alignment < 0.0 ? mat3x3::RotateX(C_PI) : mat3x3::Ident();

    axis /= axisLength;
    return mat3x3::AxisAngle(axis, std::acos(alignment)).Transpose();
}

float ypabact_GetDamagedThresholdEnergy(const NC_STACK_ypabact *bact)
{
    if ( !bact || !bact->_damaged_fx.threshold.defined ||
         bact->_damaged_fx.threshold.value <= 0.0f )
        return 0.0f;

    if ( bact->_damaged_fx.threshold.percent )
    {
        if ( bact->_energy_max <= 0 )
            return 0.0f;
        return (float)bact->_energy_max *
               (std::min(bact->_damaged_fx.threshold.value, 100.0f) * 0.01f);
    }

    return bact->_damaged_fx.threshold.value;
}

bool NC_STACK_ypabact::ShouldHideFromStrategicUI() const
{
    if ( _isUnitGunChild || _isDummy )
        return true;

    if ( _bact_type != BACT_TYPES_GUN )
        return false;

    const NC_STACK_ypagun *gun = dynamic_cast<const NC_STACK_ypagun *>(this);
    return gun && (gun->_gunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO);
}

bool ypabact_CanUseGameplayStatusMechanics(NC_STACK_ypabact *bact, bool allowCreate)
{
    // Gameplay status mechanics must not depend on strategic/UI visibility.
    // Some real gameplay actors, such as host/flak/attached gun objects, may be
    // intentionally hidden from strategic UI while still being valid damageable
    // actors. UI code can still filter them separately before rendering icons.
    // Direct-parent debuff inheritance is the intentional exception for CREATE:
    // freshly generated children must receive the inherited state during genesis.
    return bact &&
           bact->getBACT_pWorld() &&
           bact->_owner != World::OWNER_0 &&
           bact->_energy > 0 &&
           bact->_energy_max > 0 &&
           bact->_bact_type != BACT_TYPES_MISSLE &&
           bact->_status != BACT_STATUS_DEAD &&
           (allowCreate || bact->_status != BACT_STATUS_CREATE) &&
           bact->_status != BACT_STATUS_BEAM &&
           !(bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER));
}

NC_STACK_ypabact *ypabact_FindLiveBactByGid(World::RefBactList &list, int32_t gid)
{
    for (NC_STACK_ypabact *unit : list)
    {
        if ( unit->_gid == gid )
        {
            if ( unit->_kidRef.IsListType(World::BLIST_CACHE) || unit->_status == BACT_STATUS_DEAD )
                return nullptr;

            return unit;
        }

        NC_STACK_ypabact *kid = ypabact_FindLiveBactByGid(unit->_kidList, gid);
        if ( kid )
            return kid;
    }

    return nullptr;
}

// Puts the per-unit state of the OpenNeoUA features (spawning, at-death effects, proximity defense,
// artillery shell, laser, kamikaze) back to its defaults. Shared by the constructor, Init and Renew.
void NC_STACK_ypabact::ResetFeatureRuntimeState()
{
    _spawn_units = 0;
    _spawn_vehicle = 0;
    _spawn_interval = 5000;
    _spawn_trigger_radius = 0.0;
    _spawn_random_pos = 0.0;
    _spawn_offset = vec3d(0.0, 0.0, 0.0);
    _spawn_max_active = 0;
    _spawn_count = 1;
    _spawn_instant = 0;
    _spawn_last_time = 0;
    _at_death_spawn_units = 0;
    _at_death_spawn_vehicle = 0;
    _at_death_spawn_count = 1;
    _at_death_spawn_random_pos = 0.0;
    _at_death_spawn_instant = 0;
    _at_death_spawn_immunity_time = 0;
    _at_death_spawn_done = false;
    _at_death_spawn_protection_end_time = 0;
    _at_death_spawn_restore_vulnerable = false;
    _at_death_push_force = 0.0f;
    _at_death_push_radius = 0.0f;
    _at_death_push_falloff = 0;
    _at_death_energy = 0;
    _at_death_energy_radius = 0.0f;
    _at_death_energy_falloff = 0;
    _carrier_spawn_root_gid = 0;
    _carrier_spawn_root_vehicle = 0;
    _carrier_spawned_gids.clear();
    _proximity_defense_enable = 0;
    _proximity_defense_weapon = 0;
    _proximity_defense_trigger_radius = 0.0;
    _proximity_defense_interval = 1000;
    _proximity_defense_shots = 12;
    _proximity_defense_vp_launch = -1;
    _proximity_defense_3ds_launch.clear();
    _proximity_defense_base_launch.clear();
    _proximity_defense_fire_mode = 0;
    _proximity_defense_sequence_delay = 100;
    _proximity_defense_mode = 0;
    _proximity_defense_horizontal_angle_set = false;
    _proximity_defense_horizontal_angle_min = 0.0;
    _proximity_defense_horizontal_angle_max = 360.0;
    _proximity_defense_vertical_angle_set = false;
    _proximity_defense_vertical_angle_min = -10.0;
    _proximity_defense_vertical_angle_max = 45.0;
    _proximity_defense_sequence_active = false;
    _proximity_defense_sequence_shots_fired = 0;
    _proximity_defense_next_shot_time = 0;
    _proximity_defense_next_activation_time = 0;
    _proximity_defense_at_death_done = false;
    _artillery_shell_barrage_active = false;
    _artillery_shell_shots_remaining = 0;
    _artillery_shell_next_shot_time = 0;
    _artillery_shell_next_activation_time = 0;
    _artillery_shell_next_scan_time = 0;
    _artillery_shell_target_center = vec3d(0.0, 0.0, 0.0);
    _artillery_shell_has_pending = false;
    _artillery_shell_pending_target = vec3d(0.0, 0.0, 0.0);
    StopLaser();
    StopVerticalLaser();
    _kamikaze_triggered = false;
    _kamikaze_ground_decals_handled = false;
}

NC_STACK_ypabact::NC_STACK_ypabact()
: _kidList(this, GetKidRefNode, World::BLIST_KIDS)
{
    _wrldSize = vec2d();
    _wrldSectors = Common::Point();
    _bact_type = 0;
    _gid = 0;
    _vehicleID = 0;
    _mimic_disguise_vehicleID = 0;
    _bflags = 0;
    _commandID = 0;
    _host_station = nullptr;
    _isGenesisProduced = false;
    _genesisExitPending = false;
    _genesisGroundChecked = false;
    _parent = nullptr;

    _soundFlags = 0;
    _volume = 0;
    _pitch_max = 0.0;
    _energy = 0;
    _energy_max = 0;
    _buff = World::TVehicleBuffConfig();
    _buff_deflect_charges_max = 0;
    _invulnerable = false;
    _reload_const = 0;
    _shield = 0;
    _radar = 0;
    _owner = 0;
    _aggr = 0;
    _status = BACT_STATUS_NOPE;
    _status_flg = 0;
    _primTtype = 0;
    _secndTtype = 0;
    _primT_cmdID = 0;
    _secndT_cmdID = 0;
    _primT.pbact = nullptr;
    _secndT.pbact = nullptr;
    _adist_sector = 0.0;
    _adist_bact = 0.0;
    _sdist_sector = 0.0;
    _sdist_bact = 0.0;
    _ai_engage_range = 0.0f;
    _ai_retreat_range = 0.0f;
    _ai_reengage_range = 0.0f;
    _unifiedAICombatDistance = false;
    _current_waypoint = 0;
    _waypoints_count = 0;
    _m_cmdID = 0;
    _m_owner = 0;
    _fe_cmdID = 0;
    _fe_time = 0;
    _mass = 0.0;
    _base_force = 0.0;
    _base_maxrot = 0.0;
    _force = 0.0;
    _airconst = 0.0;
    _airconst_static = 0.0;
    _maxrot = 0.0;
    _viewer_horiz_angle = 0.0;
    _viewer_vert_angle = 0.0;
    _viewer_max_up = 0.0;
    _viewer_max_down = 0.0;
    _viewer_max_side = 0.0;
    _thraction = 0.0;
    _fly_dir_length = 0.0;
    _fallDamageAirborne = false;
    _fallDamageConsumed = false;
    _recoilVisualStartOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualPeakOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualStartPitch = 0.0f;
    _recoilVisualPeakPitch = 0.0f;
    _recoilVisualPhaseStartTime = 0;
    _recoilVisualKickEndTime = 0;
    _recoilVisualHoldEndTime = 0;
    _recoilVisualReturnEndTime = 0;
    _recoilAiRecoveryEndTime = 0;
    _recoilPlayerRecoveryEndTime = 0;
    _recoilPushVel = vec3d(0.0, 0.0, 0.0);
    _height = 0.0;
    _player_max_altitude_above_ground = 0.0;
    _vp_scale = vec3d(1.0, 1.0, 1.0);
    _vp_tint = World::TVisualTint();
    _vp_rotation = vec3d(0.0, 0.0, 0.0);
    _vp_spin_strength = vec3d(0.0, 0.0, 0.0);
    _vp_trail_scale = vec3d(1.0, 1.0, 1.0);
    _vp_trail_tint = World::TVisualTint();
    _vp_trail_spin_strength = vec3d(0.0, 0.0, 0.0);
    ypabact_ResetDamagedFX(this);
    _decoration_fx = World::TDecorationFXConfig();
    _decoration_fx_next_time = 0;
    _decoration_fx_persistent_id = 0;
    _damaged_force_malus = 0.0;
    _damaged_maxrot_malus = 0.0;
    _damaged_mgun_shot_time_malus = 0.0;
    _damaged_shot_time_malus = 0.0;
    _damaged_snd_pitch_multiplier = 1.0;
    _damaged_fx_active = false;
    _active_debuff.Clear();
    _debuff_soundcarrier.Clear();
    _player_launch_shake_carrier.Clear();
    _mgun_recoil_shake = TSndFxPosParam();
    _mgun_recoil_shake_carrier.Clear();
    _laser_launch_soundcarrier.Clear();
    _mimic_soundcarrier.Clear();

    _vp_active = 0;

    _vp_extra_mode = 0;

    _radius = 0.0;
    _viewer_radius = 0.0;
    _overeof = 0.0;
    _viewer_overeof = 0.0;
    _cockpit_camera_offset = vec3d(0.0, 0.0, 0.0);
    _cockpit_gun_camera_recoil = 0.0f;
    _cockpit_camera_hide_vehicle = false;
    _mgun_decal_enable = false;
    _mgun_decal = World::TChainFXConfig();
    _clock = 0;
    _AI_time1 = 0;
    _AI_time2 = 0;
    _search_time1 = 0;
    _search_time2 = 0;
    _scale_time = 0;
    _brkfr_time = 0;
    _brkfr_time2 = 0;
    _newtarget_time = 0;
    _assess_time = 0;
    _waitCol_time = 0;
    _slider_time = 0;
    _dead_time = 0;
    _beam_time = 0;
    _energy_time = 0;
    _weapon = 0;
    _extra_weapons = {0, 0, 0};
    _weapon_player_switch_mode = World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE;
    _weapon_ai_switch_mode = World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
    _weapon_slot_index = 0;
    _current_weapon_id = -1;
    _current_weapon_source_slot = 0;
    _userHomingPrimaryTargetGid = 0;
    _userHomingTargetCycleRequested = false;
    _alternativeViewActive = false;
    _weapon_flags = 0;
    _mgun = 0;
    _mgun_set = false;
    _mgun_shot_time = 0;
    _mgun_shot_time_user = 0;
    _mgun_vp_dead = 0;
    _mgun_vp_megadeth = 0;
    _mgun_3ds_dead.clear();
    _mgun_3ds_megadeth.clear();
    _mgun_base_dead.clear();
    _mgun_base_megadeth.clear();
    _mgun_energy = 0.0;
    _mgun_angle = 0.0;
    _mgun_energy_set = false;
    _cockpit_mgun_camera_recoil = 0.0f;
    _mgun_angle_set = false;
    _vehicle_fire_vp_end_time = 0;
    _weapon_spread_x = 0.0;
    _weapon_spread_y = 0.0;
    _weapon_spread_z = 0.0;
    _weapon_arc_x = 0.0;
    _weapon_arc_y = 0.0;
    _weapon_cone_xy = 0.0;
    _mgun_spread_x = 0.0;
    _mgun_spread_y = 0.0;
    _num_weapons = 0;
    _mgun_fire_energy_cost = 0.0f;
    _mgun_fire_energy_cost_defined = false;
    _num_weapons_snd_events = 0;
    _weapon_projectile_counts = {0, 0, 0, 0};
    _weapon_projectile_count_maxs = {0, 0, 0, 0};
    _weapon_time = 0;
    ResetProgressiveWeaponFireRate();
    _fire_x_mode = World::TVhclProto::FIRE_X_MODE_VANILLA;
    _fire_x_start = 0.0;
    _fire_x_step = 0.0;
    _fire_x_slots = 0;
    _fire_x_advanced = false;
    _fire_x_slot_index = 0;
    _fire_x_random_seeded = false;
    _fire_x_random_state = 0;
    _fire_x_random_order.clear();
    _gun_angle = 0.0;
    _gun_angle_user = 0.0;
    _gun_leftright = 0.0;
    _gun_radius = 0.0;
    _gun_power = 0.0;
    _mgun_time = 0;
    _mgun_energy_cost_time = 0;
    _salve_counter = 0;
    _kill_after_shot = 0;
    _suicide_handoff_wait_fire_release = false;
    ResetFeatureRuntimeState();
    _gunDisplayName.clear();
    _unitGunsParentRotation = mat3x3::Ident();
    _unitGunsSpawned = false;
    _unitGunsHaveParentRotation = false;
    _isUnitGunChild = false;
    _unitGunNoSwitch = false;
    _isDummy = false;
    _collNodes = World::rbcolls();
    _manualCompoundBroadRadius = -1.0f;
    _heading_speed = 0.0;
    _killer = nullptr;
    _killer_owner = 0;
    _sessionKillMarks = 0;
    _sessionKillsTowardMedal = 0;
    _reb_count = 0;
    _atk_ret = 0;
    _lastFrmStamp = 0;
    _scale_start = 0.0;
    _scale_speed = 0.0;
    _scale_accel = 0.0;
    _scale_duration = 0;
    _scale_pos = 0;
    _scale_delay = 0;

    for (NC_STACK_base *& vp_fx : _vp_fx_models)
        vp_fx = nullptr;


    _oflags = 0;
    _yls_time = 0;

    _world = nullptr;
    _deinitInProgress = false;
}

NC_STACK_ypabact::~NC_STACK_ypabact()
{
    Common::DeleteAndNull(&_current_vp);
}

size_t NC_STACK_ypabact::Init(IDVList &stak)
{
    if ( !NC_STACK_nucleus::Init(stak) )
        return 0;

    _attackersList.clear();
    _kidList.clear();
    _missiles_list.clear();

    _gid = ypabact_id;
    _bact_type = BACT_TYPES_BACT;
    _host_station = nullptr;
    _isGenesisProduced = false;
    _genesisExitPending = false;
    _genesisGroundChecked = false;
    _viewer_rotation = mat3x3::Ident();
    _fly_dir = vec3d(0.0, 0.0, 0.0);
    _fly_dir_length = 0;
    _recoilVisualStartOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualPeakOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualStartPitch = 0.0f;
    _recoilVisualPeakPitch = 0.0f;
    _recoilVisualPhaseStartTime = 0;
    _recoilVisualKickEndTime = 0;
    _recoilVisualHoldEndTime = 0;
    _recoilVisualReturnEndTime = 0;
    _recoilAiRecoveryEndTime = 0;
    _recoilPlayerRecoveryEndTime = 0;
    _recoilPushVel = vec3d(0.0, 0.0, 0.0);
    _deinitInProgress = false;
    _target_vec = vec3d(0.0, 0.0, 0.0);




    ypabact_id++;

    _rotation = _viewer_rotation;

    _mass = 400.0;
    _base_force = 5000.0;
    _base_maxrot = 0.5;
    _force = 5000.0;
    _airconst = 500.0;
    _maxrot = 0.5;
    _height = 150.0;
    _radius = 20.0;
    _viewer_radius = 40.0;
    _overeof = 10.0;
    _viewer_overeof = 40.0;
    _cockpit_camera_offset = vec3d(0.0, 0.0, 0.0);
    _cockpit_gun_camera_recoil = 0.0f;
    _cockpit_camera_hide_vehicle = false;
    _mgun_decal_enable = false;
    _mgun_decal = World::TChainFXConfig();
    _energy = 10000;
    _shield = 0;
    _heading_speed = 0.7;
    _yls_time = 3000;
    _aggr = 50;
    _energy_max = 10000;
    _invulnerable = false;
    ypabact_ResetDamagedFX(this);
    _vp_scale = vec3d(1.0, 1.0, 1.0);
    _vp_tint = World::TVisualTint();
    _vp_rotation = vec3d(0.0, 0.0, 0.0);
    _vp_spin_strength = vec3d(0.0, 0.0, 0.0);
    _vp_trail_scale = vec3d(1.0, 1.0, 1.0);
    _vp_trail_tint = World::TVisualTint();
    _vp_trail_spin_strength = vec3d(0.0, 0.0, 0.0);
    _decoration_fx = World::TDecorationFXConfig();
    _decoration_fx_next_time = 0;
    _decoration_fx_persistent_id = 0;
    _damaged_force_malus = 0.0;
    _damaged_maxrot_malus = 0.0;
    _damaged_mgun_shot_time_malus = 0.0;
    _damaged_shot_time_malus = 0.0;
    _damaged_snd_pitch_multiplier = 1.0;
    _damaged_fx_active = false;
    _active_debuff.Clear();
    _fire_x_slot_index = 0;
    _fire_x_random_seeded = false;
    _fire_x_random_state = 0;
    _fire_x_random_order.clear();
    _debuff_soundcarrier.Clear();
    _player_launch_shake_carrier.Clear();
    _mgun_recoil_shake = TSndFxPosParam();
    _mgun_recoil_shake_carrier.Clear();
    _laser_launch_soundcarrier.Clear();
    _mgun_soundcarrier.Clear();
    _mimic_soundcarrier.Clear();
    _mgun_sound_index = 0;
    _vehicle_fire_vp_end_time = 0;
    // The player limit is altitude above the current sector terrain and keeps
    // the vanilla 1600 default. The optional AI ceiling is read globally.
    _player_max_altitude_above_ground = ypabact_ReadPlayerMaxAltitudeAboveGround();
    _gun_radius = 5.0;
    _gun_power = 4000.0;
    ResetFeatureRuntimeState();
    _unitGuns.clear();
    _gunDisplayName.clear();
    _unitGunsParentRotation = mat3x3::Ident();
    _unitGunsSpawned = false;
    _unitGunsHaveParentRotation = false;
    _isUnitGunChild = false;
    _unitGunNoSwitch = false;
    _isDummy = false;
    _collNodes = World::rbcolls();
    _manualCompoundBroadRadius = -1.0f;
    _adist_sector = 800.0;
    _adist_bact = 650.0;
    _sdist_sector = 200.0;
    _sdist_bact = 100.0;
    _oflags = BACT_OFLAG_EXACTCOLL;

    _world = stak.Get<NC_STACK_ypaworld *>(BACT_ATT_WORLD, nullptr);// get ypaworld

    if ( _world )
    {
        for( auto& it : stak )
        {
            IDVPair &val = it.second;

            if ( !val.Skip )
            {
                switch (val.ID)
                {
                case BACT_ATT_VIEWER:
                {
                    uamessage_viewer viewMsg;

                    if ( val.Get<int32_t>() )
                    {
                        _world->ypaworld_func131(this); //Set current bact

                        _oflags |= BACT_OFLAG_VIEWER;

                        if ( _world->_isNetGame )
                            viewMsg.view = 1;

                        SFXEngine::SFXe.startSound(&_soundcarrier, 8);
                    }
                    else
                    {
                        _oflags &= ~BACT_OFLAG_VIEWER;

                        if ( _world->_isNetGame )
                            viewMsg.view = 0;

                        SFXEngine::SFXe.sub_424000(&_soundcarrier, 8);
                    }

                    if ( _world->_isNetGame ) // Network message send routine?
                    {
                        viewMsg.msgID = UAMSG_VIEWER;
                        viewMsg.owner = _owner;
                        viewMsg.classID = _bact_type;
                        viewMsg.id = _gid;

                        if ( viewMsg.classID == BACT_TYPES_MISSLE )
                        {
                            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(this);
                            viewMsg.launcher = miss->GetLauncherBact()->_gid;
                        }

                        _world->NetBroadcastMessage(&viewMsg, sizeof(viewMsg), true);
                    }
                }
                break;

                case BACT_ATT_INPUTTING:
                    if ( val.Get<int32_t>() )
                    {
                        _oflags |= BACT_OFLAG_USERINPT;
                        _world->setYW_userVehicle(this);
                    }
                    else
                    {
                        _oflags &= ~BACT_OFLAG_USERINPT;
                    }
                    break;

                case BACT_ATT_EXACTCOLL:
                    setBACT_exactCollisions(val.Get<bool>());
                    break;

                case BACT_ATT_BACTCOLL:
                    setBACT_bactCollisions ( val.Get<bool>() );
                    break;

                case BACT_ATT_AIRCONST:
                    setBACT_airconst(val.Get<int32_t>());
                    break;

                case BACT_ATT_LANDINGONWAIT:
                    setBACT_landingOnWait ( val.Get<bool>() );
                    break;

                case BACT_ATT_YOURLS:
                    setBACT_yourLastSeconds(val.Get<int32_t>());
                    break;

                case BACT_ATT_VISPROT:
                    SetVP( val.Get<NC_STACK_base *>());
                    break;

                case BACT_ATT_AGGRESSION:
                    setBACT_aggression(val.Get<int32_t>());
                    break;

                case BACT_ATT_EXTRAVIEWER:
                    setBACT_extraViewer ( val.Get<bool>() );
                    break;

                case BACT_ATT_ALWAYSRENDER:
                    setBACT_alwaysRender ( val.Get<bool>() );
                    break;

                default:
                    break;
                }
            }
        }
    }

    _tForm.Pos = _position;

    _tForm.SclRot = _rotation;

    _status = BACT_STATUS_NORMAL;

    _wrldSectors = _world->GetMapSize();

    _wrldSize = World::SectorIDToPos2( _wrldSectors );

    return 1;
}

size_t NC_STACK_ypabact::Deinit()
{
    if (_world && _world->_collisionScene) _world->_collisionScene->Forget(this);
    _deinitInProgress = true;
    SFXEngine::SFXe.StopCarrier(&_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_debuff_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_player_launch_shake_carrier);
    SFXEngine::SFXe.StopCarrier(&_mgun_recoil_shake_carrier);
    SFXEngine::SFXe.StopCarrier(&_laser_launch_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_laser_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_vertical_laser_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_laser_hit_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_vertical_laser_hit_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_mgun_soundcarrier);
    SFXEngine::SFXe.StopCarrier(&_mimic_soundcarrier);
    _active_debuff.Clear();
    _fire_x_mode = World::TVhclProto::FIRE_X_MODE_VANILLA;
    _fire_x_start = 0.0;
    _fire_x_step = 0.0;
    _fire_x_slots = 0;
    _fire_x_advanced = false;
    _fire_x_slot_index = 0;
    _fire_x_random_seeded = false;
    _fire_x_random_state = 0;
    _fire_x_random_order.clear();

    _status_flg |= BACT_STFLAG_CLEAN;

    if ( !(_status_flg & BACT_STFLAG_DEATH1) )
    {
        if ( _world && _world->IsLevelTeardownInProgress() )
        {
            // Level teardown is destruction, not a gameplay death. DeleteLevel
            // already owns the whole hierarchy, so death hooks must not damage,
            // spawn, fire or reparent objects while its lists are being erased.
            _status = BACT_STATUS_DEAD;
            _status_flg |= BACT_STFLAG_DEATH1;
        }
        else
            Die();
    }

    if ( _pSector )
        _cellRef.Detach();

    _kidRef.Detach();

    // OpenNeoUA follow_vehicle: a removed unit must never leave its trajectory
    // partner pointing at freed memory.
    ClearDeadFollowLink();

    CleanupUnitGuns(true);

    while (!_kidList.empty())
        _kidList.front()->Delete();

    while (!_missiles_list.empty())
    {
        _missiles_list.front()->Delete();
        _missiles_list.pop_front();
    }

    return NC_STACK_nucleus::Deinit();
}

void NC_STACK_ypabact::SetUnitGuns(const std::vector<World::TRoboGun> &guns)
{
    CleanupUnitGuns(true);

    _unitGuns = guns;

    for (World::TRoboGun &gun : _unitGuns)
        gun.gun_obj = nullptr;

    _unitGunsParentRotation = mat3x3::Ident();
    _unitGunsSpawned = _unitGuns.empty();
    _unitGunsHaveParentRotation = false;
}

void NC_STACK_ypabact::CleanupUnitGuns(bool releaseGuns, bool parentDying)
{
    for (World::TRoboGun &gun : _unitGuns)
    {
        NC_STACK_ypabact *gunObj = gun.gun_obj;
        gun.gun_obj = nullptr;

        if ( !gunObj )
            continue;

        NC_STACK_ypabact *fallback = parentDying ? _world->_userRobo : this;
        ypabact_SafeDetachControlFrom(gunObj, fallback);

        if ( (!_world || !_world->IsLevelTeardownInProgress()) &&
             !gunObj->IsDestroyed() && !(gunObj->_status_flg & BACT_STFLAG_DEATH1) )
        {
            gunObj->_killer = _killer;
            gunObj->Die();
        }

        if ( releaseGuns )
            gunObj->Release();
    }

    _unitGunsSpawned = false;
    _unitGunsHaveParentRotation = false;
}

void NC_STACK_ypabact::ClearUnitGunPointer(NC_STACK_ypabact *gunObj)
{
    for (World::TRoboGun &gun : _unitGuns)
    {
        if ( gun.gun_obj == gunObj )
            gun.gun_obj = nullptr;
    }
}

void NC_STACK_ypabact::UpdateUnitGuns(update_msg *)
{
    if ( _unitGuns.empty() || _isUnitGunChild || !_world )
        return;

    if ( _status == BACT_STATUS_DEAD || (_status_flg & BACT_STFLAG_DEATH1) )
    {
        CleanupUnitGuns(true);
        return;
    }

    mat3x3 parentRotation = _rotation.Transpose();
    mat3x3 parentRotationDelta = mat3x3::Ident();
    bool applyParentRotationDelta = false;

    if ( !_unitGunsHaveParentRotation )
    {
        _unitGunsParentRotation = parentRotation;
        _unitGunsHaveParentRotation = true;
    }
    else
    {
        parentRotationDelta = parentRotation * _unitGunsParentRotation.Transpose();
        applyParentRotationDelta = true;
        _unitGunsParentRotation = parentRotation;
    }

    if ( !_unitGunsSpawned )
    {
        _unitGunsSpawned = true;

        for (World::TRoboGun &gun : _unitGuns)
        {
            if ( !gun.robo_gun_type )
                continue;

            ypaworld_arg146 gunReq;
            gunReq.vehicle_id = gun.robo_gun_type;
            gunReq.pos = _position + _rotation.Transpose().Transform(gun.pos);
            gunReq.skip_unit_guns = true;

            NC_STACK_ypabact *gunObj = _world->ypaworld_func146(&gunReq);
            gun.gun_obj = gunObj;

            if ( gunObj )
            {
                // Establish attachment identity before evaluating semantic gun_type
                // behavior (notably legacy dummy, which is fully passive only when
                // mounted as a Unit Gun/Module).
                gunObj->_isUnitGunChild = true;
                gunObj->_unitGunNoSwitch = gun.noSwitch == 1;

                if ( NC_STACK_ypagun *attachedGun = dynamic_cast<NC_STACK_ypagun *>(gunObj) )
                {
                    attachedGun->ypagun_func128(_rotation.Transpose().Transform(gun.dir), false);
                    attachedGun->setGUN_roboGun(1);

                    // gun_type comes from the referenced vehicle prototype.
                    // Dummy stays excluded from voluntary AI targeting; radar
                    // and power are passive but remain normal damageable targets.
                    gunObj->_isDummy = attachedGun->getGUN_fireType() == NC_STACK_ypagun::GUN_TYPE_DUMMY;

                    if ( attachedGun->IsPassiveModule() )
                    {
                        gunObj->_aggr = 0;
                        gunObj->setBACT_bactCollisions(false);
                    }
                }

                gunObj->_owner = _owner;
                gunObj->InheritActiveDebuffFromParent(this);
                gunObj->_commandID = dword_5B1128++;
                gunObj->_host_station = nullptr;
                if ( !gunObj->_isDummy &&
                     (!dynamic_cast<NC_STACK_ypagun *>(gunObj) ||
                      !dynamic_cast<NC_STACK_ypagun *>(gunObj)->IsPassiveModule()) )
                    gunObj->_aggr = 60;
                gunObj->_gunDisplayName = gun.robo_gun_name;
                // OpenNeoUA invisible: attached guns inherit the carrier's current stealth
                // state so the whole unit cloaks/reveals as one.
                gunObj->_invisibleUnrevealed = _invisibleUnrevealed;

                if ( _world->_isNetGame )
                {
                    gunObj->_gid |= gunObj->_owner << 24;
                    gunObj->_commandID |= gunObj->_owner << 24;
                }

                AddSubject(gunObj);

                setState_msg createState;
                createState.setFlags = 0;
                createState.unsetFlags = 0;
                createState.newStatus = BACT_STATUS_CREATE;
                gunObj->SetState(&createState);
                gunObj->_scale_time = gunObj->_energy_max * 0.2;
            }
            else
            {
                ypa_log_out("Unable to create Unit-Gun\n");
            }
        }
    }

    for (World::TRoboGun &gun : _unitGuns)
    {
        NC_STACK_ypabact *gunObj = gun.gun_obj;

        if ( !gunObj )
            continue;

        if ( gunObj->IsDestroyed() || (gunObj->_status_flg & BACT_STFLAG_DEATH1) )
        {
            gun.gun_obj = nullptr;
            continue;
        }

        bact_arg80 posArg;
        posArg.pos = _position + _rotation.Transpose().Transform(gun.pos);
        posArg.field_C = 4;

        gunObj->_owner = _owner;
        gunObj->SetPosition(&posArg);

        if ( applyParentRotationDelta )
        {
            if ( NC_STACK_ypagun *attachedGun = dynamic_cast<NC_STACK_ypagun *>(gunObj) )
            {
                attachedGun->_rotation.SetX(parentRotationDelta.Transform(attachedGun->_rotation.AxisX()));
                attachedGun->_rotation.SetY(parentRotationDelta.Transform(attachedGun->_rotation.AxisY()));
                attachedGun->_rotation.SetZ(parentRotationDelta.Transform(attachedGun->_rotation.AxisZ()));
                attachedGun->_gunBasis = parentRotationDelta.Transform(attachedGun->_gunBasis);
                attachedGun->_gunRott = parentRotationDelta.Transform(attachedGun->_gunRott);
            }
        }

        // Passive modules skip ypagun::User_layer(), so preserve the old dummy
        // attachment camera behavior by keeping their extra-view orientation in sync.
        if ( NC_STACK_ypagun *attachedGun = dynamic_cast<NC_STACK_ypagun *>(gunObj) )
        {
            if ( attachedGun->IsPassiveModule() )
                gunObj->_viewer_rotation = gunObj->_rotation;
        }
    }
}

// Pick the active protective mounted Gun/module to absorb an incoming hit.
// Prefer the one closest to the attacker; fall back to the first active one.
// Host Stations use their native _roboGuns list; ordinary carriers use
// _unitGuns. The parser aliases unit_* to the Robo list for model = robo, so
// no second attachment runtime is created on Host Stations.
NC_STACK_ypabact *NC_STACK_ypabact::SelectProtectiveUnitGun(NC_STACK_ypabact *attacker)
{
    NC_STACK_ypabact *best = nullptr;
    NC_STACK_ypabact *firstActive = nullptr;
    float bestDist = 0.0;
    bool haveAttacker = (attacker != nullptr);
    vec3d srcPos;

    if ( haveAttacker )
        srcPos = attacker->_position;

    auto considerGuns = [&](std::vector<World::TRoboGun> &guns)
    {
        for (World::TRoboGun &gun : guns)
        {
            if ( !gun.protect )
                continue;

            NC_STACK_ypabact *gunObj = gun.gun_obj;
            if ( !gunObj ||
                 gunObj->IsDestroyed() ||
                 (gunObj->_status_flg & BACT_STFLAG_DEATH1) ||
                 gunObj->_status == BACT_STATUS_DEAD ||
                 gunObj->_energy <= 0 )
                continue;

            if ( !firstActive )
                firstActive = gunObj;

            if ( haveAttacker )
            {
                float d = (gunObj->_position - srcPos).length();
                if ( !best || d < bestDist )
                {
                    best = gunObj;
                    bestDist = d;
                }
            }
        }
    };

    if ( _bact_type == BACT_TYPES_ROBO )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(this);
        if ( robo )
            considerGuns(robo->GetGuns());
    }
    else
    {
        considerGuns(_unitGuns);
    }

    return (haveAttacker && best) ? best : firstActive;
}

void NC_STACK_ypabact::CopyTargetOf(NC_STACK_ypabact *unit)
{
    NC_STACK_ypabact *v6 = nullptr;

    _waypoints_count = 0;
    _m_cmdID = 0;
    _status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);

    int tgType;
    vec2d wTo;

    const int waypointCapacity = sizeof(unit->_waypoints) / sizeof(unit->_waypoints[0]);
    const bool hasValidWaypoints = (unit->_status_flg & BACT_STFLAG_WAYPOINT) &&
                                   unit->_waypoints_count > 0 &&
                                   unit->_waypoints_count <= waypointCapacity;

    if ( hasValidWaypoints )
    {
        if ( !unit->_m_cmdID )
        {
            int v9 = unit->_waypoints_count - 1;

            wTo = unit->_waypoints[v9].XZ();

            tgType = BACT_TGT_TYPE_CELL;
        }
        else
        {
            v6 = _world->FindBactByCmdOwn(unit->_m_cmdID, unit->_m_owner);

            if ( v6 )
                tgType = BACT_TGT_TYPE_UNIT;
            else
                tgType = BACT_TGT_TYPE_NONE;
        }
    }
    else
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            v6 = unit->_primT.pbact;
            tgType = BACT_TGT_TYPE_UNIT;
        }
        else if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            wTo = unit->_primTpos.XZ();
            tgType = BACT_TGT_TYPE_CELL;
        }
        else
            tgType = BACT_TGT_TYPE_NONE;
    }

    if ( tgType == BACT_TGT_TYPE_NONE )
    {
        tgType = BACT_TGT_TYPE_UNIT;
        v6 = unit;
    }

    if ( _bact_type != BACT_TYPES_TANK && _bact_type != BACT_TYPES_CAR )
    {
        setTarget_msg arg67;
        arg67.tgt_type = tgType;
        arg67.priority = 0;

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            arg67.tgt.pbact = v6;
        }
        else
        {
            arg67.tgt_pos.x = wTo.x;
            arg67.tgt_pos.z = wTo.y;
        }

        SetTarget(&arg67);
    }
    else
    {
        bact_arg124 arg125;

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            arg125.to = v6->_position.XZ();
        }
        else
        {
            arg125.to = wTo;
        }

        arg125.steps_cnt = 32;
        arg125.from = _position.XZ();
        arg125.field_12 = 1;

        SetPath(&arg125);

        if ( tgType == BACT_TGT_TYPE_UNIT )
        {
            _m_cmdID = v6->_commandID;
            _m_owner = v6->_owner;
        }
    }
}

size_t NC_STACK_ypabact::SetParameters(IDVList &stak)
{
    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_VIEWER:
                setBACT_viewer(val.Get<bool>());
                break;

            case BACT_ATT_INPUTTING:
                setBACT_inputting(val.Get<bool>());
                break;

            case BACT_ATT_EXACTCOLL:
                setBACT_exactCollisions ( val.Get<bool>() );
                break;

            case BACT_ATT_BACTCOLL:
                setBACT_bactCollisions ( val.Get<bool>() );
                break;

            case BACT_ATT_AIRCONST:
                setBACT_airconst(val.Get<int32_t>());
                break;

            case BACT_ATT_LANDINGONWAIT:
                setBACT_landingOnWait ( val.Get<bool>() );
                break;

            case BACT_ATT_YOURLS:
                setBACT_yourLastSeconds(val.Get<int32_t>());
                break;

            case BACT_ATT_VISPROT:
                SetVP( val.Get<NC_STACK_base *>());
                break;

            case BACT_ATT_AGGRESSION:
                setBACT_aggression(val.Get<int32_t>());
                break;

            case BACT_ATT_EXTRAVIEWER:
                setBACT_extraViewer ( val.Get<bool>() );
                break;

            case BACT_ATT_ALWAYSRENDER:
                setBACT_alwaysRender ( val.Get<bool>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void sub_481F94(NC_STACK_ypabact *bact)
{
    for (World::MissileList::iterator it = bact->_missiles_list.begin(); it != bact->_missiles_list.end(); )
    {
        NC_STACK_ypamissile *misl = *it;
        if ( misl->getBACT_yourLastSeconds() <= 0 )
        {
            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;

            misl->SetTarget(&arg67);

            misl->_parent = nullptr;

            misl->Release();

            it = bact->_missiles_list.erase(it);
        }
        else
            it++;
    }
}

void NC_STACK_ypabact::Update(update_msg *arg)
{
    ClearPlayerSprintPitchExtra();

    if ( _kidRef.IsListType(World::BLIST_CACHE) ) // Do not update units in dead list
        return;

    // Drop existing enemy locks before any class-specific AI can fire.
    // Projectiles already in flight and direct player control keep their behavior.
    if ( !(_oflags & BACT_OFLAG_USERINPT) && _bact_type != BACT_TYPES_MISSLE )
    {
        for ( int priority = 0; priority < 2; ++priority )
        {
            const int type = priority == 0 ? _primTtype : _secndTtype;
            NC_STACK_ypabact *target = priority == 0 ? _primT.pbact : _secndT.pbact;
            if ( type == BACT_TGT_TYPE_UNIT && target && target->_owner != _owner &&
                 target->IsIgnoredByAI() )
            {
                setTarget_msg clearTarget;
                clearTarget.tgt_type = BACT_TGT_TYPE_NONE;
                clearTarget.priority = priority;
                SetTarget(&clearTarget);
            }
        }
    }

    // Alternative View is transient player state. Any loss of its real runtime
    // prerequisites clears it immediately rather than leaving a dangling view.
    if ( _alternativeViewActive && !IsAlternativeViewAvailable() )
        ResetAlternativeView();

    // DEATH1 is permanent for this instance. A lethal event can occur while a
    // class-specific NORMAL/IDLE update is already on the stack; never let the
    // remainder of that old branch keep the logical corpse in a live status.
    if ( _status_flg & BACT_STFLAG_DEATH1 )
        _status = BACT_STATUS_DEAD;

    CrashDiag::ScopedActiveBact diagnosticBact(this, _gid, _bact_type, _owner,
                                                _status, _status_flg, _energy);

    static TF::TForm3D bact_cam;
    TF::Engine.SetViewPoint(&bact_cam);

    yw_130arg sect_info;
    sect_info.pos_x = _position.x;
    sect_info.pos_z = _position.z;

    if ( !_world->GetSectorInfo(&sect_info) )
    {
        FixBeyondTheWorld();

        sect_info.pos_x = _position.x;
        sect_info.pos_z = _position.z;

        _world->GetSectorInfo(&sect_info);
    }

    cellArea *oldcell = _pSector;

    _cellId = sect_info.CellId;


    _pSector = sect_info.pcell;

    if ( IsInvulnerableToDamage() && _energy <= 0 )
        _energy = _energy_max > 0 ? _energy_max : 1;

    if ( oldcell != sect_info.pcell ) // If cell changed
    {
        _cellRef.Detach();  //Remove unit from old cell
        _cellRef = _pSector->unitsList.push_back(this);  // Add unit to new cell
    }

    // Test if bact fall through sector
    if ( _pSector->height + 1000.0 < _position.y )
        FixSectorFall();

    NC_STACK_ypabact *roboHost = _world->getYW_userHostStation();

    if ( _pSector->PurposeType == cellArea::PT_GATEOPENED && _bact_type == BACT_TYPES_ROBO && this == roboHost )
        ((NC_STACK_yparobo *)roboHost)->ypabact_func65__sub0();

    if ( !(_status_flg & BACT_STFLAG_DEATH1) && _energy <= 0 && _bact_type != BACT_TYPES_MISSLE )
    {
        Die();

        if ( !IsDestroyed() )
        {
            setState_msg v38;
            v38.setFlags = 0;
            v38.unsetFlags = 0;
            v38.newStatus = BACT_STATUS_IDLE;

            SetState(&v38);
        }

        _status = BACT_STATUS_DEAD;
        _status_flg &= ~BACT_STFLAG_LAND;
    }

    _clock += arg->frameTime;
    ypabact_UpdateSpawnAtDeathProtection(this);

    // Keep vanilla heli landing physics exact, but gently consume the visual
    // difference left by its final ground-position correction.
    if ( (_isUnitGunChild || _isDummy) && _parent && _parent != this )
    {
        _heliLandingVisualOffsetY = _parent->_heliLandingVisualOffsetY;
    }
    else if ( _bact_type == BACT_TYPES_BACT && _heliLandingVisualOffsetY != 0.0f )
    {
        _heliLandingVisualOffsetY *= expf(-arg->frameTime / 60.0f);

        if ( fabs(_heliLandingVisualOffsetY) < 0.05f )
            _heliLandingVisualOffsetY = 0.0f;
    }

    const vec3d genesisStartPosition = _position;
    ResolveShapeMovement(_position, _rotation);
    const vec3d shapeOldPosition = _position;
    const mat3x3 shapeOldRotation = _rotation;
    UpdateActiveDebuff(arg);
    UpdateDamageFX(arg);
    UpdateDecorationFX(arg);
    UpdateCarrierSpawn(arg);
    UpdateProximityDefense(arg);
    UpdateArtilleryShell(arg);
    ResolveGenesisCompoundOverlap(arg->frameTime);
    UpdateKamikaze(arg);
    AI_layer1(arg);
    UpdateProgressiveWeaponFireRate(arg);

    // Collision damage or a Host Station death cascade may call Die() from
    // inside AI_layer1. Some legacy vehicle paths then write NORMAL/IDLE before
    // returning, so restore the death invariant before the shared update ends.
    if ( _status_flg & BACT_STFLAG_DEATH1 )
        _status = BACT_STATUS_DEAD;

    UpdateEnergyStatusFX(arg);
    UpdateLaser(arg); // OpenNeoUA custom: process this frame's laser fire request (must run after AI_layer1 firing)
    UpdateVerticalLaser(arg);
    UpdateConfiguredPush(arg);
    UpdateRecoilPush(arg);
    ResolveShapeMovement(shapeOldPosition, shapeOldRotation, arg->frameTime);
    if (_status == BACT_STATUS_CREATE &&
        (!_genesisGroundChecked || _position != genesisStartPosition))
        PlaceGenesisAboveTerrain();
    UpdateKamikaze(arg, &shapeOldPosition);
    UpdateUnitGuns(arg);
    UpdateGenesisHostExit();

    for( NC_STACK_ypamissile *misl : Utils::IterateListCopy<NC_STACK_ypamissile *>(_missiles_list))
        misl->Update(arg);

    sub_481F94(this);

    if ( _oflags & BACT_OFLAG_VIEWER )
    {
        if ( IsCockpitCameraActive() )
            bact_cam.Pos = GetCockpitCameraViewPosition();
        else if ( _oflags & BACT_OFLAG_EXTRAVIEW )
            bact_cam.Pos = _position + _rotation.Transpose().Transform(_viewer_position);
        else
            bact_cam.Pos = _position;

        if ( _bact_type == BACT_TYPES_BACT )
            bact_cam.Pos.y += _heliLandingVisualOffsetY;

        if ( IsAlternativeViewActive() )
            bact_cam.SclRot = GetAlternativeViewRotation();
        else if ( _oflags & BACT_OFLAG_EXTRAVIEW )
            bact_cam.SclRot = _viewer_rotation;
        else
            bact_cam.SclRot = _rotation;

        GFX::Engine.matrixAspectCorrection(bact_cam.SclRot, false);
    }

    _tForm.Pos = GetBodyPosition();
    if ( _heliLandingVisualOffsetY != 0.0f )
        _tForm.Pos.y += _heliLandingVisualOffsetY;

    if ( _status_flg & BACT_STFLAG_SCALE )
        _tForm.SclRot = _rotation.Transpose() * mat3x3::Scale( _scale );
    else
        _tForm.SclRot = _rotation.Transpose();

    int numbid = arg->units_count;

    arg->units_count = 0;

    /**
     * Because missiles can cause 'ModifyEnergy' and 'Die' methods of upper bact
     * in hierarchy Update->Update - it can remove all bacts from list in
     * iteration. So we just needs to get safe copy of list for modify without
     * worry of lists modify.
     **/
    for ( NC_STACK_ypabact *bnod : _world->SnapshotBacts(_kidList) )
    {
        bnod->Update(arg);

        arg->units_count++;
    }

    arg->units_count = numbid;

    _soundcarrier.Position = _position;

    if ( _oflags & BACT_OFLAG_VIEWER )
        _soundcarrier.Position += _rotation.AxisY() * 400.0;

    _soundcarrier.Vector = _fly_dir * _fly_dir_length;

    ypabact_ApplyDamagedSoundPitch(this);
    if ( _world && !_soundcarrier.Sounds.empty() && _soundcarrier.Sounds[0].PSample )
    {
        const float sprintPitchScale = _world->GetPlayerSprintPitchScale(this);
        if ( sprintPitchScale > 0.0f )
        {
            TSoundSource &sound = _soundcarrier.Sounds[0];
            const int sampleRate = sound.PSample->SampleRate;
            const int vanillaPitch = sound.Pitch;
            const int vanillaRate = sampleRate + vanillaPitch;
            const int legacyMaxRate = sampleRate > 44100 ? 192000 : 44100;
            const int audibleVanillaRate = std::max(2000, std::min(vanillaRate, legacyMaxRate));

            // Preserve the existing curve for loops that are naturally below
            // the legacy playback ceiling. Loops already saturated at 44.1 kHz
            // receive only sampleRate * sprintScale of extra headroom. This
            // prevents Fox/Wasp-style loops from jumping to an excessively high
            // pitch while keeping the smooth Weasel/Hornet response unchanged.
            const float desiredRate = audibleVanillaRate * (1.0f + sprintPitchScale);
            const float sprintRateCeiling = legacyMaxRate + sampleRate * sprintPitchScale;
            const int sprintRate = (int)floorf(std::min(desiredRate, sprintRateCeiling) + 0.5f);

            sound.Pitch = sprintRate - sampleRate;
            _playerSprintPitchExtra = sound.Pitch - vanillaPitch;
            sound.AllowExtendedRate = sprintRate > legacyMaxRate;
        }
    }
    BeforeSoundCarrierUpdate();

    // OpenNeoUA invisible: a still-cloaked stealth unit emits no loop/idle/engine/ambient
    // or status sounds. Keep the carrier hard-stopped instead of updating it; once the
    // unit reveals (after its first attack) the normal sound update resumes.
    if ( IsInvisibleUnrevealed() )
    {
        SFXEngine::SFXe.StopCarrier(&_soundcarrier);
        SFXEngine::SFXe.StopCarrier(&_mgun_soundcarrier);
    }
    else
    {
        SFXEngine::SFXe.UpdateSoundCarrier(&_soundcarrier);
        if ( !_mgun_soundcarrier.Sounds.empty() )
        {
            _mgun_soundcarrier.Position = _soundcarrier.Position;
            _mgun_soundcarrier.Vector = _soundcarrier.Vector;
            SFXEngine::SFXe.UpdateSoundCarrier(&_mgun_soundcarrier);
        }
        if ( _vehicle_fire_vp_end_time > 0 && _clock >= _vehicle_fire_vp_end_time &&
             !(_status_flg & BACT_STFLAG_FIRE) && _vp_active == 7 &&
             (_status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE) )
        {
            // IDLE starts the landing procedure before an air vehicle is
            // physically on the ground.  When a timed Vehicle fire VP expires in
            // that window, keep the animated NORMAL pose until LAND is real;
            // otherwise helicopters can descend with their WAIT rotor stopped.
            const bool useWaitVP =
                _status == BACT_STATUS_IDLE &&
                (!ypabact_IsAirVehicle(this) ||
                 !getBACT_landingOnWait() ||
                 (_status_flg & BACT_STFLAG_LAND));

            SetVP(useWaitVP ? _vp_wait : _vp_normal);
            _vp_active = useWaitVP ? 6 : 1;
            _vehicle_fire_vp_end_time = 0;
        }
        ypabact_UpdateStatusSoundCarrier(this, &_debuff_soundcarrier);
        ypabact_UpdateStatusSoundCarrier(this, &_player_launch_shake_carrier);
        ypabact_UpdateStatusSoundCarrier(this, &_mgun_recoil_shake_carrier);
        ypabact_UpdateStatusSoundCarrier(this, &_laser_launch_soundcarrier);
    }

    ypabact_UpdateMimicSoundCarrier(this);
}

bool ypabact_GetRandomFXSpawnCount(int configuredMin, int configuredMax, int &spawnCount)
{
    int countMin = std::max(0, std::min(configuredMin, 32));
    int countMax = std::max(0, std::min(configuredMax, 32));

    if ( countMin <= 0 || countMax <= 0 )
        return false;

    if ( countMax < countMin )
        std::swap(countMin, countMax);

    spawnCount = World::RandomIntRangeInclusive(countMin, countMax);
    return spawnCount > 0;
}

void NC_STACK_ypabact::Render(baseRender_msg *arg)
{
    // OpenNeoUA invisible: a still-cloaked stealth unit is never drawn in the 3D world.
    // Physics/control/collision keep running elsewhere; only the visual body is skipped.
    if ( IsInvisibleUnrevealed() )
        return;

    // OpenNeoUA Black Sect unit tint: owner-5 combat units use the configured render-only
    // tint. It deliberately overrides per-prototype vp_tint without mutating actor or
    // prototype state. Host Stations and projectiles remain untouched.
    World::TVisualTint effectiveTint =
        World::BlackSectTint::IsTintedUnit(this) ? World::BlackSectTint::Tint() : _vp_tint;

    // OpenNeoUA debuff target tint: RGB is an absolute target hue, so an active
    // debuff deliberately overrides any previous authored/Black-Sect hue instead of
    // multiplying into it. Alpha still composes multiplicatively. Prototype and
    // _vp_tint state remain untouched, so expiration restores the exact prior look.
    if ( _active_debuff.active && !_active_debuff.target_tint.IsNeutral() )
    {
        if ( _active_debuff.target_tint.ColorizesRGB() )
        {
            effectiveTint.r = _active_debuff.target_tint.r;
            effectiveTint.g = _active_debuff.target_tint.g;
            effectiveTint.b = _active_debuff.target_tint.b;
        }
        effectiveTint.a *= _active_debuff.target_tint.a;
        effectiveTint.Clamp();
    }

    auto shouldApplyVPScale = [this](NC_STACK_base *base)
    {
        if ( _vp_scale.x == 1.0 && _vp_scale.y == 1.0 && _vp_scale.z == 1.0 )
            return false;

        return ypabact_IsMainVPBase(this, base);
    };

    // OpenNeoUA custom visual tint: all authored visual-state bodies are eligible
    // (normal/fire/wait/dead/megadeth/genesis), independently from scale/spin rules.
    // RGB is rendered as a target hue while preserving source intensity; alpha remains
    // multiplicative. effectiveTint already includes Black Sect/mimic/debuff composition.
    auto shouldApplyVPTint = [this, &effectiveTint](NC_STACK_base *base)
    {
        if ( effectiveTint.IsNeutral() )
            return false;

        return ypabact_IsTintableVisualBase(this, base);
    };

    auto tintToGL = [](const World::TVisualTint &tint) -> GFX::TGLColor
    {
        return GFX::TGLColor(tint.r, tint.g, tint.b, tint.a);
    };

    auto applyRenderControls = [&](NC_STACK_base *base)
    {
        const bool mainBase = ypabact_IsMainVPBase(this, base);
        const bool tintBase = ypabact_IsTintableVisualBase(this, base);
        const bool applyTint = shouldApplyVPTint(base);
        const bool missileMain = _bact_type == BACT_TYPES_MISSLE && mainBase;

        if ( applyTint )
            arg->tint = tintToGL(effectiveTint);
        else
            arg->tint = GFX::TGLColor(1.0, 1.0, 1.0, 1.0);
        arg->colorizeTint = applyTint && effectiveTint.ColorizesRGB();

        // Shared render-only glow path used by both Buff and Elite presentation.
        // It brightens the existing model tint without adding world lights or
        // mutating authored materials. Zero intensity preserves the old path.
        auto applyGlow = [&](float intensity, const World::TVisualTint &tint,
                             float pulseSeconds)
        {
            if ( intensity <= 0.0f )
                return;

            float pulse = 1.0f;
            if ( pulseSeconds > 0.0f )
            {
                const double periodMs = (double)pulseSeconds * 1000.0;
                const double phase = fmod((double)std::max(_clock, 0), periodMs) / periodMs;
                pulse = (float)(0.5 - 0.5 * cos(phase * C_2PI));
            }

            const float strength = intensity * pulse * tint.a;
            arg->tint.r *= 1.0f + strength * tint.r;
            arg->tint.g *= 1.0f + strength * tint.g;
            arg->tint.b *= 1.0f + strength * tint.b;
        };

        if ( tintBase && _energy > 0 )
        {
            if ( HasActiveBuff() )
                applyGlow(_buff.glow_intensity, _buff.glow_tint,
                          _buff.glow_pulse_seconds);

            const ypabact_TEliteUnitConfig &eliteConfig = ypabact_GetEliteUnitConfig();
            applyGlow(GetEliteGlowIntensity(), eliteConfig.glowTint,
                      eliteConfig.glowPulseSeconds);
        }

        if ( missileMain )
        {
            arg->particleTint = tintToGL(_vp_trail_tint);
            arg->particleColorizeTint = _vp_trail_tint.ColorizesRGB();
            // Trail X/Y keep scaling each flat particle. Z is deliberately
            // repurposed as a lifetime multiplier so values below 1.0 shorten
            // the visible trail while 1.0 (or an absent key) stays vanilla.
            arg->particleScale = vec3d(_vp_trail_scale.x, _vp_trail_scale.y, 1.0f);
            arg->particleSpin = _vp_trail_spin_strength;
            arg->particleLifetimeScale = _vp_trail_scale.z;
        }
        else if ( tintBase )
        {
            // Particles emitted by any tinted visual state inherit the same target hue.
            arg->particleTint = arg->tint;
            arg->particleColorizeTint = arg->colorizeTint;
            arg->particleScale = mainBase && shouldApplyVPScale(base) ? _vp_scale : vec3d(1.0, 1.0, 1.0);
            arg->particleSpin = mainBase && ypabact_ShouldApplyVPSpin(this, base) ? _vp_spin_strength : vec3d(0.0, 0.0, 0.0);
            arg->particleLifetimeScale = 1.0f;
        }
        else
        {
            arg->particleTint = GFX::TGLColor(1.0, 1.0, 1.0, 1.0);
            arg->particleColorizeTint = false;
            arg->particleScale = vec3d(1.0, 1.0, 1.0);
            arg->particleSpin = vec3d(0.0, 0.0, 0.0);
            arg->particleLifetimeScale = 1.0f;
        }
    };

    if ( _current_vp )
    {
        if ( !(_status_flg & BACT_STFLAG_NORENDER) )
        {
            if ( !IsCockpitCameraSilhouetteHidden() &&
                 (!(_oflags & BACT_OFLAG_VIEWER) || _oflags & BACT_OFLAG_ALWAYSREND || ShouldRenderCockpitCameraBody()) )
            {
                _current_vp->Bas->TForm().Pos = _tForm.Pos;
                if ( ypabact_IsMainVPBase(this, _current_vp->Bas) &&
                     ypabact_ShouldRenderRecoilVisualOffset(this) )
                {
                    vec3d recoilVisualOffset;
                    ypabact_EvaluateRecoilVisual(this, &recoilVisualOffset, nullptr);
                    _current_vp->Bas->TForm().Pos += recoilVisualOffset;
                }
                _current_vp->Bas->TForm().SclRot = _tForm.SclRot;

                bool scaled = shouldApplyVPScale(_current_vp->Bas);
                if ( ypabact_ShouldApplyVPRotation(this, _current_vp->Bas) )
                    _current_vp->Bas->TForm().SclRot *= ypabact_BuildVPRotationMatrix(_vp_rotation);

                const float visualRecoilPitch = ypabact_GetRecoilVisualPitch(this);
                if ( visualRecoilPitch != 0.0f && ypabact_IsMainVPBase(this, _current_vp->Bas) )
                    _current_vp->Bas->TForm().SclRot *= mat3x3::RotateX(visualRecoilPitch);

                ypabact_ApplyProjectileVisualMotion(this, _current_vp->Bas);

                if ( ypabact_ShouldApplyVPSpin(this, _current_vp->Bas) )
                    _current_vp->Bas->TForm().SclRot *= World::Spin::BuildMatrix(_vp_spin_strength, _clock);

                if ( scaled )
                    _current_vp->Bas->TForm().SclRot *= mat3x3::Scale(_vp_scale);

                GFX::TGLColor oldTint = arg->tint;
                GFX::TGLColor oldParticleTint = arg->particleTint;
                bool oldColorizeTint = arg->colorizeTint;
                bool oldParticleColorizeTint = arg->particleColorizeTint;
                vec3d oldParticleScale = arg->particleScale;
                vec3d oldParticleSpin = arg->particleSpin;
                float oldParticleLifetimeScale = arg->particleLifetimeScale;
                applyRenderControls(_current_vp->Bas);

                // The genesis VP is the faction plasma visual wherever the
                // existing runtime uses it (creation, death or other genesis FX).
                // Tint only this render call; shared VP prototypes stay untouched.
                if ( _current_vp->Bas == _vp_genesis )
                {
                    GFX::TGLColor plasmaTint;
                    if ( ypabact_GetPlasmaFactionTint(this, &plasmaTint) )
                    {
                        arg->tint = plasmaTint;
                        arg->particleTint = plasmaTint;
                        arg->colorizeTint = true;
                        arg->particleColorizeTint = true;
                    }
                }

                _current_vp->Bas->Render(arg, _current_vp);
                arg->tint = oldTint;
                arg->particleTint = oldParticleTint;
                arg->colorizeTint = oldColorizeTint;
                arg->particleColorizeTint = oldParticleColorizeTint;
                arg->particleScale = oldParticleScale;
                arg->particleSpin = oldParticleSpin;
                arg->particleLifetimeScale = oldParticleLifetimeScale;
            }
        }
    }

    for (int i = 0; i < 3; i++)
    {
        extra_vproto *bd = &_vp_extra[i];

        if ( bd->vp )
        {
            if ( bd->flags & EVPROTO_FLAG_ACTIVE )
            {
                bd->vp->Bas->TForm().Pos = bd->pos + (GetBodyPosition() - _position);
                if ( _bact_type == BACT_TYPES_BACT )
                    bd->vp->Bas->TForm().Pos.y += _heliLandingVisualOffsetY;

                if ( bd->flags & EVPROTO_FLAG_SCALE )
                    bd->vp->Bas->TForm().SclRot = bd->rotate.Transpose() * mat3x3::Scale( vec3d(bd->scale, bd->scale, bd->scale) );
                else
                    bd->vp->Bas->TForm().SclRot = bd->rotate.Transpose();

                bool scaled = shouldApplyVPScale(bd->vp->Bas);
                if ( ypabact_ShouldApplyVPRotation(this, bd->vp->Bas) )
                    bd->vp->Bas->TForm().SclRot *= ypabact_BuildVPRotationMatrix(_vp_rotation);

                ypabact_ApplyProjectileVisualMotion(this, bd->vp->Bas);

                if ( ypabact_ShouldApplyVPSpin(this, bd->vp->Bas) )
                    bd->vp->Bas->TForm().SclRot *= World::Spin::BuildMatrix(_vp_spin_strength, _clock);

                if ( scaled )
                    bd->vp->Bas->TForm().SclRot *= mat3x3::Scale(_vp_scale);

                GFX::TGLColor oldTint = arg->tint;
                GFX::TGLColor oldParticleTint = arg->particleTint;
                bool oldColorizeTint = arg->colorizeTint;
                bool oldParticleColorizeTint = arg->particleColorizeTint;
                vec3d oldParticleScale = arg->particleScale;
                vec3d oldParticleSpin = arg->particleSpin;
                float oldParticleLifetimeScale = arg->particleLifetimeScale;
                applyRenderControls(bd->vp->Bas);

                // Apply the same faction tint to every extra instance that
                // reuses the genesis VP. No status/slot-specific plasma path is
                // needed because the VP identity itself is the single condition.
                if ( bd->vp->Bas == _vp_genesis )
                {
                    GFX::TGLColor plasmaTint;
                    if ( ypabact_GetPlasmaFactionTint(this, &plasmaTint) )
                    {
                        arg->tint = plasmaTint;
                        arg->particleTint = plasmaTint;
                        arg->colorizeTint = true;
                        arg->particleColorizeTint = true;
                    }
                }

                bd->vp->Bas->Render(arg, bd->vp);
                arg->tint = oldTint;
                arg->particleTint = oldParticleTint;
                arg->colorizeTint = oldColorizeTint;
                arg->particleColorizeTint = oldParticleColorizeTint;
                arg->particleScale = oldParticleScale;
                arg->particleSpin = oldParticleSpin;
                arg->particleLifetimeScale = oldParticleLifetimeScale;
            }
        }
    }

    // Continuous laser bodies are queued from the authoritative runtime beam
    // endpoints. The helper keeps the legacy VP path available as fallback.
    ypabact_RenderLaserMeshes(this, arg);
}

void NC_STACK_ypabact::SetTarget(setTarget_msg *arg)
{
    _assess_time = 0;
    yw_130arg arg130;

    constexpr float CurSectrLength = World::CVSectorLength + 10.0;

    if ( _status_flg & BACT_STFLAG_DEATH1 && arg->tgt_type == BACT_TGT_TYPE_UNIT )
    {
        ypa_log_out("ALARM!!! bact-settarget auf logische Leiche owner %d, class %d, prio %d\n", _owner, _bact_type, arg->priority);
    }
    else if ( arg->priority )
    {
        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
            _secndT.pbact->DeleteAttacker(this, 1);

        switch ( arg->tgt_type )
        {
        case BACT_TGT_TYPE_CELL:
        case BACT_TGT_TYPE_CELL_IND:
            _secndTtype = BACT_TGT_TYPE_CELL;

            arg130.pos_x = arg->tgt_pos.x;
            arg130.pos_z = arg->tgt_pos.z;

            if ( arg130.pos_x < CurSectrLength )
                arg130.pos_x = CurSectrLength;

            if ( arg130.pos_x > _wrldSize.x - CurSectrLength )
                arg130.pos_x = _wrldSize.x - CurSectrLength;

            if ( arg130.pos_z > -CurSectrLength )
                arg130.pos_z = -CurSectrLength;

            if ( arg130.pos_z < _wrldSize.y + CurSectrLength )
                arg130.pos_z = _wrldSize.y + CurSectrLength;

            if ( _world->GetSectorInfo(&arg130) )
            {
                _secndT.pcell = arg130.pcell;
                _sencdTpos.x = arg130.pos_x;
                _sencdTpos.z = arg130.pos_z;
                _sencdTpos.y = arg130.pcell->height;
            }
            else
            {
                _secndTtype = BACT_TGT_TYPE_NONE;
                _secndT.pcell = nullptr;
            }
            break;

        case BACT_TGT_TYPE_UNIT:
        case BACT_TGT_TYPE_UNIT_IND:
            _secndT.pbact = arg->tgt.pbact;
            _secndTtype = BACT_TGT_TYPE_UNIT;

            if ( _secndT.pbact )
            {
                if ( _secndT.pbact->_status_flg & BACT_STFLAG_DEATH1 )
                {
                    ypa_log_out("totes vehicle als nebenziel, owner %d, class %d\n", arg->tgt.pbact->_owner, arg->tgt.pbact->_bact_type);
                    _secndTtype = BACT_TGT_TYPE_NONE;
                }
                else
                {
                    _sencdTpos = _secndT.pbact->_position;
                    _secndT.pbact->AddAttacker(this, 1);
                }
            }
            else
            {
                ypa_log_out("Yppsn\n");
                _secndTtype = BACT_TGT_TYPE_NONE;
            }
            break;

        case BACT_TGT_TYPE_FRMT:
            _secndTtype = BACT_TGT_TYPE_FRMT;
            _sencdTpos = arg->tgt_pos;
            break;

        case BACT_TGT_TYPE_NONE:
            _secndT.pbact = nullptr;
            _secndTtype = BACT_TGT_TYPE_NONE;
            break;

        default:
            _secndTtype = arg->tgt_type;
            break;
        }
    }
    else
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
            _primT.pbact->DeleteAttacker(this, 0);

        switch ( arg->tgt_type )
        {
        case BACT_TGT_TYPE_CELL:
        case BACT_TGT_TYPE_CELL_IND:
            _primT_cmdID = 0;
            _primTtype = BACT_TGT_TYPE_CELL;

            arg130.pos_x = arg->tgt_pos.x;
            arg130.pos_z = arg->tgt_pos.z;

            if ( arg130.pos_x < CurSectrLength )
                arg130.pos_x = CurSectrLength;

            if ( arg130.pos_x > _wrldSize.x - CurSectrLength )
                arg130.pos_x = _wrldSize.x - CurSectrLength;

            if ( arg130.pos_z > -CurSectrLength )
                arg130.pos_z = -CurSectrLength;

            if ( arg130.pos_z < _wrldSize.y + CurSectrLength )
                arg130.pos_z = _wrldSize.y + CurSectrLength;

            if ( _world->GetSectorInfo(&arg130) )
            {
                _primT.pcell = arg130.pcell;
                _primTpos.x = arg130.pos_x;
                _primTpos.z = arg130.pos_z;
                _primTpos.y = arg130.pcell->height;
            }
            else
            {
                _primTtype = BACT_TGT_TYPE_NONE;
                _primT.pcell = nullptr;
            }
            break;

        case BACT_TGT_TYPE_UNIT:
        case BACT_TGT_TYPE_UNIT_IND:
            _primT.pbact = arg->tgt.pbact;
            _primTtype = BACT_TGT_TYPE_UNIT;

            if ( _primT.pbact )
            {
                if ( _primT.pbact->_status_flg & BACT_STFLAG_DEATH1 )
                {
                    ypa_log_out("totes vehicle als hauptziel, owner %d, class %d - ich bin class %d\n", arg->tgt.pbact->_owner, arg->tgt.pbact->_bact_type, _bact_type);
                    _primTtype = BACT_TGT_TYPE_NONE;
                    return;
                }

                _primTpos = _primT.pbact->_position;

                _primT.pbact->AddAttacker(this, 0);

                _primT_cmdID = arg->tgt.pbact->_commandID;
            }
            else
            {
                ypa_log_out("PrimT. without a pointer\n");
                _primTtype = BACT_TGT_TYPE_NONE;
            }
            break;

        case BACT_TGT_TYPE_FRMT:
            _primTtype = BACT_TGT_TYPE_FRMT;
            _primT_cmdID = 0;
            _primTpos = arg->tgt_pos;
            break;

        case BACT_TGT_TYPE_NONE:
            _primT.pbact = nullptr;
            _waypoints_count = 0;
            _primTtype = BACT_TGT_TYPE_NONE;
            _status_flg &= ~BACT_STFLAG_WAYPOINT;
            break;

        case BACT_TGT_TYPE_DRCT:
            _target_dir = arg->tgt_pos;
            _primTtype = BACT_TGT_TYPE_DRCT;
            _primT.pbact = nullptr;
            _primT_cmdID = 0;
            break;

        default:
            _primTtype = arg->tgt_type;
            break;
        }

        if ( arg->tgt_type == BACT_TGT_TYPE_CELL || arg->tgt_type == BACT_TGT_TYPE_UNIT )
        {
            for ( NC_STACK_ypabact* &node : _kidList )
            {
                if ( node->_status != BACT_STATUS_DEAD)
                {
                    node->SetTarget(arg);
                    if ( !(_status_flg & BACT_STFLAG_WAYPOINT)  )
                        node->_status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
                }
            }
        }
    }
}

void NC_STACK_ypabact::AddSubject(NC_STACK_ypabact *kid)
{
    newMaster_msg arg73;

    arg73.bact = this;
    arg73.list = &_kidList;

    kid->SetNewMaster(&arg73);
}

void NC_STACK_ypabact::SetNewMaster(newMaster_msg *arg)
{
    _kidRef.Detach();

    _kidRef = arg->list->push_front(this);

    _parent = arg->bact;
}

void NC_STACK_ypabact::SetState(setState_msg *arg)
{
    const bool advancesDeath =
        arg->newStatus == BACT_STATUS_DEAD ||
        (arg->setFlags & BACT_STFLAG_DEATH2);

    // Once Die() has started, live-state transitions from an already-running
    // vehicle update must not replace the death VP or restart its loop sounds.
    if ( (_status_flg & BACT_STFLAG_DEATH1) && !advancesDeath )
        return;

    if ( IsInvulnerableToDamage() && arg->newStatus == BACT_STATUS_DEAD )
    {
        if ( _energy <= 0 )
            _energy = _energy_max > 0 ? _energy_max : 1;

        return;
    }

    if ( (_bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR) && arg->newStatus == BACT_STATUS_DEAD )
    {
        setState_msg newarg;
        newarg.unsetFlags = 0;
        newarg.newStatus = BACT_STATUS_NOPE;
        newarg.setFlags = BACT_STFLAG_DEATH2;

        SetState(&newarg);
    }
    else
    {
        int v6 = SetStateInternal(arg);

        if ( _world->_isNetGame )
        {
            if ( v6 && _owner && _bact_type != BACT_TYPES_MISSLE )
            {
                uamessage_setState ssMsg;
                ssMsg.msgID = UAMSG_SETSTATE;
                ssMsg.owner = _owner;
                ssMsg.id = _gid;
                ssMsg.newStatus = arg->newStatus;
                ssMsg.setFlags = arg->setFlags;
                ssMsg.unsetFlags = arg->unsetFlags;
                ssMsg.classID = _bact_type;

                _world->NetBroadcastMessage(&ssMsg, sizeof(ssMsg), true);
            }
        }
    }
}

void NC_STACK_ypabact::ypabact_func89(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","ypabact_func89");
}

void NC_STACK_ypabact::ypabact_func93(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","ypabact_func93");
}

void NC_STACK_ypabact::ypabact_func95(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","ypabact_func95");
}

// Reset
void NC_STACK_ypabact::Renew()
{
    _debugIgnoredByAI = false;
    if (_world && _world->_collisionScene) _world->_collisionScene->Forget(this);
    _collisionShape.reset();
    _disableCollision = false;
    _weapon_use_vehicle_fire = false;
    _disable_genesis = false;
    _shapeCollisionDamageStamp = -1;
    ClearPlayerSprintPitchExtra();
    if ( _inlinePhysicalFX )
    {
        SFXEngine::SFXe.StopCarrier(&_soundcarrier);
        _soundcarrier.Clear();
        _inlinePhysicalFX.reset();
    }
    _inlinePhysicalImpactDamageSpent = false;
    _inlinePhysicalExplodeFXStarted = false;
    _inlinePhysicalLifetimeMs = 0;

    _oflags = BACT_OFLAG_EXACTCOLL;
    _status_flg = 0;
    _host_station = nullptr;
    _isGenesisProduced = false;
    _genesisExitPending = false;
    _genesisGroundChecked = false;
    _yls_time = 3000;
    _primTtype = BACT_TGT_TYPE_NONE;

    _secndTtype = BACT_TGT_TYPE_NONE;
    _primT_cmdID = 0;

    _wrldSectors = _world->GetMapSize();

    _wrldSize = World::SectorIDToPos2( _wrldSectors );

    _commandID = 0;
    _mimic_disguise_vehicleID = 0;
    _buff = World::TVehicleBuffConfig();
    _buff_deflect_charges_max = 0;
    _killer = nullptr;
    _sessionKillMarks = 0;
    _sessionKillsTowardMedal = 0;
    _brkfr_time = 0;
    _brkfr_time2 = 0;
    _mpos.x = 0;
    _mpos.y = 0;
    _mpos.z = 0;
    _handbrakeHeld = false;
    _gun_leftright = 0.0;
    _scale_time = 0;
    _clock = 0;
    _AI_time1 = 0;
    _AI_time2 = 0;
    _search_time1 = 0;
    _search_time2 = 0;
    _slider_time = 0;
    _mgun_time = 0;
    _mgun_energy_cost_time = 0;
    _weapon_time = 0;
    ResetProgressiveWeaponFireRate();
    _extra_weapons = {0, 0, 0};
    _weapon_player_switch_mode = World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE;
    _weapon_ai_switch_mode = World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
    _weapon_slot_index = 0;
    _current_weapon_id = -1;
    _current_weapon_source_slot = 0;
    _userHomingPrimaryTargetGid = 0;
    _userHomingTargetCycleRequested = false;
    _alternativeViewActive = false;
    _mgun_set = false;
    _mgun_shot_time = 0;
    _mgun_shot_time_user = 0;
    _mgun_vp_dead = 0;
    _mgun_vp_megadeth = 0;
    _mgun_3ds_dead.clear();
    _mgun_3ds_megadeth.clear();
    _mgun_base_dead.clear();
    _mgun_base_megadeth.clear();
    _mgun_energy = 0.0;
    _mgun_angle = 0.0;
    _mgun_energy_set = false;
    _cockpit_mgun_camera_recoil = 0.0f;
    _mgun_angle_set = false;
    _mgun_soundcarrier.Clear();
    _mimic_soundcarrier.Clear();
    _mgun_sound_index = 0;
    _vehicle_fire_vp_end_time = 0;
    _cockpit_camera_offset = vec3d(0.0, 0.0, 0.0);
    _cockpit_gun_camera_recoil = 0.0f;
    _cockpit_camera_hide_vehicle = false;
    _mgun_decal_enable = false;
    _mgun_decal = World::TChainFXConfig();
    ResetFeatureRuntimeState();
    _newtarget_time = 0;
    _assess_time = 0;
    _scale_pos = 0;
    _scale_delay = 0;
    _beam_time = 0;
    _energy_time = 0;
    _recoilVisualStartOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualPeakOffset = vec3d(0.0, 0.0, 0.0);
    _recoilVisualStartPitch = 0.0f;
    _recoilVisualPeakPitch = 0.0f;
    _recoilVisualPhaseStartTime = 0;
    _recoilVisualKickEndTime = 0;
    _recoilVisualHoldEndTime = 0;
    _recoilVisualReturnEndTime = 0;
    _heliLandingVisualOffsetY = 0.0f;
    _recoilAiRecoveryEndTime = 0;
    _recoilPlayerRecoveryEndTime = 0;
    _recoilPushVel = vec3d(0.0, 0.0, 0.0);
    _blastPushVel = vec3d(0.0, 0.0, 0.0);
    _fallDamageAirborne = false;
    _fallDamageConsumed = false;
    _deinitInProgress = false;
    ypabact_ResetDamagedFX(this);
    _regen_fx_next_time = 0;
    _drain_fx_next_time = 0;
    _energy_visual_state_time = -1;
    _energy_visual_state = UNIT_ENERGY_VISUAL_NONE;
    ClearActiveDebuff();
    _fe_time = -45000;
    _salve_counter = 0;
    _kill_after_shot = 0;
    _suicide_handoff_wait_fire_release = false;

    Common::DeleteAndNull(&_current_vp);

    _vp_active = 0;
    _volume = 0; //_soundcarrier.Sounds[0].Volume;

    _m_cmdID = 0;
    _gun_angle_user = _gun_angle;
    _oflags |= BACT_OFLAG_LANDONWAIT;

    for (World::DestFX &x : _destroyFX)
        x = World::DestFX();

    _extDestroyFX.clear();
    _chainFX.clear();

    for (extra_vproto &vp : _vp_extra)
        vp = extra_vproto();

    _current_waypoint = 0;

    _attackersList.clear();
    _kidList.clear();
    _missiles_list.clear();
}

void NC_STACK_ypabact::ypabact_func98(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","ypabact_func98");
}

void NC_STACK_ypabact::CreationTimeUpdate(update_msg *arg)
{
    if (!_genesisGroundChecked) PlaceGenesisAboveTerrain();
    if (ypabact_IsGenesisSeparationVehicle(this) && _host_station)
        _genesisExitPending = true;
    // disable_genesis: no scale-in, the creation phase is over on the first update.
    if ( _disable_genesis )
        _scale_time = 0;
    _scale_time -= arg->frameTime;

    float v30 = arg->frameTime / 1000.0;

    if ( _scale_time > 0 )
    {
        _status_flg |= BACT_STFLAG_SCALE;

        if ( _scale_time < 0 )
            _scale = vec3d(1.0);
        else
            _scale = vec3d( GenesisScaleCurve / ((float)_scale_time / 1000.0 + GenesisScaleCurve) + GenesisScaleBase );

        _rotation = mat3x3::RotateY( 2.5 / _scale.x * v30 ) * _rotation;
    }
    else
    {
        setState_msg v25;
        v25.newStatus = BACT_STATUS_NORMAL;
        v25.setFlags = 0;
        v25.unsetFlags = 0;

        SetState(&v25);

        _status_flg &= ~BACT_STFLAG_SCALE;

        bact_arg80 v24;

        v24.pos = _position;
        v24.field_C = 0;

        SetPosition(&v24);

        NC_STACK_ypabact *a4 = _world->getYW_userHostStation();

        if ( _host_station == a4 )
        {

            if ( IsParentMyRobo() )
            {
                robo_arg134 v23;
                v23.unit = this;
                v23.field_4 = 14;
                v23.field_8 = 0;
                v23.field_C = 0;
                v23.field_10 = 0;
                v23.field_14 = 26;

                _host_station->placeMessage(&v23);
            }
        }

        if ( _host_station )
        {
            if ( _bact_type != BACT_TYPES_GUN )
            {
                _fly_dir = v24.pos - _host_station->_position;

                float fly_len = _fly_dir.length();

                if ( fly_len > 0.001 )
                    _fly_dir /= fly_len;

                _fly_dir_length = 20.0;
            }
        }
    }
}

void NC_STACK_ypabact::MarkSectorsForView()
{
    /* Missle does not have kids, if else it's a BUG or must be another missle */
    if ( _bact_type == BACT_TYPES_MISSLE )
        return;

    /* Unit already dead also must do not have any kids */
    if ( _status == BACT_STATUS_DEAD || _status == BACT_STATUS_CREATE )
        return;

    if ( !_parent || _cellId != _parent->_cellId ||
        (_radar > _parent->_radar || _unhideRadar > _parent->_unhideRadar) )
    {
        if ( _owner < 8 )
        {
            for (int i = -_radar; i <= _radar; i++)
            {
                int yy = _cellId.y + i;

                if ( _radar == 1 )
                {
                    if ( yy > 0 && yy < _wrldSectors.y - 1 )
                    {
                        if (_unhideRadar > 0)
                        {
                            if ( _cellId.x > 1 )
                            {
                                _world->SectorAt(_cellId.x - 1, yy).AddToViewMask(_owner);
                                _world->SectorAt(_cellId.x - 1, yy).AddUnhideMask(_owner);
                            }

                            _world->SectorAt(_cellId.x, yy).AddToViewMask(_owner);
                            _world->SectorAt(_cellId.x, yy).AddUnhideMask(_owner);

                            if ( _cellId.x + 1 < _wrldSectors.x - 1 )
                            {
                                _world->SectorAt(_cellId.x + 1, yy).AddToViewMask(_owner);
                                _world->SectorAt(_cellId.x + 1, yy).AddUnhideMask(_owner);
                            }
                        }
                        else
                        {
                            if ( _cellId.x > 1 )
                                _world->SectorAt(_cellId.x - 1, yy).AddToViewMask(_owner);

                            _world->SectorAt(_cellId.x, yy).AddToViewMask(_owner);

                            if ( _cellId.x + 1 < _wrldSectors.x - 1 )
                                _world->SectorAt(_cellId.x + 1, yy).AddToViewMask(_owner);
                        }
                    }
                }
                else
                {
                    float vtmp = POW2((float)_radar) - POW2((float)i);

                    if (vtmp < 0.0)
                        vtmp = 0.0;

                    int tmp = dround( sqrt(vtmp) );

                    if (_unhideRadar > 0 && Common::ABS(i) < _unhideRadar)
                    {
                        for (int j = -tmp; j <= tmp; j++)
                        {
                            Common::Point d(_cellId.x + j, yy);

                            if ( _world->IsGamePlaySector(d) )
                            {
                                _world->SectorAt(d).AddToViewMask(_owner);
                                if (Common::ABS(j) < _unhideRadar)
                                    _world->SectorAt(d).AddUnhideMask(_owner);
                            }
                        }
                    }
                    else
                    {
                        for (int j = -tmp; j <= tmp; j++)
                        {
                            Common::Point d(_cellId.x + j, yy);

                            if ( _world->IsGamePlaySector(d) )
                                _world->SectorAt(d).AddToViewMask(_owner);
                        }
                    }
                }
            }
        }
    }

    for( NC_STACK_ypabact* &kid : _kidList )
        kid->MarkSectorsForView();
}

void NC_STACK_ypabact::ypabact_func103(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","ypabact_func103");
}

void NC_STACK_ypabact::HandleVisChildrens(int *arg)
{
    NC_STACK_base *vps[] {
    _vp_normal,
    _vp_dead,
    _vp_fire,
    _vp_genesis,
    _vp_wait,
    _vp_megadeth};

    for ( NC_STACK_base *vp : vps )
    {
        for( NC_STACK_base *kd : vp->GetKidList())
        {
            if ( *arg == 1 )
            {
                kd->SetParentFollow(true);

                kd->SetPosition( kd->GetPos() - _position );
            }
            else if ( *arg == 2 )
            {
                kd->SetParentFollow(true);

                kd->SetPosition( kd->GetPos() + _position );
            }
        }
    }
}

void NC_STACK_ypabact::BeamingTimeUpdate(update_msg *arg)
{
    float v14 = 0.66;

    if ( _scale_delay <= 0 )
    {
        if ( _scale_time >= 1980.0 )
        {
            if ( _scale_time >= 3000 )
            {
                _world->ypaworld_func168(this);

                _status_flg |= BACT_STFLAG_CLEAN;

                Die();

                if ( _oflags & BACT_OFLAG_USERINPT )
                    _status_flg |= BACT_STFLAG_NORENDER;
                else
                    Release();

                _status_flg &= ~BACT_STFLAG_SCALE;
            }
            else
            {
                _status_flg |= BACT_STFLAG_SCALE;

                _scale = vec3d(1.0, 30.0, 1.0) - vec3d::OY( (_scale_time - 1980.0) * 30.0 / 1020.0 );
            }
        }
        else
        {
            if ( GetVP() != _vp_genesis )
            {
                setState_msg arg78;
                arg78.newStatus = BACT_STATUS_BEAM;
                arg78.setFlags = 0;
                arg78.unsetFlags = 0;

                SetState(&arg78);
            }

            _status_flg |= BACT_STFLAG_SCALE;

            _scale = vec3d(1.0, 0.0, 1.0) + vec3d::OY( (30 * _scale_time)/ (v14 * 3000.0) );
        }

        _scale_time += arg->frameTime;
    }
    else
    {
        _scale_delay -= arg->frameTime;
    }
}

void ypabact_NetUpdate_VPHACKS(NC_STACK_ypabact *bact, update_msg *upd)
{
    if ( bact->_vp_extra_mode == 1 )
    {
        int engy = bact->GetPlasmaDurationMs();

        sb_0x4874c4(bact, engy, upd->frameTime, 0.75);
        bact->_scale_time -= upd->frameTime;

        if ( bact->_scale_time < 0 )
            bact->_vp_extra[0].SetVP((NC_STACK_base::Instance *)nullptr);
    }

    if ( bact->_vp_extra_mode == 2 )
    {
        NC_STACK_yparobo *roboo = dynamic_cast<NC_STACK_yparobo *>(bact);

        if (roboo)
        {
            roboo->_roboBeamTimePre -= upd->frameTime;
            if ( roboo->_roboBeamTimePre <= 0 )
            {
                roboo->_roboBeamTimePre = 0;
                SFXEngine::SFXe.startSound(&bact->_soundcarrier, 10);

                roboo->_roboState &= ~NC_STACK_yparobo::ROBOSTATE_MOVE;
                bact->_vp_extra[0].flags = 0;
                bact->_vp_extra[1].flags = 0;
            }
            else
            {
                if ( roboo->_roboBeamFXTime <= 0 )
                {
                    if ( bact->_vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
                    {
                        roboo->_roboBeamFXTime = roboo->_roboBeamTimePre / 10;
                        bact->_vp_extra[0].flags &= ~EVPROTO_FLAG_ACTIVE;
                    }
                    else
                    {
                        roboo->_roboBeamFXTime = (1500 - roboo->_roboBeamTimePre) / 10;
                        bact->_vp_extra[0].pos = bact->_position;
                        bact->_vp_extra[0].rotate = bact->_rotation;;
                        bact->_vp_extra[0].flags = 3;
                        bact->_vp_extra[0].scale = 1.25;
                        bact->_vp_extra[0].SetVP(bact->_vp_genesis);
                    }

                    if ( roboo->_vp_extra[1].flags & EVPROTO_FLAG_ACTIVE )
                    {
                        roboo->_roboBeamFXTime = roboo->_roboBeamTimePre / 10;
                        bact->_vp_extra[1].flags &= ~EVPROTO_FLAG_ACTIVE;
                    }
                    else
                    {
                        roboo->_roboBeamFXTime = (1500 - roboo->_roboBeamTimePre) / 10;
                        bact->_vp_extra[1].pos = roboo->_roboBeamPos;
                        bact->_vp_extra[1].rotate = bact->_rotation;
                        bact->_vp_extra[1].flags = 1;
                        bact->_vp_extra[1].SetVP(bact->_vp_genesis);
                    }
                }
                roboo->_roboBeamFXTime -= upd->frameTime;
            }

        }
    }
}

void NC_STACK_ypabact::NetUpdate(update_msg *upd)
{
    ypabact_NetUpdate_VPHACKS(this, upd);

    yw_130arg arg130;
    arg130.pos_x = _position.x;
    arg130.pos_z = _position.z;
    if ( !_world->GetSectorInfo(&arg130) )
    {
        FixBeyondTheWorld();

        arg130.pos_x = _position.x;
        arg130.pos_z = _position.z;
        _world->GetSectorInfo(&arg130);
    }

    cellArea *oldSect = _pSector;

    _cellId = arg130.CellId;
    _pSector = arg130.pcell;

    if ( oldSect != arg130.pcell )
    {
        _cellRef.Detach();
        _cellRef = _pSector->unitsList.push_back(this);
    }

    _clock += upd->frameTime;
    ypabact_UpdateSpawnAtDeathProtection(this);

    UpdateActiveDebuff(upd);
    UpdateDamageFX(upd);
    UpdateDecorationFX(upd);
    UpdateEnergyStatusFX(upd);

    ypabact_func117(upd);

    for ( NC_STACK_ypamissile* misl : Utils::IterateListCopy<NC_STACK_ypamissile *>(_missiles_list) )
    {
        misl->SetLauncherBact(this);
        misl->Update(upd);
    }

    sub_481F94(this);

    _tForm.Pos = _position;

    if ( _status_flg & BACT_STFLAG_SCALE )
        _tForm.SclRot = _rotation.Transpose() * mat3x3::Scale(_scale);
    else
        _tForm.SclRot = _rotation.Transpose();

    ypabact_ApplyDamagedSoundPitch(this);

    int units_cnt = upd->units_count;

    upd->units_count = 0;

    for (NC_STACK_ypabact* bct : _kidList.safe_iter())
    {
        bct->NetUpdate(upd);
        upd->units_count++;
    }

    upd->units_count = units_cnt;

    _soundcarrier.Position = _position;
    _soundcarrier.Vector = _fly_dir * _fly_dir_length;
    BeforeSoundCarrierUpdate();

    SFXEngine::SFXe.UpdateSoundCarrier(&_soundcarrier);
    ypabact_UpdateStatusSoundCarrier(this, &_debuff_soundcarrier);
    ypabact_UpdateStatusSoundCarrier(this, &_player_launch_shake_carrier);
    ypabact_UpdateStatusSoundCarrier(this, &_laser_launch_soundcarrier);
    ypabact_UpdateStatusSoundCarrier(this, &_mgun_recoil_shake_carrier);
}

void NC_STACK_ypabact::ypabact_func117(update_msg *upd)
{
    if (_world->_netInterpolate)
        ypabact_func122(upd);
    else
        ypabact_func123(upd);
}

void NC_STACK_ypabact::Release()
{
    if ( _owner )
    {
        if ( _world->_isNetGame )
        {
            if ( _bact_type != BACT_TYPES_MISSLE )
            {
                uamessage_destroyVhcl destrMsg;

                destrMsg.msgID = UAMSG_DESTROYVHCL;
                destrMsg.owner = _owner;
                destrMsg.id = _gid;
                destrMsg.type = _bact_type;

                _world->NetBroadcastMessage(&destrMsg, sizeof(destrMsg), true);
            }
        }
    }

    _world->ypaworld_func144(this);
}

size_t NC_STACK_ypabact::SetStateInternal(setState_msg *arg)
{
    int result = 0;

    if ( arg->newStatus )
        _status = arg->newStatus;

    if ( arg->setFlags )
        _status_flg |= arg->setFlags;

    if ( arg->unsetFlags )
        _status_flg &= ~arg->unsetFlags;

    // Timed Vehicle vp_fire visuals (MGUN pulses and Weapon-side requested
    // fire poses) do not rely on BACT_STFLAG_FIRE. Preserve them while ground
    // movement and handbraking alternate NORMAL/IDLE.
    const bool keepTimedVehicleFireVP =
        _vp_active == 7 &&
        _vehicle_fire_vp_end_time > _clock &&
        (arg->newStatus == BACT_STATUS_NORMAL || arg->newStatus == BACT_STATUS_IDLE);
    if ( arg->newStatus == BACT_STATUS_DEAD && (_vp_active != 2 && _vp_active != 3) )
    {
        _energy = -10000;
        SFXEngine::SFXe.StopCarrier(&_mimic_soundcarrier);

        SetVP(_vp_dead);

        _vp_active = 2;

        if ( _soundFlags & 2 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                yw_arg180 v43;
                v43.effects_type = 4;

                _world->ypaworld_func180(&v43);
            }

            SFXEngine::SFXe.sub_424000(&_soundcarrier, 1);
            _soundFlags &= ~2;
        }

        if ( _oflags & BACT_OFLAG_USERINPT )
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 8);

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 0);
        }

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
        }

        SFXEngine::SFXe.startSound(&_soundcarrier, 7);
        if ( ypabact_IsAirVehicle(this) && !(_status_flg & BACT_STFLAG_LAND) )
            SFXEngine::SFXe.startSound(&_soundcarrier, World::TVhclProto::SND_AIREXPLODE);

        _soundFlags |= 0x80;

        StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_AIR_DESTROYED);
        StartDestFXByType(World::DestFX::FX_DEATH);

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_NORMAL && 1 != _vp_active && !keepTimedVehicleFireVP )
    {
        SetVP(_vp_normal);

        _vp_active = 1;

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 1) )
        {
            _soundFlags |= 1;
            SFXEngine::SFXe.startSound(&_soundcarrier, 0);
        }

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_BEAM && 5 != _vp_active )
    {
        _vp_active = 5;
        SetVP(_vp_genesis);

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 3);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 0x200) )
        {
            _soundFlags |= 0x200;
            SFXEngine::SFXe.startSound(&_soundcarrier, 9);
        }

        StartDestFXByType(World::DestFX::FX_BEAM);

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_IDLE && _vp_active != 6 && !keepTimedVehicleFireVP )
    {
        SetVP(_vp_wait);
        _vp_active = 6;

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 0);
        }

        if ( _soundFlags & 8 )
        {
            _soundFlags &= ~8;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 3);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 4) )
        {
            _soundFlags |= 4;
            SFXEngine::SFXe.startSound(&_soundcarrier, 2);
        }

        result = 1;
    }

    if ( arg->newStatus == BACT_STATUS_CREATE && 4 != _vp_active )
    {
        _vp_active = arg->newStatus;
        // disable_genesis: the unit shows its normal model instead of the Genesis plasma.
        SetVP(_disable_genesis ? _vp_normal : _vp_genesis);

        if ( _soundFlags & 2 )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                yw_arg180 v46;
                v46.effects_type = 4;

                _world->ypaworld_func180(&v46);
            }

            SFXEngine::SFXe.sub_424000(&_soundcarrier, 1);
            _soundFlags &= ~2;
        }

        if ( _soundFlags & 1 )
        {
            _soundFlags &= ~1;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 0);
        }

        if ( _soundFlags & 4 )
        {
            _soundFlags &= ~4;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
        }

        if ( _soundFlags & 0x80 )
        {
            _soundFlags &= ~0x80;
            SFXEngine::SFXe.sub_424000(&_soundcarrier, 7);
        }

        if ( !(_soundFlags & 8) && !_disable_genesis )
        {
            _soundFlags |= 8;
            SFXEngine::SFXe.startSound(&_soundcarrier, 3);
        }

        StartDestFXByType(World::DestFX::FX_CREATE);

        result = 1;
    }

    if ( arg->unsetFlags == BACT_STFLAG_FIRE && _vp_active == 7 )
    {
        if ( _oflags & BACT_OFLAG_USERINPT )
        {
            yw_arg180 v45;
            v45.effects_type = 4;
            _world->ypaworld_func180(&v45);
        }

        SetVP(_vp_normal);
        _vp_active = 1;

        SFXEngine::SFXe.sub_424000(&_soundcarrier, 1);

        _soundFlags &= ~2;

        result = 1;
    }

    if ( arg->unsetFlags == BACT_STFLAG_DEATH2 && _vp_active == 3 )
    {
        _vp_active = 1;
        SetVP(_vp_normal);

        result = 1;
    }

    if ( arg->setFlags == BACT_STFLAG_FIRE && _vp_active != 7 )
    {
        _vp_active = 7;
        SetVP(_vp_fire);

        if ( !(_soundFlags & 2) )
        {
            if ( _oflags & BACT_OFLAG_USERINPT )
            {
                yw_arg180 v42;
                v42.effects_type = 3;
                _world->ypaworld_func180(&v42);
            }

            _soundFlags |= 2;
            SFXEngine::SFXe.startSound(&_soundcarrier, 1);
        }
        result = 1;
    }

    if ( arg->setFlags == BACT_STFLAG_DEATH2 )
    {
        _status = BACT_STATUS_DEAD;

        // Inline Physical FX use DEATH2 as their terminal impact/lifetime event.
        // Trigger the authored explode sound, palette and shake once even if the
        // impact VP is already active, so audio/feedback is not coupled to rendering.
        if ( _inlinePhysicalFX && !_inlinePhysicalExplodeFXStarted )
        {
            _inlinePhysicalExplodeFXStarted = true;
            _soundcarrier.Position = _position;
            _soundcarrier.Vector = _fly_dir * _fly_dir_length;
            SFXEngine::SFXe.startSound(&_soundcarrier, World::TVhclProto::SND_EXPLODE);
        }

        if ( _vp_active != 3 )
        {
            SetVP(_vp_megadeth);
            _vp_active = 3;

            if ( _soundFlags & 2 )
            {
                if ( _oflags & BACT_OFLAG_USERINPT )
                {
                    yw_arg180 v44;
                    v44.effects_type = 4;
                    _world->ypaworld_func180(&v44);
                }

                SFXEngine::SFXe.sub_424000(&_soundcarrier, 1);
                _soundFlags &= ~2;
            }

            if ( _oflags & BACT_OFLAG_USERINPT )
                SFXEngine::SFXe.sub_424000(&_soundcarrier, 8);

            if ( _soundFlags & 1 )
            {
                _soundFlags &= ~1;
                SFXEngine::SFXe.sub_424000(&_soundcarrier, 0);
            }

            if ( _soundFlags & 8 )
            {
                _soundFlags &= ~8;
                SFXEngine::SFXe.sub_424000(&_soundcarrier, 3);
            }

            if ( _soundFlags & 4 )
            {
                _soundFlags &= ~4;
                SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
            }

            if ( _soundFlags & 0x80 )
            {
                _soundFlags &= ~0x80;
                SFXEngine::SFXe.sub_424000(&_soundcarrier, 7);
            }

            if ( !_inlinePhysicalFX )
                SFXEngine::SFXe.startSound(&_soundcarrier, World::TVhclProto::SND_EXPLODE);

            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DESTROYED);
            StartDestFXByType(World::DestFX::FX_MEGADETH);

            _fly_dir_length = 0;

            result = 1;
        }
    }
    if (arg->newStatus == BACT_STATUS_CREATE)
    {
        if (ypabact_IsGenesisSeparationVehicle(this)) _genesisExitPending=true;
        PlaceGenesisAboveTerrain();
    }
    return result;
}

void NC_STACK_ypabact::ypabact_func122(update_msg *upd)
{
    float ftime = upd->frameTime * 0.001;

    if ( 0.001 * (upd->gTime - _lastFrmStamp) > 0.0 )
    {
        // Interpolate rotation
        _rotation += _netDRot * ftime;

        vec3d axis = _rotation.AxisX();

        if (axis.normalise() > 0.0001)
            _rotation.SetX( axis );
        else
            _rotation.SetX( vec3d::OX(1.0) );

        axis = _rotation.AxisY();

        if (axis.normalise() > 0.0001)
            _rotation.SetY( axis );
        else
            _rotation.SetY( vec3d::OY(1.0) );

        // Get "90 - angle" between interpolated X and Y
        float as = C_PI_2 - clp_acos( _rotation.AxisX().dot( _rotation.AxisY() ));

        // Calculate correction axis
        vec3d axs = _rotation.AxisX() * _rotation.AxisY();

        // FIX MY MATH ?
        // axs must be 1.0 normalised?

        // Rotate Y axis for 90" between X and Y
        vec3d newY = mat3x3::AxisAngle(axs, -as).Transform( _rotation.AxisY() );

        _rotation.SetY( newY );

        _rotation.SetZ( _rotation.AxisX() * _rotation.AxisY() );

        _position += _fly_dir * (_fly_dir_length * ftime * 6.0);

        CorrectPositionInLevelBox(nullptr);
    }
}

void NC_STACK_ypabact::ypabact_func123(update_msg *upd)
{
    float ftime = upd->frameTime * 0.001;
    float stupd = (upd->gTime - _lastFrmStamp) * 0.001;

    if ( stupd > 0.0 )
    {
        _rotation += _netDRot * ftime;

        vec3d axis = _rotation.AxisX();

        if (axis.normalise() > 0.0001)
            _rotation.SetX( axis );
        else
            _rotation.SetX( vec3d::OX(1.0) );

        axis = _rotation.AxisY();

        if (axis.normalise() > 0.0001)
            _rotation.SetY( axis );
        else
            _rotation.SetY( vec3d::OY(1.0) );

        axis = _rotation.AxisZ();

        if (axis.normalise() > 0.0001)
            _rotation.SetZ( axis );
        else
            _rotation.SetZ( vec3d::OZ(1.0) );

        vec3d spd = _fly_dir * _fly_dir_length + _netDSpeed * stupd;

        bool hgun = false;
        if (_bact_type == BACT_TYPES_GUN)
        {
            NC_STACK_ypagun *guno = dynamic_cast<NC_STACK_ypagun *>(this);
            if (guno)
            {
                hgun = guno->IsRoboGun();
            }
        }

        if (_bact_type != BACT_TYPES_GUN || hgun)
            _position = spd * ftime * 6.0;

        CorrectPositionInLevelBox(nullptr);

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            ypaworld_arg136 arg136;
            arg136.stPos = _position;
            arg136.vect = _rotation.AxisY() * 200.0;
            arg136.flags = 0;

            _world->ypaworld_func136(&arg136);

            if ( arg136.isect )
                _position = arg136.isectPos - _rotation.AxisY() * _overeof;
        }
    }
}

void NC_STACK_ypabact::setBACT_viewer(bool vwr)
{
    uamessage_viewer viewMsg;

    if ( !vwr )
        ResetAlternativeView();

    if ( vwr )
    {
        if ( _world && !_world->CanControlUnitInSpectatorMode(this) )
            return;

        // OpenNeoUA custom: artillery shell platforms are map-only artillery; never let the
        // camera/viewer enter one (that is what made it look possessed in 1st person).
        if ( _world && IsArtilleryShellPlatform() )
            return;

        if (_world->_viewerBact)
        {
            if ( _world->_viewerBact->_bact_type != BACT_TYPES_MISSLE )
                _salve_counter = 0;
        }

        _world->ypaworld_func131(this); //Set current bact

        _oflags |= BACT_OFLAG_VIEWER;

        if ( _world->_isNetGame )
            viewMsg.view = 1;

        if ( _bact_type == BACT_TYPES_BACT && !(_status_flg & BACT_STFLAG_LAND) && _status == BACT_STATUS_NORMAL )
            _thraction = _force;

        SFXEngine::SFXe.startSound(&_soundcarrier, 8);
    }
    else
    {
        _oflags &= ~BACT_OFLAG_VIEWER;

        if ( _world->_isNetGame )
            viewMsg.view = 0;

        SFXEngine::SFXe.sub_424000(&_soundcarrier, 8);

        if ( _bact_type != BACT_TYPES_MISSLE && _bact_type != BACT_TYPES_ROBO && _status != BACT_STATUS_DEAD )
        {
            if ( _host_station == _parent )
            {
                if ( !(_status_flg & BACT_STFLAG_WAYPOINT) || !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
                {
                    for (NC_STACK_ypabact* &node : _kidList)
                        node->CopyTargetOf(this);
                }
            }
            else
            {
                if ( !(_status_flg & BACT_STFLAG_WAYPOINT) || !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
                    CopyTargetOf(_parent);
            }
        }
    }

    if ( _world->_isNetGame ) // Network message send routine?
    {
        viewMsg.msgID = UAMSG_VIEWER;
        viewMsg.owner = _owner;
        viewMsg.classID = _bact_type;
        viewMsg.id = _gid;

        if ( viewMsg.classID == BACT_TYPES_MISSLE )
        {
            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(this);
            viewMsg.launcher = miss->GetLauncherBact()->_gid;
        }

        _world->NetBroadcastMessage(&viewMsg, sizeof(viewMsg), true);
    }
}

void NC_STACK_ypabact::setBACT_airconst(int air)
{
    _airconst = air;
    _airconst_static = air;
}

void NC_STACK_ypabact::setBACT_landingOnWait(bool lnding)
{
    if ( lnding )
        _oflags |= BACT_OFLAG_LANDONWAIT;
    else
        _oflags &= ~BACT_OFLAG_LANDONWAIT;
}

void NC_STACK_ypabact::setBACT_yourLastSeconds(int ls)
{
    _yls_time = ls;
}

void NC_STACK_ypabact::SetVP(NC_STACK_base *vp)
{
    Common::DeleteAndNull(&_current_vp);
    if (vp)
        _current_vp = vp->GenRenderInstance();
}

void NC_STACK_ypabact::setBACT_aggression(int aggr)
{
    _aggr = aggr;

    for (NC_STACK_ypabact* &nod : _kidList)
        nod->_aggr = aggr;
}

void NC_STACK_ypabact::setBACT_extraViewer(bool vwr)
{
    if ( vwr )
        _oflags |= BACT_OFLAG_EXTRAVIEW;
    else
        _oflags &= ~BACT_OFLAG_EXTRAVIEW;
}

void NC_STACK_ypabact::setBACT_alwaysRender(bool rndr)
{
    if ( rndr )
        _oflags |= BACT_OFLAG_ALWAYSREND;
    else
        _oflags &= ~BACT_OFLAG_ALWAYSREND;
}

bool NC_STACK_ypabact::IsParentMyRobo() const
{
    return (_host_station) && (_parent) && (_host_station == _parent);
}

void NC_STACK_ypabact::ChangeEscapeFlag(bool escape)
{
    if ( escape )
        _status_flg |= BACT_STFLAG_ESCAPE;
    else
        _status_flg &= ~BACT_STFLAG_ESCAPE;

    // May be do it in recursion?
    for( NC_STACK_ypabact* &node : _kidList )
    {
        if ( escape )
            node->_status_flg |= BACT_STFLAG_ESCAPE;
        else
            node->_status_flg &= ~BACT_STFLAG_ESCAPE;
    }
}

bool NC_STACK_ypabact::IsHidden() const
{
    if (_hidden)
        return true;

    if (_world && _world->IsHidden(_owner))
        return true;

    return false;
}

bool NC_STACK_ypabact::IsHiddenFor(uint8_t owner) const
{
    if (owner == _owner) // Own unit can be seen
        return false;

    if (_pSector && _pSector->IsUnhideFor(owner))
        return false;

    if (_hidden)
        return true;

    if (_world && _world->IsHidden(_owner))
        return true;

    return false;
}
