// Artillery shell weapon: platforms, barrage targeting and launch visuals.
#include "game/ypabact_internal.h"

// ===== OpenNeoUA custom: radar-guided artillery shell barrage =========================

// Resolve an artillery shell weapon id from one unit's own main/extra weapon slots.
// Mounted unit-guns are handled separately so their child BACT owns the barrage
// runtime state and the shell launch position.
int ypabact_GetArtilleryShellWeaponId(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
        return 0;

    std::vector<World::TWeapProto> &weapons = unit->getBACT_pWorld()->GetWeaponsProtos();

    int candidates[4] = { unit->_weapon, unit->_extra_weapons[0], unit->_extra_weapons[1], unit->_extra_weapons[2] };

    for (int i = 0; i < 4; i++)
    {
        int id = candidates[i];
        if ( id > 0 && (size_t)id < weapons.size() && weapons.at(id).IsArtilleryShell() )
            return id;
    }

    return 0;
}

int ypabact_GetVehicleProtoArtilleryShellWeaponId(NC_STACK_ypaworld *world, int vehicleId)
{
    if ( !world || vehicleId <= 0 || (size_t)vehicleId >= world->GetVhclProtos().size() )
        return 0;

    const World::TVhclProto &proto = world->GetVhclProtos().at(vehicleId);
    std::vector<World::TWeapProto> &weapons = world->GetWeaponsProtos();

    int candidates[4] = { proto.weapon, proto.extra_weapons[0], proto.extra_weapons[1], proto.extra_weapons[2] };

    for (int i = 0; i < 4; i++)
    {
        int id = candidates[i];
        if ( id > 0 && (size_t)id < weapons.size() && weapons.at(id).IsArtilleryShell() )
            return id;
    }

    return 0;
}

bool ypabact_IsLiveArtilleryShellGunActor(NC_STACK_ypabact *gunObj, int *outWeaponId)
{
    if ( outWeaponId )
        *outWeaponId = 0;

    if ( !gunObj || !gunObj->getBACT_pWorld() )
        return false;

    if ( gunObj->IsDestroyed() ||
         gunObj->_status == BACT_STATUS_DEAD ||
         gunObj->_status == BACT_STATUS_CREATE ||
         (gunObj->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER)) )
        return false;

    int weaponId = ypabact_GetArtilleryShellWeaponId(gunObj);
    if ( weaponId <= 0 )
        return false;

    if ( outWeaponId )
        *outWeaponId = weaponId;

    return true;
}

NC_STACK_ypabact *ypabact_GetArtilleryShellActorFromGunList(std::vector<World::TRoboGun> &guns, int *outWeaponId)
{
    if ( outWeaponId )
        *outWeaponId = 0;

    for (World::TRoboGun &gun : guns)
    {
        int weaponId = 0;
        if ( ypabact_IsLiveArtilleryShellGunActor(gun.gun_obj, &weaponId) )
        {
            if ( outWeaponId )
                *outWeaponId = weaponId;
            return gun.gun_obj;
        }
    }

    return nullptr;
}

int ypabact_GetProtoArtilleryShellWeaponIdFromGunList(NC_STACK_ypaworld *world, const std::vector<World::TRoboGun> &guns)
{
    if ( !world )
        return 0;

    for (const World::TRoboGun &gun : guns)
    {
        int weaponId = ypabact_GetVehicleProtoArtilleryShellWeaponId(world, gun.robo_gun_type);
        if ( weaponId > 0 )
            return weaponId;
    }

    return 0;
}

NC_STACK_ypabact *ypabact_GetMountedArtilleryShellActor(NC_STACK_ypabact *unit, int *outWeaponId)
{
    if ( outWeaponId )
        *outWeaponId = 0;

    if ( !unit || !unit->getBACT_pWorld() )
        return nullptr;

    // Vehicle-mounted unit guns. The parent is selected/rendered in UI, while
    // the child gun owns barrage cooldown/state and launch position.
    if ( NC_STACK_ypabact *actor = ypabact_GetArtilleryShellActorFromGunList(unit->_unitGuns, outWeaponId) )
        return actor;

    // Host Station / Robo guns use a separate gun list. Treat them like mounted
    // unit guns for artillery shell purposes: click/draw on the robo parent, fire from the
    // real gun child.
    if ( NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(unit) )
    {
        if ( NC_STACK_ypabact *actor = ypabact_GetArtilleryShellActorFromGunList(robo->_roboGuns, outWeaponId) )
            return actor;
    }

    return nullptr;
}

