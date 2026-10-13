// Player sprint (accelerator, pitch scale, excess speed clamp) and its configuration.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"

World::TAbsoluteOrPercent yw_ParseSprintAbsoluteOrPercent(Common::Ini::Key &key,
                                                                  float maximumPercent)
{
    World::TAbsoluteOrPercent result;
    World::TAuthoredScalar parsed;
    if ( !World::ParseAuthoredScalar(key.Get<std::string>(), parsed) ||
         !std::isfinite(parsed.value) || parsed.value < 0.0f )
    {
        return result;
    }

    result.defined = true;
    result.percent = parsed.percent;
    result.value = parsed.percent ? std::min(parsed.value, maximumPercent) : parsed.value;
    return result;
}

float yw_ParseSprintExplicitPercent(Common::Ini::Key &key, float maximum)
{
    World::TAuthoredScalar parsed;
    if ( !World::ParseAuthoredScalar(key.Get<std::string>(), parsed) ||
         !parsed.percent || !std::isfinite(parsed.value) || parsed.value <= 0.0f )
    {
        return 0.0f;
    }
    return std::min(parsed.value, maximum);
}

int32_t yw_ParseSprintTime(Common::Ini::Key &key)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return 0;

    try
    {
        size_t pos = 0;
        const long parsed = std::stol(value, &pos, 0);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos )
            return 0;

        return (int32_t)std::max<long>(0, std::min<long>(parsed, 600000));
    }
    catch (...)
    {
        return 0;
    }
}

PlayerSprintConfig yw_GetPlayerSprintConfig()
{
    PlayerSprintConfig config;
    config.complete = System::IniConf::GameSprintForceUp.WasSet &&
                      System::IniConf::GameSprintPitchUp.WasSet &&
                      System::IniConf::GameSprintRampTime.WasSet;
    if ( !config.complete )
        return config;

    config.forceUp = yw_ParseSprintAbsoluteOrPercent(
        System::IniConf::GameSprintForceUp, 1000.0f);
    config.pitchUpPercent = yw_ParseSprintExplicitPercent(
        System::IniConf::GameSprintPitchUp, 100.0f);
    config.rampTime = yw_ParseSprintTime(System::IniConf::GameSprintRampTime);
    config.energyCost = yw_ParseSprintAbsoluteOrPercent(
        System::IniConf::GameSprintEnergyCost, 100.0f);
    return config;
}

bool NC_STACK_ypaworld::IsPlayerSprintEnabledFor(const NC_STACK_ypabact *bact) const
{
    const PlayerSprintConfig config = yw_GetPlayerSprintConfig();
    if ( _isNetGame || !config.complete || config.forceUp.value <= 0.0f ||
         !bact || bact != _userUnit || bact->_isDummy || IsSpectatorBact(bact) ||
         bact->IsActiveDebuffStunning() ||
         !bact->getBACT_inputting() ||
         (config.energyCost.value > 0.0f && bact->_energy <= 0) ||
         (bact->_status != BACT_STATUS_NORMAL && bact->_status != BACT_STATUS_IDLE) )
    {
        return false;
    }

    switch ( bact->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_ZEPP:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
        return true;
    default:
        return false;
    }
}

float NC_STACK_ypaworld::GetPlayerSprintForce(const NC_STACK_ypabact *bact) const
{
    const PlayerSprintConfig config = yw_GetPlayerSprintConfig();
    if ( !IsPlayerSprintEnabledFor(bact) || bact != _playerSprintUnit )
        return bact ? bact->_force : 0.0f;

    const float increase = config.forceUp.percent
        ? bact->_force * config.forceUp.value * 0.01f
        : config.forceUp.value;
    return bact->_force + increase * _playerSprintFactor;
}

float NC_STACK_ypaworld::GetPlayerSprintPitchScale(const NC_STACK_ypabact *bact) const
{
    const PlayerSprintConfig config = yw_GetPlayerSprintConfig();
    if ( !IsPlayerSprintEnabledFor(bact) || bact != _playerSprintUnit )
        return 0.0f;

    return config.pitchUpPercent * 0.01f * _playerSprintFactor;
}

bool NC_STACK_ypaworld::IsPlayerSprintActiveFor(const NC_STACK_ypabact *bact) const
{
    return IsPlayerSprintEnabledFor(bact) && bact == _playerSprintUnit &&
           _playerSprintFactor > 0.0f &&
           (_playerSprintState == PLAYER_SPRINT_RAMP_UP ||
            _playerSprintState == PLAYER_SPRINT_ACTIVE ||
            _playerSprintState == PLAYER_SPRINT_RAMP_DOWN);
}

