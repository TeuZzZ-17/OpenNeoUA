// GUI panels: buy (Genesis) list, creation buttons, player panel, tools,
// the finder window and the energy bars of the top panel.
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

void buy_list_update_sub(NC_STACK_ypaworld *yw, int a2, GuiList *lstvw, CmdStream *cur,
                         uint32_t a5, const std::string &a6, int a7, int remaining)
{
    const int scrollbarGap = yw_GetGuiListScrollbarGapFillWidth(lstvw, yw);
    int v33 = lstvw->entryWidth - 2 * yw->_fontBorderW + scrollbarGap;

    FontUA::ColumnItem v24[3];

    int v14;
    int v15;
    int v16;
    int v17;

    if ( a2 )
    {
        v14 = 9;
        v15 = 98;
        v16 = 99;
        v17 = 100;
    }
    else
    {
        v14 = 0;
        v16 = 102;
        v15 = 102;
        v17 = 102;
    }

    // A capped Genesis entry at zero remains selectable, but its text uses a
    // fixed neutral disabled gray for every faction. Keep it bright enough to
    // remain readable over both the normal and selected row backgrounds.
    if ( remaining == 0 )
        FontUA::set_txtColor(cur, 160, 160, 160);
    else if ( a2 )
        FontUA::set_txtColor(cur, yw->_iniColors[62].r, yw->_iniColors[62].g, yw->_iniColors[62].b);
    else
        FontUA::set_txtColor(cur, yw->_iniColors[61].r, yw->_iniColors[61].g, yw->_iniColors[61].b);

    const std::string priceText = fmt::sprintf("%d", a7);

    // Keep the price block state-independent. Vanilla used five digit cells from
    // the active row tileset, so simply moving the selection changed the width
    // left for the name. Reserve only what the current price really needs, using
    // the larger requirement of the normal and selected row assets.
    const int renderedPriceWidth = yw_GetGenesisScreenTextWidth(yw, priceText);
    const int normalPricePadding = std::max(2, yw_GetGenesisScreenTextWidth(yw, "0") / 4);
    const int selectedPricePadding = normalPricePadding;
    const int normalPriceWidth = renderedPriceWidth
                               + yw->_guiTiles[0]->map[102].w
                               + normalPricePadding;
    const int selectedPriceWidth = renderedPriceWidth
                                 + yw->_guiTiles[9]->map[100].w
                                 + selectedPricePadding;
    const int effectivePriceColumnWidth = std::max(normalPriceWidth, selectedPriceWidth);

    v24[0].txt.clear();
    v24[0].fontID = 28;
    v24[0].width = squadron_manager.field_2CC;
    v24[0].prefixChar = 0;
    v24[0].postfixChar = 0;
    v24[0].spaceChar = 64;
    v24[0].flags = 4;

    v24[1].fontID = v14;
    v24[1].spaceChar = v16;
    v24[1].prefixChar = v15;
    v24[1].flags = 37;
    v24[1].width = v33 - squadron_manager.field_2CC - effectivePriceColumnWidth;
    v24[1].postfixChar = 0;

    const int normalNameTextWidth = v24[1].width - yw->_guiTiles[0]->map[102].w;
    const int selectedNameTextWidth = v24[1].width - yw->_guiTiles[9]->map[98].w;
    v24[1].txt = yw_FormatGenesisVehicleName(yw, normalNameTextWidth,
                                             selectedNameTextWidth, a6, remaining);

    v24[2].txt = priceText;
    v24[2].fontID = v14;
    v24[2].spaceChar = v16;
    v24[2].postfixChar = v17;
    v24[2].width = effectivePriceColumnWidth;
    v24[2].flags = 42;
    v24[2].prefixChar = 0;

    const uint8_t backgroundOpacity = lstvw->backgroundOpacity;
    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, v14);
    FontUA::op10(cur, squadron_manager.field_2CC);
    FontUA::store_u8(cur, v16);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    FontUA::add_xpos(cur, -squadron_manager.field_2CC);

    // The Genesis opacity controls the row background only. Vehicle/building
    // icons, names and prices remain at their original opacity.
    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);
    FontUA::select_tileset(cur, 28);
    FontUA::glyph_cell(yw->_guiTiles[28], cur, a5, squadron_manager.field_2CC);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, backgroundOpacity);

    FormateColumnItem(yw, cur, 2, &v24[1]);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);
    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 125);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);

    FontUA::next_line(cur);
}

float NC_STACK_ypaworld::sub_4498F4()
{
    float v3 = 1.0;

    if ( _isNetGame )
    {
        if ( _levelUnitLimitType == 2 )
        {
            int v1 = _countUnitsPerOwner[_userRobo->_owner] - _levelUnitLimit;
            if ( v1 > 0 )
                v3 = (_levelUnitLimitArg * v1) * 0.01 + 1.0;
        }
    }

    return v3;
}

void gui_update_create_btn__sub0(NC_STACK_ypaworld *yw)
{
    gui_lstvw.itemBlock.clear();
    Input::Engine.RemClickBox(&gui_lstvw);
    Input::Engine.AddClickBoxFront(&gui_lstvw);

    gui_lstvw.numEntries = bzda.field_8E8;
    gui_lstvw.selectedEntry = bzda.field_8F8;

    if ( gui_lstvw.maxShownEntries >= bzda.field_8E8 )
        gui_lstvw.shownEntries = bzda.field_8E8;
    else
        gui_lstvw.shownEntries = gui_lstvw.maxShownEntries;

    if ( gui_lstvw.firstShownEntries + gui_lstvw.shownEntries > gui_lstvw.numEntries )
        gui_lstvw.firstShownEntries = gui_lstvw.numEntries - gui_lstvw.shownEntries;

    gui_lstvw.SetRect(yw, -2, -2);

    const int desiredGenesisX = bzda.field_904 + bzda.x;
    gui_lstvw.x = std::max(0, std::min(desiredGenesisX,
                                      std::max(0, yw->_screenSize.x - gui_lstvw.w)));
    gui_lstvw.y = bzda.field_918 + bzda.y - gui_lstvw.h;

    gui_lstvw.ItemsPreLayout(yw, &gui_lstvw.itemBlock, 0, "uvw");

    for (int i = 0; i < gui_lstvw.shownEntries; i++ )
    {
        int v21 = 0;
        int v3 = i + gui_lstvw.firstShownEntries;

        if ( bzda.field_4DC[ v3 ].i == 1 )
        {
            int v5 = bzda.field_4DC[ v3 ].d;

            const World::TVhclProto &proto = yw->_vhclProtos[v5];

            int v17 = dround(yw->sub_4498F4() * 2 * proto.GetProductionCost() / 100.0);

            const std::string v8 = yw->ResolveGameplayVehicleName(proto);
            const int remaining = yw->_userRobo
                ? yw->GetVehicleProductionLimitRemaining(yw->_userRobo->_owner, v5)
                : -1;

            if ( v3 == gui_lstvw.selectedEntry )
                v21 = 1;

            buy_list_update_sub(yw, v21, &gui_lstvw, &gui_lstvw.itemBlock,
                                proto.type_icon, v8, v17, remaining);
        }
        else if ( bzda.field_4DC[ v3 ].i == 2 )
        {
            int v9 = bzda.field_4DC[ v3 ].d;
            const World::TBuildingProto &v10 = yw->_buildProtos[v9];

            int v18 = dround(yw->sub_4498F4() * v10.GetProductionCost() / 100.0);

            std::string v13 = yw->ResolveGameplayBuildingName(v10, yw->_isNetGame);

            if ( v3 == gui_lstvw.selectedEntry )
                v21 = 1;

            buy_list_update_sub(yw, v21, &gui_lstvw, &gui_lstvw.itemBlock,
                                v10.TypeIcon, v13, v18, -1);
        }
    }

    gui_lstvw.ItemsPostLayout(yw, &gui_lstvw.itemBlock, 0, "xyz");

    FontUA::set_end(&gui_lstvw.itemBlock);
}

