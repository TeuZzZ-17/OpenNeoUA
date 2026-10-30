// Kamikaze units: arming, ramming guidance, contact detection and detonation.
#include "game/ypabact_internal.h"

bool ypabact_IsKamikazeActorUsable(NC_STACK_ypabact *unit)
{
    return unit &&
           unit->getBACT_pWorld() &&
           unit->_energy > 0 &&
           unit->_energy_max > 0 &&
           unit->_bact_type != BACT_TYPES_MISSLE &&
           unit->_status != BACT_STATUS_DEAD &&
           unit->_status != BACT_STATUS_CREATE &&
           unit->_status != BACT_STATUS_BEAM &&
           !(unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

bool ypabact_IsMobileKamikazeCarrier(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    switch ( unit->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_ROBO:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
        return true;

    default:
        return false;
    }
}

NC_STACK_ypabact *ypabact_GetEffectiveKamikazeCarrier(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->_bact_type != BACT_TYPES_GUN )
        return unit;

    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(unit);
    if ( !gun )
        return unit;

    if ( unit->_isUnitGunChild &&
         ypabact_IsMobileKamikazeCarrier(unit->_parent) &&
         ypabact_IsKamikazeActorUsable(unit->_parent) &&
         unit->_parent->getBACT_pWorld() == unit->getBACT_pWorld() )
    {
        return unit->_parent;
    }

    if ( gun->IsRoboGun() &&
         ypabact_IsMobileKamikazeCarrier(unit->_host_station) &&
         ypabact_IsKamikazeActorUsable(unit->_host_station) &&
         unit->_host_station->getBACT_pWorld() == unit->getBACT_pWorld() )
    {
        return unit->_host_station;
    }

    return unit;
}

bool ypabact_ResolveMountedKamikazeWeapon(NC_STACK_ypabact *source,
                                                 int *outWeaponId,
                                                 const World::TWeapProto **outWeapon)
{
    if ( outWeaponId )
        *outWeaponId = -1;
    if ( outWeapon )
        *outWeapon = nullptr;

    if ( !ypabact_IsKamikazeActorUsable(source) )
        return false;

    NC_STACK_ypaworld *world = source->getBACT_pWorld();
    const std::vector<World::TWeapProto> &weapons = world->GetWeaponsProtos();
    const int weaponIds[4] = {
        source->_weapon,
        source->_extra_weapons[0],
        source->_extra_weapons[1],
        source->_extra_weapons[2]
    };

    for (int slot = 0; slot < 4; slot++)
    {
        const int weaponId = weaponIds[slot];
        if ( (slot == 0 && weaponId < 0) ||
             (slot > 0 && weaponId <= 0) ||
             (size_t)weaponId >= weapons.size() )
            continue;

        const World::TWeapProto &weapon = weapons[weaponId];
        if ( !weapon.IsKamikaze() )
            continue;

        if ( outWeaponId )
            *outWeaponId = weaponId;
        if ( outWeapon )
            *outWeapon = &weapon;
        return true;
    }

    return false;
}

bool ypabact_ResolveKamikazeFromGunList(
    NC_STACK_ypabact *carrier,
    const std::vector<World::TRoboGun> &guns,
    TKamikazeMount *outMount)
{
    if ( !carrier || !outMount )
        return false;

    for (const World::TRoboGun &gun : guns)
    {
        NC_STACK_ypabact *source = gun.gun_obj;
        if ( !source || source->getBACT_pWorld() != carrier->getBACT_pWorld() )
            continue;

        int weaponId = -1;
        const World::TWeapProto *weapon = nullptr;
        if ( !ypabact_ResolveMountedKamikazeWeapon(source, &weaponId, &weapon) )
            continue;

        outMount->carrier = carrier;
        outMount->payloadSource = source;
        outMount->weaponId = weaponId;
        outMount->weapon = weapon;
        return true;
    }

    return false;
}

bool ypabact_ResolveKamikazeMount(NC_STACK_ypabact *unit,
                                         TKamikazeMount *outMount)
{
    if ( !unit || !outMount )
        return false;

    *outMount = TKamikazeMount();

    NC_STACK_ypabact *carrier = ypabact_GetEffectiveKamikazeCarrier(unit);
    if ( !ypabact_IsKamikazeActorUsable(carrier) )
        return false;

    int weaponId = -1;
    const World::TWeapProto *weapon = nullptr;

    // The physical carrier's own canonical primary slots always win.
    if ( ypabact_ResolveMountedKamikazeWeapon(carrier, &weaponId, &weapon) )
    {
        outMount->carrier = carrier;
        outMount->payloadSource = carrier;
        outMount->weaponId = weaponId;
        outMount->weapon = weapon;
        return true;
    }

    if ( !ypabact_IsMobileKamikazeCarrier(carrier) )
        return false;

    // UnitGun/Module and native RoboGun lists are already stable attachment
    // orderings. The first live mounted Kamikaze Weapon supplies the payload.
    if ( ypabact_ResolveKamikazeFromGunList(carrier, carrier->_unitGuns, outMount) )
        return true;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(carrier);
    return robo && ypabact_ResolveKamikazeFromGunList(carrier, robo->GetGuns(), outMount);
}

bool ypabact_IsKamikazeMountArmed(const TKamikazeMount &mount)
{
    if ( !mount.carrier || !mount.payloadSource || !mount.weapon ||
         !mount.weapon->IsKamikaze() ||
         mount.carrier->_kamikaze_triggered ||
         !ypabact_IsKamikazeActorUsable(mount.carrier) ||
         !ypabact_IsKamikazeActorUsable(mount.payloadSource) )
    {
        return false;
    }

    NC_STACK_ypaworld *world = mount.carrier->getBACT_pWorld();
    return world && !world->IsSpectatorBact(mount.carrier);
}

bool ypabact_IsKamikazeArmed(NC_STACK_ypabact *unit)
{
    TKamikazeMount mount;
    return ypabact_ResolveKamikazeMount(unit, &mount) &&
           ypabact_IsKamikazeMountArmed(mount);
}

bool NC_STACK_ypabact::GetKamikazeFireTimeScale(float *outScale,
                                                  float *outHpDrainPerSecond)
{
    if ( outScale )
        *outScale = 1.0f;
    if ( outHpDrainPerSecond )
        *outHpDrainPerSecond = 0.0f;

    TKamikazeMount mount;
    if ( !ypabact_ResolveKamikazeMount(this, &mount) ||
         !ypabact_IsKamikazeMountArmed(mount) ||
         mount.carrier != this || !mount.weapon )
    {
        return false;
    }

    const float scale = mount.weapon->fire_time_scale;
    const World::TAbsoluteOrPercent &drain = mount.weapon->fire_time_scale_hp_drain;
    if ( !std::isfinite(scale) || scale >= 1.0f || scale < 0.05f ||
         !drain.defined || !std::isfinite(drain.value) || drain.value <= 0.0f )
    {
        return false;
    }

    double resolvedDrain = drain.percent
        ? (double)std::max(_energy_max, 0) * (double)drain.value / 100.0
        : (double)drain.value;
    if ( !std::isfinite(resolvedDrain) || resolvedDrain <= 0.0 )
        return false;

    resolvedDrain = std::min(resolvedDrain, (double)std::numeric_limits<float>::max());
    if ( outScale )
        *outScale = scale;
    if ( outHpDrainPerSecond )
        *outHpDrainPerSecond = (float)resolvedDrain;
    return true;
}

bool NC_STACK_ypabact::GetKamikazeDebugSphere(float *outRadius)
{
    if ( outRadius )
        *outRadius = 0.0f;

    TKamikazeMount mount;
    if ( !ypabact_ResolveKamikazeMount(this, &mount) ||
         !ypabact_IsKamikazeMountArmed(mount) ||
         mount.carrier != this )
    {
        return false;
    }

    float radius = mount.weapon->trigger_radius;
    if ( !std::isfinite(radius) || radius <= 0.0f )
        radius = GetCollisionBroadRadius();

    if ( !std::isfinite(radius) || radius <= 0.01f )
        return false;

    if ( outRadius )
        *outRadius = radius;
    return true;
}

bool ypabact_IsValidKamikazeTarget(NC_STACK_ypabact *unit, NC_STACK_ypabact *target)
{
    if ( !unit ||
         !target ||
         unit == target ||
         target->_isDummy ||
         !unit->getBACT_pWorld() ||
         target->getBACT_pWorld() != unit->getBACT_pWorld() ||
         !ypabact_CanUseGameplayStatusMechanics(target) )
    {
        return false;
    }

    if ( unit->_owner == World::OWNER_0 ||
         target->_owner == World::OWNER_0 ||
         target->_owner == unit->_owner )
    {
        return false;
    }

    if ( target->IsIgnoredByAI() )
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

    if ( target->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(target);
        if ( !gun || (gun->IsRoboGun() && target->GetEffectiveShield() >= 100.0f) )
            return false;
    }

    return true;
}

bool ypabact_IsKamikazeContact(const TKamikazeMount &mount,
                                      NC_STACK_ypabact *target,
                                      const vec3d &from, double *fraction)
{
    NC_STACK_ypabact *carrier = mount.carrier;
    if ( !carrier || !mount.weapon || carrier->CollisionsDisabled() ||
         mount.weapon->disable_collision || !target || target->CollisionsDisabled() ||
         !ypabact_IsValidKamikazeTarget(carrier, target) )
        return false;

    double radius = mount.weapon->trigger_radius;
    if ( !std::isfinite(radius) || radius <= 0.0 )
        radius = carrier->GetCollisionBroadRadius();
    if ( !std::isfinite(radius) || radius <= 0.0 )
        return false;

    // The target's collision surface enters the fuse, not just its origin.
    if ( target->HasCollisionShape() )
    {
        NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
        if ( !world->_collisionScene )
            world->_collisionScene.reset(new Collision::Scene(*world));
        Collision::Contact contact;
        if ( !world->_collisionScene->Trace(target, from, carrier->_position, radius, &contact) )
        {
            if ( !world->_collisionScene->Trace(target, carrier->_position, carrier->_position, radius, &contact) )
                return false;
            contact.fraction = 1.0;
        }
        if ( fraction ) *fraction = contact.fraction;
        return true;
    }

    std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
    target->GetCollisionSpheres(spheres, target->_position, target->_rotation, true);
    bool found = false;
    double first = 1.0;
    for (const auto &sphere : spheres)
    {
        double entry;
        if ( World::KamikazeSphereSweep(from, carrier->_position, sphere.center,
                                        radius + sphere.radius, &entry) && entry <= first )
        {
            first = entry;
            found = true;
        }
    }
    if ( found && fraction ) *fraction = first;
    return found;
}

NC_STACK_ypabact *ypabact_FindKamikazeContactTarget(const TKamikazeMount &mount,
                                                          const vec3d &from,
                                                          double *fraction)
{
    NC_STACK_ypabact *carrier = mount.carrier;
    if ( !carrier || !ypabact_IsKamikazeMountArmed(mount) )
        return nullptr;
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
    NC_STACK_ypabact *firstTarget = nullptr;
    double first = 1.0;
    const auto consider = [&](NC_STACK_ypabact *target)
    {
        double entry;
        if ( ypabact_IsKamikazeContact(mount, target, from, &entry) &&
             (!firstTarget || entry < first) )
        {
            first = entry;
            firstTarget = target;
        }
    };
    if ( carrier->_secndTtype == BACT_TGT_TYPE_UNIT ) consider(carrier->_secndT.pbact);
    if ( carrier->_primTtype == BACT_TGT_TYPE_UNIT ) consider(carrier->_primT.pbact);

    double fuseRadius = mount.weapon->trigger_radius;
    if ( !std::isfinite(fuseRadius) || fuseRadius <= 0.0 )
        fuseRadius = carrier->GetCollisionBroadRadius();
    fuseRadius = std::max(fuseRadius, (double)carrier->GetCollisionBroadRadius());
    const int sectorRadius = (int)std::min(std::ceil(fuseRadius / World::CVSectorLength) + 2.0,
                                          (double)std::max(world->_mapSize.x, world->_mapSize.y));
    const Common::Point oldCell = World::PositionToSectorID(from);
    const Common::Point newCell = World::PositionToSectorID(carrier->_position);
    const int minX = std::max(1, std::min(oldCell.x, newCell.x) - sectorRadius);
    const int maxX = std::min(world->_mapSize.x - 2, std::max(oldCell.x, newCell.x) + sectorRadius);
    const int minY = std::max(1, std::min(oldCell.y, newCell.y) - sectorRadius);
    const int maxY = std::min(world->_mapSize.y - 2, std::max(oldCell.y, newCell.y) + sectorRadius);
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x)
            for (NC_STACK_ypabact *target : world->SectorAt(Common::Point(x, y)).unitsList)
                consider(target);
    // Large authored hulls can enter the fuse while their origins lie outside
    // the nearby cells. Reuse the collision scene's spatial index for them.
    if ( world->_collisionScene )
        for (NC_STACK_ypabact *target : world->_collisionScene->ShapeTargets(from, carrier->_position, fuseRadius))
            consider(target);
    if ( firstTarget && fraction ) *fraction = first;
    return firstTarget;
}