int ypabact_GetMountedArtilleryShellWeaponId(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
        return 0;

    int liveWeaponId = 0;
    if ( ypabact_GetMountedArtilleryShellActor(unit, &liveWeaponId) )
        return liveWeaponId;

    // Prototype fallback: this lets the parent be recognized as an artillery shell platform
    // even before the mounted gun child has finished its create cycle. The live
    // child still owns actual firing once manual/AI orders are executed.
    int weaponId = ypabact_GetProtoArtilleryShellWeaponIdFromGunList(unit->getBACT_pWorld(), unit->_unitGuns);
    if ( weaponId > 0 )
        return weaponId;

    if ( NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(unit) )
    {
        weaponId = ypabact_GetProtoArtilleryShellWeaponIdFromGunList(unit->getBACT_pWorld(), robo->_roboGuns);
        if ( weaponId > 0 )
            return weaponId;
    }

    return 0;
}

NC_STACK_ypabact *ypabact_GetManualArtilleryShellActor(NC_STACK_ypabact *unit, int *outWeaponId)
{
    if ( outWeaponId )
        *outWeaponId = 0;

    int ownWeaponId = ypabact_GetArtilleryShellWeaponId(unit);
    if ( ownWeaponId > 0 )
    {
        if ( outWeaponId )
            *outWeaponId = ownWeaponId;
        return unit;
    }

    return ypabact_GetMountedArtilleryShellActor(unit, outWeaponId);
}

// OpenNeoUA custom: true if this unit carries a "model = artillery_shell" weapon in any own
// slot, or if one of its mounted unit-gun / robo-gun children carries one. Used
// to keep artillery shell platforms map-only (no first-person possession).
bool NC_STACK_ypabact::IsArtilleryShellPlatform()
{
    return ypabact_GetArtilleryShellWeaponId(this) > 0 || ypabact_GetMountedArtilleryShellWeaponId(this) > 0;
}

// OpenNeoUA custom: true if this is an artillery shell platform usable via manual map-click.
// Manual map-click control is always enabled for artillery shells (there is no opt-in flag).
bool NC_STACK_ypabact::IsManualArtilleryShellPlatform()
{
    return IsArtilleryShellPlatform();
}

// OpenNeoUA custom: bombardment zone radius of this unit's artillery shell weapon (0 if none).
// Used to size the external SVG aiming marker on the 2D map.
float NC_STACK_ypabact::GetArtilleryShellBarrageRadius()
{
    if ( !_world )
        return 0.0f;

    int weaponId = 0;
    if ( NC_STACK_ypabact *actor = ypabact_GetManualArtilleryShellActor(this, &weaponId) )
    {
        (void)actor;
        if ( weaponId > 0 )
            return _world->GetWeaponsProtos().at(weaponId).artillery_shell_barrage_radius;
    }

    weaponId = ypabact_GetMountedArtilleryShellWeaponId(this);
    if ( weaponId > 0 )
        return _world->GetWeaponsProtos().at(weaponId).artillery_shell_barrage_radius;

    return 0.0f;
}

std::string NC_STACK_ypabact::GetArtilleryShellMarkerPath()
{
    if ( !_world )
        return std::string();

    int weaponId = 0;
    if ( NC_STACK_ypabact *actor = ypabact_GetManualArtilleryShellActor(this, &weaponId) )
    {
        (void)actor;
        if ( weaponId > 0 )
            return _world->GetWeaponsProtos().at(weaponId).artillery_shell_marker_path;
    }

    weaponId = ypabact_GetMountedArtilleryShellWeaponId(this);
    if ( weaponId > 0 )
        return _world->GetWeaponsProtos().at(weaponId).artillery_shell_marker_path;

    return std::string();
}

float NC_STACK_ypabact::GetArtilleryShellReadinessRatio()
{
    if ( !_world )
        return 0.0f;

    int weaponId = 0;
    NC_STACK_ypabact *actor = ypabact_GetManualArtilleryShellActor(this, &weaponId);
    if ( !actor || weaponId <= 0 || (size_t)weaponId >= _world->GetWeaponsProtos().size() )
        return IsArtilleryShellPlatform() ? 1.0f : 0.0f;

    // While a barrage is actively spending its shot budget, the platform is not
    // available for another area. The readiness bar therefore stays empty until the
    // current barrage finishes, then fills normally across the configured cooldown.
    if ( actor->_artillery_shell_barrage_active && actor->_artillery_shell_shots_remaining > 0 )
        return 0.0f;

    const World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);
    int cooldown = wproto.artillery_shell_barrage_cooldown > 0 ? wproto.artillery_shell_barrage_cooldown : 10000;
    if ( cooldown <= 0 || actor->_clock >= actor->_artillery_shell_next_activation_time )
        return 1.0f;

    int remaining = actor->_artillery_shell_next_activation_time - actor->_clock;
    float ready = 1.0f - ((float)remaining / (float)cooldown);
    if ( ready < 0.0f )
        ready = 0.0f;
    if ( ready > 1.0f )
        ready = 1.0f;

    return ready;
}