void gui_update_create_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->_userUnit->_status == BACT_STATUS_DEAD || yw->IsSpectatorControlled() )
    {
        bzda.buttons[0] = ButtonBox(); //create_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 67);

        yw->GuiWinClose( &gui_lstvw );
    }
    else
    {
        bzda.buttons[0] = ButtonBox(bzda.field_904, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);

        if ( !(bzda.field_1CC & 0x16) )
        {
            bzda.buttons[0] = ButtonBox(); //create_btn

            FontUA::select_tileset(cur, 23);
            FontUA::store_u8(cur, 67);
        }
        else
        {
            const bool createPressed = (bzda.field_1D0 & 0x16) != 0;
            if ( createPressed )
            {
                if ( gui_lstvw.IsOpen() )
                    gui_update_create_btn__sub0(yw);

                FontUA::select_tileset(cur, 22);
            }
            else
            {
                FontUA::select_tileset(cur, 21);
            }

            int v9 = bzda.field_1D0 & 0x16;

            if (v9 == 0x10)
                FontUA::store_u8(cur, 70);
            else if (v9 == 4)
                FontUA::store_u8(cur, 68);
            else if (v9 == 2)
                FontUA::store_u8(cur, 67);
            else
                FontUA::store_u8(cur, 67);
        }
    }
}

void gui_update_map_squad_btn(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->IsSpectatorControlled() )
    {
        int v8 = bzda.field_908;

        bzda.buttons[2] = ButtonBox(v8, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);
        v8 += yw->_iconOrderW;
        bzda.buttons[3] = ButtonBox();

        if ( robo_map.IsClosed() )
            FontUA::select_tileset(cur, 21);
        else
            FontUA::select_tileset(cur, 22);

        FontUA::store_u8(cur, 72);

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 73);

        yw->GuiWinClose( &squadron_manager );
        return;
    }

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        FontUA::select_tileset(cur, 23);

        FontUA::store_u8(cur, 72);
        FontUA::store_u8(cur, 73);

        bzda.buttons[2] = ButtonBox(); //map_btn
        bzda.buttons[3] = ButtonBox(); //squad_btn

        yw->GuiWinClose( &robo_map );
        yw->GuiWinClose( &squadron_manager );
        yw->GuiWinClose( &info_log );
    }
    else
    {
        int v8 = bzda.field_908;

        for (int i = 2; i < 4; i++)
        {
            bzda.buttons[i] = ButtonBox(v8, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH);

            v8 += yw->_iconOrderW;
        }

        const bool mapPressed = robo_map.IsOpen();
        FontUA::select_tileset(cur, mapPressed ? 22 : 21);
        FontUA::store_u8(cur, 72);

        const bool squadPressed = squadron_manager.IsOpen();
        FontUA::select_tileset(cur, squadPressed ? 22 : 21);
        FontUA::store_u8(cur, 73);
    }
}

