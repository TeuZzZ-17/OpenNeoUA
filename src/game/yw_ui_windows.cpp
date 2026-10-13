// Priority gameplay windows, GUI list helpers and GUI window management.
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

void yw_SetFullScreenUiClip(NC_STACK_ypaworld *yw)
{
    if ( !yw )
        return;

    GFX::Engine.raster_func211(Common::Rect(-yw->_screenSize.x / 2,
                                            -yw->_screenSize.y / 2,
                                             yw->_screenSize.x / 2,
                                             yw->_screenSize.y / 2));
}

void yw_CloseGameplayWindowsForGemNotification(NC_STACK_ypaworld *yw)
{
    if ( !yw || !yw->_guiLoaded )
        return;

    // Close ordinary gameplay windows without destroying or unregistering the
    // GUI. Map and Squadron Manager now have higher visual priority than the GEM
    // notification, while the Exit Menu remains the absolute top-level window.
    std::vector<GuiBase *> activeWindows(yw->_guiActive.begin(),
                                         yw->_guiActive.end());
    for ( GuiBase *window : activeWindows )
    {
        if ( window &&
             window != &robo_map &&
             window != &squadron_manager &&
             window != &exit_menu )
        {
            yw->GuiWinClose(window);
        }
    }

    // The nested vehicle/building list is not guaranteed to be registered in
    // _guiActive, so close it explicitly through the same canonical window path.
    yw->GuiWinClose(&gui_lstvw);

    yw_ResetWorldSelectionDrag(yw);
    yw->_moveOrderFeedbackActive = false;
    yw->_attackOrderFeedbacks.clear();
}

bool yw_IsTopLevelExitMenuFlowWindow(const GuiBase *window)
{
    if ( !window )
        return false;

    if ( window == &exit_menu )
        return true;

    // All five Exit Menu actions (Exit/Save/Load/Restart/Quit) reuse lstvw2
    // as their confirmation dialog. Promote it only while it belongs to the
    // Exit Menu flow, so unrelated confirmations keep their normal GUI order.
    return window == &lstvw2 &&
           dword_5BAFAC == &exit_menu &&
           dword_5BAF9C == 3;
}

bool yw_IsVisiblePriorityGameplayWindow(const GuiBase *window)
{
    if ( !window || !window->IsOpen() || (window->flags & GuiBase::FLAG_ICONIFED) )
        return false;

    if ( window == &gui_lstvw )
        return bzda.IsOpen();

    return window == &robo_map || window == &squadron_manager;
}

bool yw_IsPriorityGameplayUiOpen()
{
    return yw_IsVisiblePriorityGameplayWindow(&robo_map) ||
           yw_IsVisiblePriorityGameplayWindow(&squadron_manager);
}

bool yw_IsPriorityGameplayUiOcclusionOpen()
{
    return yw_IsPriorityGameplayUiOpen() ||
           yw_IsVisiblePriorityGameplayWindow(&gui_lstvw);
}

void yw_RenderGameplayGuiWindow(NC_STACK_ypaworld *yw, GuiBase *window,
                                       const SDL_Color *uiAccent)
{
    if ( !yw || !window || (window->flags & 0x20) )
        return;

    if ( window->flags & GuiBase::FLAG_ICONIFED )
    {
        GFX::Engine.ProcessDrawSeq(window->iconString, nullptr, uiAccent);
        return;
    }

    if ( window->preDraw )
        window->preDraw(yw);

    GFX::Engine.ProcessDrawSeq(window->cmdCommands, &window->cmdInclude, uiAccent);

    if ( window->postDraw )
        window->postDraw(yw);
}

void yw_ClearVirtualUiWindowRect(NC_STACK_ypaworld *yw, const GuiBase *window)
{
    if ( !yw || !window || !GFX::Engine.IsVirtualUIPass() )
        return;

    SDL_Surface *surface = GFX::Engine.Screen();
    if ( !surface )
        return;

    const int left = std::max(0, static_cast<int>(window->x));
    const int top = std::max(0, static_cast<int>(window->y));
    const int right = std::min(yw->_screenSize.x, window->x + window->w);
    const int bottom = std::min(yw->_screenSize.y, window->y + window->h);
    if ( right <= left || bottom <= top )
        return;

    SDL_Rect oldClip;
    SDL_GetClipRect(surface, &oldClip);
    SDL_SetClipRect(surface, nullptr);

    SDL_Rect clearRect = {left, top, right - left, bottom - top};
    SDL_FillRect(surface, &clearRect,
                 SDL_MapRGBA(surface->format, 0, 0, 0, 0));

    SDL_SetClipRect(surface, &oldClip);
}

