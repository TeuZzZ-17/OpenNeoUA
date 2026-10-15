// Map toolbar, strategic/tactical map, robo map and map markers.
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

SDL_Color yw_MapToolbarColor(NC_STACK_ypaworld *yw, bool active)
{
    SDL_Color clr = (yw && yw->_userRobo)
        ? yw->GetColor(yw->_userRobo->_owner)
        : GFX::Engine.Color(90, 190, 190);

    if ( active )
    {
        clr.r = (uint8_t)std::min(255, (int)clr.r + 28);
        clr.g = (uint8_t)std::min(255, (int)clr.g + 28);
        clr.b = (uint8_t)std::min(255, (int)clr.b + 28);
    }
    else
    {
        clr.r = (uint8_t)((int)clr.r * 2 / 3);
        clr.g = (uint8_t)((int)clr.g * 2 / 3);
        clr.b = (uint8_t)((int)clr.b * 2 / 3);
    }
    clr.a = 255;
    return clr;
}

SDL_Color yw_MapToolbarFrameColor(NC_STACK_ypaworld *yw)
{
    // The PNG glyphs stay neutral, while the corner accents identify the
    // current faction. Keep them deliberately bright so Resistance blue and
    // the darker themes remain readable against the title bar.
    SDL_Color clr = yw_MapToolbarColor(yw, true);
    const int peak = std::max((int)clr.r, std::max((int)clr.g, (int)clr.b));
    if ( peak > 0 && peak < 215 )
    {
        const float scale = 215.0f / (float)peak;
        clr.r = (uint8_t)std::min(255, dround((float)clr.r * scale));
        clr.g = (uint8_t)std::min(255, dround((float)clr.g * scale));
        clr.b = (uint8_t)std::min(255, dround((float)clr.b * scale));
    }
    clr.a = 255;
    return clr;
}

void yw_DrawMapToolbarLine(int x0, int y0, int x1, int y1)
{
    GFX::Engine.raster_func201(Common::Line(x0, y0, x1, y1));
}

void yw_DrawMapToolbarStroke(int x0, int y0, int x1, int y1, int thickness)
{
    yw_DrawMapToolbarLine(x0, y0, x1, y1);
    if ( thickness <= 1 )
        return;

    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    for ( int i = 1; i < thickness; i++ )
    {
        if ( dx >= dy )
            yw_DrawMapToolbarLine(x0, y0 + i, x1, y1 + i);
        else
            yw_DrawMapToolbarLine(x0 + i, y0, x1 + i, y1);
    }
}

void yw_DrawMapToolbarRect(int left, int top, int right, int bottom, int thickness)
{
    for ( int i = 0; i < thickness; i++ )
    {
        yw_DrawMapToolbarLine(left + i, top + i, right - i, top + i);
        yw_DrawMapToolbarLine(right - i, top + i, right - i, bottom - i);
        yw_DrawMapToolbarLine(right - i, bottom - i, left + i, bottom - i);
        yw_DrawMapToolbarLine(left + i, bottom - i, left + i, top + i);
    }
}

void yw_DrawMapToolbarCircle(int cx, int cy, int radius, int segments)
{
    Common::Point prev;
    bool hasPrev = false;
    for ( int i = 0; i <= segments; i++ )
    {
        const float angle = C_2PI * (float)i / (float)segments;
        Common::Point cur(cx + dround(cosf(angle) * radius),
                          cy + dround(sinf(angle) * radius));
        if ( hasPrev )
            yw_DrawMapToolbarLine(prev.x, prev.y, cur.x, cur.y);
        prev = cur;
        hasPrev = true;
    }
}

const char *yw_MapToolbarIconName(int buttonId)
{
    switch ( buttonId )
    {
    case 0: return "close";
    case 3: return "detail";
    case 4: return "owners";
    case 5: return "energy";
    case 6: return "lock";
    case 9: return "marker";
    case 18: return "zoom_in";
    case 19: return "zoom_out";
    default: return nullptr;
    }
}

bool yw_IsMapToolbarZoomFlashActive(int buttonId)
{
    const uint32_t now = SDL_GetTicks();
    switch ( buttonId )
    {
    case 18: return robo_map.zoomInFlashUntil > now;
    case 19: return robo_map.zoomOutFlashUntil > now;
    default: return false;
    }
}

void yw_FlashMapToolbarZoomButton(int buttonId)
{
    const uint32_t until = SDL_GetTicks() + kMapToolbarZoomFlashMs;
    switch ( buttonId )
    {
    case 18: robo_map.zoomInFlashUntil = until; break;
    case 19: robo_map.zoomOutFlashUntil = until; break;
    default: break;
    }
}

YwLoadedUiBitmap yw_LoadFactionToolbarIcon(int owner,
                                                  const char *assetRoot,
                                                  const char *iconName,
                                                  bool active, int iconSize)
{
    YwLoadedUiBitmap loaded;
    if ( owner <= World::OWNER_0 || !assetRoot || !*assetRoot ||
         !iconName || !*iconName )
        return loaded;

    // Toolbar and Squadron Manager artwork stays owner-authored: the original
    // SVGs use layered dark/mid/bright strokes to keep small icons crisp.
    // Active state prefers *_active and safely falls back to the normal icon.
    const bool variants[] = {active, false};
    const int variantCount = active ? 2 : 1;
    const char *extensions[] = {".svg", ".png"};

    for ( int variant = 0; variant < variantCount; ++variant )
    {
        for ( const char *extension : extensions )
        {
            std::string path = assetRoot;
            path += "/owner_";
            path += std::to_string(owner);
            path += "/";
            path += iconName;
            if ( variants[variant] )
                path += "_active";
            path += extension;

            if ( !StatusIconResourceExists(path) )
                continue;

            loaded.bitmap = StatusIconLoad(path, iconSize, iconSize);
            if ( loaded.bitmap && loaded.bitmap->GetBitmap() )
            {
                loaded.ownerTint = false;
                return loaded;
            }
        }
    }

    loaded.bitmap = nullptr;
    loaded.ownerTint = false;
    return loaded;
}

bool yw_RenderFactionToolbarIcon(NC_STACK_ypaworld *yw,
                                        int windowX, int windowY,
                                        int buttonId, bool active,
                                        const ButtonBox &box)
{
    const char *iconName = yw_MapToolbarIconName(buttonId);
    if ( !yw || !iconName )
        return false;

    const int owner = yw_RoboMapMarkerOwner(yw);
    const int iconSize = std::max(8, std::min(box.w, box.h) - 2);

    // Faction artwork keeps the original owner-authored layered SVGs. Missing
    // custom assets safely fall back to the existing vector glyph.
    YwLoadedUiBitmap icon = yw_LoadFactionToolbarIcon(
        owner, "Interface/Map/Buttons", iconName, active, iconSize);
    if ( !icon.bitmap || !icon.bitmap->GetBitmap() )
        return false;

    const int screenLeft = windowX + box.x + (box.w - iconSize) / 2;
    const int screenTop = windowY + box.y + (box.h - iconSize) / 2;
    const SDL_Color tint = yw_GetUiOwnerTintColor(yw, owner);

    if ( icon.ownerTint )
        StatusIconRenderBitmapTinted(yw, icon.bitmap, screenLeft, screenTop,
                                     iconSize, &tint, 255);
    else
        StatusIconRenderBitmap(yw, icon.bitmap, screenLeft, screenTop,
                               iconSize, 255);
    return true;
}

void yw_DrawFactionCloseFallback(NC_STACK_ypaworld *yw,
                                        int windowX, int windowY,
                                        const ButtonBox &box)
{
    const int screenHalfW = yw->_screenSize.x / 2;
    const int screenHalfH = yw->_screenSize.y / 2;
    const int left = windowX + box.x - screenHalfW;
    const int top = windowY + box.y - screenHalfH;
    const int right = left + box.w - 1;
    const int bottom = top + box.h - 1;
    const int pad = std::max(3, std::min(box.w, box.h) / 5);
    const int x0 = left + pad;
    const int x1 = right - pad;
    const int y0 = top + pad;
    const int y1 = bottom - pad;

    GFX::Engine.raster_func217(yw_MapToolbarColor(yw, true));
    yw_DrawMapToolbarStroke(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 2);
    yw_DrawMapToolbarStroke(x0 + 1, y1 - 1, x1 - 1, y0 + 1, 2);
}

