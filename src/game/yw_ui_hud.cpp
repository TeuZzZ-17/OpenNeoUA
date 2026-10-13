// HUD: radar, compass, weapon lock markers, unit life bars, info panel, cursors and plasma display.
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

bool yw_GetPlasmaCurrencyHudLayout(NC_STACK_ypaworld *yw,
                                           uint64_t value,
                                           TPlasmaCurrencyHudLayout *layout)
{
    if ( !yw || !layout || !yw->_guiTiles[PLASMA_CURRENCY_HUD_FONT] ||
         up_panel.field_1D8 <= 4 || up_panel.field_1CC <= 2 )
        return false;

    TileMap *font = yw->_guiTiles[PLASMA_CURRENCY_HUD_FONT];
    constexpr int itemGap = 2;
    const int gapWidth = up_panel.field_1D8;
    const int availableWidth = gapWidth - 4;
    const int iconSize = std::max(8, std::min(14, up_panel.field_1CC - 2));
    const std::string fullSuffix = std::to_string(value);

    int suffixWidth = font->GetWidth(fullSuffix);
    int contentWidth = iconSize + itemGap + suffixWidth;

    layout->suffix = fullSuffix;
    layout->showIcon = true;

    if ( contentWidth > availableWidth )
    {
        layout->suffix = std::to_string(value);
        suffixWidth = font->GetWidth(layout->suffix);
        layout->showIcon = false;
        contentWidth = suffixWidth;
    }

    const int step = up_panel.field_1D0 + up_panel.field_1D8;
    const int mainEnergyLeft = up_panel.x +
                               (up_panel.field_1D4 & 0xFFFF) + step;
    const int gapLeft = mainEnergyLeft + up_panel.field_1D0;
    const int contentLeft = gapLeft + (gapWidth - contentWidth) / 2;

    const int iconTop = up_panel.y + (up_panel.field_1CC - iconSize) / 2;
    const int iconCenterY = iconTop + iconSize / 2;
    layout->textY = iconCenterY - font->h / 2 - yw->_screenSize.y / 2;

    int nextLeft = contentLeft;

    if ( layout->showIcon )
    {
        layout->iconLeft = nextLeft;
        layout->iconTop = iconTop;
        layout->iconSize = iconSize;
        nextLeft += iconSize + itemGap;
    }

    layout->suffixX = nextLeft - yw->_screenSize.x / 2;
    return true;
}

void yw_RenderPlasmaCurrencyHud(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( !yw || !cur || !yw->IsPlasmaCurrencyEnabled() )
        return;

    TPlasmaCurrencyHudLayout layout;
    if ( !yw_GetPlasmaCurrencyHudLayout(yw, yw->GetPlasmaCurrencyHudValue(), &layout) )
        return;

    SDL_Color color = yw_GetFactionUiTextColor(yw);
    FontUA::select_tileset(cur, PLASMA_CURRENCY_HUD_FONT);
    FontUA::set_opacity(cur, yw->GetPlasmaCurrencyHudOpacity());
    FontUA::set_txtColor(cur, color.r, color.g, color.b);
    FontUA::set_center_ypos(cur, layout.textY);

    FontUA::set_center_xpos(cur, layout.suffixX);
    cur->insert(cur->end(), layout.suffix.begin(), layout.suffix.end());

    FontUA::set_txtColor(cur, 255, 255, 255);
    FontUA::set_opacity(cur, 255);
}

void yw_RenderPlasmaCurrencyHudIcon(NC_STACK_ypaworld *yw)
{
    if ( !yw || !yw->IsPlasmaCurrencyEnabled() || !yw->_userRobo ||
         yw->_userRobo->_status == BACT_STATUS_DEAD || yw->IsSpectatorControlled() )
        return;

    TPlasmaCurrencyHudLayout layout;
    if ( !yw_GetPlasmaCurrencyHudLayout(yw, yw->GetPlasmaCurrencyHudValue(), &layout) ||
         !layout.showIcon )
        return;

    NC_STACK_bitmap *icon = StatusIconLoad(StatusIconPlasmaPath(yw),
                                           layout.iconSize, layout.iconSize);
    if ( icon && icon->GetBitmap() )
    {
        StatusIconRenderBitmap(yw, icon, layout.iconLeft, layout.iconTop,
                               layout.iconSize, yw->GetPlasmaCurrencyHudOpacity());
    }
}

void NC_STACK_ypaworld::RenderPlasmaCurrencyBank()
{
    if ( !IsPlasmaCurrencyEnabled() || !_guiTiles[15] )
        return;

    SDL_Surface *screen = GFX::Engine.Screen();
    if ( !screen )
        return;

    TileMap *font = _guiTiles[15];
    std::string amount = std::to_string(_plasmaCurrencyBank);
    for ( int i = (int)amount.size() - 3; i > 0; i -= 3 )
        amount.insert((size_t)i, " ");

    // Compact badge in the top-right corner: it stays clear of the selectable map.
    const int pad = std::max(4, font->h / 3);
    const int height = font->h + pad * 2;
    const int iconSize = height - 4;
    const int textWidth = std::max(GFX::Engine.MeasureScreenTextWidth(amount),
                                   GFX::Engine.MeasureScreenTextWidth("0 000")) + 2;
    const int notch = height / 3;
    const int width = std::min(_screenSize.x / 4, iconSize + textWidth + pad * 4 + notch);
    const int margin = std::max(pad, _screenSize.x / 80);
    const int left = _screenSize.x - margin - width;
    const int top = margin;

    SDL_Rect oldClip;
    SDL_GetClipRect(screen, &oldClip);
    SDL_SetClipRect(screen, nullptr);

    const Uint32 border = SDL_MapRGB(screen->format, 47, 92, 104);
    const Uint32 body = SDL_MapRGB(screen->format, 7, 18, 23);
    const Uint32 glow = SDL_MapRGB(screen->format, 83, 184, 210);

    // Body with a chamfered lower-left corner, built from one-pixel rows.
    for ( int y = 0; y < height; y++ )
    {
        const int cut = (y >= height - notch) ? (notch - (height - 1 - y)) : 0;
        SDL_Rect row = {left + cut, top + y, width - cut, 1};
        const bool edge = (y == 0 || y == height - 1);
        SDL_FillRect(screen, &row, edge ? border : body);
        SDL_Rect side = {left + cut, top + y, 1, 1};
        SDL_FillRect(screen, &side, border);
        SDL_Rect right = {left + width - 1, top + y, 1, 1};
        SDL_FillRect(screen, &right, border);
    }
    SDL_Rect strip = {left + notch, top + height - 2, width - notch - 1, 1};
    SDL_FillRect(screen, &strip, glow);

    NC_STACK_bitmap *icon = StatusIconLoad(StatusIconPlasmaPath(this), iconSize, iconSize);
    if ( icon )
        StatusIconRenderBitmap(this, icon, left + pad, top + (height - iconSize) / 2, iconSize);

    const int textLeft = left + pad * 2 + iconSize;
    const int available = width - (textLeft - left) - pad;
    CmdStream text;
    FontUA::select_tileset(&text, 15);
    FontUA::set_xpos(&text, textLeft);
    FontUA::set_ypos(&text, top + pad);
    FontUA::set_txtColor(&text, 217, 241, 245);
    FontUA::FormateClippedText(font, &text, amount, available, ' ');
    FontUA::set_end(&text);
    GFX::Engine.ProcessDrawSeq(text);
    SDL_SetClipRect(screen, &oldClip);
}

SDL_Color yw_GetNeutralSelectionShadowColor()
{
    return GFX::Engine.Color(55, 55, 50);
}

const SDL_Color *yw_GetFactionUiAccent(NC_STACK_ypaworld *yw, SDL_Color *accent)
{
    if (!yw || !accent || !yw->_userRobo ||
        yw->_userRobo->_owner < 1 || yw->_userRobo->_owner > 6)
    {
        return nullptr;
    }

    // Keep the Resistance on the original vanilla cyan/azure UI. Every other
    // faction uses the authoritative owner colour loaded from world.ini.
    if ( yw->_userRobo->_owner == World::OWNER_RESIST )
        return nullptr;

    *accent = yw->GetColor(yw->_userRobo->_owner);
    return accent;
}

SDL_Color yw_GetFactionUiTextColor(NC_STACK_ypaworld *yw)
{
    if ( yw && yw->_userRobo && yw->_userRobo->_owner >= World::OWNER_RESIST &&
         yw->_userRobo->_owner <= World::OWNER_GHOR )
    {
        return yw->GetColor(yw->_userRobo->_owner);
    }

    return GFX::Engine.Color(210, 210, 195);
}

float yw_GetWorldUiMaxDistance()
{
    constexpr float DEFAULT_DISTANCE = 5700.0f;
    const std::string value = System::IniConf::GameWorldUiMaxDistance.Get<std::string>();

    if ( value.empty() || value.find(',') != std::string::npos )
        return DEFAULT_DISTANCE;

    try
    {
        size_t pos = 0;
        float distance = std::stof(value, &pos);
        if ( value.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !isfinite(distance) || distance < 0.0f )
        {
            return DEFAULT_DISTANCE;
        }

        if (distance == 0.0f)
            return 0.0f;
        return std::max(100.0f, std::min(distance, 20000.0f));
    }
    catch (...)
    {
        return DEFAULT_DISTANCE;
    }
}

bool yw_ShouldHideControlledUnitWorldUi(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    // In cockpit view, hide only the world-space overlay that belongs to the
    // directly controlled player unit. All other units and world UI remain visible.
    return yw && bact && yw->_userUnit && !yw->IsSpectatorControlled() &&
           yw->_userUnit->IsCockpitCameraActive() && bact == yw->_userUnit;
}

float yw_GetWorldUiFadeStart(float maxDistance)
{
    // Keep the configured value as the hard cutoff, but reserve a visible
    // transition band before it. A proportional band works at normal ranges,
    // while the clamps avoid an imperceptibly short fade at low values or an
    // excessively long translucent zone at very large OpenNeoUA distances.
    const float fadeLength = std::min(maxDistance,
                                      std::max(400.0f,
                                               std::min(maxDistance * 0.35f, 2500.0f)));
    return maxDistance - fadeLength;
}

uint8_t yw_GetWorldUiOpacity(NC_STACK_ypaworld *yw, const vec3d &worldPos)
{
    if (!yw)
        return 0;

    const float maxDistance = yw_GetWorldUiMaxDistance();
    if (maxDistance <= 0.0f)
        return 255;

    const float fadeStart = yw_GetWorldUiFadeStart(maxDistance);
    const float distance = (worldPos - yw->_viewerPosition).length();

    if (distance >= maxDistance)
        return 0;
    if (distance <= fadeStart)
        return 255;

    float ratio = (maxDistance - distance) / (maxDistance - fadeStart);
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    // Smoothstep gives the same continuous curve in both directions: moving
    // away fades out, moving closer fades in, without a visible threshold pop.
    ratio = ratio * ratio * (3.0f - 2.0f * ratio);
    return (uint8_t)dround(ratio * 255.0f);
}

int yw_GetWorldUiSectorRadius(float maxDistance)
{
    if (maxDistance <= 0.0f)
        return 1;

    // A target can sit near the far edge of its sector while the viewer sits
    // near the opposite edge of another one, so keep one safety sector beyond
    // the pure distance/sector quotient. This prevents the candidate scan from
    // dropping an overlay before its opacity curve reaches zero.
    return std::max(1, (int)ceil(maxDistance / World::CVSectorLength) + 1);
}

void yw_PrepareEnemyIndicatorVisibilityStates(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    const uint32_t now = (uint32_t)yw->_timeStamp;
    if ( g_enemyIndicatorVisibilityWorld != yw ||
         yw->_timeStamp < g_enemyIndicatorVisibilityLastGameTime )
    {
        g_enemyIndicatorVisibilityStates.clear();
        g_enemyIndicatorVisibilityWorld = yw;
        g_enemyIndicatorVisibilityLastGameTime = yw->_timeStamp;
        g_enemyIndicatorVisibilityLastPrune = now;
        return;
    }

    g_enemyIndicatorVisibilityLastGameTime = yw->_timeStamp;
    if ( (uint32_t)(now - g_enemyIndicatorVisibilityLastPrune) <
         ENEMY_INDICATOR_VISIBILITY_PRUNE_INTERVAL_MS )
    {
        return;
    }

    for (auto it = g_enemyIndicatorVisibilityStates.begin();
         it != g_enemyIndicatorVisibilityStates.end(); )
    {
        const EnemyIndicatorVisibilityState &state = it->second;
        if ( state.world != yw ||
             (uint32_t)(now - state.lastTouched) >
                 ENEMY_INDICATOR_VISIBILITY_STATE_TTL_MS )
        {
            it = g_enemyIndicatorVisibilityStates.erase(it);
        }
        else
        {
            ++it;
        }
    }

    g_enemyIndicatorVisibilityLastPrune = now;
}