void yw_ClearPriorityGameplayUiUnderlay(NC_STACK_ypaworld *yw)
{
    if ( yw_IsVisiblePriorityGameplayWindow(&robo_map) )
        yw_ClearVirtualUiWindowRect(yw, &robo_map);
    if ( yw_IsVisiblePriorityGameplayWindow(&squadron_manager) )
        yw_ClearVirtualUiWindowRect(yw, &squadron_manager);
    if ( yw_IsVisiblePriorityGameplayWindow(&gui_lstvw) )
        yw_ClearVirtualUiWindowRect(yw, &gui_lstvw);
}

void yw_RenderPriorityGameplayWindows(NC_STACK_ypaworld *yw,
                                             const SDL_Color *uiAccent)
{
    for ( GuiBase *window : yw->_guiActive )
    {
        if ( yw_IsVisiblePriorityGameplayWindow(window) )
            yw_RenderGameplayGuiWindow(yw, window, uiAccent);
    }

    // The expandable Genesis/Creation list is intentionally managed outside
    // _guiActive. Keep its historical foreground position relative to Map and
    // Squadron Manager, but render it in the same occluding pass so HUD/world
    // UI beneath its translucent panel cannot bleed through.
    if ( yw_IsVisiblePriorityGameplayWindow(&gui_lstvw) )
        GFX::Engine.ProcessDrawSeq(gui_lstvw.cmdCommands, &gui_lstvw.cmdInclude,
                                   uiAccent);
}

void yw_ResetVirtualUiClipToFullScreen(NC_STACK_ypaworld *yw)
{
    if ( !yw || !GFX::Engine.IsVirtualUIPass() )
        return;

    const int halfW = yw->_screenSize.x / 2;
    const int halfH = yw->_screenSize.y / 2;
    GFX::Engine.raster_func211(Common::Rect(-halfW, -halfH,
                                            yw->_screenSize.x - halfW,
                                            yw->_screenSize.y - halfH));
}

void yw_RenderTopLevelExitMenuFlow(NC_STACK_ypaworld *yw,
                                          const SDL_Color *uiAccent)
{
    if ( !yw )
        return;

    const std::array<GuiBase *, 2> exitFlowWindows = {&exit_menu, &lstvw2};
    bool hasVisibleWindow = false;

    // Exit Menu and every confirmation spawned from it form one top-level
    // flow. Clear all covered UI pixels first, then render parent -> child so
    // a confirmation remains above the menu if both ever coexist.
    for ( GuiBase *window : exitFlowWindows )
    {
        if ( !yw_IsTopLevelExitMenuFlowWindow(window) ||
             !window->IsOpen() ||
             (window->flags & GuiBase::FLAG_ICONIFED) )
        {
            continue;
        }

        yw_ClearVirtualUiWindowRect(yw, window);
        hasVisibleWindow = true;
    }

    if ( !hasVisibleWindow )
        return;

    yw_ResetVirtualUiClipToFullScreen(yw);

    for ( GuiBase *window : exitFlowWindows )
    {
        if ( yw_IsTopLevelExitMenuFlowWindow(window) &&
             window->IsOpen() &&
             !(window->flags & GuiBase::FLAG_ICONIFED) )
        {
            yw_RenderGameplayGuiWindow(yw, window, uiAccent);
        }
    }
}

void yw_RenderAlwaysVisibleGameplayBars(NC_STACK_ypaworld *yw,
                                                const SDL_Color *uiAccent)
{
    // The two fixed gameplay bars are deliberately above Map/Squadron Manager.
    // They remain visible and usable regardless of strategic-window overlap.
    GFX::Engine.ProcessDrawSeq(up_panel.cmdCommands, &up_panel.cmdInclude, uiAccent);

    // up_panel may leave its own clip active too. Plasma is a direct bitmap, so
    // reset once more to guarantee that opening Map/Squadron never hides it.
    yw_ResetVirtualUiClipToFullScreen(yw);
    yw_RenderPlasmaCurrencyHudIcon(yw);

    if ( bzda.IsOpen() )
        GFX::Engine.ProcessDrawSeq(bzda.cmdCommands, &bzda.cmdInclude, uiAccent);
}