bool ypabact_CanUseArtilleryShell(NC_STACK_ypabact *unit)
{
    if ( !unit || !unit->getBACT_pWorld() )
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

    // Dummy attachments and spectator helpers never auto-fire artillery shells.
    if ( unit->_isDummy )
        return false;

    if ( unit->getBACT_pWorld()->IsSpectatorBact(unit) )
        return false;

    return true;
}

// OpenNeoUA custom: the actual ballistic launch direction of a shell aimed at
// targetCenter. Matches NC_STACK_ypamissile::UpdateArtilleryShellBallistic(): the shell
// follows pos(t)=lerp(start,target) horizontally with a parabolic vertical arc
// pos.y = baseY - arcHeight*4*t*(1-t). Differentiating at t=0 gives the initial
// velocity below (engine convention: +Y is DOWN, so -4*arcHeight points upward).
// The barrel is aimed along this vector so it visibly points up into the arc.
vec3d ypabact_GetArtilleryShellLaunchDir(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, const vec3d &targetCenter)
{
    if ( wproto.artillery_shell_mode == World::TWeapProto::ARTILLERY_SHELL_MODE_VERTICAL_BARRAGE )
        return vec3d(0.0, -1.0, 0.0); // engine +Y is down: fire straight up

    vec3d delta = targetCenter - unit->_position;

    vec3d dir;
    dir.x = delta.x;
    dir.z = delta.z;
    dir.y = delta.y - 4.0f * wproto.artillery_shell_arc_height;

    return dir; // caller normalises
}

float ypabact_ClampArtilleryShellAimDelta(float delta, float maxRot)
{
    if ( maxRot <= 0.0f )
        return delta;

    if ( delta > maxRot )
        return maxRot;

    if ( delta < -maxRot )
        return -maxRot;

    return delta;
}

void ypabact_AimArtilleryShellLauncherVisual(NC_STACK_ypabact *unit, const World::TWeapProto &wproto,
                                            const vec3d &targetCenter, int frameTime, bool instant)
{
    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(unit);
    if ( !gun || gun->_isDummy )
        return;

    if ( gun->_gunBasis.length() <= 0.001f || gun->_gunRott.length() <= 0.001f )
        return;

    vec3d vTgt = ypabact_GetArtilleryShellLaunchDir(unit, wproto, targetCenter);
    float dist = vTgt.length();
    if ( dist <= 0.001f )
        return;

    vTgt /= dist;

    unit->_target_vec = targetCenter - unit->_position;
    unit->_target_dir = vTgt;

    float maxRot = instant ? 0.0f : unit->_maxrot * ((float)frameTime / 1000.0f);

    float xzAngle = 0.0f;
    float xzWanted = 0.0f;
    vec3d lx = gun->_gunRott * gun->_gunBasis;

    vec2d xzRot( unit->_rotation.AxisX().dot(lx),
                 unit->_rotation.AxisX().dot(gun->_gunBasis) );

    if ( xzRot.normalise() > 0.001f )
        xzAngle = xzRot.xyAngle();

    vec2d xzWant( vTgt.dot(gun->_gunBasis),
                  vTgt.dot(-lx) );

    if ( xzWant.normalise() > 0.001f )
        xzWanted = xzWant.xyAngle();
    else
        xzWanted = xzAngle;

    if ( gun->_gunMaxSide <= 3.1f )
    {
        if ( xzWanted < -gun->_gunMaxSide )
            xzWanted = -gun->_gunMaxSide;

        if ( xzWanted > gun->_gunMaxSide )
            xzWanted = gun->_gunMaxSide;
    }

    float xzDelta = xzWanted - xzAngle;

    if ( gun->_gunMaxSide > 3.1f )
    {
        if ( fabs(xzDelta) > C_PI )
        {
            if ( xzDelta < -C_PI )
                xzDelta += C_2PI;

            if ( xzDelta > C_PI )
                xzDelta -= C_2PI;
        }
    }

    xzDelta = ypabact_ClampArtilleryShellAimDelta(xzDelta, maxRot);

    if ( fabs(xzDelta) > 0.001f )
        unit->_rotation *= mat3x3(gun->_gunRott, xzDelta);

    vec3d invRed = -gun->_gunRott;
    float yAngle = clp_asin( invRed.dot(unit->_rotation.AxisZ()) );
    float yWant = clp_asin( invRed.dot(vTgt) );

    // OpenNeoUA custom: artillery shells are high-angle artillery. Guarantee a generous upward
    // elevation envelope even if the gun model's gun_up_angle is small, so the
    // barrel convincingly points up into the ballistic arc.
    const float artilleryMinMaxUp =
        wproto.artillery_shell_mode == World::TWeapProto::ARTILLERY_SHELL_MODE_VERTICAL_BARRAGE
            ? 1.55f  // ~89 degrees: mortar mode visibly points almost straight up
            : 1.30f; // ~74 degrees: ballistic mode keeps the existing high-angle envelope
    float gunMaxUp = gun->_gunMaxUp > artilleryMinMaxUp ? gun->_gunMaxUp : artilleryMinMaxUp;

    if ( yWant > gunMaxUp )
        yWant = gunMaxUp;

    if ( yWant < -gun->_gunMaxDown )
        yWant = -gun->_gunMaxDown;

    float yDelta = yWant - yAngle;
    yDelta = ypabact_ClampArtilleryShellAimDelta(yDelta, maxRot);

    // No extra damping: yDelta is already speed-limited by maxRot when not instant,
    // so the barrel reaches the wanted elevation instead of stalling short of it.
    if ( fabs(yDelta) > 0.001f )
        unit->_rotation = mat3x3::RotateX(yDelta) * unit->_rotation;

    unit->_viewer_rotation = unit->_rotation;
}