bool ypabact_GetKamikazeCellTarget(NC_STACK_ypabact *unit, vec3d *outTarget)
{
    if ( !unit || !outTarget || !unit->getBACT_pWorld() ||
         (unit->_oflags & BACT_OFLAG_USERINPT) )
    {
        return false;
    }

    const bool roboCarrier = unit->_bact_type == BACT_TYPES_ROBO;
    if ( !roboCarrier &&
         (unit->_atk_ret != NC_STACK_ypabact::TA_FIGHT ||
          !(unit->_status_flg & BACT_STFLAG_ATTACK)) )
    {
        return false;
    }

    vec3d targetPos;
    if ( unit->_secndTtype != BACT_TGT_TYPE_NONE )
    {
        if ( unit->_secndTtype != BACT_TGT_TYPE_CELL )
            return false;
        targetPos = unit->_sencdTpos;
    }
    else if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
    {
        targetPos = unit->_primTpos;
    }
    else
    {
        return false;
    }

    yw_130arg cellInfo;
    cellInfo.pos_x = targetPos.x;
    cellInfo.pos_z = targetPos.z;
    if ( !unit->getBACT_pWorld()->GetSectorInfo(&cellInfo) || !cellInfo.pcell )
        return false;

    // Mirror normal sector aggression semantics. A non-owned sector remains a
    // valid Kamikaze objective so the unit can complete conquest orders. At
    // maximum aggression, an already-owned sector also remains a valid combat
    // target while it still has sector energy/buildings to destroy; once it is
    // razed (energy <= 0), the Kamikaze must not detonate there just because it
    // happens to be passing through its own territory.
    if ( cellInfo.pcell->owner == unit->_owner &&
         (unit->_aggr < 100 || cellInfo.pcell->GetEnergy() <= 0) )
    {
        return false;
    }

    // Reuse the same authored/best-building point selected by normal sector
    // combat so the fuse follows the same live target point as ordinary AI.
    targetPos.y = cellInfo.pcell->height;
    unit->GetBestSectorPart(&targetPos);
    *outTarget = targetPos;
    return true;
}

