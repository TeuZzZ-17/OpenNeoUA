// In-game GUI core: shared GUI state, window creation (info log, exit menu,
// energy panel), per-frame window updates and the legacy input handlers.
// The other GUI areas live next to it:
//   yw_ui_status_icons.cpp .. unit status icons and HP/shield bars
//   yw_ui_map_draw.cpp ...... tactical map drawing, map window and zoom
//   yw_ui_map.cpp ........... map toolbar, strategic map and map markers
//   yw_ui_panels.cpp ........ buy list, creation buttons, player panel, finder, energy bars
//   yw_ui_render.cpp ........ GUI render pass: messages, tech popup, cockpit info, radar
//   yw_ui_squadron.cpp ...... squadron manager
//   yw_ui_hud.cpp ........... HUD (radar, compass, lock markers, life bars, info panel, cursors)
//   yw_ui_orders.cpp ........ world selection, order feedback and unit selection by mouse
//   yw_ui_artillery.cpp ..... artillery shell map/HUD elements
//   yw_ui_windows.cpp ....... priority GUI windows, GUI list helpers and window management
//   yw_ui_voice.cpp ......... voice messages
// Shared helpers are declared in yw_game_ui_internal.h.
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

GuiBase *dword_5BAFAC = nullptr;
std::string dword_5BAF98;
int dword_5BAF9C;
GuiList lstvw2;
int dword_5BAFA0;
int dword_5BAFA4;
int dword_5BAFA8;
GuiList exit_menu;
int dword_5C8B78;
int dword_5C8B7C;
int dword_5C8B80;
int dword_5C8B84;
int dword_5C8B88;
squadMan squadron_manager;
yw_infolog info_log;
int dword_516510;
int dword_516514;
int dword_516518;
int dword_51651C;
float flt_5164F4 = 0.0;
float flt_516504 = 0.0;
float flt_51650C = 0.0;
float flt_516524;
float flt_51652C;
float flt_516528;
float flt_516530;
tehMap robo_map;
NC_STACK_ypaworld *dword_5BAA60; // For sort func
energPanel up_panel;
GuiList gui_lstvw;
bzd bzda;

////////////////////////////////////////
////////////////////////////////////////

void NC_STACK_ypaworld::sb_0x4c87fc(const std::string &a2, GuiBase *lstvw)
{
    GuiWinOpen(&lstvw2);
    GuiWinToFront(&lstvw2);

    dword_5BAF98 = a2;
    dword_5BAFAC = lstvw;
    dword_5BAF9C = 3;
}

void NC_STACK_ypaworld::sub_449DE8(const std::string &a2)
{
    // OpenNeoUA: legacy online help pages are obsolete/dead.
    // Keep this as a harmless compatibility stub for old callers/hotkeys.
    (void)a2;
    _helpURL.clear();
    _helpSavedURL.clear();
}

void sub_4C3A54(NC_STACK_ypaworld *yw)
{
    if ( yw->_userUnit )
    {
        if ( yw->_userUnit == yw->_userRobo || yw->_playerInHSGun )
        {
            bzda.field_1CC = 0x29;

            if ( bzda.field_8E0 )
            {
                bzda.field_1CC = 0x2B;

                if ( yw->_activeCmdrRemapIndex != -1 )
                    bzda.field_1CC = 0x2F;
            }

            if ( bzda.field_8E4 )
                bzda.field_1CC |= 0x10;

            bzda.field_1D4 = 1;
        }
        else
        {
            bzda.field_1D4 = 1;
            bzda.field_1CC = 41;
        }

        if ( yw->_userRobo->_status == BACT_STATUS_DEAD )
            bzda.field_1CC = 0;
    }
    else
    {
        bzda.field_1D4 = 0;
        bzda.field_1CC = 0;
    }

    bzda.field_1D0 &= bzda.field_1CC;
    bzda.field_1D8 &= bzda.field_1D4;

    if ( !bzda.field_1D0 )
        bzda.field_1D0 = bzda.field_1CC & 1;
}

void sb_0x451034__sub8(NC_STACK_ypaworld *yw)
{
    up_panel = energPanel();

    TileMap *f30 = yw->_guiTiles[30];

    up_panel.field_1CC = f30->h;
    up_panel.field_1DC = f30->map[65].w;
    up_panel.flags = 0;
    up_panel.field_1D0 = up_panel.field_1DC + f30->map[84].w;
    up_panel.buttons.resize(4);

    int v8 = yw->_screenSize.x - 4 * (up_panel.field_1DC + f30->map[84].w);

    up_panel.field_1D8 = v8 / 4;
    up_panel.field_1D4 = v8 / 8;
    up_panel.field_1E0 = 2;
    up_panel.field_1E4 = up_panel.field_1D0 - up_panel.field_1DC - 4;

    up_panel.x = 0;
    up_panel.y = 0;
    up_panel.w = yw->_screenSize.x;
    up_panel.h = up_panel.field_1CC;

    for (int i = 0; i < 4; i++)
        up_panel.buttons[i] = ButtonBox(up_panel.field_1D4 + (up_panel.field_1D0 + up_panel.field_1D8) * i,    0,
                                        up_panel.field_1D0,   up_panel.field_1CC);

    up_panel.cmdCommands.reserve(512);

    Input::Engine.AddClickBoxFront(&up_panel);
}

void NC_STACK_ypaworld::FFeedback_Init()
{
    _ffTimeStamp = 0;
    _ffEffectType = -1;
    _ffPeriod = 0;
    _ffMagnitude = 0;
}

void create_info_log(NC_STACK_ypaworld *yw)
{
    info_log = yw_infolog();

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_LOG);
    args.resizeable = true;
    args.numEntries = 1;
    args.shownEntries = 6;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 24;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = 200;
    args.maxEntryWidth = 32000;
    args.minEntryWidth = 2 * (5 * yw->_guiTiles[0]->map[48].w);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.closeChar = 74;

    if ( info_log.Init(yw, args) )
    {
        if ( yw->_timeStamp )
        {
            std::string tmp = fmt::sprintf("<%02d:%02d:%02d>", (yw->_timeStamp >> 10) / 60 / 60 % 24, (yw->_timeStamp >> 10) / 60 % 60, (yw->_timeStamp >> 10) % 60);
            std::string tmp2 = Locale::Text::Common(Locale::CMN_MISNCONT);
            sprintf(info_log.msgs[0].txt, tmp2.c_str(), tmp.c_str());
        }
        else
        {
            std::string tmp = Locale::Text::Common(Locale::CMN_WELCOME);
            strcpy(info_log.msgs[0].txt, tmp.c_str());
        }

        info_log.msgs[0].field_4 = yw->_timeStamp + 1;
        info_log.field_255C = 0;
        info_log.field_2560 = yw->_timeStamp;
        info_log.field_2564 = 0;
        info_log.field_250 = 127;
        info_log.field_248 = 6 * yw->_guiTiles[0]->map[48].w + 2 * yw->_guiTiles[0]->map[58].w + 12;
    }
}

void create_exit_menu(NC_STACK_ypaworld *yw)
{
    dword_5C8B7C = (yw->_fontH / 2) + yw->_fontH;
    dword_5C8B84 = yw->_fontH / 4;

    int tmp = yw->_guiTiles[0]->GetWidth("WWWWWWW");

    GuiList::tInit args;
    args.title = Locale::Text::WinName(Locale::WINNAME_EXIT);
    args.resizeable = false;
    args.numEntries = 5;
    args.shownEntries = 5;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 5;
    args.minShownEntries = 5;
    args.withIcon = false;
    args.entryHeight = dword_5C8B7C;
    args.entryWidth = (2 * tmp + 16);
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.factionAccentTitle = false;

    if ( exit_menu.Init(yw, args) )
    {
        int v5 = yw->_fontH + exit_menu.upperVborder + dword_5C8B84;

        dword_5C8B80 = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        dword_5C8B88 = exit_menu.entryWidth - 2 * dword_5C8B80;

        for (int i = 8; i < 13; i++)
        {
            exit_menu.buttons[i] = ButtonBox(dword_5C8B80, (i - 8) * dword_5C8B7C + v5,
                                             dword_5C8B88, yw->_fontH);
        }
    }
}

void sb_0x451034__sub5(NC_STACK_ypaworld *yw)
{
    int v2 = yw->_guiTiles[0]->GetWidth("WWWWWWW") * 3.5 + 16;

    GuiList::tInit args;
    args.title = " ";
    args.resizeable = false;
    args.numEntries = 3;
    args.shownEntries = 3;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 3;
    args.minShownEntries = 3;
    args.withIcon = false;
    args.entryHeight = yw->_fontH;
    args.entryWidth = v2;
    args.enabled = true;
    args.vborder = yw->_fontBorderH;
    args.staticItems = true;
    args.closeChar = 85;
    args.withHelp = false;

    if ( lstvw2.Init(yw, args) )
    {
        int v1 = yw->_fontH + lstvw2.upperVborder + 2 * lstvw2.entryHeight;

        dword_5BAFA8 = 2 * yw->_guiTiles[0]->map[32].w + yw->_fontBorderW;
        dword_5BAFA0 = (lstvw2.entryWidth - 2 * dword_5BAFA8) / 3;

        lstvw2.buttons.resize(10);

        lstvw2.buttons[8] = ButtonBox(dword_5BAFA8, v1, dword_5BAFA0, lstvw2.entryHeight);

        lstvw2.buttons[9] = ButtonBox(  lstvw2.entryWidth - dword_5BAFA8 - dword_5BAFA0,
                                        v1,
                                        dword_5BAFA0,
                                        lstvw2.entryHeight);

        dword_5BAFA4 = lstvw2.buttons[9].x - (dword_5BAFA0 + lstvw2.buttons[8].x);

        dword_5BAF9C = 0;
    }
}

void sb_0x451034__sub9(NC_STACK_ypaworld *yw)
{
    const std::array<std::string, 14> off_51825C
    {
        "wireless/bar.sklt",
        "wireless/compass.sklt",
        "wireless/frame.sklt",
        "wireless/arrow.sklt",
        "wireless/mgvis.sklt",
        "wireless/grenvis1.sklt",
        "wireless/rockvis1.sklt",
        "wireless/missvis1.sklt",
        "wireless/bombvis1.sklt",
        "wireless/grenvis2.sklt",
        "wireless/rockvis2.sklt",
        "wireless/missvis2.sklt",
        "wireless/bombvis2.sklt",
        "wireless/triangle.sklt"
    };

    sklt_wis *wis = &yw->_hud;

    *wis = sklt_wis();

    wis->field_0 = 1;

    Common::Env.SetPrefix("rsrc", "data:");

    for (int i = 0; i < 14; i++)
    {
        wis->sklts[i] = Nucleus::CInit<NC_STACK_sklt>({{NC_STACK_rsrc::RSRC_ATT_NAME, off_51825C.at(i)}});

        if ( wis->sklts[i] )
            wis->sklts_intern[i] = wis->sklts[i]->GetSkelet();
    }

    float CW, CH;
    GFX::Engine.getAspectCorrection(CW, CH, false);

    if ( yw->_screenSize.x >= 512 )
    {
        wis->field_9E = 8;
        wis->field_96 = 28;
        wis->field_92 = (float)yw->_guiTiles[0]->h / ((float)yw->_screenSize.y * CH);
        wis->field_8E = wis->field_92 * 14.0;
        wis->field_9A = 28;
    }
    else
    {
        wis->field_9E = 6;
        wis->field_96 = 18;
        wis->field_92 = (float)yw->_guiTiles[0]->h * 1.5 / ((float)yw->_screenSize.y * CH);
        wis->field_8E = wis->field_92 * 18.0;
        wis->field_9A = 18;
    }

    wis->field_72 = 0;
    wis->field_8A = (float)(yw->_guiTiles[51]->map[1].w * wis->field_9E + wis->field_9A + wis->field_96) / ((float)yw->_screenSize.x * CW);

    wis->field_72 = 1;
}

void sub_4DA874(NC_STACK_ypaworld *yw)
{
    exit_menu.Free();
}

void sub_46E16C(NC_STACK_ypaworld *yw)
{
    info_log.Free();
}

void sub_4C706C(NC_STACK_ypaworld *yw)
{
    squadron_manager.Free();
}

void sub_4C39A4(NC_STACK_ypaworld *yw)
{
    yw->GuiWinClose( &gui_lstvw );

    if ( bzda.field_1D8 & 1 )
    {
        bzda.field_1D8 &= 0xFFFFFFFE;
        Input::Engine.RemClickBox(&bzda);
    }

    gui_lstvw.Free();
}

void sub_4E1D24()
{
    Input::Engine.RemClickBox(&up_panel);
}

void sub_4E2B24(NC_STACK_ypaworld *yw)
{
    for (int i = 0; i < 14; i++)
    {
        if (yw->_hud.sklts[i])
        {
            yw->_hud.sklts[i]->Delete();
            yw->_hud.sklts[i] = nullptr;
            yw->_hud.sklts_intern[i] = nullptr;
        }
    }
}

void sub_4C8524(NC_STACK_ypaworld *yw)
{
    lstvw2.Free();
}

void NC_STACK_ypaworld::FFeedback_StopAll()
{
    Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_ALL);
}

