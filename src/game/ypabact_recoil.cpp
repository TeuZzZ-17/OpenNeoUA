// Recoil, blast push and configured push physics.
#include "game/ypabact_internal.h"

float ypabact_ReadHandBrakeRecoilReduction()
{
    return std::min(1.0f, ypabact_ReadHandBrakePower());
}

int ypabact_ReadRecoilPhaseTimeMs(Common::Ini::Key &key, int dflt)
{
    float parsed = ypabact_ReadNonNegativeFloatIni(key, (float)dflt);
    if ( !std::isfinite(parsed) )
        return dflt;

    parsed = std::min(parsed, (float)RECOIL_MAX_PHASE_TIME_MS);
    return std::max(0, (int)std::lround(parsed));
}

int ypabact_GetRecoilKickTimeMs()
{
    return ypabact_ReadRecoilPhaseTimeMs(System::IniConf::GameRecoilKickTime,
                                         RECOIL_DEFAULT_KICK_TIME_MS);
}

int ypabact_GetRecoilHoldTimeMs()
{
    return ypabact_ReadRecoilPhaseTimeMs(System::IniConf::GameRecoilHoldTime,
                                         RECOIL_DEFAULT_HOLD_TIME_MS);
}

int ypabact_GetRecoilReturnTimeMs()
{
    return ypabact_ReadRecoilPhaseTimeMs(System::IniConf::GameRecoilReturnTime,
                                         RECOIL_DEFAULT_RETURN_TIME_MS);
}

float ypabact_RecoilKickCurve(float t)
{
    t = std::max(0.0f, std::min(t, 1.0f));
    // Fixed ease-out: the shot reacts promptly without making the curve itself
    // another authoring parameter.
    const float oneMinus = 1.0f - t;
    return 1.0f - oneMinus * oneMinus;
}

float ypabact_RecoilReturnCurve(float t)
{
    t = std::max(0.0f, std::min(t, 1.0f));
    // Normalized exponential return, chosen to preserve the old recoil feel.
    const float end = std::exp(-3.0f);
    return (std::exp(-3.0f * t) - end) / (1.0f - end);
}

void ypabact_EvaluateRecoilVisual(const NC_STACK_ypabact *bact,
                                         vec3d *offset, float *pitch)
{
    if ( offset )
        *offset = vec3d(0.0, 0.0, 0.0);
    if ( pitch )
        *pitch = 0.0f;
    if ( !bact || bact->_recoilVisualReturnEndTime <= bact->_recoilVisualPhaseStartTime )
        return;

    const int now = bact->_clock;
    if ( now >= bact->_recoilVisualReturnEndTime )
        return;

    vec3d visualOffset;
    float visualPitch = 0.0f;

    if ( now < bact->_recoilVisualKickEndTime &&
         bact->_recoilVisualKickEndTime > bact->_recoilVisualPhaseStartTime )
    {
        const float t = (float)(now - bact->_recoilVisualPhaseStartTime) /
                        (float)(bact->_recoilVisualKickEndTime - bact->_recoilVisualPhaseStartTime);
        const float f = ypabact_RecoilKickCurve(t);
        visualOffset = bact->_recoilVisualStartOffset +
                       (bact->_recoilVisualPeakOffset - bact->_recoilVisualStartOffset) * f;
        visualPitch = bact->_recoilVisualStartPitch +
                      (bact->_recoilVisualPeakPitch - bact->_recoilVisualStartPitch) * f;
    }
    else if ( now < bact->_recoilVisualHoldEndTime )
    {
        visualOffset = bact->_recoilVisualPeakOffset;
        visualPitch = bact->_recoilVisualPeakPitch;
    }
    else if ( bact->_recoilVisualReturnEndTime > bact->_recoilVisualHoldEndTime )
    {
        const float t = (float)(now - bact->_recoilVisualHoldEndTime) /
                        (float)(bact->_recoilVisualReturnEndTime - bact->_recoilVisualHoldEndTime);
        const float f = ypabact_RecoilReturnCurve(t);
        visualOffset = bact->_recoilVisualPeakOffset * f;
        visualPitch = bact->_recoilVisualPeakPitch * f;
    }

    if ( bact->_bact_type == BACT_TYPES_GUN )
    {
        // Gun envelope distances are local to the barrel, so turning during
        // the return cannot leave the recoil pointing towards an old shot.
        visualOffset = bact->_rotation.AxisZ() * visualOffset.z;
        visualPitch = 0.0f;
    }
    if ( offset )
        *offset = visualOffset;
    if ( pitch )
        *pitch = visualPitch;
}

