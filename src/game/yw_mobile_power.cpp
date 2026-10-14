// Power stations, mobile power generators and the sector energy accumulation map.
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

void NC_STACK_ypaworld::PowerStationErase(cellArea *cell)
{
    if (!cell) return;

    auto it = _powerStations.find(cell->Id);
    if (it == _powerStations.end())
    {
        /* Oh no, power station info for this sector does not exist.
           So just clear sector purpose field if it was PowerStation*/
        if (cell->PurposeType == cellArea::PT_POWERSTATION)
        {
            cell->PurposeIndex = 0;
            cell->PurposeType = cellArea::PT_NONE;
        }

        return;
    }

    _powerStations.erase(it);

    cell->PurposeIndex = 0;
    cell->PurposeType = cellArea::PT_NONE;

    _lvlBuildingsMap(cell->CellId) = 0;
}

void NC_STACK_ypaworld::UpdatePowerEnergy()
{
    // Apply power to sectors and clean power matrix for next compute iteration.

    for (int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            EnergyAccum &accum = _energyAccumMap(x, y);

            accum.Owner = cell.owner;
            cell.energy_power = accum.Energy; // Apply power to cell
            accum.Energy = 0; // Clean matrix's power
        }
    }

    _nextPSForUpdate = 0; // Next power station for recompute power is first
}

void NC_STACK_ypaworld::ResetAccumMap()
{
    for( int y = 0; y < _mapSize.y; y++ )
    {
        for( int x = 0; x < _mapSize.x; x++ )
        {
            EnergyAccum &accum = _energyAccumMap(x, y);
            accum.Energy = 0;
            accum.Owner = _cells(x, y).owner;
        }
    }

    _nextPSForUpdate = 0;
}

void NC_STACK_ypaworld::SetupPowerStationInfo(cellArea *cell, int power, int buildingId)
{
    if (!cell) return;

    TPowerStationInfo &ps = _powerStations[cell->Id];

    ps.CellId = cell->CellId;
    ps.Power = power;
    ps.EffectivePower = power;
    ps.pCell = cell;

    cell->PurposeType = cellArea::PT_POWERSTATION;
    cell->PurposeIndex = buildingId;

    ResetAccumMap();
}

bool yw_IsValidMobilePowerGenerator(NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit)
{
    if ( !yw ||
         !unit ||
         unit->_owner == World::OWNER_0 ||
         unit->_status == BACT_STATUS_DEAD ||
         unit->_status == BACT_STATUS_CREATE ||
         unit->_status == BACT_STATUS_BEAM ||
         (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) ||
         unit->_bact_type == BACT_TYPES_MISSLE ||
         unit->_bact_type == BACT_TYPES_ROBO ||
         (size_t)(unit->_mimic_disguise_vehicleID ? unit->_mimic_disguise_vehicleID : unit->_vehicleID) >= yw->_vhclProtos.size() )
        return false;

    // OpenNeoUA: normal guns/flaks/radars never become power generators merely
    // because a prototype happens to contain power fields. Only the explicit
    // passive gun_type=power module is allowed through the existing mobile-
    // power path. Non-gun vehicles retain the previous behaviour unchanged.
    if ( unit->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(unit);
        if ( !gun || gun->getGUN_fireType() != NC_STACK_ypagun::GUN_TYPE_POWER )
            return false;
    }

    uint8_t protoId = unit->_mimic_disguise_vehicleID ? unit->_mimic_disguise_vehicleID : unit->_vehicleID;
    const World::TVhclProto &proto = yw->_vhclProtos[protoId];
    return proto.power > 0 && proto.power_radius > 0.0;
}

bool NC_STACK_ypaworld::IsValidMobilePowerGenerator(NC_STACK_ypabact *unit)
{
    return yw_IsValidMobilePowerGenerator(this, unit);
}

void NC_STACK_ypaworld::ClearMobilePowerFXRuntime()
{
    for (auto &entry : _mobilePowerFXSounds)
    {
        TMobilePowerFXSound &sound = entry.second;
        if ( sound.carrier )
            StopAtmosphericFXLoopSoundInstance(sound.sample, *sound.carrier);
        else if ( sound.sample )
        {
            sound.sample->Delete();
            sound.sample = nullptr;
        }
    }
    _mobilePowerFXSounds.clear();
    _mobilePowerFXProfileCache.clear();

    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( it->mobilePowerFX )
            it = _transientVPs.erase(it);
        else
            ++it;
    }
}

