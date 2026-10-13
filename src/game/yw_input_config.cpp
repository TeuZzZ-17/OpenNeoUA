// Key configuration: key names, conflicts, capture and the input settings pages.
#include <inttypes.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/font.h"
#include "gui/button.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include <math.h>
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "system/system.h"
#include "nucleus/loaders.h"
#include "nucleus/obj3d.h"
#include "game/ypaflyer.h"
#include "common/utils.h"
#include "nucleus/IFFile.h"
#include "world/gunrotation.h"
#include "game/yw_shell_menus.h"

std::string InputKeyDisplayTitle(int16_t keyCode)
{
    // Mouse buttons can be described in the menu without making them newly
    // remappable: key-capture support still depends on the legacy KeyTitle
    // table, while this helper is presentation-only.
    if ( keyCode == Input::KC_LMB )
        return "LMB";
    if ( keyCode == Input::KC_RMB )
        return "RMB";

    if ( keyCode > Input::KC_NONE && keyCode < Input::KC_MAX )
        return Input::Engine.KeyTitle.at(keyCode);

    return std::string();
}

std::string InputFixedShortcutTitle(int binding)
{
    const World::TInputFixedShortcut shortcut = World::GetInputFixedShortcut(binding);

    if ( shortcut.Kind == World::INPUT_FIXED_SHORTCUT_KEY )
        return InputKeyDisplayTitle(shortcut.KeyCode);

    if ( shortcut.Kind == World::INPUT_FIXED_SHORTCUT_WHEEL )
    {
        if ( shortcut.WheelDirection > 0 )
            return "MWU";
        if ( shortcut.WheelDirection < 0 )
            return "MWD";
    }

    return std::string();
}

bool InputKeyUsesOtherSlot(const UserData *usr, int target, bool positiveSlot, int16_t keyCode)
{
    if ( !usr || keyCode == Input::KC_NONE )
        return false;

    for ( int i = 1; i < World::INPUT_BIND_MAX; ++i )
    {
        if ( UserData::IsInputBindingRetired(i) )
            continue;

        const UserData::TInputConf &cfg = usr->InputConfig[i];
        if ( cfg.PKeyCode == keyCode && !(i == target && positiveSlot) )
            return true;
        if ( cfg.NKeyCode == keyCode && !(i == target && !positiveSlot) )
            return true;
    }

    return false;
}

std::string InputKeyConflictNames(const UserData *usr, int target, bool positiveSlot, int16_t keyCode)
{
    std::vector<std::string> names;

    for ( int i = 1; i < World::INPUT_BIND_MAX; ++i )
    {
        if ( UserData::IsInputBindingRetired(i) )
            continue;

        const UserData::TInputConf &cfg = usr->InputConfig[i];
        const bool conflict =
            (cfg.PKeyCode == keyCode && !(i == target && positiveSlot)) ||
            (cfg.NKeyCode == keyCode && !(i == target && !positiveSlot));

        if ( !conflict )
            continue;

        std::string title = usr->InputConfigTitle[i];
        if ( title.empty() )
            title = fmt::sprintf("Action %d", i);

        if ( std::find(names.begin(), names.end(), title) == names.end() )
            names.push_back(title);
    }

    if ( names.empty() )
        return "another action";

    std::string result = names[0];
    if ( names.size() >= 2 )
        result += " / " + names[1];
    if ( names.size() > 2 )
        result += fmt::sprintf(" (+%d)", (int)names.size() - 2);

    return result;
}

void ClearInputKeyConflicts(UserData *usr, int target, bool positiveSlot, int16_t keyCode)
{
    for ( int i = 1; i < World::INPUT_BIND_MAX; ++i )
    {
        if ( UserData::IsInputBindingRetired(i) )
            continue;

        UserData::TInputConf &cfg = usr->InputConfig[i];

        if ( cfg.PKeyCode == keyCode && !(i == target && positiveSlot) )
            cfg.PKeyCode = Input::KC_NONE;
        if ( cfg.NKeyCode == keyCode && !(i == target && !positiveSlot) )
            cfg.NKeyCode = Input::KC_NONE;
    }
}

