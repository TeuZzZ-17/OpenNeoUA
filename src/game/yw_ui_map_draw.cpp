// Tactical map drawing: sectors, units, Host Stations, map icons and lines,
// map window creation, scrolling and zoom.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <iterator>
#include <array>
#include <algorithm>
#include <map>
#include <vector>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "nucleus/loaders.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "game/ypagun.h"
#include "platform/windp.h"
#include "world/analyzer.h"
#include "system/inivals.h"
#include "game/yw_game_ui_internal.h"

Common::Point sub_4F681C(vec2d in)
{
    if ( robo_map.field_1E8 & 0x100 )
    {
        float a1a = in.x - robo_map.field_1D8;
        float a2a = in.y - robo_map.field_1DC;

        in.x = robo_map.field_1D8 + robo_map.field_264 * a1a + flt_5164F4 * a2a;
        in.y = robo_map.field_1DC + flt_516504 * a1a + flt_51650C * a2a;
    }

    return Common::Point( robo_map.field_200 + dround(in.x / robo_map.field_1E0) - robo_map.field_1F0
                        , robo_map.field_204 + dround(-in.y / robo_map.field_1E4) - robo_map.field_1F4 );
}

void sub_4F68FC(float a3, float a4, float a5, float a6, SDL_Color a7)
{
    GFX::Engine.raster_func217(a7);
    GFX::Engine.raster_func201( Common::Line( sub_4F681C({a3, a4}), sub_4F681C({a5, a6}) )  );
}

bool GetPlayerRoboAIBehaviorMapTarget(NC_STACK_ypaworld *yw, vec3d *target)
{
    if ( !yw || !yw->_userRobo || !target )
        return false;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);
    if ( !robo )
        return false;

    return robo->GetPlayerRoboAIBehaviorMoveTarget(target);
}

void sub_4F6980(CmdStream *cur, float a1, float a2, uint32_t a3, int a4, int a5, int screenYOffset)
{
    Common::Point tmp = sub_4F681C( {a1, a2} );

    int v7 = tmp.x - a4 / 2 - robo_map.field_200;
    int v8 = tmp.y - a5 / 2 - robo_map.field_204 + screenYOffset;
    int v9 = v7 + a4;
    int v10 = v8 + a5;

    if ( v7 < robo_map.field_1F8 && v8 < robo_map.field_1FC && v9 > 0 && v10 > 0 )
    {
        int v11;

        if ( v7 >= 0 )
        {
            v11 = 0;
        }
        else
        {
            v11 = -v7;
            v7 = 0;
        }

        int v33;

        if ( v8 >= 0 )
        {
            v33 = 0;
        }
        else
        {
            v33 = -v8;
            v8 = 0;
        }

        int v34;

        if ( v9 <= robo_map.field_1F8 )
            v34 = 0;
        else
            v34 = robo_map.field_1F8 - v7;

        int v35;

        if ( v10 <= robo_map.field_1FC )
            v35 = 0;
        else
            v35 = robo_map.field_1FC - v8;

        FontUA::set_center_xpos(cur, robo_map.field_200 + v7);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v8);

        if ( v33 )
        {
            FontUA::set_yoff(cur, v33);
        }
        else if ( v35 )
        {
            FontUA::set_yheight(cur, v35);
        }

        if ( v11 )
        {
            FontUA::set_xoff(cur, v11);
        }
        else if ( v34 )
        {
            FontUA::set_xwidth(cur, v34);
        }

        FontUA::store_glyph(cur, a3);
    }
}

bool yw_IsDirectPlayerControlledUnit(NC_STACK_ypaworld *yw,
                                             NC_STACK_ypabact *bact)
{
    return yw && bact &&
           yw->_userUnit == bact &&
           (bact->_oflags & BACT_OFLAG_USERINPT);
}

void sub_4F72E8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !bact || bact->ShouldHideFromStrategicUI() )
        return;

    // OpenNeoUA invisible: do not draw strategic target/command lines for a cloaked unit.
    if ( bact->IsInvisibleUnrevealed() )
        return;

    // Keep this renderer close to vanilla. Spectator target filtering belongs in
    // gameplay/target selection, not in strategic-line drawing; filtering lines
    // here proved unsafe during in-game testing. Only null-check target pointers.
    const bool directPlayerControl = yw_IsDirectPlayerControlledUnit(yw, bact);

    if ( bact != yw->_userRobo && bact->_host_station != bact->_parent && bact->_parent )
    {
        SDL_Color clr;
        NC_STACK_ypabact *bct;

        if ( !directPlayerControl && bact->_secndTtype == BACT_TGT_TYPE_UNIT )
        {
            clr = yw->GetColor(10);
            bct = bact->_secndT.pbact;
        }
        else
        {
            if ( !bact->_parent )
                return;

            clr = yw->GetColor(11);
            bct = bact->_parent;
        }

        if ( !bct )
            return;

        sub_4F68FC(bact->_position.x, bact->_position.z, bct->_position.x, bct->_position.z, clr);
        return;
    }

    // A directly controlled unit may still have a transient player lock target,
    // but that is not an AI order and must not be represented by strategic
    // attack/waypoint lines. Squad relationship lines above remain available.
    if ( directPlayerControl )
        return;

    if ( bact->_secndTtype == BACT_TGT_TYPE_UNIT )
    {
        NC_STACK_ypabact *bct = bact->_secndT.pbact;
        if ( !bct )
            return;

        sub_4F68FC(bact->_position.x, bact->_position.z, bct->_position.x, bct->_position.z,  yw->GetColor(10));
        return;
    }

    if ( bact->_secndTtype == BACT_TGT_TYPE_CELL )
    {
        sub_4F68FC(bact->_position.x, bact->_position.z, bact->_sencdTpos.x, bact->_sencdTpos.z, yw->GetColor(10));
    }
    else
    {
        if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
        {
            sub_4F68FC(bact->_position.x, bact->_position.z, bact->_primTpos.x, bact->_primTpos.z, yw->GetColor(9));
        }
        else if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            if ( !bact->_primT.pbact )
                return;

            sub_4F68FC(bact->_position.x, bact->_position.z, bact->_primT.pbact->_position.x, bact->_primT.pbact->_position.z, yw->GetColor(9));
        }

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINT )
        {
            if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = 1; i < bact->_waypoints_count; i++)
                    sub_4F68FC(bact->_waypoints[i - 1].x, bact->_waypoints[i - 1].z, bact->_waypoints[i].x, bact->_waypoints[i].z, clr);

                sub_4F68FC(bact->_waypoints[bact->_waypoints_count - 1].x, bact->_waypoints[bact->_waypoints_count - 1].z, bact->_waypoints[0].x, bact->_waypoints[0].z, clr);
            }
            else if ( bact->_waypoints_count > 0 )
            {
                SDL_Color clr = yw->GetColor(9);

                for (int i = bact->_current_waypoint; i < bact->_waypoints_count - 1; i++)
                    sub_4F68FC(bact->_waypoints[i].x, bact->_waypoints[i].z, bact->_waypoints[i + 1].x, bact->_waypoints[i + 1].z, clr);
            }
        }
    }
}

void  sb_0x4f8f64__sub1__sub0(NC_STACK_ypaworld *yw)
{
    for( NC_STACK_ypabact* &bact : yw->_unitsList )
    {
        if ( bact->_bact_type == BACT_TYPES_ROBO && bact != yw->_userRobo )
        {
            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bact);

            for (int i = 0; i < 8; i++)
            {
                int v10 = 0;

                switch ( i )
                {
                case 0:
                    if ( robo->_roboRadarValue > 0 )
                        v10 = robo->_roboRadarCellID;
                    break;

                case 1:
                    if ( robo->_roboSafetyValue > 0 )
                        v10 = robo->_roboSafetyCellID;
                    break;

                case 2:
                    if ( robo->_roboPowerValue > 0 )
                        v10 = robo->_roboPowerCellID;
                    break;

                case 3:
                    if ( robo->_roboEnemyValue > 0 )
                        v10 = robo->_roboEnemyCellID;
                    break;

                case 4:
                    if ( robo->_roboConqValue > 0 )
                        v10 = robo->_roboConqCellID;
                    break;

                case 5:
                    if ( robo->_roboPositionValue > 0 )
                        v10 = robo->_roboPositionCellID;
                    break;

                case 6:
                    if ( robo->_roboExploreValue > 0 )
                        v10 = robo->_roboExploreCellID;
                    break;

                case 7:
                    if ( robo->_roboDangerValue > 0 )
                        v10 = robo->_roboDangerCellID;
                    break;

                default:
                    break;
                }

                if ( v10 )
                {
                    vec2d secC = World::SectorIDToCenterPos2( {v10 % yw->_mapSize.x, v10 / yw->_mapSize.x} );

                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(i));
                }
            }

            if ( robo->_roboBuildingDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboBuildingCellID % yw->_mapSize.x, robo->_roboBuildingCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(0));
            }

            if ( robo->_roboVehicleDuty )
            {
                vec2d secC = World::SectorIDToCenterPos2( {robo->_roboVehicleCellID % yw->_mapSize.x, robo->_roboVehicleCellID / yw->_mapSize.x} );

                if ( (yw->_timeStamp / 300) & 1 )
                {
                    sub_4F68FC(bact->_position.x, bact->_position.z, secC.x, secC.y, yw->GetColor(7));
                }
            }
        }
    }
}