void gui_update_player_panel(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    int v35 = 0;
    bool spectatorControlled = yw->IsSpectatorControlled();

    if ( yw->_userRobo == yw->_userUnit )
    {
        if ( !yw->_cmdrsRemap.empty() )
            v35 = 0x80;

        if ( bzda.field_8DC )
            v35 |= 0x40;
    }
    else if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        v35 = 0x90;
    }
    else if ( yw->_playerInHSGun )
    {
        v35 = 0x90;

        if ( bzda.field_8DC > 1 )
            v35 = 0xD0;
    }
    else if ( yw->_userUnit->IsParentMyRobo() )
    {
        v35 = 0x10;

        if ( yw->_userUnit->_bact_type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                v35 = 0x90;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            v35 = 0x90;

        // Show the "next unit" button only if at least one child can be selected.
        // Unit Guns with unit_gun_no_switch = 1 do not count, so the button is
        // hidden (and its hotkey disabled) when they are the only children.
        for ( NC_STACK_ypabact *kid : yw->_userUnit->_kidList )
        {
            if ( !kid->_unitGunNoSwitch )
            {
                v35 |= 0x40;
                break;
            }
        }
    }
    else
    {
        v35 = 0x70;

        if ( yw->_userUnit->_bact_type == BACT_TYPES_GUN )
        {
            if ( !yw->_cmdrsRemap.empty() )
                v35 = 0xF0;
        }
        else if ( yw->_cmdrsRemap.size() > 1 )
            v35 = 0xF0;
    }

    if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
        v35 = 0;

    if ( spectatorControlled )
    {
        // Keep normal command buttons disabled in Spectator Mode. While the
        // follow camera is active, expose only the vanilla back/up button and
        // reuse it as Return to Spectator.
        v35 = yw->IsSpectatorFollowActive() ? 0x10 : 0;
        yw->GuiWinClose( &gui_lstvw );
    }

    // Reuse the vanilla Jump into Vehicle button and state machine. Spectator
    // mode exposes the same button because its command mask already includes
    // the control-mode bit; only the final action differs and starts external
    // Spectator Follow instead of taking direct control of the unit.
    if ( bzda.field_1CC & 8 )
    {
        bzda.buttons[1] = ButtonBox(bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //into_vhcl_btn

        if ( bzda.field_1D0 & 8 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 69);
    }
    else
    {
        bzda.buttons[1] = ButtonBox();

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 69);
    }

    if ( v35 & 0x10 )
    {
        bzda.buttons[4] = ButtonBox(yw->_iconOrderW + bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //to_host_btn

        if ( bzda.field_91C & 0x10 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 81);
    }
    else
    {
        bzda.buttons[4] = ButtonBox(); //to_host_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 81);
    }

    if ( v35 & 0x20 )
    {
        bzda.buttons[5] = ButtonBox(bzda.field_90C + 2 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //to_leader_btn

        if ( bzda.field_91C & 0x20 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 82);
    }
    else
    {
        bzda.buttons[5] = ButtonBox(); // to_leader_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 82);
    }

    if ( v35 & 0x40 )
    {
        if ( !bzda.field_8DC || (yw->_userRobo != yw->_userUnit && !yw->_playerInHSGun) )
        {
            bzda.buttons[6] = ButtonBox(bzda.field_90C + 3 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.field_91C & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 83);
        }
        else
        {
            bzda.buttons[6] = ButtonBox(bzda.field_90C + 3 * yw->_iconOrderW, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //turrets_btn

            if ( bzda.field_91C & 0x40 )
                FontUA::select_tileset(cur, 22);
            else
                FontUA::select_tileset(cur, 21);

            FontUA::store_u8(cur, 66);
        }
    }
    else
    {
        bzda.buttons[6] = ButtonBox(); // turrets_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 83);
    }

    if ( v35 & 0x80 )
    {
        bzda.buttons[7] = ButtonBox(4 * yw->_iconOrderW + bzda.field_90C, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); //next_squad_btn

        if ( bzda.field_91C & 0x80 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 84);
    }
    else
    {
        bzda.buttons[7] = ButtonBox(); //next_squad_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 84);
    }
}

void gui_update_tools(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( yw->IsSpectatorControlled() )
    {
        // Observer mode keeps only the exit/menu button. Situation Analyzer and
        // Help are tied to owner-1 player context and should not be available.
        bzda.buttons[8] = ButtonBox(); // analyzer_btn
        bzda.buttons[9] = ButtonBox(); // help_btn
        bzda.buttons[10] = ButtonBox(bzda.field_910, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); // menu_btn

        if ( bzda.field_91C & 0x400 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 85);
        return;
    }

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        bzda.buttons[8] = ButtonBox(); // analyzer_btn

        FontUA::select_tileset(cur, 23);
        FontUA::store_u8(cur, 63);
    }
    else
    {
        bzda.buttons[8] = ButtonBox(bzda.field_910, bzda.field_918, yw->_iconOrderW, yw->_iconOrderH); // analyzer_btn

        if ( bzda.field_91C & 0x100 )
            FontUA::select_tileset(cur, 22);
        else
            FontUA::select_tileset(cur, 21);

        FontUA::store_u8(cur, 63);
    }

    // OpenNeoUA: remove the deprecated online-help button from the gameplay toolbar.
    bzda.buttons[9] = ButtonBox(); // help_btn removed

    bzda.buttons[10] = ButtonBox(yw->_iconOrderW + bzda.field_910,  bzda.field_918,  yw->_iconOrderW,  yw->_iconOrderH); // menu_btn

    if ( bzda.field_91C & 0x400 )
        FontUA::select_tileset(cur, 22);
    else
        FontUA::select_tileset(cur, 21);

    FontUA::store_u8(cur, 85);
}

void sub_449970(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5, int a6, int a7)
{
    int v10, v22;
    const int markerGap = yw->_screenSize.x >= 512 ? 8 : 6;

    if ( yw->_screenSize.x >= 512 )
    {
        v22 = a7 - 72;
        v10 = a4 + a7 - 72 + markerGap;
    }
    else
    {
        v22 = a7 - 56;
        v10 = a4 + a7 - 56 + markerGap;
    }

    FontUA::select_tileset(cur, 15);

    FontUA::set_xpos(cur, a4);
    FontUA::set_ypos(cur, a3);

    int v14 = yw->_fontH - yw->_guiTiles[51]->h;

    FontUA::ColumnItem arg;
    arg.txt = a5;
    arg.width = v22;
    arg.fontID = 15;
    arg.prefixChar = 0;
    arg.spaceChar = 32;
    arg.postfixChar = 0;
    arg.flags = 40;

    FormateTextAlignedClippedString(yw, cur, &arg);

    FontUA::select_tileset(cur, 51);

    FontUA::set_xpos(cur, v10);
    FontUA::set_ypos(cur, (v14 / 2) + a3);

    for (int i = 0; i < a6; i++)
        FontUA::store_u8(cur, yw->_userRobo->_owner);
}

void ypaworld_func64__sub7__sub2__sub1__sub0(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( bzda.field_1D0 & 6 )
    {
        if ( bzda.field_8EC != -1 && !yw->_playerInHSGun )
        {
            World::TVhclProto *v4 = &yw->_vhclProtos[ bzda.field_2DC[ bzda.field_8EC ] ];

            auto resolvedJob = [v4](World::VehicleCombatClass targetClass, int legacyFallback)
            {
                int value = 0;
                if ( World::TryGetSpecificFightJob(*v4, targetClass, &value) )
                    return value;
                return legacyFallback;
            };

            auto jobMarkers = [](int value)
            {
                int markers = value / 2;
                // Match the original UI exactly: only a literal zero becomes
                // one visible marker; negative authored values remain non-drawing.
                if ( !markers )
                    markers = 1;
                return markers;
            };

            const int vsRobo = jobMarkers(v4->job_fightrobo);
            const int vsTank = jobMarkers(v4->job_fighttank);
            const int vsPlane = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_PLANE, v4->job_fightflyer));
            const int vsHeli = jobMarkers(v4->job_fighthelicopter);
            const int vsCruiser = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_CRUISER, v4->job_fightflyer));
            const int vsGlider = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_GLIDER, v4->job_fightflyer));
            const int vsZeppelin = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_ZEPPELIN, v4->job_fightflyer));
            const int vsUfo = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_UFO, v4->job_fightflyer));
            const int vsCar = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_CAR, v4->job_fighttank));
            // Legacy Host Station deployment treated unrecognized Gun targets as
            // the helicopter combat job; preserve that fallback until job_fightgun
            // is explicitly authored.
            const int vsGun = jobMarkers(resolvedJob(World::VEHICLE_COMBAT_CLASS_GUN, v4->job_fighthelicopter));
            const int capture = jobMarkers(v4->job_conquer);
            const int recon = jobMarkers(v4->job_reconnoitre);
            const bool hasFineJobs =
                v4->job_fightplane_defined || v4->job_fightcruiser_defined ||
                v4->job_fightglider_defined ||
                v4->job_fightzeppelin_defined || v4->job_fightufo_defined ||
                v4->job_fightcar_defined || v4->job_fightgun_defined;

            int v29_4 = 4 * yw->_screenSize.x / 7;
            int v6 = yw->_screenSize.x - v29_4;

            // Legacy-only Vehicle data keeps the original six-row layout
            // exactly. As soon as any fine job is authored, expose the extended
            // twelve-row class view (six additional specific combat classes).
            int v30 = -(yw->_downScreenBorder + (hasFineJobs ? 13 : 7) * yw->_fontH);

            const SDL_Color factionTextColor = yw_GetFactionUiTextColor(yw);
            FontUA::set_txtColor(cur, factionTextColor.r,
                                 factionTextColor.g, factionTextColor.b);

            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSROBO), vsRobo, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSTANK), vsTank, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_PLANE), vsPlane, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_VSHELI), vsHeli, v6);
            v30 += yw->_fontH;

            if ( hasFineJobs )
            {
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_CRUISER), vsCruiser, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), vsGlider, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), vsZeppelin, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_UFO), vsUfo, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_CAR), vsCar, v6);
                v30 += yw->_fontH;
                sub_449970(yw, cur, v29_4, v30, Locale::Text::OpenUA(Locale::OUA_VS_GUN), vsGun, v6);
                v30 += yw->_fontH;
            }

            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_CAPTURE), capture, v6);
            v30 += yw->_fontH;
            sub_449970(yw, cur, v29_4, v30, Locale::Text::Advanced(Locale::ADV_RECON), recon, v6);
        }
    }
}

