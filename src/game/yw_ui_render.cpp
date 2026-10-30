// GUI render pass: in-game messages, tech-update popup, cockpit vehicle and
// weapon info, radar, and the drawing order of windows and overlays.
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

void sb_0x4d7c08__sub0__sub1(const SDL_Color *uiAccent)
{
    if ( bzda.IsOpen() )
    {
        GFX::Engine.ProcessDrawSeq(bzda.cmdCommands, &bzda.cmdInclude, uiAccent);

        if ( gui_lstvw.IsOpen() )
            GFX::Engine.ProcessDrawSeq(gui_lstvw.cmdCommands, &gui_lstvw.cmdInclude, uiAccent);
    }
}

void sb_0x4d7c08__sub0(NC_STACK_ypaworld *yw)
{
    SDL_Color uiAccentColor;
    const SDL_Color *uiAccent = yw_GetFactionUiAccent(yw, &uiAccentColor);

    if ( yw->_hideHudForScreenshots )
        return;

    if ( yw->_userUnit->_bact_type != BACT_TYPES_MISSLE )
    {
        const bool priorityWindowOpen = yw_IsPriorityGameplayUiOcclusionOpen();

        // Render the complete ordinary gameplay UI first. When Map/Squadron or
        // the Genesis list is open, only the pixels under the actual window
        // rectangle are removed later; nothing outside the overlap is globally
        // suppressed.
        yw_RenderWorldSelectionDrag(yw);
        yw_RenderMoveOrderFeedback(yw);
        yw_RenderAttackOrderFeedback(yw);
        yw_RenderRoboRelocationMarker(yw);
        yw_RenderCustomWorldMapMarkers(yw);
        sb_0x4d7c08__sub0__sub4(yw);

        if ( yw->_userUnit->_status != BACT_STATUS_DEAD )
        {
            sb_0x4d7c08__sub0__sub0(yw);
            sb_0x4d7c08__sub0__sub2(yw);
        }

        // Ordinary GUI windows are foreground UI. Render them after world/HUD
        // overlays so status bars and other gameplay indicators cannot bleed
        // through dialogs or panels. Tooltips still render above these windows.
        if ( !priorityWindowOpen )
        {
            for ( GuiBase *window : yw->_guiActive )
            {
                if ( !yw_IsTopLevelExitMenuFlowWindow(window) )
                    yw_RenderGameplayGuiWindow(yw, window, uiAccent);
            }
        }

        draw_tooltip(yw, uiAccent);

        if ( !priorityWindowOpen )
        {
            // Fixed gameplay bars and creation UI remain above ordinary windows.
            GFX::Engine.ProcessDrawSeq(up_panel.cmdCommands, &up_panel.cmdInclude, uiAccent);
            yw_RenderPlasmaCurrencyHudIcon(yw);
            sb_0x4d7c08__sub0__sub1(uiAccent);

            // Unlike ordinary windows, the complete Exit Menu flow is
            // finalized after every gameplay overlay so nothing from the
            // world UI can cover either the menu or its confirmations.
            yw_RenderTopLevelExitMenuFlow(yw, uiAccent);
        }
    }
}