vec3d ypabact_GetArtilleryShellLaunchPosition(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return vec3d(0.0, 0.0, 0.0);

    return unit->GetBodyPosition() + unit->_rotation.Transpose().Transform(unit->_fire_pos);
}

void ypabact_TriggerArtilleryShellFireVisual(NC_STACK_ypabact *unit)
{
    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(unit);
    if ( !gun || gun->_isDummy )
        return;

    int fireTime = gun->_gunFireTime > 0 ? gun->_gunFireTime : 100;
    if ( gun->_gunFireCount < fireTime )
        gun->_gunFireCount = fireTime;

    if ( !(unit->_status_flg & BACT_STFLAG_FIRE) )
    {
        setState_msg state;
        state.unsetFlags = 0;
        state.newStatus = BACT_STATUS_NOPE;
        state.setFlags = BACT_STFLAG_FIRE;

        unit->SetState(&state);
    }
}

// A candidate is a valid artillery shell target only if it is a real, living enemy actor.
bool ypabact_IsArtilleryShellEnemy(NC_STACK_ypabact *unit, NC_STACK_ypabact *cand)
{
    if ( !cand || cand == unit )
        return false;

    if ( cand->_bact_type == BACT_TYPES_MISSLE )
        return false;

    if ( cand->_status == BACT_STATUS_DEAD ||
         cand->_status == BACT_STATUS_CREATE ||
         cand->_status == BACT_STATUS_BEAM )
        return false;

    if ( cand->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER | BACT_STFLAG_CLEAN) )
        return false;

    if ( cand->_owner == World::OWNER_0 || cand->_owner == unit->_owner )
        return false;

    if ( cand->_isDummy ) // dummy attachments must never be preferred targets
        return false;

    // OpenNeoUA invisible: cloaked stealth units are not valid artillery shell barrage targets.
    if ( !cand->CanBeSeenByAIOrRadar() )
        return false;

    return true;
}

// V1 target selection: pick the nearest valid enemy within the min/max of
// artillery_shell_range_minmax and use its position as the barrage center.
//
// onlyHostStation: when true, only enemy Host Stations (robo) are considered (used for
// the built-in Host Station priority pass). The radar requirement is always
// honoured in both passes: priority only chooses among radar-visible enemies.
bool ypabact_ScanArtilleryShellTarget(NC_STACK_ypabact *unit, const World::TWeapProto &wproto,
                                     bool onlyHostStation, vec3d *out)
{
    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( !world )
        return false;

    float maxRange = wproto.artillery_shell_max_range;
    if ( maxRange <= 0.0 )
        return false; // no max range: artillery shell cannot auto-fire

    float minRange = wproto.artillery_shell_min_range > 0.0 ? wproto.artillery_shell_min_range : 0.0;
    float effRangeSq = maxRange * maxRange;
    float minRangeSq = minRange * minRange;

    int sectorRadius = (int)(maxRange / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(unit->_position);

    NC_STACK_ypabact *best = nullptr;
    float bestDistSq = 0.0;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            // Radar requirement: the target sector must be visible to our faction.
            // Always enforced, including the Host-Station priority pass.
            if ( wproto.artillery_shell_requires_radar && !cell.IsCanSee(unit->_owner) )
                continue;

            for (NC_STACK_ypabact *cand : cell.unitsList)
            {
                if ( !ypabact_IsArtilleryShellEnemy(unit, cand) )
                    continue;

                if ( onlyHostStation && cand->_bact_type != BACT_TYPES_ROBO )
                    continue;

                float distSq = (cand->_position.XZ() - unit->_position.XZ()).square();
                if ( distSq < minRangeSq || distSq > effRangeSq )
                    continue;

                if ( !best || distSq < bestDistSq )
                {
                    best = cand;
                    bestDistSq = distSq;
                }
            }
        }
    }

    if ( !best )
        return false;

    *out = best->_position;
    return true;
}

