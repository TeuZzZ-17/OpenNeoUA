// Spectator mode: spectator vehicle, follow target and observer camera.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include "nucleus/loaders.h"
#include "game/yparobo.h"
#include "gui/font.h"
#include "gui/uacommon.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "world/spin.h"
#include "common/crashdiag.h"
#include "game/yw_game_impl.h"

bool NC_STACK_ypaworld::IsSpectatorModeEnabled() const
{
    return System::IniConf::GameSpectatorMode.Get<bool>();
}

bool NC_STACK_ypaworld::IsSpectatorVehicleID(int vehicleID) const
{
    return IsSpectatorModeEnabled() &&
           _spectatorVehicleProtoID > 0 &&
           vehicleID == _spectatorVehicleProtoID;
}

bool NC_STACK_ypaworld::IsSpectatorBact(const NC_STACK_ypabact *bact) const
{
    return bact && IsSpectatorVehicleID(bact->_vehicleID);
}

bool NC_STACK_ypaworld::IsSpectatorControlled() const
{
    return IsSpectatorBact(_userUnit);
}

bool NC_STACK_ypaworld::CanControlUnitInSpectatorMode(const NC_STACK_ypabact *bact) const
{
    if ( !IsSpectatorControlled() )
        return true;

    return IsSpectatorBact(bact);
}

bool yw_IsBactInSpectatorFollowTree(NC_STACK_ypabact *node, NC_STACK_ypabact *target)
{
    if ( !node )
        return false;

    if ( node == target )
        return true;

    for ( NC_STACK_ypabact *kid : node->_kidList )
    {
        if ( yw_IsBactInSpectatorFollowTree(kid, target) )
            return true;
    }

    return false;
}

bool NC_STACK_ypaworld::IsSpectatorFollowActive() const
{
    return _spectatorFollowTarget != nullptr;
}

bool NC_STACK_ypaworld::IsValidSpectatorFollowTarget(NC_STACK_ypabact *bact) const
{
    if ( !IsSpectatorControlled() || !bact )
        return false;

    bool found = false;
    for ( NC_STACK_ypabact *unit : _unitsList )
    {
        if ( yw_IsBactInSpectatorFollowTree(unit, bact) )
        {
            found = true;
            break;
        }
    }

    if ( !found )
        return false;

    return bact->_owner != World::OWNER_0 &&
           !IsSpectatorBact(bact) &&
           bact->_status != BACT_STATUS_DEAD &&
           bact->_status != BACT_STATUS_CREATE &&
           bact->_status != BACT_STATUS_BEAM &&
           bact->_bact_type != BACT_TYPES_MISSLE &&
           !bact->ShouldHideFromStrategicUI();
}

void NC_STACK_ypaworld::SetSpectatorFollowTarget(NC_STACK_ypabact *bact)
{
    if ( !IsValidSpectatorFollowTarget(bact) )
        return;

    _spectatorFollowTarget = bact;
    _viewerBact = bact;
    _spectatorFollowDistance = 900.0;
    _spectatorFollowTargetDistance = _spectatorFollowDistance;
    _spectatorFollowPitch = 0.25;
    _spectatorFollowYaw = atan2(-bact->_rotation.m20, bact->_rotation.m22);
    _spectatorFollowTerrainRaise = 0.0;
    _spectatorFollowCameraInitialized = false;

    if ( _userUnit && IsSpectatorBact(_userUnit) )
        _userUnit->setBACT_inputting(false);

    _mouseGrabbed = false;
}

void NC_STACK_ypaworld::ClearSpectatorFollowTarget()
{
    _spectatorFollowTarget = nullptr;
    _spectatorFollowTerrainRaise = 0.0;
    _spectatorFollowCameraInitialized = false;
}

void NC_STACK_ypaworld::ReturnToSpectatorVehicle()
{
    ClearSpectatorFollowTarget();

    if ( _userUnit && IsSpectatorBact(_userUnit) )
    {
        _viewerBact = _userUnit;
        _userUnit->setBACT_viewer(true);
        _userUnit->setBACT_inputting(true);
    }

    _mouseGrabbed = false;
}

