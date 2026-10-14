// NC_STACK_ypamissile core: construction, parameters, AI layers, movement, state changes, impact and the property
// getters/setters. Feature files: ypamissile_blast.cpp (blast, push, armor penetration, direct hits),
// ypamissile_chain.cpp (chain and cluster projectiles), ypamissile_flight.cpp (arc grenade, artillery shell,
// homing bomb), ypamissile_tracer.cpp (weapon tracer visuals). Shared helpers: ypamissile_internal.h.
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

float ypamissile_Clamp01(float value)
{
    if ( value < 0.0 )
        return 0.0;

    if ( value > 1.0 )
        return 1.0;

    return value;
}

bool ypamissile_NormalizeXZ(vec3d *dir)
{
    dir->y = 0.0f;

    float len = dir->length();
    if ( !isfinite(len) || len <= 0.001f )
        return false;

    *dir /= len;
    return true;
}

NC_STACK_ypabact *ypamissile_FindLiveBactByGid(World::RefBactList &list, int32_t gid)
{
    for (NC_STACK_ypabact *unit : list)
    {
        if ( unit->_gid == gid )
        {
            if ( unit->_kidRef.IsListType(World::BLIST_CACHE) || unit->_status == BACT_STATUS_DEAD )
                return nullptr;

            return unit;
        }

        NC_STACK_ypabact *kid = ypamissile_FindLiveBactByGid(unit->_kidList, gid);
        if ( kid )
            return kid;
    }

    return nullptr;
}

size_t NC_STACK_ypamissile::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _bact_type = BACT_TYPES_MISSLE;

    _mislEmitter = nullptr;
    _mislSpecificEnergy.fill(1.0f);
    _mislSpecificEnergyDefined.fill(false);
    _mislLifeTime = 5000;
    _mislHomingTimeRemaining = -1;
    _mislDelayTime = 0;
    _mislType = MISL_BOMB;
    _mislBlastPush = 0.0f;
    ResetRuntimeBlastState();
    _mislClusterSoundCarrier.Clear();
    _mislArmorPenetrationSoundCarrier.Clear();
    _weaponTracer = World::TWeaponTracerConfig();
    _weaponTracerStarted = false;
    _weaponTracerVisualSeed = 0;
    _weaponTracerPoints.clear();

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_VIEWER:
                setBACT_viewer(val.Get<bool>());
                break;

            case MISS_ATT_LAUNCHER:
                SetLauncherBact(val.Get<NC_STACK_ypabact *>());
                break;

            case MISS_ATT_TYPE:
                SetMissileType(val.Get<int32_t>());
                break;

            case MISS_ATT_LIFETIME:
                SetLifeTime(val.Get<int32_t>());
                break;

            case MISS_ATT_DELAY:
                SetDelay(val.Get<int32_t>());
                break;

            case MISS_ATT_DRIVETIME:
                SetDriveTime(val.Get<int32_t>());
                break;

            case MISS_ATT_IGNOREBUILDS:
                SetIgnoreBuilds ( val.Get<int32_t>() );
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

