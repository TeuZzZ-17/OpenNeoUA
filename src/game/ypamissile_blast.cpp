// Blast damage and push, armor penetration, direct hits and area skip rules.
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

bool ypamissile_IsAliveForDeathPush(NC_STACK_ypabact *target)
{
    return target &&
           target->CanReceiveConfiguredPush() &&
           target->_status != BACT_STATUS_DEAD &&
           target->_energy > 0 &&
           !(target->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

NC_STACK_ypabact *ypamissile_ResolveDirectPushRecipient(NC_STACK_ypabact *target)
{
    if ( !target || target->_bact_type != BACT_TYPES_GUN )
        return target;

    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(target);
    if ( !gun || (!target->_isUnitGunChild && !gun->IsRoboGun()) )
        return target;

    NC_STACK_ypabact *parent = target->_parent;
    if ( !parent || parent == target || !target->getBACT_pWorld() ||
         parent->getBACT_pWorld() != target->getBACT_pWorld() ||
         parent->_bact_type == BACT_TYPES_MISSLE ||
         !ypamissile_IsAliveForDeathPush(parent) )
        return target;

    return parent;
}

float ypamissile_GetPushAtDeathMultiplier()
{
    std::string value = System::IniConf::GamePushAtDeathMultiplier.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return 1.0f;

    try
    {
        size_t pos = 0;
        float multiplier = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !isfinite(multiplier) || multiplier < 0.0f )
            return 1.0f;

        return std::min(multiplier, 10.0f);
    }
    catch (...)
    {
        return 1.0f;
    }
}

float ypamissile_GetTargetPushMultiplier(NC_STACK_ypabact *target)
{
    return target ? target->GetPushResistanceMultiplier() : 1.0f;
}

float ypamissile_SegmentSegmentDistanceSq(const vec3d &p1, const vec3d &q1, const vec3d &p2, const vec3d &q2)
{
    const float EPSILON = 0.0001f;
    vec3d d1 = q1 - p1;
    vec3d d2 = q2 - p2;
    vec3d r = p1 - p2;
    float a = d1.dot(d1);
    float e = d2.dot(d2);
    float f = d2.dot(r);
    float s = 0.0f;
    float t = 0.0f;

    if ( a <= EPSILON && e <= EPSILON )
        return r.dot(r);

    if ( a <= EPSILON )
    {
        t = ypamissile_Clamp01(f / e);
    }
    else
    {
        float c = d1.dot(r);
        if ( e <= EPSILON )
        {
            s = ypamissile_Clamp01(-c / a);
        }
        else
        {
            float b = d1.dot(d2);
            float denom = a * e - b * b;

            if ( denom != 0.0f )
                s = ypamissile_Clamp01((b * f - c * e) / denom);

            t = (b * s + f) / e;

            if ( t < 0.0f )
            {
                t = 0.0f;
                s = ypamissile_Clamp01(-c / a);
            }
            else if ( t > 1.0f )
            {
                t = 1.0f;
                s = ypamissile_Clamp01((b - c) / a);
            }
        }
    }

    vec3d c1 = p1 + d1 * s;
    vec3d c2 = p2 + d2 * t;
    vec3d delta = c1 - c2;
    return delta.dot(delta);
}

bool ypamissile_GetCompoundImpactPoint(const vec3d &oldWeaponCenter,
                                               const vec3d &newWeaponCenter,
                                               float weaponRadius,
                                               const vec3d &targetCenter,
                                               float targetRadius,
                                               vec3d *impactPosition,
                                               float *travelFraction)
{
    if ( !impactPosition || targetRadius <= 0.0f )
        return false;

    const float combinedRadius =
        std::max(0.0f, weaponRadius) + targetRadius;
    const vec3d motion = newWeaponCenter - oldWeaponCenter;
    const vec3d relativeStart = oldWeaponCenter - targetCenter;

    const float a = motion.dot(motion);
    const float c =
        relativeStart.dot(relativeStart) - combinedRadius * combinedRadius;

    float t = 0.0f;
    if ( c > 0.0f )
    {
        if ( a <= 0.000001f )
            return false;

        const float b = relativeStart.dot(motion);
        const float discriminant = b * b - a * c;
        if ( discriminant < 0.0f )
            return false;

        t = (-b - sqrtf(std::max(0.0f, discriminant))) / a;
        if ( t < 0.0f || t > 1.0f )
            return false;
    }

    const vec3d weaponCenterAtImpact = oldWeaponCenter + motion * t;
    vec3d outward = weaponCenterAtImpact - targetCenter;

    if ( outward.normalise() <= 0.001f )
    {
        outward = -motion;
        if ( outward.normalise() <= 0.001f )
            outward = vec3d::OY(1.0);
    }

    *impactPosition = targetCenter + outward * targetRadius;
    if ( travelFraction )
        *travelFraction = t;

    return true;
}

float ypamissile_BlastFalloffFactor(float distance, float radius, bool falloff)
{
    if ( radius <= 0.0 || distance > radius )
        return 0.0;

    if ( !falloff )
        return 1.0;

    // Same attenuation shape used by NC_STACK_ypacar::DoKamikaze().
    return ypamissile_Clamp01(exp(distance * -2.8 / World::CVSectorLength));
}

int ypamissile_ScaleBlastEnergy(int energy, float factor)
{
    if ( energy <= 0 || factor <= 0.0 )
        return 0;

    return (int)((float)energy * factor);
}

bool ypamissile_GetDirectPushDir(NC_STACK_ypamissile *missile, NC_STACK_ypabact *target,
                                        const vec3d &fallbackDir, vec3d *outDir)
{
    if ( !missile || !target || !outDir )
        return false;

    vec3d dir;

    if ( missile->GetLauncherBact() && missile->GetLauncherBact() != target )
    {
        dir = target->_position - missile->GetLauncherBact()->_position;
        if ( ypamissile_NormalizeXZ(&dir) )
        {
            *outDir = dir;
            return true;
        }
    }

    dir = missile->_position - missile->_old_pos;
    if ( ypamissile_NormalizeXZ(&dir) )
    {
        *outDir = dir;
        return true;
    }

    dir = missile->_fly_dir;
    if ( ypamissile_NormalizeXZ(&dir) )
    {
        *outDir = dir;
        return true;
    }

    dir = fallbackDir;
    if ( ypamissile_NormalizeXZ(&dir) )
    {
        *outDir = dir;
        return true;
    }

    return false;
}

// Default values of the missile state that belongs to one shot (armor penetration, attachment,
// blast and push bookkeeping). Shared by Init and Renew.
void NC_STACK_ypamissile::ResetRuntimeBlastState()
{
    _mislArmorPenetrationRemaining = 0;
    _mislArmorPenetratedGids.clear();
    _mislDirectPushRecipientGids.clear();
    _mislClusterAge = 0;
    _weaponSoundEventsEnabled = true;
    _collisionOldRotationValid = false;
    _mislClusterGeneration = 0;
    _mislClusterDone = false;
    _mislClusterChild = false;
    _mislChainDepth = 0;
    _mislChainEnergy = 0;
    _mislChainSpawned = false;
    _mislChainAllowFriendly = false;
    _mislChainPending = false;
    _mislChainPendingElapsed = 0;
    _mislChainPendingDelay = 0;
    _mislChainPendingTargetGid = 0;
    _mislChainPendingEnergy = 0;
    _mislChainPendingOrigin = vec3d(0.0, 0.0, 0.0);
    _mislChainPendingOriginRadius = 0.0;
    _mislChainHitGids.clear();
    _suicideViewerReturnGids.clear();
    _suicideViewerHostGid = 0;
    _mislAttachedToTarget = false;
    _mislAttachTargetGid = 0;
    _mislAttachedDamageMultiplier = 1.0f;
    _mislAttachOffset = vec3d(0.0, 0.0, 0.0);
    _mislLastAttachedPosition = vec3d(0.0, 0.0, 0.0);
}

int NC_STACK_ypamissile::CalcEnergyForBact(NC_STACK_ypabact *bct, int baseEnergy)
{
    if ( !bct || baseEnergy <= 0 )
        return 0;

    int damage = 0;

    switch ( bct->_bact_type )
    {
    case BACT_TYPES_BACT:
        damage = baseEnergy * _mislEnergyHeli;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        damage = baseEnergy * _mislEnergyTank;
        break;

    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
        damage = baseEnergy * _mislEnergyFlyer;
        break;

    case BACT_TYPES_ROBO:
        damage = baseEnergy * _mislEnergyRobo;
        break;

    default:
        damage = baseEnergy;
        break;
    }

    // OpenNeoUA: fine-grained energy_* overrides preserve the exact vanilla
    // heli/tank/flyer/robo result above when absent. Like the legacy values,
    // these were snapshotted when the projectile was created.
    float specificEnergy = 1.0f;
    if ( TryGetSpecificEnergyForTarget(bct, &specificEnergy) )
        damage = (int)((float)baseEnergy * specificEnergy);

    float shieldedDamage = damage * (100.0f - bct->GetEffectiveShield());
    float divisor = ( bct->getBACT_inputting() || bct->getBACT_viewer() ) ? 250.0 : 100.0;

    return ceil(shieldedDamage / divisor);
}

int NC_STACK_ypamissile::ApplyEnergyToBact(NC_STACK_ypabact *bct, int baseEnergy, float damageMultiplier, bool applyDebuff)
{
    if ( !bct )
        return 0;

    // Artillery keeps its normal terminal collision and explosion, but never
    // damages units owned by the shell's emitter. This central unit-damage
    // guard covers both the direct-hit and blast paths without affecting enemy
    // units, buildings, sectors, or other projectile models.
    if ( _isArtilleryShellProjectile && _mislEmitter &&
         bct->_owner == _mislEmitter->_owner )
        return 0;

    NC_STACK_ypabact *userHost = _world ? _world->getYW_userHostStation() : nullptr;

    if ( _world && userHost && !(userHost->_owner == _owner || !_world->_isNetGame) )
        return 0;

    World::TWeapProto *wproto = nullptr;
    if ( _world && _vehicleID >= 0 && (size_t)_vehicleID < _world->GetWeaponsProtos().size() )
        wproto = &_world->GetWeaponsProtos().at(_vehicleID);

    bool preAppliedDebuff = false;
    if ( applyDebuff && wproto && wproto->debuff.valid && wproto->debuff.shield_malus > 0.0f &&
         bct->_energy > 0 && bct->_status != BACT_STATUS_DEAD )
    {
        bct->ApplyDebuff(wproto->debuff, _mislEmitter);
        preAppliedDebuff = true;
    }

    int damage = CalcEnergyForBact(bct, baseEnergy);
    if ( !damage )
        return 0;

    const float clampedDamageMultiplier =
        std::max(0.0f, std::min(std::isfinite(damageMultiplier) ? damageMultiplier : 1.0f, 1.0f));
    if ( clampedDamageMultiplier < 1.0f )
        damage = (int)std::ceil((float)damage * clampedDamageMultiplier);

    if ( damage > 0 )
    {
        bact_arg84 arg84;
        arg84.energy = -damage;
        arg84.unit = _mislEmitter;
        bct->ModifyEnergy(&arg84);
    }

    if ( applyDebuff && wproto && wproto->debuff.valid && !preAppliedDebuff && bct->_energy > 0 && bct->_status != BACT_STATUS_DEAD )
        bct->ApplyDebuff(wproto->debuff, _mislEmitter);

    return damage;
}

void NC_STACK_ypamissile::ApplyDirectHitToBact(NC_STACK_ypabact *bct, bool applyDamage, float damageMultiplier)
{
    if ( !bct )
        return;

    bct->_status_flg &= ~BACT_STFLAG_LAND;
    RememberDirectHitUnit(bct);

    NC_STACK_ypabact *pushRecipient = ypamissile_ResolveDirectPushRecipient(bct);
    bool wasAlive = pushRecipient == bct && ypamissile_IsAliveForDeathPush(bct);
    vec3d pushDir(0.0, 0.0, 0.0);
    float pushStrength = 0.0f;
    bool hasDirectPush = !IsDirectPushRecipient(pushRecipient) &&
                         ApplyDirectPushToBact(pushRecipient, &pushDir, &pushStrength,
                                               false, bct);

    int appliedDamage = applyDamage ? ApplyEnergyToBact(bct, _energy, damageMultiplier) : 0;
    if ( hasDirectPush )
    {
        bool diedNow = pushRecipient == bct && applyDamage && wasAlive &&
                       !(bct->_status_flg & BACT_STFLAG_DEATH2) &&
                       (bct->_status_flg & BACT_STFLAG_DEATH1);
        if ( diedNow )
            pushStrength *= ypamissile_GetPushAtDeathMultiplier();

        pushRecipient->ApplyConfiguredPush(pushDir, pushStrength);
        RememberDirectPushRecipient(pushRecipient);
    }

    if ( applyDamage )
        TrySpawnChainProjectile(bct, appliedDamage);
}

bool NC_STACK_ypamissile::IsArmorPenetratedTarget(NC_STACK_ypabact *bct) const
{
    if ( !bct )
        return false;

    return std::find(_mislArmorPenetratedGids.begin(), _mislArmorPenetratedGids.end(), bct->_gid) != _mislArmorPenetratedGids.end();
}

bool NC_STACK_ypamissile::ShouldArmorPenetrateTarget(NC_STACK_ypabact *bct) const
{
    return _mislArmorPenetrationRemaining > 0 &&
           bct &&
           bct->_bact_type != BACT_TYPES_MISSLE &&
           !IsArmorPenetratedTarget(bct);
}

void NC_STACK_ypamissile::RememberArmorPenetratedTarget(NC_STACK_ypabact *bct)
{
    if ( !bct || IsArmorPenetratedTarget(bct) )
        return;

    _mislArmorPenetratedGids.push_back(bct->_gid);
}

void NC_STACK_ypamissile::ApplyArmorPenetrationUnitImpactFX(
        const vec3d *impactPosition)
{
    const vec3d savedPosition = _position;
    if ( impactPosition )
        _position = *impactPosition;

    if ( World::TWeapProto::SND_HIT < _soundcarrier.Sounds.size() )
    {
        // A penetrating projectile stays in motion. Playing SND_HIT from the
        // missile carrier would therefore apply Doppler to an impact sound that
        // is stationary in the world. Mirror the configured hit source on a
        // zero-velocity carrier fixed at the penetration point.
        if ( _mislArmorPenetrationSoundCarrier.Sounds.empty() )
        {
            _mislArmorPenetrationSoundCarrier.Resize(1);

            TSoundSource &src = _soundcarrier.Sounds[World::TWeapProto::SND_HIT];
            TSoundSource &snd = _mislArmorPenetrationSoundCarrier.Sounds[0];

            snd.PSample = src.PSample;
            snd.PPFx = src.PPFx;
            snd.PShkFx = src.PShkFx;
            snd.PFragments = src.PFragments;
            snd.Volume = src.Volume;
            snd.Radius = src.Radius;
            snd.FadeDuration = src.FadeDuration;
            snd.CopyPitchConfig(src);
            snd.AllowExtendedRate = src.AllowExtendedRate;
            snd.PriorityBias = src.PriorityBias;
            snd.IgnoreTimeScale = src.IgnoreTimeScale;
            snd.SetLoop(src.IsLoop());
            snd.SetFragmented(src.IsFragmented());
            snd.SetPFx(src.IsPFx());
            snd.SetShk(src.IsShk());
        }

        _mislArmorPenetrationSoundCarrier.Position = _position;
        _mislArmorPenetrationSoundCarrier.Vector = vec3d(0.0, 0.0, 0.0);

        SFXEngine::SFXe.ForceStopSource(&_mislArmorPenetrationSoundCarrier, 0);
        SFXEngine::SFXe.startSound(&_mislArmorPenetrationSoundCarrier, 0);
        SFXEngine::SFXe.UpdateSoundCarrier(&_mislArmorPenetrationSoundCarrier);
    }

    StartChainFXByTrigger(World::TChainFXConfig::TRIGGER_DETONATE);
    StartDestFXByType(World::DestFX::FX_DEATH);

    if ( impactPosition )
        _position = savedPosition;
}

bool NC_STACK_ypamissile::ApplyDirectPushToBact(NC_STACK_ypabact *bct, vec3d *appliedDir,
                                                  float *appliedStrength, bool enqueue,
                                                  NC_STACK_ypabact *directionTarget)
{
    if ( !bct || !isfinite(_mislDirectPush) || _mislDirectPush <= 0 )
        return false;

    if ( GetAreaPushSkipReason(bct) )
        return false;

    // A transferred GUN hit keeps the original impact geometry for direction,
    // while eligibility, resistance and the physical impulse belong to the parent.
    NC_STACK_ypabact *pushDirectionTarget = directionTarget ? directionTarget : bct;
    vec3d fallbackDir = pushDirectionTarget->_position - _position;
    vec3d pushDir;
    if ( !ypamissile_GetDirectPushDir(this, pushDirectionTarget, fallbackDir, &pushDir) )
        return false;

    float pushStrength = _mislDirectPush * ypamissile_GetTargetPushMultiplier(bct);
    if ( pushStrength <= 0.0f )
        return false;

    if ( enqueue )
        bct->ApplyConfiguredPush(pushDir, pushStrength);

    if ( appliedDir )
        *appliedDir = pushDir;

    if ( appliedStrength )
        *appliedStrength = pushStrength;

    return true;
}

const char *NC_STACK_ypamissile::GetBlastEnergySkipReason(NC_STACK_ypabact *bct, bool allowFriendly) const
{
    if ( !bct || bct == this || bct == _mislEmitter )
    {
        if ( !bct )
            return "null";
        if ( bct == this )
            return "projectile_self";
        return "emitter";
    }

    if ( bct->_bact_type == BACT_TYPES_MISSLE )
        return "missile";

    if ( bct->_status == BACT_STATUS_DEAD )
        return "dead";

    if ( bct->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2) )
        return "death_fx";

    if (bct->_bact_type == BACT_TYPES_GUN && bct->GetEffectiveShield() >= 100.0f)
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

        if ( gun && gun->IsRoboGun() )
            return "shielded_robo_gun";
    }

    if ( _mislEmitter && !allowFriendly && bct->_owner == _mislEmitter->_owner )
        return "friendly";

    if ( _mislEmitter && _mislEmitter->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( _mislEmitter );

        if ( gun && bct->_owner == _owner && gun->IsRoboGun() && !_mislEmitter->_isUnitGunChild )
        {
            if ( bct->_bact_type == BACT_TYPES_ROBO )
                return "own_robo_gun_robo";

            if ( bct->_bact_type == BACT_TYPES_GUN )
            {
                NC_STACK_ypagun *bgun = dynamic_cast<NC_STACK_ypagun *>( bct );

                if ( bgun && bgun->IsRoboGun() && !bct->_isUnitGunChild )
                    return "own_robo_gun";
            }
        }
    }

    // The bomb start-height rule belongs to the legacy physical bomb path.
    // Artillery has a dedicated ballistic/vertical trajectory and must evaluate
    // unit blast around its actual timed impact point instead.
    if ( !_isArtilleryShellProjectile && _mislType == MISL_BOMB &&
         bct->_position.y < _mislStartHeight )
        return "bomb_below_start_height";

    return nullptr;
}