uint8_t yw_GetEnemyIndicatorVisibilityOpacity(NC_STACK_ypaworld *yw,
                                                      NC_STACK_ypabact *bact,
                                                      bool currentlyVisible,
                                                      vec3d *renderPosition)
{
    if ( !yw || !bact || !renderPosition )
        return 0;

    const uint32_t now = (uint32_t)yw->_timeStamp;
    auto stateIt = g_enemyIndicatorVisibilityStates.find(bact);

    if ( stateIt == g_enemyIndicatorVisibilityStates.end() )
    {
        // Never create a state for an enemy the player has not actually seen.
        // This keeps fog-of-war/radar semantics intact and avoids leaking
        // undiscovered units merely because they are within the UI distance.
        if ( !currentlyVisible )
            return 0;

        EnemyIndicatorVisibilityState state;
        state.world = yw;
        state.gid = bact->_gid;
        state.lastTouched = now;
        state.lastVisiblePosition = bact->_position;
        stateIt = g_enemyIndicatorVisibilityStates.emplace(bact, state).first;
    }

    EnemyIndicatorVisibilityState &state = stateIt->second;
    if ( state.world != yw || state.gid != bact->_gid )
    {
        if ( !currentlyVisible )
        {
            g_enemyIndicatorVisibilityStates.erase(stateIt);
            return 0;
        }

        state = EnemyIndicatorVisibilityState();
        state.world = yw;
        state.gid = bact->_gid;
        state.lastVisiblePosition = bact->_position;
    }

    state.lastTouched = now;
    if ( currentlyVisible )
        state.lastVisiblePosition = bact->_position;

    // game.world_ui_max_distance still owns the spatial fade. This short
    // time-domain transition only smooths the discrete radar/sector visibility
    // bit which otherwise removes enemy markers before distance opacity runs.
    const int32_t frameTime = std::max(0, std::min(yw->_frameTime, 100));
    const float opacityStep = ENEMY_INDICATOR_VISIBILITY_FADE_MS > 0
                                ? 255.0f * (float)frameTime /
                                      (float)ENEMY_INDICATOR_VISIBILITY_FADE_MS
                                : 255.0f;

    if ( currentlyVisible )
        state.opacity = std::min(255.0f, state.opacity + opacityStep);
    else
        state.opacity = std::max(0.0f, state.opacity - opacityStep);

    *renderPosition = currentlyVisible ? bact->_position : state.lastVisiblePosition;

    if ( !currentlyVisible && state.opacity <= 0.0f )
    {
        g_enemyIndicatorVisibilityStates.erase(stateIt);
        return 0;
    }

    return (uint8_t)dround(state.opacity);
}

void yw_RenderPlasmaCurrencyPopups(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    if ( !yw || !cur || !yw->IsPlasmaCurrencyEnabled() ||
         yw->_plasmaCurrencyPopups.empty() || !yw->_guiTiles[15] )
        return;

    constexpr uint32_t popupLifetimeMs = 1200;
    constexpr int popupItemGap = 2;
    const int iconSize = std::max(8, std::min(14, yw->_guiTiles[15]->h));
    NC_STACK_bitmap *icon = StatusIconLoad(StatusIconPlasmaPath(yw),
                                           iconSize, iconSize);
    const bool hasIcon = icon && icon->GetBitmap();

    for ( auto it = yw->_plasmaCurrencyPopups.begin();
          it != yw->_plasmaCurrencyPopups.end(); )
    {
        const uint32_t age = (uint32_t)(yw->_timeStamp - it->startTime);
        if ( age >= popupLifetimeMs )
        {
            it = yw->_plasmaCurrencyPopups.erase(it);
            continue;
        }

        Common::Point point;
        if ( !yw_ProjectWorldSelectionPoint(yw, it->worldPos, &point) )
        {
            ++it;
            continue;
        }

        const uint8_t distanceOpacity = yw_GetWorldUiOpacity(yw, it->worldPos);
        if ( distanceOpacity == 0 )
        {
            ++it;
            continue;
        }

        float ageOpacity = 1.0f;
        if ( age < 100 )
            ageOpacity = (float)age / 100.0f;
        else if ( age > 800 )
            ageOpacity = (float)(popupLifetimeMs - age) / 400.0f;

        const uint8_t opacity = (uint8_t)std::max(0, std::min(255,
            (int)std::floor((float)distanceOpacity * ageOpacity)));
        if ( opacity == 0 )
        {
            ++it;
            continue;
        }

        const std::string text = "+" + std::to_string(it->amount);
        const int textWidth = yw->_guiTiles[15]->GetWidth(text);
        const int contentWidth = textWidth +
                                 (hasIcon ? popupItemGap + iconSize : 0);
        const int contentLeft = point.x - contentWidth / 2;
        const int contentTop = point.y - yw->_guiTiles[15]->h -
                               (int)(age * 20 / popupLifetimeMs);

        SDL_Color color = yw_GetFactionUiTextColor(yw);
        color.a = opacity;
        FontUA::select_tileset(cur, 15);
        FontUA::set_opacity(cur, opacity);
        FontUA::set_txtColor(cur, color.r, color.g, color.b);
        FontUA::set_center_xpos(cur, contentLeft - yw->_screenSize.x / 2);
        FontUA::set_center_ypos(cur, contentTop - yw->_screenSize.y / 2);
        cur->insert(cur->end(), text.begin(), text.end());

        if ( hasIcon )
        {
            const int iconLeft = contentLeft + textWidth + popupItemGap;
            const int iconTop = contentTop + (yw->_guiTiles[15]->h - iconSize) / 2;
            StatusIconRenderBitmap(yw, icon, iconLeft, iconTop,
                                   iconSize, opacity);
        }

        FontUA::set_txtColor(cur, 255, 255, 255);
        FontUA::set_opacity(cur, 255);
        ++it;
    }
}

void wis_color(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float v7 = yw->_hud.field_86 * 2.0 + -1.5;

    float v18 = fabs(v7 - x1);
    float v17 = fabs(v7 - x2);

    if ( v18 > 0.5 )
        v18 = 0.5;

    if ( v17 > 0.5 )
        v17 = 0.5;

    float v8 = v18 * 2.0;
    int v9 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v8 + yw->_hud.cl1_g;
    int v10 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v8 + yw->_hud.cl1_r;
    int v11 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v8 + yw->_hud.cl1_b;

    *out1 = GFX::Engine.Color(v10, v9, v11);

    float v12 = 2.0 * v17;
    int v13 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v12 + yw->_hud.cl1_g;
    int v14 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v12 + yw->_hud.cl1_r;
    int v15 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v12 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(v13, v14, v15);
}

void yw_RenderVector2D(NC_STACK_ypaworld *yw, UAskeleton::Data *wire, float posX, float posY, float m00, float m01, float m10, float m11, float scaleX, float scaleY, SDL_Color coloooor, wis_color_func color_func, wis_color_func color_func2, bool aspectCorrection, const World::TVisualTint *wireframeTint)
{
    float CW = 1.0, CH = 1.0;

    if (aspectCorrection)
        GFX::Engine.getAspectCorrection(CW, CH, false);

    if ( wire )
    {
        GFX::Engine.raster_func217(ApplyWireframeTint(coloooor, wireframeTint));

        for (size_t i = 0; i < wire->POO.size(); i++)
        {
            float tmpx = wire->POO[i].x * 0.001;
            float tmpz = -wire->POO[i].z * 0.001;

            wire->tformedVertex[i].x = (tmpx * m00 + tmpz * m01) * scaleX * CW + posX;
            wire->tformedVertex[i].y = (tmpx * m10 + tmpz * m11) * scaleY * CH + posY;
        }

        for (size_t i = 0; i < wire->polygons.size(); i++)
        {
            UAskeleton::Polygon *poly = &wire->polygons[i];

            if (poly->num_vertices >= 2)
            {
                Common::FLine v29;
                v29.x1 = wire->tformedVertex[ poly->v[0] ].x;
                v29.y1 = wire->tformedVertex[ poly->v[0] ].y;
                v29.x2 = wire->tformedVertex[ poly->v[1] ].x;
                v29.y2 = wire->tformedVertex[ poly->v[1] ].y;

                if ( color_func )
                {
                    SDL_Color v32 = coloooor;
                    SDL_Color v31 = coloooor;

                    color_func(yw,  v29.x1 - posX,   v29.y1 - posY,   v29.x2 - posX,   v29.y2 - posY, &v32, &v31);

                    GFX::Engine.raster_func217(ApplyWireframeTint(v32, wireframeTint));
                }
                else if ( color_func2 )
                {
                    SDL_Color v34 = coloooor;
                    SDL_Color v33 = coloooor;

                    color_func2(yw, v29.x1, v29.y1, v29.x2, v29.y2, &v34, &v33);


                    GFX::Engine.raster_func217(ApplyWireframeTint(v34, wireframeTint));
                }

                GFX::Engine.raster_func200(v29);
            }
        }
    }
}

void yw_RenderInfoVehicleWire(NC_STACK_ypaworld *yw, sklt_wis *wis, World::TVhclProto *vhcl, float a4, float a5, float a6, int owner)
{
    SDL_Color color_25 = yw->GetColor(25);
    SDL_Color color_34 = yw->GetColor(34);

    UAskeleton::Data *wairufureimu = nullptr;

    wis_color_func func = nullptr;

    if ( wis->field_72 )
        func = wis_color;

    if ( vhcl->wireframe )
        wairufureimu = vhcl->wireframe->GetSkelet();

    if ( wairufureimu )
    {
        float v15 = wis->field_92 * 6.0 * 0.8;

        float a9 = 0.1;

        if ( a6 < 1.4 )
            a9 = a6 * 0.1;

        yw->_hud.cl1_r = color_34.r;
        yw->_hud.cl1_g = color_34.g;
        yw->_hud.cl1_b = color_34.b;
        yw->_hud.cl2_r = color_25.r;
        yw->_hud.cl2_b = color_25.b;
        yw->_hud.cl2_g = color_25.g;

        yw_RenderVector2D(yw, wairufureimu, a4, a5, 1.0, 0.0, 0.0, 1.0, a9, v15, color_34, nullptr, func, true, &World::GetOwnerWireframeTint(owner));
    }
}

void yw_RenderInfoLifebar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos)
{
    int a6a;
    int v10;

    if ( bact )
    {
        a6a = bact->_energy;
        v10 = bact->_energy_max;
    }
    else
    {
        v10 = vhcl->energy;
        a6a = vhcl->energy;
    }

    sub_4E4F80(yw, wis, cur, xpos, ypos, a6a, v10, 2, 6, Locale::Text::HUD(Locale::HUDSTR_EGY), fmt::sprintf("%d", (a6a + 99) / 100));
}

void yw_RenderInfoShieldbar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos)
{
    int v10;

    if ( bact )
        v10 = (int)bact->GetEffectiveShield();
    else
        v10 = vhcl->shield;

    sub_4E4F80(yw, wis, cur, xpos, ypos, v10, 100, 1, 5,
               Locale::Text::HUD(Locale::HUDSTR_AMR), fmt::sprintf("%d%%", v10), 2);
}

