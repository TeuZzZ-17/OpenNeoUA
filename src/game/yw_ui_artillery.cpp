// Artillery shell elements of the map and HUD (markers, cooldown bars, map click).
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

bool yw_ShouldRenderArtilleryShellCooldownBar(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    if ( !yw || !bact || !bact->IsArtilleryShellPlatform() )
        return false;

    if ( yw->IsSpectatorControlled() )
        return bact->_owner != World::OWNER_0 && !yw->IsSpectatorBact(bact);

    return yw->_userRobo && bact->_owner == yw->_userRobo->_owner;
}

bool yw_RenderArtilleryShellCooldownBar(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact,
                                       int left, int top, int squareCount)
{
    if ( !yw_ShouldRenderArtilleryShellCooldownBar(yw, bact) )
        return false;

    float ratio = bact->GetArtilleryShellReadinessRatio();
    if ( ratio < 0.0f )
        ratio = 0.0f;
    if ( ratio > 1.0f )
        ratio = 1.0f;

    // Tiles 3/7 are kept separate from the HP (2/6) and shield (1/5) pairs.
    yw_RenderUnitSquareBar(yw, cur, left, top, squareCount, dround(ratio * 1000.0f), 1000, 3, 7);
    return true;
}

void yw_RenderArtilleryShellCooldownRadarBar(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact,
                                            int iconWidth, int iconHeight)
{
    if ( !yw_ShouldRenderArtilleryShellCooldownBar(yw, bact) || bact->_energy_max <= 0 )
        return;

    float ratio = bact->GetArtilleryShellReadinessRatio();
    if ( ratio < 0.0f )
        ratio = 0.0f;
    if ( ratio > 1.0f )
        ratio = 1.0f;

    Common::Point center = sub_4F681C(bact->_position.XZ());

    int width = iconWidth > 5 ? iconWidth : 5;
    int y = center.y - (iconHeight / 2) - 3;
    int x0 = center.x - (width / 2);
    int x1 = x0 + width - 1;

    int clipLeft = robo_map.field_200;
    int clipTop = robo_map.field_204;
    int clipRight = robo_map.field_200 + robo_map.field_1F8 - 1;
    int clipBottom = robo_map.field_204 + robo_map.field_1FC - 1;

    if ( y < clipTop || y > clipBottom || x1 < clipLeft || x0 > clipRight )
        return;

    if ( x0 < clipLeft )
        x0 = clipLeft;
    if ( x1 > clipRight )
        x1 = clipRight;

    SDL_Color bg = {48, 48, 48, 255};
    SDL_Color fg = {255, 255, 255, 255};

    GFX::Engine.raster_func217(bg);
    GFX::Engine.raster_func201(Common::Line(Common::Point(x0, y), Common::Point(x1, y)));

    int filled = x0 + dround((float)(x1 - x0) * ratio);
    if ( filled < x0 )
        filled = x0;
    if ( filled > x1 )
        filled = x1;

    GFX::Engine.raster_func217(fg);
    GFX::Engine.raster_func201(Common::Line(Common::Point(x0, y), Common::Point(filled, y)));
}

void yw_RenderArtilleryShellCooldownRadarBars(NC_STACK_ypaworld *yw, int cellX0, int cellY0, int cellX1, int cellY1)
{
    // This helper is used by both the opened strategic map and the small gameplay
    // radar/minimap. Do not gate it on field_1EC & 4: the small radar path
    // temporarily sets field_1EC to 1 even though we still want the artillery shell
    // readiness bar there. Visibility/ownership is filtered per unit below.
    if ( !yw || robo_map.field_1EE <= 2 )
        return;

    for (int cy = cellY0; cy <= cellY1; cy++)
    {
        for (int cx = cellX0; cx <= cellX1; cx++)
        {
            cellArea &cell = yw->_cells(cx, cy);
            if ( !(robo_map.MapViewMask & cell.view_mask) )
                continue;

            for (NC_STACK_ypabact *bact : cell.unitsList)
            {
                if ( !bact || bact->ShouldHideFromStrategicUI() || bact->_bact_type == BACT_TYPES_MISSLE )
                    continue;

                // OpenNeoUA invisible: no artillery shell-cooldown radar bar for a cloaked unit.
                if ( bact->IsInvisibleUnrevealed() )
                    continue;

                int iconW = 7;
                int iconH = 7;

                if ( bact->_bact_type == BACT_TYPES_ROBO )
                {
                    iconH = yw->_guiTiles[1]->h;
                    iconW = yw->_guiTiles[1]->map[24].w;
                }

                yw_RenderArtilleryShellCooldownRadarBar(yw, bact, iconW, iconH);
            }
        }
    }
}