void ApplyCapturedInputKey(UserData *usr, int target, bool positiveSlot, int16_t keyCode)
{
    if ( !usr || target <= 0 || target >= World::INPUT_BIND_MAX )
        return;

    UserData::TInputConf &cfg = usr->InputConfig[target];

    if ( positiveSlot )
    {
        cfg.PKeyCode = keyCode;

        if ( cfg.Type == World::INPUT_BIND_TYPE_SLIDER )
            usr->confFirstKey = false;

        usr->keyCatchMode = false;
        cfg.SetFlags = 0;
    }
    else
    {
        cfg.NKeyCode = keyCode;
        cfg.SetFlags &= ~UserData::TInputConf::IF_SECOND;
        usr->confFirstKey = true;
    }
}

void ClearPendingInputKey(UserData *usr)
{
    usr->pendingInputTarget = -1;
    usr->pendingInputKeyCode = Input::KC_NONE;
    usr->pendingInputPositiveSlot = true;
}

void yw_draw_input_list(NC_STACK_ypaworld *yw, UserData *usr)
{
    usr->input_listview.SetRect(yw, -2, -2);
    GFX::Engine.GetTileset(0);

    usr->input_listview.itemBlock.clear();

    usr->input_listview.ItemsPreLayout(yw, &usr->input_listview.itemBlock, 0, "uvw");

    for (int i = 1; i <= usr->input_listview.shownEntries; i++ )
    {
        const int displayIndex = (i - 1) + usr->input_listview.firstShownEntries;
        int v24 = usr->InputBindingFromDisplayIndex(displayIndex);
        if ( v24 > 0 && !usr->InputConfigTitle[v24].empty() )
        {
            FontUA::ColumnItem a1a[2];

            int v33;
            int v31;
            int v32;
            int v30;

            if ( v24 == usr->inpListActiveElement )
            {
                v30 = 98;
                v31 = 100;
                v32 = 9;
                v33 = 99;
            }
            else
            {
                v30 = 102;
                v31 = 102;
                v32 = 0;
                v33 = 102;
            }

            int v34 = usr->input_listview.entryWidth - 2 * usr->p_YW->_fontBorderW + 1;

            std::string v19;

            if ( usr->InputConfig[ v24 ].Type == World::INPUT_BIND_TYPE_SLIDER )
            {
                std::string negKeyName, posKeyName;
                if ( usr->InputConfig[ v24 ].NKeyCode )
                    negKeyName = InputKeyDisplayTitle(usr->InputConfig[v24].NKeyCode);
                else
                    negKeyName = "-";

                if ( usr->InputConfig[ v24 ].PKeyCode )
                    posKeyName = InputKeyDisplayTitle(usr->InputConfig[v24].PKeyCode);
                else
                    posKeyName = "-";

                if ( usr->InputConfig[ v24 ].SetFlags & UserData::TInputConf::IF_FIRST )
                    posKeyName = Locale::Text::Dialogs(Locale::DLG_I_UNK);

                if ( usr->InputConfig[ v24 ].SetFlags & UserData::TInputConf::IF_SECOND )
                    negKeyName = Locale::Text::Dialogs(Locale::DLG_I_UNK);

                v19 = fmt::sprintf("%s/%s", negKeyName, posKeyName);
            }
            else
            {
                if ( usr->InputConfig[ v24 ].PKeyCode )
                    v19 = InputKeyDisplayTitle(usr->InputConfig[v24].PKeyCode);
                else
                    v19 = "-";

                if ( usr->InputConfig[ v24 ].SetFlags & UserData::TInputConf::IF_FIRST )
                    v19 = Locale::Text::Dialogs(Locale::DLG_I_UNK);

                const World::TInputFixedShortcut fixedShortcut = World::GetInputFixedShortcut(v24);
                const std::string fixedShortcutTitle = InputFixedShortcutTitle(v24);
                const bool duplicatesPrimary =
                    fixedShortcut.Kind == World::INPUT_FIXED_SHORTCUT_KEY &&
                    usr->InputConfig[v24].PKeyCode == fixedShortcut.KeyCode;

                if ( !fixedShortcutTitle.empty() && !duplicatesPrimary )
                    v19 += ", " + fixedShortcutTitle;
            }

            a1a[0].txt = usr->InputConfigTitle[v24];
            a1a[0].width = v34 * 0.68;
            a1a[0].fontID = v32;
            a1a[0].spaceChar = v33;
            a1a[0].prefixChar = v30;
            a1a[0].postfixChar = 0;
            a1a[0].flags = 37;

            a1a[1].txt = v19;
            a1a[1].width = v34 - v34 * 0.68;
            a1a[1].fontID = v32;
            a1a[1].spaceChar = v33;
            a1a[1].prefixChar = 0;
            a1a[1].postfixChar = v31;
            a1a[1].flags = 38;

            FontUA::select_tileset(&usr->input_listview.itemBlock, 0);
            FontUA::store_s8(&usr->input_listview.itemBlock, '{'); // Left wnd border

            if ( v24 == usr->inpListActiveElement )
            {
                FontUA::set_txtColor(&usr->input_listview.itemBlock, usr->p_YW->_iniColors[62].r, usr->p_YW->_iniColors[62].g, usr->p_YW->_iniColors[62].b);
            }
            else
            {
                FontUA::set_txtColor(&usr->input_listview.itemBlock, usr->p_YW->_iniColors[61].r, usr->p_YW->_iniColors[61].g, usr->p_YW->_iniColors[61].b);
            }

            FormateColumnItem(yw, &usr->input_listview.itemBlock, 2, a1a);

            FontUA::select_tileset(&usr->input_listview.itemBlock, 0);
            FontUA::store_s8(&usr->input_listview.itemBlock, '}'); // Right wnd border
            FontUA::next_line(&usr->input_listview.itemBlock);
        }
    }
    usr->input_listview.ItemsPostLayout(yw, &usr->input_listview.itemBlock, 0, "xyz");
    FontUA::set_end(&usr->input_listview.itemBlock);

    GFX::Engine.ProcessDrawSeq(usr->input_listview.cmdCommands, &usr->input_listview.cmdInclude);
}

