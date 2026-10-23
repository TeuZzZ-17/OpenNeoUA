// Squadron manager window: layout, rows, icons, aggression strip, drag and input.
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

uint8_t yw_GetSquadronManagerOpacity()
{
    return yw_GetConfiguredUiOpacity(System::IniConf::UiSquadronManagerOpacity,
                                     System::IniConf::UiSquadronManagerDefaultOpacity);
}

void create_squad_man(NC_STACK_ypaworld *yw)
{
    const std::string title = "Squad";
    const int compactEntryWidth = yw_GetSquadronManagerCompactEntryWidth(yw);
    const int defaultEntryWidth = yw_GetSquadronManagerDefaultEntryWidth(yw);

    GuiList::tInit args;
    args.title = title;
    args.resizeable = true;
    args.numEntries = 0;
    args.shownEntries = kSquadronManagerDefaultShownEntries;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 24;
    args.minShownEntries = 1;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = defaultEntryWidth;
    args.minEntryWidth = compactEntryWidth;
    args.maxEntryWidth = 32000;
    args.enabled = true;
    args.upperVborder = yw->_fontBorderH;
    // Reserve one fixed footer row for the aggression control.  The footer is
    // outside the scrollable squad rows, so it remains visible while the list
    // is scrolled or vertically resized.
    args.lowerVborder = yw->_fontH + yw->_fontBorderH;
    args.closeChar = 73;
    args.thinScrollbar = true;
    args.thinScrollbarVisualWidth = GuiList::kThinScrollbarVisualSize;
    args.fillThinScrollbarGap = true;
    // Use the original titlebar close glyph: its built-in backing tile fits
    // the legacy window frame better than the map toolbar PNG.
    args.factionCloseVisual = false;
    args.backgroundOpacity = yw_GetSquadronManagerOpacity();
    args.wheelScroll = true;

    if ( squadron_manager.Init(yw, args) )
    {
        yw_UpdateSquadronManagerColumnLayout(yw);
        int aggressionWidth = 0;
        for ( int i = 0; i < 5; i++ )
            aggressionWidth += yw->_guiTiles[24]->map[49 + i].w;
        const int footerContentWidth = squadron_manager.entryWidth
                                     - 2 * yw->_fontBorderW;
        squadron_manager.field_2DC = yw->_fontBorderW
                                   + std::max(0, (footerContentWidth
                                                - aggressionWidth) / 2);
        squadron_manager.aggressionCommandID = 0;
        squadron_manager.preDraw = yw_PreDrawSquadronManager;
        squadron_manager.postDraw = yw_PostDrawSquadronManager;
        yw_UpdateSquadronManagerTitleButtons(yw);
    }
}