void yw_RenderInfoVehicleName(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur,
                              const std::string &name, float xpos, float ypos)
{
    int v29 = (yw->_screenSize.x / 2) * xpos;
    int v33 = (yw->_screenSize.y / 2) * ypos;

    int v31 = yw->_screenSize.x * wis->field_8A;

    SDL_Color v21 = yw->GetColor(25);
    SDL_Color a11 = yw->GetColor(34);

    wis_color_func a13;

    if ( wis->field_72 )
        a13 = wis_color;
    else
        a13 = nullptr;

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

    int v30 = v29 - (v31 / 2);
    int v34 = v33 - (yw->_fontH / 2);

    if ( !wnd_vis || v29 <= wnd_xpos || v29 >= wnd_xpos2 || v33 <= wnd_ypos || v33 >= wnd_ypos2 )
    {
        FontUA::select_tileset(cur, 15);

        FontUA::set_center_xpos(cur, v30);
        FontUA::set_center_ypos(cur, v34);

        // Owner tint belongs only to the rendered wireframe graphics.
        // Names keep the original high-contrast UI colour so dark tints do not
        // make vehicle or weapon labels unreadable over the 3D world.
        FontUA::set_txtColor(cur, yw->_iniColors[65].r,
                             yw->_iniColors[65].g,
                             yw->_iniColors[65].b);

        sub_451714_with_yoffset(yw->_guiTiles[15], cur, name, v31, 32, 0);
    }

    if ( wis->sklts_intern[2] )
    {
        yw->_hud.cl1_r = a11.r;
        yw->_hud.cl1_g = a11.g;
        yw->_hud.cl1_b = a11.b;
        yw->_hud.cl2_r = v21.r;
        yw->_hud.cl2_b = v21.b;
        yw->_hud.cl2_g = v21.g;

        float a4 = (1.0 / (float)yw->_screenSize.y) * 2.0 + ypos;

        float a10 = wis->field_92 * 1.2;

        yw_RenderVector2D(yw, wis->sklts_intern[2], xpos, a4, 1.0, 0.0, 0.0, 1.0, wis->field_8A, a10, a11, nullptr, a13, true);
    }
}

void yw_RenderInfoWeaponName(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur,
                             const std::string &name, float xpos, float ypos)
{
    if ( !System::IniConf::UiWeaponName.Get<bool>() || name.empty() )
        return;

    yw_RenderInfoVehicleName(yw, wis, cur, name, xpos, ypos);
}

void yw_RenderInfoWeaponLikeWire(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                        NC_STACK_skeleton *wireframe,
                                        const World::TVisualTint *wireframeTint,
                                        float xpos, float ypos)
{
    if ( !wireframe )
        return;

    UAskeleton::Data *wairufureimu = wireframe->GetSkelet();
    if ( !wairufureimu )
        return;

    wis_color_func func = wis->field_72 ? wis_color : nullptr;

    SDL_Color v9 = yw->GetColor(20);
    SDL_Color v10 = yw->GetColor(29);

    yw->_hud.cl1_r = v10.r;
    yw->_hud.cl1_g = v10.g;
    yw->_hud.cl1_b = v10.b;
    yw->_hud.cl2_r = v9.r;
    yw->_hud.cl2_b = v9.b;
    yw->_hud.cl2_g = v9.g;

    yw_RenderVector2D(yw, wairufureimu, xpos, ypos,
                      1.0, 0.0, 0.0, 1.0, 0.0415, 0.05,
                      v10, nullptr, func, true, wireframeTint);
}

void yw_RenderInfoWeaponWire(NC_STACK_ypaworld *yw, sklt_wis *wis, World::TWeapProto *wpn, float xpos, float ypos, int owner)
{
    if ( wpn )
        yw_RenderInfoWeaponLikeWire(yw, wis, wpn->wireframe, &World::GetOwnerWireframeTint(owner),
                                    xpos, ypos);
}

void yw_RenderInfoReloadbar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TWeapProto *wpn, float xpos, float ypos)
{
    if ( wpn )
    {
        int v12;

        if ( bact )
        {

            bool laserLike = wpn->IsLaser();
            int v10 = laserLike ? wpn->laser_energy_tick_time_user : wpn->shot_time_user;
            if ( v10 <= 0 )
                v10 = laserLike ? 150 : 1000;

            bool usingSalveDelay = false;
            if ( !laserLike && wpn->salve_shots )
            {
                if ( bact->_salve_counter >= wpn->salve_shots )
                {
                    v10 = wpn->salve_delay;
                    usingSalveDelay = true;
                }
            }

            // The HUD must use the same effective cooldown as LaunchMissile().
            // salve_delay and laser tick timing intentionally keep their own
            // existing semantics.
            if ( !laserLike && !usingSalveDelay )
            {
                v10 = bact->GetProgressiveWeaponShotTime(*wpn, v10);
                v10 = bact->GetEffectiveShotTime(v10, false);
            }

            v12 = 100 * (bact->_clock - bact->_weapon_time) / std::max(1, v10);

            if ( v12 >= 100 )
                v12 = 100;
        }
        else
            v12 = 100;

        std::string txt2;

        if ( v12 == 100 )
            txt2 = Locale::Text::HUD(Locale::HUDSTR_OK);
        else
            txt2 = fmt::sprintf("%d%%", v12);

        sub_4E4F80(yw, wis, cur, xpos, ypos, v12, 100, 1, 3, Locale::Text::HUD(Locale::HUDSTR_RLD), txt2);
    }
}

bool yw_HasInfoMagazineBar(const World::TWeapProto *wpn)
{
    return wpn && !wpn->IsKamikaze() && wpn->salve_shots > 0 &&
           !wpn->IsLaser() && !wpn->IsArtilleryShell();
}

void yw_RenderInfoMagazineBar(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                     CmdStream *cur, NC_STACK_ypabact *bact,
                                     World::TWeapProto *wpn,
                                     float xpos, float ypos)
{
    // salve_shots belongs to the normal projectile firing path. Special
    // continuous/barrage payloads bypass that vanilla salve counter.
    if ( !yw_HasInfoMagazineBar(wpn) )
        return;

    int magazine = 100;

    if ( bact )
    {
        const int salveShots = std::max(1, wpn->salve_shots);

        if ( bact->_salve_counter >= salveShots )
        {
            // A completed salve is empty. salve_delay is the native refill
            // cooldown, so visualize that same timer filling the magazine
            // back to 100%. A zero delay means an instantaneous refill.
            if ( wpn->salve_delay > 0 )
            {
                const int elapsed = std::max(0, bact->_clock - bact->_weapon_time);
                magazine = 100 * elapsed / wpn->salve_delay;
                magazine = std::max(0, std::min(100, magazine));
            }
            else
            {
                magazine = 100;
            }
        }
        else
        {
            const int remaining = std::max(0, salveShots - bact->_salve_counter);
            magazine = 100 * remaining / salveShots;
            magazine = std::max(0, std::min(100, magazine));
        }
    }

    const std::string text = magazine == 100
        ? Locale::Text::HUD(Locale::HUDSTR_OK)
        : fmt::sprintf("%d%%", magazine);

    // Reuse the classic MAPMISC segmented Weapon bar renderer unchanged.
    // Tile 3 is the existing white segment; tile 5 is the existing grey
    // background segment. No new HUD style or drawing primitive is needed.
    sub_4E4F80(yw, wis, cur, xpos, ypos, magazine, 100, 3, 5,
               Locale::Text::OpenUA(Locale::OUA_HUD_MAG), text);
}

std::string yw_BuildInfoWeaponDamageText(NC_STACK_ypabact *bact,
                                                    World::TVhclProto *vhcl,
                                                    World::TWeapProto *weap,
                                                    int effectiveEnergy)
{
    if ( !weap )
        return std::string();

    std::string txt2;

    if ( weap->IsLaser() )
    {
        txt2 = fmt::sprintf("%d", effectiveEnergy / 100);

        if ( weap->laser_energy_increment_rate > 0.0f )
            txt2 += " +";

        if ( weap->laser_beam_count > 1 )
            txt2 += fmt::sprintf(" x%d", weap->laser_beam_count);
    }
    else
    {
        txt2 = fmt::sprintf("%d", effectiveEnergy / 100);

        int minWeaponCount = 1;
        int maxWeaponCount = 1;
        if ( bact )
        {
            bact->GetCurrentWeaponProjectileCountRange(
                &minWeaponCount, &maxWeaponCount);
        }
        else if ( vhcl )
        {
            vhcl->GetWeaponProjectileCountRange(
                0, minWeaponCount, maxWeaponCount);
        }

        int minDisplayCount = minWeaponCount;
        int maxDisplayCount = maxWeaponCount;
        weap->GetCombinedProjectileCountRange(
            minWeaponCount, maxWeaponCount,
            minDisplayCount, maxDisplayCount);

        if ( maxDisplayCount > 1 )
        {
            if ( minDisplayCount == maxDisplayCount )
                txt2 += fmt::sprintf("x%d", maxDisplayCount);
            else
                txt2 += fmt::sprintf("x%d-%d", minDisplayCount,
                                     maxDisplayCount);
        }
    }

    return txt2;
}

int yw_GetInfoHudTextWidth(NC_STACK_ypaworld *yw,
                                  const std::string &text)
{
    if ( !yw || text.empty() )
        return 0;

    const int screenWidth = GFX::Engine.MeasureScreenTextWidth(text);
    if ( screenWidth > 0 )
        return screenWidth;

    return yw->_guiTiles[15] ? yw->_guiTiles[15]->GetWidth(text) : 0;
}

int yw_GetInfoWeaponEffectIcons(NC_STACK_ypaworld *yw,
                                       World::TVhclProto *vhcl,
                                       World::TWeapProto *weap,
                                       int iconSize,
                                       std::array<NC_STACK_bitmap *, 2> *bitmaps)
{
    if ( bitmaps )
        bitmaps->fill(nullptr);

    if ( !yw || !weap )
        return 0;

    StatusIconList paths;
    int pathCount = 0;

    // Weapon capability icons share the same DMG-row renderer as Debuff icons.
    // Keep the Debuff last so it remains closest to the damage value when both
    // are configured.
    if ( weap->armor_penetration_targets > 0 )
        StatusIconAdd(paths, pathCount,
                      StatusIconTrimPath(weap->armor_penetration_icon));

    if ( weap->debuff.valid )
        StatusIconAdd(paths, pathCount, StatusIconTrimPath(weap->debuff.icon));

    int iconCount = 0;
    for (int i = 0; i < pathCount && iconCount < 2; ++i)
    {
        NC_STACK_bitmap *bitmap = StatusIconLoad(paths[i], iconSize, iconSize);
        if ( !bitmap || !bitmap->GetBitmap() )
            continue;

        if ( bitmaps )
            (*bitmaps)[iconCount] = bitmap;
        ++iconCount;
    }

    return iconCount;
}

int yw_GetInfoWeaponIconSize(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return 14;

    return std::max(12, std::min(20, yw->_fontH));
}

int yw_GetInfoWeaponRightContentWidth(NC_STACK_ypaworld *yw,
                                             World::TVhclProto *vhcl,
                                             World::TWeapProto *weap,
                                             const std::string &damageText,
                                             int *iconCountOut)
{
    constexpr int iconSpacing = 2;
    constexpr int iconTextGap = 4;

    const int iconSize = yw_GetInfoWeaponIconSize(yw);
    const int iconCount = yw_GetInfoWeaponEffectIcons(
        yw, vhcl, weap, iconSize, nullptr);

    if ( iconCountOut )
        *iconCountOut = iconCount;

    const int textWidth = yw_GetInfoHudTextWidth(yw, damageText);
    const int iconBlockWidth = iconCount > 0
        ? iconCount * iconSize + (iconCount - 1) * iconSpacing
        : 0;

    return textWidth + iconBlockWidth +
           ((iconCount > 0 && !damageText.empty()) ? iconTextGap : 0);
}

