// Game shell: creation of the title, sub bar, confirm, input and video (options) controls.
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

//Controls creation methods
bool NC_STACK_ypaworld::CreateTitleControls()
{
    auto *panel=Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X,(int32_t)0},{NC_STACK_button::BTN_ATT_Y,(int32_t)0},
        {NC_STACK_button::BTN_ATT_W,(int32_t)_screenSize.x},{NC_STACK_button::BTN_ATT_H,(int32_t)_screenSize.y}});
    _GameShell->titel_button=panel;
    if(!panel)return false;
    // These controls are drawn on the logical menu canvas, also at high resolutions.
    panel->screen_width=_screenSize.x;panel->screen_height=_screenSize.y;
    struct Item { int row,half,id,event,title; };
    const Item items[]={
        {0,0,UIWidgets::BTN_SINGLE_PLAYER,UIWidgets::BTN_SINGLE_PLAYER_UP,Locale::TITLE_GAME},
        {0,1,UIWidgets::BTN_MULTIPLAYER,UIWidgets::BTN_MULTIPLAYER_UP,Locale::TITLE_NETWORK},
        {1,0,UIWidgets::BTN_HELP,UIWidgets::BTN_HELP_UP,Locale::TITLE_DATABASE},
        {1,1,UIWidgets::BTN_WORKSHOP,UIWidgets::BTN_WORKSHOP_UP,-1},
        {2,-1,UIWidgets::BTN_INPUT_SETTINGS,UIWidgets::BTN_INPUT_SETTINGS_UP,Locale::TITLE_INPUT},
        {3,-1,UIWidgets::BTN_OPTIONS,UIWidgets::BTN_OPTIONS_UP,Locale::TITLE_SETTINGS},
        {4,-1,UIWidgets::BTN_ABOUT,UIWidgets::BTN_ABOUT_UP,Locale::TITLE_LOCALE},
        {5,-1,UIWidgets::BTN_SAVE_LOAD,UIWidgets::BTN_SAVE_LOAD_UP,Locale::TITLE_PROFILE},
        {6,-1,UIWidgets::BTN_QUIT,UIWidgets::BTN_QUIT_UP,Locale::TITLE_QUIT}};
    const double rows[]={0.2291666666666666,0.3083333333333334,0.4333333333333334,
                         0.5125,0.5916666666666667,0.7166666666666667,0.7958333333333333};
    const bool workshop=IsPlasmaCurrencyEnabled();
    const int left=_screenSize.x*0.3328125,width=_screenSize.x/3,gap=std::max(2,_screenSize.x/320);
    for(const auto &item:items)
    {
        if(item.id==UIWidgets::BTN_WORKSHOP&&!workshop)continue;
        NC_STACK_button::button_64_arg btn;
        btn.tileset_down=19;btn.tileset_up=18;btn.button_type=NC_STACK_button::TYPE_BUTTON;btn.field_3A=30;
        const bool split=item.half>=0&&(item.row!=1||workshop);
        btn.xpos=left+(split&&item.half==1?(width+gap)/2:0);
        btn.width=split?(width-gap)/2:width;btn.ypos=_screenSize.y*rows[item.row];
        btn.caption=item.title<0?Locale::Text::OpenUA(Locale::OUA_WORKSHOP):Locale::Text::Title(item.title);
        if(item.id==UIWidgets::BTN_ABOUT)btn.caption=Locale::Text::OpenUA(Locale::OUA_ABOUT);
        btn.caption2.clear();btn.downCode=UIWidgets::ALL_DOWN;btn.pressedCode=0;
        btn.button_id=item.id;btn.upCode=item.event;
        btn.flags=NC_STACK_button::FLAG_BORDER|NC_STACK_button::FLAG_CENTER|NC_STACK_button::FLAG_TEXT;
        btn.txt_r=_iniColors[68].r;btn.txt_g=_iniColors[68].g;btn.txt_b=_iniColors[68].b;
        if(!panel->Add(&btn))return false;
    }
    panel->HideScreen();return true;
}