bool NC_STACK_ypaworld::IsPlayerSprintInputHeld() const
{
    const PlayerSprintConfig config = yw_GetPlayerSprintConfig();
    if ( !_GameShell || _isNetGame || !config.complete ||
         config.forceUp.value <= 0.0f )
        return false;

    // This helper owns the configured physical Sprint key even while the player
    // is inside the Host Station. Actual acceleration remains gated separately
    // by IsPlayerSprintEnabledFor(), but the same Shift press can no longer leak
    // into the legacy waypoint system.
    const UserData::TInputConf &bind =
        _GameShell->InputConfig[World::INPUT_BIND_SPRINT];
    return bind.Type == World::INPUT_BIND_TYPE_HOTKEY &&
           bind.PKeyCode > Input::KC_NONE && bind.PKeyCode < Input::KC_MAX &&
           Input::Engine.GetKeyState(bind.PKeyCode);
}

bool yw_PlayerSprintAcceleratorPressed(const NC_STACK_ypabact *bact, const TInputState *inpt)
{
    if ( !bact || !inpt )
        return false;

    // Ground units use Drive Speed, while supported air classes use
    // Fly Speed.  Positive input is the forward/increase-thrust accelerator;
    // reverse or decrease-thrust input must not arm Sprint.
    switch ( bact->_bact_type )
    {
    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        return inpt->Sliders[4] > 0.001f;

    case BACT_TYPES_BACT:
    case BACT_TYPES_ZEPP:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
        return inpt->Sliders[2] > 0.001f;

    default:
        return false;
    }
}

bool yw_PlayerSprintCanRun(const NC_STACK_ypabact *bact, const TInputState *inpt)
{
    if ( !bact )
        return false;

    const bool airborne = !(bact->_status_flg & BACT_STFLAG_LAND);

    // Helicopters and UFOs may engage Sprint whenever they are airborne.
    // Their flight model supports useful thrust while hovering in place.
    if ( bact->_bact_type == BACT_TYPES_BACT ||
         bact->_bact_type == BACT_TYPES_UFO )
    {
        return airborne;
    }

    // Plane, glider and zeppelin scripts all use the flyer runtime class.
    // They must already have real translational speed: yawing in place changes
    // orientation only and must not count as forward/backward movement.  The
    // 1.5 threshold matches the flyer Move() cut-off below which position is
    // not advanced by the physics path.
    if ( bact->_bact_type == BACT_TYPES_FLYER ||
         bact->_bact_type == BACT_TYPES_ZEPP )
    {
        return airborne && fabs(bact->_fly_dir_length) > 1.5f;
    }

    // Ground units still cannot preload Sprint while stationary and must keep
    // their positive accelerator pressed. The small threshold matches the
    // movement cut-off already used by vehicle physics.
    return yw_PlayerSprintAcceleratorPressed(bact, inpt) &&
           fabs(bact->_fly_dir_length) > 0.1f;
}

void yw_ClampTankSprintExcessSpeed(NC_STACK_ypabact *bact,
                                           float sprintFactor,
                                           const World::TAbsoluteOrPercent &forceUp)
{
    if ( !bact || bact->_bact_type != BACT_TYPES_TANK || bact->_force <= 0.0f ||
         fabs(bact->_airconst_static) <= 0.001f )
        return;

    sprintFactor = std::max(0.0f, std::min(sprintFactor, 1.0f));
    const float forceIncrease = forceUp.percent
        ? bact->_force * forceUp.value * 0.01f
        : forceUp.value;
    const float effectiveForce = bact->_force + std::max(0.0f, forceIncrease) * sprintFactor;
    const float sprintSpeedLimit = fabs(effectiveForce / bact->_airconst_static);

    if ( fabs(bact->_fly_dir_length) > sprintSpeedLimit )
        bact->_fly_dir_length = bact->_fly_dir_length < 0.0f ? -sprintSpeedLimit : sprintSpeedLimit;
}

void NC_STACK_ypaworld::ResetPlayerSprint()
{
    _playerSprintState = PLAYER_SPRINT_READY;
    _playerSprintUnit = nullptr;
    _playerSprintPhaseElapsed = 0;
    _playerSprintFactor = 0.0f;
    _playerSprintEnergyRemainder = 0.0f;
    _playerSprintEnergyDrainElapsedMs = 0;
}