void yw_RenderForegroundGameplayGuiWindows(NC_STACK_ypaworld *yw,
                                                  const SDL_Color *uiAccent)
{
    // Real GUI windows/dialogs stay above strategic windows. Map/Squadron only
    // occlude gameplay overlays, never another menu/dialog.
    for ( GuiBase *window : yw->_guiActive )
    {
        if ( yw_IsVisiblePriorityGameplayWindow(window) ||
             yw_IsTopLevelExitMenuFlowWindow(window) )
            continue;

        yw_RenderGameplayGuiWindow(yw, window, uiAccent);
    }

}

void yw_FinalizePriorityGameplayUi(NC_STACK_ypaworld *yw)
{
    if ( !yw || yw->_hideHudForScreenshots || !yw->_userUnit ||
         yw->_userUnit->_bact_type == BACT_TYPES_MISSLE ||
         !yw_IsPriorityGameplayUiOcclusionOpen() )
    {
        return;
    }

    SDL_Color uiAccentColor;
    const SDL_Color *uiAccent = yw_GetFactionUiAccent(yw, &uiAccentColor);

    // Ordinary gameplay UI has already been rendered. Remove only the pixels
    // geometrically covered by Map/Squadron Manager or the Genesis list,
    // leaving every overlay outside those rectangles untouched and the 3D world
    // visible through the windows' translucent backgrounds.
    yw_ClearPriorityGameplayUiUnderlay(yw);

    // Preserve the normal relative order when priority gameplay windows overlap.
    yw_RenderPriorityGameplayWindows(yw, uiAccent);

    // Priority-window draw sequences can leave their own clip active. Restore the
    // full virtual-UI clip before drawing fixed bars/direct bitmaps; otherwise
    // the Plasma icon can disappear even when it is nowhere near the window.
    yw_ResetVirtualUiClipToFullScreen(yw);

    // Explicit exception requested by the UI hierarchy: the fixed top/bottom
    // bars are always visible, even where a priority window reaches them.
    yw_RenderAlwaysVisibleGameplayBars(yw, uiAccent);

    // Menus/dialogs are foreground GUI, not gameplay underlay. Render them once
    // above priority windows so the underlay clear cannot erase them.
    yw_ResetVirtualUiClipToFullScreen(yw);
    yw_RenderForegroundGameplayGuiWindows(yw, uiAccent);

    // Exit Menu and its confirmation dialogs remain the absolute top-level
    // gameplay flow.
    yw_RenderTopLevelExitMenuFlow(yw, uiAccent);
}

void yw_SetGuiListBackgroundOpacity(CmdStream *cur, uint8_t opacity)
{
    if ( opacity != 255 )
        FontUA::set_opacity(cur, opacity);
}

int yw_GetGuiListScrollbarGapFillWidth(const GuiList *lstvw,
                                               NC_STACK_ypaworld *yw)
{
    if ( lstvw && yw && lstvw->fillThinScrollbarGap && lstvw->thinScrollbar )
    {
        const int visualWidth = std::max(1,
            std::min((int)lstvw->thinScrollbarVisualWidth,
                     (int)yw->_fontVBScrollW));
        return yw->_fontVBScrollW - visualWidth;
    }

    return 0;
}

void yw_StoreGuiListRowBackground(const GuiList *lstvw, CmdStream *cur,
                                         int fillTileset, int fillWidth,
                                         int opacityOverride)
{
    const uint8_t opacity = opacityOverride >= 0
        ? (uint8_t)std::min(opacityOverride, 255)
        : (lstvw ? lstvw->backgroundOpacity : 255);
    yw_SetGuiListBackgroundOpacity(cur, opacity);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 123);

    FontUA::select_tileset(cur, fillTileset);
    FontUA::op17(cur, fillWidth);
    FontUA::store_u8(cur, 32);

    FontUA::select_tileset(cur, 0);
    FontUA::store_u8(cur, 125);

    if ( opacity != 255 )
        FontUA::set_opacity(cur, 255);
}

