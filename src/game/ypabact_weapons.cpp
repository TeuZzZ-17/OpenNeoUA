// Weapon slot selection, projectile counts, multi-target and homing target cycling.
#include "game/ypabact_internal.h"

bool ypabact_IsValidWeaponId(NC_STACK_ypabact *bact, int weaponId)
{
    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    return world && weaponId >= 0 && weaponId < (int)world->GetWeaponsProtos().size();
}

bool ypabact_IsValidFireWeaponId(NC_STACK_ypabact *bact, int weaponId)
{
    if ( !ypabact_IsValidWeaponId(bact, weaponId) )
        return false;

    const World::TWeapProto &weapon =
        bact->getBACT_pWorld()->GetWeaponsProtos().at(weaponId);
    return (weapon._weaponFlags & World::TWeapProto::WEAPON_FLAG_PROJECTILE) != 0 &&
           !weapon.IsKamikaze();
}

int ypabact_GetPrimaryWeaponSlots(NC_STACK_ypabact *bact, int *outSlots, int *outSourceSlots)
{
    int count = 0;

    // This list is intentionally fireable/selectable, not merely mounted.
    // model=kamikaze remains in the raw Vehicle slots for arming resolution,
    // but never enters player/AI selection, cycling, Random or Sequence.
    if ( ypabact_IsValidFireWeaponId(bact, bact->_weapon) )
    {
        outSlots[count] = bact->_weapon;
        if ( outSourceSlots )
            outSourceSlots[count] = 0;
        count++;
    }

    for (size_t extraSlot = 0; extraSlot < bact->_extra_weapons.size(); extraSlot++)
    {
        int weaponId = bact->_extra_weapons[extraSlot];
        if ( weaponId > 0 && ypabact_IsValidFireWeaponId(bact, weaponId) )
        {
            outSlots[count] = weaponId;
            if ( outSourceSlots )
                outSourceSlots[count] = (int)extraSlot + 1;
            count++;
        }
    }

    return count;
}

int ypabact_NormalizeWeaponProjectileCount(int count)
{
    return count <= 1 ? 1 : count;
}

void ypabact_GetWeaponProjectileCountRangeForSourceSlot(
        NC_STACK_ypabact *bact, int sourceSlot, int *outMin, int *outMax)
{
    int minCount = 1;
    int maxCount = 1;

    if ( bact )
    {
        if ( sourceSlot >= 0 && sourceSlot < (int)bact->_weapon_projectile_counts.size() )
        {
            int rawMin = bact->_weapon_projectile_counts[sourceSlot];
            int rawMax = bact->_weapon_projectile_count_maxs[sourceSlot];
            if ( rawMax == 0 && rawMin > 0 )
                rawMax = rawMin;

            minCount = ypabact_NormalizeWeaponProjectileCount(rawMin);
            maxCount = ypabact_NormalizeWeaponProjectileCount(rawMax);
        }
        else
        {
            minCount = ypabact_NormalizeWeaponProjectileCount(bact->_num_weapons);
            maxCount = minCount;
        }
    }

    if ( maxCount < minCount )
        std::swap(minCount, maxCount);

    if ( outMin )
        *outMin = minCount;
    if ( outMax )
        *outMax = maxCount;
}

int ypabact_GetWeaponProjectileCountForSourceSlot(NC_STACK_ypabact *bact,
                                                          int sourceSlot)
{
    int minCount = 1;
    int maxCount = 1;
    ypabact_GetWeaponProjectileCountRangeForSourceSlot(
        bact, sourceSlot, &minCount, &maxCount);
    return maxCount;
}

int ypabact_RollWeaponProjectileCountForSourceSlot(NC_STACK_ypabact *bact,
                                                           int sourceSlot)
{
    int minCount = 1;
    int maxCount = 1;
    ypabact_GetWeaponProjectileCountRangeForSourceSlot(
        bact, sourceSlot, &minCount, &maxCount);

    if ( minCount == maxCount )
        return minCount;

    const int span = maxCount - minCount + 1;
    const double randomPart = (double)rand() / ((double)RAND_MAX + 1.0);
    return minCount + (int)(randomPart * (double)span);
}