void yw_DrawMapToolbarGlyph(NC_STACK_ypaworld *yw, int buttonId, bool active)
{
    if ( buttonId < 0 || buttonId >= (int)robo_map.buttons.size() )
        return;

    if ( buttonId != 0 && buttonId != 3 && buttonId != 4
            && buttonId != 5 && buttonId != 6 && buttonId != 9
            && buttonId != 18 && buttonId != 19 )
        return;

    const ButtonBox &box = robo_map.buttons[buttonId];
    if ( !box )
        return;

    const int screenHalfW = yw->_screenSize.x / 2;
    const int screenHalfH = yw->_screenSize.y / 2;
    const int left = robo_map.x + box.x - screenHalfW;
    const int top = robo_map.y + box.y - screenHalfH;
    const int right = left + box.w - 1;
    const int bottom = top + box.h - 1;
    const int cx = (left + right) / 2;
    const int cy = (top + bottom) / 2;
    const int pad = std::max(3, std::min(box.w, box.h) / 5);
    const int x0 = left + pad;
    const int x1 = right - pad;
    const int y0 = top + pad;
    const int y1 = bottom - pad;

    // The old coloured corner frame was too visually aggressive and could
    // dominate the actual controls. Faction identity now lives inside the external
    // artwork itself; no extra lines are drawn around toolbar icons.
    if ( buttonId == 18 || buttonId == 19 )
        active = active || yw_IsMapToolbarZoomFlashActive(buttonId);

    if ( yw_RenderFactionToolbarIcon(yw, robo_map.x, robo_map.y,
                                    buttonId, active, box) )
        return;

    GFX::Engine.raster_func217(yw_MapToolbarColor(yw, true));

    switch ( buttonId )
    {
    case 0: // custom close button
        yw_DrawFactionCloseFallback(yw, robo_map.x, robo_map.y, box);
        break;

    case 3: // detailed-map grid
    {
        const int gap = 2;
        const int cellW = std::max(2, (x1 - x0 - gap) / 2);
        const int cellH = std::max(2, (y1 - y0 - gap) / 2);
        yw_DrawMapToolbarRect(x0, y0, x0 + cellW, y0 + cellH);
        yw_DrawMapToolbarRect(x1 - cellW, y0, x1, y0 + cellH);
        yw_DrawMapToolbarRect(x0, y1 - cellH, x0 + cellW, y1);
        yw_DrawMapToolbarRect(x1 - cellW, y1 - cellH, x1, y1);
        break;
    }

    case 4: // owner-colour flag
        yw_DrawMapToolbarStroke(x0 + 1, y0, x0 + 1, y1, 2);
        yw_DrawMapToolbarLine(x0 + 3, y0 + 1, x1 - 1, y0 + 3);
        yw_DrawMapToolbarLine(x1 - 1, y0 + 3, x1 - 4, cy);
        yw_DrawMapToolbarLine(x1 - 4, cy, x0 + 3, cy - 1);
        yw_DrawMapToolbarStroke(x0, y1, x0 + 5, y1, 2);
        break;

    case 5: // unit energy information
    {
        const int barBottom = y0 + 3;
        yw_DrawMapToolbarRect(x0, y0, x1, barBottom);
        const int segmentW = std::max(2, (x1 - x0 - 4) / 3);
        yw_DrawMapToolbarStroke(x0 + 2, y0 + 1,
                                std::min(x1 - 1, x0 + 1 + segmentW), y0 + 1, 2);
        yw_DrawMapToolbarStroke(x0 + 3 + segmentW, y0 + 1,
                                std::min(x1 - 1, x0 + 2 + segmentW * 2), y0 + 1, 2);
        yw_DrawMapToolbarRect(x0 + 2, cy, x1 - 2, y1 - 1);
        yw_DrawMapToolbarStroke(cx - 2, cy - 2, cx + 2, cy - 2, 2);
        yw_DrawMapToolbarLine(x0 + 1, y1, x0 + 4, y1);
        yw_DrawMapToolbarLine(x1 - 4, y1, x1 - 1, y1);
        break;
    }

    case 6: // lock viewer
        yw_DrawMapToolbarRect(x0 + 2, cy - 1, x1 - 2, y1, 2);
        yw_DrawMapToolbarLine(x0 + 4, cy - 2, x0 + 4, y0 + 3);
        yw_DrawMapToolbarLine(x0 + 4, y0 + 3, cx, y0);
        yw_DrawMapToolbarLine(cx, y0, x1 - 4, y0 + 3);
        yw_DrawMapToolbarLine(x1 - 4, y0 + 3, x1 - 4, cy - 2);
        yw_DrawMapToolbarStroke(cx, cy + 1, cx, y1 - 2, 2);
        break;

    case 9: // marker / bullseye
    {
        const int radius = std::max(3, std::min(x1 - x0, y1 - y0) / 2);
        yw_DrawMapToolbarCircle(cx, cy, radius, 16);
        yw_DrawMapToolbarCircle(cx, cy, std::max(2, radius / 2), 12);
        yw_DrawMapToolbarStroke(cx - 1, cy, cx + 1, cy, 2);
        yw_DrawMapToolbarLine(cx - radius - 2, cy, cx - radius + 1, cy);
        yw_DrawMapToolbarLine(cx + radius - 1, cy, cx + radius + 2, cy);
        yw_DrawMapToolbarLine(cx, cy - radius - 2, cx, cy - radius + 1);
        yw_DrawMapToolbarLine(cx, cy + radius - 1, cx, cy + radius + 2);
        break;
    }

    case 18: // zoom in
    case 19: // zoom out
    {
        yw_DrawMapToolbarStroke(x0, cy, x1, cy, 3);
        if ( buttonId == 18 )
            yw_DrawMapToolbarStroke(cx, y0, cx, y1, 3);
        break;
    }
    }
}

void yw_RenderMapTitleToolbar(NC_STACK_ypaworld *yw)
{
    if ( !yw || robo_map.IsClosed() )
        return;

    Common::Rect titleRect(robo_map.x - yw->_screenSize.x / 2,
                           robo_map.y - yw->_screenSize.y / 2,
                           robo_map.x + robo_map.w - yw->_screenSize.x / 2 - 1,
                           robo_map.y + yw->_fontH - yw->_screenSize.y / 2 - 1);
    GFX::Engine.raster_func211(titleRect);

    yw_DrawMapToolbarGlyph(yw, 3, (robo_map.field_1EC & 1) != 0);
    yw_DrawMapToolbarGlyph(yw, 4, (robo_map.field_1EC & 2) != 0);
    yw_DrawMapToolbarGlyph(yw, 5, (robo_map.field_1EC & 4) != 0);
    yw_DrawMapToolbarGlyph(yw, 6, robo_map.field_1ED == 1);
    yw_DrawMapToolbarGlyph(yw, 9, robo_map.markerMode);
    yw_DrawMapToolbarGlyph(yw, 18, false);
    yw_DrawMapToolbarGlyph(yw, 19, false);

    // The close control represents the currently open Map window.  Use the
    // illuminated faction variant for the entire lifetime of the window; when
    // the map is closed the toolbar is not rendered at all.
    yw_DrawMapToolbarGlyph(yw, 0, true);
}

int yw_RoboMapMarkerSizePx()
{
    switch ( robo_map.field_1EE )
    {
    case 4:
        return 17; // maximum zoom: keep custom markers compact at close range

    case 3:
        return 13; // first zoom-out

    case 2:
        return 9;  // penultimate zoom-out: compact beside Host Stations

    case 1:
    default:
        return 6;  // maximum zoom-out
    }
}

int yw_RoboMapMarkerOwner(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return 0;
    if ( yw->_userRobo )
        return yw->_userRobo->_owner;
    if ( yw->_userUnit )
        return yw->_userUnit->_owner;
    return 0;
}