size_t NC_STACK_ypamissile::Deinit()
{
    SFXEngine::SFXe.StopCarrier(&_mislClusterSoundCarrier);
    _mislClusterSoundCarrier.Clear();
    SFXEngine::SFXe.StopCarrier(&_mislArmorPenetrationSoundCarrier);
    _mislArmorPenetrationSoundCarrier.Clear();

    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_ypamissile::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_VIEWER:
                setBACT_viewer(val.Get<bool>());
                break;

            case MISS_ATT_LAUNCHER:
                SetLauncherBact(val.Get<NC_STACK_ypabact *>());
                break;

            case MISS_ATT_TYPE:
                SetMissileType(val.Get<int32_t>());
                break;

            case MISS_ATT_LIFETIME:
                SetLifeTime(val.Get<int32_t>());
                break;

            case MISS_ATT_DELAY:
                SetDelay(val.Get<int32_t>());
                break;

            case MISS_ATT_DRIVETIME:
                SetDriveTime(val.Get<int32_t>());
                break;

            case MISS_ATT_IGNOREBUILDS:
                SetIgnoreBuilds ( val.Get<int32_t>() );
                break;

            case MISS_ATT_POW_HELI:
                SetPowerHeli(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_TANK:
                SetPowerTank(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_FLYER:
                SetPowerFlyer(val.Get<int32_t>());
                break;

            case MISS_ATT_POW_ROBO:
                SetPowerRobo(val.Get<int32_t>());
                break;

            case MISS_ATT_RAD_HELI:
                SetRadiusHeli(val.Get<float>());
                break;

            case MISS_ATT_RAD_TANK:
                SetRadiusTank(val.Get<float>());
                break;

            case MISS_ATT_RAD_FLYER:
                SetRadiusFlyer(val.Get<float>());
                break;

            case MISS_ATT_RAD_ROBO:
                SetRadiusRobo(val.Get<float>());
                break;

            case MISS_ATT_STHEIGHT:
                SetStartHeight(val.Get<float>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_ypamissile::AI_layer1(update_msg *arg)
{
    _collisionOldRotation = _rotation;
    _collisionOldRotationValid = true;

    if ( !_mislClusterSoundCarrier.Sounds.empty() )
        SFXEngine::SFXe.UpdateSoundCarrier(&_mislClusterSoundCarrier);

    if ( !_mislArmorPenetrationSoundCarrier.Sounds.empty() )
        SFXEngine::SFXe.UpdateSoundCarrier(&_mislArmorPenetrationSoundCarrier);

    UpdatePendingChainJump(arg);

    if ( _status == BACT_STATUS_DEAD )
        _yls_time -= arg->frameTime;


    if ( _primTtype )
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
            _target_vec = _primT.pbact->_position - _position;
        else
            _target_vec = _primTpos - _position;
    }

    AI_layer2(arg);
}

void NC_STACK_ypamissile::AI_layer2(update_msg *arg)
{
    AI_layer3(arg);
    UpdateWeaponTracer();
}

void NC_STACK_ypamissile::Render(baseRender_msg *arg)
{
    // Preserve the complete existing projectile VP path, including particles,
    // tint, scale and projectile visual motion. The procedural tracer is queued in addition.
    NC_STACK_ypabact::Render(arg);
    RenderWeaponTracer(arg);
}

void NC_STACK_ypamissile::DeflectFromUnitCollision(NC_STACK_ypabact *target,
                                                     const vec3d &targetCenter, float targetRadius,
                                                     const vec3d &oldWeaponCenter, const vec3d &newWeaponCenter,
                                                     float weaponRadius, const vec3d *surfaceNormal)
{
    vec3d sweep = newWeaponCenter - oldWeaponCenter;
    vec3d closest = newWeaponCenter;
    const float sweepLenSq = sweep.dot(sweep);
    if ( sweepLenSq > 0.000001f )
    {
        float t = (targetCenter - oldWeaponCenter).dot(sweep) / sweepLenSq;
        t = std::max(0.0f, std::min(t, 1.0f));
        closest = oldWeaponCenter + sweep * t;
    }

    vec3d outward = surfaceNormal ? *surfaceNormal : closest - targetCenter;
    if ( outward.normalise() <= 0.001f )
    {
        outward = -_fly_dir;
        if ( outward.normalise() <= 0.001f )
            outward = vec3d::OY(-1.0);
    }

    vec3d tangent = (fabs(outward.y) < 0.9f ? vec3d::OY(1.0) : vec3d::OX(1.0)) * outward;
    if ( tangent.normalise() <= 0.001f )
        tangent = vec3d::OZ(1.0) * outward;
    if ( tangent.normalise() <= 0.001f )
        tangent = vec3d::OX(1.0);

    vec3d bitangent = outward * tangent;
    if ( bitangent.normalise() <= 0.001f )
        bitangent = vec3d::OZ(1.0);

    const float cosTheta = (float)rand() / (float)RAND_MAX;
    const float sinTheta = sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
    const float phi = ((float)rand() / (float)RAND_MAX) * 2.0f * C_PI;

    vec3d newDir = outward * cosTheta +
                   tangent * (cos(phi) * sinTheta) +
                   bitangent * (sin(phi) * sinTheta);
    if ( newDir.normalise() <= 0.001f )
        newDir = outward;

    // Move the colliding projectile sphere just outside the armor before the
    // next frame. This prevents one physical impact from spending
    // several Buff charges.
    const float separation = std::max(0.0f, targetRadius) +
                             std::max(0.0f, weaponRadius) + 1.0f;
    const vec3d separatedWeaponCenter = targetCenter + outward * separation;
    _position += separatedWeaponCenter - newWeaponCenter;
    _old_pos = _position;

    const float speed = _fly_dir_length;
    _fly_dir = newDir;
    _fly_dir_length = speed;
    _target_vec = newDir;

    setTarget_msg directTarget = {};
    directTarget.tgt_type = BACT_TGT_TYPE_DRCT;
    directTarget.tgt_pos = newDir;
    directTarget.priority = 0;
    SetTarget(&directTarget);

    if ( _mislType == MISL_ARC_GRENADE )
        _arcGrenadeVelocity = newDir * speed;

    // Artillery normally follows a fixed parametric path. Once armor physically
    // deflects it, leave that path and continue as the same live projectile.
    if ( _isArtilleryShellProjectile )
    {
        _mislLifeTime = std::max(0, _mislLifeTime - _artilleryShellElapsed);
        _isArtilleryShellProjectile = false;
    }

    _rotation.SetZ(newDir);
    vec3d x = vec3d::OY(-1.0) * newDir;
    if ( x.normalise() > 0.001f )
    {
        _rotation.SetX(x);
        _rotation.SetY(newDir * x);
    }

    if ( target )
    {
        const vec3d impactPos = targetCenter + outward * std::max(0.0f, targetRadius);
        target->SpawnBuffDeflectVisual(impactPos, _rotation);
    }
}

bool NC_STACK_ypamissile::TubeCollisionTest(bool applyDirectDamage, NC_STACK_ypabact **hitTarget, bool *deflected)
{
    if ( !applyDirectDamage )
        _mislAttachedDamageMultiplier = 1.0f;

    _mislDirectHitUnits.clear();
    if ( hitTarget )
        *hitTarget = nullptr;
    if ( deflected )
        *deflected = false;
    if (CollisionsDisabled()) return false;

    vec3d collisionSumPosition(0.0, 0.0, 0.0);
    int collisionCount = 0;
    float collisionSumRadius = 0.0;

    // Exact terminal contact is used only for compound-only vs compound-only.
    // A legacy radius on either projectile or target keeps the upstream placement.
    vec3d compoundImpactPosition(0.0, 0.0, 0.0);
    float compoundImpactTravel = 2.0f;
    bool hasCompoundImpactPosition = false;
    bool preserveLegacyImpactPosition = false;

    bool a5 = _mislEmitter->getBACT_inputting();

    if ( !a5 )
        a5 = getBACT_viewer();

    yw_130arg arg130;
    arg130.pos_x = _old_pos.x;
    arg130.pos_z = _old_pos.z;
    _world->GetSectorInfo(&arg130);

    cellArea *pCells[3];

    pCells[0] = arg130.pcell;

    arg130.pos_x = _position.x;
    arg130.pos_z = _position.z;
    _world->GetSectorInfo(&arg130);

    pCells[2] = arg130.pcell;

    if ( arg130.pcell == pCells[0] )
    {
        pCells[1] = pCells[0];
    }
    else
    {
        arg130.pos_x = (_position.x - _old_pos.x) * 0.5 + _old_pos.x;
        arg130.pos_z = (_position.z - _old_pos.z) * 0.5 + _old_pos.z;
        _world->GetSectorInfo(&arg130);

        pCells[1] = arg130.pcell;
    }

    auto legacyWeaponRadiusForTarget =
        [this](NC_STACK_ypabact *target) -> float
    {
        float radius = 0.0f;
        switch ( target->_bact_type )
        {
        case BACT_TYPES_BACT:
            radius = _mislRadiusHeli;
            break;
        case BACT_TYPES_TANK:
        case BACT_TYPES_CAR:
            radius = _mislRadiusTank;
            break;
        case BACT_TYPES_FLYER:
        case BACT_TYPES_UFO:
            radius = _mislRadiusFlyer;
            break;
        case BACT_TYPES_ROBO:
            radius = _mislRadiusRobo;
            break;
        default:
            radius = _radius;
            break;
        }

        return radius == 0.0f ? _radius : radius;
    };


    std::vector<NC_STACK_ypabact *> collisionTargets;
    for (int i = 0; i < 3; ++i)
        if (pCells[i] && (i == 0 || pCells[i] != pCells[i-1]))
            for (auto *bct : pCells[i]->unitsList) collisionTargets.push_back(bct);
    std::map<NC_STACK_ypabact *, Collision::Contact> shapeHits;
    if (_world->_collisionScene)
    {
        for (auto *bct : _world->_collisionScene->ShapeTargets(_old_pos, _position, GetCollisionBroadRadius()))
        {
            if (std::find(collisionTargets.begin(), collisionTargets.end(), bct) == collisionTargets.end())
                collisionTargets.push_back(bct);
            Collision::Contact contact;
            if (_world->_collisionScene->TraceProjectile(this, bct,
                    _collisionOldRotationValid ? _collisionOldRotation : _rotation, &contact))
                shapeHits[bct] = contact;
        }
        // A weapon with its own shape tests every nearby unit, shaped or not.
        if (HasCollisionShape())
        {
            for (auto *bct : collisionTargets)
            {
                if (bct == this || bct == _mislEmitter || bct->HasCollisionShape() ||
                    bct->_bact_type == BACT_TYPES_MISSLE)
                    continue;
                Collision::Contact contact;
                if (_world->_collisionScene->TraceProjectile(this, bct,
                        _collisionOldRotationValid ? _collisionOldRotation : _rotation, &contact))
                    shapeHits[bct] = contact;
            }
        }
        if (!shapeHits.empty())
        {
            // Compare the new contact with legacy sphere contacts as well;
            // a farther shape must not swallow a nearer ordinary target.
            std::map<NC_STACK_ypabact *, double> travel;
            std::vector<TCollisionSphereWorld> before, after;
            GetCollisionSpheres(before, _old_pos, _collisionOldRotationValid ? _collisionOldRotation : _rotation, false);
            GetCollisionSpheres(after, _position, _rotation, false);
            for (auto *target : collisionTargets)
            {
                double fraction = 2;
                auto entry = shapeHits.find(target);
                if (entry != shapeHits.end()) fraction = entry->second.fraction;
                else if (!target->HasCollisionShape() && !HasCollisionShape())
                {
                    std::vector<TCollisionSphereWorld> spheres;
                    target->GetCollisionSpheres(spheres, target->_position, target->_rotation, false);
                    for (size_t i = 0; i < std::min(before.size(), after.size()); ++i)
                        for (const auto &sphere : spheres)
                        {
                            vec3d point; float time;
                            const float radius = after[i].legacy ? legacyWeaponRadiusForTarget(target) : after[i].radius;
                            if (ypamissile_GetCompoundImpactPoint(before[i].center, after[i].center,
                                  radius, sphere.center, sphere.radius, &point, &time))
                                fraction = std::min(fraction, (double)time);
                        }
                }
                travel[target] = fraction;
            }
            std::stable_sort(collisionTargets.begin(), collisionTargets.end(), [&](NC_STACK_ypabact *a, NC_STACK_ypabact *b) {
                return travel[a] < travel[b];
            });
        }
    }
    for (NC_STACK_ypabact *bct : collisionTargets)
    {
        if (!bct || bct->CollisionsDisabled()) continue;
        if ( bct == this || bct == _mislEmitter || bct->_status == BACT_STATUS_DEAD )
            continue;

        if ( IsArmorPenetratedTarget(bct) )
            continue;

        if ( bct->_bact_type == BACT_TYPES_MISSLE )
        {
            NC_STACK_ypamissile *otherMissile = dynamic_cast<NC_STACK_ypamissile *>( bct );

            if ( CanCollideWithWeapon(otherMissile) )
            {
                if ( hitTarget )
                    *hitTarget = otherMissile;

                return true;
            }

            continue;
        }

        if (bct->_bact_type == BACT_TYPES_GUN && bct->GetEffectiveShield() >= 100.0f)
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

            if ( gun->IsRoboGun() )
                continue;
        }

        // Preserve the legacy/upstream AI friendly-collision rule for normal
        // projectiles. Artillery still detects terminal contact with same-owner
        // units so its explosion is unchanged; ApplyEnergyToBact() suppresses
        // only the resulting artillery damage to those units.
        if ( !_isArtilleryShellProjectile && !a5 &&
             bct->_owner == _mislEmitter->_owner )
            continue;

        if ( _mislEmitter->_bact_type == BACT_TYPES_GUN )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( _mislEmitter );

            if (bct->_owner == _owner)
            {
                if (gun->IsRoboGun() && !_mislEmitter->_isUnitGunChild)
                {
                    if (bct->_bact_type == BACT_TYPES_ROBO)
                        continue;

                    if (bct->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *bgun = dynamic_cast<NC_STACK_ypagun *>( bct );

                        if (bgun->IsRoboGun() && !bct->_isUnitGunChild)
                            continue;
                    }
                }
            }
        }

        // Artillery uses its own parametric trajectory and impact point; the
        // legacy bomb start-height gate is unrelated to that trajectory and
        // would incorrectly make valid artillery unit hits disappear.
        if ( !_isArtilleryShellProjectile && _mislType == MISL_BOMB &&
             bct->_position.y < _mislStartHeight )
            continue;

        if (bct->HasCollisionShape() || HasCollisionShape())
        {
            if (collisionCount > 0) break; // Keep the nearer legacy terminal contact.
            auto entry = shapeHits.find(bct);
            if (entry == shapeHits.end()) continue;
            const Collision::Contact &contact = entry->second;
            ypaworld_arg136 obstruction{};
            obstruction.stPos = _old_pos;
            obstruction.vect = _position - _old_pos;
            obstruction.flags = 0;
            _world->ypaworld_func149(&obstruction);
            if (obstruction.isect && obstruction.tVal <= contact.fraction) continue;
            const bool armorPenetrates = ShouldArmorPenetrateTarget(bct);
            float multiplier = 1.0f;
            if (bct->HasDeflectBuff())
            {
                const bool overload = !bct->CanBuffDeflectEnergy(_energy);
                if (armorPenetrates || overload)
                {
                    if (overload) multiplier = bct->GetBuffDeflectEndEnergyMultiplier();
                    bct->ClearBuffDeflectCharges();
                }
                else
                {
                    bct->ConsumeBuffDeflectCharge();
                    // The existing hemisphere response takes its normal
                    // from this actual surface contact rather than a body sphere.
                    _position = _old_pos + (_position - _old_pos) * contact.fraction;
                    DeflectFromUnitCollision(bct, contact.point, 0, _position, _position,
                        GetCollisionBroadRadius(), &contact.normal);
                    if (deflected) *deflected = true;
                    return false;
                }
            }
            if (applyDirectDamage && armorPenetrates)
            {
                ApplyDirectHitToBact(bct, true, multiplier);
                RememberArmorPenetratedTarget(bct);
                --_mislArmorPenetrationRemaining;
                ApplyArmorPenetrationUnitImpactFX(&contact.point);
                continue;
            }
            if (hitTarget) *hitTarget = bct;
            if (applyDirectDamage) ApplyDirectHitToBact(bct, true, multiplier);
            else _mislAttachedDamageMultiplier = multiplier;
            _position = contact.point;
            return true;
        }

        std::vector<TCollisionSphereWorld> targetSpheres;
        bct->GetCollisionSpheres(
            targetSpheres, bct->_position, bct->_rotation, false);
        if ( targetSpheres.empty() )
            continue;

        const bool useCompoundImpactPosition =
            HasManualCompoundCollision() &&
            !UsesLegacyRadiusCollision() &&
            bct->HasManualCompoundCollision() &&
            !bct->UsesLegacyRadiusCollision();

        bool collided = false;
        bool penetrated = false;

        if ( HasManualCompoundCollision() )
        {
            std::vector<TCollisionSphereWorld> oldWeaponSpheres;
            std::vector<TCollisionSphereWorld> newWeaponSpheres;
            const mat3x3 &oldRotation =
                _collisionOldRotationValid ? _collisionOldRotation : _rotation;

            GetCollisionSpheres(
                oldWeaponSpheres, _old_pos, oldRotation, false);
            GetCollisionSpheres(
                newWeaponSpheres, _position, _rotation, false);

            const size_t sphereCount =
                std::min(oldWeaponSpheres.size(), newWeaponSpheres.size());

            for (size_t wi = 0; wi < sphereCount && !collided; ++wi)
            {
                const float weaponRadius =
                    newWeaponSpheres[wi].legacy
                        ? legacyWeaponRadiusForTarget(bct)
                        : newWeaponSpheres[wi].radius;

                if ( weaponRadius <= 0.0f )
                    continue;

                for (const TCollisionSphereWorld &targetSphere :
                     targetSpheres)
                {
                    const float radiusSum =
                        weaponRadius + targetSphere.radius;
                    const float distanceSq =
                        ypamissile_SegmentSegmentDistanceSq(
                            oldWeaponSpheres[wi].center,
                            newWeaponSpheres[wi].center,
                            targetSphere.center,
                            targetSphere.center);

                    if ( distanceSq > radiusSum * radiusSum )
                        continue;

                    vec3d compoundHitPosition;
                    float compoundHitTravel = 2.0f;
                    const bool hasCompoundHitPosition =
                        useCompoundImpactPosition &&
                        ypamissile_GetCompoundImpactPoint(
                            oldWeaponSpheres[wi].center,
                            newWeaponSpheres[wi].center,
                            weaponRadius,
                            targetSphere.center,
                            targetSphere.radius,
                            &compoundHitPosition,
                            &compoundHitTravel);

                    const bool armorPenetrates = ShouldArmorPenetrateTarget(bct);
                    float deflectEndDamageMultiplier = 1.0f;
                    if ( collisionCount == 0 && bct->HasDeflectBuff() )
                    {
                        // Armor penetration breaks Deflect outright. An over-threshold
                        // impact also breaks it, but only that ending hit receives the
                        // configured damage reduction before normal hit logic continues.
                        const bool deflectOverloaded = !bct->CanBuffDeflectEnergy(_energy);
                        if ( armorPenetrates || deflectOverloaded )
                        {
                            if ( deflectOverloaded )
                                deflectEndDamageMultiplier = bct->GetBuffDeflectEndEnergyMultiplier();
                            bct->ClearBuffDeflectCharges();
                        }
                        else
                        {
                            bct->ConsumeBuffDeflectCharge();
                            DeflectFromUnitCollision(bct, targetSphere.center, targetSphere.radius,
                                                     oldWeaponSpheres[wi].center,
                                                     newWeaponSpheres[wi].center,
                                                     weaponRadius);
                            if ( deflected )
                                *deflected = true;
                            return false;
                        }
                    }

                    if ( applyDirectDamage && armorPenetrates )
                    {
                        ApplyDirectHitToBact(bct, true, deflectEndDamageMultiplier);
                        RememberArmorPenetratedTarget(bct);
                        _mislArmorPenetrationRemaining--;
                        ApplyArmorPenetrationUnitImpactFX(
                            hasCompoundHitPosition ? &compoundHitPosition : nullptr);
                        penetrated = true;
                        collided = true;
                        break;
                    }

                    collisionSumRadius += targetSphere.radius;
                    collisionCount++;
                    collisionSumPosition += bct->_position;

                    if ( useCompoundImpactPosition &&
                         hasCompoundHitPosition )
                    {
                        if ( !hasCompoundImpactPosition ||
                             compoundHitTravel < compoundImpactTravel )
                        {
                            compoundImpactPosition = compoundHitPosition;
                            compoundImpactTravel = compoundHitTravel;
                            hasCompoundImpactPosition = true;
                        }
                    }
                    else
                    {
                        // Exact contact is optional for the modern path.
                        // If it cannot be resolved, keep the safe legacy placement.
                        preserveLegacyImpactPosition = true;
                    }

                    if ( hitTarget && !*hitTarget )
                        *hitTarget = bct;

                    if ( applyDirectDamage )
                        ApplyDirectHitToBact(bct, true, deflectEndDamageMultiplier);
                    else
                        _mislAttachedDamageMultiplier = deflectEndDamageMultiplier;

                    collided = true;
                    break;
                }
            }
        }
        else
        {
            const float weaponRadius =
                legacyWeaponRadiusForTarget(bct);

            for (auto it = targetSpheres.rbegin();
                 it != targetSpheres.rend() && !collided; ++it)
            {
                const TCollisionSphereWorld &targetSphere = *it;
                vec3d compoundHitPosition;
                float compoundHitTravel = 2.0f;
                bool hasCompoundHitPosition = false;
                bool hitDetected = false;

                if ( useCompoundImpactPosition )
                {
                    // Precise surface contact is used only when both projectile
                    // and target are compound-only. Any legacy radius on either
                    // side keeps the original vanilla impact path below.
                    hasCompoundHitPosition =
                        ypamissile_GetCompoundImpactPoint(
                            _old_pos, _position, weaponRadius,
                            targetSphere.center, targetSphere.radius,
                            &compoundHitPosition, &compoundHitTravel);
                    hitDetected = hasCompoundHitPosition;
                }
                else
                {
                    // Preserve the original vanilla radius-vs-radius collision test.
                    vec3d to_enemy = targetSphere.center - _old_pos;
                    vec3d dist_vect = _position - _old_pos;

                    if ( to_enemy.dot(_rotation.AxisZ()) < 0.3 )
                        continue;

                    const float dist_vect_len = dist_vect.normalise();
                    const vec3d vp = dist_vect * to_enemy;
                    const float vp_len = vp.length();
                    const float to_enemy_len = to_enemy.length();

                    hitDetected =
                        targetSphere.radius + weaponRadius > vp_len &&
                        sqrt(POW2(dist_vect_len) + POW2(vp_len)) >
                            fabs(to_enemy_len - weaponRadius);
                }

                if ( !hitDetected )
                    continue;

                const bool armorPenetrates = ShouldArmorPenetrateTarget(bct);
                float deflectEndDamageMultiplier = 1.0f;
                if ( collisionCount == 0 && bct->HasDeflectBuff() )
                {
                    const bool deflectOverloaded = !bct->CanBuffDeflectEnergy(_energy);
                    if ( armorPenetrates || deflectOverloaded )
                    {
                        if ( deflectOverloaded )
                            deflectEndDamageMultiplier = bct->GetBuffDeflectEndEnergyMultiplier();
                        bct->ClearBuffDeflectCharges();
                    }
                    else
                    {
                        bct->ConsumeBuffDeflectCharge();
                        DeflectFromUnitCollision(bct, targetSphere.center, targetSphere.radius,
                                                 _old_pos, _position, weaponRadius);
                        if ( deflected )
                            *deflected = true;
                        return false;
                    }
                }

                if ( applyDirectDamage && armorPenetrates )
                {
                    ApplyDirectHitToBact(bct, true, deflectEndDamageMultiplier);
                    RememberArmorPenetratedTarget(bct);
                    _mislArmorPenetrationRemaining--;
                    ApplyArmorPenetrationUnitImpactFX(
                        hasCompoundHitPosition ? &compoundHitPosition : nullptr);
                    penetrated = true;
                    collided = true;
                    break;
                }

                collisionSumRadius += targetSphere.radius;
                collisionCount++;
                collisionSumPosition += bct->_position;

                if ( useCompoundImpactPosition )
                {
                    // Compound-only vs compound-only accepts the resolved surface
                    // contact and keeps the terminal impact at that exact point.
                    if ( !hasCompoundImpactPosition ||
                         compoundHitTravel < compoundImpactTravel )
                    {
                        compoundImpactPosition = compoundHitPosition;
                        compoundImpactTravel = compoundHitTravel;
                        hasCompoundImpactPosition = true;
                    }
                }
                else
                {
                    preserveLegacyImpactPosition = true;
                }

                if ( hitTarget && !*hitTarget )
                    *hitTarget = bct;

                if ( applyDirectDamage )
                    ApplyDirectHitToBact(bct, true, deflectEndDamageMultiplier);
                else
                    _mislAttachedDamageMultiplier = deflectEndDamageMultiplier;

                collided = true;
            }
        }

        if ( penetrated )
            continue;
    }

    if ( collisionCount > 0 )
    {
        if ( hasCompoundImpactPosition && !preserveLegacyImpactPosition )
        {
            // Compound-only collision uses the actual surface point. This is
            // intentionally separate from the legacy radius placement below.
            _position = compoundImpactPosition;
        }
        else
        {
            // Preserve the upstream placement whenever either side uses a legacy radius.
            _position = collisionSumPosition / (float)collisionCount;

            collisionSumRadius /= (float)collisionCount;

            if ( collisionSumRadius >= 50.0 )
            {
                vec3d posDelta = _position - _old_pos;
                float deltaLen = posDelta.length();

                if ( deltaLen < 1.0 )
                    deltaLen = 1.0;

                _position -= (posDelta / deltaLen) * collisionSumRadius;
            }
        }
    }

    return collisionCount > 0;
}

bool NC_STACK_ypamissile::CanCollideWithWeapon(
        NC_STACK_ypamissile *other)
{
    if ( !System::IniConf::GameWeaponWeaponCollision.Get<bool>() )
        return false;

    if (CollisionsDisabled() || !other || other->CollisionsDisabled()) return false;
    if ( !other || other == this || other == _mislEmitter )
        return false;

    if ( _owner == other->_owner )
        return false;

    if ( other->_mislEmitter == this )
        return false;

    if ( _mislClusterAge < 150 || other->_mislClusterAge < 150 )
        return false;

    if ( other->_mislEmitter == _mislEmitter &&
         (_mislClusterAge < 300 || other->_mislClusterAge < 300) )
        return false;

    if ( _status == BACT_STATUS_DEAD ||
         other->_status == BACT_STATUS_DEAD )
        return false;

    if ( (_status_flg | other->_status_flg) &
         (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2) )
        return false;

    if ( !_world || other->getBACT_pWorld() != _world )
        return false;

    // Exact legacy branch: when neither Weapon uses coll_*, keep the original
    // centre-line swept test and generic Weapon radius unchanged.
    if ( !HasManualCompoundCollision() &&
         !other->HasManualCompoundCollision() )
    {
        const float radiusSum = _radius + other->_radius;
        if ( radiusSum <= 0.0f )
            return false;

        const float distSq = ypamissile_SegmentSegmentDistanceSq(
            _old_pos, _position, other->_old_pos, other->_position);
        return distSq <= radiusSum * radiusSum;
    }

    std::vector<TCollisionSphereWorld> selfOld;
    std::vector<TCollisionSphereWorld> selfNow;
    std::vector<TCollisionSphereWorld> otherOld;
    std::vector<TCollisionSphereWorld> otherNow;

    const mat3x3 &selfOldRotation =
        _collisionOldRotationValid ? _collisionOldRotation : _rotation;
    const mat3x3 &otherOldRotation =
        other->_collisionOldRotationValid
            ? other->_collisionOldRotation
            : other->_rotation;

    GetCollisionSpheres(
        selfOld, _old_pos, selfOldRotation, false);
    GetCollisionSpheres(
        selfNow, _position, _rotation, false);
    other->GetCollisionSpheres(
        otherOld, other->_old_pos, otherOldRotation, false);
    other->GetCollisionSpheres(
        otherNow, other->_position, other->_rotation, false);

    const size_t selfCount = std::min(selfOld.size(), selfNow.size());
    const size_t otherCount = std::min(otherOld.size(), otherNow.size());

    for (size_t i = 0; i < selfCount; ++i)
    {
        for (size_t j = 0; j < otherCount; ++j)
        {
            const float radiusSum =
                selfNow[i].radius + otherNow[j].radius;
            if ( radiusSum <= 0.0f )
                continue;

            const float distSq = ypamissile_SegmentSegmentDistanceSq(
                selfOld[i].center, selfNow[i].center,
                otherOld[j].center, otherNow[j].center);
            if ( distSq <= radiusSum * radiusSum )
                return true;
        }
    }

    return false;
}

void NC_STACK_ypamissile::DetonateWeaponCollision(NC_STACK_ypamissile *other)
{
    if ( !other || other == this || _status == BACT_STATUS_DEAD )
        return;

    // Either projectile may own the missile camera. Return both viewers
    // before marking the projectiles dead.
    other->ResetViewing();
    ResetViewing();

    setState_msg deathState;
    deathState.unsetFlags = 0;
    deathState.setFlags = 0;
    deathState.newStatus = BACT_STATUS_DEAD;

    // Projectile contact has no payload impact or physical destruction FX.
    if ( other->_status != BACT_STATUS_DEAD )
        other->SetStateInternal(&deathState, false);

    SetStateInternal(&deathState, false);
}

const char *NC_STACK_ypamissile::GetAreaBuildingSkipReason(const cellArea &cell, int bldX, int bldY) const
{
    if ( !cell.IsGamePlaySector() || cell.PurposeType == cellArea::PT_NONE || cell.PurposeType == cellArea::PT_CONSTRUCTING )
    {
        if ( !cell.IsGamePlaySector() )
            return "non_gameplay_sector";
        if ( cell.PurposeType == cellArea::PT_NONE )
            return "no_functional_building";
        return "constructing";
    }

    if ( cell.buildings_health.Get(bldX, bldY) <= 0 )
        return "destroyed_slot";

    return nullptr;
}

const char *NC_STACK_ypamissile::GetAreaSectorSkipReason(const cellArea &cell, int bldX, int bldY) const
{
    if ( !cell.IsGamePlaySector() )
        return "non_gameplay_sector";

    if ( cell.PurposeType != cellArea::PT_NONE )
        return "functional_building_layer";

    if ( cell.buildings_health.Get(bldX, bldY) <= 0 )
        return "destroyed_slot";

    return nullptr;
}

void NC_STACK_ypamissile::AttachDelayedDetonationToTarget(NC_STACK_ypabact *target)
{
    if ( !target )
        return;

    _mislAttachedToTarget = true;
    _mislAttachTargetGid = target->_gid;
    _mislAttachOffset = _position - target->_position;
    _mislLastAttachedPosition = _position;
    _mislType = MISL_INTERNAL;
    _mislFlags |= FLAG_MISL_COUNTDELAY;
    _fly_dir_length = 0.0;
    _mislDirectPushRecipientGids.clear();

    // delay_time_debuff_id belongs to the successful attachment event, not to
    // the later explosion. Reuse the normal Debuff runtime so duration,
    // refresh, Host Station rules and all future Debuff behavior stay shared.
    if ( _world && _vehicleID >= 0 &&
         (size_t)_vehicleID < _world->GetWeaponsProtos().size() )
    {
        World::TWeapProto &wproto = _world->GetWeaponsProtos().at(_vehicleID);
        if ( wproto.delay_time_debuff.valid )
            target->ApplyDebuff(wproto.delay_time_debuff, _mislEmitter);
    }
}

void NC_STACK_ypamissile::AI_layer3(update_msg *arg)
{
    // OpenNeoUA custom: artillery shells use a fully isolated ballistic path so normal
    // missile/bomb behavior is left completely unchanged.
    if ( _isArtilleryShellProjectile )
    {
        UpdateArtilleryShellBallistic(arg);
        return;
    }

    _world->ypaworld_func145(this);

    float v40 = _target_vec.length();

    if ( _mislHomingTimeRemaining != 0 && v40 > 0.1 )
    {
        if ( _primTtype != BACT_TGT_TYPE_DRCT )
            _target_dir = _target_vec / v40;
    }

    _AI_time1 = 0;

    _thraction = _force;

    float v38 = arg->frameTime * 0.001;

    if ( _status == BACT_STATUS_NORMAL )
    {
        _mislClusterAge += arg->frameTime;

        if ( _mislHomingTimeRemaining > 0 )
        {
            if ( arg->frameTime >= _mislHomingTimeRemaining )
                _mislHomingTimeRemaining = 0;
            else
                _mislHomingTimeRemaining -= arg->frameTime;
        }

        if ( TryClusterSplit() )
            return;

        if ( _mislFlags & FLAG_MISL_COUNTDELAY)
            _mislDelayTime -= arg->frameTime;

        if ( _mislAttachedToTarget )
        {
            UpdateAttachedDetonationPosition();
            _world->ypaworld_func145(this);
        }

        if ( (_mislFlags & FLAG_MISL_COUNTDELAY)  &&  _mislDelayTime <= 0 )
        {
            bool applySectorDamage = (!(_mislFlags & FLAG_MISL_IGNOREBUILDS) || _pSector->PurposeType == cellArea::PT_NONE) &&
                                     (_world->_userRobo->_owner == _owner || !_world->_isNetGame);
            vec3d directDamagePos = _position + _fly_dir * 5.0;

            if ( applySectorDamage )
            {
                RememberDirectHitBuildingAt(directDamagePos);
                RememberDirectHitSectorAt(directDamagePos);
            }

            if ( _mislAttachedToTarget )
            {
                ApplyAttachedDirectHitDamage();
                _mislAttachedToTarget = false;
                _mislAttachTargetGid = 0;
            }

            Impact();

            _status = BACT_STATUS_DEAD;

            setState_msg arg78;
            arg78.setFlags = BACT_STFLAG_DEATH2;
            arg78.unsetFlags = 0;
            arg78.newStatus = BACT_STATUS_NOPE;

            SetState(&arg78);

            if ( applySectorDamage )
            {
                yw_arg129 v25;

                v25.pos.x = directDamagePos.x;
                v25.pos.z = directDamagePos.z;
                v25.field_10 = _energy;
                v25.unit = _mislEmitter;

                ChangeSectorEnergy(&v25);
            }
        }
        else
        {
            move_msg arg74;

            switch ( _mislType )
            {
            case MISL_BOMB:
                arg74.field_0 = v38;
                arg74.flag = _mislHomingTimeRemaining == 0 ? 2 : 1;
                if ( arg74.flag != 2 && ypamissile_HasHomingBombTarget(this) )
                {
                    if ( _force > 0.0 )
                    {
                        arg74.flag = 0;
                        arg74.vec = CalcForceVector();
                    }
                    else
                    {
                        SteerHomingBombDirection(v38);
                    }
                }
                Move(&arg74);
                break;

            case MISL_DIRECT:
                arg74.field_0 = v38;
                arg74.flag = 0;
                arg74.vec = CalcForceVector();
                Move(&arg74);
                break;

            case MISL_TARGETED:
                arg74.field_0 = v38;
                arg74.flag = _mislHomingTimeRemaining == 0 ? 2 : 0;
                if ( arg74.flag != 2 )
                    arg74.vec = CalcForceVector();
                Move(&arg74);
                break;

            case MISL_GRENADE:
                arg74.field_0 = v38;
                arg74.vec = _fly_dir;
                arg74.flag = 0;
                Move(&arg74);
                break;

            case MISL_ARC_GRENADE:
                UpdateArcGrenadeBallistic(v38);
                break;

            default:
                break;
            }

            if ( _mislType == MISL_INTERNAL )
                return;

            NC_STACK_ypabact *hitTarget = nullptr;
            bool deflected = false;
            if ( TubeCollisionTest(_mislDelayTime <= 0, &hitTarget, &deflected) )
            {
                if ( hitTarget && hitTarget->_bact_type == BACT_TYPES_MISSLE )
                {
                    DetonateWeaponCollision(dynamic_cast<NC_STACK_ypamissile *>(hitTarget));
                    return;
                }

                ResetViewing();

                if ( _mislDelayTime > 0 )
                {
                    AttachDelayedDetonationToTarget(hitTarget);
                    _mislDirectHitUnits.clear();
                    return;
                }

                setState_msg arg78;
                Impact();
                arg78.newStatus = BACT_STATUS_DEAD;
                arg78.unsetFlags = 0;
                arg78.setFlags = 0;

                SetState(&arg78);

                return;
            }

            if ( deflected )
                return;

            ypaworld_arg136 arg136;
            arg136.stPos = _old_pos;
            arg136.vect = _position - _old_pos;
            arg136.flags = 0;

            TraceWorldCollision(&arg136);

            if ( arg136.isect )
            {
                vec3d impactNormal = arg136.skel->polygons[ arg136.polyID ].Normal();

                // Ground decals consume the actual collision data before a
                // later trace can mutate a shared filler/slurp skeleton.
                StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_IMPACT_WORLD, &arg136);

                AlignMissileByNormal( impactNormal );

                _position = arg136.isectPos;

                ResetViewing();

                _mislType = MISL_INTERNAL;
                _mislFlags |= FLAG_MISL_COUNTDELAY;

                if ( !_mislDelayTime )
                {
                    bool applySectorDamage = (!(_mislFlags & FLAG_MISL_IGNOREBUILDS) || _pSector->PurposeType == cellArea::PT_NONE) &&
                                             (_world->_userRobo->_owner == _owner || !_world->_isNetGame);
                    vec3d directDamagePos = _position + _fly_dir * 5.0;

                    if ( applySectorDamage )
                    {
                        RememberDirectHitBuildingAt(directDamagePos);
                        RememberDirectHitSectorAt(directDamagePos);
                    }

                    Impact();

                    _status = BACT_STATUS_DEAD;

                    setState_msg arg78;
                    arg78.setFlags = BACT_STFLAG_DEATH2;
                    arg78.unsetFlags = 0;
                    arg78.newStatus = BACT_STATUS_NOPE;

                    SetState(&arg78);

                    if ( applySectorDamage )
                    {
                        yw_arg129 v25;

                        v25.pos.x = directDamagePos.x;
                        v25.pos.z = directDamagePos.z;
                        v25.field_10 = _energy;
                        v25.unit = _mislEmitter;

                        ChangeSectorEnergy(&v25);
                    }
                }

                int a4 = _mislEmitter->getBACT_inputting();

                if ( a4 )
                {
                    if ( _mislEmitter->IsParentMyRobo() )
                    {
                        setTarget_msg arg67;
                        arg67.tgt_type = BACT_TGT_TYPE_CELL;
                        arg67.tgt_pos = _position;
                        arg67.priority = 0;

                        _mislEmitter->SetTarget(&arg67);
                    }
                }
            }
            else
            {
                // A purely ballistic Arc Grenade has no powered drive phase.
                // Once delayed homing activates it is MISL_TARGETED and follows
                // the normal drive-time/fallout policy from that point onward.
                if ( _mislType != MISL_ARC_GRENADE )
                {
                    _mislDriveTime -= arg->frameTime;

                    if ( _mislDriveTime < 0 )
                    {
                        _mislType = MISL_BOMB;

                        _airconst = 10.0;
                        _airconst_static = 10.0;
                    }
                }

                _mislLifeTime -= arg->frameTime;

                if ( _mislLifeTime >= 0 )
                {
                    AlignMissile( arg->frameTime * 0.001 );
                }
                else
                {
                    Impact();

                    setState_msg arg78;
                    arg78.unsetFlags = 0;
                    arg78.setFlags = 0;
                    arg78.newStatus = BACT_STATUS_DEAD;

                    SetState(&arg78);

                    ResetViewing();
                }
            }
        }
    }
}