bool yw_RenderSquadronManagerIcon(NC_STACK_ypaworld *yw,
                                         int windowX, int windowY,
                                         const char *iconName, bool active,
                                         const ButtonBox &box)
{
    if ( !yw || !iconName || !*iconName )
        return false;

    const int owner = yw_RoboMapMarkerOwner(yw);
    const int iconSize = std::max(8, std::min(box.w, box.h) - 2);
    // StatusIconLoad resolves Data-relative paths. Squadron buttons keep the
    // original owner-authored layered SVGs for maximum small-size definition.
    YwLoadedUiBitmap icon = yw_LoadFactionToolbarIcon(
        owner, "Interface/Squad/Buttons", iconName, active, iconSize);
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

int yw_GetSquadronManagerTitleButtonWidth(NC_STACK_ypaworld *yw)
{
    return std::max(18, std::min(24, yw->_fontH));
}

int yw_GetSquadronManagerTextWidth(NC_STACK_ypaworld *yw,
                                          const std::string &text)
{
    const int screenTextWidth = GFX::Engine.MeasureScreenTextWidth(text);
    return screenTextWidth > 0
        ? screenTextWidth
        : yw->_guiTiles[0]->GetWidth(text);
}

int yw_GetSquadronManagerStatusCellWidth(NC_STACK_ypaworld *yw)
{
    const auto &fnt28 = yw->_guiTiles[28]->map;
    return std::max(
        1, fnt28[97].w - fnt28[64].w + kSquadronManagerStatusCountGap);
}

int yw_GetSquadronManagerCompactEntryWidth(NC_STACK_ypaworld *yw)
{
    const auto &fnt28 = yw->_guiTiles[28]->map;
    const int iconCellWidth = fnt28[65].w;
    const int spacerWidth = fnt28[64].w;
    const int statusCellWidth = yw_GetSquadronManagerStatusCellWidth(yw);
    const int leaderCellWidth = iconCellWidth + spacerWidth;
    const int contentWidth = statusCellWidth + leaderCellWidth + iconCellWidth
                           + yw_GetSquadronManagerCountReserveWidth(yw)
                           + kSquadronManagerRowCountIconGap
                           + 2 * yw->_fontBorderW;
    const int selectionCountWidth =
        yw_GetSquadronManagerCountReserveWidth(yw)
        + kSquadronManagerTitleTextPadding;
    const int selectionSummaryWidth = iconCellWidth
                                    + kSquadronManagerTitleIconTextGap
                                    + selectionCountWidth
                                    + kSquadronManagerTitleCompactGap;
    const int titleWidth = yw->_guiTiles[6]->GetWidth(" Squad")
                         + kSquadronManagerTitleLabelSummaryGap
                         + yw->_fontDefCloseW
                         + yw_GetSquadronManagerTitleButtonWidth(yw)
                         + selectionSummaryWidth
                         + yw->_fontBorderW;
    return std::max(contentWidth, titleWidth);
}

int yw_GetSquadronManagerDefaultEntryWidth(NC_STACK_ypaworld *yw)
{
    return yw_GetSquadronManagerCompactEntryWidth(yw)
         + yw_GetSquadronManagerTitleButtonWidth(yw);
}

int yw_GetSquadronManagerExpandedEntryWidth(NC_STACK_ypaworld *yw)
{
    const int iconCellWidth = yw->_guiTiles[28]->map[65].w;
    return yw_GetSquadronManagerCompactEntryWidth(yw)
         + std::max(0, kSquadronManagerExpandedIconSlots - 1) * iconCellWidth;
}

bool yw_IsSquadronManagerCompactSize(NC_STACK_ypaworld *yw)
{
    return yw &&
           squadron_manager.entryWidth == yw_GetSquadronManagerCompactEntryWidth(yw) &&
           squadron_manager.shownEntries == kSquadronManagerDefaultShownEntries;
}

void yw_SetSquadronManagerSize(NC_STACK_ypaworld *yw, int entryWidth,
                                      int shownEntries)
{
    if ( !yw )
        return;

    squadron_manager.entryWidth = entryWidth;
    squadron_manager.shownEntries = shownEntries;
    if ( squadron_manager.firstShownEntries + squadron_manager.shownEntries
         > squadron_manager.numEntries )
    {
        squadron_manager.firstShownEntries = std::max(
            0,
            (int)squadron_manager.numEntries - (int)squadron_manager.shownEntries);
    }
    squadron_manager.SetRect(yw, -2, -2);
}

void yw_ToggleSquadronManagerCompact(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    // Compact is the single size-mode control. Its active state is derived
    // from the real window geometry: clicking an already compact window
    // restores the larger layout; any other size compacts it.
    if ( yw_IsSquadronManagerCompactSize(yw) )
    {
        yw_SetSquadronManagerSize(yw,
                                  yw_GetSquadronManagerExpandedEntryWidth(yw),
                                  kSquadronManagerExpandedShownEntries);
    }
    else
    {
        yw_SetSquadronManagerSize(yw,
                                  yw_GetSquadronManagerCompactEntryWidth(yw),
                                  kSquadronManagerDefaultShownEntries);
    }
}

void yw_ApplySquadronManagerSavedRect(NC_STACK_ypaworld *yw,
                                             const Common::PointRect &rect)
{
    if ( !yw )
        return;

    squadron_manager.x = rect.x;
    squadron_manager.y = rect.y;
    squadron_manager.entryWidth = rect.w - yw->_fontVBScrollW;

    const int fixedHeight = yw->_fontH + squadron_manager.upperVborder
                          + squadron_manager.lowerVborder;
    squadron_manager.shownEntries =
        (rect.h - fixedHeight) / std::max(1, (int)squadron_manager.entryHeight);
    squadron_manager.SetRect(yw, -2, -2);
}

int yw_GetSquadronManagerCompactButtonId()
{
    // Button 6 is neutral in GuiList input handling. Button 7 is the legacy
    // Help control and must not be reused because GuiList still gives it
    // FLAG_HELP_DOWN semantics. Row buttons start at 8.
    return 6;
}

void yw_UpdateSquadronManagerTitleButtons(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    const int compactButtonId = yw_GetSquadronManagerCompactButtonId();
    if ( squadron_manager.buttons.size() <= (size_t)compactButtonId )
        squadron_manager.buttons.resize(compactButtonId + 1);

    const ButtonBox closeBox = squadron_manager.buttons[0];
    if ( !closeBox )
    {
        squadron_manager.buttons[compactButtonId] = ButtonBox();
        return;
    }

    const int compactWidth = yw_GetSquadronManagerTitleButtonWidth(yw);
    const int compactX = std::max(0, closeBox.x - compactWidth);
    squadron_manager.buttons[compactButtonId] =
        ButtonBox(compactX, 1, compactWidth, yw->_fontH);

    // Keep dragging out of the Compact control. The title text is short, so
    // no additional clipping or layout path is required.
    squadron_manager.buttons[1] = ButtonBox(0, 0, compactX, yw->_fontH);
}

void yw_DrawSquadronManagerCompactFallback(NC_STACK_ypaworld *yw,
                                                   const ButtonBox &box,
                                                   bool active)
{
    const int halfW = yw->_screenSize.x / 2;
    const int halfH = yw->_screenSize.y / 2;
    const int left = squadron_manager.x + box.x - halfW;
    const int top = squadron_manager.y + box.y - halfH;
    const int right = left + box.w - 1;
    const int cy = top + box.h / 2;
    const int pad = std::max(3, std::min(box.w, box.h) / 5);

    SDL_Color color = yw_MapToolbarColor(yw, active);
    color.a = 255;
    GFX::Engine.raster_func217(color);
    yw_DrawMapToolbarStroke(left + pad, cy, left + pad + 5, cy, 2);
    yw_DrawMapToolbarLine(left + pad, cy, left + pad + 3, cy - 3);
    yw_DrawMapToolbarLine(left + pad, cy, left + pad + 3, cy + 3);
    yw_DrawMapToolbarStroke(right - pad, cy, right - pad - 5, cy, 2);
    yw_DrawMapToolbarLine(right - pad, cy, right - pad - 3, cy - 3);
    yw_DrawMapToolbarLine(right - pad, cy, right - pad - 3, cy + 3);
}

bool yw_IsSquadronManagerRowSelected(NC_STACK_ypaworld *yw,
                                            NC_STACK_ypabact *bact)
{
    if ( !yw || !bact )
        return false;

    if ( bact == yw->_userRobo )
        return yw->_activeCmdrID == 0 && (bzda.field_1D0 & 0x20) != 0;

    // _activeCmdrID is the stable squad identity. UI mode bits can change
    // while the strategic map is open, but that must not erase the selected
    // row colour from the Squadron Manager.
    return yw->_activeCmdrID != 0
        && bact->_commandID == yw->_activeCmdrID;
}

NC_STACK_ypabact *yw_GetSquadronManagerSelectedCommander(
    NC_STACK_ypaworld *yw)
{
    if ( !yw || yw->_activeCmdrID == 0 )
        return nullptr;

    for ( NC_STACK_ypabact *commander : yw->_cmdrsRemap )
    {
        if ( !commander || commander->_commandID != yw->_activeCmdrID )
            continue;

        return commander->IsParentMyRobo() ? commander : nullptr;
    }

    return nullptr;
}

void yw_DrawSquadronManagerSelectionSummary(
    NC_STACK_ypaworld *yw, const ButtonBox &compactBox)
{
    NC_STACK_ypabact *commander =
        yw_GetSquadronManagerSelectedCommander(yw);
    if ( !commander )
        return;

    const std::vector<NC_STACK_ypabact *> displayUnits =
        yw_GetSquadronDisplayUnits(commander);
    if ( displayUnits.empty() )
        return;

    const uint32_t icon = yw_GetSquadronManagerUnitIcon(
        yw, commander, 0);
    const Common::PointRect &source = yw->_guiTiles[28]->Glyph(icon);
    const int iconWidth = source.w;
    const int iconHeight = source.h;
    const std::string count = fmt::sprintf("%d", (int)displayUnits.size());
    const int countTextWidth = yw_GetSquadronManagerTextWidth(yw, count);
    const int countWidth = countTextWidth
                         + kSquadronManagerTitleTextPadding;
    const int countRight = squadron_manager.x + compactBox.x
                         - kSquadronManagerTitleCompactGap;
    const int countX = countRight - countWidth;
    const int iconX = countRight - countTextWidth
                    - kSquadronManagerTitleIconTextGap - iconWidth;
    const int iconY = squadron_manager.y
                    + std::max(0, (yw->_fontH - iconHeight) / 2);

    CmdStream summary;
    summary.reserve(64);
    // ProcessDrawSeq position commands read the current tileset height. Select
    // it first so this standalone stream is valid independently of prior UI.
    FontUA::reset_tileset(&summary, 28);
    FontUA::set_center_xpos(&summary, iconX - yw->_screenSize.x / 2);
    FontUA::set_center_ypos(&summary, iconY - yw->_screenSize.y / 2);
    // At 100% the standard glyph path preserves the original unit artwork.
    // The title summary intentionally omits the separate row health glyph.
    FontUA::store_glyph(&summary, icon);

    FontUA::reset_tileset(&summary, 0);
    FontUA::set_center_xpos(&summary, countX - yw->_screenSize.x / 2);
    FontUA::set_center_ypos(&summary,
        squadron_manager.y + std::max(0, (yw->_fontH
                                          - yw->_guiTiles[0]->h) / 2)
        - yw->_screenSize.y / 2);
    FontUA::set_txtColor(&summary, yw->_iniColors[60].r,
                         yw->_iniColors[60].g, yw->_iniColors[60].b);
    FontUA::copy_position(&summary);
    // Keep the safety padding inside the measured box. Right alignment makes
    // the visible digits end at countRight instead of turning that padding
    // into an additional gap before the compact button.
    FontUA::add_txt(&summary, countWidth, 2, count);
    FontUA::set_end(&summary);

    SDL_Color uiAccentColor;
    const int halfW = yw->_screenSize.x / 2;
    const int halfH = yw->_screenSize.y / 2;
    GFX::Engine.raster_func211(Common::Rect(
        squadron_manager.x - halfW,
        squadron_manager.y - halfH,
        squadron_manager.x + squadron_manager.w - halfW,
        squadron_manager.y + yw->_fontH - halfH));
    GFX::Engine.ProcessDrawSeq(summary, nullptr,
                              yw_GetFactionUiAccent(yw, &uiAccentColor));
    yw_SetFullScreenUiClip(yw);
}

void yw_PreDrawSquadronManager(NC_STACK_ypaworld *yw)
{
    if ( !yw || squadron_manager.IsClosed() )
        return;

    // The Tactical Map uses a narrow title clip for its custom toolbar. Reset
    // it here so opening the map cannot clip away the selected squad stripe.
    yw_SetFullScreenUiClip(yw);

    int selectedVisibleIndex = -1;
    for ( int i = 0; i < squadron_manager.shownEntries; i++ )
    {
        if ( yw_IsSquadronManagerRowSelected(yw, squadron_manager.squads[i]) )
        {
            selectedVisibleIndex = i;
            break;
        }
    }

    if ( selectedVisibleIndex < 0 )
        return;

    const int visualScrollbarWidth = squadron_manager.thinScrollbar
        ? std::max(1, std::min((int)squadron_manager.thinScrollbarVisualWidth,
                              (int)yw->_fontVBScrollW))
        : yw->_fontVBScrollW;
    const int halfW = yw->_screenSize.x / 2;
    const int halfH = yw->_screenSize.y / 2;
    const int left = squadron_manager.x + 1 - halfW;
    const int right = squadron_manager.x + squadron_manager.w
                    - visualScrollbarWidth - 1 - halfW;
    const int top = squadron_manager.y + yw->_fontH
                  + squadron_manager.upperVborder
                  + selectedVisibleIndex * squadron_manager.entryHeight - halfH;
    const int bottom = top + squadron_manager.entryHeight - 1;

    if ( right < left || bottom < top )
        return;

    SDL_Color selectedColor = yw_MapToolbarColor(yw, false);
    selectedColor.a = 184;
    GFX::Engine.raster_func217(selectedColor);
    for ( int y = top; y <= bottom; y++ )
        GFX::Engine.raster_func201(Common::Line(left, y, right, y));

    yw_SetFullScreenUiClip(yw);
}

void yw_PostDrawSquadronManager(NC_STACK_ypaworld *yw)
{
    if ( !yw || squadron_manager.IsClosed() )
        return;

    yw_SetFullScreenUiClip(yw);
    yw_UpdateSquadronManagerTitleButtons(yw);

    const ButtonBox &compactBox = squadron_manager.buttons[
        yw_GetSquadronManagerCompactButtonId()];
    if ( compactBox )
    {
        yw_DrawSquadronManagerSelectionSummary(yw, compactBox);

        const bool active = yw_IsSquadronManagerCompactSize(yw);
        if ( !yw_RenderSquadronManagerIcon(yw, squadron_manager.x,
                                           squadron_manager.y, "compact",
                                           active, compactBox) )
        {
            yw_DrawSquadronManagerCompactFallback(yw, compactBox, active);
        }
    }

    yw_SetFullScreenUiClip(yw);
}

NC_STACK_ypabact * NC_STACK_ypaworld::GetDefaultCmdr() const
{
    if ( _activeCmdrRemapIndex != -1 )
        return _cmdrsRemap.at( _activeCmdrRemapIndex );

    if ( !_cmdrsRemap.empty() )
        return _cmdrsRemap.front();

    return _userRobo;
}

NC_STACK_ypabact * NC_STACK_ypaworld::GetNextCmdrByCmdId(uint32_t cmdrId) const
{
    for (auto it = _cmdrsRemap.begin(); it != _cmdrsRemap.end(); ++it)
    {
        if ( cmdrId == (*it)->_commandID )
        {
            ++it;
            if (it != _cmdrsRemap.end())
                return *it;

            return _cmdrsRemap.front();
        }
    }

    return GetDefaultCmdr();
}

NC_STACK_ypabact * NC_STACK_ypaworld::GetNextSquad() const
{
    if ( _userUnit == _userRobo || _playerInHSGun )
        return GetDefaultCmdr();

    if ( _userUnit->_status == BACT_STATUS_DEAD )
    {
        for (NC_STACK_ypabact *comm : _cmdrsRemap)
        {
            if ( _vehicleTakenCommandId == comm->_commandID )
                return comm;
        }

        return GetDefaultCmdr();
    }

    if ( _userUnit->_bact_type == BACT_TYPES_GUN )
        return GetDefaultCmdr();

    return GetNextCmdrByCmdId(_vehicleTakenCommandId);
}

bool yw_IsSquadronDisplayMember(NC_STACK_ypabact *bact)
{
    return bact && bact->_status != BACT_STATUS_DEAD
           && bact->_status != BACT_STATUS_CREATE && bact->_status != BACT_STATUS_BEAM
           && !bact->ShouldHideFromStrategicUI();
}

SquadronDisplaySummary yw_GetSquadronDisplaySummary(NC_STACK_ypabact *leader)
{
    SquadronDisplaySummary summary;

    if ( !leader )
        return summary;

    summary.visibleMemberCount = 1;

    for ( NC_STACK_ypabact *kid : leader->_kidList )
    {
        if ( !yw_IsSquadronDisplayMember(kid) )
            continue;

        summary.visibleMemberCount++;
        if ( !summary.highestHpMember || kid->_energy > summary.highestHpMember->_energy )
            summary.highestHpMember = kid;
    }

    return summary;
}

std::vector<NC_STACK_ypabact *> yw_GetSquadronDisplayUnits(
    NC_STACK_ypabact *leader)
{
    std::vector<NC_STACK_ypabact *> units;
    if ( !leader )
        return units;

    units.push_back(leader);
    const SquadronDisplaySummary summary = yw_GetSquadronDisplaySummary(leader);

    if ( summary.highestHpMember )
        units.push_back(summary.highestHpMember);

    for ( NC_STACK_ypabact *kid : leader->_kidList )
    {
        if ( !yw_IsSquadronDisplayMember(kid)
                || kid == summary.highestHpMember )
        {
            continue;
        }

        units.push_back(kid);
    }

    return units;
}

char yw_GetSquadronManagerStatusIcon(NC_STACK_ypabact *bact)
{
    if ( bact->_status == BACT_STATUS_NORMAL )
    {
        if ( bact->_status_flg & BACT_STFLAG_ESCAPE )
            return 102;
        if ( bact->_secndTtype )
            return 99;
        if ( bact->_primTtype )
            return 98;
    }
    else if ( bact->_status == BACT_STATUS_IDLE
              && bact->_status_flg & BACT_STFLAG_ESCAPE )
    {
        return 102;
    }

    return 97;
}

uint32_t yw_GetSquadronManagerUnitIcon(NC_STACK_ypaworld *yw,
                                          NC_STACK_ypabact *bact, int blink)
{
    if ( bact == yw->_userUnit && blink )
        return 33;
    if ( yw->_lastMsgSender == bact && blink )
        return 34;

    return sub_4C7134(yw, bact);
}

uint8_t yw_GetSquadronManagerHealthGlyph(NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_energy_max <= 0 )
        return 128;

    const float health = std::max(0.0f, std::min(1.0f,
        (float)bact->_energy / (float)bact->_energy_max));

    if ( health > 0.75f )
        return 131;
    if ( health > 0.50f )
        return 130;
    if ( health > 0.25f )
        return 129;

    return 128;
}

