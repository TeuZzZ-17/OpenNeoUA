// OpenNeoUA Orbiter ("model = orbiter"): guardian projectiles carried by a unit
// through "orbiter_weapon = <weapon id>" in its vehicle script.
//
//  Carrier side (NC_STACK_ypabact): keeps up to orbiter_count orbiters alive,
//  replacing a spent one after orbiter_respawn_time. Firing the primary weapon at
//  an enemy unit (AI, or the player with a locked target) starts an attack
//  (EngageOrbiters): after orbiter_strike_first_delay the guarding orbiters are
//  launched all at once, or one every shot_time of the orbiter Weapon.
//  Projectile side (NC_STACK_ypamissile): fade-in and guard, then a launch that
//  turns the orbiter into a classic guided projectile (every generic projectile key
//  works). With orbiter_strike_count = 1 it ends with the classic impact; with more
//  strikes the first unit hit becomes strike 1 and the orbiter goes on through the
//  body, dances around that unit and dives through it until the last strike.
//  The flight math is the pure module world/orbiter_motion.h.
//
// Strikes reuse the existing hit path (ApplyDirectHitToBact + the armor
// penetration hit FX/sound); the last strike reuses the normal Impact()/DEAD
// detonation. Local games only, like radial_shot: no orbiter is spawned
// in a network game, so nothing can desync.
#include "game/ypamissile_internal.h"