// Push eligibility filter for normal direct/blast push. Physical Chain FX,
// static guns and final DEATH2 wrecks are excluded here. Owner policy is
// applied by the collision/blast call sites so player-controlled legacy behavior
// remains unchanged.
const char *NC_STACK_ypamissile::GetAreaPushSkipReason(NC_STACK_ypabact *bct) const
{
    if ( !bct || bct == this || bct == _mislEmitter )
        return "self";

    // Physical Chain FX already receive their launch velocity when they spawn.
    // Weapon push must not add a second impulse to those fragments.
    if ( bct->_inlinePhysicalFX )
        return "physical_fragment";

    // OpenNeoUA custom: all flak/turret actors use model = gun / BACT_TYPES_GUN.
    // They are static defenses and should never be knocked away by weapon push.
    if ( !bct->CanReceiveConfiguredPush() )
        return "gun";

    // Final death-FX wreck: do not disturb (same guard as legacy Impact()).
    if ( bct->_status_flg & BACT_STFLAG_DEATH2 )
        return "death2";

    return nullptr;
}

bool NC_STACK_ypamissile::IsDirectHitUnit(NC_STACK_ypabact *bct) const
{
    if ( !bct )
        return false;

    if ( IsArmorPenetratedTarget(bct) )
        return true;

    return std::find(_mislDirectHitUnits.begin(), _mislDirectHitUnits.end(), bct) != _mislDirectHitUnits.end();
}