// OpenNeoUA custom: artillery bombardment marker artwork is fully asset-driven.
// Weapon scripts author a SVG relative to Data/Interface/Map/Markers.  The same
// white/alpha artwork is color-modulated at runtime for faction/queued/invalid
// states, so one vector asset is the single visual source of truth.
const char *yw_ArtilleryShellDefaultMarkerFile()
{
    return "artillery_ring_classic.svg";
}

std::string yw_ResolveArtilleryShellMarkerAsset(const std::string &authored)
{
    static const std::string ROOT = "Interface/Map/Markers/";
    static const std::string DATA_ROOT = "Data/Interface/Map/Markers/";

    std::string relative = authored.empty() ? yw_ArtilleryShellDefaultMarkerFile() : authored;
    std::replace(relative.begin(), relative.end(), '\\', '/');

    if ( relative.rfind(DATA_ROOT, 0) == 0 )
        relative.erase(0, DATA_ROOT.size());
    else if ( relative.rfind(ROOT, 0) == 0 )
        relative.erase(0, ROOT.size());

    const size_t dot = relative.find_last_of('.');
    const bool safe = !relative.empty() && relative.front() != '/' &&
                      relative.find(':') == std::string::npos &&
                      relative.find("../") == std::string::npos &&
                      relative.find("/..") == std::string::npos &&
                      relative != "." && relative != ".." &&
                      dot != std::string::npos && dot + 1 < relative.size() &&
                      !StriCmp(relative.substr(dot + 1), "svg");

    if ( !safe )
        relative = yw_ArtilleryShellDefaultMarkerFile();

    return ROOT + relative;
}

NC_STACK_bitmap *yw_LoadArtilleryShellMarkerSvg(const std::string &authored,
                                                        int width, int height)
{
    const std::string requested = yw_ResolveArtilleryShellMarkerAsset(authored);
    NC_STACK_bitmap *bitmap = StatusIconLoad(requested, width, height);
    if ( bitmap && bitmap->GetBitmap() )
        return bitmap;

    const std::string fallback = yw_ResolveArtilleryShellMarkerAsset(
        yw_ArtilleryShellDefaultMarkerFile());
    if ( requested != fallback )
        return StatusIconLoad(fallback, width, height);

    return nullptr;
}

bool yw_RenderArtilleryShellMarkerSvg(NC_STACK_ypaworld *yw,
                                              const vec3d &pos, float radius,
                                              const std::string &markerPath,
                                              const SDL_Color &color)
{
    if ( !yw || radius <= 0.01f || robo_map.field_1E0 <= 0.001f ||
         robo_map.field_1E4 <= 0.001f )
        return false;

    const Common::Point center = sub_4F681C(pos.XZ());
    const int radiusX = std::max(2, dround(radius / robo_map.field_1E0));
    const int radiusY = std::max(2, dround(radius / robo_map.field_1E4));
    const int width = radiusX * 2;
    const int height = radiusY * 2;

    NC_STACK_bitmap *bitmap = yw_LoadArtilleryShellMarkerSvg(markerPath, width, height);
    if ( !bitmap || !bitmap->GetBitmap() || !bitmap->GetBitmap()->swTex )
        return false;

    SDL_Surface *surface = bitmap->GetBitmap()->swTex;
    Uint8 oldR = 255, oldG = 255, oldB = 255;
    SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetSurfaceColorMod(surface, &oldR, &oldG, &oldB);
    SDL_GetSurfaceBlendMode(surface, &oldBlendMode);
    SDL_SetSurfaceColorMod(surface, color.r, color.g, color.b);
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    const int screenCenterX = center.x + yw->_screenSize.x / 2;
    const int screenCenterY = center.y + yw->_screenSize.y / 2;
    const int left = screenCenterX - radiusX;
    const int top = screenCenterY - radiusY;
    const float halfW = (float)yw->_screenSize.x * 0.5f;
    const float halfH = (float)yw->_screenSize.y * 0.5f;

    GFX::rstr_arg204 arg;
    arg.pbitm = bitmap->GetBitmap();
    arg.float4 = Common::FRect(-1.0, -1.0, 1.0, 1.0);
    arg.float14 = Common::FRect(
        ((float)left / halfW) - 1.0f,
        ((float)top / halfH) - 1.0f,
        ((float)(left + width) / halfW) - 1.0f,
        ((float)(top + height) / halfH) - 1.0f);
    arg.opacity = color.a;
    GFX::Engine.raster_func204(&arg);

    SDL_SetSurfaceColorMod(surface, oldR, oldG, oldB);
    SDL_SetSurfaceBlendMode(surface, oldBlendMode);
    return true;
}