bool ypabact_FindArtilleryShellTargetZone(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, vec3d *out)
{
    // Artillery always gives first priority to an enemy Host Station (robo), while
    // still honouring artillery_shell_requires_radar. If none is valid, fall back
    // to the nearest normal enemy in range.
    if ( ypabact_ScanArtilleryShellTarget(unit, wproto, true, out) )
        return true;

    return ypabact_ScanArtilleryShellTarget(unit, wproto, false, out);
}

// Fire a single ballistic artillery shell at the target zone (with per-shell spread).
// The shell is tracked in the firing unit's own missile list with the unit as
// emitter, exactly like a normal fired missile, so Die()'s reparent/cleanup keeps
// pointers valid.
bool ypabact_FireArtilleryShell(NC_STACK_ypabact *unit, int weaponId, const World::TWeapProto &wproto, const vec3d &targetCenter)
{
    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( !world || weaponId <= 0 || (size_t)weaponId >= world->GetWeaponsProtos().size() )
        return false;

    // Artillery bypasses LaunchMissile(), but still belongs to the primary Weapon.
    if ( !unit->IsPrimaryWeaponElevationAllowed(targetCenter - unit->_position) )
        return false;

    // Per-shell landing point: barrage_radius is the single canonical bombardment
    // area for both the Tactical Map marker and shell dispersion.
    vec3d landing = targetCenter;
    float spreadRadius = wproto.artillery_shell_barrage_radius;
    if ( spreadRadius > 0.0 )
    {
        float ang = ((float)rand() / (float)RAND_MAX) * 2.0 * C_PI;
        float r   = sqrt((float)rand() / (float)RAND_MAX) * spreadRadius;
        landing.x += cos(ang) * r;
        landing.z += sin(ang) * r;
    }

    // Ground-burst mode (artillery_shell_airburst = 0): snap the impact height to the real
    // terrain at THIS shell's own landing point (spread included), so it explodes on
    // contact with the ground instead of at the nominal target/arc height (airburst).
    if ( !wproto.artillery_shell_airburst )
    {
        ypaworld_arg136 gnd;
        gnd.stPos = vec3d(landing.x, -30000.0, landing.z);
        gnd.vect  = vec3d(0.0, 50000.0, 0.0);
        gnd.flags = 0;

        world->ypaworld_func136(&gnd);

        if ( gnd.isect )
            landing.y = gnd.isectPos.y;
    }

    // OpenNeoUA invisible: launching an artillery shell is a real attack -> reveal the firing
    // unit (covers manual, auto-AI and barrage-followup shells, which all land here).
    unit->RevealInvisibleOnAttack();

    ypabact_AimArtilleryShellLauncherVisual(unit, wproto, targetCenter, 0, true);
    ypabact_TriggerArtilleryShellFireVisual(unit);

    vec3d launchPos = ypabact_GetArtilleryShellLaunchPosition(unit);

    ypaworld_arg146 arg147;
    arg147.vehicle_id = weaponId;
    arg147.pos = launchPos;

    NC_STACK_ypamissile *shell = world->ypaworld_func147(&arg147);
    if ( !shell )
        return false;

    shell->SetLauncherBact(unit);
    shell->_owner = unit->_owner;
    shell->_host_station = unit->_host_station;
    shell->SetStartHeight(launchPos.y);

    // Artillery has a dedicated trajectory speed; generic start_speed keeps its
    // normal Weapon meaning and is not repurposed by model = artillery_shell.
    shell->_fly_dir_length = wproto.artillery_shell_speed;

    int flight = 0;
    if ( wproto.artillery_shell_mode == World::TWeapProto::ARTILLERY_SHELL_MODE_VERTICAL_BARRAGE )
    {
        // Vertical barrage reuses the same physical projectile. artillery_shell_speed
        // controls both ascent and descent; fall_delay is the earliest descent start.
        flight = shell->SetupArtilleryShellVerticalBarrage(
            launchPos, landing, wproto.artillery_shell_fall_delay,
            wproto.artillery_shell_arc_height, wproto.artillery_shell_speed,
            wproto.artillery_shell_vertical_spread_x,
            wproto.artillery_shell_vertical_spread_z,
            !wproto.artillery_shell_airburst);
    }
    else
    {
        // Ballistic mode derives its flight time from the parabolic path length and
        // artillery_shell_speed; <=0 keeps the previous internal 2500 ms fallback.
        flight = shell->SetupArtilleryShell(
            launchPos, landing, wproto.artillery_shell_arc_height,
            wproto.artillery_shell_speed, !wproto.artillery_shell_airburst);
    }
    shell->StartWeaponTracer();

    shell->_kidRef.Detach();
    shell->_parent = nullptr;
    unit->_missiles_list.push_back(shell);

    world->SpawnTransientVisual(wproto.vp_launch, wproto.visual_3ds.launch,
                                wproto.visual_base.launch,
                                shell->_position, shell->_rotation, 1000,
                                1.0, World::TVisualTint(), wproto.launch_scale);

    SFXEngine::SFXe.startSound(&shell->_soundcarrier, 1);

    // Make sure the shell outlives its scheduled flight time (reuses guarded flight above).
    if ( shell->GetLifeTime() < flight + 1000 )
        shell->SetLifeTime(flight + 1000);

    // OpenNeoUA custom: register/refresh the bombardment marker for this barrage zone.
    // Each shell refreshes it so the external marker stays up until the last shell has landed.
    if ( wproto.artillery_shell_barrage_radius > 0.0 )
        world->AddArtilleryShellMarker(targetCenter, wproto.artillery_shell_barrage_radius, unit->_owner, unit->_gid,
                                       wproto.artillery_shell_marker_path, false, flight);

    return true;
}

