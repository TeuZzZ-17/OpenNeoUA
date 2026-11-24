// Radial shot: projectiles fired in all directions (nearby enemies, or once at death).
#include "game/ypabact_internal.h"

bool ypabact_CanUseRadialShot(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
        return false;

    if ( !unit->_radial_shot_enable )
        return false;

    if ( unit->_radial_shot_mode == 1 )
        return false;

    if ( unit->_radial_shot_weapon <= 0 || (size_t)unit->_radial_shot_weapon >= unit->getBACT_pWorld()->GetWeaponsProtos().size() )
        return false;

    if ( unit->_radial_shot_trigger_radius <= 0.0 )
        return false;

    if ( unit->_radial_shot_shots <= 0 )
        return false;

    if ( unit->_owner == World::OWNER_0 )
        return false;

    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( unit->_status == BACT_STATUS_DEAD ||
         unit->_status == BACT_STATUS_CREATE ||
         unit->_status == BACT_STATUS_BEAM )
        return false;

    if ( unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER) )
        return false;

    return true;
}

vec3d ypabact_GetRadialShotLocalDirection(NC_STACK_ypabact *unit, int shotIndex, int totalShots)
{
    return World::ResolveYawPitchDirection(
        unit->_radial_shot_horizontal_angle_set,
        unit->_radial_shot_horizontal_angle_min,
        unit->_radial_shot_horizontal_angle_max,
        unit->_radial_shot_vertical_angle_set,
        unit->_radial_shot_vertical_angle_min,
        unit->_radial_shot_vertical_angle_max,
        shotIndex,
        totalShots,
        !unit->_radial_shot_horizontal_angle_set);
}

NC_STACK_ypabact *ypabact_GetRadialShotAtDeathMissileOwner(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return nullptr;

    if ( ypabact_IsLiveRadialShotMissileOwner(unit->_parent, unit) && unit->_parent->_owner == unit->_owner )
        return unit->_parent;

    if ( ypabact_IsLiveRadialShotMissileOwner(unit->_host_station, unit) && unit->_host_station->_owner == unit->_owner )
        return unit->_host_station;

    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( world )
    {
        NC_STACK_ypabact *userHost = world->getYW_userHostStation();
        if ( ypabact_IsLiveRadialShotMissileOwner(userHost, unit) && userHost->_owner == unit->_owner )
            return userHost;
    }

    return nullptr;
}

bool ypabact_FireRadialShotShot(NC_STACK_ypabact *unit, int shotIndex, int totalShots, bool trackLauncherMissile, NC_STACK_ypabact *missileListOwner)
{
    if ( !unit || !unit->getBACT_pWorld() || totalShots <= 0 )
        return false;

    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( (size_t)unit->_radial_shot_weapon >= world->GetWeaponsProtos().size() )
        return false;

    World::TWeapProto &wproto = world->GetWeaponsProtos().at(unit->_radial_shot_weapon);
    if ( !(wproto._weaponFlags & YPA_WEAPON_FLAG_PROJECTILE) )
        return false;

    vec3d localDir = ypabact_GetRadialShotLocalDirection(unit, shotIndex, totalShots);
    vec3d shotDir = unit->_rotation.Transpose().Transform(localDir);
    float shotDirLen = shotDir.length();
    if ( shotDirLen <= 0.001 )
        return false;

    shotDir = shotDir / shotDirLen;

    // OpenNeoUA invisible: a radial-shot shot is a real attack -> reveal the unit.
    unit->RevealInvisibleOnAttack();

    ypaworld_arg146 arg147;
    arg147.vehicle_id = unit->_radial_shot_weapon;
    arg147.pos = unit->_position;

    NC_STACK_ypamissile *wobj = world->ypaworld_func147(&arg147);
    if ( !wobj )
        return false;

    NC_STACK_ypabact *liveLauncher = missileListOwner ? missileListOwner : unit;

    NC_STACK_yparobo *hostStation = unit->_host_station
                                        ? unit->_host_station
                                        : (liveLauncher ? liveLauncher->_host_station : nullptr);
    wobj->ConfigureSpawnedProjectileBase(liveLauncher, hostStation,
                                         unit->_owner, shotDir);
    wobj->_fly_dir_length = unit->_fly_dir_length + wproto.start_speed;

    if ( !(wproto._weaponFlags & 0x12) )
        wobj->_fly_dir_length *= 0.2;

    wobj->_rotation.SetZ(wobj->_fly_dir);
    wobj->_rotation.SetX(unit->_rotation.AxisX());
    wobj->_rotation.SetY(wobj->_rotation.AxisZ() * wobj->_rotation.AxisX());
    wobj->StartWeaponTracer();

    world->SpawnTransientVisual(unit->_radial_shot_vp_launch,
                                unit->_radial_shot_3ds_launch,
                                unit->_radial_shot_base_launch,
                                wobj->_position, wobj->_rotation, 1000);

    wobj->_kidRef.Detach();
    wobj->_parent = nullptr;

    if ( trackLauncherMissile )
    {
        NC_STACK_ypabact *listOwner = missileListOwner ? missileListOwner : unit;
        if ( listOwner )
            listOwner->_missiles_list.push_back(wobj);
    }

    int missileType = wobj->GetMissileType();
    if ( missileType == NC_STACK_ypamissile::MISL_DIRECT )
    {
        wobj->_primTtype = BACT_TGT_TYPE_DRCT;
        wobj->_target_dir = wobj->_fly_dir;
    }
    else if ( missileType == NC_STACK_ypamissile::MISL_TARGETED )
    {
        setTarget_msg target = {};
        target.tgt_type = BACT_TGT_TYPE_DRCT;
        target.priority = 0;
        target.tgt_pos = arg147.pos + shotDir * 1000.0;
        wobj->SetTarget(&target);
    }

    SFXEngine::SFXe.startSound(&wobj->_soundcarrier, 1);

    if ( wobj->_primTtype != BACT_TGT_TYPE_UNIT )
    {
        const int lifeTimeNt = wproto.RollLifeTimeNt();

        if ( lifeTimeNt )
            wobj->SetLifeTime(lifeTimeNt);
    }

    return true;
}