void NC_STACK_ypamissile::RememberDirectHitUnit(NC_STACK_ypabact *bct)
{
    if ( bct && !IsDirectHitUnit(bct) )
        _mislDirectHitUnits.push_back(bct);
}

bool NC_STACK_ypamissile::IsDirectPushRecipient(NC_STACK_ypabact *bct) const
{
    if ( !bct )
        return false;

    return std::find(_mislDirectPushRecipientGids.begin(),
                     _mislDirectPushRecipientGids.end(),
                     bct->_gid) != _mislDirectPushRecipientGids.end();
}

void NC_STACK_ypamissile::RememberDirectPushRecipient(NC_STACK_ypabact *bct)
{
    if ( bct && !IsDirectPushRecipient(bct) )
        _mislDirectPushRecipientGids.push_back(bct->_gid);
}

vec3d NC_STACK_ypamissile::GetBuildingSlotCenter(const cellArea &cell, int bldX, int bldY) const
{
    vec3d pos = World::SectorIDToCenterPos3(cell.CellId);
    pos.y = cell.height;

    if ( cell.SectorType != 1 )
    {
        pos.x += (bldX - 1) * 300.0;
        pos.z += (bldY - 1) * 300.0;
    }

    return pos;
}

bool NC_STACK_ypamissile::GetBuildingSlotAtPosition(const vec3d &pos, Common::Point *cellId, int *bldX, int *bldY) const
{
    Common::Point sec = World::PositionToSectorID(pos);

    if ( !_world || !_world->IsSector(sec) )
        return false;

    cellArea &cell = _world->SectorAt(sec);

    if ( !cell.IsGamePlaySector() || cell.PurposeType == cellArea::PT_NONE || cell.PurposeType == cellArea::PT_CONSTRUCTING )
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

    if ( cell.buildings_health.At(outX, outY) <= 0 )
        return false;

    if ( cellId )
        *cellId = sec;
    if ( bldX )
        *bldX = outX;
    if ( bldY )
        *bldY = outY;

    return true;
}

