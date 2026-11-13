// OpenNeoUA Orbiter ("model = orbiter"): guardian projectiles carried by a unit
// through "orbiter_weapon = <weapon id>" in its vehicle script.
//
//  Carrier side (NC_STACK_ypabact): keeps up to orbiter_count orbiters alive,
//  replacing a spent one after orbiter_respawn_time, and sends the guarding ones
//  against an enemy unit (EngageOrbiters):
//    - AI: when the carrier fires its weapon at an enemy unit or hits one with its mgun;
//    - player: when the player hits an enemy unit with the weapon, laser or mgun.
//  Projectile side (NC_STACK_ypamissile): fade-in, guard orbit, siege dance and
//  strikes. The flight math is the pure module world/orbiter_motion.h.
//
// Strikes reuse the existing hit path (ApplyDirectHitToBact + the armor
// penetration hit FX/sound); the last strike reuses the normal Impact()/DEAD
// detonation. Local games only, like proximity_defense: no orbiter is spawned
// in a network game, so nothing can desync.
#include "game/ypamissile_internal.h"

namespace
{

// The Weapon a carrier points to, or nullptr when the link is off/invalid.
const World::TWeapProto *ypamissile_GetOrbiterProto(NC_STACK_ypabact *carrier)
{
    if ( !carrier || carrier->_orbiter_weapon <= 0 || !carrier->getBACT_pWorld() )
        return nullptr;

    auto &protos = carrier->getBACT_pWorld()->GetWeaponsProtos();
    if ( (size_t)carrier->_orbiter_weapon >= protos.size() )
        return nullptr;

    const World::TWeapProto &proto = protos.at(carrier->_orbiter_weapon);
    return proto.IsOrbiter() ? &proto : nullptr;
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

// ---------------------------------------------------------------------------
// Carrier side
// ---------------------------------------------------------------------------

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

    int alive = 0;
    for (NC_STACK_ypamissile *misl : _missiles_list)
        if ( misl->IsOrbiterAlive() )
            alive++;

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
    orbiter->BeginOrbiterGuard(this, *proto);

    SFXEngine::SFXe.startSound(&orbiter->_soundcarrier, World::TWeapProto::SND_LAUNCH);
}

void NC_STACK_ypabact::EngageOrbiters(NC_STACK_ypabact *enemy)
{
    if ( _orbiter_weapon <= 0 || !ypamissile_IsOrbiterEnemy(this, enemy) )
        return;

    for (NC_STACK_ypamissile *misl : _missiles_list)
        if ( misl->IsOrbiterGuarding() )
            misl->BeginOrbiterSiege(enemy);
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
    if ( _orbiterState == ORBITER_OFF || _orbiterFadeIn <= 0 ||
         _orbiterFadeElapsed >= _orbiterFadeIn )
        return 1.0f;

    return std::max(0.0f, (float)_orbiterFadeElapsed / (float)_orbiterFadeIn);
}

void NC_STACK_ypamissile::BeginOrbiterGuard(NC_STACK_ypabact *carrier, const World::TWeapProto &proto)
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
    _primTtype = BACT_TGT_TYPE_NONE; // orbiters never use the vanilla guidance target

    // The orbit can never go inside the carrier (body + orbiter + margin).
    const float minRadius = ypamissile_OrbiterBodyRadius(carrier) + _radius + 10.0f;
    const float radius = cfg.guard_radius_max > 0.0f
        ? ypamissile_RollFloat(cfg.guard_radius_min, cfg.guard_radius_max)
        : minRadius * 1.6f;
    _orbiterGuard = World::Orbiter::MakeGuard(_orbiterRng, radius, minRadius);

    // Appear directly on the orbit, then fade in there.
    _position = carrier->GetBodyPosition() + World::Orbiter::GuardOffset(_orbiterGuard);
    _old_pos = _position;
    _fly_dir_length = 0.0f;
}

bool NC_STACK_ypamissile::BeginOrbiterSiege(NC_STACK_ypabact *target)
{
    NC_STACK_ypabact *carrier = _world->FindLiveBactByGid(_orbiterCarrierGid);
    if ( !IsOrbiterGuarding() || !ypamissile_IsOrbiterEnemy(carrier, target) )
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
        ypamissile_RollInt(cfg.first_strike_delay_min, cfg.first_strike_delay_max) * 0.001f,
        ypamissile_RollInt(cfg.strikes_min, cfg.strikes_max),
        cfg.strike_interval_min * 0.001f, cfg.strike_interval_max * 0.001f);

    _orbiterState = ORBITER_SIEGE;
    _orbiterTargetGid = target->_gid;
    _orbiterStrikeIndex = 0;
    _orbiterSiegeElapsed = 0;
    // life_time is the safety limit of one siege (target too fast to reach).
    _orbiterSiegeTimeLimit = std::max(1000, proto.RollLifeTime());
    return true;
}

void NC_STACK_ypamissile::UpdateOrbiterGuard(NC_STACK_ypabact *carrier, float dt)
{
    _orbiterGuard.time += dt;
    const vec3d center = carrier->GetBodyPosition();
    const vec3d desired = center + World::Orbiter::GuardOffset(_orbiterGuard);

    // Smooth follow: also brings a returning orbiter back onto its orbit.
    _position += (desired - _position) * (1.0 - exp(-4.0 * dt));

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

void NC_STACK_ypamissile::OrbiterStrike(NC_STACK_ypabact *target, bool final)
{
    const World::TWeapProto &proto = _world->GetWeaponsProtos().at(_vehicleID);

    // Cascade: strike N deals energy * energy_mult^N (1 = always the same).
    const double energy = (double)_orbiterBaseEnergy *
                          std::pow((double)proto.orbiter.energy_mult, (double)_orbiterStrikeIndex);
    _energy = (int)std::max(0.0, std::min(energy + 0.5, 100000000.0));
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
        }
        else
        {
            _orbiterSiegeElapsed += frameTime;
            if ( _orbiterSiegeElapsed > _orbiterSiegeTimeLimit )
            {
                OrbiterDetonate();
                return;
            }

            const World::Orbiter::SiegeEvent event = World::Orbiter::StepSiege(
                _orbiterSiege, dt, target->_position, ypamissile_OrbiterBodyRadius(target));
            _position = _orbiterSiege.pos;
            _fly_dir = _orbiterSiege.dir;
            _fly_dir_length = _orbiterSiege.speed / 6.0f;

            if ( event != World::Orbiter::SIEGE_NONE )
            {
                // Sector membership must match the strike point (Impact() uses it).
                _world->ypaworld_func145(this);
                OrbiterStrike(target, event == World::Orbiter::SIEGE_FINAL_STRIKE);
                if ( _orbiterState == ORBITER_OFF )
                    return;
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