bool ypabact_IsKamikazeCellContact(const TKamikazeMount &mount,
                                          const vec3d &targetPos,
                                          const vec3d &from, double *fraction)
{
    if ( !mount.carrier || !mount.weapon )
        return false;
    double radius = mount.weapon->trigger_radius;
    if ( !std::isfinite(radius) || radius <= 0.0 )
        radius = mount.carrier->GetCollisionBroadRadius();
    if ( !std::isfinite(radius) || radius <= 0.0 )
        return false;
    NC_STACK_ypaworld *world = mount.carrier->getBACT_pWorld();
    if ( !world->_collisionScene )
        world->_collisionScene.reset(new Collision::Scene(*world));
    Collision::Contact contact;
    if ( !world->_collisionScene->TraceSector(targetPos, from, mount.carrier->_position, radius, &contact) )
        return false;
    if ( fraction ) *fraction = contact.fraction;
    return true;
}

NC_STACK_ypabact *ypabact_GetKamikazeRammingTarget(NC_STACK_ypabact *unit)
{
    TKamikazeMount mount;
    if ( !ypabact_ResolveKamikazeMount(unit, &mount) ||
         mount.carrier != unit ||
         !ypabact_IsKamikazeMountArmed(mount) )
        return nullptr;

    const bool roboCarrier = unit->_bact_type == BACT_TYPES_ROBO;
    if ( !roboCarrier &&
         (unit->_atk_ret != NC_STACK_ypabact::TA_FIGHT ||
          !(unit->_status_flg & BACT_STFLAG_ATTACK)) )
        return nullptr;

    // AI_layer3 always gives the secondary target precedence, including when
    // it is a cell. Mirror that choice here so Kamikaze guidance can
    // never chase a lower-priority unit while the AI is executing another
    // order.
    if ( unit->_secndTtype != BACT_TGT_TYPE_NONE )
    {
        if ( unit->_secndTtype != BACT_TGT_TYPE_UNIT )
            return nullptr;

        if ( ypabact_IsValidKamikazeTarget(unit, unit->_secndT.pbact) &&
             unit->_secndT.pbact->_pSector &&
             unit->_secndT.pbact->_pSector->IsCanSee(unit->_owner) )
            return unit->_secndT.pbact;

        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 1;
        unit->SetTarget(&arg67);
        return nullptr;
    }

    if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
    {
        if ( ypabact_IsValidKamikazeTarget(unit, unit->_primT.pbact) &&
             unit->_primT.pbact->_pSector &&
             unit->_primT.pbact->_pSector->IsCanSee(unit->_owner) )
            return unit->_primT.pbact;

        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_CELL;
        arg67.tgt_pos = unit->_position;
        arg67.priority = 0;
        unit->SetTarget(&arg67);
    }

    return nullptr;
}