void sb_0x4d7c08__sub0__sub0(NC_STACK_ypaworld *yw)
{

    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_isNetGame )
    {
        if ( yw->_netEvent.EventType )
        {
            uint32_t timeDelay;
            std::string str;

            if ( yw->_netEvent.EventType == 1 )
            {
                str = Locale::Text::Advanced(Locale::ADV_VICTORY);
                timeDelay = 40000;
            }
            else if ( yw->_netEvent.EventType > 1 && yw->_netEvent.EventType <= 4 )
            {
                str = fmt::sprintf( Locale::Text::Advanced(Locale::ADV_DEFEATED) , yw->_netEvent.PlayerName);

                timeDelay = 20000;
            }
            else
            {
                timeDelay = 0;
            }

            if ( !str.empty() && yw->_timeStamp - yw->_netEvent.TimeStamp < timeDelay )
            {
                if ( yw->_timeStamp / 300 & 1 )
                {
                    FontUA::select_tileset(&buf, 15);
                    FontUA::set_xpos(&buf, 0);
                    FontUA::set_ypos(&buf, yw->_screenSize.y / 3);

                    FontUA::set_txtColor(&buf, 255, 255, 255);

                    FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, str, yw->_screenSize.x);
                }
            }
            else
            {
                yw->_netEvent.EventType = 0;
            }
        }
    }

    if ( sb_0x4d7c08__sub0__sub0__sub0(yw) )
        return;

    int v10;

    if ( info_log.field_254 <= info_log.msg_count )
        v10 = info_log.msg_count - info_log.field_254;
    else
        v10 = info_log.msg_count + 64 - info_log.field_254;

    if ( v10 > 10 )
        v10 = 10;

    FontUA::select_tileset(&buf, 15);
    FontUA::set_ypos(&buf, up_panel.field_1CC + yw->_fontH / 2);

    int v11 = (info_log.msg_count - v10) % 64;

    if ( v11 < 0 )
        v11 += 64;

    int v28 = 0;
    int v27 = 0;

    while ( 1 )
    {
        if ( v11 >= 64 )
            v11 = 0;

        inflog_msg *v14 = &info_log.msgs[v11];

        if ( v14->field_8 > 0 )
        {
            if ( !v27 )
            {
                v27 = 1;

                int v15;

                if ( v11 > info_log.msg_count )
                    v15 = info_log.msg_count + 64 - v11;
                else
                    v15 = info_log.msg_count - v11;

                v15 = (v15 + 1) / 2;

                if ( !v15 )
                    v15 = 1;

                v14->field_8 -= yw->_frameTime * v15;
            }

            if ( v14->field_4 )
                v28 = v14->field_4;
            else
                v28 += 200;

            int v18 = yw->_timeStamp - v28;

            int v19;

            if ( v18 < 200 )
            {
                v19 = 100 * v18 / 200;

                if ( v19 < 0 )
                    v19 = 0;
                else if ( v19 > 100 )
                    v19 = 100;
            }
            else
            {
                v19 = 100;
            }

            if ( v19 > 0 )
            {
                FontUA::set_xpos(&buf, 16);
                FontUA::set_txtColor(&buf, yw->_iniColors[64].r, yw->_iniColors[64].g, yw->_iniColors[64].b);
                // Output ingame messages. From analyzer and other.
                FontUA::TextRelWidthItem(yw->_guiTiles[15], &buf, v14->txt, v19, 4);

                FontUA::next_line(&buf);
            }
        }

        if ( v11 == info_log.msg_count )
            break;

        v11++;
    }

    FontUA::set_end(&buf);

    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(buf, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));
}

void sub_4E4F80(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float x, float y, int value, int maxval, int valCH, int valBG, const std::string &txt1, const std::string &txt2, int flag)
{
    int wnd_xpos = 0;
    int wnd_xpos2 = 0;
    int wnd_ypos = 0;
    int wnd_ypos2 = 0;

    int wnd_vis = 0;

    if ( robo_map.IsOpen() )
    {
        wnd_xpos = robo_map.x - (yw->_screenSize.x / 2);
        wnd_xpos2 = robo_map.w + wnd_xpos;
        wnd_ypos = robo_map.y - (yw->_screenSize.y / 2);
        wnd_ypos2 = robo_map.h + wnd_ypos;

        wnd_vis = 1;
    }

    int v51 = (yw->_screenSize.x / 2) * x;
    int v50 = (yw->_screenSize.y / 2) * y;

    int v49 = yw->_guiTiles[51]->map[valCH].w * wis->field_9E;
    // Keep every classic HUD status row on the same shared layout, but leave
    // a small breathing space between the three-letter label and the bar.
    const int labelBarGap = txt1.empty() ? 0 : std::max(2, yw->_guiTiles[51]->map[1].w / 2);

    FontUA::set_txtColor(cur, yw->_iniColors[65].r, yw->_iniColors[65].g, yw->_iniColors[65].b);

    v51 -= (wis->field_9A + wis->field_96 + labelBarGap + v49) / 2;

    if ( !txt1.empty() )
    {
        int v19 = (wis->field_96 / 2) + v51;
        int v20 = v50;

        if ( !wnd_vis || v19 <= wnd_xpos || v19 >= wnd_xpos2 || v20 <= wnd_ypos || v20 >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, v51);
            FontUA::set_center_ypos(cur, v50 - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt1, 100, 4);
        }
    }

    FontUA::select_tileset(cur, 51);

    v51 += wis->field_96 + labelBarGap;

    FontUA::set_center_xpos(cur, v51);
    FontUA::set_center_ypos(cur, v50 - (yw->_guiTiles[51]->h / 2));

    int v29 = v51 + (yw->_guiTiles[51]->map[1].w / 2);
    int v30 = v50 + (yw->_guiTiles[51]->h >> 1);

    const bool isHpBar = valCH == 2 && valBG == 6;
    bool useProceduralStatusBar = false;
    if ( isHpBar )
    {
        const int barLeft = (yw->_screenSize.x / 2) + v51;
        const int barTop = (yw->_screenSize.y / 2) + v50 - (yw->_guiTiles[51]->h / 2);
        useProceduralStatusBar = yw_RenderProceduralHpBar(
                                     barLeft, barTop, wis->field_9E, value, maxval,
                                     yw->_guiTiles[51]->map[1].w, yw->_guiTiles[51]->h,
                                     255,
                                     (flag & 1) != 0, (flag & 2) != 0);
    }

    for (int i = 1; i <= wis->field_9E; i++)
    {
        int v35 = i * (maxval / wis->field_9E) - (maxval / wis->field_9E) / 2;

        if ( (!wnd_vis || v29 <= wnd_xpos || v29 >= wnd_xpos2 || v30 <= wnd_ypos || v30 >= wnd_ypos2) &&
             !useProceduralStatusBar )
        {
            if ( v35 > value )
            {
                if ( (flag & 2) == 0 )
                    FontUA::store_u8(cur, valBG);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
            else
            {
                if ( (flag & 1) == 0 )
                    FontUA::store_u8(cur, valCH);
                else
                    FontUA::add_xpos(cur, yw->_guiTiles[51]->map[1].w);
            }
        }

        v29 += yw->_guiTiles[51]->map[1].w;
    }

    v51 += wis->field_9A + v49;

    if ( !txt2.empty() )
    {
        int v37 = v51 - (wis->field_9A / 2);
        int v38 = v50;

        if ( !wnd_vis || v37 <= wnd_xpos || v37 >= wnd_xpos2 || v38 <= wnd_ypos || v38 >= wnd_ypos2 )
        {
            FontUA::select_tileset(cur, 15);

            FontUA::set_center_xpos(cur, v51);
            FontUA::set_center_ypos(cur, v50 - (yw->_fontH / 2));

            FontUA::TextRelWidthItem(yw->_guiTiles[15], cur, txt2, 100, 8);
        }
    }
}