bool ypabact_IsPlayerGunRecoilFirstPersonView(const NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_bact_type != BACT_TYPES_GUN ||
         bact->IsAlternativeViewActive() )
        return false;

    // Generic recoil must never add a second first-person kick to guns.
    // cockpit_gun_camera_recoil remains the sole gun camera recoil source.
    return bact->IsPlayerFirstPersonCameraActive() || bact->IsCockpitCameraActive();
}

float ypabact_GetRecoilVisualPitch(const NC_STACK_ypabact *bact)
{
    if ( !bact || ypabact_IsPlayerGunRecoilFirstPersonView(bact) )
        return 0.0f;

    if ( bact->_bact_type != BACT_TYPES_TANK )
        return 0.0f;

    float pitch = 0.0f;
    ypabact_EvaluateRecoilVisual(bact, nullptr, &pitch);
    return pitch;
}

void ypabact_StartRecoilVisual(NC_STACK_ypabact *bact,
                                      const vec3d &recoilDir, float recoil)
{
    if ( !bact || recoil <= 0.0f )
        return;

    vec3d currentOffset;
    float currentPitch = 0.0f;
    ypabact_EvaluateRecoilVisual(bact, &currentOffset, &currentPitch);

    const bool gun = bact->_bact_type == BACT_TYPES_GUN;
    vec3d peakOffset = gun
        ? vec3d(0.0, 0.0, -currentOffset.length() - recoil * RECOIL_DISTANCE_PER_UNIT)
        : currentOffset + recoilDir * (recoil * RECOIL_DISTANCE_PER_UNIT);
    const float offsetLen = peakOffset.length();
    if ( std::isfinite(offsetLen) && offsetLen > RECOIL_VISUAL_MAX_DISTANCE )
        peakOffset *= RECOIL_VISUAL_MAX_DISTANCE / offsetLen;

    const float pitchAdd = recoil * RECOIL_VISUAL_DEGREES_PER_UNIT * C_PI_180;
    const float maxPitch = RECOIL_VISUAL_MAX_DEGREES * C_PI_180;
    const float peakPitch = gun ? 0.0f : std::max(0.0f, std::min(currentPitch + pitchAdd, maxPitch));

    const int kick = ypabact_GetRecoilKickTimeMs();
    const int hold = ypabact_GetRecoilHoldTimeMs();
    const int ret = ypabact_GetRecoilReturnTimeMs();

    bact->_recoilVisualStartOffset = gun ? vec3d(0.0, 0.0, -currentOffset.length()) : currentOffset;
    bact->_recoilVisualPeakOffset = peakOffset;
    bact->_recoilVisualStartPitch = currentPitch;
    bact->_recoilVisualPeakPitch = peakPitch;
    bact->_recoilVisualPhaseStartTime = bact->_clock;
    bact->_recoilVisualKickEndTime = bact->_clock + kick;
    bact->_recoilVisualHoldEndTime = bact->_recoilVisualKickEndTime + hold;
    bact->_recoilVisualReturnEndTime = bact->_recoilVisualHoldEndTime + ret;
}

bool ypabact_IsAiTankRecoilUnit(const NC_STACK_ypabact *unit)
{
    return unit &&
           unit->_bact_type == BACT_TYPES_TANK &&
           !(unit->_oflags & BACT_OFLAG_VIEWER) &&
           !(unit->_oflags & BACT_OFLAG_USERINPT);
}

