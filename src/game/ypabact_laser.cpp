// Laser weapons: targeting, damage ticks, sound and beam visuals.
#include "game/ypabact_internal.h"
#include <limits>

bool ypabact_IsLaserDamageTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit)
{
    if ( !ypabact_IsCarrierSpawnAliveUnit(unit) || unit->CollisionsDisabled() )
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

    if ( bact->_laser_active &&
         ypabact_GetWeaponIdForSourceSlot(bact, bact->_laser_source_slot) == bact->_laser_weapon )
    {
        if ( outSourceSlot ) *outSourceSlot = bact->_laser_source_slot;
        return bact->_laser_weapon;
    }
    const int candidates[] = {bact->_laser_active ? bact->_laser_weapon : -1};

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

    return false;
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
    // Saturate amplified chain damage before converting to the integer damage API.
    int tickDamage = (int)std::min(100000000.0, ceil((double)damage * (100.0f - shield) / divisor));
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

        // OpenNeoUA Orbiter: a player laser hit sends the orbiters (the AI
        // already sent them when it opened fire through LaunchMissile).
        if ( playerControlled )
            shooter->EngageOrbiters(target);

    }
    if ( !aiFriendlyContact && ypabact_CanApplyLaserDamage(shooter) &&
         wproto.debuff.valid && target->_energy > 0 && target->_status != BACT_STATUS_DEAD )
        target->ApplyDebuff(wproto.debuff, shooter);

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

// Keep the collision samples and the repeated visuals on the same fixed grid.
// The grid uses the full weapon range so a contact does not move the samples.
struct TLaserSphereGrid
{
    float lead = 0.0f;
    float step = 0.0f;
    int count = 0;
};

static TLaserSphereGrid ypabact_LaserSphereGrid(NC_STACK_ypabact *shooter,
                                                const World::TWeapProto &weapon,
                                                float range)
{
    TLaserSphereGrid grid;
    const float spacing = ypabact_LaserClampVisualSpacing(
        std::isfinite(weapon.laser_spacing) ? weapon.laser_spacing : 0.0f);
    if ( shooter->getBACT_viewer() || shooter->getBACT_inputting() )
        grid.lead = std::min(range * 0.25f, std::max(16.0f, spacing * 0.5f));
    const float length = std::max(0.0f, range - grid.lead);
    grid.count = std::max(2, std::min(320, int(length / spacing) + 2));
    grid.step = length / grid.count;
    return grid;
}