bool yw_IsMgunWeaponInfoAvailable(NC_STACK_ypabact *bact,
                                         World::TVhclProto *vhcl)
{
    if ( !bact || !vhcl || bact->_bact_type != BACT_TYPES_GUN )
        return false;

    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(bact);
    if ( !gun || gun->getGUN_fireType() != NC_STACK_ypagun::GUN_TYPE_PROTO )
        return false;

    // Opt-in only: old data without the two new authoring fields keeps the
    // previous gun_type = mg HUD behavior.
    return !vhcl->mgun_name.empty() || vhcl->mgun_wireframe;
}

void sb_0x4d7c08__sub0__sub0__sub0__sub0(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float a4, float a5, int bldId, int a7)
{

    if ( !(a7 & 0x10) || (yw->_timeStamp - wis->field_76 - 200) / 180.0 > 0.0 )
    {
        if ( yw->_timeStamp / 200 & 1 )
        {
            std::string bldName = yw->ResolveGameplayBuildingName(bldId, yw->_isNetGame);

            yw_RenderInfoVehicleName(yw, wis, cur, bldName, a4, a5);
        }
    }
}

int sb_0x4d7c08__sub0__sub0__sub0(NC_STACK_ypaworld *yw)
{
    //Tech update draw
    GFX::Engine.raster_func210( Common::FRect(-1.0, -1.0, 1.0, 1.0) );

    sklt_wis *wis = &yw->_hud;

    CmdStream buf;
    buf.reserve(1024);

    int32_t v6 = 0;
    if ( !yw->IsGemPopupVisible(&v6) )
        return 0;

    int a8 = 0;
    int v14 = yw->_upgradeBuildId;

    wis->field_86 = 1.0;

    int v10 = yw->_techUpgrades[ yw->_upgradeId ].Type;

    if ( v10 == World::UPGRADE_WEAPON )
        a8 |= 1;
    else if ( v10 == World::UPGRADE_ARMOR )
        a8 |= 4;
    else if ( v10 == World::UPGRADE_VEHICLE )
        a8 |= 8;

    // game.gem_unlock_popup_y is the anchor of the popup: the vehicle panel is
    // symmetric around it (weapon rows above, name below), a building name sits on it.
    const float popupY = yw->GetGemPopupY();
    if ( v6 )
    {
        yw_RenderHUDInfo(yw, wis, &buf, 0.0, popupY, 0, v6, a8);

        // OpenNeoUA: unit limit row right under the vehicle name, shown while
        // the name is. Only vehicles that define max_active_at_once get it, so
        // vanilla data keeps the original popup. The limit is off in network games.
        const int maxActive = (size_t)v6 < yw->_vhclProtos.size() ? yw->_vhclProtos[v6].max_active_at_once : 0;
        // The row blinks only when this GEM raised that limit; otherwise it stays steady.
        const bool limitRaised = yw->_upgradeMaxActiveVehicleId == v6;
        const bool rowShown = !limitRaised || ((yw->_timeStamp / 200) & 1);
        if ( maxActive > 0 && !yw->_isNetGame && rowShown )
            yw_RenderInfoVehicleName(yw, wis, &buf,
                                     Locale::Text::OpenUA(Locale::OUA_HUD_MAX_UNITS) + " " + std::to_string(maxActive),
                                     0.0, popupY + wis->field_92 * 14.5f);
    }
    else if ( v14 )
        sb_0x4d7c08__sub0__sub0__sub0__sub0(yw, wis, &buf, 0.0, popupY, v14, a8);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    return 1;
}

