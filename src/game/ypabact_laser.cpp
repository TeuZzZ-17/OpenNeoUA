// Laser and vertical laser weapons: targeting, damage ticks, sound and beam visuals.
#include "game/ypabact_internal.h"

vec3d ypabact_LaserViewerVisualStart(
    const NC_STACK_ypabact *bact, const World::TWeapProto &wproto,
    const vec3d &beamStart, const vec3d &beamEnd)
{
    if ( !bact || (!bact->getBACT_viewer() && !bact->getBACT_inputting()) )
        return beamStart;

    vec3d span = beamEnd - beamStart;
    float len = span.length();
    if ( !std::isfinite(len) || len < 1.0f )
        return beamStart;

    vec3d dir = span / len;
    float lead = ypabact_LaserClampVisualSpacing(wproto.laser_spacing) * 0.5f;
    if ( lead < 16.0f )
        lead = 16.0f;
    if ( lead > len * 0.25f )
        lead = len * 0.25f;

    return beamStart + dir * lead;
}

bool ypabact_IsLaserMeshUsable(
    NC_STACK_ypabact *bact, const World::TWeapProto &wproto)
{
    NC_STACK_ypaworld *world = bact ? bact->getBACT_pWorld() : nullptr;
    if ( !world || world->_isNetGame ||
         !wproto.IsLaser() ||
         !wproto.laser_mesh.enabled )
        return false;

    const World::TWeapProto::TLaserMeshConfig &config = wproto.laser_mesh;
    const float sizeY = config.ResolveSizeY();
    return !config.mesh_path.empty() &&
           std::isfinite(config.size_x) && config.size_x > 0.01f &&
           std::isfinite(sizeY) && sizeY > 0.01f &&
           std::isfinite(config.tint.r) && std::isfinite(config.tint.g) &&
           std::isfinite(config.tint.b) && std::isfinite(config.tint.a) &&
           config.tint.a > 0.0f;
}

uint32_t ypabact_LaserMeshSeed(const NC_STACK_ypabact *bact,
                                      size_t beamIndex, uint32_t domain)
{
    const uint32_t actor = bact ? bact->_gid : 0u;
    return actor ^ domain ^
           ((uint32_t)(beamIndex + 1u) * 0x9e3779b9u);
}

void ypabact_RenderLaserMeshes(NC_STACK_ypabact *bact,
                                      baseRender_msg *arg)
{
    if ( !bact || !arg )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    if ( bact->_laser_active && bact->_laser_weapon >= 0 &&
         (size_t)bact->_laser_weapon < world->GetWeaponsProtos().size() )
    {
        const World::TWeapProto &wproto =
            world->GetWeaponsProtos().at(bact->_laser_weapon);
        if ( ypabact_IsLaserMeshUsable(bact, wproto) )
        {
            for (size_t i = 0; i < bact->_laser_beams.size(); i++)
            {
                const NC_STACK_ypabact::TLaserBeamRuntime &beam =
                    bact->_laser_beams[i];
                // Match the legacy VP viewer lead. A crossed-ribbon beam that
                // starts almost inside the first-person camera can project its two
                // planes as a temporally unstable/doubled primary laser. Advancing
                // only the rendered tail keeps gameplay start/end untouched while
                // avoiding that near-view artifact.
                const vec3d renderStart = (i == 0)
                    ? ypabact_LaserViewerVisualStart(bact, wproto, beam.start, beam.end)
                    : beam.start;
                world->RenderLaserMeshSegment(
                    arg, renderStart, beam.end, wproto.laser_mesh,
                    beam.has_contact, bact->_rotation,
                    ypabact_LaserMeshSeed(bact, i, 0x4c415352u));
            }
        }
    }

    if ( bact->_vertical_laser_active && bact->_vertical_laser_weapon >= 0 &&
         (size_t)bact->_vertical_laser_weapon < world->GetWeaponsProtos().size() )
    {
        const World::TWeapProto &wproto =
            world->GetWeaponsProtos().at(bact->_vertical_laser_weapon);
        if ( ypabact_IsLaserMeshUsable(bact, wproto) )
        {
            for (size_t i = 0; i < bact->_vertical_laser_beams.size(); i++)
            {
                const NC_STACK_ypabact::TLaserBeamRuntime &beam =
                    bact->_vertical_laser_beams[i];
                const vec3d renderStart = (i == 0)
                    ? ypabact_LaserViewerVisualStart(bact, wproto, beam.start, beam.end)
                    : beam.start;
                world->RenderLaserMeshSegment(
                    arg, renderStart, beam.end, wproto.laser_mesh,
                    beam.has_contact, bact->_rotation,
                    ypabact_LaserMeshSeed(bact, i, 0x5652544cu));
            }
        }
    }
}

bool ypabact_IsLaserDamageTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit)
{
    if ( !ypabact_IsCarrierSpawnAliveUnit(unit) )
        return false;

    if ( unit == shooter )
        return false;

    if ( unit->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( unit->_status == BACT_STATUS_CREATE || unit->_status == BACT_STATUS_BEAM )
        return false;

    if ( unit->_status_flg & BACT_STFLAG_NORENDER )
        return false;

    return true;
}

bool ypabact_IsLaserAimTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit)
{
    if ( !ypabact_IsLaserDamageTarget(shooter, unit) )
        return false;

    if ( !shooter )
        return true;

    if ( !shooter->IsPrimaryWeaponElevationAllowed(unit->_position - shooter->_position) )
        return false;

    if ( unit->_owner == World::OWNER_0 || unit->_owner == shooter->_owner )
        return false;

    // OpenNeoUA invisible: automatic Laser/Vertical Laser aiming never picks a cloaked
    // stealth unit as its aim target (the damage predicate above stays unfiltered so a
    // beam still burns one if it happens to physically cross it).
    if ( !unit->CanBeSeenByAIOrRadar() )
        return false;

    return true;
}

int ypabact_GetActiveAILaserWeaponSlot(NC_STACK_ypabact *bact, int *outSourceSlot)
{
    if ( outSourceSlot )
        *outSourceSlot = -1;

    if ( !bact || !bact->getBACT_pWorld() || (bact->_oflags & BACT_OFLAG_USERINPT) )
        return -1;

    const int candidates[2] = {
        bact->_laser_active ? bact->_laser_weapon : -1,
        bact->_vertical_laser_active ? bact->_vertical_laser_weapon : -1
    };

    for (int weaponId : candidates)
    {
        if ( !ypabact_IsValidWeaponId(bact, weaponId) )
            continue;

        const World::TWeapProto &wproto =
            bact->getBACT_pWorld()->GetWeaponsProtos().at(weaponId);
        if ( !wproto.IsLaser() )
            continue;

        if ( bact->_weapon == weaponId )
        {
            if ( outSourceSlot )
                *outSourceSlot = 0;
            return weaponId;
        }

        for (size_t i = 0; i < bact->_extra_weapons.size(); ++i)
        {
            if ( bact->_extra_weapons[i] == weaponId )
            {
                if ( outSourceSlot )
                    *outSourceSlot = (int)i + 1;
                return weaponId;
            }
        }
    }

    return -1;
}

int ypabact_SelectAIPrimaryWeaponSlotWithLaserHold(NC_STACK_ypabact *bact,
                                                           NC_STACK_ypabact *target,
                                                           int *outSourceSlot)
{
    // Random/Smart normally resolve a slot independently for every shot.  That
    // is correct for discrete weapons, but a laser is one held attack: once the
    // beam is alive, keep the same source slot until the beam actually stops.
    const int activeLaser = ypabact_GetActiveAILaserWeaponSlot(bact, outSourceSlot);
    if ( activeLaser >= 0 )
        return activeLaser;

    return ypabact_SelectAIPrimaryWeaponSlot(bact, target, outSourceSlot);
}

bool ypabact_HasActiveAILaserSectorFocus(const NC_STACK_ypabact *bact)
{
    if ( !bact || (bact->_oflags & BACT_OFLAG_USERINPT) )
        return false;

    if ( bact->_laser_active && !bact->_laser_beams.empty() &&
         bact->_laser_beams[0].target_gid < 0 )
        return true;

    return bact->_vertical_laser_active && !bact->_vertical_laser_beams.empty() &&
           bact->_vertical_laser_beams[0].target_gid < 0;
}

// ============================ OpenNeoUA custom: model = laser ============================
// A laser is a targeted-class weapon that, instead of spawning a projectile, fires a
// continuous aimed hitscan beam. The beam is always visible while firing (even into
// empty space); when it crosses a valid target it applies static tick damage using
// energy as the base tick amount. All state is transient and lives on the firing unit
// (see the _laser_* members). The firing paths only register a per-frame request via
// RequestLaserFire(); UpdateLaser() does all the real work.

// Beam reach (engine units) derived from the weapon's life_time, so a modder controls
// the laser length with the familiar life_time knob. Clamped to a sane span.
float ypabact_LaserRange(const World::TWeapProto &wproto)
{
    float range = (float)wproto.life_time;          // life_time (ms) reused as beam length
    if ( range <= 0.0f )
        range = World::CVSectorLength * 3.0f;       // safe default ~3 sectors

    float minR = World::CVSectorLength * 0.5f;
    float maxR = World::CVSectorLength * 10.0f;
    if ( range < minR ) range = minR;
    if ( range > maxR ) range = maxR;
    return range;
}

int ypabact_LaserDamageInterval(const World::TWeapProto &wproto, bool playerControlled)
{
    int interval = playerControlled && wproto.laser_energy_tick_time_user > 0
                 ? wproto.laser_energy_tick_time_user
                 : wproto.laser_energy_tick_time;
    if ( interval <= 0 )
        interval = 250;
    if ( interval < 1 )
        interval = 1;
    return interval;
}

float ypabact_LaserEnergyScale(const World::TWeapProto &wproto, NC_STACK_ypabact *target)
{
    float energy = 1.0f;
    if ( ypabact_GetWeaponEnergyForTarget(wproto, target, &energy, nullptr) )
        return energy;

    return 1.0f;
}

float ypabact_LaserNominalTickEnergy(const World::TWeapProto &wproto,
                                             int connectedTicks,
                                             float damageMult)
{
    float baseEnergy = (float)wproto.energy;
    if ( wproto.laser_energy_increment_rate > 0.0f && connectedTicks > 0 )
        baseEnergy += (float)connectedTicks * wproto.laser_energy_increment_rate;
    if ( wproto.laser_max_energy > 0.0f && baseEnergy > wproto.laser_max_energy )
        baseEnergy = wproto.laser_max_energy;
    if ( baseEnergy <= 0.0f )
        return 0.0f;

    if ( damageMult <= 0.0f )
        damageMult = 1.0f;

    return baseEnergy * damageMult;
}