void NC_STACK_ypamissile::User_layer(update_msg *arg)
{
    _old_pos = _position;

    if (_status == BACT_STATUS_NORMAL)
        AI_layer1(arg);
    else
        ResetViewing();
}

void NC_STACK_ypamissile::Move(move_msg *arg)
{
    _old_pos = _position;

    // An expired homing timer keeps the current velocity while the shared
    // movement, collision and lifetime paths continue to run.
    if ( arg->flag != 2 )
    {
        float v8;

        if ( _status != BACT_STATUS_DEAD && _mislType != MISL_BOMB )
            v8 = _mass * 9.80665;
        else
            v8 = _mass * 39.2266;

        vec3d v26(0.0, 0.0, 0.0);

        if ( !(arg->flag & 1) )
            v26 = arg->vec * _thraction;

        vec3d vec1 = vec3d(0.0, v8, 0.0) + v26 - _fly_dir * (_fly_dir_length * _airconst);

        float v33 = vec1.normalise();

        if ( v33 > 0.0 )
        {
            vec3d v36 = _fly_dir * _fly_dir_length + vec1 * (v33 / _mass * arg->field_0);

            float v32 = v36.length();

            if ( v32 > 0.0 )
                v36 /= v32;

            _fly_dir = v36;

            _fly_dir_length = v32;
        }
    }

    _position += _fly_dir * (_fly_dir_length * arg->field_0 * 6.0);

    CorrectPositionInLevelBox(nullptr);
}

