// Chain and cluster projectiles.
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "game/yw.h"
#include "game/ypamissile.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "system/inivals.h"
#include "common/log.h"
#include <algorithm>
#include <math.h>
#include "game/ypamissile_internal.h"

bool NC_STACK_ypamissile::TryClusterSplit(bool impactOrDetonation)
{
    if ( !_world || !_mislEmitter || _mislClusterDone || _mislClusterChild )
        return false;

    std::vector<World::TWeapProto> &weapons = _world->GetWeaponsProtos();

    if ( _vehicleID >= weapons.size() )
        return false;

    World::TWeapProto &parentProto = weapons.at(_vehicleID);
    World::TWeaponClusterConfig &cluster = parentProto.cluster;

    if ( !cluster.enable || cluster.count <= 0 || cluster.weapon_id <= 0 )
        return false;

    if ( !World::IsWeaponClusterTriggerDue(cluster.trigger_time,
                                            _mislClusterAge,
                                            impactOrDetonation) )
        return false;

    int childClusterGeneration = _mislClusterGeneration + 1;

    if ( _mislClusterGeneration > 0 && (cluster.generations <= 0 || childClusterGeneration > cluster.generations) )
    {
        _mislClusterDone = true;
        return false;
    }

    if ( cluster.weapon_id >= weapons.size() )
        return false;

    World::TWeapProto &childProto = weapons.at(cluster.weapon_id);

    if ( !(childProto._weaponFlags & 1) )
        return false;

    _mislClusterDone = true;

    _world->SpawnTransientVisual(cluster.vp, cluster.mesh3ds, cluster.basePath,
                                 _position, _rotation, 1000);

    cluster.snd.LoadSamples();
    TSampleData *clusterSample = cluster.snd.MainSample.Sample ? cluster.snd.MainSample.Sample->GetSampleData() : nullptr;
    bool clusterSoundPlayed = false;

    vec3d baseDir = _fly_dir;
    if ( baseDir.normalise() <= 0.001 )
        baseDir = _rotation.AxisZ();
    if ( baseDir.normalise() <= 0.001 )
        baseDir = vec3d::OZ(1.0);

    uint8_t childTargetType = BACT_TGT_TYPE_DRCT;
    BactTarget childTarget = {};
    vec3d childTargetPos = _position + baseDir * 1000.0;

    if ( _primTtype == BACT_TGT_TYPE_UNIT && _primT.pbact && _primT.pbact->_status != BACT_STATUS_DEAD )
    {
        childTargetType = BACT_TGT_TYPE_UNIT;
        childTarget = _primT;
        childTargetPos = _primT.pbact->_position;
    }
    else if ( _primTtype == BACT_TGT_TYPE_CELL )
    {
        childTargetType = BACT_TGT_TYPE_CELL;
        childTarget = _primT;
        childTargetPos = _primTpos;
    }

    int countMin = 1;
    int countMax = 1;
    cluster.GetCountRange(countMin, countMax);
    int spawnCount = World::RandomIntRangeInclusive(countMin, countMax);
    int spawned = 0;

    const bool hasClusterAngles = cluster.horizontal_angle_set ||
                                  cluster.vertical_angle_set;
    float horizontalMin = cluster.horizontal_angle_min;
    float horizontalMax = cluster.horizontal_angle_max;
    if ( horizontalMax < horizontalMin )
        std::swap(horizontalMin, horizontalMax);
    const bool evenlyDistributeYaw =
        cluster.horizontal_angle_set &&
        std::fabs((horizontalMax - horizontalMin) - 360.0f) <= 0.001f;

    for (int i = 0; i < spawnCount; i++)
    {
        ypaworld_arg146 arg147;
        arg147.vehicle_id = cluster.weapon_id;
        arg147.pos = _position;

        NC_STACK_ypamissile *child = _world->ypaworld_func147(&arg147);

        if ( !child )
            continue;

        vec3d childDir = baseDir;
        if ( hasClusterAngles )
        {
            vec3d localDir = World::ResolveYawPitchDirection(
                cluster.horizontal_angle_set,
                horizontalMin,
                horizontalMax,
                cluster.vertical_angle_set,
                cluster.vertical_angle_min,
                cluster.vertical_angle_max,
                i,
                spawnCount,
                evenlyDistributeYaw);
            childDir = _rotation.Transpose().Transform(localDir);
            if ( childDir.normalise() <= 0.001f )
                childDir = baseDir;
        }

        // Cluster spread remains an independent deviation applied after the
        // new yaw/pitch base direction, so both controls compose.
        childDir = World::ApplyDirectionalSpread(_rotation, childDir,
                                                  cluster.spread_x,
                                                  cluster.spread_y);

        NC_STACK_yparobo *childHost = _host_station
                                          ? _host_station
                                          : _mislEmitter->_host_station;
        child->ConfigureSpawnedProjectileBase(_mislEmitter, childHost,
                                               _owner, childDir);
        child->SetClusterSpawnedChild(cluster.generations <= 0);
        child->_mislClusterGeneration = childClusterGeneration;
        if ( childProto.IsArcGrenade() )
        {
            if ( hasClusterAngles )
            {
                // The Cluster angles and subsequent spread are already the
                // complete authored launch direction. Commit that vector to
                // the Arc Grenade runtime without overwriting its pitch.
                const float childSpeed =
                    std::isfinite(childProto.start_speed) && childProto.start_speed > 0.0f
                        ? childProto.start_speed
                        : 0.0f;
                child->SetupArcGrenadeVelocity(child->_fly_dir * childSpeed,
                                               childProto.grenade_arc_gravity);
            }
            else
            {
                // No Cluster angles: preserve the legacy Arc Grenade launch
                // path, including grenade_arc_angle.
                child->SetupArcGrenadeLaunch(childProto.grenade_arc_angle,
                                             childProto.grenade_arc_gravity,
                                             childProto.start_speed);
            }
        }
        else
        {
            child->_fly_dir_length = childProto.start_speed;

            if ( !(childProto._weaponFlags & 0x12) )
                child->_fly_dir_length *= 0.2;

            child->_rotation.SetZ(child->_fly_dir);
            child->_rotation.SetX(_rotation.AxisX());
            child->_rotation.SetY(child->_rotation.AxisZ() * child->_rotation.AxisX());
        }
        child->StartWeaponTracer();

        child->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_LAUNCH, nullptr,
                                     &childProto.chain_fx, &child->_position, &child->_rotation);

        child->_kidRef.Detach();
        child->_parent = nullptr;
        _mislEmitter->_missiles_list.push_back(child);

        if ( (clusterSample || cluster.snd.sndPrm.slot || cluster.snd.sndPrm_shk.slot) && !clusterSoundPlayed )
        {
            child->_mislClusterSoundCarrier.Clear();
            child->_mislClusterSoundCarrier.Resize(1);
            child->_mislClusterSoundCarrier.Position = _position;
            child->_mislClusterSoundCarrier.Vector = vec3d(0.0, 0.0, 0.0);

            TSoundSource &snd = child->_mislClusterSoundCarrier.Sounds[0];
            snd.PSample = clusterSample;
            snd.Volume = cluster.snd.volume ? cluster.snd.volume : 120;
            cluster.snd.ConfigureSoundSourcePitch(snd);
            snd.Radius = cluster.snd.radius;
            snd.SetLoop(false);
            snd.SetFragmented(false);

            if ( cluster.snd.sndPrm.slot )
            {
                snd.PPFx = &cluster.snd.sndPrm;
                snd.SetPFx(true);
            }
            else
            {
                snd.PPFx = nullptr;
                snd.SetPFx(false);
            }

            if ( cluster.snd.sndPrm_shk.slot )
            {
                snd.PShkFx = &cluster.snd.sndPrm_shk;
                snd.SetShk(true);
            }
            else
            {
                snd.PShkFx = nullptr;
                snd.SetShk(false);
            }

            SFXEngine::SFXe.startSound(&child->_mislClusterSoundCarrier, 0);
            SFXEngine::SFXe.UpdateSoundCarrier(&child->_mislClusterSoundCarrier);
            clusterSoundPlayed = true;
        }

        if ( child->GetMissileType() == MISL_TARGETED &&
             childTargetType != BACT_TGT_TYPE_DRCT )
        {
            setTarget_msg arg67;
            arg67.tgt = childTarget;
            arg67.tgt_type = childTargetType;
            arg67.priority = 0;
            arg67.tgt_pos = childTargetPos;

            child->SetTarget(&arg67);

            if ( childTargetType == BACT_TGT_TYPE_CELL )
                child->_primTpos.y = childTargetPos.y;
        }
        else
        {
            child->_primTtype = BACT_TGT_TYPE_DRCT;
            child->_target_dir = child->_fly_dir;
        }

        if ( childTargetType != BACT_TGT_TYPE_UNIT )
        {
            int life_time_nt = childProto.life_time_nt;

            if ( life_time_nt )
                child->SetLifeTime(life_time_nt);
        }

        SFXEngine::SFXe.startSound(&child->_soundcarrier, World::TWeapProto::SND_LAUNCH);
        spawned++;
    }

    if ( spawned <= 0 )
    {
        _mislClusterDone = false;
        return false;
    }

    if ( getBACT_viewer() )
    {
        if ( _mislEmitter )
            ResetViewing();
        else
        {
            setBACT_viewer(false);
            setBACT_inputting(false);
        }
    }

    _hidden = true;
    _fly_dir_length = 0.0;
    _status = BACT_STATUS_DEAD;
    setBACT_yourLastSeconds(0);

    return true;
}

