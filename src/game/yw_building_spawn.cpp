// Buildings that spawn units: candidate cells, positions and spawned unit bookkeeping.
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

bool yw_IsAliveBuildingSpawnedUnit(NC_STACK_ypabact *unit)
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

int yw_CountBuildingSpawnedUnits(NC_STACK_ypaworld *world, cellArea &cell)
{
    int aliveCount = 0;

    for (auto it = cell.BuildingSpawnedGids.begin(); it != cell.BuildingSpawnedGids.end();)
    {
        NC_STACK_ypabact *unit = yw_FindLiveBactByGidInList(world->_unitsList, *it);

        if ( yw_IsAliveBuildingSpawnedUnit(unit) )
        {
            aliveCount++;
            ++it;
        }
        else
        {
            it = cell.BuildingSpawnedGids.erase(it);
        }
    }

    return aliveCount;
}

bool yw_IsBuildingSpawnEnemy(uint8_t owner, NC_STACK_ypabact *unit)
{
    if ( !yw_IsAliveBuildingSpawnedUnit(unit) )
        return false;

    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( unit->_status == BACT_STATUS_CREATE || unit->_status == BACT_STATUS_BEAM )
        return false;

    if ( unit->_owner == World::OWNER_0 || unit->_owner == owner )
        return false;

    return true;
}

bool yw_IsActiveBuildingSpawnerCell(cellArea &cell)
{
    if ( cell.PurposeType != cellArea::PT_BUILDINGS &&
         cell.PurposeType != cellArea::PT_POWERSTATION )
        return false;

    return cell.GetEnergy() > 0;
}

bool yw_BuildingHasEnemyNearbyInUnitTree(uint8_t owner, const vec3d &centerPos, float radiusSq, NC_STACK_ypabact *unit)
{
    if ( yw_IsBuildingSpawnEnemy(owner, unit) &&
         (unit->_position.XZ() - centerPos.XZ()).square() <= radiusSq )
        return true;

    if ( unit )
    {
        for (NC_STACK_ypabact *kid : unit->_kidList)
        {
            if ( yw_BuildingHasEnemyNearbyInUnitTree(owner, centerPos, radiusSq, kid) )
                return true;
        }
    }

    return false;
}

bool yw_BuildingHasEnemyNearby(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto)
{
    float radius = proto.spawn_trigger_radius;
    float radiusSq = radius * radius;

    for (NC_STACK_ypabact *unit : world->_unitsList)
        if ( yw_BuildingHasEnemyNearbyInUnitTree(cell.owner, cell.CenterPos, radiusSq, unit) )
            return true;

    return false;
}

bool yw_IsBuildingSpawnCandidateCell(cellArea &buildingCell, cellArea *spawnCell, const char **failReason)
{
    if ( !spawnCell )
    {
        if ( failReason )
            *failReason = "no sector";
        return false;
    }

    if ( spawnCell == &buildingCell )
    {
        if ( failReason )
            *failReason = "building center sector";
        return false;
    }

    if ( !spawnCell->IsGamePlaySector() )
    {
        if ( failReason )
            *failReason = "border sector";
        return false;
    }

    if ( spawnCell->PurposeType != cellArea::PT_NONE )
    {
        if ( failReason )
            *failReason = "occupied purpose sector";
        return false;
    }

    return true;
}

bool yw_AdjustBuildingSpawnHeight(NC_STACK_ypaworld *world, const World::TBuildingProto &proto, vec3d *pos, const char **failReason)
{
    const std::vector<World::TVhclProto> &protos = world->GetVhclProtos();
    if ( proto.spawn_vehicle <= 0 || (size_t)proto.spawn_vehicle >= protos.size() )
    {
        if ( failReason )
            *failReason = "bad vehicle id";
        return false;
    }

    ypaworld_arg136 ground;
    ground.stPos = pos->X0Z() - vec3d::OY(30000.0);
    ground.vect = vec3d::OY(50000.0);
    ground.flags = 0;

    world->ypaworld_func136(&ground);
    if ( !ground.isect )
    {
        if ( failReason )
            *failReason = "no ground hit";
        return false;
    }

    // Building spawners are intended for airborne defensive units.
    // Spawn them above the building cell, not at ground level.
    // UA uses negative Y for higher altitude, so subtracting extraHeight
    // places the unit above the terrain/building.
    float minHeight = std::isfinite(proto.spawn_height_min) && proto.spawn_height_min >= 0.0f
                    ? proto.spawn_height_min : 650.0f;
    float maxHeight = std::isfinite(proto.spawn_height_max) && proto.spawn_height_max >= 0.0f
                    ? proto.spawn_height_max : 900.0f;
    if ( maxHeight < minHeight )
        std::swap(minHeight, maxHeight);

    float extraHeight = minHeight;
    if ( maxHeight > minHeight )
        extraHeight += ((float)rand() / (float)RAND_MAX) * (maxHeight - minHeight);

    pos->y = ground.isectPos.y - protos[proto.spawn_vehicle].overeof - extraHeight;
    return true;
}