void NC_STACK_ypamissile::SetState(setState_msg *arg)
{
    SetStateInternal(arg);
}

void NC_STACK_ypamissile::Renew()
{
    NC_STACK_ypabact::Renew();

    // Missiles are recycled from the world's dead cache. Never let the
    // previous shot's fine energy snapshot leak into a new projectile before
    // ypaworld_func147() reconfigures it.
    _mislSpecificEnergy.fill(1.0f);
    _mislSpecificEnergyDefined.fill(false);

    _mislFlags  = 0;
    _mislDelayTime = 0;
    _mislHomingTimeRemaining = -1;
    _mislBlastFalloff = 0;
    _mislBlastPush = 0.0f;
    _mislDirectPush = 0.0f;
    ResetRuntimeBlastState();
    SFXEngine::SFXe.StopCarrier(&_mislClusterSoundCarrier);
    _mislClusterSoundCarrier.Clear();
    _weaponTracer = World::TWeaponTracerConfig();
    _weaponTracerStarted = false;
    _weaponTracerVisualSeed = 0;
    _weaponTracerPoints.clear();

    // OpenNeoUA custom: clear Arc Grenade ballistic state on recycle.
    _arcGrenadeVelocity = vec3d(0.0, 0.0, 0.0);
    _arcGrenadeGravity = 9.80665f;

    // OpenNeoUA custom: clear artillery shell state on recycle.
    _isArtilleryShellProjectile = false;
    _artilleryShellStartPos  = vec3d(0.0, 0.0, 0.0);
    _artilleryShellTargetPos = vec3d(0.0, 0.0, 0.0);
    _artilleryShellElapsed    = 0;
    _artilleryShellFlightTime = 0;
    _artilleryShellArcHeight  = 0.0;
    _artilleryShellImpactOnSurface = false;
    _artilleryShellVerticalBarrage = false;
    _artilleryShellVerticalTransferred = false;
    _artilleryShellVerticalApexOffset = vec3d(0.0, 0.0, 0.0);
    _artilleryShellVerticalAscentTime = 0;
    _artilleryShellVerticalFallStartTime = 0;
    _artilleryShellVerticalDescentTime = 0;

    setBACT_yourLastSeconds(3000);
}