int ypabact_LaserTickDamage(NC_STACK_ypabact *shooter,
                                   const World::TWeapProto &wproto,
                                   NC_STACK_ypabact *target,
                                   int connectedTicks, float damageMult)
{
    if ( !target )
        return 0;

    const float baseEnergy = ypabact_LaserNominalTickEnergy(wproto, connectedTicks, damageMult);
    if ( baseEnergy <= 0.0f )
        return 0;

    float damage = baseEnergy * ypabact_LaserEnergyScale(wproto, target);
    if ( damage <= 0.0f )
        return 0;

    float shield = target->GetEffectiveShield();
    if ( shield >= 100.0f )
        return 0;

    float divisor = ( target->getBACT_inputting() || target->getBACT_viewer() ) ? 250.0f : 100.0f;
    int tickDamage = (int)ceil(damage * (100.0f - shield) / divisor);
    return tickDamage > 0 ? tickDamage : 0;
}

bool ypabact_CanApplyLaserDamage(NC_STACK_ypabact *shooter)
{
    if ( !shooter || !shooter->getBACT_pWorld() )
        return false;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    NC_STACK_ypabact *userHost = world->getYW_userHostStation();
    return !world->_isNetGame || (userHost && userHost->_owner == shooter->_owner);
}

void ypabact_ApplyLaserUnitTick(NC_STACK_ypabact *shooter, World::TWeapProto &wproto,
                                       NC_STACK_ypabact *target,
                                       NC_STACK_ypabact::TLaserBeamRuntime &beam,
                                       bool playerControlled, float damageMult)
{
    if ( !shooter || !target )
        return;

    // AI laser contact may remain visible, but only player-controlled lasers
    // can affect units belonging to the shooter's faction.
    const bool aiFriendlyContact = !playerControlled &&
        shooter->_owner != World::OWNER_0 && target->_owner == shooter->_owner;

    // Laser contact is a hard counter to the Deflect Buff. Break all remaining
    // Buff charges, then keep the normal laser damage/debuff path unchanged.
    if ( !aiFriendlyContact && target->HasDeflectBuff() &&
         ypabact_CanApplyLaserDamage(shooter) )
        target->ClearBuffDeflectCharges();

    if ( beam.next_damage_time > 0 && shooter->_clock < beam.next_damage_time )
        return;

    int applyNow = ypabact_LaserTickDamage(shooter, wproto, target, beam.energy_ticks, damageMult);

    if ( applyNow > 0 && !aiFriendlyContact && ypabact_CanApplyLaserDamage(shooter) )
    {
        bact_arg84 dmg;
        dmg.energy = -applyNow;
        dmg.unit = shooter;
        target->ModifyEnergy(&dmg);

        if ( wproto.debuff.valid && target->_energy > 0 && target->_status != BACT_STATUS_DEAD )
            target->ApplyDebuff(wproto.debuff, shooter);
    }

    beam.energy_ticks++;
    beam.next_damage_time = shooter->_clock + ypabact_LaserDamageInterval(wproto, playerControlled);
}

bool ypabact_LaserGetSectorHit(NC_STACK_ypaworld *world, const vec3d &pos, TLaserWorldHit *outHit)
{
    if ( !world )
        return false;

    Common::Point sec = World::PositionToSectorID(pos);
    if ( !world->IsSector(sec) )
        return false;

    cellArea &cell = world->SectorAt(sec);

    if ( !cell.IsGamePlaySector() || cell.PurposeType == cellArea::PT_CONSTRUCTING )
        return false;

    int outX = 0;
    int outY = 0;

    if ( cell.SectorType != 1 )
    {
        int sx = (int)(pos.x / 150.0) % 8;
        int sy = (int)(-pos.z / 150.0) % 8;

        int xSlot = sx < 3 ? 1 : (sx < 5 ? 2 : 3);
        int ySlot = sy < 3 ? 1 : (sy < 5 ? 2 : 3);

        outX = xSlot - 1;
        outY = 2 - (ySlot - 1);
    }

    if ( outHit )
    {
        outHit->cellId = sec;
        outHit->bldX = outX;
        outHit->bldY = outY;
        outHit->damagePos = pos;
    }

    return true;
}

int32_t ypabact_LaserSectorTargetId(const TLaserWorldHit &hit)
{
    int slot = hit.bldY * 3 + hit.bldX;
    int32_t id = (int32_t)((hit.cellId.y * 1024 + hit.cellId.x) * 9 + slot + 1);
    return id > 0 ? -id : -1;
}

int ypabact_LaserTickSectorEnergy(const World::TWeapProto &wproto, int connectedTicks)
{
    const float baseEnergy = ypabact_LaserNominalTickEnergy(wproto, connectedTicks);

    // Keep the same public/internal energy scale as normal weapons and laser unit
    // damage: energy = 1000 means "10" in script/HUD terms. The sector/building
    // path applies its own vanilla conversion inside ypaworld_func129().
    return baseEnergy > 0.0f ? (int)ceil(baseEnergy) : 0;
}

// Hitscan along the forward beam: returns the nearest valid damage target whose
// body the beam passes through, within range. This is the final damage trace, so
// it intentionally includes allies/friendly fire; AI target selection is filtered
// separately by ypabact_LaserAutoTarget().
bool ypabact_LaserHitscan(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto,
                                 const vec3d &origin, const vec3d &dir, float range,
                                 TLaserUnitHit *outHit)
{
    if ( outHit )
        *outHit = TLaserUnitHit();

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    if ( !world )
        return false;

    // Scan the whole square of sectors covering the beam, so off-axis bodies are found.
    int sectorRadius = (int)(range / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestAlong = range + 1.0f;
    vec3d bestHitPoint;
    if (world->_collisionScene)
        for (auto *bct : world->_collisionScene->ShapeTargets(origin, origin + dir * range, std::max(0.0f, wproto.radius)))
        {
            if (!ypabact_IsLaserDamageTarget(shooter, bct)) continue;
            Collision::Contact contact;
            if (world->_collisionScene->Trace(bct, origin, origin + dir * range, std::max(0.0f, wproto.radius), &contact))
            {
                const float along = contact.fraction * range;
                if (along < bestAlong) { best = bct; bestAlong = along; bestHitPoint = contact.point; }
            }
        }

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if (bct->HasCollisionShape() || !ypabact_IsLaserDamageTarget(shooter, bct))
                    continue;

                float weaponRadius = wproto.radius > 0.0f ? wproto.radius : 1.0f;
                float bodyAlong = range + 1.0f;
                vec3d bodyHitPoint;
                bool bodyHit = false;
                World::rbcolls *colls = bct->getBACT_collNodes();

                auto testBodySphere = [&](const vec3d &sphereCenter, float sphereRadius)
                {
                    if ( sphereRadius <= 0.01f )
                        return;

                    vec3d to = sphereCenter - origin;
                    float along = to.dot(dir);
                    if ( along <= 0.0f || along > range )
                        return;

                    vec3d closest = origin + dir * along;
                    if ( (sphereCenter - closest).length() > sphereRadius + weaponRadius )
                        return;

                    if ( along < bodyAlong )
                    {
                        bodyAlong = along;
                        bodyHitPoint = closest;
                        bodyHit = true;
                    }
                };

                if ( colls )
                {
                    if ( bct->HasManualCompoundCollision() && bct->UsesLegacyRadiusCollision() )
                        testBodySphere(bct->_position, bct->_radius);

                    mat3x3 rotT = bct->_rotation.Transpose();
                    for (const World::TRoboColl &sphere : colls->roboColls)
                    {
                        vec3d sphereCenter = bct->_position + rotT.Transform(sphere.coll_pos);
                        testBodySphere(sphereCenter, sphere.robo_coll_radius);
                    }
                }
                else
                {
                    testBodySphere(bct->_position, bct->_radius);
                }

                if ( !bodyHit )
                    continue;

                if ( bodyAlong < bestAlong )
                {
                    bestAlong = bodyAlong;
                    bestHitPoint = bodyHitPoint;
                    best = bct;
                }
            }
        }
    }

    if ( !best )
        return false;

    if ( outHit )
    {
        outHit->target = best;
        outHit->hitPoint = bestHitPoint;
        outHit->along = bestAlong;
    }

    return true;
}

// Shared automatic Laser target acquisition. Callers provide their own alignment
// threshold so player aim-assist can stay narrow without changing the proven AI cone.
NC_STACK_ypabact *ypabact_LaserAutoTarget(NC_STACK_ypabact *shooter, const vec3d &origin,
                                                 const vec3d &dir, float range, float minAlignment)
{
    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    if ( !world )
        return nullptr;

    int sectorRadius = (int)(range / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = range + 1.0f;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if ( !ypabact_IsLaserAimTarget(shooter, bct) )
                    continue;

                vec3d to = bct->_position - origin;
                float dist = to.length();
                if ( dist < 0.001f || dist > range )
                    continue;
                if ( to.dot(dir) / dist < minAlignment )
                    continue;

                if ( dist < bestDist )
                {
                    bestDist = dist;
                    best = bct;
                }
            }
        }
    }

    return best;
}

NC_STACK_ypabact *ypabact_RetainPlayerLaserTarget(NC_STACK_ypabact *shooter,
                                                         const vec3d &origin,
                                                         const vec3d &aimDir,
                                                         float range, int32_t targetGid)
{
    if ( !shooter || !shooter->getBACT_pWorld() || targetGid <= 0 || range <= 0.0f )
        return nullptr;

    NC_STACK_ypabact *target = shooter->getBACT_pWorld()->FindLiveBactByGid(targetGid);
    if ( !ypabact_IsLaserAimTarget(shooter, target) )
        return nullptr;

    vec3d toTarget = target->_position - origin;
    const float distance = toTarget.length();
    if ( distance < 0.001f || distance > range )
        return nullptr;

    vec3d forward = aimDir;
    if ( forward.normalise() < 0.001f )
        return nullptr;

    if ( toTarget.dot(forward) / distance < PLAYER_LASER_HOLD_ALIGNMENT )
        return nullptr;

    return target;
}