bool NC_STACK_ypaworld::UpdateSpectatorFollowCamera(TInputState *inpt)
{
    if ( !IsSpectatorFollowActive() )
        return false;

    if ( !IsValidSpectatorFollowTarget(_spectatorFollowTarget) )
    {
        ReturnToSpectatorVehicle();
        return false;
    }

    // OpenNeoUA custom: Spectator Follow camera uses one tank-style framing rule
    // for every vehicle class.  The old code treated non-ground units with a
    // higher/stranger focus, which made planes, helicopters and Host Stations
    // look inconsistent or clip the camera into large models.  Use the same
    // low, readable tank-style orbit for all targets, then scale distance by
    // the actor's scripted collision/view extents so very large ROBO/Host
    // Stations keep the camera outside their model.
    float targetRadius = _spectatorFollowTarget->_radius;
    if ( _spectatorFollowTarget->_viewer_radius > targetRadius )
        targetRadius = _spectatorFollowTarget->_viewer_radius;
    if ( targetRadius < 80.0f )
        targetRadius = 80.0f;

    float targetExtent = targetRadius;
    if ( fabs(_spectatorFollowTarget->_overeof) > targetExtent )
        targetExtent = fabs(_spectatorFollowTarget->_overeof);
    if ( fabs(_spectatorFollowTarget->_viewer_overeof) > targetExtent )
        targetExtent = fabs(_spectatorFollowTarget->_viewer_overeof);
    if ( fabs(_spectatorFollowTarget->_height) * 0.35f > targetExtent )
        targetExtent = fabs(_spectatorFollowTarget->_height) * 0.35f;
    if ( targetExtent < 80.0f )
        targetExtent = 80.0f;

    bool followTargetIsRobo = _spectatorFollowTarget->_bact_type == BACT_TYPES_ROBO;

    float followMinDistance = targetExtent * 1.65f;
    if ( followMinDistance < 220.0f )
        followMinDistance = 220.0f;

    float followMaxDistance = followMinDistance * 3.0f;
    if ( followMaxDistance < 1300.0f )
        followMaxDistance = 1300.0f;

    if ( followTargetIsRobo )
    {
        // Host Stations/ROBO use very large vertical/viewer extents, so using
        // targetExtent for zoom-in keeps the camera stuck too far away.  Use
        // the horizontal radius for the close limit and keep the larger extent
        // only for focus/terrain safety.  This allows a real zoom-in while
        // still avoiding the obvious inside-model view.
        followMinDistance = targetRadius * 0.55f;
        if ( followMinDistance < 520.0f )
            followMinDistance = 520.0f;
        if ( followMinDistance > 700.0f )
            followMinDistance = 700.0f;

        // Use the same zoom-out logic as normal vehicles after the
        // ROBO close-zoom override.  The previous ROBO minimum max-distance
        // was intentionally safe but too far away, making zoom-out excessive.
        followMaxDistance = followMinDistance * 3.0f;
        if ( followMaxDistance < 1300.0f )
            followMaxDistance = 1300.0f;
    }

    if ( followMaxDistance > 3600.0f )
        followMaxDistance = 3600.0f;
    if ( followMinDistance > followMaxDistance )
        followMinDistance = followMaxDistance;

    // A newly selected follow target can have very different viewer bounds
    // from the previous one.  Initialise both zoom values inside the valid
    // range before accepting wheel input, otherwise the first zoom tick can
    // expose the old/default 900-unit camera state for one frame and look like
    // a hard camera cut before the normal interpolation takes over.
    if ( !_spectatorFollowCameraInitialized )
    {
        if ( _spectatorFollowDistance < followMinDistance )
            _spectatorFollowDistance = followMinDistance;
        else if ( _spectatorFollowDistance > followMaxDistance )
            _spectatorFollowDistance = followMaxDistance;

        _spectatorFollowTargetDistance = _spectatorFollowDistance;
    }

    float wheelStep = 180.0f;
    if ( targetExtent > 160.0f )
        wheelStep += (targetExtent - 160.0f) * 0.35f;
    if ( wheelStep > 520.0f )
        wheelStep = 520.0f;

    if ( inpt )
    {
        const float fperiod = inpt->Period / 1000.0;

        if ( _mouseGrabbed )
        {
            float yawInput = inpt->Sliders[10];
            float pitchInput = inpt->Sliders[11];

            if ( fabs(yawInput) < 0.01 )
                yawInput = 0.0;

            if ( fabs(pitchInput) < 0.025 )
                pitchInput = 0.0;

            _spectatorFollowYaw -= yawInput * 1.8 * fperiod;
            _spectatorFollowPitch += pitchInput * 1.35 * fperiod;

            if ( _spectatorFollowPitch < -0.75 )
                _spectatorFollowPitch = -0.75;
            else if ( _spectatorFollowPitch > 0.95 )
                _spectatorFollowPitch = 0.95;
        }

        if ( !IsRoboMapOpen() )
        {
            const int wheelDelta = inpt->ClickInf.wheel;
            int zoomSteps =
                World::GetFixedInputShortcutWheelCount(World::INPUT_BIND_ZOOMIN, wheelDelta) -
                World::GetFixedInputShortcutWheelCount(World::INPUT_BIND_ZOOMOUT, wheelDelta);

            // Tactical map, UFO optical view and Spectator Follow are mutually
            // exclusive contexts, so all three reuse the same Zoom In/Out pair.
            const int zoomInHotKey =
                _GameShell->InputConfig[World::INPUT_BIND_ZOOMIN].KeyID;
            const int zoomOutHotKey =
                _GameShell->InputConfig[World::INPUT_BIND_ZOOMOUT].KeyID;

            if ( inpt->HotKeyID == zoomInHotKey )
            {
                zoomSteps++;
                inpt->HotKeyID = -1;
            }
            else if ( inpt->HotKeyID == zoomOutHotKey )
            {
                zoomSteps--;
                inpt->HotKeyID = -1;
            }

            if ( zoomSteps != 0 )
            {
                _spectatorFollowTargetDistance -= zoomSteps * wheelStep;
                inpt->ClickInf.wheel = 0;

                // Spectator Follow should never zoom out far enough to expose the
                // finite UA sky dome/skybox. Keep the camera cinematic and local,
                // but scale the useful range with large vehicles so ROBO/Host
                // Stations cannot swallow the camera.
                if ( _spectatorFollowTargetDistance < followMinDistance )
                    _spectatorFollowTargetDistance = followMinDistance;
                else if ( _spectatorFollowTargetDistance > followMaxDistance )
                    _spectatorFollowTargetDistance = followMaxDistance;
            }
        }

        if ( fperiod > 0.0 )
        {
            float zoomBlend = 1.0 - exp(-fperiod * 12.0);
            _spectatorFollowDistance += (_spectatorFollowTargetDistance - _spectatorFollowDistance) * zoomBlend;
        }
    }

    if ( _spectatorFollowTargetDistance < followMinDistance )
        _spectatorFollowTargetDistance = followMinDistance;
    else if ( _spectatorFollowTargetDistance > followMaxDistance )
        _spectatorFollowTargetDistance = followMaxDistance;

    if ( _spectatorFollowDistance < followMinDistance )
        _spectatorFollowDistance = followMinDistance;
    if ( _spectatorFollowDistance > followMaxDistance )
        _spectatorFollowDistance = followMaxDistance;

    // Tank-style camera for every class: never force non-ground units into an
    // excessively high/top-down view. This keeps spectator follow homogeneous.
    bool followTargetIsGround = true;
    if ( _spectatorFollowDistance < 1200.0 && _spectatorFollowPitch > 0.55 )
        _spectatorFollowPitch = 0.55;

    float cp = cos(_spectatorFollowPitch);
    float sp = sin(_spectatorFollowPitch);
    float sy = sin(_spectatorFollowYaw);
    float cy = cos(_spectatorFollowYaw);

    vec3d orbitForward(-sy * cp, -sp, cy * cp);
    if ( orbitForward.length() <= 0.001 )
        orbitForward = vec3d(0.0, 0.0, 1.0);
    else
        orbitForward = vec3d::Normalise(orbitForward);

    float targetHeight = targetExtent * 0.22f;
    if ( targetHeight < 45.0f )
        targetHeight = 45.0f;
    if ( targetHeight > 110.0f )
        targetHeight = 110.0f;

    if ( followTargetIsRobo )
    {
        targetHeight = targetExtent * 0.30f;
        if ( targetHeight < 140.0f )
            targetHeight = 140.0f;
        if ( targetHeight > 360.0f )
            targetHeight = 360.0f;
    }

    vec3d focus = _spectatorFollowTarget->_position + vec3d::OY(targetHeight);
    vec3d camPos = focus - orbitForward * _spectatorFollowDistance;

    // Keep the camera out of the terrain. A single final-position clamp is
    // not enough on steep ground: the camera ray can cross a higher sector
    // between the followed unit and the final camera position, especially for
    // ground units.  In UA coordinates smaller Y is higher, so "raising" the
    // camera means subtracting from camPos.y.
    float terrainSafeMargin = 140.0;
    int terrainSamples = 5;

    if ( followTargetIsGround )
    {
        if ( _spectatorFollowDistance < 900.0 )
        {
            terrainSafeMargin = 80.0;
            terrainSamples = 3;
        }
        else if ( _spectatorFollowDistance < 1400.0 )
        {
            terrainSafeMargin = 105.0;
            terrainSamples = 4;
        }
    }

    float raiseCamera = 0.0;

    for ( int i = 1; i <= terrainSamples; ++i )
    {
        float t = (float)i / (float)terrainSamples;
        vec3d sample = focus + (camPos - focus) * t;

        yw_130arg sectInfo;
        sectInfo.pos_x = sample.x;
        sectInfo.pos_z = sample.z;

        if ( GetSectorInfo(&sectInfo) && sectInfo.pcell )
        {
            float safeY = sectInfo.pcell->height - terrainSafeMargin;

            if ( sample.y > safeY )
            {
                float needed = (sample.y - safeY) / t;
                if ( needed > raiseCamera )
                    raiseCamera = needed;
            }
        }
    }

    // The required terrain lift can change discontinuously when zoom crosses
    // a sampling-distance boundary.  Blending only the orbit distance was not
    // enough: the first wheel tick after entering a vehicle could still snap
    // the final camera vertically, then become smooth on following frames.
    // Initialise the safe lift immediately for the new target, then smooth any
    // later changes so terrain protection remains intact without the visual
    // tear.
    if ( !_spectatorFollowCameraInitialized )
    {
        _spectatorFollowTerrainRaise = raiseCamera;
        _spectatorFollowCameraInitialized = true;
    }
    else if ( inpt && inpt->Period > 0 )
    {
        const float fperiod = inpt->Period / 1000.0f;
        const float terrainBlend = 1.0f - exp(-fperiod * 12.0f);
        _spectatorFollowTerrainRaise +=
            (raiseCamera - _spectatorFollowTerrainRaise) * terrainBlend;

        if ( fabs(_spectatorFollowTerrainRaise - raiseCamera) < 0.05f )
            _spectatorFollowTerrainRaise = raiseCamera;
    }

    if ( _spectatorFollowTerrainRaise > 0.0f )
        camPos.y -= _spectatorFollowTerrainRaise;

    // Rebuild the view matrix from the final camera position to the focus point
    // with world-down locked as the camera vertical reference. This keeps the
    // Spectator Follow camera roll-free, so horizontal mouse look no longer
    // banks/tilts the whole horizon.
    vec3d forward = focus - camPos;
    if ( forward.length() <= 0.001 )
        forward = orbitForward;
    else
        forward = vec3d::Normalise(forward);

    vec3d right = vec3d::OY(1.0) * forward;
    if ( right.length() <= 0.001 )
        right = vec3d(1.0, 0.0, 0.0);
    else
        right = vec3d::Normalise(right);

    vec3d down = forward * right;
    if ( down.length() <= 0.001 )
        down = vec3d::OY(1.0);
    else
        down = vec3d::Normalise(down);

    mat3x3 rot = mat3x3::Basis(right, down, forward);

    _viewerBact = _spectatorFollowTarget;
    _viewerPosition = camPos;
    _viewerRotation = rot;
    _spectatorFollowView.Pos = camPos;
    _spectatorFollowView.SclRot = rot;
    _spectatorFollowView.Scale = vec3d(1.0, 1.0, 1.0);
    _spectatorFollowView.Parent = nullptr;
    _spectatorFollowView.flags = 0;

    GFX::Engine.matrixAspectCorrection(_spectatorFollowView.SclRot, false);
    TF::Engine.SetViewPoint(&_spectatorFollowView);

    return true;
}