void ypabact_FireRadialShotBurst(NC_STACK_ypabact *unit, bool trackLauncherMissiles, NC_STACK_ypabact *missileListOwner)
{
    int shots = unit->_radial_shot_shots > 0 ? unit->_radial_shot_shots : 1;
    for (int i = 0; i < shots; i++)
        ypabact_FireRadialShotShot(unit, i, shots, trackLauncherMissiles, missileListOwner);
}

bool ypabact_CanUseRadialShotAtDeath(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
        return false;

    if ( unit->getBACT_pWorld()->_isNetGame )
        return false;

    if ( !unit->_radial_shot_enable || unit->_radial_shot_mode != 1 || unit->_radial_shot_at_death_done )
        return false;

    if ( unit->_radial_shot_weapon <= 0 || (size_t)unit->_radial_shot_weapon >= unit->getBACT_pWorld()->GetWeaponsProtos().size() )
        return false;

    if ( unit->_radial_shot_shots <= 0 )
        return false;

    if ( unit->_owner == World::OWNER_0 )
        return false;

    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( unit->_status == BACT_STATUS_CREATE || unit->_status == BACT_STATUS_BEAM )
        return false;

    if ( unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER | BACT_STFLAG_CLEAN) )
        return false;

    return true;
}

void ypabact_FireRadialShotAtDeath(NC_STACK_ypabact *unit)
{
    if ( !ypabact_CanUseRadialShotAtDeath(unit) )
        return;

    NC_STACK_ypabact *missileOwner = ypabact_GetRadialShotAtDeathMissileOwner(unit);
    if ( !missileOwner )
        return;

    // Death handling happens before the unit enters DEAD state, so at_death is
    // intentionally one immediate all-at-once burst. Sequential timing remains
    // a classic-mode behavior and does not extend the dying unit lifecycle.
    unit->_radial_shot_at_death_done = true;
    ypabact_FireRadialShotBurst(unit, true, missileOwner);
}

void NC_STACK_ypabact::UpdateRadialShot(update_msg *)
{
    if ( !ypabact_CanUseRadialShot(this) )
    {
        _radial_shot_sequence_active = false;
        return;
    }

    // V1 is single-player/local only; avoiding unsynchronised network projectiles is safer.
    if ( _world->_isNetGame )
        return;

    int interval = _radial_shot_interval > 0 ? _radial_shot_interval : 1000;
    int shots = _radial_shot_shots > 0 ? _radial_shot_shots : 1;

    if ( _radial_shot_fire_mode == 1 && _radial_shot_sequence_active )
    {
        int delay = _radial_shot_sequence_delay > 0 ? _radial_shot_sequence_delay : 100;

        if ( _clock < _radial_shot_next_shot_time )
            return;

        ypabact_FireRadialShotShot(this, _radial_shot_sequence_shots_fired, shots);
        _radial_shot_sequence_shots_fired++;

        if ( _radial_shot_sequence_shots_fired >= shots )
        {
            _radial_shot_sequence_active = false;
            _radial_shot_sequence_shots_fired = 0;
            _radial_shot_next_activation_time = _clock + interval;
        }
        else
        {
            _radial_shot_next_shot_time = _clock + delay;
        }

        return;
    }

    if ( _clock < _radial_shot_next_activation_time )
        return;

    if ( !ypabact_HasEnemyNearby(this, _radial_shot_trigger_radius) )
        return;

    if ( _radial_shot_fire_mode == 1 )
    {
        int delay = _radial_shot_sequence_delay > 0 ? _radial_shot_sequence_delay : 100;
        _radial_shot_sequence_active = true;
        _radial_shot_sequence_shots_fired = 1;

        ypabact_FireRadialShotShot(this, 0, shots);

        if ( _radial_shot_sequence_shots_fired >= shots )
        {
            _radial_shot_sequence_active = false;
            _radial_shot_sequence_shots_fired = 0;
            _radial_shot_next_activation_time = _clock + interval;
        }
        else
        {
            _radial_shot_next_shot_time = _clock + delay;
        }

        return;
    }

    ypabact_FireRadialShotBurst(this);
    _radial_shot_next_activation_time = _clock + interval;
}