void NC_STACK_ypaworld::LoadKeyNames()
{
    for (std::string &a: Input::Engine.KeyTitle)
        a.clear();

    Input::Engine.KeyTitle[Input::KC_NONE]       = "*"; // Locale::Text::GetKeyNameString(Locale::KEYNAME_NOP);
    Input::Engine.KeyTitle[Input::KC_ESCAPE]     = Locale::Text::KeyName(Locale::KEYNAME_ESC);
    Input::Engine.KeyTitle[Input::KC_SPACE]      = Locale::Text::KeyName(Locale::KEYNAME_SPACE);
    Input::Engine.KeyTitle[Input::KC_UP]         = Locale::Text::KeyName(Locale::KEYNAME_UP);
    Input::Engine.KeyTitle[Input::KC_DOWN]       = Locale::Text::KeyName(Locale::KEYNAME_DOWN);
    Input::Engine.KeyTitle[Input::KC_LEFT]       = Locale::Text::KeyName(Locale::KEYNAME_LEFT);
    Input::Engine.KeyTitle[Input::KC_RIGHT]      = Locale::Text::KeyName(Locale::KEYNAME_RIGHT);
    Input::Engine.KeyTitle[Input::KC_F1]         = Locale::Text::KeyName(Locale::KEYNAME_F1);
    Input::Engine.KeyTitle[Input::KC_F2]         = Locale::Text::KeyName(Locale::KEYNAME_F2);
    Input::Engine.KeyTitle[Input::KC_F3]         = Locale::Text::KeyName(Locale::KEYNAME_F3);
    Input::Engine.KeyTitle[Input::KC_F4]         = Locale::Text::KeyName(Locale::KEYNAME_F4);
    Input::Engine.KeyTitle[Input::KC_F5]         = Locale::Text::KeyName(Locale::KEYNAME_F5);
    Input::Engine.KeyTitle[Input::KC_F6]         = Locale::Text::KeyName(Locale::KEYNAME_F6);
    Input::Engine.KeyTitle[Input::KC_F7]         = Locale::Text::KeyName(Locale::KEYNAME_F7);
    Input::Engine.KeyTitle[Input::KC_F8]         = Locale::Text::KeyName(Locale::KEYNAME_F8);
    Input::Engine.KeyTitle[Input::KC_F9]         = Locale::Text::KeyName(Locale::KEYNAME_F9);
    Input::Engine.KeyTitle[Input::KC_F10]        = Locale::Text::KeyName(Locale::KEYNAME_F10);
    Input::Engine.KeyTitle[Input::KC_F11]        = Locale::Text::KeyName(Locale::KEYNAME_F11);
    Input::Engine.KeyTitle[Input::KC_F12]        = Locale::Text::KeyName(Locale::KEYNAME_F12);
    Input::Engine.KeyTitle[Input::KC_BACKSPACE]  = Locale::Text::KeyName(Locale::KEYNAME_BACK);
    Input::Engine.KeyTitle[Input::KC_TAB]        = Locale::Text::KeyName(Locale::KEYNAME_TAB);
    Input::Engine.KeyTitle[Input::KC_CLEAR]      = Locale::Text::KeyName(Locale::KEYNAME_CLEAR);
    Input::Engine.KeyTitle[Input::KC_RETURN]     = Locale::Text::KeyName(Locale::KEYNAME_RETURN);
    Input::Engine.KeyTitle[Input::KC_CTRL]       = Locale::Text::KeyName(Locale::KEYNAME_CTRL);
    Input::Engine.KeyTitle[Input::KC_SHIFT]      = Locale::Text::KeyName(Locale::KEYNAME_SHIFT);
    Input::Engine.KeyTitle[Input::KC_ALT]        = Locale::Text::KeyName(Locale::KEYNAME_ALT);
    Input::Engine.KeyTitle[Input::KC_PAUSE]      = Locale::Text::KeyName(Locale::KEYNAME_PAUSE);
    Input::Engine.KeyTitle[Input::KC_PGUP]       = Locale::Text::KeyName(Locale::KEYNAME_PGUP);
    Input::Engine.KeyTitle[Input::KC_PGDOWN]     = Locale::Text::KeyName(Locale::KEYNAME_PGDOWN);
    Input::Engine.KeyTitle[Input::KC_END]        = Locale::Text::KeyName(Locale::KEYNAME_END);
    Input::Engine.KeyTitle[Input::KC_HOME]       = Locale::Text::KeyName(Locale::KEYNAME_HOME);
    Input::Engine.KeyTitle[Input::KC_SELECT]     = Locale::Text::KeyName(Locale::KEYNAME_SELECT);
    Input::Engine.KeyTitle[Input::KC_EXECUTE]    = Locale::Text::KeyName(Locale::KEYNAME_EXEC);
    Input::Engine.KeyTitle[Input::KC_SNAPSHOT]   = Locale::Text::KeyName(Locale::KEYNAME_PRINT);
    Input::Engine.KeyTitle[Input::KC_INSERT]     = Locale::Text::KeyName(Locale::KEYNAME_INS);
    Input::Engine.KeyTitle[Input::KC_DELETE]     = Locale::Text::KeyName(Locale::KEYNAME_DEL);
    Input::Engine.KeyTitle[Input::KC_HELP]       = Locale::Text::KeyName(Locale::KEYNAME_HELP);
    Input::Engine.KeyTitle[Input::KC_1]          = Locale::Text::KeyName(Locale::KEYNAME_1);
    Input::Engine.KeyTitle[Input::KC_2]          = Locale::Text::KeyName(Locale::KEYNAME_2);
    Input::Engine.KeyTitle[Input::KC_3]          = Locale::Text::KeyName(Locale::KEYNAME_3);
    Input::Engine.KeyTitle[Input::KC_4]          = Locale::Text::KeyName(Locale::KEYNAME_4);
    Input::Engine.KeyTitle[Input::KC_5]          = Locale::Text::KeyName(Locale::KEYNAME_5);
    Input::Engine.KeyTitle[Input::KC_6]          = Locale::Text::KeyName(Locale::KEYNAME_6);
    Input::Engine.KeyTitle[Input::KC_7]          = Locale::Text::KeyName(Locale::KEYNAME_7);
    Input::Engine.KeyTitle[Input::KC_8]          = Locale::Text::KeyName(Locale::KEYNAME_8);
    Input::Engine.KeyTitle[Input::KC_9]          = Locale::Text::KeyName(Locale::KEYNAME_9);
    Input::Engine.KeyTitle[Input::KC_0]          = Locale::Text::KeyName(Locale::KEYNAME_0);
    Input::Engine.KeyTitle[Input::KC_A]          = Locale::Text::KeyName(Locale::KEYNAME_A);
    Input::Engine.KeyTitle[Input::KC_B]          = Locale::Text::KeyName(Locale::KEYNAME_B);
    Input::Engine.KeyTitle[Input::KC_C]          = Locale::Text::KeyName(Locale::KEYNAME_C);
    Input::Engine.KeyTitle[Input::KC_D]          = Locale::Text::KeyName(Locale::KEYNAME_D);
    Input::Engine.KeyTitle[Input::KC_E]          = Locale::Text::KeyName(Locale::KEYNAME_E);
    Input::Engine.KeyTitle[Input::KC_F]          = Locale::Text::KeyName(Locale::KEYNAME_F);
    Input::Engine.KeyTitle[Input::KC_G]          = Locale::Text::KeyName(Locale::KEYNAME_G);
    Input::Engine.KeyTitle[Input::KC_H]          = Locale::Text::KeyName(Locale::KEYNAME_H);
    Input::Engine.KeyTitle[Input::KC_I]          = Locale::Text::KeyName(Locale::KEYNAME_I);
    Input::Engine.KeyTitle[Input::KC_J]          = Locale::Text::KeyName(Locale::KEYNAME_J);
    Input::Engine.KeyTitle[Input::KC_K]          = Locale::Text::KeyName(Locale::KEYNAME_K);
    Input::Engine.KeyTitle[Input::KC_L]          = Locale::Text::KeyName(Locale::KEYNAME_L);
    Input::Engine.KeyTitle[Input::KC_M]          = Locale::Text::KeyName(Locale::KEYNAME_M);
    Input::Engine.KeyTitle[Input::KC_N]          = Locale::Text::KeyName(Locale::KEYNAME_N);
    Input::Engine.KeyTitle[Input::KC_O]          = Locale::Text::KeyName(Locale::KEYNAME_O);
    Input::Engine.KeyTitle[Input::KC_P]          = Locale::Text::KeyName(Locale::KEYNAME_P);
    Input::Engine.KeyTitle[Input::KC_Q]          = Locale::Text::KeyName(Locale::KEYNAME_Q);
    Input::Engine.KeyTitle[Input::KC_R]          = Locale::Text::KeyName(Locale::KEYNAME_R);
    Input::Engine.KeyTitle[Input::KC_S]          = Locale::Text::KeyName(Locale::KEYNAME_S);
    Input::Engine.KeyTitle[Input::KC_T]          = Locale::Text::KeyName(Locale::KEYNAME_T);
    Input::Engine.KeyTitle[Input::KC_U]          = Locale::Text::KeyName(Locale::KEYNAME_U);
    Input::Engine.KeyTitle[Input::KC_V]          = Locale::Text::KeyName(Locale::KEYNAME_V);
    Input::Engine.KeyTitle[Input::KC_W]          = Locale::Text::KeyName(Locale::KEYNAME_W);
    Input::Engine.KeyTitle[Input::KC_X]          = Locale::Text::KeyName(Locale::KEYNAME_X);
    Input::Engine.KeyTitle[Input::KC_Y]          = Locale::Text::KeyName(Locale::KEYNAME_Y);
    Input::Engine.KeyTitle[Input::KC_Z]          = Locale::Text::KeyName(Locale::KEYNAME_Z);
    Input::Engine.KeyTitle[Input::KC_NUM0]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_0);
    Input::Engine.KeyTitle[Input::KC_NUM1]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_1);
    Input::Engine.KeyTitle[Input::KC_NUM2]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_2);
    Input::Engine.KeyTitle[Input::KC_NUM3]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_3);
    Input::Engine.KeyTitle[Input::KC_NUM4]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_4);
    Input::Engine.KeyTitle[Input::KC_NUM5]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_5);
    Input::Engine.KeyTitle[Input::KC_NUM6]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_6);
    Input::Engine.KeyTitle[Input::KC_NUM7]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_7);
    Input::Engine.KeyTitle[Input::KC_NUM8]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_8);
    Input::Engine.KeyTitle[Input::KC_NUM9]       = Locale::Text::KeyName(Locale::KEYNAME_NUM_9);
    Input::Engine.KeyTitle[Input::KC_NUMMUL]     = Locale::Text::KeyName(Locale::KEYNAME_MUL);
    Input::Engine.KeyTitle[Input::KC_NUMPLUS]    = Locale::Text::KeyName(Locale::KEYNAME_ADD);
    Input::Engine.KeyTitle[Input::KC_NUMDOT]     = Locale::Text::KeyName(Locale::KEYNAME_DOT);
    Input::Engine.KeyTitle[Input::KC_NUMMINUS]   = Locale::Text::KeyName(Locale::KEYNAME_SUB);
    Input::Engine.KeyTitle[Input::KC_NUMENTER]   = Locale::Text::KeyName(Locale::KEYNAME_ENTER);
    Input::Engine.KeyTitle[Input::KC_NUMDIV]     = Locale::Text::KeyName(Locale::KEYNAME_DIV);
    Input::Engine.KeyTitle[Input::KC_EXTRA1]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_1);
    Input::Engine.KeyTitle[Input::KC_EXTRA2]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_2);
    Input::Engine.KeyTitle[Input::KC_EXTRA3]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_3);
    Input::Engine.KeyTitle[Input::KC_EXTRA4]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_4);
    Input::Engine.KeyTitle[Input::KC_EXTRA5]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_5);
    Input::Engine.KeyTitle[Input::KC_EXTRA6]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_6);
    Input::Engine.KeyTitle[Input::KC_EXTRA7]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_7);
    Input::Engine.KeyTitle[Input::KC_EXTRA8]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_8);
    Input::Engine.KeyTitle[Input::KC_EXTRA9]     = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_9);
    Input::Engine.KeyTitle[Input::KC_EXTRA10]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_10);
    Input::Engine.KeyTitle[Input::KC_EXTRA11]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_11);
    Input::Engine.KeyTitle[Input::KC_EXTRA12]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_12);
    Input::Engine.KeyTitle[Input::KC_EXTRA13]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_13);
    Input::Engine.KeyTitle[Input::KC_EXTRA14]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_14);
    Input::Engine.KeyTitle[Input::KC_EXTRA15]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_15);
    Input::Engine.KeyTitle[Input::KC_EXTRA16]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_16);
    Input::Engine.KeyTitle[Input::KC_EXTRA17]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_17);
    Input::Engine.KeyTitle[Input::KC_EXTRA18]    = Locale::Text::KeyName(Locale::KEYNAME_EXTRA_18);

    Input::Engine.KeyTitle[Input::KC_MMB]        = Locale::Text::KeyName(Locale::KEYNAME_MIDDLE_MOUSE);

    Input::Engine.KeyTitle[Input::KC_JOYB0]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB0);
    Input::Engine.KeyTitle[Input::KC_JOYB1]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB1);
    Input::Engine.KeyTitle[Input::KC_JOYB2]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB2);
    Input::Engine.KeyTitle[Input::KC_JOYB3]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB3);
    Input::Engine.KeyTitle[Input::KC_JOYB4]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB4);
    Input::Engine.KeyTitle[Input::KC_JOYB5]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB5);
    Input::Engine.KeyTitle[Input::KC_JOYB6]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB6);
    Input::Engine.KeyTitle[Input::KC_JOYB7]      = Locale::Text::KeyName(Locale::KEYNAME_JOYB7);
    Input::Engine.KeyTitle[Input::KC_LSHIFT]     = Locale::Text::KeyName(Locale::KEYNAME_LSHIFT);

    // OpenNeoUA: compact key labels for narrow Input Settings columns and all
    // other UI consumers of KeyTitle. These are display-only aliases: key
    // codes, remapping, serialization and runtime behavior are unchanged.
    Input::Engine.KeyTitle[Input::KC_ESCAPE]     = "Esc";
    Input::Engine.KeyTitle[Input::KC_BACKSPACE]  = "Bksp";
    Input::Engine.KeyTitle[Input::KC_CLEAR]      = "Clr";
    Input::Engine.KeyTitle[Input::KC_RETURN]     = "Enter";
    Input::Engine.KeyTitle[Input::KC_CTRL]       = "Ctrl";
    Input::Engine.KeyTitle[Input::KC_SHIFT]      = "Shift";
    Input::Engine.KeyTitle[Input::KC_ALT]        = "Alt";
    Input::Engine.KeyTitle[Input::KC_PGUP]       = "PgUp";
    Input::Engine.KeyTitle[Input::KC_PGDOWN]     = "PgDn";
    Input::Engine.KeyTitle[Input::KC_SELECT]     = "Sel";
    Input::Engine.KeyTitle[Input::KC_EXECUTE]    = "Exec";
    Input::Engine.KeyTitle[Input::KC_SNAPSHOT]   = "PrtSc";
    Input::Engine.KeyTitle[Input::KC_INSERT]     = "Ins";
    Input::Engine.KeyTitle[Input::KC_DELETE]     = "Del";

    Input::Engine.KeyTitle[Input::KC_NUM0]       = "Num0";
    Input::Engine.KeyTitle[Input::KC_NUM1]       = "Num1";
    Input::Engine.KeyTitle[Input::KC_NUM2]       = "Num2";
    Input::Engine.KeyTitle[Input::KC_NUM3]       = "Num3";
    Input::Engine.KeyTitle[Input::KC_NUM4]       = "Num4";
    Input::Engine.KeyTitle[Input::KC_NUM5]       = "Num5";
    Input::Engine.KeyTitle[Input::KC_NUM6]       = "Num6";
    Input::Engine.KeyTitle[Input::KC_NUM7]       = "Num7";
    Input::Engine.KeyTitle[Input::KC_NUM8]       = "Num8";
    Input::Engine.KeyTitle[Input::KC_NUM9]       = "Num9";
    Input::Engine.KeyTitle[Input::KC_NUMMUL]     = "Num*";
    Input::Engine.KeyTitle[Input::KC_NUMPLUS]    = "Num+";
    Input::Engine.KeyTitle[Input::KC_NUMDOT]     = "Num.";
    Input::Engine.KeyTitle[Input::KC_NUMMINUS]   = "Num-";
    Input::Engine.KeyTitle[Input::KC_NUMENTER]   = "NumEnt";
    Input::Engine.KeyTitle[Input::KC_NUMDIV]     = "Num/";

    // Punctuation/scancode aliases use the actual key glyph where practical.
    Input::Engine.KeyTitle[Input::KC_EXTRA1]     = ",";
    Input::Engine.KeyTitle[Input::KC_EXTRA2]     = ".";
    Input::Engine.KeyTitle[Input::KC_EXTRA3]     = "-";
    Input::Engine.KeyTitle[Input::KC_EXTRA4]     = "\\";
    Input::Engine.KeyTitle[Input::KC_EXTRA5]     = ";";
    Input::Engine.KeyTitle[Input::KC_EXTRA6]     = "=";
    Input::Engine.KeyTitle[Input::KC_EXTRA7]     = "`";
    Input::Engine.KeyTitle[Input::KC_EXTRA8]     = "'";
    Input::Engine.KeyTitle[Input::KC_EXTRA9]     = "/";
    Input::Engine.KeyTitle[Input::KC_EXTRA10]    = "]";
    Input::Engine.KeyTitle[Input::KC_EXTRA11]    = "\\";
    Input::Engine.KeyTitle[Input::KC_EXTRA12]    = "[";
    Input::Engine.KeyTitle[Input::KC_EXTRA13]    = "OEM8";
    Input::Engine.KeyTitle[Input::KC_EXTRA14]    = "ScrLk";
    Input::Engine.KeyTitle[Input::KC_EXTRA15]    = "NumLk";
    Input::Engine.KeyTitle[Input::KC_EXTRA16]    = "F13";
    Input::Engine.KeyTitle[Input::KC_EXTRA17]    = "F14";
    Input::Engine.KeyTitle[Input::KC_EXTRA18]    = "F15";

    Input::Engine.KeyTitle[Input::KC_MMB]        = "MMB";
    Input::Engine.KeyTitle[Input::KC_JOYB0]      = "JoyB0";
    Input::Engine.KeyTitle[Input::KC_JOYB1]      = "JoyB1";
    Input::Engine.KeyTitle[Input::KC_JOYB2]      = "JoyB2";
    Input::Engine.KeyTitle[Input::KC_JOYB3]      = "JoyB3";
    Input::Engine.KeyTitle[Input::KC_JOYB4]      = "JoyB4";
    Input::Engine.KeyTitle[Input::KC_JOYB5]      = "JoyB5";
    Input::Engine.KeyTitle[Input::KC_JOYB6]      = "JoyB6";
    Input::Engine.KeyTitle[Input::KC_JOYB7]      = "JoyB7";
    Input::Engine.KeyTitle[Input::KC_LSHIFT]     = "LShift";
}

