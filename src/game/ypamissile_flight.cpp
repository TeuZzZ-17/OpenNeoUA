// Special flight models: arc grenades, artillery shells and homing bombs.
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "game/yw.h"
#include "game/ypamissile.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "system/inivals.h"
#include "common/log.h"
#include <algorithm>
#include <math.h>
#include "game/ypamissile_internal.h"

vec3d NC_STACK_ypamissile::CalcForceVector()
{
    _thraction = _force;

    return vec3d::Normalise(  _fly_dir * _fly_dir_length * _airconst
                            + _target_dir * _thraction
                            - vec3d(0.0, _mass * 9.80665, 0.0));
}

void NC_STACK_ypamissile::SetupArcGrenadeVelocity(const vec3d &velocity, float gravity)
{
    _arcGrenadeGravity = (std::isfinite(gravity) && gravity > 0.0f)
                             ? std::min(gravity, 1000.0f)
                             : 9.80665f;

    _arcGrenadeVelocity = velocity;
    if ( !std::isfinite(_arcGrenadeVelocity.x) ||
         !std::isfinite(_arcGrenadeVelocity.y) ||
         !std::isfinite(_arcGrenadeVelocity.z) )
    {
        _arcGrenadeVelocity = vec3d(0.0, 0.0, 0.0);
    }

    _fly_dir_length = _arcGrenadeVelocity.length();
    if ( _fly_dir_length > 0.001f )
    {
        _fly_dir = _arcGrenadeVelocity / _fly_dir_length;
        AlignMissile();
    }
}

void NC_STACK_ypamissile::SetupArcGrenadeLaunch(float angleDegrees,
                                                 float gravity,
                                                 float startSpeed)
{
    vec3d aimDir = _fly_dir;
    if ( aimDir.normalise() <= 0.001f )
        aimDir = _rotation.AxisZ();
    if ( aimDir.normalise() <= 0.001f )
        aimDir = vec3d::OZ(1.0f);

    const float speed = (std::isfinite(startSpeed) && startSpeed > 0.0f)
                            ? startSpeed
                            : 0.0f;
    const float angle = (std::isfinite(angleDegrees) && angleDegrees > 0.0f)
                            ? std::min(angleDegrees, 89.0f)
                            : 0.0f;

    vec3d launchDir = aimDir;
    if ( angle > 0.0f )
    {
        // Preserve the normal Weapon aim azimuth, but make elevation relative
        // to world horizontal/gravity. Engine +Y is down, so upward is -Y.
        vec3d horizontal(aimDir.x, 0.0f, aimDir.z);
        if ( horizontal.normalise() <= 0.001f )
        {
            const vec3d fallback = _rotation.AxisZ();
            horizontal = vec3d(fallback.x, 0.0f, fallback.z);
            if ( horizontal.normalise() <= 0.001f )
                horizontal = vec3d::OZ(1.0f);
        }

        const float radians = angle * (float)C_PI_180;
        launchDir = horizontal * std::cos(radians);
        launchDir.y = -std::sin(radians);
        launchDir.normalise();
    }

    SetupArcGrenadeVelocity(launchDir * speed, gravity);
}

void NC_STACK_ypamissile::UpdateArcGrenadeBallistic(float dtime)
{
    if ( dtime <= 0.0f )
        return;

    _old_pos = _position;

    // Dedicated trajectory: no legacy grenade thrust and no target steering.
    // Gravity is an authored downward acceleration applied every frame.
    _arcGrenadeVelocity.y += _arcGrenadeGravity * dtime;

    // Preserve the generic Weapon air resistance as optional linear drag.
    if ( _airconst > 0.0f )
    {
        const float mass = _mass > 0.001f ? _mass : 1.0f;
        const float dragFactor = std::max(0.0f, 1.0f - (_airconst / mass) * dtime);
        _arcGrenadeVelocity *= dragFactor;
    }

    // Missile movement in UA uses the historical x6 world-distance scale.
    _position += _arcGrenadeVelocity * (dtime * 6.0f);

    _fly_dir_length = _arcGrenadeVelocity.length();
    if ( _fly_dir_length > 0.001f )
    {
        _fly_dir = _arcGrenadeVelocity / _fly_dir_length;
        AlignMissile();
    }

    CorrectPositionInLevelBox(nullptr);
}

