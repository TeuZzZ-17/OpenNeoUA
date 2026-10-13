// Host station AI: wishes (conquer, defence, radar, power, safety, reconnaissance), unit production and troubles.
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

void NC_STACK_yparobo::AI_layer1(update_msg *arg)
{
    if ( _status == BACT_STATUS_DEAD )
    {
        int a4 = getBACT_yourLastSeconds();
        a4 -= arg->frameTime;
        setBACT_yourLastSeconds(a4);
    }

    _airconst = _airconst_static;

    _soundcarrier.Sounds[0].Pitch = _soundcarrier.Sounds[0].PitchBase;
    _soundcarrier.Sounds[0].Volume = _volume;
    if ( _playerRoboAIBehavior )
        ResetPlayerMobileCockpitPitch();

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
            gun.gun_obj->_owner = _owner;
    }

    EnergyInteract(arg);

    if ( _primTtype == BACT_TGT_TYPE_CELL )
        _target_vec = _primTpos - _position;
    else if ( _primTtype != BACT_TGT_TYPE_UNIT )
        _target_vec = vec3d(0.0, 0.0, 0.0);

    int v9 = getBACT_inputting();

    if ( v9 )
        User_layer(arg);
    else
        AI_layer3(arg);
}

void NC_STACK_yparobo::AI_doMove(update_msg *arg)
{
    move_msg arg74;

    yparobo_func70__sub2__sub0();

    float v49 = arg->frameTime / 1000.0;

    _roboFlotage = _mass * 9.80665;

    if ( _target_dir.y < -0.7 )
        _roboFlotage = _mass * 3.0 * 9.80665;

    if ( _target_dir.y > 0.7 )
        _roboFlotage = _mass * 0.91 * 9.80665;

    float v39;
    if ( _primTtype )
        v39 = _target_vec.XZ().length();
    else
        v39 = 0.0;

    if ( v39 >= 100.0 )
    {
        if ( _status_flg & BACT_STFLAG_UPWRD )
        {
            _airconst *= 3.0;

            _roboFlotage = _mass * 2.5 * 9.80665;
            _thraction = 0;

            float v38 = 1.0 - v49 * 4.0;
            if ( v38 < 0.0 )
                v38 = 0.1;

            _fly_dir_length *= v38;
        }
        else if ( _primTtype )
        {
            _thraction = _force;
        }
        else
        {
            _thraction = 0;
        }

        if ( !(_status_flg & BACT_STFLAG_UPWRD) )
        {
            float v52 = _rotation.AxisZ().XZ().length();
            float v51 = _target_dir.XZ().length();

            if ( v52 <= 0.0001 || v51 <= 0.0001 )
            {
                _slider_time = 0;
                _status_flg &= ~BACT_STFLAG_XLEFT;
            }
            else
            {
                float v47 = _target_dir.XZ().dot( _rotation.AxisZ().XZ() ) / (v51 * v52);

                float v23 = clp_acos(v47);

                float v24 = fabs(v23);

                if ( v24 >= 0.001 )
                {
                    float mxx = v49 * _maxrot;

                    if ( v24 > mxx )
                        v23 = mxx;

                    if ( _rotation.AxisZ().XZ().cross( _target_dir.XZ() ) < 0.0 )
                        v23 = -v23;

                    float v28;

                    if ( v23 <= 0.0 )
                    {
                        if ( !(_status_flg & BACT_STFLAG_XLEFT) || !_slider_time )
                            _slider_time = arg->gTime;

                        v28 = (arg->gTime - _slider_time) * 0.9 * 0.001 + 0.1;
                        _status_flg |= BACT_STFLAG_XLEFT;
                    }
                    else
                    {
                        if ( _status_flg & BACT_STFLAG_XLEFT || !_slider_time )
                            _slider_time = arg->gTime;

                        v28 = (arg->gTime - _slider_time) * 0.9 * 0.001 + 0.1;
                        _status_flg &= ~BACT_STFLAG_XLEFT;
                    }

                    if ( v28 > 1.0 )
                        v28 = 1.0;

                    v23 *= v28;

                    sub_4A10E8(v23);
                }
            }
        }

        arg74.field_0 = v49;
        arg74.flag = 0;
        arg74.vec = _target_vec.X0Z() / v39;
    }
    else
    {
        _thraction = 0;

        if ( fabs(_fly_dir.x * _fly_dir_length) <= 0.1 && fabs(_fly_dir.z * _fly_dir_length) <= 0.1 )
        {
            if ( !(_roboState & ROBOSTATE_BUILDRADAR) &&
                    !(_roboState & ROBOSTATE_BUILDPOWER) &&
                    !(_roboState & ROBOSTATE_BUILDSAFE) )
            {
                bool v14 = yparobo_func70__sub2__sub1();

                if ( v14 && _roboWFlags & 1 )
                {
                    float v18;
                    if ( _status_flg & BACT_STFLAG_UPWRD )
                        v18 = _mass * 1.5;
                    else
                        v18 = _mass * 0.82;

                    _roboFlotage = v18 * 9.80665;
                }
                else
                {
                    if ( fabs(_target_dir.y) < 0.7 )
                        _fly_dir_length *= 0.35;
                }

                // robo_does_flux must stay unchanged: it is useful for the
                // final settling motion when the player Host Station uses
                // AI-style movement. Only robo_does_twist is suppressed while
                // the player is directly inside the Host Station or one of its
                // attached guns, because that rotates the camera/view.
                if ( v14 && (_roboWFlags & 2) && !IsPlayerDirectlyControllingThisRobo() )
                {
                    sub_4A1014(_maxrot * 0.3 * v49);
                }
            }
        }
        else
        {
            _airconst *= 3.0;
            _fly_dir_length *= 0.8;

            if ( fabs(_fly_dir.x * _fly_dir_length) < 0.1 && fabs(_fly_dir.z * _fly_dir_length) < 0.1 )
            {
                _fly_dir.z = 0;
                _fly_dir.x = 0;

                setTarget_msg v34;
                v34.tgt_type = BACT_TGT_TYPE_NONE;
                v34.priority = 0;
                SetTarget(&v34);
            }
        }
        arg74.vec = vec3d(0.0, 0.0, 0.0);
        arg74.flag = 0;
        arg74.field_0 = v49;
    }

    Move(&arg74);
}

int NC_STACK_yparobo::FindBestBldRadar()
{
    int id = -1;
    int power = 0;
    int i = 0;

    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 2 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }

        i++;
    }

    return id;
}

void NC_STACK_yparobo::sub_4F4FF4(int sectID, setTarget_msg *parg67)
{
    Common::Point pt(sectID % _wrldSectors.x, sectID / _wrldSectors.x);

    if ( _cellId == pt )
    {
        if ( pt.x >= 2 )
            pt.x--;
        else
            pt.x++;
    }
    else
    {
        if ( pt.x > _cellId.x )
            pt.x--;
        if ( pt.x < _cellId.x )
            pt.x++;

        if ( pt.y > _cellId.y )
            pt.y--;
        if ( pt.y < _cellId.y )
            pt.y++;
    }

    parg67->tgt_type = BACT_TGT_TYPE_CELL;
    parg67->priority = 0;
    parg67->tgt_pos = World::SectorIDToCenterPos3( pt );
}

void NC_STACK_yparobo::buildRadar(update_msg *arg)
{
    Common::Point pt( _roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x );

    float v35 = arg->frameTime * 0.001;

    if ( _world->SectorAt(_roboBuildingCellID).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 0;

        SetTarget(&arg67);
        _roboBuildingDuty = 0;
    }
    else
    {
        int build_id = FindBestBldRadar();

        if ( build_id == -1 )
        {
            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;

            SetTarget(&arg67);
            _roboBuildingDuty = 0;
        }
        else
        {
            Common::Point dist = _cellId.AbsDistance( pt );
            if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
            {
                setTarget_msg arg67;
                sub_4F4FF4(_roboBuildingCellID, &arg67);

                arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&arg67);
            }
            else if ( dist ) // Not null distance (1 sector distance)
            {
                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;
                arg67.priority = 0;
                SetTarget(&arg67);

                vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                t.normalise();

                float v41 = t.dot( _rotation.AxisZ().XZ() );

                if ( v41 <= 0.9 )
                {
                    float a2 = clp_acos(v41);

                    if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                        a2 = -a2;

                    if ( a2 < -_maxrot * v35 )
                        a2 = -_maxrot * v35;

                    if ( a2 > _maxrot * v35 )
                        a2 = _maxrot * v35;

                    sub_4A10E8(a2);
                }
                else
                {
                    ypaworld_arg148 arg148;

                    arg148.owner = _owner;
                    arg148.blg_ID = build_id;
                    arg148.CellId = pt;
                    arg148.field_C = 0;
                    arg148.field_18 = 0;

                    if ( _world->ypaworld_func148(&arg148) )
                    {
                        const World::TBuildingProto &proto = _world->GetBuildProtos().at(build_id);

                        if ( _roboBuildSpare >= proto.Energy )
                        {
                            _roboBuildSpare -= proto.Energy;
                        }
                        else
                        {
                            _energy -= proto.Energy - _roboBuildSpare;
                            _roboBuildSpare = 0;
                        }

                        _roboBuildingDuty = 0;

                        if ( _roboNewAI )
                            _roboRadarDelay = _roboTimeScale * (100 - _roboEpRadar) / 100;
                    }
                    else
                    {
                        setTarget_msg arg67_1;
                        arg67_1.tgt_type = BACT_TGT_TYPE_NONE;
                        arg67_1.priority = 0;

                        SetTarget(&arg67_1);

                        _roboBuildingDuty = 0;
                    }
                }
            }
            else
            {
                setTarget_msg arg67;

                sub_4F4FF4(_roboBuildingCellID, &arg67);

                arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&arg67);
            }
        }
    }
}

int NC_STACK_yparobo::FindBestBldPowerStation()
{
    int id = -1;
    int power = 0;
    int i = 0;

    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 1 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }
        i++;
    }

    return id;
}