float yw_GetInfoDamageWireX(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                   float vanillaX,
                                   World::TVhclProto *vhcl,
                                   World::TWeapProto *weap,
                                   const std::string &damageText)
{
    if ( !yw || !wis || !yw->_guiTiles[51] || yw->_screenSize.x <= 0 )
        return vanillaX;

    const int rightContentWidth = yw_GetInfoWeaponRightContentWidth(
        yw, vhcl, weap, damageText);
    const int labelBarGap = std::max(2, yw->_guiTiles[51]->map[1].w / 2);
    const int wireAreaWidth = yw->_guiTiles[51]->map[7].w * wis->field_9E;

    // sub_4E4F80() keeps DMG and the right-aligned damage text on fixed authored
    // edges. Derive the wireframe position from those same edges instead of
    // nudging the vanilla centre by an estimated text overflow. This keeps the
    // wireframe geometrically centred in the space that is actually free.
    const int rowCenter = (yw->_screenSize.x / 2) * vanillaX;
    const int rowWidth = wis->field_96 + labelBarGap + wireAreaWidth +
                         wis->field_9A;
    const int rowLeft = rowCenter - rowWidth / 2;
    const int wireLeft = rowLeft + wis->field_96 + labelBarGap;
    const int authoredWireRight = wireLeft + wireAreaWidth;
    const int rowRight = rowLeft + rowWidth;

    // Long damage strings and the optional Debuff icon grow left from the fixed
    // right edge. They are allowed to reduce the free wireframe area, but never
    // move its right boundary outside the original authored Weapon slot.
    const int contentLeft = rowRight - rightContentWidth;
    const int wireRight = std::min(authoredWireRight, contentLeft);

    constexpr int minWireAreaPixels = 8;
    const int safeWireRight = std::max(wireLeft + minWireAreaPixels, wireRight);
    const int wireCenter = wireLeft + (safeWireRight - wireLeft) / 2;

    return 2.0f * static_cast<float>(wireCenter) /
           static_cast<float>(yw->_screenSize.x);
}

void yw_RenderInfoWeaponEffectIcons(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                           World::TVhclProto *vhcl,
                                           World::TWeapProto *weap,
                                           const std::string &damageText,
                                           float xpos, float ypos)
{
    if ( !yw || !wis || !weap )
        return;

    constexpr int iconSpacing = 2;
    constexpr int iconTextGap = 4;

    const int iconSize = yw_GetInfoWeaponIconSize(yw);
    std::array<NC_STACK_bitmap *, 2> bitmaps;
    const int iconCount = yw_GetInfoWeaponEffectIcons(
        yw, vhcl, weap, iconSize, &bitmaps);
    if ( iconCount <= 0 )
        return;

    const int textWidth = yw_GetInfoHudTextWidth(yw, damageText);
    const int iconBlockWidth = iconCount * iconSize +
                               (iconCount - 1) * iconSpacing;
    const int labelBarGap = std::max(2, yw->_guiTiles[51]->map[1].w / 2);
    const int wireAreaWidth = yw->_guiTiles[51]->map[7].w * wis->field_9E;

    int rightEdge = (yw->_screenSize.x / 2) * xpos;
    rightEdge -= (wis->field_9A + wis->field_96 + labelBarGap +
                  wireAreaWidth) / 2;
    rightEdge += wis->field_96 + labelBarGap + wireAreaWidth +
                 wis->field_9A;

    const int iconBlockRight = rightEdge - textWidth -
                               (damageText.empty() ? 0 : iconTextGap);
    const int iconLeft = iconBlockRight - iconBlockWidth;
    const int centerY = (yw->_screenSize.y / 2) * ypos;
    const int screenLeft = iconLeft + yw->_screenSize.x / 2;
    const int screenTop = centerY + yw->_screenSize.y / 2 - iconSize / 2;

    for (int i = 0; i < iconCount; ++i)
    {
        StatusIconRenderBitmap(yw, bitmaps[i],
                               screenLeft + i * (iconSize + iconSpacing),
                               screenTop, iconSize, 255);
    }
}

void yw_RenderInfoWeaponInf(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, World::TWeapProto *weap, float xpos, float ypos)
{
    if ( weap )
    {
        const int displayBaseEnergy = weap->energy;
        const int effectiveEnergy = bact
            ? bact->GetEffectiveOutgoingEnergy(displayBaseEnergy)
            : displayBaseEnergy;

        // Show the actual current per-instance damage directly in DMG. Kill
        // marks stay runtime-only and never mutate shared weapon prototypes,
        // but the player readout now reflects the same effective value used by
        // the final damage choke point.
        const std::string txt2 = yw_BuildInfoWeaponDamageText(
            bact, vhcl, weap, effectiveEnergy);

        sub_4E4F80(yw, wis, cur, xpos, ypos, effectiveEnergy, 100, 7, 7,
                   Locale::Text::HUD(Locale::HUDSTR_EGY), txt2, 1 | 2);
    }
}

void yw_RenderInfoMgunReloadbar(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                       CmdStream *cur, NC_STACK_ypabact *bact,
                                       World::TVhclProto *vhcl,
                                       float xpos, float ypos)
{
    if ( !vhcl || vhcl->mgun_shot_time <= 0 )
        return;

    int reload = 100;
    if ( bact )
    {
        const int shotTime = bact->GetMinigunShotTime(1);
        if ( shotTime <= 0 )
            return;

        reload = 100 * (bact->_clock - bact->_mgun_time) / shotTime;
        reload = std::max(0, std::min(100, reload));
    }

    const std::string text = reload == 100
        ? Locale::Text::HUD(Locale::HUDSTR_OK)
        : fmt::sprintf("%d%%", reload);

    sub_4E4F80(yw, wis, cur, xpos, ypos, reload, 100, 1, 3,
               Locale::Text::HUD(Locale::HUDSTR_RLD), text);
}

std::string yw_BuildInfoMgunDamageText(int effectivePower)
{
    return fmt::sprintf("%d", effectivePower / 100);
}

void yw_RenderInfoMgunDamage(NC_STACK_ypaworld *yw, sklt_wis *wis,
                                    CmdStream *cur, NC_STACK_ypabact *bact,
                                    World::TVhclProto *vhcl,
                                    float xpos, float ypos)
{
    if ( !vhcl )
        return;

    const int basePower = (int)(bact
        ? bact->GetMinigunEnergy()
        : (vhcl->mgun_energy_set ? vhcl->mgun_energy : vhcl->gun_power));
    const int effectivePower = bact
        ? bact->GetEffectiveOutgoingEnergy(basePower)
        : basePower;

    const std::string text = yw_BuildInfoMgunDamageText(effectivePower);

    sub_4E4F80(yw, wis, cur, xpos, ypos, effectivePower, 100, 7, 7,
               Locale::Text::HUD(Locale::HUDSTR_EGY), text, 1 | 2);
}

void yw_RenderHUDInfo(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float xpos, float ypos, NC_STACK_ypabact *bact, int vhclid, int flag)
{
    int v23 = 1;
    int v11 = 1;
    int v22 = 1;
    int v25 = 1;

    if ( flag & 1 )
        v23 = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 2 )
        v11 = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 4 )
        v22 = ((yw->_timeStamp / 200) & 1) != 0;

    if ( flag & 8 )
        v25 = ((yw->_timeStamp / 200) & 1) != 0;

    float a6a;

    if ( flag & 0x10 )
    {
        a6a = (yw->_timeStamp - wis->field_76 - 200) / 180.0;

        if ( a6a <= 0.0 )
            return;
    }
    else
    {
        a6a = 1.0;
    }

    if ( vhclid == -1 && bact )
        vhclid = ResolveUnitDisplayVehicleProtoId(yw, bact);

    if ( vhclid < 0 || (size_t)vhclid >= yw->_vhclProtos.size() )
        return;

    World::TVhclProto *vhcl = &yw->_vhclProtos[vhclid];


    World::TWeapProto *weap;
    int weaponId = vhcl->weapon;

    if ( bact )
        weaponId = bact->GetHUDWeaponId();

    if ( weaponId == -1 )
        weap = nullptr;
    else
        weap = &yw->_weaponProtos.at(weaponId);

    // Match the unit wireframe headroom to the optional MAG row. Without
    // that row the position remains the vanilla ypos anchor.
    const bool hasWeaponMagazine = yw_HasInfoMagazineBar(weap);

    if ( v25 )
        yw_RenderInfoVehicleWire(yw, wis, vhcl, xpos,
                                 ypos + (hasWeaponMagazine ? wis->field_92 : 0.0f), a6a, bact ? bact->_owner : (yw->_userRobo ? yw->_userRobo->_owner : 0));

    if ( v11 )
        yw_RenderInfoLifebar(yw, wis, cur, bact, vhcl, xpos,   wis->field_92 * 7.0 + ypos  );

    if ( v22 )
        yw_RenderInfoShieldbar(yw, wis, cur, bact, vhcl, xpos,   wis->field_92 * 9.0 + ypos  );

    if ( v25 )
    {
        float v15 = wis->field_92 * 12.0 + ypos;
        World::TVhclProto *nameVhcl = vhcl;
        if ( bact && bact->_vehicleID >= 0 && (size_t)bact->_vehicleID < yw->_vhclProtos.size() )
            nameVhcl = &yw->_vhclProtos[bact->_vehicleID];

        yw_RenderInfoVehicleName(yw, wis, cur,
                                 yw->ResolveGameplayVehicleName(bact, *nameVhcl),
                                 xpos, v15);
    }

    const bool showMgunWeaponInfo = yw_IsMgunWeaponInfoAvailable(bact, vhcl);

    if ( v23 && showMgunWeaponInfo )
    {
        yw_RenderInfoWeaponName(yw, wis, cur, vhcl->mgun_name, xpos,
                                ypos - wis->field_92 * 12.0);

        const int mgunBasePower = (int)(bact
            ? bact->GetMinigunEnergy()
            : (vhcl->mgun_energy_set ? vhcl->mgun_energy : vhcl->gun_power));
        const int mgunEffectivePower = bact
            ? bact->GetEffectiveOutgoingEnergy(mgunBasePower)
            : mgunBasePower;
        const std::string mgunDamageText = yw_BuildInfoMgunDamageText(mgunEffectivePower);
        const float mgunWireX = yw_GetInfoDamageWireX(
            yw, wis, xpos, nullptr, nullptr, mgunDamageText);

        yw_RenderInfoWeaponLikeWire(yw, wis, vhcl->mgun_wireframe,
                                    &World::GetOwnerWireframeTint(bact ? bact->_owner : 0), mgunWireX,
                                    ypos - wis->field_92 * 9.0);
        yw_RenderInfoMgunReloadbar(yw, wis, cur, bact, vhcl, xpos,
                                   ypos - wis->field_92 * 7.0);
        yw_RenderInfoMgunDamage(yw, wis, cur, bact, vhcl, xpos,
                                ypos - wis->field_92 * 9.0);
    }
    else if ( weap && v23 )
    {
        yw_RenderInfoWeaponName(yw, wis, cur,
                                yw->ResolveGameplayWeaponName(*weap), xpos,
                                ypos - wis->field_92 * 12.0);

        const int weaponBaseEnergy = weap->energy;
        const int weaponEffectiveEnergy = bact
            ? bact->GetEffectiveOutgoingEnergy(weaponBaseEnergy)
            : weaponBaseEnergy;
        const std::string weaponDamageText = yw_BuildInfoWeaponDamageText(
            bact, vhcl, weap, weaponEffectiveEnergy);
        const float weaponWireX = yw_GetInfoDamageWireX(
            yw, wis, xpos, vhcl, weap, weaponDamageText);

        yw_RenderInfoWeaponWire(yw, wis, weap, weaponWireX,
                                ypos - wis->field_92 * 9.0, bact ? bact->_owner : (yw->_userRobo ? yw->_userRobo->_owner : 0));

        // A Kamikaze payload is not a normally fireable/cooldown weapon, so
        // keep its name/wireframe/DMG visible without inventing a fake RLD.
        if ( !weap->IsKamikaze() )
        {
            yw_RenderInfoReloadbar(yw, wis, cur, bact, weap, xpos,
                                   ypos - wis->field_92 * 7.0);
            yw_RenderInfoMagazineBar(yw, wis, cur, bact, weap, xpos,
                                     ypos - wis->field_92 * 5.0);
        }

        const float weaponDamageY = ypos - wis->field_92 * 9.0;
        yw_RenderInfoWeaponInf(yw, wis, cur, bact, vhcl, weap, xpos,
                               weaponDamageY);
        yw_RenderInfoWeaponEffectIcons(yw, wis, vhcl, weap, weaponDamageText,
                                       xpos, weaponDamageY);
    }
}

std::string NC_STACK_ypaworld::BuildNewGemNotificationLogText()
{
    std::vector<GemNotificationLine> lines;
    yw_BuildNewGemNotificationLines(this, &lines);

    // Reuse the exact visible strings selected for the center-screen popup.
    // Spacer-only rows are layout, not text, so they are not added to the info log.
    std::string text;
    for (const GemNotificationLine &line : lines)
    {
        if ( line.Text.empty() )
            continue;

        if ( !text.empty() )
            text += '\n';
        text += line.Text;
    }

    return text;
}