void NC_STACK_ypaworld::UpdateMobilePowerFX()
{
    std::map<int32_t, int32_t> activeCounts;
    std::map<int32_t, NC_STACK_ypabact *> generators;
    std::map<int32_t, const World::TAtmosphericFXProfile *> profiles;
    std::map<int32_t, float> radii;

    // Match the mechanical Mobile Power traversal exactly: mounted power
    // modules live in _kidList and must be visible to the FX system too.
    std::vector<NC_STACK_ypabact *> pendingGenerators;
    for (NC_STACK_ypabact *root : _unitsList)
        pendingGenerators.push_back(root);

    while ( !pendingGenerators.empty() )
    {
        NC_STACK_ypabact *generator = pendingGenerators.back();
        pendingGenerators.pop_back();

        if ( !generator )
            continue;

        for (NC_STACK_ypabact *kid : generator->_kidList)
            pendingGenerators.push_back(kid);

        if ( !yw_IsValidMobilePowerGenerator(this, generator) )
            continue;

        const uint8_t protoId = generator->_mimic_disguise_vehicleID
                              ? generator->_mimic_disguise_vehicleID
                              : generator->_vehicleID;
        const World::TVhclProto &proto = _vhclProtos[protoId];
        if ( proto.power_cloud_id < 0 )
            continue;

        auto cacheIt = _mobilePowerFXProfileCache.find(proto.power_cloud_id);
        if ( cacheIt == _mobilePowerFXProfileCache.end() )
        {
            World::TAtmosphericFXProfile parsed;
            if ( !FindCloudProfile(proto.power_cloud_id, parsed) )
                parsed = World::TAtmosphericFXProfile();
            cacheIt = _mobilePowerFXProfileCache.emplace(proto.power_cloud_id,
                                                          std::move(parsed)).first;
        }

        if ( !cacheIt->second.valid )
            continue;

        const int32_t gid = generator->_gid;
        generators[gid] = generator;
        profiles[gid] = &cacheIt->second;
        radii[gid] = proto.power_radius;
    }

    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( !it->mobilePowerFX )
        {
            ++it;
            continue;
        }

        const int32_t gid = it->followOwnerGid;
        auto genIt = generators.find(gid);
        auto radiusIt = radii.find(gid);
        if ( genIt == generators.end() || radiusIt == radii.end() )
        {
            it = _transientVPs.erase(it);
            continue;
        }

        const float radius = radiusIt->second;
        const double dx = it->followLocalOffset.x;
        const double dz = it->followLocalOffset.z;
        if ( dx * dx + dz * dz > (double)radius * (double)radius )
        {
            it = _transientVPs.erase(it);
            continue;
        }

        ++activeCounts[gid];
        ++it;
    }

    for (const auto &entry : generators)
    {
        const int32_t gid = entry.first;
        NC_STACK_ypabact *generator = entry.second;
        const World::TAtmosphericFXProfile &profile = *profiles[gid];
        const float radius = radii[gid];

        int32_t spawnNeeded = profile.count - activeCounts[gid];
        for (int32_t i = 0; i < spawnNeeded; ++i)
        {
            vec3d local = yw_RandomPointInDisk(radius);
            // Mobile Power owns the X/Z domain. Profile X/Z radius and offsets
            // are intentionally ignored so power_radius remains the single
            // source of truth for both mechanics and visuals.
            local.y = profile.spawn_offset.y + yw_RandomFloatSigned(profile.spawn_radius.y);

            vec3d velocity = profile.velocity;
            velocity.x += yw_RandomFloatSigned(profile.velocity_random.x);
            velocity.y += yw_RandomFloatSigned(profile.velocity_random.y);
            velocity.z += yw_RandomFloatSigned(profile.velocity_random.z);

            const int32_t lifeTime = yw_RandomInRange(profile.lifetime_min,
                                                      profile.lifetime_max);
            const int32_t id = SpawnAttachedTransientVP(0, generator, local,
                                                        lifeTime, 1.0, false,
                                                        profile.tint,
                                                        profile.scale,
                                                        profile.spin,
                                                        false, vec3d(0.0, 0.0, 0.0),
                                                        false,
                                                        TTransientVPParticleControls(),
                                                        false,
                                                        profile.fade_in,
                                                        profile.fade_out,
                                                        profile.mesh3ds);
            if ( id > 0 && !_transientVPs.empty() && _transientVPs.back().id == id )
            {
                TTransientVP &fx = _transientVPs.back();
                fx.mobilePowerFX = true;
                fx.velocity = velocity;
            }
        }

        auto soundIt = _mobilePowerFXSounds.find(gid);
        if ( !profile.loop_sound.empty() && profile.loop_sound_volume > 0 )
        {
            if ( soundIt == _mobilePowerFXSounds.end() )
            {
                TMobilePowerFXSound state;
                state.carrier.reset(new TSndCarrier());
                if ( StartAtmosphericFXLoopSoundInstance(profile, state.sample, *state.carrier) )
                    soundIt = _mobilePowerFXSounds.emplace(gid, std::move(state)).first;
            }

            if ( soundIt != _mobilePowerFXSounds.end() && soundIt->second.carrier )
            {
                TSndCarrier &carrier = *soundIt->second.carrier;
                carrier.Position = generator->_position;
                carrier.Vector = vec3d(0.0, 0.0, 0.0);
                if ( !carrier.Sounds.empty() )
                    carrier.Sounds[0].Radius = radius;
                SFXEngine::SFXe.UpdateSoundCarrier(&carrier);
            }
        }
        else if ( soundIt != _mobilePowerFXSounds.end() )
        {
            if ( soundIt->second.carrier )
                StopAtmosphericFXLoopSoundInstance(soundIt->second.sample,
                                                   *soundIt->second.carrier);
            _mobilePowerFXSounds.erase(soundIt);
        }
    }

    for (auto it = _mobilePowerFXSounds.begin(); it != _mobilePowerFXSounds.end(); )
    {
        if ( generators.find(it->first) != generators.end() )
        {
            ++it;
            continue;
        }

        if ( it->second.carrier )
            StopAtmosphericFXLoopSoundInstance(it->second.sample, *it->second.carrier);
        it = _mobilePowerFXSounds.erase(it);
    }
}