void sb_0x4d7c08__sub0__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact)
{
    if ( yw_ShouldHideControlledUnitWorldUi(yw, bact) )
        return;

    if ( yw->_GameShell )
    {
        if ( yw->_isNetGame )
        {
            if ( !yw->_GameShell->netPlayers[bact->_owner].Name.empty() )
            {
                const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, bact->_position);
                if ( worldUiOpacity == 0 )
                    return;

                float v5 = bact->_position.x - yw->_viewerPosition.x;
                float v6 = bact->_position.y - yw->_viewerPosition.y;
                float v10 = bact->_position.z - yw->_viewerPosition.z;

                mat3x3 corrected = yw->_viewerRotation;
                GFX::Engine.matrixAspectCorrection(corrected, false);

                float v32 = corrected.m00 * v5 + corrected.m01 * v6 + corrected.m02 * v10;
                float v31 = corrected.m10 * v5 + corrected.m11 * v6 + corrected.m12 * v10;
                float v26 = corrected.m20 * v5 + corrected.m21 * v6 + corrected.m22 * v10;

                if ( v26 > 30.0 && v32 < v26 )
                {
                    if ( v32 > -v26 && v31 < v26 && v31 > -v26 )
                    {
                        int v30 = yw->_guiTiles[15]->h;

                        float projectedX = v32 / v26;
                        float projectedY = v31 / v26;
                        GFX::Engine.viewZoomCorrection(projectedX, projectedY);
                        if ( projectedX < -1.0f || projectedX > 1.0f ||
                             projectedY < -1.0f || projectedY > 1.0f )
                        {
                            return;
                        }

                        int v27_4 = ((yw->_screenSize.x / 2) * (projectedX + 1.0f));
                        int v15 = (yw->_screenSize.y / 2) * (projectedY + 1.0f);

                        int v28 = 96;

                        if ( v27_4 + 96 >= yw->_screenSize.x )
                            v28 = yw->_screenSize.x - v27_4 - 1;

                        if ( v28 > 0 && v27_4 >= 0 )
                        {
                            if ( v28 + v27_4 <  yw->_screenSize.x && v15 >= 0 )
                            {
                                if ( v30 + v15 < yw->_screenSize.y )
                                {
                                    FontUA::select_tileset(cur, 15);
                                    FontUA::set_center_xpos(cur, v27_4 - (yw->_screenSize.x / 2) );
                                    FontUA::set_center_ypos(cur, v15 - (yw->_screenSize.y / 2) );

                                    FontUA::set_txtColor(cur, yw->_iniColors[ bact->_owner ].r, yw->_iniColors[ bact->_owner ].g, yw->_iniColors[ bact->_owner ].b);
                                    FontUA::set_opacity(cur, worldUiOpacity);

                                    FontUA::FormateClippedText(yw->_guiTiles[15], cur,  yw->_GameShell->netPlayers[bact->_owner].Name, v28, 32);
                                    FontUA::set_opacity(cur, 255);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void sb_0x4d7c08__sub0__sub4(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    if ( yw->_userUnit->_status == BACT_STATUS_DEAD )
    {
        std::string msg;

        if ( !(yw->_userRobo->_status_flg & BACT_STFLAG_CLEAN) )
        {
            if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
                msg = Locale::Text::Common(Locale::CMN_HSDESTR);
            else
                msg = Locale::Text::Common(Locale::CMN_UNITDESTR);
        }

        if ( !msg.empty() && yw->_timeStamp / 500 & 1 )
        {
            FontUA::select_tileset(&buf, 15);
            FontUA::set_xpos(&buf, 0);
            FontUA::set_center_ypos(&buf, -(yw->_fontH / 2));

            FontUA::set_txtColor(&buf, 255, 255, 255);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &buf, msg, yw->_screenSize.x);
        }
    }
    else if ( yw->_userUnit != yw->_userRobo && yw->_hud.field_0 && !yw->IsSpectatorControlled() )
    {
        yw_RenderHUDRadare(yw);

        if ( yw->_userUnit->_energy >= yw->_userUnit->_energy_max )
        {
            yw->_hud.field_82 = 3000;
        }
        else
        {
            // Keep the original health-driven cockpit colour cycle unchanged.
            // The procedural HP bar itself does not pulse or blink.
            yw->_hud.field_82 = 3000 * yw->_userUnit->_energy / yw->_userUnit->_energy_max;

            if ( yw->_hud.field_82 < 200 )
                yw->_hud.field_82 = 200;
        }

        yw->_hud.field_86 += (float)yw->_frameTime / (float)yw->_hud.field_82;

        if ( yw->_hud.field_86 >= 1.0 )
            yw->_hud.field_86 = 0;

        yw_RenderHUDVectorGFX(yw, &buf);
    }
    else if ( yw->IsSpectatorControlled() )
    {
        // Spectator Mode: always show the radar, without the cockpit vehicle HUD.
        // The H (HUD) hotkey is not routed to the spectator, so the radar cannot
        // depend on _hud.field_0 (it starts at 0). It is drawn at the
        // bottom-left inside yw_RenderHUDRadare.
        yw_RenderHUDRadare(yw);
    }

    if ( yw->_guiActFlags & 0x20 )
    {
        if ( !yw->_bactOnMouse->ShouldHideFromStrategicUI() )
        {
            yw_RenderCursorOverUnit(yw, yw->_bactOnMouse);

            if ( !yw->IsSpectatorControlled() )
                yw_RenderUnitLifeBar(yw, &buf, yw->_bactOnMouse);
        }
    }

    std::vector<NC_STACK_ypabact *> damageHoverTargets = yw->GetUserDamageHoverTargets();
    if ( !yw->IsSpectatorControlled() )
    {
        for (NC_STACK_ypabact *damageHoverTarget : damageHoverTargets)
        {
            bool sameAsMouseHover = (yw->_guiActFlags & 0x20) && yw->_bactOnMouse == damageHoverTarget;
            if ( !sameAsMouseHover )
                yw_RenderCursorOverUnit(yw, damageHoverTarget);

            if ( !sameAsMouseHover && yw->_guiVisor.field_18 != damageHoverTarget )
                yw_RenderUnitLifeBar(yw, &buf, damageHoverTarget);
        }
    }

    if ( !yw->IsSpectatorControlled() && yw->_guiVisor.field_18 && !yw->_guiVisor.field_18->ShouldHideFromStrategicUI() )
        yw_RenderUnitLifeBar(yw, &buf, yw->_guiVisor.field_18);

    if ( yw->IsUfoSpyUiControlContext() )
    {
        // Arrows are the lightweight default UFO sensor layer and stay visible
        // independently from the optional detailed HP/status toggle.
        for ( NC_STACK_ypabact *unit : yw->_unitsList )
            yw_RenderUfoSpyWorldArrows(yw, unit, damageHoverTargets);
    }

    if ( yw_IsUfoSpyHudActive(yw) )
    {
        for ( NC_STACK_ypabact *unit : yw->_unitsList )
            yw_RenderUfoSpyWorldStatus(yw, &buf, unit, damageHoverTargets);
    }

    yw_RenderOverlayCursors(yw, &buf, damageHoverTargets);
    yw_RenderPlasmaCurrencyPopups(yw, &buf);

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    yw->_guiVisor.field_18 = nullptr;
}

void sub_4E3B80(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float v15 = yw->_hud.field_86 * 2.0 - 1.0;

    float v17 = fabs( v15 - sqrt(POW2(x1) + POW2(y1)) );
    float v16 = fabs( v15 - sqrt(POW2(x2) + POW2(y2)) );

    if ( v17 > 1.0)
        v17 = 1.0;
    else if ( v17 < 0.0)
        v17 = 0.0;

    if ( v16 > 1.0)
        v16 = 1.0;
    else if ( v16 < 0.0)
        v16 = 0.0;

    int v9 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v17 + yw->_hud.cl1_g;
    int v10 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v17 + yw->_hud.cl1_r;
    int v11 =  (yw->_hud.cl2_b - yw->_hud.cl1_b) * v17 + yw->_hud.cl1_b;
    *out1 = GFX::Engine.Color(v10, v9, v11);

    int v12 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v16 + yw->_hud.cl1_g;
    int v13 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v16 + yw->_hud.cl1_r;
    int v14 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v16 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(v13, v12, v14);
}

void sub_4E3D98(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    int v7 = yw->_timeStamp % 300;
    float v9 = v7 / 30000.0;

    sklt_wis *v8 = &yw->_hud;

    int v10 = (v8->cl2_g - v8->cl1_g) * v9 + v8->cl1_g;
    int v11 = (v8->cl2_r - v8->cl1_r) * v9 + v8->cl1_r;
    int v12 = (v8->cl2_b - v8->cl1_b) * v9 + v8->cl1_b;
    *out1 = GFX::Engine.Color(v11, v10, v12);

    int v13 = (v8->cl2_r - v8->cl1_r) * v9 + v8->cl1_r;
    int v16 = (v8->cl2_b - v8->cl1_b) * v9 + v8->cl1_b;
    int v15 = (v8->cl2_g - v8->cl1_g) * v9 + v8->cl1_g;
    *out2 = GFX::Engine.Color(v13, v15, v16);
}

void sb_0x4d7c08__sub0__sub4__sub2__sub0(NC_STACK_ypaworld *yw)
{
    // Render units on radare

    CmdStream buf;
    buf.reserve(8192);

    FontUA::select_tileset(&buf, 61);

    float v6 = yw->_screenSize.x / 2;
    float v7 = yw->_screenSize.y / 2;

    robo_map.field_1F8 = dround((robo_map.field_25C - robo_map.field_254) * v6);
    robo_map.field_1FC = dround((robo_map.field_260 - robo_map.field_258) * v7);

    robo_map.field_1F0 = dround(robo_map.field_1D8 / robo_map.field_1E0) - (robo_map.field_1F8 / 2);
    robo_map.field_1F4 = -dround(robo_map.field_1DC / robo_map.field_1E4) - (robo_map.field_1FC / 2);

    robo_map.field_200 = dround(v6 * robo_map.field_254);
    robo_map.field_204 = dround(v7 * robo_map.field_258);

    Common::Rect drect;
    drect.left = robo_map.field_200;
    drect.top = robo_map.field_204;
    drect.right = robo_map.field_1F8 + robo_map.field_200 - 1;
    drect.bottom = robo_map.field_1FC + robo_map.field_204 - 1;

    GFX::Engine.raster_func211(drect);
    yw->RenderArtilleryShellMapMarkers();
    yw->RenderLaserMapBeams(61);

    int v14 = dround(robo_map.field_1F0 * robo_map.field_1E0) / World::CVSectorLength;
    int v29 = dround(robo_map.field_1F4 * robo_map.field_1E4) / World::CVSectorLength;
    int v28 = dround( (robo_map.field_1F8 + robo_map.field_1F0) * robo_map.field_1E0 ) / World::CVSectorLength;
    int v30 = dround( (robo_map.field_1FC + robo_map.field_1F4) * robo_map.field_1E4 ) / World::CVSectorLength;

    if ( v14 < 1 )
        v14 = 1;

    if ( v29 < 1 )
        v29 = 1;

    if ( v28 >= yw->_mapSize.x )
        v28 = yw->_mapSize.x - 1;

    if ( v30 >= yw->_mapSize.y )
        v30 = yw->_mapSize.y - 1;

    int v38 = 0;

    for (int i = v29; i <= v30; i++ )
    {
        for (int j = v14; j <= v28; j++ )
        {
            if ( v38 < 512 ) // Units maximum?
            {
                cellArea &v17 = yw->_cells(j, i);

                if ( robo_map.MapViewMask & v17.view_mask )
                {
                    for( NC_STACK_ypabact* &bact : v17.unitsList )
                    {
                        if ( bact->ShouldHideFromStrategicUI() )
                            continue;

                        int v19 = 0;
                        if ( robo_map.IsOpen() )
                        {
                            Common::PointRect r { robo_map.x - (yw->_screenSize.x / 2)
                                                , robo_map.y - (yw->_screenSize.y / 2)
                                                , robo_map.w
                                                , robo_map.h };

                            if ( r.IsIn( sub_4F681C( bact->_position.XZ() ) ) )
                                v19 = 1;
                        }

                        if ( bact->_bact_type == BACT_TYPES_ROBO )
                        {
                            if ( !v19 )
                            {
                                FontUA::select_tileset(&buf, 1);

                                sub_4F6DFC(yw, &buf, yw->_guiTiles[1]->h, yw->_guiTiles[1]->map[24].w, bact, 0);

                                FontUA::select_tileset(&buf, 61);

                                v38++;
                            }
                        }
                        else if ( bact->_bact_type == BACT_TYPES_GUN )
                        {
                            NC_STACK_ypagun *gun = (NC_STACK_ypagun *)bact;

                            if ( !gun->IsRoboGun() && !v19 )
                            {
                                sub_4F6DFC(yw, &buf, 7, 7, bact, 0);

                                v38++;
                            }
                        }
                        else if ( !v19 )
                        {
                            sub_4F6DFC(yw, &buf, 7, 7, bact, 0);

                            v38++;
                        }
                    }
                }
            }
        }
    }

    FontUA::set_end(&buf);

    GFX::Engine.ProcessDrawSeq(buf);

    // OpenNeoUA custom: the 3D world already shows the artillery shell cooldown/readiness bar
    // above the HP bar. Mirror that feedback on the small gameplay radar too,
    // drawing it after unit icons so it stays visible.
    GFX::Engine.raster_func211(drect);
    yw_RenderArtilleryShellCooldownRadarBars(yw, v14, v29, v28, v30);

    // Draw custom annotations last so they remain readable above unit icons and
    // cooldown bars while the player is controlling a unit.
    yw_RenderCustomHudRadarMarkers(yw);
}

int sub_47F360(const void *a1, const void *a2)
{
    return (((const aab *)a2)->b) - (((const aab *)a1)->b);
}

void sb_0x4d7c08__sub0__sub2__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, const std::array<World::TPlayerStatus, World::CVFractionsCount> &statuses, int a3)
{
    if ( yw->_isNetGame )
    {
        aab v25[8];

        int nums = 0;

        for (int i = 0; i < 8; i++)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
            {
                v25[nums].a = i;
                v25[nums].b = statuses[i].Score;
                nums++;
            }
        }

        qsort(v25, nums, sizeof(aab), sub_47F360);

        for (int i = 0; i < nums; i++)
        {
            int clrid;

            FontUA::ColumnItem a4a[2];

            switch ( v25[i].a )
            {
            case 1:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_RESIST);
                clrid = 1;
                break;

            case 2:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_SULG);
                clrid = 2;
                break;

            case 3:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_MYKO);
                clrid = 3;
                break;

            case 4:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_TAER);
                clrid = 4;
                break;

            case 5:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_BLKSCT);
                clrid = 5;
                break;

            case 6:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_GHOR);
                clrid = 6;
                break;

            default:
                a4a[0].txt = Locale::Text::Advanced(Locale::ADV_NEUTRAL);
                clrid = 7;
                break;
            }

            FontUA::set_txtColor(cur, yw->_iniColors[clrid].r, yw->_iniColors[clrid].g, yw->_iniColors[clrid].b);

            a4a[0].spaceChar = 32;
            a4a[0].prefixChar = 0;
            a4a[0].postfixChar = 0;
            a4a[0].fontID = 15;
            a4a[0].flags = 36;
            a4a[0].width = a3 * 0.5;

            a4a[1].txt = fmt::sprintf("%d", statuses[ v25[i].a ].Score);
            a4a[1].width = a3 * 0.5;
            a4a[1].fontID = 15;
            a4a[1].spaceChar = 32;
            a4a[1].flags = 36;
            a4a[1].prefixChar = 0;
            a4a[1].postfixChar = 0;

            FormateColumnItem(yw, cur, 2, a4a);

            FontUA::next_line(cur);
        }

        FontUA::next_line(cur);
    }
}