size_t NC_STACK_ypamissile::SetStateInternal(setState_msg *arg)
{
    return SetStateInternal(arg, true);
}

size_t NC_STACK_ypamissile::SetStateInternal(setState_msg *arg, bool allowConfiguredEffects)
{
    SFXEngine::SFXe.sub_424000(&_soundcarrier, 2);
    SFXEngine::SFXe.sub_424000(&_soundcarrier, 0);
    SFXEngine::SFXe.sub_424000(&_soundcarrier, 1);

    if ( arg->newStatus )
        _status = arg->newStatus;

    if ( arg->setFlags )
        _status_flg |= arg->setFlags;

    if ( arg->unsetFlags )
        _status_flg &= ~arg->unsetFlags;

    if ( arg->newStatus == BACT_STATUS_DEAD )
    {
        FreezeProjectileVisualMotion();
        SetVP(_vp_dead);

        SFXEngine::SFXe.StopCarrier(&_mislArmorPenetrationSoundCarrier);
        SFXEngine::SFXe.startSound(&_soundcarrier, 2);

        if ( allowConfiguredEffects )
        {
            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DETONATE);
            StartDestFXByType(World::DestFX::FX_DEATH);
        }
        else
        {
            std::vector<World::TChainFXConfig> visualFX;
            for (const World::TChainFXConfig &fx : _chainFX)
            {
                if ( fx.trigger == World::TChainFXConfig::TRIGGER_DETONATE &&
                     fx.mode == World::TChainFXConfig::MODE_VISUAL )
                    visualFX.push_back(fx);
            }
            StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DETONATE,
                                  nullptr, &visualFX);
        }

        _fly_dir_length = 0;
    }

    if ( arg->newStatus == BACT_STATUS_NORMAL )
    {
        ResetProjectileVisualMotionFreeze();
        SetVP(_vp_normal);

        SFXEngine::SFXe.startSound(&_soundcarrier, 0);
    }

    if ( arg->unsetFlags == BACT_STFLAG_DEATH2 )
    {
        ResetProjectileVisualMotionFreeze();
        SetVP(_vp_normal);

        SFXEngine::SFXe.startSound(&_soundcarrier, 0);
    }

    if ( arg->setFlags == BACT_STFLAG_DEATH2 )
    {
        _status = BACT_STATUS_DEAD;

        FreezeProjectileVisualMotion();
        SetVP(_vp_megadeth);

        SFXEngine::SFXe.StopCarrier(&_mislArmorPenetrationSoundCarrier);
        SFXEngine::SFXe.startSound(&_soundcarrier, 2);

        StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_IMPACT_WORLD);
        StartDestFXByType(World::DestFX::FX_MEGADETH);

        _fly_dir_length = 0;
    }

    return 1;
}