bool yw_IsUsableSpectatorBact(NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit)
{
    return unit &&
           yw &&
           yw->IsSpectatorBact(unit) &&
           unit->_status != BACT_STATUS_DEAD &&
           unit->_status != BACT_STATUS_CREATE &&
           unit->_status != BACT_STATUS_BEAM &&
           unit->_energy > 0 &&
           !(unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

NC_STACK_ypabact *yw_FindSpectatorBactInList(NC_STACK_ypaworld *yw, World::RefBactList &list)
{
    for (NC_STACK_ypabact *unit : list)
    {
        if ( yw_IsUsableSpectatorBact(yw, unit) )
            return unit;

        if ( NC_STACK_ypabact *kid = yw_FindSpectatorBactInList(yw, unit->_kidList) )
            return kid;
    }

    return nullptr;
}

void yw_ApplySpectatorObserverSafety(NC_STACK_ypabact *spectator)
{
    if ( !spectator )
        return;

    // Runtime-only observer restrictions. The Spectator remains its authored
    // vehicle class (normally model = ufo), so class-level movement/camera/UFO
    // features keep flowing through the normal implementation instead of a
    // parallel spectator path.
    // Owner 1 keeps the LDF-loaded setup, while the observer itself is
    // neutral/intangible and cannot participate in combat.
    spectator->_owner = World::OWNER_0;
    spectator->_m_owner = World::OWNER_0;
    spectator->_killer = nullptr;
    spectator->_killer_owner = World::OWNER_0;
    spectator->_host_station = nullptr;
    spectator->_weapon = -1;
    spectator->_mgun = -1;
    spectator->_weapon_flags = 0;
    spectator->_radar = 0;
    spectator->_unhideRadar = 0;
    spectator->_status_flg |= BACT_STFLAG_NOMSG;
    spectator->setBACT_bactCollisions(false);
    spectator->setBACT_exactCollisions(false);
}

float yw_ClampSpectatorCoord(float value, float minValue, float maxValue)
{
    if ( maxValue < minValue )
        return value;

    if ( value < minValue )
        return minValue;

    if ( value > maxValue )
        return maxValue;

    return value;
}

vec3d yw_GetSpectatorSpawnPosition(NC_STACK_ypaworld *yw)
{
    Common::Point cell(1, 1);
    if ( yw->_mapSize.x > 2 && yw->_mapSize.y > 2 )
        cell = Common::Point(yw->_mapSize.x / 2, yw->_mapSize.y / 2);

    // Spectator Mode is an observer mode, not owner-1 gameplay. Spawn the
    // observer in the map center instead of near the player Host Station.
    vec3d pos = World::SectorIDToCenterPos3(cell);

    const float halfSector = World::CVSectorHalfLength;
    pos.x = yw_ClampSpectatorCoord(pos.x, halfSector, yw->_mapLength.x - halfSector);
    pos.z = yw_ClampSpectatorCoord(pos.z, -yw->_mapLength.y + halfSector, -halfSector);

    ypaworld_arg136 ray;
    ray.stPos = vec3d(pos.x, -50000.0, pos.z);
    ray.vect = vec3d::OY(100000.0);
    ray.flags = 0;

    yw->ypaworld_func136(&ray);

    if ( ray.isect )
    {
        pos.y = ray.isectPos.y - 50.0;
    }
    else
    {
        yw_130arg sectInfo;
        sectInfo.pos_x = pos.x;
        sectInfo.pos_z = pos.z;

        if ( yw->GetSectorInfo(&sectInfo) )
            pos.y = sectInfo.pcell->height - 50.0;
    }

    return pos;
}

void NC_STACK_ypaworld::TryActivateSpectatorMode()
{
    // Detailed UFO Spy UI is normally opt-in. Reset it on every level load so
    // only Spectator Mode starts with the detailed layer enabled by default.
    _ufoSpyUiEnabled = false;

    if ( !System::IniConf::GameSpectatorMode.Get<bool>() || _levelInfo.Mode == 1 )
        return;

    if ( !LoadSpectatorVehicleProto() )
        return;

    int spectatorVehicleID = _spectatorVehicleProtoID;

    NC_STACK_ypabact *spectator = yw_FindSpectatorBactInList(this, _unitsList);

    if ( !spectator )
    {
        ypaworld_arg146 arg146;
        arg146.vehicle_id = spectatorVehicleID;
        arg146.pos = yw_GetSpectatorSpawnPosition(this);

        spectator = ypaworld_func146(&arg146);
        if ( !spectator )
        {
            ypa_log_out("WARNING: could not create spectator vehicle id %d.\n", spectatorVehicleID);
            return;
        }

        ypaworld_func134(spectator);
    }

    yw_ApplySpectatorObserverSafety(spectator);

    if ( _viewerBact && _viewerBact != spectator )
        _viewerBact->setBACT_viewer(false);

    if ( _userUnit && _userUnit != spectator )
        _userUnit->setBACT_inputting(false);

    spectator->setBACT_viewer(true);
    spectator->setBACT_inputting(true);

    // Spectator Mode starts with the normal UFO detailed Spy UI visible. The
    // regular UFO toggle (including the fixed middle-click shortcut) remains
    // authoritative afterwards, so the player can disable/re-enable it normally.
    _ufoSpyUiEnabled = true;

    // Spectator mode must not start with owner-1 squads pre-selected. The
    // original player faction is simulated by AI only; direct player command
    // selection remains disabled while the spectator is controlled.
    _activeCmdrID = 0;
    _activeCmdrRemapIndex = -1;
    _activeCmdrKidsCount = 0;
    _cmdrIdToSelect = -1;
    _cmdrsRemap.clear();
}