static bool ypabact_LaserCompoundHitscan(NC_STACK_ypabact *shooter,
                                         const World::TWeapProto &weapon,
                                         const vec3d &origin, const vec3d &dir,
                                         float range, TLaserUnitHit *out)
{
    auto *world = shooter->getBACT_pWorld();
    const TLaserSphereGrid grid = ypabact_LaserSphereGrid(shooter, weapon, range);
    if ( grid.step <= 0.0f || !std::isfinite(grid.step) ) return false;
    const mat3x3 rotation = ypabact_LaserRotationFromDir(dir, shooter->_rotation).Transpose();
    float extent = 0.0f;
    for (const auto &sphere : weapon.coll.roboColls)
        if ( sphere.robo_coll_radius > 0.01f && std::isfinite(sphere.robo_coll_radius) )
            extent = std::max(extent, float(sphere.coll_pos.length() + sphere.robo_coll_radius));
    if ( weapon.radius_defined && weapon.radius > 0.01f && std::isfinite(weapon.radius) )
        extent = std::max(extent, weapon.radius);

    NC_STACK_ypabact *best = nullptr;
    float bestAlong = range + 1.0f;
    vec3d bestPoint;
    const auto testTarget = [&](NC_STACK_ypabact *target)
    {
        if ( !ypabact_IsLaserDamageTarget(shooter, target) ) return;
        std::vector<NC_STACK_ypabact::TCollisionSphereWorld> targetSpheres;
        const bool shape = target->HasCollisionShape();
        if ( shape && !world->_collisionScene ) return;
        if ( !shape ) target->GetCollisionSpheres(targetSpheres, target->_position, target->_rotation, false);
        const auto testSphere = [&](const vec3d &offset, float radius)
        {
            if ( radius <= 0.01f || !std::isfinite(radius) ||
                 !std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) ) return;
            const vec3d base = origin + rotation.Transform(offset);
            const vec3d toTarget = target->_position - base;
            const float projection = toTarget.dot(dir);
            const float broad = target->GetCollisionBroadRadius() + radius;
            if ( !std::isfinite(projection) || !std::isfinite(broad) ) return;
            if ( (toTarget - dir * projection).length() > broad ) return;
            const float firstIndex = (projection - broad - grid.lead) / grid.step - 0.5f;
            const float lastIndex = (projection + broad - grid.lead) / grid.step - 0.5f;
            if ( firstIndex > grid.count - 1 || lastIndex < 0.0f ) return;
            int first = int(std::ceil(std::max(0.0f, firstIndex)));
            int last = int(std::floor(std::min(float(grid.count - 1), lastIndex)));
            for (int i = first; i <= last; ++i)
            {
                const float along = grid.lead + (i + 0.5f) * grid.step;
                if ( along >= bestAlong ) break;
                const vec3d center = base + dir * along;
                if ( shape )
                {
                    Collision::Contact contact;
                    if ( world->_collisionScene->Trace(target, center, center, radius, &contact) )
                    { best = target; bestAlong = along; bestPoint = contact.point; }
                }
                else for (const auto &body : targetSpheres)
                {
                    const vec3d delta = center - body.center;
                    const float distance = delta.length();
                    if ( distance <= body.radius + radius )
                    {
                        best = target;
                        bestAlong = along;
                        bestPoint = distance > 0.001f ? body.center + delta * (body.radius / distance) : center;
                        break;
                    }
                }
            }
        };
        if ( weapon.radius_defined ) testSphere(vec3d(0, 0, 0), weapon.radius);
        for (const auto &sphere : weapon.coll.roboColls)
            testSphere(sphere.coll_pos, sphere.robo_coll_radius);
    };

    const float mapReach = float(std::max(world->_mapSize.x, world->_mapSize.y));
    const float maximumExtent = (mapReach * 2.0f + 2.0f) * World::CVSectorLength + range;
    extent = std::min(extent, maximumExtent);
    if ( world->_collisionScene )
        for (auto *target : world->_collisionScene->ShapeTargets(origin, origin + dir * range, extent))
            testTarget(target);
    const Common::Point center = World::PositionToSectorID(origin);
    const int sectorRadius = int(std::min(mapReach, (range + extent) / World::CVSectorLength)) + 2;
    for (int y = std::max(0, center.y - sectorRadius); y <= std::min(world->_mapSize.y - 1, center.y + sectorRadius); ++y)
        for (int x = std::max(0, center.x - sectorRadius); x <= std::min(world->_mapSize.x - 1, center.x + sectorRadius); ++x)
            for (auto *target : world->SectorAt(Common::Point(x, y)).unitsList)
                if ( !target->HasCollisionShape() ) testTarget(target);

    if ( best && out ) { out->target = best; out->along = bestAlong; out->hitPoint = bestPoint; }
    return best != nullptr;
}