bool ypabact_UsesRenderOnlyRecoilTranslation(const NC_STACK_ypabact *unit)
{
    // AI tanks use the established render-only recoil translation.
    // Fixed/attached guns reuse the same shared envelope so recoil
    // cannot displace their logical pedestal/attachment position. This is the
    // recoil-only fall protection: normal pedestal loss/destruction still reaches
    // ypagun::CheckPedestal()/Die() unchanged, and gun_levitate is untouched.
    return unit && (ypabact_IsAiTankRecoilUnit(unit) ||
                    unit->_bact_type == BACT_TYPES_GUN);
}

bool ypabact_ShouldRenderRecoilVisualOffset(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    if ( !ypabact_UsesRenderOnlyRecoilTranslation(unit) )
        return false;

    // Externally viewed guns show the same kick. In first person the gun's
    // shared body recoil is intentionally invisible; cockpit_gun_camera_recoil
    // remains the only local camera recoil source.
    return unit->_bact_type != BACT_TYPES_GUN ||
           !ypabact_IsPlayerGunRecoilFirstPersonView(unit);
}

// One local shake per MGUN shot, only in the player's cockpit view. It reuses the
// shake carrier path of the weapon launch shake and never moves the vehicle.
void ypabact_TriggerPlayerMgunRecoilShake(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_cockpit_mgun_camera_recoil <= 0.0f || !bact->IsCockpitCameraActive() )
        return;

    const float recoilDegrees = std::min(
        bact->_cockpit_mgun_camera_recoil * MGUN_RECOIL_PLAYER_DEGREES_PER_UNIT,
        MGUN_RECOIL_PLAYER_MAX_DEGREES);

    bact->_mgun_recoil_shake.slot = 1;
    bact->_mgun_recoil_shake.mag0 = recoilDegrees * C_PI_180;
    bact->_mgun_recoil_shake.mag1 = 0.0f;
    bact->_mgun_recoil_shake.time = MGUN_RECOIL_PLAYER_SHAKE_DURATION_MS;
    bact->_mgun_recoil_shake.radius = 0.0f;
    bact->_mgun_recoil_shake.mute = 0.0f;
    bact->_mgun_recoil_shake.pos = vec3d(
        MGUN_RECOIL_PLAYER_SHAKE_AXIS_X,
        MGUN_RECOIL_PLAYER_SHAKE_AXIS_Y,
        MGUN_RECOIL_PLAYER_SHAKE_AXIS_Z);

    ypabact_TriggerLocalShakeCarrier(
        bact,
        &bact->_mgun_recoil_shake_carrier,
        &bact->_mgun_recoil_shake);
}

bool ypabact_IsBlastPushGroundAlignedUnit(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    if ( !(unit->_status_flg & BACT_STFLAG_LAND) )
        return false;

    return unit->_bact_type == BACT_TYPES_TANK ||
           unit->_bact_type == BACT_TYPES_CAR;
}

bool ypabact_IsGroundRecoilSurfaceUnit(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    if ( unit->_bact_type == BACT_TYPES_GUN )
        return true;

    if ( !(unit->_status_flg & BACT_STFLAG_LAND) )
        return false;

    return unit->_bact_type == BACT_TYPES_TANK ||
           unit->_bact_type == BACT_TYPES_CAR;
}