bool ypabact_LaserTargetInList(NC_STACK_ypabact *target, const std::vector<NC_STACK_ypabact *> &targets)
{
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

bool ypabact_IsLaserFriendlyToShooter(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit)
{
    return shooter && unit && unit->_owner != World::OWNER_0 && unit->_owner == shooter->_owner;
}

bool ypabact_IsLaserEnemyToShooter(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit)
{
    return shooter && unit && unit->_owner != World::OWNER_0 && unit->_owner != shooter->_owner;
}

// Shared owner/validity filter for laser secondary targets. Used both for direct
// multi-target beams and for chain jumps (identical logic); "friendly" selects
// same-owner vs enemy-owner targets.
bool ypabact_IsLaserSecondaryTargetCandidate(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit,
                                                    bool friendly)
{
    if ( !ypabact_IsLaserDamageTarget(shooter, unit) )
        return false;

    if ( unit->IsInvisibleUnrevealed() || (!friendly && unit->IsIgnoredByAI()) )
        return false;

    if ( !shooter || unit->_owner == World::OWNER_0 )
        return false;

    if ( !shooter->IsPrimaryWeaponElevationAllowed(unit->_position - shooter->_position) )
        return false;

    if ( friendly )
        return ypabact_IsLaserFriendlyToShooter(shooter, unit);

    return ypabact_IsLaserEnemyToShooter(shooter, unit);
}

NC_STACK_ypabact *ypabact_FindNearestLaserMultiTarget(NC_STACK_ypabact *shooter, const vec3d &origin,
                                                            const vec3d &aimDir, float range,
                                                            bool friendlyTargets,
                                                            const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !shooter || !shooter->getBACT_pWorld() || range <= 0.0f )
        return nullptr;

    vec3d forward = aimDir;
    bool useForwardFilter = forward.normalise() >= 0.001f;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    int sectorRadius = (int)(range / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = range + 1.0f;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, bct, friendlyTargets) )
                    continue;
                if ( ypabact_LaserTargetInList(bct, excluded) )
                    continue;

                vec3d to = bct->_position - origin;
                float dist = to.length();
                if ( dist < 0.001f || dist > range )
                    continue;
                if ( useForwardFilter && to.dot(forward) / dist < 0.4f )
                    continue;

                if ( dist < bestDist )
                {
                    bestDist = dist;
                    best = bct;
                }
            }
        }
    }

    return best;
}

NC_STACK_ypabact *ypabact_FindNearestLaserChainUnit(NC_STACK_ypabact *shooter, const vec3d &origin,
                                                          float range, bool friendlyChain,
                                                          const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !shooter || !shooter->getBACT_pWorld() || range <= 0.0f )
        return nullptr;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    int sectorRadius = (int)(range / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = range + 1.0f;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, bct, friendlyChain) )
                    continue;
                if ( ypabact_LaserTargetInList(bct, excluded) )
                    continue;

                vec3d to = bct->_position - origin;
                float dist = to.length();
                if ( dist < 0.001f || dist > range )
                    continue;

                if ( dist < bestDist )
                {
                    bestDist = dist;
                    best = bct;
                }
            }
        }
    }

    return best;
}

void ypabact_AddLaserMultiTargetRequests(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto,
                                                std::vector<NC_STACK_ypabact::TLaserBeamRequest> *requests,
                                                float range, bool friendlyTargets, bool playerControlled)
{
    if ( !shooter || !requests || requests->empty() || wproto.laser_beam_count <= 1 )
        return;
    if ( friendlyTargets && !playerControlled )
        return;

    size_t originalRequestCount = requests->size();
    if ( originalRequestCount >= (size_t)wproto.laser_beam_count )
        return;

    std::vector<NC_STACK_ypabact *> selectedTargets;
    selectedTargets.reserve((size_t)wproto.laser_beam_count);

    for (const NC_STACK_ypabact::TLaserBeamRequest &request : *requests)
    {
        if ( request.target && ypabact_IsLaserSecondaryTargetCandidate(shooter, request.target, friendlyTargets) &&
             !ypabact_LaserTargetInList(request.target, selectedTargets) )
            selectedTargets.push_back(request.target);
    }

    while ( requests->size() < (size_t)wproto.laser_beam_count )
    {
        size_t sourceIndex = (requests->size() - originalRequestCount) % originalRequestCount;
        const NC_STACK_ypabact::TLaserBeamRequest &source = requests->at(sourceIndex);
        vec3d primaryAim = requests->front().dir;
        if ( primaryAim.normalise() < 0.001f )
            primaryAim = shooter->_rotation.AxisZ();

        NC_STACK_ypabact *target = ypabact_FindNearestLaserMultiTarget(shooter, source.start, primaryAim,
                                                                       range, friendlyTargets, selectedTargets);
        if ( !target )
            break;

        NC_STACK_ypabact::TLaserBeamRequest request;
        request.target = target;
        request.start = source.start;
        request.dir = target->_position - request.start;
        if ( request.dir.normalise() < 0.001f )
            request.dir = source.dir;
        if ( request.dir.normalise() < 0.001f )
            request.dir = shooter->_rotation.AxisZ();

        requests->push_back(request);
        selectedTargets.push_back(target);
    }
}

bool ypabact_PrepareLaserSoundSource(TSoundSource &snd, World::TVhclSound &fx)
{
    fx.LoadSamples();

    TSampleData *mainSample = fx.MainSample.Sample
                            ? fx.MainSample.Sample->GetSampleData()
                            : nullptr;

    snd.PSample = mainSample;

    snd.Volume = fx.volume;
    fx.ConfigureSoundSourcePitch(snd);
    snd.Radius = fx.radius;
    snd.PriorityBias = 0;
    snd.SetLoop(false);
    snd.SetFragmented(false);
    snd.PFragments = nullptr;

    if ( fx.sndPrm.slot )
    {
        snd.PPFx = &fx.sndPrm;
        snd.SetPFx(true);
    }
    else
    {
        snd.PPFx = nullptr;
        snd.SetPFx(false);
    }

    if ( fx.sndPrm_shk.slot )
    {
        snd.PShkFx = &fx.sndPrm_shk;
        snd.SetShk(true);
    }
    else
    {
        snd.PShkFx = nullptr;
        snd.SetShk(false);
    }

    return mainSample || fx.sndPrm.slot || fx.sndPrm_shk.slot;
}

bool ypabact_LaserSoundHasContent(World::TVhclSound &fx)
{
    fx.LoadSamples();
    return fx.MainSample.Sample || fx.sndPrm.slot || fx.sndPrm_shk.slot;
}

void ypabact_UpdateLaserNormalSound(NC_STACK_ypabact *bact,
                                            World::TWeapProto &wproto,
                                            TSndCarrier *carrier,
                                            const vec3d &position,
                                            const vec3d &direction)
{
    if ( !bact || !carrier )
        return;

    World::TVhclSound &normalFx = wproto.sndFXes[World::TWeapProto::SND_NORMAL];

    if ( carrier->Sounds.size() != 2 )
    {
        if ( !carrier->Sounds.empty() )
            SFXEngine::SFXe.StopCarrier(carrier);
        carrier->Resize(2);
    }

    normalFx.LoadSamples();
    TSoundSource &snd = carrier->Sounds[0];
    TSoundSource &sustained = carrier->Sounds[1];
    if ( !normalFx.MainSample.Sample && !normalFx.sndPrm.slot &&
         !normalFx.sndPrm_shk.slot )
    {
        if ( snd.IsEnabled() || sustained.IsPFxEnabled() || sustained.IsShkEnabled() )
            SFXEngine::SFXe.StopCarrier(carrier);
        return;
    }

    carrier->Position = position;
    carrier->Vector = direction;

    // Keep the normal sample serialized independently of sustained PAL/SHK.
    if ( normalFx.MainSample.Sample && !snd.IsEnabled() )
    {
        if ( ypabact_PrepareLaserSoundSource(snd, normalFx) )
        {
            snd.PPFx = nullptr;
            snd.PShkFx = nullptr;
            snd.SetPFx(false);
            snd.SetShk(false);
            SFXEngine::SFXe.startSound(carrier, 0);
        }
    }
    else
    {
        snd.Volume = normalFx.volume;
        snd.Radius = normalFx.radius;
    }

    // A looped, sample-free source holds mag0 until the laser stops; the sound
    // engine ignores authored normal PAL/SHK times for this source.
    if ( (normalFx.sndPrm.slot || normalFx.sndPrm_shk.slot) &&
         !sustained.IsPFxEnabled() && !sustained.IsShkEnabled() )
    {
        sustained.PSample = nullptr;
        sustained.PPFx = normalFx.sndPrm.slot ? &normalFx.sndPrm : nullptr;
        sustained.PShkFx = normalFx.sndPrm_shk.slot ? &normalFx.sndPrm_shk : nullptr;
        sustained.SetPFx(sustained.PPFx != nullptr);
        sustained.SetShk(sustained.PShkFx != nullptr);
        sustained.SetLoop(true);
        sustained.FadeDuration = 0.0;
        SFXEngine::SFXe.startSound(carrier, 1);
    }

    SFXEngine::SFXe.UpdateSoundCarrier(carrier);
}

void ypabact_PlayLaserLaunchSound(NC_STACK_ypabact *bact,
                                          World::TWeapProto &wproto,
                                          const vec3d &position,
                                          const vec3d &direction)
{
    if ( !bact )
        return;

    World::TVhclSound &launchFx = wproto.sndFXes[World::TWeapProto::SND_LAUNCH];
    TSndCarrier &carrier = bact->_laser_launch_soundcarrier;

    if ( !ypabact_LaserSoundHasContent(launchFx) )
        return;

    if ( carrier.Sounds.empty() )
        carrier.Resize(1);

    TSoundSource &snd = carrier.Sounds[0];

    // A new FIRE activation owns exactly one launch event. If the player releases
    // and presses again before the previous launch sample ends, restart this same
    // carrier so the new activation is audible immediately without overlap.
    if ( snd.IsEnabled() || snd.IsPFxEnabled() || snd.IsShkEnabled() )
        SFXEngine::SFXe.StopCarrier(&carrier);

    if ( !ypabact_PrepareLaserSoundSource(snd, launchFx) )
        return;

    carrier.Position = position;
    carrier.Vector = direction;
    SFXEngine::SFXe.startSound(&carrier, 0);
    SFXEngine::SFXe.UpdateSoundCarrier(&carrier);
}