int sb_0x451034(NC_STACK_ypaworld *yw)
{
    if ( yw->_guiLoaded )
        return yw->_guiLoaded;

    yw->_guiDragging = false;
    yw->_guiDragElement = nullptr;
    yw->_guiDragDefaultMouse = false;
    yw->_mouseCursorHidden = false;
    yw->_prevMousePos = Common::Point();
    yw->_bactPrevClicked = nullptr;
    yw_ResetWorldSelectionDrag(yw);
    yw->_moveOrderFeedbackActive = false;
    yw->_attackOrderFeedbacks.clear();

    sb_0x451034__sub9(yw);
    sb_0x451034__sub8(yw);
    sb_0x451034__sub2(yw);
    create_squad_man(yw);
    create_info_log(yw);
    create_exit_menu(yw);
    sb_0x451034__sub5(yw);
    sb_0x451034__sub6(yw);

    yw->ypaworld_func139(&robo_map);
    yw->ypaworld_func139(&squadron_manager);
    yw->ypaworld_func139(&info_log);
    yw->ypaworld_func139(&exit_menu);
    yw->ypaworld_func139(&lstvw2);
    yw->ypaworld_func139(&netgame_wnd);

    if ( !sb_0x451034__sub3(yw) )
    {
        yw->ypaworld_func140(&lstvw2);
        yw->ypaworld_func140(&exit_menu);
        yw->ypaworld_func140(&info_log);
        yw->ypaworld_func140(&squadron_manager);
        yw->ypaworld_func140(&robo_map);
        yw->ypaworld_func140(&netgame_wnd);

        sub_4DA874(yw);
        sub_46E16C(yw);
        sub_4C706C(yw);
        return 0;
    }

    yw->_guiLoaded = true;

    if ( yw->_preferences & World::PREF_CDMUSICDISABLE )
    {
        SFXEngine::SFXe.SetMusicTrack(yw->_levelInfo.MusicTrack, yw->_levelInfo.MusicTrackMinDelay, yw->_levelInfo.MusicTrackMaxDelay);
        SFXEngine::SFXe.PlayMusicTrack();
    }

    return 1;
}

uint32_t sub_4C7134(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact)
{
    int protoId = ResolveUnitDisplayVehicleProtoId(yw, bact);
    uint32_t icon = protoId >= 0 ? yw->_vhclProtos[protoId].type_icon : 0;

    if ( !icon )
        icon = 65;

    return icon;
}

void sub_4C7950(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3)
{
    if ( a4 > 0 && a3 > 0 )
    {
        if ( a4 + squadron_manager.field_2CC < yw->_screenSize.x )
        {
            if ( a3 + yw->_fontH < yw->_screenSize.y )
            {
                FontUA::select_tileset(cur, 28);

                FontUA::set_center_xpos(cur, a4 - (yw->_screenSize.x / 2));
                FontUA::set_center_ypos(cur, a3 - (yw->_screenSize.y / 2));

                FontUA::set_opacity(cur, kSquadronManagerDragOpacity);
                FontUA::store_glyph(cur, sub_4C7134(yw, squadron_manager.field_2BC));
                FontUA::set_opacity(cur, 255);
            }
        }
    }
}

void ypaworld_func64__sub7__sub3__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    yw_StoreGuiListRowBackground(&squadron_manager, cur, 0,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw));

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub3__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur)
{
    const int blink = (yw->_timeStamp / 300) & 1;
    uint32_t icon = sub_4C7134(yw, bact);
    if ( bact == yw->_userUnit && blink )
        icon = 33;
    else if ( info_log.field_255C == bact->_gid
              && yw->_timeStamp - info_log.msgs[info_log.field_24C].field_4 < 10000
              && blink )
    {
        icon = 34;
    }
    const bool selected = yw_IsSquadronManagerRowSelected(yw, bact);
    const bool hovered = yw_IsSquadronManagerRowHovered(bact);
    const int rowTileset = hovered && !selected ? 9 : 0;
    const int rowOpacity = selected ? 0 : (hovered ? 208 : -1);

    yw_StoreGuiListRowBackground(&squadron_manager, cur, rowTileset,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw),
                                 rowOpacity);
    FontUA::add_xpos(cur,
        -(squadron_manager.entryWidth - 2 * yw->_fontBorderW + 1
          + yw_GetSquadronManagerScrollbarGapFillWidth(yw)));

    FontUA::select_tileset(cur, 28);
    FontUA::store_glyph(cur, icon);

    // Keep the classic lifemeter visible for the Host Station as well.  The
    // 128..131 glyphs carry their own vertical offset and render above the
    // unit icon, matching the original Squadron Manager health indicator.
    FontUA::add_xpos(cur, -yw->_guiTiles[28]->Glyph(icon).w);
    FontUA::store_u8(cur, yw_GetSquadronManagerHealthGlyph(bact));

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub3__sub0__sub0(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur)
{
    const bool selected = yw_IsSquadronManagerRowSelected(yw, bact);
    const bool hovered = yw_IsSquadronManagerRowHovered(bact);
    const int rowTileset = hovered && !selected ? 9 : 0;
    const int rowOpacity = selected ? 0 : (hovered ? 208 : -1);
    const int rowFillWidth = yw_GetSquadronManagerBackgroundFillWidth(yw);
    const int rowRewind = squadron_manager.entryWidth
                        - 2 * yw->_fontBorderW + 1
                        + yw_GetSquadronManagerScrollbarGapFillWidth(yw);

    yw_StoreGuiListRowBackground(&squadron_manager, cur, rowTileset,
                                 rowFillWidth, rowOpacity);

    FontUA::add_xpos(cur, -rowRewind);
    yw_RenderSquadronManagerSquadContent(yw, bact, cur, 255);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub3__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur)
{
    const uint8_t backgroundOpacity = squadron_manager.backgroundOpacity;

    // Fixed aggression footer.  It is rendered after all visible squad rows,
    // inside the extra lower border reserved by create_squad_man().
    yw_StoreGuiListRowBackground(&squadron_manager, cur, 0,
                                 yw_GetSquadronManagerBackgroundFillWidth(yw));
    FontUA::add_xpos(cur,
        -(squadron_manager.entryWidth - 2 * yw->_fontBorderW + 1
          + yw_GetSquadronManagerScrollbarGapFillWidth(yw)));

    const int aggressionOffset = squadron_manager.field_2DC - yw->_fontBorderW;
    if ( aggressionOffset > 0 )
        FontUA::add_xpos(cur, aggressionOffset);

    yw_RenderSquadronAggressionStrip(yw, cur);
    FontUA::next_line(cur);

    yw_SetGuiListBackgroundOpacity(cur, backgroundOpacity);

    FontUA::reset_tileset(cur, 28);

    FontUA::set_yoff(cur, yw->_guiTiles[28]->h - yw->_fontBorderH);

    FontUA::store_u8(cur, 38);

    FontUA::op17(cur, yw_GetSquadronManagerBackgroundFillWidth(yw));

    FontUA::store_u8(cur, 47);
    FontUA::store_u8(cur, 61);

    if ( backgroundOpacity != 255 )
        FontUA::set_opacity(cur, 255);
}

void ypaworld_func64__sub7__sub3__sub0(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    squadron_manager.itemBlock.clear();

    squadron_manager.ItemsPreLayout(yw, &squadron_manager.itemBlock, 0, "{ }");

    FontUA::select_tileset(&squadron_manager.itemBlock, 28);

    for (int i = 0; i < squadron_manager.shownEntries; i++)
    {
        NC_STACK_ypabact *v8 = squadron_manager.squads[i];

        if ( v8 )
        {
            if ( v8 == yw->_userRobo )
            {
                ypaworld_func64__sub7__sub3__sub0__sub1(yw, v8, &squadron_manager.itemBlock);
            }
            else
            {
                if ( v8->IsParentMyRobo() )
                    ypaworld_func64__sub7__sub3__sub0__sub0(yw, v8, &squadron_manager.itemBlock);
                else
                    ypaworld_func64__sub7__sub3__sub0__sub2(yw, &squadron_manager.itemBlock);
            }
        }
        else
        {
            ypaworld_func64__sub7__sub3__sub0__sub2(yw, &squadron_manager.itemBlock);
        }
    }

    ypaworld_func64__sub7__sub3__sub0__sub3(yw, &squadron_manager.itemBlock);

    if ( squadron_manager.field_2A8 & 1 )
    {
        if ( squadron_manager.field_2C8 != winpt->move.ScreenPos.x || squadron_manager.field_2CA != winpt->move.ScreenPos.y )
        {
            if ( squadron_manager.draggingWholeSquad )
            {
                yw_RenderSquadronManagerWholeSquadDragGhost(
                    yw, &squadron_manager.itemBlock,
                    squadron_manager.field_2C4, squadron_manager.field_2C6);
            }
            else
            {
                sub_4C7950(yw, &squadron_manager.itemBlock,
                           squadron_manager.field_2C4,
                           squadron_manager.field_2C6);

                if ( squadron_manager.field_2A8 & 2 )
                {
                    sub_4C7950(yw, &squadron_manager.itemBlock,
                               squadron_manager.field_2C4
                                   + squadron_manager.field_2CC / 4,
                               squadron_manager.field_2C6
                                   + (yw->_fontH / 4));
                }
            }
        }
    }

    FontUA::set_end(&squadron_manager.itemBlock);
}

void sub_4C707C(NC_STACK_ypaworld *yw)
{
    squadron_manager.squads.fill(nullptr);
    squadron_manager.squadRemapIndices.fill(-2);

    const int entryCount = (int)yw->_cmdrsRemap.size() + 1;
    if ( (size_t)(squadron_manager.firstShownEntries
                   + squadron_manager.shownEntries) >= (size_t)entryCount )
    {
        squadron_manager.firstShownEntries = entryCount
                                             - squadron_manager.shownEntries;
        if ( squadron_manager.firstShownEntries < 0 )
            squadron_manager.firstShownEntries = 0;
    }

    const int first = squadron_manager.firstShownEntries;
    for ( int i = 0; i < entryCount; i++ )
    {
        const int visibleIndex = i - first;
        if ( visibleIndex >= 0 && visibleIndex < squadron_manager.shownEntries )
        {
            squadron_manager.squads[visibleIndex] =
                i == 0 ? yw->_userRobo : yw->_cmdrsRemap[i - 1];
            squadron_manager.squadRemapIndices[visibleIndex] = i - 1;
        }
    }

    squadron_manager.numEntries = entryCount;
}

bool NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub2()
{
    NC_STACK_ypabact *bct = _cmdrsRemap[squadron_manager.field_2B4];
    if ( !bct )
        return false;

    if ( bct == squadron_manager.field_2BC )
    {
        if (bct->_status != BACT_STATUS_DEAD && bct->_status != BACT_STATUS_CREATE && bct->_status != BACT_STATUS_BEAM && !bct->ShouldHideFromStrategicUI() )
            return true;
    }

    for ( NC_STACK_ypabact* &kid : bct->_kidList )
    {
        if ( kid == squadron_manager.field_2BC )
        {
            if ( kid->_status != BACT_STATUS_DEAD && kid->_status != BACT_STATUS_CREATE && kid->_status != BACT_STATUS_BEAM && !kid->ShouldHideFromStrategicUI() )
                return true;
        }
    }
    return false;
}