void ypaworld_func64__sub7__sub2__sub1(NC_STACK_ypaworld *yw)
{
    sub_4C3A54(yw);

    bzda.cmdCommands.clear();

    if ( bzda.field_1D4 & 1 )
    {
        int v20 = bzda.x - (yw->_screenSize.x / 2);
        int v21 = bzda.y - (yw->_screenSize.y / 2);

        if ( !(bzda.field_1D8 & 1) )
        {
            bzda.field_1D8 |= 1;
            Input::Engine.AddClickBoxFront(&bzda);
        }

        Input::Engine.RemClickBox(&bzda);
        Input::Engine.AddClickBoxFront(&bzda);

        FontUA::select_tileset(&bzda.cmdCommands, 21);
        FontUA::set_center_xpos(&bzda.cmdCommands, v20);
        FontUA::set_center_ypos(&bzda.cmdCommands, v21);

        if ( bzda.field_904 > 0 )
            FontUA::add_xpos(&bzda.cmdCommands, bzda.field_904);

        gui_update_create_btn(yw, &bzda.cmdCommands);

        // The create glyph advances the command cursor by one icon. From here
        // onward use the same absolute anchors as the click boxes instead of
        // accumulating legacy gaps from the Genesis origin. This keeps visual
        // buttons and input hitboxes perfectly aligned for every Genesis X.
        const int afterCreateX = bzda.field_904 + yw->_iconOrderW;
        const int toMapSquad = bzda.field_908 - afterCreateX;
        if ( toMapSquad != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toMapSquad);

        gui_update_map_squad_btn(yw, &bzda.cmdCommands);

        const int afterMapSquadX = bzda.field_908 + 2 * yw->_iconOrderW;
        const int toPlayerPanel = bzda.field_90C - afterMapSquadX;
        if ( toPlayerPanel != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toPlayerPanel);

        gui_update_player_panel(yw, &bzda.cmdCommands);

        const int afterPlayerPanelX = bzda.field_90C + 5 * yw->_iconOrderW;
        const int toTools = bzda.field_910 - afterPlayerPanelX;
        if ( toTools != 0 )
            FontUA::add_xpos(&bzda.cmdCommands, toTools);

        gui_update_tools(yw, &bzda.cmdCommands);

        if ( gui_lstvw.IsOpen() )
            ypaworld_func64__sub7__sub2__sub1__sub0(yw, &bzda.cmdCommands);
    }

    FontUA::set_opacity(&bzda.cmdCommands, 255);
    FontUA::set_end(&bzda.cmdCommands);

    if ( gui_lstvw.IsOpen() )
        gui_lstvw.Formate(yw);
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub6(NC_STACK_ypaworld *yw)
{
    if ( !yw->_prevUnitId )
        return nullptr;

    if ( yw->_prevUnitId == yw->_userRobo->_gid )
        return yw->_userRobo;

    for( NC_STACK_ypabact* &comm : yw->_userRobo->_kidList )
    {
        if ( comm->_status != BACT_STATUS_DEAD && comm->_status != BACT_STATUS_CREATE && comm->_status != BACT_STATUS_BEAM )
        {
            if ( yw->_prevUnitId == comm->_gid )
                return comm;

            for( NC_STACK_ypabact* &unit : comm->_kidList )
            {
                if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_CREATE && unit->_status != BACT_STATUS_BEAM && yw->_prevUnitId == unit->_gid )
                    return unit;
            }
        }
    }

    return nullptr;
}

void ypaworld_func64__sub7__sub2__sub7(NC_STACK_ypaworld *yw)
{
    if ( !yw->_userUnit->_parent || yw->_userUnit->_parent != yw->_userRobo )
    {
        if ( yw->_userUnit->_bact_type != BACT_TYPES_ROBO && yw->_userUnit->_bact_type != BACT_TYPES_MISSLE && yw->_userUnit->_bact_type != BACT_TYPES_GUN )
        {
            bact_arg109 v5;
            v5.field_0 = 2;
            v5.field_4 = yw->_userUnit->_parent;

            yw->_userUnit->ReorganizeGroup(&v5);
        }
    }
}