int ypabact_GetWeaponIdForSourceSlot(NC_STACK_ypabact *bact, int sourceSlot)
{
    if ( !bact )
        return -1;

    if ( sourceSlot == 0 )
        return bact->_weapon;

    if ( sourceSlot > 0 && sourceSlot <= (int)bact->_extra_weapons.size() )
        return bact->_extra_weapons[sourceSlot - 1];

    return -1;
}

bool ypabact_GetWeaponEnergyForTarget(const World::TWeapProto &wproto,
                                              NC_STACK_ypabact *target,
                                              float *outEnergy,
                                              bool *outDefined)
{
    if ( outEnergy )
        *outEnergy = 0.0f;
    if ( outDefined )
        *outDefined = false;

    if ( !target )
        return false;

    float energy = 0.0f;
    bool defined = false;
    bool supported = true;

    switch ( target->_bact_type )
    {
    case BACT_TYPES_BACT: // model = heli
        energy = wproto.energy_heli;
        defined = wproto.energy_heli_defined;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        energy = wproto.energy_tank;
        defined = wproto.energy_tank_defined;
        break;

    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
        energy = wproto.energy_flyer;
        defined = wproto.energy_flyer_defined;
        break;

    case BACT_TYPES_ROBO:
        energy = wproto.energy_robo;
        defined = wproto.energy_robo_defined;
        break;

    default:
        // Classes without an override use the same generic energy as a real hit.
        energy = 1.0f;
        defined = true;
        supported = true;
        break;
    }

    float specificEnergy = 1.0f;
    if ( World::TryGetSpecificWeaponEnergy(
             wproto, World::ResolveVehicleCombatClass(target), &specificEnergy) )
    {
        energy = specificEnergy;
        defined = true;
        supported = true;
    }

    if ( !supported )
        return false;

    if ( outEnergy )
        *outEnergy = energy;
    if ( outDefined )
        *outDefined = defined;
    return true;
}

int ypabact_SelectRandomPrimaryWeaponSlot(NC_STACK_ypabact *bact, int *outSourceSlot)
{
    if ( outSourceSlot )
        *outSourceSlot = -1;

    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots, sourceSlots);
    if ( count <= 0 )
        return -1;

    int index = count > 1 ? rand() % count : 0;
    if ( outSourceSlot )
        *outSourceSlot = sourceSlots[index];
    return slots[index];
}

int ypabact_SelectSmartAIPrimaryWeaponSlot(NC_STACK_ypabact *bact,
                                                   NC_STACK_ypabact *target,
                                                   int *outSourceSlot)
{
    if ( outSourceSlot )
        *outSourceSlot = -1;

    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots, sourceSlots);
    if ( count <= 0 )
        return -1;
    if ( count == 1 )
    {
        if ( outSourceSlot )
            *outSourceSlot = sourceSlots[0];
        return slots[0];
    }

    float bestEnergy = -std::numeric_limits<float>::infinity();
    int bestIndices[4];
    int bestCount = 0;

    for (int i = 0; i < count; ++i)
    {
        const World::TWeapProto &wproto = bact->getBACT_pWorld()->GetWeaponsProtos().at(slots[i]);
        float energy = 0.0f;
        bool defined = false;

        // Compare the actual weapon energy with the target multiplier. Missing
        // fine-class keys retain their normal legacy/generic hit semantics.
        if ( !ypabact_GetWeaponEnergyForTarget(wproto, target, &energy, &defined) ||
             !isfinite(energy) )
            continue;

        energy *= std::max(0, wproto.energy);

        if ( bestCount == 0 || energy > bestEnergy )
        {
            bestEnergy = energy;
            bestIndices[0] = i;
            bestCount = 1;
        }
        else if ( energy == bestEnergy )
        {
            bestIndices[bestCount++] = i;
        }
    }

    if ( bestCount == 0 )
        return ypabact_SelectRandomPrimaryWeaponSlot(bact, outSourceSlot);
    int bestIndex = bestIndices[bestCount > 1 ? rand() % bestCount : 0];
    if ( outSourceSlot )
        *outSourceSlot = sourceSlots[bestIndex];
    return slots[bestIndex];
}