void NC_STACK_ypamissile::ConfigureSuicideViewerReturn(
    const std::vector<int32_t> &squadGids, int32_t hostGid)
{
    _suicideViewerReturnGids = squadGids;
    _suicideViewerHostGid = hostGid;
}

bool ypamissile_IsUsableSuicideReturn(NC_STACK_ypabact *candidate)
{
    return candidate && candidate->_energy > 0 &&
           candidate->_status != BACT_STATUS_DEAD &&
           candidate->_status != BACT_STATUS_CREATE &&
           candidate->_status != BACT_STATUS_BEAM &&
           !(candidate->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) &&
           candidate->_bact_type != BACT_TYPES_MISSLE &&
           candidate->_bact_type != BACT_TYPES_GUN &&
           !candidate->ShouldHideFromStrategicUI();
}

void NC_STACK_ypamissile::ResetViewing()
{
    if ( !getBACT_viewer() )
        return;

    setBACT_viewer(false);
    setBACT_inputting(false);

    NC_STACK_ypabact *returnUnit = nullptr;
    if ( !_suicideViewerReturnGids.empty() || _suicideViewerHostGid )
    {
        for ( int32_t gid : _suicideViewerReturnGids )
        {
            NC_STACK_ypabact *candidate = _world ? _world->FindLiveBactByGid(gid) : nullptr;
            if ( ypamissile_IsUsableSuicideReturn(candidate) )
            {
                returnUnit = candidate;
                break;
            }
        }

        if ( !returnUnit && _suicideViewerHostGid && _world )
        {
            NC_STACK_ypabact *host = _world->FindLiveBactByGid(_suicideViewerHostGid);
            if ( ypamissile_IsUsableSuicideReturn(host) )
                returnUnit = host;
        }

        _suicideViewerReturnGids.clear();
        _suicideViewerHostGid = 0;
    }

    if ( returnUnit )
    {
        returnUnit->setBACT_viewer(true);
        returnUnit->setBACT_inputting(true);
        returnUnit->ArmSuicideHandoffFireRelease();
        return;
    }

    // Non-suicide cameras retain the original launcher/parent return semantics.
    if ( _mislEmitter )
    {
        if ( _mislEmitter->_status != BACT_STATUS_DEAD || _mislEmitter->_parent == nullptr )
        {
            _mislEmitter->setBACT_viewer(true);
            _mislEmitter->setBACT_inputting(true);
        }
        else
        {
            _mislEmitter->_parent->setBACT_viewer(true);
            _mislEmitter->_parent->setBACT_inputting(true);
        }
    }
}