void ypabact_UpdateLaserHitSound(
    NC_STACK_ypabact *bact, World::TWeapProto &wproto, TSndCarrier *carrier,
    const std::vector<NC_STACK_ypabact::TLaserBeamRuntime> &beams)
{
    if ( !bact || !carrier )
        return;

    const NC_STACK_ypabact::TLaserBeamRuntime *contactBeam = nullptr;
    for (const NC_STACK_ypabact::TLaserBeamRuntime &beam : beams)
    {
        if ( beam.has_contact )
        {
            contactBeam = &beam;
            break;
        }
    }

    World::TVhclSound &hitFx = wproto.sndFXes[World::TWeapProto::SND_HIT];
    hitFx.LoadSamples();
    TSampleData *sample = hitFx.MainSample.Sample
                        ? hitFx.MainSample.Sample->GetSampleData()
                        : nullptr;

    if ( !contactBeam || !sample )
    {
        if ( !carrier->Sounds.empty() && carrier->Sounds[0].IsEnabled() )
            SFXEngine::SFXe.StopCarrier(carrier);
        return;
    }

    if ( carrier->Sounds.empty() )
        carrier->Resize(1);

    TSoundSource &snd = carrier->Sounds[0];
    snd.PSample = sample;
    snd.Volume = hitFx.volume;
    hitFx.ConfigureSoundSourcePitch(snd);
    snd.Radius = hitFx.radius;
    snd.PriorityBias = 0;
    snd.SetLoop(false);
    snd.SetFragmented(false);
    snd.PFragments = nullptr;
    snd.PPFx = nullptr;
    snd.SetPFx(false);
    snd.PShkFx = nullptr;
    snd.SetShk(false);

    // One shared carrier per continuous weapon: with multiple direct/chain
    // beams there is still only one ordered hit sound. The source naturally
    // becomes disabled at the end of the one-shot; if contact persists the
    // next update starts exactly one new play, never overlapping the previous.
    carrier->Position = contactBeam->end;
    carrier->Vector = bact->_fly_dir * bact->_fly_dir_length;

    if ( !snd.IsEnabled() )
        SFXEngine::SFXe.startSound(carrier, 0);

    SFXEngine::SFXe.UpdateSoundCarrier(carrier);
}

void ypabact_StoreHUDLaserMultiLockTargets(NC_STACK_ypabact *shooter,
                                                  const std::vector<NC_STACK_ypabact::TLaserBeamRequest> &requests)
{
    if ( !shooter || !shooter->getBACT_pWorld() || !(shooter->_oflags & BACT_OFLAG_USERINPT) )
        return;

    std::vector<NC_STACK_ypabact *> targets;
    targets.reserve(requests.size());

    for (const NC_STACK_ypabact::TLaserBeamRequest &request : requests)
    {
        if ( request.target && ypabact_IsLaserAimTarget(shooter, request.target) &&
             !ypabact_LaserTargetInList(request.target, targets) )
            targets.push_back(request.target);
    }

    ypabact_StoreHUDMissileMultiLockTargets(shooter, targets);
}

void ypabact_UpdateHUDLaserMultiLockTargets(NC_STACK_ypabact *shooter, const bact_arg79 *arg,
                                                   const World::TWeapProto &wproto)
{
    if ( !shooter || !arg || !shooter->getBACT_pWorld() || !(shooter->_oflags & BACT_OFLAG_USERINPT) )
        return;

    if ( !wproto.IsLaser() || wproto.laser_beam_count <= 1 )
    {
        shooter->getBACT_pWorld()->_hudMissileMultiLockTargets.clear();
        return;
    }

    if ( arg->tgType != BACT_TGT_TYPE_UNIT || !ypabact_IsLaserAimTarget(shooter, arg->target.pbact) )
    {
        shooter->getBACT_pWorld()->_hudMissileMultiLockTargets.clear();
        return;
    }

    std::vector<NC_STACK_ypabact::TLaserBeamRequest> requests;
    requests.resize(1);
    requests[0].target = arg->target.pbact;
    requests[0].start = ypabact_LaserSourceOrigin(shooter);
    requests[0].dir = arg->target.pbact->_position - requests[0].start;

    if ( requests[0].dir.normalise() < 0.001f )
        requests[0].dir = shooter->_rotation.AxisZ();

    ypabact_AddLaserMultiTargetRequests(shooter, wproto, &requests, ypabact_LaserRange(wproto), false, true);
    ypabact_StoreHUDLaserMultiLockTargets(shooter, requests);
}

NC_STACK_ypabact *ypabact_FindLaserChainTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *from,
                                                      float radius, bool friendlyChain,
                                                      const std::vector<NC_STACK_ypabact *> &hitHistory)
{
    if ( !from || radius <= 0.0f )
        return nullptr;

    return ypabact_FindNearestLaserChainUnit(shooter, from->_position, radius, friendlyChain, hitHistory);
}

vec3d ypabact_LaserSourceOrigin(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return vec3d(0.0, 0.0, 0.0);

    vec3d localOffset = bact->_fire_pos;

    // First-person/viewer fire originates from the actual view point plus the single
    // configured fire_x/fire_y/fire_z muzzle offset. num_weapons never expands this
    // into multiple laser sources.
    if ( bact->getBACT_viewer() )
        return bact->GetBodyPosition() + bact->_rotation.Transpose().Transform(bact->_viewer_position + localOffset);

    return bact->GetBodyPosition() + bact->_rotation.Transpose().Transform(localOffset);
}

bool ypabact_LaserWorldHit(NC_STACK_ypabact *shooter, const vec3d &origin,
                                      const vec3d &dir, float range, vec3d *outHitPoint,
                                      ypaworld_arg136 *outHit)
{
    if ( !shooter || !shooter->getBACT_pWorld() || range <= 1.0f )
        return false;

    vec3d rayDir = dir;
    if ( rayDir.normalise() < 0.001f )
        return false;

    // ypaworld_func136() is reliable for short ray spans, but it only checks a tiny
    // collision set derived from the segment start/end grid cells. A full laser beam
    // can be several sectors long, so one single long ray may miss terrain/buildings
    // for many frames and make vp_megadeth appear only sporadically. Walk the beam
    // in short segments and return the first real world contact.
    const float maxSegmentLen = 240.0f;
    float travelled = 0.0f;

    while ( travelled < range )
    {
        float segmentLen = range - travelled;
        if ( segmentLen > maxSegmentLen )
            segmentLen = maxSegmentLen;

        ypaworld_arg136 ray;
        ray.stPos = origin + rayDir * travelled;
        ray.vect = rayDir * segmentLen;
        ray.flags = 0;

        shooter->getBACT_pWorld()->ypaworld_func136(&ray);

        if ( ray.isect )
        {
            vec3d toHit = ray.isectPos - origin;
            float along = toHit.dot(rayDir);

            // Ignore bogus/backward hits and keep the contact inside the requested beam range.
            if ( along >= 0.0f && along <= range + 1.0f )
            {
                if ( outHitPoint )
                    *outHitPoint = ray.isectPos;
                if ( outHit )
                    *outHit = ray;

                return true;
            }
        }

        travelled += segmentLen;
    }

    return false;
}

mat3x3 ypabact_LaserRotationFromDir(const vec3d &beamDir, const mat3x3 &fallback)
{
    vec3d z = beamDir;
    if ( z.normalise() < 0.001 )
        return fallback;

    vec3d x = fallback.AxisX();
    x -= z * x.dot(z);

    if ( x.normalise() < 0.001 )
    {
        x = (fabs(z.y) < 0.9) ? vec3d(0.0, 1.0, 0.0) : vec3d(1.0, 0.0, 0.0);
        x = x * z;
        if ( x.normalise() < 0.001 )
            return fallback;
    }

    vec3d y = z * x;
    if ( y.normalise() < 0.001 )
        return fallback;

    mat3x3 rot;
    rot.SetX(x);
    rot.SetY(y);
    rot.SetZ(z);
    return rot;
}

void ypabact_SpawnLaserImpactChainFX(
    NC_STACK_ypabact *shooter, World::TWeapProto &wproto,
    const NC_STACK_ypabact::TLaserBeamRuntime &beam, const vec3d &dir,
    bool worldContact, std::vector<vec3d> &spawnedPoints)
{
    if ( !shooter || wproto.chain_fx.empty() )
        return;

    bool relevant = false;
    for (const World::TChainFXConfig &fx : wproto.chain_fx)
        if ( fx.trigger == World::TChainFXConfig::TRIGGER_DETONATE ||
             (worldContact && fx.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD) )
            relevant = true;
    if ( !relevant )
        return;

    for (const vec3d &point : spawnedPoints)
        if ( (point - beam.end).length() < 1.0f )
            return;
    spawnedPoints.push_back(beam.end);

    mat3x3 rotation = ypabact_LaserRotationFromDir(dir, shooter->_rotation);
    if ( worldContact )
    {
        // Decals need the short-lived collision polygon, not just beam.end.
        bool needsWorldHit = false;
        for (const World::TChainFXConfig &fx : wproto.chain_fx)
            if ( fx.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
                 fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL )
                needsWorldHit = true;

        ypaworld_arg136 collision;
        const ypaworld_arg136 *hit = nullptr;
        if ( needsWorldHit )
        {
            vec3d confirmed;
            const float reach = (beam.end - beam.start).length() + 1.0f;
            if ( ypabact_LaserWorldHit(shooter, beam.start, dir, reach,
                                      &confirmed, &collision) &&
                 (confirmed - beam.end).length() < 2.0f )
                hit = &collision;
        }

        shooter->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_IMPACT_WORLD,
                                       hit, &wproto.chain_fx, &beam.end, &rotation);
    }

    // Laser contact is the projectile-equivalent detonation event. The same
    // authored trigger works for unit and world hits without a new parser name.
    shooter->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DETONATE,
                                   nullptr, &wproto.chain_fx, &beam.end, &rotation);
}

float ypabact_LaserClampVisualSpacing(float spacing)
{
    if ( spacing <= 0.0f )
        spacing = 40.0f;
    if ( spacing < 20.0f )
        spacing = 20.0f;
    if ( spacing > 500.0f )
        spacing = 500.0f;
    return spacing;
}

vec3d ypabact_LaserVisualScale(const World::TWeapProto &wproto)
{
    return vec3d(wproto.visual_scale.x > 0.0f ? wproto.visual_scale.x : 1.0f,
                 wproto.visual_scale.y > 0.0f ? wproto.visual_scale.y : 1.0f,
                 wproto.visual_scale.z > 0.0f ? wproto.visual_scale.z : 1.0f);
}