bool NC_STACK_ypamissile::IsDirectHitBuilding(const Common::Point &cellId, int bldX, int bldY) const
{
    for ( const TBuildingHitRef &hit : _mislDirectHitBuildings )
    {
        if ( hit.cellId == cellId && hit.bldX == bldX && hit.bldY == bldY )
            return true;
    }

    return false;
}

void NC_STACK_ypamissile::RememberDirectHitBuildingAt(const vec3d &pos)
{
    TBuildingHitRef hit;

    if ( !GetBuildingSlotAtPosition(pos, &hit.cellId, &hit.bldX, &hit.bldY) )
        return;

    if ( !IsDirectHitBuilding(hit.cellId, hit.bldX, hit.bldY) )
        _mislDirectHitBuildings.push_back(hit);
}

bool NC_STACK_ypamissile::GetSectorSlotAtPosition(const vec3d &pos, Common::Point *cellId, int *bldX, int *bldY) const
{
    Common::Point sec = World::PositionToSectorID(pos);

    if ( !_world || !_world->IsSector(sec) )
        return false;

    cellArea &cell = _world->SectorAt(sec);

    if ( !cell.IsGamePlaySector() || cell.PurposeType != cellArea::PT_NONE )
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

    if ( cell.buildings_health.At(outX, outY) <= 0 )
        return false;

    if ( cellId )
        *cellId = sec;
    if ( bldX )
        *bldX = outX;
    if ( bldY )
        *bldY = outY;

    return true;
}

