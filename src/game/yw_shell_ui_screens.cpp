// Game shell: creation of the atmosphere, disk (save slots), language, about,
// database and network screens.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/framepacer.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"
#include "game/database_session.h"

bool NC_STACK_ypaworld::CreateAtmosphereControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    int usableWidth = menuWidth - 3 * buttonsSpace - _fontVBScrollW;
    int labelWidth = (int)(usableWidth * 0.36f);
    int sliderWidth = (int)(usableWidth * 0.49f);
    int valueWidth = usableWidth - labelWidth - sliderWidth;
    int rowHeight = _fontH + vertMenuSpace;
    const int pageHeight = _screenSize.y - scaledFontHeight;
    const int maxButtonY = pageHeight - _fontH - buttonsSpace;

    // This page is taller than the legacy video page. Compact row spacing only
    // when needed, then move the action buttons just far enough down to clear
    // the final slider while keeping the familiar legacy placement when it fits.
    const int maxRowHeight =
        (maxButtonY - buttonsSpace - _fontH) / (UserData::ATMOPT_COUNT + 2);
    if (maxRowHeight >= _fontH)
        rowHeight = std::min(rowHeight, maxRowHeight);

    const int lastOptionBottom =
        (UserData::ATMOPT_COUNT + 2) * rowHeight + _fontH;
    const int buttonY = std::min(
        maxButtonY,
        std::max(bottomButtonsY, lastOptionBottom + buttonsSpace));

    _GameShell->atmosphere_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});

    if (!_GameShell->atmosphere_button)
    {
        ypa_log_out("Unable to create Atmosphere-Button\n");
        return false;
    }

    const std::array<std::string, UserData::ATMOPT_COUNT> labels =
    {{
        Locale::Text::OpenUA(Locale::OUA_VISUAL_FILTER_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_ATMOSPHERE_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_EXPOSURE),
        Locale::Text::OpenUA(Locale::OUA_CONTRAST),
        Locale::Text::OpenUA(Locale::OUA_SATURATION),
        Locale::Text::OpenUA(Locale::OUA_VIGNETTE),
        Locale::Text::OpenUA(Locale::OUA_SKY_HORIZON_DISTANCE),
        Locale::Text::OpenUA(Locale::OUA_SKY_HEIGHT),
        Locale::Text::OpenUA(Locale::OUA_FOG_START),
        Locale::Text::OpenUA(Locale::OUA_FOG_LENGTH),
        Locale::Text::OpenUA(Locale::OUA_FOG_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_DARK_START),
        Locale::Text::OpenUA(Locale::OUA_DARK_LENGTH),
        Locale::Text::OpenUA(Locale::OUA_DARK_STRENGTH),
        Locale::Text::OpenUA(Locale::OUA_WORLD_UI_MAX_DISTANCE),
        Locale::Text::OpenUA(Locale::OUA_VIDEO_FILTER_INTENSITY),
        Locale::Text::OpenUA(Locale::OUA_PARTICLE_LIMIT),
        Locale::Text::OpenUA(Locale::OUA_VIDEO_FILTER_MODE),
        Locale::Text::OpenUA(Locale::OUA_HORIZON_FADE_MODE)
    }};

    const std::array<int, UserData::ATMOPT_COUNT> mins =
    {{0, 0, 25, 50, 0, 0,
      YW_SKY_HORIZON_DISTANCE_MIN / YW_SKY_HORIZON_DISTANCE_UI_STEP,
      YW_SKY_HEIGHT_MIN,
      0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0}};
    const std::array<int, UserData::ATMOPT_COUNT> maxs =
    {{100, 100, 200, 200, 200, 100,
      YW_SKY_HORIZON_DISTANCE_MAX / YW_SKY_HORIZON_DISTANCE_UI_STEP,
      YW_SKY_HEIGHT_MAX,
      10000, 10000, 100, 10000, 10000, 100, 20000, 100,
      YW_PARTICLE_LIMIT_UI_MAX, 1, 1}};

    NC_STACK_button::button_64_arg btn;
    btn.caption2.clear();
    btn.pressedCode = 0;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;

    // Page title.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = 0;
    btn.width = usableWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_ADVANCED_GRAPHICS_SETTINGS);
    btn.downCode = 0;
    btn.upCode = 0;
    btn.button_id = 1390;
    btn.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    // Data-driven graphics preset selector. Profile names come from
    // Data/Scripts/Graphic_Profiles/*.cfg; .txt remains a compatibility fallback; Custom is an in-memory UI state.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = rowHeight;
    btn.width = labelWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_GRAPHIC_PRESET);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 0;
    btn.pressedCode = 0;
    btn.button_id = 1388;
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.xpos = labelWidth + buttonsSpace;
    btn.ypos = rowHeight;
    btn.width = sliderWidth + valueWidth + buttonsSpace;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_CUSTOM);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 1449;
    btn.pressedCode = 0;
    btn.button_id = 1389;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    // Global visual-filter selector, persisted with the sliders in OpenNeoUA.ini.
    btn.tileset_down = 16;
    btn.tileset_up = 16;
    btn.field_3A = 16;
    btn.button_type = NC_STACK_button::TYPE_CAPTION;
    btn.xpos = 0;
    btn.ypos = 2 * rowHeight;
    btn.width = labelWidth;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_VISUAL_FILTER);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 0;
    btn.pressedCode = 0;
    btn.button_id = 1391;
    btn.flags = NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[60].r;
    btn.txt_g = _iniColors[60].g;
    btn.txt_b = _iniColors[60].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.xpos = labelWidth + buttonsSpace;
    btn.ypos = 2 * rowHeight;
    btn.width = sliderWidth + valueWidth + buttonsSpace;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_STANDARD);
    btn.caption2.clear();
    btn.downCode = 0;
    btn.upCode = 1136;
    btn.pressedCode = 0;
    btn.button_id = 1392;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    for (int i = 0; i < UserData::ATMOPT_COUNT; ++i)
    {
        int y = (i + 3) * rowHeight;

        btn.tileset_down = 16;
        btn.tileset_up = 16;
        btn.field_3A = 16;
        btn.button_type = NC_STACK_button::TYPE_CAPTION;
        btn.xpos = 0;
        btn.ypos = y;
        btn.width = labelWidth;
        btn.caption = labels[i];
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1500 + i;
        btn.flags = NC_STACK_button::FLAG_TEXT;
        btn.txt_r = _iniColors[60].r;
        btn.txt_g = _iniColors[60].g;
        btn.txt_b = _iniColors[60].b;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;

        NC_STACK_button::Slider slider;
        slider.value = mins[i];
        slider.min = mins[i];
        slider.max = maxs[i];
        slider.field_6_ = 0;
        slider.field_8_ = 0;
        slider.pressPart = 0;
        slider.oldValue = slider.value;
        slider.scrDownX = 0;

        btn.tileset_down = 18;
        btn.tileset_up = 18;
        btn.field_3A = 30;
        btn.button_type = NC_STACK_button::TYPE_SLIDER;
        btn.xpos = labelWidth + buttonsSpace;
        btn.ypos = y;
        btn.width = sliderWidth;
        btn.caption = " ";
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1400 + i;
        btn.flags = 0;
        btn.field_34 = &slider;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;

        btn.tileset_down = 16;
        btn.tileset_up = 16;
        btn.field_3A = 16;
        btn.button_type = NC_STACK_button::TYPE_CAPTION;
        btn.xpos = labelWidth + sliderWidth + 2 * buttonsSpace;
        btn.ypos = y;
        btn.width = valueWidth;
        btn.caption = "0";
        btn.caption2.clear();
        btn.downCode = 0;
        btn.upCode = 0;
        btn.pressedCode = 0;
        btn.button_id = 1420 + i;
        btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn.txt_r = _iniColors[60].r;
        btn.txt_g = _iniColors[60].g;
        btn.txt_b = _iniColors[60].b;
        btn.field_34 = nullptr;
        if (!_GameShell->atmosphere_button->Add(&btn))
            return false;
    }

    int buttonWidth = (menuWidth - 2 * buttonsSpace) / 3;
    btn.tileset_down = 19;
    btn.tileset_up = 18;
    btn.field_3A = 30;
    btn.button_type = NC_STACK_button::TYPE_BUTTON;
    btn.ypos = buttonY;
    btn.width = buttonWidth;
    btn.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn.txt_r = _iniColors[68].r;
    btn.txt_g = _iniColors[68].g;
    btn.txt_b = _iniColors[68].b;

    btn.xpos = 0;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_SAVE_SETTINGS);
    btn.upCode = 1450;
    btn.button_id = 1450;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.xpos = buttonWidth + buttonsSpace;
    btn.caption = Locale::Text::OpenUA(Locale::OUA_SAVE_PRESET);
    btn.upCode = 1453;
    btn.button_id = 1453;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    btn.xpos = 2 * (buttonWidth + buttonsSpace);
    btn.caption = Locale::Text::Common(Locale::CMN_CANCEL);
    btn.upCode = 1452;
    btn.button_id = 1452;
    if (!_GameShell->atmosphere_button->Add(&btn))
        return false;

    _GameShell->atmosphere_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateDiskControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    // OpenNeoUA: keep all Save/Load actions on one row. Back sits immediately
    // to the right of Save; Delete keeps its own final slot when enabled.
    const int diskActionButtonWidth = (menuWidth - 4 * buttonsSpace) / 5;
    const int diskActionButtonY = 7 * buttonsSpace + 15 * _fontH;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = _GameShell->profiles.size();
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->disk_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create disk-listview\n");
        return false;
    }

    _GameShell->disk_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->disk_button )
    {
        ypa_log_out("Unable to create disk-buttonobject\n");
        return false;
    }

    _GameShell->disk_listvw.x = posLeftPaddingX;
    _GameShell->disk_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    _GameShell->userNameDir = _GameShell->UserName;

    _GameShell->userNameDirCursor = _GameShell->userNameDir.size();

    std::string v223 = _GameShell->userNameDir;

    if ( _GameShell->diskScreenMode )
        v223 += "h";

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;

    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;

    btn_64arg.xpos = 0;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.button_id = 1108;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->disk_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = buttonsSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT2);
        btn_64arg.button_id = 1109;
        btn_64arg.caption2.clear();
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->disk_button->Add(&btn_64arg))
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1110;

            if ( _GameShell->disk_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (buttonsSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1111;

                if ( _GameShell->disk_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 17;
                    btn_64arg.tileset_up = 17;
                    btn_64arg.field_3A = 17;
                    btn_64arg.xpos = 0;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                    btn_64arg.width = menuWidth;
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.upCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.caption = v223.c_str();
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1100;
                    btn_64arg.ypos = 6 * buttonsSpace + 14 * _fontH;

                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 19;
                        btn_64arg.tileset_up = 18;
                        btn_64arg.field_3A = 30;
                        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                        btn_64arg.xpos = buttonsSpace + diskActionButtonWidth;
                        btn_64arg.ypos = diskActionButtonY;
                        btn_64arg.width = diskActionButtonWidth;
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_LOAD);
                        btn_64arg.downCode = 1251;
                        btn_64arg.upCode = 1160;
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.button_id = 1101;
                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[68].r;
                        btn_64arg.txt_g = _iniColors[68].g;
                        btn_64arg.txt_b = _iniColors[68].b;

                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = 4 * (buttonsSpace + diskActionButtonWidth);
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_DELETE);;
                            btn_64arg.caption2.clear();
                            btn_64arg.upCode = 1161;
                            btn_64arg.button_id = 1102;

                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                            {
                                btn_64arg.xpos = 0;
                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_NEW);
                                btn_64arg.button_id = 1103;
                                btn_64arg.caption2.clear();
                                btn_64arg.upCode = 1162;

                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.xpos = 2 * (buttonsSpace + diskActionButtonWidth);
                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_P_SAVE);
                                    btn_64arg.button_id = 1104;
                                    btn_64arg.caption2.clear();
                                    btn_64arg.upCode = 1163;

                                    if ( _GameShell->disk_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = bottomButtonsY;
                                        btn_64arg.width = button1LineWidth;
                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 1251;
                                        btn_64arg.button_id = 1105;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.upCode = 1164;

                                        if ( _GameShell->disk_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.ypos = bottomButtonsY;
                                            btn_64arg.width = button1LineWidth;
                                            btn_64arg.xpos = bottomThirdBtnPosX;
                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                            btn_64arg.button_id = 1107;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.upCode = 1250;

                                            if ( _GameShell->disk_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.ypos = diskActionButtonY;
                                                btn_64arg.width = diskActionButtonWidth;
                                                btn_64arg.xpos = 3 * (buttonsSpace + diskActionButtonWidth);
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                btn_64arg.button_id = 1106;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.upCode = 1165;

                                                if ( _GameShell->disk_button->Add(&btn_64arg) )
                                                {
                                                    v70 = 1;
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
        }
    }

    if ( !v70 )
    {
        ypa_log_out("Unable to add button to disk-buttonobject\n");
        return false;
    }

        // OpenNeoUA: remove deprecated online-help button from Save/Load.
    _GameShell->disk_button->Remove(1107);

_GameShell->disk_button->HideScreen();
    NC_STACK_button::button_66arg v228;
    v228.field_4 = 0;
    v228.butID = 1105;
    _GameShell->disk_button->Disable(&v228);
    return true;
}

bool NC_STACK_ypaworld::CreateLocaleControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 10;
    args.shownEntries = 10;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 10;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = menuWidth - _fontVBScrollW;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->local_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create local-listview\n");
        return false;
    }

    _GameShell->locale_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}} );

    if ( !_GameShell->locale_button )
    {
        ypa_log_out("Unable to create locale-buttonobject\n");
        return false;
    }

    _GameShell->local_listvw.x = posLeftPaddingX;
    _GameShell->local_listvw.y = 4 * (vertMenuSpace + _fontH) + scaledFontHeight;

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.field_3A = 30;
    btn_64arg.ypos = 0;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1253;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->locale_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1254;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->locale_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.button_id = 1255;

            if ( _GameShell->locale_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_L_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1256;

                if ( _GameShell->locale_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                    btn_64arg.field_3A = 30;
                    btn_64arg.xpos = bottomCenteredFirstBtnPosX;
                    btn_64arg.ypos = bottomButtonsY;
                    btn_64arg.width = button1LineWidth;
                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.upCode = 1300;
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.button_id = 1250;
                    btn_64arg.txt_r = _iniColors[68].r;
                    btn_64arg.txt_g = _iniColors[68].g;
                    btn_64arg.txt_b = _iniColors[68].b;

                    if ( _GameShell->locale_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = bottomThirdBtnPosX;
                        btn_64arg.ypos = bottomButtonsY;
                        btn_64arg.width = button1LineWidth;
                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                        btn_64arg.button_id = 1252;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 1250;
                        btn_64arg.pressedCode = 0;

                        if ( _GameShell->locale_button->Add(&btn_64arg) )
                        {
                            btn_64arg.xpos = bottomCenteredSecondBtnPosX;
                            btn_64arg.ypos = bottomButtonsY;
                            btn_64arg.width = button1LineWidth;
                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.upCode = 1301;
                            btn_64arg.button_id = 1251;

                            if ( _GameShell->locale_button->Add(&btn_64arg) )
                                v70 = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add locale-button\n");
        return false;
    }

        // OpenNeoUA: remove deprecated online-help button from Language.
    _GameShell->locale_button->Remove(1252);

_GameShell->locale_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateAboutControls()
{
    auto *panel=Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X,(int32_t)0},{NC_STACK_button::BTN_ATT_Y,(int32_t)0},
        {NC_STACK_button::BTN_ATT_W,(int32_t)_screenSize.x},{NC_STACK_button::BTN_ATT_H,(int32_t)_screenSize.y}});
    _GameShell->about_button=panel;if(!panel)return false;
    panel->screen_width=_screenSize.x;panel->screen_height=_screenSize.y;
    const int line=std::max(12,_fontH+vertMenuSpace);
    const std::vector<std::string> credits={
        "OpenNeoUA",
        Locale::Text::OpenUA(Locale::OUA_ABOUT_SUMMARY),
        Locale::Text::OpenUA(Locale::OUA_ABOUT_BUILD)+": "+__DATE__+" "+__TIME__,
        "OpenNeoUA: TeuZzZ-17 and contributors",
        "UA_source / OpenUA: Marisa-Chan and contributors",
        "Urban Assault: TerraTools / Microsoft",
        " ",Locale::Text::OpenUA(Locale::OUA_ABOUT_CREDITS),
        "Bernd Beyreuther, Andre 'Floh' Weissflog, Andreas Flemming",
        "Stefan 'Metzel Hetzel' Karau, Sylvius Lack",
        "Dietmar 'Didi' Koebelin, Nico Nitsch, Steffen Priebus",
        "Stefan Warias, Henrik Volkening, Uta Kapp",
        " ",Locale::Text::OpenUA(Locale::OUA_ABOUT_THANKS),
        "Microsoft: Michael Lyons, Jonathan Sposato, Earnest Yuen",
        "Robert Birker, Andre 'Goetz' Blechschmidt, Jan Blechschmidt",
        "Stephan Bludau, Andre Kunth, Markus Lorenz, Dirk Mansbart",
        "GoldEd - dPaint - SAS/C"};
    for(size_t i=0;i<credits.size();++i)
    {
        NC_STACK_button::button_64_arg b;
        b.tileset_down=16;b.tileset_up=16;b.field_3A=16;
        b.button_type=NC_STACK_button::TYPE_CAPTION;b.xpos=8;b.ypos=(i+1)*line;
        b.width=_screenSize.x-16;b.caption=credits[i];b.caption2.clear();
        b.downCode=0;b.upCode=0;b.pressedCode=0;b.button_id=2300+i;
        b.flags=NC_STACK_button::FLAG_CENTER|NC_STACK_button::FLAG_TEXT;
        b.txt_r=_iniColors[68].r;b.txt_g=_iniColors[68].g;b.txt_b=_iniColors[68].b;
        if(!panel->Add(&b))return false;
    }
    NC_STACK_button::button_64_arg back;
    back.tileset_down=19;back.tileset_up=18;back.field_3A=30;
    back.button_type=NC_STACK_button::TYPE_BUTTON;back.xpos=_screenSize.x/3;
    back.ypos=_screenSize.y-_fontH;back.width=_screenSize.x/3;
    back.caption=Locale::Text::OpenUA(Locale::OUA_DB_BACK);back.caption2.clear();
    back.downCode=UIWidgets::ALL_DOWN;back.upCode=UIWidgets::ABOUT_UP_BACK;back.pressedCode=0;
    back.button_id=UIWidgets::ABOUT_BTN_BACK;
    back.flags=NC_STACK_button::FLAG_BORDER|NC_STACK_button::FLAG_CENTER|NC_STACK_button::FLAG_TEXT;
    back.txt_r=_iniColors[68].r;back.txt_g=_iniColors[68].g;back.txt_b=_iniColors[68].b;
    if(!panel->Add(&back))return false;
    panel->HideScreen();return true;
}