void NC_STACK_ypamissile::Impact()
{
    bact_arg83 arg83;
    arg83.energ = _energy;
    arg83.pos = _position;
    arg83.pos2 = _fly_dir;
    arg83.force = _fly_dir_length;
    arg83.mass = _mass;

    float v16 = _fly_dir_length * _mass;

    if ( v16 > _world->_maxImpulse && _world->_maxImpulse > 0.0 )
    {
        float v7 = _world->_maxImpulse / v16;
        arg83.force *= v7;
        arg83.mass *= v7;
    }

    /* FIXME:
       Needs to check all near sectors too if effective radius affect it*/

    const bool validDirectPush = isfinite(_mislDirectPush) && _mislDirectPush > 0;
    const bool validBlastPush = isfinite(_mislBlastPush) && _mislBlastPush > 0 &&
                             isfinite(_mislBlastRadius) &&
                             _mislBlastRadius > 0.0f;
    bool modernPushWeapon = validDirectPush || validBlastPush;
    bool hasLegacyImpulseTarget = false;

    for( NC_STACK_ypabact* &bct : _pSector->unitsList )
    {
        if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_ROBO && bct->_bact_type != BACT_TYPES_TANK && bct->_bact_type != BACT_TYPES_CAR && bct->_bact_type != BACT_TYPES_GUN && !(bct->_status_flg & BACT_STFLAG_DEATH2) )
        {
            int v10 = 1;

            if ( _world->_isNetGame && _owner != bct->_owner )
                v10 = 0;

            if ( v10 )
            {
                if ( bct->_status == BACT_STATUS_DEAD ||
                     (bct->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
                    continue;

                const bool modernPushTarget =
                    modernPushWeapon && bct->CanReceiveConfiguredPush();

                if ( modernPushTarget )
                    continue;

                hasLegacyImpulseTarget = true;
                bct->ApplyImpulse(&arg83);
            }
            else
            {
                hasLegacyImpulseTarget = true;
            }
        }
    }

    if ( _world->_isNetGame && hasLegacyImpulseTarget )
    {
        uamessage_impulse impMsg;
        impMsg.msgID = UAMSG_IMPULSE;
        impMsg.p[0] = modernPushWeapon ? 1 : 0;
        impMsg.owner = _owner;
        impMsg.id = _gid;
        impMsg.pos = _position;
        impMsg.impulse = _energy;
        impMsg.dir = _fly_dir;
        impMsg.dir_len = _fly_dir_length;
        impMsg.mass = _mass;

        _world->NetBroadcastMessage(&impMsg, sizeof(impMsg), true);
    }

    ApplyBlastToUnits();
    ApplyBlastToBuildings();
    ApplyBlastToSectors();

    // A missing, zero or invalid cluster_trigger_time uses the existing
    // impact/detonation path. Positive times are handled only by AI_layer3.
    TryClusterSplit(true);

    // F10 shows the one shared blast radius used by all target categories.
    if ( _world && _world->_showCollDebug && !_isArtilleryShellProjectile )
        _world->DebugAddBlastRing(_position, _mislBlastRadius, 255, 140, 0);

    _mislDirectHitUnits.clear();
    _mislDirectPushRecipientGids.clear();
    _mislDirectHitBuildings.clear();
    _mislDirectHitSectors.clear();
}

void NC_STACK_ypamissile::DetonateAtContact(NC_STACK_ypabact *directHit)
{
    DetonateKamikazePayload(directHit);
}

void NC_STACK_ypamissile::DetonateKamikazePayload(NC_STACK_ypabact *directHit)
{
    if ( _status == BACT_STATUS_DEAD )
        return;

    if ( directHit &&
         directHit != this &&
         directHit != _mislEmitter &&
         directHit->_bact_type != BACT_TYPES_MISSLE &&
         directHit->_status != BACT_STATUS_DEAD )
    {
        ApplyDirectHitToBact(directHit);
    }

    Impact();

    setState_msg arg78;
    arg78.unsetFlags = 0;
    arg78.setFlags = 0;
    arg78.newStatus = BACT_STATUS_DEAD;

    SetState(&arg78);

    StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_IMPACT_WORLD);
    StartDestFXByType(World::DestFX::FX_MEGADETH);
}