static bool ypabact_LaserCompoundWorldHit(NC_STACK_ypabact *shooter,
                                         const World::TWeapProto &weapon,
                                         const vec3d &origin, const vec3d &dir,
                                         float range, float limit, vec3d *point,
                                         float *hitAlong)
{
    auto *world = shooter->getBACT_pWorld();
    const TLaserSphereGrid grid = ypabact_LaserSphereGrid(shooter, weapon, range);
    const mat3x3 rotation = ypabact_LaserRotationFromDir(dir, shooter->_rotation).Transpose();
    // These four directions cover every nonzero face normal for func137's
    // one-sided normal test, without querying all six cardinal directions.
    const vec3d directions[] = {vec3d(1,1,1), vec3d(1,-1,-1),
                                vec3d(-1,1,-1), vec3d(-1,-1,1)};
    for (int i = 0; i < grid.count; ++i)
    {
        const float along = grid.lead + (i + 0.5f) * grid.step;
        if ( along > limit ) break;
        const auto testSphere = [&](const vec3d &offset, float radius)
        {
            if ( radius <= 0.01f || !std::isfinite(radius) ||
                 !std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z) ) return false;
            yw_137col contact;
            ypaworld_arg137 query{};
            query.pos = origin + dir * along + rotation.Transform(offset);
            query.radius = radius;
            // The legacy world query converts coordinates and radius to ints.
            const double safeCoordinate = std::numeric_limits<int>::max() / 4.0;
            if ( !std::isfinite(query.pos.x) || !std::isfinite(query.pos.y) ||
                 !std::isfinite(query.pos.z) || std::fabs(query.pos.x) + radius > safeCoordinate ||
                 std::fabs(query.pos.z) + radius > safeCoordinate ) return false;
            query.collisions = &contact;
            query.coll_max = 1;
            for (const vec3d &direction : directions)
            {
                query.pos2 = direction;
                world->ypaworld_func137(&query);
                if ( query.coll_count > 0 )
                {
                    if ( point ) *point = contact.pos1;
                    if ( hitAlong ) *hitAlong = along;
                    return true;
                }
            }
            return false;
        };
        if ( weapon.radius_defined && testSphere(vec3d(0, 0, 0), weapon.radius) ) return true;
        for (const auto &sphere : weapon.coll.roboColls)
            if ( testSphere(sphere.coll_pos, sphere.robo_coll_radius) ) return true;
    }
    return false;
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
    if ( !world || wproto.disable_collision )
        return false;
    if ( !wproto.coll.roboColls.empty() )
        return ypabact_LaserCompoundHitscan(shooter, wproto, origin, dir, range, outHit);

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
                if (bct->CollisionsDisabled() || bct->HasCollisionShape() || !ypabact_IsLaserDamageTarget(shooter, bct))
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
                                                 const vec3d &dir, float range, float minAlignment,
                                                 bool useNativeLimits)
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

                if ( useNativeLimits && !shooter->IsPrimaryWeaponElevationAllowed(bct->_position - shooter->_position, true) )
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
                                                            const std::vector<NC_STACK_ypabact *> &excluded,
                                                            bool useNativeLimits)
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
                if ( useNativeLimits && !shooter->IsPrimaryWeaponElevationAllowed(bct->_position - shooter->_position, true) )
                    continue;
                if ( ypabact_LaserTargetInList(bct, excluded) )
                    continue;
                if ( !(shooter->_oflags & BACT_OFLAG_USERINPT) && shooter->_laser_weapon >= 0 )
                {
                    const auto &proto = world->GetWeaponsProtos().at(shooter->_laser_weapon);
                    TFireTriggerArea area;
                    if ( !shooter->GetAIFireTriggerArea(proto, &area) ||
                         !ypabact_FireTriggerAreaContains(area, bct->_position, bct->GetCollisionBroadRadius()) )
                        continue;
                }

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
    if ( !shooter || !requests || requests->empty() )
        return;
    const int limit = std::max(1, ypabact_GetMultiTargetLimit(shooter, wproto, (int)requests->size()));
    NC_STACK_ypabact *primary = requests->front().target;
    if ( !primary || (friendlyTargets && !playerControlled) )
        return;
    std::vector<NC_STACK_ypabact *> targets{primary};
    while ( (int)targets.size() < limit )
    {
        NC_STACK_ypabact *target = nullptr;
        if ( wproto.IsDownwardLaser() && !playerControlled )
        {
            TFireTriggerArea area;
            if ( shooter->GetAIFireTriggerArea(wproto, &area) )
                target = ypabact_FindNearestDownwardLaserTarget(shooter,
                    requests->front().start, range, area, friendlyTargets, targets);
        }
        else
            target = ypabact_FindNearestLaserMultiTarget(
                shooter, requests->front().start, requests->front().dir, range,
                friendlyTargets, targets, !playerControlled);
        if ( !target )
            break;
        targets.push_back(target);
    }
    for (size_t i = 0; i < requests->size(); ++i)
    {
        auto &request = requests->at(i);
        request.target = targets[i % targets.size()];
        request.dir = request.target->_position - request.start;
        if ( request.dir.normalise() < 0.001f )
            request.dir = requests->front().dir;
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

    if ( !wproto.IsLaser() || shooter->_num_targets <= 1 )
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
    requests.resize(shooter->GetCurrentWeaponProjectileCount());
    requests[0].target = arg->target.pbact;
    requests[0].start = ypabact_LaserSourceOrigin(shooter);
    requests[0].dir = arg->target.pbact->_position - requests[0].start;

    if ( requests[0].dir.normalise() < 0.001f )
        requests[0].dir = shooter->_rotation.AxisZ();

    for (size_t i = 1; i < requests.size(); ++i)
        requests[i] = requests[0];
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

    // Reference origin for target-lock preview. Runtime beams use their own muzzle positions.
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

// True for the ground decal blocks a burning laser paints itself as a strip.
static bool ypabact_LaserBurnOwnsDecal(const World::TWeapProto &wproto, const World::TChainFXConfig &fx)
{
    return wproto.laser_burn_decal_frequency > 0.0f &&
           fx.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD &&
           fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL;
}

void ypabact_LaserBurnTrail(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto,
                            NC_STACK_ypabact::TLaserBeamRuntime &beam, bool worldContact)
{
    // Not touching the world (or burn off): the strip ends here, the next contact starts a new one.
    if ( !shooter || !worldContact || wproto.laser_burn_decal_frequency <= 0.0f )
    {
        beam.burn_active = false;
        return;
    }
    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    if ( !world )
        return;

    std::vector<World::TChainFXConfig> decals;
    for (const World::TChainFXConfig &fx : wproto.chain_fx)
    {
        if ( !ypabact_LaserBurnOwnsDecal(wproto, fx) )
            continue;
        decals.push_back(fx);
        // A strip has one constant width: no random size between stamps.
        decals.back().ground_decal_size_max = decals.back().ground_decal_size_min;
        decals.back().ground_decal_stretch_z_max = decals.back().ground_decal_stretch_z_min;
    }
    const World::TCraterConfig &craterBase = wproto.crater;
    const bool crater = craterBase.defined && !craterBase.invalid;
    if ( decals.empty() && !crater )
    {
        beam.burn_active = false;
        return;
    }

    // Frequency is decals per 100 world units: the distance between two stamps.
    const float step = 100.0f / wproto.laser_burn_decal_frequency;
    // Overlapping craters add up (about 1.07 * radius / step at the centre): scale the
    // depth of each stamp so the trench ends up as deep as crater_depth says.
    World::TCraterConfig stampCrater = craterBase;
    if ( crater )
    {
        const float radius = 0.5f * (craterBase.radius_min + craterBase.radius_max);
        const float scale = radius > 0.0f
                          ? std::max(0.05f, std::min(step / (1.07f * radius), 1.0f)) : 1.0f;
        stampCrater.depth_min *= scale;
        stampCrater.depth_max *= scale;
    }

    const auto stamp = [&](const vec3d &point, const World::TDecalStroke *stroke)
    {
        ypaworld_arg136 hit;
        if ( !world->FindWorldSurfaceInSphere(point, step + 30.0f, hit) )
            return;
        // Decal geometry preparation can reuse temporary collision data: look the
        // surface up again before the crater.
        for (const World::TChainFXConfig &fx : decals)
            world->SpawnGroundDecal(fx, hit, stroke);
        if ( crater && !world->FindWorldSurfaceInSphere(point, step + 30.0f, hit) )
            return;
        if ( crater )
            world->SpawnTerrainCrater(stampCrater, hit);
    };

    if ( !beam.burn_active )
    {
        // First contact: a round stamp (no travel direction yet).
        stamp(beam.end, nullptr);
        beam.burn_active = true;
        beam.burn_last = beam.end;
        return;
    }

    vec3d travel = beam.end - beam.burn_last;
    float remaining = travel.normalise();
    if ( !std::isfinite(remaining) || remaining < step )
        return;

    // A fast sweep fills the gap with evenly spaced stamps (at most 12 per frame).
    vec3d from = beam.burn_last;
    World::TDecalStroke stroke;
    stroke.dir = travel;
    stroke.length = step * 1.6f;
    for (int i = 0; i < 12 && remaining >= step; ++i)
    {
        from += travel * step;
        remaining -= step;
        stamp(from, &stroke);
    }
    beam.burn_last = remaining >= step ? beam.end : from;
}

void ypabact_SpawnLaserImpactChainFX(
    NC_STACK_ypabact *shooter, int weaponId, World::TWeapProto &wproto,
    const NC_STACK_ypabact::TLaserBeamRuntime &beam, const vec3d &dir,
    bool worldContact, std::vector<vec3d> &spawnedPoints)
{
    if ( !shooter || wproto.chain_fx.empty() )
        return;

    // With laser_burn_decal_frequency the ground decals are painted by ypabact_LaserBurnTrail.
    std::vector<World::TChainFXConfig> otherFX;
    const std::vector<World::TChainFXConfig> *fxList = &wproto.chain_fx;
    if ( wproto.laser_burn_decal_frequency > 0.0f )
    {
        for (const World::TChainFXConfig &fx : wproto.chain_fx)
            if ( !ypabact_LaserBurnOwnsDecal(wproto, fx) )
                otherFX.push_back(fx);
        fxList = &otherFX;
    }

    bool relevant = false;
    for (const World::TChainFXConfig &fx : *fxList)
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
        for (const World::TChainFXConfig &fx : *fxList)
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
                                       hit, fxList, &beam.end, &rotation);
    }

    // Laser contact is the projectile-equivalent detonation event. The same
    // authored trigger works for unit and world hits without a new parser name.
    shooter->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DETONATE,
                                   nullptr, fxList, &beam.end, &rotation);
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