bool NC_STACK_ypaworld::CreateDatabaseControls()
{
    auto *panel=Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X,(int32_t)0},{NC_STACK_button::BTN_ATT_Y,(int32_t)0},
        {NC_STACK_button::BTN_ATT_W,(int32_t)_screenSize.x},{NC_STACK_button::BTN_ATT_H,(int32_t)_screenSize.y}});
    _GameShell->database_button=panel;if(!panel)return false;
    panel->screen_width=_screenSize.x;panel->screen_height=_screenSize.y;
    const auto l=DatabaseLayout::For(*this);
    auto add=[&](int id,int event,int x,int y,int width,const std::string &title,bool caption=false) {
        NC_STACK_button::button_64_arg b;
        b.tileset_down=caption?16:19;b.tileset_up=caption?16:18;b.field_3A=caption?16:30;
        b.button_type=caption?NC_STACK_button::TYPE_CAPTION:NC_STACK_button::TYPE_BUTTON;
        b.downCode=caption?0:UIWidgets::ALL_DOWN;b.upCode=event;b.pressedCode=0;
        b.xpos=x;b.ypos=y;b.width=width;b.caption=title;b.caption2.clear();b.button_id=id;
        b.flags=NC_STACK_button::FLAG_TEXT|(caption?0:NC_STACK_button::FLAG_BORDER|NC_STACK_button::FLAG_CENTER);
        b.txt_r=_iniColors[68].r;b.txt_g=_iniColors[68].g;b.txt_b=_iniColors[68].b;
        return panel->Add(&b)!=0;
    };
    const int third=_screenSize.x/3;
    if(!add(UIWidgets::DB_BTN_UNITS,UIWidgets::DB_UP_UNITS,0,0,third,Locale::Text::OpenUA(Locale::OUA_DB_UNITS)))return false;
    if(!add(UIWidgets::DB_BTN_BUILDINGS,UIWidgets::DB_UP_BUILDINGS,third,0,third,Locale::Text::OpenUA(Locale::OUA_DB_BUILDINGS)))return false;
    if(!add(DatabaseLayout::WorkshopButton,DatabaseLayout::WorkshopEvent,2*third,0,_screenSize.x-2*third,"Workshop"))return false;
    if(!add(UIWidgets::DB_BTN_PURCHASE,UIWidgets::DB_UP_PURCHASE,0,3*l.line,l.listWidth,"Purchase"))return false;
    if(!add(UIWidgets::DB_LABEL_PAGE,0,0,l.line,l.listWidth," ",true))return false;
    for(int i=0;i<l.rows;++i)
        if(!add(DatabaseLayout::ListButton+i,DatabaseLayout::ListEvent+i,0,l.listTop+i*l.line,l.listWidth," "))return false;
    for(int i=0;i<3;++i)
        if(!add(2040+i,2140+i,0,l.bottom-(3-i)*l.line,l.listWidth," "))return false;
    const int nav[]={Locale::OUA_DB_PREV,Locale::OUA_DB_BACK,Locale::OUA_DB_NEXT};
    for(int i=0;i<3;++i)
        if(!add(UIWidgets::DB_BTN_PREV+i,UIWidgets::DB_UP_PREV+i,i*_screenSize.x/3,l.bottom,_screenSize.x/3,
            (i==0?"< ":"")+Locale::Text::OpenUA(nav[i])+(i==2?" >":"")))return false;
    panel->HideScreen();return true;
}