bool yw_IsArtilleryShellMarkerVisible(NC_STACK_ypaworld *yw, const NC_STACK_ypaworld::ArtilleryShellMarker &marker, const Common::Point &cellId)
{
    if ( !yw || !yw->IsSector(cellId) )
        return false;

    if ( yw->_userRobo && marker.owner == yw->_userRobo->_owner )
        return true;

    cellArea &cell = yw->SectorAt(cellId);
    if ( yw->IsSpectatorControlled() )
        return (robo_map.MapViewMask & cell.view_mask) != 0;

    return yw->_userRobo && cell.IsCanSee(yw->_userRobo->_owner);
}

// Resolve exactly the point that HandleArtilleryShellMapClick() would accept under
// the current cursor. Keeping preview and click on the same target resolver avoids
// colour/position disagreements when the cursor is over a unit instead of a cell.
bool yw_GetArtilleryShellMapCursorTarget(NC_STACK_ypaworld *yw, vec3d *outTarget)
{
    if ( !yw || !outTarget )
        return false;

    if ( yw->_guiActFlags & 0x10 )
    {
        *outTarget = yw->_cellMouseIsectPos;
        return true;
    }

    if ( (yw->_guiActFlags & 0x20) && yw->_bactOnMouse )
    {
        *outTarget = yw->_bactOnMouse->_position;
        return true;
    }

    return false;
}

void NC_STACK_ypaworld::RenderArtilleryShellMapMarkers()
{
    ExpireArtilleryShellMarkers();

    if ( robo_map.field_1E0 <= 0.001f || robo_map.field_1E4 <= 0.001f )
        return;

    // Active bombardment zones use the weapon-authored SVG. The rasterized
    // size follows barrage_radius at the current map zoom; only the runtime tint changes.
    for ( const ArtilleryShellMarker &marker : _artilleryShellMarkers )
    {
        if ( marker.radius <= 0.01f )
            continue;

        Common::Point cellId = World::PositionToSectorID(marker.pos);
        if ( !yw_IsArtilleryShellMarkerVisible(this, marker, cellId) )
            continue;

        // A pending artillery order is intentionally unavailable until the source
        // platform finishes its cooldown, so render it with a neutral disabled
        // grey. Active/in-flight bombardment zones keep the normal faction colour.
        SDL_Color clr = marker.pending
            ? SDL_Color{128, 128, 128, 255}
            : GetColor(marker.owner);
        yw_RenderArtilleryShellMarkerSvg(this, marker.pos, marker.radius,
                                         marker.markerPath, clr);
    }

    // Cursor-following aiming preview on the opened map:
    //   valid + ready now   -> selected platform faction colour
    //   valid + queued      -> disabled grey
    //   invalid target      -> bright red
    // The small radar only shows accepted barrage/pending markers, never this preview.
    if ( robo_map.IsOpen() && _artilleryShellManualGid && _artilleryShellManualRadius > 0.01f )
    {
        vec3d previewTarget;
        if ( yw_GetArtilleryShellMapCursorTarget(this, &previewTarget) )
        {
            NC_STACK_ypabact *artilleryShell = FindLiveBactByGid(_artilleryShellManualGid);
            bool validTarget = false;
            bool readyNow = false;

            if ( artilleryShell && _userRobo && artilleryShell->_owner == _userRobo->_owner &&
                 artilleryShell->IsManualArtilleryShellPlatform() )
            {
                validTarget = artilleryShell->CanManualArtilleryShell(previewTarget, nullptr, &readyNow);
            }

            SDL_Color previewColor;
            if ( !validTarget )
                previewColor = SDL_Color{255, 48, 48, 255};
            else if ( !readyNow )
                previewColor = SDL_Color{128, 128, 128, 255};
            else
                previewColor = GetColor(artilleryShell->_owner);

            const std::string markerPath = artilleryShell
                ? artilleryShell->GetArtilleryShellMarkerPath()
                : std::string();
            yw_RenderArtilleryShellMarkerSvg(this, previewTarget,
                                              _artilleryShellManualRadius,
                                              markerPath, previewColor);
        }
    }
}