YwLoadedUiBitmap yw_LoadRoboMapMarkerBitmap(NC_STACK_ypaworld *yw, int markerSize)
{
    const int owner = yw_RoboMapMarkerOwner(yw);
    if ( owner > World::OWNER_0 )
    {
        // Reuse the owner-authored marker asset from the strategic-map toolbar.
        return yw_LoadFactionToolbarIcon(
            owner, "Interface/Map/Buttons", "marker", true, markerSize);
    }

    return YwLoadedUiBitmap();
}

void yw_RenderCustomMapMarkers(NC_STACK_ypaworld *yw)
{
    // Marker mode controls both editing and visibility, matching the other
    // map-layer toggles: markers remain stored for the session, but disappear
    // immediately while the layer is disabled.
    if ( !yw || !robo_map.markerMode || robo_map.customMarkers.empty() )
        return;

    SDL_Color color = yw_MapToolbarColor(yw, true);
    GFX::Engine.raster_func217(color);

    const int clipLeft = robo_map.field_200;
    const int clipTop = robo_map.field_204;
    const int clipRight = robo_map.field_200 + robo_map.field_1F8 - 1;
    const int clipBottom = robo_map.field_204 + robo_map.field_1FC - 1;

    const int markerSize = yw_RoboMapMarkerSizePx();
    YwLoadedUiBitmap markerBitmap = yw_LoadRoboMapMarkerBitmap(yw, markerSize);
    const SDL_Color markerTint = yw_GetUiOwnerTintColor(yw, yw_RoboMapMarkerOwner(yw));

    for ( const vec2d &marker : robo_map.customMarkers )
    {
        const Common::Point p = sub_4F681C(marker);
        if ( p.x < clipLeft || p.x > clipRight || p.y < clipTop || p.y > clipBottom )
            continue;

        if ( markerBitmap.bitmap && markerBitmap.bitmap->GetBitmap() )
        {
            const int screenX = p.x + yw->_screenSize.x / 2;
            const int screenY = p.y + yw->_screenSize.y / 2;
            if ( markerBitmap.ownerTint )
                StatusIconRenderBitmapTinted(yw, markerBitmap.bitmap,
                                             screenX - markerSize / 2,
                                             screenY - markerSize / 2,
                                             markerSize, &markerTint, 255);
            else
                StatusIconRenderBitmap(yw, markerBitmap.bitmap,
                                       screenX - markerSize / 2,
                                       screenY - markerSize / 2,
                                       markerSize, 255);
            continue;
        }

        // Asset-free fallback.
        const int r = std::max(3, markerSize / 3);
        yw_DrawMapToolbarLine(p.x - r, p.y - r, p.x + r, p.y + r);
        yw_DrawMapToolbarLine(p.x - r, p.y + r, p.x + r, p.y - r);
        yw_DrawMapToolbarLine(p.x - r - 2, p.y, p.x - r + 1, p.y);
        yw_DrawMapToolbarLine(p.x + r - 1, p.y, p.x + r + 2, p.y);
    }
}

void yw_RenderCustomHudRadarMarkers(NC_STACK_ypaworld *yw)
{
    // Keep personal map annotations useful after the large strategic map is
    // closed.  The small gameplay radar uses the same world-to-map transform,
    // but renders a deliberately compact symbol so markers cannot be confused
    // with unit icons or dominate the compass.
    if ( !yw || !robo_map.markerMode || robo_map.customMarkers.empty() )
        return;

    Common::Rect clipRect;
    clipRect.left = robo_map.field_200;
    clipRect.top = robo_map.field_204;
    clipRect.right = robo_map.field_200 + robo_map.field_1F8 - 1;
    clipRect.bottom = robo_map.field_204 + robo_map.field_1FC - 1;
    GFX::Engine.raster_func211(clipRect);

    constexpr int markerSize = 12;
    YwLoadedUiBitmap markerBitmap = yw_LoadRoboMapMarkerBitmap(yw, markerSize);
    const SDL_Color markerTint = yw_GetUiOwnerTintColor(yw, yw_RoboMapMarkerOwner(yw));

    for ( const vec2d &marker : robo_map.customMarkers )
    {
        const Common::Point p = sub_4F681C(marker);
        if ( p.x < clipRect.left || p.x > clipRect.right
                || p.y < clipRect.top || p.y > clipRect.bottom )
        {
            continue;
        }

        if ( markerBitmap.bitmap && markerBitmap.bitmap->GetBitmap() )
        {
            const int screenX = p.x + yw->_screenSize.x / 2;
            const int screenY = p.y + yw->_screenSize.y / 2;
            if ( markerBitmap.ownerTint )
                StatusIconRenderBitmapTinted(yw, markerBitmap.bitmap,
                                             screenX - markerSize / 2,
                                             screenY - markerSize / 2,
                                             markerSize, &markerTint, 255);
            else
                StatusIconRenderBitmap(yw, markerBitmap.bitmap,
                                       screenX - markerSize / 2,
                                       screenY - markerSize / 2,
                                       markerSize, 255);
            continue;
        }

        // Asset-free fallback: a tiny hollow diamond is visually distinct from
        // the circle/square/triangle unit vocabulary of the legacy radar.
        GFX::Engine.raster_func217(yw_MapToolbarColor(yw, true));
        const int r = 3;
        yw_DrawMapToolbarLine(p.x, p.y - r, p.x + r, p.y);
        yw_DrawMapToolbarLine(p.x + r, p.y, p.x, p.y + r);
        yw_DrawMapToolbarLine(p.x, p.y + r, p.x - r, p.y);
        yw_DrawMapToolbarLine(p.x - r, p.y, p.x, p.y - r);
    }
}

bool yw_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint32 *out)
{
    if ( !surface || !out || x < 0 || y < 0 || x >= surface->w || y >= surface->h )
        return false;

    const int bpp = surface->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch ( bpp )
    {
    case 1:
        *out = *p;
        return true;

    case 2:
        *out = *(Uint16 *)p;
        return true;

    case 3:
        if ( SDL_BYTEORDER == SDL_BIG_ENDIAN )
            *out = (p[0] << 16) | (p[1] << 8) | p[2];
        else
            *out = p[0] | (p[1] << 8) | (p[2] << 16);
        return true;

    case 4:
        *out = *(Uint32 *)p;
        return true;

    default:
        break;
    }

    return false;
}

// OpenNeoUA custom: laser map/radar beams should visually match the normal
// missile dots. Missile dots are not drawn with GetColor(owner); they use the
// owner-specific missile tile (owner + 1) from the active map/radar tileset.
// This samples the same tile and picks a bright/saturated non-transparent pixel.
// If anything goes wrong, fallback is the old owner color.
SDL_Color yw_GetMapProjectileOwnerColor(NC_STACK_ypaworld *yw, int owner, int tilesetId)
{
    SDL_Color fallback = {255, 255, 255, 255};
    if ( yw && owner >= 0 )
        fallback = yw->GetColor(owner);

    if ( !yw || owner < 0 || tilesetId < 0 || tilesetId >= (int)yw->_guiTiles.size() )
        return fallback;

    TileMap *tiles = yw->_guiTiles[tilesetId];
    if ( !tiles || !tiles->img )
        return fallback;

    SDL_Surface *surface = tiles->img->GetSwTex();
    if ( !surface )
        return fallback;

    const int tileId = owner + 1;
    if ( tileId < 0 || tileId >= (int)tiles->map.size() )
        return fallback;

    Common::PointRect src = tiles->map[tileId];
    if ( src.IsEmpty() )
        return fallback;

    Uint32 colorKey = 0;
    const bool hasColorKey = SDL_GetColorKey(surface, &colorKey) == 0;

    bool locked = false;
    if ( SDL_MUSTLOCK(surface) )
    {
        if ( SDL_LockSurface(surface) != 0 )
            return fallback;
        locked = true;
    }

    SDL_Color best = fallback;
    int bestScore = -1;

    for (int y = 0; y < src.h; y++)
    {
        for (int x = 0; x < src.w; x++)
        {
            Uint32 pixel = 0;
            if ( !yw_ReadSurfacePixel(surface, src.x + x, src.y + y, &pixel) )
                continue;

            if ( hasColorKey && pixel == colorKey )
                continue;

            Uint8 r = 0, g = 0, b = 0, a = 255;
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);

            // Font transparency is usually yellow colorkey; keep this guard too
            // because some loaded/converted surfaces can hide the raw key value.
            if ( a == 0 || (r == 255 && g == 255 && b == 0) )
                continue;

            const int maxc = std::max((int)r, std::max((int)g, (int)b));
            const int minc = std::min((int)r, std::min((int)g, (int)b));
            const int sat = maxc - minc;
            const int luma = (int)r + (int)g + (int)b;

            // Skip black/near-black outline pixels so the line follows the
            // visible owner colour of the missile dot, not its shadow/border.
            if ( luma < 20 )
                continue;

            const int score = luma + sat * 4;
            if ( score > bestScore )
            {
                bestScore = score;
                best = {r, g, b, 255};
            }
        }
    }

    if ( locked )
        SDL_UnlockSurface(surface);

    return bestScore >= 0 ? best : fallback;
}