int ypabact_SelectAIPrimaryWeaponSlot(NC_STACK_ypabact *bact,
                                              NC_STACK_ypabact *target,
                                              int *outSourceSlot)
{
    if ( outSourceSlot )
        *outSourceSlot = -1;

    if ( !bact )
        return -1;

    switch ( bact->_weapon_ai_switch_mode )
    {
    case World::TVhclProto::WEAPON_AI_SWITCH_MODE_RANDOM:
        return ypabact_SelectRandomPrimaryWeaponSlot(bact, outSourceSlot);

    case World::TVhclProto::WEAPON_AI_SWITCH_MODE_SMART:
        return ypabact_SelectSmartAIPrimaryWeaponSlot(bact, target, outSourceSlot);

    case World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE:
    default:
        break;
    }

    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots, sourceSlots);
    if ( count <= 0 )
        return -1;

    int index = bact->_weapon_slot_index % count;
    if ( index < 0 )
        index = 0;

    if ( outSourceSlot )
        *outSourceSlot = sourceSlots[index];
    return slots[index];
}

int ypabact_GetCurrentPrimaryWeaponSourceSlot(NC_STACK_ypabact *bact)
{
    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots, sourceSlots);

    if ( count <= 0 )
        return 0;

    if ( count == 1 )
        return sourceSlots[0];

    const bool keepResolvedSlot =
        (bact->_oflags & BACT_OFLAG_USERINPT)
            ? bact->_weapon_player_switch_mode == World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_RANDOM
            : (bact->_weapon_ai_switch_mode == World::TVhclProto::WEAPON_AI_SWITCH_MODE_RANDOM ||
               bact->_weapon_ai_switch_mode == World::TVhclProto::WEAPON_AI_SWITCH_MODE_SMART);

    if ( keepResolvedSlot )
    {
        int sourceSlot = bact->_current_weapon_source_slot;
        int weaponId = ypabact_GetWeaponIdForSourceSlot(bact, sourceSlot);
        if ( weaponId == bact->_current_weapon_id && ypabact_IsValidFireWeaponId(bact, weaponId) )
            return sourceSlot;

        return sourceSlots[0];
    }

    int index = bact->_weapon_slot_index % count;
    if ( index < 0 )
        index = 0;

    return sourceSlots[index];
}

bool ypabact_IsHomingBombWeapon(const World::TWeapProto &wproto)
{
    return wproto.IsHomingBomb();
}

bool ypabact_UsesMissileTargeting(const World::TWeapProto &wproto)
{
    return wproto._weaponFlags == YPA_WEAPON_FLAGS_MISSILE;
}

bool ypabact_IsCompatibleMultiTargetWeapon(const World::TWeapProto &wproto)
{
    return ypabact_UsesMissileTargeting(wproto) ||
           ypabact_IsHomingBombWeapon(wproto);
}

int ypabact_GetMultiTargetLimit(const World::TWeapProto &wproto, int weaponCount)
{
    if ( !ypabact_IsCompatibleMultiTargetWeapon(wproto) || wproto.multi_target <= 1 || weaponCount <= 1 )
        return 0;

    int maxTargets = wproto.multi_target;
    if ( maxTargets > weaponCount )
        maxTargets = weaponCount;

    return maxTargets;
}

bool ypabact_IsMissileMultiTargetUnitType(NC_STACK_ypabact *target)
{
    switch ( target->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_ROBO:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
    case BACT_TYPES_GUN:
        return true;

    default:
        return false;
    }
}