bool yw_IsSquadronManagerRowHovered(NC_STACK_ypabact *bact)
{
    if ( !bact || squadron_manager.mouseItem < squadron_manager.firstShownEntries )
        return false;

    const int visibleIndex = squadron_manager.mouseItem
                           - squadron_manager.firstShownEntries;
    return visibleIndex >= 0
        && visibleIndex < squadron_manager.shownEntries
        && squadron_manager.squads[visibleIndex] == bact;
}

int yw_GetSquadronManagerScrollbarGapFillWidth(NC_STACK_ypaworld *yw)
{
    return yw_GetGuiListScrollbarGapFillWidth(&squadron_manager, yw);
}

int yw_GetSquadronManagerBackgroundFillWidth(NC_STACK_ypaworld *yw)
{
    return squadron_manager.entryWidth - yw->_fontBorderW
         + yw_GetSquadronManagerScrollbarGapFillWidth(yw);
}

int yw_GetSquadronManagerCountReserveWidth(NC_STACK_ypaworld *yw)
{
    size_t digitCount = 1;
    for ( NC_STACK_ypabact *commander : yw->_cmdrsRemap )
    {
        if ( !commander || !commander->IsParentMyRobo() )
            continue;

        digitCount = std::max(
            digitCount,
            std::to_string(yw_GetSquadronDisplayUnits(commander).size()).size());
    }

    const std::string countReserve(digitCount, '9');
    return yw_GetSquadronManagerTextWidth(yw, countReserve);
}