void ypabact_SpawnLaserBeamVisuals(NC_STACK_ypabact *bact, const World::TWeapProto &wproto,
                                      const vec3d &beamStart, const vec3d &beamEnd)
{
    if ( !bact )
        return;

    // Enabling the external laser mesh explicitly replaces the continuous
    // normal-state segmented body. The external 3DS is mandatory in this mode: missing or
    // invalid geometry intentionally leaves the beam body invisible rather
    // than falling back to the normal visual or hidden procedural geometry.
    if ( wproto.laser_mesh.enabled ||
         (wproto.vp_normal <= 0 && wproto.visual_3ds.normal.empty() &&
          wproto.visual_base.normal.empty()) )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !world )
        return;

    vec3d span = beamEnd - beamStart;
    float len = span.length();
    if ( len < 1.0f )
        return;

    vec3d dir = span / len;
    mat3x3 rot = ypabact_LaserRotationFromDir(dir, bact->_rotation);

    vec3d axisScale = ypabact_LaserVisualScale(wproto);

    // Visual-only density control: damage timing stays controlled only by
    // laser_energy_tick_time, while radius remains the gameplay hit thickness.
    float spacing = ypabact_LaserClampVisualSpacing(wproto.laser_spacing);

    vec3d visualStart = beamStart;
    if ( bact->getBACT_viewer() || bact->getBACT_inputting() )
    {
        float lead = spacing * 0.5f;
        if ( lead < 16.0f )
            lead = 16.0f;
        if ( lead > len * 0.25f )
            lead = len * 0.25f;

        visualStart += dir * lead;
        span = beamEnd - visualStart;
        len = span.length();
        if ( len < 1.0f )
            return;
    }

    int count = (int)(len / spacing) + 2;
    if ( count < 2 ) count = 2;
    if ( count > 320 ) count = 320;

    for (int i = 0; i < count; i++)
    {
        float t = ((float)i + 0.5f) / (float)count;
        vec3d pos = visualStart + span * t;
        // OpenNeoUA custom: the laser beam body uses vp_normal, so honour the weapon's
        // main VP controls here. Impact/launch FX below deliberately stay neutral.
        world->SpawnTransientVisual(wproto.vp_normal, wproto.visual_3ds.normal,
                                    wproto.visual_base.normal, pos, rot, 45, 1.0,
                                    wproto.visual_tint, axisScale, wproto.visual_spin);
    }
}

void NC_STACK_ypabact::RequestLaserFire(int weaponId, bact_arg79 *arg)
{
    // OpenNeoUA invisible: firing the continuous laser is a real attack -> reveal now,
    // in the same frame the beam is requested.
    RevealInvisibleOnAttack();

    _laser_weapon = weaponId;
    _laser_fire_request = true;

    ypabact_StartVehicleFireVPForWeapon(this, weaponId, _clock);

    TLaserBeamRequest request;
    request.target = (arg->tgType == BACT_TGT_TYPE_UNIT) ? arg->target.pbact : nullptr;

    // Stable laser source: model=laser ignores num_weapons as a beam count, but
    // still honours fire_x/fire_y/fire_z as one muzzle/source offset.
    request.start = ypabact_LaserSourceOrigin(this);

    request.dir = arg->direction;

    if ( request.dir.normalise() < 0.001 )
    {
        if ( arg->tgType == BACT_TGT_TYPE_UNIT && arg->target.pbact )
            request.dir = arg->target.pbact->_position - request.start;
        else if ( arg->tgType == BACT_TGT_TYPE_CELL )
            request.dir = arg->tgt_pos - request.start;
        else if ( arg->tgType == BACT_TGT_TYPE_DRCT )
            request.dir = arg->tgt_pos;
    }

    if ( request.dir.normalise() < 0.001 )
        request.dir = _rotation.AxisZ();

    _laser_requests.push_back(request);

    // Compatibility mirror for older single-beam debug/UI code paths.
    _laser_target = request.target;
    _laser_request_start = request.start;
    _laser_request_dir = request.dir;
}

void NC_STACK_ypabact::StopLaser()
{
    // Disconnect: stop active snd_normal/snd_hit playback, drop the beam and reset tick state.
    if ( !_laser_soundcarrier.Sounds.empty() &&
         (_laser_soundcarrier.Sounds[0].IsEnabled() ||
          _laser_soundcarrier.Sounds[0].IsPFxEnabled() ||
          _laser_soundcarrier.Sounds[0].IsShkEnabled() ||
          (_laser_soundcarrier.Sounds.size() > 1 &&
           (_laser_soundcarrier.Sounds[1].IsPFxEnabled() ||
            _laser_soundcarrier.Sounds[1].IsShkEnabled()))) )
        SFXEngine::SFXe.StopCarrier(&_laser_soundcarrier);
    if ( !_laser_hit_soundcarrier.Sounds.empty() &&
         _laser_hit_soundcarrier.Sounds[0].IsEnabled() )
        SFXEngine::SFXe.StopCarrier(&_laser_hit_soundcarrier);

    _laser_active = false;
    _laser_fire_request = false;
    _laser_weapon = -1;
    _laser_energy_ticks = 0;
    _laser_target_gid = 0;
    _laser_target = nullptr;
    _laser_request_start = vec3d(0.0, 0.0, 0.0);
    _laser_request_dir = vec3d(0.0, 0.0, 0.0);
    _laser_beam_start = vec3d(0.0, 0.0, 0.0);
    _laser_beam_end = vec3d(0.0, 0.0, 0.0);
    _laser_next_damage_time = 0;
    _laser_next_fx_time = 0;
    _laser_next_beam_vp_time = 0;
    _laser_requests.clear();
    _laser_beams.clear();
}