bool ypabact_ProbeLocalRecoilSupport(NC_STACK_ypabact *unit,
                                             ypaworld_arg136 *support)
{
    if ( !unit || !support || !ypabact_IsGroundRecoilSurfaceUnit(unit) )
        return false;

    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( !world )
        return false;

    // Recoil must stay attached to the surface immediately below the actor.
    // Probe world-down rather than actor AxisY because gun pitch is an aiming
    // transform. Starting near the body also excludes roofs/bridges above it.
    const float probeReach =
        std::max(2000.0f, std::fabs(unit->_height) * 6.0f + 1000.0f);
    support->stPos = unit->_position - vec3d::OY(10.0f);
    support->vect = vec3d::OY(probeReach);
    support->flags = 0;
    world->ypaworld_func136(support);

    // +Y is down in UA. A valid support hit must therefore be at or below the
    // actor origin, never on an overhead surface crossed by the short lead-in.
    return support->isect && std::isfinite(support->isectPos.y) &&
           support->isectPos.y >= unit->_position.y - 0.25f;
}

bool ypabact_GetRecoilSupportNormal(NC_STACK_ypabact *unit, vec3d *normal)
{
    if ( !normal )
        return false;

    ypaworld_arg136 support;
    if ( !ypabact_ProbeLocalRecoilSupport(unit, &support) ||
         !support.skel || support.polyID < 0 ||
         (size_t)support.polyID >= support.skel->polygons.size() )
        return false;

    vec3d n = support.skel->polygons[support.polyID].Normal();
    if ( !std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z) ||
         n.normalise() <= 0.001f || std::fabs(n.y) < 0.05f )
        return false;

    *normal = n;
    return true;
}

bool ypabact_ResolveRecoilDirection(NC_STACK_ypabact *unit,
                                           const vec3d &requestedDir,
                                           vec3d *resolvedDir)
{
    if ( !unit || !resolvedDir )
        return false;

    if ( unit->_bact_type == BACT_TYPES_GUN )
    {
        // A fixed gun recoils along its barrel, including elevation. Surface
        // projection belongs to moving vehicles, not to this visual slide.
        vec3d axis = -unit->_rotation.AxisZ();
        const double length = axis.length();
        if ( !std::isfinite(length) || length <= .001 ) return false;
        *resolvedDir = axis / length;
        return true;
    }

    auto resolveOne = [unit](vec3d dir, vec3d *out) -> bool
    {
        vec3d supportNormal;
        if ( ypabact_GetRecoilSupportNormal(unit, &supportNormal) )
        {
            // Project onto the actual support plane. This preserves backwards
            // travel along a hill/structure surface without letting barrel aim
            // or chassis pitch create a vertical launch component.
            dir -= supportNormal * dir.dot(supportNormal);
            const float len = dir.length();
            if ( std::isfinite(len) && len > 0.001f )
            {
                *out = dir / len;
                return true;
            }
        }

        if ( ypabact_NormalizeXZ(&dir) )
        {
            *out = dir;
            return true;
        }

        return false;
    };

    if ( resolveOne(requestedDir, resolvedDir) )
        return true;

    return resolveOne(-unit->_rotation.AxisZ(), resolvedDir);
}

bool ypabact_SnapBlastPushGroundUnit(NC_STACK_ypabact *unit)
{
    ypaworld_arg136 ground;
    if ( !ypabact_ProbeLocalRecoilSupport(unit, &ground) )
    {
        // Keep the old long-range fallback for steep drops, but never accept a
        // surface above the unit. Otherwise an overhead roof becomes the first
        // world hit and the pushed vehicle is snapped onto the building top.
        ground.stPos = unit->_position.X0Z() - vec3d::OY(30000.0);
        ground.vect = vec3d::OY(50000.0);
        ground.flags = 0;
        unit->getBACT_pWorld()->ypaworld_func136(&ground);

        if ( !ground.isect || !std::isfinite(ground.isectPos.y) ||
             ground.isectPos.y < unit->_position.y - 0.25f )
            return false;
    }

    // Snap only to support at or below the actor, never to geometry overhead.
    unit->_position.y = ground.isectPos.y -
        (unit->getBACT_viewer() ? unit->_viewer_overeof : unit->_overeof);
    unit->_status_flg |= BACT_STFLAG_LAND;
    return true;
}