void NC_STACK_yparobo::buildPower(update_msg *arg)
{
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);

    float v38 = arg->frameTime * 0.001;

    if ( _world->SectorAt( _roboBuildingCellID ).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 0;
        SetTarget(&arg67);

        _roboBuildingDuty = 0;
    }
    else
    {
        yw_arg176 arg176;
        arg176.owner = _owner;
        _world->ypaworld_func176(&arg176);

        if ( _world->SectorAt( _roboBuildingCellID ).energy_power != 255 && ( arg176.field_4 >= 0.9 || arg176.field_4 <= 0.001) )
        {
            int bldid = FindBestBldPowerStation();

            if ( bldid == -1 )
            {
                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;
                arg67.priority = 0;
                SetTarget(&arg67);
                _roboBuildingDuty = 0;
            }
            else
            {
                Common::Point dist = _cellId.AbsDistance( pt );
                if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
                {
                    setTarget_msg arg67;
                    sub_4F4FF4(_roboBuildingCellID, &arg67);
                    arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
                    SetTarget(&arg67);
                }
                else if ( dist ) // Not null distance (1 sector distance)
                {
                    setTarget_msg arg67;
                    arg67.tgt_type = BACT_TGT_TYPE_NONE;
                    arg67.priority = 0;
                    SetTarget(&arg67);

                    vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                    t.normalise();

                    float v44 = t.dot( _rotation.AxisZ().XZ() );

                    if ( v44 <= 0.9 )
                    {
                        float a2 = clp_acos(v44);

                        if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                            a2 = -a2;

                        if ( a2 < -_maxrot * v38 )
                            a2 = -_maxrot * v38;

                        if ( a2 > _maxrot * v38 )
                            a2 = _maxrot * v38;

                        sub_4A10E8(a2);
                    }
                    else
                    {
                        ypaworld_arg148 arg148;
                        arg148.owner = _owner;
                        arg148.blg_ID = bldid;
                        arg148.CellId = pt;
                        arg148.field_C = 0;
                        arg148.field_18 = 0;

                        if ( _world->ypaworld_func148(&arg148) )
                        {
                            const World::TBuildingProto &proto = _world->GetBuildProtos().at(bldid);

                            if ( _roboBuildSpare >= proto.Energy )
                            {
                                _roboBuildSpare -= proto.Energy;
                            }
                            else
                            {
                                _energy -= proto.Energy - _roboBuildSpare;
                                _roboBuildSpare = 0;
                            }

                            _roboBuildingDuty = 0;

                            if ( _roboNewAI )
                                _roboPowerDelay = _roboTimeScale * (100 - _roboEpPower) / 100;
                        }
                        else
                        {
                            setTarget_msg arg67_1;
                            arg67_1.tgt_type = BACT_TGT_TYPE_NONE;
                            arg67_1.priority = 0;
                            SetTarget(&arg67_1);
                            _roboBuildingDuty = 0;
                        }
                    }
                }
                else
                {
                    setTarget_msg arg67;
                    sub_4F4FF4(_roboBuildingCellID, &arg67);
                    arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
                    SetTarget(&arg67);
                }
            }
        }
        else
        {
            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;
            SetTarget(&arg67);
            _roboBuildingDuty = 0;
        }
    }
}

int NC_STACK_yparobo::FindBestBldDefenceCenter()
{
    int id = -1;
    int power = 0;
    int i = 0;

    for (const World::TBuildingProto &proto : _world->GetBuildProtos())
    {
        if ( proto.EnableMask & (1 << _owner) )
        {
            if ( proto.ModelID == 3 )
            {
                if ( (_roboBuildSpare + _energy - _energy_max * 0.2) > proto.Energy )
                {
                    int protoPow = proto.Power;

                    for (const World::TBuildingProto::TGun &gun : proto.Guns)
                    {
                        if ( gun.VhclID )
                            protoPow += 10;
                    }

                    if ( protoPow > power )
                    {
                        power = protoPow;
                        id = i;
                    }
                }
            }
        }
        i++;
    }

    return id;
}

void NC_STACK_yparobo::buildSafe(update_msg *arg)
{
    // Copy of sub4_sub0 with minor changes
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);

    float v35 = arg->frameTime * 0.001;

    if ( _world->SectorAt( _roboBuildingCellID ).PurposeType != cellArea::PT_NONE )
    {
        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 0;

        SetTarget(&arg67);
        _roboBuildingDuty = 0;
    }
    else
    {
        int build_id = FindBestBldDefenceCenter();

        if ( build_id == -1 )
        {
            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;

            SetTarget(&arg67);
            _roboBuildingDuty = 0;
        }
        else
        {
            Common::Point dist = _cellId.AbsDistance( pt );
            if ( dist.x > 1 || dist.y > 1 ) // More than one sector distance
            {
                setTarget_msg arg67;
                sub_4F4FF4(_roboBuildingCellID, &arg67);

                arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&arg67);
            }
            else if ( dist ) // Not null distance (1 sector distance)
            {
                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;
                arg67.priority = 0;
                SetTarget(&arg67);

                vec2d t = World::SectorIDToCenterPos2( pt ) - _position.XZ();

                t.normalise();

                float v41 = t.dot( _rotation.AxisZ().XZ() );

                if ( v41 <= 0.9 )
                {
                    float a2 = clp_acos(v41);

                    if ( t.cross( _rotation.AxisZ().XZ() ) > 0.0 )
                        a2 = -a2;

                    if ( a2 < -_maxrot * v35 )
                        a2 = -_maxrot * v35;

                    if ( a2 > _maxrot * v35 )
                        a2 = _maxrot * v35;

                    sub_4A10E8(a2);
                }
                else
                {
                    ypaworld_arg148 arg148;

                    arg148.owner = _owner;
                    arg148.blg_ID = build_id;
                    arg148.CellId = pt;
                    arg148.field_C = 0;
                    arg148.field_18 = 0;

                    if ( _world->ypaworld_func148(&arg148) )
                    {
                        const World::TBuildingProto &proto = _world->GetBuildProtos().at(build_id);

                        if ( _roboBuildSpare >= proto.Energy )
                        {
                            _roboBuildSpare -= proto.Energy;
                        }
                        else
                        {
                            _energy -= proto.Energy - _roboBuildSpare;
                            _roboBuildSpare = 0;
                        }

                        _roboBuildingDuty = 0;

                        if ( _roboNewAI )
                            _roboSafetyDelay = _roboTimeScale * (100 - _roboEpSafety) / 100;
                    }
                    else
                    {
                        setTarget_msg arg67_1;
                        arg67_1.tgt_type = BACT_TGT_TYPE_NONE;
                        arg67_1.priority = 0;

                        SetTarget(&arg67_1);

                        _roboBuildingDuty = 0;
                    }
                }
            }
            else
            {
                setTarget_msg arg67;

                sub_4F4FF4(_roboBuildingCellID, &arg67);

                arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;

                SetTarget(&arg67);
            }

        }
    }
}

void NC_STACK_yparobo::changePlace()
{
    Common::Point pt(_roboBuildingCellID % _wrldSectors.x, _roboBuildingCellID / _wrldSectors.x);

    if ( _world->IsGamePlaySector(pt) )
    {
        float v12 = _cellId.LengthTo<float>( pt );

        if ( v12 >= 0.1 )
        {
            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
            arg67.priority = 0;
            arg67.tgt_pos = World::SectorIDToCenterPos3( pt );

            SetTarget(&arg67);
        }
        else
        {
            _roboSafetyCellID = 0;
            _roboSafetyValue = 0;
            _roboSafetyCellIndex = 0;
            _roboPositionCellID = 0;
            _roboRadarCellID = 0;
            _roboPowerCellID = 0;
            _roboPositionValue = 0;
            _roboRadarValue = 0;
            _roboPowerValue = 0;
            _roboPositionCellIndex = 0;
            _roboRadarCellIndex = 0;
            _roboPowerCellIndex = 0;

            _roboBuildingDuty = 0;

            _roboState &= ~(ROBOSTATE_READYRADAR | ROBOSTATE_READYPOWER
                                 | ROBOSTATE_READYSAFE | ROBOSTATE_FOUNDPLACE);

            setTarget_msg arg67;
            arg67.tgt_type = BACT_TGT_TYPE_NONE;
            arg67.priority = 0;
            SetTarget(&arg67);

            if ( _roboNewAI )
                _roboPositionDelay = _roboTimeScale * (100 - _roboEpChangePlace) / 100;
        }
    }
    else
    {
        _roboBuildingDuty = 0;
    }
}