void NC_STACK_ypaworld::RenderLaserMapBeams(int mapTilesetId)
{
    if ( robo_map.field_1E0 <= 0.001f || robo_map.field_1E4 <= 0.001f )
        return;

    std::vector<NC_STACK_ypabact *> drawn;

    for (int y = 1; y < _mapSize.y; y++)
    {
        for (int x = 1; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            if ( !(robo_map.MapViewMask & cell.view_mask) )
                continue;

            for ( NC_STACK_ypabact *bact : cell.unitsList )
            {
                if ( !bact || !bact->_laser_active )
                    continue;
                if ( bact->_status == BACT_STATUS_DEAD || bact->ShouldHideFromStrategicUI() )
                    continue;
                // OpenNeoUA invisible: a cloaked unit projects no laser beam onto the map.
                if ( bact->IsInvisibleUnrevealed() )
                    continue;
                if ( bact->IsHiddenFor(GetPlayerOwner()) )
                    continue;
                if ( std::find(drawn.begin(), drawn.end(), bact) != drawn.end() )
                    continue;

                drawn.push_back(bact);

                SDL_Color clr = yw_GetMapProjectileOwnerColor(this, bact->_owner, mapTilesetId);
                if ( !bact->_laser_beams.empty() )
                {
                    for (const NC_STACK_ypabact::TLaserBeamRuntime &beam : bact->_laser_beams)
                        sub_4F68FC(beam.start.x, beam.start.z, beam.end.x, beam.end.z, clr);
                }
                else
                {
                    sub_4F68FC(bact->_laser_beam_start.x, bact->_laser_beam_start.z,
                               bact->_laser_beam_end.x, bact->_laser_beam_end.z, clr);
                }
            }
        }
    }
}

bool yw_IsStrategicMapCellVisible(NC_STACK_ypaworld *yw, const cellArea *cell)
{
    if ( !yw || !cell )
        return false;

    if ( yw->IsSpectatorControlled() )
        return (robo_map.MapViewMask & cell->view_mask) != 0;

    return yw->_userRobo && cell->IsCanSee(yw->_userRobo->_owner);
}

int yw_RoboMapVehicleTileset()
{
    switch ( robo_map.field_1EE )
    {
    case 2:
        return 60;

    case 3:
        return 61;

    case 4:
        return 28;

    case 0:
    case 1:
    default:
        return 59;
    }
}