bool NC_STACK_ypabact::ApplyKamikazeRammingGuidance()
{
    TKamikazeMount mount;
    if ( (_oflags & BACT_OFLAG_USERINPT) || IsActiveDebuffStunning() ||
         !ypabact_ResolveKamikazeMount(this, &mount) || mount.carrier != this ||
         !ypabact_IsKamikazeMountArmed(mount) )
        return false;

    vec3d targetPos;
    NC_STACK_ypabact *target = ypabact_GetKamikazeRammingTarget(this);
    if ( target )
    {
        targetPos = target->_position;
    }
    else if ( !ypabact_GetKamikazeCellTarget(this, &targetPos) )
    {
        return false;
    }

    vec3d desired = targetPos - _position;
    double desiredLen = desired.length();
    if ( desiredLen <= 0.001 )
        return false;

    // Keep closing on the live target without combat standoff. The vehicle
    // controller still owns obstacle avoidance and its pending maneuvers.
    _target_vec = desired;
    _status_flg |= BACT_STFLAG_MOVE | BACT_STFLAG_ATTACK;
    _status_flg &= ~BACT_STFLAG_APPROACH;

    return true;
}

NC_STACK_ypabact *ypabact_GetKamikazePayloadListOwner(NC_STACK_ypabact *unit)
{
    if ( unit->_parent && unit->_parent->_status != BACT_STATUS_DEAD )
        return unit->_parent;

    if ( unit->_host_station && unit->_host_station->_status != BACT_STATUS_DEAD )
        return unit->_host_station;

    if ( unit->getBACT_pWorld() )
    {
        NC_STACK_ypabact *userHost = unit->getBACT_pWorld()->getYW_userHostStation();
        if ( userHost && userHost != unit && userHost->_status != BACT_STATUS_DEAD )
            return userHost;
    }

    return unit;
}