bool ypabact_IsValidMissileMultiTarget(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target)
{
    if ( !launcher || !target || launcher == target || target->_isDummy || !launcher->getBACT_pWorld() )
        return false;

    if ( !launcher->IsPrimaryWeaponElevationAllowed(target->_position - launcher->_position) )
        return false;

    if ( !target->CanBeSeenByAIOrRadar() )
        return false;

    if ( target->getBACT_pWorld() != launcher->getBACT_pWorld() )
        return false;

    if ( launcher->_owner == World::OWNER_0 || target->_owner == World::OWNER_0 || target->_owner == launcher->_owner )
        return false;

    if ( !ypabact_IsMissileMultiTargetUnitType(target) )
        return false;

    if ( target->_bact_type == BACT_TYPES_MISSLE ||
         target->_status == BACT_STATUS_DEAD ||
         target->_status == BACT_STATUS_CREATE ||
         target->_status == BACT_STATUS_BEAM ||
         target->_energy <= 0 ||
         target->_energy_max <= 0 ||
         target->IsDestroyed() ||
         (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
        return false;

    if ( target->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(target);
        if ( !gun || (gun->IsRoboGun() && target->GetEffectiveShield() >= 100.0f) )
            return false;
    }

    return true;
}

bool ypabact_GetMissileMultiTargetScore(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target, const vec3d &aimDir, float weaponRadius, float *outScore)
{
    if ( !ypabact_IsValidMissileMultiTarget(launcher, target) )
        return false;

    vec3d targetDelta = target->_position - launcher->_old_pos;
    if ( targetDelta.dot(aimDir) < 0.0 )
        return false;

    float targetDistance = targetDelta.length();
    if ( targetDistance >= YPA_MISSILE_MULTI_TARGET_RANGE )
        return false;

    vec3d lineDelta = aimDir * targetDelta;
    float lineDistance = lineDelta.length();
    float lockRadius = targetDistance * 0.5 + 20.0;

    if ( lineDistance >= lockRadius && lineDistance >= target->_radius + weaponRadius )
        return false;

    if ( outScore )
        *outScore = lineDistance + targetDistance * 0.001;

    return true;
}

void ypabact_AddMissileMultiTargetCandidate(std::vector<TMissileMultiTargetCandidate> &targets, NC_STACK_ypabact *target, float score)
{
    if ( !target )
        return;

    for (const TMissileMultiTargetCandidate &candidate : targets)
    {
        if ( candidate.target == target )
            return;
    }

    TMissileMultiTargetCandidate candidate;
    candidate.target = target;
    candidate.score = score;
    targets.push_back(candidate);
}

void ypabact_CollectMissileMultiTargetsFromCell(std::vector<TMissileMultiTargetCandidate> &candidates, NC_STACK_ypabact *launcher, cellArea *cell, const vec3d &aimDir, float weaponRadius)
{
    if ( !cell )
        return;

    for( NC_STACK_ypabact* &unit : cell->unitsList )
    {
        float score = 0.0;
        if ( ypabact_GetMissileMultiTargetScore(launcher, unit, aimDir, weaponRadius, &score) )
            ypabact_AddMissileMultiTargetCandidate(candidates, unit, score);
    }
}

void ypabact_CollectHomingBombTargetsFromCell(std::vector<TMissileMultiTargetCandidate> &candidates, NC_STACK_ypabact *launcher, cellArea *cell, const vec3d &referencePos, float weaponRadius)
{
    if ( !cell )
        return;

    for( NC_STACK_ypabact* &unit : cell->unitsList )
    {
        if ( !ypabact_IsValidMissileMultiTarget(launcher, unit) )
            continue;

        vec3d targetDelta = unit->_position - referencePos;
        float horizontalDistance = targetDelta.XZ().length();
        if ( horizontalDistance >= YPA_MISSILE_MULTI_TARGET_RANGE )
            continue;

        float score = horizontalDistance + fabs(targetDelta.y) * 0.05 + weaponRadius * 0.001;
        ypabact_AddMissileMultiTargetCandidate(candidates, unit, score);
    }
}

std::vector<NC_STACK_ypabact *> ypabact_CollectMissileMultiTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int maxTargets, bool useTargetAimDir)
{
    std::vector<TMissileMultiTargetCandidate> candidates;
    std::vector<NC_STACK_ypabact *> targets;

    if ( !launcher || !arg || !launcher->getBACT_pWorld() || maxTargets <= 0 )
        return targets;

    vec3d aimDir = arg->direction;
    if ( useTargetAimDir )
    {
        if ( arg->tgType == BACT_TGT_TYPE_UNIT && arg->target.pbact )
            aimDir = arg->target.pbact->_position - launcher->_old_pos;
        else if ( arg->tgType == BACT_TGT_TYPE_CELL )
            aimDir = arg->tgt_pos - launcher->_old_pos;
    }

    float aimLen = aimDir.length();
    if ( aimLen <= 0.001 )
    {
        aimDir = launcher->_rotation.AxisZ();
        aimLen = aimDir.length();
    }

    if ( aimLen <= 0.001 )
        return targets;

    aimDir = aimDir / aimLen;

    if ( arg->tgType == BACT_TGT_TYPE_UNIT && ypabact_IsValidMissileMultiTarget(launcher, arg->target.pbact) )
        ypabact_AddMissileMultiTargetCandidate(candidates, arg->target.pbact, -1.0);

    int sectorRadius = (int)(YPA_MISSILE_MULTI_TARGET_RANGE / World::CVSectorLength) + 2;
    for (int y = -sectorRadius; y <= sectorRadius; y++)
    {
        for (int x = -sectorRadius; x <= sectorRadius; x++)
        {
            Common::Point cellId(launcher->_cellId.x + x, launcher->_cellId.y + y);

            if ( !launcher->getBACT_pWorld()->IsGamePlaySector(cellId) )
                continue;

            ypabact_CollectMissileMultiTargetsFromCell(candidates, launcher, &launcher->getBACT_pWorld()->SectorAt(cellId), aimDir, wproto.radius);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const TMissileMultiTargetCandidate &a, const TMissileMultiTargetCandidate &b) {
        return a.score < b.score;
    });

    if ( maxTargets <= 0 || maxTargets > (int)candidates.size() )
        maxTargets = candidates.size();

    for (int i = 0; i < maxTargets; i++)
        targets.push_back(candidates[i].target);

    return targets;
}

std::vector<NC_STACK_ypabact *> ypabact_CollectHomingBombTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int maxTargets)
{
    std::vector<TMissileMultiTargetCandidate> candidates;
    std::vector<NC_STACK_ypabact *> targets;

    if ( !launcher || !arg || !launcher->getBACT_pWorld() || maxTargets <= 0 )
        return targets;

    if ( arg->tgType == BACT_TGT_TYPE_UNIT && ypabact_IsValidMissileMultiTarget(launcher, arg->target.pbact) )
        ypabact_AddMissileMultiTargetCandidate(candidates, arg->target.pbact, -1.0);

    vec3d referencePos = launcher->_position;
    if ( arg->tgType == BACT_TGT_TYPE_CELL )
        referencePos = arg->tgt_pos;

    int sectorRadius = (int)(YPA_MISSILE_MULTI_TARGET_RANGE / World::CVSectorLength) + 2;
    for (int y = -sectorRadius; y <= sectorRadius; y++)
    {
        for (int x = -sectorRadius; x <= sectorRadius; x++)
        {
            Common::Point cellId(launcher->_cellId.x + x, launcher->_cellId.y + y);

            if ( !launcher->getBACT_pWorld()->IsGamePlaySector(cellId) )
                continue;

            ypabact_CollectHomingBombTargetsFromCell(candidates, launcher, &launcher->getBACT_pWorld()->SectorAt(cellId), referencePos, wproto.radius);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const TMissileMultiTargetCandidate &a, const TMissileMultiTargetCandidate &b) {
        return a.score < b.score;
    });

    if ( maxTargets <= 0 || maxTargets > (int)candidates.size() )
        maxTargets = candidates.size();

    for (int i = 0; i < maxTargets; i++)
        targets.push_back(candidates[i].target);

    return targets;
}