namespace
{

// The Weapon a carrier points to, or nullptr when the link is off/invalid.
const World::TWeapProto *ypamissile_GetOrbiterProto(NC_STACK_ypabact *carrier)
{
    if ( !carrier || carrier->_orbiter_weapon <= 0 || !carrier->getBACT_pWorld() )
        return nullptr;

    const World::TWeapProto *proto = carrier->getBACT_pWorld()->FindWeaponProto(carrier->_orbiter_weapon);
    return proto && proto->IsOrbiter() ? proto : nullptr;
}

bool ypamissile_IsDyingOrDead(NC_STACK_ypabact *unit)
{
    return !unit || unit->_status == BACT_STATUS_DEAD ||
           (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

// Same rule for AI and player: only a live enemy unit (never an ally, a
// projectile, a building or the neutral owner) can be besieged.
bool ypamissile_IsOrbiterEnemy(NC_STACK_ypabact *carrier, NC_STACK_ypabact *target)
{
    return carrier && target && target != carrier &&
           target->_bact_type != BACT_TYPES_MISSLE &&
           target->_owner != World::OWNER_0 &&
           target->_owner != carrier->_owner &&
           !ypamissile_IsDyingOrDead(target);
}

int ypamissile_RollInt(int minValue, int maxValue)
{
    if ( maxValue <= minValue )
        return minValue;
    return minValue + rand() % (maxValue - minValue + 1);
}

float ypamissile_RollFloat(float minValue, float maxValue)
{
    if ( maxValue <= minValue )
        return minValue;
    return minValue + (maxValue - minValue) * ((float)rand() / (float)RAND_MAX);
}

// Collision size used for keep-out distances (compound shapes included).
float ypamissile_OrbiterBodyRadius(NC_STACK_ypabact *unit)
{
    return std::max(unit->_radius, unit->GetCollisionBroadRadius());
}

// Orbiter speed in world units per second (same scale as the boomerang return).
float ypamissile_OrbiterSpeed(const World::TWeapProto &proto)
{
    return std::max(proto.start_speed, 1.0f) * 6.0f;
}

} // namespace

// First point where the segment from -> to meets the unit's real collision: its
// collision shape when it has one, otherwise its collision spheres (the same
// surface a normal projectile hits).
bool ypamissile_OrbiterSurfaceHit(NC_STACK_ypabact *target, const vec3d &from, const vec3d &to, vec3d *point)
{
    if ( !target || !point )
        return false;

    if ( target->HasCollisionShape() )
    {
        Collision::Scene *scene = target->getBACT_pWorld()->_collisionScene.get();
        Collision::Contact contact;
        if ( !scene || !scene->Trace(target, from, to, 0.0, &contact) )
            return false;
        *point = contact.point;
        return true;
    }

    std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
    target->GetCollisionSpheres(spheres, target->_position, target->_rotation, false);

    bool found = false;
    float best = 2.0f;
    for (const auto &sphere : spheres)
    {
        vec3d hit;
        float time = 2.0f;
        if ( ypamissile_GetCompoundImpactPoint(from, to, 0.0f, sphere.center, sphere.radius, &hit, &time) &&
             time < best )
        {
            best = time;
            *point = hit;
            found = true;
        }
    }
    return found;
}

// True while the point is inside the unit's collision (shape or spheres).
bool ypamissile_OrbiterInsideTarget(NC_STACK_ypabact *target, const vec3d &point)
{
    if ( !target )
        return false;

    if ( target->HasCollisionShape() )
    {
        Collision::Scene *scene = target->getBACT_pWorld()->_collisionScene.get();
        Collision::Contact contact;
        return scene && scene->Trace(target, point, point, 1.0, &contact);
    }

    std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
    target->GetCollisionSpheres(spheres, target->_position, target->_rotation, false);
    for (const auto &sphere : spheres)
    {
        if ( (point - sphere.center).length() < sphere.radius )
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Carrier side
// ---------------------------------------------------------------------------

// Running attack: launches the guarding orbiters when its delay or cadence allows.
// all_at_once launches every ready one and ends; one_by_one launches the ready one
// nearest the enemy and waits shot_time (shot_time_user when the player drives).
// No ready orbiter or a lost enemy ends the attack; the next trigger starts a new one.
static void ypamissile_UpdateOrbiterAttack(NC_STACK_ypabact *carrier, const World::TWeapProto &proto)
{
    if ( !carrier->_orbiter_attack_gid || carrier->_clock < carrier->_orbiter_next_launch_time )
        return;

    NC_STACK_ypabact *enemy = carrier->getBACT_pWorld()->FindLiveBactByGid(carrier->_orbiter_attack_gid);
    if ( ypamissile_IsOrbiterEnemy(carrier, enemy) )
    {
        const bool oneByOne = proto.orbiter.strike_mode == World::TWeaponOrbiterConfig::STRIKE_ONE_BY_ONE;
        NC_STACK_ypamissile *nearest = nullptr;
        double nearestDist = 0.0;
        for (NC_STACK_ypamissile *misl : carrier->_missiles_list)
        {
            if ( !misl->IsOrbiterGuarding() )
                continue;
            if ( !oneByOne )
            {
                misl->LaunchOrbiter(enemy);
                continue;
            }
            const double dist = (misl->_position - enemy->_position).length();
            if ( !nearest || dist < nearestDist )
            {
                nearest = misl;
                nearestDist = dist;
            }
        }

        if ( nearest && nearest->LaunchOrbiter(enemy) )
        {
            const int cadence = carrier->getBACT_inputting() ? proto.shot_time_user : proto.shot_time;
            carrier->_orbiter_next_launch_time = carrier->_clock + std::max(1, cadence);
            return;
        }
    }

    carrier->_orbiter_attack_gid = 0;
    carrier->_orbiter_next_launch_time = 0;
}

void NC_STACK_ypabact::UpdateOrbiters(update_msg *)
{
    const World::TWeapProto *proto = ypamissile_GetOrbiterProto(this);
    if ( !proto || _world->_isNetGame || _owner == World::OWNER_0 ||
         _bact_type == BACT_TYPES_MISSLE ||
         _status == BACT_STATUS_CREATE || _status == BACT_STATUS_BEAM ||
         ypamissile_IsDyingOrDead(this) )
        return;

    if ( _orbiter_count < 0 )
        _orbiter_count = ypamissile_RollInt(proto->orbiter.count_min, proto->orbiter.count_max);

    ypamissile_UpdateOrbiterAttack(this, *proto);

    // Live orbiters and the ring places they hold.
    int alive = 0;
    uint32_t usedSlots = 0;
    for (NC_STACK_ypamissile *misl : _missiles_list)
        if ( misl->IsOrbiterAlive() )
        {
            alive++;
            usedSlots |= 1u << (misl->GetOrbiterSlot() & 31);
        }

    if ( alive >= _orbiter_count )
    {
        _orbiter_next_spawn_time = 0;
        return;
    }

    // One orbiter at a time, each after its own respawn time.
    if ( _orbiter_next_spawn_time == 0 )
    {
        _orbiter_next_spawn_time = std::max(1, _clock +
            ypamissile_RollInt(proto->orbiter.respawn_time_min, proto->orbiter.respawn_time_max));
        return;
    }

    if ( _clock < _orbiter_next_spawn_time )
        return;

    _orbiter_next_spawn_time = 0;

    int slot = 0;
    while ( slot < _orbiter_count - 1 && (usedSlots & (1u << slot)) )
        slot++;

    ypaworld_arg146 spawn;
    spawn.vehicle_id = _orbiter_weapon;
    spawn.pos = GetBodyPosition();

    NC_STACK_ypamissile *orbiter = _world->ypaworld_func147(&spawn);
    if ( !orbiter )
        return;

    orbiter->ConfigureSpawnedProjectileBase(this, _host_station, _owner, _rotation.AxisZ());
    orbiter->_kidRef.Detach();
    orbiter->_parent = nullptr;
    _missiles_list.push_back(orbiter);
    orbiter->BeginOrbiterGuard(this, *proto, slot, _orbiter_count);
    // The launch sound belongs to the launch (LaunchOrbiter), not to the appearance.
}

// The only trigger: the carrier fires its primary weapon at an enemy unit. A trigger
// during a running attack only re-aims it; the first delay runs once per attack.
void NC_STACK_ypabact::EngageOrbiters(NC_STACK_ypabact *enemy)
{
    const World::TWeapProto *proto = ypamissile_GetOrbiterProto(this);
    if ( !proto || _world->_isNetGame || !ypamissile_IsOrbiterEnemy(this, enemy) )
        return;

    if ( !_orbiter_attack_gid )
        _orbiter_next_launch_time = _clock +
            ypamissile_RollInt(proto->orbiter.strike_first_delay_min, proto->orbiter.strike_first_delay_max);
    _orbiter_attack_gid = enemy->_gid;
}

// ---------------------------------------------------------------------------
// Projectile side
// ---------------------------------------------------------------------------

void NC_STACK_ypamissile::ResetOrbiterState()
{
    _orbiterState = ORBITER_OFF;
    _orbiterCarrierGid = 0;
    _orbiterTargetGid = 0;
    _orbiterFadeIn = 0;
    _orbiterFadeElapsed = 0;
    _orbiterBaseEnergy = 0;
    _orbiterStrikeIndex = 0;
    _orbiterSiegeElapsed = 0;
    _orbiterSiegeTimeLimit = 0;
}

bool NC_STACK_ypamissile::IsOrbiterAlive() const
{
    return _orbiterState != ORBITER_OFF && _status == BACT_STATUS_NORMAL;
}

bool NC_STACK_ypamissile::IsOrbiterGuarding() const
{
    return _orbiterState == ORBITER_GUARD && _status == BACT_STATUS_NORMAL &&
           _orbiterFadeElapsed >= _orbiterFadeIn;
}

float NC_STACK_ypamissile::GetRenderFadeAlpha() const
{
    if ( _artilleryShellVerticalBarrage )
        return GetArtilleryTransferFadeAlpha();

    if ( _orbiterState == ORBITER_OFF || _orbiterFadeIn <= 0 ||
         _orbiterFadeElapsed >= _orbiterFadeIn )
        return 1.0f;

    return std::max(0.0f, (float)_orbiterFadeElapsed / (float)_orbiterFadeIn);
}

void NC_STACK_ypamissile::BeginOrbiterGuard(NC_STACK_ypabact *carrier, const World::TWeapProto &proto,
                                            int slot, int slotCount)
{
    ResetOrbiterState();
    if ( !carrier )
        return;

    const World::TWeaponOrbiterConfig &cfg = proto.orbiter;
    _orbiterRng = World::Orbiter::Rng((uint32_t)rand() * 2654435761u + (uint32_t)carrier->_gid);
    _orbiterState = ORBITER_GUARD;
    _orbiterCarrierGid = carrier->_gid;
    _orbiterFadeIn = cfg.fade_in;
    _orbiterBaseEnergy = _energy;
    _orbiterSlot = std::max(0, slot);
    _orbiterSlotCount = std::max(1, slotCount);
    _primTtype = BACT_TGT_TYPE_NONE; // orbiters never use the vanilla guidance target
    _chaos_factor = 0.0f;            // chaos_factor belongs to the classic flight after the launch

    // The orbit can never go inside the carrier (body + orbiter + margin).
    const float minRadius = ypamissile_OrbiterBodyRadius(carrier) + _radius + 10.0f;
    // The ring needs one radius for every place: the middle of the range.
    const float radius = cfg.guard_radius_max <= 0.0f ? minRadius * 1.6f
        : cfg.HasRingGuard() ? (cfg.guard_radius_min + cfg.guard_radius_max) * 0.5f
        : ypamissile_RollFloat(cfg.guard_radius_min, cfg.guard_radius_max);
    _orbiterGuard = World::Orbiter::MakeGuard(_orbiterRng, radius, minRadius);

    // Appear directly on the orbit, then fade in there.
    _position = OrbiterGuardTarget(carrier);
    _old_pos = _position;
    _fly_dir_length = 0.0f;
}

// Guard position: the classic free orbit, or the place on the ring turned with the
// carrier's heading (ring time is the carrier clock, shared by every place).
vec3d NC_STACK_ypamissile::OrbiterGuardTarget(NC_STACK_ypabact *carrier) const
{
    const vec3d center = carrier->GetBodyPosition();
    const World::TWeaponOrbiterConfig &cfg = _world->GetWeaponsProtos().at(_vehicleID).orbiter;
    if ( !cfg.HasRingGuard() )
        return center + World::Orbiter::GuardOffset(_orbiterGuard);

    vec3d forward = carrier->_rotation.AxisZ();
    forward.y = 0.0;
    if ( forward.normalise() <= 0.001 )
        forward = vec3d(0.0, 0.0, 1.0);
    const vec3d right(forward.z, 0.0, -forward.x);

    // Only one of the two given: the absent one is 0.
    const vec3d local = World::Orbiter::RingGuardOffset(
        _orbiterGuard, _orbiterSlot, _orbiterSlotCount,
        cfg.guard_speed >= 0.0f ? cfg.guard_speed : 0.0f,
        cfg.guard_chaos >= 0.0f ? cfg.guard_chaos : 0.0f,
        carrier->_clock * 0.001f);
    return center + right * local.x + forward * local.z + vec3d(0.0, local.y, 0.0);
}

// Launch: the orbiter becomes a classic guided projectile aimed at the enemy, so every
// generic projectile key of its Weapon works (start_speed, homing_*, chaos_factor,
// drive_time, life_time, tracer, launch FX and sound, impact, blast, debuffs...).
bool NC_STACK_ypamissile::LaunchOrbiter(NC_STACK_ypabact *target)
{
    NC_STACK_ypabact *carrier = _world->FindLiveBactByGid(_orbiterCarrierGid);
    if ( !IsOrbiterGuarding() || !ypamissile_IsOrbiterEnemy(carrier, target) )
        return false;

    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(_vehicleID);
    _orbiterState = ORBITER_FLIGHT;
    _orbiterTargetGid = target->_gid;
    _orbiterStrikeCount = ypamissile_RollInt(proto.orbiter.strike_count_min, proto.orbiter.strike_count_max);
    _orbiterStrikeIndex = 0;

    vec3d dir = target->_position - _position;
    if ( dir.normalise() <= 0.001 )
        dir = carrier->_rotation.AxisZ();

    SetMissileType(MISL_TARGETED);
    SetHomingTime(proto.homing_time);
    SetHomingForce(proto.homing_force);
    SetDriveTime(proto.drive_time);
    SetLifeTime(proto.RollLifeTime());
    _chaos_factor = proto.chaos_factor;
    ResetProjectileVisualMotionFreeze();
    SetLauncherBact(carrier);
    SetStartHeight(_position.y);
    _mislDirectHitUnits.clear();

    _fly_dir = dir;
    _fly_dir_length = carrier->_fly_dir_length + proto.start_speed;
    _mislHomingExitDirection = dir;
    // Level frame built on the launch direction: the carrier's X axis is not
    // perpendicular to it from a side ring place, and would shear the shard.
    vec3d x = vec3d::OY(-1.0) * dir;
    if ( x.normalise() <= 0.001 )
        x = carrier->_rotation.AxisX();
    _rotation.SetZ(dir);
    _rotation.SetX(x);
    _rotation.SetY(dir * x);
    _old_pos = _position;

    setTarget_msg tgt;
    tgt.tgt.pbact = target;
    tgt.tgt_type = BACT_TGT_TYPE_UNIT;
    tgt.priority = 0;
    tgt.tgt_pos = target->_position;
    SetTarget(&tgt);

    StartWeaponTracer();
    carrier->StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_LAUNCH, nullptr,
                                   &proto.chain_fx, &_position, &_rotation);
    SFXEngine::SFXe.startSound(&_soundcarrier, World::TWeapProto::SND_LAUNCH);
    return true;
}

// Classic flight hit a unit. With one strike the classic impact follows (false). With
// more strikes this hit is strike 1: its entry hit has just been applied by the classic
// hit path, so the orbiter goes on through the body to its exit hit and then dances
// around the unit for the remaining strikes.
bool NC_STACK_ypamissile::ContinueOrbiterSiege(NC_STACK_ypabact *target)
{
    if ( _orbiterState != ORBITER_FLIGHT || _orbiterStrikeCount <= 1 )
        return false;

    NC_STACK_ypabact *carrier = _world->FindLiveBactByGid(_orbiterCarrierGid);
    if ( ypamissile_IsDyingOrDead(carrier) || !ypamissile_IsOrbiterEnemy(carrier, target) )
        return false;

    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(_vehicleID);
    const World::TWeaponOrbiterConfig &cfg = proto.orbiter;

    // The dance shell always stays clear of the target body.
    const float minRadius = ypamissile_OrbiterBodyRadius(target) * 1.5f + _radius;
    const float rolled = cfg.siege_radius_max > 0.0f
        ? ypamissile_RollFloat(cfg.siege_radius_min, cfg.siege_radius_max)
        : std::max(ypamissile_OrbiterBodyRadius(target) * 2.5f, 60.0f);

    vec3d dir = _fly_dir;
    if ( dir.normalise() <= 0.001 )
        dir = target->_position - _position;

    World::Orbiter::BeginSiege(
        _orbiterSiege, _orbiterRng, _position, dir, target->_position,
        ypamissile_OrbiterSpeed(proto), std::max(rolled, minRadius),
        ypamissile_RollInt(cfg.strike_interval_min, cfg.strike_interval_max) * 0.001f,
        _orbiterStrikeCount - 1,
        cfg.strike_interval_min * 0.001f, cfg.strike_interval_max * 0.001f);

    // Entry hit look (hit sound, detonate FX) while the orbiter flies on.
    ApplyArmorPenetrationUnitImpactFX(&_position);

    _orbiterState = ORBITER_SIEGE;
    _mislType = MISL_ORBITER;
    _chaos_factor = 0.0f;
    _orbiterTargetGid = target->_gid;
    _orbiterSiegeElapsed = 0;
    _orbiterInside = true;
    _orbiterPassStruck = true;
    _orbiterFinalPass = false;
    _orbiterExiting = true;
    _orbiterExitTravel = 0.0f;
    // life_time is the safety limit of one siege (target too fast to reach).
    _orbiterSiegeTimeLimit = std::max(1000, proto.RollLifeTime());
    return true;
}

void NC_STACK_ypamissile::LeaveOrbiterFlight()
{
    if ( _orbiterState == ORBITER_FLIGHT )
        _orbiterState = ORBITER_OFF;
}

void NC_STACK_ypamissile::UpdateOrbiterGuard(NC_STACK_ypabact *carrier, float dt)
{
    _orbiterGuard.time += dt;
    const vec3d center = carrier->GetBodyPosition();
    const vec3d desired = OrbiterGuardTarget(carrier);

    // Smooth follow: also brings a returning orbiter back onto its orbit. An ordered
    // ring follows tighter so it keeps its shape while the carrier moves.
    const World::TWeaponOrbiterConfig &cfg = _world->GetWeaponsProtos().at(_vehicleID).orbiter;
    const double follow = !cfg.HasRingGuard() ? 4.0
        : 9.0 - 0.5 * (double)(cfg.guard_chaos >= 0.0f ? cfg.guard_chaos : 0.0f);
    _position += (desired - _position) * (1.0 - exp(-follow * dt));

    // Hard rule: never inside the carrier, even while it moves or turns.
    vec3d out = _position - center;
    const double dist = out.normalise();
    if ( dist < _orbiterGuard.minRadius )
    {
        if ( dist <= 0.001 )
        {
            out = desired - center;
            out.normalise();
        }
        _position = center + out * _orbiterGuard.minRadius;
    }
}

void NC_STACK_ypamissile::KeepOrbiterAboveWorld()
{
    ypaworld_arg136 contact;
    contact.stPos = _old_pos;
    contact.vect = _position - _old_pos;
    contact.flags = 0;
    if ( contact.vect.length() <= 0.001 )
        return;

    TraceWorldCollision(&contact);
    if ( !contact.isect )
        return;

    // Slide up instead of hitting terrain or buildings: orbiters only hurt units.
    _position = contact.isectPos + vec3d(0.0, -10.0, 0.0);
    if ( _orbiterState == ORBITER_SIEGE )
    {
        _orbiterSiege.pos = _position;
        _orbiterSiege.dir.y = -std::max(std::fabs(_orbiterSiege.dir.y), 0.4);
        _orbiterSiege.dir.normalise();
    }
}

// One hit of a pass: entering and leaving the body are two hits with identical damage and effects
// (a strike's cascade step advances after its second hit). `final` detonates here.
void NC_STACK_ypamissile::OrbiterStrike(NC_STACK_ypabact *target, bool final, bool advance)
{
    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(_vehicleID);

    // Cascade: strike N deals energy * energy_mult^N (1 = always the same).
    const double energy = (double)_orbiterBaseEnergy *
                          std::pow((double)proto.orbiter.strike_energy_mult, (double)_orbiterStrikeIndex);
    _energy = (int)std::max(0.0, std::min(energy + 0.5, 100000000.0));
    if ( advance )
        _orbiterStrikeIndex++;
    _mislDirectHitUnits.clear();

    // Same Deflect Buff rule as a normal projectile hit.
    float multiplier = 1.0f;
    bool deflected = false;
    if ( target->HasDeflectBuff() )
    {
        if ( !target->CanBuffDeflectEnergy(_energy) )
        {
            multiplier = target->GetBuffDeflectEndEnergyMultiplier();
            target->ClearBuffDeflectCharges();
        }
        else
        {
            target->ConsumeBuffDeflectCharge();
            deflected = true;
        }
    }

    if ( !deflected )
        ApplyDirectHitToBact(target, true, multiplier);

    if ( !final )
    {
        // Classic hit look (hit sound, detonate FX) while the orbiter flies on.
        ApplyArmorPenetrationUnitImpactFX(&_position);
        return;
    }

    OrbiterDetonate();
}

// Strikes happen on the besieged unit's collision surface, like a normal weapon that
// penetrates: the dive meets the surface (entry hit: damage and every hit effect there),
// the orbiter flies through the body, and where it leaves the surface it hits again with
// the same damage and the same effects (exit hit). The last strike detonates at the exit.
// `before` is the position before this frame's step, `_position` the one after it.
void NC_STACK_ypamissile::OrbiterSurfacePass(NC_STACK_ypabact *target, const vec3d &before, bool wasDiving,
                                             World::Orbiter::SiegeEvent event)
{
    const vec3d after = _position;

    if ( !_orbiterInside )
    {
        vec3d entry;
        if ( !wasDiving || !ypamissile_OrbiterSurfaceHit(target, before, after, &entry) )
            return;

        _orbiterInside = true;
        _orbiterPassStruck = true;

        // The step may already have counted this pass (event); otherwise the pass
        // is the last one when a single strike is left.
        _orbiterFinalPass = event == World::Orbiter::SIEGE_FINAL_STRIKE ||
                            (event == World::Orbiter::SIEGE_NONE && _orbiterSiege.strikesLeft <= 1);
        _position = entry;
        _world->ypaworld_func145(this);
        OrbiterStrike(target, false, false);
        _position = after;
        _world->ypaworld_func145(this);
        return;
    }

    if ( ypamissile_OrbiterInsideTarget(target, after) )
        return;

    // Left the body: exit hit at the surface where it comes out.
    vec3d exitPoint = after;
    if ( !ypamissile_OrbiterSurfaceHit(target, after, before, &exitPoint) )
        exitPoint = after;
    OrbiterExitHit(target, exitPoint, after);
}

// Exit hit: same damage and effects as the entry hit, then the pass is over. The last
// strike detonates here with the normal impact and blast.
void NC_STACK_ypamissile::OrbiterExitHit(NC_STACK_ypabact *target, const vec3d &exitPoint, const vec3d &resumePos)
{
    const bool final = _orbiterFinalPass;
    _orbiterInside = false;
    _orbiterFinalPass = false;
    _orbiterExiting = false;
    _orbiterExitTravel = 0.0f;

    _position = exitPoint;
    _world->ypaworld_func145(this);
    OrbiterStrike(target, final, true);
    if ( _orbiterState == ORBITER_OFF )
        return;

    _position = resumePos;
    _world->ypaworld_func145(this);
}

void NC_STACK_ypamissile::OrbiterDetonate()
{
    _orbiterState = ORBITER_OFF;
    ResetViewing();
    Impact();

    setState_msg state;
    state.newStatus = BACT_STATUS_DEAD;
    state.unsetFlags = 0;
    state.setFlags = 0;
    SetState(&state);
}

void NC_STACK_ypamissile::UpdateOrbiter(update_msg *arg)
{
    const int frameTime = std::max(0, arg->frameTime);
    const float dt = frameTime * 0.001f;

    // Carrier lost: the orbiter explodes where it is.
    NC_STACK_ypabact *carrier = _world->FindLiveBactByGid(_orbiterCarrierGid);
    if ( ypamissile_IsDyingOrDead(carrier) )
    {
        OrbiterDetonate();
        return;
    }

    _mislEmitter = carrier;
    _old_pos = _position;
    if ( _orbiterFadeElapsed < _orbiterFadeIn )
        _orbiterFadeElapsed = std::min(_orbiterFadeIn, _orbiterFadeElapsed + frameTime);

    if ( _orbiterState == ORBITER_SIEGE )
    {
        NC_STACK_ypabact *target = _world->FindLiveBactByGid(_orbiterTargetGid);
        if ( !ypamissile_IsOrbiterEnemy(carrier, target) )
        {
            // Target gone: back to guarding the carrier.
            _orbiterState = ORBITER_GUARD;
            _orbiterTargetGid = 0;
            _orbiterInside = false;
            _orbiterPassStruck = false;
            _orbiterFinalPass = false;
            _orbiterExiting = false;
        }
        else
        {
            _orbiterSiegeElapsed += frameTime;
            if ( _orbiterSiegeElapsed > _orbiterSiegeTimeLimit )
            {
                OrbiterDetonate();
                return;
            }

            if ( _orbiterExiting )
            {
                // Last strike, already inside: fly straight on through the body to the exit.
                const vec3d before = _position;
                const float step = _orbiterSiege.speed * dt;
                vec3d dir = _orbiterSiege.dir;
                if ( dir.normalise() <= 0.001 )
                    dir = _fly_dir;
                _position += dir * step;
                _orbiterSiege.pos = _position;
                _fly_dir = dir;
                _fly_dir_length = _orbiterSiege.speed / 6.0f;
                _orbiterExitTravel += step;

                OrbiterSurfacePass(target, before, false, World::Orbiter::SIEGE_NONE);
                if ( _orbiterState == ORBITER_OFF )
                    return;

                // Safety: never fly on forever inside a body (huge or odd collision).
                if ( _orbiterExiting && _orbiterInside &&
                     _orbiterExitTravel > std::max(400.0f, ypamissile_OrbiterBodyRadius(target) * 4.0f) )
                {
                    OrbiterExitHit(target, _position, _position);
                    if ( _orbiterState == ORBITER_OFF )
                        return;
                }
            }
            else
            {
                const vec3d before = _position;
                const bool wasDiving = _orbiterSiege.diving;
                const World::Orbiter::SiegeEvent event = World::Orbiter::StepSiege(
                    _orbiterSiege, dt, target->_position, ypamissile_OrbiterBodyRadius(target));
                _position = _orbiterSiege.pos;
                _fly_dir = _orbiterSiege.dir;
                _fly_dir_length = _orbiterSiege.speed / 6.0f;

                // Entry hit where the dive meets the collision surface (exit hit follows later).
                OrbiterSurfacePass(target, before, wasDiving, event);
                if ( _orbiterState == ORBITER_OFF )
                    return;

                if ( event != World::Orbiter::SIEGE_NONE )
                {
                    // The pass reached the body centre.
                    if ( !_orbiterPassStruck )
                    {
                        // Fallback: the surface was never found. Entry and exit hit happen here.
                        _world->ypaworld_func145(this);
                        OrbiterStrike(target, false, false);
                        OrbiterStrike(target, event == World::Orbiter::SIEGE_FINAL_STRIKE, true);
                        if ( _orbiterState == ORBITER_OFF )
                            return;
                    }
                    else if ( event == World::Orbiter::SIEGE_FINAL_STRIKE )
                    {
                        // Last strike: go on through the body, detonate at the exit surface.
                        _orbiterExiting = true;
                        _orbiterExitTravel = 0.0f;
                    }
                    _orbiterPassStruck = false;
                }
            }
        }
    }

    if ( _orbiterState == ORBITER_GUARD )
    {
        UpdateOrbiterGuard(carrier, dt);
        vec3d moved = _position - _old_pos;
        const double length = moved.normalise();
        if ( length > 0.001 )
            _fly_dir = moved;
        _fly_dir_length = dt > 0.0f ? (float)(length / dt) / 6.0f : 0.0f;
    }

    KeepOrbiterAboveWorld();
    AlignMissile();
    _world->ypaworld_func145(this);
}