void wis_color2(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2)
{
    float v7 = yw->_hud.field_86 * 6.28;

    float v24 = cos(v7);
    float v22 = sin(v7);

    float v23 = sqrt(POW2(x1) + POW2(y1));
    if ( v23 > 0.0 )
    {
        x1 /= v23;
        y1 /= v23;
    }

    float v21 = sqrt(POW2(x2) + POW2(y2));
    if ( v21 > 0.0 )
    {
        x2 /= v21;
        y2 /= v21;
    }

    float v12 = (v24 * x1 + v22 * y1 + 1.0) * 0.5;
    int v13 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v12 + yw->_hud.cl1_r;
    int v16 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v12 + yw->_hud.cl1_b;
    int v15 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v12 + yw->_hud.cl1_g;
    *out1 = GFX::Engine.Color(v13, v15, v16);

    float v17 = (v24 * x2 + v22 * y2 + 1.0) * 0.5;
    int v18 = (yw->_hud.cl2_g - yw->_hud.cl1_g) * v17 + yw->_hud.cl1_g;
    int v20 = (yw->_hud.cl2_r - yw->_hud.cl1_r) * v17 + yw->_hud.cl1_r;
    int v19 = (yw->_hud.cl2_b - yw->_hud.cl1_b) * v17 + yw->_hud.cl1_b;
    *out2 = GFX::Engine.Color(v20, v18, v19);
}

void yw_RenderHUDCompass(NC_STACK_ypaworld *yw, sklt_wis *wis)
{
    float v90 = (yw->_timeStamp - wis->field_76) / 180.0;

    wis_color_func func = nullptr;

    if ( wis->field_72 )
        func = wis_color2;

    float xx = yw->_viewerRotation.m02;
    float yy = yw->_viewerRotation.m22;

    float v70 = sqrt( POW2(xx) + POW2(yy) );

    if ( v70 > 0.0 )
    {
        xx /= v70;
        yy /= v70;
    }

    SDL_Color v9 = yw->GetColor(21);
    SDL_Color v73 = yw->GetColor(30);

    float tx = yw->_userUnit->_parent->_position.x - yw->_userUnit->_position.x;
    float ty = yw->_userUnit->_parent->_position.z - yw->_userUnit->_position.z;

    float v13 = tx * yy + ty * xx;
    float v15 = tx * -xx + ty * yy;

    float v17 = sqrt( POW2(v13) + POW2(v15) );

    if ( v17 > 30.0 )
    {
        float v20 = v13 * (1.0 / v17);
        float v23 = v15 * (1.0 / v17);

        yw->_hud.cl1_r = v9.r;
        yw->_hud.cl1_g = v9.g;
        yw->_hud.cl1_b = v9.b;
        yw->_hud.cl2_r = v73.r;
        yw->_hud.cl2_b = v73.b;
        yw->_hud.cl2_g = v73.g;

        yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, v23, -v20, v20, v23, 0.25, 0.3, v9, func, 0, true);
    }

    if ( yw->_userUnit->IsParentMyRobo() )
    {
        int v27 = 0;

        if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_CELL )
        {
            v27 = 1;
            ty = yw->_userUnit->_primTpos.z - yw->_userUnit->_position.z;
            tx = yw->_userUnit->_primTpos.x - yw->_userUnit->_position.x;
        }
        else if ( yw->_userUnit->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            v27 = 1;
            tx = yw->_userUnit->_primT.pbact->_position.x - yw->_userUnit->_position.x;
            ty = yw->_userUnit->_primT.pbact->_position.z - yw->_userUnit->_position.z;
        }

        if ( v27 )
        {
            SDL_Color v31 = yw->GetColor(22);
            SDL_Color v74 = yw->GetColor(31);

            float v88 = tx * yy + ty * xx;
            float a8 = tx * -xx + ty * yy;

            float v36 = sqrt( POW2(v88) + POW2(a8) );

            if ( v36 > 30.0 )
            {
                float v40 = v88 * (1.0 / v36);
                float v42 = a8 * (1.0 / v36);

                yw->_hud.cl1_r = v31.r;
                yw->_hud.cl1_g = v31.g;
                yw->_hud.cl1_b = v31.b;
                yw->_hud.cl2_r = v74.r;
                yw->_hud.cl2_b = v74.b;
                yw->_hud.cl2_g = v74.g;

                yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, v42, -v40, v40, v42, 0.25, 0.3, v31, func, 0, true);
            }
        }
    }

    if ( yw->_guiVisor.field_18 )
    {
        SDL_Color a11 = yw->GetColor(23);
        SDL_Color v50 = yw->GetColor(32);

        float v45 = yw->_guiVisor.field_18->_position.x - yw->_userUnit->_position.x;
        float v46 = yw->_guiVisor.field_18->_position.z - yw->_userUnit->_position.z;

        float v81 = v45 * yy + v46 * xx;
        float v79 = v45 * -xx + v46 * yy;


        float v52 = sqrt( POW2(v81) + POW2(v79) );

        if ( v52 > 30.0 )
        {
            float v55 = v81 * (1.0 / v52);
            float v58 = v79 * (1.0 / v52);

            yw->_hud.cl1_r = a11.r;
            yw->_hud.cl1_g = a11.g;
            yw->_hud.cl1_b = a11.b;
            yw->_hud.cl2_r = v50.r;
            yw->_hud.cl2_b = v50.b;
            yw->_hud.cl2_g = v50.g;

            yw_RenderVector2D(yw, wis->sklts_intern[3], 0.7, 0.3, v58, -v55, v55, v58, 0.25, 0.3, a11, func, 0, true);
        }
    }

    SDL_Color v60 = yw->GetColor(24);
    SDL_Color v75 = yw->GetColor(33);

    float v72 = 0.25;
    float v71 = 0.3;

    if ( v90 < 1.4 )
    {
        v71 = v90 * 0.3;
        v72 = v90 * 0.25;
    }

    yw->_hud.cl1_r = v60.r;
    yw->_hud.cl1_g = v60.g;
    yw->_hud.cl1_b = v60.b;
    yw->_hud.cl2_r = v75.r;
    yw->_hud.cl2_b = v75.b;
    yw->_hud.cl2_g = v75.g;

    yw_RenderVector2D(yw, wis->sklts_intern[1], 0.7, 0.3, yy, -xx, xx, yy, v72, v71, v60, func, 0, true);

    int v64;

    switch ( yw->_userUnit->_pSector->owner )
    {
    case 0:
        v64 = 0;
        break;

    case 1:
        v64 = 1;
        break;

    case 2:
        v64 = 2;
        break;

    case 3:
        v64 = 3;
        break;

    case 4:
        v64 = 4;
        break;

    case 5:
        v64 = 5;
        break;

    case 6:
        v64 = 6;
        break;

    case 7:
        v64 = 7;
        break;

    default:
        v64 = 0;
        break;
    }

    SDL_Color v65 = yw->GetColor(v64);

    yw->_hud.cl1_r = v65.r;
    yw->_hud.cl1_g = v65.g;
    yw->_hud.cl1_b = v65.b;
    yw->_hud.cl2_r = v65.r;
    yw->_hud.cl2_b = v65.b;
    yw->_hud.cl2_g = v65.g;

    yw_RenderVector2D(yw, wis->sklts_intern[2], 0.7, 0.3, yy, -xx, xx, yy, 0.07, 0.08, v65, func, 0, true);
}

void yw_RenderHUDVectorGFX(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    GFX::Engine.raster_func210( Common::FRect(-1.0, -1.0, 1.0, 1.0) );

    Common::Rect v7;

    if ( !robo_map.IsClosed() )
    {
        v7.left = robo_map.x - (yw->_screenSize.x / 2);
        v7.top = robo_map.y - (yw->_screenSize.y / 2);
        v7.right = robo_map.w + v7.left;
        v7.bottom = robo_map.h + v7.top;
    }

    GFX::Engine.raster_func221(v7);

    sklt_wis *wis = &yw->_hud;

    yw_RenderHUDCompass(yw, wis);

    GFX::Engine.raster_func221(Common::Rect());

    yw_RenderHUDTarget(yw, wis);

    GFX::Engine.raster_func221(v7);

    // Keep the complete left cockpit information cluster continuously visible.
    // The procedural HP bar changes tint with health but never adds a custom
    // pulse/blink; reticle, targeting and the vanilla HUD cadence stay untouched.
    yw_RenderHUDInfo(yw, wis, cur, -0.7, 0.3, yw->_userUnit, -1, 0x10);

    if ( robo_map.IsOpen() )
        GFX::Engine.raster_func221(Common::Rect());

    if ( yw->_userUnit && yw->_userUnit->_vehicleID >= 0 && (size_t)yw->_userUnit->_vehicleID < yw->_vhclProtos.size() )
    {
        uint8_t protoId = yw->_userUnit->_mimic_disguise_vehicleID ? yw->_userUnit->_mimic_disguise_vehicleID : yw->_userUnit->_vehicleID;
        if ( (size_t)protoId < yw->_vhclProtos.size() )
        {
            constexpr float hudX = -0.7f;
            constexpr float hudY = 0.3f;
            StatusIconRenderCockpit(yw, wis, yw->_userUnit,
                                    &yw->_vhclProtos[protoId], hudX, hudY);

            const uint8_t killMarks = yw->_userUnit->GetSessionKillMarks();
            if ( killMarks > 0 )
            {
                constexpr int markGap = 2;
                constexpr int markHeight = 3;
                const float weaponNameY = hudY - wis->field_92 * 12.0f;
                const int weaponNameCenterX = (yw->_screenSize.x / 2) +
                                              (yw->_screenSize.x / 2) * hudX;
                const int weaponNameCenterY = (yw->_screenSize.y / 2) +
                                              (yw->_screenSize.y / 2) * weaponNameY;
                const int tipYScreen = weaponNameCenterY - yw->_fontH - markGap;
                if ( tipYScreen - markHeight >= 4 )
                {
                    const int killMarkCenterX = weaponNameCenterX -
                                                yw->_screenSize.x / 2;
                    const int tipY = tipYScreen - yw->_screenSize.y / 2;
                    SDL_Color markerColor = yw->GetColor(yw->_userUnit->_owner);
                    SDL_Color shadowColor = yw_GetNeutralSelectionShadowColor();

                    yw_DrawWorldKillMarks(killMarkCenterX + 1, tipY + 1,
                                          killMarks, shadowColor);
                    yw_DrawWorldKillMarks(killMarkCenterX, tipY,
                                          killMarks, markerColor);
                }
            }
        }
    }
}

bool yw_IsUfoSpyHudActive(NC_STACK_ypaworld *yw)
{
    return yw &&
           yw->IsUfoSpyUiControlContext() &&
           yw->IsUfoSpyUiEnabled();
}