void sb_0x4f8f64__sub1(NC_STACK_ypaworld *yw)
{
    if ( robo_map.field_1E8 & 0x100 )
    {
        if ( yw->_userRobo != yw->_userUnit && yw->_userUnit->_parent )
        {
            sub_4F68FC(yw->_userUnit->_position.x, yw->_userUnit->_position.z, yw->_userUnit->_parent->_position.x, yw->_userUnit->_parent->_position.z, yw->GetColor(11));

            if ( yw->_userUnit->IsParentMyRobo() &&
                 !yw_IsDirectPlayerControlledUnit(yw, yw->_userUnit) )
            {
                int v7 = 0;
                float a5, a6;

                if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    a5 = yw->_userUnit->_primTpos.x;
                    a6 = yw->_userUnit->_primTpos.z;
                    v7 = 1;
                }
                else if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_UNIT )
                {
                    a5 = yw->_userUnit->_primT.pbact->_position.x;
                    a6 = yw->_userUnit->_primT.pbact->_position.z;
                    v7 = 1;
                }

                if ( v7 )
                    sub_4F68FC(yw->_userUnit->_position.x, yw->_userUnit->_position.z, a5, a6, yw->GetColor(9));
            }
        }
    }
    else
    {
        if ( robo_map.field_1EC & 4 )
        {
            for (NC_STACK_ypabact* const comm: yw->_cmdrsRemap)
            {
                if (comm->_status != BACT_STATUS_DEAD)
                    sub_4F72E8(yw, comm);

                for( NC_STACK_ypabact* &kid : comm->_kidList )
                {
                    if ( kid->_status != BACT_STATUS_DEAD && !kid->ShouldHideFromStrategicUI() )
                        sub_4F72E8(yw, kid);
                }
            }
        }
        else
        {
            if ( yw->_activeCmdrRemapIndex != -1 )
            {
                NC_STACK_ypabact *tmp = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];

                if ( tmp->_status != BACT_STATUS_DEAD )
                    sub_4F72E8(yw, tmp);
            }
        }

        // Position cross of the controlled unit; the Spectator has none.
        if ( yw->_userUnit && !yw->IsSpectatorControlled() )
        {
            sub_4F68FC(yw->_userUnit->_position.x, 0.0, yw->_userUnit->_position.x, -yw->_mapLength.y, yw->GetColor(13));
            sub_4F68FC(0.0, yw->_userUnit->_position.z, yw->_mapLength.x, yw->_userUnit->_position.z, yw->GetColor(13));
        }

        if ( !yw->IsSpectatorControlled() )
        {
            vec3d playerRoboTarget;
            if ( GetPlayerRoboAIBehaviorMapTarget(yw, &playerRoboTarget) )
            {
                sub_4F68FC(yw->_userRobo->_position.x,
                           yw->_userRobo->_position.z,
                           playerRoboTarget.x,
                           playerRoboTarget.z,
                           yw->GetColor(9));
            }
        }

        if ( robo_map.field_1E8 & 0x200 )
        {
            int wh = yw->_screenSize.x / 2;
            int hh = yw->_screenSize.y / 2;

            SDL_Color selectionColor = yw->GetColor(12);
            yw_GetFactionUiAccent(yw, &selectionColor);
            GFX::Engine.raster_func217(selectionColor);

            GFX::Engine.raster_func201( {dword_516510 - wh,
                                         dword_516514 - hh,
                                         dword_516518 - wh,
                                         dword_516514 - hh} );
            GFX::Engine.raster_func201( {dword_516518 - wh,
                                         dword_516514 - hh,
                                         dword_516518 - wh,
                                         dword_51651C - hh} );
            GFX::Engine.raster_func201( {dword_516518 - wh,
                                         dword_51651C - hh,
                                         dword_516510 - wh,
                                         dword_51651C - hh} );
            GFX::Engine.raster_func201( {dword_516510 - wh,
                                         dword_51651C - hh,
                                         dword_516510 - wh,
                                         dword_516514 - hh} );
        }
    }

    if ( yw->_showDebugMode )
        sb_0x4f8f64__sub1__sub0(yw);
}

void sb_0x4f8f64__sub2__sub0(CmdStream *cur, float a1, float a2, char a3, int a4, int a5)
{
    Common::Point tmp = sub_4F681C( {a1, a2} );

    int v8 = tmp.x - a4 / 2 - robo_map.field_200;
    int v9 = tmp.y - a4 / 2 - robo_map.field_204;
    int v10 = v8 + a4;
    int v11 = v9 + a5;

    if ( v8 < robo_map.field_1F8 && v9 < robo_map.field_1FC && v10 > 0 && v11 > 0 )
    {
        int v31;
        if ( v8 >= 0 )
        {
            v31 = 0;
        }
        else
        {
            v31 = -v8;
            v8 = 0;
        }

        int v12;
        if ( v9 >= 0 )
        {
            v12 = 0;
        }
        else
        {
            v12 = -v9;
            v9 = 0;
        }

        int v30;
        if ( v10 <= robo_map.field_1F8 )
            v30 = 0;
        else
            v30 = robo_map.field_1F8 - v8;

        int v32;
        if ( v11 <= robo_map.field_1FC )
            v32 = a5;
        else
            v32 = robo_map.field_1FC - v9;

        FontUA::set_center_xpos(cur, robo_map.field_200 + v8);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v9);

        if ( v12 )
            FontUA::set_yoff(cur, v12);

        if ( v32 )
            FontUA::set_yheight(cur, v32);

        if ( v31 )
        {
            FontUA::set_xoff(cur, v31);
        }
        else if ( v30 )
        {
            FontUA::set_xwidth(cur, v30);
        }

        FontUA::store_u8(cur, a3);
    }
}