bool NC_STACK_ypaworld::CreateNetworkControls()
{
    int posLeftPaddingX = (_screenSize.x * 0.3) / 2;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = 12;
    args.shownEntries = 12;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 12;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = dword_5A50B2_h;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->network_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create network-listview\n");
        return false;
    }

    int nypos = scaledFontHeight - _fontH;

    _GameShell->network_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)nypos},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - nypos)}});
    if ( !_GameShell->network_button )
    {
        ypa_log_out("Unable to create network-buttonobject\n");
        return false;
    }

    _GameShell->netListY = 3 * (vertMenuSpace + _fontH) + nypos;

    _GameShell->network_listvw.x = posLeftPaddingX;
    _GameShell->network_listvw.y = _GameShell->netListY;

    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    btn_64arg.tileset_down = 17;
    btn_64arg.tileset_up = 17;
    btn_64arg.field_3A = 17;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.xpos = 0;
    btn_64arg.caption = "???";
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.ypos = 14 * (vertMenuSpace + _fontH);
    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.width = dword_5A50B6 * 0.8;
    btn_64arg.txt_r = _iniColors[60].r;
    btn_64arg.txt_g = _iniColors[60].g;
    btn_64arg.txt_b = _iniColors[60].b;

    v70 = 0;

    if ( _GameShell->network_button->Add(&btn_64arg) )
    {
        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.xpos = buttonsSpace + dword_5A50B6 * 0.8;
        btn_64arg.field_3A = 30;
        btn_64arg.width = dword_5A50B6 * 0.2 - buttonsSpace;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SEND);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = 1210;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1225;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( _GameShell->network_button->Add(&btn_64arg) )
        {
            int v284 = ((dword_5A50B6 - 3 * buttonsSpace) * 0.25 - 3 * buttonsSpace) * 0.25;

            TileMap *v198 = GFX::Engine.GetTileset(8);

            btn_64arg.tileset_down = 16;
            btn_64arg.tileset_up = 16;
            btn_64arg.field_3A = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.ypos = (15 * (vertMenuSpace + _fontH));
            btn_64arg.xpos = 0;
            btn_64arg.width = dword_5A50B6 * 0.4 - 2 * buttonsSpace;
            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELRACE);
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_TEXT | NC_STACK_button::FLAG_RALIGN;
            btn_64arg.button_id = 1220;
            btn_64arg.txt_r = _iniColors[60].r;
            btn_64arg.txt_g = _iniColors[60].g;
            btn_64arg.txt_b = _iniColors[60].b;

            if ( _GameShell->network_button->Add(&btn_64arg) )
            {
                btn_64arg.tileset_down = 8;
                btn_64arg.tileset_up = 8;
                btn_64arg.field_3A = 30;
                btn_64arg.button_type = 4;
                btn_64arg.xpos += btn_64arg.width + 2 * buttonsSpace;
                btn_64arg.width = v198->map[65].w;
                btn_64arg.caption = "A";
                btn_64arg.caption2 = "B";
                btn_64arg.button_id = 1206;
                btn_64arg.downCode = 1204;
                btn_64arg.flags = 0;

                if ( _GameShell->network_button->Add(&btn_64arg) )
                {
                    btn_64arg.caption = "C";
                    btn_64arg.caption2 = "D";
                    btn_64arg.downCode = 1205;
                    btn_64arg.button_id = 1207;
                    btn_64arg.xpos += v284 + buttonsSpace;

                    if ( _GameShell->network_button->Add(&btn_64arg) )
                    {
                        btn_64arg.caption = "E";
                        btn_64arg.caption2 = "F";
                        btn_64arg.downCode = 1206;
                        btn_64arg.button_id = 1208;
                        btn_64arg.xpos += v284 + buttonsSpace;

                        if ( _GameShell->network_button->Add(&btn_64arg) )
                        {
                            btn_64arg.caption = "G";
                            btn_64arg.caption2 = "H";
                            btn_64arg.downCode = 1207;
                            btn_64arg.button_id = 1209;
                            btn_64arg.xpos += v284 + buttonsSpace;

                            if ( _GameShell->network_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 19;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.field_3A = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                btn_64arg.xpos += v284 + 2 * buttonsSpace;
                                btn_64arg.width = dword_5A50B2_h - btn_64arg.xpos;
                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_BACK);
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.button_id = 1205;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.upCode = 1203;
                                btn_64arg.downCode = 1251;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 16;
                                    btn_64arg.xpos = 0;
                                    btn_64arg.ypos = 0;
                                    btn_64arg.tileset_up = 16;
                                    btn_64arg.field_3A = 16;
                                    btn_64arg.width = dword_5A50B2_h;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_SELPROV);
                                    btn_64arg.caption2.clear();
                                    btn_64arg.downCode = 0;
                                    btn_64arg.upCode = 0;
                                    btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE;
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.xpos = 0;
                                        btn_64arg.ypos = buttonsSpace + _fontH;
                                        btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT2);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1;
                                        btn_64arg.txt_r = _iniColors[60].r;
                                        btn_64arg.txt_g = _iniColors[60].g;
                                        btn_64arg.txt_b = _iniColors[60].b;

                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.xpos = 0;
                                            btn_64arg.ypos = 2 * (_fontH + buttonsSpace);
                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_TXT3);
                                            btn_64arg.caption2.clear();
                                            btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2;

                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 19;
                                                btn_64arg.tileset_up = 18;
                                                btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                btn_64arg.field_3A = 30;
                                                btn_64arg.xpos = dword_5A50B6 * 0.3;
                                                btn_64arg.ypos = (buttonsSpace + _fontH) * 15.2;
                                                btn_64arg.width = dword_5A50B6 * 0.4;
                                                btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEW);
                                                btn_64arg.button_id = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
                                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                btn_64arg.downCode = 1251;
                                                btn_64arg.upCode = 1201;
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.txt_r = _iniColors[68].r;
                                                btn_64arg.txt_g = _iniColors[68].g;
                                                btn_64arg.txt_b = _iniColors[68].b;

                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = bottomCenteredFirstBtnPosX;
                                                    btn_64arg.ypos = bottomButtonsY + _fontH;
                                                    btn_64arg.width = button1LineWidth;
                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_NEXT);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1201;
                                                    btn_64arg.upCode = 1200;

                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY + _fontH;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.button_id = 1218;

                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = bottomCenteredSecondBtnPosX;
                                                            btn_64arg.ypos = bottomButtonsY + _fontH;
                                                            btn_64arg.width = button1LineWidth;
                                                            btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_BACK);
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.upCode = 1202;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.button_id = 1203;

                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                            {
                                                                int v204;

                                                                if ( _screenSize.x < 512 )
                                                                    v204 = 6 * checkBoxWidth;
                                                                else
                                                                    v204 = 4 * checkBoxWidth;

                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.field_3A = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = v204 + checkBoxWidth;
                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                btn_64arg.caption = " ";
                                                                btn_64arg.width = dword_5A50B2_h - v204 - checkBoxWidth;
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 1210;
                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.ypos = 5 * (buttonsSpace + _fontH);
                                                                    btn_64arg.button_id = 1211;

                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                    {
                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                        btn_64arg.button_id = 1212;

                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                        {
                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                            btn_64arg.button_id = 1213;

                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                            {
                                                                                btn_64arg.tileset_down = 8;
                                                                                btn_64arg.tileset_up = 8;
                                                                                btn_64arg.field_3A = 8;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                btn_64arg.xpos = 0;
                                                                                btn_64arg.width = v204;
                                                                                btn_64arg.caption2.clear();
                                                                                btn_64arg.downCode = 0;
                                                                                btn_64arg.upCode = 0;
                                                                                btn_64arg.ypos = 4 * (_fontH + buttonsSpace);
                                                                                btn_64arg.pressedCode = 0;
                                                                                btn_64arg.caption = " ";
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.button_id = 1214;
                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.ypos = 5 * (_fontH + buttonsSpace);
                                                                                    btn_64arg.button_id = 1215;

                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                    {
                                                                                        btn_64arg.ypos = 6 * (buttonsSpace + _fontH);
                                                                                        btn_64arg.button_id = 1216;

                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                        {
                                                                                            btn_64arg.ypos = 7 * (buttonsSpace + _fontH);
                                                                                            btn_64arg.button_id = 1217;

                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                btn_64arg.tileset_down = 19;
                                                                                                btn_64arg.tileset_up = 18;
                                                                                                btn_64arg.xpos = 0;
                                                                                                btn_64arg.field_3A = 30;
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                                btn_64arg.width = checkBoxWidth;
                                                                                                btn_64arg.caption = "g";
                                                                                                btn_64arg.caption2 = "g";
                                                                                                btn_64arg.pressedCode = 0;
                                                                                                btn_64arg.button_id = 1219;
                                                                                                btn_64arg.ypos = bottomButtonsY + _fontH;
                                                                                                btn_64arg.downCode = 1208;
                                                                                                btn_64arg.flags = 0;
                                                                                                btn_64arg.upCode = 1209;

                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                {
                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                    btn_64arg.field_3A = 16;
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos += buttonsSpace + checkBoxWidth;
                                                                                                    btn_64arg.width = button1LineWidth - checkBoxWidth - buttonsSpace;
                                                                                                    btn_64arg.caption = Locale::Text::Netdlg(Locale::NETDLG_READY);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                    btn_64arg.button_id = 1221;

                                                                                                    if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        btn_64arg.xpos = 0;
                                                                                                        btn_64arg.tileset_down = 16;
                                                                                                        btn_64arg.tileset_up = 16;
                                                                                                        btn_64arg.field_3A = 16;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                        btn_64arg.ypos = 3 * (_fontH + buttonsSpace);
                                                                                                        btn_64arg.width = dword_5A50B6 * 0.3;
                                                                                                        btn_64arg.caption = Locale::Text::Get(Locale::LBL_YOUPLAY, Locale::DefaultStrings::YouPlay);
                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.downCode = 0;
                                                                                                        btn_64arg.upCode = 0;
                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                        btn_64arg.button_id = 1227;
                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                        btn_64arg.txt_r = _iniColors[68].r;
                                                                                                        btn_64arg.txt_g = _iniColors[68].g;
                                                                                                        btn_64arg.txt_b = _iniColors[68].b;

                                                                                                        if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.xpos = dword_5A50B6 * 0.3;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.width = dword_5A50B6 * 0.7;
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                            btn_64arg.button_id = 1226;
                                                                                                            btn_64arg.caption = "...";

                                                                                                            if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.tileset_down = 16;
                                                                                                                btn_64arg.tileset_up = 16;
                                                                                                                btn_64arg.field_3A = 16;
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = (14 * (vertMenuSpace + _fontH));
                                                                                                                btn_64arg.width = dword_5A50B2_h;
                                                                                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_REFRESHSESS);
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.upCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.button_id = 1228;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                                btn_64arg.txt_r = _iniColors[60].r;
                                                                                                                btn_64arg.txt_g = _iniColors[60].g;
                                                                                                                btn_64arg.txt_b = _iniColors[60].b;

                                                                                                                if ( _GameShell->network_button->Add(&btn_64arg) )
                                                                                                                    v70 = 1;
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
                            }
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add network-button\n");
        return false;
    }
    NC_STACK_button::button_66arg v228;
    v228.butID = 1210;
    v228.field_4 = 0;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1211;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1212;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1213;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1214;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1215;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1216;
    _GameShell->network_button->Disable(&v228);

    v228.butID = 1217;
    _GameShell->network_button->Disable(&v228);

        // OpenNeoUA: remove deprecated online-help button from Network.
    _GameShell->network_button->Remove(1218);

_GameShell->network_button->HideScreen();
    return true;
}