void ypabact_StoreHUDMissileMultiLockTargets(NC_STACK_ypabact *launcher, const std::vector<NC_STACK_ypabact *> &missileTargets)
{
    if ( !launcher || !launcher->getBACT_pWorld() || !(launcher->_oflags & BACT_OFLAG_USERINPT) )
        return;

    if ( missileTargets.size() > 1 )
        launcher->getBACT_pWorld()->_hudMissileMultiLockTargets = missileTargets;
    else
        launcher->getBACT_pWorld()->_hudMissileMultiLockTargets.clear();
}

void ypabact_UpdateHUDWeaponMultiLockTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int weaponCount)
{
    if ( !launcher || !launcher->getBACT_pWorld() || !(launcher->_oflags & BACT_OFLAG_USERINPT) )
        return;

    int maxTargets = ypabact_GetMultiTargetLimit(wproto, weaponCount);
    if ( maxTargets <= 1 )
    {
        launcher->getBACT_pWorld()->_hudMissileMultiLockTargets.clear();
        return;
    }

    std::vector<NC_STACK_ypabact *> targets;
    if ( ypabact_UsesMissileTargeting(wproto) )
        targets = ypabact_CollectMissileMultiTargets(launcher, arg, wproto, maxTargets);

    // Homing bombs intentionally keep automatic targeting with no multi-lock HUD.
    // This preview path is reserved for manually cycled missile targets.
    ypabact_StoreHUDMissileMultiLockTargets(launcher, targets);
}