void sb_0x4f8f64__sub2(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int a4 = 0, v4 = 0;

    switch ( robo_map.field_1EE )
    {
    case 0:
        v4 = 16;
        a4 = 4;
        break;

    case 1:
        v4 = 17;
        a4 = 8;
        break;

    case 2:
        v4 = 18;
        a4 = 16;
        break;

    case 3:
        v4 = 19;
        a4 = 32;
        break;

    case 4:
        v4 = 20;
        a4 = 64;
        break;

    default:
        break;
    }

    if ( a4 >= 8 )
    {
        if ( robo_map.field_1EC & 1 )
        {
            FontUA::select_tileset(cur, v4);

            for ( const auto &ps : yw->_powerStations )
            {
                if ( ps.second.pCell )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, ps.second.pCell) )
                    {
                        if ( ps.second.EffectivePower > 0 )
                        {
                            int v9 = 0;

                            vec2d tmp = World::SectorIDToCenterPos2(ps.second.CellId);

                            if ( ps.second.EffectivePower <= 32)
                                v9 = 0x80;
                            else if ( ps.second.EffectivePower <= 64)
                                v9 = 0x81;
                            else if ( ps.second.EffectivePower <= 96)
                                v9 = 0x82;
                            else if ( ps.second.EffectivePower <= 128)
                                v9 = 0x83;
                            else if ( ps.second.EffectivePower <= 160)
                                v9 = 0x84;
                            else if ( ps.second.EffectivePower <= 192)
                                v9 = 0x85;
                            else if ( ps.second.EffectivePower <= 224)
                                v9 = 0x86;
                            else if ( ps.second.EffectivePower <= 256)
                                v9 = 0x87;

                            sub_4F6980(cur, tmp.x, tmp.y, 0x89, a4, a4);
                            sb_0x4f8f64__sub2__sub0(cur, tmp.x, tmp.y, v9, a4, a4 / 8);
                        }
                    }
                }
            }

            for ( const TMapGem &gem : yw->_techUpgrades )
            {
                cellArea &cell = yw->SectorAt(gem.CellId);
                if ( yw_IsStrategicMapCellVisible(yw, &cell) )
                {
                    int v13 = 1;

                    if ( cell.PurposeType == cellArea::PT_TECHUPGRADE )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                            v13 = 0;
                    }

                    if ( v13 )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gem.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, 0x88, a4, a4);
                    }
                }
            }

            for ( const TMapGate &gate : yw->_levelInfo.Gates )
            {
                if ( gate.PCell->PurposeType == cellArea::PT_GATECLOSED )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, gate.PCell) )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, 0x93, a4, a4);
                    }

                    for ( const TMapKeySector &ks : gate.KeySectors )
                    {
                        if ( ks.PCell )
                        {
                            if ( yw_IsStrategicMapCellVisible(yw, ks.PCell) )
                            {
                                if ( ks.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 300 & 1 )
                                {
                                    vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                    sub_4F6980(cur, tmp.x, tmp.y, 0x8A, a4, a4);
                                }
                            }
                        }
                    }
                }
                else if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
                {
                    if ( yw_IsStrategicMapCellVisible(yw, gate.PCell) )
                    {
                        if ( yw->_timeStamp / 300 & 1 )
                        {
                            vec2d tmp = World::SectorIDToCenterPos2( gate.CellId );
                            sub_4F6980(cur, tmp.x, tmp.y, 0x94, a4, a4);
                        }
                    }
                }
            }

            for (const TMapSuperItem &sitem : yw->_levelInfo.SuperItems)
            {
                int v29 = 0;
                int v61 = 0;

                if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
                {
                    v29 = 0x8E;

                    if ( sitem.State == TMapSuperItem::STATE_INACTIVE )
                        v61 = 0x8B;
                    else if ( sitem.State == TMapSuperItem::STATE_ACTIVE ||
                              sitem.State == TMapSuperItem::STATE_STOPPED )
                        v61 = 0x8C;
                    else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
                        v61 = 0x8D;
                }
                else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
                {
                    v29 = 0x92;

                    if ( sitem.State == TMapSuperItem::STATE_INACTIVE )
                        v61 = 0x8F;
                    else if ( sitem.State == TMapSuperItem::STATE_ACTIVE ||
                              sitem.State == TMapSuperItem::STATE_STOPPED )
                        v61 = 0x90;
                    else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
                        v61 = 0x91;
                }

                if ( v61 )
                {
                    int v34 = 0;

                    if ( yw_IsStrategicMapCellVisible(yw, sitem.PCell) )
                    {
                        if ( sitem.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 300 & 1 )
                            v34 = 1;
                    }

                    if ( v34 )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( sitem.CellId );
                        sub_4F6980(cur, tmp.x, tmp.y, v61, a4, a4);
                    }

                    if ( v29 )
                    {
                        for ( const TMapKeySector &ks : sitem.KeySectors )
                        {
                            if ( ks.PCell )
                            {
                                if ( yw_IsStrategicMapCellVisible(yw, ks.PCell) )
                                {
                                    if ( ks.PCell->owner == yw->_userRobo->_owner || yw->_timeStamp / 500 & 1 )
                                    {
                                        vec2d tmp = World::SectorIDToCenterPos2( ks.CellId );
                                        sub_4F6980(cur, tmp.x, tmp.y, v61, a4, a4);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void sub_4F6DFC(NC_STACK_ypaworld *yw, CmdStream *cur, int height, int width, NC_STACK_ypabact *bact, int a6)
{
    // OpenNeoUA invisible: cloaked stealth units never appear on the strategic map / radar
    // (icon, view triangle and health bar are all drawn from here).
    // The Spectator's own UFO never appears on the map or radar (icon and view triangle).
    if ( yw->IsSpectatorBact(bact) )
        return;

    // Spectator Mode sees every faction, including the hidden Black Sect.
    if ( bact->_status != BACT_STATUS_DEAD && !bact->IsInvisibleUnrevealed() &&
         (yw->IsSpectatorControlled() || !bact->IsHiddenFor( yw->GetPlayerOwner() )) )
    {
        int v8 = 0;

        if ( a6 )
        {
            if ( bact->_bact_type == BACT_TYPES_MISSLE )
                v8 = bact->_owner + 1;
            else
            {
                int protoId = ResolveUnitDisplayVehicleProtoId(yw, bact);
                v8 = protoId >= 0 ? yw->_vhclProtos[protoId].type_icon : 0;
            }
        }
        else
        {
            switch ( bact->_bact_type )
            {
            case BACT_TYPES_BACT:
                v8 = bact->_owner + 9;
                break;

            case BACT_TYPES_ROBO:
                switch ( robo_map.field_1EE )
                {
                case 0:
                case 1:
                    v8 = bact->_owner + 0x30;
                    break;

                case 2:
                    v8 = bact->_owner + 0x28;
                    break;

                case 3:
                    v8 = bact->_owner + 0x20;
                    break;

                case 4:
                    v8 = bact->_owner + 0x18;
                    break;
                }
                break;

            case BACT_TYPES_MISSLE:
                v8 = bact->_owner + 1;
                break;

            case BACT_TYPES_TANK:
            case BACT_TYPES_CAR:
                v8 = bact->_owner + 0x19;
                break;

            case BACT_TYPES_FLYER:
                v8 = bact->_owner + 0x11;
                break;

            case BACT_TYPES_UFO:
                v8 = bact->_owner + 0x21;
                break;

            case BACT_TYPES_GUN:
                v8 = bact->_owner + 0x29;
                break;

            default:
                v8 = 0x41;
                break;
            }
        }

        sub_4F6980(cur, bact->_position.x, bact->_position.z, v8, height, width);

        if ( bact->_bact_type != BACT_TYPES_MISSLE && robo_map.field_1EE > 2 )
        {
            SDL_Color clr = yw->GetColor(bact->_owner);

            int v31 = 0;
            float v33, v35;

            float v21;
            float v22;
            float v23;
            float v24;
            float v25;

            if ( bact == yw->_userRobo )
            {
                v33 = bact->_viewer_rotation.m02;
                v35 = bact->_viewer_rotation.m22;

                if ( bact == yw->_userUnit )
                {
                    v21 = -380.0;
                    v22 = 380.0;

                    v23 = -300.0;
                    v24 = 600.0;
                    v25 = -300.0;

                    v31 = 1;

                    clr = yw->GetColor(13);
                }
                else
                {
                    v21 = -85.0;
                    v22 = 85.0;

                    v23 = 0.0;
                    v24 = 700.0;
                    v25 = 0.0;
                }
            }
            else if ( bact == yw->_userUnit )
            {
                v33 = yw->_viewerRotation.m02;
                v35 = yw->_viewerRotation.m22;

                v21 = -190.0;
                v22 = 190.0;

                v23 = -200.0;
                v24 = 400.0;
                v25 = -200.0;

                v31 = 1;

                clr = yw->GetColor(13);
            }
            else
            {
                v33 = bact->_rotation.m02;
                v35 = bact->_rotation.m22;

                v21 = -40.0;
                v22 = 40.0;

                v23 = 0.0;
                v24 = 350.0;
                v25 = 0.0;
            }

            float v30 = sqrt(POW2(v33) + POW2(v35));
            float v34, v36;

            if ( v30 <= 0.0 )
            {
                v36 = 1.0;
                v34 = 0.0;
            }
            else
            {
                v34 = v33 / v30;
                v36 = v35 / v30;
            }

            float v14 = -v34;

            float a3 = v36 * v21 + v14 * v23 + bact->_position.x;
            float v28 = v34 * v21 + v36 * v23 + bact->_position.z;
            float v16 = v36 * 0.0 + v14 * v24 + bact->_position.x;
            float v15 = v34 * 0.0 + v36 * v24 + bact->_position.z;

            sub_4F68FC(a3, v28, v16, v15, clr);

            float v27 = v36 * v22 + v14 * v25 + bact->_position.x;
            float v29 = v34 * v22 + v36 * v25 + bact->_position.z;

            sub_4F68FC(v27, v29, v16, v15, clr);

            if ( v31 )
                sub_4F68FC(a3, v28, v27, v29, clr); // Also draw base of view triangle
        }

        // This function is reached only for units in cells visible through
        // MapViewMask. When the Energy layer is enabled, show HP for every
        // currently visible non-neutral unit, including enemies and remote
        // Host Stations.
        const bool drawHealthBar = bact->_owner != World::OWNER_0
            && (!yw->IsSpectatorControlled() || !yw->IsSpectatorBact(bact));
        const bool isPlayerHostStation = bact->_bact_type == BACT_TYPES_ROBO
            && bact == yw->_userRobo;

        const bool drawHostStationAtSmallZoom = bact->_bact_type == BACT_TYPES_ROBO;

        if ( bact->_bact_type != BACT_TYPES_MISSLE && !isPlayerHostStation && drawHealthBar )
        {
            if ( robo_map.field_1EC & 4 && bact->_energy_max > 0
                    && (robo_map.field_1EE > 2 || drawHostStationAtSmallZoom) )
            {
                float v32 = (float)bact->_energy / (float)bact->_energy_max;
                int v19;

                if ( v32 > 0.25 )
                {
                    if ( v32 > 0.5 )
                    {
                        if ( v32 > 0.75 )
                            v19 = 0x83;
                        else
                            v19 = 0x82;
                    }
                    else
                    {
                        v19 = 0x81;
                    }
                }
                else
                {
                    v19 = 0x80;
                }

                if ( bact->_bact_type == BACT_TYPES_ROBO )
                {
                    // Host Station icons use MAPROBO, which has no lifemeter
                    // glyphs. Use the same zoom-dependent lifemeter tileset
                    // and dimensions as ordinary units, then restore MAPROBO
                    // for the caller's next Host Station icon.
                    const int healthTileset = yw_RoboMapVehicleTileset();
                    const int healthWidth = yw->_guiTiles[healthTileset]->map[128].w;
                    const int healthHeight = yw->_guiTiles[healthTileset]->h;

                    FontUA::select_tileset(cur, healthTileset);

                    const int hostIconHeight = yw->_guiTiles[1]->h;
                    const int healthBarYOffset = -(hostIconHeight + healthHeight) / 2 + 14;

                    sub_4F6980(cur, bact->_position.x, bact->_position.z, v19,
                                healthWidth, healthHeight, healthBarYOffset);
                    FontUA::select_tileset(cur, 1);
                }
                else
                {
                    // Keep the vanilla vehicle lifemeter dimensions and
                    // tileset. Only Host Stations need the separate MAPROBO
                    // lifemeter path above.
                    sub_4F6980(cur, bact->_position.x, bact->_position.z, v19,
                                height, width);
                }
            }
        }
    }
}

NC_STACK_ypabact * sub_4D3C3C(NC_STACK_ypabact *bact)
{
    if ( !bact )
        return nullptr;

    if ( !bact->_parent )
        return nullptr;

    if ( bact->_parent != bact->_host_station )
        return bact->_parent;

    return bact;
}

void sub_4F7BE8(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact, int a2, int a4, int a5, int a6)
{
    if ( bact )
    {
        if ( bact->ShouldHideFromStrategicUI() )
            return;

        // OpenNeoUA invisible: no strategic-map marker dots for a cloaked unit/squad member.
        if ( bact->IsInvisibleUnrevealed() )
            return;

        FontUA::select_tileset(cur, a2);

        sub_4F6980(cur, bact->_position.x, bact->_position.z, a4, a5, a6);

        for( NC_STACK_ypabact* &kid : bact->_kidList )
        {
            if ( kid->_status != BACT_STATUS_DEAD && !kid->ShouldHideFromStrategicUI() )
                sub_4F6980(cur, kid->_position.x, kid->_position.z, a4, a5, a6);
        }
    }
}

int NC_STACK_ypaworld::sb_0x4f8f64__sub3__sub0(const Common::Point &sector)
{
    int enrg = 0;

    for( NC_STACK_ypabact* &v2 : _cells(sector).unitsList )
    {
        if ( v2->_status != BACT_STATUS_DEAD && v2->_status != BACT_STATUS_BEAM )
        {
            if ( v2->_bact_type != BACT_TYPES_ROBO && v2->_bact_type != BACT_TYPES_MISSLE && v2->_bact_type != BACT_TYPES_GUN )
                enrg += (v2->_energy_max + 99) / 100;
        }
    }

    return enrg;
}

void sb_0x4f8f64__sub3__sub1(NC_STACK_ypaworld *yw, const std::string &labl, int tileset_id, CmdStream *cur, int a4, int a5)
{
    int v8 = yw->_guiTiles[tileset_id]->GetWidth(labl);

    Common::Point tmp = sub_4F681C( World::SectorIDToPos2( {a4, a5} ) );

    int v9 = tmp.x - robo_map.field_200;
    int v10 = tmp.y - robo_map.field_204;

    if ( tmp.x - robo_map.field_200 > 0 && v10 > 0 && tmp.x - robo_map.field_200 + v8 < robo_map.field_1F8 && tmp.y - robo_map.field_204 + yw->_guiTiles[tileset_id]->h < robo_map.field_1FC )
    {
        FontUA::select_tileset(cur, tileset_id);
        FontUA::set_center_xpos(cur, robo_map.field_200 + v9);
        FontUA::set_center_ypos(cur, robo_map.field_204 + v10);

        for ( uint8_t c : labl )
            FontUA::store_s8(cur, c);
    }
}

void sb_0x4f8f64__sub3(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int v111 = 0;
    int a5 = 0;
    int a4 = 0;
    int v114 = 0;
    int v110 = 0;
    int v7 = 0;
    int v99 = 0;

    switch ( robo_map.field_1EE )
    {
    case 0:
        v111 = 4;
        v114 = 59;
        v110 = 16;
        a5 = yw->_guiTiles[59]->h;
        a4 = yw->_guiTiles[59]->map[1].w;
        v7 = 23;
        v99 = 19;
        break;

    case 1:
        v114 = 59;
        v111 = 8;
        v110 = 17;
        a5 = yw->_guiTiles[59]->h;
        a4 = yw->_guiTiles[59]->map[1].w;
        v7 = 23;
        v99 = 19;
        break;

    case 2:
        v111 = 16;
        v114 = 60;
        v110 = 18;
        a5 = yw->_guiTiles[60]->h;
        a4 = yw->_guiTiles[60]->map[1].w;
        v7 = 22;
        v99 = 18;
        break;

    case 3:
        v110 = 19;
        v111 = 32;
        v114 = 61;
        a5 = yw->_guiTiles[61]->h;
        a4 = yw->_guiTiles[61]->map[1].w;
        v7 = 21;
        v99 = 17;
        break;

    case 4:
        v110 = 20;
        v111 = 64;
        v114 = 28;
        a5 = yw->_guiTiles[28]->h;
        a4 = yw->_guiTiles[28]->map[1].w;
        v7 = 20;
        v99 = 16;
        break;
    }

    switch ( yw->_doAction )
    {
    case World::DOACTION_2:
        if ( !(robo_map.field_1E8 & 0x200) )
        {
            if ( yw->_guiActFlags & 0x20 )
            {
                FontUA::select_tileset(cur, v114);

                sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, 0x86, a4, a5);
            }
            else if ( yw->_guiActFlags & 0x10 && yw->_mouseGrabbed == false)
            {
                if ( yw->_cellOnMouse->owner == yw->_userRobo->_owner && yw->_cellOnMouse->IsCanSee(yw->_userRobo->_owner) )
                {
                    FontUA::select_tileset(cur, v114);

                    sub_4F6980(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, a4, a5);
                }
                else
                {
                    FontUA::select_tileset(cur, v110);

                    sub_4F6980(cur, yw->_cellOnMouse->CenterPos.x, yw->_cellOnMouse->CenterPos.z, 66, v111, v111);
                }
            }
        }
        break;

    case World::DOACTION_5:
        FontUA::select_tileset(cur, v114);

        sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, 0x85, a4, a5);
        break;

    case World::DOACTION_6:
        FontUA::select_tileset(cur, v110);

        sub_4F6980(cur, yw->_cellOnMouse->CenterPos.x, yw->_cellOnMouse->CenterPos.z, 65, v111, v111);
        break;

    case World::DOACTION_8:
        if ( !(robo_map.field_1E8 & 0x200) )
        {
            if ( yw->_bactOnMouse )
            {
                if ( yw->_bactOnMouse == yw->_userRobo )
                {
                    int v22 = yw->_guiTiles[1]->h;

                    FontUA::select_tileset(cur, 1);

                    sub_4F6980(cur, yw->_bactOnMouse->_position.x, yw->_bactOnMouse->_position.z, v7, v22, v22);
                }
                else
                {
                    NC_STACK_ypabact *v25 = sub_4D3C3C(yw->_bactOnMouse);

                    if ( v25 )
                    {
                        sub_4F7BE8(yw, cur, v25, v114, 0x85, a4, a5);
                    }

                }
            }
        }
        break;

    case World::DOACTION_10:
        FontUA::select_tileset(cur, v114);

        sub_4F6980(cur, yw->_cellMouseIsectPos.x, yw->_cellMouseIsectPos.z, 0x88, a4, a5);
        break;

    default:
        break;
    }

    {
        vec3d playerRoboTarget;
        if ( GetPlayerRoboAIBehaviorMapTarget(yw, &playerRoboTarget) )
        {
            FontUA::select_tileset(cur, v114);
            sub_4F6980(cur, playerRoboTarget.x, playerRoboTarget.z, 0x88, a4, a5);
        }
    }

    if ( bzda.field_1D0 & 0x20 )
    {
        FontUA::select_tileset(cur, 1);

        int v44 = yw->_guiTiles[1]->h;

        sub_4F6980(cur, yw->_userRobo->_position.x, yw->_userRobo->_position.z, v99, v44, v44);
    }
    else
    {
        if ( yw->_activeCmdrRemapIndex != -1 )
        {
            NC_STACK_ypabact *v47 = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];

            if ( v47 )
            {
                sub_4F7BE8(yw, cur, v47, v114, 0x84, a4, a5);

                if ( v47->_primTtype == BACT_TGT_TYPE_CELL )
                {
                    if ( v47->_primT.pcell->owner != yw->_userRobo->_owner )
                    {
                        vec2d tmp = World::SectorIDToCenterPos2( World::PositionToSectorID(v47->_primTpos) );

                        FontUA::select_tileset(cur, v110);

                        sub_4F6980(cur, tmp.x, tmp.y, 66, v111, v111);

                        FontUA::select_tileset(cur, v114);

                        sub_4F6980(cur, v47->_primTpos.x, v47->_primTpos.z, 0x88, a4, a5);
                    }
                    else
                    {
                        FontUA::select_tileset(cur, v114);

                        sub_4F6980(cur, v47->_primTpos.x, v47->_primTpos.z, 0x88, a4, a5);
                    }
                }
                else if ( v47->_primTtype == BACT_TGT_TYPE_UNIT )
                {
                    FontUA::select_tileset(cur, v114);

                    sub_4F6980(cur, v47->_primT.pbact->_position.x, v47->_primT.pbact->_position.z, 0x86, a4, a5);
                }

                if ( v47->_status_flg & BACT_STFLAG_WAYPOINT )
                {
                    if ( v47->_waypoints_count > 0 )
                    {
                        int v68;

                        if ( v47->_status_flg & BACT_STFLAG_WAYPOINTCCL )
                            v68 = 0;
                        else
                            v68 = v47->_current_waypoint;

                        for (int i = v68; i < v47->_waypoints_count; i++)
                        {
                            FontUA::select_tileset(cur, v114);

                            sub_4F6980(cur, v47->_waypoints[i].x, v47->_waypoints[i].z, 0x88, a4, a5);
                        }
                    }
                }
            }
        }
    }

    // Blinking marker of the controlled unit; the Spectator has none.
    if ( yw->_userUnit && !yw->IsSpectatorControlled() )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            FontUA::select_tileset(cur, v114);

            sub_4F6980(cur, yw->_userUnit->_position.x, yw->_userUnit->_position.z, 0x89, a4, a5);
        }
    }

    if ( yw->_lastMsgSender )
    {
        if ( yw->_timeStamp / 300 & 1 )
        {
            FontUA::select_tileset(cur, v114);

            sub_4F6980(cur, yw->_lastMsgSender->_position.x, yw->_lastMsgSender->_position.z, 0x8A, a4, a5);
        }
    }

    for ( const TMapGate &gate : yw->_levelInfo.Gates )
    {
        if ( gate.PCell->PurposeType == cellArea::PT_GATEOPENED )
        {
            int v81 = yw->sb_0x4f8f64__sub3__sub0(gate.CellId);

            int v85 = 1;

            if ( 2 * yw->_beamEnergyCapacity < 2 * v81 )
            {
                v85 = (yw->_timeStamp / 300) & 1; // FLASHING GATES TITLE
            }

            if ( v85 )
                sb_0x4f8f64__sub3__sub1(yw,  fmt::sprintf("%d / %d", 2 * v81, 2 * yw->_beamEnergyCapacity), 0x1F, cur, gate.CellId.x, gate.CellId.y);
        }
    }
}

void sb_0x4f8f64__sub0(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    // OpenNeoUA: the strategic-map controls are now drawn in the title bar.
    // Keep this legacy hook as a no-op so the map render order stays intact.
    (void)yw;
    (void)cur;
}

void sb_0x4f8f64(NC_STACK_ypaworld *yw)
{
    yw_RenderLegacyStrategicMap(yw);
    yw_RenderCustomMapMarkers(yw);
    yw_RenderMapTitleToolbar(yw);
}

void sub_4C157C(NC_STACK_ypaworld *yw)
{
    float v1 = (robo_map.h - robo_map.field_250) * robo_map.field_1E4;
    float v3 = (robo_map.w - robo_map.field_24C) * robo_map.field_1E0;

    float v6 = (yw->_mapSize.x * World::CVSectorLength);
    int v20 = robo_map.w - robo_map.field_248;

    int v17 = ((robo_map.field_1D8 - v3 * 0.5) * v20) / v6;
    int v19 = v20 * v3 / v6;

    if ( v19 >= robo_map.field_228 )
    {
        if ( v19 > v20 )
            v19 = v20;
    }
    else
    {
        v17 -= (robo_map.field_228 - v19) >> 1;
        v19 = robo_map.field_228;
    }

    if ( v17 < 0 )
        v17 = 0;

    if ( v17 + v19 > v20 )
        v17 = v20 - v19;

    robo_map.field_1CC = v20;
    robo_map.field_1CE = v17;
    robo_map.field_1D0 = v19;

    float v15 = -(robo_map.field_1DC + v1 * 0.5);
    float v14 = (yw->_mapSize.y * World::CVSectorLength);

    v20 = robo_map.h - robo_map.field_250;
    v19 = (v20 * v1 / v14);

    int v18 = ((v20 * v15) / v14);

    if ( v19 >= robo_map.field_228 )
    {
        if ( v19 > v20 )
            v19 = v20;
    }
    else
    {
        v18 -= (robo_map.field_228 - v19) >> 1;
        v19 = robo_map.field_228;
    }

    if ( v18 < 0 )
        v18 = 0;

    if ( v19 + v18 > v20 )
        v18 = v20 - v19;

    robo_map.field_1D2 = v20;
    robo_map.field_1D4 = v18;
    robo_map.field_1D6 = v19;
}

void sub_4C0C00(NC_STACK_ypaworld *yw)
{
    int v20 = robo_map.x - (yw->_screenSize.x / 2);
    int v21 = robo_map.y - (yw->_screenSize.y / 2);
    int v22 = robo_map.w;
    int v23 = robo_map.h;
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);

    const int closeW = std::max(18, yw->_fontDefCloseW);
    const int toolW = std::max(18, std::min(24, yw->_fontH));
    const int toolbarIds[] = {3, 4, 5, 6, 9, 18, 19};
    const int toolCount = (int)(sizeof(toolbarIds) / sizeof(toolbarIds[0]));
    const int toolStart = std::max(0, robo_map.w - closeW - toolCount * toolW);
    const int toolbarOffsetX = 1;
    const int toolbarOffsetY = 1;

    robo_map.buttons[0] = ButtonBox(robo_map.w - closeW + toolbarOffsetX, toolbarOffsetY,
                                    closeW, yw->_fontH);
    robo_map.buttons[1] = ButtonBox(0, 0, toolStart, yw->_fontH);
    robo_map.buttons[2] = ButtonBox();
    robo_map.buttons[7] = ButtonBox();
    robo_map.buttons[8] = ButtonBox();

    for ( int i = 0; i < toolCount; i++ )
        robo_map.buttons[toolbarIds[i]] = ButtonBox(toolStart + i * toolW + toolbarOffsetX, toolbarOffsetY,
                                                    toolW, yw->_fontH);

    robo_map.cmdCommands.clear();

    // Keep FLAG_WITH_CLOSE on the window so the existing input/close lifecycle
    // remains untouched, but suppress the legacy atlas X. A matching vector X
    // is drawn by yw_RenderMapTitleToolbar() in the exact ButtonBox[0].
    GuiBase::FormateTitle(yw, v20, v21, v22,
                          Locale::Text::WinName(Locale::WINNAME_MAP),
                          &robo_map.cmdCommands, 0,
                          robo_map.flags & ~GuiBase::FLAG_WITH_CLOSE);

    FontUA::next_line(&robo_map.cmdCommands);
    // next_line() returns to the line origin set by FormateTitle. Keep this
    // frame glyph on the map's left edge; the right scrollbar has its own
    // command buffer and remains visually trimmed there.
    FontUA::set_center_xpos(&robo_map.cmdCommands, v20);
    FontUA::reset_tileset(&robo_map.cmdCommands, 13);

    FontUA::store_u8(&robo_map.cmdCommands, 65);

    FontUA::next_line(&robo_map.cmdCommands);
    FontUA::reset_tileset(&robo_map.cmdCommands, 12);

    // The visible horizontal scrollbar starts horizontalTrim pixels below
    // its logical track. Extend only the left frame's final vertical segment
    // by the same amount so the lower-left corner remains connected.
    int v13 = v23 - robo_map.field_250 - 1 + horizontalTrim;
    while (v13 > yw->_fontVScrollH)
    {
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);

        v13 -= yw->_fontVScrollH;
    }

    if ( v13 > 1 )
    {
        FontUA::set_yheight(&robo_map.cmdCommands, v13 - 1);
        FontUA::store_u8(&robo_map.cmdCommands, 65);
        FontUA::next_line(&robo_map.cmdCommands);
    }

    FontUA::reset_tileset(&robo_map.cmdCommands, 13);
    FontUA::store_u8(&robo_map.cmdCommands, 66);
    FontUA::next_line(&robo_map.cmdCommands);

    FontUA::include(&robo_map.cmdCommands, 0);
    FontUA::include(&robo_map.cmdCommands, 1);
    FontUA::include(&robo_map.cmdCommands, 2);
    FontUA::set_end(&robo_map.cmdCommands);
}