bool yw_IsBuildingSpawnPositionClear(NC_STACK_ypaworld *world, cellArea &buildingCell, const World::TBuildingProto &proto, vec3d *pos, const char **failReason)
{
    yw_130arg sect;
    sect.pos_x = pos->x;
    sect.pos_z = pos->z;

    if ( !world->GetSectorInfo(&sect) )
    {
        if ( failReason )
            *failReason = "outside map";
        return false;
    }

    // Building spawners intentionally spawn inside their own building cell,
    // above the powerstation/building. Adjacent cells are still validated
    // by the old candidate rules if future code ever uses them again.
    if ( sect.pcell != &buildingCell && !yw_IsBuildingSpawnCandidateCell(buildingCell, sect.pcell, failReason) )
        return false;

    float localX = pos->x - sect.pcell->CenterPos.x;
    float localZ = pos->z - sect.pcell->CenterPos.z;
    float edgeLimit = World::CVSectorHalfLength - 180.0;
    if ( fabs(localX) > edgeLimit || fabs(localZ) > edgeLimit )
    {
        if ( failReason )
            *failReason = "too close to sector edge";
        return false;
    }

    if ( !yw_AdjustBuildingSpawnHeight(world, proto, pos, failReason) )
        return false;

    float spawnRadius = 20.0;
    const std::vector<World::TVhclProto> &protos = world->GetVhclProtos();

    if ( proto.spawn_vehicle > 0 && (size_t)proto.spawn_vehicle < protos.size() && protos[proto.spawn_vehicle].radius > 0.0 )
        spawnRadius = protos[proto.spawn_vehicle].radius;

    for (NC_STACK_ypabact *unit : sect.pcell->unitsList)
    {
        if ( !yw_IsAliveBuildingSpawnedUnit(unit) || unit->_bact_type == BACT_TYPES_MISSLE )
            continue;

        float otherRadius = unit->_radius > 0.0 ? unit->_radius : 20.0;
        float minDist = spawnRadius + otherRadius + 20.0;

        if ( (unit->_position.XZ() - pos->XZ()).square() < minDist * minDist )
        {
            if ( failReason )
                *failReason = "unit collision";
            return false;
        }
    }

    return true;
}

void yw_LogBuildingSpawnPositionFailure(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, const char *reason)
{
    static int32_t lastLogTime = 0;

    if ( !world )
        return;

    if ( lastLogTime && world->_timeStamp - lastLogTime < 3000 )
        return;

    lastLogTime = world->_timeStamp;
    ypa_log_out("Building spawner: no safe spawn position building=%d vehicle=%d cell=%d,%d reason=%s\n",
                proto.Index,
                proto.spawn_vehicle,
                cell.CellId.x,
                cell.CellId.y,
                reason ? reason : "unknown");
}