bool ypabact_IsHomingCycleTargetLockable(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target,
                                                 const World::TWeapProto &wproto, const vec3d &requestedAimDir)
{
    // Manual Cycle Target follows the shared missile lock path. Homing bombs
    // select automatically and purely ballistic Arc Grenades stay excluded.
    if ( !launcher || !target || !ypabact_UsesMissileTargeting(wproto) )
        return false;

    vec3d aimDir = requestedAimDir;
    float aimLen = aimDir.length();
    if ( aimLen <= 0.001 )
    {
        aimDir = launcher->_rotation.AxisZ();
        aimLen = aimDir.length();
    }

    if ( aimLen <= 0.001 )
        return false;

    aimDir = aimDir / aimLen;
    return ypabact_GetMissileMultiTargetScore(launcher, target, aimDir, wproto.radius, nullptr);
}

NC_STACK_ypabact *ypabact_GetHomingCycleLogicalTarget(NC_STACK_ypabact *target)
{
    if ( !target )
        return nullptr;

    // Attached Unit Guns/modules belong to their carrying vehicle for manual
    // target cycling. They remain independent damageable actors everywhere else.
    if ( (target->_isUnitGunChild || target->_isDummy) &&
         target->_parent && target->_parent != target )
        return target->_parent;

    // Host Station Robo-Guns have their own GIDs, but presenting those children
    // as separate TAB targets makes one Host Station look like several targets.
    if ( target->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(target);
        if ( gun && gun->IsRoboGun() && target->_host_station &&
             target->_host_station != target )
            return target->_host_station;
    }

    return target;
}

NC_STACK_ypabact *ypabact_SelectNextHomingCycleTarget(NC_STACK_ypabact *launcher,
                                                              const bact_arg106 *targetingArg,
                                                              const World::TWeapProto &wproto,
                                                              NC_STACK_ypabact *currentTarget)
{
    if ( !launcher || !targetingArg || !ypabact_UsesMissileTargeting(wproto) )
        return nullptr;

    bact_arg79 collectArg = {};
    collectArg.direction = targetingArg->field_4;
    collectArg.tgType = BACT_TGT_TYPE_DRCT;
    collectArg.tgt_pos = launcher->_position + targetingArg->field_4 * YPA_MISSILE_MULTI_TARGET_RANGE;

    // Cycle Target traverses the full natural candidate list. multi_target is
    // volley fan-out only and does not limit manual cycling.
    std::vector<NC_STACK_ypabact *> rawTargets = ypabact_CollectMissileMultiTargets(
        launcher, &collectArg, wproto, YPA_HOMING_TARGET_CYCLE_MAX);

    if ( rawTargets.empty() )
        return nullptr;

    // Collapse attached components to one player-facing logical target. This is
    // especially important for a Host Station and its independently damageable
    // Robo-Guns, which otherwise have different GIDs but represent one vehicle
    // for TAB cycling. Keep this normalization local to manual cycling so normal
    // missile targeting and multi-target fan-out retain their existing semantics.
    std::vector<NC_STACK_ypabact *> targets;
    std::vector<int32_t> targetGids;
    targets.reserve(rawTargets.size());
    targetGids.reserve(rawTargets.size());

    for (NC_STACK_ypabact *rawTarget : rawTargets)
    {
        NC_STACK_ypabact *logicalTarget = ypabact_GetHomingCycleLogicalTarget(rawTarget);
        if ( !logicalTarget )
            continue;

        const int32_t logicalGid = logicalTarget->_gid;
        if ( std::find(targetGids.begin(), targetGids.end(), logicalGid) != targetGids.end() )
            continue;

        targets.push_back(logicalTarget);
        targetGids.push_back(logicalGid);
    }

    if ( targets.empty() )
        return nullptr;

    NC_STACK_ypabact *logicalCurrent = ypabact_GetHomingCycleLogicalTarget(currentTarget);
    const int32_t currentGid = logicalCurrent ? logicalCurrent->_gid : 0;

    // One logical target cannot be cycled when it is already the current lock.
    // This covers one Host Station with any number of Robo-Guns or one vehicle
    // with attached Unit Guns. If the old lock has left the candidate set, a
    // single different valid target may still replace it.
    if ( targets.size() == 1 )
    {
        if ( currentGid <= 0 || targets[0]->_gid == currentGid )
            return nullptr;

        return targets[0];
    }

    size_t startIndex = 0;
    for (size_t i = 0; i < targets.size(); ++i)
    {
        if ( targets[i] && currentGid > 0 && targets[i]->_gid == currentGid )
        {
            startIndex = (i + 1) % targets.size();
            break;
        }
    }

    // Never return the currently locked logical unit. If another distinct unit
    // exists it must be selected; otherwise TAB is disabled for this press.
    for (size_t offset = 0; offset < targets.size(); ++offset)
    {
        NC_STACK_ypabact *candidate = targets[(startIndex + offset) % targets.size()];
        if ( candidate && (currentGid <= 0 || candidate->_gid != currentGid) )
            return candidate;
    }

    return nullptr;
}