void UserData::ShowInputSettings()
{
    titel_button->HideScreen();

    button_input_button->ShowScreen();

    EnvMode = ENVMODE_INPUT;
    p_YW->GuiWinClose( &input_listview );
    p_YW->GuiWinOpen( &input_listview );
}

void ShowInputSettings2(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_RESTART;
}

void ShowInputSettings1(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.TimerStatus = TBriefengScreen::TIMER_FAST;
}

void UserData::InputConfCancel()
{
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCode = konf.PKeyCodeBkp;
        konf.NKeyCode = konf.NKeyCodeBkp;
    }
}

void UserData::InputPageCancel()
{
    EnvMode = ENVMODE_TITLE;
    InputConfCancel();

    NC_STACK_button::button_66arg v6;
    v6.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
    v6.field_4 = 2;
    sub_bar_button->SetState(&v6);

    inputChangedParts = 0;
    ClearPendingInputKey(this);

    v6.butID = 1050;
    v6.field_4 = (joystickEnabled == false) + 1;
    button_input_button->SetState(&v6);

    v6.field_4 = (altJoystickEnabled == false) + 1;
    v6.butID = 1061;
    button_input_button->SetState(&v6);

    v6.butID = 1055;
    v6.field_4 = ((p_YW->_preferences & World::PREF_FFDISABLE) != 0) + 1;
    button_input_button->SetState(&v6);

    button_input_button->HideScreen();

    p_YW->GuiWinClose( &input_listview );

    titel_button->ShowScreen();
}