bool NC_STACK_ypabact::TriggerKamikazeDetonation(NC_STACK_ypabact *directHit)
{
    TKamikazeMount mount;
    if ( !ypabact_ResolveKamikazeMount(this, &mount) ||
         mount.carrier != this ||
         !ypabact_IsKamikazeMountArmed(mount) )
    {
        return false;
    }

    _kamikaze_triggered = true;
    RevealInvisibleOnAttack();
    if ( mount.payloadSource != this )
        mount.payloadSource->RevealInvisibleOnAttack();

    ypaworld_arg146 arg147;
    arg147.vehicle_id = mount.weaponId;
    arg147.pos = _position;

    NC_STACK_ypamissile *payload = _world->ypaworld_func147(&arg147);
    if ( !payload )
    {
        _kamikaze_triggered = false;
        return false;
    }

    vec3d payloadDir = _rotation.AxisZ();
    if ( payloadDir.normalise() <= 0.001 )
        payloadDir = vec3d::OZ(1.0);

    payload->SetLauncherBact(mount.payloadSource);
    // The payload exists only long enough to reuse the normal Weapon direct-hit,
    // Impact/blast/debuff/push and FX pipeline at the physical carrier position.
    payload->SetStartHeight(std::numeric_limits<float>::lowest());
    payload->_owner = _owner;
    payload->_host_station = mount.payloadSource->_host_station
        ? mount.payloadSource->_host_station : _host_station;
    payload->_fly_dir = payloadDir;
    payload->_fly_dir_length = 0.0;
    payload->_rotation.SetZ(payload->_fly_dir);
    payload->_rotation.SetX(_rotation.AxisX());
    payload->_rotation.SetY(payload->_rotation.AxisZ() * payload->_rotation.AxisX());
    payload->_kidRef.Detach();
    payload->_parent = nullptr;
    ypabact_GetKamikazePayloadListOwner(mount.payloadSource)->_missiles_list.push_back(payload);

    // Positive trigger_radius gives the kamikaze its own terrain-decal rule.
    // Check at the explosion position, before blast damage can kill the carrier
    // or its mounted payload source. The later wreck impact must not repeat it.
    if ( !_world->IsDebugGlobalInvulnerabilityEnabled() &&
         std::isfinite(mount.weapon->trigger_radius) && mount.weapon->trigger_radius > 0.0f )
    {
        _kamikaze_ground_decals_handled = true;
        for (const World::TChainFXConfig &fx : _chainFX)
        {
            if ( fx.trigger == World::TChainFXConfig::TRIGGER_DESTROYED &&
                 fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL )
            {
                _world->SpawnGroundDecalInSphere(
                    fx, _position, mount.weapon->trigger_radius);
            }
        }
    }

    payload->DetonateKamikazePayload(directHit);

    // Global debug invulnerability protects the carrier while preserving the
    // payload detonation and its visual/gameplay reactions. Keep the trigger
    // latched so the same unit does not emit a new payload every frame.
    if ( _world && _world->IsDebugGlobalInvulnerabilityEnabled() )
        return true;

    bool wasInvulnerable = _invulnerable;
    _invulnerable = false;

    PrepareSuicideControlHandoff();

    int suicideDamage = _energy_max > 0 ? _energy_max : (_energy > 0 ? _energy : 1);
    if ( suicideDamage > std::numeric_limits<int>::max() / 2 )
        suicideDamage = std::numeric_limits<int>::max() / 2;

    bact_arg84 arg84;
    arg84.unit = this;
    arg84.energy = -2 * suicideDamage;

    ModifyEnergy(&arg84);

    if ( _status != BACT_STATUS_DEAD )
    {
        _energy = 0;

        setState_msg arg78;
        arg78.unsetFlags = 0;
        arg78.setFlags = 0;
        arg78.newStatus = BACT_STATUS_DEAD;

        SetState(&arg78);
        Die();
    }

    _invulnerable = wasInvulnerable;
    return true;
}