int ypabact_SelectPrimaryWeaponSlot(NC_STACK_ypabact *bact,
                                             int requestedWeapon,
                                             int requestedSourceSlot,
                                             NC_STACK_ypabact *target,
                                             int *outSourceSlot)
{
    if ( outSourceSlot )
        *outSourceSlot = 0;

    if ( requestedWeapon != bact->_weapon )
        return requestedWeapon;

    if ( requestedSourceSlot >= 0 )
    {
        int exactWeapon = ypabact_GetWeaponIdForSourceSlot(bact, requestedSourceSlot);
        if ( ypabact_IsValidFireWeaponId(bact, exactWeapon) )
        {
            if ( outSourceSlot )
                *outSourceSlot = requestedSourceSlot;
            return exactWeapon;
        }
        return -1;
    }

    if ( !(bact->_oflags & BACT_OFLAG_USERINPT) )
        return ypabact_SelectAIPrimaryWeaponSlot(bact, target, outSourceSlot);

    if ( bact->_weapon_player_switch_mode == World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_RANDOM )
        return ypabact_SelectRandomPrimaryWeaponSlot(bact, outSourceSlot);

    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots, sourceSlots);
    if ( count <= 0 )
        return -1;

    int index = bact->_weapon_slot_index % count;
    if ( index < 0 )
        index = 0;

    if ( outSourceSlot )
        *outSourceSlot = sourceSlots[index];
    return slots[index];
}

void ypabact_AdvancePrimaryWeaponSlot(NC_STACK_ypabact *bact, int requestedWeapon)
{
    if ( requestedWeapon != bact->_weapon )
        return;

    const bool sequenceMode =
        (bact->_oflags & BACT_OFLAG_USERINPT)
            ? bact->_weapon_player_switch_mode == World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE
            : bact->_weapon_ai_switch_mode == World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;

    if ( !sequenceMode )
        return;

    int slots[4];
    int count = ypabact_GetPrimaryWeaponSlots(bact, slots);

    if ( count > 1 )
        bact->_weapon_slot_index = (bact->_weapon_slot_index + 1) % count;
    else
        bact->_weapon_slot_index = 0;
}

int NC_STACK_ypabact::GetCurrentWeaponId()
{
    int sourceSlot = ypabact_GetCurrentPrimaryWeaponSourceSlot(this);
    int weaponId = ypabact_GetWeaponIdForSourceSlot(this, sourceSlot);
    return ypabact_IsValidFireWeaponId(this, weaponId) ? weaponId : -1;
}

int NC_STACK_ypabact::GetHUDWeaponId()
{
    const int weaponId = GetCurrentWeaponId();
    if ( weaponId >= 0 )
        return weaponId;

    // Kamikaze Weapons are intentionally excluded from normal weapon selection
    // and firing. The HUD still needs to expose the real mounted payload, so
    // reuse the authoritative Kamikaze mount resolver instead of duplicating
    // slot/attachment lookup logic in the UI.
    TKamikazeMount mount;
    if ( ypabact_ResolveKamikazeMount(this, &mount) &&
         ypabact_IsKamikazeMountArmed(mount) )
    {
        return mount.weaponId;
    }

    return -1;
}

int NC_STACK_ypabact::GetCurrentWeaponProjectileCount()
{
    return ypabact_GetWeaponProjectileCountForSourceSlot(
        this, ypabact_GetCurrentPrimaryWeaponSourceSlot(this));
}