void ypaworld_func64__sub7__sub3__sub3(NC_STACK_ypaworld *yw, TClickBoxInf *winpt)
{
    if ( winpt->selected_btn != &squadron_manager
            || winpt->selected_btnID < 8
            || squadron_manager.numEntries <= (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries)
            || squadron_manager.squads[ winpt->selected_btnID - 8 ] == 0
            || squadron_manager.squads[ winpt->selected_btnID - 8 ] == yw->_userRobo )
    {
        if ( squadron_manager.field_2A8 & 2 )
        {
            if ( squadron_manager.field_2BC->_host_station != squadron_manager.field_2BC->_parent )
            {
                NC_STACK_ypabact *v3 = nullptr;

                World::RefBactList *kidlist = squadron_manager.field_2BC->_kidRef.PList();
                if (kidlist)
                {
                    World::RefBactList::iterator it = squadron_manager.field_2BC->_kidRef;

                    while(it != kidlist->end())
                    {
                        NC_STACK_ypabact *itbact = *it;
                        it++;

                        bact_arg109 arg109;

                        if ( v3 )
                        {
                            arg109.field_4 = itbact;
                            arg109.field_0 = 4;
                        }
                        else
                        {
                            arg109.field_4 = nullptr;
                            arg109.field_0 = 3;

                            v3 = itbact;
                        }
                        v3->ReorganizeGroup(&arg109);
                    }
                }
            }
        }
        else
        {
            bact_arg109 arg109;
            arg109.field_4 = nullptr;
            arg109.field_0 = 3;

            squadron_manager.field_2BC->ReorganizeGroup(&arg109);
        }
        yw->_activeCmdrID = squadron_manager.field_2BC->_commandID;
        yw->sub_4C40AC();
        sub_4C707C(yw);
    }
    else
    {
        NC_STACK_ypabact *v6 = squadron_manager.squads[ winpt->selected_btnID - 8 ];
        int v7;
        if ( squadron_manager.field_2BC->IsParentMyRobo() )
            v7 = v6 == squadron_manager.field_2BC;
        else
            v7 = v6 == squadron_manager.field_2BC->_parent;

        if ( !v7 )
        {
            if ( squadron_manager.field_2A8 & 2 )
            {
                if ( squadron_manager.field_2BC->_host_station != squadron_manager.field_2BC->_parent )
                {
                    World::RefBactList *kidlist = squadron_manager.field_2BC->_kidRef.PList();
                    if (kidlist)
                    {
                        World::RefBactList::iterator it = squadron_manager.field_2BC->_kidRef;
                        while ( it != kidlist->end() )
                        {
                            NC_STACK_ypabact *itbact = *it;
                            it++;

                            bact_arg109 arg109;
                            arg109.field_0 = 1;
                            arg109.field_4 = v6;
                            itbact->ReorganizeGroup(&arg109);
                        }
                    }
                }
                else
                {
                    bact_arg109 arg109;
                    arg109.field_0 = 1;
                    arg109.field_4 = v6;
                    squadron_manager.field_2BC->ReorganizeGroup(&arg109);
                }
            }
            else
            {
                bact_arg109 arg109;

                if ( squadron_manager.field_2BC == yw->_userUnit )
                {
                    // Moving the directly controlled vehicle into another squad
                    // makes it that squad's commander. Reuse the existing
                    // ORG_BECOMECHIEF path so command ID, targets and members are
                    // transferred atomically instead of appending the player at
                    // the end of the destination list.
                    arg109.field_0 = 2;
                    arg109.field_4 = v6;
                    yw->_userUnit->ReorganizeGroup(&arg109);
                    v6 = yw->_userUnit;
                }
                else
                {
                    arg109.field_0 = 4;
                    arg109.field_4 = squadron_manager.field_2BC;
                    v6->ReorganizeGroup(&arg109);
                }
            }
            yw->_activeCmdrID = v6->_commandID;
            yw->sub_4C40AC();
            sub_4C707C(yw);
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub4(NC_STACK_ypabact *bact)
{
    if ( bact->_status == BACT_STATUS_NORMAL )
    {
        if ( bact->_status_flg & BACT_STFLAG_ESCAPE )
            SetShowingTooltip(Locale::TIP_FINDER_ESCAPE);
        else if ( bact->_secndTtype )
            SetShowingTooltip(Locale::TIP_FINDER_FIGHTING);
        else if ( bact->_primTtype )
            SetShowingTooltip(Locale::TIP_FINDER_MOVE);
    }
    else if (bact->_status == BACT_STATUS_IDLE)
        SetShowingTooltip(Locale::TIP_FINDER_WAITORDER);
}

NC_STACK_ypabact * NC_STACK_ypaworld::sub_4C7B0C(int sqid, int a3)
{
    NC_STACK_ypabact *v3 = squadron_manager.squads[sqid];

    if ( !v3 )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    if ( v3 == _userRobo )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return _userRobo;
    }

    squadron_manager.field_2B0 = -1;
    squadron_manager.field_2AC = squadron_manager.squadRemapIndices[sqid];
    if ( squadron_manager.field_2AC < 0
            || (size_t)squadron_manager.field_2AC >= _cmdrsRemap.size() )
    {
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    const std::vector<NC_STACK_ypabact *> displayUnits =
        yw_GetSquadronDisplayUnits(v3);
    const int unitSlotCount = yw_GetSquadronManagerUnitSlotCount(this);

    if ( a3 >= squadron_manager.field_2D0
            && a3 < squadron_manager.field_2D0 + squadron_manager.field_2CC )
        return v3;

    if ( a3 < squadron_manager.field_2D4 )
    {
        squadron_manager.field_2B0 = -1;
        squadron_manager.field_2AC = -1;
        return nullptr;
    }

    const int slot = 1 + (a3 - squadron_manager.field_2D4)
                         / std::max(1, squadron_manager.field_2CC);
    if ( slot < unitSlotCount && slot < (int)displayUnits.size() )
    {
        squadron_manager.field_2B0 = slot - 1;
        return displayUnits[slot];
    }

    squadron_manager.field_2B0 = -1;
    squadron_manager.field_2AC = -1;
    return nullptr;
}

int NC_STACK_ypaworld::ypaworld_func64__sub7__sub3__sub1(TClickBoxInf *winpt)
{
    squadron_manager.draggingWholeSquad = false;

    if ( winpt->selected_btnID < 8 )
        return 0;
    if ( bzda.field_1D0 & 8 )
        return 0;
    if ( (int32_t)(winpt->selected_btnID - 8 + squadron_manager.firstShownEntries) >= squadron_manager.numEntries )
        return 0;

    const int squadIndex = winpt->selected_btnID - 8;
    NC_STACK_ypabact *v5 = sub_4C7B0C(squadIndex, winpt->move.BtnPos.x);

    // The blank part after the visible unit icons is the drag handle for the
    // complete squad.
    // A normal LMB click still goes through the GuiList selection path; holding
    // LMB and moving starts the existing drag/drop flow.  The whole-group flag
    // makes the drop reuse ORG_NEWCHIEF so leader and members are absorbed by
    // the destination squad together.
    if ( !v5 && (winpt->flag & TClickBoxInf::FLAG_LM_DOWN)
            && yw_IsSquadronManagerWholeSquadDragArea(
                   this, squadIndex, winpt->move.BtnPos.x) )
    {
        v5 = squadron_manager.squads[squadIndex];
        squadron_manager.draggingWholeSquad = true;
        squadron_manager.field_2AC =
            squadron_manager.squadRemapIndices[squadIndex];
        squadron_manager.field_2B0 = -1;
    }

    if ( !v5 )
        return 0;

    if ( robo_map.IsOpen() )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_RM_DOWN )
        {
            robo_map.field_1D8 = v5->_position.x;
            robo_map.field_1ED = 0;
            robo_map.field_1DC = v5->_position.z;

            sub_4C1814(this, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
        }
    }

    if ( v5 == _userRobo )
        return 0;

    if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        // The list rows are rebuilt before the generic DOACTION_8 handler runs.
        // Publish the new stable squad identity now so that buffer and overlay
        // agree in this same frame; the normal action path completes selection.
        NC_STACK_ypabact *squadCommander = sub_4D3C3C(v5);
        if ( squadCommander )
            _activeCmdrID = squadCommander->_commandID;
    }

    squadron_manager.field_2B4 = squadron_manager.field_2AC;
    squadron_manager.field_2B8 = squadron_manager.field_2B0;
    squadron_manager.field_2C8 = winpt->move.ScreenPos.x;
    squadron_manager.field_2BC = v5;

    if ( squadron_manager.draggingWholeSquad )
    {
        // The whole-squad ghost follows the pointer, not the row's original
        // icon column.  This keeps the dragged icons centered under the mouse
        // regardless of whether the Squadron Manager is compact or widened.
        const std::vector<NC_STACK_ypabact *> displayUnits =
            yw_GetSquadronDisplayUnits(v5);
        const int ghostWidth = std::max(1, (int)displayUnits.size())
                             * std::max(1, squadron_manager.field_2CC);
        squadron_manager.field_2C0 = -(ghostWidth / 2);
        squadron_manager.field_2C2 = -(std::max(1, _fontH) / 2);
    }
    else
    {
        if ( v5->IsParentMyRobo() )
        {
            squadron_manager.field_2C0 = -(winpt->move.BtnPos.x - squadron_manager.field_2D0);
        }
        else
        {
            const int relativeX = winpt->move.BtnPos.x - squadron_manager.field_2D4;
            squadron_manager.field_2C0 = -(relativeX
                                          % std::max(1, squadron_manager.field_2CC));
        }

        squadron_manager.field_2C2 = -winpt->move.BtnPos.y;
    }
    squadron_manager.field_2CA = winpt->move.ScreenPos.y;
    return 1;
}

void sub_4C1FBC()
{
    ButtonBox &pMapInter = robo_map.buttons[17];

    int v0 = -(pMapInter.x + robo_map.x + (pMapInter.w / 2));
    int v3 = -(pMapInter.y + robo_map.y + (pMapInter.h / 2));
    flt_516524 = (float)(v0 + dword_516510) * robo_map.field_1E0 + robo_map.field_1D8;
    flt_51652C = (float)(v0 + dword_516518) * robo_map.field_1E0 + robo_map.field_1D8;
    flt_516528 = robo_map.field_1DC - (float)(v3 + dword_516514) * robo_map.field_1E4;
    flt_516530 = robo_map.field_1DC - (float)(v3 + dword_51651C) * robo_map.field_1E4;

    if ( flt_516524 > flt_51652C )
    {
        float v4 = flt_516524;
        flt_516524 = flt_51652C;
        flt_51652C = v4;
    }

    if ( flt_516528 > flt_516530 )
    {
        float v5 = flt_516528;
        flt_516528 = flt_516530;
        flt_516530 = v5;
    }
}

void ypaworld_func64__sub7__sub1__sub0(NC_STACK_ypaworld *yw)
{
    if ( yw && yw->IsSpectatorControlled() )
    {
        yw->_activeCmdrID = 0;
        yw->_activeCmdrRemapIndex = -1;
        yw->_activeCmdrKidsCount = 0;
        yw->_cmdrIdToSelect = -1;
        return;
    }

    int v8 = 0;

    int v22 = flt_516524 / World::CVSectorLength;
    int v6 = -flt_516530 / World::CVSectorLength;
    int v24 = flt_51652C / World::CVSectorLength;
    int v23 = -flt_516528 / World::CVSectorLength;

    if ( v22 >= yw->_mapSize.x )
        v22 = yw->_mapSize.x - 1;
    else if ( v22 < 1 )
        v22 = 1;

    if ( v24 >= yw->_mapSize.x )
        v24 = yw->_mapSize.x - 1;
    else if ( v24 < 1 )
        v24 = 1;

    if ( v6 >= yw->_mapSize.y )
        v6 = yw->_mapSize.y - 1;
    else if ( v6 < 1 )
        v6 = 1;

    if ( v23 >= yw->_mapSize.y )
        v23 = yw->_mapSize.y - 1;
    else if ( v23 < 1 )
        v23 = 1;

    NC_STACK_ypabact *v13 = nullptr;

    for (int i = v6; i <= v23; i++)
    {
        for (int j = v22; j <= v24; j++)
        {
            cellArea &cll = yw->_cells(j, i);

            if ( robo_map.MapViewMask & cll.view_mask )
            {
                for ( NC_STACK_ypabact* &bct : cll.unitsList )
                {
                    if ( bct->_owner == yw->_userRobo->_owner )
                    {
                        if ( bct->_position.x > flt_516524 && bct->_position.z > flt_516528 && bct->_position.x < flt_51652C && bct->_position.z < flt_516530 )
                        {
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_bact_type != BACT_TYPES_GUN )
                            {
                                if ( bct->_status != BACT_STATUS_CREATE && bct->_status != BACT_STATUS_BEAM && bct->_status != BACT_STATUS_DEAD )
                                {

                                    if ( v13 )
                                    {
                                        if ( bct->_bact_type != BACT_TYPES_ROBO )
                                        {
                                            bact_arg109 arg109;
                                            arg109.field_4 = bct;
                                            arg109.field_0 = 4;

                                            v13->ReorganizeGroup(&arg109);

                                            v8 = 0;
                                        }
                                    }
                                    else
                                    {
                                        if ( bct->_bact_type == BACT_TYPES_ROBO )
                                        {
                                            if ( bct == yw->_userRobo )
                                                v8 = 1;
                                        }
                                        else
                                        {
                                            v13 = bct;

                                            bact_arg109 arg109;
                                            arg109.field_4 = nullptr;
                                            arg109.field_0 = 3;

                                            v13->ReorganizeGroup(&arg109);

                                            v8 = 0;
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

    if ( v8 )
        bzda.field_1D0 = bzda.field_1CC & 0x20;

    if ( v13 )
        yw->_activeCmdrID = v13->_commandID;

    yw->sub_4C40AC();
}

SDL_Color yw_GetFactionSelectionColor(NC_STACK_ypaworld *yw)
{
    if (yw && yw->_userRobo && yw->_userRobo->_owner >= 1 &&
        yw->_userRobo->_owner <= 6)
    {
        return yw->GetColor(yw->_userRobo->_owner);
    }

    return GFX::Engine.Color(210, 210, 195);
}

void ypaworld_func64__sub7__sub0__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int id)
{
    int v5 = id + info_log.field_254 + info_log.firstShownEntries;

    if ( v5 >= 64 )
        v5 -= 64;
    else if ( v5 < 0 )
        v5 += 64;

    FontUA::store_u8(cur, 123);

    FontUA::FormateClippedText(yw->_guiTiles[0], cur, info_log.msgs[v5].txt, info_log.entryWidth - 2 * yw->_fontBorderW, 32);

    FontUA::store_u8(cur, 125);
    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub0__sub0(NC_STACK_ypaworld *yw)
{
    info_log.itemBlock.clear();

    info_log.ItemsPreLayout(yw, &info_log.itemBlock, 0, "{ }");

    for (int i = 0; i < info_log.shownEntries; i++ )
        ypaworld_func64__sub7__sub0__sub0__sub0(yw, &info_log.itemBlock, i);

    info_log.ItemsPostLayout(yw, &info_log.itemBlock, 0, "xyz");

    FontUA::set_end(&info_log.itemBlock);
}

void ypaworld_func64__sub7__sub0(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( info_log.flags & (GuiBase::FLAG_CLOSED | GuiBase::FLAG_ICONIFED) )
    {
        info_log.InputHandle(yw, inpt);
    }
    else
    {
        info_log.InputHandle(yw, inpt);
        info_log.Formate(yw);
        ypaworld_func64__sub7__sub0__sub0(yw);
    }
}

GuiBase * ypaworld_func64__sub7__sub6__sub1()
{
    return dword_5BAFAC;
}

void NC_STACK_ypaworld::NetSendExitMsg(uint8_t normExit)
{
    uamessage_exit exMsg;
    exMsg.msgID = UAMSG_EXIT;
    exMsg.owner = _GameShell->netPlayerOwner;
    exMsg.norm = normExit;

    NetBroadcastMessage(&exMsg, sizeof(exMsg), true);

    _GameShell->sentAQ = false;
}

void sub_451714_with_yoffset(TileMap *, CmdStream *cur, const std::string &txt, int a2, uint8_t a4, int textYOffset)
{
    if ( a2 > 0 )
    {
        if ( textYOffset )
            FontUA::add_ypos(cur, textYOffset);

        FontUA::copy_position(cur);

        if ( textYOffset )
            FontUA::add_ypos(cur, -textYOffset);

        int v6 = a2;
        while (v6 > 0)
        {
            if ( v6 <= 255 )
                FontUA::op10(cur, v6);
            else
                FontUA::op10(cur, 255);

            FontUA::store_u8(cur, a4);

            v6 -= 255;
        }

        FontUA::add_txt(cur, a2, 4, txt);
    }
}

void sub_451714(TileMap *tiles, CmdStream *cur, const std::string &txt, int a2, uint8_t a4)
{
    sub_451714_with_yoffset(tiles, cur, txt, a2, a4, 0);
}

void sub_4DA8DC(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5)
{
    std::string v5 = a5;

    FontUA::set_yheight(cur, dword_5C8B84);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, dword_5C8B80 + dword_5C8B88);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int v11;

    if ( a4 )
        v11 = 4;
    else if ( a3 )
        v11 = 8;
    else
        v11 = 3;

    FontUA::select_tileset(cur, v11);
    FontUA::store_u8(cur, 98);

    if ( a4 )
        v5 = " ";

    if ( a3 )
    {
        FontUA::set_flag(cur, 0x10);
    }

    sub_451714(yw->_guiTiles[v11], cur, v5, dword_5C8B88 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);

    FontUA::set_yheight(cur, dword_5C8B84);

    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::op17(cur, dword_5C8B80 + dword_5C8B88);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub6__sub3(NC_STACK_ypaworld *yw, int a2, int a4)
{
    exit_menu.itemBlock.clear();
    exit_menu.ItemsPreLayout(yw, &exit_menu.itemBlock, 0, "{ }");

    const SDL_Color boxTextColor = yw->GetFactionBoxTextColor();
    FontUA::set_txtColor(&exit_menu.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x100, a2 & 0x100, Locale::Text::Common(Locale::CMN_EXITMISN));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x200, a2 & 0x200, Locale::Text::Common(Locale::CMN_SAVE));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x400, a2 & 0x400, Locale::Text::Common(Locale::CMN_LOAD));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x800, a2 & 0x800, Locale::Text::Common(Locale::CMN_RESTART));
    sub_4DA8DC(yw, &exit_menu.itemBlock, a4 & 0x1000, a2 & 0x1000, Locale::Text::Common(Locale::CMN_QUITGAME));

    exit_menu.ItemsPostLayout(yw, &exit_menu.itemBlock, 0, "xyz");

    FontUA::set_end(&exit_menu.itemBlock);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub6(TInputState *inpt)
{
    if ( exit_menu.IsClosed() )
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PLAYING )
        {
            if ( _levelInfo.State == TLevelInfo::STATE_SAVE )
            {
                yw_arg159 v18;
                v18.unit = 0;
                v18.Priority = 10;
                v18.txt = Locale::Text::Feedback(Locale::FEEDBACK_GAMESAVED);
                v18.MsgID = 0;

                ypaworld_func159(&v18);
            }

            _levelInfo.State = TLevelInfo::STATE_PLAYING;
        }
    }
    else
    {
        if ( _levelInfo.State != TLevelInfo::STATE_PAUSED )
        {
            _levelInfo.State = TLevelInfo::STATE_PAUSED;

            if ( _GameShell )
            {
                _hasSGMSave = UserData::IsHasSGM(_GameShell->UserName, 0);

                if ( UserData::IsHasRestartForLevel(_GameShell->UserName, _levelInfo.LevelID) )
                    _hasRSTSave = true;
                else
                    _hasRSTSave = false;
            }
        }
    }

    if ( ypaworld_func64__sub7__sub6__sub1() == &exit_menu )
    {
        int v7 = sub_4C885C();

        if ( v7 == 1 )
        {
            switch ( dword_5C8B78 )
            {
            case 8:
                _levelInfo.State = TLevelInfo::STATE_ABORTED;

                if ( _isNetGame )
                    NetSendExitMsg(1);

                break;

            case 9:
                _levelInfo.State = TLevelInfo::STATE_SAVE;
                break;

            case 10:
                _levelInfo.State = TLevelInfo::STATE_LOAD;
                break;

            case 11:
                _levelInfo.State = TLevelInfo::STATE_RESTART;
                break;

            case 14:
                _levelInfo.State = TLevelInfo::STATE_QUIT_GAME;

                if ( _isNetGame )
                    NetSendExitMsg(1);

                break;

            case 13:
                _helpURL = _helpSavedURL;
                _helpSavedURL.clear();
                break;

            default:
                break;
            }

            dword_5BAF9C = 0;
        }
        else if ( v7 == 2 )
        {
            dword_5BAF9C = 0;
        }
    }

    if ( exit_menu.IsClosed() )
    {
        exit_menu.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int a2 = 0;
        int a4 = 0;

        if ( !_hasSGMSave )
            a4 = 1024;

        if ( !_hasRSTSave )
            a4 |= 0x800;

        if ( _userUnit->_status == BACT_STATUS_DEAD )
            a4 |= 0x200;

        if ( _isNetGame )
            a4 |= 0xE00;

        if ( inpt->ClickInf.selected_btn == &exit_menu )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            switch ( inpt->ClickInf.selected_btnID )
            {
            case 7:
                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    sub_449DE8(Locale::Text::Help(Locale::HELP_GAMEMENU));

                SetShowingTooltip(Locale::TIP_ONLINEHELP);
                break;

            case 8:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a2 |= 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    dword_5C8B78 = 8;

                    if ( sub_4C885C() != 3 )
                        GuiWinClose( &exit_menu );

                    sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYEXIT) , &exit_menu);
                }
                break;

            case 9:
                if ( !(a4 & 0x200) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x200;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 9;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYSAVE) , &exit_menu);
                    }
                }
                break;

            case 10:
                if ( !(a4 & 0x400) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x400;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 10;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc( Locale::Text::Advanced(Locale::ADV_RLYLOAD) , &exit_menu);
                    }
                }
                break;

            case 11:
                if ( !(a4 & 0x800) )
                {
                    if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                        a2 |= 0x800;

                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                    {
                        dword_5C8B78 = 11;

                        if ( sub_4C885C() != 3 )
                            GuiWinClose( &exit_menu );

                        sb_0x4c87fc(Locale::Text::Advanced(Locale::ADV_RLYRESTART) , &exit_menu);
                    }
                }
                break;

            case 12:
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a2 |= 0x1000;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    dword_5C8B78 = 14;

                    if ( sub_4C885C() != 3 )
                        GuiWinClose( &exit_menu );

                    sb_0x4c87fc(Locale::Text::Advanced(Locale::ADV_RLYQUITGAME), &exit_menu);
                }
                break;

            default:
                break;
            }
        }
        exit_menu.InputHandle(this, inpt);
        exit_menu.Formate(this);

        ypaworld_func64__sub7__sub6__sub3(this, a2, a4);
    }
}