void NC_STACK_ypaworld::UpdatePlayerSprint(TInputState *inpt, int32_t frameTime)
{
    const PlayerSprintConfig sprintConfig = yw_GetPlayerSprintConfig();
    if ( !sprintConfig.complete || !IsPlayerSprintEnabledFor(_userUnit) )
    {
        ResetPlayerSprint();
        return;
    }

    if ( _playerSprintUnit != _userUnit )
    {
        ResetPlayerSprint();
        _playerSprintUnit = _userUnit;
    }

    // The explicit handbrake is an immediate gameplay cancel for Sprint.
    // It removes the temporary force and pitch bonus in the same frame and
    // blocks Sprint for as long as the handbrake remains held.  The handbrake
    // keeps ownership of the actual deceleration curve, so no extra velocity
    // clamp or prototype mutation is introduced here.
    if ( inpt && inpt->HandBrakePressed )
    {
        ResetPlayerSprint();
        return;
    }

    const bool sprintWasActive = _playerSprintUnit &&
                                 _playerSprintFactor > 0.0f &&
                                 (_playerSprintState == PLAYER_SPRINT_RAMP_UP ||
                                  _playerSprintState == PLAYER_SPRINT_ACTIVE ||
                                  _playerSprintState == PLAYER_SPRINT_RAMP_DOWN);
    if ( sprintWasActive && sprintConfig.energyCost.value > 0.0f && frameTime > 0 )
    {
        if ( !_playerSprintUnit->IsInvulnerableToDamage() )
        {
            const float sprintFactor = std::max(0.0f, std::min(_playerSprintFactor, 1.0f));
            const float configuredCostPerSecond = sprintConfig.energyCost.percent
                ? (float)std::max(_playerSprintUnit->_energy_max, 0) *
                  sprintConfig.energyCost.value * 0.01f
                : sprintConfig.energyCost.value;
            const float rawEnergyCost = configuredCostPerSecond *
                                        ((float)frameTime / 1000.0f) * sprintFactor;
            _playerSprintEnergyRemainder +=
                _playerSprintUnit->CalcShieldedActionEnergyCost(rawEnergyCost);
            _playerSprintEnergyDrainElapsedMs += frameTime;

            const int32_t drainIntervalMs = NC_STACK_ypabact::GetEnergyDrainIntervalMs(
                System::IniConf::GameSprintEnergyDrainIntervalMs);
            if ( drainIntervalMs <= 0 ||
                 _playerSprintEnergyDrainElapsedMs >= drainIntervalMs )
            {
                const int energyCost = (int)_playerSprintEnergyRemainder;
                if ( energyCost > 0 )
                {
                    _playerSprintEnergyRemainder -= energyCost;
                    _playerSprintUnit->_energy -= energyCost;
                    if ( _playerSprintUnit->_energy <= 0 )
                    {
                        _playerSprintUnit->_energy = 0;
                        ResetPlayerSprint();
                        return;
                    }
                }

                _playerSprintEnergyDrainElapsedMs = drainIntervalMs > 0
                    ? _playerSprintEnergyDrainElapsedMs % drainIntervalMs
                    : 0;
            }
        }
        else
        {
            _playerSprintEnergyRemainder = 0.0f;
            _playerSprintEnergyDrainElapsedMs = 0;
        }
    }

    const bool sprintRequested = IsPlayerSprintInputHeld() &&
                                 yw_PlayerSprintCanRun(_playerSprintUnit, inpt);

    const int32_t rampDuration = sprintConfig.rampTime;

    // Hold-to-sprint with no duration limit or cooldown. Sprint can only run
    // while the unit is moving and its forward accelerator is held. Pressing
    // again during the return ramp reverses it smoothly instead of waiting for
    // zero.
    if ( sprintRequested )
    {
        if ( _playerSprintState == PLAYER_SPRINT_READY )
        {
            _playerSprintState = PLAYER_SPRINT_RAMP_UP;
            _playerSprintPhaseElapsed = 0;
            _playerSprintFactor = 0.0f;
        }
        else if ( _playerSprintState == PLAYER_SPRINT_RAMP_DOWN )
        {
            if ( rampDuration == 0 )
            {
                _playerSprintState = PLAYER_SPRINT_ACTIVE;
                _playerSprintPhaseElapsed = 0;
                _playerSprintFactor = 1.0f;
            }
            else
            {
                _playerSprintState = PLAYER_SPRINT_RAMP_UP;
                _playerSprintPhaseElapsed = (int32_t)(_playerSprintFactor * rampDuration);
                _playerSprintPhaseElapsed = std::max(0, std::min(_playerSprintPhaseElapsed, rampDuration));
            }
        }
    }

    if ( !sprintRequested && (_playerSprintState == PLAYER_SPRINT_RAMP_UP ||
                              _playerSprintState == PLAYER_SPRINT_ACTIVE) )
    {
        if ( rampDuration == 0 || _playerSprintFactor <= 0.0f )
        {
            yw_ClampTankSprintExcessSpeed(_playerSprintUnit, 0.0f,
                                          sprintConfig.forceUp);
            _playerSprintState = PLAYER_SPRINT_READY;
            _playerSprintPhaseElapsed = 0;
            _playerSprintFactor = 0.0f;
        }
        else
        {
            _playerSprintState = PLAYER_SPRINT_RAMP_DOWN;
            _playerSprintPhaseElapsed = (int32_t)((1.0f - _playerSprintFactor) * rampDuration);
            _playerSprintPhaseElapsed = std::max(0, std::min(_playerSprintPhaseElapsed, rampDuration));
        }
    }

    int32_t remaining = std::max(frameTime, 0);
    while ( remaining > 0 )
    {
        int32_t phaseDuration = 0;
        switch ( _playerSprintState )
        {
        case PLAYER_SPRINT_READY:
            _playerSprintFactor = 0.0f;
            return;

        case PLAYER_SPRINT_RAMP_UP:
            phaseDuration = rampDuration;
            if ( phaseDuration == 0 )
            {
                _playerSprintState = PLAYER_SPRINT_ACTIVE;
                _playerSprintPhaseElapsed = 0;
                _playerSprintFactor = 1.0f;
                continue;
            }
            break;

        case PLAYER_SPRINT_ACTIVE:
            _playerSprintFactor = 1.0f;
            return;

        case PLAYER_SPRINT_RAMP_DOWN:
            phaseDuration = rampDuration;
            if ( phaseDuration == 0 )
            {
                yw_ClampTankSprintExcessSpeed(_playerSprintUnit, 0.0f,
                                              sprintConfig.forceUp);
                _playerSprintState = PLAYER_SPRINT_READY;
                _playerSprintPhaseElapsed = 0;
                _playerSprintFactor = 0.0f;
                continue;
            }
            break;
        }

        const int32_t phaseRemaining = std::max(phaseDuration - _playerSprintPhaseElapsed, 0);
        const int32_t step = std::min(remaining, phaseRemaining);
        _playerSprintPhaseElapsed += step;
        remaining -= step;

        if ( _playerSprintState == PLAYER_SPRINT_RAMP_UP )
            _playerSprintFactor = (float)_playerSprintPhaseElapsed / (float)phaseDuration;
        else if ( _playerSprintState == PLAYER_SPRINT_RAMP_DOWN )
        {
            _playerSprintFactor = 1.0f - (float)_playerSprintPhaseElapsed / (float)phaseDuration;
            // Tank traction returns to vanilla immediately, but its accumulated
            // overspeed (and therefore the vanilla engine pitch) otherwise lingers
            // until the player brakes. Recover only the sprint-created excess
            // along the same smooth ramp-down curve.
            yw_ClampTankSprintExcessSpeed(_playerSprintUnit, _playerSprintFactor,
                                          sprintConfig.forceUp);
        }

        if ( _playerSprintPhaseElapsed < phaseDuration )
            return;

        _playerSprintPhaseElapsed = 0;
        switch ( _playerSprintState )
        {
        case PLAYER_SPRINT_RAMP_UP:
            _playerSprintState = PLAYER_SPRINT_ACTIVE;
            _playerSprintFactor = 1.0f;
            break;
        case PLAYER_SPRINT_RAMP_DOWN:
            yw_ClampTankSprintExcessSpeed(_playerSprintUnit, 0.0f,
                                          sprintConfig.forceUp);
            _playerSprintState = PLAYER_SPRINT_READY;
            _playerSprintFactor = 0.0f;
            break;
        default:
            break;
        }
    }
}
