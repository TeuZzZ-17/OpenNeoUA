// Carrier unit spawning and spawn-at-death units.
#include "game/ypabact_internal.h"

void ypabact_UpdateStatusSoundCarrier(NC_STACK_ypabact *bact, TSndCarrier *carrier)
{
    if ( !bact || !carrier || carrier->Sounds.empty() )
        return;

    TSoundSource &snd = carrier->Sounds[0];
    if ( !snd.IsEnabled() && !snd.IsPFxEnabled() && !snd.IsShkEnabled() )
        return;

    carrier->Position = bact->_position;
    carrier->Vector = bact->_fly_dir * bact->_fly_dir_length;

    SFXEngine::SFXe.UpdateSoundCarrier(carrier);
}

void ypabact_TriggerLocalShakeCarrier(NC_STACK_ypabact *bact,
                                              TSndCarrier *carrier,
                                              TSndFxPosParam *shake)
{
    if ( !bact || !carrier || !shake || shake->time <= 0 )
        return;

    if ( carrier->Sounds.empty() )
        carrier->Resize(1);

    TSoundSource &snd = carrier->Sounds[0];
    snd.PSample = nullptr;
    snd.PFragments = nullptr;
    snd.PPFx = nullptr;
    snd.PShkFx = shake;
    snd.Volume = 0;
    snd.Pitch = 0;
    snd.PriorityBias = 0;
    snd.SetLoop(false);
    snd.SetFragmented(false);
    snd.SetPFx(false);
    snd.SetPFxEnable(false);
    snd.SetShk(true);

    carrier->Position = bact->_position;
    carrier->Vector = bact->_fly_dir * bact->_fly_dir_length;

    SFXEngine::SFXe.startSound(carrier, 0);
    SFXEngine::SFXe.UpdateSoundCarrier(carrier);
}

void ypabact_UpdateMimicSoundCarrier(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_mimic_soundcarrier.Sounds.empty() )
        return;

    if ( bact->_status == BACT_STATUS_DEAD ||
         bact->_status == BACT_STATUS_NOPE ||
         (bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
    {
        SFXEngine::SFXe.StopCarrier(&bact->_mimic_soundcarrier);
        return;
    }

    bact->_mimic_soundcarrier.Position = bact->_soundcarrier.Position;
    bact->_mimic_soundcarrier.Vector = bact->_soundcarrier.Vector;

    TSoundSource &snd = bact->_mimic_soundcarrier.Sounds[0];
    if ( !snd.IsEnabled() && !snd.IsPFxEnabled() && !snd.IsShkEnabled() )
        SFXEngine::SFXe.startSound(&bact->_mimic_soundcarrier, 0);

    SFXEngine::SFXe.UpdateSoundCarrier(&bact->_mimic_soundcarrier);
}

bool ypabact_IsCarrierSpawnAliveUnit(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    if ( unit->_kidRef.IsListType(World::BLIST_CACHE) )
        return false;

    if ( unit->_status == BACT_STATUS_DEAD )
        return false;

    if ( unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2) )
        return false;

    return true;
}

int ypabact_CountCarrierSpawnedUnits(NC_STACK_ypabact *carrier)
{
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();

    if ( !world )
        return 0;

    int aliveCount = 0;

    for (auto it = carrier->_carrier_spawned_gids.begin(); it != carrier->_carrier_spawned_gids.end();)
    {
        NC_STACK_ypabact *unit = ypabact_FindLiveBactByGid(world->_unitsList, *it);

        if ( ypabact_IsCarrierSpawnAliveUnit(unit) )
        {
            aliveCount++;
            ++it;
        }
        else
        {
            it = carrier->_carrier_spawned_gids.erase(it);
        }
    }

    return aliveCount;
}