void sub_4C0FEC(NC_STACK_ypaworld *yw)
{
    int v1 = robo_map.x - (yw->_screenSize.x / 2);
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);
    const int verticalTrim = std::max(0, robo_map.field_248 - kRoboMapScrollbarVisualSize);
    int v3 = robo_map.y + robo_map.h - robo_map.field_240 + horizontalTrim
             - (yw->_screenSize.y / 2);

    robo_map.buttons[13] = ButtonBox(0, robo_map.h - robo_map.field_240,
                                     robo_map.field_1CE, robo_map.field_240);

    robo_map.buttons[14] = ButtonBox(robo_map.field_1CE, robo_map.h - robo_map.field_240,
                                     robo_map.field_1D0, robo_map.field_240);

    robo_map.buttons[16] = ButtonBox(robo_map.w - robo_map.field_248,  robo_map.h - robo_map.field_240,
                                     robo_map.field_248,  robo_map.field_240);

    robo_map.buttons[15] = ButtonBox(robo_map.field_1D0 + robo_map.field_1CE,   robo_map.h - robo_map.field_240,
                                     robo_map.field_1CC - robo_map.field_1CE - robo_map.field_1D0,   robo_map.field_240);

    robo_map.t1_cmdbuf_2.clear();

    FontUA::select_tileset(&robo_map.t1_cmdbuf_2, 11);
    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_2, v1);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_2, v3);

    int v4 = robo_map.field_1CE;

    if ( v4 > 0 )
    {
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 65, horizontalTrim);
        if ( v4 > 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, v4);
            yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 66, horizontalTrim);
        }
    }

    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 68, horizontalTrim);

    FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.field_1D0 + v4 - 1);

    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 69, horizontalTrim);
    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 70, horizontalTrim);

    int v11 = v4 + robo_map.field_1D0;
    if ( v11 < robo_map.field_1CC )
    {
        if ( v11 < robo_map.field_1CC - 1 )
        {
            FontUA::op17(&robo_map.t1_cmdbuf_2, robo_map.field_1CC - 1);

            yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 66, horizontalTrim);
        }
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 67, horizontalTrim);
    }

    if ( verticalTrim > 0 )
    {
        // Keep the resize cube trimmed, but compensate half of the removed
        // scrollbar width so it joins the two bars without overshooting.
        FontUA::op17(&robo_map.t1_cmdbuf_2,
                     robo_map.field_1CC + verticalTrim - 1);
        yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 67, horizontalTrim);

        FontUA::add_xpos(&robo_map.t1_cmdbuf_2,
                         std::max(0, verticalTrim / 2 - 1));
        FontUA::set_xoff(&robo_map.t1_cmdbuf_2, (uint8_t)verticalTrim);
    }
    yw_RoboMapStoreTrimmedHorizontalGlyph(&robo_map.t1_cmdbuf_2, 71, horizontalTrim);

    FontUA::set_end(&robo_map.t1_cmdbuf_2);
}