void yw_AddMobilePowerInfluenceFromGenerator(NC_STACK_ypaworld *yw,
                                                    NC_STACK_ypabact *target,
                                                    NC_STACK_ypabact *generator,
                                                    TMobilePowerInfluence &influence)
{
    if ( !yw || !target || !generator )
        return;

    if ( yw_IsValidMobilePowerGenerator(yw, generator) )
    {
        uint8_t protoId = generator->_mimic_disguise_vehicleID ? generator->_mimic_disguise_vehicleID : generator->_vehicleID;
        const World::TVhclProto &proto = yw->_vhclProtos[protoId];

        float dx = target->_position.x - generator->_position.x;
        float dz = target->_position.z - generator->_position.z;
        float distSq = dx * dx + dz * dz;
        float radiusSq = proto.power_radius * proto.power_radius;

        if ( distSq <= radiusSq )
        {
            const float dist = sqrt(distSq);
            const float factor = proto.power_falloff
                ? World::ComputeSpatialFalloff(dist, proto.power_radius)
                : 1.0f;

            int addPower = (int)(proto.power * factor + 0.5f);
            if ( addPower > 0 )
            {
                float addEnergyPower = addPower;

                if ( generator->_owner == target->_owner )
                {
                    influence.AlliedPower += addPower;
                    if ( influence.AlliedPower > 255 )
                        influence.AlliedPower = 255;

                    influence.AlliedEnergyPower += addEnergyPower;
                    if ( influence.AlliedEnergyPower > 255.0 )
                        influence.AlliedEnergyPower = 255.0;
                }
                else
                {
                    influence.EnemyPower += addPower;
                    if ( influence.EnemyPower > 255 )
                        influence.EnemyPower = 255;

                    influence.EnemyEnergyPower += addEnergyPower;
                    if ( influence.EnemyEnergyPower > 255.0 )
                        influence.EnemyEnergyPower = 255.0;
                }
            }
        }
    }

    for (NC_STACK_ypabact *kid : generator->_kidList)
        yw_AddMobilePowerInfluenceFromGenerator(yw, target, kid, influence);
}

TMobilePowerInfluence NC_STACK_ypaworld::FindMobilePowerInfluenceForUnit(NC_STACK_ypabact *target)
{
    TMobilePowerInfluence influence;

    if ( !target ||
         target->_owner == World::OWNER_0 ||
         target->_status == BACT_STATUS_DEAD ||
         target->_status == BACT_STATUS_CREATE ||
         target->_status == BACT_STATUS_BEAM ||
         (target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) ||
         target->_bact_type == BACT_TYPES_MISSLE )
        return influence;

    for (NC_STACK_ypabact *generator : _unitsList)
        yw_AddMobilePowerInfluenceFromGenerator(this, target, generator, influence);

    return influence;
}