int yw_GetGenesisScreenTextWidth(NC_STACK_ypaworld *yw, const std::string &text)
{
    if ( !yw )
        return 0;

    // Genesis names are ultimately rendered by the screen-text/TTF path (op18).
    // Measuring them with the legacy tile glyph widths can substantially
    // overestimate the real rendered width when ui.menu_font is active, which
    // caused premature "..." even with hundreds of free pixels in the row.
    const int screenWidth = GFX::Engine.MeasureScreenTextWidth(text);
    if ( screenWidth > 0 )
        return screenWidth;

    int fallbackWidth = 0;
    if ( yw->_guiTiles[0] )
        fallbackWidth = yw->_guiTiles[0]->GetWidth(text);
    if ( yw->_guiTiles[9] )
        fallbackWidth = std::max(fallbackWidth, yw->_guiTiles[9]->GetWidth(text));

    return fallbackWidth;
}

std::string yw_FormatGenesisVehicleName(NC_STACK_ypaworld *yw,
                                                int normalTextWidth,
                                                int selectedTextWidth,
                                                const std::string &name,
                                                int remaining)
{
    if ( !yw || remaining < 0 )
        return name;

    const int availableWidth = std::max(0, std::min(normalTextWidth, selectedTextWidth));
    const std::string count = fmt::sprintf("[%d]", remaining);

    const auto fits = [&](const std::string &text) -> bool
    {
        return yw_GetGenesisScreenTextWidth(yw, text) <= availableWidth;
    };

    const std::string full = name + " " + count;
    if ( fits(full) )
        return full;

    const std::string compact = name + count;
    if ( fits(compact) )
        return compact;

    const std::string ellipsis = "...";
    if ( !fits(ellipsis + count) )
        return count;

    std::string clipped;
    for (unsigned char ch : name)
    {
        const std::string candidate = clipped + (char)ch + ellipsis + count;
        if ( !fits(candidate) )
            break;

        clipped.push_back((char)ch);
    }

    return clipped + ellipsis + count;
}

bool yw_GuiListOwnsWheelAtPointer(const GuiList &list,
                                         const TClickBoxInf &click)
{
    if ( !list.wheelScroll || list.IsClosed() ||
         (list.flags & GuiBase::FLAG_ICONIFED) )
        return false;

    return click.move.ScreenPos.x >= list.x &&
           click.move.ScreenPos.x < list.x + list.w &&
           click.move.ScreenPos.y >= list.y &&
           click.move.ScreenPos.y < list.y + list.h;
}

void NC_STACK_ypaworld::GUI_Close()
{
    if ( _guiLoaded )
    {
        yw_ResetWorldSelectionDrag(this);
        _moveOrderFeedbackActive = false;
        _attackOrderFeedbacks.clear();
        ypaworld_func140(&lstvw2);
        ypaworld_func140(&exit_menu);
        ypaworld_func140(&info_log);
        ypaworld_func140(&squadron_manager);
        ypaworld_func140(&robo_map);
        ypaworld_func140(&netgame_wnd);

        sub_4DA874(this);
        sub_46E16C(this);
        sub_4C706C(this);
        sub_4C39A4(this);
        sub_4E1D24();
        sub_4E2B24(this);
        sub_4C8524(this);

        FFeedback_StopAll();

        _mouseCursorHidden = false;
        _guiLoaded = false;
    }
}

void NC_STACK_ypaworld::GuiWinToFront(GuiBase *win)
{
    if ( !(win->flags & GuiBase::FLAG_CLOSED) )
    {
        win->Detach();
        win->Attach(_guiActive);
        Input::Engine.RemClickBox(win);
        Input::Engine.AddClickBoxFront(win);
    }
}

void NC_STACK_ypaworld::GuiWinOpen(GuiBase *win)
{
    if ( win->flags & GuiBase::FLAG_CLOSED )
    {
        win->flags &= ~GuiBase::FLAG_CLOSED;
        Input::Engine.AddClickBoxFront(win);
        _guiDragDefaultMouse = false;
    }
}

void NC_STACK_ypaworld::GuiWinClose(GuiBase *win)
{
    if ( !(win->flags & GuiBase::FLAG_CLOSED) )
    {
        win->flags |= GuiBase::FLAG_CLOSED;
        Input::Engine.RemClickBox(win);
        _guiDragDefaultMouse = false;
    }
}