void sub_4C1214(NC_STACK_ypaworld *yw)
{
    const int horizontalTrim = std::max(0, robo_map.field_240 - kRoboMapScrollbarVisualSize);
    const int verticalTrim = std::max(0, robo_map.field_248 - kRoboMapScrollbarVisualSize);
    int v2 = robo_map.x + robo_map.w - robo_map.field_248 + verticalTrim
             - (yw->_screenSize.x / 2);
    int v3 = robo_map.y + robo_map.field_23C - (yw->_screenSize.y / 2);

    robo_map.buttons[10] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D4);

    robo_map.buttons[11] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_1D4 + robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D6);

    robo_map.buttons[12] = ButtonBox(robo_map.w - robo_map.field_248,    robo_map.field_1D6 + robo_map.field_1D4 + robo_map.field_23C,
                                     robo_map.field_248,   robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6);

    robo_map.t1_cmdbuf_1.clear();

    FontUA::set_center_xpos(&robo_map.t1_cmdbuf_1, v2);
    FontUA::set_center_ypos(&robo_map.t1_cmdbuf_1, v3);

    if ( robo_map.field_1D4 > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        int v9 = robo_map.field_1D4 - 1;
        if ( robo_map.field_1D6 <= 0
                && robo_map.field_1D2 - robo_map.field_1D4 <= 0 )
        {
            // If the scrollbar has no knob or lower track, this is the
            // final vertical segment. Extend it to the trimmed bottom bar.
            v9 += horizontalTrim;
        }

        while (v9 >= yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v9 -= yw->_fontVScrollH;
        }

        if ( v9 > 0 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v9);
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
    }

    if ( robo_map.field_1D6 > 0 )
    {
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);
        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 69, verticalTrim);

        FontUA::next_line(&robo_map.t1_cmdbuf_1);

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        int v14 = robo_map.field_1D6 - 1;
        if ( robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6 <= 0 )
            v14 += horizontalTrim;

        while ( v14 > yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v14 -= yw->_fontVScrollH;
        }

        if ( v14 > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v14 - 1);
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 67, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }

        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 70, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }

    int v21 = robo_map.field_1D2 - robo_map.field_1D4 - robo_map.field_1D6;

    if ( v21 > 0 )
    {
        // When the lower scrollbar track is visible, continue it to the
        // shifted horizontal scrollbar instead of leaving a lower-right gap.
        v21 += horizontalTrim;
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 12);

        while ( v21 > yw->_fontVScrollH )
        {
            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);

            v21 -= yw->_fontVScrollH;
        }

        if ( v21 > 1 )
        {
            FontUA::set_yheight(&robo_map.t1_cmdbuf_1, v21 - 1);

            yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 66, verticalTrim);
            FontUA::next_line(&robo_map.t1_cmdbuf_1);
        }
        FontUA::reset_tileset(&robo_map.t1_cmdbuf_1, 13);

        yw_RoboMapStoreTrimmedVerticalGlyph(&robo_map.t1_cmdbuf_1, 68, verticalTrim);
        FontUA::next_line(&robo_map.t1_cmdbuf_1);
    }
    FontUA::set_end(&robo_map.t1_cmdbuf_1);
}