uint8_t yw_GetUfoSpyWorldOpacity(NC_STACK_ypaworld *yw,
                                        NC_STACK_ypabact *bact)
{
    if ( !yw || !yw->IsUfoSpyUiControlContext() || !bact )
        return 0;

    const uint8_t playerOwner = yw->_userRobo->_owner;
    if ( bact == yw->_userUnit ||
         bact->_owner == World::OWNER_0 ||
         bact->_energy <= 0 ||
         bact->_energy_max <= 0 ||
         bact->_bact_type == BACT_TYPES_MISSLE ||
         bact->_status == BACT_STATUS_CREATE ||
         bact->_status == BACT_STATUS_BEAM ||
         bact->_status == BACT_STATUS_DEAD ||
         (bact->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 |
                              BACT_STFLAG_CLEAN | BACT_STFLAG_NORENDER)) ||
         bact->ShouldHideFromStrategicUI() ||
         bact->IsInvisibleUnrevealed() ||
         bact->IsHiddenFor(playerOwner) )
    {
        return 0;
    }

    const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, bact->_position);
    if ( worldUiOpacity == 0 )
        return 0;

    const float spyRadius = yw->GetUfoSpyUiRadius();
    if ( spyRadius <= 0.0f )
        return 0;

    // Keep Spy markers inside the same real 3D world window used by the
    // renderer. The authored Spy radius remains an independent horizontal
    // sensor distance and is never modified by optical zoom.
    const float renderDistance = (float)yw->getYW_normVisLimit();
    if ( renderDistance > 0.0f )
    {
        const vec3d renderDelta = bact->_position - yw->_viewerPosition;
        if ( renderDelta.dot(renderDelta) >= renderDistance * renderDistance )
            return 0;
    }

    const float dx = bact->_position.x - yw->_userUnit->_position.x;
    const float dz = bact->_position.z - yw->_userUnit->_position.z;
    const float horizontalDistance = sqrt(dx * dx + dz * dz);
    if ( horizontalDistance >= spyRadius )
        return 0;

    // Reuse the exact world-UI smoothstep curve for the UFO sensor boundary.
    // This gives Resistance and enemy arrows the same fade-in/fade-out instead
    // of allowing enemy markers to pop at the edge of the scanned area.
    const float fadeStart = yw_GetWorldUiFadeStart(spyRadius);
    if ( horizontalDistance <= fadeStart )
        return worldUiOpacity;

    float ratio = (spyRadius - horizontalDistance) / (spyRadius - fadeStart);
    ratio = std::max(0.0f, std::min(1.0f, ratio));
    ratio = ratio * ratio * (3.0f - 2.0f * ratio);
    const uint8_t spyOpacity = (uint8_t)dround(ratio * 255.0f);
    return std::min(worldUiOpacity, spyOpacity);
}

bool yw_ShouldRenderUfoSpyWorldArrow(NC_STACK_ypaworld *yw,
                                             NC_STACK_ypabact *bact)
{
    return yw_GetUfoSpyWorldOpacity(yw, bact) > 0;
}

bool yw_ShouldRenderUfoSpyWorldStatus(NC_STACK_ypaworld *yw,
                                              NC_STACK_ypabact *bact)
{
    return yw_IsUfoSpyHudActive(yw) &&
           yw_GetUfoSpyWorldOpacity(yw, bact) > 0;
}

void yw_RenderUnitLifeBar(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact)
{
    if ( yw_ShouldHideControlledUnitWorldUi(yw, bact) )
        return;

    // OpenNeoUA invisible: no HP/shield/status bar or fraction triangle over a cloaked unit.
    if ( bact && bact->IsInvisibleUnrevealed() )
        return;

    const uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, bact->_position);
    if (worldUiOpacity == 0)
        return;

    // Render fraction triangles above units

    float v6 = bact->_position.x - yw->_viewerPosition.x;
    float v5 = bact->_position.y - yw->_viewerPosition.y;
    float v9 = bact->_position.z - yw->_viewerPosition.z;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    float v44 = corrected.m00 * v6 + corrected.m01 * v5 + corrected.m02 * v9;
    float v46 = corrected.m10 * v6 + corrected.m11 * v5 + corrected.m12 * v9;
    float v37 = corrected.m20 * v6 + corrected.m21 * v5 + corrected.m22 * v9;

    if ( v37 > 30.0 && v44 < v37 )
    {
        if ( v44 > -v37 && v46 < v37 && v46 > -v37 )
        {
            int v11 = 0;
            int v33 = 0;
            int v34 = 0;
            int v36 = 0;
            int v30 = 0;

            if ( robo_map.IsOpen() )
            {
                v30 = 1;
                v11 = robo_map.x;
                v33 = v11 + robo_map.w;
                v36 = robo_map.y;
                v34 = v36 + robo_map.h;
            }

            float v45 = v44 * (1.0 / v37);
            float v47 = v46 * (1.0 / v37);
            GFX::Engine.viewZoomCorrection(v45, v47);
            if ( v45 < -1.0f || v45 > 1.0f || v47 < -1.0f || v47 > 1.0f )
                return;

            int v13;

            if ( yw->_screenSize.x >= 512 )
                v13 = 8;
            else
                v13 = 4;

            int v43 = v13 * yw->_guiTiles[50]->map[0].w;

            int v42 = (yw->_screenSize.x / 2) * (v45 + 1.0);
            int v41 = (yw->_screenSize.y / 2) * (v47 + 1.0);

            if ( !v30 || v42 <= v11 || v42 >= v33 || v41 <= v36 || v41 >= v34 )
            {
                v42 -= v43 / 2;
                v41 -= (yw->_guiTiles[50]->h / 2) + (yw->_screenSize.y / 16);

                int barHeight = yw->_guiTiles[50]->h;
                const YWProceduralStatusBarConfig &statusBarConfig = yw_GetProceduralStatusBarConfig();
                const bool hideWorldShield = statusBarConfig.hpEnabled;
                const bool newWorldShieldBar = statusBarConfig.shieldEnabled;
                int shieldTop = v41;
                int lifeTop = hideWorldShield ? shieldTop : shieldTop - barHeight - 1;

                // Optional world Shield bar, always directly under the HP bar.
                // It replaces the classic Shield row only while it is enabled.
                int shieldBarLeft = 0;
                int shieldBarTop = 0;
                int shieldBarWidth = 0;
                int shieldBarHeight = 0;
                if ( newWorldShieldBar )
                    yw_GetWorldShieldBarRect(v42, lifeTop, v43, barHeight, hideWorldShield,
                                             &shieldBarLeft, &shieldBarTop,
                                             &shieldBarWidth, &shieldBarHeight);

                // Bottom edge of the stacked unit UI: HP row plus whichever
                // Shield row/bar is actually drawn. Nothing else may enter it.
                int uiStackBottom = shieldTop + barHeight;
                if ( newWorldShieldBar )
                    uiStackBottom = std::max(uiStackBottom, shieldBarTop + shieldBarHeight);

                // Selection owns the small strip immediately above the vanilla
                // faction arrow. Keep the X fixed there and, only if needed,
                // lift the detailed HP/status stack so it can never overlap it.
                if ( yw_IsActiveSquadronSelectionUnit(yw, bact) )
                {
                    Common::Point selectionPoint(
                        v42 + v43 / 2,
                        shieldTop + barHeight / 2 + yw->_screenSize.y / 16);
                    int selectionCenterY = 0;
                    if ( yw_GetWorldSelectionXAnchor(yw, bact, selectionPoint,
                                                     nullptr, &selectionCenterY) )
                    {
                        const int selectionHalfArm =
                            yw_GetWorldSelectionXHalfArm(yw, bact);
                        const int selectionTop = selectionCenterY -
                                                 selectionHalfArm;
                        const int maxDetailedUiBottom = selectionTop -
                                                        kWorldSelectionXUiGap;
                        const int detailedUiBottom = uiStackBottom;
                        if ( detailedUiBottom > maxDetailedUiBottom )
                        {
                            const int lift = detailedUiBottom - maxDetailedUiBottom;
                            shieldTop -= lift;
                            lifeTop -= lift;
                            shieldBarTop -= lift;
                            uiStackBottom -= lift;
                        }
                    }
                }

                if ( v42 >= 0 )
                {
                    if ( v42 + v43 < yw->_screenSize.x && lifeTop >= 0 )
                    {
                        if ( uiStackBottom < yw->_screenSize.y )
                        {
                            FontUA::set_opacity(cur, worldUiOpacity);
                            yw_RenderUnitSquareBar(yw, cur, v42, lifeTop, v13, bact->_energy, bact->_energy_max, 2, 6, worldUiOpacity);
                            if ( newWorldShieldBar )
                                yw_RenderProceduralShieldBar(shieldBarLeft, shieldBarTop,
                                                             shieldBarWidth, shieldBarHeight,
                                                             (int)bact->GetEffectiveShield(), 100,
                                                             worldUiOpacity);
                            else if ( !hideWorldShield )
                                yw_RenderUnitSquareBar(yw, cur, v42, shieldTop, v13, (int)bact->GetEffectiveShield(), 100, 1, 5, worldUiOpacity);

                            int statusAnchorTop = lifeTop;
                            int artilleryShellCooldownTop = lifeTop - barHeight - 1;
                            if ( artilleryShellCooldownTop >= 0 &&
                                 yw_RenderArtilleryShellCooldownBar(yw, cur, bact, v42, artilleryShellCooldownTop, v13) )
                            {
                                statusAnchorTop = artilleryShellCooldownTop;
                            }

                            uint8_t protoId = bact->_mimic_disguise_vehicleID ? bact->_mimic_disguise_vehicleID : bact->_vehicleID;
                            const uint8_t killMarks = bact->GetSessionKillMarks();
                            const bool showKillMarks = killMarks > 0;
                            constexpr int markGap = 2;
                            constexpr int markHeight = 3;
                            const int killMarkReserve = showKillMarks ? markGap + markHeight + markGap : 0;
                            int killMarkCenterX = v42 + v43 / 2 - yw->_screenSize.x / 2;
                            if ( (size_t)protoId < yw->_vhclProtos.size() )
                                StatusIconRenderWorld(yw, bact, &yw->_vhclProtos[protoId],
                                                      v42, statusAnchorTop, v43, barHeight,
                                                      worldUiOpacity, killMarkReserve,
                                                      &killMarkCenterX);

                            if ( showKillMarks )
                            {
                                if ( statusAnchorTop - markGap - markHeight >= 0 )
                                {
                                    const int tipY = statusAnchorTop - markGap - yw->_screenSize.y / 2;
                                    SDL_Color markerColor = yw->GetColor(bact->_owner);
                                    markerColor.a = worldUiOpacity;
                                    SDL_Color shadowColor = yw_GetNeutralSelectionShadowColor();
                                    shadowColor.a = worldUiOpacity;

                                    yw_DrawWorldKillMarks(killMarkCenterX + 1, tipY + 1,
                                                          killMarks, shadowColor);
                                    yw_DrawWorldKillMarks(killMarkCenterX, tipY,
                                                          killMarks, markerColor);
                                }
                            }
                            FontUA::set_opacity(cur, 255);
                        }
                    }
                }
            }
        }
    }
}

bool yw_IsExplicitWorldStatusTarget(NC_STACK_ypaworld *yw,
                                            NC_STACK_ypabact *bact,
                                            const std::vector<NC_STACK_ypabact *> &damageHoverTargets)
{
    if ( !yw || !bact )
        return false;

    if ( (yw->_guiActFlags & 0x20) && yw->_bactOnMouse == bact )
        return true;

    if ( yw->_guiVisor.field_18 == bact )
        return true;

    return std::find(damageHoverTargets.begin(), damageHoverTargets.end(), bact) !=
           damageHoverTargets.end();
}

bool yw_IsExplicitWorldCursorTarget(
    NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact,
    const std::vector<NC_STACK_ypabact *> &damageHoverTargets)
{
    if ( !yw || !bact )
        return false;

    if ( (yw->_guiActFlags & 0x20) && yw->_bactOnMouse == bact )
        return true;

    return std::find(damageHoverTargets.begin(), damageHoverTargets.end(), bact) !=
           damageHoverTargets.end();
}

void yw_RenderUfoSpyWorldStatus(NC_STACK_ypaworld *yw, CmdStream *cur,
                                        NC_STACK_ypabact *bact,
                                        const std::vector<NC_STACK_ypabact *> &damageHoverTargets)
{
    if ( !bact )
        return;

    if ( yw_ShouldRenderUfoSpyWorldStatus(yw, bact) &&
         !yw_IsExplicitWorldStatusTarget(yw, bact, damageHoverTargets) )
    {
        // HP/shield/status is the optional detailed Spy layer toggled by U/MMB.
        // Normal hover/target/damage paths remain authoritative and are never
        // duplicated by this pass.
        yw_RenderUnitLifeBar(yw, cur, bact);
    }

    for ( NC_STACK_ypabact *kid : bact->_kidList )
        yw_RenderUfoSpyWorldStatus(yw, cur, kid, damageHoverTargets);
}