int ypaworld_func64__sub7__sub2__sub3(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( yw->IsSpectatorControlled() )
    {
        const int mapHotKey = yw->_GameShell
            ? yw->_GameShell->InputConfig[World::INPUT_BIND_MAP].KeyID
            : 8;
        const int quitHotKey = yw->_GameShell
            ? yw->_GameShell->InputConfig[World::INPUT_BIND_QUIT].KeyID
            : 24;

        if ( inpt->HotKeyID == mapHotKey && yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 2;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        else if ( inpt->HotKeyID == quitHotKey )
        {
            // Reuse the ordinary in-game Exit Menu button path. This supports
            // both Escape and any remapped Quit binding without a spectator-only
            // menu implementation.
            winpt->selected_btnID = 10;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            inpt->HotKeyID = -1;
        }

        // Spectator gameplay commands remain disabled here. Class-level input
        // owned by the controlled UFO is left intact for its normal User_layer.
        return 0;
    }

    switch ( inpt->HotKeyID )
    {
    case 0:
        if ( bzda.field_1CC & 1 )
            bzda.field_1D0 = 1;
        break;

    case 1:
        if ( yw->_userUnit == yw->_userRobo )
        {
            if ( bzda.buttons[6] ) //turrets_btn
            {
                winpt->selected_btnID = 6;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
            }
        }
        break;

    case 2:
        if ( bzda.field_1CC & 2 )
        {
            if ( bzda.field_1D0 & 0x16 )
            {
                bzda.field_1D0 = bzda.field_1CC & 1;
            }
            else
            {
                if ( !(bzda.field_1D0 & 2) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                if ( bzda.field_8E0 <= bzda.field_8F8 )
                    bzda.field_1D0 = 16;
                else
                    bzda.field_1D0 = 2;

                return 0;
            }
        }
        break;

    case 3:
        if ( bzda.field_1CC & 4 )
        {
            if ( bzda.field_1D0 & 0x16 )
            {
                bzda.field_1D0 = bzda.field_1CC & 1;
            }
            else
            {
                if ( !(bzda.field_1D0 & 4) || gui_lstvw.IsClosed() )
                {
                    yw->GuiWinOpen( &gui_lstvw );
                    gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                }
                else
                {
                    yw->GuiWinClose( &gui_lstvw );
                }

                bzda.field_1D0 = 4;
                return 0;
            }
        }
        break;

    case 4:
        if ( bzda.field_1CC & 8 )
        {
            winpt->selected_btn = &bzda;
            winpt->selected_btnID = 1;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 7:
        if ( bzda.field_1CC & 0x20 )
        {
            if ( bzda.field_1D0 & 0x20 )
                bzda.field_1D0 = bzda.field_1CC & 1;
            else
                bzda.field_1D0 = 32;
        }
        break;

    case 8:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 2;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 9:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 3;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
        }
        break;

    case 20:
        if ( bzda.buttons[7] ) //next_squad_btn
        {
            winpt->selected_btnID = 7;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 21:
        if ( bzda.buttons[4] ) //to_host_btn
        {
            winpt->selected_btnID = 4;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 22:
        if ( bzda.buttons[6] ) //turrets_btn
        {
            winpt->selected_btnID = 6;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 23:
        if ( bzda.buttons[5] ) //to_leader_btn
        {
            winpt->selected_btnID = 5;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    case 24:
        if ( yw->_userRobo->_status == BACT_STATUS_DEAD || bzda.field_1D0 & 1 )
        {
            if ( yw->_userRobo == yw->_userUnit || yw->_userRobo->_status == BACT_STATUS_DEAD )
            {
                if ( bzda.buttons[10] ) //menu_btn
                {
                    winpt->selected_btnID = 10;
                    winpt->selected_btn = &bzda;
                    winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                }
            }
            else
            {
                winpt->selected_btnID = 4;
                winpt->selected_btn = &bzda;
                winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
                inpt->HotKeyID = -1;
            }
        }
        else
        {
            bzda.field_1D0 = 1;
        }
        break;

    case 25:
        if ( yw->_userUnit != yw->_userRobo )
        {
            if ( yw->_hud.field_0 )
                yw->_hud.field_0 = 0;
            else
                yw->_hud.field_0 = 1;
        }
        break;

    case 27:
        if ( info_log.IsClosed() )
        {
            yw->GuiWinOpen(&info_log);
            yw->GuiWinToFront(&info_log);
        }
        else
            yw->GuiWinClose(&info_log);
        break;

    case 28:
        if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 )
        {
            bzda.field_8F8++;
            if ( bzda.field_8F8 >= bzda.field_8E8 )
                bzda.field_8F8 = 0;

            if ( bzda.field_8F8 >= bzda.field_8E0 )
            {
                bzda.field_8F4 = bzda.field_8F8 - bzda.field_8E0;
                bzda.field_1D0 = 16;
                bzda.field_8EC = -1;
            }
            else
            {
                bzda.field_8EC = bzda.field_8F8;
                bzda.field_8F4 = -1;

                if ( !(bzda.field_1D0 & 4) )
                    bzda.field_1D0 = 2;
            }
        }
        return 0;

    case 29:
        if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 )
        {
            bzda.field_8F8--;

            if ( bzda.field_8F8 < 0 )
                bzda.field_8F8 = bzda.field_8E8 - 1;

            if ( bzda.field_8F8 >= bzda.field_8E0 )
            {
                bzda.field_8F4 = bzda.field_8F8 - bzda.field_8E0;
                bzda.field_8EC = -1;
                bzda.field_1D0 = 16;
            }
            else
            {
                bzda.field_8EC = bzda.field_8F8;
                bzda.field_8F4 = -1;

                if ( !(bzda.field_1D0 & 4) )
                    bzda.field_1D0 = 2;
            }
        }
        return 0;

    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
        if ( yw->_activeCmdrRemapIndex != -1 )
        {
            int v16;

            switch ( inpt->HotKeyID )
            {
            case 38:
                v16 = 0;
                break;

            case 39:
                v16 = 25;
                break;

            case 40:
                v16 = 50;
                break;

            case 41:
                v16 = 75;
                break;

            case 42:
                v16 = 100;
                break;
            }

            yw->_cmdrsRemap[ yw->_activeCmdrRemapIndex ]->setBACT_aggression(v16);

            if ( yw->_GameShell )
                SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
        }
        break;

    case 43:
        yw->sub_449DE8(Locale::Text::Help(Locale::HELP_GAMEPLAY));
        break;

    case 46:
        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            winpt->selected_btnID = 8;
            winpt->selected_btn = &bzda;
            winpt->flag |= TClickBoxInf::FLAG_BTN_UP;
        }
        break;

    default:
        break;
    }

    return 1;
}

void ypaworld_func64__sub7__sub2__sub2(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
    {
        if ( yw->_userUnit == yw->_userRobo )
        {
            yw->_roboMapStatus.Valid = true;
            yw->_roboMapStatus.IsOpen = robo_map.IsOpen();
            yw->_roboMapStatus.Rect.x = robo_map.x;
            yw->_roboMapStatus.Rect.y = robo_map.y;
            yw->_roboMapStatus.Rect.w = robo_map.w;
            yw->_roboMapStatus.Rect.h = robo_map.h;

            yw->_roboMapStatus.Data[0] = robo_map.field_1EC;
            yw->_roboMapStatus.Data[1] = robo_map.field_1ED;
            yw->_roboMapStatus.Data[2] = robo_map.field_1EE;
            yw->_roboMapStatus.Data[3] = robo_map.field_208;
            yw->_roboMapStatus.Data[4] = robo_map.field_20A;
            yw->_roboMapStatus.Data[5] = robo_map.field_20C;
            yw->_roboMapStatus.Data[6] = robo_map.field_20E;
            yw->_roboMapStatus.Data[7] = 0;

            yw->_roboFinderStatus.Valid = true;
            yw->_roboFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_roboFinderStatus.Rect.x = squadron_manager.x;
            yw->_roboFinderStatus.Rect.y = squadron_manager.y;
            yw->_roboFinderStatus.Rect.w = squadron_manager.w;
            yw->_roboFinderStatus.Rect.h = squadron_manager.h;
        }
        else
        {
            yw->_vhclMapStatus.Valid = true;
            yw->_vhclMapStatus.IsOpen = robo_map.IsOpen();
            yw->_vhclMapStatus.Rect.x = robo_map.x;
            yw->_vhclMapStatus.Rect.y = robo_map.y;
            yw->_vhclMapStatus.Rect.w = robo_map.w;
            yw->_vhclMapStatus.Rect.h = robo_map.h;

            yw->_vhclMapStatus.Data[0] = robo_map.field_1EC;
            yw->_vhclMapStatus.Data[1] = robo_map.field_1ED;
            yw->_vhclMapStatus.Data[2] = robo_map.field_1EE;
            yw->_vhclMapStatus.Data[3] = robo_map.field_208;
            yw->_vhclMapStatus.Data[4] = robo_map.field_20A;
            yw->_vhclMapStatus.Data[5] = robo_map.field_20C;
            yw->_vhclMapStatus.Data[6] = robo_map.field_20E;
            yw->_vhclMapStatus.Data[7] = 0;

            yw->_vhclFinderStatus.Valid = true;
            yw->_vhclFinderStatus.IsOpen = squadron_manager.IsOpen();
            yw->_vhclFinderStatus.Rect.x = squadron_manager.x;
            yw->_vhclFinderStatus.Rect.y = squadron_manager.y;
            yw->_vhclFinderStatus.Rect.w = squadron_manager.w;
            yw->_vhclFinderStatus.Rect.h = squadron_manager.h;
        }
    }
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub5(NC_STACK_ypaworld *yw)
{
    if ( bzda.field_8F0 != -1 )
    {
        if ( yw->_playerInHSGun )
        {
            bzda.field_8F0++;

            if ( bzda.field_8F0 >= bzda.field_8DC )
            {
                bzda.field_8F0 = 0;
                yw->_playerInHSGun = false;
                return yw->_userRobo;
            }

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            yw->_playerInHSGun = true;

            return robo->GetGuns().at(  bzda.field_1DC[ bzda.field_8F0 ]  ).gun_obj;
        }
        else
        {
            bzda.field_8F0 = 0;

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            yw->_playerInHSGun = true;

            return robo->GetGuns().at( bzda.field_1DC[ bzda.field_8F0 ] ).gun_obj;
        }
    }
    return nullptr;
}

NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub4(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit->IsParentMyRobo() )
    {
        for ( NC_STACK_ypabact* &unit : yw->_userUnit->_kidList )
        {
            if ( unit->_status != BACT_STATUS_CREATE && unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM &&
                 !unit->_unitGunNoSwitch )
            return unit;
        }
    }
    else
    {
        World::RefBactList *lst = yw->_userUnit->_kidRef.PList();
        if (lst)
        {
            for( World::RefBactList::iterator it = std::next(yw->_userUnit->_kidRef.iter(), 1); it != lst->end(); it++) // Start from current unit in this list
            {
               if ( (*it)->_status != BACT_STATUS_CREATE && (*it)->_status != BACT_STATUS_DEAD && (*it)->_status != BACT_STATUS_BEAM &&
                    !(*it)->_unitGunNoSwitch )
                    return *it;
            }
        }
    }

    if ( yw->_userUnit->_parent != yw->_userUnit->_host_station && yw->_userUnit->_parent )
        return yw->_userUnit->_parent;

    return nullptr;
}

int sub_4C885C()
{
    if ( dword_5BAF9C == 3 )
    {
        if ( lstvw2.IsClosed() )
            dword_5BAF9C = 2;
    }
    return dword_5BAF9C;
}

void ypaworld_func64__sub7__sub2__sub8(NC_STACK_ypaworld *yw)
{
    if ( sub_4C885C() != 3 )
    {
        yw->GuiWinOpen( &exit_menu );
        exit_menu.Attach(yw->_guiActive);
    }
}

void ypaworld_func64__sub7__sub2__sub9(NC_STACK_ypaworld *yw)
{
    if ( sub_4C885C() != 3 )
        yw->GuiWinClose( &exit_menu );
}

void NC_STACK_ypaworld::SituationAnalyzer()
{
    yw_arg159 v38;
    v38.txt = World::GameAnalyzer::Analyze(this);
    v38.unit = 0;
    v38.Priority = 100;
    v38.MsgID = 0;

    ypaworld_func159(&v38);
}

void ypaworld_func64__sub7__sub2__sub0()
{
    bzda.field_8E8 = 0;

    for (int i = 0; i < bzda.field_8E0; i++)
    {
        bzda.field_4DC[i].i = 1;
        bzda.field_4DC[i].d = bzda.field_2DC[i];
    }

    bzda.field_8E8 += bzda.field_8E0;

    if ( !(bzda.field_1D0 & 4) )
    {
        for (int i = 0; i < bzda.field_8E4; i++)
        {
            bzda.field_4DC[i + bzda.field_8E0].i = 2;
            bzda.field_4DC[i + bzda.field_8E0].d = bzda.field_3DC[i];
        }

        bzda.field_8E8 += bzda.field_8E4;
    }

    if ( bzda.field_8E8 )
    {
        if ( bzda.field_8E8 == 1 )
        {
            bzda.field_8F8 = 0;
        }
        else if ( bzda.field_8E8 <= bzda.field_8F8 )
        {
            bzda.field_8F8 = bzda.field_8E8 - 1;
        }
    }
    else
    {
        bzda.field_8F8 = -1;
    }
}

void sb_0x4c66f8__sub0(NC_STACK_ypaworld *yw)
{
    if ( yw->_timeStamp - yw->_msgTimestampHSReturn > 35000 )
    {
        yw_arg159 arg159;
        arg159.unit = yw->_userRobo;
        arg159.Priority = 10;
        arg159.MsgID = 39;

        yw->ypaworld_func159(&arg159);
    }

    yw->_msgTimestampHSReturn = yw->_timeStamp;
}

void sb_0x4c66f8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2)
{
    if ( !yw->CanControlUnitInSpectatorMode(bact1) )
        return;

    // A latched player Kamikaze sequence is intentionally irreversible. Keep
    // control on the armed carrier until its shared detonation path completes;
    // this guard runs before viewer/input flags are dropped from the current unit.
    if ( yw->_kamikazeFireTimeScaleDrainGid > 0 &&
         yw->_userUnit &&
         yw->_userUnit->_gid == yw->_kamikazeFireTimeScaleDrainGid &&
         bact1 != bact2 )
    {
        return;
    }

    // OpenNeoUA custom: never take control of an artillery shell platform. Bail out before the
    // current unit's viewer/input is dropped, so the player is not left viewer-less.
    if ( bact1 && bact1->IsArtilleryShellPlatform() )
        return;

    if ( bact1 != bact2 )
    {
        if ( bact1->_status != BACT_STATUS_CREATE && bact1->_status != BACT_STATUS_DEAD && bact1->_status != BACT_STATUS_BEAM )
        {
            yw->_hud.field_76 = yw->_timeStamp;

            bact2->setBACT_viewer(false);
            bact2->setBACT_inputting(false);

            bact1->setBACT_viewer(true);
            bact1->setBACT_inputting(true);

            yw->GuiWinClose( &gui_lstvw );

            if ( bzda.field_1CC & 1 )
            {
                if ( bzda.field_1D0 == 8 )
                    bzda.field_1D0 = 1;
            }

            if ( bact1 == yw->_userRobo )
            {
                yw->_mouseGrabbed = false;
                sb_0x4c66f8__sub0(yw);
            }
            else
            {
                yw_arg159 arg159;
                arg159.unit = yw->_userUnit;
                arg159.Priority = 33;
                arg159.MsgID = 17;

                yw->ypaworld_func159(&arg159);
            }

            yw->_guiDragDefaultMouse = false;
        }
    }
}

void  ypaworld_func64__sub7__sub2(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    NC_STACK_ypabact *bact1 = nullptr;
    NC_STACK_ypabact *bact2 = yw->_viewerBact;

    bzda.field_91C = 0;

    int a2a = 1;

    if ( bzda.field_1D4 & 1 )
    {
        if ( yw->IsSpectatorControlled() )
        {
            // Spectator UI accepts only the observer-safe actions handled by
            // sub3 (Map and Exit Menu). Unknown hotkeys are deliberately left
            // untouched so the controlled UFO's normal User_layer can consume
            // present and future class-level controls.
            if ( inpt->HotKeyID >= 0 )
                a2a = ypaworld_func64__sub7__sub2__sub3(yw, inpt);
        }
        else
        {
            switch ( inpt->HotKeyID )
            {
                case 31:
                {
                    NC_STACK_ypabact *v9 = yw->GetLastMsgSender();

                    if ( v9 )
                        bact1 = v9;
                }
                break;

                case 44:
                    bact1 = ypaworld_func64__sub7__sub2__sub6(yw);
                    break;

                case 45:
                    ypaworld_func64__sub7__sub2__sub7(yw);
                    break;

                default:
                    if ( inpt->HotKeyID >= 0 )
                        a2a = ypaworld_func64__sub7__sub2__sub3(yw, inpt);

                    break;
            }
        }

        if ( yw->_userUnit->_status == BACT_STATUS_DEAD && !yw->IsSpectatorControlled() )
        {
            if ( yw->_fireBtnDownHappen )
            {
                bact1 = yw->GetNextSquad();
                inpt->Buttons.UnSet(0);
            }
        }

        if ( gui_lstvw.IsOpen() )
        {
            if ( yw->_kbdLastKeyHit == Input::KC_ESCAPE || yw->_kbdLastKeyHit == Input::KC_RETURN )
            {
                yw->GuiWinClose(&gui_lstvw);

                inpt->KbdLastDown = Input::KC_NONE;
                inpt->HotKeyID = -1;
                inpt->KbdLastHit = Input::KC_NONE;
            }
        }

        if ( yw->_userUnit == yw->_userRobo )
        {
            if ( robo_map.field_1E8 & 8 )
                robo_map.field_1E8 &= 0xFFFFFFF7;
        }
        else if ( !(robo_map.field_1E8 & 8) )
        {
            robo_map.field_1E8 |= 8;
        }

        ypaworld_func64__sub7__sub2__sub2(yw);

        if ( winpt->selected_btn == &bzda )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( yw->_GameShell )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
                }
            }

            switch ( winpt->selected_btnID )
            {
            case 0:
                if ( !(winpt->flag & TClickBoxInf::FLAG_BTN_DOWN) )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
                    {
                        if ( bzda.field_1D0 & 0x16 )
                        {
                            bzda.field_1D0 = bzda.field_1CC & 1;
                            yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                        }
                        else
                        {

                            if ( !(bzda.field_1D0 & 4) || gui_lstvw.IsClosed() )
                            {
                                yw->GuiWinOpen( &gui_lstvw );
                                gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                            }
                            else
                            {
                                yw->GuiWinClose( &gui_lstvw );
                            }

                            a2a = 0;
                            bzda.field_1D0 = 4;
                        }
                    }
                    else
                        yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);

                }
                else if ( bzda.field_1D0 & 0x16 )
                {
                    bzda.field_1D0 = bzda.field_1CC & 1;

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                else
                {
                    if ( !(bzda.field_1D0 & 2) || gui_lstvw.IsClosed() )
                    {
                        yw->GuiWinOpen( &gui_lstvw );
                        gui_lstvw.listFlags |= (GuiList::GLIST_FLAG_IN_SELECT | GuiList::GLIST_FLAG_NO_SCROLL);
                    }
                    else
                    {
                        yw->GuiWinClose( &gui_lstvw );
                    }

                    if ( bzda.field_8E0 <= bzda.field_8F8 )
                        bzda.field_1D0 = 16;
                    else
                        bzda.field_1D0 = 2;

                    a2a = 0;
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEWSQUAD, 2);
                }
                break;

            case 1: // Jump into vehicle
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( bzda.field_1D0 & 8 )
                        bzda.field_1D0 = bzda.field_1CC & 1;
                    else
                        bzda.field_1D0 = 8;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_TAKECONTROL, 4);
                break;

            case 2: //MAP
            case 3: //SQUAD
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( winpt->selected_btnID == 2 )
                    {
                        if (robo_map.IsClosed())
                        {
                            yw->GuiWinOpen(&robo_map);
                            yw->GuiWinToFront(&robo_map);
                        }
                        else
                            yw->GuiWinClose(&robo_map);
                    }
                    else if ( winpt->selected_btnID == 3 && !yw->IsSpectatorControlled() )
                    {
                        if (squadron_manager.IsClosed())
                        {
                            yw->GuiWinOpen(&squadron_manager);
                            yw->GuiWinToFront(&squadron_manager);
                        }
                        else
                            yw->GuiWinClose(&squadron_manager);
                    }
                }

                if ( winpt->selected_btnID == 2 )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_MAPWND, 8);
                else if ( winpt->selected_btnID == 3 && !yw->IsSpectatorControlled() )
                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_FINDERWND, 9);

                break;

            case 4: //Back to host station
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x10;

                if ( yw->IsSpectatorFollowActive() )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        yw->ReturnToSpectatorVehicle();
                        yw->_guiDragDefaultMouse = false;
                    }

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_RETURNSPECTATOR, 21);
                    break;
                }

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    bact1 = yw->_userRobo;

                    if ( yw->_playerInHSGun )
                        bzda.field_1D0 = 1;
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOHS, 21);
                break;

            case 5:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x20;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    bact1 = yw->_userUnit->_parent;

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOTOCMDR, 23);
                break;

            case 6: // Cycle through units in squad
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x40;

                if ( yw->_userRobo == yw->_userUnit || yw->_playerInHSGun )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        bact1 = ypaworld_func64__sub7__sub2__sub5(yw);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_GOINTOGUNS, 22);
                }
                else
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        bact1 = ypaworld_func64__sub7__sub2__sub4(yw);

                    yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTUNIT, 22);
                }
                break;

            case 7: //Cycle through squad leader
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x80;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    bact1 = yw->GetNextSquad();

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_NEXTCMDR, 20);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    yw->SituationAnalyzer();

                yw->SetShowingTooltipWithHotkey(Locale::TIP_ANALYZER, 46);
                break;

            case 9:
                // Deprecated online-help button removed from the toolbar.
                break;

            case 10:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    bzda.field_91C |= 0x400;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    if ( exit_menu.IsClosed() )
                        ypaworld_func64__sub7__sub2__sub8(yw);
                    else
                        ypaworld_func64__sub7__sub2__sub9(yw);
                }

                yw->SetShowingTooltipWithHotkey(Locale::TIP_GUI_PAUSECANCEL, 24);
                break;

            default:
                break;
            }
        }

        if ( bzda.field_1D0 & 0x29 )
            yw->GuiWinClose(&gui_lstvw);

        if ( a2a )
        {
            gui_lstvw.InputHandle(yw, inpt);
            if ( gui_lstvw.IsOpen() )
            {
                if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16)
                {
                    bzda.field_8F8 = gui_lstvw.selectedEntry;

                    if ( gui_lstvw.selectedEntry >= bzda.field_8E0 )
                    {
                        bzda.field_8F4 = gui_lstvw.selectedEntry - bzda.field_8E0;
                        bzda.field_8EC = -1;
                        bzda.field_1D0 = 16;
                    }
                    else
                    {
                        bzda.field_8F4 = -1;
                        bzda.field_8EC = gui_lstvw.selectedEntry;

                        if ( !(bzda.field_1D0 & 4) )
                            bzda.field_1D0 = 2;
                    }
                }
            }
        }
    }

    if ( bact1 )
    {
        if ( bact1 != bact2 )
            sb_0x4c66f8(yw, bact1, bact2);
    }

    ypaworld_func64__sub7__sub2__sub0();
    ypaworld_func64__sub7__sub2__sub1(yw);
}