void yw_UpdateSquadronManagerColumnLayout(NC_STACK_ypaworld *yw)
{
    const auto &fnt28 = yw->_guiTiles[28]->map;
    const int iconCellWidth = fnt28[65].w;
    const int spacerWidth = fnt28[64].w;
    const int statusCellWidth = yw_GetSquadronManagerStatusCellWidth(yw);
    const int leaderCellWidth = iconCellWidth + spacerWidth;

    squadron_manager.field_2CC = iconCellWidth;
    squadron_manager.field_2D0 = yw->_fontBorderW + statusCellWidth
                               + yw_GetSquadronManagerCountReserveWidth(yw)
                               + kSquadronManagerRowCountIconGap;
    squadron_manager.field_2D4 = squadron_manager.field_2D0 + leaderCellWidth;
}

int yw_GetSquadronManagerUnitSlotCount(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return 2;

    const int iconCellWidth = squadron_manager.field_2CC;
    const int spacerWidth = yw->_guiTiles[28]->map[64].w;
    const int statusCellWidth = yw_GetSquadronManagerStatusCellWidth(yw);
    const int leaderCellWidth = iconCellWidth + spacerWidth;
    int available = squadron_manager.entryWidth - 2 * yw->_fontBorderW
                  - statusCellWidth
                  - yw_GetSquadronManagerCountReserveWidth(yw)
                  - kSquadronManagerRowCountIconGap;

    if ( available <= leaderCellWidth )
        return 1;

    available -= leaderCellWidth;
    return 1 + std::max(1, available / std::max(1, iconCellWidth));
}