void NC_STACK_ypabact::GetCurrentWeaponProjectileCountRange(int *outMin,
                                                             int *outMax)
{
    ypabact_GetWeaponProjectileCountRangeForSourceSlot(
        this, ypabact_GetCurrentPrimaryWeaponSourceSlot(this), outMin, outMax);
}

bool NC_STACK_ypabact::RequestHomingTargetCycle()
{
    if ( !(_oflags & BACT_OFLAG_USERINPT) || !_world || _status == BACT_STATUS_DEAD )
        return false;

    if ( !IsPlayerPrimaryWeaponElevationAllowed() )
        return false;

    int weaponId = GetCurrentWeaponId();
    if ( !ypabact_IsValidWeaponId(this, weaponId) )
        return false;

    const World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);
    if ( !ypabact_UsesMissileTargeting(wproto) )
        return false;

    _userHomingTargetCycleRequested = true;
    return true;
}

bool NC_STACK_ypabact::CycleControlledWeapon()
{
    // Manual switching is intentionally available only in the explicit player
    // manual mode. Sequence/random remain automatic player modes.
    if ( !(_oflags & BACT_OFLAG_USERINPT) ||
         _weapon_player_switch_mode != World::TVhclProto::WEAPON_PLAYER_SWITCH_MODE_MANUAL ||
         _status == BACT_STATUS_DEAD )
        return false;

    int slots[4];
    int sourceSlots[4];
    int count = ypabact_GetPrimaryWeaponSlots(this, slots, sourceSlots);
    if ( count <= 1 )
        return false;

    int index = _weapon_slot_index % count;
    if ( index < 0 )
        index = 0;

    _weapon_slot_index = (index + 1) % count;
    _current_weapon_id = slots[_weapon_slot_index];
    _current_weapon_source_slot = sourceSlots[_weapon_slot_index];
    _userHomingPrimaryTargetGid = 0;
    _userHomingTargetCycleRequested = false;
    if ( _alternativeViewActive && !IsAlternativeViewAvailable() )
        ResetAlternativeView();
    if ( _world )
        _world->_hudMissileMultiLockTargets.clear();
    return true;
}

int ypabact_GetAuthoredPercentEnergyCost(int energyMax, float percent)
{
    const double rawCost =
        (double)std::max(energyMax, 0) *
        (double)std::max(0.0f, std::min(percent, 100.0f)) * 0.01;

    return rawCost > 0.0
        ? (int)std::min<double>(std::ceil(rawCost),
                               (double)std::numeric_limits<int>::max())
        : 0;
}

bool NC_STACK_ypabact::IsPrimaryWeaponElevationAllowed(const vec3d &direction) const
{
    if ( !IsGroundUnit() || !_world || _vehicleID >= _world->GetVhclProtos().size() )
        return true;

    const World::TVhclProto &proto = _world->GetVhclProtos().at(_vehicleID);
    if ( !proto.scope_angle_min_max_set )
        return true;

    // World Y points down. Compare hull-local vertical aim with the same
    // vanilla reticle offsets for the player and AI, independent of lateral aim.
    const vec3d local = _rotation.Transform(direction == vec3d() ? _rotation.AxisZ() : direction);
    if ( !std::isfinite(local.x) || !std::isfinite(local.y) || !std::isfinite(local.z) )
        return false;
    float corW, corH;
    GFX::Engine.getAspectCorrection(corW, corH, true);
    const double pitch = std::atan2(-local.y, local.z);
    const auto referencePitch = [corH](float reference) {
        const double travel = reference < 0.0f ? -USER_GROUND_AIM_MIN : USER_GROUND_AIM_MAX;
        return std::atan(reference * 0.01 * travel * corH);
    };
    constexpr double EndpointTolerance = 0.0000001; // Inclusive bounds despite roundoff.
    return pitch >= referencePitch(proto.scope_angle_min_max[0]) - EndpointTolerance &&
           pitch <= referencePitch(proto.scope_angle_min_max[1]) + EndpointTolerance;
}

bool NC_STACK_ypabact::IsPlayerPrimaryWeaponElevationAllowed() const
{
    return IsPrimaryWeaponElevationAllowed(GetUserWeaponAimDirection());
}