void UserData::InputConfCopyToBackup()
{
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCodeBkp = konf.PKeyCode;
        konf.NKeyCodeBkp = konf.NKeyCode;
    }
}

void UserData::InputConfigRestoreDefault()
{
    for(TInputConf &konf : InputConfig)
    {
        konf.PKeyCode = konf.PKeyCodeDef;
        konf.NKeyCode = konf.NKeyCodeDef;
    }
}

int UserData::InputIndexFromConfig(uint32_t type, uint32_t index)
{
    static const std::array<int, 8> BUTTON
    {
        World::INPUT_BIND_FIRE,       World::INPUT_BIND_SWITCH_WEAPON,
        World::INPUT_BIND_GUN,        World::INPUT_BIND_BRAKE,
        World::INPUT_BIND_WAPOINT,    World::INPUT_BIND_CAMFIRE,
        World::INPUT_BIND_CYCLE_TARGET, World::INPUT_BIND_ALTERNATIVE_VIEW
    };

    static const std::array<int, 6> SLIDER
    {
        World::INPUT_BIND_FLY_DIR,    World::INPUT_BIND_FLY_HEIGHT,
        World::INPUT_BIND_FLY_SPEED,  World::INPUT_BIND_DRIVE_DIR,
        World::INPUT_BIND_DRIVE_SPEED,World::INPUT_BIND_GUN_HEIGHT,
    };

    static const std::array<int, 55> HOTKEY
    {
        World::INPUT_BIND_ORDER,      World::INPUT_BIND_ATTACK,
        World::INPUT_BIND_NEW,        World::INPUT_BIND_ADD,
        World::INPUT_BIND_CONTROL,    -1,
        -1,                           World::INPUT_BIND_AUTOPILOT,
        World::INPUT_BIND_MAP,        World::INPUT_BIND_SQ_MANAGE,

        // 10
        World::INPUT_BIND_LANDLAYER,  World::INPUT_BIND_OWNER,
        World::INPUT_BIND_HEIGHT,     -1,
        World::INPUT_BIND_LOCKVIEW,   -1,
        World::INPUT_BIND_ZOOMIN,     World::INPUT_BIND_ZOOMOUT,
        World::INPUT_BIND_MINIMAP,    -1,

        // 20
        World::INPUT_BIND_NEXT_COMM,  World::INPUT_BIND_TO_HOST,
        World::INPUT_BIND_NEXT_UNIT,  World::INPUT_BIND_TO_COMM,
        World::INPUT_BIND_QUIT,       World::INPUT_BIND_HUD,
        -1,                           World::INPUT_BIND_LOG_WND,
        -1,                           -1,

        // 30
        -1,                           World::INPUT_BIND_LAST_MSG,
        World::INPUT_BIND_PAUSE,      -1,
        -1,                           -1,
        -1,                           World::INPUT_BIND_TO_ALL,
        World::INPUT_BIND_AGGR_1,     World::INPUT_BIND_AGGR_2,

        // 40
        World::INPUT_BIND_AGGR_3,     World::INPUT_BIND_AGGR_4,
        World::INPUT_BIND_AGGR_5,     World::INPUT_BIND_HELP,
        World::INPUT_BIND_LAST_SEAT,  World::INPUT_BIND_SET_COMM,
        World::INPUT_BIND_ANALYZER,   World::INPUT_BIND_COCKPIT_CAMERA,
        World::INPUT_BIND_SPRINT,     World::INPUT_BIND_PLACE_MAP_MARKER,

        // 50/51 are intentionally reserved for the retired split Camera Zoom
        // profile slots. New remappable hotkeys continue at 52.
        -1,                           -1,
        World::INPUT_BIND_TOGGLE_UFO_SPY_UI,
        World::INPUT_BIND_MAP_FOCUS,
        World::INPUT_BIND_HIDE_VEHICLE
    };

    if ( type == World::INPUT_BIND_TYPE_BUTTON && index < BUTTON.size())
        return BUTTON[index];
    else if ( type == World::INPUT_BIND_TYPE_SLIDER && index < SLIDER.size() )
        return SLIDER[index];
    else if ( type == World::INPUT_BIND_TYPE_HOTKEY && index < HOTKEY.size() )
        return HOTKEY[index];
    return -1;
}