void yw_UpdateSquadronManagerDynamicLayout(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    const int oldCompactWidth = squadron_manager.minEntryWidth;
    const int newCompactWidth = yw_GetSquadronManagerCompactEntryWidth(yw);
    if ( oldCompactWidth != newCompactWidth )
    {
        const int titleButtonWidth = yw_GetSquadronManagerTitleButtonWidth(yw);
        const int expandedExtra = std::max(
            0, kSquadronManagerExpandedIconSlots - 1)
            * yw->_guiTiles[28]->map[65].w;
        const bool followsCompact =
            squadron_manager.entryWidth == oldCompactWidth;
        const bool followsDefault =
            squadron_manager.entryWidth == oldCompactWidth + titleButtonWidth;
        const bool followsExpanded =
            squadron_manager.entryWidth == oldCompactWidth + expandedExtra;

        squadron_manager.minEntryWidth = newCompactWidth;
        if ( followsCompact )
            squadron_manager.entryWidth = newCompactWidth;
        else if ( followsDefault )
            squadron_manager.entryWidth = newCompactWidth + titleButtonWidth;
        else if ( followsExpanded )
            squadron_manager.entryWidth = newCompactWidth + expandedExtra;
        else if ( squadron_manager.entryWidth < newCompactWidth )
            squadron_manager.entryWidth = newCompactWidth;

        squadron_manager.SetRect(yw, -2, -2);
    }

    yw_UpdateSquadronManagerColumnLayout(yw);

    int aggressionWidth = 0;
    for ( int i = 0; i < 5; i++ )
        aggressionWidth += yw->_guiTiles[24]->map[49 + i].w;

    const int footerContentWidth = squadron_manager.entryWidth
                                 - 2 * yw->_fontBorderW;
    squadron_manager.field_2DC = yw->_fontBorderW
                               + std::max(0, (footerContentWidth
                                            - aggressionWidth) / 2);

    yw_UpdateSquadronManagerTitleButtons(yw);
}