void yw_RenderLegacyStrategicMap(NC_STACK_ypaworld *yw)
{
    int v41 = 0;

    int v39 = robo_map.field_1EE + 2;

    const int setid = yw_RoboMapVehicleTileset();
    const int a6 = setid == 28 ? 1 : 0;

    int height = yw->_guiTiles[setid]->map[128].w;
    int width = yw->_guiTiles[setid]->h;

    robo_map.field_1F8 = robo_map.w - robo_map.field_24C;
    robo_map.field_1FC = robo_map.h - robo_map.field_250;

    robo_map.field_200 = robo_map.x + robo_map.field_244 - (yw->_screenSize.x / 2);
    robo_map.field_204 = robo_map.y + robo_map.field_23C - (yw->_screenSize.y / 2);

    Common::Rect rect;
    rect.left = robo_map.field_200;
    rect.right = robo_map.w - robo_map.field_24C + robo_map.field_200 - 1;
    rect.top = robo_map.y + robo_map.field_23C - (yw->_screenSize.y / 2);
    rect.bottom = robo_map.h - robo_map.field_250 + rect.top - 1;

    robo_map.field_1F0 = robo_map.field_1D8 / robo_map.field_1E0 - (robo_map.w - robo_map.field_24C) / 2;
    robo_map.field_1F4 = -(robo_map.field_1DC / robo_map.field_1E4 + (robo_map.h - robo_map.field_250) / 2);

    GFX::Engine.raster_func211(rect);

    int v33 = robo_map.field_1F0 >> v39;
    int v38 = robo_map.field_1F4 >> v39;
    int v43 = (robo_map.field_1F8 + robo_map.field_1F0) >> v39;
    int vii = (robo_map.field_1FC + robo_map.field_1F4) >> v39;

    if ( v33 < 1 )
        v33 = 1;

    if ( v38 < 1 )
        v38 = 1;

    if ( v43 >= yw->_mapSize.x )
        v43 = yw->_mapSize.x - 1;

    if ( vii >= yw->_mapSize.y )
        vii = yw->_mapSize.y - 1;

    robo_map.t1_cmdbuf_3.clear();

    sb_0x4f8f64__sub2(yw, &robo_map.t1_cmdbuf_3);

    FontUA::select_tileset(&robo_map.t1_cmdbuf_3, setid);

    sb_0x4f8f64__sub1(yw);
    yw->RenderArtilleryShellMapMarkers();
    yw->RenderLaserMapBeams(setid);

    for (int v42 = v38; v42 <= vii; v42++)
    {
        for (int j = v33; j <= v43; j++ )
        {
            if ( v41 < 2048 )
            {
                cellArea &cell = yw->_cells(j, v42);

                if ( robo_map.MapViewMask & cell.view_mask )
                {
                    for ( NC_STACK_ypabact* &bct : cell.unitsList )
                    {
                        if ( bct->ShouldHideFromStrategicUI() )
                            continue;

                        if ( bct->_bact_type == BACT_TYPES_ROBO )
                        {
                            FontUA::select_tileset(&robo_map.t1_cmdbuf_3, 1);

                            sub_4F6DFC(yw, &robo_map.t1_cmdbuf_3, yw->_guiTiles[1]->h, yw->_guiTiles[1]->map[24].w, bct, 0);

                            FontUA::select_tileset(&robo_map.t1_cmdbuf_3, setid);

                            v41++;
                        }
                        else if ( bct->_bact_type == BACT_TYPES_GUN )
                        {
                            NC_STACK_ypagun *gn = (NC_STACK_ypagun *)bct;

                            if ( !gn->IsRoboGun() )
                            {
                                sub_4F6DFC(yw, &robo_map.t1_cmdbuf_3, height, width, bct, a6);
                                v41++;
                            }
                        }
                        else
                        {
                            sub_4F6DFC(yw, &robo_map.t1_cmdbuf_3, height, width, bct, a6);
                            v41++;
                        }
                    }

                }
            }
        }
    }

    if ( robo_map.field_1E8 & 0x200 )
    {
        for (int l = v38; l <= vii; l++ )
        {
            for (int m = v33; m <= v43; m++ )
            {
                if ( v41 < 2048 )
                {
                    cellArea &cell = yw->_cells(m, l);

                    if ( robo_map.MapViewMask & cell.view_mask )
                    {
                        for ( NC_STACK_ypabact* &bct : cell.unitsList )
                        {
                            if ( bct->_owner == yw->_userRobo->_owner
                                    && bct->_position.x > flt_516524
                                    && bct->_position.z > flt_516528
                                    && bct->_position.x < flt_51652C
                                    && bct->_position.z < flt_516530 )
                            {
                                if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_ROBO && bct->_bact_type != BACT_TYPES_GUN )
                                {
                                    if ( bct->_status != BACT_STATUS_CREATE && bct->_status != BACT_STATUS_BEAM && bct->_status != BACT_STATUS_DEAD )
                                    {
                                        sub_4F6980(&robo_map.t1_cmdbuf_3, bct->_position.x, bct->_position.z, 0x85, height, width);
                                        v41++;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    sb_0x4f8f64__sub3(yw, &robo_map.t1_cmdbuf_3);
    sb_0x4f8f64__sub0(yw, &robo_map.t1_cmdbuf_3);
    FontUA::set_end(&robo_map.t1_cmdbuf_3);

    SDL_Color uiAccentColor;
    GFX::Engine.ProcessDrawSeq(robo_map.t1_cmdbuf_3, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));

    // OpenNeoUA custom: draw the artillery shell cooldown/readiness bar on top of the
    // strategic-map unit icons/HP bars, using the same parent/child artillery shell
    // platform rules as the 3D world overlay and the small radar.
    GFX::Engine.raster_func211(rect);
    yw_RenderArtilleryShellCooldownRadarBars(yw, v33, v38, v43, vii);
}

SDL_Color yw_GetMapTitleBackgroundColor(NC_STACK_ypaworld *yw)
{
    SDL_Color fallback = {12, 24, 28, 255};
    if ( !yw || !yw->_guiTiles[6] || !yw->_guiTiles[6]->img )
        return fallback;

    SDL_Surface *surface = yw->_guiTiles[6]->img->GetSwTex();
    if ( !surface || !surface->format || !surface->pixels )
        return fallback;

    const Common::PointRect &glyph = yw->_guiTiles[6]->map[98];
    if ( glyph.w <= 0 || glyph.h <= 0 )
        return fallback;

    bool locked = false;
    if ( SDL_MUSTLOCK(surface) )
    {
        if ( SDL_LockSurface(surface) != 0 )
            return fallback;
        locked = true;
    }

    const int x = std::max(0, std::min(surface->w - 1, glyph.x + glyph.w / 2));
    const int y = std::max(0, std::min(surface->h - 1, glyph.y + glyph.h / 2));
    Uint32 pixel = 0;
    const bool read = yw_ReadSurfacePixel(surface, x, y, &pixel);

    if ( locked )
        SDL_UnlockSurface(surface);

    if ( !read )
        return fallback;

    SDL_Color color;
    SDL_GetRGBA(pixel, surface->format, &color.r, &color.g, &color.b, &color.a);
    color.a = 255;
    return color;
}

void yw_RenderMapBackground(NC_STACK_ypaworld *yw)
{
    if ( !yw || robo_map.IsClosed() )
        return;

    const uint8_t opacity = yw_GetMapOpacity();
    if ( opacity == 0 )
        return;

    SDL_Surface *surface = GFX::Engine.Screen();
    if ( !surface || !surface->format )
        return;

    const int width = robo_map.w;
    const int height = robo_map.h - robo_map.field_23C;
    if ( width <= 0 || height <= 0 )
        return;

    SDL_Color color = yw_GetMapTitleBackgroundColor(yw);
    color.a = opacity;

    // Fill the complete map body below the titlebar. The legacy map/frame is
    // rendered afterwards, so borders, scrollbars, grid and icons keep their
    // original opacity while the empty side and bottom strips match the map.
    SDL_Rect rect = {
        robo_map.x,
        robo_map.y + robo_map.field_23C,
        width,
        height
    };

    if ( GFX::Engine.IsVirtualUIPass() )
    {
        // The virtual UI surface already contains any lower-priority window.
        // Blend the map background over those pixels instead of replacing them,
        // so translucent map areas still reveal the window underneath.
        const float inv255 = 1.0f / 255.0f;
        GFX::Engine.DrawVirtualUISolidRect(
            (float)rect.x, (float)rect.y,
            (float)(rect.x + rect.w), (float)(rect.y + rect.h),
            GFX::TGLColor(color.r * inv255, color.g * inv255,
                          color.b * inv255, color.a * inv255));
    }
    else
    {
        SDL_FillRect(surface, &rect,
                     SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
    }
}

void yw_RoboMapStoreTrimmedVerticalGlyph(CmdStream *cmd, uint8_t glyph, int trim)
{
    if ( trim > 0 )
        FontUA::set_xoff(cmd, (uint8_t)trim);

    FontUA::store_u8(cmd, glyph);
}

void yw_RoboMapStoreTrimmedHorizontalGlyph(CmdStream *cmd, uint8_t glyph, int trim)
{
    if ( trim > 0 )
        FontUA::set_yoff(cmd, (uint8_t)trim);

    FontUA::store_u8(cmd, glyph);
}

void MapRoboUpdateMapInt(NC_STACK_ypaworld *yw)
{
    robo_map.buttons[17] = ButtonBox(robo_map.field_244,
                                     robo_map.field_23C,
                                     robo_map.w - robo_map.field_24C,
                                     robo_map.h - robo_map.field_250);

    const int mapX = robo_map.x + robo_map.field_244 - (yw->_screenSize.x / 2);
    const int mapY = robo_map.y + robo_map.field_23C - (yw->_screenSize.y / 2);

    robo_map.t1_cmdbuf_3.clear();
    sb_0x4f6650(yw, &robo_map.t1_cmdbuf_3, mapX, mapY);
}

void yw_RebuildRoboMapLayout(NC_STACK_ypaworld *yw)
{
    sub_4C157C(yw);
    sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
    sub_4C0C00(yw);
    sub_4C0FEC(yw);
    sub_4C1214(yw);
    MapRoboUpdateMapInt(yw);
}

void yw_ToggleRoboMapFullscreen(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    if ( !robo_map.maximized )
    {
        robo_map.restoreX = robo_map.x;
        robo_map.restoreY = robo_map.y;
        robo_map.restoreW = robo_map.w;
        robo_map.restoreH = robo_map.h;
        robo_map.restoreRectValid = true;

        robo_map.x = 0;
        robo_map.y = yw->_upScreenBorder;
        robo_map.w = yw->_screenSize.x;
        robo_map.h = yw->_screenSize.y - yw->_upScreenBorder - yw->_downScreenBorder;
        robo_map.maximized = true;
    }
    else
    {
        if ( robo_map.restoreRectValid )
        {
            const int maxW = yw->_screenSize.x;
            const int maxH = yw->_screenSize.y - yw->_upScreenBorder - yw->_downScreenBorder;
            robo_map.x = std::max(0, robo_map.restoreX);
            robo_map.y = std::max(yw->_upScreenBorder, robo_map.restoreY);
            robo_map.w = std::min(maxW, std::max((int)robo_map.field_210, robo_map.restoreW));
            robo_map.h = std::min(maxH, std::max((int)robo_map.field_212, robo_map.restoreH));

            if ( robo_map.x + robo_map.w > yw->_screenSize.x )
                robo_map.x = std::max(0, yw->_screenSize.x - robo_map.w);
            if ( robo_map.y + robo_map.h > yw->_screenSize.y - yw->_downScreenBorder )
                robo_map.y = std::max(yw->_upScreenBorder,
                                      yw->_screenSize.y - yw->_downScreenBorder - robo_map.h);
        }
        robo_map.maximized = false;
    }

    robo_map.field_1E8 &= ~(1 | 2 | 4 | 0x10 | 0x20 | 0x200);
    yw->_guiDragDefaultMouse = false;
    yw->_guiDragging = false;
    yw->_guiDragElement = nullptr;
    yw_RebuildRoboMapLayout(yw);
}

vec2d yw_RoboMapButtonPosToWorld(const Common::Point &btnPos)
{
    const float contentW = (float)(robo_map.w - robo_map.field_24C);
    const float contentH = (float)(robo_map.h - robo_map.field_250);

    return vec2d(robo_map.field_1D8 + ((float)btnPos.x - contentW * 0.5f) * robo_map.field_1E0,
                 robo_map.field_1DC - ((float)btnPos.y - contentH * 0.5f) * robo_map.field_1E4);
}

bool yw_IsRoboMapMarkerPositionValid(NC_STACK_ypaworld *yw, const vec2d &worldPos)
{
    return yw && yw->IsGamePlaySector(World::PositionToSectorID(worldPos));
}

bool yw_RoboMapMarkerOverlaps(const vec2d &worldPos, int ignoreIndex)
{
    const Common::Point candidate = sub_4F681C(worldPos);
    const int markerSize = yw_RoboMapMarkerSizePx();
    const int minDistance = markerSize + 3;
    const int minDistanceSq = minDistance * minDistance;

    for ( size_t i = 0; i < robo_map.customMarkers.size(); i++ )
    {
        if ( (int)i == ignoreIndex )
            continue;

        const Common::Point other = sub_4F681C(robo_map.customMarkers[i]);
        const int dx = other.x - candidate.x;
        const int dy = other.y - candidate.y;
        if ( dx * dx + dy * dy < minDistanceSq )
            return true;
    }

    return false;
}

int yw_FindRoboMapMarkerAt(const Common::Point &btnPos)
{
    if ( robo_map.customMarkers.empty() )
        return -1;

    const vec2d worldPos = yw_RoboMapButtonPosToWorld(btnPos);
    const float hitRadius = (float)(yw_RoboMapMarkerSizePx() / 2 + 4);
    const float maxDx = hitRadius * robo_map.field_1E0;
    const float maxDz = hitRadius * robo_map.field_1E4;
    int bestIndex = -1;
    float bestScore = 1.0e30f;

    for ( size_t i = 0; i < robo_map.customMarkers.size(); i++ )
    {
        const float dx = (robo_map.customMarkers[i].x - worldPos.x) / std::max(1.0f, maxDx);
        const float dz = (robo_map.customMarkers[i].y - worldPos.y) / std::max(1.0f, maxDz);
        const float score = dx * dx + dz * dz;
        if ( score <= 1.0f && score < bestScore )
        {
            bestScore = score;
            bestIndex = (int)i;
        }
    }

    return bestIndex;
}

int yw_AddRoboMapMarker(NC_STACK_ypaworld *yw, const Common::Point &btnPos)
{
    const vec2d worldPos = yw_RoboMapButtonPosToWorld(btnPos);
    if ( !yw_IsRoboMapMarkerPositionValid(yw, worldPos) )
        return -1;

    // Markers are personal annotations and may be placed in unexplored
    // gameplay sectors. They do not reveal terrain, units or ownership.
    if ( yw_RoboMapMarkerOverlaps(worldPos) )
        return -1;

    robo_map.customMarkers.push_back(worldPos);
    return (int)robo_map.customMarkers.size() - 1;
}

bool yw_PlaceRoboMapMarker(NC_STACK_ypaworld *yw, const Common::Point &btnPos)
{
    if ( yw_AddRoboMapMarker(yw, btnPos) < 0 )
        return false;

    robo_map.markerMode = true;
    yw->PlayConfiguredMapMarkerSound();
    return true;
}

bool yw_RemoveNearestRoboMapMarker(const Common::Point &btnPos)
{
    const int markerIndex = yw_FindRoboMapMarkerAt(btnPos);
    if ( markerIndex < 0 )
        return false;

    robo_map.customMarkers.erase(robo_map.customMarkers.begin() + markerIndex);
    return true;
}

bool yw_MoveRoboMapMarker(NC_STACK_ypaworld *yw, int markerIndex,
                                 const Common::Point &btnPos)
{
    if ( markerIndex < 0 || markerIndex >= (int)robo_map.customMarkers.size() )
        return false;

    const vec2d worldPos = yw_RoboMapButtonPosToWorld(btnPos);
    if ( !yw_IsRoboMapMarkerPositionValid(yw, worldPos)
            || yw_RoboMapMarkerOverlaps(worldPos, markerIndex) )
        return false;

    robo_map.customMarkers[markerIndex] = worldPos;
    return true;
}

int NC_STACK_ypaworld::GetTacticalMapZoomLevel() const
{
    return robo_map.field_1EE;
}

void NC_STACK_ypaworld::SetTacticalMapZoomLevel(int zoomLevel)
{
    robo_map.field_1EE = std::max(1, std::min(4, zoomLevel));
    sub_4C1970(this, 0);
}

void NC_STACK_ypaworld::SetFreshLevelTacticalMapZoom()
{
    // Zoom 3 is the second-closest tactical-map level.
    SetTacticalMapZoomLevel(3);
}

void yw_RenderCustomWorldMapMarkers(NC_STACK_ypaworld *yw)
{
    if ( !yw || !robo_map.markerMode || robo_map.customMarkers.empty() ||
         yw->IsSpectatorControlled() )
        return;

    // Reuse the same world-space SVG projection and faction tint as the rest
    // of the world UI, but keep map markers visible at any distance: they are
    // long-range navigation references, not local tactical hints.
    const uint32_t phase = yw->_timeStamp % 900;
    const uint32_t triangle = phase <= 450 ? phase : 900 - phase;
    const float bob = 90.0f + (float)triangle * 30.0f / 450.0f;
    const int radius = 8 + (int)(triangle * 2 / 450);

    for ( const vec2d &marker : robo_map.customMarkers )
    {
        const Common::Point cellId = World::PositionToSectorID(marker);
        if ( !yw->IsGamePlaySector(cellId) )
            continue;

        vec3d worldPos(marker.x, yw->_cells(cellId).height - bob, marker.y);

        Common::Point point;
        if ( !yw_ProjectWorldSelectionPoint(yw, worldPos, &point) )
            continue;

        yw_RenderOrderTemplateAt(yw, point, radius, 255,
                                 YW_ORDER_TEMPLATE_MAP_MARKER);
    }
}

bool NC_STACK_ypaworld::IsRoboMapOpen() const
{
    return robo_map.IsOpen();
}

// True when a visible wheel-enabled list (unit list, Squadron Manager) is under
// the pointer and therefore has priority over the world's wheel zoom.
bool NC_STACK_ypaworld::IsWheelOwnedByGuiList(const TClickBoxInf &click) const
{
    return yw_GuiListOwnsWheelAtPointer(gui_lstvw, click) ||
           yw_GuiListOwnsWheelAtPointer(squadron_manager, click);
}

void  RoboMap_InputHandle(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( robo_map.IsClosed() )
    {
        robo_map.field_1E8 &= 0xFFFFFDE8;
        robo_map.markerDragIndex = -1;
    }
    else
    {
        robo_map.MapViewMask = yw->IsSpectatorControlled() ? -1 : cellArea::ViewMask( yw->_userRobo->_owner );

        TClickBoxInf *winpt = &inpt->ClickInf;

        if ( yw->IsSpectatorControlled() && (robo_map.field_1E8 & 0x200) )
        {
            robo_map.field_1E8 &= 0xFFFFFDFF;
            yw->_guiDragDefaultMouse = false;
        }

        if ( yw->_showDebugMode )
            robo_map.MapViewMask = -1;

        // The Tactical Map normally owns the wheel globally while open, but a
        // foreground wheel-enabled list under the pointer has higher UI
        // priority.  Both the unit/operations list and Squadron Manager are
        // processed later in the same input pass, so leave the wheel untouched
        // here and let that list consume it.
        const bool listOwnsWheel =
            yw_GuiListOwnsWheelAtPointer(gui_lstvw, *winpt) ||
            yw_GuiListOwnsWheelAtPointer(squadron_manager, *winpt);
        const int mapWheel = listOwnsWheel ? 0 : winpt->wheel;
        const int mapZoomInSteps =
            World::GetFixedInputShortcutWheelCount(World::INPUT_BIND_ZOOMIN, mapWheel);
        const int mapZoomOutSteps =
            World::GetFixedInputShortcutWheelCount(World::INPUT_BIND_ZOOMOUT, mapWheel);

        for ( int i = 0; i < mapZoomInSteps; ++i )
            sub_4C1970(yw, 1);
        for ( int i = 0; i < mapZoomOutSteps; ++i )
            sub_4C1970(yw, 2);

        if ( mapZoomInSteps != 0 || mapZoomOutSteps != 0 )
            winpt->wheel = 0;

        switch ( inpt->HotKeyID )
        {
        case 10:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 3;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 11:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 4;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 12:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 5;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 53:
            winpt->selected_btn = &robo_map;
            winpt->selected_btnID = 6;
            winpt->flag |= TClickBoxInf::FLAG_BTN_DOWN;
            break;

        case 16:
            // The shared Zoom In/Out pair is contextual. While the tactical
            // map is open it belongs exclusively to the map, anywhere on screen.
            sub_4C1970(yw, 1);
            yw_FlashMapToolbarZoomButton(18);
            inpt->HotKeyID = -1;
            break;

        case 17:
            sub_4C1970(yw, 2);
            yw_FlashMapToolbarZoomButton(19);
            inpt->HotKeyID = -1;
            break;

        case 49:
            // The remappable action is an additional route. Right click
            // remains the permanent contextual marker shortcut handled below.
            // If the user remaps the primary action itself to RMB, let the
            // contextual route own that click so add/remove still toggles cleanly.
            if ( winpt->selected_btn == &robo_map && winpt->selected_btnID == 17 &&
                 !World::IsFixedInputShortcutPressed(inpt, World::INPUT_BIND_PLACE_MAP_MARKER) )
            {
                yw_PlaceRoboMapMarker(yw, winpt->move.BtnPos);
                yw->_guiDragDefaultMouse = false;
            }
            break;

        default:
            break;
        }

        if ( winpt->selected_btn == &robo_map
                && (winpt->selected_btnID == 1 || winpt->selected_btnID == 16)
                && (winpt->flag & TClickBoxInf::FLAG_DBL_CLICK) )
        {
            yw_ToggleRoboMapFullscreen(yw);
            winpt->flag &= ~(TClickBoxInf::FLAG_LM_DOWN |
                             TClickBoxInf::FLAG_LM_HOLD |
                             TClickBoxInf::FLAG_BTN_DOWN |
                             TClickBoxInf::FLAG_BTN_HOLD);
        }

        if ( robo_map.field_1E8 & 1 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                int v15 = winpt->move.ScreenPos.x + robo_map.field_218;
                int v16 = winpt->move.ScreenPos.y + robo_map.field_21A;

                if ( v15 < robo_map.field_210 )
                    v15 = robo_map.field_210;
                else if ( v15 > robo_map.field_214 )
                    v15 = robo_map.field_214;

                if ( v16 < robo_map.field_212 )
                    v16 = robo_map.field_212;
                else if ( v16 > robo_map.field_216 )
                    v16 = robo_map.field_216;

                if ( v15 + robo_map.x >= yw->_screenSize.x )
                    v15 = yw->_screenSize.x - robo_map.x;

                if ( robo_map.y + v16 >= yw->_screenSize.y - yw->_downScreenBorder )
                    v16 = yw->_screenSize.y - yw->_downScreenBorder - robo_map.y;

                robo_map.h = v16;
                robo_map.w = v15;
            }
            else
            {
                robo_map.field_1E8 &= 0xFFFFFFFE;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.field_1E8 & 2 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                robo_map.field_1ED = 0;
                robo_map.field_1D8 = (yw->_mapSize.x * World::CVSectorLength) / (float)robo_map.field_1CC * (float)(winpt->move.ScreenPos.x - robo_map.field_21C ) + robo_map.field_220;
            }
            else
            {
                robo_map.field_1E8 &= 0xFFFFFFFD;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.field_1E8 & 4 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                robo_map.field_1ED = 0;
                robo_map.field_1DC = robo_map.field_224 - (yw->_mapSize.y * World::CVSectorLength) / (float)robo_map.field_1D2 * (float)(winpt->move.ScreenPos.y - robo_map.field_21E);
            }
            else
            {
                robo_map.field_1E8 &= 0xFFFFFFFB;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.markerDragIndex >= 0 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
            {
                yw_MoveRoboMapMarker(yw, robo_map.markerDragIndex,
                                     winpt->move.BtnPos);
                winpt->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
                yw->_guiDragDefaultMouse = true;
            }
            else
            {
                robo_map.markerDragIndex = -1;
                winpt->flag &= ~(TClickBoxInf::FLAG_LM_DOWN
                               | TClickBoxInf::FLAG_LM_UP);
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.field_1E8 & 0x10 )
        {
            if ( winpt->flag & TClickBoxInf::FLAG_MM_HOLD )
            {
                robo_map.field_1ED = 0;
                robo_map.field_1D8 = robo_map.field_220 + (float)(robo_map.field_21C - winpt->move.ScreenPos.x) * robo_map.field_1E0;
                robo_map.field_1DC = robo_map.field_224 - (float)(robo_map.field_21E - winpt->move.ScreenPos.y) * robo_map.field_1E4;
            }
            else
            {
                int v25 = abs(robo_map.field_21C - winpt->move.ScreenPos.x);
                int v27 = abs(robo_map.field_21E - winpt->move.ScreenPos.y);

                if ( v25 <= 1 && v27 <= 1 )
                {
                    yw->_activeCmdrRemapIndex = -1;
                    yw->_activeCmdrID = 0;

                    if ( bzda.field_1D0 & 0x28 )
                        bzda.field_1D0 = bzda.field_1CC & 1;
                }
                robo_map.field_1E8 &= 0xFFFFFFEF;
                yw->_guiDragDefaultMouse = false;
            }
        }
        else if ( robo_map.field_1E8 & 0x200 )
        {
            dword_516518 = winpt->move.ScreenPos.x;
            dword_51651C = winpt->move.ScreenPos.y;

            sub_4C1FBC();

            if ( !(winpt->flag & TClickBoxInf::FLAG_LM_HOLD) )
            {
                if ( abs(dword_516518 - dword_516510) <= 5 || abs(dword_51651C - dword_516514) <= 5 )
                {
                    robo_map.field_1E8 &= 0xFFFFFDFF;
                    winpt->flag |= TClickBoxInf::FLAG_LM_DOWN;
                }
                else
                {
                    robo_map.field_1E8 &= 0xFFFFFDFF;

                    if ( !yw->IsSpectatorControlled() )
                    {
                        bzda.field_1D0 = 1;
                        ypaworld_func64__sub7__sub1__sub0(yw);
                    }
                }
            }
        }
        else if ( winpt->selected_btn == &robo_map )
        {
            // While an artillery shell is in manual aiming mode, RMB belongs to
            // artillery cancel. Do not let the normal map-marker handler consume
            // the same edge before ypaworld_func64__sub21() can clear the white ring.
            if ( !yw->_artilleryShellManualGid &&
                 World::IsFixedInputShortcutPressed(inpt, World::INPUT_BIND_PLACE_MAP_MARKER) )
            {
                if ( winpt->selected_btnID == 17 )
                {
                    // Right mouse owns marker interaction on the tactical map:
                    // click an existing marker to remove it, otherwise place one.
                    // The fixed shortcut comes from the same shared definition
                    // used by Input Settings, so runtime and menu text stay aligned.
                    if ( yw_FindRoboMapMarkerAt(winpt->move.BtnPos) >= 0 )
                        yw_RemoveNearestRoboMapMarker(winpt->move.BtnPos);
                    else
                        yw_PlaceRoboMapMarker(yw, winpt->move.BtnPos);

                    const World::TInputFixedShortcut markerShortcut =
                        World::GetInputFixedShortcut(World::INPUT_BIND_PLACE_MAP_MARKER);
                    if ( markerShortcut.KeyCode == Input::KC_LMB )
                        winpt->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
                    else if ( markerShortcut.KeyCode == Input::KC_RMB )
                        winpt->flag &= ~TClickBoxInf::FLAG_RM_DOWN;
                    else if ( markerShortcut.KeyCode == Input::KC_MMB )
                        winpt->flag &= ~TClickBoxInf::FLAG_MM_DOWN;

                    yw->_guiDragDefaultMouse = false;
                }
            }

            if ( winpt->flag & TClickBoxInf::FLAG_MM_DOWN )
            {
                if ( winpt->selected_btnID == 17 )
                {
                    // Middle mouse exclusively owns tactical-map panning.
                    // Reuse the existing pan state/path rather than adding a
                    // second implementation with subtly different behavior.
                    robo_map.field_220 = robo_map.field_1D8;
                    robo_map.field_21C = winpt->move.ScreenPos.x;
                    robo_map.field_21E = winpt->move.ScreenPos.y;
                    robo_map.field_224 = robo_map.field_1DC;
                    robo_map.field_1E8 |= 0x10;
                    winpt->flag &= ~TClickBoxInf::FLAG_MM_DOWN;
                    yw->_guiDragDefaultMouse = true;
                }
            }

            if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
            {
                const int markerIndex = (winpt->selected_btnID == 17
                                         && robo_map.markerMode)
                    ? yw_FindRoboMapMarkerAt(winpt->move.BtnPos)
                    : -1;

                if ( markerIndex >= 0 )
                {
                    // Left press/hold moves the marker. The drag owns the click
                    // so unit selection and move/attack orders are not emitted.
                    robo_map.markerDragIndex = markerIndex;
                    winpt->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
                    yw->_guiDragDefaultMouse = true;
                }
                else if ( winpt->selected_btnID == 17
                        && !yw->IsSpectatorControlled() )
                {
                    robo_map.field_1E8 |= 0x200;
                    dword_516510 = winpt->move.ScreenPos.x;
                    dword_516514 = winpt->move.ScreenPos.y;
                    dword_516518 = dword_516510;
                    dword_51651C = dword_516514;

                    sub_4C1FBC();

                    winpt->flag &= ~TClickBoxInf::FLAG_LM_DOWN;
                }
            }

            if ( (winpt->selected_btnID > 1 && winpt->selected_btnID <= 9)
                 || winpt->selected_btnID == 18
                 || winpt->selected_btnID == 19 )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                {
                    if ( yw->_GameShell )
                        SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
                }
            }

            if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
            {
                switch ( winpt->selected_btnID )
                {
                case 16:
                    robo_map.field_218 = robo_map.w - winpt->ldw_pos.ScreenPos.x;
                    robo_map.field_21A = robo_map.h - winpt->ldw_pos.ScreenPos.y;

                    robo_map.field_1E8 |= 1;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 14:
                    robo_map.field_21C = winpt->ldw_pos.ScreenPos.x;
                    robo_map.field_220 = robo_map.field_1D8;
                    robo_map.field_1E8 |= 2;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 11:
                    robo_map.field_21E = winpt->ldw_pos.ScreenPos.y;
                    robo_map.field_224 = robo_map.field_1DC;
                    robo_map.field_1E8 |= 4;
                    yw->_guiDragDefaultMouse = true;
                    break;

                case 3:
                    if ( robo_map.field_1EC & 1 )
                        robo_map.field_1EC &= 0xFE;
                    else
                        robo_map.field_1EC |= 1;

                    sub_4C1970(yw, 0);
                    break;

                case 18:
                    sub_4C1970(yw, 1);
                    yw_FlashMapToolbarZoomButton(18);
                    break;

                case 19:
                    sub_4C1970(yw, 2);
                    yw_FlashMapToolbarZoomButton(19);
                    break;

                case 4:
                    if ( robo_map.field_1EC & 2 )
                        robo_map.field_1EC &= 0xFD;
                    else
                        robo_map.field_1EC |= 2;

                    sub_4C1970(yw, 0);
                    break;

                case 5:
                    if ( robo_map.field_1EC & 4 )
                        robo_map.field_1EC &= 0xFB;
                    else
                        robo_map.field_1EC |= 4;

                    sub_4C1970(yw, 0);
                    break;

                case 6:
                    robo_map.field_1ED = robo_map.field_1ED != 1;
                    break;

                case 9:
                    robo_map.markerMode = !robo_map.markerMode;
                    robo_map.markerDragIndex = -1;
                    robo_map.field_1E8 &= ~(0x10 | 0x200);
                    yw->_guiDragDefaultMouse = false;
                    break;

                default:
                    break;
                }
            }

            switch ( winpt->selected_btnID )
            {
            case 13:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.field_220 = robo_map.field_1D8;
                    robo_map.field_1ED = 0;
                    robo_map.field_1D8 = yw->_mapSize.x * World::CVSectorLength / (-1.0 * (float)robo_map.field_1CC) + robo_map.field_1D8;
                }
                break;

            case 15:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.field_220 = robo_map.field_1D8;
                    robo_map.field_1ED = 0;
                    robo_map.field_1D8 = yw->_mapSize.x * World::CVSectorLength / (float)robo_map.field_1CC + robo_map.field_1D8;
                }
                break;

            case 10:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.field_224 = robo_map.field_1DC;
                    robo_map.field_1ED = 0;
                    robo_map.field_1DC = robo_map.field_1DC - yw->_mapSize.y * World::CVSectorLength / (-1.0 * (float)robo_map.field_1D2);
                }
                break;

            case 12:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                {
                    robo_map.field_224 = robo_map.field_1DC;
                    robo_map.field_1ED = 0;
                    robo_map.field_1DC = robo_map.field_1DC - yw->_mapSize.y * World::CVSectorLength / (float)robo_map.field_1D2;
                }
                break;

            default:
                break;
            }

            switch ( winpt->selected_btnID )
            {
            case 3:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_DETAIL, 10);
                break;

            case 4:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_OWNERS, 11);
                break;

            case 5:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_EINFO, 12);
                break;

            case 6:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_LOCKVIEWER, 53);
                break;

            case 18:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_ZOOMIN, 16);
                break;

            case 19:
                yw->SetShowingTooltipWithHotkey(Locale::TIP_MAP_ZOOMOUT, 17);
                break;

            default:
                break;
            }
        }
    }
}