void sb_0x4d7c08__sub0__sub2__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3)
{
    if ( yw->_isNetGame )
    {
        if ( yw->_countUnitsPerOwner[ yw->_userRobo->_owner ] <= yw->_levelUnitLimit )
            FontUA::set_txtColor(cur, yw->_iniColors[63].r, yw->_iniColors[63].g, yw->_iniColors[63].b);
        else
            FontUA::set_txtColor(cur, yw->_iniColors[6].r, yw->_iniColors[6].g, yw->_iniColors[6].b);

        FontUA::ColumnItem a4[2];

        a4[0].txt = Locale::Text::Advanced(Locale::ADV_UNITS);
        a4[0].width = a3 * 0.5;
        a4[0].spaceChar = 32;
        a4[0].fontID = 15;
        a4[0].prefixChar = 0;
        a4[0].postfixChar = 0;
        a4[0].flags = 36;

        a4[1].txt = fmt::sprintf("%d / %d", yw->_countUnitsPerOwner[ yw->_userRobo->_owner ], yw->_levelUnitLimit);
        a4[1].width = a3 * 0.5;
        a4[1].fontID = 15;
        a4[1].postfixChar = 0;
        a4[1].spaceChar = 32;
        a4[1].flags = 36;
        a4[1].prefixChar = 0;

        FormateColumnItem(yw, cur, 2, a4);

        FontUA::next_line(cur);
    }
}

