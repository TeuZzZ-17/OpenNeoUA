#ifndef YPABACT_INTERNAL_H_INCLUDED
#define YPABACT_INTERNAL_H_INCLUDED

// Internal header of the NC_STACK_ypabact implementation (ypabact*.cpp).
// Not part of the public interface: it holds what the feature files share (helpers that used to be
// file-static, constants and small types). Public declarations stay in ypabact.h.

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <cstdlib>
#include <stack>
#include <map>
#include <functional>
#include <algorithm>
#include <limits>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <cctype>
#include "game/yw.h"
#include "game/ypabact.h"
#include "game/yparobo.h"
#include "game/ypagun.h"
#include "game/ypamissile.h"
#include "game/yw_net.h"
#include "system/inpt.h"
#include "system/inivals.h"
#include "world/blacksecttint.h"
#include "world/energyfx.h"
#include "world/spin.h"
#include "world/tools.h"
#include "world/kamikaze_contact.h"
#include "common/log.h"
#include "common/crashdiag.h"

// ---- constants shared by the parts ----
static constexpr double YPABACT_MISSILE_DISTANCE_SCALE = 6.0;
static constexpr float RECOIL_DISTANCE_PER_UNIT = 7.0f;
static constexpr float RECOIL_MAX_INTENSITY = 10.0f;
static constexpr float RECOIL_VISUAL_MAX_DISTANCE =
    RECOIL_DISTANCE_PER_UNIT * RECOIL_MAX_INTENSITY;
static constexpr float RECOIL_VISUAL_DEGREES_PER_UNIT = 0.75f;
static constexpr float RECOIL_VISUAL_MAX_DEGREES = 5.0f;
static constexpr int RECOIL_DEFAULT_KICK_TIME_MS = 0;
static constexpr int RECOIL_DEFAULT_HOLD_TIME_MS = 0;
static constexpr int RECOIL_DEFAULT_RETURN_TIME_MS = 420;
static constexpr int RECOIL_MAX_PHASE_TIME_MS = 5000;
static constexpr int MGUN_RECOIL_PLAYER_SHAKE_DURATION_MS = 220;
static constexpr float MGUN_RECOIL_PLAYER_DEGREES_PER_UNIT = 1.2f;
static constexpr float MGUN_RECOIL_PLAYER_MAX_DEGREES = 12.0f;
static constexpr float MGUN_RECOIL_PLAYER_SHAKE_AXIS_X = 0.35f;
static constexpr float MGUN_RECOIL_PLAYER_SHAKE_AXIS_Y = 0.20f;
static constexpr float MGUN_RECOIL_PLAYER_SHAKE_AXIS_Z = 0.35f;
static const float blast_PUSH_TAU = 0.30f;
static const float blast_PUSH_MAX_DT = 0.05f;
static const float blast_PUSH_MAX_STEP = 80.0f;
static const float CONFIGURED_PUSH_MAX_INTENSITY = 10.0f;
static const float CONFIGURED_PUSH_DISTANCE_PER_SQUARED_LEVEL = 100.0f;
static const float RECOIL_MECHANICAL_TAU = 0.14f;
static const int RECOIL_AI_TANK_RECOVERY_MS = 220;
static const int RECOIL_PLAYER_TANK_RECOVERY_MS = 220;
static constexpr int YPA_WEAPON_FLAG_PROJECTILE = World::TWeapProto::WEAPON_FLAG_PROJECTILE;
static constexpr int YPA_WEAPON_FLAGS_MISSILE = World::TWeapProto::WEAPON_FLAGS_MISSILE;
static constexpr float YPA_MISSILE_MULTI_TARGET_RANGE = 2000.0;
static constexpr int YPA_HOMING_TARGET_CYCLE_MAX = 64;
static constexpr float PLAYER_LASER_ACQUIRE_ALIGNMENT = 0.9659258f; // cos(15 deg)
static constexpr float PLAYER_LASER_HOLD_ALIGNMENT = 0.9271839f;    // cos(22 deg)

// ---- globals defined in the core file ----
extern int ypabact_id;
extern int dword_5B1128;

struct TUnitCollisionPairKey
{
    const NC_STACK_ypaworld *world;
    int gidLow;
    int gidHigh;

    bool operator<(const TUnitCollisionPairKey &other) const
    {
        if ( world != other.world )
            return std::less<const NC_STACK_ypaworld *>()(world, other.world);
        if ( gidLow != other.gidLow )
            return gidLow < other.gidLow;
        return gidHigh < other.gidHigh;
    }
};

struct TAiTargetRangeConfig
{
    float range;
    bool enforceViewerRange;
};

struct ypabact_TEliteUnitConfig
{
    float maxStatBonusPercent = 0.0f;
    uint8_t maxMedals = 4;
    uint32_t killsPerMedal = 5;
    float maxGlowIntensity = 0.0f;
    World::TVisualTint glowTint;
    float glowPulseSeconds = 0.0f;
};

struct TDamagedVisualRef
{
    int16_t vp = 0;
    std::string mesh3ds;
};

struct TKamikazeMount
{
    NC_STACK_ypabact *carrier = nullptr;
    NC_STACK_ypabact *payloadSource = nullptr;
    int weaponId = -1;
    const World::TWeapProto *weapon = nullptr;
};

struct TMissileMultiTargetCandidate
{
    NC_STACK_ypabact *target = nullptr;
    float score = 0.0;
};

struct TLaserWorldHit
{
    Common::Point cellId;
    int bldX = 0;
    int bldY = 0;
    vec3d damagePos;
};