void sub_4E1D6C(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, uint8_t icon, uint8_t icon2, uint8_t icon3, float a6, float a7, const std::string &a8)
{
    int v26 = (a6 * up_panel.field_1E4);
    int v25 = (a7 * up_panel.field_1E4);

    if ( !v26 && a6 > 0.0 )
        v26 = 1;

    if ( !v25 && a7 > 0.0 )
        v25 = 1;

    FontUA::select_tileset(cur, 30);
    FontUA::set_center_xpos(cur, x);
    FontUA::set_center_ypos(cur, y);

    FontUA::store_glyph(cur, icon);

    FontUA::store_u8(cur, 84);

    if ( v26 > 0 || v25 > 0 )
    {
        int v13 = up_panel.field_1E0 + up_panel.field_1DC + x;

        FontUA::set_center_xpos(cur, v13);

        if ( v26 > 0 )
        {
            if ( v26 > 1 )
                FontUA::op11(cur, v26);

            FontUA::store_u8(cur, icon2);
        }

        if ( v25 > v26 )
        {
            if ( v25 > 1 )
                FontUA::op11(cur, v25);

            FontUA::store_u8(cur, icon3);
        }
    }

    if ( !a8.empty() )
    {
        FontUA::select_tileset(cur, 31);

        FontUA::set_center_xpos(cur, x + up_panel.field_1DC + 4);
        FontUA::set_ypos(cur, 0);

        cur->insert(cur->end(), a8.begin(), a8.end());
    }
}