bool ypabact_CanCarrierSpawn(NC_STACK_ypabact *carrier)
{
    if ( !carrier || !carrier->getBACT_pWorld() )
        return false;

    if ( !carrier->_spawn_units )
        return false;

    if ( carrier->_spawn_vehicle <= 0 || (size_t)carrier->_spawn_vehicle >= carrier->getBACT_pWorld()->GetVhclProtos().size() )
        return false;

    if ( carrier->_spawn_trigger_radius <= 0.0 )
        return false;

    if ( carrier->_carrier_spawn_root_vehicle > 0 && carrier->_spawn_vehicle == carrier->_carrier_spawn_root_vehicle )
        return false;

    if ( carrier->_owner == World::OWNER_0 )
        return false;

    if ( carrier->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( carrier->_status == BACT_STATUS_DEAD ||
         carrier->_status == BACT_STATUS_CREATE ||
         carrier->_status == BACT_STATUS_BEAM )
        return false;

    if ( carrier->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER) )
        return false;

    return true;
}

bool NC_STACK_ypabact::CanUseCarrierSpawn()
{
    return ypabact_CanCarrierSpawn(this);
}

bool ypabact_IsCarrierSpawnEnemy(NC_STACK_ypabact *carrier, NC_STACK_ypabact *unit)
{
    if ( !ypabact_IsCarrierSpawnAliveUnit(unit) )
        return false;

    if ( unit == carrier )
        return false;

    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( unit->_status == BACT_STATUS_CREATE || unit->_status == BACT_STATUS_BEAM )
        return false;

    if ( unit->_owner == World::OWNER_0 || unit->_owner == carrier->_owner )
        return false;

    // OpenNeoUA invisible: cloaked stealth units are ignored by carrier spawn-trigger and
    // proximity-defense scans (both go through this predicate).
    if ( !unit->CanBeSeenByAIOrRadar() )
        return false;

    return true;
}

bool ypabact_CarrierHasEnemyNearby(NC_STACK_ypabact *carrier)
{
    return ypabact_HasEnemyNearby(carrier, carrier->_spawn_trigger_radius);
}

bool ypabact_IsCarrierSpawnPositionValid(NC_STACK_ypabact *carrier, const vec3d &pos)
{
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
    if ( !world )
        return false;

    yw_130arg sect;
    sect.pos_x = pos.x;
    sect.pos_z = pos.z;

    if ( !world->GetSectorInfo(&sect) || !sect.pcell )
        return false;

    return true;
}

bool ypabact_FindCarrierSpawnPosition(NC_STACK_ypabact *carrier, vec3d *outPos)
{
    int attempts = carrier->_spawn_random_pos > 0.0 ? 8 : 1;
    const vec3d basePos = carrier->_position + carrier->_rotation.Transpose().Transform(carrier->_spawn_offset);

    for (int i = 0; i < attempts; i++)
    {
        vec3d pos = basePos;

        if ( carrier->_spawn_random_pos > 0.0 )
        {
            float angle = ((float)rand() / (float)RAND_MAX) * (2.0 * C_PI);
            float dist = ((float)rand() / (float)RAND_MAX) * carrier->_spawn_random_pos;
            vec3d localOffset(cos(angle) * dist, 0.0, sin(angle) * dist);

            pos += carrier->_rotation.Transpose().Transform(localOffset);
        }

        if ( ypabact_IsCarrierSpawnPositionValid(carrier, pos) )
        {
            *outPos = pos;
            return true;
        }
    }

    if ( carrier->_spawn_random_pos > 0.0 && ypabact_IsCarrierSpawnPositionValid(carrier, basePos) )
    {
        *outPos = basePos;
        return true;
    }

    return false;
}