void yw_RenderUfoSpyWorldArrows(
    NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact,
    const std::vector<NC_STACK_ypabact *> &damageHoverTargets)
{
    if ( !bact )
        return;

    // The lightweight faction arrow is always part of the UFO sensor view,
    // even when the detailed HP/status overlay is toggled off. Reuse the exact
    // existing cursor renderer and let yw_GetUfoSpyWorldOpacity() provide the
    // same smooth fade curve for friendly and enemy units.
    if ( yw_ShouldRenderUfoSpyWorldArrow(yw, bact) &&
         !yw_IsExplicitWorldCursorTarget(yw, bact, damageHoverTargets) )
    {
        yw_RenderCursorOverUnitWithOpacity(yw, bact, bact->_position, 255);
    }

    for ( NC_STACK_ypabact *kid : bact->_kidList )
        yw_RenderUfoSpyWorldArrows(yw, kid, damageHoverTargets);
}

void yw_DrawWorldSelectionX(int centerX, int centerY, int halfArm, SDL_Color color)
{
    GFX::Engine.raster_func217(color);
    GFX::Engine.raster_func201(Common::Line(centerX - halfArm,
                                            centerY - halfArm,
                                            centerX + halfArm,
                                            centerY + halfArm));
    GFX::Engine.raster_func201(Common::Line(centerX - halfArm,
                                            centerY + halfArm,
                                            centerX + halfArm,
                                            centerY - halfArm));
}

bool yw_GetWorldSelectionXAnchor(NC_STACK_ypaworld *yw,
                                         NC_STACK_ypabact *bact,
                                         const Common::Point &point,
                                         int *centerXOut,
                                         int *centerYOut)
{
    if ( !yw || !bact || (!centerXOut && !centerYOut) )
        return false;

    // Selection replaces the vanilla faction arrow instead of stacking another
    // symbol above it. Keep the same one-pixel optical X correction already used
    // by the selection marker and the exact vanilla vertical arrow offset.
    const int markerCenterX = point.x - 1;
    const int markerCenterY = point.y - yw->_screenSize.y * 4 / 100;

    const int selectionHalfArm = yw_GetWorldSelectionXHalfArm(yw, bact);

    if ( markerCenterY - selectionHalfArm -
         kWorldSelectionXShadowOffset < 2 )
    {
        return false;
    }

    if ( centerXOut )
        *centerXOut = markerCenterX;
    if ( centerYOut )
        *centerYOut = markerCenterY;
    return true;
}

void yw_DrawWorldKillMarks(int centerX, int tipY, uint8_t marks, SDL_Color color)
{
    if ( marks == 0 )
        return;

    constexpr int halfMarkWidth = 4;
    constexpr int markHeight = 3;
    constexpr int markSpacing = 2;
    constexpr int opticalCenterOffsetX = -1;
    const int markWidth = halfMarkWidth * 2;
    const int totalWidth = marks * markWidth + (marks - 1) * markSpacing;
    const int firstCenterX = centerX + opticalCenterOffsetX -
                             totalWidth / 2 + halfMarkWidth;
    const int baseY = tipY - markHeight;

    GFX::Engine.raster_func217(color);
    for (uint8_t i = 0; i < marks; i++)
    {
        const int markCenterX = firstCenterX + i * (markWidth + markSpacing);
        GFX::Engine.raster_func201(Common::Line(markCenterX, tipY,
                                                markCenterX - halfMarkWidth, baseY));
        GFX::Engine.raster_func201(Common::Line(markCenterX - halfMarkWidth, baseY,
                                                markCenterX + halfMarkWidth, baseY));
        GFX::Engine.raster_func201(Common::Line(markCenterX + halfMarkWidth, baseY,
                                                markCenterX, tipY));
    }
}

bool yw_IsWorldBossMarkerUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !yw || !bact )
        return false;

    return bact->_bact_type == BACT_TYPES_ROBO ||
           (yw->_userRobo && bact->IsParentMyRobo() &&
            bact->_owner == yw->_userRobo->_owner);
}

int yw_GetWorldSelectionXHalfArm(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    return yw_IsWorldBossMarkerUnit(yw, bact) ?
           kWorldSelectionXBossHalfArm :
           kWorldSelectionXHalfArm;
}

void yw_RenderOverlayCursors(
    NC_STACK_ypaworld *yw, CmdStream *cur,
    const std::vector<NC_STACK_ypabact *> &damageHoverTargets)
{
    NC_STACK_ypabact *activeCommander = nullptr;

    if ( yw->_activeCmdrRemapIndex >= 0 &&
         (size_t)yw->_activeCmdrRemapIndex < yw->_cmdrsRemap.size() )
    {
        activeCommander = yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];
        if ( activeCommander->_status != BACT_STATUS_CREATE &&
             activeCommander->_status != BACT_STATUS_BEAM &&
             activeCommander->_status != BACT_STATUS_DEAD &&
             !activeCommander->ShouldHideFromStrategicUI() )
        {
            // For the active selection the cursor renderer replaces the normal
            // faction arrow with the X in the exact same marker position.
            if ( !yw_ShouldRenderUfoSpyWorldArrow(yw, activeCommander) )
                yw_RenderCursorOverUnit(yw, activeCommander);
        }

        for ( NC_STACK_ypabact *unit : activeCommander->_kidList )
        {
            if ( unit->_status != BACT_STATUS_CREATE &&
                 unit->_status != BACT_STATUS_BEAM &&
                 unit->_status != BACT_STATUS_DEAD &&
                 !unit->ShouldHideFromStrategicUI() )
            {
                if ( !yw_ShouldRenderUfoSpyWorldArrow(yw, unit) )
                    yw_RenderCursorOverUnit(yw, unit);
            }
        }
    }

    if ( yw->_preferences & World::PREF_ENEMYINDICATOR )
    {
        const int worldUiSectorRadius =
            yw_GetWorldUiSectorRadius(yw_GetWorldUiMaxDistance());
        int v5 = yw->_userUnit->_cellId.x - worldUiSectorRadius;
        int v16 = yw->_userUnit->_cellId.x + worldUiSectorRadius;
        int v17 = yw->_userUnit->_cellId.y - worldUiSectorRadius;
        int v14 = yw->_userUnit->_cellId.y + worldUiSectorRadius;

        if ( v5 < 1 )
            v5 = 1;

        if ( v17 < 1 )
            v17 = 1;

        if ( v16 >= yw->_mapSize.x )
            v16 = yw->_mapSize.x - 1;

        if ( v14 >= yw->_mapSize.y )
            v14 = yw->_mapSize.y - 1;

        yw_PrepareEnemyIndicatorVisibilityStates(yw);

        for (int i = v17; i <= v14; i++ )
        {
            for (int  j = v5; j <= v16; j++ )
            {
                cellArea &v8 = yw->_cells(j, i);
                const bool cellVisible = (robo_map.MapViewMask & v8.view_mask) != 0;

                for ( NC_STACK_ypabact* &bct : v8.unitsList )
                {
                    if ( bct->ShouldHideFromStrategicUI() )
                        continue;

                    if ( activeCommander &&
                         (bct == activeCommander || bct->_parent == activeCommander) )
                        continue;

                    // Host Stations use the same world-space inverted triangle as
                    // ordinary units. Keep missiles and hidden Robo guns excluded.
                    if ( bct->_bact_type == BACT_TYPES_MISSLE ||
                         bct->_status == BACT_STATUS_CREATE ||
                         bct->_status == BACT_STATUS_BEAM ||
                         bct->_status == BACT_STATUS_DEAD )
                    {
                        continue;
                    }

                    if ( bct->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = (NC_STACK_ypagun *)bct;
                        if ( gun->IsRoboGun() )
                            continue;
                    }

                    vec3d renderPosition;
                    const uint8_t visibilityOpacity =
                        yw_GetEnemyIndicatorVisibilityOpacity(yw, bct, cellVisible,
                                                              &renderPosition);
                    if ( visibilityOpacity > 0 &&
                         !yw_ShouldRenderUfoSpyWorldArrow(yw, bct) )
                    {
                        yw_RenderCursorOverUnitWithOpacity(yw, bct, renderPosition,
                                                           visibilityOpacity);
                    }

                    if ( cellVisible && yw->_isNetGame )
                    {
                        if ( bct->_status_flg & BACT_STFLAG_ISVIEW )
                        {
                            if ( yw->_GameShell )
                                sb_0x4d7c08__sub0__sub4__sub0__sub0(yw, cur, bct);
                        }
                    }
                }
            }
        }
    }
}

bool yw_ProjectHUDMissileLockTarget(NC_STACK_ypaworld *yw, NC_STACK_ypabact *target, float *outX, float *outY)
{
    if ( !yw->_userUnit || !target || target->IsDestroyed() || target->_status == BACT_STATUS_DEAD )
        return false;

    if ( yw_GetWorldUiOpacity(yw, target->_position) == 0 )
        return false;

    vec3d targetPos = target->_position - yw->_userUnit->_position;

    mat3x3 corrected = yw->_userUnit->_rotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    vec3d hudPos = corrected.Transform(targetPos);
    if ( hudPos.z == 0.0 )
        return false;

    *outX = hudPos.x / hudPos.z;
    *outY = hudPos.y / hudPos.z;
    GFX::Engine.viewZoomCorrection(*outX, *outY);
    return true;
}

void yw_RenderHUDWeaponLockMarker(NC_STACK_ypaworld *yw, sklt_wis *wis, UAskeleton::Data *wpn_wure, UAskeleton::Data *wpn_wure2,
        bool locked, float fromX, float fromY, float toX, float toY, float hudAppearProgress, const World::TVhclProto *vhcl)
{
    float lockProgress[2];
    wis_color_func func;
    SDL_Color primaryColor;
    SDL_Color secondaryColor;

    if ( locked )
    {
        for (int i = 0; i < 2; i++)
        {
            lockProgress[i] = (yw->_timeStamp - wis->field_7A) / 200.0;

            if ( lockProgress[i] > 1.0 )
                lockProgress[i] = 1.0;
        }

        primaryColor = yw->GetColor(27);
        secondaryColor = yw->GetColor(36);

        if ( wis->field_72 )
            func = sub_4E3D98;
        else
            func = nullptr;
    }
    else
    {
        lockProgress[0] = 0;
        lockProgress[1] = 0;

        secondaryColor = yw->GetColor(28);
        primaryColor = yw->GetColor(37);

        if ( wis->field_72 )
            func = sub_4E3B80;
        else
            func = nullptr;
    }

    if ( yw->_userUnit && !yw->_userUnit->IsPlayerPrimaryWeaponElevationAllowed() )
    {
        primaryColor = SDL_Color{255, 0, 0, 255};
        secondaryColor = primaryColor;
    }

    int cl1r = primaryColor.r;
    int cl1g = primaryColor.g;
    int cl1b = primaryColor.b;
    int cl2r = secondaryColor.r;
    int cl2b = secondaryColor.b;
    int cl2g = secondaryColor.g;

    for (int i = 0; i < 2; i++)
    {
        float drawX, drawY;

        if ( locked )
        {
            drawX = (toX - fromX) * lockProgress[i] + fromX;
            drawY = (toY - fromY) * lockProgress[i] + fromY;
        }
        else
        {
            drawX = toX;
            drawY = toY;
        }

        float scaleY = lockProgress[i] * -0.23 + 0.4;
        float scaleX = lockProgress[i] * -0.17 + 0.3;

        if ( hudAppearProgress < 1.4 )
            scaleY = scaleY * hudAppearProgress;

        float angle;

        if ( i & 1 )
        {
            angle = lockProgress[i] * 1.571;
        }
        else if ( lockProgress[i] >= 1.0 )
        {
            angle = (yw->_timeStamp - wis->field_7A) * 0.001 * 6.282;
        }
        else
        {
            angle = lockProgress[i] * -3.141;
        }

        float s = sin(angle);
        float c = cos(angle);

        UAskeleton::Data *sklt;

        if ( i & 1 )
        {
            sklt = wpn_wure;
        }
        else
        {
            sklt = wpn_wure2;
        }

        if ( sklt )
        {
            yw->_hud.cl1_r = cl1r;
            yw->_hud.cl1_g = cl1g;
            yw->_hud.cl1_b = cl1b;
            yw->_hud.cl2_r = cl2r;
            yw->_hud.cl2_g = cl2g;
            yw->_hud.cl2_b = cl2b;

            yw_RenderVector2D(yw, sklt, drawX, drawY, c, -s, s, c, scaleX, scaleY, primaryColor, func, 0, true);
        }
    }
}