void ypaworld_func64__sub7__sub7__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, int a3, int a4, int a5, int a6, float a7)
{
    int icon = 83; // Neutral trend unless a3 says otherwise.

    if ( a3 == -1 )
        icon = 82;
    else if ( a3 == 0 )
        icon = 83;
    else if ( a3 == 1 )
        icon = 81;

    int v11 = 0;
    switch ( yw->_userRobo->_pSector->owner )
    {
    case 1:
        v11 = 49;
        break;

    case 2:
        v11 = 50;
        break;

    case 3:
        v11 = 51;
        break;

    case 4:
        v11 = 52;
        break;

    case 5:
        v11 = 53;
        break;

    case 6:
        v11 = 54;
        break;

    case 7:
        v11 = 55;
        break;

    default:
        v11 = 0;
        break;
    }

    float a6a, a7a;

    if ( a4 <= 0 )
    {
        a7a = 0.0;
        a6a = 0.0;
    }
    else
    {
        a7a = a4 / 255.0;
        a6a = a7 * a7a;
    }

    std::string a1a;

    if ( a3 >= 0 )
    {
        if ( a5 )
        {
            a1a = fmt::sprintf("+%d   (%d%%)", a5, (int)(a7 * 100.0) );
        }
        else
        {
            a1a = fmt::sprintf("%d", 0);
        }
    }
    else
    {
        a1a = fmt::sprintf("%d", -a5);
    }

    sub_4E1D6C(yw, cur, x, y, icon, v11, 57, a6a, a7a, a1a);
}