bool ypamissile_IsHomingBombProto(const World::TWeapProto &wproto)
{
    return wproto.IsHomingBomb();
}

bool ypamissile_IsHomingBomb(NC_STACK_ypamissile *missile)
{
    if ( !missile || !missile->getBACT_pWorld() || missile->_vehicleID < 0 )
        return false;

    std::vector<World::TWeapProto> &weapons = missile->getBACT_pWorld()->GetWeaponsProtos();
    if ( (size_t)missile->_vehicleID >= weapons.size() )
        return false;

    return ypamissile_IsHomingBombProto(weapons.at(missile->_vehicleID));
}

bool ypamissile_HasHomingBombTarget(NC_STACK_ypamissile *missile)
{
    if ( !ypamissile_IsHomingBomb(missile) )
        return false;

    if ( missile->_primTtype == BACT_TGT_TYPE_CELL )
        return true;

    if ( missile->_primTtype != BACT_TGT_TYPE_UNIT || !missile->_primT.pbact )
        return false;

    NC_STACK_ypabact *target = missile->_primT.pbact;
    return target->_status != BACT_STATUS_DEAD &&
           target->_status != BACT_STATUS_CREATE &&
           target->_energy > 0 &&
           !target->IsDestroyed() &&
           !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_NORENDER));
}

void NC_STACK_ypamissile::SteerHomingBombDirection(float dtime)
{
    vec3d desired = _target_dir;
    if ( desired.normalise() <= 0.001 )
        return;

    vec3d current = _fly_dir;
    if ( current.normalise() <= 0.001 )
        current = _rotation.AxisZ();

    if ( current.normalise() <= 0.001 )
    {
        _fly_dir = desired;
        return;
    }

    // mat3x3::AxisAngle() uses the engine's inverted-sine convention.
    // With Transform(), desired x current is the axis that rotates the current
    // flight direction toward the target instead of away from it.
    vec3d axis = desired * current;
    if ( axis.normalise() <= 0.001 )
        return;

    float rotAngle = clp_acos(current.dot(desired));
    float maxAngle = _maxrot * dtime;

    if ( maxAngle > 0.0 && rotAngle > maxAngle )
        rotAngle = maxAngle;

    if ( fabs(rotAngle) > BOMB_MIN_ANGLE )
        _fly_dir = mat3x3::AxisAngle(axis, rotAngle).Transform(_fly_dir);

    _fly_dir.normalise();
}