bool NC_STACK_ypamissile::IsDirectHitSector(const Common::Point &cellId, int bldX, int bldY) const
{
    for ( const TBuildingHitRef &hit : _mislDirectHitSectors )
    {
        if ( hit.cellId == cellId && hit.bldX == bldX && hit.bldY == bldY )
            return true;
    }

    return false;
}

void NC_STACK_ypamissile::RememberDirectHitSectorAt(const vec3d &pos)
{
    TBuildingHitRef hit;

    if ( !GetSectorSlotAtPosition(pos, &hit.cellId, &hit.bldX, &hit.bldY) )
        return;

    if ( !IsDirectHitSector(hit.cellId, hit.bldX, hit.bldY) )
        _mislDirectHitSectors.push_back(hit);
}

void NC_STACK_ypamissile::ApplyBlastToUnits()
{
    bool doBlastDamage = (_mislBlastEnergy > 0);
    bool doBlastPush   = isfinite(_mislBlastPush) && _mislBlastPush > 0;
    World::TWeapProto *proto = _world && (size_t)_vehicleID < _world->GetWeaponsProtos().size()
        ? &_world->GetWeaponsProtos().at(_vehicleID) : nullptr;
    const bool hasDebuff = proto && proto->debuff.valid;

    if ( !isfinite(_mislBlastRadius) || _mislBlastRadius <= 0.0 || (!doBlastDamage && !doBlastPush && !hasDebuff) || !_world )
        return;

    // Artillery retains its normal blast target scan, including same-owner units,
    // so collision and explosion behavior are unchanged. ApplyEnergyToBact()
    // suppresses artillery damage to those units; normal weapons retain the
    // existing player/AI friendly-fire policy unchanged.
    bool allowFriendly = _isArtilleryShellProjectile || getBACT_viewer();

    if ( _mislEmitter && _mislEmitter->getBACT_inputting() )
        allowFriendly = true;

    Common::Point impactCell = World::PositionToSectorID(_position);

    if ( !_world->IsSector(impactCell) )
        return;

    int cellRadius = (int)ceil(_mislBlastRadius / World::CVSectorLength) + 1;
    std::vector<NC_STACK_ypabact *> damagedUnits;

    for ( int dy = -cellRadius; dy <= cellRadius; dy++ )
    {
        for ( int dx = -cellRadius; dx <= cellRadius; dx++ )
        {
            Common::Point cellId = impactCell + Common::Point(dx, dy);

            if ( !_world->IsSector(cellId) )
                continue;

            cellArea &cell = _world->SectorAt(cellId);

            for ( NC_STACK_ypabact *bct : cell.unitsList.safe_iter() )
            {
                // Blast of a mounted Unit Gun never reaches its carrier or sibling guns
                // (damage, push and debuff).
                if ( _mislEmitter && _mislEmitter->IsMountedFamilyWith(bct) )
                    continue;

                const char *dmgSkip  = GetBlastEnergySkipReason(bct, allowFriendly);
                const char *pushSkip = doBlastPush ? GetAreaPushSkipReason(bct) : "push_disabled";

                if ( doBlastPush && !pushSkip && !allowFriendly && _mislEmitter &&
                     bct->_owner == _mislEmitter->_owner )
                    pushSkip = "friendly";

                const bool applyDebuff = hasDebuff && bct && bct != this &&
                    bct->_bact_type != BACT_TYPES_MISSLE && bct->_energy > 0 &&
                    bct->_status != BACT_STATUS_DEAD &&
                    !(bct->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
                if ( dmgSkip && pushSkip && !applyDebuff )
                    continue;

                vec3d delta = bct->_position - _position;
                float distance = delta.length();

                if ( distance > _mislBlastRadius )
                    continue;

                if ( std::find(damagedUnits.begin(), damagedUnits.end(), bct) != damagedUnits.end() )
                    continue;

                damagedUnits.push_back(bct);

                int areaEnergy = 0;
                if ( doBlastDamage && !dmgSkip )
                {
                    areaEnergy = ypamissile_ScaleBlastEnergy(
                        _mislBlastEnergy,
                        ypamissile_BlastFalloffFactor(distance, _mislBlastRadius,
                                                    _mislBlastFalloff != 0));
                }

                bool hasBlastPush = false;
                bool wasAlive = false;
                vec3d appliedPushDir(0.0, 0.0, 0.0);
                float appliedPushStrength = 0.0f;

                const bool validDirectPush = isfinite(_mislDirectPush) && _mislDirectPush > 0;
                if ( doBlastPush && !pushSkip &&
                     !(validDirectPush &&
                       (IsDirectHitUnit(bct) || IsDirectPushRecipient(bct))) )
                {
                    wasAlive = ypamissile_IsAliveForDeathPush(bct);

                    if ( distance > 1.0f )
                        appliedPushDir = delta / distance;
                    else
                        appliedPushDir = vec3d(1.0f, 0.0f, 0.0f);

                    appliedPushStrength =
                        _mislBlastPush *
                        World::BlastPushFalloffFactor(distance, _mislBlastRadius,
                                                    _mislBlastFalloff != 0);

                    appliedPushStrength *= ypamissile_GetTargetPushMultiplier(bct);
                    hasBlastPush = appliedPushStrength > 0.0f;
                }

                // A conventional projectile explosion spends one Deflect charge and blocks
                // every unit-side effect from that single blast event. If the local blast
                // energy exceeds the configured Buff threshold, the blast breaks all charges
                // and continues normally. Persistent fields and scripted damage use other
                // call paths and are untouched.
                float deflectEndDamageMultiplier = 1.0f;
                if ( bct->HasDeflectBuff() && (areaEnergy > 0 || hasBlastPush || applyDebuff) )
                {
                    if ( areaEnergy > 0 && !bct->CanBuffDeflectEnergy(areaEnergy) )
                    {
                        deflectEndDamageMultiplier = bct->GetBuffDeflectEndEnergyMultiplier();
                        bct->ClearBuffDeflectCharges();
                    }
                    else
                    {
                        bct->ConsumeBuffDeflectCharge();
                        continue;
                    }
                }

                // A blast also affects the direct target. Debuff coverage is
                // independent of energy/push and includes allied units.
                if ( applyDebuff )
                    bct->ApplyDebuff(proto->debuff, _mislEmitter);
                if ( areaEnergy > 0 )
                    ApplyEnergyToBact(bct, areaEnergy, deflectEndDamageMultiplier, false);

                if ( hasBlastPush )
                {
                    bool diedNow = wasAlive &&
                                   !(bct->_status_flg & BACT_STFLAG_DEATH2) &&
                                   (bct->_status_flg & BACT_STFLAG_DEATH1);
                    if ( diedNow )
                        appliedPushStrength *= ypamissile_GetPushAtDeathMultiplier();

                    bct->ApplyConfiguredPush(appliedPushDir, appliedPushStrength);
                }
            }
        }
    }
}

float NC_STACK_ypamissile::GetBuildingBlastDistance(const vec3d &buildingCenter) const
{
    const float legacyDistance = (buildingCenter - _position).length();
    if ( !_world || (size_t)_vehicleID >= _world->GetWeaponsProtos().size() ||
         !_world->GetWeaponsProtos()[_vehicleID].IsKamikaze() )
        return legacyDistance;

    // Kamikaze detonates near the building surface, which can be far above its
    // historical ground-level damage point. Query only this building slot.
    if ( !_world->_collisionScene )
        _world->_collisionScene.reset(new Collision::Scene(*_world));
    Collision::Contact contact;
    if ( !_world->_collisionScene->TraceSector(buildingCenter, _position, _position,
                                                _mislBlastRadius, &contact, true) )
        return legacyDistance;
    return std::min(legacyDistance, (float)(contact.point - _position).length());
}

void NC_STACK_ypamissile::ApplyBlastToBuildings()
{
    if ( _mislBlastRadius <= 0.0 || _mislBlastEnergy <= 0 || !_world )
        return;

    if ( _mislFlags & FLAG_MISL_IGNOREBUILDS )
        return;

    if ( _world->_isNetGame && _world->_userRobo->_owner != _owner )
        return;

    Common::Point impactCell = World::PositionToSectorID(_position);

    if ( !_world->IsSector(impactCell) )
        return;

    int cellRadius = (int)ceil(_mislBlastRadius / World::CVSectorLength) + 1;

    for ( int dy = -cellRadius; dy <= cellRadius; dy++ )
    {
        for ( int dx = -cellRadius; dx <= cellRadius; dx++ )
        {
            Common::Point cellId = impactCell + Common::Point(dx, dy);

            if ( !_world->IsSector(cellId) )
                continue;

            cellArea &cell = _world->SectorAt(cellId);
            int slots = cell.SectorType == 1 ? 1 : 3;

            for ( int bldY = 0; bldY < slots; bldY++ )
            {
                for ( int bldX = 0; bldX < slots; bldX++ )
                {
                    if ( GetAreaBuildingSkipReason(cell, bldX, bldY) )
                        continue;

                    vec3d bldPos = GetBuildingSlotCenter(cell, bldX, bldY);
                    float distance = GetBuildingBlastDistance(bldPos);

                    if ( distance > _mislBlastRadius )
                        continue;

                    yw_arg129 arg129;
                    arg129.field_0 = 0;
                    arg129.pos = bldPos;
                    arg129.field_10 = ypamissile_ScaleBlastEnergy(_mislBlastEnergy, ypamissile_BlastFalloffFactor(distance, _mislBlastRadius, _mislBlastFalloff != 0));
                    if ( arg129.field_10 <= 0 )
                        continue;

                    arg129.unit = _mislEmitter;

                    ChangeSectorEnergy(&arg129);
                }
            }
        }
    }
}

void NC_STACK_ypamissile::ApplyBlastToSectors()
{
    if ( _mislBlastRadius <= 0.0 || _mislBlastEnergy <= 0 || !_world )
        return;

    if ( _mislFlags & FLAG_MISL_IGNOREBUILDS )
        return;

    if ( _world->_isNetGame && _world->_userRobo->_owner != _owner )
        return;

    Common::Point impactCell = World::PositionToSectorID(_position);

    if ( !_world->IsSector(impactCell) )
        return;

    int cellRadius = (int)ceil(_mislBlastRadius / World::CVSectorLength) + 1;

    for ( int dy = -cellRadius; dy <= cellRadius; dy++ )
    {
        for ( int dx = -cellRadius; dx <= cellRadius; dx++ )
        {
            Common::Point cellId = impactCell + Common::Point(dx, dy);

            if ( !_world->IsSector(cellId) )
                continue;

            cellArea &cell = _world->SectorAt(cellId);
            int slots = cell.SectorType == 1 ? 1 : 3;

            for ( int bldY = 0; bldY < slots; bldY++ )
            {
                for ( int bldX = 0; bldX < slots; bldX++ )
                {
                    if ( GetAreaSectorSkipReason(cell, bldX, bldY) )
                        continue;

                    vec3d sectorPos = GetBuildingSlotCenter(cell, bldX, bldY);
                    float distance = GetBuildingBlastDistance(sectorPos);

                    if ( distance > _mislBlastRadius )
                        continue;

                    yw_arg129 arg129;
                    arg129.field_0 = 0;
                    arg129.pos = sectorPos;
                    arg129.field_10 = ypamissile_ScaleBlastEnergy(_mislBlastEnergy, ypamissile_BlastFalloffFactor(distance, _mislBlastRadius, _mislBlastFalloff != 0));
                    if ( arg129.field_10 <= 0 )
                        continue;

                    arg129.unit = _mislEmitter;

                    // Sector blast uses the same path as direct hits on normal sector architecture.
                    ChangeSectorEnergy(&arg129);
                }
            }
        }
    }
}

NC_STACK_ypabact *NC_STACK_ypamissile::FindAttachedTarget()
{
    if ( !_world || !_mislAttachedToTarget || !_mislAttachTargetGid )
        return nullptr;

    return ypamissile_FindLiveBactByGid(_world->_unitsList, _mislAttachTargetGid);
}

void NC_STACK_ypamissile::UpdateAttachedDetonationPosition()
{
    NC_STACK_ypabact *target = FindAttachedTarget();

    if ( target )
    {
        _position = target->_position + _mislAttachOffset;
        _mislLastAttachedPosition = _position;
    }
    else if ( _mislAttachedToTarget )
    {
        _position = _mislLastAttachedPosition;
    }
}

void NC_STACK_ypamissile::ApplyAttachedDirectHitDamage()
{
    NC_STACK_ypabact *target = FindAttachedTarget();

    if ( target )
        ApplyDirectHitToBact(target, true, _mislAttachedDamageMultiplier);

    _mislAttachedDamageMultiplier = 1.0f;
}

void NC_STACK_ypamissile::ConfigureSpecificEnergyMultipliers(const World::TWeapProto &proto)
{
    _mislSpecificEnergy.fill(1.0f);
    _mislSpecificEnergyDefined.fill(false);

    for (int i = (int)World::VEHICLE_COMBAT_CLASS_UNKNOWN + 1;
         i < (int)World::VEHICLE_COMBAT_CLASS_COUNT; ++i)
    {
        float energy = 1.0f;
        const World::VehicleCombatClass targetClass =
            static_cast<World::VehicleCombatClass>(i);
        if ( World::TryGetSpecificWeaponEnergy(proto, targetClass, &energy) )
        {
            // Match the existing projectile snapshot path exactly: legacy
            // energy_* values are converted to integer thousandths before
            // SetPower*() stores them back as floats. Keep the new fine-class
            // overrides on the same precision so enabling a more specific key
            // cannot subtly change projectile damage rounding.
            const int energyThousandths = (int)(energy * 1000.0f);
            _mislSpecificEnergy[(size_t)i] = energyThousandths * 0.001f;
            _mislSpecificEnergyDefined[(size_t)i] = true;
        }
    }
}

bool NC_STACK_ypamissile::TryGetSpecificEnergyForTarget(NC_STACK_ypabact *bct, float *outEnergy) const
{
    if ( !bct )
        return false;

    const int targetClass = (int)World::ResolveVehicleCombatClass(bct);
    if ( targetClass <= (int)World::VEHICLE_COMBAT_CLASS_UNKNOWN ||
         targetClass >= (int)World::VEHICLE_COMBAT_CLASS_COUNT ||
         !_mislSpecificEnergyDefined[(size_t)targetClass] )
        return false;

    if ( outEnergy )
        *outEnergy = _mislSpecificEnergy[(size_t)targetClass];
    return true;
}

void NC_STACK_ypamissile::SetBlast(float radius, int energy, int falloff)
{
    _mislBlastRadius = std::isfinite(radius) ? std::max(0.0f, std::min(radius, 12000.0f)) : 0.0f;
    _mislBlastEnergy = std::max(0, energy);
    _mislBlastFalloff = falloff ? 1 : 0;
}

void NC_STACK_ypamissile::SetBlastPush(float push)
{
    _mislBlastPush = isfinite(push) ? std::max(0.0f, std::min(push, 10.0f)) : 0.0f;
}

void NC_STACK_ypamissile::SetDirectPush(float push)
{
    _mislDirectPush = isfinite(push) ? std::max(0.0f, std::min(push, 10.0f)) : 0.0f;
}

void NC_STACK_ypamissile::SetArmorPenetrationTargets(int targets)
{
    _mislArmorPenetrationRemaining = std::max(targets, 0);
    _mislArmorPenetratedGids.clear();
    _mislDirectPushRecipientGids.clear();
}