void ypaworld_func64__sub7__sub7__sub0__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7)
{
    int v16 = 0;
    if ( a4 & 1 )
    {
        if ( a5 == -1 )
        {
            v16 = 66;
        }
        else if ( a5 == 0 )
        {
            v16 = 67;
        }
        else if ( a5 == 1 )
        {
            v16 = 65;
        }
    }
    else
    {
        v16 = 68;
    }

    float a6a = (float)a6 / (float)a7;
    std::string a1a = fmt::sprintf("%d", a6 / 100);

    int v10;

    if ( a7 / 5 <= a6 )
    {
        v10 = 56;
    }
    else
    {
        v10 = 54;

        if ( yw->_timeStamp / 300 & 1 )
            a1a = "";
    }

    sub_4E1D6C(yw, cur, x, y, v16, v10, 0, a6a, 0.0, a1a);
}

void ypaworld_func64__sub7__sub7__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur, int a3, int a2, char a4, int a5, int a6, int a7)
{
    int v18 = 0;
    if ( a4 & 4 )
    {
        if ( a5 == -1 )
            v18 = 70;
        else if ( a5 == 0 )
            v18 = 71;
        else if ( a5 == 1 )
            v18 = 69;
    }
    else
    {
        v18 = 72;
    }

    float v15 = (float)a7;
    int v9 = 57;
    float a7a = 0.0;
    float a6a = (float)a6 / v15;

    if ( bzda.field_1D0 & 0x16 )
    {
        if ( bzda.field_8F8 != -1 )
            a7a = yw->_updateMessage.energy / v15;
    }

    if ( a7a <= a6a )
    {
        float v10 = a6a;
        a6a -= a7a;
        a7a = v10;
    }
    else
    {
        a7a = a6a;
        v9 = 54;
        a6a = 0.0;
    }

    sub_4E1D6C(yw, cur, a3, a2, v18, 56, v9, a6a, a7a,  fmt::sprintf("%d", a6 / 100) );
}

void ypaworld_func64__sub7__sub7__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7)
{
    int v18 = 0;
    if ( a4 & 8 )
    {
        if ( a5 == -1 )
            v18 = 78;
        else if ( a5 == 0 )
            v18 = 79;
        else if ( a5 == 1 )
            v18 = 77;
    }
    else
    {
        v18 = 80;
    }

    float v15 = (float)a7;
    int v9 = 57;
    float a7a = 0.0;
    float a6a = (float)a6 / v15;

    if ( bzda.field_1D0 & 0x20 )
        a7a = yw->_updateMessage.energy / v15;

    if ( a7a <= a6a )
    {
        float v10 = a6a;
        a6a -= a7a;
        a7a = v10;
    }
    else
    {
        a7a = a6a;
        v9 = 54;
        a6a = 0.0;
    }

    sub_4E1D6C(yw, cur, x, y, v18, 56, v9, a6a, a7a,  fmt::sprintf("%d", a6 / 100) );
}

void ypaworld_func64__sub7__sub7__sub0(NC_STACK_ypaworld *yw)
{
    up_panel.cmdCommands.clear();

    if ( yw->IsSpectatorControlled() )
    {
        Input::Engine.RemClickBox(&up_panel);
        FontUA::set_end(&up_panel.cmdCommands);
        return;
    }

    if ( yw->_userRobo->_status != BACT_STATUS_DEAD )
    {
        int v3 = up_panel.x - yw->_screenSize.x / 2;
        int y = up_panel.y - yw->_screenSize.y / 2;
        int v41 = up_panel.field_1D8 + up_panel.field_1D0;

        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

        int a4 = robo->getROBO_fillMode();
        int v29 = robo->getROBO_battVehicle();
        int v28 = robo->getROBO_battBeam();
        int v27 = robo->getROBO_loadFlags();
        int v26 = robo->getROBO_lossFlags();

        int v36 = yw->_userRobo->_energy_max;

        yw_arg176 arg176;
        arg176.owner = yw->_userRobo->_pSector->owner;

        yw->ypaworld_func176(&arg176);

        int v32 = yw->_userRobo->_pSector->energy_power;
        int v33 = yw->_userRobo->_pSector->energy_power * arg176.field_4;

        int idx = robo->getROBO_absReload();

        int v11 = 10 * idx / 100;
        int v38 = 10 * ( idx * arg176.field_4 ) / 100;

        int v13, v34, v37, v35;

        if ( yw->_userRobo->_owner == yw->_userRobo->_pSector->owner )
            v13 = v33 != 0;
        else if ( v33 )
            v13 = -1;
        else
            v13 = 0;

        if ( v27 & 1 )
            v34 = 1;
        else if ( v26 & 1 )
            v34 = -1;
        else
            v34 = 0;

        if ( v27 & 4 )
            v37 = 1;
        else if ( v26 & 4 )
            v37 = -1;
        else
            v37 = 0;

        if ( v27 & 8 )
            v35 = 1;
        else if ( v26 & 8 )
            v35 = -1;
        else
            v35 = 0;

        Input::Engine.RemClickBox(&up_panel);
        Input::Engine.AddClickBoxFront(&up_panel);

        int x = (up_panel.field_1D4 & 0xFFFF) + v3;

        ypaworld_func64__sub7__sub7__sub0__sub0(yw, &up_panel.cmdCommands, x, y, v13, v32, v38, v11, arg176.field_4);

        int v18 = v41 + x;
        ypaworld_func64__sub7__sub7__sub0__sub1(yw, &up_panel.cmdCommands, v18, y, a4, v34, yw->_userRobo->_energy, v36);

        yw_RenderPlasmaCurrencyHud(yw, &up_panel.cmdCommands);

        int v20 = v41 + v18;
        ypaworld_func64__sub7__sub7__sub0__sub2(yw, &up_panel.cmdCommands, v20, y, a4, v37, v29, v36);
        ypaworld_func64__sub7__sub7__sub0__sub3(yw, &up_panel.cmdCommands, v41 + v20, y, a4, v35, v28, v36);
    }

    FontUA::set_end(&up_panel.cmdCommands);
}

void ypaworld_func64__sub7__sub7(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( yw->IsSpectatorControlled() )
    {
        ypaworld_func64__sub7__sub7__sub0(yw);
        return;
    }

    if ( winpt->selected_btn == &up_panel )
    {
        if ( yw->_GameShell )
        {
            if ( winpt->selected_btnID != -1 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                    SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            }
        }

        if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
        {
            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

            int a4 = robo->getROBO_fillMode();

            if ( winpt->selected_btnID == 1 )
                a4 ^= 1;
            else if ( winpt->selected_btnID == 2 )
                a4 ^= 4;
            else if ( winpt->selected_btnID == 3 )
                a4 ^= 8;

            robo->setROBO_fillMode(a4);
        }

        switch ( winpt->selected_btnID )
        {
        case 0:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_RELOAD);
            break;

        case 1:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_MAIN);
            break;

        case 2:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_CREATION);
            break;

        case 3:
            yw->SetShowingTooltip(Locale::TIP_ENERGY_TELEPORT);
            break;

        default:
            break;
        }
    }

    ypaworld_func64__sub7__sub7__sub0(yw);
}