void NC_STACK_ypaworld::yw_MAP_MouseSelect(TClickBoxInf *winp)
{
    int v23 = winp->move.BtnPos.x + robo_map.field_1D8 / robo_map.field_1E0 - (robo_map.field_1F8 / 2);
    int v24 = winp->move.BtnPos.y - (robo_map.field_1DC / robo_map.field_1E4) - (robo_map.field_1FC / 2);

    int v9 = robo_map.field_1EE + 2;

    Common::Point pt( v23 >> v9, v24 >> v9);

    if ( IsGamePlaySector( pt ) )
    {
        cellArea &cell = _cells( pt );

        if ( _guiActFlags & 1 )
        {
            _cellOnMouse = &cell;
            _guiActFlags |= 0x10;
            _cellMouseIsectPos.x = (float)v23 * robo_map.field_1E0 + 0.5;
            _cellMouseIsectPos.y = cell.height;
            _cellMouseIsectPos.z = -((float)v24 * robo_map.field_1E4 + 0.75);
        }

        bool canSelectCell = IsSpectatorControlled() ? ((robo_map.MapViewMask & cell.view_mask) != 0) : cell.IsCanSee(_userRobo->_owner);

        if ( _guiActFlags & 2 && !(bzda.field_1D0 & 0x30) && canSelectCell )
        {

            int v16 = 0;

            while ( v16 < 2 )
            {
                for( NC_STACK_ypabact* &v17 : cell.unitsList )
                {
                    if ( v17->ShouldHideFromStrategicUI() )
                        continue;

                    if ( IsSpectatorControlled() && v17->_owner == World::OWNER_0 )
                        continue;

                    if ( (v16 != 1 || v17->_owner != _userRobo->_owner) && (v16 || v17->_owner == _userRobo->_owner) )
                    {
                        if ( v17->_status != BACT_STATUS_CREATE && v17->_status != BACT_STATUS_DEAD && v17->_status != BACT_STATUS_BEAM && v17->_bact_type != BACT_TYPES_MISSLE )
                        {
                            int v19 = v17->_position.x / robo_map.field_1E0;
                            int v20 = v17->_position.z / robo_map.field_1E4;

                            if ( abs(v19 - v23) <= 8 && abs(-v20 - v24) <= 8 )
                            {
                                _guiActFlags |= 0x20;

                                _bactOnMouse = v17;
                                _guiActFlags &= ~0x10;

                                v16 = 2;
                                break;
                            }
                        }
                    }
                }

                v16++;
            }
        }
    }
}
