// NC_STACK_yparobo core: host station construction, parameters, beam updates, collisions, death, energy and the property
// getters/setters. The AI and the player-specific behaviour live in yparobo_ai.cpp and yparobo_player.cpp
// (shared helpers: yparobo_internal.h).
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

int dword_5B1128 = 1;
robo_t2 stru_5B0628[100];
int dword_515138[8];

size_t NC_STACK_yparobo::Init(IDVList &stak)
{
    if ( !NC_STACK_ypabact::Init(stak) )
        return 0;

    _roboEpConquer = 30;
    _roboEpDefense = 90;
    _roboEpRadar = 30;
    _roboEpPower = 50;
    _roboEpSafety = 50;
    _roboEpRecon = 80;
    _roboEpChangePlace = 100;
    _roboWFlags = 0;
    _roboFillMode = 15;

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_INPUTTING:
                setBACT_inputting(val.Get<bool>());
                break;

            case ROBO_ATT_EP_CONQUER:
                setROBO_epConquer(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_DEFENSE:
                setROBO_epDefense(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RADAR:
                setROBO_epRadar(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_POWER:
                setROBO_epPower(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_SAFETY:
                setROBO_epSafety(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_VEHICLE:
                setROBO_battVehicle(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_BEAM:
                setROBO_battBeam(val.Get<int32_t>());
                break;

            case ROBO_ATT_FILL_MODE:
                setROBO_fillMode(val.Get<int32_t>());
                break;

            case ROBO_ATT_WAIT_SWAY:
                setROBO_waitSway( val.Get<int32_t>() );
                break;

            case ROBO_ATT_WAIT_ROTATE:
                setROBO_waitRotate( val.Get<int32_t>() );
                break;

            case ROBO_ATT_EP_CHANGEPLACE:
                setROBO_epChangeplace(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RECONNOITRE:
                setROBO_epReconnoitre(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_ROBO:
                setROBO_epRobo(val.Get<int32_t>());
                break;

            case ROBO_ATT_VIEWANGLE:
                setROBO_viewAngle(val.Get<int32_t>());
                break;

            case ROBO_ATT_SAFDELAY:
                setROBO_safDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_POWDELAY:
                setROBO_powDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RADDELAY:
                setROBO_radDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CPLDELAY:
                setROBO_cplDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_DEFDELAY:
                setROBO_defDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CONDELAY:
                setROBO_conDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_ROBDELAY:
                setROBO_robDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RECDELAY:
                setROBO_recDelay(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    _bact_type = BACT_TYPES_ROBO;

    return 1;
}

size_t NC_STACK_yparobo::Deinit()
{
    return NC_STACK_ypabact::Deinit();
}

size_t NC_STACK_yparobo::SetParameters(IDVList &stak)
{
    NC_STACK_ypabact::SetParameters(stak);

    for( auto& it : stak )
    {
        IDVPair &val = it.second;

        if ( !val.Skip )
        {
            switch (val.ID)
            {
            case BACT_ATT_INPUTTING:
                setBACT_inputting(val.Get<bool>());
                break;

            case ROBO_ATT_PROTO:
                setROBO_proto(val.Get<World::TRoboProto *>());
                break;

            case ROBO_ATT_EP_CONQUER:
                setROBO_epConquer(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_DEFENSE:
                setROBO_epDefense(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RADAR:
                setROBO_epRadar(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_POWER:
                setROBO_epPower(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_SAFETY:
                setROBO_epSafety(val.Get<int32_t>());
                break;

            case ROBO_ATT_COMM_COUNT:
                setROBO_commCount(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_VEHICLE:
                setROBO_battVehicle(val.Get<int32_t>());
                break;

            case ROBO_ATT_BATT_BEAM:
                setROBO_battBeam(val.Get<int32_t>());
                break;

            case ROBO_ATT_FILL_MODE:
                setROBO_fillMode(val.Get<int32_t>());
                break;

            case ROBO_ATT_WAIT_SWAY:
                setROBO_waitSway( val.Get<int32_t>() );
                break;

            case ROBO_ATT_WAIT_ROTATE:
                setROBO_waitRotate( val.Get<int32_t>() );
                break;

            case ROBO_ATT_EP_CHANGEPLACE:
                setROBO_epChangeplace(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_RECONNOITRE:
                setROBO_epReconnoitre(val.Get<int32_t>());
                break;

            case ROBO_ATT_EP_ROBO:
                setROBO_epRobo(val.Get<int32_t>());
                break;

            case ROBO_ATT_VIEWANGLE:
                setROBO_viewAngle(val.Get<int32_t>());
                break;

            case ROBO_ATT_SAFDELAY:
                setROBO_safDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_POWDELAY:
                setROBO_powDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RADDELAY:
                setROBO_radDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CPLDELAY:
                setROBO_cplDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_DEFDELAY:
                setROBO_defDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_CONDELAY:
                setROBO_conDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_ROBDELAY:
                setROBO_robDelay(val.Get<int32_t>());
                break;

            case ROBO_ATT_RECDELAY:
                setROBO_recDelay(val.Get<int32_t>());
                break;

            default:
                break;
            }
        }
    }

    return 1;
}

void NC_STACK_yparobo::sub_4A9F24(NC_STACK_ypabact *unit)
{
    if ( _roboState & ROBOSTATE_SETDOCKTARGET )
    {
        setTarget_msg arg67;
        arg67.tgt_type = _roboDockTargetType;
        arg67.priority = 0;
        arg67.tgt.pbact = _roboDockTargetBact;
        arg67.tgt_pos.x = _roboDockTargetPos.x;
        arg67.tgt_pos.z = _roboDockTargetPos.z;

        if ( arg67.tgt_type == BACT_TGT_TYPE_UNIT )
        {
            arg67.priority = _roboDockTargetCommandID;

            if ( yparobo_func132(&arg67) )
            {
                if ( !arg67.tgt.pbact->_pSector->IsCanSee(_owner) )
                {
                    arg67.tgt_type = BACT_TGT_TYPE_CELL;
                    arg67.tgt_pos.x = _roboDockTargetPos.x;
                    arg67.tgt_pos.z = _roboDockTargetPos.z;

                    _roboDockAggr = 50;
                }
            }
            else
            {
                arg67.tgt_type = BACT_TGT_TYPE_CELL;
                arg67.tgt_pos.x = unit->_position.x;
                arg67.tgt_pos.z = unit->_position.z;
            }
            arg67.priority = 0;
        }

        if ( unit->_status != BACT_STATUS_DEAD )
        {
            bact_arg124 arg124;
            arg124.from = unit->_position.XZ();

            if ( arg67.tgt_type == BACT_TGT_TYPE_UNIT )
            {
                arg124.to = arg67.tgt.pbact->_position.XZ();
            }
            else
            {
                arg124.to = arg67.tgt_pos.XZ();
            }

            arg124.field_12 = 1;
            arg124.steps_cnt = 32;

            if ( unit->IsNeedsWaypoints() && unit->PathFinder(&arg124) )
            {
                arg124.steps_cnt = 32;

                unit->SetPath(&arg124);

                if ( arg67.tgt_type == BACT_TGT_TYPE_UNIT )
                {
                    unit->_m_cmdID = arg67.tgt.pbact->_commandID;
                    unit->_m_owner = arg67.tgt.pbact->_owner;
                }
            }
            else
            {
                unit->SetTarget(&arg67);
            }
        }

        _roboState &= ~ROBOSTATE_SETDOCKTARGET;
    }

    unit->setBACT_aggression(_roboDockAggr);
    _roboDockCnt = 0;
    _roboDockUser = 0;
    _roboDockTime = 2000;
    _roboState &= ~ROBOSTATE_USEVHCLPARTS;
}

void NC_STACK_yparobo::InitForce(NC_STACK_ypabact *unit)
{
    if ( _roboDockTime > 0 )
        return;

    if ( _roboDockEnerg <= 0 )
    {
        sub_4A9F24(unit);
        return;
    }

    int v6 = _energy - _energy_max * 0.2;

    if ( _roboState & ROBOSTATE_USEVHCLPARTS )
        v6 += _roboVehicleSpare;

    bact_arg81 arg81;
    arg81.enrg_type = 3;
    arg81.enrg_sum = 0;

    GetSummary(&arg81);

    NC_STACK_ypabact *newUnit = nullptr;

    int unitCost = yparobo_GetUnitProductionCost(unit);
    int v23 = (arg81.enrg_sum * 0.05 + 1.0) * (float)unitCost;

    if ( v6 > (signed int)v23 )
    {
        ypaworld_arg146 arg146;
        arg146.vehicle_id = unit->_vehicleID;
        arg146.pos = _position + _roboDockPos;

        newUnit = yparobo_TryCreateGenesisUnit(this, &arg146);
    }

    if ( !newUnit )
    {
        sub_4A9F24(unit);
        return;
    }

    newUnit->_status = BACT_STATUS_CREATE;
    newUnit->_owner = _owner;
    newUnit->_commandID = unit->_commandID;
    newUnit->_host_station = this;
    newUnit->InheritActiveDebuffFromParent(this);

    int v20 = getBACT_bactCollisions();
    newUnit->setBACT_bactCollisions(v20);

    unit->AddSubject(newUnit);

    setTarget_msg arg67;
    arg67.tgt_type = BACT_TGT_TYPE_CELL;
    arg67.priority = 0;
    arg67.tgt_pos.x = _position.x + _rotation.m20 * World::CVSectorLength * 0.5;
    arg67.tgt_pos.z = _position.z + _rotation.m22 * World::CVSectorLength * 0.5;
    newUnit->SetTarget(&arg67);

    if ( _world->_isNetGame )
    {
        newUnit->_gid |= newUnit->_owner << 24;

        uamessage_newVhcl nvMsg;
        nvMsg.msgID = UAMSG_NEWVHCL;
        nvMsg.owner = newUnit->_owner;
        nvMsg.pos = newUnit->_position;
        nvMsg.type = newUnit->_vehicleID;
        nvMsg.parent = newUnit->_parent->_gid;
        nvMsg.id = newUnit->_gid;
        nvMsg.kind = 2;
        nvMsg.commandID = unit->_commandID;

        _world->NetBroadcastMessage(&nvMsg, sizeof(nvMsg), true);
    }

    if ( _roboState & ROBOSTATE_USEVHCLPARTS )
    {
        if ( v23 > _roboVehicleSpare )
        {
            _energy -= v23 - _roboVehicleSpare;
            _roboVehicleSpare = 0;
        }
        else
        {
            _roboVehicleSpare -= v23;
        }
    }
    else
    {
        _energy -= v23;
    }

    _roboDockEnerg -= unitCost;

    setState_msg arg78;
    arg78.setFlags = 0;
    arg78.unsetFlags = 0;
    arg78.newStatus = BACT_STATUS_CREATE;
    newUnit->SetState(&arg78);

    newUnit->_scale_time = newUnit->_energy_max * 0.2;
    _roboDockTime = newUnit->_scale_time + 2000;
    _roboDockCnt++;

    _world->HistoryAktCreate(newUnit);
    yparobo_AdvanceMimicProductionCost(_world, newUnit->_vehicleID);
}

int NC_STACK_yparobo::sub_4A5A08(NC_STACK_ypabact *bact, float a2, float a3)
{
    int v5 = 0;
    if ( bact->_bact_type == BACT_TYPES_TANK || bact->_bact_type == BACT_TYPES_CAR )
    {
        v5 = 1;
    }
    else
    {
        for ( NC_STACK_ypabact* &node : bact->_kidList )
        {
            if (node->_bact_type == BACT_TYPES_TANK || node->_bact_type == BACT_TYPES_CAR)
            {
                v5 = 1;
                break;
            }
        }
    }

    if ( v5 )
    {
        bact_arg124 arg125;
        arg125.steps_cnt = 32;
        arg125.from = bact->_position.XZ();
        arg125.to = vec2d(a2, a3);
        arg125.field_12 = 1;
        if ( !bact->SetPath(&arg125) )
            return 0;
    }
    else
    {
        setTarget_msg arg67;
        arg67.tgt_pos.x = a2;
        arg67.tgt_pos.z = a3;
        arg67.tgt_type = BACT_TGT_TYPE_CELL;
        arg67.priority = 0;
        bact->SetTarget(&arg67);
    }
    return 1;
}

int NC_STACK_yparobo::sub_4A58C0(NC_STACK_ypabact *bact, NC_STACK_ypabact *bact2)
{
    int v5 = 0;
    if ( bact->_bact_type == BACT_TYPES_TANK || bact->_bact_type == BACT_TYPES_CAR )
    {
        v5 = 1;
    }
    else
    {
        for ( NC_STACK_ypabact* &node : bact->_kidList )
        {
            if (node->_bact_type == BACT_TYPES_TANK || node->_bact_type == BACT_TYPES_CAR)
            {
                v5 = 1;
                break;
            }
        }
    }

    if ( v5 )
    {
        bact_arg124 arg124;
        arg124.steps_cnt = 32;
        arg124.from = bact->_position.XZ();
        arg124.to = bact2->_position.XZ();
        arg124.field_12 = 1;

        if ( !bact->PathFinder(&arg124) )
            return 0;

        if ( arg124.steps_cnt > 1 )
        {
            arg124.steps_cnt = 32;
            bact->SetPath(&arg124);

            bact->_m_cmdID = bact2->_commandID;
            bact->_m_owner = bact2->_owner;
            return 1;
        }
    }

    setTarget_msg arg67;
    arg67.tgt.pbact = bact2;
    arg67.priority = 0;
    arg67.tgt_type = BACT_TGT_TYPE_UNIT;
    bact->SetTarget(&arg67);
    return 1;
}

void NC_STACK_yparobo::sb_0x4a7010__sub1__sub0(NC_STACK_ypabact *unit1, NC_STACK_ypabact *unit2)
{
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            Common::Point pt = unit1->_cellId + Common::Point(i, j);

            if ( _world->IsGamePlaySector(pt) )
            {
                cellArea &pcell = _world->SectorAt(pt);

                if ( pcell.owner == unit1->_owner && pcell.PurposeType == cellArea::PT_POWERSTATION )
                {
                    vec2d xy = World::SectorIDToCenterPos2(pt);
                    sub_4A5A08(unit2, xy.x, xy.y);

                    unit2->setBACT_aggression(25);
                    return;
                }

                if ( pcell.IsCanSee(unit2->_owner) )
                {
                    for( NC_STACK_ypabact* &v10 : pcell.unitsList )
                    {
                        if ( v10->_bact_type == BACT_TYPES_GUN && unit1->_owner == v10->_owner && v10->_status != BACT_STATUS_DEAD )
                        {
                            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( v10 );

                            if ( !gun->IsRoboGun() )
                            {
                                if ( v10->IsParentMyRobo() )
                                    sub_4A58C0(unit2, v10);
                                else if (v10->_parent)
                                    sub_4A58C0(unit2, v10->_parent);

                                unit2->setBACT_aggression(25);
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    if ( unit1->_pSector->IsCanSee(unit2->_owner) )
    {
        sub_4A58C0(unit2, unit1);

        unit2->setBACT_aggression(25);
    }
    else
    {
        vec2d xy = World::SectorIDToCenterPos2( unit1->_cellId );

        sub_4A5A08(unit2, xy.x, xy.y);

        unit2->setBACT_aggression(50);
    }
}

void NC_STACK_yparobo::sb_0x4a7010__sub1(NC_STACK_ypabact *unit, robo_t1 *rbt)
{
    NC_STACK_ypabact *foundBact = nullptr;

    if (unit->_parent->_kidRef)
    {
        for(NC_STACK_ypabact* &nod : *unit->_parent->_kidRef.PList())
        {
            if ( nod->_commandID == rbt->field_4 )
            {
                foundBact = nod;
                break;
            }
        }
    }

    if ( foundBact )
    {
        if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            if ( unit->_primT.pbact->_bact_type == BACT_TYPES_ROBO )
                sb_0x4a7010__sub1__sub0(foundBact, unit);
        }
        else if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            if (unit->_primT.pcell->owner == unit->_owner || foundBact->_pSector == unit->_primT.pcell)
                sb_0x4a7010__sub1__sub0(foundBact, unit);
        }
    }
    else
    {
        rbt->field_4 = 0;
        rbt->field_0 = 0;
    }
}

void NC_STACK_yparobo::doBeamUpdate(int a2)
{
    _roboBeamTimePre -= a2;

    if ( _roboBeamTimePre > 0 )
    {
        if ( _roboBeamFXTime <= 0 )
        {
            int v23 = (1500 - _roboBeamTimePre) / 10;
            int v10 = _roboBeamTimePre / 10;

            if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
            {
                _roboBeamFXTime = v10;
                _vp_extra[0].flags &= ~EVPROTO_FLAG_ACTIVE;
            }
            else
            {
                _roboBeamFXTime = v23;

                _vp_extra[0].pos = _position;
                _vp_extra[0].rotate = _rotation;
                _vp_extra[0].flags = (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);
                _vp_extra[0].scale = 1.0;
                _vp_extra[0].SetVP(_vp_genesis);
            }

            if ( _vp_extra[1].flags & EVPROTO_FLAG_ACTIVE )
            {
                _roboBeamFXTime = v10;
                _vp_extra[1].flags &= ~EVPROTO_FLAG_ACTIVE;
            }
            else
            {
                _roboBeamFXTime = v23;

                _vp_extra[1].pos = _roboBeamPos;
                _vp_extra[1].rotate = _rotation;
                _vp_extra[1].flags = EVPROTO_FLAG_ACTIVE;
                _vp_extra[1].SetVP(_vp_genesis);
            }
        }

        _roboBeamFXTime -= a2;
    }
    else
    {
        _roboBeamTimePre = 0;

        vec3d tmp = _position;

        tmp.y = _roboYPos;

        bact_arg80 v18;
        v18.pos = _roboBeamPos;
        v18.field_C = 0;

        SetPosition(&v18);

        if ( _world->_isNetGame )
        {
            _status_flg |= BACT_STFLAG_DSETTED;

            for (NC_STACK_ypabact* &node : _kidList)
            {
                if ( node->_bact_type == BACT_TYPES_GUN )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( node );
                    if ( gun->IsRoboGun() )
                        node->_status_flg |= BACT_STFLAG_DSETTED;
                }
            }
        }

        _old_pos = tmp;
        _old_pos.y = tmp.y;

        SFXEngine::SFXe.startSound(&_soundcarrier, 10);

        _roboState &= ~ROBOSTATE_MOVE;
        _vp_extra[0].flags = 0;
        _vp_extra[1].flags = 0;

        if ( _world->_isNetGame )
        {
            uamessage_endBeam ebMsg;
            ebMsg.msgID = UAMSG_ENDBEAM;
            ebMsg.owner = _owner;
            ebMsg.id = _gid;

            _world->NetBroadcastMessage(&ebMsg, sizeof(ebMsg), true);
        }
    }
}

void NC_STACK_yparobo::sub_4A1270(const vec3d &vaxis, float angle)
{
    for (World::TRoboGun &gun : _roboGuns)
    {
        if ( gun.gun_obj )
        {
            NC_STACK_ypagun *gungun = dynamic_cast<NC_STACK_ypagun *>(gun.gun_obj);

            if (gungun)
                gun.dir = gungun->ypagun_func129(vaxis, -angle);
        }
    }
}

void NC_STACK_yparobo::sub_4A1014(float angle)
{
    _rotation = mat3x3::RotateY(angle) * _rotation;

    sub_4A1270(_rotation.AxisY(), angle);

    _roboColls.field_0 = angle;
}

void NC_STACK_yparobo::sub_4A10E8(float angle)
{
    _rotation = mat3x3::RotateY(angle) * _rotation;

    sub_4A1270(vec3d::OY(1.0), angle);

    _roboColls.field_0 = angle;
}

void NC_STACK_yparobo::ChangeSectorEnergyFromRoboCollision(yw_arg129 *arg)
{
    yw_130arg sectorInfo;
    sectorInfo.pos_x = arg->pos.x;
    sectorInfo.pos_z = arg->pos.z;

    int buildingsBefore = 0;
    if ( _world->GetSectorInfo(&sectorInfo) )
    {
        for (int health : sectorInfo.pcell->buildings_health)
        {
            if ( health > 0 )
                buildingsBefore++;
        }
    }

    ChangeSectorEnergy(arg);

    if ( buildingsBefore <= 0 || !_world->GetSectorInfo(&sectorInfo) )
        return;

    int buildingsAfter = 0;
    for (int health : sectorInfo.pcell->buildings_health)
    {
        if ( health > 0 )
            buildingsAfter++;
    }

    int destroyedBuildings = buildingsBefore - buildingsAfter;
    if ( destroyedBuildings <= 0 || _energy_max <= 0 || IsInvulnerableToDamage() )
        return;

    World::TAuthoredScalar authoredDamage;
    if ( !World::ParseAuthoredScalar(
             System::IniConf::GameRoboBuildingCollisionEnergy.Get<std::string>(),
             authoredDamage) ||
         !std::isfinite(authoredDamage.value) || authoredDamage.value <= 0.0f )
    {
        return;
    }

    double resolvedRawDamage = authoredDamage.percent
        ? (double)_energy_max * (double)std::min(authoredDamage.value, 100.0f) / 100.0
        : (double)authoredDamage.value;
    if ( !std::isfinite(resolvedRawDamage) || resolvedRawDamage <= 0.0 )
        return;

    resolvedRawDamage = std::min(resolvedRawDamage, (double)std::numeric_limits<int>::max());
    int rawDamage = std::max(1, (int)ceil(resolvedRawDamage));

    const int selfDamage = CalcShieldedCustomDamage(rawDamage);
    if ( selfDamage <= 0 )
        return;

    while ( destroyedBuildings > 0 && _energy > 0 )
    {
        if ( selfDamage >= _energy )
            _energy = 0;
        else
            _energy -= selfDamage;

        destroyedBuildings--;
    }
}

size_t NC_STACK_yparobo::checkCollisions(float a2)
{
    if (CollisionsDisabled()) return 0;
    bool isViewer = getBACT_viewer();
    int v81 = getBACT_inputting();
    int v79 = getBACT_exactCollisions();

    if ( _position == _old_pos )
        return 0;

    bact_arg88 arg88_2;
    arg88_2.pos1 = vec3d(0.0, 0.0, 0.0);

    int v86 = 0;

    ypaworld_arg136 arg136;

    // A valid shape owns physical contacts; retain the height probe below.
    if (!HasCollisionShape())
    for (const World::TRoboColl &rcoll : _roboColls.roboColls)
    {
        if ( !v81 || _primTtype )
        {
            vec3d tmp = rcoll.field_10;
            vec3d t = _position + _rotation.Transform( rcoll.coll_pos );

            arg136.stPos = tmp;
            arg136.flags = 0;

            float v82 = (tmp.XZ() - t.XZ()).length();

            if ( v82 <= 1.0 )
                arg136.vect = _fly_dir * 300.0;
            else
                arg136.vect = vec3d::X0Z( (t.XZ() - tmp.XZ()) * 300.0 / v82 );

            TraceWorldCollision(&arg136);


            if ( arg136.isect )
            {
                if ( arg136.tVal * 300.0 >= rcoll.robo_coll_radius + v82)
                {
                    _status_flg &= ~BACT_STFLAG_LCRASH;
                    _status_flg |= BACT_STFLAG_UPWRD;
                    return 0;
                }
                else if ( rcoll.robo_coll_radius > 0.01 )
                {
                    bact_arg88 arg88;
                    arg88.pos1 = arg136.skel->polygons[ arg136.polyID ].Normal();

                    Recoil(&arg88);

                    if ( fabs(_roboColls.field_0) > 0.0 )
                    {
                        sub_4A1014(-_roboColls.field_0);

                        _roboColls.field_0 = 0;
                    }

                    yw_130arg arg130;
                    arg130.pos_x = arg136.isectPos.x;
                    arg130.pos_z = arg136.isectPos.z;

                    _world->GetSectorInfo(&arg130);

                    if ( v81 || arg130.pcell->PurposeType == cellArea::PT_NONE )
                    {
                        yw_arg129 v60;
                        v60.pos = arg136.isectPos;
                        v60.unit = nullptr;
                        v60.OwnerID = World::OWNER_RECALC;
                        v60.field_10 = v82 * 15000.0 / a2;

                        ChangeSectorEnergyFromRoboCollision(&v60);
                    }

                    _fly_dir_length *= 0.4;
                    _status_flg |= BACT_STFLAG_UPWRD;

                    return 1;
                }
            }
        }


        int v26 = _world->ypaworld_func145(this);

        if ( (isViewer || (v26 && v79)) && rcoll.robo_coll_radius > 0.01 )
        {
            vec3d tmp = rcoll.field_10;
            vec3d t = _position + _rotation.Transform( rcoll.coll_pos );

            ypaworld_arg137 arg137;
            arg137.pos = tmp;

            float v83 = (tmp.XZ() - t.XZ()).length();

            if ( v83 <= 1.0 )
                arg137.pos2 = _fly_dir;
            else
            {
                arg137.pos2 = vec3d::X0Z( (t.XZ() - tmp.XZ()) ) / v83;

                // TYPO??
                /*arg137.pos2.x = 1.0 / v83 * (t.x - tmp.x);
                arg137.pos2.y = 0;
                arg137.pos2.z = 1.0 / v83 * (t.x - tmp.x); */
            }

            yw_137col v57[32];

            arg137.radius = rcoll.robo_coll_radius;
            arg137.collisions = v57;
            arg137.field_30 = 0;
            arg137.coll_max = 32;

            QueryWorldCollisions(&arg137);

            if ( arg137.coll_count )
            {
                v86++;

                float v89 = v83 * 15000.0;

                vec3d ttmp(0.0, 0.0, 0.0);

                for (int j = arg137.coll_count - 1; j >= 0; j--)
                {
                    yw_137col *clzn = &arg137.collisions[j];

                    ttmp += clzn->pos2;

                    yw_130arg arg130;
                    arg130.pos_x = clzn->pos1.x;
                    arg130.pos_z = clzn->pos1.z;

                    _world->GetSectorInfo(&arg130);

                    if ( v81 || arg130.pcell->PurposeType == cellArea::PT_NONE )
                    {
                        yw_arg129 v60;
                        v60.pos = clzn->pos1;
                        v60.unit = nullptr;
                        v60.OwnerID = World::OWNER_RECALC;
                        v60.field_10 = v89 / a2;

                        ChangeSectorEnergyFromRoboCollision(&v60);
                    }
                }

                float v84 = ttmp.length();

                if ( v84 != 0.0 )
                    arg88_2.pos1 += ttmp / v84;
                else
                    arg88_2.pos1 += _fly_dir;
            }

            if ( v86 )
            {
                arg88_2.pos1 /= (float)v86;

                _position = _old_pos;


                Recoil(&arg88_2);

                if ( fabs(_roboColls.field_0) > 0.0 )
                {
                    sub_4A1014(-_roboColls.field_0);
                    _roboColls.field_0 = 0;
                }

                _fly_dir_length *= 0.4;
                _status_flg |= BACT_STFLAG_UPWRD;

                return 1;
            }
        }
    }

    _status_flg &= ~BACT_STFLAG_LCRASH;

    arg136.stPos = _position;
    double hoverHeight = _height;
    double bottom = 0.0;
    if (HasCollisionShape())
        bottom = GetBodyPosition().y - _position.y +
            Collision::DownExtent(*_collisionShape, _rotation) + _collisionShape->tolerance;

    if (_roboWFlags & 1)
    {
        if (!HasCollisionShape())
            for (const World::TRoboColl &sphere : _roboColls.roboColls)
                bottom = std::max(bottom,
                    _rotation.Transform(sphere.coll_pos).y + sphere.robo_coll_radius);

        // The legacy height probe starts rising before any mounted gun touches
        // the floor, rather than waiting for the final collision correction.
        for (const World::TRoboGun &mount : _roboGuns)
        {
            NC_STACK_ypabact *gun = mount.gun_obj;
            if (!gun || gun->IsDestroyed()) continue;
            const double gunDown = gun->HasCollisionShape()
                ? Collision::DownExtent(*gun->_collisionShape, gun->_rotation) +
                    gun->_collisionShape->tolerance
                : std::max(double(gun->_overeof), double(gun->GetCollisionBroadRadius()));
            bottom = std::max(bottom,
                _rotation.Transpose().Transform(mount.pos).y + gunDown);
        }
    }
    // The native 0.66 trigger on a 1.5-height ray must precede body contact.
    if (bottom > 0.0)
        hoverHeight = std::max(hoverHeight, bottom / (0.66 * 1.5));
    arg136.vect = vec3d::OY( hoverHeight * 1.5 ) + _fly_dir.X0Z() * 100.0;
    arg136.flags = 0;

    TraceWorldCollision(&arg136);

    if ( arg136.isect )
    {
        if ( arg136.tVal < 0.66 )
        {
            _target_dir.y = -1.0;
            _status_flg |= BACT_STFLAG_UPWRD;
        }
    }
    else
    {
        _target_dir.y = 1.0;
        _status_flg &= ~BACT_STFLAG_UPWRD;
    }

    _roboColls.field_0 = 0;

    return 0;
}

void NC_STACK_yparobo::wallow(update_msg *arg)
{
    _position.y = sin(arg->gTime * C_PI / 3000.0) * 25.0 + _roboYPos;
    _shapeWallowPending = HasCollisionShape();
    _shapeWallowRequestedY = _position.y;

    PositionRoboGuns();}

void NC_STACK_yparobo::yparobo_func70__sub2__sub0()
{
    for (World::TRoboColl &rcoll : _roboColls.roboColls)
        rcoll.field_10 = _position + _rotation.Transform( rcoll.coll_pos );
}

bool NC_STACK_yparobo::yparobo_func70__sub2__sub1()
{
    NC_STACK_ypabact *yw_1b7c = _world->getYW_userVehicle();

    if ( yw_1b7c == this )
        return ShouldUsePlayerRoboIdleMotion();

    for (World::TRoboGun &gun : _roboGuns)
        if (gun.gun_obj && gun.gun_obj == yw_1b7c)
            return ShouldUsePlayerRoboIdleMotion();

    return true;
}

void NC_STACK_yparobo::sub_4A4538(NC_STACK_ypabact *bact)
{
    setTarget_msg arg67;
    arg67.tgt_type = BACT_TGT_TYPE_NONE;
    arg67.priority = 1;

    bact->SetTarget(&arg67);

    for( NC_STACK_ypabact* &node : bact->_kidList )
        node->SetTarget(&arg67);
}

void NC_STACK_yparobo::sub_4A448C(NC_STACK_ypabact *bact)
{
    for( NC_STACK_ypabact* &node : bact->_kidList )
    {
        node->_waypoints_count = bact->_waypoints_count;

        if ( node->_waypoints_count == 1 )
            node->_current_waypoint = 0;

        node->_status_flg |= BACT_STFLAG_WAYPOINT;

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            node->_status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            node->_status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        for (int i = 0; i < 32; i++)
            node->_waypoints[i] = bact->_waypoints[i];
    }
}

int NC_STACK_yparobo::sb_0x4a45cc__sub0(NC_STACK_ypabact *bact)
{
    vec3d tmp;
    if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
    {
        tmp = bact->_primT.pbact->_position;
    }
    else if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
    {
        tmp = bact->_primTpos;
    }
    else
        return 1;

    vec3d tmp2 = bact->_position - tmp;

    float v19 = tmp2.length();

    for( NC_STACK_ypabact* &node : bact->_kidList )
    {
        vec3d tmp3 = node->_position - tmp;

        if ( tmp3.length() < v19 )
            return 0;
    }
    return 1;
}

bool NC_STACK_yparobo::IsPlayerDirectlyControllingThisRobo() const
{
    if ( !IsPlayerRobo() || !_world || this != _world->getYW_userHostStation() )
        return false;

    NC_STACK_ypabact *userVehicle = _world->getYW_userVehicle();

    if ( userVehicle == this )
        return true;

    for (const World::TRoboGun &gun : _roboGuns)
    {
        if ( gun.gun_obj && gun.gun_obj == userVehicle )
            return true;
    }

    return false;
}

void NC_STACK_yparobo::PositionRoboGuns()
{
    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
        {
            bact_arg80 arg80;
            arg80.pos = _position + _rotation.Transpose().Transform( gun.pos );
            arg80.field_C = 4;

            gun.gun_obj->SetPosition(&arg80);
        }
    }
}

void NC_STACK_yparobo::BeforeSoundCarrierUpdate()
{
    if ( !ShouldUsePlayerMobileMove() )
        return;

    float speedPitchScale = 0.0f;

    // The normal Robo Move() path computes pitch from current speed, but generic
    // damaged/debuff sound processing can reset SND_NORMAL immediately before
    // the sound carrier is submitted to the audio engine. Apply the player
    // mobile pitch here as the final per-frame audio step, after those resets
    // and before UpdateSoundCarrier().
    if ( _force > 0.0f && _airconst_static > 0.0f )
        speedPitchScale = fabs(_fly_dir_length) / (_force / _airconst_static) * 1.4f;

    // If the Host Station is physically travelling but its current speed is too
    // low to produce an audible raw legacy pitch delta, keep a small movement
    // floor. This only runs for the player mobile Host Station, not for AI HS.
    if ( speedPitchScale <= 0.0f && _primTtype == BACT_TGT_TYPE_CELL )
        speedPitchScale = 0.08f;

    ApplyPlayerMobileMovePitch(speedPitchScale);
}

void NC_STACK_yparobo::yparobo_func71__sub0(update_msg *arg)
{
    TInputState *inpt = arg->inpt;
    float v18 = arg->frameTime / 1000.0;


    if ( inpt->Sliders[1] < -0.001 )
    {
        _viewer_vert_angle += inpt->Sliders[1] * _maxrot * v18 * 2.0;

        if ( _viewer_vert_angle > _viewer_max_up )
            _viewer_vert_angle = _viewer_max_up;

        if ( _viewer_vert_angle < -_viewer_max_down )
            _viewer_vert_angle = -_viewer_max_down;
    }
    else if (inpt->Sliders[1] > 0.001)
    {
        _viewer_vert_angle += inpt->Sliders[1] * _maxrot * v18 * 2.0;

        if ( _viewer_vert_angle > _viewer_max_up )
            _viewer_vert_angle = _viewer_max_up;

        if ( _viewer_vert_angle < -_viewer_max_down )
            _viewer_vert_angle = -_viewer_max_down;
    }

    if ( inpt->Sliders[0] < -0.001 )
    {
        _viewer_horiz_angle -= inpt->Sliders[0] * _maxrot * v18 * 2.0;

        if ( _viewer_max_side < 3.15 )
        {
            if ( _viewer_horiz_angle > _viewer_max_side )
                _viewer_horiz_angle = _viewer_max_side;

            if ( _viewer_horiz_angle < -_viewer_max_side )
                _viewer_horiz_angle = -_viewer_max_side;
        }
    }
    else if (inpt->Sliders[0] > 0.001)
    {
        _viewer_horiz_angle -= inpt->Sliders[0] * _maxrot * v18 * 2.0;

        if ( _viewer_max_side < 3.15 )
        {
            if ( _viewer_horiz_angle > _viewer_max_side )
                _viewer_horiz_angle = _viewer_max_side;

            if ( _viewer_horiz_angle < -_viewer_max_side )
                _viewer_horiz_angle = -_viewer_max_side;
        }
    }
}

void NC_STACK_yparobo::User_layer(update_msg *arg)
{
    UpdateHandBrakeInput(arg->inpt->HandBrakePressed);

    int a4 = getBACT_bactCollisions();

    float v36 = _target_vec.length();

    if ( v36 <= 0.0 )
        _target_dir = vec3d(0.0, 0.0, 0.0);
    else
        _target_dir = _target_vec / v36;

    if ( _status == BACT_STATUS_NORMAL )
    {
        if ( _roboState & ROBOSTATE_MOVE )
            doBeamUpdate(arg->frameTime);

        if ( !a4 || !CollisionWithBact(arg->frameTime) )
        {
            checkCollisions(arg->frameTime * 0.001);

            checkCommander();
            checkDanger();
            searchEnemyRobo();
            usersRoboEnergyCheck();

            if ( arg->inpt->Buttons.Is(3) )
                HandBrake(arg);

            yparobo_func71__sub0(arg);
            if ( !UpdatePlayerMobileMove(arg) && !UpdatePlayerRoboIdleMotion(arg) )
                wallow(arg);

            _viewer_rotation = _rotation;

            _viewer_rotation = mat3x3::RotateY(_viewer_horiz_angle) * _viewer_rotation;
            _viewer_rotation = mat3x3::RotateX(_viewer_vert_angle) * _viewer_rotation;
        }
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
    else if ( _status == BACT_STATUS_IDLE )
    {
        setState_msg arg78;
        arg78.newStatus = BACT_STATUS_NORMAL;
        arg78.setFlags = 0;
        arg78.unsetFlags = 0;

        SetState(&arg78);

        _status_flg &= ~BACT_STFLAG_LAND;
    }
}

void NC_STACK_yparobo::Move(move_msg *arg)
{
    _old_pos = _position;
    const mat3x3 shapeOldRotation = _rotation;

    vec3d v63;
    if ( _status == BACT_STATUS_DEAD )
        v63 = vec3d::OY(_mass * 39.2266);
    else
        v63 = vec3d::OY(_mass * 9.80665);


    vec3d v52(0.0, 0.0, 0.0);
    if ( !(arg->flag & 1) )
        v52 = vec3d::OY(-_roboFlotage);

    vec3d v51(0.0, 0.0, 0.0);
    if ( !(arg->flag & 1) )
        v51 = arg->vec * _thraction;

    vec3d v57 = v63 + v51 - _fly_dir * (_fly_dir_length * _airconst) + v52;

    if ( v57.length() > 0.0 )
    {
        vec3d v67 = _fly_dir * _fly_dir_length + v57 * (arg->field_0 / _mass);

        _fly_dir_length = v67.normalise();
        _fly_dir = v67;
    }

    if ( fabs(_fly_dir_length) > 0.1 )
        _position += _fly_dir * (_fly_dir_length * arg->field_0 * 6.0);

    CorrectPositionInLevelBox(nullptr);

    ResolveShapeMovement(_old_pos, shapeOldRotation);

    PositionRoboGuns();
    _soundcarrier.Sounds[0].Pitch = _soundcarrier.Sounds[0].PitchBase;
    _soundcarrier.Sounds[0].Volume = _volume;

    float v60 = fabs(_fly_dir_length) / (_force / _airconst_static) * 1.4;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch = (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * v60;

    if ( ShouldUsePlayerMobileMove() )
        ApplyPlayerMobileMovePitch(v60);
}

void NC_STACK_yparobo::HandleShapeWorldCollision(const Collision::Contact &contact)
{
    NC_STACK_ypabact::HandleShapeWorldCollision(contact);
    _fly_dir_length *= 0.4;
    _status_flg |= BACT_STFLAG_UPWRD;

    // Native Robo impact damage is horizontal travel per second * 15000.
    // Move converts velocity to travel with a factor of six.
    const double damage = contact.incomingVelocity.XZ().length() * 6.0 * 15000.0;
    yw_130arg sector;
    sector.pos_x = contact.point.x;
    sector.pos_z = contact.point.z;
    if (damage > 0 && _world->GetSectorInfo(&sector) &&
        (getBACT_inputting() || sector.pcell->PurposeType == cellArea::PT_NONE))
    {
        yw_arg129 impact;
        impact.pos = contact.point;
        impact.unit = nullptr;
        impact.OwnerID = World::OWNER_RECALC;
        impact.field_10 = damage;
        ChangeSectorEnergyFromRoboCollision(&impact);
    }
}

void NC_STACK_yparobo::UpdateUnitGuns(update_msg *arg)
{
    NC_STACK_ypabact::UpdateUnitGuns(arg);

    // The final collision pass may change the Host pose after Move or wallow.
    // Keep its mounted guns above the terrain in that final pose as well.
    if (_shapeWallowPending && HasCollisionShape() && !IsDestroyed())
        _roboYPos += _position.y - _shapeWallowRequestedY;

    bool lifted = false;
    if (_status == BACT_STATUS_NORMAL && (_roboWFlags & 1) &&
        _world && _world->_collisionScene)
    {
        const double penetration = _world->_collisionScene->RoboGroundPenetration(this);
        if (penetration > 0.0)
        {
            _position.y -= penetration;
            lifted = true;
            if (_shapeWallowPending)
            {
                _roboYPos -= penetration;
            }
            else
            {
                // Stop the downward drift and let the existing flux controller rise.
                vec3d velocity = _fly_dir * _fly_dir_length;
                if (velocity.y >= 0.0)
                {
                    velocity.y = -std::max(0.5, std::min(8.0, velocity.y * 0.35));
                    _fly_dir_length = velocity.normalise();
                    _fly_dir = velocity;
                }
                _status_flg |= BACT_STFLAG_UPWRD;
            }
            _world->_collisionScene->UpdateActor(this);
        }
    }
    if ((HasCollisionShape() && !IsDestroyed()) || lifted)
        PositionRoboGuns();
    _shapeWallowPending = false;
}

void NC_STACK_yparobo::Die()
{
    CrashDiag::Breadcrumb("HOST_DEATH",
                          "enter ptr=%p gid=%d owner=%d status=%d flags=0x%x energy=%d killer_gid=%d killer_owner=%d commanders=%zu guns=%zu deinit=%d",
                          this, _gid, _owner, _status, _status_flg, _energy,
                          _killer ? _killer->_gid : 0, _killer_owner,
                          _kidList.size(), _roboGuns.size(), _deinitInProgress ? 1 : 0);

    // DeleteLevel already destroys every hierarchy node. Calling the normal
    // Host death cascade from NC_STACK_ypabact::Deinit() walked squad lists
    // while child cleanup was mutating them, leaving a stale subnode pointer
    // (WER crash RVA 0x232c16). Gameplay deaths still use the full path below.
    if ( _deinitInProgress )
    {
        CrashDiag::Breadcrumb("HOST_DEATH", "deinit shortcut gid=%d", _gid);
        _status = BACT_STATUS_DEAD;
        _status_flg |= BACT_STFLAG_DEATH1;
        return;
    }

    int a4 = getBACT_yourLastSeconds();

    uamessage_hostDie hdMsg;

    if ( !(_status_flg & BACT_STFLAG_DEATH1) )
    {
        CrashDiag::RequestCheckpoint("host_station_death_begin");

        // Only a real gameplay Host Station death may request the global
        // slowdown. The world applies the configured scale, duration and
        // maximum-distance gates, independently of the damage source.
        if ( _world )
            _world->StartRoboDeathTimeScale(this);

        hdMsg.msgID = UAMSG_HOSTDIE;
        hdMsg.owner = _owner;

        if (_killer)
            hdMsg.killer = _killer->_gid;
        else
            hdMsg.killer = 0;

        hdMsg.killerOwner = _killer_owner;

        if ( _killer_owner )
        {
            if ( !(_status_flg & BACT_STFLAG_CLEAN) )
            {
                yw_arg177 arg177;
                arg177.field_4 = _killer_owner;
                arg177.bact = this;

                _world->ypaworld_func177(&arg177); //Reown sectors for new owner
            }
        }

        // Snapshot the complete descendant hierarchy before the first Die().
        // A Host Station normally owns commanders and their squad members, but
        // regrouping and carrier-like units may create deeper levels.  Killing
        // only two levels could therefore leave a live descendant attached to
        // a dead hierarchy: it remained visible, but AI target filters ignored
        // it because its Host Station was already dead.  Build a stable
        // post-order snapshot so every descendant dies before its parent while
        // the live RefLists are still untouched.
        struct RoboDeathCascadeFrame
        {
            NC_STACK_ypabact *unit;
            bool expanded;
        };

        std::vector<NC_STACK_ypabact *> deathCascade;
        std::unordered_set<NC_STACK_ypabact *> visited;
        std::vector<RoboDeathCascadeFrame> pending;

        auto collectPendingPostOrder = [&]()
        {
            while ( !pending.empty() )
            {
                const RoboDeathCascadeFrame frame = pending.back();
                pending.pop_back();

                NC_STACK_ypabact *unit = frame.unit;
                if ( !unit || unit == this )
                    continue;

                if ( frame.expanded )
                {
                    deathCascade.push_back(unit);
                    continue;
                }

                if ( !visited.insert(unit).second )
                    continue;

                pending.push_back({unit, true});
                const std::vector<NC_STACK_ypabact *> children = unit->_kidList.safe_iter();
                for ( auto it = children.rbegin(); it != children.rend(); ++it )
                    pending.push_back({*it, false});
            }
        };

        const std::vector<NC_STACK_ypabact *> roots = _kidList.safe_iter();
        for ( auto it = roots.rbegin(); it != roots.rend(); ++it )
            pending.push_back({*it, false});
        collectPendingPostOrder();

        // A unit can remain registered in a world/cell list with this Host
        // Station in _host_station even after a legacy reparenting path has
        // detached it from the Robo's _kidList. Such a unit is still visually
        // alive, but faction/AI logic treats its dead Host Station as defeated
        // and may stop acquiring it. Recover those host-bound roots here and
        // feed them through the same post-order cascade; missiles are excluded
        // so already-fired projectiles preserve their vanilla lifetime.
        size_t recoveredHostBoundRoots = 0;
        std::unordered_set<NC_STACK_ypabact *> recoveredQueued;
        auto queueHostBoundRoot = [&](NC_STACK_ypabact *unit)
        {
            if ( !unit || unit == this || unit->_host_station != this )
                return;

            const bool gameplayVehicle =
                unit->_bact_type == BACT_TYPES_BACT
                || unit->_bact_type == BACT_TYPES_TANK
                || unit->_bact_type == BACT_TYPES_ZEPP
                || unit->_bact_type == BACT_TYPES_FLYER
                || unit->_bact_type == BACT_TYPES_UFO
                || unit->_bact_type == BACT_TYPES_CAR
                || unit->_bact_type == BACT_TYPES_GUN;

            if ( !gameplayVehicle || unit->_status == BACT_STATUS_DEAD
                    || (unit->_status_flg & (BACT_STFLAG_DEATH1
                                           | BACT_STFLAG_DEATH2
                                           | BACT_STFLAG_CLEAN))
                    || visited.find(unit) != visited.end()
                    || !recoveredQueued.insert(unit).second )
                return;

            pending.push_back({unit, false});
            recoveredHostBoundRoots++;
        };

        if ( _world )
        {
            for ( NC_STACK_ypabact *unit : _world->_unitsList.safe_iter() )
                queueHostBoundRoot(unit);

            for ( cellArea &cell : _world->_cells )
            {
                for ( NC_STACK_ypabact *unit : cell.unitsList.safe_iter() )
                    queueHostBoundRoot(unit);
            }
        }

        collectPendingPostOrder();

        for ( NC_STACK_ypabact *unit : deathCascade )
        {
            unit->_killer = _killer;

            if ( _status_flg & BACT_STFLAG_CLEAN )
                unit->_status_flg |= BACT_STFLAG_CLEAN;

            unit->Die();

            // The cascade reuses the genesis VP before the normal dead-time
            // phase. Landed vehicles may already have entered DEATH2 in Die();
            // clear that phase together with LAND so DeadTimeUpdate can enter
            // DEATH2 once, stop the godown loop and replace the genesis VP.
            unit->_status_flg &= ~(BACT_STFLAG_LAND | BACT_STFLAG_DEATH2);

            setState_msg arg119;
            arg119.unsetFlags = 0;
            arg119.setFlags = 0;
            arg119.newStatus = BACT_STATUS_CREATE;
            unit->SetStateInternal(&arg119);

            unit->_status = BACT_STATUS_DEAD;

            SFXEngine::SFXe.sub_424000(&unit->_soundcarrier, 3);
            SFXEngine::SFXe.startSound(&unit->_soundcarrier, 7);

            unit->_soundFlags &= 0xFFFFFFF7;
            unit->_soundFlags |= 0x80;

            unit->setBACT_yourLastSeconds(a4);
        }

        CrashDiag::Breadcrumb("HOST_DEATH",
                              "descendant cascade count=%zu recovered_roots=%zu gid=%d",
                              deathCascade.size(), recoveredHostBoundRoots, _gid);

        NC_STACK_ypabact *v20 = _world->getYW_userHostStation();

        if ( this == v20 )
        {
            if ( !(_status_flg & BACT_STFLAG_CLEAN) )
            {
                robo_arg134 arg134;
                arg134.field_4 = 12;
                arg134.field_8 = 0;
                arg134.field_10 = 0;
                arg134.field_C = 0;
                arg134.unit = this;
                arg134.field_14 = 100;

                placeMessage(&arg134);
            }
        }
        else
        {
            robo_arg134 arg134;
            arg134.field_4 = 11;
            arg134.field_8 = _owner;
            arg134.field_10 = 0;
            arg134.field_C = 0;
            arg134.unit = _killer;
            arg134.field_14 = 50;

            placeMessage(&arg134);
        }

        CrashDiag::Breadcrumb("HOST_DEATH", "child cascade complete gid=%d", _gid);
        NC_STACK_ypabact::Die();
        CrashDiag::Breadcrumb("HOST_DEATH", "base death complete gid=%d flags=0x%x", _gid, _status_flg);

        _vp_extra[1].flags = 0;

        if ( _world->_isNetGame )
        {
            if ( _owner )
            {
                _world->NetBroadcastMessage(&hdMsg, sizeof(hdMsg), true);

                if ( this == _world->_userRobo )
                {
                    _world->_userUnit->setBACT_inputting(false);
                    _world->_userUnit->setBACT_viewer(false);

                    setBACT_inputting(true);
                    setBACT_viewer(true);
                }
            }
        }

        CrashDiag::Breadcrumb("HOST_DEATH", "exit gid=%d status=%d flags=0x%x", _gid, _status, _status_flg);
        CrashDiag::RequestCheckpoint("host_station_death_end");
    }
}

size_t NC_STACK_yparobo::SetPosition(bact_arg80 *arg)
{
    int v5 = NC_STACK_ypabact::SetPosition(arg);
    if ( !v5 )
        return 0;

    _roboFlotage = _mass * 9.80665;

    for (World::TRoboGun &gun : _roboGuns)
    {
        if (gun.gun_obj)
        {
            bact_arg80 v11;
            v11.pos = _position + _rotation.Transpose().Transform(gun.pos);
            v11.field_C = 4;

            gun.gun_obj->SetPosition(&v11);
            if (HasCollisionShape() && gun.gun_obj->HasCollisionShape() && _world->_collisionScene)
                _world->_collisionScene->ResetActorPose(gun.gun_obj);
        }
    }

    _roboYPos = _position.y;
    _shapeWallowPending = false;
    if (HasCollisionShape() && _world->_collisionScene)
        _world->_collisionScene->ResetActorPose(this);

    return 1;
}

// Update robo energys
void NC_STACK_yparobo::EnergyInteract(update_msg *arg)
{
    float v65 = _reload_const;
    float v63 = (_clock - _energy_time) / 1000.0;

    if ( _status != BACT_STATUS_DEAD && v63 >= 0.25 )
    {
        int v62 = _energy;

        int v61 = _roboEnergyLife;
        int v59 = _roboEnergyMove;

        _energy_time = _clock;

        yw_arg176 arg176;
        arg176.owner = _pSector->owner;

        _world->ypaworld_func176(&arg176);

        float v64 = _pSector->energy_power;
        const float powerStationEnergyMultiplier = ReadPowerStationEnergyMultiplier();

        float v70 = powerStationEnergyMultiplier * v65 * v63 * v64 * arg176.field_4;

        if ( IsPlayerRobo() )
        {
            int v67 = 0;

            float v68 = v70 / 6000.0;

            if ( _pSector->owner )
            {
                if ( _pSector->owner != _owner )
                    v68 = -v68;
            }
            else
            {
                v68 = 0.0;
            }

            uint8_t playerFillMode = _roboFillMode;

            // Player Host Station physical relocation charges the same resource
            // as vanilla teleport: _roboEnergyMove / beam energy.  The vanilla
            // player resource logic below can slowly rebalance selected resource
            // bars toward their average according to _roboFillMode.  During a
            // long physical relocation that rebalancing was refilling the beam
            // bar from the other two bars, so the final beam cost no longer
            // matched the instant vanilla teleport cost.
            //
            // While the explicit player mobile move is active, keep normal
            // main/build resource behaviour, but do not let the fill-mode
            // balancer regenerate or average the beam/move battery.  The mobile
            // move drain itself is applied separately in UpdatePlayerMobileMoveEnergy().
            if ( _playerRoboAIBehaviorMoveActive && IsPlayerRobo() && _world && this == _world->getYW_userHostStation() )
                playerFillMode &= ~8;

            if ( playerFillMode & 1 || v68 < 0.0 )
                v67++;

            if ( playerFillMode & 4 )
                v67++;

            if ( playerFillMode & 8 )
                v67++;

            _roboEnergyReloadPS = powerStationEnergyMultiplier * v65 * v64 / 6000.0;

            if ( v67 > 0 )
            {
                int v25 = 0;
                int v26 = 0;
                int v66 = 0;

                v68 = v68 / (float)v67;

                if ( playerFillMode & 1 || v68 < 0.0 )
                    _energy += v68;

                if ( playerFillMode & 4 )
                    _roboEnergyLife += v68;

                if ( playerFillMode & 8 )
                    _roboEnergyMove += v68;

                float v60 = v63 * v65 / 30.0;

                if ( playerFillMode & 1 )
                    v25 = _energy;

                if ( playerFillMode & 4 )
                    v25 += _roboEnergyLife;

                if ( playerFillMode & 8 )
                    v25 += _roboEnergyMove;

                int v34 = v25 / v67;

                if ( playerFillMode & 1 )
                {
                    if ( v34 > _energy )
                        v26++;
                    else if ( v34 < _energy )
                        v66++;
                }

                if ( playerFillMode & 4 )
                {
                    if ( v34 > _roboEnergyLife )
                        v26++;
                    else if ( v34 < _roboEnergyLife )
                        v66++;
                }

                if ( playerFillMode & 8 )
                {
                    if ( v34 > _roboEnergyMove )
                        v26++;
                    else if ( v34 < _roboEnergyMove )
                        v66++;
                }

                if ( v26 )
                    v26 = (int)v60 / v26;

                if ( v66 )
                    v66 = (int)v60 / v66;

                if ( playerFillMode & 1 )
                {
                    if ( _energy <= v34 )
                    {
                        _energy += v26;

                        if ( _energy >= v34 )
                            _energy = v34;
                    }
                    else
                    {
                        _energy -= v66;
                        if ( _energy <= v34 )
                            _energy = v34;
                    }
                }

                if ( playerFillMode & 4 )
                {
                    if ( _roboEnergyLife <= v34 )
                    {
                        _roboEnergyLife += v26;
                        if ( _roboEnergyLife >= v34)
                            _roboEnergyLife = v34;
                    }
                    else
                    {
                        _roboEnergyLife -= v66;
                        if ( _roboEnergyLife <= v34 )
                            _roboEnergyLife = v34;
                    }
                }

                if ( playerFillMode & 8 )
                {
                    if ( v34 >= _roboEnergyMove )
                    {
                        _roboEnergyMove += v26;
                        if ( _roboEnergyMove >= v34)
                            _roboEnergyMove = v34;
                    }
                    else
                    {
                        _roboEnergyMove -= v66;
                        if ( _roboEnergyMove <= v34 )
                            _roboEnergyMove = v34;
                    }
                }

            }
        }
        else
        {
            _roboEnergyReloadPS = powerStationEnergyMultiplier * v65 * v64 / 7000.0;
            float v71 = v70 / 7000.0;
            if ( _owner == _pSector->owner )
            {
                _roboBuildSpare += v71 * 0.15;
                _roboVehicleSpare += v71 * 0.7;
                _energy += v71 * 0.7;
            }
            else
            {
                if ( !_world || !_world->IsDebugGlobalInvulnerabilityEnabled() )
                    _energy -= v71;
            }
        }

        if ( _energy < 0 )
            _energy = 0;

        if ( _roboEnergyLife < 0 )
            _roboEnergyLife = 0;

        if ( _roboEnergyMove < 0 )
            _roboEnergyMove = 0;

        if ( _roboBuildSpare < 0 )
            _roboBuildSpare = 0;

        if ( _roboVehicleSpare < 0 )
            _roboVehicleSpare = 0;

        if ( _energy_max < _energy )
            _energy = _energy_max;

        if ( _energy_max < _roboEnergyLife )
            _roboEnergyLife = _energy_max;

        if ( _energy_max < _roboEnergyMove )
            _roboEnergyMove = _energy_max;

        if ( _energy_max < _roboBuildSpare )
            _roboBuildSpare = _energy_max;

        if ( _energy_max < _roboVehicleSpare )
            _roboVehicleSpare = _energy_max;

        _roboEnergyLossFlags = 0;
        _roboEnergyLoadFlags = 0;

        if ( v62 < _energy )
            _roboEnergyLoadFlags |= 1;
        else if ( v62 > _energy )
            _roboEnergyLossFlags |= 1;

        if ( v61 < _roboEnergyLife )
            _roboEnergyLoadFlags |= 4;
        else if ( v61 > _roboEnergyLife )
            _roboEnergyLossFlags |= 4;

        if ( v59 < _roboEnergyMove )
            _roboEnergyLoadFlags |= 8;
        else if ( v59 > _roboEnergyMove )
            _roboEnergyLossFlags |= 8;

        UpdatePlayerRoboResourceTrend((int)(v63 * 1000.0));
    }
}

void NC_STACK_yparobo::Renew()
{
    NC_STACK_ypabact::Renew();
    _shapeWallowPending = false;

    _roboRadarValue = 0;
    _roboEnemyValue = 0;
    _roboPowerValue = 0;
    _roboSafetyValue = 0;
    _roboConqValue = 0;
    _roboPositionValue = 0;
    _roboDangerValue = 0;
    _roboExploreValue = 0;
    _roboPowerCellIndex = 0;
    _roboRadarCellIndex = 0;
    _roboSafetyCellIndex = 0;
    _roboConqCellIndex = 0;
    _roboRadarDelay = 0;
    _roboEnemyDelay = 0;
    _roboPowerDelay = 0;
    _roboSafetyDelay = 0;
    _roboConqDelay = 0;
    _roboDangerDelay = 0;
    _roboExploreDelay = 0;
    _roboPositionDelay = 0;
    _roboBuildSpare = 0;

    _roboDockEnerg = 0;
    _roboDockCnt = 0;
    _roboDockTime = 0;
    _roboDockTargetType = BACT_TGT_TYPE_NONE;
    _roboDockAggr = 0;
    _roboDockTargetCommandID = 0;
    _roboDockTargetPos.x = 0;
    _roboDockTargetPos.y = 0;
    _roboDockTargetPos.z = 0;
    _roboDockTargetBact = nullptr;
    _roboTestEnemyTime = 0;
    _roboBeamTimePre = 0;
    _playerRoboAIBehavior = false;
    ResetPlayerMobileMove();
    ResetPlayerRoboResourceTrend();
    _roboAttackersClearTime = 0;
    _roboAttackersTime = 0;
    _roboState = 0;

    _roboDockTime = 0;

    _roboGuns.clear();

    _roboAttackers.fill( robo_t1() );

    _commandID = dword_5B1128;

    dword_5B1128++;

    setBACT_yourLastSeconds(3000);

    System::IniConf::ReadFromNucleusIni();

    _roboNewAI = System::IniConf::GameNewAI.Get<bool>();
    _roboTimeScale = System::IniConf::GameTimeLine.Get<int>();
}

void NC_STACK_yparobo::HandBrake(update_msg *arg)
{
    NC_STACK_ypabact::HandBrake(arg);

    if ( GetHandBrakePower() <= 0.0f )
        return;

    _thraction = 0;
    _roboFlotage = _mass * 9.80665;
}

void NC_STACK_yparobo::CorrectPositionOnLand()
{
}

void NC_STACK_yparobo::DeadTimeUpdate(update_msg *arg)
{
    int a4 = getBACT_yourLastSeconds();

    if ( !(_status_flg & BACT_STFLAG_DEATH2) )
    {
        setState_msg arg78;
        arg78.newStatus = BACT_STATUS_NOPE;
        arg78.unsetFlags = 0;
        arg78.setFlags = BACT_STFLAG_DEATH2;
        SetState(&arg78);
    }

    _status_flg |= BACT_STFLAG_LAND;

    // disable_genesis: the host station falls and explodes as usual but releases no death plasma.
    if ( _owner == 0 || _vp_genesis == nullptr || _disable_genesis )
    {
        if ( a4 > 0 )
            return;

        int v31 = getBACT_inputting();

        if ( v31 )
            _status_flg |= BACT_STFLAG_NORENDER;
        else
            Release();

        return;
    }

    int v33 = GetPlasmaDurationMs();

    if ( _vp_extra[0].flags & EVPROTO_FLAG_ACTIVE )
    {
        UpdateDeathPlasmaMagnet(arg->frameTime);
        _scale_time -= arg->frameTime;

        if ( _scale_time <= 0 )
        {
            _vp_extra[0].SetVP((NC_STACK_base::Instance *)nullptr);

            if ( a4 > 0 )
                return;

            int v30 = getBACT_inputting();

            if ( v30 )
                _status_flg |= BACT_STFLAG_NORENDER;
            else
                Release();

            return;
        }

        _vp_extra[0].scale = sqrt((float)_scale_time / (float)v33) * 0.75;

        if ( _vp_extra[0].scale < 0.0 )
            _vp_extra[0].scale = 0;

        float v16 = _maxrot * 2.0 * ((float)arg->frameTime / 1000.0);

        _vp_extra[0].rotate = mat3x3::RotateY(v16) * _vp_extra[0].rotate;

        if ( a4 <= 0 )
            _status_flg |= BACT_STFLAG_NORENDER;
    }
    else
    {
        _scale_time = v33;
        _vp_extra[0].scale = 0.75;
        _vp_extra[0].pos = _position;
        _vp_extra[0].rotate = _rotation;
        _vp_extra[0].SetVP(_vp_genesis);
        _vp_extra[0].flags |= (EVPROTO_FLAG_ACTIVE | EVPROTO_FLAG_SCALE);

        if ( _world->_isNetGame )
        {
            uamessage_startPlasma sbMsg;
            sbMsg.msgID = UAMSG_STARTPLASMA;
            sbMsg.owner = _owner;
            sbMsg.scale = 0.75;
            sbMsg.time = v33;
            sbMsg.id = _gid;
            sbMsg.pos = _position;
            sbMsg.dir = _rotation;

            _world->NetBroadcastMessage(&sbMsg, sizeof(sbMsg), true);
        }
    }
}

void NC_STACK_yparobo::yparobo_func128(robo_arg128 *arg)
{
    arg->comm_bacto = nullptr;
    arg->comm_bact = nullptr;

    for ( NC_STACK_ypabact* &node : _kidList )
    {
        if ( !(node->_status_flg & BACT_STFLAG_DEATH1) )
        {
            int v7;

            if ( arg->flags & 1 )
            {
                if ( arg->comm_id == node->_commandID )
                {
                    arg->comm_bact = node;
                    arg->comm_bacto = node;
                    v7 = 1;
                }
                else
                {
                    v7 = 0;
                }
            }
            else
            {
                v7 = 1;
            }

            int v21;
            if ( arg->flags & 2 )
            {
                if ( node->_primTtype == arg->tgType || _roboDockTargetType == arg->tgType )
                {
                    Common::Point tgsec = World::PositionToSectorID(arg->tgt_pos);
                    Common::Point rbtg;

                    uint32_t v16;
                    if ( node->_commandID == _roboDockUser && _roboDockUser )
                    {
                        rbtg = World::PositionToSectorID( _roboDockTargetPos );
                        v16 = _roboDockTargetCommandID;
                    }
                    else
                    {
                        rbtg = World::PositionToSectorID( node->_primTpos );
                        v16 = node->_primT_cmdID;
                    }

                    if ( (arg->tgType == BACT_TGT_TYPE_UNIT && !arg->prim_comm_id && arg->tgt.pbact == node->_primT.pbact)
                            || (arg->tgType == BACT_TGT_TYPE_UNIT && arg->prim_comm_id != 0 && v16 == arg->prim_comm_id)
                            || (arg->tgType == BACT_TGT_TYPE_CELL && rbtg == tgsec) )
                    {
                        arg->comm_bact = node;
                        arg->comm_bacto = node;
                    }
                }
                v21 = 0;
            }
            else
            {
                v21 = 1;
            }

            int v26;
            if ( !(arg->flags & 4) )
            {
                v26 = 1;
            }
            else
            {
                if ( node->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    float tx, ty;

                    if ( node->_commandID == _roboDockUser && _roboDockUser )
                    {
                        tx = _roboDockTargetPos.x;
                        ty = _roboDockTargetPos.z;
                    }
                    else
                    {
                        tx = node->_primTpos.x;
                        ty = node->_primTpos.z;
                    }
                    if ( POW2(arg->distance) > POW2(tx - arg->tgt_pos.x) + POW2(ty - arg->tgt_pos.z) )
                    {
                        arg->comm_bact = node;
                        arg->comm_bacto = node;

                        v26 = 1;
                    }
                    else
                    {
                        v26 = 0;
                    }
                }
                else
                {
                    v26 = 0;
                }
            }

            if ( v7 && v21 && v26 )
                break;
        }
    }
}

void NC_STACK_yparobo::yparobo_func129(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","yparobo_func129");
}

void NC_STACK_yparobo::yparobo_func130(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","yparobo_func130");
}

void NC_STACK_yparobo::yparobo_func131(IDVPair *arg)
{
    dprintf("MAKE ME %s\n","yparobo_func131");
}

size_t NC_STACK_yparobo::yparobo_func132(setTarget_msg *arg)
{
    if ( !arg->priority )
    {
        arg->tgt_type = BACT_TGT_TYPE_NONE;
        return 0;
    }

    if (_kidRef) //FIXME Think it's must be yw->_unitsList
    {
        for (NC_STACK_ypabact* &node : *_kidRef.PList())
        {
            if (arg->priority == node->_commandID)
            {
                if ( node->_status != BACT_STATUS_DEAD &&
                     (node->_owner == _owner || !node->IsIgnoredByAI()) )
                {
                    arg->priority = 0;
                    arg->tgt_type = BACT_TGT_TYPE_UNIT;
                    arg->tgt.pbact = node;
                    return 1;
                }
                else
                {
                    arg->priority = 0;
                    arg->tgt_type = BACT_TGT_TYPE_NONE;
                    return 0;
                }
            }

            for (NC_STACK_ypabact* &subnode : node->_kidList)
            {
                if (arg->priority == subnode->_commandID)
                {
                    if ( subnode->_status != BACT_STATUS_DEAD &&
                         (subnode->_owner == _owner || !subnode->IsIgnoredByAI()) )
                    {
                        arg->priority = 0;
                        arg->tgt_type = BACT_TGT_TYPE_UNIT;
                        arg->tgt.pbact = subnode;
                        return 1;
                    }
                    else
                    {
                        arg->priority = 0;
                        arg->tgt_type = BACT_TGT_TYPE_NONE;
                        return 0;
                    }
                }
            }
        }
    }

    arg->tgt_type = BACT_TGT_TYPE_NONE;
    arg->priority = 0;
    return 0;
}

int NC_STACK_yparobo::yparobo_func134__sub1(robo_arg134 *arg)
{
    int v2 = 1;

    switch(arg->field_4)
    {
    case 1:
    {
        NC_STACK_ypabact *bacto = _world->getYW_userVehicle();

        if ( bacto->_bact_type != BACT_TYPES_MISSLE )
            v2 = 1;
        else
            v2 = 0;
    }
    break;

    case 2:
    case 3:
    case 4:
        if ( arg->field_8 == stru_5B0628[ arg->field_4 ].field_0 && arg->field_C == stru_5B0628[ arg->field_4 ].field_4 )
            v2 = 0;
        else
            v2 = 1;
        break;

    case 6:
        v2 = (_clock - dword_515138[ arg->field_8 ] > 45000) || dword_515138[ arg->field_8 ] == 0;
        dword_515138[ arg->field_8 ] = _clock;
        break;

    case 7:
        v2 = 1;
        if ( arg->unit && arg->unit->_bact_type == BACT_TYPES_GUN )
        {
            NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( arg->unit );

            if ( gun->IsRoboGun() )
                v2 = 0;
        }

        break;

    case 9:
    case 10:
    case 13:
        if ( arg->field_8 == stru_5B0628[arg->field_4].field_0 )
            v2 = 0;
        else
            v2 = 1;
        break;

    case 5:
    case 8:
    case 11:
    case 12:
    case 14:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
        v2 = 1;
        break;

    case 18:
    case 40:
        if ( (_clock - stru_5B0628[ arg->field_4 ].field_10) > arg->field_8 )
        {
            stru_5B0628[ arg->field_4 ].field_10 = _clock;
            v2 = 1;
        }
        else
            v2 = 0;

        break;

    default:
        break;
    }

    if ( v2 )
    {
        stru_5B0628[arg->field_4].field_0 = arg->field_8;
        stru_5B0628[arg->field_4].field_4 = arg->field_C;
        stru_5B0628[arg->field_4].field_8 = arg->field_10;
    }

    return v2;
}

void NC_STACK_yparobo::ypabact_func65__sub0()
{
    if ( _beam_time )
    {
        if ( _clock - _beam_time > 2000 )
        {
            bool isNotViewBeaming = false;

            for ( NC_STACK_ypabact* &v1 : _pSector->unitsList )
            {
                if ( v1->_status == BACT_STATUS_BEAM && v1->_owner == _owner && !v1->getBACT_viewer() )
                {
                    isNotViewBeaming = true;
                    break;
                }
            }

            if ( !isNotViewBeaming )
            {
                _world->ypaworld_func168(this);

                vec3d tt = _position;

                _position = _old_pos;
                _roboYPos = _old_pos.y;

                if ( !_world->SaveGame( fmt::sprintf("save:%s/%d.fin", _world->_GameShell->UserName, _world->_levelInfo.LevelID) ) )
                    ypa_log_out("Warning, final sgm save error\n");

                _position = tt;

                if ( _world->_GameShell )
                    uaDeleteFile( fmt::sprintf("save:%s/%d.rst", _world->_GameShell->UserName, _world->_levelInfo.LevelID) );

                _status_flg |= BACT_STFLAG_CLEAN;

                Die();
            }

        }
    }
    else
    {
        // Manual Host Station movement already brought the Robo physically
        // into the open Beam Gate sector. Keep its real position and start the
        // finish sequence there: no forced centering, no Y snap and no hidden
        // teleport. Vanilla teleport mode keeps the original sector-center snap.
        const bool manualPlayerRobo =
            _playerRoboAIBehavior && IsPlayerRobo() && _world &&
            this == _world->getYW_userHostStation();

        if ( !manualPlayerRobo )
            _position = World::SectorIDToCenterPos3( _cellId );

        for (NC_STACK_ypabact* &unit : _kidList )
        {
            unit->_status_flg |= BACT_STFLAG_CLEAN;

            for (NC_STACK_ypabact* &sub_unit : unit->_kidList )
                sub_unit->_status_flg |= BACT_STFLAG_CLEAN;
        }

        _world->_levelInfo.Buddies.clear();

        int v15 = 0;

        for ( NC_STACK_ypabact* &bct : _pSector->unitsList )
        {
            if  ( bct->_status != BACT_STATUS_DEAD )
            {
                if ( bct->_bact_type != BACT_TYPES_ROBO && bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_GUN && bct->_owner == _owner )
                {
                    vec2d temp = World::SectorIDToCenterPos2( _cellId );
                    float v17 = fabs(bct->_position.x - temp.x);
                    float v29 = fabs(bct->_position.z - temp.y);

                    if ( v17 <= 450.0 || v29 <= 450.0 )
                    {
                        bct->_status = BACT_STATUS_BEAM;

                        bct->_scale_time = 0;

                        bct->_scale_delay = v15 + 1000;
                        v15 += 400;
                    }
                }
            }
        }

        _beam_time = _clock;
    }
}

void NC_STACK_yparobo::setBACT_inputting(bool inpt)
{
    if ( inpt && _world && !_world->CanControlUnitInSpectatorMode(this) )
        return;

    NC_STACK_ypabact::setBACT_inputting(inpt);

    if ( inpt )
    {
        _world->setYW_userHostStation(this);

        _roboState |= ROBOSTATE_PLAYERROBO;
        _playerRoboAIBehavior = System::IniConf::GameRoboPlayerAIBehavior.Get<bool>();
    }
}

void NC_STACK_yparobo::setROBO_proto(World::TRoboProto *proto)
{
    for (World::TRoboGun &gun : _roboGuns)
    {
        if ( gun.gun_obj )
        {
            if ( !( gun.gun_obj->_status_flg & BACT_STFLAG_DEATH1 ) )
                gun.gun_obj->Die();

            gun.gun_obj->Release();

            gun.gun_obj = nullptr;
        }
    }

    _roboGuns = proto->guns;

    for (World::TRoboGun &gun : _roboGuns)
    {
        ypaworld_arg146 gun_req;

        gun_req.pos = _position + _rotation.Transpose().Transform(gun.pos);

        gun_req.vehicle_id = gun.robo_gun_type;

        NC_STACK_ypabact *gun_obj = _world->ypaworld_func146(&gun_req);
        NC_STACK_ypagun *gungun = dynamic_cast<NC_STACK_ypagun *>(gun_obj);

        gun.gun_obj = gun_obj;

        if ( gun_obj )
        {
            if (gungun)
            {
                gungun->ypagun_func128(gun.dir, false);

                gungun->setGUN_roboGun(1);
            }

            gun_obj->_owner = _owner;
            gun_obj->_commandID = dword_5B1128;
            gun_obj->_host_station = this;
            gun_obj->InheritActiveDebuffFromParent(this);
            gun_obj->_gunDisplayName = gun.robo_gun_name;

            dword_5B1128++;

            if ( _world->_isNetGame )
            {
                gun_obj->_gid |= gun_obj->_owner << 24;
                gun_obj->_commandID |= gun_obj->_owner << 24;
            }

            gun_obj->_aggr = 60;

            AddSubject(gun_obj);
        }
        else
        {
            ypa_log_out("Unable to create Robo-Gun\n");
        }
    }

    _roboDockPos = proto->dock;
    _viewer_position = proto->viewer;
    _viewer_max_up = proto->robo_viewer_max_up;
    _viewer_max_down = proto->robo_viewer_max_down;
    _viewer_max_side = proto->robo_viewer_max_side;

    setBACT_extraViewer(true);
    setBACT_alwaysRender(true);

    _roboColls = proto->coll;
}

void NC_STACK_yparobo::setROBO_epConquer(int ep)
{
    _roboEpConquer = ep;
}

void NC_STACK_yparobo::setROBO_epDefense(int ep)
{
    _roboEpDefense = ep;
}

void NC_STACK_yparobo::setROBO_epRadar(int ep)
{
    _roboEpRadar = ep;
}

void NC_STACK_yparobo::setROBO_epPower(int ep)
{
    _roboEpPower = ep;
}

void NC_STACK_yparobo::setROBO_epSafety(int ep)
{
    _roboEpSafety = ep;
}

void NC_STACK_yparobo::setROBO_commCount(int comm)
{
    dword_5B1128 = comm;
}

void NC_STACK_yparobo::setROBO_battVehicle(int batt)
{
    _roboEnergyLife = batt;
}

void NC_STACK_yparobo::setROBO_battBeam(int batt)
{
    _roboEnergyMove = batt;
}

void NC_STACK_yparobo::setROBO_fillMode(int fil)
{
    _roboFillMode = fil;
}

void NC_STACK_yparobo::setROBO_waitSway(int wait)
{
    if ( wait )
        _roboWFlags |= 1;
    else
        _roboWFlags &= ~1;
}

void NC_STACK_yparobo::setROBO_waitRotate(int wait)
{
    if ( wait )
        _roboWFlags |= 2;
    else
        _roboWFlags &= ~2;
}

void NC_STACK_yparobo::setROBO_epChangeplace(int ep)
{
    _roboEpChangePlace = ep;
}

void NC_STACK_yparobo::setROBO_epReconnoitre(int ep)
{
    _roboEpRecon = ep;
}

void NC_STACK_yparobo::setROBO_epRobo(int ep)
{
    _roboEpRobo = ep;
}

void NC_STACK_yparobo::setROBO_viewAngle(int angl)
{
    _viewer_rotation = mat3x3::Ident();
    _viewer_horiz_angle = angl * C_PI_180;
    _viewer_rotation = mat3x3::RotateY(_viewer_horiz_angle) * _viewer_rotation;
}

void NC_STACK_yparobo::setROBO_safDelay(int delay)
{
    _roboSafetyDelay = delay;
}

void NC_STACK_yparobo::setROBO_powDelay(int delay)
{
    _roboPowerDelay = delay;
}

void NC_STACK_yparobo::setROBO_radDelay(int delay)
{
    _roboRadarDelay = delay;
}

void NC_STACK_yparobo::setROBO_cplDelay(int delay)
{
    _roboPositionDelay = delay;
}

void NC_STACK_yparobo::setROBO_defDelay(int delay)
{
    _roboEnemyDelay = delay;
}

void NC_STACK_yparobo::setROBO_conDelay(int delay)
{
    _roboConqDelay = delay;
}

void NC_STACK_yparobo::setROBO_robDelay(int delay)
{
    _roboDangerDelay = delay;
}

void NC_STACK_yparobo::setROBO_recDelay(int delay)
{
    _roboExploreDelay = delay;
}

int NC_STACK_yparobo::getROBO_epConquer()
{
    return _roboEpConquer;
}

int NC_STACK_yparobo::getROBO_epDefense()
{
    return _roboEpDefense;
}

int NC_STACK_yparobo::getROBO_epRadar()
{
    return _roboEpRadar;
}

int NC_STACK_yparobo::getROBO_epPower()
{
    return _roboEpPower;
}

int NC_STACK_yparobo::getROBO_epSafety()
{
    return _roboEpSafety;
}

int NC_STACK_yparobo::getROBO_commCount()
{
    return dword_5B1128;
}

int NC_STACK_yparobo::getROBO_battVehicle()
{
    return _roboEnergyLife;
}

int NC_STACK_yparobo::getROBO_battBeam()
{
    return _roboEnergyMove;
}

int NC_STACK_yparobo::getROBO_fillMode()
{
    return _roboFillMode;
}

std::vector<World::TRoboGun> &NC_STACK_yparobo::GetGuns()
{
    return _roboGuns;
}

int NC_STACK_yparobo::getROBO_epChangeplace()
{
    return _roboEpChangePlace;
}

int NC_STACK_yparobo::getROBO_epReconnoitre()
{
    return _roboEpRecon;
}

int NC_STACK_yparobo::getROBO_epRobo()
{
    return _roboEpRobo;
}

int NC_STACK_yparobo::getROBO_roboState()
{
    return _roboState;
}

int NC_STACK_yparobo::getROBO_safDelay()
{
    return _roboSafetyDelay;
}

int NC_STACK_yparobo::getROBO_powDelay()
{
    return _roboPowerDelay;
}

int NC_STACK_yparobo::getROBO_radDelay()
{
    return _roboRadarDelay;
}

int NC_STACK_yparobo::getROBO_cplDelay()
{
    return _roboPositionDelay;
}

int NC_STACK_yparobo::getROBO_defDelay()
{
    return _roboEnemyDelay;
}

int NC_STACK_yparobo::getROBO_conDelay()
{
    return _roboConqDelay;
}

int NC_STACK_yparobo::getROBO_robDelay()
{
    return _roboDangerDelay;
}

int NC_STACK_yparobo::getROBO_recDelay()
{
    return _roboExploreDelay;
}

int NC_STACK_yparobo::getROBO_loadFlags()
{
    if ( ShouldUsePlayerRoboResourceTrend() )
        return _playerRoboResourceTrendLoadFlags;

    return _roboEnergyLoadFlags;
}

int NC_STACK_yparobo::getROBO_lossFlags()
{
    if ( ShouldUsePlayerRoboResourceTrend() )
        return _playerRoboResourceTrendLossFlags;

    return _roboEnergyLossFlags;
}

int NC_STACK_yparobo::getROBO_absReload()
{
    return _roboEnergyReloadPS;
}

NC_STACK_yparobo::NC_STACK_yparobo()
{
    _roboFlotage = 0.;
    _roboColls = World::rbcolls();
    _roboYPos = 0.;

    _roboWFlags = 0;
    _roboState = 0;
    _roboNewAI = 0;
    _roboTimeScale = 0;

    _roboEpConquer = 0;
    _roboEpRadar = 0;
    _roboEpPower = 0;
    _roboEpDefense = 0;
    _roboEpSafety = 0;
    _roboEpRecon = 0;
    _roboEpChangePlace = 0;
    _roboEpRobo = 0;

    _roboDockEnerg = 0;
    _roboDockCnt = 0;
    _roboDockUser = 0;
    _roboDockTime = 0;
    _roboDockTargetPos = vec3d();
    _roboDockTargetBact = nullptr;
    _roboDockTargetType = 0;
    _roboDockAggr = 0;
    _roboDockTargetCommandID = 0;
    _roboDockPos = vec3d();

    _roboRadarValue = 0;
    _roboRadarCell = nullptr;
    _roboRadarCellID = 0;  //For AI
    _roboRadarCellIndex = 0;
    _roboRadarTime = 0;
    _roboRadarDelay = 0; //??

    _roboSafetyValue = 0;
    _roboSafetyCell = nullptr;  //For AI
    _roboSafetyCellID = 0;  //For AI
    _roboSafetyCellIndex = 0;
    _roboSafetyTime = 0;
    _roboSafetyDelay = 0; //??

    //Power
    _roboPowerValue = 0;
    _roboPowerCell = nullptr;  //For AI
    _roboPowerCellID = 0;  //For AI
    _roboPowerCellIndex = 0;
    _roboPowerTime = 0;
    _roboPowerDelay = 0; //??

    //Enemy
    _roboEnemyValue = 0;
    _roboEnemyCell = nullptr;  //For AI
    _roboEnemyCellID = 0;  //For AI
    _roboEnemyCommandID = 0;  //For AI
    _roboEnemyTime = 0;
    _roboEnemyDiv = 0;
    _roboEnemyDelay = 0; //??

    _roboConqValue = 0;
    _roboConqCell = nullptr;  //For AI
    _roboConqCellID = 0;  //For AI
    _roboConqCellIndex = 0;
    _roboConqTime = 0;
    _roboConqDelay = 0; //??

    _roboPositionValue = 0;
    _roboPositionCell = nullptr;  //For AI
    _roboPositionCellID = 0;  //For AI
    _roboPositionCellIndex = 0;  //For AI
    _roboPositionTime = 0;
    _roboPositionDelay = 0; //??
    _roboExploreValue = 0;
    _roboExploreCell = nullptr;  //For AI
    _roboExploreCellID = 0;  //For AI
    _roboExploreCellIndex = 0;  //For AI
    _roboExploreTime = 0;
    _roboExploreDelay = 0; //??
    _roboDangerValue = 0;
    _roboDangerCell = nullptr;  //For AI
    _roboDangerCellID = 0;  //For AI
    _roboDangerCommandID = 0;  //For AI
    _roboDangerTime = 0;
    _roboDangerDelay = 0; //??
    _roboVehicleCellID = 0;  //For AI
    _roboVehicleCell = nullptr;  //For AI

    _roboVehicleDuty = 0;  //For AI
    _roboVehicleCommandID = 0;  //For AI
    _roboBuildingCellID = 0;  //For AI
    _roboBuildingCell = nullptr;  //For AI
    _roboBuildingDuty = 0;  //For AI

    _roboTestEnemyTime = 0;

    _roboGuns.clear();

    _roboEnergyLife = 0; //??

    _roboEnergyMove = 0; //??

    _roboFillMode = 0;
    _roboEnergyLoadFlags = 0;
    _roboEnergyLossFlags = 0;
    ResetPlayerRoboResourceTrend();

    _roboEnergyReloadPS = 0;
    _roboBuildSpare = 0;
    _roboVehicleSpare = 0;
    _roboBeamTimePre = 0;
    _roboBeamPos = vec3d();
    _roboBeamFXTime = 0;

    _playerRoboAIBehavior = false;
    ResetPlayerMobileMove();

    for (auto &t : _roboAttackers)
    {
        t.field_0 = 0;
        t.field_4 = 0;
    }

    _roboAttackersTime = 0;
    _roboAttackersClearTime = 0;
}