// Keep the single queued artillery target visible while it is waiting behind either
// an active barrage or its cooldown. Pending orders are unique per platform, so this
// refreshes/moves one disabled-grey marker instead of creating parallel future zones.
void ypabact_RefreshPendingArtilleryShellMarker(NC_STACK_ypabact *unit, const World::TWeapProto &wproto)
{
    if ( !unit || !unit->_artillery_shell_has_pending )
        return;

    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( world && wproto.artillery_shell_barrage_radius > 0.0 )
    {
        world->AddArtilleryShellMarker(unit->_artillery_shell_pending_target,
                                       wproto.artillery_shell_barrage_radius,
                                       unit->_owner, unit->_gid,
                                       wproto.artillery_shell_marker_path, true, 1000);
    }
}

// Queue/replace the next artillery target while the platform is unavailable. Manual
// orders and automatic target acquisition intentionally share this one pending state, so
// each artillery platform can advertise exactly one future strike while the current
// barrage finishes and/or its cooldown is running.
void ypabact_QueueArtilleryShellTarget(NC_STACK_ypabact *unit, const World::TWeapProto &wproto,
                                               const vec3d &targetPos)
{
    if ( !unit )
        return;

    unit->_artillery_shell_pending_target = targetPos;
    unit->_artillery_shell_has_pending = true;
    ypabact_RefreshPendingArtilleryShellMarker(unit, wproto);
}

// Fire one shell at the unit's current artillery shell target, then advance the shared shot
// budget. When the budget is spent the barrage ends and the cooldown starts. This
// single place owns the budget/cooldown bookkeeping for both the AI and manual paths.
void ypabact_FireArtilleryShellShotAndAdvance(NC_STACK_ypabact *unit, int weaponId,
                                             const World::TWeapProto &wproto)
{
    int delay    = wproto.artillery_shell_barrage_shot_delay > 0 ? wproto.artillery_shell_barrage_shot_delay : 0;
    int cooldown = wproto.artillery_shell_barrage_cooldown  > 0 ? wproto.artillery_shell_barrage_cooldown  : 10000;

    if ( !ypabact_FireArtilleryShell(unit, weaponId, wproto, unit->_artillery_shell_target_center) )
    {
        // Do not spend barrage budget if the projectile could not be created. Retry
        // after the normal shot delay instead of silently eating a shell.
        unit->_artillery_shell_next_shot_time = unit->_clock + delay;
        return;
    }

    unit->_artillery_shell_shots_remaining--;

    if ( unit->_artillery_shell_shots_remaining <= 0 )
    {
        unit->_artillery_shell_shots_remaining = 0;
        unit->_artillery_shell_barrage_active = false;
        unit->_artillery_shell_next_activation_time = unit->_clock + cooldown; // cooldown starts now
    }
    else
    {
        unit->_artillery_shell_next_shot_time = unit->_clock + delay;
    }
}