void NC_STACK_ypabact::UpdateKamikaze(update_msg *, const vec3d *oldPosition)
{
    if (CollisionsDisabled()) return;
    TKamikazeMount mount;
    if ( !ypabact_ResolveKamikazeMount(this, &mount) ||
         mount.carrier != this || mount.weapon->disable_collision ||
         !ypabact_IsKamikazeMountArmed(mount) )
        return;

    const vec3d from = oldPosition ? *oldPosition : _position;
    double unitEntry = 1.0, cellEntry = 1.0;
    NC_STACK_ypabact *target = ypabact_FindKamikazeContactTarget(mount, from, &unitEntry);
    vec3d cellTarget;
    const bool cellContact = ypabact_GetKamikazeCellTarget(this, &cellTarget) &&
        ypabact_IsKamikazeCellContact(mount, cellTarget, from, &cellEntry);
    if ( !target && !cellContact )
        return;

    const bool useUnit = target && (!cellContact || unitEntry <= cellEntry);
    const double entry = useUnit ? unitEntry : cellEntry;
    if ( oldPosition && entry < 1.0 )
    {
        // Detonate at the first contact reached during this frame's movement,
        // so a fast carrier cannot fly through the fuse or explode beyond it.
        bact_arg80 position;
        position.pos = from + (_position - from) * std::max(0.0, entry);
        position.field_C = 0;
        SetPosition(&position);
    }
    TriggerKamikazeDetonation(useUnit ? target : nullptr);
}