struct TLaserUnitHit
{
    NC_STACK_ypabact *target = nullptr;
    vec3d hitPoint;
    float along = 0.0f;
};

// ---- functions that used to be file-static ----
double ypabact_GetArcGrenadeGravity(const World::TWeapProto &wproto);
vec3d ypabact_LaserViewerVisualStart( const NC_STACK_ypabact *bact, const World::TWeapProto &wproto, const vec3d &beamStart, const vec3d &beamEnd);
bool ypabact_IsLaserMeshUsable( NC_STACK_ypabact *bact, const World::TWeapProto &wproto);
uint32_t ypabact_LaserMeshSeed(const NC_STACK_ypabact *bact, size_t beamIndex, uint32_t domain);
void ypabact_RenderLaserMeshes(NC_STACK_ypabact *bact, baseRender_msg *arg);
bool ypabact_BeginUnitCollisionEvent(NC_STACK_ypabact *first, NC_STACK_ypabact *second, int frameTime);
bool ypabact_IsGenesisSeparationVehicle(const NC_STACK_ypabact *unit);
float ypabact_ReadNonNegativeFloatIni(Common::Ini::Key &key, float dflt);
bool ypabact_ReadAbsoluteOrPercentIni(Common::Ini::Key &key, World::TAbsoluteOrPercent *out, float maxPercent = 100.0f);
bool ypabact_ReadFallDamage(World::TAbsoluteOrPercent *out);
float ypabact_ReadPlayerMaxAltitudeAboveGround();
float ypabact_GetAiMaxAltitudeAboveGround();
float ypabact_GetMinigunRange();
const TAiTargetRangeConfig &ypabact_GetAiTargetRangeConfig();
float ypabact_GetMinigunAiFireAlignment();
float ypabact_ReadHandBrakePower();
float ypabact_GetPlasmaDeathDurationMultiplier();
float ypabact_GetPlasmaDeathMagnetRadius();
float ypabact_GetPlasmaDeathMagnetSpeed();
float ypabact_ReadHandBrakeRecoilReduction();
const ypabact_TEliteUnitConfig &ypabact_GetEliteUnitConfig();
bool ypabact_IsDirectLocalPlayerHandBrakeActive(NC_STACK_ypabact *bact);
float ypabact_GetHandBrakeRandomSpreadScale(NC_STACK_ypabact *bact);
bool ypabact_ReadUnitCollisionDamage(Common::Ini::Key &key, World::TAbsoluteOrPercent *out);
bool ypabact_IsCustomFallDamageConfigActive();
void ypabact_ApplyFallDamage(NC_STACK_ypabact *unit);
bool ypabact_ShouldPlayCustomAiCrashlandSound(const NC_STACK_ypabact *unit);
bool ypabact_ShouldPlayCrashlandSound(const NC_STACK_ypabact *unit, float speed, float minSpeed);
void ypabact_StartSoundOnce(NC_STACK_ypabact *unit, int soundId);
bool ypabact_IsAirVehicle(const NC_STACK_ypabact *unit);
void ypabact_ResetDamagedFX(NC_STACK_ypabact *bact);
bool ypabact_IsDamagedFXSystemDisabled(const NC_STACK_ypabact *bact);
bool ypabact_IsMainVPBase(NC_STACK_ypabact *bact, NC_STACK_base *base);
bool ypabact_IsTintableVisualBase(NC_STACK_ypabact *bact, NC_STACK_base *base);
bool ypabact_ShouldApplyVPRotation(NC_STACK_ypabact *bact, NC_STACK_base *base);
bool ypabact_ShouldApplyVPSpin(NC_STACK_ypabact *bact, NC_STACK_base *base);
bool ypabact_HasProjectileChaos(const NC_STACK_ypabact *bact);
bool ypabact_HasProjectileSpiral(const NC_STACK_ypabact *bact);
bool ypabact_HasProjectileVisualMotion(const NC_STACK_ypabact *bact);
bool ypabact_ShouldApplyProjectileVisualMotion(NC_STACK_ypabact *bact, NC_STACK_base *base);
double ypabact_GetProjectileSpiralPhase(float speed, int32_t age);
mat3x3 ypabact_BuildProjectileSpiralRoll(float speed, int32_t age);
vec3d ypabact_BuildProjectileSpiralLocalOffset(const NC_STACK_ypabact *bact);
vec3d ypabact_BuildProjectileSpiralOffset(const NC_STACK_ypabact *bact);
mat3x3 ypabact_BuildProjectileVisualTangent(const NC_STACK_ypabact *bact, const vec3d &visualVelocityLocal);
mat3x3 ypabact_BuildProjectileSpiralTangent(const NC_STACK_ypabact *bact);
uint32_t ypabact_ProjectileChaosHash(uint32_t value);
double ypabact_ProjectileChaosUnit(uint32_t value);
vec3d ypabact_GetProjectileChaosTarget(const NC_STACK_ypabact *bact, uint32_t segment);
vec3d ypabact_BuildProjectileChaosLocalOffsetAtAge(const NC_STACK_ypabact *bact, double ageMilliseconds);
vec3d ypabact_BuildProjectileChaosOffset(const NC_STACK_ypabact *bact);
mat3x3 ypabact_BuildProjectileChaosTangent(const NC_STACK_ypabact *bact);
void ypabact_ApplyProjectileVisualMotion(NC_STACK_ypabact *bact, NC_STACK_base *base);
mat3x3 ypabact_BuildVPRotationMatrix(const vec3d &degrees);
int ypabact_ReadRecoilPhaseTimeMs(Common::Ini::Key &key, int dflt);
int ypabact_GetRecoilKickTimeMs();
int ypabact_GetRecoilHoldTimeMs();
int ypabact_GetRecoilReturnTimeMs();
float ypabact_RecoilKickCurve(float t);
float ypabact_RecoilReturnCurve(float t);
void ypabact_EvaluateRecoilVisual(const NC_STACK_ypabact *bact, vec3d *offset, float *pitch);
bool ypabact_IsPlayerGunRecoilFirstPersonView(const NC_STACK_ypabact *bact);
float ypabact_GetRecoilVisualPitch(const NC_STACK_ypabact *bact);
void ypabact_StartRecoilVisual(NC_STACK_ypabact *bact, const vec3d &recoilDir, float recoil);
bool ypabact_IsAiTankRecoilUnit(const NC_STACK_ypabact *unit);
bool ypabact_UsesRenderOnlyRecoilTranslation(const NC_STACK_ypabact *unit);
bool ypabact_ShouldRenderRecoilVisualOffset(const NC_STACK_ypabact *unit);
bool ypabact_IsCockpitCameraSupportedType(const NC_STACK_ypabact *unit);
float ypabact_GetDamagedThresholdEnergy(const NC_STACK_ypabact *bact);
bool ypabact_CanUseGameplayStatusMechanics(NC_STACK_ypabact *bact, bool allowCreate = false);
bool ypabact_CanSpawnDecorationFX(NC_STACK_ypabact *bact);
bool ypabact_IsUsableControlFallback(NC_STACK_ypabact *bact, NC_STACK_ypabact *dying);
bool ypabact_IsMindcontrolUnitType(NC_STACK_ypabact *bact);
bool ypabact_CanBeMindcontrolled(NC_STACK_ypabact *target, NC_STACK_ypabact *source);
NC_STACK_yparobo *ypabact_GetMindcontrolHost(NC_STACK_ypabact *source);
void ypabact_ApplyMindcontrol(NC_STACK_ypabact *target, NC_STACK_ypabact *source);
void ypabact_SafeDetachControlFrom(NC_STACK_ypabact *dying, NC_STACK_ypabact *preferredFallback);
bool ypabact_IsUsableSquadControlCandidate(NC_STACK_ypabact *candidate, NC_STACK_ypabact *dying);
std::vector<NC_STACK_ypabact *> ypabact_GetSquadControlFallbacks(NC_STACK_ypabact *dying);
NC_STACK_ypabact *ypabact_GetNextSquadControlFallback(NC_STACK_ypabact *dying);
bool ypabact_IsDamagedStateActive(const NC_STACK_ypabact *bact);
float ypabact_SafeDamageMult(float mult);
float ypabact_DebuffMalusToMult(float malus);
vec3d ypabact_BuildLegacyAttachedFXOffset(float overeof);
vec3d ypabact_BuildUniformStatusFXScale(float scale);
bool ypabact_BuildAttachedFXOffset(NC_STACK_ypabact *bact, const World::TAbsoluteOrPercent &randomMaxOffset, vec3d *localOffset);
int ypabact_GetDebuffFXLifetime(const TActiveDebuffState &debuff);
void ypabact_SpawnDebuffFXEvent(NC_STACK_ypabact *bact, int lifetime);
void ypabact_ApplyDamagedRuntime(NC_STACK_ypabact *bact, bool active);
int ypabact_ScaledPitch(TSoundSource &snd, int basePitch, float mult);
void ypabact_ApplyDamagedSoundPitch(NC_STACK_ypabact *bact);
void ypabact_UpdateStatusSoundCarrier(NC_STACK_ypabact *bact, TSndCarrier *carrier);
void ypabact_StartStatusSoundIfIdle(NC_STACK_ypabact *bact, TSndCarrier *carrier, int volume, int pitch);
bool ypabact_ShouldUsePlayerLaunchShake(NC_STACK_ypabact *bact, const World::TWeapProto &wproto);
void ypabact_TriggerLocalShakeCarrier(NC_STACK_ypabact *bact, TSndCarrier *carrier, TSndFxPosParam *shake);
void ypabact_TriggerPlayerLaunchShake(NC_STACK_ypabact *bact, World::TWeapProto &wproto);
void ypabact_TriggerPlayerMgunRecoilShake(NC_STACK_ypabact *bact);
void ypabact_UpdateMimicSoundCarrier(NC_STACK_ypabact *bact);
NC_STACK_ypabact *ypabact_FindLiveBactByGid(World::RefBactList &list, int32_t gid);
bool ypabact_IsCarrierSpawnAliveUnit(NC_STACK_ypabact *unit);
int ypabact_CountCarrierSpawnedUnits(NC_STACK_ypabact *carrier);
bool ypabact_CanCarrierSpawn(NC_STACK_ypabact *carrier);
bool ypabact_IsCarrierSpawnEnemy(NC_STACK_ypabact *carrier, NC_STACK_ypabact *unit);
bool ypabact_IsLaserDamageTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit);
bool ypabact_IsLaserAimTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit);
bool ypabact_HasEnemyNearby(NC_STACK_ypabact *carrier, float radius);
bool ypabact_CarrierHasEnemyNearby(NC_STACK_ypabact *carrier);
bool ypabact_IsCarrierSpawnPositionValid(NC_STACK_ypabact *carrier, const vec3d &pos);
bool ypabact_FindCarrierSpawnPosition(NC_STACK_ypabact *carrier, vec3d *outPos);
NC_STACK_ypabact *ypabact_CreateCarrierSpawnedUnit(NC_STACK_ypabact *carrier, const vec3d &pos);
void ypabact_AttachCarrierSpawnLeader(NC_STACK_ypabact *carrier, NC_STACK_ypabact *leader);
NC_STACK_yparobo *ypabact_FindSpawnAtDeathOwnerRobo(NC_STACK_ypaworld *world, uint8_t owner);
bool ypabact_IsSpawnAtDeathPositionValid(NC_STACK_ypabact *parent, const vec3d &pos);
bool ypabact_FindSpawnAtDeathPosition(NC_STACK_ypabact *parent, vec3d *outPos);
void ypabact_EnableSpawnAtDeathProtection(NC_STACK_ypabact *unit, int immunityTime);
void ypabact_UpdateSpawnAtDeathProtection(NC_STACK_ypabact *unit);
NC_STACK_yparobo *ypabact_GetSpawnAtDeathOwnerRobo(NC_STACK_ypabact *parent);
NC_STACK_ypabact *ypabact_CreateSpawnAtDeathUnit(NC_STACK_ypabact *parent, const vec3d &pos);
void ypabact_AttachSpawnAtDeathLeader(NC_STACK_ypabact *parent, NC_STACK_ypabact *leader);
void ypabact_TrySpawnAtDeath(NC_STACK_ypabact *parent);
bool ypabact_GetRandomFXSpawnCount(int configuredMin, int configuredMax, int &spawnCount);
void ypabact_ShuffleDamagedVisuals(std::vector<TDamagedVisualRef> &visuals);
void ypabact_SpawnSingleDamagedFX(NC_STACK_ypabact *bact, NC_STACK_ypaworld *world, const TDamagedVisualRef &visual);
void ypabact_SpawnDamagedFXEvent(NC_STACK_ypabact *bact);
vec3d ypabact_BuildRandomLocalDecorationFXOffset(float radius);
bool ypabact_GetDecorationFXSpawnCount(const World::TDecorationFXConfig &config, int &spawnCount);
void ypabact_SpawnDecorationFXEvent(NC_STACK_ypabact *bact);
void ypabact_SpawnEnergyStatusFXEvent(NC_STACK_ypabact *bact, const World::EnergyFX::Config &config, bool plusSymbol);
void ypabact_UpdateEnergyStatusFXProfile(NC_STACK_ypabact *bact, bool active, const World::EnergyFX::Config &config, bool plusSymbol, int32_t &nextTime);
bool ypabact_IsBlastPushGroundAlignedUnit(NC_STACK_ypabact *unit);
bool ypabact_ShouldFlattenAirKnockback(const NC_STACK_ypabact *unit);
bool ypabact_NormalizeXZ(vec3d *dir);
bool ypabact_IsGroundRecoilSurfaceUnit(const NC_STACK_ypabact *unit);
bool ypabact_ProbeLocalRecoilSupport(NC_STACK_ypabact *unit, ypaworld_arg136 *support);
bool ypabact_GetRecoilSupportNormal(NC_STACK_ypabact *unit, vec3d *normal);
bool ypabact_ResolveRecoilDirection(NC_STACK_ypabact *unit, const vec3d &requestedDir, vec3d *resolvedDir);
bool ypabact_SnapBlastPushGroundUnit(NC_STACK_ypabact *unit);
bool ypabact_SnapRecoilGroundUnit(NC_STACK_ypabact *unit);
float ypabact_ClampRecoil(float recoil);
bool ypabact_ConstrainRecoilStepToLevelBox(NC_STACK_ypabact *unit, const vec3d &requestedStep, vec3d *constrainedStep, bool *blockedX, bool *blockedZ);
void ypabact_UpdateFakePushVel(NC_STACK_ypabact *unit, vec3d *pushVel, update_msg *arg, float tau, bool recoilMotion);
bool ypabact_GetPlasmaFactionTint(NC_STACK_ypabact *bact, GFX::TGLColor *outTint);
void ypabact_RunPlayerUserLayer(NC_STACK_ypabact *bact, update_msg *arg);
bool ypabact_IsConfiguredAreaEffectTarget(const NC_STACK_ypabact *source, const NC_STACK_ypabact *target);
void ypabact_ApplyConfiguredDamage(NC_STACK_ypabact *source, NC_STACK_ypabact *target, int rawDamage);
void ypabact_ApplyInlinePhysicalImpactDamage(NC_STACK_ypabact *fragment, NC_STACK_ypabact *directTarget);
void ypabact_ApplyConfiguredAtDeathEffects(NC_STACK_ypabact *source);
NC_STACK_ypabact *ypabact_ResolveSessionKillCreditedUnit(NC_STACK_ypabact *victim);
bool ypabact_HasGroundDecalSupport(NC_STACK_ypabact *unit);
bool ypabact_IsValidWeaponId(NC_STACK_ypabact *bact, int weaponId);
bool ypabact_IsValidFireWeaponId(NC_STACK_ypabact *bact, int weaponId);
bool ypabact_IsKamikazeActorUsable(NC_STACK_ypabact *unit);
bool ypabact_IsMobileKamikazeCarrier(const NC_STACK_ypabact *unit);
NC_STACK_ypabact *ypabact_GetEffectiveKamikazeCarrier(NC_STACK_ypabact *unit);
bool ypabact_ResolveMountedKamikazeWeapon(NC_STACK_ypabact *source, int *outWeaponId, const World::TWeapProto **outWeapon);
bool ypabact_ResolveKamikazeFromGunList( NC_STACK_ypabact *carrier, const std::vector<World::TRoboGun> &guns, TKamikazeMount *outMount);
bool ypabact_ResolveKamikazeMount(NC_STACK_ypabact *unit, TKamikazeMount *outMount);
bool ypabact_IsKamikazeMountArmed(const TKamikazeMount &mount);
bool ypabact_IsKamikazeArmed(NC_STACK_ypabact *unit);
bool ypabact_IsValidKamikazeTarget(NC_STACK_ypabact *unit, NC_STACK_ypabact *target);
bool ypabact_IsKamikazeContact(const TKamikazeMount &mount, NC_STACK_ypabact *target, const vec3d &from, double *fraction);
NC_STACK_ypabact *ypabact_FindKamikazeContactTarget(const TKamikazeMount &mount, const vec3d &from, double *fraction);
bool ypabact_GetKamikazeCellTarget(NC_STACK_ypabact *unit, vec3d *outTarget);
bool ypabact_IsKamikazeCellContact(const TKamikazeMount &mount, const vec3d &targetPos, const vec3d &from, double *fraction);
NC_STACK_ypabact *ypabact_GetKamikazeRammingTarget(NC_STACK_ypabact *unit);
int ypabact_GetPrimaryWeaponSlots(NC_STACK_ypabact *bact, int *outSlots, int *outSourceSlots = nullptr);
int ypabact_NormalizeWeaponProjectileCount(int count);
void ypabact_GetWeaponProjectileCountRangeForSourceSlot( NC_STACK_ypabact *bact, int sourceSlot, int *outMin, int *outMax);
int ypabact_GetWeaponProjectileCountForSourceSlot(NC_STACK_ypabact *bact, int sourceSlot);
int ypabact_RollWeaponProjectileCountForSourceSlot(NC_STACK_ypabact *bact, int sourceSlot);
int ypabact_GetWeaponIdForSourceSlot(NC_STACK_ypabact *bact, int sourceSlot);
bool ypabact_GetWeaponEnergyForTarget(const World::TWeapProto &wproto, NC_STACK_ypabact *target, float *outEnergy, bool *outDefined);
int ypabact_SelectRandomPrimaryWeaponSlot(NC_STACK_ypabact *bact, int *outSourceSlot);
int ypabact_SelectSmartAIPrimaryWeaponSlot(NC_STACK_ypabact *bact, NC_STACK_ypabact *target, int *outSourceSlot);
int ypabact_SelectAIPrimaryWeaponSlot(NC_STACK_ypabact *bact, NC_STACK_ypabact *target, int *outSourceSlot);
int ypabact_GetActiveAILaserWeaponSlot(NC_STACK_ypabact *bact, int *outSourceSlot);
int ypabact_SelectAIPrimaryWeaponSlotWithLaserHold(NC_STACK_ypabact *bact, NC_STACK_ypabact *target, int *outSourceSlot);
bool ypabact_HasActiveAILaserSectorFocus(const NC_STACK_ypabact *bact);
int ypabact_GetCurrentPrimaryWeaponSourceSlot(NC_STACK_ypabact *bact);
bool ypabact_IsHomingBombWeapon(const World::TWeapProto &wproto);
bool ypabact_UsesMissileTargeting(const World::TWeapProto &wproto);
bool ypabact_IsCompatibleMultiTargetWeapon(const World::TWeapProto &wproto);
int ypabact_GetMultiTargetLimit(const World::TWeapProto &wproto, int weaponCount);
bool ypabact_IsMissileMultiTargetUnitType(NC_STACK_ypabact *target);
bool ypabact_IsValidMissileMultiTarget(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target);
bool ypabact_GetMissileMultiTargetScore(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target, const vec3d &aimDir, float weaponRadius, float *outScore);
void ypabact_AddMissileMultiTargetCandidate(std::vector<TMissileMultiTargetCandidate> &targets, NC_STACK_ypabact *target, float score);
void ypabact_CollectMissileMultiTargetsFromCell(std::vector<TMissileMultiTargetCandidate> &candidates, NC_STACK_ypabact *launcher, cellArea *cell, const vec3d &aimDir, float weaponRadius);
void ypabact_CollectHomingBombTargetsFromCell(std::vector<TMissileMultiTargetCandidate> &candidates, NC_STACK_ypabact *launcher, cellArea *cell, const vec3d &referencePos, float weaponRadius);
std::vector<NC_STACK_ypabact *> ypabact_CollectMissileMultiTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int maxTargets, bool useTargetAimDir = false);
std::vector<NC_STACK_ypabact *> ypabact_CollectHomingBombTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int maxTargets);
NC_STACK_ypabact *ypabact_GetDistributedMissileTarget(const std::vector<NC_STACK_ypabact *> &targets, int shotIndex);
void ypabact_StoreHUDMissileMultiLockTargets(NC_STACK_ypabact *launcher, const std::vector<NC_STACK_ypabact *> &missileTargets);
void ypabact_UpdateHUDWeaponMultiLockTargets(NC_STACK_ypabact *launcher, const bact_arg79 *arg, const World::TWeapProto &wproto, int weaponCount);
bool ypabact_IsHomingCycleTargetLockable(NC_STACK_ypabact *launcher, NC_STACK_ypabact *target, const World::TWeapProto &wproto, const vec3d &requestedAimDir);
NC_STACK_ypabact *ypabact_GetHomingCycleLogicalTarget(NC_STACK_ypabact *target);
NC_STACK_ypabact *ypabact_SelectNextHomingCycleTarget(NC_STACK_ypabact *launcher, const bact_arg106 *targetingArg, const World::TWeapProto &wproto, NC_STACK_ypabact *currentTarget);
int ypabact_SelectPrimaryWeaponSlot(NC_STACK_ypabact *bact, int requestedWeapon, int requestedSourceSlot, NC_STACK_ypabact *target, int *outSourceSlot);
void ypabact_AdvancePrimaryWeaponSlot(NC_STACK_ypabact *bact, int requestedWeapon);
bool ypabact_CanUseProximityDefense(NC_STACK_ypabact *unit);
vec3d ypabact_GetProximityDefenseLocalDirection(NC_STACK_ypabact *unit, int shotIndex, int totalShots);
bool ypabact_IsLiveProximityMissileOwner(NC_STACK_ypabact *candidate, NC_STACK_ypabact *unit);
NC_STACK_ypabact *ypabact_GetProximityDefenseAtDeathMissileOwner(NC_STACK_ypabact *unit);
bool ypabact_FireProximityDefenseShot(NC_STACK_ypabact *unit, int shotIndex, int totalShots, bool trackLauncherMissile = true, NC_STACK_ypabact *missileListOwner = nullptr);
void ypabact_FireProximityDefenseBurst(NC_STACK_ypabact *unit, bool trackLauncherMissiles = true, NC_STACK_ypabact *missileListOwner = nullptr);
bool ypabact_CanUseProximityDefenseAtDeath(NC_STACK_ypabact *unit);
void ypabact_FireProximityDefenseAtDeath(NC_STACK_ypabact *unit);
int ypabact_GetArtilleryShellWeaponId(NC_STACK_ypabact *unit);
int ypabact_GetVehicleProtoArtilleryShellWeaponId(NC_STACK_ypaworld *world, int vehicleId);
bool ypabact_IsLiveArtilleryShellGunActor(NC_STACK_ypabact *gunObj, int *outWeaponId = nullptr);
NC_STACK_ypabact *ypabact_GetArtilleryShellActorFromGunList(std::vector<World::TRoboGun> &guns, int *outWeaponId = nullptr);
int ypabact_GetProtoArtilleryShellWeaponIdFromGunList(NC_STACK_ypaworld *world, const std::vector<World::TRoboGun> &guns);
NC_STACK_ypabact *ypabact_GetMountedArtilleryShellActor(NC_STACK_ypabact *unit, int *outWeaponId = nullptr);
int ypabact_GetMountedArtilleryShellWeaponId(NC_STACK_ypabact *unit);
NC_STACK_ypabact *ypabact_GetManualArtilleryShellActor(NC_STACK_ypabact *unit, int *outWeaponId = nullptr);
bool ypabact_CanUseArtilleryShell(NC_STACK_ypabact *unit);
vec3d ypabact_GetArtilleryShellLaunchDir(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, const vec3d &targetCenter);
float ypabact_ClampArtilleryShellAimDelta(float delta, float maxRot);
void ypabact_AimArtilleryShellLauncherVisual(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, const vec3d &targetCenter, int frameTime, bool instant);
vec3d ypabact_GetArtilleryShellLaunchPosition(NC_STACK_ypabact *unit);
void ypabact_TriggerArtilleryShellFireVisual(NC_STACK_ypabact *unit);
bool ypabact_IsArtilleryShellEnemy(NC_STACK_ypabact *unit, NC_STACK_ypabact *cand);
bool ypabact_ScanArtilleryShellTarget(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, bool onlyHostStation, vec3d *out);
bool ypabact_FindArtilleryShellTargetZone(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, vec3d *out);
bool ypabact_FireArtilleryShell(NC_STACK_ypabact *unit, int weaponId, const World::TWeapProto &wproto, const vec3d &targetCenter);
void ypabact_RefreshPendingArtilleryShellMarker(NC_STACK_ypabact *unit, const World::TWeapProto &wproto);
void ypabact_QueueArtilleryShellTarget(NC_STACK_ypabact *unit, const World::TWeapProto &wproto, const vec3d &targetPos);
void ypabact_FireArtilleryShellShotAndAdvance(NC_STACK_ypabact *unit, int weaponId, const World::TWeapProto &wproto);
NC_STACK_ypabact *ypabact_GetKamikazePayloadListOwner(NC_STACK_ypabact *unit);
float ypabact_LaserRange(const World::TWeapProto &wproto);
int ypabact_LaserDamageInterval(const World::TWeapProto &wproto, bool playerControlled);
float ypabact_LaserEnergyScale(const World::TWeapProto &wproto, NC_STACK_ypabact *target);
float ypabact_LaserNominalTickEnergy(const World::TWeapProto &wproto, int connectedTicks, float damageMult = 1.0f);
int ypabact_LaserTickDamage(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto, NC_STACK_ypabact *target, int connectedTicks, float damageMult = 1.0f);
bool ypabact_CanApplyLaserDamage(NC_STACK_ypabact *shooter);
void ypabact_ApplyLaserUnitTick(NC_STACK_ypabact *shooter, World::TWeapProto &wproto, NC_STACK_ypabact *target, NC_STACK_ypabact::TLaserBeamRuntime &beam, bool playerControlled, float damageMult);
bool ypabact_LaserGetSectorHit(NC_STACK_ypaworld *world, const vec3d &pos, TLaserWorldHit *outHit);
int32_t ypabact_LaserSectorTargetId(const TLaserWorldHit &hit);
int ypabact_LaserTickSectorEnergy(const World::TWeapProto &wproto, int connectedTicks);
bool ypabact_LaserHitscan(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto, const vec3d &origin, const vec3d &dir, float range, TLaserUnitHit *outHit);
NC_STACK_ypabact *ypabact_LaserAutoTarget(NC_STACK_ypabact *shooter, const vec3d &origin, const vec3d &dir, float range, float minAlignment, bool useNativeLimits = false);
NC_STACK_ypabact *ypabact_RetainPlayerLaserTarget(NC_STACK_ypabact *shooter, const vec3d &origin, const vec3d &aimDir, float range, int32_t targetGid);
bool ypabact_LaserTargetInList(NC_STACK_ypabact *target, const std::vector<NC_STACK_ypabact *> &targets);
bool ypabact_IsLaserFriendlyToShooter(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit);
bool ypabact_IsLaserEnemyToShooter(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit);
bool ypabact_IsLaserSecondaryTargetCandidate(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit, bool friendly);
NC_STACK_ypabact *ypabact_FindNearestLaserMultiTarget(NC_STACK_ypabact *shooter, const vec3d &origin, const vec3d &aimDir, float range, bool friendlyTargets, const std::vector<NC_STACK_ypabact *> &excluded, bool useNativeLimits = false);
NC_STACK_ypabact *ypabact_FindNearestLaserChainUnit(NC_STACK_ypabact *shooter, const vec3d &origin, float range, bool friendlyChain, const std::vector<NC_STACK_ypabact *> &excluded);
void ypabact_AddLaserMultiTargetRequests(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto, std::vector<NC_STACK_ypabact::TLaserBeamRequest> *requests, float range, bool friendlyTargets, bool playerControlled);
bool ypabact_PrepareLaserSoundSource(TSoundSource &snd, World::TVhclSound &fx);
bool ypabact_LaserSoundHasContent(World::TVhclSound &fx);
void ypabact_UpdateLaserNormalSound(NC_STACK_ypabact *bact, World::TWeapProto &wproto, TSndCarrier *carrier, const vec3d &position, const vec3d &direction);
void ypabact_PlayLaserLaunchSound(NC_STACK_ypabact *bact, World::TWeapProto &wproto, const vec3d &position, const vec3d &direction);
void ypabact_UpdateLaserHitSound( NC_STACK_ypabact *bact, World::TWeapProto &wproto, TSndCarrier *carrier, const std::vector<NC_STACK_ypabact::TLaserBeamRuntime> &beams);
void ypabact_StoreHUDLaserMultiLockTargets(NC_STACK_ypabact *shooter, const std::vector<NC_STACK_ypabact::TLaserBeamRequest> &requests);
void ypabact_UpdateHUDLaserMultiLockTargets(NC_STACK_ypabact *shooter, const bact_arg79 *arg, const World::TWeapProto &wproto);
NC_STACK_ypabact *ypabact_FindLaserChainTarget(NC_STACK_ypabact *shooter, NC_STACK_ypabact *from, float radius, bool friendlyChain, const std::vector<NC_STACK_ypabact *> &hitHistory);
vec3d ypabact_LaserSourceOrigin(NC_STACK_ypabact *bact);
bool ypabact_LaserWorldHit(NC_STACK_ypabact *shooter, const vec3d &origin, const vec3d &dir, float range, vec3d *outHitPoint, ypaworld_arg136 *outHit = nullptr);
mat3x3 ypabact_LaserRotationFromDir(const vec3d &beamDir, const mat3x3 &fallback);
void ypabact_SpawnLaserImpactChainFX( NC_STACK_ypabact *shooter, World::TWeapProto &wproto, const NC_STACK_ypabact::TLaserBeamRuntime &beam, const vec3d &dir, bool worldContact, std::vector<vec3d> &spawnedPoints);
float ypabact_LaserClampVisualSpacing(float spacing);
vec3d ypabact_LaserVisualScale(const World::TWeapProto &wproto);
void ypabact_SpawnWeaponImpactVisual(NC_STACK_ypaworld *world, const World::TWeapProto &wproto, bool preferMegadeth, const vec3d &pos, const mat3x3 &rot, int32_t lifeTime);
void ypabact_SpawnLaserBeamVisuals(NC_STACK_ypabact *bact, const World::TWeapProto &wproto, const vec3d &beamStart, const vec3d &beamEnd);
void ypabact_StartVehicleFireVP(NC_STACK_ypabact *bact, int now);
void ypabact_StartVehicleFireVPIfEnabled(NC_STACK_ypabact *bact, int now);
float ypabact_AiVerticalFireTriggerRadius(const World::TWeapProto &wproto);
vec3d ypabact_VerticalLaserSourceOrigin(NC_STACK_ypabact *bact, const bact_arg79 *arg);
bool ypabact_IsInVerticalLaserFireZone(const TFireTriggerArea *box, const vec3d &origin, float aiTriggerRadius, const vec3d &pos, float radius);
bool ypabact_IsVerticalLaserMultiCandidate(NC_STACK_ypabact *shooter, NC_STACK_ypabact *unit, bool friendlyTargets, const vec3d &origin, float range, float aiTriggerRadius, const TFireTriggerArea *box, const std::vector<NC_STACK_ypabact *> &excluded);
NC_STACK_ypabact *ypabact_FindNearestVerticalLaserTarget(NC_STACK_ypabact *shooter, const vec3d &origin, float range, float aiTriggerRadius, const TFireTriggerArea *box, bool friendlyTargets, const std::vector<NC_STACK_ypabact *> &excluded);
bool ypabact_FireTriggerAreaContains(const TFireTriggerArea &box, const vec3d &pos, float radius);
float ypabact_FireTriggerAreaSearchRadius(const TFireTriggerArea &box);
NC_STACK_ypabact *ypabact_FindFireTriggerAreaEnemy(NC_STACK_ypabact *shooter, const TFireTriggerArea &box, const World::TWeapProto *wproto = nullptr);
void ypabact_AimAtFireTriggerAreaTarget(const bact_arg101 &fireTest, bact_arg79 *shot);
NC_STACK_ypabact *ypabact_UpdateVerticalLaserBeam(NC_STACK_ypabact *shooter, World::TWeapProto &wproto, NC_STACK_ypabact::TLaserBeamRuntime &beam, const vec3d &start, const vec3d &down, float range, bool beamWasActive, bool spawnBeamVPs, bool playerControlled, float damageMult, std::vector<vec3d> &chainFXPoints);
void ypabact_EnsureFireXRandomSeeded(NC_STACK_ypabact *unit);
uint32_t ypabact_PeekFireXRandom(NC_STACK_ypabact *unit);
uint32_t ypabact_NextFireXRandom(NC_STACK_ypabact *unit);
void ypabact_BuildFireXRandomOrder(NC_STACK_ypabact *unit);
void ypabact_GetFireXSpan(const NC_STACK_ypabact *unit, float *minX, float *maxX);
float ypabact_GetProjectileFireX(NC_STACK_ypabact *unit, float vanillaFireX, int projectileIndex, int projectileCount);
void ypabact_ConsumeProjectileFireX(NC_STACK_ypabact *unit);
int ypabact_GetAuthoredPercentEnergyCost(int energyMax, float percent);
int ypabact_GetStunPhaseDuration(const NC_STACK_ypabact *bact, int phase);
int ypabact_GetNextStunPhase(const NC_STACK_ypabact *bact, int currentPhase);
void ypabact_AdvanceStunPhase(NC_STACK_ypabact *bact, int clock);
vec3d ypabact_BuildStunIntent(const mat3x3 &rotation, bool verticalUnit, bool ufo, int phase);
void ypabact_UpdateStunFloorSafety(NC_STACK_ypabact *bact);
int ypabact_GetProgressiveWeaponBaseShotTime(const NC_STACK_ypabact *bact, const World::TWeapProto &proto);
bool ypabact_HasProgressiveWeaponFireRate(const NC_STACK_ypabact *bact, const World::TWeapProto &proto);
World::TAbsoluteOrPercent ypabact_GetPlasmaCurrencyGain();
uint64_t ypabact_CalculatePlasmaCurrencyValue(const NC_STACK_ypabact *source);
bool ypabact_ShouldPersistAILaserFire(NC_STACK_ypabact *bact, int weaponId, const bact_arg101 *arg, NC_STACK_ypabact *unitTarget);
vec3d ypabact_ApplyWeaponDirectionPattern(const mat3x3 &rotation, const vec3d &direction, int shotIndex, int weaponCount, float arcX, float arcY, float coneXY);
vec3d ypabact_GetCockpitViewDirection(NC_STACK_ypabact *bact, const vec3d &viewDir);
vec3d ypabact_GetCockpitAimTarget(NC_STACK_ypabact *bact, const vec3d &viewDir, float range);
vec3d ypabact_GetCockpitAimDirection(NC_STACK_ypabact *bact, const vec3d &origin, const vec3d &viewDir, const vec3d &fallbackDir, float range);
void ypabact_PlayVehicleMinigunPulse(NC_STACK_ypabact *bact);
vec3d ypabact_GetMinigunFireDir(NC_STACK_ypabact *bact, const vec3d &requestedDir);
bool ypabact_SpawnVehicleMinigunImpact(NC_STACK_ypabact *bact, const vec3d &pos, const vec3d &dir, bool worldHit);
int ypabact_GetMinigunSectorDamageStep(NC_STACK_ypabact *bact, const vec3d &pos);
void ypabact_ApplyMinigunSectorDamage(NC_STACK_ypabact *bact, const vec3d &pos, float energy);
bool ypabact_GetMinigunSpreadImpactPoint(const vec3d &origin, const vec3d &dir, const vec3d &center, float radius, vec3d *outPos, float *outDistance);
bool ypabact_GetRaySphereEntryDistance(const vec3d &origin, const vec3d &dir, const vec3d &center, float radius, float *outDistance);
bool ypabact_GetCompoundFXGeometry(NC_STACK_ypabact *bact, vec3d *center, float *radius);
void ypabact_CopyDeadFollowTransform(NC_STACK_ypabact *dst, NC_STACK_ypabact *src);