bool ypabact_SnapRecoilGroundUnit(NC_STACK_ypabact *unit)
{
    ypaworld_arg136 ground;
    if ( !ypabact_ProbeLocalRecoilSupport(unit, &ground) )
        return false;

    unit->_position.y = ground.isectPos.y -
        (unit->getBACT_viewer() ? unit->_viewer_overeof : unit->_overeof);
    unit->_status_flg |= BACT_STFLAG_LAND;
    return true;
}

float ypabact_ClampRecoil(float recoil)
{
    if ( !(recoil > 0.0f) )
        return 0.0f;

    if ( recoil > RECOIL_MAX_INTENSITY )
        return RECOIL_MAX_INTENSITY;

    return recoil;
}

bool ypabact_ConstrainRecoilStepToLevelBox(NC_STACK_ypabact *unit,
                                                         const vec3d &requestedStep,
                                                         vec3d *constrainedStep,
                                                         bool *blockedX,
                                                         bool *blockedZ)
{
    *constrainedStep = requestedStep;
    *blockedX = false;
    *blockedZ = false;

    NC_STACK_ypaworld *world = unit ? unit->getBACT_pWorld() : nullptr;
    if ( !world )
        return false;

    const vec2d worldSize = World::SectorIDToPos2(world->GetMapSize());
    const float levelBoxEdge = World::CVSectorLength + 10.0f;
    const float minX = levelBoxEdge;
    const float maxX = worldSize.x - levelBoxEdge;
    const float minZ = worldSize.y + levelBoxEdge;
    const float maxZ = -levelBoxEdge;

    if ( !isfinite(worldSize.x) || !isfinite(worldSize.y) ||
         maxX < minX || maxZ < minZ )
        return false;

    vec3d candidate = unit->_position + requestedStep;

    if ( candidate.x < minX )
    {
        candidate.x = minX;
        *blockedX = true;
    }
    else if ( candidate.x > maxX )
    {
        candidate.x = maxX;
        *blockedX = true;
    }

    if ( candidate.z < minZ )
    {
        candidate.z = minZ;
        *blockedZ = true;
    }
    else if ( candidate.z > maxZ )
    {
        candidate.z = maxZ;
        *blockedZ = true;
    }

    *constrainedStep = candidate - unit->_position;
    return *blockedX || *blockedZ;
}

void ypabact_UpdateFakePushVel(NC_STACK_ypabact *unit, vec3d *pushVel, update_msg *arg,
                                      float tau, bool recoilMotion)
{
    NC_STACK_ypaworld *world = unit->getBACT_pWorld();
    if ( !world )
        return;

    if ( ypabact_ShouldFlattenAirKnockback(unit) )
        pushVel->y = 0.0f;

    float pushSpeed = pushVel->length();
    if ( !isfinite(pushSpeed) || pushSpeed < 1.0f )
    {
        *pushVel = vec3d(0.0, 0.0, 0.0);
        return;
    }

    float dtime = arg->frameTime / 1000.0;
    if ( !isfinite(dtime) || dtime <= 0.0f )
        return;

    if ( dtime > blast_PUSH_MAX_DT )
        dtime = blast_PUSH_MAX_DT;

    vec3d totalStep = *pushVel * dtime;
    float stepLen = totalStep.length();
    if ( !isfinite(stepLen) || stepLen <= 0.0f )
    {
        *pushVel = vec3d(0.0, 0.0, 0.0);
        return;
    }

    int slices = (int)ceil(stepLen / blast_PUSH_MAX_STEP);
    if ( slices < 1 )
        slices = 1;

    vec3d step = totalStep / (float)slices;
    bool groundAligned = ypabact_IsBlastPushGroundAlignedUnit(unit);

    for (int i = 0; i < slices; i++)
    {
        vec3d moveStep = step;
        bool blockedX = false;
        bool blockedZ = false;
        if ( recoilMotion &&
             ypabact_ConstrainRecoilStepToLevelBox(unit, step, &moveStep, &blockedX, &blockedZ) )
        {
            // Remove only the outward component. A recoil vector parallel to
            // the wall may still move the unit along the valid map area.
            if ( blockedX )
                step.x = pushVel->x = 0.0f;
            if ( blockedZ )
                step.z = pushVel->z = 0.0f;
        }

        if ( moveStep.length() <= 0.001f )
            break;

        ypaworld_arg136 moveTest;
        moveTest.stPos = unit->_position;
        moveTest.vect  = moveStep;
        moveTest.flags = 0;

        world->ypaworld_func136(&moveTest);

        if ( moveTest.isect )
        {
            *pushVel = vec3d(0.0, 0.0, 0.0);
            break;
        }

        unit->_position += moveStep;

        if ( groundAligned )
        {
            const bool snapped = recoilMotion
                ? ypabact_SnapRecoilGroundUnit(unit)
                : ypabact_SnapBlastPushGroundUnit(unit);
            if ( !snapped )
            {
                *pushVel = vec3d(0.0, 0.0, 0.0);
                break;
            }
        }

    }

    *pushVel *= expf(-dtime / tau);
}