void NC_STACK_ypabact::UpdateLaser(update_msg *arg)
{
    if ( IsActiveDebuffStunFireBlocked() )
    {
        StopLaser();
        return;
    }

    bool requested = _laser_fire_request;
    std::vector<TLaserBeamRequest> requests = _laser_requests;
    _laser_fire_request = false;
    _laser_target = nullptr;
    _laser_requests.clear();

    // Not firing this frame, or weapon invalid => beam off.
    if ( !requested || requests.empty() ||
         !_world || _laser_weapon < 0 || (size_t)_laser_weapon >= _world->GetWeaponsProtos().size() )
    {
        StopLaser();
        return;
    }

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_laser_weapon);
    if ( !wproto.IsLaser() )
    {
        StopLaser();
        return;
    }

    float range = ypabact_LaserRange(wproto);
    bool playerControlled = getBACT_inputting() || getBACT_viewer();

    bool wasActive = _laser_active;
    size_t oldBeamCount = _laser_beams.size();
    _laser_active = true;

    if ( !wasActive )
    {
        vec3d launchPos = requests.front().start;
        vec3d launchDir = requests.front().dir;
        if ( launchDir.normalise() < 0.001f )
            launchDir = _rotation.AxisZ();
        ypabact_PlayLaserLaunchSound(this, wproto, launchPos, launchDir);
    }
    if ( _laser_beams.size() < requests.size() )
        _laser_beams.resize(requests.size());
    size_t activeBeamCount = requests.size();
    NC_STACK_ypabact *primaryHitTarget = nullptr;
    NC_STACK_ypabact *primaryChainGroupTarget = nullptr;
    std::vector<NC_STACK_ypabact *> directHitTargets;
    directHitTargets.reserve(requests.size());
    std::vector<vec3d> chainFXPoints;

    bool spawnBeamVPs = (_laser_next_beam_vp_time <= 0 || _clock >= _laser_next_beam_vp_time);

    for (size_t i = 0; i < requests.size(); i++)
    {
        TLaserBeamRequest &request = requests[i];
        if ( _laser_beams.size() <= i )
            _laser_beams.resize(i + 1);
        TLaserBeamRuntime &beam = _laser_beams[i];
        bool beamWasActive = wasActive && i < oldBeamCount;

        // Forward aim direction. Its reach comes from the weapon's life_time.
        vec3d dir = request.dir;
        if ( dir.normalise() < 0.001f )
            dir = _rotation.AxisZ();

        // Aiming model:
        //   * Explicit target -> always aim at that target while it remains valid.
        //   * Player free fire -> aggressively acquire a visible enemy in the current
        //                         aim cone, then retain that unit with mild hysteresis.
        //   * AI free fire     -> keep the existing auto-target behaviour unchanged.
        // The final damage trace below is always run along the final direction and can
        // still hit allies/friendly units if they physically stand in front of the lock.
        NC_STACK_ypabact *aimTarget = nullptr;

        if ( request.target && ypabact_IsLaserDamageTarget(this, request.target) )
        {
            // Preserve explicit-target compatibility, including deliberate friendly
            // fire paths. Only automatic acquisition below is enemy-only.
            aimTarget = request.target;
        }
        else if ( playerControlled )
        {
            if ( beamWasActive && beam.target_gid > 0 )
            {
                aimTarget = ypabact_RetainPlayerLaserTarget(this, request.start, dir,
                                                            range, beam.target_gid);
            }

            if ( !aimTarget )
                aimTarget = ypabact_LaserAutoTarget(this, request.start, dir, range,
                                                     PLAYER_LASER_ACQUIRE_ALIGNMENT);
        }
        else
        {
            aimTarget = ypabact_LaserAutoTarget(this, request.start, dir, range, 0.4f);
        }

        if ( aimTarget )
        {
            vec3d toT = aimTarget->_position - request.start;
            float l = toT.length();
            if ( l > 0.001f && l <= range )
            {
                dir = toT / l;
                request.target = aimTarget;
            }
        }

        TLaserUnitHit unitHit;
        bool hasUnitHit = ypabact_LaserHitscan(this, wproto, request.start, dir, range, &unitHit);

        vec3d worldHitPoint;
        bool worldHit = ypabact_LaserWorldHit(this, request.start, dir, range, &worldHitPoint);
        float worldAlong = range + 1.0f;
        if ( worldHit )
        {
            worldAlong = (worldHitPoint - request.start).dot(dir);
            if ( worldAlong < 0.0f )
                worldHit = false;
        }

        bool useWorldHit = worldHit && (!hasUnitHit || worldAlong <= unitHit.along);
        NC_STACK_ypabact *target = (!useWorldHit && hasUnitHit) ? unitHit.target : nullptr;
        vec3d hitPoint = target ? unitHit.hitPoint : vec3d(0.0, 0.0, 0.0);

        TLaserWorldHit sectorHit;
        bool hasSectorHit = false;

        beam.start = request.start;
        if ( target )
            beam.end = hitPoint;
        else if ( useWorldHit )
            beam.end = worldHitPoint;
        else
            beam.end = request.start + dir * range;
        beam.has_contact = target != nullptr || useWorldHit;

        if ( i == 0 )
        {
            NC_STACK_ypabact *requestedGroupTarget =
                (request.target && request.target->_owner != World::OWNER_0 &&
                 ypabact_IsLaserDamageTarget(this, request.target))
                    ? request.target
                    : nullptr;

            _laser_beam_start = beam.start;
            _laser_beam_end = beam.end;
            _laser_request_start = request.start;
            _laser_request_dir = dir;
            _laser_target = target;
            primaryHitTarget = target;
            primaryChainGroupTarget = requestedGroupTarget ? requestedGroupTarget : target;
            request.target = target;

            if ( primaryChainGroupTarget && primaryChainGroupTarget->_owner != World::OWNER_0 )
            {
                bool friendlyMultiTargets = ypabact_IsLaserFriendlyToShooter(this, primaryChainGroupTarget);
                ypabact_AddLaserMultiTargetRequests(this, wproto, &requests, range,
                                                    friendlyMultiTargets, playerControlled);
            }
            ypabact_StoreHUDLaserMultiLockTargets(this, requests);
        }

        if ( useWorldHit )
        {
            // Step a little beyond the rendered contact, matching the direct-hit missile
            // path. This reliably selects the sector/building slot under the collision point.
            hasSectorHit = ypabact_LaserGetSectorHit(_world, worldHitPoint + dir * 5.0f, &sectorHit);
            if ( !hasSectorHit )
                hasSectorHit = ypabact_LaserGetSectorHit(_world, worldHitPoint, &sectorHit);
        }

        if ( !beamWasActive )
            _world->SpawnTransientVisual(wproto.vp_launch, wproto.visual_3ds.launch,
                                         wproto.visual_base.launch,
                                         beam.start, ypabact_LaserRotationFromDir(dir, _rotation), 90,
                                         1.0, World::TVisualTint(), wproto.launch_scale);

        if ( spawnBeamVPs )
            ypabact_SpawnLaserBeamVisuals(this, wproto, beam.start, beam.end);

        if ( target )
        {
            // Fresh contact or target switch => reset tick timers and optional ramp.
            if ( !beamWasActive || target->_gid != beam.target_gid )
            {
                beam.energy_ticks = 0;
                beam.next_damage_time = 0;
                beam.next_fx_time = 0;
            }
            beam.target_gid = target->_gid;

            if ( target->_owner != World::OWNER_0 && ypabact_IsLaserDamageTarget(this, target) &&
                 !ypabact_LaserTargetInList(target, directHitTargets) )
                directHitTargets.push_back(target);

            ypabact_ApplyLaserUnitTick(this, wproto, target, beam, playerControlled, 1.0f);

            // ---- Throttled impact/contact FX ----
            if ( _clock >= beam.next_fx_time )
            {
                ypabact_SpawnLaserImpactChainFX(this, wproto, beam, dir,
                                                false, chainFXPoints);
                ypabact_SpawnWeaponImpactVisual(_world, wproto, false, beam.end,
                                                   ypabact_LaserRotationFromDir(dir, _rotation), 90);
                beam.next_fx_time = _clock + 160;
            }
        }
        else
        {
            if ( hasSectorHit )
            {
                int32_t sectorTargetId = ypabact_LaserSectorTargetId(sectorHit);
                if ( !beamWasActive || sectorTargetId != beam.target_gid )
                {
                    beam.energy_ticks = 0;
                    beam.next_damage_time = 0;
                    beam.next_fx_time = 0;
                }
                beam.target_gid = sectorTargetId;

                if ( beam.next_damage_time <= 0 || _clock >= beam.next_damage_time )
                {
                    int applyNow = ypabact_LaserTickSectorEnergy(wproto, beam.energy_ticks);

                    NC_STACK_ypabact *userHost = _world->getYW_userHostStation();
                    bool canApplyDamage = !_world->_isNetGame || (userHost && userHost->_owner == _owner);

                    if ( applyNow > 0 && canApplyDamage )
                    {
                        yw_arg129 dmg;
                        dmg.field_0 = 0;
                        dmg.pos = sectorHit.damagePos;
                        dmg.field_10 = applyNow;
                        dmg.unit = this;
                        ChangeSectorEnergy(&dmg);
                    }

                    beam.energy_ticks++;
                    beam.next_damage_time = _clock + ypabact_LaserDamageInterval(wproto, playerControlled);
                }
            }
            else
            {
                // No damage contact: the beam still shows forward, but the ramp resets.
                beam.energy_ticks = 0;
                beam.target_gid = 0;
                beam.next_damage_time = 0;
            }

            // Terrain/building/world contact FX. This is the laser equivalent of a
            // projectile megadeth impact: firing into the ground must show vp_megadeth
            // even when no unit was hit. If vp_megadeth is not configured, fall back to
            // vp_dead so old test weapons still show something instead of nothing.
            if ( useWorldHit && _clock >= beam.next_fx_time )
            {
                ypabact_SpawnLaserImpactChainFX(this, wproto, beam, dir,
                                                true, chainFXPoints);
                ypabact_SpawnWeaponImpactVisual(_world, wproto, true, beam.end,
                                                   ypabact_LaserRotationFromDir(dir, _rotation), 90);
                beam.next_fx_time = _clock + 160;
            }
        }
    }

    activeBeamCount = requests.size();

    if ( wproto.laser_chain_allow && wproto.laser_chain_max_jumps > 0 &&
         wproto.laser_chain_radius > 0.0f && primaryHitTarget &&
         primaryHitTarget->_owner != World::OWNER_0 &&
         ypabact_IsLaserDamageTarget(this, primaryHitTarget) )
    {
        NC_STACK_ypabact *chainGroupTarget = primaryChainGroupTarget ? primaryChainGroupTarget : primaryHitTarget;
        bool friendlyChain = ypabact_IsLaserFriendlyToShooter(this, chainGroupTarget);
        bool canRunChain = (!friendlyChain || playerControlled) &&
                           ypabact_IsLaserSecondaryTargetCandidate(this, primaryHitTarget, friendlyChain);

        if ( canRunChain )
        {
            std::vector<NC_STACK_ypabact *> chainHits;
            chainHits.reserve(directHitTargets.size() + (size_t)wproto.laser_chain_max_jumps + 1);
            chainHits = directHitTargets;
            if ( !ypabact_LaserTargetInList(primaryHitTarget, chainHits) )
                chainHits.push_back(primaryHitTarget);

            NC_STACK_ypabact *from = primaryHitTarget;
            float chainDamageMult = 1.0f;
            float perJumpMult = wproto.laser_chain_energy_mult > 0.0f ? wproto.laser_chain_energy_mult : 1.0f;

            for (int jump = 0; jump < wproto.laser_chain_max_jumps; jump++)
            {
                NC_STACK_ypabact *target = ypabact_FindLaserChainTarget(this, from, wproto.laser_chain_radius,
                                                                        friendlyChain, chainHits);
                if ( !target )
                    break;

                size_t beamIndex = activeBeamCount++;
                if ( _laser_beams.size() <= beamIndex )
                    _laser_beams.resize(beamIndex + 1);

                TLaserBeamRuntime &beam = _laser_beams[beamIndex];
                bool beamWasActive = wasActive && beamIndex < oldBeamCount;

                beam.start = from->_position;
                beam.end = target->_position;
                beam.has_contact = true;

                vec3d dir = beam.end - beam.start;
                if ( dir.normalise() < 0.001f )
                    dir = _rotation.AxisZ();

                if ( !beamWasActive || target->_gid != beam.target_gid )
                {
                    beam.energy_ticks = 0;
                    beam.next_damage_time = 0;
                    beam.next_fx_time = 0;
                }
                beam.target_gid = target->_gid;

                chainDamageMult *= perJumpMult;

                if ( spawnBeamVPs )
                    ypabact_SpawnLaserBeamVisuals(this, wproto, beam.start, beam.end);

                ypabact_ApplyLaserUnitTick(this, wproto, target, beam, playerControlled, chainDamageMult);

                if ( _clock >= beam.next_fx_time )
                {
                    ypabact_SpawnLaserImpactChainFX(this, wproto, beam, dir,
                                                    false, chainFXPoints);
                    ypabact_SpawnWeaponImpactVisual(_world, wproto, false, beam.end,
                                                       ypabact_LaserRotationFromDir(dir, _rotation), 90);
                    beam.next_fx_time = _clock + 160;
                }

                chainHits.push_back(target);
                from = target;
            }
        }
    }

    if ( _laser_beams.size() > activeBeamCount )
        _laser_beams.resize(activeBeamCount);

    if ( spawnBeamVPs )
    {
        // Refresh as fast as the engine clock allows. Keep this at 1ms: 0 would be
        // treated as invalid in several timing paths and would risk duplicate spam in
        // the same update tick.
        _laser_next_beam_vp_time = _clock + 1;
    }

    if ( !_laser_beams.empty() )
    {
        _laser_target_gid = _laser_beams[0].target_gid;
        _laser_energy_ticks = _laser_beams[0].energy_ticks;
        _laser_next_damage_time = _laser_beams[0].next_damage_time;
        _laser_next_fx_time = _laser_beams[0].next_fx_time;
    }

    ypabact_UpdateLaserHitSound(this, wproto, &_laser_hit_soundcarrier, _laser_beams);

    ypabact_UpdateLaserNormalSound(this, wproto, &_laser_soundcarrier,
                                   _laser_beam_start, _fly_dir * _fly_dir_length);
}

vec3d ypabact_VerticalLaserSourceOrigin(NC_STACK_ypabact *bact, const bact_arg79 *arg)
{
    if ( !bact )
        return vec3d(0.0, 0.0, 0.0);

    vec3d localOffset = arg ? arg->start_point : bact->_fire_pos;
    return bact->GetBodyPosition() + bact->_rotation.Transpose().Transform(localOffset);
}

bool ypabact_IsVerticalLaserMultiCandidate(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit,
                                                  bool friendlyTargets, const vec3d &origin,
                                                  float range, float aiTriggerRadius,
                                                  const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, unit, friendlyTargets) )
        return false;
    if ( ypabact_LaserTargetInList(unit, excluded) )
        return false;

    vec3d to = unit->_position - origin;
    if ( to.y < 0.0f || to.y > range )
        return false;

    return to.XZ().length() <= aiTriggerRadius;
}

NC_STACK_ypabact *ypabact_FindNearestVerticalLaserTarget(NC_STACK_ypabact *shooter,
                                                               const vec3d &origin,
                                                               float range, float aiTriggerRadius,
                                                               bool friendlyTargets,
                                                               const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !shooter || !shooter->getBACT_pWorld() || range <= 0.0f || aiTriggerRadius <= 0.0f )
        return nullptr;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    int sectorRadius = (int)(aiTriggerRadius / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = aiTriggerRadius + 1.0f;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if ( !ypabact_IsVerticalLaserMultiCandidate(shooter, bct, friendlyTargets,
                                                            origin, range, aiTriggerRadius, excluded) )
                    continue;

                float dist = (bct->_position.XZ() - origin.XZ()).length();
                if ( dist < bestDist )
                {
                    bestDist = dist;
                    best = bct;
                }
            }
        }
    }

    return best;
}