NC_STACK_ypabact *yw_GetSquadronAggressionCommander(
    NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return nullptr;

    if ( yw->_activeCmdrRemapIndex >= 0
            && (size_t)yw->_activeCmdrRemapIndex < yw->_cmdrsRemap.size() )
    {
        NC_STACK_ypabact *commander =
            yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];
        if ( commander )
        {
            squadron_manager.aggressionCommandID = commander->_commandID;
            return commander;
        }
    }

    if ( squadron_manager.aggressionCommandID != 0 )
    {
        for ( NC_STACK_ypabact *commander : yw->_cmdrsRemap )
        {
            if ( commander
                    && commander->_commandID
                       == squadron_manager.aggressionCommandID )
            {
                return commander;
            }
        }
    }

    return nullptr;
}

int yw_GetSquadronAggressionIndexAtX(NC_STACK_ypaworld *yw, int x)
{
    if ( !yw || x < squadron_manager.field_2DC )
        return -1;

    int glyphX = squadron_manager.field_2DC;
    for ( int i = 0; i < 5; i++ )
    {
        glyphX += yw->_guiTiles[24]->map[49 + i].w;
        if ( x < glyphX )
            return i;
    }

    return -1;
}

void yw_RenderSquadronAggressionStrip(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    NC_STACK_ypabact *commander = yw_GetSquadronAggressionCommander(yw);
    const int aggression = commander ? commander->_aggr : -1;

    if ( !commander )
        FontUA::select_tileset(cur, 24);
    else
        FontUA::select_tileset(cur, 25);

    FontUA::store_u8(cur, 49);

    if ( !commander || aggression < 25 )
        FontUA::select_tileset(cur, 24);
    FontUA::store_u8(cur, 50);

    if ( aggression >= 25 && aggression < 50 )
        FontUA::select_tileset(cur, 24);
    FontUA::store_u8(cur, 51);

    if ( aggression >= 50 && aggression < 75 )
        FontUA::select_tileset(cur, 24);
    FontUA::store_u8(cur, 52);

    if ( aggression >= 75 && aggression < 100 )
        FontUA::select_tileset(cur, 24);
    FontUA::store_u8(cur, 53);
}

void yw_RenderSquadronManagerSquadContent(NC_STACK_ypaworld *yw,
                                                  NC_STACK_ypabact *bact,
                                                  CmdStream *cur,
                                                  uint8_t opacity)
{
    const std::vector<NC_STACK_ypabact *> displayUnits =
        yw_GetSquadronDisplayUnits(bact);
    const int blink = (yw->_timeStamp / 300) & 1;
    const int iconCellWidth = squadron_manager.field_2CC;
    const int spacerWidth = yw->_guiTiles[28]->map[64].w;
    const int statusCellWidth = yw_GetSquadronManagerStatusCellWidth(yw);
    const int leaderCellWidth = iconCellWidth + spacerWidth;
    const int unitSlotCount = yw_GetSquadronManagerUnitSlotCount(yw);
    const int countWidth = yw_GetSquadronManagerCountReserveWidth(yw);

    if ( opacity != 255 )
        FontUA::set_opacity(cur, opacity);

    FontUA::select_tileset(cur, 28);
    const uint8_t statusIcon = yw_GetSquadronManagerStatusIcon(bact);
    FontUA::store_u8(cur, statusIcon);
    FontUA::add_xpos(cur,
                    statusCellWidth
                    - yw->_guiTiles[28]->map[statusIcon].w);

    if ( !displayUnits.empty() )
    {
        const std::string count = fmt::sprintf(
            "%d", (int)displayUnits.size());
        FontUA::select_tileset(cur, 0);
        if ( displayUnits.size() < 10 )
        {
            FontUA::add_xpos(cur,
                            kSquadronManagerSingleDigitCenterNudge);
            FontUA::copy_position(cur);
            FontUA::add_xpos(cur,
                            -kSquadronManagerSingleDigitCenterNudge);
        }
        else
        {
            FontUA::copy_position(cur);
        }
        FontUA::add_txt(cur, countWidth, 4, count);
        FontUA::add_xpos(cur,
                        countWidth + kSquadronManagerRowCountIconGap);
        FontUA::select_tileset(cur, 28);
    }

    for ( int slot = 0; slot < unitSlotCount; slot++ )
    {
        const int cellWidth = slot == 0 ? leaderCellWidth : iconCellWidth;
        NC_STACK_ypabact *unit = slot < (int)displayUnits.size()
            ? displayUnits[slot]
            : nullptr;

        FontUA::glyph_cell(yw->_guiTiles[28], cur,
            unit ? yw_GetSquadronManagerUnitIcon(yw, unit, blink) : 0, cellWidth);

        if ( unit )
        {
            const uint8_t healthGlyph = yw_GetSquadronManagerHealthGlyph(unit);
            const int healthWidth = yw->_guiTiles[28]->map[healthGlyph].w;
            FontUA::add_xpos(cur, -cellWidth);
            FontUA::store_u8(cur, healthGlyph);
            FontUA::add_xpos(cur, cellWidth - healthWidth);
        }
    }

    if ( opacity != 255 )
        FontUA::set_opacity(cur, 255);
}