float NC_STACK_ypabact::GetPushResistanceMultiplier() const
{
    if ( !_world )
        return 1.0f;

    const std::vector<World::TVhclProto> &protos = _world->GetVhclProtos();
    const uint8_t protoId = _mimic_disguise_vehicleID
        ? _mimic_disguise_vehicleID
        : _vehicleID;
    if ( protoId >= protos.size() )
        return 1.0f;

    float resistance = protos.at(protoId).push_resistance;
    if ( !isfinite(resistance) )
        return 0.0f;
    resistance = std::max(0.0f, std::min(resistance, 1.0f));
    return 1.0f - resistance;
}

bool NC_STACK_ypabact::CanReceiveConfiguredPush() const
{
    return _bact_type != BACT_TYPES_GUN;
}

void NC_STACK_ypabact::ApplyConfiguredPush(const vec3d &dir, float intensity)
{
    if ( !CanReceiveConfiguredPush() || !isfinite(intensity) || intensity <= 0.0f )
        return;

    const float pushIntensity =
        std::min(intensity, CONFIGURED_PUSH_MAX_INTENSITY);
    const float mechanicalDistance =
        pushIntensity * pushIntensity *
        CONFIGURED_PUSH_DISTANCE_PER_SQUARED_LEVEL;

    QueueConfiguredPush(dir, mechanicalDistance);
}

void NC_STACK_ypabact::QueueConfiguredPush(const vec3d &dir, float distance)
{
    if ( _bact_type == BACT_TYPES_GUN )
        return;

    if ( !isfinite(distance) || distance <= 0.0f )
        return;

    vec3d pushDir = dir;

    if ( ypabact_ShouldFlattenAirKnockback(this) )
    {
        if ( !ypabact_NormalizeXZ(&pushDir) )
            return;
    }

    float dirLen = pushDir.length();
    if ( !isfinite(dirLen) || dirLen <= 0.001f )
        return;

    _blastPushVel += (pushDir / dirLen) * (distance / blast_PUSH_TAU);
}