void sub_4F6114(NC_STACK_ypaworld *yw, int a2, CmdStream *cmdbuf, int a3, int a4, int a5, int a6, int a7, mapFunc ffunc, int xpos, int ypos)
{
    int v33;

    if ( a2 == 4)
        v33 = 2;
    else if ( a2 == 8)
        v33 = 3;
    else if ( a2 == 16 )
        v33 = 4;
    else if ( a2 == 32 )
        v33 = 5;
    else if ( a2 == 64 )
        v33 = 6;
    else
    {
        ypa_log_out("yw_maprnd.c/yw_RMapLego(): wrong size\n");

        FontUA::set_end(cmdbuf);
        return ;
    }

    FontUA::set_center_xpos(cmdbuf, xpos);
    FontUA::set_center_ypos(cmdbuf, ypos);

    int msk = a2 - 1;

    FontUA::reset_tileset(cmdbuf, a7);

    int row = a4;

    while (row < a6)
    {
        int v37 = 0;
        int col = a3;
        int v17 = row >> v33;

        if ( msk & row )
        {
            FontUA::set_yoff(cmdbuf, row & msk);

            row += a2 - (msk & row);
        }
        else
        {
            if ( a6 - row >= a2 )
            {
                row += a2;
            }
            else
            {
                FontUA::set_yheight(cmdbuf, a6 - row);

                row = a6;
            }
        }

        if ( col & msk )
        {
            int v37_4 = msk & col;

            int v21 = ffunc(yw, col >> v33, v17) & 0xFF;

            if ( v21 )
            {
                FontUA::set_xoff(cmdbuf, v37_4);
                FontUA::store_u8(cmdbuf, v21);

                v37 = 0;
            }
            else
            {
                v37 = a2 - v37_4;

                FontUA::add_xpos(cmdbuf, v37);
            }

            col += a2 - v37_4;
        }

        while ( col < a5 - a2 )
        {
            int v24 = ffunc(yw, col >> v33, v17) & 0xFF;

            if ( v24 )
            {
                FontUA::store_u8(cmdbuf, v24);

                v37 = 0;
            }
            else
            {
                if ( v37 <= 0 )
                {
                    v37 = a2;

                    FontUA::add_xpos(cmdbuf, v37);
                }
                else
                {
                    v37 += a2;

                    cmdbuf->pop_back();
                    cmdbuf->pop_back();
                    FontUA::store_s16(cmdbuf, v37);
                }
            }

            col += a2;
        }

        if ( a5 - col > 0 )
        {
            int v28 = ffunc(yw, col >> v33, v17) & 0xFF;
            if ( v28 )
            {
                FontUA::set_xwidth(cmdbuf, a5 - col);
                FontUA::store_u8(cmdbuf, v28);
            }
        }
        FontUA::next_line(cmdbuf);

    }
}

