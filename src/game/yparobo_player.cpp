// Host station under player control: mobile move, idle motion, resource trend and user commands.
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "common/log.h"
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "fmtlib/printf.h"
#include "system/inivals.h"
#include "common/crashdiag.h"
#include "world/tools.h"
#include <math.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>
#include <limits>
#include "game/yparobo_internal.h"

int ScalePlayerMobileMoveSecondaryCost(int cost, float mult)
{
    if ( cost <= 0 || mult <= 0.0f )
        return 0;

    int scaledCost = (int)ceil((float)cost * mult);
    if ( scaledCost < 1 )
        scaledCost = 1;

    return scaledCost;
}

float yparobo_ReadMobileEnergyCostMultiplier(Common::Ini::Key &key, float fallback)
{
    const std::string value = key.Get<std::string>();
    if ( value.empty() || value.find(',') != std::string::npos )
        return fallback;

    try
    {
        size_t pos = 0;
        const float parsed = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !std::isfinite(parsed) || parsed < 0.0f )
            return fallback;

        return parsed;
    }
    catch (...)
    {
        return fallback;
    }
}

int NC_STACK_yparobo::ScalePlayerMobileMoveEnergyCost(int cost)
{
    return ScalePlayerMobileMoveSecondaryCost(
        cost,
        yparobo_ReadMobileEnergyCostMultiplier(
            System::IniConf::GameRoboMobileMoveEnergyCostMultiplier,
            1.0f));
}

float ScalePlayerMobileMovePitchEffect(float speedPitchScale)
{
    if ( speedPitchScale != speedPitchScale || speedPitchScale < 0.0f )
        speedPitchScale = 0.0f;

    // Player Host Station relocation is slower than normal AI Robo movement,
    // so the raw legacy speed scale can be too subtle to hear from cockpit view.
    // Give any real movement a small audible floor, then cap it to avoid silly
    // cartoon-engine pitch spikes. This is player-mobile only; enemy Robos keep
    // the original Move() pitch behavior.
    if ( speedPitchScale > 0.0f && speedPitchScale < 0.08f )
        speedPitchScale = 0.08f;

    speedPitchScale *= 1.6f;

    if ( speedPitchScale > 0.70f )
        speedPitchScale = 0.70f;

    return speedPitchScale;
}

int PlayerMobileMoveScaledPitch(TSoundSource &snd, int basePitch, float speedPitchScale)
{
    float effect = ScalePlayerMobileMovePitchEffect(speedPitchScale);
    if ( effect <= 0.0f )
        return basePitch;

    int baseRate = 3000;
    if ( snd.PSample )
        baseRate = snd.PSample->SampleRate + basePitch;

    if ( baseRate < 1000 )
        baseRate = 1000;

    return basePitch + (int)((float)baseRate * effect);
}