void NC_STACK_ypabact::ApplyRecoil(const vec3d &dir, float recoil)
{
    recoil = ypabact_ClampRecoil(recoil);
    if ( recoil <= 0.0f )
        return;

    // Landed tanks retain the established Weapon-recoil rule that recoil is
    // disabled while airborne.
    if ( _bact_type == BACT_TYPES_TANK && !(_status_flg & BACT_STFLAG_LAND) )
        return;

    vec3d recoilDir;
    if ( !ypabact_ResolveRecoilDirection(this, dir, &recoilDir) )
        return;

    // Normal Weapon recoil retains its established physical path.
    ypabact_StartRecoilVisual(this, recoilDir, recoil);

    if ( _bact_type == BACT_TYPES_TANK )
    {
        // Preserve the small controller-recovery guard around the unified kick.
        if ( !(_oflags & BACT_OFLAG_VIEWER) && !(_oflags & BACT_OFLAG_USERINPT) )
            _recoilAiRecoveryEndTime = _clock + RECOIL_AI_TANK_RECOVERY_MS;
        else
            _recoilPlayerRecoveryEndTime = _clock + RECOIL_PLAYER_TANK_RECOVERY_MS;
    }

    if ( ypabact_UsesRenderOnlyRecoilTranslation(this) )
    {
        // AI tanks and attached/fixed guns present the same recoil envelope
        // without moving their logical world position. For guns this is the
        // recoil-only fall protection: pedestal destruction and gun_levitate
        // remain entirely owned by ypagun and are untouched here.
        _recoilPushVel = vec3d(0.0, 0.0, 0.0);
        return;
    }

    // Mechanical recoil is Weapon-only. The stable impulse integrator remains
    // unchanged for normal Weapon recoil.
    _recoilPushVel += recoilDir *
        ((recoil * RECOIL_DISTANCE_PER_UNIT) / RECOIL_MECHANICAL_TAU);
}

void NC_STACK_ypabact::UpdateConfiguredPush(update_msg *arg)
{
    ypabact_UpdateFakePushVel(this, &_blastPushVel, arg, blast_PUSH_TAU, false);
}

void NC_STACK_ypabact::UpdateRecoilPush(update_msg *arg)
{
    if ( ypabact_UsesRenderOnlyRecoilTranslation(this) )
    {
        _recoilPushVel = vec3d(0.0, 0.0, 0.0);
        return;
    }

    ypabact_UpdateFakePushVel(this, &_recoilPushVel, arg,
                              RECOIL_MECHANICAL_TAU, true);
}

float NC_STACK_ypabact::GetRecoilForwardControlScale(const vec3d &forwardDir) const
{
    // Convert only the component of the still-active mechanical recoil that
    // opposes forward travel into a 0..1 engine-thrust scale. This deliberately
    // does not touch vehicle velocity: the normal movement integrator keeps its
    // inertia, while UpdateRecoilPush() contributes the actual backward motion.
    // Low recoil therefore causes only a small deceleration; sufficiently large
    // recoil can suppress forward thrust and let the physical kick win.
    vec3d forward = forwardDir;
    forward.y = 0.0f;
    const float forwardLen = forward.length();
    if ( !std::isfinite(forwardLen) || forwardLen <= 0.001f )
        return 1.0f;
    forward /= forwardLen;

    vec3d recoilVel = _recoilPushVel;
    recoilVel.y = 0.0f;
    const float opposingSpeed = -recoilVel.dot(forward);
    if ( !std::isfinite(opposingSpeed) || opposingSpeed <= 0.0f )
        return 1.0f;

    const float maxConfiguredRecoilSpeed =
        (RECOIL_MAX_INTENSITY * RECOIL_DISTANCE_PER_UNIT) / RECOIL_MECHANICAL_TAU;
    if ( maxConfiguredRecoilSpeed <= 0.001f )
        return 1.0f;

    const float load = std::max(0.0f, std::min(opposingSpeed / maxConfiguredRecoilSpeed, 1.0f));
    return 1.0f - load;
}

void NC_STACK_ypabact::Recoil(bact_arg88 *arg)
{
    if ( !(_status_flg & BACT_STFLAG_LAND) )
    {
        if ( _fly_dir.dot(arg->pos1) >= 0.0 )
        {
            if ( _fly_dir_length != 0.0 )
            {
                // CCD already chose a safe pose for the current orientation.
                // Restoring the old centre can undo its wall clearance.
                if (!HasCollisionShape()) _position = _old_pos;

                float v4 = _fly_dir.dot(arg->pos1) * 2.0;

                _fly_dir -= arg->pos1 * v4;

                _fly_dir_length *= 25.0 / (fabs(_fly_dir_length) + 10.0);
            }
        }
    }
}