NC_STACK_ypabact *ypabact_UpdateVerticalLaserBeam(NC_STACK_ypabact *shooter,
                                                        World::TWeapProto &wproto,
                                                        NC_STACK_ypabact::TLaserBeamRuntime &beam,
                                                        const vec3d &start, const vec3d &down,
                                                        float range, bool beamWasActive,
                                                        bool spawnBeamVPs, bool playerControlled,
                                                        float damageMult,
                                                        std::vector<vec3d> &chainFXPoints)
{
    NC_STACK_ypaworld *world = shooter ? shooter->getBACT_pWorld() : nullptr;
    if ( !world )
        return nullptr;

    TLaserUnitHit unitHit;
    bool hasUnitHit = ypabact_LaserHitscan(shooter, wproto, start, down, range, &unitHit);

    vec3d worldHitPoint;
    bool worldHit = ypabact_LaserWorldHit(shooter, start, down, range, &worldHitPoint);
    float worldAlong = range + 1.0f;
    if ( worldHit )
    {
        worldAlong = (worldHitPoint - start).dot(down);
        if ( worldAlong < 0.0f )
            worldHit = false;
    }

    bool useWorldHit = worldHit && (!hasUnitHit || worldAlong <= unitHit.along);
    NC_STACK_ypabact *target = (!useWorldHit && hasUnitHit) ? unitHit.target : nullptr;

    beam.start = start;
    if ( target )
        beam.end = unitHit.hitPoint;
    else if ( useWorldHit )
        beam.end = worldHitPoint;
    else
        beam.end = start + down * range;
    beam.has_contact = target != nullptr || useWorldHit;

    if ( !beamWasActive )
        world->SpawnTransientVisual(wproto.vp_launch, wproto.visual_3ds.launch,
                                    wproto.visual_base.launch,
                                    beam.start, ypabact_LaserRotationFromDir(down, shooter->_rotation), 90,
                                    1.0, World::TVisualTint(), wproto.launch_scale);

    if ( spawnBeamVPs )
        ypabact_SpawnLaserBeamVisuals(shooter, wproto, beam.start, beam.end);

    if ( target )
    {
        if ( !beamWasActive || target->_gid != beam.target_gid )
        {
            beam.energy_ticks = 0;
            beam.next_damage_time = 0;
            beam.next_fx_time = 0;
        }

        beam.target_gid = target->_gid;
        ypabact_ApplyLaserUnitTick(shooter, wproto, target, beam, playerControlled, damageMult);

        if ( shooter->_clock >= beam.next_fx_time )
        {
            ypabact_SpawnLaserImpactChainFX(shooter, wproto, beam, down,
                                            false, chainFXPoints);
            ypabact_SpawnWeaponImpactVisual(world, wproto, false, beam.end,
                                               ypabact_LaserRotationFromDir(down, shooter->_rotation), 90);
            beam.next_fx_time = shooter->_clock + 160;
        }
    }
    else
    {
        TLaserWorldHit sectorHit;
        bool hasSectorHit = false;

        if ( useWorldHit )
        {
            hasSectorHit = ypabact_LaserGetSectorHit(world, worldHitPoint + down * 5.0f, &sectorHit);
            if ( !hasSectorHit )
                hasSectorHit = ypabact_LaserGetSectorHit(world, worldHitPoint, &sectorHit);
        }

        if ( hasSectorHit )
        {
            int32_t sectorTargetId = ypabact_LaserSectorTargetId(sectorHit);
            if ( !beamWasActive || sectorTargetId != beam.target_gid )
            {
                beam.energy_ticks = 0;
                beam.next_damage_time = 0;
                beam.next_fx_time = 0;
            }
            beam.target_gid = sectorTargetId;

            if ( beam.next_damage_time <= 0 || shooter->_clock >= beam.next_damage_time )
            {
                int applyNow = ypabact_LaserTickSectorEnergy(wproto, beam.energy_ticks);

                NC_STACK_ypabact *userHost = world->getYW_userHostStation();
                bool canApplyDamage = !world->_isNetGame ||
                                      (userHost && userHost->_owner == shooter->_owner);

                if ( applyNow > 0 && canApplyDamage )
                {
                    yw_arg129 dmg;
                    dmg.field_0 = 0;
                    dmg.pos = sectorHit.damagePos;
                    dmg.field_10 = applyNow;
                    dmg.unit = shooter;
                    shooter->ChangeSectorEnergy(&dmg);
                }

                beam.energy_ticks++;
                beam.next_damage_time = shooter->_clock + ypabact_LaserDamageInterval(wproto, playerControlled);
            }
        }
        else
        {
            beam.energy_ticks = 0;
            beam.target_gid = 0;
            beam.next_damage_time = 0;
        }

        if ( useWorldHit && shooter->_clock >= beam.next_fx_time )
        {
            ypabact_SpawnLaserImpactChainFX(shooter, wproto, beam, down,
                                            true, chainFXPoints);
            ypabact_SpawnWeaponImpactVisual(world, wproto, true, beam.end,
                                               ypabact_LaserRotationFromDir(down, shooter->_rotation), 90);
            beam.next_fx_time = shooter->_clock + 160;
        }
    }

    return target;
}

void NC_STACK_ypabact::RequestVerticalLaserFire(int weaponId, bact_arg79 *arg)
{
    // OpenNeoUA invisible: firing the vertical laser is a real attack -> reveal now.
    RevealInvisibleOnAttack();

    _vertical_laser_weapon = weaponId;
    _vertical_laser_fire_request = true;

    ypabact_StartVehicleFireVPForWeapon(this, weaponId, _clock);
    _vertical_laser_request_target = (arg && arg->tgType == BACT_TGT_TYPE_UNIT) ? arg->target.pbact : nullptr;
    _vertical_laser_request_start = ypabact_VerticalLaserSourceOrigin(this, arg);
}

void NC_STACK_ypabact::StopVerticalLaser()
{
    if ( !_vertical_laser_soundcarrier.Sounds.empty() &&
         (_vertical_laser_soundcarrier.Sounds[0].IsEnabled() ||
          _vertical_laser_soundcarrier.Sounds[0].IsPFxEnabled() ||
          _vertical_laser_soundcarrier.Sounds[0].IsShkEnabled() ||
          (_vertical_laser_soundcarrier.Sounds.size() > 1 &&
           (_vertical_laser_soundcarrier.Sounds[1].IsPFxEnabled() ||
            _vertical_laser_soundcarrier.Sounds[1].IsShkEnabled()))) )
    {
        SFXEngine::SFXe.StopCarrier(&_vertical_laser_soundcarrier);
    }
    if ( !_vertical_laser_hit_soundcarrier.Sounds.empty() &&
         _vertical_laser_hit_soundcarrier.Sounds[0].IsEnabled() )
    {
        SFXEngine::SFXe.StopCarrier(&_vertical_laser_hit_soundcarrier);
    }

    _vertical_laser_active = false;
    _vertical_laser_fire_request = false;
    _vertical_laser_weapon = -1;
    _vertical_laser_request_target = nullptr;
    _vertical_laser_request_start = vec3d(0.0, 0.0, 0.0);
    _vertical_laser_next_beam_vp_time = 0;
    _vertical_laser_beam = TLaserBeamRuntime();
    _vertical_laser_beams.clear();
}