void NC_STACK_ypabact::UpdateArtilleryShell(update_msg *arg)
{
    if ( IsActiveDebuffStunFireBlocked() )
        return;

    if ( !_world )
        return;

    // V1 is single-player/local only; unsynchronised network projectiles are unsafe.
    if ( _world->_isNetGame )
    {
        _artillery_shell_barrage_active = false;
        return;
    }

    int weaponId = ypabact_GetArtilleryShellWeaponId(this);
    if ( weaponId <= 0 || !ypabact_CanUseArtilleryShell(this) )
    {
        _artillery_shell_barrage_active = false;
        return;
    }

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);

    int shots = wproto.artillery_shell_barrage_shots;
    if ( shots <= 0 )
    {
        _artillery_shell_barrage_active = false;
        return;
    }

    // Active barrage: its target is immutable until the shared shot budget is spent.
    // A manual follow-up order may already be waiting, but it is only a grey pending
    // zone and must never redirect any shell from the current barrage.
    if ( _artillery_shell_barrage_active )
    {
        ypabact_RefreshPendingArtilleryShellMarker(this, wproto);
        ypabact_AimArtilleryShellLauncherVisual(this, wproto, _artillery_shell_target_center, arg ? arg->frameTime : 0, false);

        if ( _clock < _artillery_shell_next_shot_time )
            return;

        ypabact_FireArtilleryShellShotAndAdvance(this, weaponId, wproto);
        return;
    }

    // One pending order (manual or automatic) is authoritative while the platform is
    // unavailable. Keep its disabled-grey map ring alive and promote it to an active
    // faction-colour barrage only after the previous barrage has ended and its cooldown
    // has elapsed.
    if ( _artillery_shell_has_pending )
    {
        ypabact_RefreshPendingArtilleryShellMarker(this, wproto);

        if ( _clock >= _artillery_shell_next_activation_time )
        {
            vec3d pending = _artillery_shell_pending_target;
            _artillery_shell_has_pending = false;
            _world->RemovePendingArtilleryShellMarker(_gid);
            StartArtilleryShellBarrage(pending);
        }

        return;
    }

    // Manual-only mode: the auto AI is disabled. Active barrages and queued manual
    // orders (handled above) still run; we just never auto-acquire a target here.
    if ( wproto.artillery_shell_manual_mode_only )
        return;

    // Throttle automatic target acquisition both while ready and while cooling down.
    // During cooldown a valid target is queued and shown in disabled grey instead of
    // being fired early; when ready, the same acquisition starts the barrage directly.
    if ( _clock < _artillery_shell_next_scan_time )
        return;
    _artillery_shell_next_scan_time = _clock + 500;

    vec3d targetCenter;
    if ( !ypabact_FindArtilleryShellTargetZone(this, wproto, &targetCenter) )
        return;

    if ( _clock < _artillery_shell_next_activation_time )
    {
        ypabact_QueueArtilleryShellTarget(this, wproto, targetCenter);
        return;
    }

    StartArtilleryShellBarrage(targetCenter);
}

// Begin a fresh barrage aimed at targetCenter. Shared by the automatic AI and the
// manual call. An in-progress barrage is deliberately immutable: follow-up manual
// orders belong in the single pending slot and may only start after this barrage and
// its cooldown have completed.
bool NC_STACK_ypabact::StartArtilleryShellBarrage(const vec3d &targetCenter)
{
    if ( !_world )
        return false;

    int weaponId = ypabact_GetArtilleryShellWeaponId(this);
    if ( weaponId <= 0 )
    {
        if ( NC_STACK_ypabact *mountedArtilleryShell = ypabact_GetMountedArtilleryShellActor(this) )
            return mountedArtilleryShell->StartArtilleryShellBarrage(targetCenter);
        return false;
    }

    if ( !ypabact_CanUseArtilleryShell(this) )
        return false;

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);

    int shots = wproto.artillery_shell_barrage_shots;
    if ( shots <= 0 )
        return false;

    // Never redirect an in-progress barrage. Manual follow-up targets are accepted by
    // CanManualArtilleryShell() and QueueManualArtilleryShell() into the one pending
    // slot, leaving every remaining shell committed to the original target centre.
    if ( _artillery_shell_barrage_active )
        return false;

    // Fresh barrage: only allowed once the cooldown has fully elapsed.
    if ( _clock < _artillery_shell_next_activation_time )
        return false;

    // A direct fresh start wins over any stale pending order that may still exist at
    // the exact cooldown boundary (for example a new manual click before the next
    // UpdateArtilleryShell() promotes the old one). Keep one authoritative target.
    if ( _artillery_shell_has_pending )
    {
        _artillery_shell_has_pending = false;
        _world->RemovePendingArtilleryShellMarker(_gid);
    }

    _artillery_shell_barrage_active = true;
    _artillery_shell_shots_remaining = shots; // refill the cycle budget
    _artillery_shell_target_center = targetCenter;

    // Fire the first shell immediately (this also spends one budget shot and, if the
    // budget is just 1, ends the barrage and starts the cooldown right away).
    ypabact_FireArtilleryShellShotAndAdvance(this, weaponId, wproto);

    return true;
}