bool NC_STACK_ypamissile::IsChainHit(NC_STACK_ypabact *target) const
{
    if ( !target || !target->_gid )
        return false;

    return std::find(_mislChainHitGids.begin(), _mislChainHitGids.end(), target->_gid) != _mislChainHitGids.end();
}

void NC_STACK_ypamissile::RememberChainHit(NC_STACK_ypabact *target)
{
    if ( target && target->_gid && !IsChainHit(target) )
        _mislChainHitGids.push_back(target->_gid);
}

bool NC_STACK_ypamissile::CanChainToTarget(NC_STACK_ypabact *target, NC_STACK_ypabact *currentHit) const
{
    if ( !target || target == this || target == _mislEmitter || target == currentHit )
        return false;

    if ( _mislEmitter && _mislEmitter->IsMountedFamilyWith(target) )
        return false;

    if ( !_world || target->getBACT_pWorld() != _world )
        return false;

    if ( _owner == World::OWNER_0 || _owner == World::OWNER_7 ||
         target->_owner == World::OWNER_0 || target->_owner == World::OWNER_7 )
        return false;

    if ( !_mislChainAllowFriendly && target->_owner == _owner )
        return false;

    if ( target->_isDummy )
        return false;

    switch ( target->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_ROBO:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
    case BACT_TYPES_GUN:
        break;

    default:
        return false;
    }

    if ( target->_bact_type == BACT_TYPES_MISSLE ||
         target->_status == BACT_STATUS_DEAD ||
         target->_status == BACT_STATUS_CREATE ||
         target->_status == BACT_STATUS_BEAM ||
         target->_energy <= 0 ||
         target->_energy_max <= 0 ||
         target->IsDestroyed() ||
         (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
        return false;

    if ( target->_bact_type == BACT_TYPES_GUN && target->GetEffectiveShield() >= 100.0f )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(target);
        if ( !gun || (gun->IsRoboGun() && !target->_isUnitGunChild) )
            return false;
    }

    return !IsChainHit(target);
}

NC_STACK_ypabact *NC_STACK_ypamissile::FindNextChainTarget(NC_STACK_ypabact *currentHit) const
{
    if ( !_world || !currentHit )
        return nullptr;

    if ( _vehicleID < 0 || (size_t)_vehicleID >= _world->GetWeaponsProtos().size() )
        return nullptr;

    const World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_vehicleID);
    if ( wproto.chain.radius <= 0.0 )
        return nullptr;

    Common::Point centerCell = World::PositionToSectorID(currentHit->_position);
    int cellRadius = (int)ceil(wproto.chain.radius / World::CVSectorLength) + 1;
    float radiusSq = wproto.chain.radius * wproto.chain.radius;
    float bestScore = radiusSq + 1.0f;
    NC_STACK_ypabact *bestTarget = nullptr;

    for ( int dy = -cellRadius; dy <= cellRadius; dy++ )
    {
        for ( int dx = -cellRadius; dx <= cellRadius; dx++ )
        {
            Common::Point cellId = centerCell + Common::Point(dx, dy);

            if ( !_world->IsSector(cellId) )
                continue;

            cellArea &cell = _world->SectorAt(cellId);

            for ( NC_STACK_ypabact *candidate : cell.unitsList )
            {
                if ( !CanChainToTarget(candidate, currentHit) )
                    continue;

                vec3d delta = candidate->_position - currentHit->_position;
                float distSq = delta.dot(delta);

                if ( distSq > radiusSq || distSq >= bestScore )
                    continue;

                bestScore = distSq;
                bestTarget = candidate;
            }
        }
    }

    return bestTarget;
}