int sub_4F6048(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        cellArea &v4 = yw->_cells(x, y);

        if ( robo_map.MapViewMask & v4.view_mask )
        {
            if ( !v4.owner )
                return 8;
            else
                return v4.owner;
        }
    }

    return 0;
}

int sub_4F5FE0(NC_STACK_ypaworld *yw, int x, int y)
{
    if ( yw->IsSector( {x, y} ) )
    {
        cellArea &v6 = yw->_cells(x, y);

        if ( robo_map.MapViewMask & v6.view_mask )
            return yw->_secTypeArray[v6.type_id].GUIElementID;
    }

    return 0;
}

int sub_4F5CEC(NC_STACK_ypaworld *yw, int x, int y)
{
    int v9 = x / 4;
    int v8 = x & 3;
    int v7 = y / 4;
    int v27 = y & 3;

    if ( !yw->IsSector( {v9, v7} ) )
        return 0;

    cellArea &v12 = yw->_cells(v9, v7);

    if ( !(robo_map.MapViewMask & v12.view_mask) )
        return 0;

    if ( v8 && v27 )
    {
        if ( v12.SectorType == 1 )
        {
            TLego *v26 = &yw->_legoArray[ yw->GetLegoBld(&v12, 0, 0) ];
            int v25 = (16 * (v27 - 1) + v8 - 1 + v26->GUIElementID) & 0xFF;
            return v25;
        }
        else
        {
            TLego *v26 = &yw->_legoArray[ yw->GetLegoBld(&v12, v8 - 1, 2 - (v27 - 1))  ];
            return v26->GUIElementID;
        }
    }
    else
    {
        // OpenNeoUA: hide only the strategic-map grid/elevation-boundary layer.
        // LEGO terrain and building glyphs are returned by the branch above.
        return 0;
    }
}

void sb_0x4f6650__sub0(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int xpos, int ypos, int w, int h)
{
    float v7 = robo_map.field_1D8 / robo_map.field_1E0;
    float v8 = -robo_map.field_1DC / robo_map.field_1E4;
    int v9 = v7 - (w / 2);
    int v23 = w + v9;
    int v10 = v8 - (h / 2);
    int v22 = h + v10;

    if ( robo_map.field_1EC & 2 )
        sub_4F6114(yw, 4 << robo_map.field_1EE, cmdbuf, v9, v10, v23, v22, robo_map.field_1EE + 50, sub_4F6048, xpos, ypos);

    if ( robo_map.field_1EC & 1 )
    {
        switch ( robo_map.field_1EE )
        {
        case 0:
            sub_4F6114(yw, 4, cmdbuf, v9, v10, v23, v22, 43, sub_4F5FE0, xpos, ypos);
            break;

        case 1:
            sub_4F6114(yw, 8, cmdbuf, v9, v10, v23, v22, 44, sub_4F5FE0, xpos, ypos);
            break;

        case 2:
            sub_4F6114(yw, 4, cmdbuf, v9 + 2, v10 + 2, v23 + 2, v22 + 2, 42, sub_4F5CEC, xpos, ypos);
            break;

        case 3:
            sub_4F6114(yw, 8, cmdbuf, v9 + 4, v10 + 4, v23 + 4, v22 + 4, 41, sub_4F5CEC, xpos, ypos);
            break;

        case 4:
            sub_4F6114(yw, 16, cmdbuf, v9 + 8, v10 + 8, v23 + 8, v22 + 8, 40, sub_4F5CEC, xpos, ypos);
            break;
        }
    }
}

void sb_0x4f6650(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int x, int y)
{
    sb_0x4f6650__sub0(yw, cmdbuf, x, y, robo_map.w - robo_map.field_24C, robo_map.h - robo_map.field_250);
    FontUA::set_end(cmdbuf);
}

void sub_4C1814(NC_STACK_ypaworld *yw, int a2, int a3)
{
    float v15 = yw->_mapSize.x * World::CVSectorLength;
    float v12 = yw->_mapSize.y * World::CVSectorLength;

    float v4 = a2 * robo_map.field_1E0;
    float v13 = v4 * 0.5;

    if ( v4 <= v15 )
    {
        if ( robo_map.field_1D8 - v13 >= 0.0 )
        {
            if ( robo_map.field_1D8 + v13 > v15 )
                robo_map.field_1D8 = v15 - v13;
        }
        else
        {
            robo_map.field_1D8 = v13;
        }
    }
    else
    {
        robo_map.field_1D8 = v15 * 0.5;
    }

    float v8 = a3 * robo_map.field_1E4;
    float v14 = v8 * 0.5;

    if ( v8 > v12 )
    {
        robo_map.field_1DC = -v12 * 0.5;
    }
    else
    {
        if ( robo_map.field_1DC + v14 > 0.0 )
        {
            robo_map.field_1DC = -v8 * 0.5;
        }
        else
        {
            if ( robo_map.field_1DC - v14 < -v12 )
                robo_map.field_1DC = -v12 + v14;
        }
    }
}

void  sb_0x451034__sub2(NC_STACK_ypaworld *yw)
{
    robo_map.t1_cmdbuf_1.reserve(256);
    robo_map.t1_cmdbuf_2.reserve(256);
    robo_map.t1_cmdbuf_3.reserve(32768);

    robo_map.flags = (GuiBase::FLAG_CLOSED | GuiBase::FLAG_WITH_CLOSE | GuiBase::FLAG_WITH_DRAGBAR);
    robo_map.field_228 = 8;
    robo_map.field_22C = yw->_guiTiles[10]->map[65].w;
    robo_map.field_230 = yw->_guiTiles[10]->h;
    robo_map.field_234 = 4;
    robo_map.field_238 = yw->_guiTiles[0]->map[113].w;
    robo_map.field_23C = yw->_fontH;
    robo_map.field_240 = yw->_fontHScrollH;
    robo_map.field_244 = yw->_guiTiles[12]->map[65].w;
    robo_map.field_248 = yw->_fontVBScrollW;
    robo_map.field_24C = robo_map.field_248 + robo_map.field_244;
    robo_map.field_250 = robo_map.field_240 + robo_map.field_23C;
    robo_map.field_210 = robo_map.field_248 + 7 * robo_map.field_22C + robo_map.field_244 + 8;
    robo_map.field_212 = robo_map.field_240 + robo_map.field_23C + 96;
    robo_map.field_214 = 32000;
    robo_map.field_216 = 32000;
    robo_map.x = 0;
    robo_map.y = yw->_upScreenBorder;
    robo_map.w = 2 * yw->_screenSize.x / 3;
    robo_map.h = 2 * yw->_screenSize.y / 3;
    robo_map.field_1EE = 4;
    robo_map.field_1EC = 7;
    robo_map.buttons.resize(20);

    robo_map.cmdCommands.reserve(512);
    robo_map.cmdInclude = {&robo_map.t1_cmdbuf_1, &robo_map.t1_cmdbuf_2, &robo_map.t1_cmdbuf_3};

    robo_map.preDraw = yw_RenderMapBackground;
    robo_map.postDraw = sb_0x4f8f64;

    if ( yw->_userRobo )
    {
        robo_map.field_1D8 = yw->_userRobo->_position.x;
        robo_map.field_1DC = yw->_userRobo->_position.z;
    }
    else
    {
        robo_map.field_1DC = -39000.0;
        robo_map.field_1D8 = 39000.0;
    }

    robo_map.field_1E4 = 18.75f;
    robo_map.field_1E0 = 18.75f;

    robo_map.field_1ED = 0;
    robo_map.field_1E8 = 0;
    robo_map.markerMode = true;
    robo_map.markerDragIndex = -1;
    robo_map.zoomInFlashUntil = 0;
    robo_map.zoomOutFlashUntil = 0;
    robo_map.maximized = false;
    robo_map.restoreRectValid = false;
    robo_map.customMarkers.clear();
    robo_map.MapViewMask = cellArea::ViewMask(1);

    robo_map.field_254 = 0.45;
    robo_map.field_258 = 0;
    robo_map.field_25C = 0.95;
    robo_map.field_260 = 0.6;

    robo_map.field_208 = yw->_screenSize.x - robo_map.field_210;
    robo_map.field_20A = yw->_screenSize.y - yw->_downScreenBorder - robo_map.field_212;
    robo_map.field_20C = robo_map.field_210;
    robo_map.field_20E = robo_map.field_212;

    sub_4C157C(yw);
    sub_4C0C00(yw);
    sub_4C0FEC(yw);
    sub_4C1214(yw);
    MapRoboUpdateMapInt(yw);
    sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
}