void yw_RenderSquadronManagerWholeSquadDragGhost(
    NC_STACK_ypaworld *yw, CmdStream *cur, int firstIconX, int rowY)
{
    NC_STACK_ypabact *leader = squadron_manager.field_2BC;
    if ( !yw || !cur || !leader || !leader->IsParentMyRobo() )
        return;

    const std::vector<NC_STACK_ypabact *> displayUnits =
        yw_GetSquadronDisplayUnits(leader);
    if ( displayUnits.empty() )
        return;

    const int iconCellWidth = std::max(1, squadron_manager.field_2CC);
    const int totalWidth = (int)displayUnits.size() * iconCellWidth;
    if ( firstIconX + totalWidth <= 0
            || firstIconX >= yw->_screenSize.x
            || rowY + yw->_fontH <= 0
            || rowY >= yw->_screenSize.y )
    {
        return;
    }

    // Whole-squad drag intentionally shows only the unit glyphs.  Keep their
    // relative row position under the mouse, but do not drag the row
    // background, status glyph, health bars or the xN counter.
    FontUA::set_center_xpos(cur, firstIconX - (yw->_screenSize.x / 2));
    FontUA::set_center_ypos(cur, rowY - (yw->_screenSize.y / 2));
    FontUA::select_tileset(cur, 28);
    FontUA::set_opacity(cur, kSquadronManagerDragOpacity);

    const int blink = (yw->_timeStamp / 300) & 1;
    for ( NC_STACK_ypabact *unit : displayUnits )
    {
        if ( !unit )
            continue;

        FontUA::glyph_cell(yw->_guiTiles[28], cur,
            yw_GetSquadronManagerUnitIcon(yw, unit, blink), iconCellWidth);
    }

    FontUA::set_opacity(cur, 255);
}

bool yw_IsSquadronManagerWholeSquadDragArea(
    NC_STACK_ypaworld *yw, int squadIndex, int mouseX)
{
    if ( !yw || squadIndex < 0
            || squadIndex >= squadron_manager.shownEntries )
        return false;

    NC_STACK_ypabact *leader = squadron_manager.squads[squadIndex];
    if ( !leader || leader == yw->_userRobo || !leader->IsParentMyRobo() )
        return false;

    const int unitSlotCount = yw_GetSquadronManagerUnitSlotCount(yw);
    const int unitContentEnd = squadron_manager.field_2D4
                             + std::max(0, unitSlotCount - 1)
                               * squadron_manager.field_2CC;
    const int contentEnd = squadron_manager.entryWidth - yw->_fontBorderW;

    return mouseX >= unitContentEnd && mouseX < contentEnd;
}