void NC_STACK_ypamissile::AlignMissile(float dtime)
{
    if ( _fly_dir != vec3d(0.0, 0.0, 0.0) )
    {
        vec3d dir = _rotation.AxisZ(); // Get Z-axis, as dir
        vec3d u = vec3d::Normalise(dir * _fly_dir); // vector cross product

        // If length == 0 - no rotation
        if ( u.length() > 0.0 )
        {
            //scalar cross product
            float rotAngle = clp_acos( dir.dot(_fly_dir) );

            if ( _mislType == MISL_BOMB )
            {
                if ( dtime != 0.0 )
                {
                    float mxrot = _maxrot * dtime;

                    if ( rotAngle < -mxrot )
                        rotAngle = -mxrot;

                    if ( rotAngle > mxrot )
                        rotAngle = mxrot;
                }
            }

            if ( fabs(rotAngle) > BOMB_MIN_ANGLE )
                _rotation *= mat3x3::AxisAngle(u, rotAngle);
        }

        // Fix camera Z-axis rotation
        if ( _mislFlags & FLAG_MISL_VIEW )
        {
            float ZAngle = clp_acos( _rotation.AxisX().XZ().length() ); // Get degree of current Z-axis rotation

            if ( _rotation.m11 < 0.0 )
                ZAngle = C_PI - ZAngle;

            if ( _rotation.m01 < 0.0 )
                ZAngle = -ZAngle;

            _rotation = mat3x3::RotateZ(-ZAngle) * _rotation;
        }
    }
}

void NC_STACK_ypamissile::AlignMissileByNormal(const vec3d &normal)
{
    vec3d UpVector = _rotation.AxisY();

    vec3d vaxis = UpVector * normal;

    if ( vaxis.normalise() != 0.0 )
    {
        float angle = clp_acos( UpVector.dot(normal) );

        if ( fabs(angle) > BACT_MIN_ANGLE )
            _rotation *= mat3x3::AxisAngle(vaxis, angle);
    }
}

NC_STACK_ypamissile::NC_STACK_ypamissile()
{
    _mislType = 0;
    _mislEmitter = nullptr;
    _mislLifeTime = 0;
    _mislDriveTime = 0;
    _mislDelayTime = 0;
    _mislFlags = 0;
    _mislStartHeight = 0.;
    _mislEnergyHeli = 0.;
    _mislEnergyTank = 0.;
    _mislEnergyFlyer = 0.;
    _mislEnergyRobo = 0.;
    _mislBlastFalloff = 0;
    _mislClusterGeneration = 0;
    _artilleryShellImpactOnSurface = false;
}

void NC_STACK_ypamissile::setBACT_viewer(bool vwr)
{
    NC_STACK_ypabact::setBACT_viewer(vwr);

    if ( vwr )
        _mislFlags |= FLAG_MISL_VIEW;
    else
        _mislFlags &= ~FLAG_MISL_VIEW;
}

void NC_STACK_ypamissile::SetLauncherBact(NC_STACK_ypabact *bact)
{
    _mislEmitter = bact;

}

void NC_STACK_ypamissile::SetMissileType(int tp)
{
    _mislType = tp;
}

void NC_STACK_ypamissile::SetLifeTime(int time)
{
    _mislLifeTime = time;
}

void NC_STACK_ypamissile::SetDelay(int delay)
{
    _mislDelayTime = delay;
}

void NC_STACK_ypamissile::SetDriveTime(int time)
{
    _mislDriveTime = time;
}

void NC_STACK_ypamissile::SetHomingTime(int time)
{
    _mislHomingTimeRemaining = time > 0 ? time : -1;
}

void NC_STACK_ypamissile::SetIgnoreBuilds(int ign)
{
    if ( ign )
        _mislFlags |= FLAG_MISL_IGNOREBUILDS;
    else
        _mislFlags &= ~FLAG_MISL_IGNOREBUILDS;
}

void NC_STACK_ypamissile::SetPowerHeli(int po)
{
    _mislEnergyHeli = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerTank(int po)
{
    _mislEnergyTank = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerFlyer(int po)
{
    _mislEnergyFlyer = po * 0.001;
}

void NC_STACK_ypamissile::SetPowerRobo(int po)
{
    _mislEnergyRobo = po * 0.001;
}

void NC_STACK_ypamissile::SetRadiusHeli(float rad)
{
    _mislRadiusHeli = rad;
}

void NC_STACK_ypamissile::SetRadiusTank(float rad)
{
    _mislRadiusTank = rad;
}

void NC_STACK_ypamissile::SetRadiusFlyer(float rad)
{
    _mislRadiusFlyer = rad;
}

void NC_STACK_ypamissile::SetRadiusRobo(float rad)
{
    _mislRadiusRobo = rad;
}

void NC_STACK_ypamissile::SetStartHeight(float posy)
{
    _mislStartHeight = posy;
}

void NC_STACK_ypamissile::SetClusterSpawnedChild(bool child)
{
    _mislClusterChild = child;
    _mislClusterDone = child;
    _mislClusterAge = 0;
}

void NC_STACK_ypamissile::ConfigureSpawnedProjectileBase(
    NC_STACK_ypabact *launcher,
    NC_STACK_yparobo *hostStation,
    uint8_t owner,
    const vec3d &direction)
{
    SetLauncherBact(launcher);
    SetStartHeight(_position.y);
    _owner = owner;
    _host_station = hostStation;
    _fly_dir = direction;
}

NC_STACK_ypabact *NC_STACK_ypamissile::GetLauncherBact()
{
    return _mislEmitter;
}

int NC_STACK_ypamissile::GetMissileType()
{
    return _mislType;
}

int NC_STACK_ypamissile::GetLifeTime()
{
    return _mislLifeTime;
}

int NC_STACK_ypamissile::GetDelay()
{
    return _mislDelayTime;
}

int NC_STACK_ypamissile::GetDriveTime()
{
    return _mislDriveTime;
}

int NC_STACK_ypamissile::GetIgnoreBuilds()
{
    return (_mislFlags & FLAG_MISL_IGNOREBUILDS) != 0;
}

int NC_STACK_ypamissile::GetPowerHeli()
{
    return _mislEnergyHeli * 1000.0;
}

int NC_STACK_ypamissile::GetPowerTank()
{
    return _mislEnergyTank * 1000.0;
}

int NC_STACK_ypamissile::GetPowerFlyer()
{
    return _mislEnergyFlyer * 1000.0;
}

int NC_STACK_ypamissile::GetPowerRobo()
{
    return _mislEnergyRobo * 1000.0;
}

float NC_STACK_ypamissile::GetRadiusHeli()
{
    return _mislRadiusHeli;
}

float NC_STACK_ypamissile::GetRadiusTank()
{
    return _mislRadiusTank;
}

float NC_STACK_ypamissile::GetRadiusFlyer()
{
    return _mislRadiusFlyer;
}

float NC_STACK_ypamissile::GetRadiusRobo()
{
    return _mislRadiusRobo;
}

float NC_STACK_ypamissile::GetStartHeight()
{
    return _mislStartHeight;
}