void sub_4C32A8(NC_STACK_ypaworld *yw, TMFWinStatus *status)
{
    if ( status->Valid )
    {
        if ( status->Rect.y < yw->_upScreenBorder
                || status->Rect.x + status->Rect.w > yw->_screenSize.x
                || status->Rect.y + status->Rect.h > yw->_screenSize.y - yw->_downScreenBorder )
        {
            status->Valid = false;
        }
    }

    if ( status->Valid )
    {
        if ( yw->_roboMapStatus.Data[4] < yw->_upScreenBorder
                || yw->_roboMapStatus.Data[3] + yw->_roboMapStatus.Data[5] > yw->_screenSize.x
                || yw->_roboMapStatus.Data[6] + yw->_roboMapStatus.Data[4] > yw->_screenSize.y - yw->_downScreenBorder )
            status->Valid = false;
    }
}

void sub_4C1970(NC_STACK_ypaworld *yw, int a2)
{
    if ( a2 == 1 )
        robo_map.field_1EE++;
    else if ( a2 == 2 )
        robo_map.field_1EE--;

    if ( robo_map.field_1EE > 4 )
        robo_map.field_1EE = 4;
    else if ( robo_map.field_1EE < 1 )
        robo_map.field_1EE = 1;

    // The tactical map uses one live zoom across Host Station/vehicle contexts.
    // Keep both saved UI states aligned so save/load never revives a stale zoom.
    if ( yw )
    {
        yw->_roboMapStatus.Data[2] = robo_map.field_1EE;
        yw->_vhclMapStatus.Data[2] = robo_map.field_1EE;
    }

    switch ( robo_map.field_1EE )
    {
    case 0:
        robo_map.field_1E4 = 300.0f;
        robo_map.field_1E0 = 300.0f;
        break;

    case 1:
        robo_map.field_1E4 = 150.0f;
        robo_map.field_1E0 = 150.0f;
        break;

    case 2:
        robo_map.field_1E4 = 75.0f;
        robo_map.field_1E0 = 75.0f;
        break;

    case 3:
        robo_map.field_1E4 = 37.5f;
        robo_map.field_1E0 = 37.5f;
        break;

    case 4:
        robo_map.field_1E4 = 18.75f;
        robo_map.field_1E0 = 18.75f;
        break;
    }
}

int sb_0x451034__sub3(NC_STACK_ypaworld *yw)
{
    bzda = bzd();

    int v3 = yw->_screenSize.x - 11 * yw->_iconOrderW;
    bzda.field_8FC = v3 / 4;

    // OpenNeoUA default: keep the Genesis/Creation panel compact and anchored to
    // the left edge. nucleus.ini can still override all three geometry values.
    int configuredGenesisWidth = System::IniConf::UiGenesisListWidth.Get<int32_t>();
    if ( configuredGenesisWidth <= 0 )
        configuredGenesisWidth = System::IniConf::UiGenesisListDefaultWidth;
    bzda.field_900 = std::min(yw->_screenSize.x, std::max(60, configuredGenesisWidth));

    int configuredGenesisX = System::IniConf::UiGenesisListX.Get<int32_t>();
    if ( configuredGenesisX < 0 )
        configuredGenesisX = System::IniConf::UiGenesisListDefaultX;
    bzda.field_904 = std::min(configuredGenesisX, std::max(0, yw->_screenSize.x - bzda.field_900));

    // Preserve the legacy spacing inside the non-Genesis toolbar block, then
    // shift that whole block as one unit so its final Analyzer/Menu pair is
    // flush with the right edge. Render positions and click boxes share these
    // same absolute anchors below, avoiding the historical desync caused by
    // moving only the Genesis/create origin.
    bzda.field_908 = yw->_iconOrderW + bzda.field_8FC + bzda.field_8FC / 2;
    bzda.field_90C = 2 * yw->_iconOrderW + bzda.field_908 + bzda.field_8FC;
    bzda.field_910 = 5 * yw->_iconOrderW + bzda.field_90C + bzda.field_8FC;

    const int toolbarRight = bzda.field_910 + 2 * yw->_iconOrderW;
    const int toolbarShiftRight = std::max(0, yw->_screenSize.x - toolbarRight);
    bzda.field_908 += toolbarShiftRight;
    bzda.field_90C += toolbarShiftRight;
    bzda.field_910 += toolbarShiftRight;
    bzda.field_914 = 1;
    bzda.field_918 = 0;
    bzda.flags = 0;
    bzda.buttons.resize(11);

    bzda.x = 0;
    bzda.y = yw->_screenSize.y - yw->_iconOrderH;
    bzda.w = yw->_screenSize.x;
    bzda.h = yw->_iconOrderH;

    for (int i = 0; i < 11; i++)
        bzda.buttons[i] = ButtonBox();

    bzda.cmdCommands.reserve(1088);
    bzda.field_1D8 = 0;

    sub_4C3A54(yw);

    bzda.field_8EC = 0;
    bzda.field_8F0 = 0;
    bzda.field_8F4 = 0;
    bzda.field_8F8 = 0;

    if ( bzda.field_1D4 & 1 )
    {
        if ( !(bzda.field_1D8 & 1) )
        {
            bzda.field_1D8 |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }
    }

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = 8;
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 16;

    int genesisListHeight = System::IniConf::UiGenesisListHeight.Get<int32_t>();
    if ( genesisListHeight <= 0 )
        genesisListHeight = System::IniConf::UiGenesisListDefaultHeight;

    const int usableHeight = std::max(1, genesisListHeight - 2 * yw->_fontBorderH);
    const int configuredRows = std::max(1, usableHeight / std::max(1, yw->_fontH));
    args.maxShownEntries = std::min(configuredRows, 64);
    args.shownEntries = std::min(args.shownEntries, args.maxShownEntries);

    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = bzda.field_900;
    args.maxEntryWidth = std::max(1024, yw->_screenSize.x);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;
    args.thinScrollbar = true;
    args.fillThinScrollbarGap = true;
    args.wheelScroll = true;
    // Genesis background opacity is data-driven. The OpenNeoUA runtime default is 220
    // when the nucleus.ini key is absent; explicit values still override it.
    int genesisListOpacity = System::IniConf::UiGenesisListOpacity.Get<int32_t>();
    genesisListOpacity = std::max(0, std::min(255, genesisListOpacity));
    args.backgroundOpacity = static_cast<uint8_t>(genesisListOpacity);

    if ( !gui_lstvw.Init(yw, args) )
        return 0;

    sub_4C32A8(yw, &yw->_roboMapStatus);
    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_roboFinderStatus.Rect.x + yw->_roboFinderStatus.Rect.w > yw->_screenSize.x
                || yw->_roboFinderStatus.Rect.h + yw->_roboFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_roboFinderStatus.Valid = false;
        }
    }

    sub_4C32A8(yw, &yw->_vhclMapStatus);
    if ( yw->_vhclFinderStatus.Valid )
    {
        if ( yw->_vhclFinderStatus.Rect.y < yw->_upScreenBorder
                || yw->_vhclFinderStatus.Rect.w + yw->_vhclFinderStatus.Rect.x > yw->_screenSize.x
                || yw->_vhclFinderStatus.Rect.h + yw->_vhclFinderStatus.Rect.y > yw->_screenSize.y - yw->_downScreenBorder )
        {
            yw->_vhclFinderStatus.Valid = false;
        }
    }

    if ( !yw->_roboMapStatus.Data[5] || !yw->_roboMapStatus.Data[6] )
        yw->_roboMapStatus.Valid = false;

    if ( !yw->_vhclMapStatus.Data[5] || !yw->_vhclMapStatus.Data[6] )
        yw->_vhclMapStatus.Valid = false;

    if ( yw->_roboMapStatus.Valid )
    {
        if ( yw->_roboMapStatus.IsOpen )
        {
            yw->GuiWinOpen(&robo_map);
            yw->GuiWinToFront(&robo_map);
        }
        else
            yw->GuiWinClose(&robo_map);

        robo_map.x = yw->_roboMapStatus.Rect.x;
        robo_map.y = yw->_roboMapStatus.Rect.y;
        robo_map.w = yw->_roboMapStatus.Rect.w;
        robo_map.h = yw->_roboMapStatus.Rect.h;
        robo_map.field_1EC = yw->_roboMapStatus.Data[0];
        robo_map.field_1ED = yw->_roboMapStatus.Data[1];
        robo_map.field_1EE = std::max(0, std::min((int)yw->_roboMapStatus.Data[2], 4)); // Zoom levels 0..4.
        robo_map.field_208 = yw->_roboMapStatus.Data[3];
        robo_map.field_20A = yw->_roboMapStatus.Data[4];
        robo_map.field_20C = yw->_roboMapStatus.Data[5];
        robo_map.field_20E = yw->_roboMapStatus.Data[6];

        sub_4C1970(yw, 0);
    }

    if ( yw->_roboFinderStatus.Valid )
    {
        if ( yw->_roboFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
        {
            yw->GuiWinOpen(&squadron_manager);
            yw->GuiWinToFront(&squadron_manager);
        }
        else
            yw->GuiWinClose(&squadron_manager);

        yw_ApplySquadronManagerSavedRect(yw, yw->_roboFinderStatus.Rect);
    }

    return 1;
}