uint8_t NC_STACK_ypaworld::GetUnitEnergyVisualState(NC_STACK_ypabact *target)
{
    if ( !target )
        return UNIT_ENERGY_VISUAL_NONE;

    // Match the actor domain where a Regen/Drain Status Icon can be meaningful.
    // Missiles and strategic-UI-hidden helper actors must never start unit FX.
    if ( target->_owner == World::OWNER_0 ||
         target->_bact_type == BACT_TYPES_MISSLE ||
         target->ShouldHideFromStrategicUI() )
    {
        target->_energy_visual_state_time = _timeStamp;
        target->_energy_visual_state = UNIT_ENERGY_VISUAL_NONE;
        return UNIT_ENERGY_VISUAL_NONE;
    }

    // Both the actor update and world-space UI may query this in the same frame.
    // Cache per world timestamp so the shared condition is evaluated once rather
    // than duplicating power-station and mobile-generator scans.
    if ( target->_energy_visual_state_time == _timeStamp )
        return target->_energy_visual_state;

    uint8_t state = UNIT_ENERGY_VISUAL_NONE;

    // Preserve the exact power-station condition previously used by the Status
    // Icon renderer: a powered station owned by the sector must be within the
    // legacy 3x3 sector neighbourhood. Friendly units regen only below max
    // energy; hostile units are in drain state.
    cellArea *cell = target->_pSector;
    if ( cell &&
         target->_owner != World::OWNER_0 &&
         target->_status != BACT_STATUS_DEAD &&
         cell->owner &&
         cell->energy_power > 0 )
    {
        bool hasPowerStationInRange = false;
        for (const auto &psNode : _powerStations)
        {
            const TPowerStationInfo &ps = psNode.second;
            if ( !ps.pCell || ps.EffectivePower <= 0 || ps.pCell->owner != cell->owner )
                continue;

            if ( Common::ABS(ps.CellId.x - target->_cellId.x) <= 1 &&
                 Common::ABS(ps.CellId.y - target->_cellId.y) <= 1 )
            {
                hasPowerStationInRange = true;
                break;
            }
        }

        if ( hasPowerStationInRange )
        {
            if ( target->_owner == cell->owner )
            {
                if ( target->_energy < target->_energy_max )
                    state |= UNIT_ENERGY_VISUAL_REGEN;
            }
            else
            {
                state |= UNIT_ENERGY_VISUAL_DRAIN;
            }
        }
    }

    // Preserve the exact mobile-power eligibility that the Status Icon path
    // used before this condition was moved into the world: ordinary live
    // vehicle units only, with a meaningful energy pool and no hidden/death
    // transition state. Regen and drain remain independent because allied and
    // enemy mobile influences can overlap.
    const bool canUseMobilePower =
        target->_owner != World::OWNER_0 &&
        target->_bact_type != BACT_TYPES_MISSLE &&
        target->_bact_type != BACT_TYPES_ROBO &&
        target->_bact_type != BACT_TYPES_GUN &&
        target->_status != BACT_STATUS_DEAD &&
        target->_status != BACT_STATUS_CREATE &&
        target->_status != BACT_STATUS_BEAM &&
        !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) &&
        target->_energy > 0 &&
        target->_energy_max > 0;

    if ( canUseMobilePower )
    {
        TMobilePowerInfluence mobilePower = FindMobilePowerInfluenceForUnit(target);
        if ( mobilePower.AlliedPower > 0 && target->_energy < target->_energy_max )
            state |= UNIT_ENERGY_VISUAL_REGEN;
        if ( mobilePower.EnemyPower > 0 )
            state |= UNIT_ENERGY_VISUAL_DRAIN;
    }

    target->_energy_visual_state_time = _timeStamp;
    target->_energy_visual_state = state;
    return state;
}

void NC_STACK_ypaworld::AddMobileVehiclePowerToAccumMap()
{
    // Mobile power is owner-aware, so gameplay is applied per target unit in
    // EnergyInteract instead of through cell.energy_power/cell.owner.
}

void NC_STACK_ypaworld::DoSectorsEnergyRecalc()
{
    // Recompute power on sectors
    if ( !_powerStations.empty() ) // If we have powerstations
    {
        auto itPs = _powerStations.lower_bound(_nextPSForUpdate);

        if (itPs == _powerStations.end()) // If we reach end of power stations list, apply power to sectors
        {
            AddMobileVehiclePowerToAccumMap();
            UpdatePowerEnergy(); // Apply power to sectors and clean power matrix for next compute iteration.
        }
        else
        {
            if ( itPs->second.EffectivePower ) // if this power station has power
                RecalcSectorsPowerForPS(itPs->second); // Add power to power matrix

            _nextPSForUpdate = itPs->first + 1; // go to next station in next update loop
        }
    }
    else
    {
        AddMobileVehiclePowerToAccumMap();
        UpdatePowerEnergy();
    }
}