// OpenNeoUA custom: ultra-simple manual artillery shell control on the 2D strategic map.
// Flow (no extra key, no energy cost):
//   1. Player clicks one of their own manual-capable artillery shell platforms -> selected.
//   2. Player clicks a target area (empty cell or a visible enemy) -> that single
//      artillery shell starts a bombardment there if the strike is valid.
// Returns true when the click was consumed (so no first-person entry / RTS order
// is generated for it). Single-player only.
bool NC_STACK_ypaworld::HandleArtilleryShellMapClick()
{
    if ( _isNetGame || !_userRobo )
        return false;

    int playerOwner = _userRobo->_owner;

    // A artillery shell is already selected: this click chooses the target area.
    // Clicking another own artillery shell keeps the old convenient re-select behaviour.
    if ( _artilleryShellManualGid )
    {
        if ( (_guiActFlags & 0x20) && _bactOnMouse &&
             _bactOnMouse->_owner == playerOwner &&
             _bactOnMouse->IsManualArtilleryShellPlatform() )
        {
            _artilleryShellManualGid = _bactOnMouse->_gid;
            _artilleryShellManualRadius = _bactOnMouse->GetArtilleryShellBarrageRadius();
            return true;
        }

        // Resolve the exact same point used by the faction/grey/red preview.
        vec3d target;
        if ( !yw_GetArtilleryShellMapCursorTarget(this, &target) )
            return false; // nothing useful under the cursor: leave selection pending

        uint32_t gid = _artilleryShellManualGid;

        // Re-resolve the selected artillery shell by gid (no dangling pointer risk if it
        // died between the two clicks).
        NC_STACK_ypabact *artilleryShell = nullptr;
        for (int y = 0; y < _mapSize.y && !artilleryShell; y++)
        {
            for (int x = 0; x < _mapSize.x && !artilleryShell; x++)
            {
                Common::Point cellId(x, y);
                if ( !IsSector(cellId) )
                    continue;

                for (NC_STACK_ypabact *unit : SectorAt(cellId).unitsList)
                {
                    if ( unit && unit->_gid == gid && unit->_owner == playerOwner )
                    {
                        artilleryShell = unit;
                        break;
                    }
                }
            }
        }

        if ( !artilleryShell )
        {
            // The selected artillery shell is gone: clear the selection/preview.
            _artilleryShellManualGid = 0;
            _artilleryShellManualRadius = 0.0f;
            return true;
        }

        int wid = 0;
        bool readyNow = false;
        if ( artilleryShell->CanManualArtilleryShell(target, &wid, &readyNow) )
        {
            // Target is valid (range + radar + manual-call). Fire now only if the
            // platform is idle and off cooldown. If a barrage is already active or
            // cooling down, queue this as the single future area: the current barrage
            // stays locked to its original target and the queued ring remains grey.
            if ( readyNow )
                artilleryShell->StartArtilleryShellBarrage(target);
            else
                artilleryShell->QueueManualArtilleryShell(target);

            // Order accepted: clear selection and hide the cursor-following SVG preview.
            _artilleryShellManualGid = 0;
            _artilleryShellManualRadius = 0.0f;
        }
        // Else (out of range / no radar): invalid target, keep the selection + red
        // preview so the player can pick a different spot without re-selecting.

        return true; // consume the target click
    }

    // No artillery shell selected yet: clicking one of our manual artillery shells selects it.
    if ( (_guiActFlags & 0x20) && _bactOnMouse &&
         _bactOnMouse->_owner == playerOwner &&
         _bactOnMouse->IsManualArtilleryShellPlatform() )
    {
        _artilleryShellManualGid = _bactOnMouse->_gid;
        _artilleryShellManualRadius = _bactOnMouse->GetArtilleryShellBarrageRadius();
        return true; // consume: no first-person / RTS select on an artillery shell platform
    }

    return false; // not an artillery shell interaction: keep vanilla behaviour
}