bool NC_STACK_yparobo::TryStartPlayerMobileMove(update_msg *arg)
{
    if ( !_playerRoboAIBehavior || !IsPlayerRobo() )
        return false;

    if ( !_world || this != _world->getYW_userHostStation() )
        return false;

    if ( _status != BACT_STATUS_NORMAL || (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
        return false;

    if ( !arg || !arg->target_Sect )
        return false;

    if ( !_world->IsGamePlaySector(arg->target_Sect->CellId) )
        return false;

    // Start from the exact raw cost carried by the vanilla teleport command.
    // The strategic UI computes this value in ypaworld_func64__sub21__sub3(),
    // while the physical move applies the configurable multiplier at runtime.
    const int vanillaMobileEnergyCost = CalcPlayerMobileMoveEnergyCost(arg);
    const int mobileEnergyCost = ScalePlayerMobileMoveEnergyCost(vanillaMobileEnergyCost);

    // The UI should already reject unaffordable Host relocation commands, but
    // keep a runtime guard here so mobile mode cannot silently fall through to
    // vanilla beam relocation when the player lacks movement/beam energy.
    if ( mobileEnergyCost > _roboEnergyMove )
        return true;

    setTarget_msg arg67;
    arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
    arg67.priority = 0;
    arg67.tgt_pos = arg->target_point;

    SetTarget(&arg67);

    if ( _primTtype != BACT_TGT_TYPE_CELL || !_primT.pcell )
        return false;

    _playerRoboAIBehaviorMoveActive = true;
    _playerRoboAIBehaviorTargetCell = _primT.pcell;
    _playerRoboAIBehaviorTargetCellID = _primT.pcell->Id;

    _target_vec = _primTpos - _position;

    _playerRoboAIBehaviorEnergyTotal = mobileEnergyCost;
    _playerRoboAIBehaviorEnergyRemaining = (float)mobileEnergyCost;
    _playerRoboAIBehaviorEnergyRemainder = 0.0;

    // The vanilla teleport command spends only the beam / movement battery,
    // and that beam cost is still the authoritative relocation price.
    //
    // However, with robo_player_ai_behavior enabled the trip takes time.  If
    // only _roboEnergyMove is drained while the Host Station travels, the
    // vanilla player fill-mode balancer will later pull energy/creation down
    // in one visible lump when the movement finishes and beam rebalancing is
    // restored.
    //
    // To make the physical relocation feel continuous and punitive instead of
    // "free until arrival", also drain a distance-scaled penalty from the main
    // energy and creation/life batteries. Each resource uses its own multiplier
    // against the same raw vanilla relocation cost. The current defaults keep
    // the beam at 100% and the two extra player-only costs at 33%.
    _playerRoboAIBehaviorMainEnergyTotal = ScalePlayerMobileMoveSecondaryCost(
        vanillaMobileEnergyCost,
        yparobo_ReadMobileEnergyCostMultiplier(
            System::IniConf::GameRoboMobileMainEnergyCostMultiplier,
            0.33f));
    _playerRoboAIBehaviorBuildEnergyTotal = ScalePlayerMobileMoveSecondaryCost(
        vanillaMobileEnergyCost,
        yparobo_ReadMobileEnergyCostMultiplier(
            System::IniConf::GameRoboMobileBuildEnergyCostMultiplier,
            0.33f));
    _playerRoboAIBehaviorMoveEnergyTotal = mobileEnergyCost;

    _playerRoboAIBehaviorMainEnergyRemaining = (float)_playerRoboAIBehaviorMainEnergyTotal;
    _playerRoboAIBehaviorBuildEnergyRemaining = (float)_playerRoboAIBehaviorBuildEnergyTotal;
    _playerRoboAIBehaviorMoveEnergyRemaining = (float)_playerRoboAIBehaviorMoveEnergyTotal;
    _playerRoboAIBehaviorMainEnergyRemainder = 0.0;
    _playerRoboAIBehaviorBuildEnergyRemainder = 0.0;
    _playerRoboAIBehaviorMoveEnergyRemainder = 0.0;

    _playerRoboAIBehaviorTotalDistance = _target_vec.XZ().length();
    if ( _playerRoboAIBehaviorTotalDistance < 1.0 )
        _playerRoboAIBehaviorTotalDistance = 1.0;
    _playerRoboAIBehaviorLastDistance = _playerRoboAIBehaviorTotalDistance;

    return true;
}

bool NC_STACK_yparobo::ShouldUsePlayerMobileMove() const
{
    return _playerRoboAIBehavior &&
           _playerRoboAIBehaviorMoveActive &&
           IsPlayerRobo() &&
           _world &&
           this == _world->getYW_userHostStation() &&
           _status == BACT_STATUS_NORMAL &&
           !(_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

bool NC_STACK_yparobo::ShouldUsePlayerRoboIdleMotion() const
{
    // Manual Host Station movement must keep using the AI movement path after
    // arrival even when robo_does_flux / robo_does_twist are absent. Falling
    // back to wallow() would restore the vanilla teleport-only vertical bob
    // around the stale pre-move _roboYPos, causing a Y-axis snap at destination.
    // AI_doMove() keeps the reached position stable and still applies the two
    // optional idle effects when their script flags are present.
    return _playerRoboAIBehavior &&
           !_playerRoboAIBehaviorMoveActive &&
           IsPlayerRobo() &&
           _world &&
           this == _world->getYW_userHostStation() &&
           _status == BACT_STATUS_NORMAL &&
           !(_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

int NC_STACK_yparobo::CalcPlayerMobileMoveEnergyCost(update_msg *arg) const
{
    if ( _world && _world->IsDebugHostStationCheatEnabled() && IsPlayerRobo() )
        return 0;

    if ( !arg || !arg->target_Sect )
        return 0;

    // This is the exact value that vanilla DOACTION_10 would subtract from
    // _roboEnergyMove. Prefer it over recalculating from target_point, because
    // the UI can adjust the final target height after computing the preview
    // cost, and vanilla charges the already-computed message value.
    if ( arg->energy > 0 )
        return arg->energy;

    // Fallback only: keep the movement robust if some non-UI path ever sends a
    // relocation message without the preview energy filled in.
    vec3d delta = arg->target_point - _position;
    float distanceSq = POW2(delta.x) + POW2(delta.y) + POW2(delta.z);
    if ( distanceSq <= 0.0 )
        return 0;

    int cost = (int)(distanceSq / 230.4f);
    if ( cost < 0 )
        cost = 0;

    return cost;
}

bool NC_STACK_yparobo::UpdatePlayerMobileMove(update_msg *arg)
{
    if ( !ShouldUsePlayerMobileMove() )
        return false;

    AI_doMove(arg);

    bool finished = true;
    float currentDistance = 0.0;

    if ( _primTtype == BACT_TGT_TYPE_CELL && _playerRoboAIBehaviorTargetCell )
    {
        currentDistance = (_primTpos.XZ() - _position.XZ()).length();
        finished = currentDistance < 120.0;
    }

    UpdatePlayerMobileMoveEnergy(currentDistance, finished);

    if ( _playerRoboAIBehaviorMoveEnergyRemaining > 0.0 && _roboEnergyMove <= 0 )
    {
        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 0;
        SetTarget(&arg67);
        ResetPlayerMobileMove();
        return true;
    }

    if ( finished )
        ResetPlayerMobileMove();

    return true;
}

bool NC_STACK_yparobo::UpdatePlayerRoboIdleMotion(update_msg *arg)
{
    if ( !ShouldUsePlayerRoboIdleMotion() )
        return false;

    AI_doMove(arg);
    return true;
}

void NC_STACK_yparobo::ResetPlayerMobileMove()
{
    _playerRoboAIBehaviorMoveActive = false;
    _playerRoboAIBehaviorTargetCell = nullptr;
    _playerRoboAIBehaviorTargetCellID = 0;
    _playerRoboAIBehaviorEnergyTotal = 0;
    _playerRoboAIBehaviorEnergyRemaining = 0.0;
    _playerRoboAIBehaviorEnergyRemainder = 0.0;
    _playerRoboAIBehaviorMainEnergyTotal = 0;
    _playerRoboAIBehaviorMainEnergyRemaining = 0.0;
    _playerRoboAIBehaviorMainEnergyRemainder = 0.0;
    _playerRoboAIBehaviorBuildEnergyTotal = 0;
    _playerRoboAIBehaviorBuildEnergyRemaining = 0.0;
    _playerRoboAIBehaviorBuildEnergyRemainder = 0.0;
    _playerRoboAIBehaviorMoveEnergyTotal = 0;
    _playerRoboAIBehaviorMoveEnergyRemaining = 0.0;
    _playerRoboAIBehaviorMoveEnergyRemainder = 0.0;
    _playerRoboAIBehaviorTotalDistance = 0.0;
    _playerRoboAIBehaviorLastDistance = 0.0;
}

bool NC_STACK_yparobo::ShouldUsePlayerRoboResourceTrend() const
{
    return _playerRoboAIBehavior &&
           IsPlayerRobo() &&
           _world &&
           this == _world->getYW_userHostStation();
}

void NC_STACK_yparobo::ResetPlayerRoboResourceTrend()
{
    for (PlayerRoboResourceTrend &trend : _playerRoboResourceTrends)
    {
        trend.sampleValue = 0;
        trend.windowTime = 0;
        trend.holdTime = 0;
        trend.direction = 0;
        trend.initialized = false;
    }

    _playerRoboResourceTrendLoadFlags = 0;
    _playerRoboResourceTrendLossFlags = 0;
}

void NC_STACK_yparobo::UpdatePlayerRoboResourceTrendSlot(int slot, int value, uint8_t flag, int frameTime)
{
    PlayerRoboResourceTrend &trend = _playerRoboResourceTrends[slot];

    if ( !trend.initialized )
    {
        trend.sampleValue = value;
        trend.windowTime = 0;
        trend.holdTime = 0;
        trend.direction = 0;
        trend.initialized = true;
    }

    if ( frameTime < 0 )
        frameTime = 0;

    if ( trend.holdTime > 0 )
    {
        trend.holdTime -= frameTime;
        if ( trend.holdTime < 0 )
            trend.holdTime = 0;
    }

    trend.windowTime += frameTime;

    if ( trend.windowTime >= PLAYER_ROBO_RESOURCE_TREND_WINDOW_TIME )
    {
        int delta = value - trend.sampleValue;
        int newDirection = 0;

        if ( delta > PLAYER_ROBO_RESOURCE_TREND_THRESHOLD )
            newDirection = 1;
        else if ( delta < -PLAYER_ROBO_RESOURCE_TREND_THRESHOLD )
            newDirection = -1;

        trend.sampleValue = value;
        trend.windowTime = 0;

        if ( newDirection == 0 )
        {
            if ( trend.holdTime <= 0 )
                trend.direction = 0;
        }
        else if ( trend.direction == 0 || trend.direction == newDirection || trend.holdTime <= 0 )
        {
            trend.direction = newDirection;
            trend.holdTime = PLAYER_ROBO_RESOURCE_TREND_HOLD_TIME;
        }
    }

    if ( trend.direction > 0 )
        _playerRoboResourceTrendLoadFlags |= flag;
    else if ( trend.direction < 0 )
        _playerRoboResourceTrendLossFlags |= flag;
}

void NC_STACK_yparobo::UpdatePlayerRoboResourceTrend(int frameTime)
{
    _playerRoboResourceTrendLoadFlags = 0;
    _playerRoboResourceTrendLossFlags = 0;

    if ( !ShouldUsePlayerRoboResourceTrend() )
    {
        ResetPlayerRoboResourceTrend();
        return;
    }

    UpdatePlayerRoboResourceTrendSlot(0, _energy, 1, frameTime);
    UpdatePlayerRoboResourceTrendSlot(1, _roboEnergyLife, 4, frameTime);
    UpdatePlayerRoboResourceTrendSlot(2, _roboEnergyMove, 8, frameTime);
}

void NC_STACK_yparobo::UpdatePlayerMobileMoveEnergy(float currentTargetDistance, bool forceFinish)
{
    if ( _playerRoboAIBehaviorEnergyRemaining <= 0.0 || _playerRoboAIBehaviorEnergyTotal <= 0 )
        return;

    float progressDistance = 0.0;

    if ( forceFinish )
    {
        progressDistance = _playerRoboAIBehaviorTotalDistance;
    }
    else if ( currentTargetDistance < _playerRoboAIBehaviorLastDistance )
    {
        progressDistance = _playerRoboAIBehaviorLastDistance - currentTargetDistance;
    }

    if ( progressDistance <= 0.0 )
        return;

    float totalToDrain = ((float)_playerRoboAIBehaviorEnergyTotal * progressDistance / _playerRoboAIBehaviorTotalDistance) + _playerRoboAIBehaviorEnergyRemainder;
    int totalDrain = forceFinish ? (int)ceil(totalToDrain) : (int)floor(totalToDrain);

    if ( totalDrain <= 0 )
    {
        _playerRoboAIBehaviorEnergyRemainder = totalToDrain;
        if ( currentTargetDistance < _playerRoboAIBehaviorLastDistance )
            _playerRoboAIBehaviorLastDistance = currentTargetDistance;
        return;
    }

    if ( totalDrain > (int)ceil(_playerRoboAIBehaviorEnergyRemaining) )
        totalDrain = (int)ceil(_playerRoboAIBehaviorEnergyRemaining);

    DrainPlayerMobileMoveResource(_playerRoboAIBehaviorMainEnergyTotal,
                                  _playerRoboAIBehaviorMainEnergyRemaining,
                                  _playerRoboAIBehaviorMainEnergyRemainder,
                                  _energy,
                                  1,
                                  progressDistance,
                                  forceFinish);

    DrainPlayerMobileMoveResource(_playerRoboAIBehaviorBuildEnergyTotal,
                                  _playerRoboAIBehaviorBuildEnergyRemaining,
                                  _playerRoboAIBehaviorBuildEnergyRemainder,
                                  _roboEnergyLife,
                                  4,
                                  progressDistance,
                                  forceFinish);

    DrainPlayerMobileMoveResource(_playerRoboAIBehaviorMoveEnergyTotal,
                                  _playerRoboAIBehaviorMoveEnergyRemaining,
                                  _playerRoboAIBehaviorMoveEnergyRemainder,
                                  _roboEnergyMove,
                                  8,
                                  progressDistance,
                                  forceFinish);

    _playerRoboAIBehaviorEnergyRemaining -= (float)totalDrain;
    if ( _playerRoboAIBehaviorEnergyRemaining < 0.0 )
        _playerRoboAIBehaviorEnergyRemaining = 0.0;

    _playerRoboAIBehaviorEnergyRemainder = forceFinish ? 0.0 : (totalToDrain - (float)totalDrain);

    if ( currentTargetDistance < _playerRoboAIBehaviorLastDistance )
        _playerRoboAIBehaviorLastDistance = currentTargetDistance;
}

void NC_STACK_yparobo::DrainPlayerMobileMoveResource(int resourceTotal, float &resourceRemaining, float &resourceRemainder, int &resourceValue, uint8_t lossFlag, float progressDistance, bool forceFinish)
{
    if ( resourceTotal <= 0 || resourceRemaining <= 0.0 )
        return;

    float energyToDrain = 0.0;

    if ( forceFinish )
        energyToDrain = resourceRemaining + resourceRemainder;
    else
        energyToDrain = ((float)resourceTotal * progressDistance / _playerRoboAIBehaviorTotalDistance) + resourceRemainder;

    if ( energyToDrain <= 0.0 )
        return;

    int drain = forceFinish ? (int)ceil(energyToDrain) : (int)floor(energyToDrain);

    if ( drain <= 0 )
    {
        resourceRemainder = energyToDrain;
        return;
    }

    if ( drain > resourceValue )
        drain = resourceValue;

    if ( drain > (int)ceil(resourceRemaining) )
        drain = (int)ceil(resourceRemaining);

    if ( drain <= 0 )
    {
        resourceRemainder = energyToDrain;
        return;
    }

    resourceValue -= drain;
    if ( resourceValue < 0 )
        resourceValue = 0;

    resourceRemaining -= (float)drain;
    if ( resourceRemaining < 0.0 )
        resourceRemaining = 0.0;

    resourceRemainder = forceFinish ? 0.0 : (energyToDrain - (float)drain);
    _roboEnergyLossFlags |= lossFlag;
}

void NC_STACK_yparobo::ResetPlayerMobileCockpitPitch()
{
    if ( _soundcarrier.Sounds.size() <= World::TVhclProto::SND_COCKPIT )
        return;

    TSoundSource &cockpit = _soundcarrier.Sounds[World::TVhclProto::SND_COCKPIT];
    cockpit.Pitch = cockpit.PitchBase;
}

void NC_STACK_yparobo::ApplyPlayerMobileMovePitch(float speedPitchScale)
{
    if ( !_playerRoboAIBehavior || !_playerRoboAIBehaviorMoveActive )
        return;

    if ( _soundcarrier.Sounds.size() > World::TVhclProto::SND_NORMAL )
    {
        TSoundSource &normal = _soundcarrier.Sounds[World::TVhclProto::SND_NORMAL];
        normal.Pitch = PlayerMobileMoveScaledPitch(normal, normal.PitchBase, speedPitchScale);
    }

    if ( _soundcarrier.Sounds.size() <= World::TVhclProto::SND_COCKPIT )
        return;

    TSoundSource &cockpit = _soundcarrier.Sounds[World::TVhclProto::SND_COCKPIT];
    const int cockpitBasePitch = cockpit.PitchBase;

    // Enemy Robos get movement pitch on SND_NORMAL through Move(). The player can
    // hear either the normal loop or the cockpit loop depending on control/audio path,
    // so update both while physical relocation is active.
    cockpit.Pitch = PlayerMobileMoveScaledPitch(cockpit, cockpitBasePitch, speedPitchScale);
}

void NC_STACK_yparobo::doUserCommands(update_msg *arg)
{
    setTarget_msg arg67;
    ypaworld_arg146 arg146;
    setState_msg arg78;
    bact_arg109 arg109;
    ypaworld_arg148 arg148;
    robo_arg134 arg134;
    bact_arg124 arg124;

    int v6;

    switch ( arg->user_action )
    {
    case World::DOACTION_2: //Move squad
        arg->selectBact->_status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);

        if ( arg->target_Sect )
        {
            arg67.tgt_pos.x = arg->target_point.x;
            arg67.tgt_pos.z = arg->target_point.z;
            arg67.tgt_type = BACT_TGT_TYPE_CELL;
        }
        else
        {
            arg67.tgt_type = BACT_TGT_TYPE_UNIT;
            arg67.tgt.pbact = arg->target_Bact;
        }

        arg67.priority = 0;
        arg->selectBact->SetTarget(&arg67);

        if ( !sb_0x4a45cc__sub0(arg->selectBact) )
        {
            arg109.field_4 = nullptr;
            arg109.field_0 = 6;

            arg->selectBact->ReorganizeGroup(&arg109);
        }

        if ( arg->selectBact->_host_station != arg->selectBact->_parent &&
             arg->selectBact->_parent )
            arg->selectBact = arg->selectBact->_parent;

        if ( arg->target_Sect )
            v6 = sub_4A5A08(arg->selectBact, arg->target_point.x, arg->target_point.z);
        else
            v6 = sub_4A58C0(arg->selectBact, arg->target_Bact);

        if ( !v6 )
        {
            if ( _world->_GameShell )
                SFXEngine::SFXe.startSound(&_world->_GameShell->samples1_info, World::SOUND_ID_ERROR);
        }
        else
        {

            NC_STACK_ypabact *v8 = arg->selectBact;

            if ( v8->_parent != v8->_host_station &&
                 v8->_parent )
                v8 = v8->_parent;

            arg134.unit = v8;
            arg134.field_C = 0;
            arg134.field_8 = 0;
            arg134.field_10 = 0;
            arg134.field_14 = 38;

            if ( v8->_primTtype == BACT_TGT_TYPE_CELL )
            {
                if ( v8->_primT.pcell->owner == _owner )
                {
                    arg134.field_4 = 15;
                }
                else
                {
                    arg134.field_4 = 16;
                }

                placeMessage(&arg134);
            }
            else if ( v8->_primTtype == BACT_TGT_TYPE_UNIT )
            {
                if ( v8->_primT.pbact->_owner == _owner )
                {
                    arg134.field_4 = 15;
                }
                else
                {
                    arg134.field_4 = 16;
                }

                placeMessage(&arg134);
            }
        }
        sub_4A4538(arg->selectBact);
        arg->selectBact->ChangeEscapeFlag(false);
        break;

    case World::DOACTION_ADD_UNIT1:
        if ( arg->energy <= _roboEnergyLife )
        {
            arg146.pos = arg->target_point;
            arg146.vehicle_id = arg->protoID;

            NC_STACK_ypabact *newbact = yparobo_TryCreateGenesisUnit(this, &arg146);

            if ( newbact )
            {
                arg67.tgt_type = BACT_TGT_TYPE_CELL;
                arg67.priority = 0;
                arg67.tgt_pos.x = (arg146.pos.x - _position.x) * 4.0 + _position.x;
                arg67.tgt_pos.y = arg146.pos.y;
                arg67.tgt_pos.z = (arg146.pos.z - _position.z) * 4.0 + _position.z;

                newbact->SetTarget(&arg67);

                arg78.newStatus = BACT_STATUS_CREATE;
                arg78.setFlags = 0;
                arg78.unsetFlags = 0;

                newbact->SetState(&arg78);

                newbact->_scale_time = newbact->_energy_max * 0.2;
                newbact->_commandID = dword_5B1128;
                dword_5B1128++;

                newbact->_owner = _owner;
                AddSubject(newbact);

                if ( _world->_isNetGame )
                {
                    newbact->_gid |= newbact->_owner << 24;
                    newbact->_commandID |= newbact->_owner << 24;

                    uamessage_newVhcl nvMsg;
                    nvMsg.msgID = UAMSG_NEWVHCL;
                    nvMsg.owner = newbact->_owner;
                    nvMsg.pos = newbact->_position;
                    nvMsg.type = newbact->_vehicleID;
                    nvMsg.parent = newbact->_parent->_gid;
                    nvMsg.id = newbact->_gid;
                    nvMsg.kind = 1;
                    nvMsg.commandID = newbact->_commandID;

                    _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
                }

                if ( newbact->_bact_type == BACT_TYPES_UFO )
                    newbact->setBACT_aggression(25);
                else
                    newbact->setBACT_aggression(60);

                int v67 = getBACT_bactCollisions();
                newbact->setBACT_bactCollisions(v67);

                newbact->_host_station = this;
                newbact->InheritActiveDebuffFromParent(this);
                _roboEnergyLife -= arg->energy;

                _world->SetCmdrIdToSelect(newbact->_commandID); // Select it for add next units

                _world->HistoryAktCreate(newbact);
                yparobo_AdvanceMimicProductionCost(_world, newbact->_vehicleID);
            }
        }
        break;

    case World::DOACTION_ADD_UNIT2:
        if ( arg->energy <= _roboEnergyLife )
        {
            arg146.pos = arg->target_point;
            arg146.vehicle_id = arg->protoID;
            NC_STACK_ypabact *newbact2 = yparobo_TryCreateGenesisUnit(this, &arg146);

            if ( newbact2 )
            {
                arg78.newStatus = BACT_STATUS_CREATE;
                arg78.setFlags = 0;
                arg78.unsetFlags = 0;
                newbact2->SetState(&arg78);

                newbact2->_scale_time = newbact2->_energy_max * 0.2;
                newbact2->_owner = _owner;
                newbact2->_commandID = arg->selectBact->_commandID;

                arg->selectBact->AddSubject(newbact2);

                newbact2->_aggr = arg->selectBact->_aggr;

                if ( _world->_isNetGame )
                {
                    newbact2->_gid |= newbact2->_owner << 24;

                    uamessage_newVhcl nvMsg;
                    nvMsg.msgID = UAMSG_NEWVHCL;
                    nvMsg.owner = newbact2->_owner;
                    nvMsg.pos = newbact2->_position;
                    nvMsg.type = newbact2->_vehicleID;
                    nvMsg.parent = newbact2->_parent->_gid;
                    nvMsg.id = newbact2->_gid;
                    nvMsg.kind = 2;
                    nvMsg.commandID = newbact2->_commandID;

                    _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
                }

                int v67 = getBACT_bactCollisions();
                newbact2->setBACT_bactCollisions(v67);

                newbact2->_host_station = this;
                newbact2->InheritActiveDebuffFromParent(this);
                _roboEnergyLife -= arg->energy;

                _world->HistoryAktCreate(newbact2);
                yparobo_AdvanceMimicProductionCost(_world, newbact2->_vehicleID);
                newbact2->CopyTargetOf(arg->selectBact);
            }
        }
        break;

    case World::DOACTION_6:
        if ( arg->energy <= _roboEnergyLife )
        {
            arg148.owner = _owner;
            arg148.blg_ID = arg->protoID;
            arg148.field_C = 0;
            arg148.CellId = arg->target_Sect->CellId;
            arg148.field_18 = 0;
            if ( _world->ypaworld_func148(&arg148) )
            {
                _roboEnergyLife -= arg->energy;

                if ( _world->_isNetGame )
                {
                    uamessage_startBuild sbMsg;
                    sbMsg.msgID = UAMSG_STARTBUILD;
                    sbMsg.owner = _owner;
                    sbMsg.bproto = arg->protoID;
                    sbMsg.sec_x = arg->target_Sect->CellId.x;
                    sbMsg.sec_y = arg->target_Sect->CellId.y;

                    _world->NetBroadcastMessage(&sbMsg, sizeof(sbMsg), true);
                }
            }
        }
        break;

    case World::DOACTION_10:
        if ( TryStartPlayerMobileMove(arg) )
            break;

        if ( !(_roboState & ROBOSTATE_MOVE) )
        {
            SFXEngine::SFXe.startSound(&_soundcarrier, 9);

            _roboState |= ROBOSTATE_MOVE;
            _roboEnergyMove -= arg->energy;
            _roboBeamPos = arg->target_point;
            _roboBeamTimePre = 1500;
            if ( _world->_isNetGame )
            {
                uamessage_startBeam stbMsg;
                stbMsg.msgID = UAMSG_STARTBEAM;
                stbMsg.owner = _owner;
                stbMsg.id = _gid;
                stbMsg.pos = _roboBeamPos;

                _world->NetBroadcastMessage(&stbMsg, sizeof(stbMsg), true);
            }
        }
        break;

    case World::DOACTION_WP_BEGIN:
    {
        arg->selectBact->_status_flg |= BACT_STFLAG_WAYPOINT;
        arg->selectBact->_m_cmdID = 0;

        int v11 = 0;

        if ( arg->selectBact->_bact_type != BACT_TYPES_TANK && arg->selectBact->_bact_type != BACT_TYPES_CAR )
        {
            for( NC_STACK_ypabact* &node : arg->selectBact->_kidList )
            {
                if ( node->_bact_type == BACT_TYPES_TANK || node->_bact_type == BACT_TYPES_CAR )
                {
                    v11 = 1;
                    break;
                }
            }
        }
        else
            v11 = 1;

        if ( v11 )
        {
            arg124.from = arg->selectBact->_position.XZ();
            arg124.to = arg->target_point.XZ();
            arg124.steps_cnt = 32;
            arg124.field_12 = 1;

            int v14 = PathFinder(&arg124);

            if ( v14 && arg124.steps_cnt > 0)
            {
                for (int i = 0; i < arg124.steps_cnt; i++)
                {
                    arg->selectBact->_waypoints[i].x = arg124.waypoints[i].x;
                    arg->selectBact->_waypoints[i].z = arg124.waypoints[i].z;
                }
                arg->selectBact->_waypoints_count = arg124.steps_cnt;
            }
            else
            {
                arg->selectBact->_waypoints_count = 1;
                arg->selectBact->_waypoints[0].x = arg->target_point.x;
                arg->selectBact->_waypoints[0].z = arg->target_point.z;

                if ( _world->_GameShell )
                    SFXEngine::SFXe.startSound(&_world->_GameShell->samples1_info, World::SOUND_ID_ERROR);
            }
        }
        else
        {
            arg->selectBact->_waypoints_count = 1;
            arg->selectBact->_waypoints[0].x = arg->target_point.x;
            arg->selectBact->_waypoints[0].z = arg->target_point.z;
        }

        arg->selectBact->_current_waypoint = 0;

        arg67.tgt_pos.x = arg->selectBact->_waypoints[0].x;
        arg67.tgt_pos.z = arg->selectBact->_waypoints[0].z;
        arg67.priority = 0;
        arg67.tgt_type = BACT_TGT_TYPE_CELL;
        arg->selectBact->SetTarget(&arg67);

        sub_4A448C(arg->selectBact);
        sub_4A4538(arg->selectBact);
        arg->selectBact->ChangeEscapeFlag(false);
    }
    break;

    case World::DOACTION_WP_ADD:
        if ( arg->selectBact->_waypoints_count < 32 && (arg->selectBact->_status_flg & BACT_STFLAG_WAYPOINT) )
        {
            int v72 = arg->selectBact->_waypoints_count;
            int v20 = 0;

            if ( arg->selectBact->_bact_type != BACT_TYPES_TANK && arg->selectBact->_bact_type != BACT_TYPES_CAR )
            {
                for(NC_STACK_ypabact* &nod : arg->selectBact->_kidList)
                {
                    if ( nod->_bact_type == BACT_TYPES_TANK || nod->_bact_type == BACT_TYPES_CAR )
                    {
                        v20 = 1;
                        break;
                    }
                }
            }
            else
                v20 = 1;

            if ( v20 )
            {
                arg124.from = arg->selectBact->_waypoints[v72 - 1].XZ();
                arg124.to = arg->target_point.XZ();
                arg124.steps_cnt = 32 - v72;
                arg124.field_12 = 1;

                if ( PathFinder(&arg124) && arg124.steps_cnt > 0 )
                {
                    for (int i = 0; i < arg124.steps_cnt; i++)
                    {
                        arg->selectBact->_waypoints[i + v72].x = arg124.waypoints[i].x;
                        arg->selectBact->_waypoints[i + v72].z = arg124.waypoints[i].z;
                    }
                    arg->selectBact->_waypoints_count += arg124.steps_cnt;

                }
                else
                {
                    arg->selectBact->_waypoints[v72].x = arg->target_point.x;
                    arg->selectBact->_waypoints[v72].z = arg->target_point.z;
                    arg->selectBact->_waypoints_count++;

                    if ( _world->_GameShell )
                        SFXEngine::SFXe.startSound(&_world->_GameShell->samples1_info, World::SOUND_ID_ERROR);
                }
            }
            else
            {
                arg->selectBact->_waypoints[v72].x = arg->target_point.x;
                arg->selectBact->_waypoints[v72].z = arg->target_point.z;
                arg->selectBact->_waypoints_count++;
            }
            if ( !(arg->selectBact->_status_flg & BACT_STFLAG_WAYPOINT) )
            {
                arg->selectBact->_status_flg |= BACT_STFLAG_WAYPOINT;

                arg67.tgt_pos.x = arg->selectBact->_waypoints[v72].x;
                arg67.tgt_pos.z = arg->selectBact->_waypoints[v72].z;
                arg67.priority = 0;
                arg67.tgt_type = BACT_TGT_TYPE_CELL;
                arg->selectBact->SetTarget(&arg67);
            }
            sub_4A448C(arg->selectBact);
        }
        break;

    case World::DOACTION_WP_CYCLE:
        arg->selectBact->_status_flg |= BACT_STFLAG_WAYPOINTCCL;
        sub_4A448C(arg->selectBact);
        break;

    default:
        break;
    }
}

void NC_STACK_yparobo::HandleUserCommands(update_msg *arg)
{
    if ( _kidRef.IsListType(World::BLIST_CACHE) ) // Do not update units in dead list
        return;

    if (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2))
        return;

    if ( _status != BACT_STATUS_NORMAL)
        return;

    if ( getBACT_inputting() || IsPlayerRobo() )
        doUserCommands(arg);
}

bool NC_STACK_yparobo::IsPlayerRobo() const
{
    if ( !(_roboState & ROBOSTATE_PLAYERROBO) )
        return false;

    if ( _world && _world->IsSpectatorControlled() && this == _world->getYW_userHostStation() )
        return false;

    return true;
}

bool NC_STACK_yparobo::IsPlayerRoboAIBehavior() const
{
    return _playerRoboAIBehavior;
}

bool NC_STACK_yparobo::IsPlayerRoboAIBehaviorMoveActive() const
{
    return _playerRoboAIBehavior &&
           _playerRoboAIBehaviorMoveActive &&
           _primTtype == BACT_TGT_TYPE_CELL;
}

bool NC_STACK_yparobo::GetPlayerRoboAIBehaviorMoveTarget(vec3d *target) const
{
    if ( !target || !IsPlayerRoboAIBehaviorMoveActive() )
        return false;

    *target = _primTpos;
    return true;
}

bool NC_STACK_yparobo::GetPlayerRoboRelocationTarget(vec3d *target, bool *isTeleport) const
{
    if ( !target || !isTeleport || !IsPlayerRobo() || !_world ||
         this != _world->getYW_userHostStation() ||
         _status != BACT_STATUS_NORMAL ||
         (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN)) )
    {
        return false;
    }

    if ( IsPlayerRoboAIBehaviorMoveActive() )
    {
        *target = _primTpos;
        *isTeleport = false;
        return true;
    }

    if ( _roboState & ROBOSTATE_MOVE )
    {
        *target = _roboBeamPos;
        *isTeleport = true;
        return true;
    }

    return false;
}