void yw_RenderHUDTarget(NC_STACK_ypaworld *yw, sklt_wis *wis)
{
    UAskeleton::Data *mg_wure = nullptr;
    UAskeleton::Data *hud_wure = nullptr;
    UAskeleton::Data *wpn_wure = nullptr;
    UAskeleton::Data *wpn_wure2 = nullptr;

    const bool hideWireframes = yw->HasActiveNewGemNotification();
    float v86 = (yw->_timeStamp - wis->field_76 - 350) / 200.0;

    if ( v86 > 0.0 )
    {
        World::TVhclProto *vhcl;

        if ( yw->_userUnit )
        {
            uint8_t protoId = yw->_userUnit->_mimic_disguise_vehicleID ? yw->_userUnit->_mimic_disguise_vehicleID : yw->_userUnit->_vehicleID;
            if ( (size_t)protoId < yw->_vhclProtos.size() )
                vhcl = &yw->_vhclProtos[ protoId ];
            else
                vhcl = nullptr;
        }
        else
            vhcl = nullptr;

        if ( vhcl && vhcl->hud_wireframe )
            hud_wure = vhcl->hud_wireframe->GetSkelet();
        else
            hud_wure = nullptr;

        if ( vhcl && vhcl->mg_wireframe )
            mg_wure = vhcl->mg_wireframe->GetSkelet();
        else
            mg_wure = wis->sklts_intern[4];

        if ( vhcl && vhcl->wpn_wireframe_1 )
            wpn_wure = vhcl->wpn_wireframe_1->GetSkelet();
        else if ( yw->_guiVisor.field_4 == 2)
            wpn_wure = wis->sklts_intern[5];
        else if ( yw->_guiVisor.field_4 == 3 )
            wpn_wure = wis->sklts_intern[6];
        else if ( yw->_guiVisor.field_4 == 4 )
            wpn_wure = wis->sklts_intern[7];

        if ( vhcl && vhcl->wpn_wireframe_2 )
            wpn_wure2 = vhcl->wpn_wireframe_2->GetSkelet();
        else if ( yw->_guiVisor.field_4 == 2)
            wpn_wure2 = wis->sklts_intern[9];
        else if ( yw->_guiVisor.field_4 == 3 )
            wpn_wure2 = wis->sklts_intern[10];
        else if ( yw->_guiVisor.field_4 == 4 )
            wpn_wure2 = wis->sklts_intern[11];

        if ( yw->_guiVisor.field_18 )
        {
            if ( yw->_guiVisor.field_18 != wis->field_7E )
            {
                wis->field_7E = yw->_guiVisor.field_18;
                wis->field_7A = yw->_timeStamp;
            }
        }
        else
        {
            wis->field_7E = nullptr;
        }

        // Alternative View keeps only the vehicle info wireframe rendered by
        // yw_RenderInfoVehicleWire(). Hide the authored visor/weapon overlays.
        const bool hideAlternativeViewHudWireframes =
            yw->_userUnit && yw->_userUnit->IsAlternativeViewActive();

        if ( !hideWireframes && !hideAlternativeViewHudWireframes && hud_wure )
        {
            SDL_Color v14 = yw->GetColor(34);
            yw_RenderVector2D(yw, hud_wure, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.99, 0.99, v14, nullptr, nullptr, true);
        }

        if ( !hideWireframes && !hideAlternativeViewHudWireframes && yw->_guiVisor.field_0 )
        {
            if ( mg_wure )
            {
                SDL_Color v15 = yw->GetColor(26);
                SDL_Color v16 = yw->GetColor(35);

                float a9 = 0.3;

                if ( v86 < 1.4 )
                    a9 = v86 * 0.3;

                wis_color_func func;

                if ( wis->field_72 )
                    func = sub_4E3B80;
                else
                    func = nullptr;

                sklt_wis *v19 = &yw->_hud;

                v19->cl1_r = v16.r;
                v19->cl1_g = v16.g;
                v19->cl1_b = v16.b;
                v19->cl2_r = v15.r;
                v19->cl2_g = v15.g;
                v19->cl2_b = v15.b;

                yw_RenderVector2D(yw, mg_wure, yw->_guiVisor.field_8, yw->_guiVisor.field_C, 1.0, 0.0, 0.0, 1.0, a9, 0.4, v16, func, nullptr, true);
            }
        }

        if ( !hideWireframes && !hideAlternativeViewHudWireframes && yw->_guiVisor.field_4 )
        {
            bool locked = yw->_guiVisor.field_18 != nullptr;
            const bool lockedTargetVisible = !locked ||
                yw_GetWorldUiOpacity(yw, yw->_guiVisor.field_18->_position) > 0;

            if ( lockedTargetVisible )
            {
                yw_RenderHUDWeaponLockMarker(yw, wis, wpn_wure, wpn_wure2, locked,
                        yw->_guiVisor.field_8, yw->_guiVisor.field_C,
                        yw->_guiVisor.field_10, yw->_guiVisor.field_14, v86, vhcl);
            }

            if ( locked && lockedTargetVisible && yw->_hudMissileMultiLockTargets.size() > 1 )
            {
                bool primaryInMultiLock = false;
                for (NC_STACK_ypabact *target : yw->_hudMissileMultiLockTargets)
                {
                    if ( target == yw->_guiVisor.field_18 )
                    {
                        primaryInMultiLock = true;
                        break;
                    }
                }

                if ( primaryInMultiLock )
                {
                    for (NC_STACK_ypabact *target : yw->_hudMissileMultiLockTargets)
                    {
                        if ( target == yw->_guiVisor.field_18 )
                            continue;

                        float targetX;
                        float targetY;
                        if ( yw_ProjectHUDMissileLockTarget(yw, target, &targetX, &targetY) )
                        {
                            yw_RenderHUDWeaponLockMarker(yw, wis, wpn_wure, wpn_wure2, true,
                                    yw->_guiVisor.field_8, yw->_guiVisor.field_C,
                                    targetX, targetY, v86, vhcl);
                        }
                    }
                }
            }
        }
    }
}

void yw_RenderCursorOverUnitWithOpacity(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact,
                                               const vec3d &renderPosition, uint8_t visibilityOpacity)
{
    const bool activeSquadronSelection =
        yw_IsActiveSquadronSelectionUnit(yw, bact);
    if ( yw_ShouldHideControlledUnitWorldUi(yw, bact) )
        return;

    // OpenNeoUA invisible: never draw the targeting cursor/box over a cloaked stealth unit.
    if ( bact && bact->IsInvisibleUnrevealed() )
        return;

    uint8_t worldUiOpacity = yw_GetWorldUiOpacity(yw, renderPosition);
    worldUiOpacity = (uint8_t)(((uint32_t)worldUiOpacity * visibilityOpacity + 127U) / 255U);
    if (worldUiOpacity == 0)
        return;

    float v6 = renderPosition.x - yw->_viewerPosition.x;
    float v4 = renderPosition.y - yw->_viewerPosition.y;
    float v8 = renderPosition.z - yw->_viewerPosition.z;

    mat3x3 corrected = yw->_viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, false);

    float a3  = corrected.m00 * v6 + corrected.m01 * v4 + corrected.m02 * v8;
    float v33 = corrected.m10 * v6 + corrected.m11 * v4 + corrected.m12 * v8;
    float v30 = corrected.m20 * v6 + corrected.m21 * v4 + corrected.m22 * v8;

    if ( v30 > 30.0 && a3 < v30 )
    {
        if ( a3 > -v30 && v33 < v30 && v33 > -v30 )
        {
            SDL_Color v11 = yw->GetColor(bact->_owner);
            v11.a = worldUiOpacity;
            UAskeleton::Data *v12 = yw->_hud.sklts_intern[13];

            float a3a = a3 / v30;
            float v34 = v33 / v30;
            GFX::Engine.viewZoomCorrection(a3a, v34);
            if ( a3a < -1.0f || a3a > 1.0f || v34 < -1.0f || v34 > 1.0f )
                return;

            if ( activeSquadronSelection )
            {
                Common::Point point;
                if ( yw_ProjectWorldSelectionPoint(yw, renderPosition, &point) )
                {
                    int markerCenterX = 0;
                    int markerCenterY = 0;
                    if ( yw_GetWorldSelectionXAnchor(yw, bact, point,
                                                     &markerCenterX, &markerCenterY) )
                    {
                        const int centerX = markerCenterX - yw->_screenSize.x / 2;
                        const int centerY = markerCenterY - yw->_screenSize.y / 2;

                        SDL_Color markerColor = yw_GetFactionSelectionColor(yw);
                        markerColor.a = worldUiOpacity;
                        SDL_Color shadowColor = yw_GetNeutralSelectionShadowColor();
                        shadowColor.a = worldUiOpacity;

                        const int selectionHalfArm =
                            yw_GetWorldSelectionXHalfArm(yw, bact);
                        yw_DrawWorldSelectionX(centerX + kWorldSelectionXShadowOffset,
                                               centerY + kWorldSelectionXShadowOffset,
                                               selectionHalfArm, shadowColor);
                        yw_DrawWorldSelectionX(centerX, centerY,
                                               selectionHalfArm, markerColor);
                    }
                }
                return;
            }

            if ( v12 )
            {
                float a4 = v34 - 0.08;
                // Keep the vanilla arrow centered on the projected unit.
                const float arrowX = a3a;
                // Squad leaders (their parent is the player's Host Station) and
                // Host Stations keep the larger vanilla arrow treatment.
                const bool isBossMarker = yw_IsWorldBossMarkerUnit(yw, bact);

                if ( isBossMarker )
                {
                    // Large commander arrow plus its smaller inner pass.
                    yw_RenderVector2D(yw, v12, arrowX, a4, 1.0, 0.0, 0.0, 1.0, 0.0125, 0.0165, v11, nullptr, nullptr, true);
                    yw_RenderVector2D(yw, v12, arrowX, a4, 1.0, 0.0, 0.0, 1.0, 0.005, 0.00666, v11, nullptr, nullptr, true);
                }
                else
                {
                    yw_RenderVector2D(yw, v12, arrowX, a4, 1.0, 0.0, 0.0, 1.0, 0.0075, 0.01, v11, nullptr, nullptr, true);
                }
            }
        }
    }
}

void yw_RenderCursorOverUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !bact )
        return;

    yw_RenderCursorOverUnitWithOpacity(yw, bact, bact->_position, 255);
}

void yw_RenderHUDRadare(NC_STACK_ypaworld *yw)
{
    float v12 = robo_map.field_1D8;
    float v15 = robo_map.field_1DC;
    float v14 = robo_map.field_1E0;
    float v13 = robo_map.field_1E4;
    int v7 = robo_map.field_1EC;
    int32_t v8 = robo_map.MapViewMask;
    int v9 = robo_map.field_1EE;

    robo_map.field_1EC = 1;
    robo_map.MapViewMask = -1;
    robo_map.field_1E8 |= 0x100;
    robo_map.field_1EE = 3;
    robo_map.field_1D8 = yw->_viewerBact->_position.x;
    robo_map.field_1DC = yw->_viewerBact->_position.z;
    robo_map.field_1E0 = 4800.0 / ((float)(yw->_screenSize.x / 2) * (robo_map.field_25C - robo_map.field_254));
    robo_map.field_1E4 = 4800.0 / ((float)(yw->_screenSize.y / 2) * (robo_map.field_260 - robo_map.field_258));

    float v19 = yw->_viewerRotation.m20;
    float v17 = yw->_viewerRotation.m22;

    float v18, v20;

    float v16 = sqrt( POW2(v17) + POW2(v19));
    if ( v16 <= 0.0 )
    {
        v18 = 1.0;
        v20 = 0.0;
    }
    else
    {
        v20 = v19 / v16;
        v18 = v17 / v16;
    }
    robo_map.field_264 = v18;

    flt_5164F4 = -v20;
    flt_516504 = v20;
    flt_51650C = v18;

    sb_0x4d7c08__sub0__sub4__sub2__sub0(yw);

    robo_map.field_1EC = v7;
    robo_map.field_1EE = v9;
    robo_map.field_1D8 = v12;
    robo_map.MapViewMask = v8;
    robo_map.field_1DC = v15;
    robo_map.field_1E0 = v14;
    robo_map.field_1E8 &= 0xFFFFFEFF;
    robo_map.field_1E4 = v13;
}