int NC_STACK_ypamissile::SetupArtilleryShell(const vec3d &startPos, const vec3d &targetPos,
                                                  float arcHeight, float shellSpeed, bool impactOnSurface)
{
    _isArtilleryShellProjectile = true;
    _artilleryShellVerticalBarrage = false;
    _artilleryShellVerticalTransferred = false;
    _artilleryShellVerticalApexOffset = vec3d(0.0, 0.0, 0.0);
    _artilleryShellVerticalAscentTime = 0;
    _artilleryShellVerticalFallStartTime = 0;
    _artilleryShellVerticalDescentTime = 0;
    _artilleryShellStartPos     = startPos;
    _artilleryShellTargetPos    = targetPos;
    _artilleryShellArcHeight    = (std::isfinite(arcHeight) && arcHeight > 0.0f) ? arcHeight : 0.0f;
    _artilleryShellElapsed      = 0;
    _artilleryShellImpactOnSurface = impactOnSurface;

    // Derive flight time from the authored artillery speed. The trajectory itself is
    // unchanged: sample its real parabolic path so artillery_shell_speed remains an
    // intuitive world-units/second control instead of another fixed time parameter.
    int flightTime = 2500;
    if ( std::isfinite(shellSpeed) && shellSpeed > 0.0f )
    {
        const int SEGMENTS = 16;
        float pathLength = 0.0f;
        vec3d prev = startPos;

        for ( int i = 1; i <= SEGMENTS; ++i )
        {
            float t = (float)i / (float)SEGMENTS;
            vec3d cur;
            cur.x = startPos.x + (targetPos.x - startPos.x) * t;
            cur.z = startPos.z + (targetPos.z - startPos.z) * t;
            float baseY = startPos.y + (targetPos.y - startPos.y) * t;
            float arc = _artilleryShellArcHeight * 4.0f * t * (1.0f - t);
            cur.y = baseY - arc;
            pathLength += (cur - prev).length();
            prev = cur;
        }

        if ( pathLength > 0.0f && std::isfinite(pathLength) )
            flightTime = std::max(1, (int)std::ceil((pathLength / shellSpeed) * 1000.0f));
    }

    _artilleryShellFlightTime = flightTime;
    _position = startPos;
    _old_pos  = startPos;

    // Artillery shells never home; they fly a fixed parametric arc.
    _primTtype = BACT_TGT_TYPE_DRCT;

    // Initial facing toward the target zone (purely cosmetic; refreshed each frame).
    vec3d dir = targetPos - startPos;
    if ( dir.normalise() > 0.001 )
    {
        _fly_dir = dir;
        _rotation.SetZ(_fly_dir);
        vec3d x = vec3d::OY(-1.0) * _fly_dir; // cross product (engine: vec3d operator* = cross)
        if ( x.normalise() > 0.001 )
        {
            _rotation.SetX(x);
            _rotation.SetY(_fly_dir * x);
        }
    }

    return _artilleryShellFlightTime;
}