NC_STACK_ypabact *ypabact_CreateCarrierSpawnedUnit(NC_STACK_ypabact *carrier, const vec3d &pos)
{
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
    if ( !world )
        return nullptr;

    ypaworld_arg146 arg146;
    arg146.vehicle_id = carrier->_spawn_vehicle;
    arg146.pos = pos;

    NC_STACK_ypabact *unit = world->ypaworld_func146(&arg146);
    if ( !unit )
        return nullptr;

    unit->_owner = carrier->_owner;
    unit->_host_station = carrier->_host_station;
    unit->_carrier_spawn_root_gid = carrier->_carrier_spawn_root_gid ? carrier->_carrier_spawn_root_gid : carrier->_gid;
    unit->_carrier_spawn_root_vehicle = carrier->_carrier_spawn_root_vehicle > 0 ? carrier->_carrier_spawn_root_vehicle : carrier->_vehicleID;

    unit->InheritActiveDebuffFromParent(carrier);

    if ( unit->_spawn_units )
        unit->_spawn_last_time = unit->_clock > 0 ? unit->_clock : 1;

    NC_STACK_yparobo *carrierRobo = dynamic_cast<NC_STACK_yparobo *>(carrier);
    if ( !unit->_host_station )
        unit->_host_station = carrierRobo;

    unit->setBACT_bactCollisions(carrier->getBACT_bactCollisions());

    if ( !carrier->_spawn_instant )
    {
        setState_msg state;
        state.setFlags = 0;
        state.unsetFlags = 0;
        state.newStatus = BACT_STATUS_CREATE;
        unit->SetState(&state);

        unit->_scale_time = unit->_energy_max * 0.2;
    }

    world->HistoryAktCreate(unit);

    return unit;
}

void ypabact_AttachCarrierSpawnLeader(NC_STACK_ypabact *carrier, NC_STACK_ypabact *leader)
{
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
    if ( !world || !leader )
        return;

    NC_STACK_yparobo *carrierRobo = dynamic_cast<NC_STACK_yparobo *>(carrier);
    if ( carrierRobo )
        carrier->AddSubject(leader);
    else if ( carrier->_parent )
        carrier->_parent->AddSubject(leader);
    else
        world->ypaworld_func134(leader);
}

NC_STACK_yparobo *ypabact_FindSpawnAtDeathOwnerRobo(NC_STACK_ypaworld *world, uint8_t owner)
{
    if ( !world )
        return nullptr;

    for (NC_STACK_ypabact *unit : world->_unitsList)
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(unit);
        if ( robo &&
             robo->_owner == owner &&
             robo->_status != BACT_STATUS_DEAD &&
             !(robo->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
            return robo;
    }

    return nullptr;
}

bool ypabact_IsSpawnAtDeathPositionValid(NC_STACK_ypabact *parent, const vec3d &pos)
{
    NC_STACK_ypaworld *world = parent->getBACT_pWorld();
    if ( !world )
        return false;

    yw_130arg sect;
    sect.pos_x = pos.x;
    sect.pos_z = pos.z;

    if ( !world->GetSectorInfo(&sect) || !sect.pcell )
        return false;

    return true;
}

bool ypabact_FindSpawnAtDeathPosition(NC_STACK_ypabact *parent, vec3d *outPos)
{
    int attempts = parent->_at_death_spawn_random_pos > 0.0 ? 8 : 1;

    for (int i = 0; i < attempts; i++)
    {
        vec3d pos = parent->_position;

        if ( parent->_at_death_spawn_random_pos > 0.0 )
        {
            float angle = ((float)rand() / (float)RAND_MAX) * (2.0 * C_PI);
            float dist = ((float)rand() / (float)RAND_MAX) * parent->_at_death_spawn_random_pos;

            pos.x += cos(angle) * dist;
            pos.z += sin(angle) * dist;
        }

        if ( ypabact_IsSpawnAtDeathPositionValid(parent, pos) )
        {
            *outPos = pos;
            return true;
        }
    }

    if ( parent->_at_death_spawn_random_pos > 0.0 && ypabact_IsSpawnAtDeathPositionValid(parent, parent->_position) )
    {
        *outPos = parent->_position;
        return true;
    }

    return false;
}

void ypabact_EnableSpawnAtDeathProtection(NC_STACK_ypabact *unit, int immunityTime)
{
    if ( !unit || immunityTime <= 0 )
        return;

    unit->_at_death_spawn_protection_end_time = unit->_clock + immunityTime;
    unit->_at_death_spawn_restore_vulnerable = !unit->_invulnerable;
    unit->_invulnerable = true;
}

void ypabact_UpdateSpawnAtDeathProtection(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->_at_death_spawn_protection_end_time <= 0 )
        return;

    if ( unit->_clock < unit->_at_death_spawn_protection_end_time )
        return;

    if ( unit->_at_death_spawn_restore_vulnerable )
        unit->_invulnerable = false;

    unit->_at_death_spawn_protection_end_time = 0;
    unit->_at_death_spawn_restore_vulnerable = false;
}