void sub_4C8534(NC_STACK_ypaworld *yw, CmdStream *cur, const std::string &a2)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    sub_451714(yw->_guiTiles[0], cur, a2, lstvw2.entryWidth - 2 * dword_5BAFA8, 32);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void ypaworld_func64__sub7__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3)
{
    FontUA::store_u8(cur, 123);
    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    int v8;

    if ( a3 & 0x100 )
    {
        FontUA::set_flag(cur, 0x10);
        v8 = 8;
    }
    else
    {
        v8 = 3;
    }

    FontUA::select_tileset(cur, v8);

    FontUA::store_u8(cur, 98);

    sub_451714(yw->_guiTiles[v8], cur, Locale::Text::Common(Locale::CMN_OK), dword_5BAFA0 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::op10(cur, dword_5BAFA4);

    FontUA::store_u8(cur, 32);

    if ( a3 & 0x200 )
    {
        FontUA::set_flag(cur, 0x10);
        v8 = 8;
    }
    else
    {
        v8 = 3;
    }

    FontUA::select_tileset(cur, v8);

    FontUA::store_u8(cur, 98);

    sub_451714(yw->_guiTiles[v8], cur, Locale::Text::Common(Locale::CMN_CANCEL), dword_5BAFA0 - 2 * yw->_fontBorderW, 32);

    FontUA::unset_flag(cur, 0x10);

    FontUA::store_u8(cur, 100);

    FontUA::select_tileset(cur, 0);

    FontUA::store_u8(cur, 32);
    FontUA::store_u8(cur, 32);

    FontUA::store_u8(cur, 125);

    FontUA::next_line(cur);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub4__sub0(int a2)
{
    lstvw2.itemBlock.clear();
    lstvw2.ItemsPreLayout(this, &lstvw2.itemBlock, 0, "{ }");

    const SDL_Color boxTextColor = GetFactionBoxTextColor();
    FontUA::set_txtColor(&lstvw2.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    sub_4C8534(this, &lstvw2.itemBlock, dword_5BAF98);
    sub_4C8534(this, &lstvw2.itemBlock, " ");

    FontUA::set_txtColor(&lstvw2.itemBlock, boxTextColor.r,
                         boxTextColor.g, boxTextColor.b);

    ypaworld_func64__sub7__sub4__sub0__sub0(this, &lstvw2.itemBlock, a2);
    lstvw2.ItemsPostLayout(this, &lstvw2.itemBlock, 0, "xyz");

    FontUA::set_end(&lstvw2.itemBlock);
}

void NC_STACK_ypaworld::ypaworld_func64__sub7__sub4(TInputState *inpt)
{
    if ( lstvw2.IsClosed() )
    {
        lstvw2.InputHandle(this, inpt);
    }
    else
    {
        TClickBoxInf *winpt = &inpt->ClickInf;

        int a1_1 = 0;

        if ( _kbdLastKeyHit == Input::KC_RETURN )
        {
            GuiWinClose( &lstvw2 );

            dword_5BAF9C = 1;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( _kbdLastKeyHit == Input::KC_ESCAPE )
        {
            GuiWinClose( &lstvw2 );

            dword_5BAF9C = 2;

            inpt->KbdLastHit = Input::KC_NONE;
            inpt->KbdLastDown = Input::KC_NONE;
            inpt->HotKeyID = -1;
        }
        else if ( inpt->ClickInf.selected_btn == &lstvw2 )
        {
            if ( _GameShell )
            {
                if ( inpt->ClickInf.selected_btnID >= 8 )
                {
                    if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                }
            }

            if ( winpt->selected_btnID == 8 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a1_1 = 0x100;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    dword_5BAF9C = 1;
                }
            }
            else if ( winpt->selected_btnID == 9 )
            {
                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                    a1_1 = 0x200;

                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                {
                    GuiWinClose( &lstvw2 );
                    dword_5BAF9C = 2;
                }
            }
        }

        lstvw2.InputHandle(this, inpt);
        lstvw2.Formate(this);
        ypaworld_func64__sub7__sub4__sub0(a1_1);
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub7(TInputState *inpt)
{
    TClickBoxInf *winpt = &inpt->ClickInf;

    if ( _userUnit->_bact_type != BACT_TYPES_MISSLE )
    {
        if ( winpt->flag & TClickBoxInf::FLAG_OK )
        {
            for(GuiBaseList::iterator it = _guiActive.begin(); it != _guiActive.end(); it++)
            {
                GuiBase *llst = *it;

                if ( llst->flags & GuiBase::FLAG_WITH_ICON )
                {
                    if ( llst->flags & GuiBase::FLAG_ICONIFED )
                        llst->iconString[2] = 1;
                }

                if ( llst->flags & GuiBase::FLAG_WITH_CLOSE )
                {
                    if ( !( llst->flags & (GuiBase::FLAG_ICONIFED | GuiBase::FLAG_CLOSED) ) )
                        llst->flags &= ~GuiBase::FLAG_CLOSE_DOWN;
                }
            }

            if ( inpt->ClickInf.selected_btn )
            {
                GuiBase *v9 = (GuiBase *)inpt->ClickInf.selected_btn->pobject;
                if ( v9 )
                {
                    if ( v9->flags & 1 )
                    {
                        if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                            v9->iconString[2] = 2;

                        if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                        {
                            v9->flags &= 0xFFFFFFFE;

                            Input::Engine.RemClickBox(&v9->iconBox);
                            Input::Engine.AddClickBoxFront(v9);

                            v9->Attach(_guiActive);
                        }
                    }
                    else
                    {
                        if ( winpt->flag & TClickBoxInf::FLAG_LM_DOWN )
                        {
                            Input::Engine.RemClickBox(v9);
                            Input::Engine.AddClickBoxFront(v9);

                            v9->Attach(_guiActive);
                        }

                        if ( v9->flags & 0x10 )
                        {
                            if ( winpt->selected_btnID == 0)
                            {
                                if ( winpt->flag & (TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD) )
                                    v9->flags |= 0x40;

                                if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                                {
                                    if ( _GameShell )
                                        SFXEngine::SFXe.startSound(&_GameShell->samples1_info, 3);
                                }

                                if ( winpt->flag & TClickBoxInf::FLAG_BTN_UP )
                                {
                                    Input::Engine.RemClickBox(v9);

                                    if ( v9->flags & 4 )
                                    {
                                        Input::Engine.AddClickBoxFront(&v9->iconBox);

                                        v9->flags |= 1;
                                    }
                                    else
                                    {
                                        v9->flags |= 0x20;
                                    }

                                    v9->Attach(_guiActive);
                                }
                            }
                        }

                        if ( v9->flags & 8 )
                        {
                            if ( winpt->flag & TClickBoxInf::FLAG_BTN_DOWN )
                            {
                                if ( winpt->selected_btnID == 1 )
                                {
                                    _guiDragDefaultMouse = true;
                                    _guiDragElement = v9;
                                    _guiDragging = true;
                                    _guiDragPos.x = winpt->ldw_pos.BoxPos.x;
                                    _guiDragPos.y = winpt->ldw_pos.BoxPos.y;
                                }
                            }
                        }
                    }
                }
            }

            if ( _guiDragging )
            {
                if ( winpt->flag & TClickBoxInf::FLAG_LM_HOLD )
                {
                    const int rawX = winpt->move.ScreenPos.x - _guiDragPos.x;
                    const int rawY = winpt->move.ScreenPos.y - _guiDragPos.y;
                    int v13 = rawX;
                    int v15 = rawY;

                    if ( v13 >= 0 )
                    {
                        if ( _guiDragElement->w + v13 > _screenSize.x )
                            v13 = _screenSize.x - _guiDragElement->w;
                    }
                    else
                    {
                        v13 = 0;
                    }

                    if ( v15 >= _upScreenBorder )
                    {
                        if ( _guiDragElement->h + v15 > _screenSize.y - _downScreenBorder )
                            v15 = _screenSize.y - _downScreenBorder - _guiDragElement->h;
                    }
                    else
                    {
                        v15 = _upScreenBorder;
                    }

                    // Map/Squadron keep the mouse attached to the dragged title
                    // even after the window reaches a screen limit. Rebase the
                    // grab offset at the clamp so reversing direction moves the
                    // window immediately instead of crossing a dead zone first.
                    if ( _guiDragElement == &robo_map ||
                         _guiDragElement == &squadron_manager )
                    {
                        if ( v13 != rawX )
                            _guiDragPos.x = winpt->move.ScreenPos.x - v13;
                        if ( v15 != rawY )
                            _guiDragPos.y = winpt->move.ScreenPos.y - v15;
                    }

                    _guiDragElement->y = v15;
                    _guiDragElement->x = v13;

                    int16_t tmp = v13 - (_screenSize.x / 2);
                    _guiDragElement->cmdCommands[5] = (tmp >>  8) & 0xFF ;
                    _guiDragElement->cmdCommands[6] = tmp & 0xFF ;

                    tmp = v15 - (_screenSize.y / 2);
                    _guiDragElement->cmdCommands[9] = (tmp >>  8) & 0xFF ;
                    _guiDragElement->cmdCommands[10] = tmp & 0xFF ;
                }
            }

            if ( _guiDragging )
            {
                if ( !(winpt->flag & TClickBoxInf::FLAG_LM_HOLD) )
                {
                    _guiDragDefaultMouse = false;
                    _guiDragging = false;
                }
            }

            if ( _isNetGame )
                ypaworld_func64__sub7__sub5(this, inpt);

            ypaworld_func64__sub7__sub4(inpt);
            ypaworld_func64__sub7__sub6(inpt);
            RoboMap_InputHandle(this, inpt);
            SquadManager_InputHandle(inpt);
            ypaworld_func64__sub7__sub0(this, inpt);
            ypaworld_func64__sub7__sub7(this, inpt);
        }

        // The tactical map input handler runs first and consumes wheel/+/-
        // whenever the map is open. Spectator Follow then receives the same
        // contextual controls here. The controlled Spectator UFO keeps its
        // remaining class-level input for the normal User_layer below. Keeping
        // this call outside FLAG_OK also preserves distance interpolation on
        // frames without a click-box event.
        UpdateSpectatorFollowCamera(inpt);

        yw_WorldSelectionDragInput(this, inpt);
        ypaworld_func64__sub7__sub2(this, inpt);
    }
    else
    {
        yw_ResetWorldSelectionDrag(this);
    }
}

NC_STACK_ypabact * NC_STACK_ypaworld::GetLastMsgSender()
{
    if ( !info_log.field_255C )
        return nullptr;

    for (NC_STACK_ypabact *comm : _cmdrsRemap)
    {
        NC_STACK_ypabact *found = nullptr;

        if ( info_log.field_255C == comm->_gid )
        {
            found = comm;
        }
        else
        {
            for (NC_STACK_ypabact *bact : comm->_kidList)
            {
                if ( bact->_status != BACT_STATUS_DEAD &&
                     bact->_status != BACT_STATUS_CREATE &&
                     bact->_status != BACT_STATUS_BEAM &&
                     !bact->ShouldHideFromStrategicUI() &&
                     bact->_gid == info_log.field_255C )
                {
                    found = bact;
                    break;
                }
            }
        }

        if (found)
        {
            if ( (_timeStamp - info_log.field_2560) > 10000 )
                return nullptr;

            return found;
        }
    }

    return nullptr;
}

bool sub_4C3E34(NC_STACK_ypabact* const &a1, NC_STACK_ypabact* const &a2)
{
    return (a1->_commandID & 0xFFFFFF) < (a2->_commandID & 0xFFFFFF);
}

void NC_STACK_ypaworld::sub_4C40AC()
{
    _cmdrsRemap.clear();
    _cmdrsRemap.reserve(128);

    if ( IsSpectatorControlled() )
    {
        _activeCmdrID = 0;
        _activeCmdrRemapIndex = -1;
        _activeCmdrKidsCount = 0;
        _cmdrIdToSelect = -1;
        _lastMsgSender = GetLastMsgSender();
        return;
    }

    if ( _userRobo )
    {
        for ( NC_STACK_ypabact * &bact : _userRobo->GetKidList())
        {
            if (bact->_status != BACT_STATUS_DEAD && bact->_status != BACT_STATUS_BEAM && bact->_bact_type != BACT_TYPES_GUN)
                _cmdrsRemap.push_back(bact);
        }
    }

    std::sort(_cmdrsRemap.begin(), _cmdrsRemap.end(), sub_4C3E34);

    if ( !_cmdrsRemap.empty() )
    {
        bool selected = false;
        if (_cmdrIdToSelect != -1)
        {
            for (size_t i = 0; i < _cmdrsRemap.size(); i++)
            {
                if (_cmdrIdToSelect == (int32_t)_cmdrsRemap[i]->_commandID)
                {
                    _activeCmdrRemapIndex = i;
                    _activeCmdrID = _cmdrIdToSelect;
                    selected = true;
                    break;
                }
            }
        }

        if (!selected)
        {
            if ( _activeCmdrID == 0 )
            {
                if ( bzda.field_1D0 & 6 )
                {
                    _activeCmdrID = _cmdrsRemap.back()->_commandID;
                    _activeCmdrRemapIndex = _cmdrsRemap.size() - 1;
                }
            }
            else
            {
                _activeCmdrRemapIndex = -1;

                for (size_t i = 0; i < _cmdrsRemap.size(); i++)
                {
                    if (_activeCmdrID == _cmdrsRemap[i]->_commandID)
                    {
                        _activeCmdrRemapIndex = i;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        _activeCmdrRemapIndex = -1;
    }

    _cmdrIdToSelect = -1;

    if ( _activeCmdrRemapIndex == -1 )
    {
        _activeCmdrKidsCount = 0;
    }
    else
    {
        _activeCmdrKidsCount = 0;
        for ( NC_STACK_ypabact* &bact : _cmdrsRemap[ _activeCmdrRemapIndex ]->_kidList )
        {
            if (bact->_status != BACT_STATUS_DEAD && bact->_status != BACT_STATUS_BEAM && bact->_bact_type != BACT_TYPES_GUN)
                _activeCmdrKidsCount++;
        }
    }

    _lastMsgSender = GetLastMsgSender();
}

void ypaworld_func64__sub14(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        sub_4C157C(yw);
        sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
        sub_4C0C00(yw);
        sub_4C0FEC(yw);
        sub_4C1214(yw);
        MapRoboUpdateMapInt(yw);
    }
}

void ypaworld_func64__sub8(NC_STACK_ypaworld *yw)
{
    if ( robo_map.IsOpen() )
    {
        if ( !(robo_map.field_1E8 & 0x16) && robo_map.field_1ED == 1 )
        {
            if ( yw->_viewerBact )
            {
                robo_map.field_1D8 = yw->_viewerBact->_position.x;
                robo_map.field_1DC = yw->_viewerBact->_position.z;

                sub_4C1814(yw, robo_map.field_1CC - robo_map.field_244, robo_map.field_1D2);
            }
        }
        robo_map.field_1E8 &= 0xFFFFFF1F;
    }
}

// OpenNeoUA: Genesis list order switch. Only an explicit energy value
// selects the Energy order; missing or unknown values keep vanilla.
bool yw_IsGenesisEnergyOrder()
{
    return !StriCmp(System::IniConf::UiGenesisListOrder.Get<std::string>(), "energy");
}

int sub_4C3C88(const void *a1, const void *a2)
{
    uint8_t aa1 = *(const uint8_t *)a1;
    uint8_t aa2 = *(const uint8_t *)a2;

    const World::TVhclProto &v4 = dword_5BAA60->_vhclProtos[aa1];
    const World::TVhclProto &v5 = dword_5BAA60->_vhclProtos[aa2];

    // OpenNeoUA: Genesis list order follows ui.genesis_list_order.
    // Energy sorts by final production cost only; missing or unknown
    // values keep the vanilla type-grouped order.
    if ( yw_IsGenesisEnergyOrder() )
    {
        const int cost1 = v4.GetProductionCost();
        const int cost2 = v5.GetProductionCost();

        if ( cost1 < cost2 )
            return -1;
        else if ( cost1 > cost2 )
            return 1;

        return 0;
    }

    int v7, v8;

    switch ( v4.model_id )
    {
    case BACT_TYPES_BACT:
        v7 = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        v7 = 0;
        break;

    case BACT_TYPES_ROBO:
        v7 = 5;
        break;

    case BACT_TYPES_FLYER:
        v7 = 2;
        break;

    case BACT_TYPES_UFO:
        v7 = 3;
        break;

    case BACT_TYPES_GUN:
        v7 = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        v7 = 10;
        break;

    default:
        v7 = 0;
        break;
    }

    switch ( v5.model_id )
    {
    case BACT_TYPES_BACT:
        v8 = 1;
        break;

    case BACT_TYPES_TANK:
    case BACT_TYPES_CAR:
        v8 = 0;
        break;

    case BACT_TYPES_ROBO:
        v8 = 5;
        break;

    case BACT_TYPES_FLYER:
        v8 = 2;
        break;

    case BACT_TYPES_UFO:
        v8 = 3;
        break;

    case BACT_TYPES_GUN:
        v8 = 4;
        break;

    case BACT_TYPES_MISSLE:
    case BACT_TYPES_ZEPP:
        v8 = 10;
        break;

    default:
        v8 = 0;
        break;
    }

    if ( v7 < v8 )
        return -1;
    else if ( v7 > v8)
        return 1;
    else if ( v7 == v8 )
    {
        if ( v4.energy > v5.energy )
            return 1;
        else if (v4.energy < v5.energy)
            return -1;
    }

    return 0;
}

void ypaworld_func64__sub16(NC_STACK_ypaworld *yw)
{
    int v6 = 0;

    if ( yw->_userRobo )
        v6 = 1 << yw->_userRobo->_owner;

    int v3 = 0;
    int i = 0;
    for (World::TVhclProto &proto : yw->_vhclProtos)
    {
        if ( v6 & proto.disable_enable_bitmask )
        {
            bzda.field_2DC[v3] = i;
            v3++;
        }
        i++;
    }

    bzda.field_8E0 = v3;

    dword_5BAA60 = yw;

    qsort(bzda.field_2DC, v3, 1, sub_4C3C88);

    if ( !v3 )
    {
        bzda.field_8EC = -1;
    }
    else
    {
        if ( v3 == 1 )
        {
            bzda.field_8EC = 0;
        }
        else if ( v3 <= bzda.field_8EC )
        {
            bzda.field_8EC = v3 - 1;
        }
    }
}

int sub_4C3D6C(const void *a1, const void *a2)
{

    uint8_t aa1 = *(const uint8_t *)a1;
    uint8_t aa2 = *(const uint8_t *)a2;

    World::TBuildingProto *v3 = &dword_5BAA60->_buildProtos[aa1];
    World::TBuildingProto *v4 = &dword_5BAA60->_buildProtos[aa2];

    // OpenNeoUA: same ui.genesis_list_order switch as the vehicle list.
    if ( yw_IsGenesisEnergyOrder() )
    {
        const int cost1 = v3->GetProductionCost();
        const int cost2 = v4->GetProductionCost();

        if ( cost1 < cost2 )
            return -1;
        else if ( cost1 > cost2 )
            return 1;

        return 0;
    }

    int v6, v8;

    if ( v3->ModelID == 1 )
        v6 = 0;
    else if ( v3->ModelID == 2 )
        v6 = 2;
    else if ( v3->ModelID == 3 )
        v6 = 1;
    else
        v6 = 10;

    if ( v4->ModelID == 1 )
        v8 = 0;
    else if ( v4->ModelID == 2 )
        v8 = 2;
    else if ( v4->ModelID == 3 )
        v8 = 1;
    else
        v8 = 10;

    if ( v6 >= v8 )
    {
        if ( v6 <= v8 )
        {
            if ( v3->Energy >= v4->Energy )
                return v3->Energy > v4->Energy;
            else
                return -1;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}

void ypaworld_func64__sub17(NC_STACK_ypaworld *yw)
{
    int v6 = 0;

    if ( yw->_userRobo )
        v6 = 1 << yw->_userRobo->_owner;

    int v3 = 0;

    int i = 0;
    for (const World::TBuildingProto &proto : yw->_buildProtos)
    {
        if ( v6 & proto.EnableMask )
        {
            bzda.field_3DC[v3] = i;
            v3++;
        }
        i++;
    }

    bzda.field_8E4 = v3;

    dword_5BAA60 = yw;

    qsort(bzda.field_3DC, v3, 1, sub_4C3D6C);

    if ( !v3 )
    {
        bzda.field_8F4 = -1;
    }
    else
    {
        if ( v3 == 1 )
        {
            bzda.field_8F4 = 0;
        }
        else if ( v3 <= bzda.field_8F4 )
        {
            bzda.field_8F4 = v3 - 1;
        }
    }
}

void ypaworld_func64__sub15(NC_STACK_ypaworld *yw)
{
    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

    int v4 = 0;
    int i = 0;

    for(World::TRoboGun &gun : robo->GetGuns())
    {
        if ( gun.gun_obj )
        {
            bzda.field_1DC[v4] = i;

            if ( gun.gun_obj == yw->_userUnit )
                bzda.field_8F0 = v4;

            v4++;
        }
        i++;
    }

    bzda.field_8DC = v4;

    if ( !v4 )
    {
        bzda.field_8F0 = -1;
    }
    else
    {
        if ( v4 == 1 )
        {
            bzda.field_8F0 = 0;
        }
        else if ( v4 <= bzda.field_8F0 )
        {
            bzda.field_8F0 = v4 - 1;
        }
    }
}

void ypaworld_func2__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2)
{
    if ( bact2->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_roboMapStatus.Valid )
        {
            if ( yw->_roboMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_roboMapStatus.Rect.x;
            robo_map.y = yw->_roboMapStatus.Rect.y;
            robo_map.w = yw->_roboMapStatus.Rect.w;
            robo_map.h = yw->_roboMapStatus.Rect.h;
            robo_map.field_1EC = yw->_roboMapStatus.Data[0];
            robo_map.field_1ED = yw->_roboMapStatus.Data[1];
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
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            yw_ApplySquadronManagerSavedRect(yw, yw->_roboFinderStatus.Rect);
        }
    }
    else if ( bact1->_bact_type == BACT_TYPES_ROBO )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
            else
                yw->GuiWinClose( &robo_map );

            robo_map.x = yw->_vhclMapStatus.Rect.x;
            robo_map.y = yw->_vhclMapStatus.Rect.y;
            robo_map.w = yw->_vhclMapStatus.Rect.w;
            robo_map.h = yw->_vhclMapStatus.Rect.h;
            robo_map.field_1EC = yw->_vhclMapStatus.Data[0];
            robo_map.field_1ED = yw->_vhclMapStatus.Data[1];
            robo_map.field_208 = yw->_vhclMapStatus.Data[3];
            robo_map.field_20A = yw->_vhclMapStatus.Data[4];
            robo_map.field_20C = yw->_vhclMapStatus.Data[5];
            robo_map.field_20E = yw->_vhclMapStatus.Data[6];

            sub_4C1970(yw, 0);
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
            else
                yw->GuiWinClose( &squadron_manager );

            yw_ApplySquadronManagerSavedRect(yw, yw->_vhclFinderStatus.Rect);
        }
    }
    else if ( bact1->_status == BACT_STATUS_DEAD )
    {
        if ( yw->_vhclMapStatus.Valid )
        {
            if ( yw->_vhclMapStatus.IsOpen )
            {
                yw->GuiWinOpen( &robo_map );
                yw->GuiWinToFront( &robo_map );
            }
        }

        if ( yw->_vhclFinderStatus.Valid )
        {
            if ( yw->_vhclFinderStatus.IsOpen && !yw->IsSpectatorControlled() )
            {
                yw->GuiWinOpen( &squadron_manager );
                yw->GuiWinToFront( &squadron_manager );
            }
        }

        if ( bact2->_bact_type != BACT_TYPES_ROBO )
            yw->_invulnerable = 1;
    }

    if ( bact2->_bact_type != BACT_TYPES_ROBO )
        robo_map.field_1ED = 1;
}

int NC_STACK_ypaworld::sb_0x4d3d44(TClickBoxInf *winp)
{
    if ( _guiActFlags & 8 || bzda.field_8EC == -1 )
        return 4;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    int a4 = robo->getROBO_battVehicle();

    if ( _updateMessage.energy > a4 )
        return 2;

    const int vehicleId = bzda.field_2DC[bzda.field_8EC];
    if ( _userRobo && IsVehicleProductionLimitReached(_userRobo->_owner, vehicleId) )
        return 3;

    if ( !yw_MouseFindCreationPoint(winp) )
        return 1;

    if ( _isNetGame && !_levelUnitLimitType && _levelUnitLimit <= _countUnitsPerOwner[_userRobo->_owner] )
        return 3;

    return 0;
}

int NC_STACK_ypaworld::UserActBuildCheck()
{
    if ( !(_guiActFlags & 0x10) || bzda.field_8F4 == -1 )
        return 1; // NO with no tip

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    int a4 = robo->getROBO_battVehicle();

    if ( _updateMessage.energy > a4 )
        return 6; // TIP_NOENERGY

    if ( _cellOnMouse->owner && _cellOnMouse->owner != _userRobo->_owner )
        return 2; // TIP_NOTOWN

    Common::Point cellDist = _cellOnMouse->CellId.AbsDistance( _userRobo->_cellId );

    if ( cellDist.IsNull() )
        return 3; // TIP_BUILD_TOOCLOSE

    if ( cellDist.x > 1 || cellDist.y > 1 )
        return 4; // TIP_BUILD_TOOFAR

    if ( _cellOnMouse->PurposeType == cellArea::PT_CONSTRUCTING ||
         (!_allowMultiBuildLevel && IsAnyBuildingProcess(_playerOwner)) )
        return 5; // TIP_BUILD_INPROC

    if ( _cellOnMouse->PurposeType == cellArea::PT_TECHUPGRADE ||
         _cellOnMouse->PurposeType == cellArea::PT_GATECLOSED ||
         _cellOnMouse->PurposeType == cellArea::PT_GATEOPENED ||
         _cellOnMouse->PurposeType == cellArea::PT_STOUDSON ||
         (_cellOnMouse->PurposeType == cellArea::PT_TECHDEACTIVE && _isNetGame) )
        return 1; // NO with no tip

    return 0; // Can build
}

int NC_STACK_ypaworld::ypaworld_func64__sub21__sub3()
{
    if ( !(_guiActFlags & 0x10) )
        return 6;

    if ( !_cellOnMouse->IsCanSee(_userRobo->_owner) )
        return 3;

    if ( _cellOnMouse->PurposeType == cellArea::PT_CONSTRUCTING )
        return 6;

    float v5 = _cellMouseIsectPos.x - _userRobo->_position.x;
    float v6 = _cellMouseIsectPos.y - _userRobo->_position.y;
    float v8 = _cellMouseIsectPos.z - _userRobo->_position.z;

    float v15 = sqrt(POW2(v5) + POW2(v6) + POW2(v8));

    if ( v15 < 100.0 )
        return 4;

    int v18 = 0;

    NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

    bool playerRoboAIBehavior = robo && robo->IsPlayerRoboAIBehavior();

    if ( playerRoboAIBehavior ||
         (_cellOnMouse->PurposeType != cellArea::PT_GATEOPENED && (_cellOnMouse->PurposeType != cellArea::PT_POWERSTATION || _cellOnMouse->owner != _userRobo->_owner)) )
    {
        v18 = (POW2(v15) / 230.4);
    }

    if ( IsDebugHostStationCheatEnabled() )
        v18 = 0;

    int a4 = robo->getROBO_battBeam();

    _updateMessage.energy = v18;

    int mobileEnergyCost = v18;
    if ( playerRoboAIBehavior )
        mobileEnergyCost = NC_STACK_yparobo::ScalePlayerMobileMoveEnergyCost(v18);

    if ( mobileEnergyCost > a4 )
        return 1;

    for ( NC_STACK_ypabact* &v11 : _cellOnMouse->unitsList )
    {
        if ( v11->_bact_type == BACT_TYPES_ROBO && v11->_status != BACT_STATUS_DEAD && v11->_owner != _userRobo->_owner )
            return 5;
    }

    float v20 = 100000.0;
    float v19 = -100000.0;

    ypaworld_arg136 arg136;
    arg136.vect.x = 0;
    arg136.vect.y = 50000.0;
    arg136.vect.z = 0;
    arg136.flags = 0;

    for (int j = -3; j <= 3; j++)
    {
        for (int i = -3; i <= 3; i++)
        {
            arg136.stPos.y = -25000.0;
            arg136.stPos.x = _cellMouseIsectPos.x - j * 57.14285714285715;
            arg136.stPos.z = _cellMouseIsectPos.z - i * 57.14285714285715;

            ypaworld_func136(&arg136);

            if ( arg136.isect )
            {
                if ( arg136.isectPos.y < v20 )
                    v20 = arg136.isectPos.y;

                if ( arg136.isectPos.y > v19 )
                    v19 = arg136.isectPos.y;
            }
        }
    }

    if ( abs(v19 - v20) > 800 )
        return 2;

    if ( _cellMouseIsectPos.x < 1500.0 || _cellMouseIsectPos.z > -1500.0 || _cellMouseIsectPos.x > _mapLength.x - 1500.0 || _cellMouseIsectPos.z < 1500.0 - _mapLength.y )
        return 2;

    _cellMouseIsectPos.y = v20 - _userRobo->_height;
    return 0;
}

int NC_STACK_ypaworld::ypaworld_func64__sub21__sub4(TInputState *arg, int a3)
{
    const bool waypointInputHeld =
        arg->Buttons.Is(4) && !IsPlayerSprintInputHeld();

    if ( !_makingWaypointsMode && waypointInputHeld )
    {
        _makingWaypointsMode = true;
        _waypointCount = 0;
        return a3;
    }

    if ( _makingWaypointsMode && !waypointInputHeld )
    {
        _makingWaypointsMode = false;
        return a3;
    }

    if ( _makingWaypointsMode   &&   a3 == World::DOACTION_2 &&   arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
    {
        if ( _waypointCount )
        {
            if ( (_firstWaypointPosition.XZ() - _cellMouseIsectPos.XZ()).length() < 100.0 )
            {
                _waypointCount++;
                return World::DOACTION_WP_CYCLE;
            }

            _waypointCount++;

            return World::DOACTION_WP_ADD;
        }
        else
        {
            _firstWaypointPosition = _cellMouseIsectPos;

            _waypointCount++;

            return World::DOACTION_WP_BEGIN;
        }
    }

    return a3;
}

bool NC_STACK_ypaworld::ypaworld_func64__sub21__sub6(TClickBoxInf *winp)
{
    if ( winp->flag & TClickBoxInf::FLAG_DBL_CLICK
            && _bactPrevClicked
            && Common::ABS(winp->move.ScreenPos.x - _prevMousePos.x) < 4
            && Common::ABS(winp->move.ScreenPos.y - _prevMousePos.y) < 4 )
    {
        _prevMousePos = Common::Point();

        _bactOnMouse = _bactPrevClicked;
        _bactPrevClicked = nullptr;

        _guiActFlags |= 0x20;

        return true;
    }
    else
    {
        if ( (winp->flag & TClickBoxInf::FLAG_LM_DOWN)   &&   _guiActFlags & 0x20   &&   _userRobo->_owner == _bactOnMouse->_owner )
        {
            _bactPrevClicked = _bactOnMouse;
            _prevMousePos = winp->move.ScreenPos;
        }
        else if ( winp->flag & TClickBoxInf::FLAG_LM_DOWN )
        {
            if ( !(_guiActFlags & 0x20) )
            {
                _bactPrevClicked = nullptr;
                _prevMousePos = Common::Point();
            }
        }
    }
    return false;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub1__sub0(TInputState *arg)
{
    _guiActFlags = 0;

    TClickBoxInf *winp = &arg->ClickInf;

    if ( arg->ClickInf.flag & TClickBoxInf::FLAG_OK )
    {
        if ( winp->selected_btn )
        {
            _guiActFlags |= 4;

            if ( winp->selected_btn == &robo_map )
            {
                if ( winp->selected_btnID == 17 )
                    _guiActFlags |= 8;
            }
            else if ( winp->selected_btn == &squadron_manager )
            {
                if ( winp->selected_btnID >= 8 && (int32_t)(squadron_manager.firstShownEntries + winp->selected_btnID - 8) < squadron_manager.numEntries )
                    _guiActFlags |= 0x40;
            }
        }

        if ( bzda.field_1D0 == 8 )
            _guiActFlags |= 2;
        else if ( bzda.field_1D0 == 1 || bzda.field_1D0 == 2 || bzda.field_1D0 == 4 || bzda.field_1D0 == 16 || bzda.field_1D0 == 32 )
            _guiActFlags |= 0x83;
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub1__sub3__sub0(TClickBoxInf *winp)
{
    float v3 = (float)(winp->move.ScreenPos.x - (_screenSize.x / 2)) / (float)(_screenSize.x / 2);
    float v4 = (float)(winp->move.ScreenPos.y - (_screenSize.y / 2)) / (float)(_screenSize.y / 2);

    GFX::Engine.viewZoomCorrection(v3, v4, true);

    mat3x3 corrected = _viewerRotation;
    GFX::Engine.matrixAspectCorrection(corrected, true);

    float v16 = corrected.m00 * v3 + corrected.m10 * v4 + corrected.m20;
    float v17 = corrected.m01 * v3 + corrected.m11 * v4 + corrected.m21;
    float v18 = corrected.m02 * v3 + corrected.m12 * v4 + corrected.m22;

    float v14 = sqrt(POW2(v16) + POW2(v17) + POW2(v18));

    _mouseVector.x = v16 / v14;
    _mouseVector.y = v17 / v14;
    _mouseVector.z = v18 / v14;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub7()
{
    _updateMessage.energy = 0;

    if ( bzda.field_1D0 == 2 || bzda.field_1D0 == 4 )
    {
        if ( bzda.field_8EC != -1 )
        {
            _updateMessage.energy = dround(sub_4498F4() * (2 * _vhclProtos[bzda.field_2DC[bzda.field_8EC]].GetProductionCost()));
        }
    }
    else if ( bzda.field_1D0 == 16 && bzda.field_8F4 != -1 )
    {
        _updateMessage.energy = dround(sub_4498F4() * _buildProtos[bzda.field_3DC[bzda.field_8F4]].GetProductionCost());
    }

    // New Debug Shift+F11 keeps the normal authored prices visible in the Genesis
    // UI, but the actual player Host Station command is free. Zero the shared
    // action message here so affordability checks and the runtime use the same
    // authoritative cost instead of maintaining a parallel cheat path.
    if ( IsDebugHostStationCheatEnabled() )
        _updateMessage.energy = 0;
}

int sub_4D3C80(NC_STACK_ypaworld *yw)
{
    if ( !yw->_bactOnMouse )
        return 0;

    if ( yw->_bactOnMouse == yw->_userRobo )
        return 0;

    NC_STACK_ypabact *a4 = yw->_bactOnMouse;

    if ( !yw->_bactOnMouse || !yw->_bactOnMouse->_parent )
        return 0;

    if ( yw->_bactOnMouse->_parent != yw->_bactOnMouse->_host_station )
        a4 = yw->_bactOnMouse->_parent;

    if ( !a4 )
        return 0;

    yw->_bactOnMouse = a4;
    return 1;
}

void NC_STACK_ypaworld::ypaworld_func64__sub21__sub5(int arg)
{
    if ( IsSpectatorControlled() && arg != World::DOACTION_0 )
        return;

    switch ( arg )
    {
    case World::DOACTION_8:
        if ( _bactOnMouse->_bact_type != BACT_TYPES_GUN && _bactOnMouse != _userRobo )
        {
            // Resolve the clicked unit's squad commander without replacing the
            // actual world-space hover target. sub_4D3C80() mutates _bactOnMouse
            // to the commander, which makes the HP bar flash on the leader for
            // this frame before the next mouse raycast restores the clicked member.
            NC_STACK_ypabact *squadCommander = sub_4D3C3C(_bactOnMouse);
            if ( squadCommander )
            {
                _activeCmdrID = squadCommander->_commandID;
                sub_4C40AC();
            }
        }

        if ( _userRobo != _bactOnMouse )
            bzda.field_1D0 = bzda.field_1CC & 1;
        else
            bzda.field_1D0 = bzda.field_1CC & 0x20;
        break;

    case World::DOACTION_2:
        if (_activeCmdrRemapIndex >= 0)
        {
            NC_STACK_ypabact *orderedSquad = _cmdrsRemap[_activeCmdrRemapIndex];
            _updateMessage.user_action = World::DOACTION_2;
            _updateMessage.selectBact = orderedSquad;

            if ( _guiActFlags & 0x10 )
            {
                _updateMessage.target_Sect = _cellOnMouse;
                _updateMessage.target_point = _cellMouseIsectPos;
                _updateMessage.target_Bact = nullptr;

                if ( _cellOnMouse )
                {
                    // The move marker confirms the player's click, not sector
                    // ownership, fog state or distance from the ordered squad.
                    // Any valid world intersection accepted as a move order
                    // must therefore display the canonical move-order marker.
                    _moveOrderFeedbackActive = true;
                    _moveOrderFeedbackPos = _cellMouseIsectPos;
                    _moveOrderFeedbackStartTime = _timeStamp;
                }

                if ( orderedSquad )
                    yw_ClearAttackOrderFeedback(this, orderedSquad->_commandID,
                                                orderedSquad->_owner);
            }
            else
            {
                _updateMessage.target_Sect = 0;
                _updateMessage.target_Bact = _bactOnMouse;

                // OpenNeoUA custom: show a short animated feedback only for an
                // explicit attack order on an enemy unit. Sector attacks and
                // friendly-unit clicks do not use this marker.
                if ( _bactOnMouse &&
                     orderedSquad &&
                     _bactOnMouse != _userRobo &&
                     _bactOnMouse->_owner != _userRobo->_owner &&
                     _bactOnMouse->_status != BACT_STATUS_CREATE &&
                     _bactOnMouse->_status != BACT_STATUS_BEAM &&
                     _bactOnMouse->_status != BACT_STATUS_DEAD )
                {
                    yw_SetAttackOrderFeedback(this, orderedSquad, _bactOnMouse);
                }
                else if ( orderedSquad )
                {
                    yw_ClearAttackOrderFeedback(this, orderedSquad->_commandID,
                                                orderedSquad->_owner);
                }
            }

            bzda.field_1D0 = bzda.field_1CC & 1;
        }
        break;

    case World::DOACTION_WP_BEGIN:
    case World::DOACTION_WP_ADD:
    case World::DOACTION_WP_CYCLE:
        if (_activeCmdrRemapIndex >= 0)
        {
            _updateMessage.user_action = arg;
            _updateMessage.selectBact = _cmdrsRemap[_activeCmdrRemapIndex];

            if ( _guiActFlags & 0x30 )
            {
                _updateMessage.target_Sect = _cellOnMouse;
                _updateMessage.target_point = _cellMouseIsectPos;
                _updateMessage.target_Bact = nullptr;
            }
        }
        break;

    case World::DOACTION_ADD_UNIT1:
        _updateMessage.user_action = World::DOACTION_ADD_UNIT1;
        _updateMessage.protoID = bzda.field_2DC[bzda.field_8EC];
        _updateMessage.target_point = _mouseVector * _mouseSelDistance + _viewerPosition;

        bzda.field_1D0 = 4;

        _activeCmdrRemapIndex = -1;
        _activeCmdrID = 0;
        break;

    case World::DOACTION_ADD_UNIT2:
        if (_activeCmdrRemapIndex >= 0)
        {
            _updateMessage.user_action = World::DOACTION_ADD_UNIT2;
            _updateMessage.selectBact = _cmdrsRemap[_activeCmdrRemapIndex];

            _updateMessage.protoID = bzda.field_2DC[bzda.field_8EC];
            _updateMessage.target_point = _mouseVector * _mouseSelDistance + _viewerPosition;
        }
        break;

    case World::DOACTION_5:
        sb_0x4c66f8(this, _bactOnMouse, _viewerBact);

        if ( _bactOnMouse->_bact_type != BACT_TYPES_GUN && _bactOnMouse != _userRobo )
        {
            if ( sub_4D3C80(this) )
            {
                _activeCmdrID = _bactOnMouse->_commandID;
                sub_4C40AC();
            }
        }
        break;

    case World::DOACTION_6:
        _updateMessage.user_action = World::DOACTION_6;
        _updateMessage.protoID = bzda.field_3DC[bzda.field_8F4];
        _updateMessage.target_Sect = _cellOnMouse;
        _updateMessage.target_point = _cellMouseIsectPos;
        _updateMessage.target_Bact = nullptr;
        bzda.field_1D0 = bzda.field_1CC & 1;
        break;

    case World::DOACTION_10:
        _updateMessage.user_action = World::DOACTION_10;
        _updateMessage.target_Sect = _cellOnMouse;
        _updateMessage.target_point = _cellMouseIsectPos;
        _updateMessage.target_Bact = nullptr;
        bzda.field_1D0 = 1;
        break;

    case World::DOACTION_19:
        // OpenNeoUA custom: refuse to cycle the viewer into an artillery shell platform.
        if ( _bactOnMouse && _bactOnMouse->IsArtilleryShellPlatform() )
            break;
        _viewerBact->setBACT_viewer(false);
        _viewerBact->setBACT_inputting(false);
        _bactOnMouse->setBACT_viewer(true);
        break;

    default:
        return;
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub21(TInputState *arg)
{
    // OpenNeoUA custom: while the white artillery targeting preview is active,
    // RMB is first and foremost "cancel artillery aiming". Do this before the
    // vanilla FLAG_OK/viewer gate so the map-marker RMB binding cannot swallow it.
    if ( _userRobo && !IsSpectatorControlled() &&
         bzda.field_1D0 == 1 &&
         (_guiActFlags & 8) &&
         _artilleryShellManualGid &&
         (arg->ClickInf.flag & TClickBoxInf::FLAG_RM_DOWN) )
    {
        _artilleryShellManualGid = 0;
        _artilleryShellManualRadius = 0.0f;
        arg->ClickInf.flag &= ~TClickBoxInf::FLAG_RM_DOWN;
        return;
    }

    if ( _userRobo && (arg->ClickInf.flag & TClickBoxInf::FLAG_OK) && _viewerBact )
    {
        yw_MouseSelect(arg);
        ypaworld_func64__sub21__sub7();

        if ( !IsSpectatorControlled() && yw_TryAddClickedUnitToActiveSquad(this, arg) )
        {
            _bactPrevClicked = nullptr;
            _prevMousePos = Common::Point();
            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        if ( IsSpectatorControlled() &&
             (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) &&
             (_guiActFlags & 0x20) &&
             _bactOnMouse &&
             IsValidSpectatorFollowTarget(_bactOnMouse) )
        {
            const bool jumpIntoVehicleMode = (bzda.field_1D0 & 8) != 0;

            SetSpectatorFollowTarget(_bactOnMouse);

            // Match the vanilla one-shot control mode: after a valid unit is
            // selected, release the Jump into Vehicle button. Direct one-click
            // Spectator Follow remains available when the mode was not armed.
            if ( jumpIntoVehicleMode )
                bzda.field_1D0 = bzda.field_1CC & 1;

            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        if ( !IsSpectatorControlled() &&
             bzda.field_1D0 == 1 &&
             (_guiActFlags & 8) &&
             (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN) &&
             HandleArtilleryShellMapClick() )
        {
            arg->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
            return;
        }

        int tooltip = 0;
        int mousePointer = 0;
        int doAction = World::DOACTION_0;

        if ( _mouseGrabbed )
        {
            mousePointer = -1;
        }
        else if ( _guiDragDefaultMouse || ((_guiActFlags & 4) && !(_guiActFlags & 0x48)) )
        {
            mousePointer = 0;
        }
        else if ( robo_map.field_1E8 & 0x200 )
        {
            mousePointer = 2;
        }
        else
        {

            // The tactical map always permits marker placement, so its neutral
            // cursor is Pointer rather than the blocking Cancel cursor.
            mousePointer = (_guiActFlags & 8) ? 0 : 1;
            doAction = World::DOACTION_0;

            if ( bzda.field_1D0 == 1 )
            {
                if ( !(_guiActFlags & 0x10) )
                {
                    if ( (_guiActFlags & 0x20) )
                    {
                        if ( _userRobo->_owner == _bactOnMouse->_owner )
                        {
                            doAction = World::DOACTION_8;
                            mousePointer = 2;
                            tooltip = Locale::TIP_DO_SELECT;
                        }
                        else if ( _activeCmdrRemapIndex != -1 )
                        {
                            doAction = World::DOACTION_2;
                            mousePointer = 3;
                            tooltip = Locale::TIP_DO_ATKVHCL;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                        mousePointer = 3;
                    }
                }
            }
            else if ( bzda.field_1D0 == 2 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        switch ( sb_0x4d3d44(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT1;
                            mousePointer = 6;
                            tooltip = Locale::TIP_DO_NEWSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {
                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( _userRobo->_owner == _bactOnMouse->_owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if ( bzda.field_1D0 == 4 )
            {
                if ( !(_guiActFlags & 8) )
                {
                    if ( (_guiActFlags & 0x20) && _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else if ( _activeCmdrRemapIndex != -1 )
                    {
                        switch ( sb_0x4d3d44(&arg->ClickInf) )
                        {
                        case 0:
                            doAction = World::DOACTION_ADD_UNIT2;
                            mousePointer = 7;
                            tooltip = Locale::TIP_DO_ADDSQUAD;
                            break;

                        case 1:
                            tooltip = Locale::TIP_ERR_NOROOM;
                            break;

                        case 2:
                            tooltip = Locale::TIP_ERR_NOENERGY;
                            break;

                        case 3:
                            tooltip = Locale::TIP_UNITLIMIT;
                            break;

                        default:
                            break;
                        }
                    }
                }
                else if ( _activeCmdrRemapIndex != -1 )
                {

                    if ( !(_guiActFlags & 0x10) )
                    {
                        if ( _guiActFlags & 0x20 )
                        {
                            if ( _userRobo->_owner == _bactOnMouse->_owner )
                            {
                                doAction = World::DOACTION_8;
                                mousePointer = 2;
                                tooltip = Locale::TIP_DO_SELECT;
                            }
                            else
                            {
                                doAction = World::DOACTION_2;
                                mousePointer = 3;
                                tooltip = Locale::TIP_DO_ATKVHCL;
                            }
                        }
                    }
                    else if ( _userRobo->_owner == _cellOnMouse->owner && _cellOnMouse->IsCanSee(_userRobo->_owner) )
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 4;
                        tooltip = Locale::TIP_DO_MOVE;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKSECTOR;
                    }
                }
            }
            else if (bzda.field_1D0 == 8 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    // OpenNeoUA custom: artillery shell platforms cannot be possessed, so show no
                    // "take control" cursor/tooltip when hovering one (it would wrongly
                    // suggest the player can enter it).
                    if ( _bactOnMouse->IsArtilleryShellPlatform() )
                    {
                        // leave mousePointer at its neutral value: no control prompt
                    }
                    else if ( !System::IniConf::IsGameNewDebugEnabled() &&
                              arg->KbdLastDown == Input::KC_F7 &&
                              (arg->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD) )
                    {
                        doAction = World::DOACTION_19;
                        mousePointer = 8;
                    }
                    else if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_5;
                        mousePointer = 8;
                        tooltip = Locale::TIP_DO_CONTROL;
                    }
                }
            }
            else if ( bzda.field_1D0 == 16 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( UserActBuildCheck() )
                    {
                    case 0:
                        doAction = World::DOACTION_6;
                        mousePointer = 10;
                        tooltip = Locale::TIP_DO_BUILD;
                        break;

                    case 6:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_NOTOWNED;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_BUILD_TOOCLOSE;
                        break;

                    case 4:
                        tooltip = Locale::TIP_ERR_BUILD_TOOFAR;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_BUILD_INPROC;
                        break;

                    default:
                        break;
                    }
                }
            }
            else if ( bzda.field_1D0 == 32 )
            {
                if ( _guiActFlags & 0x20 )
                {
                    if ( _userRobo->_owner == _bactOnMouse->_owner )
                    {
                        doAction = World::DOACTION_8;
                        mousePointer = 2;
                        tooltip = Locale::TIP_DO_SELECT;
                    }
                    else
                    {
                        doAction = World::DOACTION_2;
                        mousePointer = 3;
                        tooltip = Locale::TIP_DO_ATKVHCL;
                    }
                }
                else
                {
                    switch ( ypaworld_func64__sub21__sub3() )
                    {
                    case 0:
                        doAction = World::DOACTION_10;
                        mousePointer = 9;
                        tooltip = Locale::TIP_DO_TELEPORT;
                        break;

                    case 5:
                        tooltip = Locale::TIP_ERR_UNITSINSECTOR;
                        break;

                    case 1:
                        tooltip = Locale::TIP_ERR_NOENERGY;
                        break;

                    case 2:
                        tooltip = Locale::TIP_ERR_SECTORJAGGY;
                        break;

                    case 3:
                        tooltip = Locale::TIP_ERR_LOCUNKNOWN;
                        break;

                    default:
                        break;
                    }
                }
            }

            if ( IsSpectatorControlled() )
            {
                doAction = World::DOACTION_0;

                if ( (_guiActFlags & 0x20) && IsValidSpectatorFollowTarget(_bactOnMouse) )
                {
                    if ( bzda.field_1D0 & 8 )
                    {
                        // The armed button uses the same cursor and tooltip as
                        // vanilla Jump into Vehicle, while the resulting view
                        // remains the external Spectator Follow camera.
                        mousePointer = 8;
                        tooltip = Locale::TIP_DO_CONTROL;
                    }
                    else
                    {
                        mousePointer = 2;
                        tooltip = 0;
                    }
                }
                else
                {
                    mousePointer = 1;
                    tooltip = 0;
                }
            }

            // OpenNeoUA custom: artillery shell cursor feedback.
            //  - In the 3D world, hovering one of our artillery shells shows the "can't enter"
            //    cursor (it can't be possessed).
            //  - On the strategic map it must instead read as USABLE: a "target here"
            //    cursor while aiming a selected artillery shell, and a "selectable" cursor when
            //    hovering one. (The blue "your unit" marker overlay is unaffected.)
            if ( !IsSpectatorControlled() )
            {
                bool onMap = (_guiActFlags & 8) != 0;
                bool hoverOwnArtilleryShell = (_guiActFlags & 0x20) && _bactOnMouse &&
                                      _bactOnMouse->_owner == _userRobo->_owner &&
                                      _bactOnMouse->IsArtilleryShellPlatform();

                if ( onMap && _artilleryShellManualGid )
                {
                    mousePointer = 3;   // aiming the bombardment zone: "target here"
                    tooltip = 0;
                }
                else if ( onMap && hoverOwnArtilleryShell )
                {
                    mousePointer = 2;   // hovering our artillery shell on the map: "selectable"
                    tooltip = 0;
                }
                else if ( !onMap && hoverOwnArtilleryShell )
                {
                    mousePointer = 1;   // 3D world: can't be entered
                    tooltip = 0;
                }
            }

            const bool mouseOverMapMarker =
                arg->ClickInf.selected_btn == &robo_map &&
                arg->ClickInf.selected_btnID == 17 &&
                yw_FindRoboMapMarkerAt(arg->ClickInf.move.BtnPos) >= 0;
            if ( mouseOverMapMarker )
            {
                // A marker is always an annotation, never a move/attack target.
                // Pointer feedback matches LMB drag and RMB deletion.
                mousePointer = 0;
                doAction = World::DOACTION_0;
                tooltip = 0;
            }

            _doAction = ypaworld_func64__sub21__sub4(arg, doAction);
            int v18 = _doAction;

            if ( !IsSpectatorControlled() && ypaworld_func64__sub21__sub6(&arg->ClickInf) )
            {
                // OpenNeoUA custom: a double-click must never possess an artillery shell platform.
                if ( !_bactOnMouse || !_bactOnMouse->IsArtilleryShellPlatform() )
                {
                    v18 = World::DOACTION_5;
                    mousePointer = 8;
                    arg->ClickInf.flag |= TClickBoxInf::FLAG_LM_DOWN;
                }
            }

            if ( arg->ClickInf.flag & TClickBoxInf::FLAG_LM_DOWN )
                ypaworld_func64__sub21__sub5(v18);

            if ( (arg->ClickInf.flag & TClickBoxInf::FLAG_LM_HOLD) && v18 != World::DOACTION_0 )
                mousePointer = 0;
        }

        if ( _mouseCursorHidden )
            mousePointer = -1;

        GFX::displ_arg263 v19;

        if ( mousePointer == -1 )
            v19.bitm = nullptr;
        else if (_mousePointers[mousePointer])
            v19.bitm = _mousePointers[mousePointer]->GetBitmap();

        switch ( mousePointer )
        {
        case 0:
            v19.pointer_id = 1;
            break;

        case 1:
            v19.pointer_id = 2;
            break;

        case 2:
            v19.pointer_id = 3;
            break;

        case 3:
            v19.pointer_id = 4;
            break;

        case 4:
            v19.pointer_id = 5;
            break;

        case 5:
            v19.pointer_id = 6;
            break;

        case 6:
            v19.pointer_id = 7;
            break;

        case 7:
            v19.pointer_id = 8;
            break;

        case 8:
            v19.pointer_id = 9;
            break;

        case 9:
            v19.pointer_id = 10;
            break;

        case 10:
            v19.pointer_id = 11;
            break;

        default:
            v19.pointer_id = 0;
            break;
        }

        if ( squadron_manager.field_2A8 & 1 )
            v19.pointer_id = 1;

        GFX::Engine.SetCursor(v19.pointer_id, 0);

        if ( tooltip )
            SetShowingTooltip(tooltip);
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub1(TInputState *inpt)
{
    //Precompute input
    TClickBoxInf *winp = &inpt->ClickInf;

    inpt->Buttons.UnSet(31);
    inpt->HandBrakePressed = inpt->Buttons.Is(3);

    // model = ufo never owns the generic Stop/Hand Brake action. Spy Mode has
    // its own remappable binding (Space by default), while other classes keep
    // the existing brake behavior unchanged.
    const bool controlledUfo = _userUnit && _userUnit->_bact_type == BACT_TYPES_UFO;
    if ( controlledUfo )
    {
        inpt->Buttons.UnSet(3);
        inpt->HandBrakePressed = false;
    }

    const bool cycleTargetPressed = inpt->Buttons.Is(6);
    if ( cycleTargetPressed && !_cycleTargetBtnIsDown && _userUnit )
        _userUnit->RequestHomingTargetCycle();
    _cycleTargetBtnIsDown = cycleTargetPressed;

    const bool alternativeViewPressed = inpt->Buttons.Is(7);
    if ( _userUnit )
        _userUnit->SetAlternativeViewActive(alternativeViewPressed);

    // Switch Weapon is a remappable player action, so the configured physical
    // key is authoritative here. Keep the legacy Button[1] channel only as a
    // boot-time fallback when the user binding is not available yet.
    bool weaponSwitchPressed = false;
    if ( _GameShell &&
         World::INPUT_BIND_SWITCH_WEAPON < (int)_GameShell->InputConfig.size() )
    {
        const UserData::TInputConf &switchBind =
            _GameShell->InputConfig[World::INPUT_BIND_SWITCH_WEAPON];

        if ( switchBind.Type == World::INPUT_BIND_TYPE_BUTTON &&
             switchBind.PKeyCode > Input::KC_NONE &&
             switchBind.PKeyCode < Input::KC_MAX )
            weaponSwitchPressed = Input::Engine.GetKeyState(switchBind.PKeyCode);
    }
    else
    {
        weaponSwitchPressed = inpt->Buttons.Is(1);
    }

    // The fixed secondary shortcut comes from the shared input definition.
    // On an eligible UFO the same MMB edge belongs to UFO Spy Mode, so do
    // not also route the held button into Switch Weapon. Everywhere else the
    // existing MMB weapon shortcut remains unchanged.
    const bool ufoSpyOwnsFixedShortcut =
        _mouseGrabbed && IsUfoSpyUiControlContext() &&
        World::IsFixedInputShortcutHeld(World::INPUT_BIND_TOGGLE_UFO_SPY_UI);
    if ( _mouseGrabbed && !ufoSpyOwnsFixedShortcut &&
         World::IsFixedInputShortcutHeld(World::INPUT_BIND_SWITCH_WEAPON) )
    {
        weaponSwitchPressed = true;
    }

    if ( !weaponSwitchPressed )
    {
        _weaponSwitchBtnIsDown = false;
    }
    else if ( !_weaponSwitchBtnIsDown && _userUnit && _userUnit->CycleControlledWeapon() )
    {
        // Consume the press only after a real switch. A temporarily unavailable
        // switch can therefore succeed later without requiring an unrelated key.
        _weaponSwitchBtnIsDown = true;
    }

    int v38 = 0;

    if ( _timeStamp - _vehicleTakenControlTimestamp < 500 )
        inpt->Buttons.UnSet(0);

    if ( _timeStamp - _vehicleTakenControlTimestamp > 5000 )
        _invulnerable = 0;

    if ( _shellConfIsParsed )
    {
        if ( _preferences & World::PREF_JOYDISABLE )
            v38 = 1;
    }

    _fireBtnDownHappen = false;

    if ( inpt->Buttons.Is(0) )
    {
        if ( !_fireBtnIsDown )
            _fireBtnDownHappen = true;

        _fireBtnIsDown = true;
    }
    else
    {
        _fireBtnIsDown = false;
    }

    if ( _userUnit->_status == BACT_STATUS_DEAD )
    {
        _mouseGrabbed = false;
    }
    else if ( winp->flag & TClickBoxInf::FLAG_RM_DOWN )
    {
        if ( _mouseGrabbed
                || winp->selected_btn == &robo_map
                || winp->selected_btn == &bzda
                || winp->selected_btn == &squadron_manager
                || (!(bzda.field_1D0 & 1) && _activeCmdrRemapIndex != -1) )
        {
            if ( _mouseGrabbed )
            {
                _mouseGrabbed = false;
            }
            else
            {
                if ( winp->selected_btn != &robo_map && winp->selected_btn != &bzda && !(bzda.field_1D0 & 1) )
                    bzda.field_1D0 = bzda.field_1CC & 1;
            }
        }
        else
        {
            _mouseGrabbed = true;

            Input::Engine.ResetSlider(10);
            Input::Engine.ResetSlider(11);
        }
    }

    if ( _mouseGrabbed ) // If grabbed mouse
    {
        // Piu-piu mazafaka
        winp->selected_btnID = -1;
        winp->flag &= ~(TClickBoxInf::FLAG_BTN_DOWN | TClickBoxInf::FLAG_BTN_HOLD | TClickBoxInf::FLAG_BTN_UP);

        inpt->Sliders[0] += inpt->Sliders[10];
        inpt->Sliders[1] += inpt->Sliders[11];
        inpt->Sliders[3] += inpt->Sliders[10];
        inpt->Sliders[5] -= inpt->Sliders[11] * 1.5;

        if ( World::IsFixedInputShortcutHeld(World::INPUT_BIND_FIRE) )
            inpt->Buttons.Set(0);

    }

    if ( !v38 )
    {
        int v13 = _userUnit->_bact_type == BACT_TYPES_TANK || _userUnit->_bact_type == BACT_TYPES_CAR;
        int v34 = _userUnit->_bact_type == BACT_TYPES_ROBO;

        if ( !_framesElapsed )
        {
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        int v45 = fabs(_oldJoyX - inpt->Sliders[12]) >= 0.05;
        int v44 = fabs(_oldJoyY - inpt->Sliders[13]) >= 0.05;
        int v46 = fabs(_oldJoyZ - inpt->Sliders[14]) >= 0.05;
        int v43 = fabs(_oldJoyHatX - inpt->Sliders[15]) >= 0.05;
        int v47 = fabs(_oldJoyHatY - inpt->Sliders[16]) >= 0.05;

        if ( v45 || v44 || v46 || v43 || v47)
        {
            _oldJoyX = inpt->Sliders[12];
            _oldJoyY = inpt->Sliders[13];
            _oldJoyZ = inpt->Sliders[14];
            _oldJoyHatX = inpt->Sliders[15];
            _oldJoyHatY = inpt->Sliders[16];
        }

        if ( _joyIgnoreX )
        {
            if ( v45 )
                _joyIgnoreX = 0;
        }
        else
        {
            if ( ( v13 && inpt->Sliders[3] != 0.0 ) || inpt->Sliders[0] != 0.0 )
                _joyIgnoreX = 1;
        }

        if ( _joyIgnoreY )
        {
            if ( v44 || v43 || v47 )
                _joyIgnoreY = 0;
        }
        else
        {
            if ( v13 )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK ) && inpt->Sliders[5] != 0.0 )
                    _joyIgnoreY = 1;
                else if ( inpt->Sliders[4] != 0.0 )
                    _joyIgnoreY = 1;
            }
            else if ( inpt->Sliders[1] != 0.0 )
                _joyIgnoreY = 1;
        }

        if ( _joyIgnoreZ )
        {
            if ( v46 || v43 || v47 )
                _joyIgnoreZ = 0;
        }
        else
        {
            if ( v13 )
            {
                if ( (_preferences & World::PREF_ALTJOYSTICK) && inpt->Sliders[4] != 0.0 )
                    _joyIgnoreZ = 1;
            }
            else if ( !v34 && inpt->Sliders[2] != 0.0 )
                _joyIgnoreZ = 1;
        }

        if ( inpt->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
        {
            if ( !(v43 || v44 || v45 || v46 || v47) )
            {
                _joyIgnoreY = 1;
                _joyIgnoreX = 1;
            }
        }

        if ( _joyIgnoreX )
            inpt->Sliders[12] = 0;

        if ( _joyIgnoreY )
            inpt->Sliders[13] = 0;

        if ( _joyIgnoreZ )
            inpt->Sliders[14] = 0;

        if ( !_joyIgnoreX )
        {
            if ( v13 )
                inpt->Sliders[3] += inpt->Sliders[12];
            else
                inpt->Sliders[0] += inpt->Sliders[12];
        }

        if ( !_joyIgnoreY )
        {
            if ( v13 )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[16] != 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                    else
                        inpt->Sliders[5] += inpt->Sliders[13] * 2.0;
                }
                else
                {

                    if ( fabs(inpt->Sliders[13]) < 0.3 )
                        inpt->Buttons.Set(3);
                    else
                        inpt->Sliders[4] += -inpt->Sliders[13];

                    inpt->Buttons.Set(31);
                }
            }
            else
            {
                inpt->Sliders[1] += inpt->Sliders[13];
            }
        }

        if ( !_joyIgnoreZ )
        {
            if ( v13 )
            {
                if ( _preferences & World::PREF_ALTJOYSTICK )
                {
                    if ( inpt->Sliders[4] != 0.0 )
                    {
                    }
                    else if ( fabs(inpt->Sliders[14]) < 0.3 )
                    {
                        inpt->Buttons.Set(3);
                    }
                    else
                        inpt->Sliders[4] += inpt->Sliders[14];

                    inpt->Buttons.Set(31);
                }
                else
                {
                    if ( inpt->Sliders[5] == 0.0 )
                        inpt->Sliders[5] += inpt->Sliders[16];
                }
            }
            else
            {
                if ( inpt->Sliders[2] == 0.0 )
                    inpt->Sliders[2] += inpt->Sliders[14];
            }
        }
        if ( inpt->Buttons.Is(16) )
            inpt->Buttons.Set(0);

        if ( inpt->Buttons.Is(17) )
            inpt->Buttons.Set(2);

        if ( inpt->Buttons.Is(18) )
            inpt->Buttons.Set(1);

        if ( inpt->Buttons.Is(19) )
        {
            inpt->Buttons.Set(3);
            inpt->HandBrakePressed = true;
        }
    }

    if ( _userUnit && _userUnit->_bact_type == BACT_TYPES_UFO )
    {
        // Synthetic joystick auto-brake must not reintroduce the removed UFO
        // handbrake after the explicit input was consumed above.
        inpt->Buttons.UnSet(3);
        inpt->HandBrakePressed = false;
    }
}