void NC_STACK_ypabact::UpdateVerticalLaser(update_msg *arg)
{
    if ( IsActiveDebuffStunFireBlocked() )
    {
        StopVerticalLaser();
        return;
    }

    bool requested = _vertical_laser_fire_request;
    _vertical_laser_fire_request = false;

    if ( !requested || !_world || _vertical_laser_weapon < 0 ||
         (size_t)_vertical_laser_weapon >= _world->GetWeaponsProtos().size() )
    {
        StopVerticalLaser();
        return;
    }

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_vertical_laser_weapon);
    if ( !wproto.IsVerticalLaser() )
    {
        StopVerticalLaser();
        return;
    }

    // UA world coordinates use +Y as "down" toward terrain/buildings.
    const vec3d down(0.0, 1.0, 0.0);
    float range = ypabact_LaserRange(wproto);
    float aiTriggerRadius = ypabact_AiVerticalFireTriggerRadius(wproto);
    bool playerControlled = getBACT_inputting() || getBACT_viewer();
    bool wasActive = _vertical_laser_active;
    size_t oldBeamCount = _vertical_laser_beams.size();
    bool spawnBeamVPs = (_vertical_laser_next_beam_vp_time <= 0 ||
                         _clock >= _vertical_laser_next_beam_vp_time);

    _vertical_laser_active = true;

    if ( !wasActive )
        ypabact_PlayLaserLaunchSound(this, wproto, _vertical_laser_request_start, down);

    if ( _vertical_laser_beams.empty() )
        _vertical_laser_beams.resize(1);

    size_t activeBeamCount = 0;
    std::vector<NC_STACK_ypabact *> directHitTargets;
    directHitTargets.reserve((size_t)wproto.laser_beam_count + 1);
    std::vector<vec3d> chainFXPoints;

    // Vertical Laser shares the same aggressive player targeting as the normal
    // Laser. vertical_laser_ai_trigger_radius remains AI-only: player acquisition uses
    // the common laser range and a downward aim cone, not the AI fire cylinder.
    NC_STACK_ypabact *primaryAimTarget = nullptr;
    vec3d primaryDir = down;

    if ( playerControlled )
    {
        if ( _vertical_laser_request_target &&
             ypabact_IsLaserDamageTarget(this, _vertical_laser_request_target) )
        {
            primaryAimTarget = _vertical_laser_request_target;
        }
        else
        {
            if ( wasActive && !_vertical_laser_beams.empty() &&
                 _vertical_laser_beams[0].target_gid > 0 )
            {
                primaryAimTarget = ypabact_RetainPlayerLaserTarget(
                    this, _vertical_laser_request_start, down, range,
                    _vertical_laser_beams[0].target_gid);
            }

            if ( !primaryAimTarget )
                primaryAimTarget = ypabact_LaserAutoTarget(
                    this, _vertical_laser_request_start, down, range,
                    PLAYER_LASER_ACQUIRE_ALIGNMENT);
        }
    }

    if ( primaryAimTarget )
    {
        vec3d toTarget = primaryAimTarget->_position - _vertical_laser_request_start;
        const float targetDistance = toTarget.length();
        if ( targetDistance > 0.001f && targetDistance <= range )
            primaryDir = toTarget / targetDistance;
        else
            primaryAimTarget = nullptr;
    }

    NC_STACK_ypabact *primaryHitTarget =
        ypabact_UpdateVerticalLaserBeam(this, wproto, _vertical_laser_beams[0],
                                        _vertical_laser_request_start, primaryDir, range,
                                        wasActive && oldBeamCount > 0,
                                        spawnBeamVPs, playerControlled, 1.0f, chainFXPoints);
    activeBeamCount = 1;

    NC_STACK_ypabact *primaryChainGroupTarget = nullptr;
    if ( playerControlled )
    {
        primaryChainGroupTarget = primaryAimTarget ? primaryAimTarget : primaryHitTarget;
    }
    else
    {
        // Preserve the proven AI Vertical Laser path exactly: its requested target
        // remains only the grouping reference while the primary beam stays vertical.
        primaryChainGroupTarget =
            (_vertical_laser_request_target && _vertical_laser_request_target->_owner != World::OWNER_0 &&
             ypabact_IsLaserDamageTarget(this, _vertical_laser_request_target))
                ? _vertical_laser_request_target
                : primaryHitTarget;
    }

    if ( primaryHitTarget && primaryHitTarget->_owner != World::OWNER_0 &&
         ypabact_IsLaserDamageTarget(this, primaryHitTarget) )
    {
        directHitTargets.push_back(primaryHitTarget);
    }

    bool friendlyMultiTargets = ypabact_IsLaserFriendlyToShooter(this, primaryChainGroupTarget);
    bool allowFriendlyExtraTargets = !friendlyMultiTargets || playerControlled;

    if ( primaryChainGroupTarget && primaryChainGroupTarget->_owner != World::OWNER_0 &&
         wproto.laser_beam_count > 1 && allowFriendlyExtraTargets )
    {
        std::vector<NC_STACK_ypabact *> selectedTargets = directHitTargets;

        while ( activeBeamCount < (size_t)wproto.laser_beam_count )
        {
            NC_STACK_ypabact *extraTarget = nullptr;

            if ( playerControlled )
            {
                // Player Vertical Laser uses the same common multi-beam acquisition
                // as model=laser. The vertical fire radius is deliberately not used
                // here; it remains only an AI decision threshold.
                extraTarget = ypabact_FindNearestLaserMultiTarget(
                    this, _vertical_laser_request_start, primaryDir, range,
                    friendlyMultiTargets, selectedTargets);
            }
            else
            {
                extraTarget = ypabact_FindNearestVerticalLaserTarget(
                    this, _vertical_laser_request_start, range, aiTriggerRadius,
                    friendlyMultiTargets, selectedTargets);
            }

            if ( !extraTarget )
                break;

            if ( _vertical_laser_beams.size() <= activeBeamCount )
                _vertical_laser_beams.resize(activeBeamCount + 1);

            vec3d extraDir = extraTarget->_position - _vertical_laser_request_start;
            if ( extraDir.normalise() < 0.001f )
                extraDir = down;

            NC_STACK_ypabact *hitTarget =
                ypabact_UpdateVerticalLaserBeam(this, wproto, _vertical_laser_beams[activeBeamCount],
                                                _vertical_laser_request_start, extraDir, range,
                                                wasActive && activeBeamCount < oldBeamCount,
                                                spawnBeamVPs, playerControlled, 1.0f, chainFXPoints);

            selectedTargets.push_back(extraTarget);
            if ( hitTarget && hitTarget->_owner != World::OWNER_0 &&
                 ypabact_IsLaserDamageTarget(this, hitTarget) &&
                 !ypabact_LaserTargetInList(hitTarget, directHitTargets) )
            {
                directHitTargets.push_back(hitTarget);
            }

            activeBeamCount++;
        }
    }


    if ( wproto.laser_chain_allow && wproto.laser_chain_max_jumps > 0 &&
         wproto.laser_chain_radius > 0.0f && primaryHitTarget &&
         primaryHitTarget->_owner != World::OWNER_0 &&
         ypabact_IsLaserDamageTarget(this, primaryHitTarget) )
    {
        NC_STACK_ypabact *chainGroupTarget = primaryChainGroupTarget ? primaryChainGroupTarget : primaryHitTarget;
        bool friendlyChain = ypabact_IsLaserFriendlyToShooter(this, chainGroupTarget);
        bool canRunChain = (!friendlyChain || playerControlled) &&
                           ypabact_IsLaserSecondaryTargetCandidate(this, primaryHitTarget, friendlyChain);

        if ( canRunChain )
        {
            std::vector<NC_STACK_ypabact *> chainHits = directHitTargets;
            if ( !ypabact_LaserTargetInList(primaryHitTarget, chainHits) )
                chainHits.push_back(primaryHitTarget);

            NC_STACK_ypabact *from = primaryHitTarget;
            float chainDamageMult = 1.0f;
            float perJumpMult = wproto.laser_chain_energy_mult > 0.0f ? wproto.laser_chain_energy_mult : 1.0f;

            for (int jump = 0; jump < wproto.laser_chain_max_jumps; jump++)
            {
                NC_STACK_ypabact *chainTarget =
                    ypabact_FindLaserChainTarget(this, from, wproto.laser_chain_radius,
                                                 friendlyChain, chainHits);
                if ( !chainTarget )
                    break;

                if ( _vertical_laser_beams.size() <= activeBeamCount )
                    _vertical_laser_beams.resize(activeBeamCount + 1);

                TLaserBeamRuntime &chainBeam = _vertical_laser_beams[activeBeamCount];
                bool beamWasActive = wasActive && activeBeamCount < oldBeamCount;

                chainBeam.start = from->_position;
                chainBeam.end = chainTarget->_position;
                chainBeam.has_contact = true;

                vec3d chainDir = chainBeam.end - chainBeam.start;
                if ( chainDir.normalise() < 0.001f )
                    chainDir = _rotation.AxisZ();

                if ( !beamWasActive || chainTarget->_gid != chainBeam.target_gid )
                {
                    chainBeam.energy_ticks = 0;
                    chainBeam.next_damage_time = 0;
                    chainBeam.next_fx_time = 0;
                }
                chainBeam.target_gid = chainTarget->_gid;

                chainDamageMult *= perJumpMult;

                if ( spawnBeamVPs )
                    ypabact_SpawnLaserBeamVisuals(this, wproto, chainBeam.start, chainBeam.end);

                ypabact_ApplyLaserUnitTick(this, wproto, chainTarget, chainBeam,
                                           playerControlled, chainDamageMult);

                if ( _clock >= chainBeam.next_fx_time )
                {
                    ypabact_SpawnLaserImpactChainFX(this, wproto, chainBeam,
                                                    chainDir, false, chainFXPoints);
                    ypabact_SpawnWeaponImpactVisual(_world, wproto, false, chainBeam.end,
                                                       ypabact_LaserRotationFromDir(chainDir, _rotation), 90);
                    chainBeam.next_fx_time = _clock + 160;
                }

                chainHits.push_back(chainTarget);
                from = chainTarget;
                activeBeamCount++;
            }
        }
    }

    if ( _vertical_laser_beams.size() > activeBeamCount )
        _vertical_laser_beams.resize(activeBeamCount);

    if ( !_vertical_laser_beams.empty() )
        _vertical_laser_beam = _vertical_laser_beams[0];

    if ( spawnBeamVPs )
        _vertical_laser_next_beam_vp_time = _clock + 1;

    ypabact_UpdateLaserHitSound(this, wproto, &_vertical_laser_hit_soundcarrier,
                                _vertical_laser_beams);

    ypabact_UpdateLaserNormalSound(this, wproto, &_vertical_laser_soundcarrier,
                                   _vertical_laser_beams.empty()
                                       ? _vertical_laser_request_start
                                       : _vertical_laser_beams[0].start,
                                   down);
}

bool ypabact_ShouldPersistAILaserFire(NC_STACK_ypabact *bact, int weaponId,
                                                const bact_arg101 *arg,
                                                NC_STACK_ypabact *unitTarget)
{
    if ( !bact || !arg || !bact->getBACT_pWorld() ||
         (bact->_oflags & BACT_OFLAG_USERINPT) ||
         !ypabact_IsValidWeaponId(bact, weaponId) )
        return false;

    if ( !bact->IsPrimaryWeaponElevationAllowed(arg->pos - bact->_position) )
        return false;

    const World::TWeapProto &wproto =
        bact->getBACT_pWorld()->GetWeaponsProtos().at(weaponId);

    const bool activeNormal =
        wproto.IsLaser() && !wproto.IsVerticalLaser() &&
        bact->_laser_active && bact->_laser_weapon == weaponId;
    const bool activeVertical =
        wproto.IsVerticalLaser() && bact->_vertical_laser_active &&
        bact->_vertical_laser_weapon == weaponId;

    if ( !activeNormal && !activeVertical )
        return false;

    // A dead/invalid unit is never retained merely because the trigger was held.
    // TargetAssess remains authoritative for ownership/radar/order decisions; this
    // check only prevents one-frame fire-control jitter from breaking a valid beam.
    if ( unitTarget && !ypabact_IsLaserAimTarget(bact, unitTarget) )
        return false;

    if ( activeVertical )
    {
        const vec3d fireOrigin =
            bact->_position + bact->_rotation.Transpose().Transform(bact->_fire_pos);

        // Preserve the existing vertical-laser semantics: the target must remain
        // below the emitter and inside the horizontal fire cylinder.  Also stop
        // when it has moved beyond the beam's real life_time-derived reach.
        if ( arg->pos.y < fireOrigin.y )
            return false;

        if ( (arg->pos.XZ() - fireOrigin.XZ()).length() >
             ypabact_AiVerticalFireTriggerRadius(wproto) )
            return false;

        return (arg->pos - fireOrigin).length() <= ypabact_LaserRange(wproto);
    }

    const vec3d fireOrigin = ypabact_LaserSourceOrigin(bact);
    vec3d toAim = arg->pos - fireOrigin;
    const float distance = toAim.normalise();
    if ( distance <= 0.001f || distance > ypabact_LaserRange(wproto) )
        return false;

    vec3d forward = bact->_rotation.AxisZ();
    if ( forward.normalise() <= 0.001f )
        return false;

    // Hysteresis: CheckFireAI() remains the strict acquisition gate.  Once a
    // laser is already connected, allow the AI to track within a broader forward
    // cone instead of releasing the trigger on tiny steering/target movements.
    // The beam still cannot persist behind the vehicle or beyond its real range.
    constexpr float HoldAlignment = 0.50f;
    return toAim.dot(forward) >= HoldAlignment;
}