void NC_STACK_ypaworld::SquadManager_InputHandle(TInputState *inpt)
{
    if ( squadron_manager.flags & (GuiBase::FLAG_CLOSED | GuiBase::FLAG_ICONIFED) )
    {
        squadron_manager.field_2A8 &= 0xFFFFFFFE;
        squadron_manager.draggingWholeSquad = false;
        squadron_manager.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;
        bool onAggressionControl = false;
        bool onSizeControl = false;
        sub_4C707C(this);
        yw_UpdateSquadronManagerDynamicLayout(this);

        const int compactButtonId = yw_GetSquadronManagerCompactButtonId();

        // Squadron Manager has no double-click titlebar action. Consume the
        // second click explicitly so no generic drag/window path can reinterpret
        // it as a size action later in the frame. Tactical Map double-click is
        // handled independently and remains unchanged.
        if ( winpt->selected_btn == &squadron_manager
                && winpt->selected_btnID == 1
                && (winpt->flag & TClickBoxInf::FLAG_DBL_CLICK) )
        {
            winpt->flag &= ~(TClickBoxInf::FLAG_DBL_CLICK
                           | TClickBoxInf::FLAG_LM_DOWN
                           | TClickBoxInf::FLAG_LM_HOLD
                           | TClickBoxInf::FLAG_BTN_DOWN
                           | TClickBoxInf::FLAG_BTN_HOLD);
        }

        if ( squadron_manager.field_2A8 & 1 )
        {
            if ( ypaworld_func64__sub7__sub3__sub2() && (winpt->flag & (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD)) != (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD) )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_LM_HOLD | TClickBoxInf::FLAG_RM_HOLD) )
                {
                    squadron_manager.field_2C4 = winpt->move.ScreenPos.x + squadron_manager.field_2C0;
                    squadron_manager.field_2C6 = winpt->move.ScreenPos.y + squadron_manager.field_2C2;
                }
                else
                {
                    ypaworld_func64__sub7__sub3__sub3(this, winpt);
                    squadron_manager.field_2A8 &= 0xFFFFFFFC;
                    squadron_manager.draggingWholeSquad = false;
                }
            }
            else
            {
                squadron_manager.field_2A8 &= 0xFFFFFFFC;
                squadron_manager.draggingWholeSquad = false;
            }
        }
        else if ( inpt->ClickInf.selected_btn == &squadron_manager )
        {
            if ( inpt->ClickInf.selected_btnID == compactButtonId )
            {
                onSizeControl = true;
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN
                                   | TClickBoxInf::FLAG_LM_DOWN) )
                {
                    yw_ToggleSquadronManagerCompact(this);

                    if ( _GameShell )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            NC_STACK_ypabact *aggressionCommander =
                yw_GetSquadronAggressionCommander(this);
            if ( aggressionCommander )
            {
                const int footerTop = squadron_manager.h
                                    - squadron_manager.lowerVborder;
                const int localX = winpt->move.ScreenPos.x - squadron_manager.x;
                const int localY = winpt->move.ScreenPos.y - squadron_manager.y;
                const bool insideFooterRow = localY >= footerTop
                                          && localY
                                             < footerTop + squadron_manager.entryHeight;
                const int v9 = insideFooterRow
                    ? yw_GetSquadronAggressionIndexAtX(this, localX)
                    : -1;

                if ( v9 >= 0 && v9 <= 4 )
                {
                    onAggressionControl = true;
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN
                                      | TClickBoxInf::FLAG_LM_DOWN) )
                    {
                        aggressionCommander->setBACT_aggression(25 * v9);

                        if ( _GameShell )
                            SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                    }
                }

                switch ( v9 )
                {
                case 0:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_0, 38);
                    break;

                case 1:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_1, 39);
                    break;

                case 2:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_2, 40);
                    break;

                case 3:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_3, 41);
                    break;

                case 4:
                    SetShowingTooltipWithHotkey(Locale::TIP_GUI_AGGR_4, 42);
                    break;

                default:
                    break;
                }
            }

            if ( !onAggressionControl && !onSizeControl
                    && (winpt->flag & (TClickBoxInf::FLAG_RM_DOWN
                                      | TClickBoxInf::FLAG_LM_DOWN)) )
            {
                if ( ypaworld_func64__sub7__sub3__sub1(winpt) )
                {
                    squadron_manager.field_2A8 &= 0xFFFFFFFC;
                    squadron_manager.field_2A8 |= 1;

                    if ( squadron_manager.draggingWholeSquad
                            || (winpt->flag & TClickBoxInf::FLAG_RM_DOWN) )
                        squadron_manager.field_2A8 |= 3;

                    squadron_manager.field_2C4 = winpt->move.ScreenPos.x + squadron_manager.field_2C0;
                    squadron_manager.field_2C6 = winpt->move.ScreenPos.y + squadron_manager.field_2C2;
                }
            }
            if ( squadron_manager.mouseItem != -1 )
            {
                int v17 = squadron_manager.mouseItem - squadron_manager.firstShownEntries;

                if ( v17 >= 0 && v17 < squadron_manager.maxShownEntries )
                {
                    NC_STACK_ypabact *v18 = squadron_manager.squads[v17];
                    if ( v18 )
                    {
                        if ( winpt->move.BtnPos.x < squadron_manager.field_2D0 )
                            ypaworld_func64__sub7__sub3__sub4(v18);
                    }
                }
            }
        }

        // The footer and Compact control own their clicks; do not let GuiList start
        // row selection, title dragging or help-button handling from the same
        // mouse event.
        const int savedSelectedButtonId = winpt->selected_btnID;
        if ( onAggressionControl || onSizeControl )
        {
            squadron_manager.listFlags &= ~(GuiList::GLIST_FLAG_IN_SELECT
                                           | GuiList::GLIST_FLAG_SEL_DONE);
            winpt->selected_btnID = -1;
        }

        squadron_manager.InputHandle(this, inpt);

        if ( onAggressionControl || onSizeControl )
            winpt->selected_btnID = savedSelectedButtonId;

        squadron_manager.Formate(this);
        yw_UpdateSquadronManagerDynamicLayout(this);
        ypaworld_func64__sub7__sub3__sub0(this, inpt);
    }
}

bool yw_IsActiveSquadronSelectionUnit(NC_STACK_ypaworld *yw,
                                             NC_STACK_ypabact *bact)
{
    if ( !yw || !bact || yw->_activeCmdrRemapIndex < 0 ||
         (size_t)yw->_activeCmdrRemapIndex >= yw->_cmdrsRemap.size() )
    {
        return false;
    }

    NC_STACK_ypabact *activeCommander =
        yw->_cmdrsRemap[yw->_activeCmdrRemapIndex];
    return activeCommander &&
           (bact == activeCommander || bact->_parent == activeCommander);
}

void NC_STACK_ypaworld::yw_SMAN_MouseSelect(TClickBoxInf *winp)
{
    if ( _guiActFlags & 2 )
    {
        int v3 = winp->selected_btnID - 8;

        NC_STACK_ypabact *v4 = sub_4C7B0C(v3, winp->move.BtnPos.x);

        if ( v4 )
        {
            if ( v4->_status != BACT_STATUS_CREATE && v4->_status != BACT_STATUS_DEAD && v4->_status != BACT_STATUS_BEAM )
            {
                _bactOnMouse = v4;
                _guiActFlags |= 0x20;
            }
        }
        else
        {
            v4 = squadron_manager.squads[v3];

            if ( v4 )
            {
                if ( v4->_status == BACT_STATUS_CREATE || v4->_status == BACT_STATUS_BEAM || v4->_status == BACT_STATUS_DEAD )
                    v4 = nullptr;

                if ( v4 )
                {
                    _bactOnMouse = v4;
                    _guiActFlags |= 0x20;
                }
            }
        }
    }
}