bool NC_STACK_ypamissile::SpawnChainProjectile(const vec3d &originPos, float originRadius, NC_STACK_ypabact *nextTarget, int childEnergy)
{
    if ( !_world || !_mislEmitter || !nextTarget || childEnergy <= 0 )
        return false;

    if ( _vehicleID < 0 || (size_t)_vehicleID >= _world->GetWeaponsProtos().size() )
        return false;

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_vehicleID);

    if ( !wproto.chain.allow || wproto.IsLaser() || wproto.IsArtilleryShell() ||
         wproto.IsKamikaze() ||
         !(wproto._weaponFlags & World::TWeapProto::WEAPON_FLAG_PROJECTILE) )
        return false;

    vec3d chainDir = nextTarget->_position - originPos;
    float chainDirLen = chainDir.length();
    if ( chainDirLen <= 0.001 )
        return false;

    chainDir /= chainDirLen;

    float launchOffset = originRadius + wproto.radius + 1.0f;
    if ( launchOffset < 1.0f )
        launchOffset = 1.0f;

    ypaworld_arg146 arg147;
    arg147.vehicle_id = _vehicleID;
    arg147.pos = originPos + chainDir * launchOffset;

    NC_STACK_ypamissile *child = _world->ypaworld_func147(&arg147);
    if ( !child )
        return false;

    child->SetLauncherBact(_mislEmitter);
    child->SetStartHeight(arg147.pos.y);
    child->_owner = _owner;
    child->_host_station = _host_station;
    child->_energy = childEnergy;
    child->_energy_max = childEnergy;
    child->_fly_dir = chainDir;
    if ( wproto.IsArcGrenade() )
    {
        child->SetupArcGrenadeLaunch(wproto.grenade_arc_angle,
                                     wproto.grenade_arc_gravity,
                                     wproto.start_speed);
    }
    else
    {
        child->_fly_dir_length = wproto.start_speed;

        if ( !(wproto._weaponFlags & 0x12) )
            child->_fly_dir_length *= 0.2;

        child->_rotation.SetZ(child->_fly_dir);
        child->_rotation.SetX(_rotation.AxisX());
        child->_rotation.SetY(child->_rotation.AxisZ() * child->_rotation.AxisX());
    }
    child->StartWeaponTracer();

    child->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_LAUNCH, nullptr,
                                 &wproto.chain_fx, &child->_position, &child->_rotation);

    child->_kidRef.Detach();
    child->_parent = nullptr;
    _mislEmitter->_missiles_list.push_back(child);

    child->_mislChainDepth = _mislChainDepth + 1;
    child->_mislChainEnergy = childEnergy;
    child->_mislChainHitGids = _mislChainHitGids;
    child->_mislChainSpawned = false;
    child->_mislChainAllowFriendly = _mislChainAllowFriendly;

    if ( child->GetMissileType() == MISL_TARGETED || wproto.IsHomingBomb() )
    {
        setTarget_msg arg67;
        arg67.tgt.pbact = nextTarget;
        arg67.tgt_type = BACT_TGT_TYPE_UNIT;
        arg67.priority = 0;
        arg67.tgt_pos = nextTarget->_position;

        child->SetTarget(&arg67);
    }
    else
    {
        child->_primTtype = BACT_TGT_TYPE_DRCT;
        child->_target_dir = child->_fly_dir;
    }

    SFXEngine::SFXe.startSound(&child->_soundcarrier, World::TWeapProto::SND_LAUNCH);

    return true;
}