static void ypabact_SpawnLaserBeamVisualsImpl(NC_STACK_ypabact *bact, const World::TWeapProto &wproto,
                                      const vec3d &beamStart, const vec3d &beamEnd,
                                      bool hasContact, bool useCompoundGrid)
{
    if ( !bact )
        return;

    if ( (wproto.vp_normal <= 0 && wproto.visual_3ds.normal.empty() &&
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
    const bool compound = useCompoundGrid && !wproto.coll.roboColls.empty();
    TLaserSphereGrid fixedGrid;
    if ( compound )
    {
        fixedGrid = ypabact_LaserSphereGrid(bact, wproto, ypabact_LaserRange(wproto));
        visualStart += dir * fixedGrid.lead;
        span = beamEnd - visualStart;
        len -= fixedGrid.lead;
        if ( len <= 0.0f ) return;
    }
    else if ( bact->getBACT_viewer() || bact->getBACT_inputting() )
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

    int count = compound ? fixedGrid.count : (int)(len / spacing) + 2;
    if ( count < 2 ) count = 2;
    if ( count > 320 ) count = 320;

    for (int i = 0; i < count; i++)
    {
        float t;
        vec3d pos;
        if ( compound )
        {
            const float distance = (i + 0.5f) * fixedGrid.step;
            if ( distance > len + 0.001f ) break;
            t = distance / len;
            pos = visualStart + dir * distance;
        }
        else
        {
            t = ((float)i + 0.5f) / (float)count;
            pos = visualStart + span * t;
        }
        float alpha = 1.0f;
        if ( wproto.laser_fade_in > 0.0f )
            alpha = std::min(alpha, (t * len) / wproto.laser_fade_in);
        if ( hasContact && wproto.laser_fade_out > 0.0f )
            alpha = std::min(alpha, ((1.0f - t) * len) / wproto.laser_fade_out);
        alpha = std::max(0.0f, std::min(alpha, 1.0f));
        World::TVisualTint tint = wproto.visual_tint;
        tint.a *= alpha;
        if ( tint.a <= 0.0f )
            continue;
        // OpenNeoUA custom: the laser beam body uses vp_normal, so honour the weapon's
        // main VP controls here. Impact/launch FX below deliberately stay neutral.
        const int32_t id = world->SpawnTransientVisual(wproto.vp_normal, wproto.visual_3ds.normal,
                                    wproto.visual_base.normal, pos, rot, 45, 1.0,
                                    tint, axisScale, wproto.visual_spin);
        // Additive beam surfaces ignore alpha: also dim their brightness with the same fade.
        if ( id != 0 && !world->_transientVPs.empty() && world->_transientVPs.back().id == id )
            world->_transientVPs.back().intensity = alpha;
    }
}

void ypabact_SpawnLaserBeamVisuals(NC_STACK_ypabact *bact, const World::TWeapProto &wproto,
                                   const vec3d &beamStart, const vec3d &beamEnd, bool hasContact)
{
    ypabact_SpawnLaserBeamVisualsImpl(bact, wproto, beamStart, beamEnd, hasContact, true);
}

void NC_STACK_ypabact::RequestLaserFire(int weaponId, bact_arg79 *arg, int sourceSlot)
{
    if ( !arg || !_world )
        return;
    RevealInvisibleOnAttack();
    const auto &proto = _world->GetWeaponsProtos().at(weaponId);
    if ( _laser_weapon != weaponId || _laser_source_slot != sourceSlot )
        StopLaser();
    if ( _laser_muzzle_x.empty() )
    {
        const int count = ypabact_RollWeaponProjectileCountForSourceSlot(this, sourceSlot);
        for (int i = 0; i < count; ++i)
        {
            const float span = fabs(arg->start_point.x);
            const float vanillaX = count == 1 ? arg->start_point.x : (i * 2.0f * span / (count - 1) - span);
            _laser_muzzle_x.push_back(ypabact_GetProjectileFireX(this, vanillaX, i, count));
            ypabact_ConsumeProjectileFireX(this);
        }
        if ( _fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE )
            _fire_x_slot_index = (_fire_x_slot_index + 1) & 1;
    }
    _laser_weapon = weaponId;
    _laser_source_slot = sourceSlot;
    _laser_fire_request = true;
    _laser_requests.clear();
    ypabact_StartVehicleFireVPIfEnabled(this, proto, _clock);
    for (float x : _laser_muzzle_x)
    {
        TLaserBeamRequest request;
        request.target = arg->tgType == BACT_TGT_TYPE_UNIT ? arg->target.pbact : nullptr;
        request.start = GetBodyPosition() + _rotation.Transpose().Transform(vec3d(x, arg->start_point.y, arg->start_point.z));
        request.dir = proto.IsDownwardLaser() ? vec3d(0, 1, 0) : arg->direction;
        if ( request.target )
            request.dir = request.target->_position - request.start;
        else if ( arg->tgType == BACT_TGT_TYPE_CELL )
            request.dir = arg->tgt_pos - request.start;
        if ( request.dir.normalise() < 0.001f )
            request.dir = proto.IsDownwardLaser() ? vec3d(0, 1, 0) : _rotation.AxisZ();
        _laser_requests.push_back(request);
    }
    _laser_target = _laser_requests.front().target;
    _laser_request_start = _laser_requests.front().start;
    _laser_request_dir = _laser_requests.front().dir;
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
    _laser_muzzle_x.clear();
    _laser_source_slot = 0;
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
    const bool useNativeLimits = !playerControlled && wproto.HasAIFireTriggerArea() && !wproto.IsBombLike();

    auto &primary = requests.front();
    if ( !primary.target || !ypabact_IsLaserDamageTarget(this, primary.target) )
    {
        primary.target = nullptr;
        if ( _laser_active && !_laser_beams.empty() && playerControlled )
            primary.target = ypabact_RetainPlayerLaserTarget(this, primary.start, primary.dir, range, _laser_beams[0].target_gid);
        if ( !primary.target )
            primary.target = ypabact_LaserAutoTarget(this, primary.start, primary.dir, range,
                playerControlled ? PLAYER_LASER_ACQUIRE_ALIGNMENT : 0.4f, useNativeLimits);
    }
    if ( !playerControlled && primary.target )
    {
        TFireTriggerArea area;
        if ( !GetAIFireTriggerArea(wproto, &area) ||
             !ypabact_FireTriggerAreaContains(area, primary.target->_position, primary.target->GetCollisionBroadRadius()) )
            primary.target = nullptr;
    }
    for (auto &request : requests)
        request.target = primary.target;
    ypabact_AddLaserMultiTargetRequests(this, wproto, &requests, range,
        ypabact_IsLaserFriendlyToShooter(this, primary.target), playerControlled);
    ypabact_StoreHUDLaserMultiLockTargets(this, requests);

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

        if ( request.target && ypabact_IsLaserDamageTarget(this, request.target) &&
             (!useNativeLimits || IsPrimaryWeaponElevationAllowed(request.target->_position - _position, true)) )
        {
            // Preserve explicit-target compatibility, including deliberate friendly
            // fire paths. Only automatic acquisition below is enemy-only.
            aimTarget = request.target;
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
        float worldAlong = range + 1.0f;
        const bool compound = !wproto.coll.roboColls.empty();
        bool worldHit = false;
        if ( !wproto.disable_collision && compound )
            worldHit = ypabact_LaserCompoundWorldHit(this, wproto, request.start, dir, range,
                                                      hasUnitHit ? unitHit.along : range,
                                                      &worldHitPoint, &worldAlong);
        else if ( !wproto.disable_collision )
        {
            worldHit = ypabact_LaserWorldHit(this, request.start, dir, range, &worldHitPoint);
            if ( worldHit )
            {
                worldAlong = (worldHitPoint - request.start).dot(dir);
                if ( worldAlong < 0.0f ) worldHit = false;
            }
        }

        bool useWorldHit = worldHit && (!hasUnitHit || worldAlong <= unitHit.along);
        NC_STACK_ypabact *target = (!useWorldHit && hasUnitHit) ? unitHit.target : nullptr;
        vec3d hitPoint = target ? unitHit.hitPoint : vec3d(0.0, 0.0, 0.0);

        TLaserWorldHit sectorHit;
        bool hasSectorHit = false;

        beam.start = request.start;
        if ( target )
            beam.end = compound ? request.start + dir * unitHit.along : hitPoint;
        else if ( useWorldHit )
            beam.end = compound ? request.start + dir * worldAlong : worldHitPoint;
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
        {
            const mat3x3 launchRot = ypabact_LaserRotationFromDir(dir, _rotation);
            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_LAUNCH, nullptr,
                                  &wproto.chain_fx, &beam.start, &launchRot);
        }

        if ( spawnBeamVPs )
            ypabact_SpawnLaserBeamVisuals(this, wproto, beam.start, beam.end, beam.has_contact);

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
            beam.burn_active = false;   // a unit hit ends the burn strip

            if ( target->_owner != World::OWNER_0 && ypabact_IsLaserDamageTarget(this, target) &&
                 !ypabact_LaserTargetInList(target, directHitTargets) )
                directHitTargets.push_back(target);

            ypabact_ApplyLaserUnitTick(this, wproto, target, beam, playerControlled, 1.0f);

            // ---- Throttled impact/contact FX ----
            if ( _clock >= beam.next_fx_time )
            {
                TLaserBeamRuntime impactBeam = beam;
                if ( !wproto.coll.roboColls.empty() ) impactBeam.end = hitPoint;
                ypabact_SpawnLaserImpactChainFX(this, _laser_weapon, wproto, impactBeam, dir,
                                                false, chainFXPoints);
                ypabact_SpawnWeaponImpactVisual(_world, wproto, false, impactBeam.end,
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
                TLaserBeamRuntime impactBeam = beam;
                if ( compound ) impactBeam.end = worldHitPoint;
                ypabact_SpawnLaserImpactChainFX(this, _laser_weapon, wproto, impactBeam, dir,
                                                true, chainFXPoints);
                ypabact_SpawnWeaponImpactVisual(_world, wproto, true, impactBeam.end,
                                                   ypabact_LaserRotationFromDir(dir, _rotation), 90);
                beam.next_fx_time = _clock + 160;
            }

            // Continuous burn strip (laser_burn_decal_frequency): every frame, not on the FX throttle.
            if ( compound && useWorldHit )
            {
                TLaserBeamRuntime impactBeam = beam;
                impactBeam.end = worldHitPoint;
                ypabact_LaserBurnTrail(this, wproto, impactBeam, true);
                beam.burn_active = impactBeam.burn_active;
                beam.burn_last = impactBeam.burn_last;
            }
            else ypabact_LaserBurnTrail(this, wproto, beam, useWorldHit);
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
                    ypabact_SpawnLaserBeamVisualsImpl(this, wproto, beam.start, beam.end, true, false);

                ypabact_ApplyLaserUnitTick(this, wproto, target, beam, playerControlled, chainDamageMult);

                if ( _clock >= beam.next_fx_time )
                {
                    ypabact_SpawnLaserImpactChainFX(this, _laser_weapon, wproto, beam, dir,
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

bool ypabact_IsDownwardLaserMultiCandidate(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit,
                                                  bool friendlyTargets, const vec3d &origin,
                                                  float range, const TFireTriggerArea &box,
                                                  const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, unit, friendlyTargets) )
        return false;
    if ( ypabact_LaserTargetInList(unit, excluded) )
        return false;

    if ( (unit->_position - origin).length() > range )
        return false;

    return ypabact_FireTriggerAreaContains(box, unit->_position,
                                           unit->GetCollisionBroadRadius());
}

NC_STACK_ypabact *ypabact_FindNearestDownwardLaserTarget(NC_STACK_ypabact *shooter,
                                                               const vec3d &origin,
                                                               float range, const TFireTriggerArea &box,
                                                               bool friendlyTargets,
                                                               const std::vector<NC_STACK_ypabact *> &excluded)
{
    if ( !shooter || !shooter->getBACT_pWorld() || range <= 0.0f )
        return nullptr;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    const float searchRadius = ypabact_FireTriggerAreaSearchRadius(box);
    const Common::Point mapSize = world->GetMapSize();
    const int sectorRadius = (int)std::min((double)searchRadius / World::CVSectorLength + 2,
                                         (double)std::max(mapSize.x, mapSize.y));
    Common::Point center = World::PositionToSectorID(origin);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = std::numeric_limits<float>::max();

    for (int y = std::max(0, center.y-sectorRadius); y <= std::min(mapSize.y-1, center.y+sectorRadius); y++)
    {
        for (int x = std::max(0, center.x-sectorRadius); x <= std::min(mapSize.x-1, center.x+sectorRadius); x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *bct : cell.unitsList)
            {
                if ( !ypabact_IsDownwardLaserMultiCandidate(shooter, bct, friendlyTargets,
                                                            origin, range, box, excluded) )
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

bool ypabact_ShouldPersistAILaserFire(NC_STACK_ypabact *bact, int weaponId,
                                                const bact_arg101 *arg,
                                                NC_STACK_ypabact *unitTarget)
{
    if ( !bact || !arg || !bact->getBACT_pWorld() ||
         (bact->_oflags & BACT_OFLAG_USERINPT) ||
         !ypabact_IsValidWeaponId(bact, weaponId) )
        return false;

    const World::TWeapProto &wproto =
        bact->getBACT_pWorld()->GetWeaponsProtos().at(weaponId);

    if ( !bact->IsPrimaryWeaponElevationAllowed(arg->pos - bact->_position,
                                               wproto.HasAIFireTriggerArea() && !wproto.IsBombLike()) )
        return false;

    const bool activeVertical = wproto.IsDownwardLaser();
    if ( !wproto.IsLaser() || !bact->_laser_active || bact->_laser_weapon != weaponId )
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

        // The target stays inside the configured fire volume and beam reach.
        TFireTriggerArea triggerArea;
        if ( !bact->GetAIFireTriggerArea(wproto, &triggerArea, &arg->pos, ypabact_IsGroundFireQuery(*arg)) ||
             !ypabact_FireTriggerAreaContains(triggerArea, arg->pos,
                                              arg->unkn == 2 ? arg->radius : 0.0f) )
            return false;

        return (arg->pos - fireOrigin).length() <= ypabact_LaserRange(wproto);
    }

    const vec3d fireOrigin = ypabact_LaserSourceOrigin(bact);
    TFireTriggerArea triggerArea;
    const bool hasTriggerArea = bact->GetAIFireTriggerArea(wproto, &triggerArea, &arg->pos, ypabact_IsGroundFireQuery(*arg));
    if ( hasTriggerArea &&
         !ypabact_FireTriggerAreaContains(triggerArea, arg->pos, arg->unkn == 2 ? arg->radius : 0.0f) )
        return false;
    vec3d toAim = arg->pos - fireOrigin;
    const float distance = toAim.normalise();
    if ( distance <= 0.001f || distance > ypabact_LaserRange(wproto) )
        return false;

    vec3d forward = hasTriggerArea ? triggerArea.forward : bact->_rotation.AxisZ();
    if ( forward.normalise() <= 0.001f )
        return false;

    // Hysteresis: CheckFireAI() remains the strict acquisition gate.  Once a
    // laser is already connected, allow the AI to track within a broader forward
    // cone instead of releasing the trigger on tiny steering/target movements.
    // The beam still cannot persist behind the vehicle or beyond its real range.
    constexpr float HoldAlignment = 0.50f;
    return toAim.dot(forward) >= HoldAlignment;
}