void sb_0x4d7c08__sub0__sub2(NC_STACK_ypaworld *yw)
{
    CmdStream buf;
    buf.reserve(1024);

    int v25 = 2 * yw->_screenSize.x / 3;
    int v2 = yw->_screenSize.x - v25;
    const int startY = up_panel.field_1CC + (yw->_fontH / 2);

    FontUA::select_tileset(&buf, 15);
    FontUA::set_xpos(&buf, v25);
    FontUA::set_ypos(&buf, startY);

    sb_0x4d7c08__sub0__sub2__sub0(yw, &buf, v2);
    sb_0x4d7c08__sub0__sub2__sub1(yw, &buf, yw->_gameplayStats, v2);

    FontUA::set_end(&buf);

    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(buf, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));

    // Super-item text carries gameplay ownership information. Render it outside
    // the player's UI-accent remap so the displayed colour stays the real owner colour.
    CmdStream superItemBuf;
    superItemBuf.reserve(512);

    int superItemY = startY;
    if ( yw->_isNetGame )
    {
        int ownerRows = 0;
        for (int i = 0; i < World::CVFractionsCount; ++i)
        {
            if ( (1 << i) & yw->_levelInfo.OwnerMask )
                ++ownerRows;
        }

        // Unit count row + faction score rows + the existing blank separator.
        superItemY += (ownerRows + 2) * yw->_guiTiles[15]->h;
    }

    FontUA::select_tileset(&superItemBuf, 15);
    FontUA::set_xpos(&superItemBuf, v25);
    FontUA::set_ypos(&superItemBuf, superItemY);

    for (const TMapSuperItem &sitem : yw->_levelInfo.SuperItems)
    {
        if ( sitem.Type != 0 )
        {
            std::string timeStr;
            std::string typeStr = yw->GetSuperItemDisplayName(sitem);

            int v23 = 0;

            if ( sitem.State == TMapSuperItem::STATE_ACTIVE || sitem.State == TMapSuperItem::STATE_STOPPED )
            {
                int v10 = (sitem.CountDown + 1023) / 1024;

                if ( v10 < 0 )
                    v10 = 0;

                timeStr = fmt::sprintf("%s: %02d:%02d", typeStr, v10 / 60 % 60, v10 % 60);

                v23 = 1;
            }
            else if ( sitem.State == TMapSuperItem::STATE_TRIGGED )
            {
                timeStr = fmt::sprintf("%s: %s", typeStr, Locale::Text::Advanced(Locale::ADV_TRIGGERED));

                v23 = 1;
            }

            if ( v23 )
            {
                const uint32_t owner = sitem.ActivateOwner <= World::OWNER_7
                                     ? sitem.ActivateOwner
                                     : (uint32_t)World::OWNER_0;

                FontUA::set_xpos(&superItemBuf, v25);
                FontUA::set_txtColor(&superItemBuf,
                                     yw->_iniColors[owner].r,
                                     yw->_iniColors[owner].g,
                                     yw->_iniColors[owner].b);

                FontUA::FormateClippedText(yw->_guiTiles[15], &superItemBuf, timeStr.c_str(),
                                           yw->_screenSize.x - v25, 32);

                FontUA::next_line(&superItemBuf);
            }
        }
    }

    FontUA::set_end(&superItemBuf);
    GFX::Engine.ProcessDrawSeq(superItemBuf);
}