int NC_STACK_ypamissile::SetupArtilleryShellVerticalBarrage(const vec3d &startPos, const vec3d &targetPos,
                                                            int fallDelay, float fallHeight, float verticalSpeed,
                                                            float spreadX, float spreadZ, bool impactOnSurface)
{
    _isArtilleryShellProjectile = true;
    _artilleryShellVerticalBarrage = true;
    _artilleryShellVerticalTransferred = false;
    _artilleryShellStartPos = startPos;
    _artilleryShellTargetPos = targetPos;
    _artilleryShellArcHeight = (std::isfinite(fallHeight) && fallHeight > 0.0f) ? fallHeight : 0.0f;
    _artilleryShellElapsed = 0;
    _artilleryShellImpactOnSurface = impactOnSurface;

    // Vertical spread is authored like the normal Weapon spread: degrees. It only
    // tilts the visible ascent. The landing point and vertical descent remain
    // controlled exclusively by artillery_shell_barrage_radius/targeting.
    const float safeSpreadX = (std::isfinite(spreadX) && spreadX > 0.0f) ? std::min(spreadX, 45.0f) : 0.0f;
    const float safeSpreadZ = (std::isfinite(spreadZ) && spreadZ > 0.0f) ? std::min(spreadZ, 45.0f) : 0.0f;
    const float angleX = safeSpreadX > 0.0f
        ? ((((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f) * safeSpreadX
        : 0.0f;
    const float angleZ = safeSpreadZ > 0.0f
        ? ((((float)rand() / (float)RAND_MAX) * 2.0f) - 1.0f) * safeSpreadZ
        : 0.0f;

    _artilleryShellVerticalApexOffset = vec3d(
        tan(angleX * C_PI_180) * _artilleryShellArcHeight,
        0.0f,
        tan(angleZ * C_PI_180) * _artilleryShellArcHeight);

    // Spread is visual-only: it must not change barrage timing or the authored
    // artillery_shell_speed cadence. Ascent/descent time therefore remains based
    // on the vertical arc height exactly as with zero spread.
    const float speed = (std::isfinite(verticalSpeed) && verticalSpeed > 0.0f) ? verticalSpeed : 70.0f;
    const int travelTime = _artilleryShellArcHeight > 0.0f
        ? std::max(1, (int)std::ceil((_artilleryShellArcHeight / speed) * 1000.0f))
        : 0;

    _artilleryShellVerticalAscentTime = travelTime;
    _artilleryShellVerticalFallStartTime = std::max(std::max(fallDelay, 0), travelTime);
    _artilleryShellVerticalDescentTime = travelTime;
    _artilleryShellFlightTime = std::max(1, _artilleryShellVerticalFallStartTime + _artilleryShellVerticalDescentTime);

    _position = startPos;
    _old_pos = startPos;
    _primTtype = BACT_TGT_TYPE_DRCT;

    vec3d ascentDir(_artilleryShellVerticalApexOffset.x, -_artilleryShellArcHeight,
                    _artilleryShellVerticalApexOffset.z);
    if ( ascentDir.normalise() <= 0.001f )
        ascentDir = vec3d(0.0, -1.0, 0.0);
    _fly_dir = ascentDir;
    _rotation.SetZ(_fly_dir);
    vec3d x = vec3d::OY(-1.0) * _fly_dir;
    if ( x.normalise() <= 0.001f )
        x = vec3d::OX(1.0);
    _rotation.SetX(x);
    _rotation.SetY(_fly_dir * x);

    return _artilleryShellFlightTime;
}

void NC_STACK_ypamissile::UpdateArtilleryShellBallistic(update_msg *arg)
{
    if ( _status != BACT_STATUS_NORMAL )
        return;

    _artilleryShellElapsed += arg->frameTime;

    int flightTime = _artilleryShellFlightTime > 0 ? _artilleryShellFlightTime : 2500;
    bool impactNow = _artilleryShellElapsed >= flightTime;
    vec3d prevPos = _position;
    vec3d pos = _position;

    if ( _artilleryShellVerticalBarrage )
    {
        // Mortar-style mode: the physical projectile first climbs straight above the
        // launcher, waits there if fall_delay is longer than the ascent, then the same
        // projectile is transferred above the target and falls vertically.
        if ( _artilleryShellElapsed < _artilleryShellVerticalFallStartTime )
        {
            float ascentT = 1.0f;
            if ( _artilleryShellVerticalAscentTime > 0 )
                ascentT = ypamissile_Clamp01((float)_artilleryShellElapsed / (float)_artilleryShellVerticalAscentTime);

            pos = _artilleryShellStartPos + _artilleryShellVerticalApexOffset * ascentT;
            pos.y -= _artilleryShellArcHeight * ascentT;
        }
        else
        {
            if ( !_artilleryShellVerticalTransferred )
            {
                // This is intentionally the same projectile, not a second spawned shell.
                // Reset tracer history so the transfer never draws a giant horizontal line
                // from launcher apex to target apex.
                _artilleryShellVerticalTransferred = true;
                _position = vec3d(_artilleryShellTargetPos.x,
                                  _artilleryShellTargetPos.y - _artilleryShellArcHeight,
                                  _artilleryShellTargetPos.z);
                _old_pos = _position;
                StartWeaponTracer();
                prevPos = _position;
            }

            float descentT = 1.0f;
            if ( _artilleryShellVerticalDescentTime > 0 )
            {
                descentT = ypamissile_Clamp01(
                    (float)(_artilleryShellElapsed - _artilleryShellVerticalFallStartTime) /
                    (float)_artilleryShellVerticalDescentTime);
            }

            pos.x = _artilleryShellTargetPos.x;
            pos.z = _artilleryShellTargetPos.z;
            pos.y = (_artilleryShellTargetPos.y - _artilleryShellArcHeight) +
                    _artilleryShellArcHeight * descentT;
        }
    }
    else
    {
        float t = ypamissile_Clamp01((float)_artilleryShellElapsed / (float)flightTime);

        // Horizontal interpolation start -> target.
        pos.x = _artilleryShellStartPos.x + (_artilleryShellTargetPos.x - _artilleryShellStartPos.x) * t;
        pos.z = _artilleryShellStartPos.z + (_artilleryShellTargetPos.z - _artilleryShellStartPos.z) * t;

        // Vertical: straight-line baseline + parabolic arc.
        // Engine convention: +Y is DOWN, so "up" means subtracting from Y.
        float baseY = _artilleryShellStartPos.y + (_artilleryShellTargetPos.y - _artilleryShellStartPos.y) * t;
        float arc   = _artilleryShellArcHeight * 4.0f * t * (1.0f - t); // peak == arc_height at t = 0.5
        pos.y = baseY - arc;

    }

    _old_pos  = prevPos;
    _position = pos;

    // Orient the model along its current travel direction (visual only).
    vec3d vel = _position - prevPos;
    if ( vel.normalise() > 0.001 )
    {
        _fly_dir = vel;
        _rotation.SetZ(_fly_dir);
        vec3d x = vec3d::OY(-1.0) * _fly_dir;
        if ( x.normalise() > 0.001 )
        {
            _rotation.SetX(x);
            _rotation.SetY(_fly_dir * x);
        }
    }

    CorrectPositionInLevelBox(nullptr);

    if ( !impactNow )
        return;

    // The dedicated artillery trajectory bypasses the normal missile AI path,
    // therefore it never reaches the usual TubeCollisionTest(). Reuse that exact
    // direct-hit path on the terminal flight segment before the timed explosion:
    // a physically intersected unit receives `energy`, is remembered as the direct
    // hit, and Impact() below will then exclude it from blast_energy to avoid
    // double damage. If no unit is intersected, the shell simply detonates at its
    // authored landing/airburst point as before.
    NC_STACK_ypabact *directHit = nullptr;
    bool deflected = false;
    TubeCollisionTest(true, &directHit, &deflected);
    if ( deflected )
        return;

    if ( directHit && directHit->_bact_type == BACT_TYPES_MISSLE )
    {
        DetonateWeaponCollision(dynamic_cast<NC_STACK_ypamissile *>(directHit));
        return;
    }

    // Ground-burst artillery shells land on a point previously snapped to world
    // collision geometry. Reacquire that same real surface at impact time so
    // ground decals receive valid, short-lived skeleton/poly data; airbursts
    // intentionally have no world-hit context.
    bool hasGroundDecalChainFX = false;
    for (const World::TChainFXConfig &fx : _chainFX)
    {
        if ( fx.mode == World::TChainFXConfig::MODE_GROUND_DECAL &&
             fx.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD )
        {
            hasGroundDecalChainFX = true;
            break;
        }
    }

    if ( _artilleryShellImpactOnSurface && !_world->_isNetGame && hasGroundDecalChainFX )
    {
        ypaworld_arg136 groundHit;
        groundHit.stPos = vec3d(_position.x, -30000.0, _position.z);
        groundHit.vect = vec3d(0.0, 50000.0, 0.0);
        groundHit.flags = 0;
        _world->ypaworld_func136(&groundHit);

        if ( groundHit.isect && std::fabs(groundHit.isectPos.y - _position.y) <= 5.0 )
            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_IMPACT_WORLD,
                                  &groundHit);
    }

    // Timed impact: reuse the same path a normal bomb uses on contact for blast
    // damage/push, building/sector damage, VP dead/megadeth and chain FX. The artillery shell
    // F10 blast rings are suppressed in Impact() so they do not look like target
    // markers in the 3D view.
    bool applySectorDamage = (!(_mislFlags & FLAG_MISL_IGNOREBUILDS) ||
                              (_pSector && _pSector->PurposeType == cellArea::PT_NONE)) &&
                             (_world->_userRobo->_owner == _owner || !_world->_isNetGame);
    vec3d directDamagePos = _position;

    if ( applySectorDamage )
    {
        RememberDirectHitBuildingAt(directDamagePos);
        RememberDirectHitSectorAt(directDamagePos);
    }

    Impact();

    _status = BACT_STATUS_DEAD;

    setState_msg arg78;
    arg78.setFlags   = BACT_STFLAG_DEATH2;
    arg78.unsetFlags = 0;
    arg78.newStatus  = BACT_STATUS_NOPE;

    SetState(&arg78);

    if ( applySectorDamage )
    {
        yw_arg129 v25;
        v25.pos.x    = directDamagePos.x;
        v25.pos.z    = directDamagePos.z;
        v25.field_10 = _energy;
        v25.unit     = _mislEmitter;

        ChangeSectorEnergy(&v25);
    }

    ResetViewing();
}