bool NC_STACK_ypaworld::CreateSubBarControls(){
    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;
    dword_5A50B6_h = _screenSize.x / 4 - 20;

    _GameShell->sub_bar_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)(_screenSize.y - _fontH)},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_fontH}});

    if ( !_GameShell->sub_bar_button )
    {
        ypa_log_out("Unable to create Button-Object\n");
        return false;
    }

    v70 = 0;

    btn_64arg.tileset_down = 19;
    btn_64arg.field_3A = 30;
    btn_64arg.ypos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.tileset_up = 18;
    btn_64arg.xpos = dword_5A50B6_h + buttonsSpace;
    btn_64arg.width = dword_5A50B6_h;
    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_REWIND);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 1251;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1011;
    btn_64arg.upCode = 1016;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 2 * (buttonsSpace + dword_5A50B6_h);
        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_STEPFWD);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1020;
        btn_64arg.pressedCode = 1018;
        btn_64arg.button_id = 1013;

        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_START);
            btn_64arg.caption2.clear();
            btn_64arg.upCode = 1019;
            btn_64arg.pressedCode = 0;
            btn_64arg.downCode = 1251;
            btn_64arg.button_id = 1014;

            if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = (_screenSize.x - 3 * dword_5A50B6_h - 2 * buttonsSpace);
                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_GOTOLDSV);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.downCode = 1251;
                btn_64arg.button_id = 1020;
                btn_64arg.upCode = 1026;

                if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                {
                    btn_64arg.xpos = (_screenSize.x - 2 * dword_5A50B6_h - buttonsSpace);
                    btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_LOAD);
                    btn_64arg.caption2.clear();
                    btn_64arg.upCode = 1021;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.downCode = 1251;
                    btn_64arg.button_id = 1015;

                    if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                    {
                        btn_64arg.xpos = _screenSize.x - dword_5A50B6_h;
                        btn_64arg.caption = Locale::Text::GlobMap(Locale::GLOBMAP_GOBACK);
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.downCode = 1251;
                        btn_64arg.button_id = 1019;
                        btn_64arg.upCode = 1013;

                        if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                        {
                            int playAsWidth = _screenSize.x * 0.36;

                            btn_64arg.xpos = (_screenSize.x - playAsWidth) / 2;
                            btn_64arg.width = playAsWidth;
                            btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_PLAY_AS);
                            btn_64arg.caption2.clear();
                            btn_64arg.pressedCode = 0;
                            btn_64arg.downCode = 1251;
                            btn_64arg.button_id = 1027;
                            btn_64arg.upCode = 1027;

                            if ( _GameShell->sub_bar_button->Add(&btn_64arg) )
                                v70 = 1;
                        }
                    }
                }
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add button to sub-bar\n");
        return false;
    }
    NC_STACK_button::button_66arg v228;
    if ( _GameShell->sgmSaveExist != 1 )
    {
        v228.butID = 1015;
        v228.field_4 = 0;
        _GameShell->sub_bar_button->Disable(&v228);
    }

    v228.field_4 = 0;
    v228.butID = 1014;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1013;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1011;
    _GameShell->sub_bar_button->Disable(&v228);

    v228.butID = 1027;
    _GameShell->sub_bar_button->Disable(&v228);

    _GameShell->sub_bar_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateConfirmControls()
{

    _GameShell->confirm_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)0},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)0},
        {NC_STACK_button::BTN_ATT_W, (int32_t)_screenSize.x},
        {NC_STACK_button::BTN_ATT_H, (int32_t)_screenSize.y}} );
    if ( !_GameShell->confirm_button )
    {
        ypa_log_out("Unable to create Confirm-Button-Object\n");
        return false;
    }

    NC_STACK_button::button_64_arg btn_64arg;
    const SDL_Color boxTextColor = GetFactionBoxTextColor();
    btn_64arg.tileset_up = 18;
    btn_64arg.tileset_down = 19;
    btn_64arg.field_3A = 30;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    btn_64arg.xpos = _screenSize.x * 0.25;
    btn_64arg.ypos = _screenSize.y * 0.53125;
    btn_64arg.width = _screenSize.x * 0.125;
    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
    btn_64arg.caption2.clear();
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.upCode = 1350;
    btn_64arg.downCode = 1251;
    btn_64arg.button_id = 1300;
    btn_64arg.txt_r = boxTextColor.r;
    btn_64arg.txt_g = boxTextColor.g;
    btn_64arg.txt_b = boxTextColor.b;

    if ( _GameShell->confirm_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = _screenSize.x * 0.625;
        btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
        btn_64arg.upCode = 1351;
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1301;
        btn_64arg.downCode = 1251;
        btn_64arg.pressedCode = 0;

        if ( _GameShell->confirm_button->Add(&btn_64arg) )
        {
            btn_64arg.tileset_down = 16;
            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
            btn_64arg.tileset_up = 16;
            btn_64arg.field_3A = 16;
            btn_64arg.xpos = _screenSize.x * 0.25;
            btn_64arg.ypos = _screenSize.y * 0.4375;
            btn_64arg.caption = " ";
            btn_64arg.caption2.clear();
            btn_64arg.downCode = 0;
            btn_64arg.upCode = 0;
            btn_64arg.pressedCode = 0;
            btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
            btn_64arg.button_id = 1302;
            btn_64arg.width = _screenSize.x * 0.5;
            btn_64arg.txt_r = boxTextColor.r;
            btn_64arg.txt_g = boxTextColor.g;
            btn_64arg.txt_b = boxTextColor.b;

            if ( _GameShell->confirm_button->Add(&btn_64arg) )
            {
                btn_64arg.button_id = 1303;
                btn_64arg.ypos = _screenSize.y * 0.46875;
                btn_64arg.caption = " ";
                btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                btn_64arg.caption2.clear();

                _GameShell->confirm_button->Add(&btn_64arg);
            }
        }
    }
    NC_STACK_button::button_66arg v228;
    v228.field_4 = 0;
    v228.butID = 1300;
    _GameShell->confirm_button->Disable(&v228);

    v228.butID = 1301;
    _GameShell->confirm_button->Disable(&v228);

    _GameShell->confirm_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateInputControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;
    const int inputActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
    const int inputActionButtonStep = inputActionButtonWidth + buttonsSpace;

    GuiList::tInit args;
    args.resizeable = false;
    args.numEntries = _GameShell->InputDisplayCount();
    args.shownEntries = 8;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 8;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = dword_5A50B2_h;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = false;
    args.keyboardInput = true;
    args.wheelScroll = true;

    if ( !_GameShell->input_listview.Init(this, args) )
    {
        ypa_log_out("Unable to create Input-ListView\n");
        return false;
    }

    _GameShell->input_listview.x = posLeftPaddingX;
    _GameShell->input_listview.y = scaledFontHeight + (vertMenuSpace + _fontH) * 4;

    _GameShell->button_input_button = Nucleus::CInit<NC_STACK_button>( {
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});
    if ( !_GameShell->button_input_button )
    {
        ypa_log_out("Unable to create Input-Button\n");
        return false;
    }
    int v70 = 0;
    NC_STACK_button::button_64_arg btn_64arg;

    v70 = 0;
    btn_64arg.tileset_down = 16;
    btn_64arg.tileset_up = 16;
    btn_64arg.field_3A = 16;
    btn_64arg.xpos = 0;
    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
    btn_64arg.ypos = 0;
    btn_64arg.width = menuWidth;
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TITLE);
    btn_64arg.downCode = 0;
    btn_64arg.caption2.clear();
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.button_id = 1057;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( _GameShell->button_input_button->Add(&btn_64arg) )
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT2);
        btn_64arg.caption2.clear();
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1058;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( _GameShell->button_input_button->Add(&btn_64arg) )
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT3);
            btn_64arg.caption2.clear();
            btn_64arg.pressedCode = 0;
            btn_64arg.button_id = 1059;

            if ( _GameShell->button_input_button->Add(&btn_64arg) )
            {
                btn_64arg.xpos = 0;
                btn_64arg.ypos = 3 * (vertMenuSpace + _fontH);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.pressedCode = 0;
                btn_64arg.button_id = 1060;

                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                {
                    btn_64arg.tileset_down = 19;
                    btn_64arg.field_3A = 30;
                    btn_64arg.tileset_up = 18;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                    btn_64arg.xpos = menuWidth / 6;
                    btn_64arg.caption = "g";
                    btn_64arg.caption2 = "g";
                    btn_64arg.upCode = 1051;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.flags = 0;
                    btn_64arg.ypos = 6 * vertMenuSpace + 14 * _fontH;
                    btn_64arg.width = checkBoxWidth;
                    btn_64arg.downCode = 1050;
                    btn_64arg.button_id = 1050;

                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 16;
                        btn_64arg.tileset_up = 16;
                        btn_64arg.field_3A = 16;
                        btn_64arg.xpos = (checkBoxWidth + buttonsSpace + menuWidth / 6);
                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                        btn_64arg.width = (menuWidth / 2 - buttonsSpace);
                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_JOYSTICK);
                        btn_64arg.button_id = 2;
                        btn_64arg.caption2.clear();
                        btn_64arg.downCode = 0;
                        btn_64arg.upCode = 0;
                        btn_64arg.pressedCode = 0;
                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                        btn_64arg.txt_r = _iniColors[60].r;
                        btn_64arg.txt_g = _iniColors[60].g;
                        btn_64arg.txt_b = _iniColors[60].b;

                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                        {
                            btn_64arg.tileset_down = 19;
                            btn_64arg.field_3A = 30;
                            btn_64arg.tileset_up = 18;
                            btn_64arg.caption = "g";
                            btn_64arg.caption2 = "g";
                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                            btn_64arg.xpos = buttonsSpace + (menuWidth / 2);
                            btn_64arg.width = checkBoxWidth;
                            btn_64arg.downCode = 1058;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.button_id = 1061;
                            btn_64arg.upCode = 1059;
                            btn_64arg.flags = 0;

                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                            {
                                btn_64arg.tileset_down = 16;
                                btn_64arg.tileset_up = 16;
                                btn_64arg.field_3A = 16;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                btn_64arg.xpos = (checkBoxWidth + (menuWidth / 2) + 2 * buttonsSpace);
                                btn_64arg.width = ((menuWidth / 2) - buttonsSpace);
                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_ALTJOYMODEL);
                                btn_64arg.caption2.clear();
                                btn_64arg.downCode = 0;
                                btn_64arg.upCode = 0;
                                btn_64arg.pressedCode = 0;
                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                btn_64arg.button_id = 2;
                                btn_64arg.txt_r = _iniColors[60].r;
                                btn_64arg.txt_g = _iniColors[60].g;
                                btn_64arg.txt_b = _iniColors[60].b;

                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                {
                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.field_3A = 30;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.xpos = menuWidth / 3;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.upCode = 1055;
                                    btn_64arg.button_id = 1055;
                                    btn_64arg.ypos = 7 * vertMenuSpace + (15 * _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.flags = 0;
                                    btn_64arg.downCode = 1056;

                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                    {
                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.field_3A = 16;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.xpos = (checkBoxWidth + (menuWidth / 3) + buttonsSpace);
                                        btn_64arg.width = menuWidth / 2;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_FF);
                                        btn_64arg.button_id = 2;
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;

                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                        {
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                            btn_64arg.field_3A = 30;
                                            btn_64arg.ypos = bottomButtonsY;
                                            btn_64arg.width = inputActionButtonWidth;
                                            btn_64arg.xpos = 2 * inputActionButtonStep;
                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_I_DELETE);
                                            btn_64arg.downCode = 1251;
                                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                            btn_64arg.caption2.clear();
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.upCode = 1057;
                                            btn_64arg.button_id = 1056;
                                            btn_64arg.txt_r = _iniColors[68].r;
                                            btn_64arg.txt_g = _iniColors[68].g;
                                            btn_64arg.txt_b = _iniColors[68].b;

                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                            {
                                                // OpenNeoUA: keep all Input Settings actions aligned in one
                                                // compact bottom row: OK / Back / Delete / Reset Defaults.
                                                btn_64arg.xpos = 3 * inputActionButtonStep;
                                                btn_64arg.ypos = bottomButtonsY;
                                                btn_64arg.width = inputActionButtonWidth;
                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_RESETDEF);
                                                btn_64arg.caption2.clear();
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.upCode = 1053;
                                                btn_64arg.button_id = 1053;

                                                if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.ypos = bottomButtonsY;
                                                    btn_64arg.width = inputActionButtonWidth;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                    btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                    btn_64arg.caption2.clear();
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.button_id = 1051;
                                                    btn_64arg.upCode = 1052;
                                                    btn_64arg.downCode = 1251;

                                                    if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                    {
                                                        btn_64arg.xpos = bottomThirdBtnPosX;
                                                        btn_64arg.ypos = bottomButtonsY;
                                                        btn_64arg.width = button1LineWidth;
                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_HELP);
                                                        btn_64arg.upCode = 1250;
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.button_id = 1052;
                                                        btn_64arg.pressedCode = 0;

                                                        if ( _GameShell->button_input_button->Add(&btn_64arg) )
                                                        {
                                                            btn_64arg.xpos = inputActionButtonStep;
                                                            btn_64arg.ypos = bottomButtonsY;
                                                            btn_64arg.width = inputActionButtonWidth;
                                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                            btn_64arg.upCode = 1054;
                                                            btn_64arg.button_id = 1054;
                                                            btn_64arg.caption2.clear();
                                                            btn_64arg.pressedCode = 0;

                                                            if ( _GameShell->button_input_button->Add(&btn_64arg) )
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
            }
        }
    }
    if ( !v70 )
    {
        ypa_log_out("Unable to add input-button\n");
        return false;
    }

        // OpenNeoUA: remove deprecated online-help button from Input Settings.
    _GameShell->button_input_button->Remove(1052);

_GameShell->button_input_button->HideScreen();
    return true;
}

bool NC_STACK_ypaworld::CreateVideoControls()
{
    int menuWidth = _screenSize.x * 0.7;
    int posLeftPaddingX = (_screenSize.x - menuWidth) / 2;

    int v261 = 0;
    int v3 = 0;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();

    for ( const GFX::TGFXDeviceInfo &dev : devices )
    {
        if ( dev.isCurrent )
        {
            _GameShell->win3d_guid = dev.guid ;

            if ( !StriCmp(dev.name, "software") )
                _GameShell->win3d_name = Locale::Text::Advanced(Locale::ADV_SOFTWARE);
            else
                _GameShell->win3d_name = dev.name;

            v3 = v261;
            break;
        }
        v261++;
    }

    int v294 = menuWidth - 3 * buttonsSpace - _fontVBScrollW;
    int v94 = (menuWidth - 3 * buttonsSpace - _fontVBScrollW) * 0.6;

    GuiList::tInit args;
    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = GFX::GFXEngine::Instance.GetAvailableModes().size();
    args.shownEntries = 4;
    args.firstShownEntry = 0;
    args.selectedEntry = 0;
    args.maxShownEntries = 4;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = v94;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->video_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create Game-Video-Menu\n");
        return false;
    }

    args = GuiList::tInit();
    args.resizeable = false;
    args.numEntries = devices.size();
    args.shownEntries = 4;
    args.firstShownEntry = 0;
    args.selectedEntry = v3;
    args.maxShownEntries = 4;
    args.withIcon = false;
    args.entryHeight = _fontH;
    args.entryWidth = v94;
    args.enabled = true;
    args.vborder = _fontBorderH;
    args.instantInput = true;
    args.keyboardInput = true;

    args.wheelScroll = true;

    if ( !_GameShell->d3d_listvw.Init(this, args) )
    {
        ypa_log_out("Unable to create D3D-Menu\n");
        return false;
    }

    _GameShell->video_button = Nucleus::CInit<NC_STACK_button>({
        {NC_STACK_button::BTN_ATT_X, (int32_t)posLeftPaddingX},
        {NC_STACK_button::BTN_ATT_Y, (int32_t)scaledFontHeight},
        {NC_STACK_button::BTN_ATT_W, (int32_t)(_screenSize.x - posLeftPaddingX)},
        {NC_STACK_button::BTN_ATT_H, (int32_t)(_screenSize.y - scaledFontHeight)}});

    if ( !_GameShell->video_button )
    {
        ypa_log_out("Unable to create Video-Button\n");
        return false;
    }

    int v98 = v294 * 0.4;
    int v99 = posLeftPaddingX + buttonsSpace + v98;

    _GameShell->video_listvw.x = v99;
    _GameShell->video_listvw.y = 4 * vertMenuSpace + 4 * _fontH + scaledFontHeight; // Opens below Interface Resolution (row 3).

    _GameShell->d3d_listvw.x = v99;
    _GameShell->d3d_listvw.y = 7 * vertMenuSpace + 7 * _fontH + scaledFontHeight;

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
    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TITLE);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 0;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 1168;
    btn_64arg.txt_r = _iniColors[68].r;
    btn_64arg.txt_g = _iniColors[68].g;
    btn_64arg.txt_b = _iniColors[68].b;

    if ( true ) // OpenNeoUA: legacy Options title hidden; the screen content is self-explanatory.
    {
        btn_64arg.xpos = 0;
        btn_64arg.ypos = vertMenuSpace + _fontH;
        btn_64arg.width = menuWidth;
        btn_64arg.caption.clear();
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 1169;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( true ) // OpenNeoUA: legacy Options helper line hidden; Help button is removed.
        {
            btn_64arg.xpos = 0;
            btn_64arg.ypos = 2 * (_fontH + vertMenuSpace);
            btn_64arg.width = menuWidth;
            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TXT3);
            btn_64arg.button_id = 1170;
            btn_64arg.caption2.clear();

            if ( true ) // OpenNeoUA: legacy instruction line 2 hidden (mentioned the removed Direct3D selector)
            {
                btn_64arg.xpos = 0;
                btn_64arg.width = menuWidth;
                btn_64arg.ypos = 3 * (_fontH + vertMenuSpace);
                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_TXT4);
                btn_64arg.caption2.clear();
                btn_64arg.button_id = 1171;

                if ( true ) // OpenNeoUA: legacy instruction line 3 hidden
                {
                    btn_64arg.tileset_down = 16;
                    btn_64arg.tileset_up = 16;
                    btn_64arg.field_3A = 16;
                    btn_64arg.xpos = 0;
                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                    btn_64arg.ypos = 3 * (_fontH + vertMenuSpace); // Interface Resolution appears above Atmosphere.
                    btn_64arg.width = v98;
                    btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_INTERFACE_RESOLUTION);
                    btn_64arg.caption2.clear();
                    btn_64arg.downCode = 0;
                    btn_64arg.upCode = 0;
                    btn_64arg.pressedCode = 0;
                    btn_64arg.button_id = 2;
                    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_TEXT;
                    btn_64arg.txt_r = _iniColors[60].r;
                    btn_64arg.txt_g = _iniColors[60].g;
                    btn_64arg.txt_b = _iniColors[60].b;

                    if ( _GameShell->video_button->Add(&btn_64arg) )
                    {
                        btn_64arg.tileset_down = 19;
                        btn_64arg.field_3A = 30;
                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                        btn_64arg.caption = _gfxMode.name;
                        btn_64arg.caption2.clear();
                        btn_64arg.pressedCode = 0;
                        btn_64arg.tileset_up = 18;
                        btn_64arg.downCode = 1100;
                        btn_64arg.button_id = 1156;
                        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
                        btn_64arg.upCode = 1101;
                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                        btn_64arg.width = v294 * 0.6;
                        btn_64arg.txt_r = _iniColors[68].r;
                        btn_64arg.txt_g = _iniColors[68].g;
                        btn_64arg.txt_b = _iniColors[68].b;

                        if ( _GameShell->video_button->Add(&btn_64arg) )
                        {
                            btn_64arg.tileset_down = 16;
                            btn_64arg.tileset_up = 16;
                            btn_64arg.field_3A = 16;
                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                            btn_64arg.xpos = 0;
                            btn_64arg.ypos = 2 * (3 * (vertMenuSpace + _fontH));
                            btn_64arg.width = v294 * 0.4;
                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SEL3D);
                            btn_64arg.caption2.clear();
                            btn_64arg.downCode = 0;
                            btn_64arg.upCode = 0;
                            btn_64arg.pressedCode = 0;
                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_TEXT;
                            btn_64arg.button_id = 2;
                            btn_64arg.txt_r = _iniColors[60].r;
                            btn_64arg.txt_g = _iniColors[60].g;
                            btn_64arg.txt_b = _iniColors[60].b;

                            if ( true ) // OpenNeoUA: 3D device label hidden from Options UI (device still chosen from NUCLEUS.INI / auto)
                            {
                                btn_64arg.width = v294 * 0.6;
                                btn_64arg.tileset_down = 19;
                                btn_64arg.field_3A = 30;
                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                btn_64arg.downCode = 1134;
                                btn_64arg.upCode = 1135;
                                btn_64arg.tileset_up = 18;
                                btn_64arg.caption2.clear();
                                btn_64arg.pressedCode = 0;
                                btn_64arg.xpos = buttonsSpace + v294 * 0.4;
                                btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                btn_64arg.caption = _GameShell->win3d_name;
                                btn_64arg.button_id = 1172;
                                btn_64arg.txt_r = _iniColors[68].r;
                                btn_64arg.txt_g = _iniColors[68].g;
                                btn_64arg.txt_b = _iniColors[68].b;

                                if ( true ) // OpenNeoUA: 3D device selector hidden from Options UI (renderer = current OpenNeoUA/OpenGL device)
                                {
                                    int v117 = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;

                                    btn_64arg.tileset_down = 19;
                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                    btn_64arg.tileset_up = 18;
                                    btn_64arg.field_3A = 30;
                                    btn_64arg.xpos = 0;
                                    btn_64arg.caption = "g";
                                    btn_64arg.caption2 = "g";
                                    btn_64arg.downCode = 1102;
                                    btn_64arg.width = checkBoxWidth;
                                    btn_64arg.upCode = 1103;
                                    btn_64arg.ypos = 6 * (vertMenuSpace + _fontH);
                                    btn_64arg.pressedCode = 0;
                                    btn_64arg.flags = 0;
                                    btn_64arg.button_id = 1157;

                                    if ( true ) // OpenNeoUA: Horizon Depth checkbox hidden (gfx FARVIEW still read from NUCLEUS.INI)
                                    {
                                        int v120 = v117 / 2;

                                        btn_64arg.tileset_down = 16;
                                        btn_64arg.tileset_up = 16;
                                        btn_64arg.field_3A = 16;
                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                        btn_64arg.width = v120;
                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_FARVIEW);
                                        btn_64arg.caption2.clear();
                                        btn_64arg.downCode = 0;
                                        btn_64arg.upCode = 0;
                                        btn_64arg.pressedCode = 0;
                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                        btn_64arg.button_id = 2;
                                        btn_64arg.txt_r = _iniColors[60].r;
                                        btn_64arg.txt_g = _iniColors[60].g;
                                        btn_64arg.txt_b = _iniColors[60].b;

                                        if ( true ) // OpenNeoUA: Horizon Depth label hidden
                                        {
                                            btn_64arg.tileset_down = 19;
                                            btn_64arg.tileset_up = 18;
                                            btn_64arg.width = checkBoxWidth;
                                            btn_64arg.caption = "g";
                                            btn_64arg.caption2 = "g";
                                            btn_64arg.field_3A = 30;
                                            btn_64arg.upCode = 1107;
                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                            btn_64arg.downCode = 1106;
                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Sky -> left column, row 10
                                            btn_64arg.ypos = 10 * (vertMenuSpace + _fontH);
                                            btn_64arg.pressedCode = 0;
                                            btn_64arg.flags = 0;
                                            btn_64arg.button_id = 1160;

                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                            {
                                                btn_64arg.tileset_down = 16;
                                                btn_64arg.tileset_up = 16;
                                                btn_64arg.field_3A = 16;
                                                btn_64arg.width = v120;
                                                btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Sky label -> left column
                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SKY);
                                                btn_64arg.caption2.clear();
                                                btn_64arg.downCode = 0;
                                                btn_64arg.upCode = 0;
                                                btn_64arg.pressedCode = 0;
                                                btn_64arg.button_id = 2;
                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                {
                                                    btn_64arg.tileset_down = 19;
                                                    btn_64arg.tileset_up = 18;
                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                    btn_64arg.xpos = 0;
                                                    btn_64arg.field_3A = 30;
                                                    btn_64arg.width = checkBoxWidth;
                                                    btn_64arg.caption = "g";
                                                    btn_64arg.caption2 = "g";
                                                    btn_64arg.pressedCode = 0;
                                                    btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
                                                    btn_64arg.downCode = 1132;
                                                    btn_64arg.upCode = 1133;
                                                    btn_64arg.button_id = 1165;
                                                    btn_64arg.flags = 0;

                                                    if ( true ) // OpenNeoUA: Software Cursor checkbox hidden (gfx SOFTMOUSE still read from NUCLEUS.INI)
                                                    {
                                                        btn_64arg.tileset_down = 16;
                                                        btn_64arg.tileset_up = 16;
                                                        btn_64arg.field_3A = 16;
                                                        btn_64arg.width = v120;
                                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SWMOUSE);
                                                        btn_64arg.caption2.clear();
                                                        btn_64arg.downCode = 0;
                                                        btn_64arg.upCode = 0;
                                                        btn_64arg.pressedCode = 0;
                                                        btn_64arg.button_id = 2;
                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                        if ( true ) // OpenNeoUA: Software Cursor label hidden
                                                        {
                                                            btn_64arg.width = checkBoxWidth;
                                                            btn_64arg.tileset_down = 19;
                                                            btn_64arg.tileset_up = 18;
                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                            btn_64arg.field_3A = 30;
                                                            btn_64arg.downCode = 1130;
                                                            btn_64arg.pressedCode = 0;
                                                            btn_64arg.flags = 0;
                                                            btn_64arg.caption = "g";
                                                            btn_64arg.caption2 = "g";
                                                            btn_64arg.upCode = 1131;
                                                            btn_64arg.button_id = 1166;
                                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Windowed -> former VHS Filter position
                                                            btn_64arg.ypos = 9 * (vertMenuSpace + _fontH);

                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                            {
                                                                btn_64arg.tileset_down = 16;
                                                                btn_64arg.tileset_up = 16;
                                                                btn_64arg.field_3A = 16;
                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                btn_64arg.ypos = 9 * (vertMenuSpace + _fontH);
                                                                btn_64arg.width = v120;
                                                                btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_WINDOWEDMODE);
                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                btn_64arg.caption2.clear();
                                                                btn_64arg.downCode = 0;
                                                                btn_64arg.upCode = 0;
                                                                btn_64arg.pressedCode = 0;
                                                                btn_64arg.button_id = 2;

                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                {
                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                    btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                    btn_64arg.ypos = 7 * (vertMenuSpace + _fontH);
                                                                    btn_64arg.width = v120;
                                                                    btn_64arg.caption = Locale::Text::Advanced(Locale::ADV_USE16BIT);
                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                    btn_64arg.caption2.clear();
                                                                    btn_64arg.downCode = 0;
                                                                    btn_64arg.upCode = 0;
                                                                    btn_64arg.pressedCode = 0;
                                                                    btn_64arg.button_id = 0;

                                                                    if ( true ) // OpenNeoUA: 16-Bit Textures label hidden (gfx 16BITTEXTURE still read from NUCLEUS.INI)
                                                                    {
                                                                        btn_64arg.width = checkBoxWidth;
                                                                        btn_64arg.tileset_down = 19;
                                                                        btn_64arg.tileset_up = 18;
                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                        btn_64arg.pressedCode = 0;
                                                                        btn_64arg.flags = 0;
                                                                        btn_64arg.field_3A = 30;
                                                                        btn_64arg.xpos = 0;
                                                                        btn_64arg.button_id = 1150;
                                                                        btn_64arg.caption = "g";
                                                                        btn_64arg.caption2 = "g";
                                                                        btn_64arg.downCode = 1113;
                                                                        btn_64arg.upCode = 1114;

                                                                        if ( true ) // OpenNeoUA: 16-Bit Textures checkbox hidden
                                                                        {
                                                                            btn_64arg.tileset_down = 16;
                                                                            btn_64arg.tileset_up = 16;
                                                                            btn_64arg.field_3A = 16;
                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                            btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Music label -> left column, row 11
                                                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                                                            btn_64arg.width = v120;
                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENCDAUD);
                                                                            btn_64arg.caption2.clear();
                                                                            btn_64arg.downCode = 0;
                                                                            btn_64arg.upCode = 0;
                                                                            btn_64arg.pressedCode = 0;
                                                                            btn_64arg.button_id = 0;
                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                            {
                                                                                btn_64arg.width = checkBoxWidth;
                                                                                btn_64arg.tileset_down = 19;
                                                                                btn_64arg.tileset_up = 18;
                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                btn_64arg.field_3A = 30;
                                                                                btn_64arg.downCode = 1128;
                                                                                btn_64arg.pressedCode = 0;
                                                                                btn_64arg.flags = 0;
                                                                                btn_64arg.caption = "g";
                                                                                btn_64arg.caption2 = "g";
                                                                                btn_64arg.upCode = 1129;
                                                                                btn_64arg.button_id = 1164;
                                                                                btn_64arg.xpos = 0; // OpenNeoUA repack: Music checkbox -> left column, row 11

                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                {
                                                                                    btn_64arg.tileset_down = 19;
                                                                                    btn_64arg.field_3A = 30;
                                                                                    btn_64arg.tileset_up = 18;
                                                                                    btn_64arg.xpos = 0;
                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                    btn_64arg.width = checkBoxWidth;
                                                                                    btn_64arg.caption = "g";
                                                                                    btn_64arg.caption2 = "g";
                                                                                    btn_64arg.ypos = 8 * (vertMenuSpace + _fontH);
                                                                                    btn_64arg.downCode = 1126;
                                                                                    btn_64arg.pressedCode = 0;
                                                                                    btn_64arg.button_id = 1163;
                                                                                    btn_64arg.upCode = 1127;
                                                                                    btn_64arg.flags = 0;

                                                                                    if ( true ) // OpenNeoUA: Enemy Indicators checkbox hidden (enemyIndicator still read from NUCLEUS.INI)
                                                                                    {
                                                                                        btn_64arg.tileset_down = 16;
                                                                                        btn_64arg.tileset_up = 16;
                                                                                        btn_64arg.field_3A = 16;
                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
                                                                                        btn_64arg.width = v120 - checkBoxWidth;
                                                                                        btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_ENMYINDIC);
                                                                                        btn_64arg.caption2.clear();
                                                                                        btn_64arg.downCode = 0;
                                                                                        btn_64arg.upCode = 0;
                                                                                        btn_64arg.pressedCode = 0;
                                                                                        btn_64arg.button_id = 0;
                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                        if ( true ) // OpenNeoUA: Enemy Indicators label hidden
                                                                                        {
                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                            btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120; // OpenNeoUA repack: Host Station AI -> right column, row 11
                                                                                            btn_64arg.ypos = 11 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1137;
                                                                                            btn_64arg.upCode = 1138;
                                                                                            btn_64arg.button_id = 1174;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add player Host Station AI behavior checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = 4 * buttonsSpace + v120 + 2 * checkBoxWidth; // OpenNeoUA repack: Host Station AI label -> right column
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_HOSTSTATIONAI);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add player Host Station AI behavior label\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                            btn_64arg.xpos = 0; // OpenNeoUA repack: Spectator Mode -> left column, row 12
                                                                                            btn_64arg.ypos = 12 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1139;
                                                                                            btn_64arg.upCode = 1140;
                                                                                            btn_64arg.button_id = 1175;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add spectator mode checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = checkBoxWidth + buttonsSpace; // OpenNeoUA repack: Spectator Mode label -> left column
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_SPECTATORMODE);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 1175;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add spectator mode label\n");
                                                                                                return false;
                                                                                            }

                                                                                            // OpenNeoUA: profile-saved Retro Interface checkbox.
                                                                                            // Checked = Retro/nearest; unchecked = Smooth/linear.
                                                                                            // It shares row 13 with Spectator Mode and uses the free right column.
                                                                                            btn_64arg.width = checkBoxWidth;
                                                                                            btn_64arg.tileset_down = 19;
                                                                                            btn_64arg.tileset_up = 18;
                                                                                            btn_64arg.field_3A = 30;
                                                                                                    btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120;
                                                                                                    btn_64arg.ypos = 12 * (vertMenuSpace + _fontH);
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.flags = 0;
                                                                                            btn_64arg.caption = "g";
                                                                                            btn_64arg.caption2 = "g";
                                                                                            btn_64arg.downCode = 1314;
                                                                                            btn_64arg.upCode = 1315;
                                                                                            btn_64arg.button_id = 1189;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add Retro Interface checkbox\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = 4 * buttonsSpace + v120 + 2 * checkBoxWidth;
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_RETRO_INTERFACE);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                            btn_64arg.txt_r = _iniColors[60].r;
                                                                                            btn_64arg.txt_g = _iniColors[60].g;
                                                                                            btn_64arg.txt_b = _iniColors[60].b;

                                                                                            if ( !_GameShell->video_button->Add(&btn_64arg) )
                                                                                            {
                                                                                                ypa_log_out("Unable to add Retro Interface label\n");
                                                                                                return false;
                                                                                            }

                                                                                            btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
                                                                                            btn_64arg.tileset_down = 16;
                                                                                            btn_64arg.tileset_up = 16;
                                                                                            btn_64arg.field_3A = 16;
                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                            btn_64arg.xpos = v120 + 2 * checkBoxWidth + 4 * buttonsSpace;
                                                                                            btn_64arg.width = v120;
                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_INVERT);
                                                                                            btn_64arg.caption2.clear();
                                                                                            btn_64arg.downCode = 0;
                                                                                            btn_64arg.upCode = 0;
                                                                                            btn_64arg.pressedCode = 0;
                                                                                            btn_64arg.button_id = 0;
                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                            if ( true ) // OpenNeoUA: Stereo Reverse label hidden (SF_INVERTLR still read from NUCLEUS.INI)
                                                                                            {
                                                                                                btn_64arg.width = checkBoxWidth;
                                                                                                btn_64arg.tileset_down = 19;
                                                                                                btn_64arg.tileset_up = 18;
                                                                                                btn_64arg.pressedCode = 0;
                                                                                                btn_64arg.flags = 0;
                                                                                                btn_64arg.caption = "g";
                                                                                                btn_64arg.caption2 = "g";
                                                                                                btn_64arg.field_3A = 30;
                                                                                                btn_64arg.button_id = 1151;
                                                                                                btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + v120;
                                                                                                btn_64arg.downCode = 1112;
                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
                                                                                                btn_64arg.upCode = 1111;

                                                                                                if ( true ) // OpenNeoUA: Stereo Reverse checkbox hidden
                                                                                                {
                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                    btn_64arg.field_3A = 16;
                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                    btn_64arg.xpos = 0;
                                                                                                btn_64arg.ypos = 13 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Explosion Effects row 13
                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                    btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_DESTRFX);
                                                                                                    btn_64arg.caption2.clear();
                                                                                                    btn_64arg.downCode = 0;
                                                                                                    btn_64arg.upCode = 0;
                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                    btn_64arg.button_id = 2;
                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                    {
                                                                                                        NC_STACK_button::Slider v225;

                                                                                                        v225.value = 8;
                                                                                                        v225.max = 16;
                                                                                                        v225.min = 0;

                                                                                                        btn_64arg.caption2.clear();
                                                                                                        btn_64arg.tileset_down = 18;
                                                                                                        btn_64arg.tileset_up = 18;
                                                                                                        btn_64arg.field_3A = 30;
                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                        btn_64arg.pressedCode = 1110;
                                                                                                        btn_64arg.button_id = 1159;
                                                                                                        btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                        btn_64arg.caption = " ";
                                                                                                        btn_64arg.downCode = 1108;
                                                                                                        btn_64arg.flags = 0;
                                                                                                        btn_64arg.field_34 = &v225;
                                                                                                        btn_64arg.upCode = 1109;
                                                                                                        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;

                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                        {
                                                                                                            btn_64arg.tileset_down = 16;
                                                                                                            btn_64arg.tileset_up = 16;
                                                                                                            btn_64arg.field_3A = 16;
                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                            btn_64arg.caption2.clear();
                                                                                                            btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                            btn_64arg.downCode = 0;
                                                                                                            btn_64arg.upCode = 0;
                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                            btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                            btn_64arg.button_id = 1158;
                                                                                                            btn_64arg.caption = " 4";
                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                            {
                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                btn_64arg.xpos = 0;
                                                                                                                btn_64arg.ypos = 14 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Sound Volume row 14
                                                                                                                btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_FXVOL);
                                                                                                                btn_64arg.caption2.clear();
                                                                                                                btn_64arg.downCode = 0;
                                                                                                                btn_64arg.upCode = 0;
                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                btn_64arg.button_id = 2;
                                                                                                                btn_64arg.flags = NC_STACK_button::FLAG_TEXT;

                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                {
                                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;

                                                                                                                    v225.min = 1;
                                                                                                                    v225.max = 127;
                                                                                                                    v225.value = 100;

                                                                                                                    btn_64arg.field_3A = 30;
                                                                                                                    btn_64arg.tileset_down = 18;
                                                                                                                    btn_64arg.tileset_up = 18;
                                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                                    btn_64arg.caption2.clear();
                                                                                                                    btn_64arg.button_id = 1152;
                                                                                                                    btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                    btn_64arg.caption = " ";
                                                                                                                    btn_64arg.downCode = 1115;
                                                                                                                    btn_64arg.upCode = 1117;
                                                                                                                    btn_64arg.field_34 = &v225;
                                                                                                                    btn_64arg.pressedCode = 1116;
                                                                                                                    btn_64arg.flags = 0;

                                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                    {
                                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                        btn_64arg.tileset_down = 16;
                                                                                                                        btn_64arg.tileset_up = 16;
                                                                                                                        btn_64arg.field_3A = 16;
                                                                                                                        btn_64arg.caption = "4";
                                                                                                                        btn_64arg.button_id = 1153;
                                                                                                                        btn_64arg.caption2.clear();
                                                                                                                        btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                                        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                                        btn_64arg.downCode = 0;
                                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                                        btn_64arg.upCode = 0;
                                                                                                                        btn_64arg.pressedCode = 0;

                                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                        {
                                                                                                                            btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                            btn_64arg.xpos = 0;
                                                                                                                            btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                            btn_64arg.ypos = 15 * (vertMenuSpace + _fontH); // OpenNeoUA repack: Music Volume row 15
                                                                                                                            btn_64arg.caption = Locale::Text::Dialogs(Locale::DLG_S_CDVOL);
                                                                                                                            btn_64arg.caption2.clear();
                                                                                                                            btn_64arg.downCode = 0;
                                                                                                                            btn_64arg.upCode = 0;
                                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                                            btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
                                                                                                                            btn_64arg.button_id = 2;

                                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                            {
                                                                                                                                btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;
                                                                                                                                v225.min = 1;
                                                                                                                                v225.max = 127;
                                                                                                                                v225.value = 100;

                                                                                                                                btn_64arg.tileset_down = 18;
                                                                                                                                btn_64arg.tileset_up = 18;
                                                                                                                                btn_64arg.upCode = 1120;
                                                                                                                                btn_64arg.field_3A = 30;
                                                                                                                                btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
                                                                                                                                btn_64arg.caption2.clear();
                                                                                                                                btn_64arg.downCode = 1118;
                                                                                                                                btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
                                                                                                                                btn_64arg.caption = " ";
                                                                                                                                btn_64arg.pressedCode = 1119;
                                                                                                                                btn_64arg.field_34 = &v225;
                                                                                                                                btn_64arg.flags = 0;
                                                                                                                                btn_64arg.button_id = 1154;

                                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                {
                                                                                                                                    btn_64arg.tileset_down = 16;
                                                                                                                                    btn_64arg.tileset_up = 16;
                                                                                                                                    btn_64arg.field_3A = 16;
                                                                                                                                    btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
                                                                                                                                    btn_64arg.caption = "4";
                                                                                                                                    btn_64arg.caption2.clear();
                                                                                                                                    btn_64arg.downCode = 0;
                                                                                                                                    btn_64arg.upCode = 0;
                                                                                                                                    btn_64arg.pressedCode = 0;
                                                                                                                                    btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
                                                                                                                                    btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
                                                                                                                                    btn_64arg.button_id = 1155;
                                                                                                                                    btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;

                                                                                                                                    if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                    {
                                                                                                                                        btn_64arg.tileset_up = 18;
                                                                                                                                        btn_64arg.field_3A = 30;
                                                                                                                                        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
                                                                                                                                        const int optionsActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
                                                                                                                                        const int optionsActionButtonStep = optionsActionButtonWidth + buttonsSpace;

                                                                                                                                        btn_64arg.xpos = 0;
                                                                                                                                        btn_64arg.ypos = bottomButtonsY;
                                                                                                                                        btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                        btn_64arg.tileset_down = 19;
                                                                                                                                        btn_64arg.caption = Locale::Text::Common(Locale::CMN_OK);
                                                                                                                                        btn_64arg.upCode = 1124;
                                                                                                                                        btn_64arg.caption2.clear();
                                                                                                                                        btn_64arg.downCode = 0;
                                                                                                                                        btn_64arg.pressedCode = 0;
                                                                                                                                        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
                                                                                                                                        btn_64arg.button_id = 1161;
                                                                                                                                        btn_64arg.txt_r = _iniColors[68].r;
                                                                                                                                        btn_64arg.txt_g = _iniColors[68].g;
                                                                                                                                        btn_64arg.txt_b = _iniColors[68].b;

                                                                                                                                        if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                        {
                                                                                                                                            btn_64arg.xpos = 2 * optionsActionButtonStep;
                                                                                                                                            btn_64arg.ypos = bottomButtonsY;
                                                                                                                                            btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                            btn_64arg.caption = Locale::Text::Common(Locale::CMN_RESETDEF);
                                                                                                                                            btn_64arg.upCode = 1321;
                                                                                                                                            btn_64arg.caption2.clear();
                                                                                                                                            btn_64arg.downCode = 0;
                                                                                                                                            btn_64arg.pressedCode = 0;
                                                                                                                                            btn_64arg.button_id = 1194;

                                                                                                                                            if ( _GameShell->video_button->Add(&btn_64arg) )
                                                                                                                                            {
                                                                                                                                                btn_64arg.xpos = optionsActionButtonStep;
                                                                                                                                                btn_64arg.ypos = bottomButtonsY;
                                                                                                                                                btn_64arg.width = optionsActionButtonWidth;
                                                                                                                                                btn_64arg.caption = Locale::Text::Common(Locale::CMN_CANCEL);
                                                                                                                                                btn_64arg.upCode = 1125;
                                                                                                                                                btn_64arg.caption2.clear();
                                                                                                                                                btn_64arg.downCode = 0;
                                                                                                                                                btn_64arg.pressedCode = 0;
                                                                                                                                                btn_64arg.button_id = 1162;

                                                                                                                                                if ( _GameShell->video_button->Add(&btn_64arg) )
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
        ypa_log_out("Unable to add video-button\n");
        return false;
    }

    // OpenNeoUA: global mission ambience volume. This is intentionally placed
    // directly under the existing Music Volume row and persists to
    // game.ambient_sound_volume in Nucleus.ini.
    {
        NC_STACK_button::Slider ambientSlider;
        ambientSlider.min = 0;
        ambientSlider.max = 127;
        ambientSlider.value = 100;

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 16 * (vertMenuSpace + _fontH);
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_AMBIENT_VOLUME);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume label\n");
            return false;
        }

        btn_64arg.tileset_down = 18;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_SLIDER;
        btn_64arg.xpos = buttonsSpace + (dword_5A50B2 - 5 * buttonsSpace) * 0.3;
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.55;
        btn_64arg.caption = " ";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 1141;
        btn_64arg.pressedCode = 1142;
        btn_64arg.upCode = 1143;
        btn_64arg.button_id = 1191;
        btn_64arg.flags = 0;
        btn_64arg.field_34 = &ambientSlider;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume slider\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = (2 * buttonsSpace) + (dword_5A50B2 - 5 * buttonsSpace) * 0.85;
        btn_64arg.width = (dword_5A50B2 - 5 * buttonsSpace) * 0.15;
        btn_64arg.caption = "100";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1192;
        btn_64arg.flags = NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Ambient Volume value\n");
            return false;
        }
    }

    btn_64arg.tileset_down = 19;
    btn_64arg.tileset_up = 18;
    btn_64arg.field_3A = 30;
    btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
    // OpenNeoUA: fourth compact bottom action after OK / Back / Reset Defaults.
    const int optionsActionButtonWidth = (menuWidth - 3 * buttonsSpace) / 4;
    const int optionsActionButtonStep = optionsActionButtonWidth + buttonsSpace;
    btn_64arg.xpos = 3 * optionsActionButtonStep;
    btn_64arg.ypos = bottomButtonsY;
    btn_64arg.width = optionsActionButtonWidth;
    btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_MORE_OPTIONS);
    btn_64arg.caption2.clear();
    btn_64arg.downCode = 0;
    btn_64arg.upCode = 1320;
    btn_64arg.pressedCode = 0;
    btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
    btn_64arg.button_id = 2;
    btn_64arg.txt_r = _iniColors[60].r;
    btn_64arg.txt_g = _iniColors[60].g;
    btn_64arg.txt_b = _iniColors[60].b;

    if ( !_GameShell->video_button->Add(&btn_64arg) )
    {
        ypa_log_out("Unable to add More Options button\n");
        return false;
    }

    // --- Enable Play As checkbox (row 9, free right column) ---
    // Disabled by default: the briefing keeps the original Resistance-only
    // presentation until the player explicitly enables the optional selector.
    {
        int playAsAvailableWidth = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        int playAsColumnWidth = playAsAvailableWidth / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + playAsColumnWidth;
        btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1316;
        btn_64arg.upCode = 1317;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1190;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Enable Play As checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 4 * buttonsSpace + playAsColumnWidth + 2 * checkBoxWidth;
        btn_64arg.ypos = 9 * (_fontH + vertMenuSpace);
        btn_64arg.width = playAsColumnWidth;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_ENABLE_PLAY_AS);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 0;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Enable Play As label\n");
            return false;
        }
    }

    // ===== OpenNeoUA: modern graphics options =====================================
    // Blending, menu font, frame limit and the remaining main-page controls.
    {
        // --- Blending label + cycle-button (row 4, left column) ---
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_BLENDING);
        btn_64arg.caption2.clear();
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Blending label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 4 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_DEFAULT);
        btn_64arg.caption2.clear();
        btn_64arg.upCode = 1306;
        btn_64arg.button_id = 1183;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Blending button\n");
            return false;
        }

        // --- Menu Font cycle-button (row 5, left column) ---
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 5 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_MENU_FONT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Menu Font label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 5 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_DEFAULT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1311;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1186;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Menu Font button\n");
            return false;
        }

    }

        // --- Intro Movies checkbox (row 9, right column) ---
    {
        int gv117 = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        int gv120 = gv117 / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + gv120;
        btn_64arg.ypos = 10 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1307;
        btn_64arg.upCode = 1308;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1184;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Intro Movies checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 4 * buttonsSpace + gv120 + 2 * checkBoxWidth;
        btn_64arg.width = gv120;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_INTRO_MOVIES);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Intro Movies label\n");
            return false;
        }
    }

    // --- FPS Limit cycle-button (row 6, below Menu Font) ---
    {
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_FPS_LIMIT);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add FPS Limit label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 6 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = "60";
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1312;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1187;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add FPS Limit button\n");
            return false;
        }
    }

    // --- Genesis List Order cycle-button (row 7, below FPS Limit) ---
    {
        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
        btn_64arg.width = v98;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_GENESIS_LIST_ORDER);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 2;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Genesis List Order label\n");
            return false;
        }

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_BUTTON;
        btn_64arg.xpos = buttonsSpace + v294 * 0.4;
        btn_64arg.ypos = 7 * (_fontH + vertMenuSpace);
        btn_64arg.width = v294 * 0.6;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_GENESIS_ORDER_ENERGY);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 1313;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1188;
        btn_64arg.flags = NC_STACK_button::FLAG_BORDER | NC_STACK_button::FLAG_CENTER | NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[68].r;
        btn_64arg.txt_g = _iniColors[68].g;
        btn_64arg.txt_b = _iniColors[68].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Genesis List Order button\n");
            return false;
        }
    }

    // ===== end OpenNeoUA modern graphics options ==================================

    // OpenNeoUA: visual-only map boundary wall toggle (left column, row 8).
    // Unchecked by default: vanilla RAND boundary walls remain visible.
    {
        const int availableWidth = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        const int columnWidth = availableWidth / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 0;
        btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1318;
        btn_64arg.upCode = 1319;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1193;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Hide Map Border Walls checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = checkBoxWidth + buttonsSpace;
        btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
        btn_64arg.width = columnWidth;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_HIDE_MAP_BORDER_WALLS);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 0;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Hide Map Border Walls label\n");
            return false;
        }
    }

    // OpenNeoUA: alpha mouse cursors toggle (right column, same row as Hide Map Border Walls).
    // Unchecked by default: the release cursors of Res stay in use.
    {
        const int availableWidth = dword_5A50B2 - 6 * buttonsSpace - 2 * checkBoxWidth;
        const int columnWidth = availableWidth / 2;

        btn_64arg.tileset_down = 19;
        btn_64arg.tileset_up = 18;
        btn_64arg.field_3A = 30;
        btn_64arg.button_type = NC_STACK_button::TYPE_CHECKBX;
        btn_64arg.xpos = 3 * buttonsSpace + checkBoxWidth + columnWidth;
        btn_64arg.ypos = 8 * (_fontH + vertMenuSpace);
        btn_64arg.width = checkBoxWidth;
        btn_64arg.caption = "g";
        btn_64arg.caption2 = "g";
        btn_64arg.downCode = 1322;
        btn_64arg.upCode = 1323;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 1196;
        btn_64arg.flags = 0;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Retro Cursors checkbox\n");
            return false;
        }

        btn_64arg.tileset_down = 16;
        btn_64arg.tileset_up = 16;
        btn_64arg.field_3A = 16;
        btn_64arg.button_type = NC_STACK_button::TYPE_CAPTION;
        btn_64arg.xpos = 4 * buttonsSpace + columnWidth + 2 * checkBoxWidth;
        btn_64arg.width = columnWidth;
        btn_64arg.caption = Locale::Text::OpenUA(Locale::OUA_RETRO_CURSORS);
        btn_64arg.caption2.clear();
        btn_64arg.downCode = 0;
        btn_64arg.upCode = 0;
        btn_64arg.pressedCode = 0;
        btn_64arg.button_id = 0;
        btn_64arg.flags = NC_STACK_button::FLAG_TEXT;
        btn_64arg.txt_r = _iniColors[60].r;
        btn_64arg.txt_g = _iniColors[60].g;
        btn_64arg.txt_b = _iniColors[60].b;

        if ( !_GameShell->video_button->Add(&btn_64arg) )
        {
            ypa_log_out("Unable to add Retro Cursors label\n");
            return false;
        }
    }

    _GameShell->UpdatePaletteThemeText();
    _GameShell->UpdateGfxOptionTexts();
    _GameShell->UpdateMenuFontText();

    NC_STACK_button::button_66arg v229;
    v229.butID = 1151;
    v229.field_4 = ((_GameShell->soundFlags & World::SF_INVERTLR) == 0) + 1;

    _GameShell->video_button->SetState(&v229);

_GameShell->video_button->HideScreen();
    return true;
}