void NC_STACK_ypamissile::UpdatePendingChainJump(update_msg *arg)
{
    if ( !_mislChainPending )
        return;

    if ( !arg )
        return;

    _mislChainPendingElapsed += arg->frameTime;
    if ( _mislChainPendingElapsed < _mislChainPendingDelay )
        return;

    _mislChainPending = false;

    if ( !_world || !_mislChainPendingTargetGid || _mislChainPendingEnergy <= 0 )
        return;

    NC_STACK_ypabact *target = ypamissile_FindLiveBactByGid(_world->_unitsList, _mislChainPendingTargetGid);
    if ( !CanChainToTarget(target, nullptr) )
        return;

    SpawnChainProjectile(_mislChainPendingOrigin, _mislChainPendingOriginRadius, target, _mislChainPendingEnergy);
}

void NC_STACK_ypamissile::TrySpawnChainProjectile(NC_STACK_ypabact *currentHit, int appliedDamage)
{
    if ( currentHit && !_mislChainAllowFriendly && _mislChainDepth == 0 && currentHit->_owner == _owner &&
         _mislEmitter && (_mislEmitter->getBACT_inputting() || _mislEmitter->getBACT_viewer()) )
    {
        _mislChainAllowFriendly = true;
    }

    if ( _mislChainSpawned || appliedDamage <= 0 || !CanChainToTarget(currentHit, nullptr) )
        return;

    if ( !_world || _vehicleID < 0 || (size_t)_vehicleID >= _world->GetWeaponsProtos().size() )
        return;

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_vehicleID);
    const World::TWeaponChainConfig &chain = wproto.chain;

    if ( !chain.allow || chain.max_jumps <= 0 || chain.radius <= 0.0 || chain.energy_mult <= 0.0 )
        return;

    if ( wproto.IsLaser() || wproto.IsArtilleryShell() || wproto.IsKamikaze() )
        return;

    if ( !(wproto._weaponFlags & World::TWeapProto::WEAPON_FLAG_PROJECTILE) )
        return;

    if ( _mislChainDepth >= chain.max_jumps )
        return;

    int currentEnergy = _mislChainEnergy > 0 ? _mislChainEnergy : _energy;
    if ( currentEnergy <= 0 )
        return;

    RememberChainHit(currentHit);

    NC_STACK_ypabact *nextTarget = FindNextChainTarget(currentHit);
    if ( !nextTarget )
        return;

    int childEnergy = (int)((float)currentEnergy * chain.energy_mult);
    if ( childEnergy <= 0 )
        return;

    _mislChainEnergy = currentEnergy;
    if ( chain.jump_delay <= 0 )
    {
        _mislChainSpawned = SpawnChainProjectile(currentHit->_position, currentHit->_radius, nextTarget, childEnergy);
        return;
    }

    _mislChainSpawned = true;
    _mislChainPending = true;
    _mislChainPendingElapsed = 0;
    _mislChainPendingDelay = chain.jump_delay;
    _mislChainPendingTargetGid = nextTarget->_gid;
    _mislChainPendingEnergy = childEnergy;
    _mislChainPendingOrigin = currentHit->_position;
    _mislChainPendingOriginRadius = currentHit->_radius;

    if ( _yls_time < _mislChainPendingDelay + 100 )
        setBACT_yourLastSeconds(_mislChainPendingDelay + 100);
}