// ---- other free functions shared between the parts ----
bool ypabact_TrySolveArcGrenadeDirection( const vec3d &launchPos, const vec3d &targetPos, const World::TWeapProto &wproto, vec3d *outDirection);
void sub_481F94(NC_STACK_ypabact *bact);
void ypabact_NetUpdate_VPHACKS(NC_STACK_ypabact *bact, update_msg *upd);
void AI_layer3__sub1(NC_STACK_ypabact *bact, update_msg *arg);
void AI_layer3__sub0(NC_STACK_ypabact *bact, int a2);
NC_STACK_ypabact *sb_0x493984__sub1(NC_STACK_ypabact *bact);
NC_STACK_ypabact *sb_0x493984__sub0(NC_STACK_ypabact *bact);
NC_STACK_ypabact *sb_0x493984(NC_STACK_ypabact *bact, int a2);
void sb_0x4874c4(NC_STACK_ypabact *bact, int a2, int a3, float a4);
void CrashOrLand__sub1(NC_STACK_ypabact *bact);
void sub_48AB14(NC_STACK_ypabact *bact, const vec3d &vec);
void CrashOrLand__sub0(NC_STACK_ypabact *bact, int a2);
void CollisionWithBact__sub0(NC_STACK_ypabact *bact, NC_STACK_ypabact *a2);

#endif // YPABACT_INTERNAL_H_INCLUDED