bool yparobo_IsActiveProducedVehicle(NC_STACK_ypabact *unit)
{
    return unit &&
           unit->_isGenesisProduced &&
           unit->_status != BACT_STATUS_NOPE &&
           unit->_status != BACT_STATUS_DEAD &&
           !(unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

int yparobo_CountActiveProducedVehicleRecursive(NC_STACK_ypabact *unit, int owner,
                                                       int vehicleId,
                                                       std::unordered_set<NC_STACK_ypabact *> &visited)
{
    if ( !unit || visited.count(unit) )
        return 0;

    visited.insert(unit);

    int count = 0;

    if ( yparobo_IsActiveProducedVehicle(unit) &&
         unit->_owner == owner &&
         unit->_vehicleID == vehicleId )
    {
        count++;
    }

    for ( NC_STACK_ypabact *kid : unit->GetKidList() )
        count += yparobo_CountActiveProducedVehicleRecursive(kid, owner, vehicleId, visited);

    return count;
}

int yparobo_CountActiveProducedVehicle(NC_STACK_ypaworld *world, int owner, int vehicleId)
{
    if ( !world || vehicleId <= 0 )
        return 0;

    int count = 0;
    std::unordered_set<NC_STACK_ypabact *> visited;

    // _unitsList mostly contains root/major BACTs, while sector cell lists contain
    // many live regular units. Scan both and deduplicate pointers so Genesis
    // production cannot bypass the owner-wide count through hierarchy placement.
    for ( NC_STACK_ypabact *unit : world->_unitsList )
        count += yparobo_CountActiveProducedVehicleRecursive(unit, owner, vehicleId, visited);

    for ( cellArea &cell : world->Sectors() )
    {
        for ( NC_STACK_ypabact *unit : cell.unitsList )
            count += yparobo_CountActiveProducedVehicleRecursive(unit, owner, vehicleId, visited);
    }

    return count;
}

int NC_STACK_ypaworld::GetVehicleProductionLimitRemaining(int owner, int vehicleId)
{
    // This campaign/GEM limit is intentionally disabled in network games.
    if ( _isNetGame )
        return -1;

    if ( owner < 0 || vehicleId <= 0 ||
         (size_t)vehicleId >= _vhclProtos.size() )
        return -1;

    // max_active_at_once is a player campaign/GEM progression limit. AI owners
    // must remain free to produce units regardless of the selected player faction.
    if ( !_userRobo || owner != _userRobo->_owner )
        return -1;

    const int effectiveMax = _vhclProtos[vehicleId].max_active_at_once;
    if ( effectiveMax <= 0 )
        return -1;

    const int activeProduced = yparobo_CountActiveProducedVehicle(this, owner, vehicleId);
    return activeProduced >= effectiveMax ? 0 : effectiveMax - activeProduced;
}

bool NC_STACK_ypaworld::IsVehicleProductionLimitReached(int owner, int vehicleId)
{
    return GetVehicleProductionLimitRemaining(owner, vehicleId) == 0;
}

NC_STACK_ypabact *yparobo_TryCreateGenesisUnit(NC_STACK_yparobo *robo,
                                                       ypaworld_arg146 *arg)
{
    if ( !robo || !arg )
        return nullptr;

    NC_STACK_ypaworld *world = robo->getBACT_pWorld();
    if ( !world || world->IsVehicleProductionLimitReached(robo->_owner, arg->vehicle_id) )
        return nullptr;

    NC_STACK_ypabact *unit = world->ypaworld_func146(arg);
    if (unit) unit->PlaceGenesisAboveTerrain();
    if ( unit && !world->_isNetGame )
    {
        unit->_owner = robo->_owner;
        unit->_isGenesisProduced = true;
    }

    return unit;
}

int yparobo_GetVehicleProductionCost(NC_STACK_ypaworld *world, int vehicleId)
{
    if ( !world || vehicleId <= 0 || (size_t)vehicleId >= world->GetVhclProtos().size() )
        return 0;

    return world->GetVhclProtos().at(vehicleId).GetProductionCost();
}

int yparobo_GetUnitProductionCost(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return 0;

    int cost = yparobo_GetVehicleProductionCost(unit->getBACT_pWorld(), unit->_vehicleID);
    return cost > 0 ? cost : unit->_energy_max;
}

void yparobo_AdvanceMimicProductionCost(NC_STACK_ypaworld *world, int vehicleId)
{
    if ( !world || vehicleId <= 0 || (size_t)vehicleId >= world->GetVhclProtos().size() )
        return;

    World::TVhclProto &proto = world->GetVhclProtos().at(vehicleId);
    if ( proto.is_mimic )
        proto.RollMimicProductionCost();
}

NC_STACK_ypabact *NC_STACK_yparobo::AllocForce(robo_loct1 *arg)
{
    int v72 = 0;
    float v73 = 0.0;

    int v85 = -1;
    int v86 = -1;
    int v87 = -1;

    int v75 = 10000000;
    int v2 = 0;
    int v77 = 0;
    int v82 = -2;
    float v76 = 153600.0;
    NC_STACK_ypabact *selectedCommander = nullptr;

    for (NC_STACK_ypabact* &node : _kidList)
    {
        //Unused flag
        if ( node->_status == BACT_STATUS_IDLE && /*!( node->status_flg & BACT_STFLAG_AKKU ) &&*/ !( node->_status_flg & BACT_STFLAG_UNUSE ) && !( node->_status_flg & BACT_STFLAG_ESCAPE ) &&  node->_bact_type != BACT_TYPES_GUN )
        {
            v2++;

            int v8 = _world->TestVehicle(node->_vehicleID, arg->job, arg->tgt_bact);

            Common::Point tmp = _cellId.AbsDistance( node->_cellId );
            if ( tmp.x < 2 && tmp.y < 2 )
                v77 = 1;

            if ( v8 > -1 && v8 >= v82 )
            {

                bact_arg81 arg81;
                arg81.enrg_type = 1;
                arg81.enrg_sum = 0;
                node->GetSummary(&arg81);

                int v11 = abs(arg81.enrg_sum - arg->energ);
                int v15 = 0;

                int v16;

                if ( v11 )
                    v16 = arg->energ / v11;
                else
                    v16 = 6;

                float v78 = (node->_position.XZ() - arg->tgt_pos.XZ()).length();

                if ( v16 <= 5 )
                {
                    if ( v11 < v75 )
                    {
                        v15 = 1;
                    }
                }
                else if ( v78 < v76 )
                {
                    v15 = 1;
                }

                if ( v8 > v82 || (v8 == v82 && v15) )
                {
                    v75 = v11;
                    v76 = v78;
                    selectedCommander = node;
                    v82 = v8;
                }

            }
        }
    }

    if ( !(_roboState & ROBOSTATE_DOCKINUSE) )
    {
        bact_arg81 arg81_2;
        arg81_2.enrg_sum = 0;
        arg81_2.enrg_type = 3;

        GetSummary(&arg81_2);

        v85 = -1;
        v86 = -1;
        v87 = -1;

        v73 = arg81_2.enrg_sum * 0.05 + 1.0;

        int i = 0;
        for ( const World::TVhclProto &proto : _world->GetVhclProtos() )
        {
            if ( proto.model_id != BACT_TYPES_NOPE )
            {
                float v20 = _energy_max * 0.2;
                int v67 = _energy - v20;

                if ( _roboState & ROBOSTATE_BUILDROBO )
                    v67 += _roboVehicleSpare;

                if ( (1 << _owner) & proto.disable_enable_bitmask )
                {
                    if ( proto.model_id != BACT_TYPES_ROBO && proto.model_id != BACT_TYPES_GUN && proto.model_id != BACT_TYPES_MISSLE )
                    {
                        int protoCost = proto.GetProductionCost();
                        if ( v67 > ((float)protoCost * v73) &&
                             !_world->IsVehicleProductionLimitReached(_owner, i) )
                        {
                            int v80 = _world->TestVehicle(i, arg->job, arg->tgt_bact);

                            int v27 = 0;
                            if ( v2 < (_wrldSectors.x + _wrldSectors.y) / 5 )
                                v27 = 1;

                            if ( _energy < ((float)_energy_max * 0.9) )
                                v27 = 0;

                            if ( _roboState & ROBOSTATE_DOCKINUSE )
                                v27 = 0;

                            if ( v77 )
                                v27 = 0;

                            if ( v80 > -1 && (v80 > v82 || (v80 == v82 && v27)) )
                            {
                                v82 = v80;
                                v87 = v86;
                                v86 = v85;
                                v85 = i;
                            }
                        }
                    }
                }
            }

            i++;
        }
    }

    //there must be switch, but it's originally deleted
    //arg->field_34;

    if ( v85 == -1 && !selectedCommander )
        return nullptr;

    if ( v85 == -1 )
    {
        if ( !(_roboState & ROBOSTATE_DOCKINUSE) )
        {
            bact_arg81 arg81_3;
            arg81_3.enrg_type = 1;
            arg81_3.enrg_sum = 0;

            selectedCommander->GetSummary(&arg81_3);

            if ( arg81_3.enrg_sum < arg->energ )
            {
                _roboDockUser = selectedCommander->_commandID;
                _roboDockEnerg = arg->energ - arg81_3.enrg_sum;
                v72 = 1;
            }
        }
    }
    else
    {
        int v32 = _clock % 10;
        int v33;

        if ( v32 <= 8 || v87 == -1 )
        {
            if ( v32 <= 5 || v86 == -1 )
                v33 = v85;
            else
                v33 = v86;
        }
        else
        {
            v33 = v87;
        }

        if ( _world->IsVehicleProductionLimitReached(_owner, v33) )
        {
            if ( selectedCommander )
            {
                setTarget_msg arg67_2;
                arg67_2.tgt_pos = arg->tgt_pos;
                arg67_2.priority = 0;
                arg67_2.tgt.pbact = arg->tgt_bact;
                arg67_2.tgt_type = arg->tgType;

                selectedCommander->SetTarget(&arg67_2);
                selectedCommander->setBACT_aggression(arg->aggr);
            }

            return selectedCommander;
        }

        ypaworld_arg146 arg146;
        arg146.vehicle_id = v33;
        arg146.pos = _position + _roboDockPos;

        const int producedUnitCost = yparobo_GetVehicleProductionCost(_world, v33);

        NC_STACK_ypabact *newUnit = yparobo_TryCreateGenesisUnit(this, &arg146);

        if ( !newUnit )
            return nullptr;

        newUnit->_commandID = dword_5B1128;
        dword_5B1128++;

        newUnit->_owner = _owner;
        newUnit->_host_station = this;
        newUnit->InheritActiveDebuffFromParent(this);

        int v69 = getBACT_bactCollisions();
        newUnit->setBACT_bactCollisions(v69);

        AddSubject(newUnit);
        if ( _world->_isNetGame )
        {
            newUnit->_gid |= newUnit->_owner << 24;
            newUnit->_commandID |= newUnit->_owner << 24;

            uamessage_newVhcl nvMsg;

            nvMsg.msgID = UAMSG_NEWVHCL;
            nvMsg.owner = newUnit->_owner;
            nvMsg.pos = newUnit->_position;
            nvMsg.type = newUnit->_vehicleID;
            nvMsg.parent = newUnit->_parent->_gid;
            nvMsg.id = newUnit->_gid;
            nvMsg.kind = 1;
            nvMsg.commandID = newUnit->_commandID;

            _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
        }

        _roboState |= ROBOSTATE_DOCKINUSE;

        newUnit->setBACT_aggression(60);

        _roboDockUser = newUnit->_commandID;
        _roboDockEnerg = arg->energ - producedUnitCost;

        newUnit->_scale_time = (float)newUnit->_energy_max * 0.2;

        _roboDockTime = newUnit->_scale_time + 2000;

        newUnit->ChangeEscapeFlag(false);

        setState_msg arg78;
        arg78.newStatus = BACT_STATUS_CREATE;
        arg78.setFlags = 0;
        arg78.unsetFlags = 0;

        newUnit->SetState(&arg78);


        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_CELL;
        arg67.priority = 0;
        arg67.tgt_pos.x = _rotation.m20 * World::CVSectorLength * 0.5 + _position.x;
        arg67.tgt_pos.z = _rotation.m22 * World::CVSectorLength * 0.5 + _position.z;

        newUnit->SetTarget(&arg67);

        int v70 = (float)producedUnitCost * v73;

        if ( _roboState & ROBOSTATE_BUILDROBO )
        {
            if ( v70 > _roboVehicleSpare )
            {
                _energy -= v70 - _roboVehicleSpare;
                _roboVehicleSpare = 0;
            }
            else
            {
                _roboVehicleSpare -= v70;
            }
        }
        else
        {
            _energy -= v70;
        }

        selectedCommander = newUnit;
        v72 = 1;
        _world->HistoryAktCreate(newUnit);
        yparobo_AdvanceMimicProductionCost(_world, newUnit->_vehicleID);
    }

    if ( v72 )
    {
        _roboState |= (ROBOSTATE_SETDOCKTARGET | ROBOSTATE_DOCKINUSE);
        _roboDockTargetBact = arg->tgt_bact;
        _roboDockTargetType = arg->tgType;
        _roboDockTargetCommandID = arg->command_id;

        _roboDockTargetPos = arg->tgt_pos;
        _roboDockAggr = arg->aggr;

        if ( arg->tgType == BACT_TGT_TYPE_UNIT )
            _roboDockTargetPos = arg->tgt_bact->_position;
    }
    else
    {
        setTarget_msg arg67_2;
        arg67_2.tgt_pos = arg->tgt_pos;
        arg67_2.priority = 0;
        arg67_2.tgt.pbact = arg->tgt_bact;
        arg67_2.tgt_type = arg->tgType;

        selectedCommander->SetTarget(&arg67_2);

        selectedCommander->setBACT_aggression(arg->aggr);
    }

    return selectedCommander;
}

void NC_STACK_yparobo::buildConquer()
{
    bact_arg92 arg92;
    arg92.field_14 = 2;
    arg92.pos = World::SectorIDToCenterPos3( {_roboVehicleCellID % _wrldSectors.x, _roboVehicleCellID / _wrldSectors.x} );

    GetForcesRatio(&arg92);

    int v20 = (arg92.energ2 - arg92.energ1) * 0.5;

    if ( v20 < 15000 )
        v20 = 15000;

    yw_130arg arg130;
    arg130.pos_x = arg92.pos.x;
    arg130.pos_z = arg92.pos.z;

    _world->GetSectorInfo(&arg130);

    int enrg = 0;

    for ( NC_STACK_ypabact* &cell_unit : arg130.pcell->unitsList )
    {
        if ( _owner != cell_unit->_owner && cell_unit->_bact_type == BACT_TYPES_GUN && (cell_unit->_weapon != -1 || cell_unit->HasMinigun()) )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( cell_unit );
            int a4 = gun->IsRoboGun();

            if ( !a4 )
            {
                for (NC_STACK_ypabact* &bctt : _world->_unitsList)
                {
                    if ( bctt->_bact_type == BACT_TYPES_ROBO && bctt->_status != BACT_STATUS_DEAD && cell_unit->_owner == bctt->_owner )
                    {
                        int v6 = bctt->_energy_max / 2;

                        if ( v6 < 120000 )
                            v6 = 120000;

                        enrg = v6;
                        break;
                    }
                }
            }
        }

        if ( enrg )
            break;
    }

    if ( enrg > v20 )
        v20 = enrg;

    ypaworld_arg136 arg136;
    arg136.stPos = _position + _roboDockPos - vec3d::OY(100.0);
    arg136.vect = vec3d::OY(20000.0);
    arg136.flags = 0;

    _world->ypaworld_func136(&arg136);

    if ( _pSector->height - arg136.isectPos.y >= 50.0 )
    {
        setTarget_msg arg67;
        arg67.tgt_pos.x = _position.x + 200.0;
        arg67.tgt_pos.z = _position.z + 300.0;
        arg67.priority = 0;
        arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;

        SetTarget(&arg67);
    }
    else
    {
        robo_loct1 loct;

        loct.distance = (arg92.pos.XZ() - _position.XZ()).length();
        loct.tgt_pos = arg92.pos.X0Z();

        loct.tgt_bact = nullptr;

        if ( v20 <= 20000 )
            loct.energ = 20000;
        else
            loct.energ = v20;

        loct.tgType = BACT_TGT_TYPE_CELL;

        if ( _roboVehicleCell->PurposeType == cellArea::PT_POWERSTATION )
            loct.aggr = 49;
        else
            loct.aggr = 60;

        loct.forbidden = 1;
        loct.want = 32;
        loct.good = 130;
        loct.bad = 0;
        loct.job = 6;

        AllocForce(&loct);

        if ( _roboNewAI )
            _roboConqDelay = _roboTimeScale * (100 - _roboEpConquer) / 100;

        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildDefense()
{
    setTarget_msg arg132;
    arg132.priority = _roboVehicleCommandID;

    if ( yparobo_func132(&arg132) )
    {
        bact_arg81 arg81;
        arg81.enrg_sum = 0;
        arg81.enrg_type = 1;

        arg132.tgt.pbact->GetSummary(&arg81);

        int v18 = arg81.enrg_sum / 2;

        if ( v18 < 15000 )
            v18 = 15000;

        if ( arg132.tgt.pbact->_bact_type == BACT_TYPES_GUN && (arg132.tgt.pbact->_weapon != -1 || arg132.tgt.pbact->HasMinigun()) )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( arg132.tgt.pbact );
            int a4 = gun->IsRoboGun();

            if ( !a4 )
            {
                for (NC_STACK_ypabact* &node : _world->_unitsList)
                {
                    if ( node->_bact_type == BACT_TYPES_ROBO && node->_status != BACT_STATUS_DEAD && arg132.tgt.pbact->_owner == node->_owner )
                    {
                        int v4 = node->_energy_max / 2;

                        if ( v4 < 120000 )
                            v4 = 120000;

                        v18 = v4;
                        break;
                    }
                }
            }
        }

        ypaworld_arg136 arg136;
        arg136.stPos = _position + _roboDockPos - vec3d::OY(100.0);
        arg136.vect = vec3d::OY(20000.0);
        arg136.flags = 0;

        _world->ypaworld_func136(&arg136);

        if ( _pSector->height - arg136.isectPos.y >= 50.0 )
        {
            arg132.tgt_pos.x = _position.x + 200.0;
            arg132.tgt_pos.z = _position.z + 300.0;
            arg132.priority = 0;
            arg132.tgt_type = BACT_TGT_TYPE_CELL_IND;

            SetTarget(&arg132);
        }
        else
        {
            robo_loct1 loct;
            loct.distance = (arg132.tgt.pbact->_position.XZ() - _position.XZ()).length();
            loct.tgt_pos = arg132.tgt.pbact->_position;

            int v9 = v18;
            if ( v9 >= 40000 )
                v9 = 40000;

            loct.energ = v9;
            loct.tgType = BACT_TGT_TYPE_UNIT;
            loct.tgt_bact = arg132.tgt.pbact;
            loct.command_id = _roboVehicleCommandID;
            loct.aggr = 60;

            if ( arg132.tgt.pbact->_bact_type != BACT_TYPES_TANK && arg132.tgt.pbact->_bact_type != BACT_TYPES_CAR )
            {
                loct.bad = 2;
                loct.want = 32;
                loct.good = 852;
                loct.forbidden = 129;
            }
            else
            {
                loct.want = 32;
                loct.good = 578;
                loct.bad = 256;
                loct.forbidden = 1;
            }

            switch ( arg132.tgt.pbact->_bact_type )
            {
            case BACT_TYPES_TANK:
            case BACT_TYPES_CAR:
                loct.job = 2;
                break;

            case BACT_TYPES_FLYER:
            case BACT_TYPES_UFO:
                loct.job = 3;
                break;

            default:
                loct.job = 4;
                break;
            }

            AllocForce(&loct);

            if ( _roboNewAI )
                _roboEnemyDelay = _roboTimeScale * (100 - _roboEpDefense) / 100;

            _roboVehicleDuty = 0;
        }
    }
    else
    {
        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildRobo()
{
    setTarget_msg arg132;
    arg132.priority = _roboVehicleCommandID;
    if ( yparobo_func132(&arg132) )
    {
        ypaworld_arg136 arg136;
        arg136.stPos = _position + _roboDockPos - vec3d::OY(100.0);
        arg136.vect = vec3d::OY(20000.0);
        arg136.flags = 0;

        _world->ypaworld_func136(&arg136);

        if ( _pSector->height - arg136.isectPos.y >= 50.0 )
        {
            arg132.tgt_pos.x = _position.x + 200.0;
            arg132.tgt_pos.z = _position.z + 300.0;
            arg132.tgt_type = BACT_TGT_TYPE_CELL_IND;
            arg132.priority = 0;
            SetTarget(&arg132);
        }
        else
        {
            robo_loct1 loct;
            loct.distance = (arg132.tgt.pbact->_position.XZ() - _position.XZ()).length();
            loct.tgt_pos = arg132.tgt.pbact->_position;

            int v6 = arg132.tgt.pbact->_energy_max / 2;
            if ( v6 < 120000 )
                v6 = 120000;

            loct.energ = v6;
            loct.tgType = BACT_TGT_TYPE_UNIT;
            loct.tgt_bact = arg132.tgt.pbact;
            loct.command_id = _roboVehicleCommandID;
            loct.aggr = 60;
            loct.want = 32;
            loct.good = 852;
            loct.bad = 2;
            loct.forbidden = 1;
            loct.job = 1;
            _roboState |= ROBOSTATE_BUILDROBO;

            NC_STACK_ypabact *force = AllocForce(&loct);
            if ( force )
            {
                for ( robo_t1 &ataker : _roboAttackers )
                {
                    if ( !ataker.field_0 )
                    {
                        ataker.field_0 = force->_commandID;
                        ataker.field_4 = arg132.tgt.pbact->_commandID;
                        break;
                    }
                }
            }

            if ( _roboNewAI )
                _roboDangerDelay = _roboTimeScale * (100 - _roboEpRobo) / 100;

            _roboVehicleDuty = 0;
        }
    }
    else
    {
        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::buildReconnoitre()
{
    ypaworld_arg136 arg136;
    arg136.stPos = _position + _roboDockPos - vec3d::OY(100.0);
    arg136.vect = vec3d::OY(20000.0);
    arg136.flags = 0;

    _world->ypaworld_func136(&arg136);

    if ( _pSector->height - arg136.isectPos.y >= 50.0 )
    {
        setTarget_msg arg67;
        arg67.tgt_pos.x = _position.x + 200.0;
        arg67.tgt_pos.z = _position.z + 300.0;
        arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
        arg67.priority = 0;

        SetTarget(&arg67);
    }
    else
    {
        robo_loct1 loct;
        loct.tgt_pos = World::SectorIDToCenterPos3( {_roboVehicleCellID % _wrldSectors.x, _roboVehicleCellID / _wrldSectors.x} );

        loct.energ = 10;
        loct.aggr = 25;
        loct.forbidden = 0;
        loct.want = 1024;
        loct.good = 9;
        loct.job = 5;
        loct.tgType = BACT_TGT_TYPE_CELL;
        loct.bad = 32;
        loct.distance = (loct.tgt_pos.XZ() - _position.XZ()).length();
        loct.tgt_bact = nullptr;

        NC_STACK_ypabact *force = AllocForce(&loct);

        if ( force )
            force->_aggr = 25;

        if ( _roboNewAI )
            _roboExploreDelay = _roboTimeScale * (100 - _roboEpRecon) / 100;

        _roboVehicleDuty = 0;
    }
}

void NC_STACK_yparobo::AI_doWish(update_msg *arg)
{
    if ( !_roboBuildingDuty )
    {
        int found_time = 0;
        int found = 0;

        if ( _roboState & ROBOSTATE_READYPOWER )
        {
            if ( _roboPowerValue )
            {
                if ( -_roboPowerDelay > found_time )
                {
                    found = ROBOSTATE_BUILDPOWER;
                    found_time = -_roboPowerDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYRADAR )
        {
            if ( _roboRadarValue )
            {
                if ( -_roboRadarDelay > found_time )
                {
                    found = ROBOSTATE_BUILDRADAR;
                    found_time = -_roboRadarDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYSAFE )
        {
            if ( _roboSafetyValue )
            {
                if ( -_roboSafetyDelay > found_time )
                {
                    found = ROBOSTATE_BUILDSAFE;
                    found_time = -_roboSafetyDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_FOUNDPLACE )
        {
            if ( _roboPositionValue )
            {
                if ( -_roboPositionDelay > found_time )
                {
                    found = ROBOSTATE_CHANGEPLACE;
                    found_time = -_roboPositionDelay;
                }
            }
        }

        if ( found_time > 0 )
        {
            if ( found == ROBOSTATE_BUILDRADAR )
            {
                _roboRadarValue = 0;
                _roboRadarCellIndex = 0;
                _roboBuildingDuty = ROBOSTATE_BUILDRADAR;
                _roboBuildingCellID = _roboRadarCellID;
                _roboBuildingCell = _roboRadarCell;
                _roboState &= ~ROBOSTATE_READYRADAR;
            }
            else if ( found == ROBOSTATE_BUILDPOWER )
            {
                _roboPowerValue = 0;
                _roboPowerCellIndex = 0;
                _roboBuildingDuty = ROBOSTATE_BUILDPOWER;
                _roboBuildingCellID = _roboPowerCellID;
                _roboState &= ~ROBOSTATE_READYPOWER;
                _roboBuildingCell = _roboPowerCell;
            }
            else if ( found == ROBOSTATE_BUILDSAFE )
            {
                _roboSafetyValue = 0;
                _roboSafetyCellIndex = 0;
                _roboBuildingCellID = _roboSafetyCellID;
                _roboBuildingCell = _roboSafetyCell;
                _roboBuildingDuty = ROBOSTATE_BUILDSAFE;
                _roboState &= ~ROBOSTATE_READYSAFE;
            }
            else if ( found == ROBOSTATE_CHANGEPLACE )
            {
                _roboPositionValue = 0;
                _roboPositionCellIndex = 0;
                _roboBuildingCellID = _roboPositionCellID;
                _roboBuildingCell = _roboPositionCell;
                _roboBuildingDuty = ROBOSTATE_CHANGEPLACE;
                _roboState &= ~ROBOSTATE_FOUNDPLACE;
            }
        }
    }

    if ( !_roboVehicleDuty && !(_roboState & ROBOSTATE_DOCKINUSE) )
    {
        int found_time = 0;
        int found = 0;

        if ( _roboState & ROBOSTATE_READYDEFENSE )
        {
            if ( _roboEnemyValue )
            {
                if ( -_roboEnemyDelay > found_time )
                {
                    found = ROBOSTATE_BUILDDEFENSE;
                    found_time = -_roboEnemyDelay;
                }
            }
        }

        if ( _roboState & ROBOSTATE_READYCONQUER )
        {
            if ( _roboConqValue )
            {
                if ( -_roboConqDelay > found_time )
                {
                    found = ROBOSTATE_BUILDCONQUER;
                    found_time = -_roboConqDelay;
                }
            }
        }

        if ( _roboState & ROBOSTATE_READYRECON )
        {
            if ( _roboExploreValue )
            {
                if ( -_roboExploreDelay > found_time )
                {
                    found = ROBOSTATE_BUILDRECON;
                    found_time = -_roboExploreDelay;
                }
            }
        }
        if ( _roboState & ROBOSTATE_READYROBO )
        {
            if ( _roboDangerValue )
            {
                if ( -_roboDangerDelay > found_time )
                {
                    found = ROBOSTATE_BUILDROBO;
                    found_time = -_roboDangerDelay;
                }
            }
        }

        if ( found_time > 0 )
        {
            if ( found == ROBOSTATE_BUILDCONQUER )
            {
                _roboConqValue = 0;
                _roboConqCellIndex = 0;
                _roboVehicleCellID = _roboConqCellID;
                _roboVehicleCell = _roboConqCell;
                _roboVehicleDuty = ROBOSTATE_BUILDCONQUER;
                _roboState &= ~ROBOSTATE_READYCONQUER;
            }
            else if ( found == ROBOSTATE_BUILDDEFENSE )
            {
                _roboVehicleDuty = ROBOSTATE_BUILDDEFENSE;
                _roboEnemyValue = 0;
                _roboVehicleCommandID = _roboEnemyCommandID;
                _roboState &= ~ROBOSTATE_READYDEFENSE;
                _roboVehicleCellID = _roboEnemyCellID;
                _roboVehicleCell = _roboEnemyCell;
            }
            else if ( found == ROBOSTATE_BUILDRECON )
            {
                _roboVehicleDuty = ROBOSTATE_BUILDRECON;
                _roboExploreValue = 0;
                _roboExploreCellIndex = 0;
                _roboVehicleCellID = _roboExploreCellID;
                _roboVehicleCell = _roboExploreCell;
                _roboState &= ~ROBOSTATE_READYRECON;
            }
            else if ( found == ROBOSTATE_BUILDROBO )
            {
                _roboDangerValue = 0;
                _roboVehicleCommandID = _roboDangerCommandID;
                _roboVehicleCellID = _roboDangerCellID;
                _roboVehicleCell = _roboDangerCell;
                _roboVehicleDuty = ROBOSTATE_BUILDROBO;
                _roboState &= ~ROBOSTATE_READYROBO;
            }
        }
    }

    switch (_roboBuildingDuty)
    {
    case ROBOSTATE_BUILDRADAR:
        buildRadar(arg);
        break;

    case ROBOSTATE_BUILDPOWER:
        buildPower(arg);
        break;

    case ROBOSTATE_BUILDSAFE:
        buildSafe(arg);
        break;

    case ROBOSTATE_CHANGEPLACE:
        changePlace();
        break;

    default:
        break;
    }

    switch (_roboVehicleDuty)
    {
    case ROBOSTATE_BUILDCONQUER:
        buildConquer();
        break;

    case ROBOSTATE_BUILDDEFENSE:
        buildDefense();
        break;

    case ROBOSTATE_BUILDRECON:
        buildReconnoitre();
        break;

    case ROBOSTATE_BUILDROBO:
        buildRobo();
        break;

    default:
        break;
    }
}

void NC_STACK_yparobo::AI_service(update_msg *arg)
{
    if ( _roboDockUser )
    {
        int v4 = 0;
        for( NC_STACK_ypabact* &node : _kidList )
        {
            if (_roboDockUser == node->_commandID)
            {
                v4 = 1;
                break;
            }
        }

        if ( !v4 )
        {
            _roboDockTime = 0;
            _roboDockUser = 0;
            _roboState &= ~ROBOSTATE_DOCKINUSE;
        }
    }

    if ( _roboDockTime > 0 )
    {
        _roboDockTime -= arg->frameTime;
        if ( _roboDockTime <= 0 )
        {
            _roboDockTime = 0;

            if ( !_roboDockUser )
                _roboState &= ~ROBOSTATE_DOCKINUSE;
        }
    }

    if ( _roboDockTime <= 0 && !_roboDockUser )
        _roboState &= ~ROBOSTATE_DOCKINUSE;
}

void NC_STACK_yparobo::AI_solveTrouble(update_msg *arg)
{
    if ( (float)_energy / (float)_energy_max < 0.2 && (arg->gTime - _roboTestEnemyTime) > 20000 )
    {
        _roboTestEnemyTime = arg->gTime;

        int v3 = (float)_energy / (float)_energy_max < 0.1;

        for (NC_STACK_ypabact* &unit : _kidList )
        {
            if ( unit->_status == BACT_STATUS_NORMAL || unit->_status == BACT_STATUS_IDLE )
            {
                if ( unit->_bact_type != BACT_TYPES_GUN )
                {
                    int v7;

                    if ( v3 )
                    {
                        v7 = 1;
                        unit->_status_flg &= ~BACT_STFLAG_UNUSE;
                    }
                    else
                    {
                        v7 = !(unit->_status_flg & BACT_STFLAG_UNUSE) && !unit->_secndTtype;
                    }

                    float v10 = unit->_position.x - _position.x;
                    float v11 = unit->_position.z - _position.z;

                    if ( (v10 * v10 + v11 * v11) < 4320000.0 && unit->_primTtype == BACT_TGT_TYPE_UNIT && this == unit->_primT.pbact )
                    {
                        if ( v3 )
                        {
                            unit->setBACT_aggression(99);
                            unit->_status_flg &= ~BACT_STFLAG_ESCAPE;
                        }
                        else
                        {
                            unit->setBACT_aggression(60);
                        }
                    }

                    if ( v7 )
                    {
                        setTarget_msg arg67;

                        arg67.tgt_type = BACT_TGT_TYPE_UNIT;
                        arg67.priority = 0;
                        arg67.tgt.pbact = this;

                        unit->SetTarget(&arg67);
                    }
                }
            }
        }
    }
}

void NC_STACK_yparobo::AI_clearAttackerSlots()
{
    if ( _clock - _roboAttackersClearTime > 4000 )
    {
        _roboAttackersClearTime = _clock;
        for ( robo_t1 &ataker : _roboAttackers )
        {
            if ( ataker.field_0 )
            {
                int v6 = 0;
                for (NC_STACK_ypabact* &node : _kidList )
                {
                    if (node->_commandID == ataker.field_0)
                    {
                        v6 = 1;
                        break;
                    }
                }

                if ( !v6 )
                {
                    ataker.field_0 = 0;
                    ataker.field_4 = 0;
                }
            }
        }
    }
}

void NC_STACK_yparobo::searchEnemyRobo()
{
    for (NC_STACK_ypabact* &unit : _world->_unitsList)
    {
        if ( unit->_status != BACT_STATUS_DEAD &&
             unit != this && unit->_bact_type == BACT_TYPES_ROBO && !unit->IsIgnoredByAI() )
        {
            if ( unit->_pSector->IsCanSee(_owner) )
            {
                NC_STACK_ypabact *sel = nullptr;
                float selfl = 0.0;

                for (NC_STACK_ypabact* &kbct : _kidList )
                {
                    if (kbct->_status != BACT_STATUS_DEAD)
                    {
                        float v14 = (kbct->_position.XZ() - unit->_position.XZ()).square();

                        if ( !sel || v14 < selfl )
                        {
                            sel = kbct;
                            selfl = v14;
                        }
                    }
                }

                if ( sel )
                {
                    robo_arg134 v12;

                    v12.field_4 = 6;
                    v12.field_8 = unit->_owner;
                    v12.field_14 = 48;
                    v12.unit = sel;
                    v12.field_C = 0;
                    v12.field_10 = 0;

                    placeMessage(&v12);
                }
            }
        }
    }
}

void NC_STACK_yparobo::usersRoboEnergyCheck()
{
    float v1 = (float)_energy / (float)_energy_max;

    if ( v1 < 0.2 )
    {
        if ( v1 < 0.07 )
            v1 = 0.01;

        robo_arg134 v2;
        v2.field_4 = 40;
        v2.field_C = 0;
        v2.field_10 = 0;
        v2.field_14 = 98;
        v2.field_8 = (v1 * 60000.0 * 5.0);
        v2.unit = this;

        placeMessage(&v2);
    }
}

void NC_STACK_yparobo::checkCommander()
{
    bool userRobo =  (this == _world->getYW_userHostStation());

    float v31;

    for ( NC_STACK_ypabact* &commander : _kidList )
    {
        if ( commander->_status == BACT_STATUS_DEAD )
            continue;

        if ( _roboDockUser == commander->_commandID && _roboDockUser )
        {
            InitForce(commander);
            continue;
        }

        if ( commander->_bact_type == BACT_TYPES_GUN )
            continue;

        if ( commander->_status_flg & BACT_STFLAG_ESCAPE )
        {
            if ( commander->GetFightMotivation(&v31) )
            {
                if ( commander->_owner == commander->_pSector->owner && commander->_pSector->energy_power )
                {
                    setTarget_msg arg67;
                    arg67.tgt_type = BACT_TGT_TYPE_CELL;
                    arg67.priority = 0;
                    arg67.tgt_pos = commander->_position;

                    commander->SetTarget(&arg67);
                }

                commander->ChangeEscapeFlag(false);

                if ( userRobo )
                {
                    robo_arg134 arg134;
                    arg134.field_8 = commander->_commandID;
                    arg134.field_4 = 10;
                    arg134.field_10 = 0;
                    arg134.field_C = 0;
                    arg134.unit = commander;
                    arg134.field_14 = 28;

                    placeMessage(&arg134);
                }
            }
            continue;
        }

        if ( commander->GetFightMotivation(&v31) )
        {
            if ( v31 < 0.5 && userRobo )
            {
                bact_arg81 arg81;
                arg81.enrg_sum = 0;
                arg81.enrg_type = 3;

                commander->GetSummary(&arg81);

                int v18 = arg81.enrg_sum;

                arg81.enrg_sum = 0;
                arg81.enrg_type = 5;
                commander->GetSummary(&arg81);

                if ( v18 < arg81.enrg_sum )
                {
                    robo_arg134 arg134;
                    arg134.field_8 = 20000;
                    arg134.field_10 = 0;
                    arg134.field_C = 0;
                    arg134.field_4 = 18;
                    arg134.unit = commander;
                    arg134.field_14 = 42;

                    placeMessage(&arg134);
                }
            }

            if ( !userRobo )
            {
                if ( _clock - _roboAttackersTime > 1000 )
                {
                    _roboAttackersTime = _clock;

                    for ( robo_t1 &ataker : _roboAttackers )
                    {
                        if ( ataker.field_0 == commander->_commandID)
                        {
                            sb_0x4a7010__sub1(commander, &ataker);
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            commander->ChangeEscapeFlag(true);


            commander->_status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
            commander->_waypoints_count = 0;
            commander->_status_flg &= ~BACT_STFLAG_UNUSE;


            for (NC_STACK_ypabact* &nod : commander->_kidList)
                nod->_status_flg &= ~BACT_STFLAG_UNUSE;

            if ( commander->IsNeedsWaypoints() )
            {
                bact_arg124 arg124;
                arg124.steps_cnt = 32;
                arg124.from = commander->_position.XZ();
                arg124.to = _position.XZ();
                arg124.field_12 = 1;

                if ( ! commander->PathFinder(&arg124) )
                    break;

                if ( arg124.steps_cnt > 1 )
                {
                    arg124.steps_cnt = 32;
                    commander->SetPath(&arg124);

                    commander->_m_cmdID = _commandID;
                    commander->_m_owner = _owner;
                }
                else
                {
                    setTarget_msg arg67;
                    arg67.tgt.pbact = this;
                    arg67.priority = 0;
                    arg67.tgt_type = BACT_TGT_TYPE_UNIT;
                    commander->SetTarget(&arg67);
                }
            }
            else
            {
                setTarget_msg arg67;
                arg67.tgt.pbact = this;
                arg67.tgt_type = BACT_TGT_TYPE_UNIT;
                arg67.priority = 0;
                commander->SetTarget(&arg67);
            }

            if ( userRobo )
            {
                robo_arg134 arg134;
                arg134.field_8 = commander->_commandID;
                arg134.field_10 = 0;
                arg134.field_C = 0;
                arg134.field_4 = 9;
                arg134.unit = commander;
                arg134.field_14 = 40;

                placeMessage(&arg134);

                if ( _world->_isNetGame  )
                {
                    NC_STACK_ypabact *enemy = commander->GetSectorTarget(commander->_cellId);
                    if ( enemy )
                    {
                        uamessage_logmsg logMsg;
                        logMsg.msgID = UAMSG_LOGMSG;
                        logMsg.owner = _owner;
                        logMsg.sender = enemy->_gid;
                        logMsg.senderOwner = enemy->_owner;
                        logMsg.pr1 = 0;
                        logMsg.pr2 = 0;
                        logMsg.pr3 = 0;
                        logMsg.pri = 34;
                        logMsg.id = 19;

                        _world->NetBroadcastMessage(&logMsg, sizeof(logMsg), true);
                    }
                }
            }
            else
            {
                for ( const TBactAttacker &ainf : commander->_attackersList )
                {
                    if (_world->_userRobo->_owner == ainf.attacker->_owner)
                    {
                        NC_STACK_ypabact *realbact = ainf.attacker;
                        if ( realbact->_bact_type == BACT_TYPES_MISSLE ) //If missile
                        {
                            NC_STACK_ypamissile *miss = dynamic_cast<NC_STACK_ypamissile *>(realbact);
                            realbact = miss->GetLauncherBact(); //Get emitter bact
                        }

                        if ( realbact->_host_station != realbact->_parent &&
                             realbact->_parent )
                            realbact = realbact->_parent;

                        robo_arg134 arg134;
                        arg134.field_8 = 0;
                        arg134.field_10 = 0;
                        arg134.field_C = 0;
                        arg134.field_4 = 19;
                        arg134.unit = realbact;
                        arg134.field_14 = 34;
                        placeMessage(&arg134);
                        break;
                    }
                }
            }
        }
    }
}

void NC_STACK_yparobo::checkDanger()
{
    int v11 = 0;
    int v12;

    for(int i = -1; i <= 1; i++)
    {
        for(int j = -1; j <= 1; j++)
        {
            Common::Point pt = _cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                for ( NC_STACK_ypabact* &v7 : _world->SectorAt(pt).unitsList )
                {
                    if (v7->_owner != _owner && !v7->IsIgnoredByAI() &&
                        v7->_status != BACT_STATUS_DEAD && v7->_weapon != -1 && v7->HasMinigun())
                    {
                        v11 = 1;
                        v12 = v7->_commandID;
                        break;
                    }
                }
            }

        }
    }

    if ( v11 )
    {
        if ( !(_roboState & ROBOSTATE_DANGER) && _roboState & ROBOSTATE_PLAYERROBO )
        {
            robo_arg134 arg134;
            arg134.field_4 = 13;
            arg134.field_8 = v12;
            arg134.field_10 = 0;
            arg134.field_C = 0;
            arg134.unit = this;
            arg134.field_14 = 99;

            placeMessage(&arg134);
        }

        _roboState |= ROBOSTATE_DANGER;
    }
    else
    {
        _roboState &= ~ROBOSTATE_DANGER;
    }
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub0(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

//

    if ( cell.owner != _owner )
        return -1;

    if ( cell.PurposeType != cellArea::PT_NONE )
        return -1;

    float v8 = _cellId.LengthTo<float>( sc );

    float v14 = ( v8 <= 0.01 ? 500.0 : 1000.0 / v8 );

    int v9 = 0;

    for (Common::Point ds : NearDxy)
    {
        if ( !_world->SectorAt(sc + ds).IsCanSee(_owner) )
        {
            v9 = 1;
            v14 += 10.0;
        }
    }

    if (  !v9 || _world->IsSectorBorder(sc, 2) )
        return -1;

    return v14;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub1(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( cell.IsCanSee(_owner) )
        return -1;

    float v13 = _cellId.LengthTo<float>( sc );

    if ( v13 <= 0.01 )
        v13 = 0.0;

    for (Common::Point ds : NearDxy)
    {
        if ( !_world->SectorAt(sc + ds).IsCanSee(_owner) )
        {
            v13 += 5.0;
        }
    }

    return v13;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub2(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

//

    if ( cell.owner != _owner || cell.PurposeType != cellArea::PT_NONE )
        return -1;

    float v9 = _cellId.LengthTo<float>( sc );

    float v14;

    if ( v9 <= 0.01 )
        v14 = 500.0;
    else
        v14 = 1000.0 / v9;

    for (Common::Point ds : NearDxy)
    {
        cellArea &cll = _world->SectorAt(sc + ds);

        if ( cll.PurposeType != cellArea::PT_NONE && _owner == cll.owner)
            v14 += 5.0;

    }

    return v14;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub3(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( !cell.IsCanSee(_owner) )
        return -1;

    if ( cell.energy_power >= 255 )
        return -1;

    if ( cell.PurposeType != cellArea::PT_NONE )
        return -1;

//

    if ( cell.owner != _owner )
        return -1;

    float v9 = _cellId.LengthTo<float>( sc );
    float v26 = v9 <= 0.01 ? 500.0 : 1000.0 / v9;

    if ( v9 > 8.0 )
        return -1;

    float v27 = v26 + (255 - cell.energy_power) / 3.0;

    for (Common::Point ds : NearDxy)
    {
        cellArea &cll = _world->SectorAt(sc + ds);

        if ( cll.owner == _owner )
        {
            int v11 = cll.GetEnergy();
            v27 += (255 - v11) * 0.05 + 10.0;
        }

        if ( cll.PurposeType == cellArea::PT_POWERSTATION )
            v27 *= 0.7;
    }

    v27 -= 5.0 * cell.unitsList.size();

    return v27;
}

int32_t NC_STACK_yparobo::yparobo_func70__sub6__sub4(const Common::Point &sc)
{
    cellArea &cell = _world->SectorAt(sc);

    if ( cell.owner == _owner )
        return -1;

    float v7 = _cellId.LengthTo<float>( sc );
    int32_t val = 0;

    if ( v7 <= 0.01 )
        val = 1500;
    else
        val = 1000.0 / v7;

    if ( cell.IsCanSee(_owner) )
    {
        if ( cell.PurposeType != cellArea::PT_NONE )
            val = 20;
        if ( cell.PurposeType == cellArea::PT_GATEOPENED )
            val += 40;
        if ( cell.PurposeType == cellArea::PT_POWERSTATION )
            val += 50;
    }

    for ( Common::Point ds : NearDxy )
    {
        Common::Point pt = sc + ds;

        if ( _world->IsGamePlaySector(pt))
        {
            if ( _world->SectorAt(pt).owner == _owner )
                val += 3;
        }
    }

    if ( cell.IsCanSee(_owner) )
    {
        for ( NC_STACK_ypabact* &bct : cell.unitsList )
        {
            if ( bct->_owner != _owner && bct->_owner && !bct->IsIgnoredByAI() )
            {
                if ( bct->_bact_type != BACT_TYPES_MISSLE)
                {
                    if ( bct->_bact_type == BACT_TYPES_ROBO)
                        val += 100;
                    else
                        val += 5;
                }
            }
        }
    }

    return val;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub8()
{
    int v4 = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.PurposeType == cellArea::PT_BUILDINGS )
        {
            for ( NC_STACK_ypabact* &bct : cll.unitsList )
            {
                if ( bct->_bact_type == BACT_TYPES_GUN && bct->_status != BACT_STATUS_DEAD && _owner == bct->_owner )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                    int a4 = gun->IsRoboGun();

                    if ( a4 == 0 && ( bct->_weapon != -1 || bct->HasMinigun() ) )
                    {
                        v4++;
                        break;
                    }
                }
            }
        }
    }

    return v4;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub9()
{
    int v4 = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.PurposeType == cellArea::PT_BUILDINGS )
        {
            for (NC_STACK_ypabact* &bct : cll.unitsList )
            {
                if ( bct->_bact_type == BACT_TYPES_GUN && bct->_status != BACT_STATUS_DEAD && _owner == bct->_owner )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                    int a4 = gun->IsRoboGun();

                    if ( a4 == 0 && bct->_weapon == -1 && !bct->HasMinigun() )
                    {
                        v4++;
                        break;
                    }
                }
            }
        }
    }

    return v4;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub10()
{
    int v4 = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( cll.owner == _owner )
            v4++;
    }

    return v4;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub11()
{
    int v4 = 0;

    for (cellArea &cll : _world->Sectors())
    {
        if ( !cll.IsCanSee(_owner) )
            v4++;
    }

    return v4;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub12()
{
    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_bact_type == BACT_TYPES_ROBO)
            {
                if ( _owner != node->_owner && !node->IsIgnoredByAI() )
                {
                    Common::Point dist = _cellId.AbsDistance( node->_cellId );
                    if ( dist.x < 3 && dist.y < 3 )
                        return 1;
                }
            }
        }
    }

    return 0;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub13()
{
    for (int i = -2; i <= 2; i++)
    {
        for (int j = -2; j <= 2; j++)
        {
            Common::Point pt = _cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                for ( NC_STACK_ypabact* &bct : _world->SectorAt(pt).unitsList )
                {
                    if ( bct->_owner )
                    {
                        if ( bct->_owner != _owner && !bct->IsIgnoredByAI() )
                        {
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_ROBO && bct->_status != BACT_STATUS_DEAD )
                                return 1;
                        }
                    }
                }
            }
        }
    }

    return 0;
}

float NC_STACK_yparobo::sub_4F4C6C(NC_STACK_ypabact *bact)
{
    if ( (bact->_status_flg & BACT_STFLAG_DEATH1) || bact->IsIgnoredByAI() )
        return -1.0;

    float v8 = _cellId.LengthTo<float>( bact->_cellId );

    if (v8 != 0.0)
        v8 = 100.0 / v8;
    else
        v8 = 200.0;

    if ( bact->_pSector->owner == _owner )
        v8 += 100.0;

    return v8;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub5(int *a2, Common::Point *pCellId)
{
    float v32 = -0.5;

    int v29 = 0;

    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_owner != _owner  &&  node->_owner  &&  node->_status != BACT_STATUS_DEAD )
            {
                Common::Point dist = _cellId.AbsDistance( node->_cellId );
                if ( dist.x <= 2 && dist.y <= 2 && !node->IsIgnoredByAI() )
                {
                    *a2 = node->_commandID;
                    *pCellId = node->_cellId;

                    return 500;
                }

                for(NC_STACK_ypabact* &ndbct : node->_kidList)
                {

                    if ( (ndbct->_gid & 0xF) == _roboEnemyDiv )
                    {
                        int v26 = 0;

                        if ( ndbct->_bact_type == BACT_TYPES_GUN )
                        {
                            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( ndbct );
                            v26 = gun->IsRoboGun();
                        }

                        if ( ndbct->_pSector->IsCanSee(_owner) )
                        {
                            if ( ndbct->_status != BACT_STATUS_DEAD && !v26 )
                            {
                                float tmp = sub_4F4C6C(ndbct);
                                if ( tmp > v32 )
                                {
                                    v32 = tmp;
                                    v29 = ndbct->_commandID;
                                    *pCellId = ndbct->_cellId;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    _roboEnemyDiv++;

    if ( _roboEnemyDiv > 15 )
        _roboEnemyDiv = 0;

    if ( v32 < 0.0 )
    {
        *a2 = 0;
        return -1;
    }

    *a2 = v29;
    return v32;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub6(int *a2, Common::Point *pCellId)
{
    float v32 = -0.5;
    int v21 = 0;

    if (_kidRef)
    {
        for(NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if ( node->_owner != _owner && node->_owner && node->_status != BACT_STATUS_DEAD )
            {
                if ( node->_pSector->IsCanSee(_owner) )
                {
                    float v13 = sub_4F4C6C(node);
                    if ( v13 > v32 )
                    {
                        v21 = node->_commandID;
                        v32 = v13;
                        *pCellId = node->_cellId;
                    }
                }
            }
        }
    }

    if ( v32 < 0.0 )
    {
        *a2 = 0;
        return -1;
    }

    *a2 = v21;
    return v32;
}

int NC_STACK_yparobo::sub_4F4E48(const Common::Point &cellId)
{
    float v14 = 0.0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            cellArea &cll = _world->SectorAt( cellId + Common::Point(i, j) );

            yw_arg176 arg176;
            arg176.owner = cll.owner;

            _world->ypaworld_func176(&arg176);

            float v15;

            if ( i || j )
                v15 = 1.0;
            else
                v15 = 3.0;

            if ( _owner == cll.owner )
                v14 += cll.energy_power * arg176.field_4 + v15;
            else
                v14 -= cll.energy_power * arg176.field_4 + v15;

            for ( NC_STACK_ypabact* &bct : cll.unitsList )
            {
                float v16 = 0;

                if ( bct->_bact_type == BACT_TYPES_ROBO && bct != this )
                {
                    v16 = 5.0;
                }
                else
                {
                    if ( bct->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

                        if ( !gun->IsRoboGun() )
                            v16 = 1.0;
                    }
                }

                if ( v16 > 0.0 )
                {
                    if ( bct->_owner == _owner )
                        v14 += v16;
                    else
                        v14 -= v16;
                }
            }
        }
    }

    return v14;
}

int NC_STACK_yparobo::yparobo_func70__sub6__sub7()
{
    Common::Point pt( _roboPositionCellIndex % _wrldSectors.x, _roboPositionCellIndex / _wrldSectors.x);

    int a1_4 = sub_4F4E48(_cellId);
    int v14 = sub_4F4E48( pt );

    float v12 = _cellId.LengthTo<float>( pt );

    if ( v12 > 0.0 && a1_4 < v14 )
        v14 = ((float)v14 * (1.0 - v12 * 0.8 / 91.0));

    if ( v14 <= a1_4 )
        return -1;

    return v14;
}

void NC_STACK_yparobo::AI_checkWorld(update_msg *arg)
{
    int v6 = 1;
    if ( _roboNewAI )
    {
        _roboConqDelay -= arg->frameTime;
        if ( _roboConqDelay > 0 )
            v6 = 0;
    }

    robo_arg128 arg128;

    if ( _roboEpConquer && v6 )
    {
        int v89 = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for (int i = v89; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboConqCellIndex )
            {
                _roboConqCellIndex = 0;
                _roboState |= ROBOSTATE_READYCONQUER;
                return;
            }

            Common::Point pt (_roboConqCellIndex % _wrldSectors.x, _roboConqCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int v12 = yparobo_func70__sub6__sub4(pt);
                if ( v12 > _roboConqValue )
                {
                    arg128.flags = 2;
                    arg128.tgType = BACT_TGT_TYPE_CELL;
                    arg128.tgt_pos = World::SectorIDToCenterPos3(pt);

                    yparobo_func128(&arg128);

                    if ( !arg128.comm_bacto )
                    {
                        _roboConqValue = v12;
                        _roboConqCellID = _roboConqCellIndex;
                        _roboConqCell = &_world->SectorAt( _roboConqCellIndex );
                        _roboConqTime = arg->gTime;
                    }
                }
            }

            _roboConqCellIndex++;
        }
    }
    else
    {
        _roboConqCellIndex = 0;
        _roboConqValue = 0;
        _roboState &= ~ROBOSTATE_READYCONQUER;
    }

    v6 = 1;
    if ( _roboNewAI )
    {
        _roboEnemyDelay -= arg->frameTime;

        if ( _roboEnemyDelay > 0 )
            v6 = 0;
    }

    if ( yparobo_func70__sub6__sub13() )
        v6 = 1;

    if ( _roboEpDefense && v6 )
    {
        Common::Point cellId;
        int v91;

        int v17 = yparobo_func70__sub6__sub5(&v91, &cellId);
        if ( v17 > _roboEnemyValue )
        {
            arg128.flags = 2;
            arg128.tgType = BACT_TGT_TYPE_UNIT;
            arg128.prim_comm_id = v91;
            yparobo_func128(&arg128);

            if ( !arg128.comm_bacto )
            {
                _roboEnemyValue = v17;
                _roboEnemyCommandID = v91;
                _roboEnemyTime = arg->gTime;
                _roboEnemyCellID = cellId.y * _wrldSectors.x + cellId.x;
            }
        }
        _roboState |= ROBOSTATE_READYDEFENSE;
    }
    else
    {
        _roboEnemyValue = 0;
        _roboState &= ~ROBOSTATE_READYDEFENSE;
    }

    v6 = 1;
    if ( _roboNewAI )
    {
        _roboDangerDelay -= arg->frameTime;
        if ( _roboDangerDelay > 0 )
            v6 = 0;
    }

    if ( yparobo_func70__sub6__sub12() )
        v6 = 1;

    if ( _roboEpRobo && v6 )
    {
        Common::Point cellId;
        int v91;
        int v22 = yparobo_func70__sub6__sub6(&v91, &cellId);

        if ( v22 > _roboDangerValue )
        {
            robo_arg128 arg128_1;
            arg128_1.flags = 2;
            arg128_1.tgType = BACT_TGT_TYPE_UNIT;
            arg128_1.prim_comm_id = v91;

            yparobo_func128(&arg128_1);

            if ( !arg128_1.comm_bacto )
            {
                _roboDangerValue = v22;
                _roboDangerCommandID = v91;
                _roboDangerTime = arg->gTime;
                _roboDangerCellID = _wrldSectors.x * cellId.y + cellId.x;
            }
        }
        _roboState |= ROBOSTATE_READYROBO;
    }
    else
    {
        _roboDangerValue = 0;
        _roboState &= ~ROBOSTATE_READYROBO;
    }

    v6 = 1;
    if ( _roboNewAI )
    {
        _roboPositionDelay -= arg->frameTime;
        if ( _roboPositionDelay > 0 )
            v6 = 0;
    }

    if ( _roboEpChangePlace && v6 )
    {
        int v25 = (_wrldSectors.x + _wrldSectors.y) / 10 + 1;

        for (int i = v25; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboPositionCellIndex )
            {
                _roboPositionCellIndex = 0;
                _roboState |= ROBOSTATE_FOUNDPLACE;
                return;
            }

            if ( _world->IsGamePlaySector( {_roboPositionCellIndex % _wrldSectors.x, _roboPositionCellIndex / _wrldSectors.x} ) )
            {
                int v34 = yparobo_func70__sub6__sub7();
                if ( v34 > _roboPositionValue )
                {
                    _roboPositionValue = v34;
                    _roboPositionCellID = _roboPositionCellIndex;
                    _roboPositionTime = arg->gTime;
                }
            }
            _roboPositionCellIndex++;
        }
    }
    else
    {
        _roboPositionValue = 0;
        _roboPositionCellIndex = 0;
        _roboState &= ~ROBOSTATE_FOUNDPLACE;
    }

    yw_arg176 v85;
    v85.owner = _owner;
    _world->ypaworld_func176(&v85);

    v6 = 1;
    if ( _roboNewAI )
    {
        _roboPowerDelay -= arg->frameTime;
        if ( _roboPowerDelay > 0 )
            v6 = 0;
    }

    if ( v85.field_4 == 0.0 )
    {
        if ( _roboEpPower )
            v6 = 1;
    }

    float v93;
    if ( _roboEpPower )
        v93 = 100.0 / (float)_roboEpPower * 0.9;
    else
        v93 = 0.89999998;

    if ( _roboEpPower && (v85.field_8 > v93 || v85.field_4 < 0.0001) && v6 )
    {
        int v99 = (_wrldSectors.x + _wrldSectors.y) / 20 + 1;
        for (int i = v99; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboPowerCellIndex )
            {
                _roboPowerCellIndex = 0;
                _roboState |= ROBOSTATE_READYPOWER;
                return;
            }

            Common::Point pt(_roboPowerCellIndex % _wrldSectors.x, _roboPowerCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int v46 = yparobo_func70__sub6__sub3(pt);
                if ( v46 > _roboPowerValue )
                {
                    _roboPowerValue = v46;
                    _roboPowerCell = &_world->SectorAt( _roboPowerCellIndex );
                    _roboPowerCellID = _roboPowerCellIndex;
                    _roboPowerTime = arg->gTime;
                }
            }
            _roboPowerCellIndex++;
        }
    }
    else
    {
        _roboPowerValue = 0;
        _roboPowerCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYPOWER;
    }

    v6 = 1;
    if ( _roboNewAI )
    {
        _roboRadarDelay -= arg->frameTime;
        if ( _roboRadarDelay > 0 )
            v6 = 0;
    }

    if ( _roboEpRadar && (yparobo_func70__sub6__sub11() / 64 > yparobo_func70__sub6__sub9()) && v6 )
    {
        int v48 = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for(int i = v48; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboRadarCellIndex )
            {
                _roboRadarCellIndex = 0;
                _roboState |= ROBOSTATE_READYRADAR;
                return;
            }

            Common::Point pt(_roboRadarCellIndex % _wrldSectors.x, _roboRadarCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int v58 = yparobo_func70__sub6__sub0(pt);

                if ( v58 > _roboRadarValue )
                {
                    _roboRadarValue = v58;
                    _roboRadarCell = &_world->SectorAt( _roboRadarCellIndex );
                    _roboRadarCellID = _roboRadarCellIndex;
                    _roboRadarTime = arg->gTime;
                }
            }
            _roboRadarCellIndex++;
        }
    }
    else
    {
        _roboRadarCellIndex = 0;
        _roboRadarValue = 0;
        _roboState &= ~ROBOSTATE_READYRADAR;
    }


    v6 = 1;
    if ( _roboNewAI )
    {
        _roboSafetyDelay -= arg->frameTime;
        if ( _roboSafetyDelay > 0 )
            v6 = 0;
    }

    int v92;
    if ( _roboEpSafety )
        v92 = 10 * (100 / _roboEpSafety);
    else
        v92 = 10;

    if ( _roboEpSafety && (yparobo_func70__sub6__sub10() / v92) >= yparobo_func70__sub6__sub8() && v6 )
    {
        int v60 = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for(int i = v60; i > 0; i--)
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboSafetyCellIndex )
            {
                _roboSafetyCellIndex = 0;
                _roboState |= ROBOSTATE_READYSAFE;
                return;
            }

            Common::Point pt(_roboSafetyCellIndex % _wrldSectors.x, _roboSafetyCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int v71 = yparobo_func70__sub6__sub2(pt);

                if ( v71 > _roboSafetyValue )
                {
                    _roboSafetyValue = v71;
                    _roboSafetyCell = &_world->SectorAt( _roboSafetyCellIndex );
                    _roboSafetyCellID = _roboSafetyCellIndex;
                    _roboSafetyTime = arg->gTime;
                }
            }
            _roboSafetyCellIndex++;
        }
    }
    else
    {
        _roboSafetyValue = 0;
        _roboSafetyCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYSAFE;
    }


    v6 = 1;
    if ( _roboNewAI )
    {
        _roboExploreDelay -= arg->frameTime;
        if ( _roboExploreDelay > 0 )
            v6 = 0;
    }

    if ( _roboEpRecon && v6 )
    {
        int v90 = (_wrldSectors.y + _wrldSectors.x) / 20 + 1;

        for (int i = v90; i > 0; i-- )
        {
            if ( _wrldSectors.x * _wrldSectors.y == _roboExploreCellIndex )
            {
                _roboExploreCellIndex = 0;
                _roboState |= ROBOSTATE_READYRECON;
                return;
            }

            Common::Point pt(_roboExploreCellIndex % _wrldSectors.x, _roboExploreCellIndex / _wrldSectors.x);

            if ( _world->IsGamePlaySector(pt) )
            {
                int v81 = yparobo_func70__sub6__sub1(pt);

                if ( v81 > _roboExploreValue )
                {
                    robo_arg128 arg128_1;
                    arg128_1.tgType = BACT_TGT_TYPE_CELL;
                    arg128_1.flags = 2;
                    arg128_1.tgt_pos = World::SectorIDToCenterPos3(pt);

                    yparobo_func128(&arg128_1);

                    if ( !arg128_1.comm_bacto )
                    {
                        _roboExploreValue = v81;
                        _roboExploreCellID = _roboExploreCellIndex;
                        _roboExploreCell = &_world->SectorAt( _roboExploreCellIndex );
                        _roboExploreTime = arg->gTime;
                    }
                }
            }
            _roboExploreCellIndex++;
        }
    }
    else
    {
        _roboExploreValue = 0;
        _roboExploreCellIndex = 0;
        _roboState &= ~ROBOSTATE_READYRECON;
    }
}

void NC_STACK_yparobo::AI_layer3(update_msg *arg)
{
    int a4 = getBACT_bactCollisions();

    // Feed the shared live XYZ Kamikaze destination into the native Robo
    // locomotion path. Direct player control is rejected by the shared helper.
    ApplyKamikazeRammingGuidance();

    float tmp = _target_vec.length();

    if ( tmp <= 0.0 )
        _target_dir = vec3d(0.0, 0.0, 0.0);
    else
        _target_dir = _target_vec / tmp;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _roboState & ROBOSTATE_MOVE )
            doBeamUpdate(arg->frameTime);

        if ( !a4 || ! CollisionWithBact(arg->frameTime) )
        {
            checkCollisions(arg->frameTime * 0.001);

            if ( IsPlayerRobo() )
            {
                if ( !UpdatePlayerMobileMove(arg) && !UpdatePlayerRoboIdleMotion(arg) )
                    wallow(arg);
            }
            else
            {
                AI_doMove(arg);
            }

            if ( IsPlayerRobo() )
            {
                searchEnemyRobo();
                usersRoboEnergyCheck();
            }

            if ( !IsPlayerRobo() )
            {
                AI_checkWorld(arg);
                AI_doWish(arg);
                AI_service(arg);
                AI_clearAttackerSlots();
            }

            checkCommander();

            checkDanger();

            if ( _roboState & ROBOSTATE_DANGER && !IsPlayerRobo() )
                AI_solveTrouble(arg);
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
}

// Create squad for robo
bool NC_STACK_yparobo::MakeSquad(const std::vector<int> &VhclIDS, vec3d pos, bool usable)
{
    size_t curid = 0;
    int col = sqrt(VhclIDS.size()) + 2;

    ypaworld_arg146 arg146;
    arg146.pos.y = pos.y;
    arg146.pos.x = 100.0 * (curid % col - col / 2.0) + pos.x;
    arg146.pos.z = 100.0 * (curid / col) + pos.z;

    arg146.vehicle_id = VhclIDS[0];

    NC_STACK_ypabact *squad_commander = _world->ypaworld_func146(&arg146); //Create first bact

    if ( !squad_commander )
        return false;

    squad_commander->setBACT_bactCollisions( getBACT_bactCollisions() );

    if ( !usable )
        squad_commander->_status_flg |= BACT_STFLAG_UNUSE;

    squad_commander->_owner = _owner;
    squad_commander->_commandID = dword_5B1128;
    squad_commander->_host_station = this;
    squad_commander->InheritActiveDebuffFromParent(this);
    squad_commander->_aggr = 60;

    setState_msg arg78;
    arg78.newStatus = BACT_STATUS_IDLE;
    arg78.setFlags = 0;
    arg78.unsetFlags = 0;

    squad_commander->SetState(&arg78);

    setTarget_msg arg67;
    // The squad is already at its spawn position. A CELL target here is not a
    // real order: in a hostile sector TargetAssess() treats it as an attack and
    // makes the newly created squad fire at the ground around itself.
    arg67.tgt_type = BACT_TGT_TYPE_NONE;
    arg67.priority = 0;

    squad_commander->SetTarget(&arg67);

    for ( curid = 1; curid < VhclIDS.size(); curid++)
    {
        arg146.vehicle_id = VhclIDS[VhclIDS.size() - curid]; // CHECK IT Why from end ?

        arg146.pos.x = 100.0 * (curid % col - col / 2.0) + pos.x;
        arg146.pos.z = 100.0 * (curid / col) + pos.z;

        NC_STACK_ypabact *next_bact = (NC_STACK_ypabact *)_world->ypaworld_func146(&arg146);

        if ( !next_bact )
            break;

        squad_commander->AddSubject(next_bact); // Add to squad commander list

        next_bact->SetTarget(&arg67);

        next_bact->setBACT_bactCollisions( getBACT_bactCollisions() );

        if ( !usable )
            next_bact->_status_flg |= BACT_STFLAG_UNUSE;

        next_bact->_owner = _owner;
        next_bact->_commandID = dword_5B1128;
        next_bact->_host_station = this;
        next_bact->InheritActiveDebuffFromParent(this);
        next_bact->_aggr = 60;

        next_bact->SetState(&arg78);
    }

    dword_5B1128++;

    AddSubject(squad_commander);  //Add squad commander into robo list

    return true;
}

int NC_STACK_yparobo::placeMessage(robo_arg134 *arg)
{
    if ( arg->unit && arg->unit->_bact_type == BACT_TYPES_GUN && arg->field_4 != 7 && arg->field_4 != 19 && arg->field_4 != 6 )
        return 0;

    if ( !yparobo_func134__sub1(arg) )
        return 0;

    yw_arg159 v8;
    v8.Priority = arg->field_14;
    v8.unit = arg->unit;
    v8.MsgID = arg->field_4;

    switch ( arg->field_4 )
    {
    case 2:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_SECTCONQ);
        break;

    case 6:
        v8.txt =  Locale::Text::Feedback(Locale::FEEDBACK_FOUNDEHS);
        break;

    case 11:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_HSDESTR);
        break;

    case 12:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_OWNHSDESTR);
        break;

    case 13:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_ENMYNEARHS);
        break;

    case 18:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_REQSUP);
        break;

    case 31:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_FLAKDESTR);
        break;

    case 32:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_RADARDESTR);
        break;

    case 33:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_ATKPS);
        break;

    case 34:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_ATKFLAK);
        break;

    case 35:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_ATKRADAR);
        break;

    case 40:
        v8.txt = Locale::Text::Feedback(Locale::FEEDBACK_HSENERGY);
        break;

    default:
        break;
    }

    _world->ypaworld_func159(&v8);

    return 1;
}