// Check whether this unit can ACCEPT a manual bombardment call against targetPos.
// Returns true when the target is valid (range and radar; manual control is always
// enabled for artillery shells). It does NOT reject an in-progress barrage/cooldown:
// *outReadyNow is true only for an idle platform that may start a fresh barrage now.
// Otherwise the valid order is queued as the single future target.
bool NC_STACK_ypabact::CanManualArtilleryShell(const vec3d &targetPos, int *outWeaponId, bool *outReadyNow)
{
    if ( outReadyNow )
        *outReadyNow = false;

    if ( !_world || _world->_isNetGame )
        return false;

    int weaponId = ypabact_GetArtilleryShellWeaponId(this);
    if ( weaponId <= 0 )
    {
        if ( NC_STACK_ypabact *mountedArtilleryShell = ypabact_GetMountedArtilleryShellActor(this) )
            return mountedArtilleryShell->CanManualArtilleryShell(targetPos, outWeaponId, outReadyNow);
        return false;
    }

    if ( !ypabact_CanUseArtilleryShell(this) )
        return false;

    World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);

    // Manual map-click control is always available for artillery shells (no opt-in flag).

    if ( wproto.artillery_shell_barrage_shots <= 0 )
        return false;

    // Range from this unit to the requested target point. The max of artillery_shell_range_minmax is the
    // single authoritative maximum range for both manual fire and auto-search.
    float maxRange = wproto.artillery_shell_max_range;
    if ( maxRange <= 0.0 )
        return false;

    float minRange = wproto.artillery_shell_min_range > 0.0 ? wproto.artillery_shell_min_range : 0.0;
    float distSq = (targetPos.XZ() - _position.XZ()).square();
    if ( distSq < minRange * minRange || distSq > maxRange * maxRange )
        return false;

    // Radar: the target sector must be visible to our faction. This is the only
    // visibility gate on a manual strike.
    if ( wproto.artillery_shell_requires_radar )
    {
        Common::Point cellId = World::PositionToSectorID(targetPos);
        if ( !_world->IsSector(cellId) || !_world->SectorAt(cellId).IsCanSee(_owner) )
            return false;
    }

    // An active barrage is never ready for a second area: every remaining shell stays
    // committed to its current target. A valid follow-up click is queued and shown in
    // grey until the barrage ends and its cooldown elapses.
    if ( outReadyNow )
        *outReadyNow = !_artillery_shell_barrage_active && _clock >= _artillery_shell_next_activation_time;

    if ( outWeaponId )
        *outWeaponId = weaponId;

    return true;
}

// Queue a manual strike while the current barrage is still firing or its cooldown is
// running. It shares the same one-per-platform pending state used by automatic
// acquisition; another manual follow-up replaces only that future target immediately.
// It never redirects the active barrage or fires early.
void NC_STACK_ypabact::QueueManualArtilleryShell(const vec3d &targetPos)
{
    if ( ypabact_GetArtilleryShellWeaponId(this) <= 0 )
    {
        if ( NC_STACK_ypabact *mountedArtilleryShell = ypabact_GetMountedArtilleryShellActor(this) )
        {
            mountedArtilleryShell->QueueManualArtilleryShell(targetPos);
            return;
        }
    }

    if ( !_world )
        return;

    int weaponId = ypabact_GetArtilleryShellWeaponId(this);
    if ( weaponId <= 0 || (size_t)weaponId >= _world->GetWeaponsProtos().size() )
        return;

    ypabact_QueueArtilleryShellTarget(this, _world->GetWeaponsProtos().at(weaponId), targetPos);
}