NC_STACK_yparobo *yw_FindBuildingSpawnOwnerRobo(NC_STACK_ypaworld *world, uint8_t owner)
{
    if ( !world || owner == World::OWNER_0 )
        return nullptr;

    for (NC_STACK_ypabact *unit : world->_unitsList)
    {
        if ( unit->_bact_type != BACT_TYPES_ROBO ||
             unit->_owner != owner ||
             unit->_status == BACT_STATUS_DEAD ||
             unit->_kidRef.IsListType(World::BLIST_CACHE) ||
             (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
            continue;

        return dynamic_cast<NC_STACK_yparobo *>(unit);
    }

    return nullptr;
}

void yw_LogBuildingSpawnOwnerFailure(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto)
{
    static int32_t lastLogTime = 0;

    if ( !world )
        return;

    if ( lastLogTime && world->_timeStamp - lastLogTime < 3000 )
        return;

    lastLogTime = world->_timeStamp;
    ypa_log_out("Building spawner: no live owner robo building=%d owner=%d vehicle=%d cell=%d,%d\n",
                proto.Index,
                cell.owner,
                proto.spawn_vehicle,
                cell.CellId.x,
                cell.CellId.y);
}

bool yw_FindBuildingSpawnPosition(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, vec3d *outPos)
{
    const char *lastFailReason = "no candidate";

    // Building spawners are now intentionally local: spawned units appear
    // above the same powerstation/building cell, with limited random X/Z
    // jitter. This makes Sulg infected powerstations look like they are
    // emitting airborne spores from themselves instead of from nearby sectors.
    const float randomXZ = std::isfinite(proto.spawn_random_pos) && proto.spawn_random_pos > 0.0f
                         ? proto.spawn_random_pos : 0.0f;
    const int maxAttempts = randomXZ > 0.0f ? 16 : 1;

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
        vec3d pos = cell.CenterPos;
        pos.x += proto.spawn_offset.x;
        pos.z += proto.spawn_offset.z;

        if ( randomXZ > 0.0f )
        {
            float rndX = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            float rndZ = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            pos.x += rndX * randomXZ;
            pos.z += rndZ * randomXZ;
        }

        if ( yw_IsBuildingSpawnPositionClear(world, cell, proto, &pos, &lastFailReason) )
        {
            *outPos = pos;
            return true;
        }
    }

    yw_LogBuildingSpawnPositionFailure(world, cell, proto, lastFailReason);
    return false;
}

NC_STACK_ypabact *yw_CreateBuildingSpawnedUnit(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, const vec3d &pos)
{
    NC_STACK_yparobo *ownerRobo = yw_FindBuildingSpawnOwnerRobo(world, cell.owner);
    if ( !ownerRobo )
    {
        yw_LogBuildingSpawnOwnerFailure(world, cell, proto);
        return nullptr;
    }

    ypaworld_arg146 arg146;
    arg146.vehicle_id = proto.spawn_vehicle;
    arg146.pos = pos;

    NC_STACK_ypabact *unit = world->ypaworld_func146(&arg146);
    if ( !unit )
        return nullptr;

    unit->_owner = cell.owner;
    unit->_host_station = ownerRobo;
    unit->_carrier_spawn_root_gid = 0;
    unit->_carrier_spawn_root_vehicle = 0;

    // Building spawners are still part of the owning Host Station's production
    // tree.  If that Host Station is carrying an inheritable debuff, every unit
    // produced afterwards must start with the same remaining debuff state.
    unit->InheritActiveDebuffFromParent(ownerRobo);

    if ( unit->_spawn_units )
        unit->_spawn_last_time = unit->_clock > 0 ? unit->_clock : 1;

    unit->setBACT_bactCollisions(ownerRobo->getBACT_bactCollisions());
    ownerRobo->AddSubject(unit);

    setTarget_msg target;
    target.tgt_type = BACT_TGT_TYPE_CELL;
    target.priority = 0;
    target.tgt_pos = pos;
    unit->SetTarget(&target);

    if ( !proto.spawn_instant )
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

void yw_UpdateBuildingSpawner(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto)
{
    if ( !world || world->_isNetGame )
        return;

    if ( !proto.spawn_units )
        return;

    if ( cell.owner == World::OWNER_0 || cell.owner == World::OWNER_7 )
        return;

    if ( cell.BuildingSpawnInitialOwner == 0 )
        cell.BuildingSpawnInitialOwner = cell.owner;

    if ( cell.owner != cell.BuildingSpawnInitialOwner )
        return;

    if ( proto.spawn_vehicle <= 0 || (size_t)proto.spawn_vehicle >= world->GetVhclProtos().size() )
        return;

    if ( !yw_IsActiveBuildingSpawnerCell(cell) )
        return;

    if ( proto.spawn_trigger_radius <= 0.0 )
        return;

    int interval = proto.spawn_interval > 0 ? proto.spawn_interval : 5000;
    if ( interval < 1000 )
        interval = 1000;

    if ( cell.BuildingSpawnLastTime && world->_timeStamp - cell.BuildingSpawnLastTime < interval )
        return;

    int maxActive = proto.spawn_max_active > 0 ? proto.spawn_max_active : 1;
    int activeCount = yw_CountBuildingSpawnedUnits(world, cell);
    if ( activeCount >= maxActive )
        return;

    if ( !yw_BuildingHasEnemyNearby(world, cell, proto) )
        return;

    cell.BuildingSpawnLastTime = world->_timeStamp;

    int spawnCount = proto.spawn_count > 0 ? proto.spawn_count : 1;
    if ( spawnCount > 8 )
        spawnCount = 8;

    int remainingSlots = maxActive - activeCount;
    if ( spawnCount > remainingSlots )
        spawnCount = remainingSlots;

    for (int i = 0; i < spawnCount; i++)
    {
        vec3d spawnPos;
        if ( !yw_FindBuildingSpawnPosition(world, cell, proto, &spawnPos) )
            continue;

        NC_STACK_ypabact *unit = yw_CreateBuildingSpawnedUnit(world, cell, proto, spawnPos);
        if ( unit )
            cell.BuildingSpawnedGids.push_back(unit->_gid);
    }
}