NC_STACK_yparobo *ypabact_GetSpawnAtDeathOwnerRobo(NC_STACK_ypabact *parent)
{
    if ( !parent )
        return nullptr;

    if ( parent->_host_station &&
         parent->_host_station != parent &&
         parent->_host_station->_status != BACT_STATUS_DEAD &&
         !(parent->_host_station->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
        return parent->_host_station;

    return ypabact_FindSpawnAtDeathOwnerRobo(parent->getBACT_pWorld(), parent->_owner);
}

NC_STACK_ypabact *ypabact_CreateSpawnAtDeathUnit(NC_STACK_ypabact *parent, const vec3d &pos)
{
    NC_STACK_ypaworld *world = parent->getBACT_pWorld();
    if ( !world )
        return nullptr;

    ypaworld_arg146 arg146;
    arg146.vehicle_id = parent->_at_death_spawn_vehicle;
    arg146.pos = pos;

    NC_STACK_ypabact *unit = world->ypaworld_func146(&arg146);
    if ( !unit )
        return nullptr;

    unit->_owner = parent->_owner;
    unit->_carrier_spawn_root_gid = 0;
    unit->_carrier_spawn_root_vehicle = 0;
    unit->_aggr = parent->_aggr;
    ypabact_EnableSpawnAtDeathProtection(unit, parent->_at_death_spawn_immunity_time);

    if ( unit->_spawn_units )
        unit->_spawn_last_time = unit->_clock > 0 ? unit->_clock : 1;

    NC_STACK_yparobo *ownerRobo = ypabact_GetSpawnAtDeathOwnerRobo(parent);

    unit->_host_station = ownerRobo;
    if ( ownerRobo )
        unit->setBACT_bactCollisions(ownerRobo->getBACT_bactCollisions());
    else
        unit->setBACT_bactCollisions(parent->getBACT_bactCollisions());

    unit->InheritActiveDebuffFromParent(parent);

    setTarget_msg target;
    target.tgt_type = BACT_TGT_TYPE_CELL;
    target.priority = 0;
    target.tgt_pos = pos;
    unit->SetTarget(&target);

    if ( !parent->_at_death_spawn_instant )
    {
        setState_msg state;
        state.setFlags = 0;
        state.unsetFlags = 0;
        state.newStatus = BACT_STATUS_CREATE;
        unit->SetState(&state);
        unit->_scale_time = unit->_energy_max * 0.2;
    }

    world->HistoryAktCreate(unit);

    return unit;
}

void ypabact_AttachSpawnAtDeathLeader(NC_STACK_ypabact *parent, NC_STACK_ypabact *leader)
{
    NC_STACK_ypaworld *world = parent->getBACT_pWorld();
    if ( !world || !leader )
        return;

    NC_STACK_yparobo *ownerRobo = ypabact_GetSpawnAtDeathOwnerRobo(parent);
    if ( ownerRobo )
        ownerRobo->AddSubject(leader);
    else
        world->ypaworld_func134(leader);
}

void ypabact_TrySpawnAtDeath(NC_STACK_ypabact *parent)
{
    if ( !parent ||
         parent->_at_death_spawn_done ||
         !parent->_at_death_spawn_units ||
         parent->_bact_type == BACT_TYPES_MISSLE ||
         parent->_energy > 0 ||
         (parent->_status_flg & BACT_STFLAG_CLEAN) )
        return;

    parent->_at_death_spawn_done = true;

    NC_STACK_ypaworld *world = parent->getBACT_pWorld();
    if ( !world || world->_isNetGame )
        return;

    const std::vector<World::TVhclProto> &protos = world->GetVhclProtos();
    if ( parent->_at_death_spawn_vehicle <= 0 || (size_t)parent->_at_death_spawn_vehicle >= protos.size() )
        return;

    const World::TVhclProto &proto = protos[parent->_at_death_spawn_vehicle];
    if ( proto.model_id == BACT_TYPES_NOPE )
        return;

    int spawnCount = parent->_at_death_spawn_count > 0 ? parent->_at_death_spawn_count : 1;
    if ( spawnCount > 8 )
        spawnCount = 8;

    NC_STACK_ypabact *squadLeader = nullptr;
    int squadCommandId = dword_5B1128;

    for (int i = 0; i < spawnCount; i++)
    {
        vec3d spawnPos;
        if ( !ypabact_FindSpawnAtDeathPosition(parent, &spawnPos) )
            continue;

        NC_STACK_ypabact *unit = ypabact_CreateSpawnAtDeathUnit(parent, spawnPos);
        if ( !unit )
            continue;

        unit->_commandID = squadCommandId;

        if ( !squadLeader )
        {
            squadLeader = unit;
            ypabact_AttachSpawnAtDeathLeader(parent, squadLeader);
        }
        else
        {
            squadLeader->AddSubject(unit);
        }
    }

    if ( squadLeader )
        dword_5B1128++;
}

void NC_STACK_ypabact::BeforeSoundCarrierUpdate()
{
}

void NC_STACK_ypabact::UpdateCarrierSpawn(update_msg *)
{
    if ( !ypabact_CanCarrierSpawn(this) )
        return;

    // V1 is single-player/local only; avoiding unsynchronised network spawns is safer.
    if ( _world->_isNetGame )
        return;

    int interval = _spawn_interval > 0 ? _spawn_interval : 5000;
    if ( interval < 1000 )
        interval = 1000;

    if ( _spawn_last_time && _clock - _spawn_last_time < interval )
        return;

    int maxActive = _spawn_max_active > 0 ? _spawn_max_active : 1;
    int activeCount = ypabact_CountCarrierSpawnedUnits(this);
    if ( activeCount >= maxActive )
        return;

    if ( !ypabact_CarrierHasEnemyNearby(this) )
        return;

    _spawn_last_time = _clock;

    int spawnCount = _spawn_count > 0 ? _spawn_count : 1;
    if ( spawnCount > 8 )
        spawnCount = 8;

    int remainingSlots = maxActive - activeCount;
    if ( spawnCount > remainingSlots )
        spawnCount = remainingSlots;

    NC_STACK_ypabact *squadLeader = nullptr;
    int squadCommandId = dword_5B1128;

    for (int i = 0; i < spawnCount; i++)
    {
        vec3d spawnPos;
        if ( !ypabact_FindCarrierSpawnPosition(this, &spawnPos) )
            continue;

        NC_STACK_ypabact *unit = ypabact_CreateCarrierSpawnedUnit(this, spawnPos);
        if ( !unit )
            continue;

        unit->_commandID = squadCommandId;

        if ( !squadLeader )
        {
            squadLeader = unit;
            ypabact_AttachCarrierSpawnLeader(this, squadLeader);
        }
        else
        {
            squadLeader->AddSubject(unit);
        }

        _carrier_spawned_gids.push_back(unit->_gid);
    }

    if ( squadLeader )
        dword_5B1128++;
}
