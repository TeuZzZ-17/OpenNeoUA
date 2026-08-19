#include <inttypes.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <cmath>

#include "includes.h"
#include "yw.h"
#include "yw_internal.h"
#include "font.h"
#include "button.h"

#include "yw_net.h"
#include "windp.h"

#include <math.h>

#include "gui/uamsgbox.h"
#include "env.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "system/system.h"
#include "loaders.h"
#include "obj3d.h"
#include "ypaflyer.h"
#include "utils.h"
#include "IFFile.h"

extern int vertMenuSpace;
extern int dword_5A50B2;
extern int dword_5A50B2_h;
extern int word_5A50AE;
extern int word_5A50BC;
extern int word_5A50BA;
extern int word_5A50BE;
extern int buttonsSpace;

extern int dword_5A50B6;
extern int dword_5A50B6_h;

static constexpr int SETTINGS_CHANGE_PALETTE_THEME = 0x2000;
static constexpr int SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR = 0x4000;
static constexpr int SETTINGS_CHANGE_SPECTATOR_MODE = 0x8000;
static constexpr int SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS = 0x10000;
static constexpr int SETTINGS_CHANGE_AMBIENT_VOLUME = 0x20000;
// OpenUA: modern graphics options
static constexpr int SETTINGS_CHANGE_MAXFPS                 = 0x40000;
static constexpr int SETTINGS_CHANGE_BLENDING              = 0x80000;
static constexpr int SETTINGS_CHANGE_MOVIE_PLAYER          = 0x100000;
static constexpr int SETTINGS_CHANGE_MENU_FONT             = 0x200000;
static constexpr int SETTINGS_CHANGE_DEFAULT_CAMERA_VIEW   = 0x400000;
static constexpr int SETTINGS_CHANGE_INTERFACE_STYLE       = 0x800000;
static constexpr int SETTINGS_CHANGE_RESTART_REQUIRED_GRAPHICS =
    SETTINGS_CHANGE_BLENDING | SETTINGS_CHANGE_MENU_FONT;
static constexpr int MENU_MSGBOX_RESTORE_DEFAULT_KEYS = 1;
static constexpr int MENU_MSGBOX_INPUT_KEY_CONFLICT = 2;

static std::string InputKeyDisplayTitle(int16_t keyCode)
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

static std::string InputFixedShortcutTitle(int binding)
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

static bool InputKeyUsesOtherSlot(const UserData *usr, int target, bool positiveSlot, int16_t keyCode)
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

static std::string InputKeyConflictNames(const UserData *usr, int target, bool positiveSlot, int16_t keyCode)
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

static void ClearInputKeyConflicts(UserData *usr, int target, bool positiveSlot, int16_t keyCode)
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

static void ApplyCapturedInputKey(UserData *usr, int target, bool positiveSlot, int16_t keyCode)
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

static void ClearPendingInputKey(UserData *usr)
{
    usr->pendingInputTarget = -1;
    usr->pendingInputKeyCode = Input::KC_NONE;
    usr->pendingInputPositiveSlot = true;
}

static void LayoutSaveLoadActionButtons(UserData *usr, bool deleteVisible)
{
    if ( !usr || !usr->p_YW || !usr->disk_button )
        return;

    const int menuWidth = (int)(usr->p_YW->_screenSize.x * 0.7);
    const int buttonWidth = (menuWidth - 4 * buttonsSpace) / 5;
    const int buttonStep = buttonWidth + buttonsSpace;
    const int firstX = deleteVisible ? 0 : buttonStep / 2;
    const int primaryIds[] = {1103, 1101, 1104, 1106}; // New, Load, Save, Back

    NC_STACK_button::button_arg76 layout;
    layout.ypos = -1;
    layout.width = (int16_t)buttonWidth;

    for (int index = 0; index < 4; index++)
    {
        layout.butID = primaryIds[index];
        layout.xpos = (int16_t)(firstX + index * buttonStep);
        usr->disk_button->setXYWidth(&layout);
    }

    layout.butID = 1102; // Delete occupies the fifth slot when available.
    layout.xpos = (int16_t)(4 * buttonStep);
    usr->disk_button->setXYWidth(&layout);
}

static std::string DefaultCameraViewLabel(bool cockpit)
{
    return Locale::Text::OpenUA(cockpit ? Locale::OUA_COCKPIT : Locale::OUA_POV);
}

static std::string BlendingLabel(int v)
{
    switch (v)
    {
        case 1:  return Locale::Text::OpenUA(Locale::OUA_ADDITIVE);
        case 2:  return Locale::Text::OpenUA(Locale::OUA_SHARP);
        default: return Locale::Text::OpenUA(Locale::OUA_DEFAULT);
    }
}

static int CycleBlending(int v)   { return (v == 0) ? 1 : (v == 1) ? 2 : 0; }

static int NormalizeFrameRateLimit(int value)
{
    static const int validLimits[] = {60, 75, 90, 120, 144, 165, 200, 240};

    for (int limit : validLimits)
    {
        if (value == limit)
            return value;
    }

    return 60;
}

static int CycleFrameRateLimit(int value)
{
    static const int validLimits[] = {60, 75, 90, 120, 144, 165, 200, 240};
    value = NormalizeFrameRateLimit(value);

    for (size_t i = 0; i < sizeof(validLimits) / sizeof(validLimits[0]); i++)
    {
        if (validLimits[i] == value)
            return validLimits[(i + 1) % (sizeof(validLimits) / sizeof(validLimits[0]))];
    }

    return 60;
}

// OpenUA: the "Atmosphere" dropdown now selects a modern fullscreen visual filter from
// Data/Filters/*.pal (see GFXEngine::SetVisualFilter), NOT the legacy SET palette-theme.
// The existing paletteTheme* members/functions are reused as the filter selector to keep
// the menu infrastructure unchanged. An empty selection means "Standard" (no filter).
static std::string NormalizePaletteThemeName(std::string theme)
{
    size_t first = theme.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    size_t last = theme.find_last_not_of(" \t\r\n");
    theme = theme.substr(first, last - first + 1);

    // "Standard" is the new sentinel; "Original" kept for backward compatibility.
    if (!StriCmp(theme, "Standard") || !StriCmp(theme, "Original"))
        return std::string();

    return theme;
}

static std::string PaletteThemeStorageValue(const std::string &theme)
{
    return theme.empty() ? std::string("Standard") : theme;
}

static std::string NormalizeMenuFontName(std::string fontName)
{
    size_t first = fontName.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string("Default");

    size_t last = fontName.find_last_not_of(" \t\r\n");
    fontName = fontName.substr(first, last - first + 1);

    if (!StriCmp(fontName, "Default"))
        return std::string("Default");

    return fontName;
}

static std::string MenuFontDisplayName(const std::string &fontName)
{
    const std::string normalized = NormalizeMenuFontName(fontName);
    if (!StriCmp(normalized, "Default"))
        return Locale::Text::OpenUA(Locale::OUA_DEFAULT);
    return normalized;
}

static void SavePaletteThemeCache(const std::string &theme)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc("env:visual_filter.txt", "w");
    if (!fil)
        return;

    fil->puts(PaletteThemeStorageValue(theme) + "\n");
    delete fil;
}

static std::string VisualFilterStrengthStorageValue(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    return std::to_string(value / 100) + "."
         + (value % 100 < 10 ? "0" : "")
         + std::to_string(value % 100);
}

static std::string TrimIniValue(std::string s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static int VisualFilterStrengthPercentFromString(std::string s, int fallback)
{
    s = TrimIniValue(s);
    if (s.empty())
        return fallback;

    try
    {
        if (s.find(',') != std::string::npos)
            return fallback;

        size_t pos = 0;
        float st = std::stof(s, &pos);
        if (TrimIniValue(s.substr(pos)).size() != 0)
            return fallback;

        if (st < 0.0f) st = 0.0f;
        if (st > 1.0f) st = 1.0f;
        return (int)(st * 100.0f + 0.5f);
    }
    catch (...)
    {
        return fallback;
    }
}


static int FloatHundredFromString(std::string s, int fallback, int minValue, int maxValue)
{
    s = TrimIniValue(s);
    if (s.empty())
        return fallback;

    try
    {
        if (s.find(',') != std::string::npos)
            return fallback;

        size_t pos = 0;
        float value = std::stof(s, &pos);
        if (TrimIniValue(s.substr(pos)).size() != 0 || !std::isfinite(value))
            return fallback;

        int scaled = (int)(value * 100.0f + (value >= 0.0f ? 0.5f : -0.5f));
        if (scaled < minValue) scaled = minValue;
        if (scaled > maxValue) scaled = maxValue;
        return scaled;
    }
    catch (...)
    {
        return fallback;
    }
}

static int IntFromString(std::string s, int fallback, int minValue, int maxValue)
{
    s = TrimIniValue(s);
    if (s.empty())
        return fallback;

    try
    {
        size_t pos = 0;
        int value = std::stoi(s, &pos);
        if (TrimIniValue(s.substr(pos)).size() != 0)
            return fallback;
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
        return value;
    }
    catch (...)
    {
        return fallback;
    }
}

static std::string HundredStorageValue(int value)
{
    const bool negative = value < 0;
    int absValue = negative ? -value : value;
    std::string out = std::to_string(absValue / 100) + ".";
    if ((absValue % 100) < 10)
        out += "0";
    out += std::to_string(absValue % 100);
    return negative ? "-" + out : out;
}

static std::string PaletteThemeDisplayName(const std::string &fileName)
{
    if (fileName.empty())
        return Locale::Text::OpenUA(Locale::OUA_STANDARD);

    std::string name = fileName;
    if (name.size() >= 4 && !StriCmp(name.substr(name.size() - 4), ".pal"))
        name.resize(name.size() - 4);

    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}

void sb_0x4eb94c__sub0(NC_STACK_ypaworld *yw, bool clockwise, int a3, vec3d *pos, baseRender_msg *arg)
{
    NC_STACK_base *model_base = yw->_vhclModels.at(yw->_vhclProtos[yw->_briefScreen.ViewingObject.ID].vp_normal);

    model_base->SetVizLimit(16000);
    model_base->SetFadeLength(100);

    model_base->SetPosition(*pos);

    if (clockwise)
    {
        yw->_briefScreen.ViewingObjectAngle += (arg->frameTime / 5);
        if (yw->_briefScreen.ViewingObjectAngle >= 360)
            yw->_briefScreen.ViewingObjectAngle -= 360;
    }
    else
    {
        yw->_briefScreen.ViewingObjectAngle -= (arg->frameTime / 5);
        if (yw->_briefScreen.ViewingObjectAngle < 0)
            yw->_briefScreen.ViewingObjectAngle += 360;
    }

    model_base->SetEulerRotation(a3 + 10, yw->_briefScreen.ViewingObjectAngle, 0);

    NC_STACK_base::CheckOpts(&yw->_briefScreen.ViewingObject.VP, model_base);

    model_base->Render(arg, yw->_briefScreen.ViewingObject.VP);
}

void sb_0x4eb94c__sub1(NC_STACK_ypaworld *yw, bool clockwise, int rot, vec3d *pos, baseRender_msg *arg)
{
    TSectorDesc *scType = &yw->_secTypeArray[yw->_briefScreen.ViewingObject.ID];

    NC_STACK_base *v7 = yw->_vhclModels.at(0);

    if (clockwise)
    {
        yw->_briefScreen.ViewingObjectAngle += (arg->frameTime / 5);
        if (yw->_briefScreen.ViewingObjectAngle >= 360)
            yw->_briefScreen.ViewingObjectAngle -= 360;
    }
    else
    {
        yw->_briefScreen.ViewingObjectAngle -= (arg->frameTime / 5);
        if (yw->_briefScreen.ViewingObjectAngle < 0)
            yw->_briefScreen.ViewingObjectAngle += 360;
    }

    v7->SetEulerRotation(rot + 10, yw->_briefScreen.ViewingObjectAngle, 0);

    int first;
    int demens;

    if (scType->SectorType == 1)
    {
        first = 0;
        demens = 1;
    }
    else
    {
        first = -1;
        demens = 3;
    }

    int v22 = first;
    for (int y = 0; y < demens; y++)
    {
        int v30 = first;
        for (int x = 0; x < demens; x++)
        {
            vec3d inSectorPos = vec3d(v30, 0.0, v22) * 300.0;

            NC_STACK_base *lego = yw->_legoArray[scType->SubSectors.At(x, y)->HPModels[0]].Base;
            lego->SetStatic(false);
            lego->SetVizLimit(16000);
            lego->SetFadeLength(100);

            lego->SetEulerRotation(rot + 10, yw->_briefScreen.ViewingObjectAngle, 0);
            lego->SetPosition(*pos + v7->TForm().SclRot.Transform(inSectorPos));

            NC_STACK_base::CheckOpts(&yw->_briefScreen.ViewingObject.VP, lego);

            lego->Render(arg, yw->_briefScreen.ViewingObject.VP);

            v30++;
        }
        v22++;
    }
}

void sb_0x4eb94c(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc, int a5)
{
    brf->ObjRenderParams.frameTime = struc->Period;
    brf->ObjRenderParams.globTime = brf->CurrTime;

    TF::TForm3D v14;
    v14.Scale = vec3d(1.0, 1.0, 1.0);
    v14.SclRot = mat3x3::Ident();

    TF::Engine.SetViewPoint(&v14);
    v14.CalcGlobal();

    vec3d pos;

    if (brf->ViewingObject)
    {
        pos.x = (brf->ViewingObjectRect.right + brf->ViewingObjectRect.left) / 2.0;
        pos.y = (brf->ViewingObjectRect.bottom + brf->ViewingObjectRect.top) / 2.0;

        float v16;
        float v17;
        float v18;
        int rot;

        if (brf->ViewingObject.ObjType == TBriefObject::TYPE_SECTOR)
        {
            v16 = 9600.0;
            v17 = 3600.0;
        }
        else if (brf->ViewingObject.ObjType == TBriefObject::TYPE_VEHICLE)
        {
            float radius = yw->_vhclProtos[brf->ViewingObject.ID].radius;

            v17 = radius * 7.0;
            v16 = radius * 32.0;
        }

        if (a5 >= 500)
        {
            v18 = v17;
            rot = 0;
        }
        else
        {
            v18 = v16 + (v17 - v16) * a5 / 500.0;
            rot = -90 * a5 / 500 + 90;
        }

        pos.z = v18;
        pos.y = pos.y * v18;
        pos.x = pos.x * v18;

        if (brf->ViewingObject.ObjType == TBriefObject::TYPE_SECTOR)
            sb_0x4eb94c__sub1(yw, true, rot, &pos, &brf->ObjRenderParams);
        else if (brf->ViewingObject.ObjType == TBriefObject::TYPE_VEHICLE)
            sb_0x4eb94c__sub0(yw, true, rot, &pos, &brf->ObjRenderParams);
    }
}

void ypaworld_func158__DrawVehicle(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc)
{
    GFX::Engine.SetFBOBlending(1);
    GFX::Engine.BeginScene();

    brf->ObjRenderParams.frameTime = 1;
    brf->ObjRenderParams.globTime = 1;
    brf->ObjRenderParams.adeCount = 0;
    brf->ObjRenderParams.minZ = 17.0;
    brf->ObjRenderParams.maxZ = 32000.0;
    brf->ObjRenderParams.flags = 0;

    if (brf->ViewingObject)
    {
        int v7 = brf->CurrTime - brf->ViewingObjectStartTime;
        if (v7 > 50)
            sb_0x4eb94c(yw, brf, struc, v7 - 50);
    }

    GFX::Engine.Rasterize();

    GFX::Engine.EndScene();
    GFX::Engine.SetFBOBlending(0);
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

    // OpenUA: compact key labels for narrow Input Settings columns and all
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


int yw_loadSky(NC_STACK_ypaworld *yw, const std::string &skyname)
{
    struct SkyLooseScopeGuard
    {
        bool active = false;

        explicit SkyLooseScopeGuard(const std::string &name)
        {
            active = IFFile::BeginSkyLooseScope(name);
        }

        ~SkyLooseScopeGuard()
        {
            if ( active )
                IFFile::EndSkyLooseScope();
        }
    } skyLooseGuard(skyname);

    std::string tmprsrc = Common::Env.SetPrefix("rsrc", "data:");
    std::string skyfilename = fmt::sprintf("data:%s", skyname);

    NC_STACK_base *sky = Utils::ProxyLoadBase(skyfilename);
    yw->_skyObject = sky;
    if ( !sky )
    {
        ypa_log_out("Couldn't create %s\n", skyfilename.c_str());
        Common::Env.SetPrefix("rsrc", tmprsrc);
        return 0;
    }

    Common::Env.SetPrefix("rsrc", tmprsrc);

    sky->SetStatic(true); // Don't rotate sky
    sky->SetVizLimit(yw->_skyVizLimit);
    sky->SetFadeLength(yw->_skyFadeLength);
    sky->ComputeStaticFog();
    sky->MakeVBO();
    return 1;
}

void NC_STACK_ypaworld::listSaveDir(const std::string &saveDir)
{
    auto savedStatuses = _playersStats;
    auto savedCallsign = _GameShell->netPlayerName;
    auto savedMaxroboenrgy = _maxRoboEnergy;
    auto savedMaxreloadconst = _maxReloadConst;

    FSMgr::DirIter dir = uaOpenDir(saveDir);
    if ( dir )
    {
        FSMgr::iNode *dirNode;
        while ( dir.getNext(&dirNode) )
        {
            if ( dirNode->getType() == FSMgr::iNode::NTYPE_DIR )
            {
                if ( StriCmp(dirNode->getName(), ".") && StriCmp(dirNode->getName(), "..") )
                {
                    _GameShell->profiles.emplace_back();
                    ProfilesNode &profile = _GameShell->profiles.back();

                    profile.name = dirNode->getName();

                    ScriptParser::HandlersList hndls{
                        new World::Parsers::UserParser(this)
                    };
                    std::string buf = fmt::sprintf("%s/%s/user.txt\n", saveDir, dirNode->getName());

                    if ( !ScriptParser::ParseFile(buf, hndls,0) )
                        ypa_log_out("Warning, cannot parse %s for time scanning\n", buf.c_str());
                    profile.fraction = 1;
                    profile.totalElapsedTime = _playersStats[1].ElapsedTime;
                }
            }
        }
    }
    else
    {
        ypa_log_out("Unknown Game-Directory %s\n", saveDir.c_str());
    }

    _playersStats = savedStatuses;
    _maxReloadConst = savedMaxreloadconst;
    _maxRoboEnergy = savedMaxroboenrgy;
    _GameShell->netPlayerName = savedCallsign;
}


void listLocaleDir(UserData *usr, const char *dirname)
{
    if (!usr)
        return;

    // OpenUA supports only the English vanilla catalogue. Retail layouts may
    // call it LANGUAGE.DLL or ENGLISH.DLL, but both map to one ENGLISH entry.
    usr->default_lang_dll = nullptr;

    bool englishCatalogueFound = false;
    FSMgr::DirIter dir = uaOpenDir(dirname);
    if (dir)
    {
        FSMgr::iNode *node = nullptr;
        while (dir.getNext(&node))
        {
            if (!node || node->getType() != FSMgr::iNode::NTYPE_FILE)
                continue;

            std::string filename = node->getName();
            std::transform(filename.begin(), filename.end(), filename.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (filename == "language.dll" || filename == "english.dll")
            {
                englishCatalogueFound = true;
                break;
            }
        }
    }
    else
    {
        ypa_log_out("Unknown Locale-Directory %s\n", dirname);
    }

    if (englishCatalogueFound)
    {
        usr->lang_dlls.push_back("ENGLISH");
        usr->default_lang_dll = &usr->lang_dlls.back();
    }
}

void UserData::sub_46A7F8()
{
    disk_button->HideScreen();

    p_YW->GuiWinClose( &disk_listvw );

    if ( diskEnterFromMapSelect )
    {
        EnvMode = ENVMODE_SINGLEPLAY;
        sub_bar_button->ShowScreen();
    }
    else
    {
        EnvMode = ENVMODE_TITLE;
        titel_button->ShowScreen();
    }

    NC_STACK_button::button_66arg v3;
    v3.field_4 = 2;
    v3.butID = 1156;
    video_button->SetState(&v3);

    diskScreenMode = 0;
}



void NC_STACK_ypaworld::PlayIntroMovie()
{
    if ( !_movies[World::MOVIE_INTRO].empty() )
    {
        std::string buf = correctSeparatorAndExt( Common::Env.ApplyPrefix(_movies[World::MOVIE_INTRO]) );

        if ( System::IniConf::GfxMoviePlayer.Get<bool>() )
        {
            GFX::Engine.EndFrame();
            System::Movie.PlayMovie(buf, _GameShell->soundVolume);
            GFX::Engine.BeginFrame();
        }

        Input::Engine.QueryInput(&input_states);
        input_states.KbdLastHit = Input::KC_NONE;
        input_states.KbdLastDown = Input::KC_NONE;
        input_states.HotKeyID = -1;
    }
}



void ypaworld_func156__sub1(UserData *usr)
{
    usr->mapDescriptions.clear();

    for (size_t i = 0; i < usr->p_YW->_globalMapRegions.MapRegions.size(); i++)
    {
        if (usr->p_YW->_globalMapRegions.MapRegions[i].Status == TMapRegionInfo::STATUS_NETWORK)
            usr->mapDescriptions.push_back( UserData::TMapDescription(i, usr->p_YW->GetLevelName(i)) );
    }

    std::stable_sort(usr->mapDescriptions.begin(), usr->mapDescriptions.end(), UserData::TMapDescription::compare);
}


void UserData::GameShellUiOpenNetwork()
{
    titel_button->HideScreen();

    network_button->ShowScreen();

    EnvMode = ENVMODE_NETPLAY;

    p_YW->GuiWinClose( &network_listvw );
    p_YW->GuiWinOpen( &network_listvw );
}



void sb_0x46ca74__sub0(const std::string &a1, const std::string &a2)
{
    FSMgr::FileHandle *f1 = uaOpenFileAlloc(a1, "r");
    if ( f1 )
    {
        FSMgr::FileHandle *f2 = uaOpenFileAlloc(a2, "w");

        if ( f2 )
        {
            char v9[300];

            while ( f1->gets(v9, 299) )
                f2->puts(v9);

            delete f2;
        }

        delete f1;
    }
}

void  UserData::sb_0x46ca74()
{
    std::string oldsave;

    if ( diskListActiveElement )
    {
        if ( StriCmp(userNameDir, UserName) )
            sub_46D0F8(fmt::sprintf("save:%s", userNameDir));
    }
    else
    {
        profiles.emplace_back();
        ProfilesNode &profile = profiles.back();

        profile.name = userNameDir;
        InputConfig[World::INPUT_BIND_SWITCH_WEAPON] = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 1, Input::KC_CTRL);
        InputConfig[World::INPUT_BIND_CYCLE_TARGET] = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 6, Input::KC_TAB);
        InputConfig[World::INPUT_BIND_ALTERNATIVE_VIEW] = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 7, Input::KC_F);
        InputConfig[World::INPUT_BIND_CAMFIRE] = UserData::TInputConf(World::INPUT_BIND_TYPE_BUTTON, 5, Input::KC_EXTRA7);
        InputConfig[World::INPUT_BIND_COCKPIT_CAMERA] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 47, Input::KC_K);
        InputConfig[World::INPUT_BIND_SPRINT] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 48, Input::KC_LSHIFT);
        InputConfig[World::INPUT_BIND_PLACE_MAP_MARKER] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 49, Input::KC_R);
        InputConfig[World::INPUT_BIND_TOGGLE_UFO_SPY_UI] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 52, Input::KC_U);
        InputConfig[World::INPUT_BIND_ZOOMIN] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 16, Input::KC_NUMPLUS);
        InputConfig[World::INPUT_BIND_ZOOMOUT] = UserData::TInputConf(World::INPUT_BIND_TYPE_HOTKEY, 17, Input::KC_NUMMINUS);

        std::string tmp = fmt::sprintf("save:%s", userNameDir);
        if ( !uaCreateDir(tmp) )
        {
            ypa_log_out("Unable to create directory %s\n", tmp.c_str());
            return;
        }

        disk_listvw.numEntries++;
        diskListActiveElement = disk_listvw.numEntries;
    }

    if ( ! p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", userNameDir), World::SDF_ALL) )
        ypa_log_out("Warning! Error while saving user data for %s\n", userNameDir.c_str());

    oldsave = fmt::sprintf("save:%s", UserName);

    if ( StriCmp(userNameDir, UserName) )
    {
        FSMgr::DirIter dir = uaOpenDir(oldsave);
        if ( dir )
        {
            FSMgr::iNode *a2a;
            while ( dir.getNext(&a2a) )
            {
                std::string tmp = a2a->getName();
                if ( a2a->getType() == FSMgr::iNode::NTYPE_FILE
                        && (tmp.rfind(".sgm") != std::string::npos
                            || tmp.rfind(".SGM") != std::string::npos
                            || tmp.rfind(".rst") != std::string::npos
                            || tmp.rfind(".RST") != std::string::npos
                            || tmp.rfind(".fin") != std::string::npos
                            || tmp.rfind(".FIN") != std::string::npos
                            || tmp.rfind(".def") != std::string::npos
                            || tmp.rfind(".DEF") != std::string::npos) )
                {
                    std::string v11 = fmt::sprintf("%s/%s", oldsave, tmp);
                    std::string v12 = fmt::sprintf("save:%s/%s", userNameDir, tmp);
                    sb_0x46ca74__sub0(v11.c_str(), v12.c_str());
                }
            }
        }
    }

    diskScreenMode = 0;
    UserName = userNameDir;

    disk_button->HideScreen();

    p_YW->GuiWinClose( &disk_listvw );

    if ( diskEnterFromMapSelect )
    {
        EnvMode = ENVMODE_SINGLEPLAY;

        sub_bar_button->ShowScreen();
    }
    else
    {
        EnvMode = ENVMODE_TITLE;

        titel_button->ShowScreen();
    }
}

void sb_0x47f810(NC_STACK_ypaworld *yw)
{
    yw->_roboProtos.clear();
    yw->_buildProtos.clear();

    yw->_weaponProtos.clear();

    yw->_vhclProtos.clear();
}

void sub_44A1FC(NC_STACK_ypaworld *yw)
{
    int v2 = 0;

    if ( yw->_GameShell )
    {
        FSMgr::FileHandle *fil = NULL;

        // Optional legacy level-index file. Modern builds scan level folders.
        if ( uaFileExist("env:levels.def") )
            fil = uaOpenFileAlloc("env:levels.def", "r");

        if ( fil )
        {
            std::string line;
            if ( fil->ReadLine(&line) )
            {
                Stok parse(line, "\t ,");
                std::string token;
                while( parse.GetNext(&token) )
                {
                    uint32_t tmp = std::stol(token, 0, 10);

                    if (tmp < 256)
                        yw->_globalMapRegions.MapRegions[tmp].Status = TMapRegionInfo::STATUS_ENABLED;
                }
            }

            v2 = 1;
            delete fil;
        }
    }

    if ( !v2 )
    {
        yw->_globalMapRegions.MapRegions[1].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[25].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[26].Status = TMapRegionInfo::STATUS_ENABLED;
        yw->_globalMapRegions.MapRegions[27].Status = TMapRegionInfo::STATUS_ENABLED;
    }
}

void UserData::sb_0x46cdf8()
{
    if ( ! p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", UserName), World::SDF_ALL) )
        ypa_log_out("Warning! Error while saving user data for %s\n", UserName.c_str());

    if ( diskListActiveElement )
    {
        sub_46D0F8(fmt::sprintf("save:%s", userNameDir));
    }
    else
    {
        profiles.emplace_back();
        ProfilesNode &profile = profiles.back();

        profile.name = userNameDir;

        std::string v10 = fmt::sprintf("save:%s", userNameDir);

        if ( !uaCreateDir(v10) )
        {
            ypa_log_out("Unable to create directory %s\n", v10.c_str());
            return;
        }

        UserName = userNameDir;
        disk_listvw.numEntries++;
        diskListActiveElement = disk_listvw.numEntries;
    }

    p_YW->_levelInfo.Buddies.clear();

    sb_0x47f810(p_YW);

    Common::DeleteAndNull(&p_YW->_script);

    if ( p_YW->ProtosInit() )
    {
        for (TMapRegionInfo &mp : p_YW->_globalMapRegions.MapRegions)
        {
            if ( mp.Status != TMapRegionInfo::STATUS_NONE && mp.Status != TMapRegionInfo::STATUS_NETWORK )
                mp.Status = TMapRegionInfo::STATUS_DISABLED;
        }

        sub_44A1FC(p_YW);

        p_YW->_maxRoboEnergy = 0;
        p_YW->_maxReloadConst = 0;

        p_YW->_playersStats.fill(World::TPlayerStatus());

        diskScreenMode = 0;

        p_YW->_beamEnergyCapacity = p_YW->_beamEnergyStart;

        p_YW->_levelInfo.JodieFoster.fill(0);

        sgmSaveExist = 0;

        disk_button->HideScreen();

        p_YW->GuiWinClose( &disk_listvw );

        EnvMode = ENVMODE_SINGLEPLAY;

        sub_bar_button->ShowScreen();
    }
    else
    {
        ypa_log_out("Warning, error while parsing prototypes\n");
    }
}

void UserData::sub_46D960()
{
    NC_STACK_button::button_66arg v4;
    v4.butID = 1300;
    confirm_button->Disable(&v4);

    v4.butID = 1301;
    confirm_button->Disable(&v4);

    confirm_button->HideScreen();

    confirmMode = 0;
}

void NC_STACK_ypaworld::SetFarView(bool farvw)
{
    if ( farvw )
    {
        setYW_visSectors(9);
        setYW_normVisLimit(3100);
        setYW_fadeLength(2100);
    }
    else
    {
        setYW_visSectors(5);
        setYW_normVisLimit(1400);
        setYW_fadeLength(600);
    }

    ApplyNucleusViewDistanceOverrides();
}


//Options OK
void UserData::sb_0x46aa8c()
{
    NC_STACK_ypaworld *yw = p_YW;

    bool forceChange = false;
    Common::Point resolution;

    if ( _settingsChangeOptions & 0x200 )
    {
        if ( confSoundFlags & World::SF_CDSOUND )
        {
            soundFlags |= World::SF_CDSOUND;
            yw->_preferences |= World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(true);
            if ( shelltrack )
            {
                SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                SFXEngine::SFXe.PlayMusicTrack();
            }
        }
        else
        {
            soundFlags &= ~World::SF_CDSOUND;
            yw->_preferences &= ~World::PREF_CDMUSICDISABLE;

            SFXEngine::SFXe.StopMusicTrack();
            SFXEngine::SFXe.SetMusicIgnoreCommandsFlag(false);
        }

    }

    if ( _settingsChangeOptions & 2 )
    {
        if ( confSoundFlags & World::SF_INVERTLR )
        {
            soundFlags |= World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(1);
        }
        else
        {
            soundFlags &= ~World::SF_INVERTLR;
            SFXEngine::SFXe.setReverseStereo(0);
        }
    }

    if ( _settingsChangeOptions & 0x10 )
    {
        if ( confGFXFlags & World::GFX_FLAG_FARVIEW )
        {
            GFXFlags |= World::GFX_FLAG_FARVIEW;
            p_YW->SetFarView(true);
        }
        else
        {
            GFXFlags &= ~World::GFX_FLAG_FARVIEW;
            p_YW->SetFarView(false);
        }
    }

    if ( _settingsChangeOptions & 8 )
    {
        if ( confGFXFlags & World::GFX_FLAG_SKYRENDER )
        {
            GFXFlags |= World::GFX_FLAG_SKYRENDER;
            yw->setYW_skyRender(1);
        }
        else
        {
            GFXFlags &= ~World::GFX_FLAG_SKYRENDER;
            yw->setYW_skyRender(0);
        }

    }

    if ( _settingsChangeOptions & 0x800 )
    {
        if ( confGFXFlags & World::GFX_FLAG_SOFTMOUSE )
        {
            GFXFlags |= World::GFX_FLAG_SOFTMOUSE;
            yw->_preferences |= World::PREF_SOFTMOUSE;
            GFX::Engine.setWDD_cursor(1);
        }
        else
        {
            GFXFlags &= ~World::GFX_FLAG_SOFTMOUSE;
            yw->_preferences &= ~World::PREF_SOFTMOUSE;
            GFX::Engine.setWDD_cursor(0);
        }

    }

    if ( _settingsChangeOptions & 0x20 )
    {
        enemyIndicator = confEnemyIndicator;

        if ( enemyIndicator )
            p_YW->_preferences |= World::PREF_ENEMYINDICATOR;
        else
            p_YW->_preferences &= ~World::PREF_ENEMYINDICATOR;
    }

    if ( _settingsChangeOptions & 0x40 )
    {
        fxnumber = confFxNumber;
        yw->_fxLimit = fxnumber;
    }

    if ( _settingsChangeOptions & 0x100 )
    {
        musicVolume = confMusicVolume;

        SFXEngine::SFXe.SetMusicVolume(confMusicVolume);
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_AMBIENT_VOLUME )
    {
        confAmbientSoundVolume = std::max<int16_t>(0, std::min<int16_t>(127, confAmbientSoundVolume));
        ambientSoundVolume = confAmbientSoundVolume;
        System::IniConf::GameAmbientSoundVolume.Value = std::to_string(ambientSoundVolume);

        if ( !SaveKeyToNucleusIni("game.ambient_sound_volume", std::to_string(ambientSoundVolume)) )
            ypa_log_out("WARNING: Could not save game.ambient_sound_volume to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & 0x80 )
    {
        soundVolume = confSoundVolume;
        SFXEngine::SFXe.setMasterVolume(soundVolume);
    }

    if ( _settingsChangeOptions & 1 )
    {
        if ( _gfxMode != p_YW->_gfxMode && _gfxMode)
            p_YW->_gfxMode = _gfxMode;
    }

    if ( _settingsChangeOptions & 0x1000 )
    {
        if ( !conf3DGuid.empty() )
        {
            if ( StriCmp(conf3DGuid, win3d_guid) )
            {
                win3d_name = conf3DName;
                win3d_guid = conf3DGuid;

                GFX::Engine.SetDeviceByGUID(win3d_guid); //Save to file current gfx device

                p_YW->_gfxMode = Common::Point(GFX::DEFAULT_WIDTH, GFX::DEFAULT_HEIGHT);
                forceChange = true;
            }
        }
    }

    if ( _settingsChangeOptions & 4 )
    {
        if ( confGFXFlags & World::GFX_FLAG_16BITTEXTURE )
        {
            GFXFlags |= World::GFX_FLAG_16BITTEXTURE;
            GFX::Engine.setWDD_16bitTex(1);
        }
        else
        {
            GFXFlags &= ~World::GFX_FLAG_16BITTEXTURE;
            GFX::Engine.setWDD_16bitTex(0);
        }

        forceChange = true;
    }

    if ( _settingsChangeOptions & 0x400 )
    {
        if ( confGFXFlags & World::GFX_FLAG_WINDOWED )
            GFXFlags |= World::GFX_FLAG_WINDOWED;
        else
            GFXFlags &= ~World::GFX_FLAG_WINDOWED;

        forceChange = true;
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_PALETTE_THEME )
    {
        // OpenUA: apply & persist the selected fullscreen visual filter (Data/Filters/*.pal).
        paletteTheme = confPaletteTheme;
        System::IniConf::GfxVisualFilter.Value = PaletteThemeStorageValue(paletteTheme);
        SavePaletteThemeCache(paletteTheme);

        // Apply immediately so the change is visible without restarting.
        GFX::Engine.SetVisualFilter(PaletteThemeStorageValue(paletteTheme));

        if ( !SavePaletteThemeToNucleusIni() )
            ypa_log_out("WARNING: Could not save gfx.visual_filter to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR )
    {
        System::IniConf::GameRoboPlayerAIBehavior.Value = confPlayerRoboAIBehavior;

        if ( !SavePlayerRoboAIBehaviorToNucleusIni() )
            ypa_log_out("WARNING: Could not save game.robo_player_ai_behavior to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_SPECTATOR_MODE )
    {
        System::IniConf::GameSpectatorMode.Value = confSpectatorMode;

        if ( !SaveSpectatorModeToNucleusIni() )
            ypa_log_out("WARNING: Could not save game.spectator_mode to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS )
    {
        System::IniConf::GamePlayAsOtherFactions.Value = confPlayAsOtherFactions;

        if ( !SaveKeyToNucleusIni("game.play_as_other_factions", confPlayAsOtherFactions ? "yes" : "no") )
            ypa_log_out("WARNING: Could not save game.play_as_other_factions to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_BLENDING )
    {
        System::IniConf::GfxBlending.Value = (int32_t)confBlending;
        if ( !SaveKeyToNucleusIni("gfx.blending", std::to_string(confBlending)) )
            ypa_log_out("WARNING: Could not save gfx.blending to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_MAXFPS )
    {
        confMaxFps = NormalizeFrameRateLimit(confMaxFps);
        System::IniConf::GfxMaxFps.Value = (int32_t)confMaxFps;
        GFX::Engine.fpsLimitter(confMaxFps);
        if ( !SaveKeyToNucleusIni("gfx.maxfps", std::to_string(confMaxFps)) )
            ypa_log_out("WARNING: Could not save gfx.maxfps to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_MOVIE_PLAYER )
    {
        System::IniConf::GfxMoviePlayer.Value = confMoviePlayer;
        if ( !SaveKeyToNucleusIni("gfx.movie_player", confMoviePlayer ? "yes" : "no") )
            ypa_log_out("WARNING: Could not save gfx.movie_player to nucleus.ini\n");
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_MENU_FONT )
    {
        // OpenUA: persist menu font with the same nucleus.ini writer used by
        // gfx.blending, but store it as a safe single token because font display
        // names contain spaces. Example on disk:
        //   ui.menu_font = Liberation_Mono_Regular
        // The menu still shows the decoded display name: Liberation Mono Regular.
        menuFont = NormalizeMenuFontName(confMenuFont);
        const std::string storedMenuFont = System::MenuFontStorageValue(menuFont);
        System::IniConf::UiMenuFont.Value = storedMenuFont;

        if ( !SaveKeyToNucleusIni("ui.menu_font", storedMenuFont) )
            ypa_log_out("WARNING: Could not save ui.menu_font to nucleus.ini\n");
        else
            ypa_log_out("OpenUA: saved ui.menu_font = %s (%s)\n", menuFont.c_str(), storedMenuFont.c_str());
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_DEFAULT_CAMERA_VIEW )
    {
        defaultCockpitCamera = confDefaultCockpitCamera;
        cockpitCameraRuntimeMode = confDefaultCockpitCamera;
    }

    if ( _settingsChangeOptions & SETTINGS_CHANGE_INTERFACE_STYLE )
    {
        interfaceStyle = confInterfaceStyle;
        GFX::Engine.SetVirtualUIStyle(interfaceStyle);

        const bool retroInterface = interfaceStyle == GFX::VirtualUIStyle::RETRO;
        System::IniConf::UiRetroInterface.Value = retroInterface;
        if ( !SaveKeyToNucleusIni("ui.retro_interface", retroInterface ? "yes" : "no") )
            ypa_log_out("WARNING: Could not save ui.retro_interface to nucleus.ini\n");
    }

    if ( forceChange )
    {
        yw->SetGameShellVideoMode( IsWindowedFlag() );

        int v24 = 0;
        for (const GFX::GfxMode &nod : GFX::GFXEngine::Instance.GetAvailableModes())
        {
            if ( yw->_gfxMode == nod )
            {
                _gfxModeIndex = v24;
                video_button->SetText(1156, nod.name);

                break;
            }

            v24++;
        }
    }

    bool settingsChanged = _settingsChangeOptions != 0;

    if ( settingsChanged && !UserName.empty() )
        SaveSettings();

    _settingsChangeOptions = 0;
    EnvMode = ENVMODE_TITLE;

    video_button->HideScreen();

    if ( video_listvw.IsOpen() )
        p_YW->GuiWinClose( &video_listvw );

    if ( d3d_listvw.IsOpen() )
        p_YW->GuiWinClose( &d3d_listvw );

    NC_STACK_button::button_66arg v38;
    v38.field_4 = 2;
    v38.butID = 1156;

    video_button->SetState(&v38);

    v38.butID = 1172;
    video_button->SetState(&v38);

    titel_button->ShowScreen();
}


void UserData::sub_46DC1C()
{
    uamessage_load lvlMsg;
    lvlMsg.msgID = UAMSG_LOAD;
    lvlMsg.owner = 0;
    lvlMsg.level = netLevelID;

    p_YW->NetBroadcastMessage(&lvlMsg, sizeof(lvlMsg), true);

    p_YW->_netDriver->FlushBroadcastBuffer();

    envAction.action = EnvAction::ACTION_NETPLAY;
    envAction.params[0] = netLevelID;
    network_listvw.firstShownEntries = 0;
    envAction.params[1] = netLevelID;

    int v12 = 1;
    p_YW->_netDriver->LockSession(&v12);

    yw_NetPrintStartInfo();
}


int UserData::ypaworld_func158__sub0__sub7()
{
    FSMgr::FileHandle *fl = uaOpenFileAlloc(fmt::sprintf("save:%s/sgisold.txt",UserName), "r");
    if ( !fl )
        return 0;

    delete fl;
    return 1;
}



void UserData::ShowToolTip(int id)
{
    // OpenUA: ui.hide_menu_hints (default yes) suppresses the legacy passive menu
    // hover/help description texts (e.g. "Turn Sky On/Off...", "Go to Save/Load Screen").
    // This is the single central place where menu buttons assign their hover hint, so the
    // gate is fully centralized; no language.lng strings are touched. Set the key to "no"
    // to restore the original behavior.
    if ( System::IniConf::UiHideMenuHints.Get<bool>() )
        return;

    switch (id)
    {
    default:
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_PROFILESCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_INPUTSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_VIDEOSCREEN);
        break;

    case 1005:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SOUNDSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_QUIT);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGSCREEN);
        break;

    case 1011:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_BRIEFRESET);
        break;

    case 1013:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_BRIEFFAST);
        break;

    case 1014:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_PLAYLEVEL);
        break;

    case 1015:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LOADLAST);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSCREEN);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP:
    case 1052:
    case 1107:
    case 1167:
    case 1218:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_HELP);
        break;

    case UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LVLSELECTMAP);
        break;

    case 1019:
        if ( p_YW->_levelInfo.State == TLevelInfo::STATE_MENU )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_TOMAIN);
        else if ( p_YW->_levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_EXITDEBRIEF);
        else
            p_YW->SetShowingTooltip(Locale::TIP_MENU_BACKTOLVLSEL);
        break;

    case 1020:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_GOTOLOADSAVE);
        break;

    case 1050:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_USEJOY);
        break;

    case 1051:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_APPLY);
        break;

    case 1053:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_RESET);
        break;

    case 1054:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_CANCEL);
        break;

    case 1055:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_FFJOY);
        break;

    case 1056:
        p_YW->SetShowingTooltip(Locale::TIP_INPUT_REMOVEKEY);
        break;

    case 1061:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_ALTJOY);
        break;

    case 1101:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LOADUSER);
        break;

    case 1102:
        p_YW->SetShowingTooltip(Locale::TIP_PROFILE_DEL);
        break;

    case 1103:
        p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_NEW);
        break;

    case 1104:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SAVEUSER);
        break;

    case 1105:
        switch ( diskScreenMode )
        {
        case 1:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_SAVETHIS);
            break;
        case 2:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_LOADTHIS);
            break;
        case 3:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_NEWTHIS);
            break;
        case 4:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_DELTHIS);
            break;
        default:
            break;
        }
        break;

    case 1106:
        switch ( diskScreenMode )
        {
        case 0:
            p_YW->SetShowingTooltip(Locale::TIP_PFOFILE_CLOSE);
            break;
        case 1:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELSAVE);
            break;
        case 2:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELLOAD);
            break;
        case 3:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELNEW);
            break;
        case 4:
            p_YW->SetShowingTooltip(Locale::TIP_PROFILE_CANCELDEL);
            break;
        default:
            break;
        }
        break;
    case 1150:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_16BITTEX);
        break;

    case 1151:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_CHANNELS);
        break;

    case 1152:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_FXVOLUME);
        break;

    case 1154:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MUSICVOLUME);
        break;

    case 1156:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_RESOLUTION);
        break;

    case 1157:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_FARDEPTH);
        break;

    case 1159:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_EXPLODES);
        break;

    case 1160:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_SKY);
        break;

    case 1161:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_APPLY);
        break;

    case 1162:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_CLOSE);
        break;

    case 1163:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_ENEMYSIGNS);
        break;

    case 1164:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_MUSIC);
        break;

    case 1165:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_HWSWMOUSE);
        break;

    case 1166:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_WINDOWED); // windowed
        break;

    case 1174:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_HOSTSTATIONAI);
        break;

    case 1175:
        p_YW->SetShowingTooltip(Locale::TIP_CONF_SPECTATORMODE);
        break;

    case 1172:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SEL3DDEV);
        break;

    case 1201:
        switch ( netSelMode )
        {
        case NETSCREEN_MODE_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELPROV);
            break;
        case NETSCREEN_SESSION_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELSESS);
            break;
        case NETSCREEN_ENTER_NAME:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETPLNAME);
            break;
        case NETSCREEN_CHOOSE_MAP:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSELLVL);
            break;
        case NETSCREEN_INSESSION:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETSTART);
            break;
        default:
            break;
        }
        break;

    case 1202:
        if ( netSelMode == NETSCREEN_SESSION_SELECT )
        {
            p_YW->SetShowingTooltip(Locale::TIP_MENU_SELNEWLEVEL);
        }
        else if ( netSelMode == NETSCREEN_INSESSION )
        {
            p_YW->SetShowingTooltip(Locale::TIP_MENU_SENDTOALL);
        }
        break;

    case 1203:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_NETCLOSE);
        break;

    case 1205:
        switch ( netSelMode )
        {
        case NETSCREEN_SESSION_SELECT:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKPLAYER);
            break;
        case NETSCREEN_ENTER_NAME:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKPROV);
            break;
        case NETSCREEN_CHOOSE_MAP:
        case NETSCREEN_INSESSION:
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NETBACKSESS);
            break;
        default:
            break;
        }
        break;

    case 1206:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELRESIST);
        break;

    case 1207:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELGHOR);
        break;

    case 1208:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELMYKO);
        break;

    case 1209:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SELTAER);
        break;

    case 1219:
        if ( rdyStart )
            p_YW->SetShowingTooltip(Locale::TIP_MENU_NOTREADY);
        else
            p_YW->SetShowingTooltip(Locale::TIP_MENU_READY);
        break;

    case 1225:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_SENDMSG);
        break;

    case 1250:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGCHOOSE);
        break;

    case 1251:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_LANGEXIT);
        break;

    case 1252:
        p_YW->SetShowingTooltip(Locale::TIP_MENU_HELP);
        break;
    }
}

// Go to options menu
void UserData::ShowOptionsMenu()
{
    atmospherePageActive = false;
    if (atmosphere_button)
        atmosphere_button->HideScreen();
    titel_button->HideScreen();

    RefreshPaletteThemes();
    RefreshMenuFonts();
    confPaletteTheme = paletteTheme;
    confMenuFont = menuFont;
    confPlayerRoboAIBehavior = System::IniConf::GameRoboPlayerAIBehavior.Get<bool>();
    confSpectatorMode = System::IniConf::GameSpectatorMode.Get<bool>();
    confPlayAsOtherFactions = System::IniConf::GamePlayAsOtherFactions.Get<bool>();
    confDefaultCockpitCamera = defaultCockpitCamera;
    confInterfaceStyle = interfaceStyle;
    confMaxFps = NormalizeFrameRateLimit(System::IniConf::GfxMaxFps.Get<int32_t>());
    ambientSoundVolume = p_YW->GetAmbientSoundGlobalVolume();
    confAmbientSoundVolume = ambientSoundVolume;
    UpdatePaletteThemeText();
    UpdateMenuFontText();
    UpdateGfxOptionTexts();

    NC_STACK_button::button_66arg state;
    state.butID = 1174;
    state.field_4 = (!confPlayerRoboAIBehavior) + 1;
    video_button->SetState(&state);

    state.butID = 1175;
    state.field_4 = (!confSpectatorMode) + 1;
    video_button->SetState(&state);

    state.butID = 1190;
    state.field_4 = (!confPlayAsOtherFactions) + 1;
    video_button->SetState(&state);

    state.butID = 1189;
    state.field_4 = (confInterfaceStyle == GFX::VirtualUIStyle::RETRO) ? 1 : 2;
    video_button->SetState(&state);

    if ( NC_STACK_button::Slider *ambientSlider = video_button->GetSliderData(1191) )
    {
        ambientSlider->value = confAmbientSoundVolume;
        video_button->Refresh(1191);
        video_button->SetText(1192, std::to_string(confAmbientSoundVolume));
    }


    video_button->ShowScreen();

    EnvMode = ENVMODE_SETTINGS;

    if ( video_listvw.IsOpen() )
    {
        p_YW->GuiWinClose( &video_listvw );
        p_YW->GuiWinOpen( &video_listvw );
    }

    video_listvw.selectedEntry = _gfxModeIndex;
}


void UserData::AtmosphereOptionsLoad()
{
    atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxVisualFilterStrength.Get<std::string>(), 25);
    atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxAtmosphereStrength.Get<std::string>(), 50);
    atmosphereValues[ATMOPT_EXPOSURE] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereExposure.Get<std::string>(), 170, 25, 200);
    atmosphereValues[ATMOPT_CONTRAST] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereContrast.Get<std::string>(), 95, 50, 200);
    atmosphereValues[ATMOPT_SATURATION] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereSaturation.Get<std::string>(), 80, 0, 200);
    atmosphereValues[ATMOPT_VIGNETTE] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxAtmosphereVignette.Get<std::string>(), 60);

    atmosphereValues[ATMOPT_FOG_START] =
        IntFromString(System::IniConf::GfxHorizonFogStart.Get<std::string>(), 4000, 0, 10000);
    atmosphereValues[ATMOPT_FOG_LENGTH] =
        IntFromString(System::IniConf::GfxHorizonFogLength.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_FOG_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxHorizonFogStrength.Get<std::string>(), 80);

    atmosphereValues[ATMOPT_DARK_START] =
        IntFromString(System::IniConf::GfxHorizonDarkStart.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_DARK_LENGTH] =
        IntFromString(System::IniConf::GfxHorizonDarkLength.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_DARK_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxHorizonDarkStrength.Get<std::string>(), 65);

    atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE] =
        IntFromString(System::IniConf::GameWorldUiMaxDistance.Get<std::string>(), 5700, 100, 20000);

    atmosphereValues[ATMOPT_VHS_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxVhsFilterStrength.Get<std::string>(), 60);

    atmosphereSavedValues = atmosphereValues;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider)
        {
            slider->value = (int16_t)atmosphereValues[i];
            atmosphere_button->Refresh(1400 + i);
        }
    }
    UpdateAtmosphereOptionTexts();
}

void UserData::UpdateAtmosphereOptionTexts()
{
    if (!atmosphere_button)
        return;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        std::string text;
        switch (i)
        {
            case ATMOPT_VISUAL_FILTER_STRENGTH:
            case ATMOPT_ATMOSPHERE_STRENGTH:
            case ATMOPT_VIGNETTE:
            case ATMOPT_FOG_STRENGTH:
            case ATMOPT_DARK_STRENGTH:
            case ATMOPT_VHS_STRENGTH:
                text = std::to_string(atmosphereValues[i]) + "%";
                break;
            case ATMOPT_EXPOSURE:
            case ATMOPT_CONTRAST:
            case ATMOPT_SATURATION:
                text = HundredStorageValue(atmosphereValues[i]);
                break;
            default:
                text = std::to_string(atmosphereValues[i]);
                break;
        }
        atmosphere_button->SetText(1420 + i, text);
    }
}

void UserData::AtmosphereOptionsApplyLive()
{
    if (!atmosphere_button)
        return;

    bool changed = false;
    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider && atmosphereValues[i] != slider->value)
        {
            atmosphereValues[i] = slider->value;
            changed = true;
        }
    }

    if (!changed)
        return;

    UpdateAtmosphereOptionTexts();

    // Keep the framebuffer and world-only atmosphere path active internally.
    System::IniConf::GfxAtmosphereFx.Value = true;
    System::IniConf::GfxVisualFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH]);
    System::IniConf::GfxAtmosphereStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH]);
    System::IniConf::GfxAtmosphereExposure.Value = HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE]);
    System::IniConf::GfxAtmosphereContrast.Value = HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST]);
    System::IniConf::GfxAtmosphereSaturation.Value = HundredStorageValue(atmosphereValues[ATMOPT_SATURATION]);
    System::IniConf::GfxAtmosphereVignette.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE]);

    System::IniConf::GfxHorizonFogEnable.Value = true;
    System::IniConf::GfxHorizonFogStart.Value = std::to_string(atmosphereValues[ATMOPT_FOG_START]);
    System::IniConf::GfxHorizonFogLength.Value = std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH]);
    System::IniConf::GfxHorizonFogStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH]);

    System::IniConf::GfxHorizonDarkEnable.Value = true;
    System::IniConf::GfxHorizonDarkStart.Value = std::to_string(atmosphereValues[ATMOPT_DARK_START]);
    System::IniConf::GfxHorizonDarkLength.Value = std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH]);
    System::IniConf::GfxHorizonDarkStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH]);

    System::IniConf::GameWorldUiMaxDistance.Value =
        std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE]);

    System::IniConf::GfxVhsFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH]);

    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);
}

void UserData::AtmosphereOptionsSave()
{
    AtmosphereOptionsApplyLive();

    paletteTheme = confPaletteTheme;
    System::IniConf::GfxVisualFilter.Value = PaletteThemeStorageValue(paletteTheme);
    SavePaletteThemeCache(paletteTheme);
    GFX::Engine.SetVisualFilter(PaletteThemeStorageValue(paletteTheme));
    if (!SavePaletteThemeToNucleusIni())
        ypa_log_out("WARNING: Could not save gfx.visual_filter to nucleus.ini\n");

    const std::array<std::pair<const char *, std::string>, 14> values =
    {{
        {"gfx.visual_filter_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH])},
        {"gfx.atmosphere_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH])},
        {"gfx.atmosphere_exposure", HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE])},
        {"gfx.atmosphere_contrast", HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST])},
        {"gfx.atmosphere_saturation", HundredStorageValue(atmosphereValues[ATMOPT_SATURATION])},
        {"gfx.atmosphere_vignette", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE])},
        {"gfx.horizon_fog_start", std::to_string(atmosphereValues[ATMOPT_FOG_START])},
        {"gfx.horizon_fog_length", std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH])},
        {"gfx.horizon_fog_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH])},
        {"gfx.horizon_dark_start", std::to_string(atmosphereValues[ATMOPT_DARK_START])},
        {"gfx.horizon_dark_length", std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH])},
        {"gfx.horizon_dark_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH])},
        {"game.world_ui_max_distance", std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE])},
        {"gfx.vhs_filter_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH])}
    }};

    for (const auto &entry : values)
    {
        if (!SaveKeyToNucleusIni(entry.first, entry.second))
            ypa_log_out("WARNING: Could not save %s to nucleus.ini\n", entry.first);
    }

    // These are infrastructure switches now enabled by safe internal defaults.
    // Removing them keeps Nucleus.ini focused on artistic values while preserving
    // backward compatibility for users who deliberately add the keys again.
    RemoveKeyFromNucleusIni("gfx.color_effects");
    RemoveKeyFromNucleusIni("gfx.atmosphere_fx");

    System::IniConf::GfxAtmosphereFx.Value = true;
    System::IniConf::GfxHorizonFogEnable.Value = true;
    System::IniConf::GfxHorizonDarkEnable.Value = true;
    SaveKeyToNucleusIni("gfx.horizon_fog_enable", "yes");
    SaveKeyToNucleusIni("gfx.horizon_dark_enable", "yes");

    // Force the final saved state into the current session even when the user
    // opened the page and pressed Save without moving a slider.
    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    atmosphereSavedValues = atmosphereValues;
    _settingsChangeOptions &= ~SETTINGS_CHANGE_PALETTE_THEME;
    atmospherePageActive = false;
    atmosphere_button->HideScreen();
    video_button->ShowScreen();
}

void UserData::AtmosphereOptionsCancel()
{
    atmosphereValues = atmosphereSavedValues;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider)
        {
            slider->value = (int16_t)atmosphereValues[i];
            atmosphere_button->Refresh(1400 + i);
        }
    }

    // Force one live re-apply of the saved snapshot.
    if (ATMOPT_COUNT > 0)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400);
        if (slider)
            slider->value = (int16_t)atmosphereValues[0];
    }
    UpdateAtmosphereOptionTexts();

    // Apply snapshot directly.
    System::IniConf::GfxVisualFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH]);
    System::IniConf::GfxAtmosphereStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH]);
    System::IniConf::GfxAtmosphereExposure.Value = HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE]);
    System::IniConf::GfxAtmosphereContrast.Value = HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST]);
    System::IniConf::GfxAtmosphereSaturation.Value = HundredStorageValue(atmosphereValues[ATMOPT_SATURATION]);
    System::IniConf::GfxAtmosphereVignette.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE]);
    System::IniConf::GfxHorizonFogStart.Value = std::to_string(atmosphereValues[ATMOPT_FOG_START]);
    System::IniConf::GfxHorizonFogLength.Value = std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH]);
    System::IniConf::GfxHorizonFogStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH]);
    System::IniConf::GfxHorizonDarkStart.Value = std::to_string(atmosphereValues[ATMOPT_DARK_START]);
    System::IniConf::GfxHorizonDarkLength.Value = std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH]);
    System::IniConf::GfxHorizonDarkStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH]);
    System::IniConf::GameWorldUiMaxDistance.Value =
        std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE]);
    System::IniConf::GfxVhsFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH]);

    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    confPaletteTheme = paletteTheme;
    _settingsChangeOptions &= ~SETTINGS_CHANGE_PALETTE_THEME;
    UpdatePaletteThemeText();

    atmospherePageActive = false;
    atmosphere_button->HideScreen();
    video_button->ShowScreen();
}

void UserData::AtmosphereOptionsReset()
{
    atmosphereValues =
    {{
        25, 50, 170, 95, 80, 60,
        4000, 2000, 80,
        2000, 2000, 65,
        5700,
        60
    }};

    confPaletteTheme = "Black_Wadi.pal";
    _settingsChangeOptions |= SETTINGS_CHANGE_PALETTE_THEME;
    UpdatePaletteThemeText();

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider)
        {
            slider->value = (int16_t)atmosphereValues[i];
            atmosphere_button->Refresh(1400 + i);
        }
    }

    UpdateAtmosphereOptionTexts();

    // Nudge the live path by temporarily changing the cached first value.
    int first = atmosphereValues[0];
    atmosphereValues[0] = first == 100 ? 99 : first + 1;
    AtmosphereOptionsApplyLive();
}

void UserData::ShowAtmosphereOptionsMenu()
{
    if (!atmosphere_button)
        return;

    RefreshPaletteThemes();
    confPaletteTheme = paletteTheme;
    AtmosphereOptionsLoad();
    UpdatePaletteThemeText();
    atmospherePageActive = true;
    video_button->HideScreen();
    atmosphere_button->ShowScreen();
}

void UserData::ShowSaveLoadMenu()
{
    titel_button->HideScreen();

    // UserAll/current profile cannot be deleted, so center the four visible
    // primary actions as one group when the menu first opens.
    LayoutSaveLoadActionButtons(this, false);
    disk_button->ShowScreen();

    EnvMode = ENVMODE_SELPLAYER;

    for( ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++ )
    {
        if ( !StriCmp(it->name, UserName))
        {
            it->totalElapsedTime = p_YW->_playersStats[1].ElapsedTime;
            break;
        }
    }
    p_YW->GuiWinClose( &disk_listvw );
    p_YW->GuiWinOpen( &disk_listvw );
}

void UserData::ShowInputSettings()
{
    titel_button->HideScreen();

    button_input_button->ShowScreen();

    EnvMode = ENVMODE_INPUT;
    p_YW->GuiWinClose( &input_listview );
    p_YW->GuiWinOpen( &input_listview );
}

void sub_4D9550(NC_STACK_ypaworld *yw, int arg)
{
    UserData *usr = yw->_GameShell;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    std::string wavName;
    if ( usr->default_lang_dll )
        wavName = fmt::sprintf("sounds/speech/%s/9%d.wav", *usr->default_lang_dll, arg);
    else
        wavName = fmt::sprintf("sounds/speech/language/9%d.wav", arg);

    if ( !uaFileExist(std::string("rsrc:") + wavName) )
        wavName = fmt::sprintf("sounds/speech/language/9%d.wav", arg);

    NC_STACK_sample *&pSmpl = usr->samples1.at(World::SOUND_ID_CHAT);

    if ( pSmpl )
    {
        SFXEngine::SFXe.sub_424000(&usr->samples1_info, World::SOUND_ID_CHAT);
        SFXEngine::SFXe.ForceStopSource(&usr->samples1_info, World::SOUND_ID_CHAT);
        pSmpl->Delete();
        pSmpl = NULL;
    }

    pSmpl = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, wavName}});
    if ( pSmpl )
    {
        TSoundSource &pSnd = usr->samples1_info.Sounds.at(World::SOUND_ID_CHAT);
        pSnd.Volume = 500;
        pSnd.Pitch = 0;
        pSnd.PSample = pSmpl->GetSampleData();

        SFXEngine::SFXe.startSound(&usr->samples1_info, World::SOUND_ID_CHAT);
    }

    Common::Env.SetPrefix("rsrc", oldRsrc);
}

void sub_4D0C24(NC_STACK_ypaworld *yw, const std::string &a1, const std::string &a2)
{
    UserData *usr = yw->_GameShell;

    if ( StriCmp(a1, usr->lastSender) )
    {
        if ( usr->msgBuffers.size() )
            usr->msgBuffers.pop_front();

        usr->msgBuffers.push_back( fmt::sprintf("> %s:", a1));
        usr->lastSender = a1;
    }

    if ( usr->msgBuffers.size() >= 31 )
        usr->msgBuffers.pop_front();

    usr->msgBuffers.push_back( a2 );

    if ( usr->netSelMode == UserData::NETSCREEN_INSESSION )
    {
        int v22 = usr->msgBuffers.size() - 6;

        if ( v22 < 0 )
            v22 = 0;

        yw->_GameShell->network_listvw.firstShownEntries = v22;


        yw->_GameShell->network_listvw.numEntries = yw->_GameShell->msgBuffers.size();

        int v24;

        if ( usr->network_listvw.numEntries >= 6 )
            v24 = 6;
        else
            v24 = usr->network_listvw.numEntries;

        yw->_GameShell->network_listvw.shownEntries = v24;
    }
}


void UserData::yw_returnToTitle()
{
    yw_calcPlayerScore(p_YW);
    p_YW->FreeDebrief();

    sub_bar_button->HideScreen();

    titel_button->ShowScreen();

    EnvMode = ENVMODE_TITLE;
    returnToTitle = false;
}

void UserData::ShowLanguageMenu()
{
    titel_button->HideScreen();

    locale_button->ShowScreen();

    EnvMode = ENVMODE_SELLOCALE;
    p_YW->GuiWinClose( &local_listvw );
    p_YW->GuiWinOpen( &local_listvw );

    int i = 0;
    for(const auto &x : lang_dlls)
    {
        if ( &x == default_lang_dll )
            break;

        i++;
    }

    local_listvw.selectedEntry = i;
}

void sub_4EDCD8(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.Stage = TBriefengScreen::STAGE_CANCEL;
}

void UserData::ShowMenuMsgBox(int code, const std::string &txt1, const std::string &txt2, bool okOnly)
{
    _menuMsgBoxCode = code;

    Gui::UAMessageBox *bx = _menuMsgBox->GetMsgBox();
    bx->SetInform(okOnly);
    bx->Result = 0;
    bx->SetTexts(txt1, txt2);

    _menuMsgBox->ToFront();
    _menuMsgBox->SetEnable(true);
}


void UserData::ShowConfirmDialog( int a2, const std::string &txt1, const std::string &txt2, int a5)
{
    confirmMode = a2;

    const SDL_Color boxTextColor = p_YW->GetFactionBoxTextColor();
    for ( int buttonId = 1300; buttonId <= 1303; ++buttonId )
        confirm_button->SetTextColor(buttonId, boxTextColor.r,
                                     boxTextColor.g, boxTextColor.b);

    NC_STACK_button::button_66arg v12;
    v12.butID = 1300;
    confirm_button->Enable(&v12);


    NC_STACK_button::button_arg76 v10;

    if ( a5 )
    {
        v10.butID = 1300;
        v10.xpos = p_YW->_screenSize.x * 0.4375;
        v10.ypos = -1;
        v10.width = -1;
        //v11 = -1;
        confirm_button->setXYWidth(&v10);
    }
    else
    {
        v12.butID = 1301;
        confirm_button->Enable(&v12);

        v10.butID = 1300;
        v10.xpos = p_YW->_screenSize.x * 0.25;
        v10.ypos = -1;
        v10.width = -1;
        //v11 = -1;
        confirm_button->setXYWidth(&v10);

        v10.butID = 1301;
        v10.xpos = p_YW->_screenSize.x * 0.625;
        confirm_button->setXYWidth(&v10);
    }

    confirm_button->SetText(1302, txt1);

    if ( !txt2.empty() )
        confirm_button->SetText(1303, txt2);
    else
        confirm_button->SetText(1303, " ");

    confirm_button->ShowScreen();
}

void ypaworld_func158__sub0__sub9(NC_STACK_ypaworld *yw)
{
    yw->_briefScreen.Stage = TBriefengScreen::STAGE_PLAYLEVEL;
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

void UserData::sub_46C5F0()
{
    if ( video_listvw.selectedEntry != _gfxModeIndex)
    {
        _gfxModeIndex = video_listvw.selectedEntry;

        _gfxMode = GFX::GFXEngine::Instance.GetAvailableModes().at(_gfxModeIndex);
        video_button->SetText(1156, _gfxMode.name);
    }
}

//Options Cancel
void UserData::sub_46A3C0()
{
    _settingsChangeOptions = 0;
    EnvMode = ENVMODE_TITLE;
    confPaletteTheme = paletteTheme;
    confMenuFont = menuFont;
    confPlayerRoboAIBehavior = System::IniConf::GameRoboPlayerAIBehavior.Get<bool>();
    confSpectatorMode = System::IniConf::GameSpectatorMode.Get<bool>();
    confPlayAsOtherFactions = System::IniConf::GamePlayAsOtherFactions.Get<bool>();
    confDefaultCockpitCamera = defaultCockpitCamera;
    confInterfaceStyle = interfaceStyle;
    confMaxFps = NormalizeFrameRateLimit(System::IniConf::GfxMaxFps.Get<int32_t>());
    confAmbientSoundVolume = ambientSoundVolume;

    int gfxId = GFX::GFXEngine::Instance.GetGfxModeIndex(p_YW->_gfxMode);

    if (gfxId < 0)
        gfxId = 0;

    video_listvw.selectedEntry = gfxId;
    _gfxModeIndex = gfxId;
    _gfxMode = p_YW->_gfxMode;

    video_button->SetText(1156, _gfxMode.name);
    UpdatePaletteThemeText();
    UpdateMenuFontText();

    video_button->SetText(1172, win3d_name);

    conf3DGuid = win3d_guid;
    conf3DName = win3d_name;

    NC_STACK_button::button_66arg v10;
    v10.butID = 1151;
    v10.field_4 = ((soundFlags & World::SF_INVERTLR) == 0) + 1;
    video_button->SetState(&v10);

    v10.field_4 = ((soundFlags & World::SF_CDSOUND) == 0) + 1;
    v10.butID = 1164;
    video_button->SetState(&v10);

    v10.butID = 1157;
    v10.field_4 = ((GFXFlags & World::GFX_FLAG_FARVIEW) == 0) + 1;
    video_button->SetState(&v10);

    v10.field_4 = ((GFXFlags & World::GFX_FLAG_SKYRENDER) == 0) + 1;
    v10.butID = 1160;
    video_button->SetState(&v10);

    v10.butID = 1150;
    v10.field_4 = ((GFXFlags & World::GFX_FLAG_16BITTEXTURE) == 0) + 1;
    video_button->SetState(&v10);

    v10.butID = 1166;
    v10.field_4 = (!IsWindowedFlag()) + 1;
    video_button->SetState(&v10);

    v10.butID = 1165;
    v10.field_4 = ((GFXFlags & World::GFX_FLAG_SOFTMOUSE) == 0) + 1;
    video_button->SetState(&v10);

    v10.field_4 = (enemyIndicator == 0) + 1;
    v10.butID = 1163;
    video_button->SetState(&v10);

    v10.field_4 = (!confPlayerRoboAIBehavior) + 1;
    v10.butID = 1174;
    video_button->SetState(&v10);

    v10.field_4 = (!confSpectatorMode) + 1;
    v10.butID = 1175;
    video_button->SetState(&v10);

    v10.field_4 = (!confPlayAsOtherFactions) + 1;
    v10.butID = 1190;
    video_button->SetState(&v10);

    // OpenUA: reset modern graphics options to the saved/config values
    confBlending = System::IniConf::GfxBlending.Get<int32_t>();
    confMoviePlayer = System::IniConf::GfxMoviePlayer.Get<bool>();
    confMenuFont = menuFont;
    v10.field_4 = (!confMoviePlayer) + 1;
    v10.butID = 1184;
    video_button->SetState(&v10);
    v10.field_4 = (confInterfaceStyle == GFX::VirtualUIStyle::RETRO) ? 1 : 2;
    v10.butID = 1189;
    video_button->SetState(&v10);
    UpdateGfxOptionTexts();
    UpdateMenuFontText();

    NC_STACK_button::Slider *tmp = video_button->GetSliderData(1159);
    tmp->value = fxnumber;
    video_button->Refresh(1159);

    tmp = video_button->GetSliderData(1152);
    tmp->value = soundVolume;
    video_button->Refresh(1152);

    tmp = video_button->GetSliderData(1154);
    tmp->value = musicVolume;
    video_button->Refresh(1154);

    tmp = video_button->GetSliderData(1191);
    if ( tmp )
    {
        tmp->value = ambientSoundVolume;
        video_button->Refresh(1191);
        video_button->SetText(1192, std::to_string(ambientSoundVolume));
    }


    video_button->HideScreen();

    if ( video_listvw.IsOpen() )
        p_YW->GuiWinClose( &video_listvw );

    if ( d3d_listvw.IsOpen() )
        p_YW->GuiWinClose( &d3d_listvw );

    titel_button->ShowScreen();

    v10.field_4 = 2;
    v10.butID = 1156;
    video_button->SetState(&v10);

    v10.butID = 1172;
    video_button->SetState(&v10);
}

void  UserData::UpdateSelected3DDevFromList()
{
    std::string name;
    std::string guid;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();

    if ((size_t)d3d_listvw.selectedEntry < devices.size())
    {
        const GFX::TGFXDeviceInfo &dev = devices.at(d3d_listvw.selectedEntry);
        if ( !StriCmp(dev.name, "software") )
            name = Locale::Text::Advanced(Locale::ADV_SOFTWARE);
        else
            name = dev.name;

        guid = dev.guid;
    }

    conf3DName = name;
    conf3DGuid = guid;

    video_button->SetText(1172, name);
}

void UserData::RefreshPaletteThemes()
{
    paletteThemes.clear();
    paletteThemes.push_back(std::string());

    // OpenUA: enumerate modern fullscreen filters from Data/Filters/*.pal.
    // Missing folder simply yields the single "Standard" (none) entry -> identical rendering.
    FSMgr::DirIter dir = uaOpenDir("data:Filters");
    FSMgr::iNode *node = NULL;

    while (dir.getNext(&node))
    {
        if (!node || node->getType() != FSMgr::iNode::NTYPE_FILE)
            continue;

        std::string name = node->getName();
        if (name.size() < 4 || StriCmp(name.substr(name.size() - 4), ".pal"))
            continue;

        paletteThemes.push_back(name);
    }

    std::sort(paletteThemes.begin() + 1, paletteThemes.end(),
        [](const std::string &a, const std::string &b) { return StriCmp(a, b) < 0; });

    // gfx.visual_filter is global: Nucleus.ini remains authoritative across
    // restarts and player-profile creation/switching.
    std::string currentTheme =
        NormalizePaletteThemeName(System::IniConf::GfxVisualFilter.Get<std::string>());

    bool found = currentTheme.empty();
    for (const std::string &theme : paletteThemes)
    {
        if ( !StriCmp(theme, currentTheme) )
        {
            found = true;
            break;
        }
    }

    paletteTheme = found ? currentTheme : std::string();
    confPaletteTheme = paletteTheme;
}

void UserData::UpdatePaletteThemeText()
{
    if (atmosphere_button)
        atmosphere_button->SetText(1392, PaletteThemeDisplayName(confPaletteTheme));
}

void UserData::CyclePaletteTheme()
{
    if (paletteThemes.empty())
        RefreshPaletteThemes();

    size_t next = 0;
    for (size_t i = 0; i < paletteThemes.size(); i++)
    {
        if (!StriCmp(paletteThemes[i], confPaletteTheme))
        {
            next = (i + 1) % paletteThemes.size();
            break;
        }
    }

    confPaletteTheme = paletteThemes[next];
    _settingsChangeOptions |= SETTINGS_CHANGE_PALETTE_THEME;
    UpdatePaletteThemeText();
}

void UserData::RefreshMenuFonts()
{
    menuFonts.clear();
    menuFonts.push_back("Default");

    System::RescanFonts();

    std::vector<std::string> scannedFonts = System::GetScannedFontNames();
    menuFonts.insert(menuFonts.end(), scannedFonts.begin(), scannedFonts.end());

    // Use the committed in-memory value first. After OK this is updated by the
    // same Options commit path as gfx.blending, so reopening Options in the same
    // session shows the saved choice immediately. On first startup menuFont is
    // empty, so we read the parsed nucleus.ini value.
    std::string currentFont = NormalizeMenuFontName(menuFont.empty() ? System::GetConfiguredMenuFontName() : menuFont);
    bool found = !StriCmp(currentFont, "Default");

    if (!found)
    {
        for (const std::string &fontName : menuFonts)
        {
            if (!StriCmp(fontName, currentFont))
            {
                currentFont = fontName;
                found = true;
                break;
            }
        }
    }

    menuFont = found ? currentFont : std::string("Default");
    confMenuFont = menuFont;
}

void UserData::UpdateMenuFontText()
{
    video_button->SetText(1186, MenuFontDisplayName(confMenuFont));
}

void UserData::CycleMenuFont()
{
    if (menuFonts.empty())
        RefreshMenuFonts();

    if (menuFonts.size() <= 1)
    {
        confMenuFont = "Default";
        UpdateMenuFontText();
        return;
    }

    size_t next = 0;
    for (size_t i = 0; i < menuFonts.size(); i++)
    {
        if (!StriCmp(menuFonts[i], confMenuFont))
        {
            next = (i + 1) % menuFonts.size();
            break;
        }
    }

    confMenuFont = menuFonts[next];
    _settingsChangeOptions |= SETTINGS_CHANGE_MENU_FONT;
    UpdateMenuFontText();
}

bool UserData::SavePaletteThemeToNucleusIni()
{
    // OpenUA: the Atmosphere selector persists the modern visual filter name.
    const std::string key = "gfx.visual_filter";
    const std::string newLine = key + " = " + PaletteThemeStorageValue(paletteTheme);

    std::vector<std::string> lines;
    bool replaced = false;
    const std::string nucleusIni = uaDataFirstNucleusIniPath();

    FSMgr::FileHandle *in = uaOpenFileAlloc(nucleusIni, "r");
    if (in)
    {
        std::string line;
        while (in->ReadLine(&line))
        {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            std::string test = line;
            size_t comment = test.find_first_of(";");
            if (comment != std::string::npos)
                test.erase(comment);

            Stok tokens(test, "= \t");
            std::string token;
            if (tokens.GetNext(&token) && !StriCmp(token, key))
            {
                lines.push_back(newLine);
                replaced = true;
            }
            else
            {
                lines.push_back(line);
            }
        }

        delete in;
    }

    if (!replaced)
        lines.push_back(newLine);

    FSMgr::FileHandle *out = uaOpenFileAlloc(nucleusIni, "w");
    if (!out)
        return false;

    for (const std::string &line : lines)
        out->puts(line + "\n");

    delete out;
    return true;
}

// OpenUA: generic "key = value" writer for nucleus.ini. Replaces the line for `key`
// if present, otherwise appends it. ALL other lines (including hidden/legacy settings)
// are preserved verbatim, so saving Options never erases unrelated settings.
bool UserData::SaveKeyToNucleusIni(const std::string &key, const std::string &value)
{
    std::string saveValue = value;
    if (!StriCmp(key, "gfx.visual_filter_strength"))
        saveValue = VisualFilterStrengthStorageValue(VisualFilterStrengthPercentFromString(value, 25));

    const std::string newLine = key + " = " + saveValue;

    std::vector<std::string> lines;
    bool replaced = false;
    const std::string nucleusIni = uaDataFirstNucleusIniPath();

    FSMgr::FileHandle *in = uaOpenFileAlloc(nucleusIni, "r");
    if (in)
    {
        std::string line;
        while (in->ReadLine(&line))
        {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            std::string test = line;
            size_t comment = test.find_first_of(";");
            if (comment != std::string::npos)
                test.erase(comment);

            Stok tokens(test, "= \t");
            std::string token;
            if (tokens.GetNext(&token) && !StriCmp(token, key))
            {
                if (!replaced)
                    lines.push_back(newLine);
                replaced = true;
            }
            else
            {
                lines.push_back(line);
            }
        }

        delete in;
    }

    if (!replaced)
        lines.push_back(newLine);

    FSMgr::FileHandle *out = uaOpenFileAlloc(nucleusIni, "w");
    if (!out)
        return false;

    for (const std::string &line : lines)
        out->puts(line + "\n");

    delete out;
    return true;
}


bool UserData::RemoveKeyFromNucleusIni(const std::string &key)
{
    const std::string nucleusIni = uaDataFirstNucleusIniPath();
    FSMgr::FileHandle *in = uaOpenFileAlloc(nucleusIni, "r");
    if (!in)
        return true;

    std::vector<std::string> lines;
    std::string line;
    while (in->ReadLine(&line))
    {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        std::string test = line;
        size_t comment = test.find_first_of(";");
        if (comment != std::string::npos)
            test.erase(comment);

        Stok tokens(test, "= \t");
        std::string token;
        if (tokens.GetNext(&token) && !StriCmp(token, key))
            continue;

        lines.push_back(line);
    }
    delete in;

    FSMgr::FileHandle *out = uaOpenFileAlloc(nucleusIni, "w");
    if (!out)
        return false;

    for (const std::string &kept : lines)
        out->puts(kept + "\n");
    delete out;
    return true;
}

// OpenUA: refresh the captions of the modern graphics cycle-buttons.
void UserData::UpdateGfxOptionTexts()
{
    video_button->SetText(1183, BlendingLabel(confBlending));
    video_button->SetText(1187, std::to_string(NormalizeFrameRateLimit(confMaxFps)));
    video_button->SetText(1188, DefaultCameraViewLabel(confDefaultCockpitCamera));
}

bool UserData::SavePlayerRoboAIBehaviorToNucleusIni()
{
    const std::string key = "game.robo_player_ai_behavior";
    const std::string newLine = key + std::string(" = ") + (System::IniConf::GameRoboPlayerAIBehavior.Get<bool>() ? "yes" : "no");

    std::vector<std::string> lines;
    bool replaced = false;
    const std::string nucleusIni = uaDataFirstNucleusIniPath();

    FSMgr::FileHandle *in = uaOpenFileAlloc(nucleusIni, "r");
    if (in)
    {
        std::string line;
        while (in->ReadLine(&line))
        {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            std::string test = line;
            size_t comment = test.find_first_of(";");
            if (comment != std::string::npos)
                test.erase(comment);

            Stok tokens(test, "= \t");
            std::string token;
            if (tokens.GetNext(&token) && !StriCmp(token, key))
            {
                lines.push_back(newLine);
                replaced = true;
            }
            else
            {
                lines.push_back(line);
            }
        }

        delete in;
    }

    if (!replaced)
        lines.push_back(newLine);

    FSMgr::FileHandle *out = uaOpenFileAlloc(nucleusIni, "w");
    if (!out)
        return false;

    for (const std::string &line : lines)
        out->puts(line + "\n");

    delete out;
    return true;
}

bool UserData::SaveSpectatorModeToNucleusIni()
{
    const std::string key = "game.spectator_mode";
    const std::string newLine = key + std::string(" = ") + (System::IniConf::GameSpectatorMode.Get<bool>() ? "yes" : "no");

    std::vector<std::string> lines;
    bool replaced = false;
    const std::string nucleusIni = uaDataFirstNucleusIniPath();

    FSMgr::FileHandle *in = uaOpenFileAlloc(nucleusIni, "r");
    if (in)
    {
        std::string line;
        while (in->ReadLine(&line))
        {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();

            std::string test = line;
            size_t comment = test.find_first_of(";");
            if (comment != std::string::npos)
                test.erase(comment);

            Stok tokens(test, "= \t");
            std::string token;
            if (tokens.GetNext(&token) && !StriCmp(token, key))
            {
                lines.push_back(newLine);
                replaced = true;
            }
            else
            {
                lines.push_back(line);
            }
        }

        delete in;
    }

    if (!replaced)
        lines.push_back(newLine);

    FSMgr::FileHandle *out = uaOpenFileAlloc(nucleusIni, "w");
    if (!out)
        return false;

    for (const std::string &line : lines)
        out->puts(line + "\n");

    delete out;
    return true;
}

void UserData::sub_46C914()
{
    if ( diskListActiveElement )
    {
        ProfileList::iterator it = profiles.begin();

        for (int i = 0; i < diskListActiveElement - 1; i++) // check usr->field_1612 - 1
            it++;

        EnvMode = ENVMODE_SINGLEPLAY;
printf("%s, %d\n",__FILE__, __LINE__);
        p_YW->LoadSettings(fmt::sprintf("%s/user.txt", it->name),
                           it->name,
                           World::SDF_ALL,
                           true);
printf("%s, %d\n",__FILE__, __LINE__);
        UserName = it->name;
        userNameDir = it->name;


        diskScreenMode = 0;
        sgmSaveExist = 0;

        disk_button->HideScreen();

        p_YW->GuiWinClose( &disk_listvw );

        sub_bar_button->ShowScreen();
    }
}

void sub_46D0F8(const std::string &path)
{
    FSMgr::DirIter dir = uaOpenDir(path);
    if ( dir )
    {
        FSMgr::iNode *v5;

        while ( dir.getNext(&v5) )
        {
            if ( v5->getType() == FSMgr::iNode::NTYPE_FILE || (v5->getName().compare(".") && v5->getName().compare("..")) )
                uaDeleteFile(fmt::sprintf("%s/%s", path, v5->getName()));
        }
    }
}

void UserData::sub_46C748()
{
    if ( diskListActiveElement )
    {
        if ( StriCmp(userNameDir, UserName) )
        {

            ProfileList::iterator it = profiles.begin();

            for (int i = 0; i < diskListActiveElement - 1; i++) // check usr->field_1612 - 1
                it++;

            ProfileList::iterator nextIt = std::next(it);

            bool HasElements;

            if ( nextIt != profiles.end() )
                HasElements = true;
            else
            {
                nextIt = std::prev(it);
                HasElements = false;
            }

            std::string a1 = fmt::sprintf("save:%s", it->name);

            sub_46D0F8(a1);

            uaDeleteDir(a1);

            profiles.erase(it);

            disk_listvw.numEntries--;
            if ( profiles.empty() )
            {
                diskListActiveElement = 0;
                userNameDir = "NEWUSER";
            }
            else
            {
                if ( !HasElements )
                    diskListActiveElement--;

                userNameDir = nextIt->name;
            }

            userNameDirCursor = userNameDir.size();

            if ( diskListActiveElement )
                disk_listvw.PosOnSelected(diskListActiveElement - 1);

            diskScreenMode = 0;

            disk_button->HideScreen();

            p_YW->GuiWinClose( &disk_listvw );

            if ( diskEnterFromMapSelect )
            {
                EnvMode = ENVMODE_SINGLEPLAY;
                sub_bar_button->ShowScreen();
            }
            else
            {
                EnvMode = ENVMODE_TITLE;
                titel_button->ShowScreen();
            }
        }
    }
}

void UserData::sub_46B0E0()
{
    Engine::StringList::iterator lang = std::next(lang_dlls.begin(), local_listvw.selectedEntry);

    prev_lang = &(*lang);

    if ( prev_lang != default_lang_dll )
    {
        if ( prev_lang )
        {
            default_lang_dll = prev_lang;
            p_YW->ReloadLanguage();
        }
    }

    EnvMode = ENVMODE_TITLE;

    prev_lang = default_lang_dll;

    locale_button->HideScreen();

    p_YW->GuiWinClose( &local_listvw );

    titel_button->ShowScreen();
}

void UserData::ExitFromLanguageMenu()
{
    EnvMode = ENVMODE_TITLE;

    prev_lang = default_lang_dll;

    locale_button->HideScreen();

    p_YW->GuiWinClose( &local_listvw );

    titel_button->ShowScreen();
}


int NC_STACK_ypaworld::sub_449678(TInputState *struc, int kkode)
{
    return struc->KbdLastHit == kkode && ( (struc->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD) || _easyCheatKeys );
}

void UserData::ShowAbout()
{
    titel_button->HideScreen();

    about_button->ShowScreen();

    EnvMode = ENVMODE_ABOUT;
}

void UserData::ShowDatabaseMenu()
{
    if ( !database_button )
        return;

    titel_button->HideScreen();
    db_tab      = 0;
    db_page     = 0;
    db_selected = 0;
    PopulateDatabasePage();
    database_button->ShowScreen();
    EnvMode = ENVMODE_DATABASE;
}

// Truncate a name for display; never modifies the proto itself.
static std::string db_trunc(const std::string &s, int max_len)
{
    if ((int)s.size() <= max_len)
        return s;
    return s.substr(0, max_len - 3) + "...";
}

// Collect valid prototype indices for the active tab.
static void db_collect_valid(const UserData *usr, std::vector<int> &out)
{
    out.clear();
    const std::vector<World::TVhclProto>    &vhcls = usr->p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = usr->p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = usr->p_YW->GetBuildProtos();

    if (usr->db_tab == 0)
    {
        for (int i = 0; i < (int)vhcls.size(); i++)
            if (vhcls[i].Index >= 0 && i != usr->p_YW->GetSpectatorVehicleProtoID()) out.push_back(i);
    }
    else if (usr->db_tab == 1)
    {
        for (int i = 0; i < (int)wpns.size(); i++)
            if (!wpns[i].name.empty()) out.push_back(i);
    }
    else
    {
        for (int i = 0; i < (int)blds.size(); i++)
            if (!blds[i].Name.empty()) out.push_back(i);
    }
}

static std::string db_weapon_display_name(const std::vector<World::TWeapProto> &wpns, int weaponId)
{
    if ( weaponId < 0 || weaponId >= (int)wpns.size() || wpns[weaponId].name.empty() )
        return "none";

    return wpns[weaponId].name;
}

static int db_vehicle_flyer_type(const World::TVhclProto &p)
{
    IDVList::const_iterator it = p.initParams.find(NC_STACK_ypaflyer::FLY_ATT_TYPE);
    if ( it == p.initParams.end() )
        return -1;

    try
    {
        return nonstd::any_cast<int32_t>(it->second.Value);
    }
    catch ( const nonstd::bad_any_cast& )
    {
        return -1;
    }
}

static std::string db_vehicle_model_display_name(const World::TVhclProto &p)
{
    if ( p.is_mimic )
        return "mimic";

    switch ( p.model_id )
    {
        case BACT_TYPES_BACT:
            return "heli";
        case BACT_TYPES_TANK:
            return "tank";
        case BACT_TYPES_ROBO:
            return "robo";
        case BACT_TYPES_MISSLE:
            return "missile";
        case BACT_TYPES_FLYER:
        {
            // Several original script models share the same runtime class.
            // Show the script-facing model name, not the internal class name
            // "flyer", because "flyer" is not a real SCR model keyword.
            switch ( db_vehicle_flyer_type(p) )
            {
                case 0: return "zeppelin";
                case 2: return "glider";
                case 3: return "plane";
                default:return "plane";
            }
        }
        case BACT_TYPES_UFO:
            return "ufo";
        case BACT_TYPES_CAR:
            return "car";
        case BACT_TYPES_GUN:
            return "gun";
        default:
            return "unknown";
    }
}

static std::string db_weapon_model_display_name(const World::TWeapProto &p)
{
    if ( p.IsLaser() )
        return "laser";
    if ( p.IsVerticalLaser() )
        return "vertical_laser";
    if ( p.IsMortar() )
        return "mortar";
    if ( p.IsHomingBomb() )
        return "homing_bomb";

    switch ( p._weaponFlags )
    {
        case World::TWeapProto::WEAPON_FLAGS_GRENADE: return "grenade";
        case World::TWeapProto::WEAPON_FLAGS_ROCKET:  return "rocket";
        case World::TWeapProto::WEAPON_FLAGS_MISSILE: return "missile";
        case World::TWeapProto::WEAPON_FLAGS_BOMB:    return "bomb";
        case World::TWeapProto::WEAPON_FLAGS_OBSAVOID:return "obsavoid";
        default:                                      return "unknown";
    }
}

static std::string db_weapon_aoe_atk_display(const World::TWeapProto &p)
{
    if ( p.aoe_unit_energy <= 0 )
        return Locale::Text::OpenUA(Locale::OUA_DB_NONE);

    // Keep the same player-facing damage scale used by the normal ATK row.
    return fmt::sprintf("%d", p.aoe_unit_energy / 100);
}

static std::string db_optional_int_display(int value)
{
    if ( value <= 0 )
        return Locale::Text::OpenUA(Locale::OUA_DB_NONE);

    return fmt::sprintf("%d", value);
}

static std::string db_float_display(float value)
{
    std::string out = fmt::sprintf("%.2f", value);
    while ( out.size() > 1 && out.back() == '0' )
        out.pop_back();
    if ( !out.empty() && out.back() == '.' )
        out.pop_back();
    return out;
}

static bool db_static_gun_support_active(const World::TVhclProto &p)
{
    return p.model_id == BACT_TYPES_GUN &&
           p.initParams.find(NC_STACK_ypagun::GUN_ATT_NO_FALL) != p.initParams.end();
}

static std::string db_vehicle_job_stars(int value)
{
    // Match the vanilla advanced unit-info scale: every two job points become
    // one marker, with a minimum visible rating of one marker.
    int stars = value / 2;
    if ( stars < 1 )
        stars = 1;

    return std::string((size_t)stars, '*');
}

static bool db_vehicle_has_job_stats(const World::TVhclProto &p)
{
    return p.job_fightrobo_defined || p.job_fighttank_defined ||
           p.job_fightflyer_defined || p.job_fighthelicopter_defined ||
           p.job_fightplane_defined || p.job_fightglider_defined ||
           p.job_fightzeppelin_defined || p.job_fightufo_defined ||
           p.job_fightcar_defined || p.job_fightgun_defined ||
           p.job_conquer_defined || p.job_reconnoitre_defined;
}

static std::string db_compact_pair_line(const std::string &leftLabel,
                                        const std::string &leftValue,
                                        bool hasLeft,
                                        const std::string &rightLabel,
                                        const std::string &rightValue,
                                        bool hasRight)
{
    std::string line;
    if ( hasLeft )
        line = leftLabel + ": " + leftValue;
    if ( hasRight )
    {
        if ( !line.empty() )
            line += "  ";
        line += rightLabel + ": " + rightValue;
    }
    return db_trunc(line, 30);
}

static void db_add_vehicle_job_lines(std::vector<std::string> *lines,
                                     int maxLines,
                                     const World::TVhclProto &p)
{
    if ( !lines || !db_vehicle_has_job_stats(p) )
        return;

    if ( (p.job_fightrobo_defined || p.job_fighttank_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_HOST), db_vehicle_job_stars(p.job_fightrobo), p.job_fightrobo_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_TANKS), db_vehicle_job_stars(p.job_fighttank), p.job_fighttank_defined));
    }
    const bool planeDefined = p.job_fightplane_defined || p.job_fightflyer_defined;
    const int planeJob = p.job_fightplane_defined ? p.job_fightplane : p.job_fightflyer;
    if ( (planeDefined || p.job_fighthelicopter_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_PLANES), db_vehicle_job_stars(planeJob), planeDefined,
            Locale::Text::OpenUA(Locale::OUA_DB_HELIS), db_vehicle_job_stars(p.job_fighthelicopter), p.job_fighthelicopter_defined));
    }
    if ( (p.job_fightglider_defined || p.job_fightzeppelin_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), db_vehicle_job_stars(p.job_fightglider), p.job_fightglider_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), db_vehicle_job_stars(p.job_fightzeppelin), p.job_fightzeppelin_defined));
    }
    if ( (p.job_fightufo_defined || p.job_fightcar_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_UFO), db_vehicle_job_stars(p.job_fightufo), p.job_fightufo_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_CAR), db_vehicle_job_stars(p.job_fightcar), p.job_fightcar_defined));
    }
    if ( p.job_fightgun_defined && (int)lines->size() < maxLines )
    {
        lines->push_back(Locale::Text::OpenUA(Locale::OUA_VS_GUN) + ": " +
                         db_vehicle_job_stars(p.job_fightgun));
    }
    if ( (p.job_conquer_defined || p.job_reconnoitre_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_CONQUER), db_vehicle_job_stars(p.job_conquer), p.job_conquer_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_RECON), db_vehicle_job_stars(p.job_reconnoitre), p.job_reconnoitre_defined));
    }
}

static bool db_weapon_has_energy_multipliers(const World::TWeapProto &p)
{
    return p.energy_robo_defined || p.energy_tank_defined ||
           p.energy_flyer_defined || p.energy_heli_defined ||
           p.energy_plane_defined || p.energy_glider_defined ||
           p.energy_zeppelin_defined || p.energy_ufo_defined ||
           p.energy_car_defined || p.energy_gun_defined;
}

static std::string db_weapon_energy_stars(float value)
{
    if ( value <= 0.0f )
        return "-";

    // Weapon class multipliers are commonly authored in 0.5 steps:
    // 0.5 = one marker, 1.0 = two markers, 1.5 = three markers.
    int stars = (int)(value * 2.0f + 0.5f);
    if ( stars < 1 )
        stars = 1;

    return std::string((size_t)stars, '*');
}

static void db_add_weapon_energy_lines(std::vector<std::string> *lines,
                                       int maxLines,
                                       const World::TWeapProto &p)
{
    if ( !lines || !db_weapon_has_energy_multipliers(p) )
        return;

    if ( (p.energy_robo_defined || p.energy_tank_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_HOST), db_weapon_energy_stars(p.energy_robo), p.energy_robo_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_TANKS), db_weapon_energy_stars(p.energy_tank), p.energy_tank_defined));
    }
    const bool planeDefined = p.energy_plane_defined || p.energy_flyer_defined;
    const float planeEnergy = p.energy_plane_defined ? p.energy_plane : p.energy_flyer;
    if ( (planeDefined || p.energy_heli_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_PLANES), db_weapon_energy_stars(planeEnergy), planeDefined,
            Locale::Text::OpenUA(Locale::OUA_DB_HELIS), db_weapon_energy_stars(p.energy_heli), p.energy_heli_defined));
    }
    if ( (p.energy_glider_defined || p.energy_zeppelin_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), db_weapon_energy_stars(p.energy_glider), p.energy_glider_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), db_weapon_energy_stars(p.energy_zeppelin), p.energy_zeppelin_defined));
    }
    if ( (p.energy_ufo_defined || p.energy_car_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_UFO), db_weapon_energy_stars(p.energy_ufo), p.energy_ufo_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_CAR), db_weapon_energy_stars(p.energy_car), p.energy_car_defined));
    }
    if ( p.energy_gun_defined && (int)lines->size() < maxLines )
    {
        lines->push_back(Locale::Text::OpenUA(Locale::OUA_VS_GUN) + ": " +
                         db_weapon_energy_stars(p.energy_gun));
    }
}

static void db_add_speciality_lines(std::vector<std::string> *lines,
                                    int maxLines,
                                    const std::vector<std::string> &items)
{
    if ( !lines || (int)lines->size() >= maxLines )
        return;

    lines->push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPECIALITIES));

    if ( items.empty() )
    {
        if ( (int)lines->size() < maxLines )
            lines->push_back("- " + Locale::Text::OpenUA(Locale::OUA_DB_NONE));
        return;
    }

    size_t shown = 0;
    for (const std::string &item : items)
    {
        if ( (int)lines->size() >= maxLines )
            break;

        lines->push_back("- " + db_trunc(item, 30));
        shown++;
    }

    if ( shown < items.size() && !lines->empty() )
        lines->back() = "- ...";
}

static std::vector<std::string> db_weapon_specialties(const World::TWeapProto &p)
{
    std::vector<std::string> items;

    if ( p.cluster.enable || p.cluster.generations > 0 || p.cluster.count > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_CLUSTER_WEAPON));
    if ( p.chain.allow )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_CHAIN_WEAPON));
    if ( p.armor_penetration_targets > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_ARMOR_PENETRATION));
    if ( p.debuff.allow || p.debuff.damage != 0 || p.debuff.damage_percent != 0.0 ||
         p.debuff.duration > 0 && (!p.debuff.name.empty() || p.debuff.mindcontrol ||
         p.debuff.force_malus != 0.0 || p.debuff.maxrot_malus != 0.0 ||
         p.debuff.shield_malus != 0.0 || p.debuff.snd_pitch_mult != 1.0 ||
         !p.debuff.fx_vps.empty()) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_DEBUFF));
    if ( p.multi_target > 1 &&
         (p._weaponFlags == World::TWeapProto::WEAPON_FLAGS_MISSILE || p.IsHomingBomb()) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MULTI_TARGET));
    return items;
}

static std::vector<std::string> db_vehicle_specialties(const World::TVhclProto &p)
{
    std::vector<std::string> items;

    if ( p.spawn_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWNER));
    if ( p.spawn_at_death_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWN_ON_DEATH));
    if ( p.power > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MOBILE_POWER_GENERATOR));
    if ( p.radar >= 2 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_RADAR_UNIT));
    if ( !p.unit_guns.empty() )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MOBILE_GUN_PLATFORM));
    if ( p.is_mimic )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MIMIC));
    if ( p.invisible )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_INVISIBILITY));
    if ( p.invulnerable )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_INVULNERABLE));
    if ( p.seek_and_explode )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SEEK_AND_EXPLODE));
    if ( p.proximity_defense_enable || p.proximity_defense_at_death )
        items.push_back(Locale::Text::OpenUA(p.proximity_defense_at_death ? Locale::OUA_DB_PROXIMITY_DEFENSE_AT_DEATH : Locale::OUA_DB_PROXIMITY_DEFENSE));
    if ( p.extra_weapons[0] > 0 || p.extra_weapons[1] > 0 || p.extra_weapons[2] > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MULTI_WEAPON));
    if ( db_static_gun_support_active(p) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_STATIC_GUN_SUPPORT));

    return items;
}

static std::vector<std::string> db_building_specialties(const World::TBuildingProto &p)
{
    std::vector<std::string> items;

    if ( p.Power > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_POWER_GENERATOR));
    if ( p.spawn_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWNER));
    if ( !p.Guns.empty() )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_DEFENSIVE_GUNS));
    if ( p.ModelID == 2 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_RADAR_BUILDING));

    return items;
}

static std::string db_building_model_display_name(const World::TBuildingProto &p)
{
    switch ( p.ModelID )
    {
        case 0: return "building";
        case 1: return "kraftwerk";
        case 2: return "radar";
        case 3: return "defcenter";
        default:return "unknown";
    }
}

static std::string db_database_asset_path(const char *kind, const char *prefix, int id, const char *ext)
{
    return fmt::sprintf("Database/%s/%s_%d.%s", kind, prefix, id, ext);
}

static std::string db_database_flat_asset_path(const char *prefix, int id, const char *ext)
{
    return fmt::sprintf("Database/%s_%d.%s", prefix, id, ext);
}

static const char *db_asset_kind(int tab)
{
    static const char *kinds[] = { "Units", "Weapons", "Buildings" };
    return kinds[tab >= 0 && tab < 3 ? tab : 0];
}

static const char *db_asset_prefix(int tab)
{
    static const char *prefixes[] = { "vehicle", "weapon", "building" };
    return prefixes[tab >= 0 && tab < 3 ? tab : 0];
}

static std::string db_database_asset_path(int tab, int id, const char *ext)
{
    return db_database_asset_path(db_asset_kind(tab), db_asset_prefix(tab), id, ext);
}

static std::string db_database_flat_asset_path(int tab, int id, const char *ext)
{
    return db_database_flat_asset_path(db_asset_prefix(tab), id, ext);
}

static bool db_load_text_file(const std::string &path, std::string *out)
{
    out->clear();

    FSMgr::FileHandle fil = uaOpenFile("data:" + path, "r");
    if ( !fil.OK() )
        return false;

    std::string line;
    while ( fil.ReadLine(&line) )
    {
        if ( out->size() + line.size() > 8192 )
            break;
        *out += line;
    }

    return true;
}

static bool db_load_database_text(int tab, int id, std::string *out)
{
    if ( db_load_text_file(db_database_asset_path(tab, id, "txt"), out) )
        return true;

    return db_load_text_file(db_database_flat_asset_path(tab, id, "txt"), out);
}

static int db_text_width(TileMap *tiles, const std::string &txt)
{
    if ( !tiles )
        return (int)txt.size() * 12;

    int width = 0;
    for (char ch : txt)
        width += tiles->map[(uint8_t)ch].w;
    return width;
}

static std::string db_fit_text(TileMap *tiles, const std::string &txt, int max_width)
{
    if ( max_width <= 0 || txt.empty() )
        return std::string();

    if ( db_text_width(tiles, txt) <= max_width )
        return txt;

    const std::string ellipsis = "...";
    if ( db_text_width(tiles, ellipsis) > max_width )
    {
        std::string tiny;
        for (char ch : ellipsis)
        {
            std::string candidate = tiny + ch;
            if ( db_text_width(tiles, candidate) > max_width )
                break;
            tiny = candidate;
        }
        return tiny;
    }

    std::string out = txt;
    while ( !out.empty() && db_text_width(tiles, out + ellipsis) > max_width )
        out.pop_back();

    return out + ellipsis;
}

static std::string db_centered_proto_id(int proto_id)
{
    // Fixed-width slot: names keep the same X, while two-digit IDs no longer
    // look glued to the left side of the brackets.
    if ( proto_id >= 0 && proto_id < 10 )
        return fmt::sprintf("[ %d  ]", proto_id);
    if ( proto_id >= 10 && proto_id < 100 )
        return fmt::sprintf("[ %d ]", proto_id);
    if ( proto_id >= 100 && proto_id < 1000 )
        return fmt::sprintf("[%d]", proto_id);

    return fmt::sprintf("[%d]", proto_id);
}

static std::string db_make_name_row(const std::string &name,
                                    int proto_id,
                                    bool selected,
                                    int row_width)
{
    const char *sel = selected ? ">" : " ";
    std::string prefix = std::string(sel) + db_centered_proto_id(proto_id) + " ";

    TileMap *tiles = GFX::Engine.GetTileset(16);
    int name_width = row_width - db_text_width(tiles, prefix) - 8;
    if ( name_width < 24 )
        name_width = 24;

    return prefix + db_fit_text(tiles, name, name_width);
}

static bool db_text_fits(TileMap *tiles, const std::string &txt, int max_width)
{
    return db_text_width(tiles, txt) <= max_width;
}

static int db_entry_text_width(NC_STACK_ypaworld *yw)
{
    const int rx = (yw->_screenSize.x * 36) / 100;
    const int rw = yw->_screenSize.x - rx;
    return std::max(120, (rw * 70) / 100);
}

static int db_detail_text_columns(NC_STACK_ypaworld *yw)
{
    // The database screen uses the old shell font through button captions.
    // TileMap pixel metrics under-report its final on-screen width, so pixel
    // wrapping still lets long Italian lore lines get clipped by the widget.
    // Use a conservative character budget scaled from the tested 1536-wide
    // shell instead. This favors readable wrapping over edge-hugging text.
    if ( !yw || yw->_screenSize.x <= 0 )
        return 38;

    int columns = (yw->_screenSize.x * 40) / 1536;
    if ( columns < 30 ) columns = 30;
    if ( columns > 52 ) columns = 52;
    return columns;
}

static void db_push_wrapped_line(TileMap *tiles, const std::string &line, int max_width, int max_lines, std::vector<std::string> *out)
{
    auto push_word = [&](std::string word)
    {
        if ( word.empty() )
            return;

        if ( !out->empty() && !out->back().empty() )
        {
            std::string candidate = out->back() + " " + word;
            if ( db_text_fits(tiles, candidate, max_width) )
            {
                out->back() = candidate;
                return;
            }
        }

        while ( !word.empty() && (int)out->size() < max_lines )
        {
            int cut = (int)word.size();
            while ( cut > 1 && !db_text_fits(tiles, word.substr(0, cut), max_width) )
                cut--;

            out->push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
    };

    size_t pos = 0;
    while ( pos < line.size() && (int)out->size() < max_lines )
    {
        while ( pos < line.size() && (line[pos] == ' ' || line[pos] == '\t') )
            pos++;

        size_t start = pos;
        while ( pos < line.size() && line[pos] != ' ' && line[pos] != '\t' )
            pos++;

        push_word(line.substr(start, pos - start));
    }
}

static std::vector<std::string> db_wrap_text(const std::string &text, int max_width, int max_lines)
{
    std::vector<std::string> out;
    TileMap *tiles = GFX::Engine.GetTileset(16);

    size_t start = 0;
    while ( start <= text.size() && (int)out.size() < max_lines )
    {
        size_t end = text.find('\n', start);
        std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
        if ( !line.empty() && line.back() == '\r' )
            line.pop_back();

        if ( line.empty() )
            out.push_back(std::string(" "));
        else
            db_push_wrapped_line(tiles, line, max_width, max_lines, &out);

        if ( end == std::string::npos )
            break;
        start = end + 1;
    }

    return out;
}

static std::vector<std::string> db_wrap_text_columns(const std::string &text, int max_columns, int max_lines)
{
    std::vector<std::string> out;
    if ( max_columns < 8 )
        max_columns = 8;

    auto push_word = [&](std::string word)
    {
        if ( word.empty() || (int)out.size() >= max_lines )
            return;

        while ( !word.empty() && (int)out.size() < max_lines )
        {
            if ( !out.empty() && !out.back().empty() && out.back() != " " )
            {
                std::string candidate = out.back() + " " + word;
                if ( (int)candidate.size() <= max_columns )
                {
                    out.back() = candidate;
                    return;
                }
            }

            if ( (int)word.size() <= max_columns )
            {
                out.push_back(word);
                return;
            }

            int cut = max_columns;
            while ( cut > 1 && cut < (int)word.size() && ((uint8_t)word[cut] & 0xC0) == 0x80 )
                cut--; // avoid splitting in the middle of a UTF-8 continuation byte
            out.push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
    };

    size_t start = 0;
    while ( start <= text.size() && (int)out.size() < max_lines )
    {
        size_t end = text.find('\n', start);
        std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
        while ( !line.empty() && (line.back() == '\r' || line.back() == '\n') )
            line.pop_back();

        if ( line.empty() )
        {
            out.push_back(" ");
        }
        else
        {
            size_t pos = 0;
            while ( pos < line.size() && (int)out.size() < max_lines )
            {
                while ( pos < line.size() && (line[pos] == ' ' || line[pos] == '\t') )
                    pos++;

                size_t wordStart = pos;
                while ( pos < line.size() && line[pos] != ' ' && line[pos] != '\t' )
                    pos++;

                push_word(line.substr(wordStart, pos - wordStart));
            }
        }

        if ( end == std::string::npos )
            break;
        start = end + 1;
    }

    return out;
}

static NC_STACK_bitmap *db_load_database_image(int tab, int id)
{
    std::string path = db_database_asset_path(tab, id, "png");
    if ( !uaFileExist("data:" + path) )
        path = db_database_flat_asset_path(tab, id, "png");

    if ( !uaFileExist("data:" + path) )
        return NULL;

    std::string oldRsrc = Common::Env.GetPrefix("rsrc");
    Common::Env.SetPrefix("rsrc", "data:");

    NC_STACK_bitmap *img = Utils::ProxyLoadImage({
        {NC_STACK_rsrc::RSRC_ATT_NAME, path},
        {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});

    if ( !oldRsrc.empty() )
        Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( img && img->GetBitmap() )
        return img;

    if ( img )
        img->Delete();
    return NULL;
}

void UserData::ReleaseDatabaseEntryImage()
{
    if ( db_entry_image )
    {
        db_entry_image->Delete();
        db_entry_image = NULL;
    }
}

static void db_get_entry_image_rect(NC_STACK_ypaworld *yw, int *left, int *top, int *width, int *height)
{
    const int lh = yw->_fontH + vertMenuSpace;
    const int rx = (yw->_screenSize.x * 36) / 100;
    const int rw = yw->_screenSize.x - rx;
    const int panelH = yw->_screenSize.y;
    int nav_y = panelH - lh - vertMenuSpace;
    if (nav_y < 10 * lh) nav_y = 10 * lh;

    const int detailLines = 14;
    int imgW = std::max(120, (rw * 70) / 100);
    int imgX = rx + (rw - imgW) / 2;
    int imgY = (detailLines + 3) * lh;
    int imgH = nav_y - imgY - vertMenuSpace;

    if (imgH < lh * 3) imgH = lh * 3;
    if (imgX + imgW > yw->_screenSize.x) imgW = yw->_screenSize.x - imgX;
    if (imgY + imgH > nav_y) imgH = nav_y - imgY - vertMenuSpace;
    if (imgW < 1) imgW = 1;
    if (imgH < 1) imgH = 1;

    *left = imgX;
    *top = imgY;
    *width = imgW;
    *height = imgH;
}

static Common::FRect db_screen_rect_to_ndc(NC_STACK_ypaworld *yw, int left, int top, int width, int height)
{
    const float halfW = (float)yw->_screenSize.x * 0.5f;
    const float halfH = (float)yw->_screenSize.y * 0.5f;

    return Common::FRect(
        ((float)left / halfW) - 1.0f,
        ((float)top / halfH) - 1.0f,
        ((float)(left + width) / halfW) - 1.0f,
        ((float)(top + height) / halfH) - 1.0f);
}

static void db_draw_frame(SDL_Surface *scr, int left, int top, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
    if ( !scr || width <= 0 || height <= 0 )
        return;

    int right = left + width - 1;
    int bottom = top + height - 1;

    GFX::GFXEngine::DrawLine(scr, Common::Line(left, top, right, top), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(right, top, right, bottom), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(right, bottom, left, bottom), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(left, bottom, left, top), r, g, b);
}

static void db_fill_card_background(SDL_Surface *scr, int left, int top, int width, int height)
{
    if ( !scr || width <= 2 || height <= 2 )
        return;

    SDL_Rect rect;
    rect.x = left + 1;
    rect.y = top + 1;
    rect.w = width - 2;
    rect.h = height - 2;

    // Solid dark matte: this makes the image preview read as an intentional UI
    // card instead of a floating PNG inside a mismatched legacy border.
    SDL_FillRect(scr, &rect, SDL_MapRGB(scr->format, 5, 12, 14));
}

void UserData::RenderDatabaseEntryMedia()
{
    if ( !p_YW || db_tab < 0 || db_tab > 2 )
        return;

    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    db_get_entry_image_rect(p_YW, &left, &top, &width, &height);

    SDL_Surface *scr = GFX::Engine.Screen();
    uint8_t r = p_YW->_iniColors[68].r;
    uint8_t g = p_YW->_iniColors[68].g;
    uint8_t b = p_YW->_iniColors[68].b;

    bool hasImage = (db_entry_has_image && db_entry_image && db_entry_image->GetBitmap());

    if ( hasImage && scr )
        db_fill_card_background(scr, left, top, width, height);

    int drawX = left;
    int drawY = top;
    int drawW = width;
    int drawH = height;

    if ( hasImage )
    {
        ResBitmap *bitm = db_entry_image->GetBitmap();
        const int pad = std::max(8, p_YW->_fontH / 3);
        int maxW = std::max(1, width - pad * 2);
        int maxH = std::max(1, height - pad * 2);
        float scale = std::min((float)maxW / (float)bitm->width, (float)maxH / (float)bitm->height);
        drawW = std::max(1, (int)((float)bitm->width * scale));
        drawH = std::max(1, (int)((float)bitm->height * scale));
        drawX = left + (width - drawW) / 2;
        drawY = top + (height - drawH) / 2;

        GFX::rstr_arg204 arg;
        arg.pbitm = bitm;
        arg.float4 = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        arg.float14 = db_screen_rect_to_ndc(p_YW, drawX, drawY, drawW, drawH);
        GFX::Engine.raster_func204(&arg);
    }

    if ( scr )
    {
        // Outer frame: the stable preview card. Missing-image placeholders keep
        // using this full box so the centered "No image" caption remains visible.
        db_draw_frame(scr, left, top, width, height, r, g, b);

        // Inner frame: only when an actual PNG is drawn. It follows the real
        // aspect-fit image rectangle, so non-matching aspect ratios look
        // deliberate instead of visually offset.
        if ( hasImage )
        {
            uint8_t ir = (uint8_t)std::max(32, (int)r / 2);
            uint8_t ig = (uint8_t)std::max(32, (int)g / 2);
            uint8_t ib = (uint8_t)std::max(32, (int)b / 2);
            db_draw_frame(scr, drawX - 1, drawY - 1, drawW + 2, drawH + 2, ir, ig, ib);
        }
    }
}

void UserData::PopulateDatabasePage()
{
    if ( !database_button )
        return;

    // Fill the available vertical space; rows stay name-only and the right pane
    // carries the database text/image entry.
    // Keep this in sync with CreateDatabaseControls().
    static const int LINES = 22;

    const std::vector<World::TVhclProto>    &vhcls = p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = p_YW->GetBuildProtos();

    std::vector<int> valid;
    db_collect_valid(this, valid);
    int total = (int)valid.size();
    int total_pages = (total == 0) ? 1 : (total + LINES - 1) / LINES;

    if (db_page < 0) db_page = 0;
    if (db_page >= total_pages) db_page = total_pages - 1;

    int entries_on_page = total - db_page * LINES;
    if (entries_on_page > LINES) entries_on_page = LINES;
    if (entries_on_page < 0)    entries_on_page = 0;

    if (db_selected >= entries_on_page) db_selected = (entries_on_page > 0) ? entries_on_page - 1 : 0;
    if (db_selected < 0) db_selected = 0;

    const std::string tabLabels[] = {
        Locale::Text::OpenUA(Locale::OUA_DB_UNITS),
        Locale::Text::OpenUA(Locale::OUA_DB_WEAPONS),
        Locale::Text::OpenUA(Locale::OUA_DB_BUILDINGS)
    };
    std::string page_lbl;
    if (total == 0)
        page_lbl = fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_NO_PROTOTYPE_FORMAT), tabLabels[db_tab]);
    else
        page_lbl = fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PAGE_FORMAT),
            tabLabels[db_tab], db_page + 1, total_pages, total);
    database_button->SetText(UIWidgets::DB_LABEL_PAGE, page_lbl);

    int start = db_page * LINES;
    for (int k = 0; k < LINES; k++)
    {
        int btn_id = UIWidgets::DB_LINE_0 + k;
        int idx    = start + k;
        std::string line;

        if (idx < total)
        {
            int pid = valid[idx];
            const int listWidth = (p_YW->_screenSize.x * 34) / 100;

            if (db_tab == 0)
            {
                const World::TVhclProto &p = vhcls[pid];
                line = db_make_name_row(p.name, pid, k == db_selected, listWidth);
            }
            else if (db_tab == 1)
            {
                const World::TWeapProto &p = wpns[pid];
                line = db_make_name_row(p.name, pid, k == db_selected, listWidth);
            }
            else
            {
                const World::TBuildingProto &p = blds[pid];
                line = db_make_name_row(p.Name, pid, k == db_selected, listWidth);
            }
        }
        else
        {
            line = " ";
        }

        database_button->SetText(btn_id, line);
    }

    PopulateDetailPane();
}

void UserData::PopulateDetailPane()
{
    if ( !database_button )
        return;

    // Must match PopulateDatabasePage()/CreateDatabaseControls().
    // If this differs, selected rows on page 2+ show the wrong detail entry.
    static const int LINES = 22;
    static const int DETAIL_LINES = 14;

    const std::vector<World::TVhclProto>    &vhcls = p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = p_YW->GetBuildProtos();

    std::vector<int> valid;
    db_collect_valid(this, valid);
    int total = (int)valid.size();

    database_button->SetText(UIWidgets::DB_DETAIL_HEADER, Locale::Text::OpenUA(Locale::OUA_DATABASE));
    database_button->SetText(UIWidgets::DB_IMAGE_TEXT, " ");
    database_button->SetText(UIWidgets::DB_STATS_HEADER, " ");
    for (int k = 0; k < 12; k++)
        database_button->SetText(UIWidgets::DB_STATS_0 + k, " ");

    int abs_idx = db_page * LINES + db_selected;
    if (abs_idx >= total)
    {
        for (int k = 0; k < DETAIL_LINES; k++)
            database_button->SetText(UIWidgets::DB_DETAIL_0 + k, " ");
        database_button->SetText(UIWidgets::DB_STATS_HEADER, " ");
        for (int k = 0; k < 12; k++)
            database_button->SetText(UIWidgets::DB_STATS_0 + k, " ");
        ReleaseDatabaseEntryImage();
        db_entry_tab = -1;
        db_entry_id = -1;
        db_entry_has_image = false;
        return;
    }

    int pid = valid[abs_idx];
    std::string lines[DETAIL_LINES];
    for (int i = 0; i < DETAIL_LINES; i++) lines[i] = " ";

    std::string entryName;
    if (db_tab == 0)
    {
        entryName = vhcls[pid].name;
    }
    else if (db_tab == 1)
    {
        entryName = wpns[pid].name;
    }
    else
    {
        entryName = blds[pid].Name;
    }

    database_button->SetText(UIWidgets::DB_DETAIL_HEADER, db_trunc(entryName, 28));

    static const int DB_STATS_LINES = 12;
    std::vector<std::string> statLines;
    statLines.reserve(DB_STATS_LINES);
    std::string statHeader = Locale::Text::OpenUA(Locale::OUA_DB_STATS);
    if ( db_tab == 0 )
    {
        const World::TVhclProto &p = vhcls[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_vehicle_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_HP_FORMAT), p.energy));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_DEF_FORMAT), p.shield));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_WEAPON_FORMAT), db_trunc(db_weapon_display_name(wpns, p.weapon), 20)));
        statLines.push_back(p.has_push_resistance && p.push_resistance > 0.0 ?
                            fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PUSH_RESISTANCE_FORMAT), p.push_resistance) :
                            Locale::Text::OpenUA(Locale::OUA_DB_PUSH_RESISTANCE_NONE));
        db_add_vehicle_job_lines(&statLines, DB_STATS_LINES, p);
        db_add_speciality_lines(&statLines, DB_STATS_LINES, db_vehicle_specialties(p));
    }
    else if ( db_tab == 1 )
    {
        const World::TWeapProto &p = wpns[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_weapon_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_ATK_FORMAT), p.energy / 100));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_AOE_ATK_FORMAT), db_weapon_aoe_atk_display(p)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PUSH_FORMAT), db_optional_int_display(p.push)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_AOE_PUSH_FORMAT), db_optional_int_display(p.aoe_unit_push)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_WEAPON_RECOIL_FORMAT),
                            p.recoil > 0.0 ? db_float_display(p.recoil) : Locale::Text::OpenUA(Locale::OUA_DB_NONE)));
        db_add_weapon_energy_lines(&statLines, DB_STATS_LINES, p);
        db_add_speciality_lines(&statLines, DB_STATS_LINES, db_weapon_specialties(p));
    }
    else
    {
        const World::TBuildingProto &p = blds[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_building_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_HP_FORMAT), p.Energy));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_POWER_FORMAT), p.Power));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_GUNS_FORMAT), (int)p.Guns.size()));
        db_add_speciality_lines(&statLines, DB_STATS_LINES, db_building_specialties(p));
    }

    database_button->SetText(UIWidgets::DB_STATS_HEADER, statHeader);
    for (int k = 0; k < DB_STATS_LINES; k++)
        database_button->SetText(UIWidgets::DB_STATS_0 + k, k < (int)statLines.size() ? statLines[k] : " ");

    std::string body;
    if ( !db_load_database_text(db_tab, pid, &body) || body.empty() )
        body = Locale::Text::OpenUA(Locale::OUA_DB_NO_ENTRY);

    const int wrapColumns = db_detail_text_columns(p_YW);
    std::vector<std::string> wrapped = db_wrap_text_columns(body, wrapColumns, DETAIL_LINES);
    for (int i = 0; i < DETAIL_LINES && i < (int)wrapped.size(); i++)
        lines[i] = wrapped[i].empty() ? std::string(" ") : wrapped[i];

    if ( db_entry_tab != db_tab || db_entry_id != pid )
    {
        ReleaseDatabaseEntryImage();
        db_entry_image = db_load_database_image(db_tab, pid);
        db_entry_tab = db_tab;
        db_entry_id = pid;
        db_entry_has_image = (db_entry_image && db_entry_image->GetBitmap());
    }

    if ( !db_entry_has_image )
        database_button->SetText(UIWidgets::DB_IMAGE_TEXT, Locale::Text::OpenUA(Locale::OUA_DB_NO_IMAGE));

    for (int k = 0; k < DETAIL_LINES; k++)
        database_button->SetText(UIWidgets::DB_DETAIL_0 + k, lines[k]);
}

int ypaworld_func158__sub0__sub6(char a1)
{
    if (a1 == '/' || a1 == '\\' || a1 == ':' || a1 == '*' || a1 == '?' || a1 == '"' || a1 == '<' || a1 == '>' || a1 == '|')
        return 0;

    return 1;
}




void UserData::ConnectToServer(std::string connStr){
    fmt::printf("Connectiong to: %s\n", connStr);
    if ( p_YW->_netDriver->Connect(connStr) )
    {
        if (p_YW->_netDriver->HasLobby())
        {
            netSelMode = NETSCREEN_SESSION_SELECT;
            netSel = -1;
            network_listvw.firstShownEntries = 0;

            p_YW->GuiWinOpen( &network_listvw );
        }
        else
        {
            JoinLobbyLessGame();
        }
    }
    else
    {
        printf("Can't connect: Time OUT\n");
    }
}


void UserData::GameShellUiHandleInput()
{
    int v3 = 0;

    if ( _menuMsgBox )
    {
        Gui::UAMessageBox *menuBox = _menuMsgBox->GetMsgBox();

        if ( !_menuMsgBox->IsEnabled() && menuBox->Result != 0 )
        {
            const uint8_t result = menuBox->Result;
            const int msgBoxCode = _menuMsgBoxCode;
            menuBox->Result = 0;
            _menuMsgBoxCode = 0;

            if ( msgBoxCode == MENU_MSGBOX_RESTORE_DEFAULT_KEYS )
            {
                if ( result == 1 )
                    InputConfigRestoreDefault();
            }
            else if ( msgBoxCode == MENU_MSGBOX_INPUT_KEY_CONFLICT )
            {
                if ( result == 1 &&
                     pendingInputTarget > 0 && pendingInputTarget < World::INPUT_BIND_MAX &&
                     pendingInputKeyCode != Input::KC_NONE )
                {
                    ClearInputKeyConflicts(this, pendingInputTarget,
                                           pendingInputPositiveSlot, pendingInputKeyCode);
                    ApplyCapturedInputKey(this, pendingInputTarget,
                                          pendingInputPositiveSlot, pendingInputKeyCode);
                }

                // Cancel leaves key-capture active on the same slot so the player
                // can immediately choose a different key. No binding is changed.
                ClearPendingInputKey(this);
                Input->ClickInf.flag = 0;
                Input->KbdLastHit = Input::KC_NONE;
                Input->KbdLastDown = Input::KC_NONE;
                Input->chr = 0;
                Input->HotKeyID = -1;
                return;
            }
        }

        // The classic menu message box is modal for the old settings UI.
        // Do not let the click that confirms/cancels also activate controls
        // underneath it.
        if ( _menuMsgBox->IsEnabled() )
            return;
    }

    if ( Input->ClickInf.flag & TClickBoxInf::FLAG_BTN_DOWN )
        SFXEngine::SFXe.startSound(&samples1_info, 3);

    if ( netSelMode != NETSCREEN_MODE_SELECT )
        yw_HandleNetMsg(p_YW);

    if ( netSelMode == NETSCREEN_SESSION_SELECT )
    {
        if ( p_YW->_netDriver->GetProvType() == 4 )
        {
            if ( modemAskSession )
            {
                if ( Input->KbdLastHit == Input::KC_SPACE )
                {
                    GFX::Engine.windd_func320(NULL);
                    p_YW->_netDriver->EnumSessions(NULL);
                    GFX::Engine.windd_func321(NULL);
                }
            }
        }
        else if ( p_YW->_netDriver->GetProvType() != 3 || Input->KbdLastHit == Input::KC_SPACE )
        {
            p_YW->_netDriver->EnumSessions(NULL);
        }
    }

    NC_STACK_button::button_66arg v408;
    NC_STACK_button::button_66arg v410;

    v410.field_4 = 0;
    v410.butID = 1015;
    sub_bar_button->Disable(&v410);
    v410.butID = 1011;
    sub_bar_button->Disable(&v410);
    v410.butID = 1019;
    sub_bar_button->Disable(&v410);
    v410.butID = 1014;
    sub_bar_button->Disable(&v410);
    v410.butID = 1013;
    sub_bar_button->Disable(&v410);
    v410.butID = 1020;
    sub_bar_button->Disable(&v410);
    v410.butID = 1027;
    sub_bar_button->Disable(&v410);

    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SAVE_LOAD;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
    titel_button->Disable(&v410);
    v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;
    titel_button->Disable(&v410);

    NC_STACK_button::button_arg76 v393;
    v393.ypos = -1;
    v393.width = -1;
    //v394 = -1;
    v393.butID = 1014;
    v393.xpos = 0;
    sub_bar_button->setXYWidth(&v393);

    v393.butID = 1019;
    v393.xpos = p_YW->_screenSize.x - dword_5A50B6_h;
    sub_bar_button->setXYWidth(&v393);

    v393.butID = 1011;
    v393.xpos = buttonsSpace + dword_5A50B6_h;
    sub_bar_button->setXYWidth(&v393);

    sub_bar_button->SetText(1019,  Locale::Text::GlobMap(Locale::GLOBMAP_GOBACK));

    if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
    {
        if ( p_YW->_levelInfo.State != TLevelInfo::STATE_DEBRIEFING && !GameIsOver )
        {
            v410.butID = 1014;
            sub_bar_button->Enable(&v410);

            v393.butID = 1014;
            v393.xpos = p_YW->_screenSize.x - dword_5A50B6_h;
            sub_bar_button->setXYWidth(&v393);

            v393.butID = 1019;
            v393.xpos = 0;
            sub_bar_button->setXYWidth(&v393);

            sub_bar_button->SetText(1019, Locale::Text::Advanced(Locale::ADV_BACK));
        }

        if ( p_YW->_levelInfo.State == TLevelInfo::STATE_DEBRIEFING )
        {
            v410.butID = 1011;
            sub_bar_button->Enable(&v410);

            v393.butID = 1011;
            v393.xpos = 0;
            sub_bar_button->setXYWidth(&v393);

            sub_bar_button->SetText(1019, Locale::Text::Advanced(Locale::ADV_CONTINUE));
        }

        v410.butID = 1019;
        sub_bar_button->Enable(&v410);

        if ( p_YW->_levelInfo.State == TLevelInfo::STATE_BRIEFING && p_YW->BriefingHasPlayAsChoices() )
        {
            int playAsWidth = p_YW->_screenSize.x * 0.36;

            v410.butID = 1027;
            sub_bar_button->Enable(&v410);

            v393.butID = 1027;
            v393.xpos = (p_YW->_screenSize.x - playAsWidth) / 2;
            v393.width = playAsWidth;
            sub_bar_button->setXYWidth(&v393);
            sub_bar_button->SetText(1027, p_YW->BriefingPlayAsButtonText());
        }

        button_input_button->HideScreen();
        video_button->HideScreen();
        disk_button->HideScreen();
        locale_button->HideScreen();
        network_button->HideScreen();

        p_YW->GuiWinClose( &input_listview );
        p_YW->GuiWinClose( &video_listvw );
        p_YW->GuiWinClose( &disk_listvw );
        p_YW->GuiWinClose( &local_listvw );
        p_YW->GuiWinClose( &network_listvw );
    }
    else if ( EnvMode == ENVMODE_TITLE )
    {
        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_INPUT_SETTINGS;
        titel_button->Enable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_OPTIONS;
        titel_button->Enable(&v410);

        v410.butID = 1001;
        titel_button->Enable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_LANGUAGE;
        if ( lang_dlls.size() > 1 )
            titel_button->Enable(&v410);
        else
            titel_button->Disable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_QUIT;
        titel_button->Enable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_HELP;
        titel_button->Enable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_SINGLE_PLAYER;
        titel_button->Enable(&v410);

        v410.butID = UIWidgets::MAIN_MENU_WIDGET_IDS::BTN_MULTIPLAYER;
        titel_button->Enable(&v410);
    }
    else if ( EnvMode == ENVMODE_SINGLEPLAY || EnvMode == ENVMODE_TUTORIAL )
    {
        if ( !sgmSaveExist )
        {
            if ( IsHasSGM(UserName, 0) )
                sgmSaveExist = 1;
            else
                sgmSaveExist = 2; // No, and We check it, so do not check it twice
        }

        if ( sgmSaveExist == 1 )
        {
            v410.butID = 1015;
            sub_bar_button->Enable(&v410);
        }

        v410.butID = 1019;
        sub_bar_button->Enable(&v410);

        v410.butID = 1020;
        sub_bar_button->Enable(&v410);
    }

    if ( confirmMode )
        v3 = 1;

    NC_STACK_button::ResCode r = confirm_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1350 ) // OK
        {
            switch ( confirmMode )
            {
            case 1:
            {
                sgmSaveExist = 0;
                p_YW->_isNetGame = false;

                sub_bar_button->HideScreen();

                envAction.action = EnvAction::ACTION_LOAD;
                envAction.params[0] = 0;
            }
            break;

            case 2:
                sub_46DC1C();
                break;
            case 3:
                sb_0x46ca74();
                break;
            case 4:
                sub_46D960();
                break;
            case 5:
                sb_0x46aa8c();
                break;
            case 6:
                sb_0x46cdf8();
                break;
            default:
                break;
            }
            sub_46D960();
        }
        else if ( r.code == 1351 ) // Cancel
        {
            if ( confirmMode == 3 || confirmMode == 6 )
                diskScreenMode = 0;

            sub_46D960();
        }
    }

    if ( confirmMode )
    {
        if ( Input->HotKeyID == 24 )
        {
            if ( confirmMode == 3 || confirmMode == 6 )
                diskScreenMode = 0;
            sub_46D960();
        }
        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
            printf("Line = %d\n", __LINE__);
            switch ( confirmMode )
            {
            case 1:
            {
                sgmSaveExist = 0;
                p_YW->_isNetGame = false;
                sub_bar_button->HideScreen();
                envAction.action = EnvAction::ACTION_LOAD;
                envAction.params[0] = 0;
            }
            break;
            case 2:
                sub_46DC1C();
                break;
            case 3:
                sb_0x46ca74();
                break;
            case 4:
                sub_46D960();
                break;
            case 5:
                sb_0x46aa8c();
                break;
            case 6:
                sb_0x46cdf8();
                break;
            default:
                break;
            }
            sub_46D960();
        }
    }

    if ( v3 )
    {
        Input->ClickInf.flag = 0;
        Input->KbdLastHit = Input::KC_NONE;
        Input->KbdLastDown = Input::KC_NONE;
        Input->chr = 0;
        Input->HotKeyID = -1;
    }

    if ( EnvMode == ENVMODE_TITLE && Input->HotKeyID == 43 )
        p_YW->_helpURL = Locale::Text::Help(Locale::HELP_MAIN);

    r = titel_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SAVE_LOAD_UP )
        {
            ShowSaveLoadMenu();
            diskEnterFromMapSelect = false;
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_OPTIONS_UP )
        {
            ShowOptionsMenu();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_INPUT_SETTINGS_UP )
        {
            ShowInputSettings();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_LANGUAGE_UP )
        {
            ShowLanguageMenu();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_QUIT_UP )
        {
            titel_button->HideScreen();

            envAction.action = EnvAction::ACTION_QUIT;
            SFXEngine::SFXe.startSound(&samples1_info, 4);
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_MULTIPLAYER_UP )
        {
            GameShellUiOpenNetwork();
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_SINGLE_PLAYER_UP )
        {
            sub_bar_button->ShowScreen();

            titel_button->HideScreen();

            EnvMode = ENVMODE_SINGLEPLAY;
        }
        else if ( r.code == UIWidgets::MAIN_MENU_EVENT_IDS::BTN_HELP_UP )
        {
            ShowDatabaseMenu();
        }
    }

    // OpenUA: Database screen event processing
    if ( EnvMode == ENVMODE_DATABASE && database_button )
    {
        NC_STACK_button::ResCode dbr = database_button->ProcessWidgetsEvents(Input);
        if ( dbr )
        {
            if ( dbr.code == UIWidgets::DB_UP_UNITS )
            {
                db_tab = 0; db_page = 0; db_selected = 0;
                PopulateDatabasePage();
            }
            else if ( dbr.code == UIWidgets::DB_UP_WEAPONS )
            {
                db_tab = 1; db_page = 0; db_selected = 0;
                PopulateDatabasePage();
            }
            else if ( dbr.code == UIWidgets::DB_UP_BUILDINGS )
            {
                db_tab = 2; db_page = 0; db_selected = 0;
                PopulateDatabasePage();
            }
            else if ( dbr.code == UIWidgets::DB_UP_PREV )
            {
                db_page--; db_selected = 0;
                PopulateDatabasePage();
            }
            else if ( dbr.code == UIWidgets::DB_UP_NEXT )
            {
                db_page++; db_selected = 0;
                PopulateDatabasePage();
            }
            else if ( dbr.code == UIWidgets::DB_UP_BACK )
            {
                EnvMode = ENVMODE_TITLE;
                database_button->HideScreen();
                titel_button->ShowScreen();
            }
            else if ( dbr.code >= UIWidgets::DB_UP_LINE_BASE &&
                      dbr.code <  UIWidgets::DB_UP_LINE_BASE + 22 )
            {
                db_selected = dbr.code - UIWidgets::DB_UP_LINE_BASE;
                PopulateDatabasePage();
            }
        }

        // Keyboard Up/Down row navigation
        if ( Input->KbdLastHit == Input::KC_UP )
        {
            if ( db_selected > 0 ) { db_selected--; PopulateDatabasePage(); }
        }
        else if ( Input->KbdLastHit == Input::KC_DOWN )
        {
            db_selected++;   // PopulateDatabasePage clamps to valid entries
            PopulateDatabasePage();
        }
    }

    if ( (EnvMode == ENVMODE_SINGLEPLAY || EnvMode == ENVMODE_TUTORIAL) && Input->HotKeyID == 24 )
    {
        if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
        {
            sub_4EDCD8(p_YW);
            if ( p_YW->_preferences & World::PREF_CDMUSICDISABLE )
            {
                SFXEngine::SFXe.StopMusicTrack();
                if ( shelltrack )
                {
                    SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                    SFXEngine::SFXe.PlayMusicTrack();
                }
            }
            if ( returnToTitle )
                yw_returnToTitle();
        }
        else
        {
            sub_bar_button->HideScreen();

            titel_button->ShowScreen();

            EnvMode = ENVMODE_TITLE;
        }
    }

    r = sub_bar_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);
        switch ( r.code )
        {
        case 1013:
            if ( p_YW->_levelInfo.State != TLevelInfo::STATE_MENU )
            {
                sub_4EDCD8(p_YW);
                if ( p_YW->_preferences & World::PREF_CDMUSICDISABLE )
                {
                    SFXEngine::SFXe.StopMusicTrack();
                    if ( shelltrack )
                    {
                        SFXEngine::SFXe.SetMusicTrack(shelltrack, shelltrack__adv.min_delay, shelltrack__adv.max_delay);
                        SFXEngine::SFXe.PlayMusicTrack();
                    }
                }
                if ( returnToTitle )
                    yw_returnToTitle();
            }
            else
            {
                sub_bar_button->HideScreen();

                titel_button->ShowScreen();

                EnvMode = ENVMODE_TITLE;
            }
            break;

        case 1019:
        {
            sgmSaveExist = 0;
            p_YW->_isNetGame = false;

            sub_bar_button->HideScreen();

            ypaworld_func158__sub0__sub9(p_YW);
        }
        break;

        case 1021:
            if ( ypaworld_func158__sub0__sub7() )
            {
                ShowConfirmDialog(1, Locale::Text::Advanced(Locale::ADV_WANTTOLOAD)
                            , Locale::Text::Advanced(Locale::ADV_DISCARDPROGRESS), 0);
            }
            else
            {
                ShowConfirmDialog(1, Locale::Text::Advanced(Locale::ADV_RLYLOAD)
                            , Locale::Text::Advanced(Locale::ADV_2440), 0);
            }
            break;

        case 1016:
            ShowInputSettings2(p_YW);
            break;

        case 1020:
            ShowInputSettings1(p_YW);
            break;

        case 1027:
            p_YW->BriefingCyclePlayAsOwner();
            sub_bar_button->SetText(1027, p_YW->BriefingPlayAsButtonText());
            break;

        case 1026:
            ShowSaveLoadMenu();
            diskEnterFromMapSelect = true;
            break;

        default:
            break;
        }
    }

    if ( EnvMode == ENVMODE_INPUT )
    {
        if ( !keyCatchMode && Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_INPUTCONF);

        if ( Input->KbdLastHit != Input::KC_NONE )
        {
            if ( keyCatchMode )
            {
                input_listview.listFlags &= ~GuiList::GLIST_FLAG_KEYB_INPUT;

                if ( !Input::Engine.KeyTitle.at( Input->KbdLastHit ).empty() )
                {
                    const int16_t capturedKey = Input->KbdLastHit;
                    const bool positiveSlot = confFirstKey;

                    if ( InputKeyUsesOtherSlot(this, inpListActiveElement, positiveSlot, capturedKey) )
                    {
                        pendingInputTarget = inpListActiveElement;
                        pendingInputKeyCode = capturedKey;
                        pendingInputPositiveSlot = positiveSlot;

                        const std::string conflictNames =
                            InputKeyConflictNames(this, inpListActiveElement, positiveSlot, capturedKey);
                        ShowMenuMsgBox(
                            MENU_MSGBOX_INPUT_KEY_CONFLICT,
                            fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_KEY_CONFLICT_FORMAT),
                                         Input::Engine.KeyTitle.at(capturedKey), conflictNames),
                            Locale::Text::OpenUA(Locale::OUA_KEY_CONFLICT_REASSIGN),
                            false);
                    }
                    else
                    {
                        ApplyCapturedInputKey(this, inpListActiveElement, positiveSlot, capturedKey);
                    }
                }
                Input->KbdLastHit = Input::KC_NONE;
            }
            else
            {
                input_listview.listFlags |= GuiList::GLIST_FLAG_KEYB_INPUT;

                if ( Input->KbdLastHit == Input::KC_BACKSPACE  || Input->KbdLastHit == Input::KC_DELETE)
                {
                    if (InputConfig[inpListActiveElement].Type != World::INPUT_BIND_TYPE_SLIDER)
                        InputConfig[inpListActiveElement].PKeyCode = 0;
                }
                else if ( Input->KbdLastHit == Input::KC_RETURN )
                {
                                printf("Line = %d\n", __LINE__);

                    InputConfig[inpListActiveElement].SetFlags = (TInputConf::IF_FIRST | TInputConf::IF_SECOND);
                    keyCatchMode = true;
                    if ( InputConfig[inpListActiveElement].Type == World::INPUT_BIND_TYPE_SLIDER )
                        confFirstKey = false;
                }
                else if ( Input->KbdLastHit == Input::KC_ESCAPE )
                {
                    InputPageCancel();
                }
            }
        }
    }

    if ( InputConfig[inpListActiveElement].Type == World::INPUT_BIND_TYPE_SLIDER )
    {
        v410.field_4 = 0;
        v410.butID = 1056;
        button_input_button->Disable(&v410);
    }
    else
    {
        v410.field_4 = 0;
        v410.butID = 1056;
        button_input_button->Enable(&v410);
    }

    r = button_input_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if (r.code == 1050)
        {
            confJoystickEnabled = true;
            inputChangedParts |= ICHG_JOYSTICK;
        }
        else if (r.code == 1051)
        {
            confJoystickEnabled = false;
            inputChangedParts |= ICHG_JOYSTICK;
        }
        else if (r.code == 1052) // on OK press
        {
            if ( inputChangedParts & ICHG_JOYSTICK )
            {
                joystickEnabled = confJoystickEnabled;
                if ( confJoystickEnabled )
                    p_YW->_preferences &= ~World::PREF_JOYDISABLE;
                else
                    p_YW->_preferences |= World::PREF_JOYDISABLE;
            }

            if ( inputChangedParts & ICHG_ALTJOYSTICK )
            {
                altJoystickEnabled = confAltJoystickEnabled;
                if ( confAltJoystickEnabled )
                    p_YW->_preferences |= World::PREF_ALTJOYSTICK;
                else
                    p_YW->_preferences &= ~World::PREF_ALTJOYSTICK;
            }

            if ( inputChangedParts & ICHG_FORCEFEEDBACK )
            {
                if ( confFFEnabled )
                    p_YW->_preferences &= ~World::PREF_FFDISABLE;
                else
                    p_YW->_preferences |= World::PREF_FFDISABLE;
            }

            inputChangedParts = 0;
            sub_46D2B4();
            InputConfCopyToBackup();
            if ( !UserName.empty() )
                SaveSettings();

            button_input_button->HideScreen();

            titel_button->ShowScreen();

            p_YW->GuiWinClose( &input_listview );
            EnvMode = ENVMODE_TITLE;
        }
        else if (r.code == 1053)
        {
            ShowMenuMsgBox(MENU_MSGBOX_RESTORE_DEFAULT_KEYS,
                           Locale::Text::OpenUA(Locale::OUA_RESTORE_DEFAULT_KEYS_TITLE),
                           Locale::Text::OpenUA(Locale::OUA_RESTORE_DEFAULT_KEYS_TEXT), false);
        }
        else if (r.code == 1054)
        {
            InputPageCancel();
        }
        else if ( r.code == 1055 )
        {
            confFFEnabled = false;
            inputChangedParts |= ICHG_FORCEFEEDBACK;
        }
        else if ( r.code == 1056 )
        {
            confFFEnabled = true;
            inputChangedParts |= ICHG_FORCEFEEDBACK;
        }
        else if ( r.code == 1057 )
        {
            InputConfig[ inpListActiveElement ].PKeyCode = 0;
        }
        else if ( r.code == 1058 )
        {
            confAltJoystickEnabled = true;
            inputChangedParts |= ICHG_ALTJOYSTICK;
        }
        else if ( r.code == 1059 )
        {
            confAltJoystickEnabled = false;
            inputChangedParts |= ICHG_ALTJOYSTICK;
        }
        else if ( r.code == 1250 )
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_INPUTCONF);
            keyCatchMode = false;
        }
    }

    if ( EnvMode == ENVMODE_INPUT )
    {
        input_listview.InputHandle(p_YW, Input);

        inpListActiveElement = InputBindingFromDisplayIndex(input_listview.selectedEntry);
        if ( inpListActiveElement <= 0 )
            inpListActiveElement = World::INPUT_BIND_PAUSE;

        if ( input_listview.listFlags & GuiList::GLIST_FLAG_IN_SELECT )
        {
            InputConfig[ inpListActiveElement ].SetFlags = 0;
            confFirstKey = true;
            keyCatchMode = false;
        }
        input_listview.Formate(p_YW);
    }

    if ( EnvMode == ENVMODE_SETTINGS && atmospherePageActive )
    {
        if (Input->KbdLastHit == Input::KC_ESCAPE)
        {
            AtmosphereOptionsCancel();
        }
        else
        {
            NC_STACK_button::ResCode atmosphereResult = atmosphere_button->ProcessWidgetsEvents(Input);
            if (atmosphereResult)
            {
                if (atmosphereResult.btn)
                    ShowToolTip(atmosphereResult.btn);

                if (atmosphereResult.code == 1450)
                    AtmosphereOptionsSave();
                else if (atmosphereResult.code == 1451)
                    AtmosphereOptionsReset();
                else if (atmosphereResult.code == 1452)
                    AtmosphereOptionsCancel();
                else if (atmosphereResult.code == 1136)
                    CyclePaletteTheme();
            }

            if (atmospherePageActive)
                AtmosphereOptionsApplyLive();
        }
    }
    else if ( EnvMode == ENVMODE_SETTINGS )
    {
        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
                        printf("Line = %d\n", __LINE__);

            if ( video_listvw.IsClosed() && d3d_listvw.IsClosed() )
            {
                if ( _settingsChangeOptions & 1 && _gfxMode != p_YW->_gfxMode && _gfxMode )
                {
                    ShowConfirmDialog(5, Locale::Text::Dialogs(Locale::DLG_S_RESCHANGE)
                                , Locale::Text::Dialogs(Locale::DLG_S_RESWARN), 0);
                }
                else
                {
                    sb_0x46aa8c();
                }
            }

            if ( video_listvw.IsOpen() )
            {
                p_YW->GuiWinClose( &video_listvw );

                if ( video_listvw.IsClosed() )
                {
                    v408.butID = 1156;
                    v408.field_4 = 2;

                    video_button->SetState(&v408);
                }

                _settingsChangeOptions |= 1;
                sub_46C5F0();
            }

        }
        else if ( Input->KbdLastHit == Input::KC_ESCAPE )
        {
            sub_46A3C0();
            EnvMode = ENVMODE_TITLE;
        }
        if ( Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SETTINGS);
    }


    r = video_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1100 )
        {
            p_YW->GuiWinOpen( &video_listvw );
            SFXEngine::SFXe.startSound(&samples1_info, 7);

            Input->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
        }
        else if ( r.code == 1101 )
        {
            p_YW->GuiWinClose( &video_listvw );
        }
        else if ( r.code == 1102 )
        {
            _settingsChangeOptions |= 0x10;
            confGFXFlags |= World::GFX_FLAG_FARVIEW;
        }
        else if ( r.code == 1103 )
        {
            confGFXFlags &= ~World::GFX_FLAG_FARVIEW;
            _settingsChangeOptions |= 0x10;
        }
        else if ( r.code == 1106 )
        {
            _settingsChangeOptions |= 8;
            confGFXFlags |= World::GFX_FLAG_SKYRENDER;
        }
        else if ( r.code == 1107 )
        {
            confGFXFlags &= ~World::GFX_FLAG_SKYRENDER;
            _settingsChangeOptions |= 8;
        }
        else if ( r.code == 1108 )
        {
            _settingsChangeOptions |= 0x40;
        }
        else if ( r.code == 1111 )
        {
            _settingsChangeOptions |= 2;
            confSoundFlags &= ~World::SF_INVERTLR;
        }
        else if ( r.code == 1112 )
        {
            confSoundFlags |= World::SF_INVERTLR;
            _settingsChangeOptions |= 2;
        }
        else if ( r.code == 1113 )
        {
            confGFXFlags |= World::GFX_FLAG_16BITTEXTURE;
            _settingsChangeOptions |= 4;
        }
        else if ( r.code == 1114 )
        {
            confGFXFlags &= ~World::GFX_FLAG_16BITTEXTURE;
            _settingsChangeOptions |= 4;
        }
        else if ( r.code == 1115 )
        {
            SFXEngine::SFXe.startSound(&samples1_info, 0);
            _settingsChangeOptions |= 0x80;
        }
        else if ( r.code == 1117 )
        {
            SFXEngine::SFXe.sub_424000(&samples1_info, 0);
        }
        else if ( r.code == 1118 )
        {
            _settingsChangeOptions |= 0x100;
        }
        else if ( r.code == 1141 )
        {
            _settingsChangeOptions |= SETTINGS_CHANGE_AMBIENT_VOLUME;
        }
        // OpenUA: modern graphics options
        else if ( r.code == 1320 ) // Atmosphere & Visibility page
        {
            ShowAtmosphereOptionsMenu();
        }
        else if ( r.code == 1306 ) // Blending cycle
        {
            if ( !(_settingsChangeOptions & SETTINGS_CHANGE_BLENDING) )
                ShowMenuMsgBox(0, Locale::Text::OpenUA(Locale::OUA_RESTART_REQUIRED_TITLE), Locale::Text::OpenUA(Locale::OUA_RESTART_REQUIRED_TEXT), true);
            confBlending = CycleBlending(confBlending);
            video_button->SetText(1183, BlendingLabel(confBlending));
            _settingsChangeOptions |= SETTINGS_CHANGE_BLENDING;
        }
        else if ( r.code == 1312 ) // FPS Limit cycle
        {
            confMaxFps = CycleFrameRateLimit(confMaxFps);
            video_button->SetText(1187, std::to_string(confMaxFps));
            _settingsChangeOptions |= SETTINGS_CHANGE_MAXFPS;
        }
        else if ( r.code == 1307 ) // Intro Movies checkbox (checked)
        {
            confMoviePlayer = true;
            _settingsChangeOptions |= SETTINGS_CHANGE_MOVIE_PLAYER;
        }
        else if ( r.code == 1308 ) // Intro Movies checkbox (unchecked)
        {
            confMoviePlayer = false;
            _settingsChangeOptions |= SETTINGS_CHANGE_MOVIE_PLAYER;
        }
        else if ( r.code == 1311 ) // Menu Font cycle
        {
            bool alreadyWarnedForMenuFont = (_settingsChangeOptions & SETTINGS_CHANGE_MENU_FONT) != 0;
            std::string oldMenuFont = confMenuFont;
            CycleMenuFont();
            if ( (_settingsChangeOptions & SETTINGS_CHANGE_MENU_FONT) &&
                 !alreadyWarnedForMenuFont &&
                 StriCmp(oldMenuFont, confMenuFont) )
                ShowMenuMsgBox(0, Locale::Text::OpenUA(Locale::OUA_RESTART_REQUIRED_TITLE), Locale::Text::OpenUA(Locale::OUA_RESTART_REQUIRED_TEXT), true);
        }
        else if ( r.code == 1313 ) // Default View cycle
        {
            confDefaultCockpitCamera = !confDefaultCockpitCamera;
            video_button->SetText(1188, DefaultCameraViewLabel(confDefaultCockpitCamera));
            _settingsChangeOptions |= SETTINGS_CHANGE_DEFAULT_CAMERA_VIEW;
        }
        else if ( r.code == 1314 ) // Retro Interface checkbox (checked)
        {
            confInterfaceStyle = GFX::VirtualUIStyle::RETRO;
            _settingsChangeOptions |= SETTINGS_CHANGE_INTERFACE_STYLE;
        }
        else if ( r.code == 1315 ) // Retro Interface checkbox (unchecked)
        {
            confInterfaceStyle = GFX::VirtualUIStyle::SMOOTH;
            _settingsChangeOptions |= SETTINGS_CHANGE_INTERFACE_STYLE;
        }
        else if ( r.code == 1124 )
        {
            if ( (_settingsChangeOptions & 1) &&  _gfxMode != p_YW->_gfxMode && _gfxMode )
            {
                ShowConfirmDialog(5, Locale::Text::Dialogs(Locale::DLG_S_RESCHANGE)
                            , Locale::Text::Dialogs(Locale::DLG_S_RESWARN), 0);
            }
            else
            {
                sb_0x46aa8c();
            }
        }
        else if ( r.code == 1125 ) // Options CANCEL
        {
            sub_46A3C0();
        }
        else if ( r.code == 1126 )
        {
            confEnemyIndicator = true;
            _settingsChangeOptions |= 0x20;
        }
        else if ( r.code == 1127 )
        {
            confEnemyIndicator = false;
            _settingsChangeOptions |= 0x20;
        }
        else if ( r.code == 1128 )
        {
            _settingsChangeOptions |= 0x200;
            confSoundFlags |= World::SF_CDSOUND;
        }
        else if ( r.code == 1129 )
        {
            confSoundFlags &= ~World::SF_CDSOUND;
            _settingsChangeOptions |= 0x200;
        }
        else if ( r.code == 1130 )
        {
            _settingsChangeOptions |= 0x400;
            confGFXFlags |= World::GFX_FLAG_WINDOWED;
        }
        else if ( r.code == 1131 )
        {
            confGFXFlags &= ~World::GFX_FLAG_WINDOWED;
            _settingsChangeOptions |= 0x400;
        }
        else if ( r.code == 1132 )
        {
            _settingsChangeOptions |= 0x800;
            confGFXFlags |= World::GFX_FLAG_SOFTMOUSE;
        }
        else if ( r.code == 1133 )
        {
            confGFXFlags &= ~World::GFX_FLAG_SOFTMOUSE;
            _settingsChangeOptions |= 0x800;
        }
        else if ( r.code == 1134 )
        {
            p_YW->GuiWinOpen( &d3d_listvw );
            SFXEngine::SFXe.startSound(&samples1_info, 7);

            Input->ClickInf.flag &= ~TClickBoxInf::FLAG_LM_DOWN;
        }
        else if ( r.code == 1135 )
        {
            p_YW->GuiWinClose( &d3d_listvw );
        }
        else if ( r.code == 1136 )
        {
            CyclePaletteTheme();
        }
        else if ( r.code == 1137 )
        {
            confPlayerRoboAIBehavior = true;
            _settingsChangeOptions |= SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR;
        }
        else if ( r.code == 1138 )
        {
            confPlayerRoboAIBehavior = false;
            _settingsChangeOptions |= SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR;
        }
        else if ( r.code == 1139 )
        {
            confSpectatorMode = true;
            _settingsChangeOptions |= SETTINGS_CHANGE_SPECTATOR_MODE;
        }
        else if ( r.code == 1140 )
        {
            confSpectatorMode = false;
            _settingsChangeOptions |= SETTINGS_CHANGE_SPECTATOR_MODE;
        }
        else if ( r.code == 1316 )
        {
            confPlayAsOtherFactions = true;
            _settingsChangeOptions |= SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS;
        }
        else if ( r.code == 1317 )
        {
            confPlayAsOtherFactions = false;
            _settingsChangeOptions |= SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS;
        }
        else if ( r.code == 1250 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SETTINGS);
    }

    if ( EnvMode == ENVMODE_SETTINGS && video_listvw.IsOpen() )
    {
        video_listvw.InputHandle(p_YW, Input);

        if ( video_listvw.listFlags & GuiList::GLIST_FLAG_SEL_DONE )
        {
            _settingsChangeOptions |= 1;

            if (!remoteMode)
                sub_46C5F0();
        }

        if ( video_listvw.IsClosed() )
        {
            v408.field_4 = 2;
            v408.butID = 1156;

            video_button->SetState(&v408);
        }

        video_listvw.Formate(p_YW);
    }

    if ( EnvMode == ENVMODE_SETTINGS && d3d_listvw.IsOpen() )
    {
        d3d_listvw.InputHandle(p_YW, Input);

        if ( d3d_listvw.listFlags & GuiList::GLIST_FLAG_SEL_DONE )
        {
            _settingsChangeOptions |= 0x1000;

            if (!remoteMode)
                UpdateSelected3DDevFromList();
        }

        if ( d3d_listvw.IsClosed() )
        {
            v408.field_4 = 2;
            v408.butID = 1172;

            video_button->SetState(&v408);
        }

        d3d_listvw.Formate(p_YW);
    }

    NC_STACK_button::Slider *v67 = video_button->GetSliderData(1159);

    video_button->SetText(1158, fmt::sprintf("%d", v67->value));
    confFxNumber = v67->value;

    v67 = video_button->GetSliderData(1152);

    video_button->SetText(1153, fmt::sprintf("%d", v67->value));
    confSoundVolume = v67->value;

    SFXEngine::SFXe.setMasterVolume(confSoundVolume);

    v67 = video_button->GetSliderData(1154);

    video_button->SetText(1155, fmt::sprintf("%d", v67->value));
    confMusicVolume = v67->value;

    SFXEngine::SFXe.SetMusicVolume(confMusicVolume);

    v67 = video_button->GetSliderData(1191);
    if ( v67 )
    {
        video_button->SetText(1192, fmt::sprintf("%d", v67->value));
        if ( confAmbientSoundVolume != v67->value )
            _settingsChangeOptions |= SETTINGS_CHANGE_AMBIENT_VOLUME;
        confAmbientSoundVolume = v67->value;
    }


    if ( EnvMode == ENVMODE_SELPLAYER ) //Load/Save
    {
        if ( Input->KbdLastHit != Input::KC_NONE || Input->chr )
        {
            if ( diskScreenMode )
            {
                if ( Input->KbdLastHit == Input::KC_BACKSPACE )
                {
                    if ( userNameDirCursor > 0 )
                    {
                        userNameDir.erase( userNameDirCursor - 1, 1 );
                        userNameDirCursor--;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_RETURN )
                {
                                printf("Line = %d\n", __LINE__);

                    switch ( diskScreenMode )
                    {
                    case 1:
                        if ( diskListActiveElement )
                        {
                            ShowConfirmDialog(3, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                        , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                        }
                        else if (userNameDir.size() > 0)
                        {
                            sb_0x46ca74();
                        }
                        break;

                    case 2:
                        sub_46C914();
                        break;

                    case 3:
                        if ( diskListActiveElement )
                        {
                            ShowConfirmDialog(6, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                        , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                        }
                        else if (userNameDir.size() > 0)
                        {
                            sb_0x46cdf8();
                        }
                        break;

                    case 4:
                        sub_46C748();
                        break;

                    default:
                        break;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_ESCAPE )
                {
                    diskScreenMode = 0;
                }
                else if ( Input->KbdLastHit == Input::KC_LEFT )
                {
                    if ( userNameDirCursor > 0 )
                        userNameDirCursor--;
                }
                else if ( Input->KbdLastHit == Input::KC_RIGHT )
                {
                    if ( userNameDirCursor < (int)userNameDir.size() )
                        userNameDirCursor++;
                }
                else if ( Input->KbdLastHit == Input::KC_DELETE )
                {
                    if ( userNameDirCursor < (int)userNameDir.size() )
                        userNameDir.erase(userNameDirCursor, 1);
                }

                if ( userNameDir.size() < 32 )
                {
                    if ( Input->chr >= ' ' )
                    {
                        if ( ypaworld_func158__sub0__sub6(Input->chr) )
                        {
                            userNameDir.insert(userNameDirCursor, 1, Input->chr);
                            userNameDirCursor++;
                        }
                    }
                }
            }
            else
            {
                if ( Input->KbdLastHit == Input::KC_ESCAPE )
                    sub_46A7F8();

                if ( Input->HotKeyID == 43 )
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SAVEGAME);

            }

            if ( diskListActiveElement )
                disk_listvw.PosOnSelected(diskListActiveElement - 1);
        }
    }


    diskListActiveElement = 0;
    int v108 = 1;

    for ( ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++)
    {
        if ( !StriCmp(it->name, userNameDir) )
        {
            diskListActiveElement = v108;
            break;
        }
        v108++;
    }

    r = disk_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 103 )
        {
            sub_46A7F8();
        }
        else if ( r.code == 1160 )
        {
            diskScreenMode = 2;

            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }

            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1161 )
        {
            diskScreenMode = 4;
            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }

            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1162 )
        {
            diskScreenMode = 3;

            std::string tmp = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);

            int maxN = 0;

            for (ProfileList::iterator it = profiles.begin(); it != profiles.end(); it++)
            {
                if ( !StriCmp(tmp, it->name.substr(0, tmp.size())) )
                {
                    int n = std::stoi( it->name.substr(tmp.size()) );

                    if ( n > maxN )
                        maxN = n;
                }
            }

            userNameDir = fmt::sprintf("%s%d", tmp, maxN + 1);

            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1163 )
        {
            diskScreenMode = 1;
            if ( !diskListActiveElement )
            {
                userNameDir = Locale::Text::Dialogs(Locale::DLG_P_UNNAMED);
            }

            userNameDirCursor = userNameDir.size();

            disk_button->SetText(1100, userNameDir + 'h');
        }
        else if ( r.code == 1164)
        {
            switch ( diskScreenMode )
            {
            case 1:
                if ( diskListActiveElement )
                {
                    ShowConfirmDialog(3, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                }
                else
                {
                    sb_0x46ca74();
                }
                break;
            case 2:
                sub_46C914();
                break;
            case 3:
                if ( diskListActiveElement )
                {
                    ShowConfirmDialog(6, Locale::Text::Advanced(Locale::ADV_WANTOVERWRITE)
                                , Locale::Text::Advanced(Locale::ADV_EXISTSAVEDGAME), 0);
                }
                else
                {
                    sb_0x46cdf8();
                }
                break;
            case 4:
                sub_46C748();
                break;
            default:
                break;
            }
        }
        else if (r.code == 1165)
        {
            if ( diskScreenMode )
            {
                diskScreenMode = 0;
            }
            else
                sub_46A7F8();
        }
        else if (r.code == 1250)
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_SAVEGAME);
            diskScreenMode = 0;
        }
    }

    if ( EnvMode == ENVMODE_SELPLAYER ) // Multiplayer
    {
        disk_listvw.InputHandle(p_YW, Input);

        if ( disk_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT || Input->KbdLastHit == Input::KC_UP || Input->KbdLastHit == Input::KC_DOWN )
        {
            diskListActiveElement = disk_listvw.selectedEntry + 1;

            if ( diskListActiveElement < 1 )
                diskListActiveElement = 1;

            if ( diskListActiveElement > disk_listvw.numEntries  )
                diskListActiveElement = disk_listvw.numEntries;


            ProfileList::iterator it = profiles.begin();

            for (int i = 0; i < diskListActiveElement - 1; i++) // check field_1612 - 1
            {
                if ( it == profiles.end() )
                {
                    diskListActiveElement = 0;
                    break;
                }

                it++;
            }

            if (it != profiles.end())
            {
                userNameDir = it->name;
                userNameDirCursor = userNameDir.size();
            }
        }
        disk_listvw.Formate(p_YW);
    }

    if ( diskScreenMode )
    {
        v410.butID = 1105;
        disk_button->Enable(&v410);

        v410.field_4 = 0;
        v410.butID = 1104;
        disk_button->Disable(&v410);

        v410.butID = 1101;
        disk_button->Disable(&v410);

        v410.butID = 1102;
        disk_button->Disable(&v410);

        v410.butID = 1103;
        disk_button->Disable(&v410);

        v410.butID = 1100;
        disk_button->Disable(&v410);

        if ( diskScreenMode == 4 )
        {
            v410.field_4 = 0;
            v410.butID = 1105;

            if ( !diskListActiveElement || !StriCmp(userNameDir, UserName) )
                disk_button->Disable(&v410);
            else
                disk_button->Enable(&v410);
        }

        if ( diskScreenMode == 2 && !diskListActiveElement )
        {
            v410.field_4 = 0;
            v410.butID = 1105;
            disk_button->Disable(&v410);
        }

        if ( diskScreenMode == 1 || diskScreenMode == 3 )
        {
            v410.butID = 1100;
            disk_button->Enable(&v410);
        }

        std::string tmp = userNameDir;
        tmp.insert(userNameDirCursor, 1, '_');

        disk_button->SetText(1100, tmp);
    }
    else
    {
        v410.field_4 = 0;
        v410.butID = 1105;
        disk_button->Disable(&v410);

        v410.butID = 1104;
        disk_button->Enable(&v410);

        v410.butID = 1101;
        disk_button->Enable(&v410);

        v410.butID = 1103;
        disk_button->Enable(&v410);

        v410.butID = 1100;
        disk_button->Disable(&v410);

        v410.butID = 1102;
        const bool deleteVisible = StriCmp(userNameDir, UserName) != 0;
        if ( !deleteVisible )
            disk_button->Disable(&v410);
        else
            disk_button->Enable(&v410);

        // Keep the visible row centered: four buttons for UserAll/current
        // profile, five buttons when Delete is actually available.
        LayoutSaveLoadActionButtons(this, deleteVisible);
        disk_button->SetText(1100, userNameDir);
    }

    if ( EnvMode == ENVMODE_SELLOCALE )
    {

        if ( Input->KbdLastHit == Input::KC_RETURN )
        {
                        printf("Line = %d\n", __LINE__);

            sub_46B0E0();
        }
        else if ( Input->KbdLastHit == Input::KC_ESCAPE )
        {
            ExitFromLanguageMenu();
        }

        if ( Input->HotKeyID == 43 )
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_LOCALE);
    }


    r = locale_button->ProcessWidgetsEvents(Input);

    if (r)
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 103 )
        {
            ExitFromLanguageMenu();
        }
        else if ( r.code == 1250 )
        {
            p_YW->_helpURL = Locale::Text::Help(Locale::HELP_LOCALE);
        }
        else if ( r.code == 1300 )
        {
            sub_46B0E0();
        }
        else if ( r.code == 1301 )
        {
            ExitFromLanguageMenu();
        }
    }

    if ( EnvMode == ENVMODE_SELLOCALE ) //Locale
    {
        local_listvw.InputHandle(p_YW, Input);

        if ( local_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT )
        {
            Engine::StringList::iterator it = std::next(lang_dlls.begin(), local_listvw.selectedEntry);

            prev_lang = &(*it);
        }

        local_listvw.Formate(p_YW);
    }

    r = about_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.code == 103 )
        {
            EnvMode = ENVMODE_TITLE;

            about_button->HideScreen();
        }
    }

    if ( EnvMode == ENVMODE_ABOUT )
    {
        if ( Input->KbdLastHit == Input::KC_RETURN || Input->KbdLastHit == Input::KC_ESCAPE )
        {
                        printf("Line = %d\n", __LINE__);

            EnvMode = ENVMODE_TITLE;

            about_button->HideScreen();

            titel_button->ShowScreen();
        }
    }

    if ( EnvMode == ENVMODE_DATABASE && database_button )
    {
        if ( Input->KbdLastHit == Input::KC_ESCAPE )
        {
            EnvMode = ENVMODE_TITLE;
            database_button->HideScreen();
            titel_button->ShowScreen();
        }
    }

    if ( EnvMode == ENVMODE_TITLE )
    {
        if ( aboutDlgKeyCount && GlobalTime - aboutDlgLastKeyTime >= 700 )
        {
            aboutDlgKeyCount = 0;
        }
        else
        {
            switch ( aboutDlgKeyCount )
            {
            case 0:
                if ( p_YW->sub_449678(Input, Input::KC_A) ) // VK_A
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 1:
                if ( p_YW->sub_449678(Input, Input::KC_M) )
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 2:
                if ( p_YW->sub_449678(Input, Input::KC_O) )
                {
                    aboutDlgLastKeyTime = GlobalTime;
                    aboutDlgKeyCount++;
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;

            case 3:
                if ( p_YW->sub_449678(Input, Input::KC_K) )
                {
                    ShowAbout();
                    SFXEngine::SFXe.startSound(&samples1_info, World::SOUND_ID_SECRET);
                }
                else
                {
                    if ( Input->KbdLastHit != Input::KC_NONE )
                        aboutDlgKeyCount = 0;
                }
                break;
            default:
                break;
            }
        }
    }
    else
    {
        aboutDlgKeyCount = 0;
    }

    switch ( netSelMode )
    {
    case NETSCREEN_MODE_SELECT:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_SESSION_SELECT:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_CHOOSE_MAP:
        nInputMode = 0;
        network_listvw.maxShownEntries = 12;
        netListY = 3 * (p_YW->_fontH + vertMenuSpace);
        break;
    case NETSCREEN_ENTER_NAME:
        network_listvw.maxShownEntries = 12;
        nInputMode = 1;
        break;
    case NETSCREEN_ENTER_IP:
        network_listvw.maxShownEntries = 12;
        nInputMode = 1;
        break;
    case NETSCREEN_INSESSION:
        nInputMode = 1;
        network_listvw.maxShownEntries = 6;
        netListY = p_YW->_fontH * 9.5 + 2 * vertMenuSpace;
        break;
    default:
        break;
    }

    uamessage_fraction fracMsg;

    r = network_button->ProcessWidgetsEvents(Input);

    if ( r )
    {
        if ( r.btn )
            ShowToolTip(r.btn);

        if ( r.code == 1204 || r.code == 1205 || r.code == 1206 || r.code == 1207 )
        {
            fracMsg.msgID = UAMSG_FRACTION;
            fracMsg.owner = 0;
        }

        if ( r.code == 103 || r.code == 1202 )
        {
            ExitFromNetworkToMain();
        }
        else if ( r.code == 1204 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_RESIST_BIT;
            SelectedFraction = World::OWNER_RESIST_BIT;
            FreeFraction &= ~World::OWNER_RESIST_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1205 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_GHOR_BIT;
            FreeFraction &= ~World::OWNER_GHOR_BIT;
            SelectedFraction = World::OWNER_GHOR_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1206 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_MYKO_BIT;
            SelectedFraction = World::OWNER_MYKO_BIT;
            FreeFraction &= ~World::OWNER_MYKO_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }
        else if ( r.code == 1207 )
        {
            fracMsg.freefrac = SelectedFraction;
            FreeFraction |= SelectedFraction;
            fracMsg.newfrac = World::OWNER_TAER_BIT;
            SelectedFraction = World::OWNER_TAER_BIT;
            FreeFraction &= ~World::OWNER_TAER_BIT;

            p_YW->NetBroadcastMessage(&fracMsg, sizeof(fracMsg), true);
        }

        switch ( netSelMode )
        {
        case NETSCREEN_MODE_SELECT:
            if ( r.code == 1200 )
            {
                yw_NetOKProvider();
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELPROV);
            }
            break;

        case NETSCREEN_SESSION_SELECT:
            if ( r.code == 1200 )
            {
                yw_JoinNetGame();
            }
            else if ( r.code == 1201 )
            {
                isHost = true;
                netSel = -1;
                network_listvw.firstShownEntries = 0;
                netSelMode = NETSCREEN_CHOOSE_MAP;
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELSESS);
            }
            break;

        case NETSCREEN_ENTER_IP:
            if ( r.code == 1200 ){
                if ( !netName.empty() )
                {
                    std::string ip = netName;
                    netName = "";
                    ConnectToServer(ip);
                }
                else
                {
                    netName = "127.0.0.1";
                    netNameCurPos = netName.size();
                }
            }
            break;

        case NETSCREEN_ENTER_NAME:
            if ( r.code == 1200 )
            {
                if ( !netName.empty() )
                {
                    netPlayerName = netName;
                    netName = "";
                }
                p_YW->_netDriver->SetWantedName(netPlayerName);
                switch ( p_YW->_netDriver->GetMode() )
                {
                    case 1:
                        isHost = true;
                        netSel = -1;
                        network_listvw.firstShownEntries = 0;
                        netSelMode = NETSCREEN_CHOOSE_MAP;
                        p_YW->GuiWinOpen( &network_listvw );
                        break;

                    case 2:
                    {
                        netSelMode = NETSCREEN_ENTER_IP;
                        netName = "";
                        netNameCurPos = 0;
                    }
                        break;

                    default:
                        break;
                }
            }
            else if ( r.code == 1201 )
            {
//                if ( str17_NOT_FALSE )
//                {
//                    windd_dlgBox v339;
//                    memset(&v339, 0, sizeof(windd_dlgBox));
//
//                    v339.title = get_lang_string(413, "ENTER CALLSIGN");
//                    v339.ok = get_lang_string(2, "OK");
//                    v339.cancel = get_lang_string(3, "CANCEL");
//                    v339.maxLen = 32;
//                    v339.timer_func = NULL;
//                    v339.startText = netName;
//
//                    windd->windd_func322(&v339);
//
//                    if ( v339.result )
//                    {
//                        strncpy(netName, v339.result, 64);
//                        netName[63] = 0;
//                    }
//                }
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETNAME);
            }
            break;

        case NETSCREEN_CHOOSE_MAP:
            if ( r.code == 1200 )
            {
                AfterMapChoose();
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELLVL);
            }
            break;

        case NETSCREEN_INSESSION:
            if ( r.code == 1200 )
            {
                if ( isHost )
                {
                    std::string v425;
                    std::string v425_1;

                    if ( p_YW->_netDriver->GetPlayerCount() <= 1 )
                    {
                        ShowConfirmDialog(2, Locale::Text::Advanced(Locale::ADV_WANTSTART)
                                    , Locale::Text::Advanced(Locale::ADV_NETALONE), 0);
                    }
                    else
                    {
                        sub_46DC1C();
                    }
                }
            }
            else if ( r.code == 1203 )
            {
                if ( isHost )
                {
                    netSel = -1;
                    network_listvw.firstShownEntries = 0;
                    msgBuffers.clear();
                    lastSender.clear();
                    netName = "";
                    netSelMode = NETSCREEN_CHOOSE_MAP;
                }
            }
            else if ( r.code == 1208 )
            {
                uamessage_ready rdyMsg;

                rdyStart = true;

                int myIndex = p_YW->_netDriver->GetMyIndex();

                if (myIndex >= 0)
                    lobbyPlayers[myIndex].Ready = true;

                rdyMsg.msgID = UAMSG_READY;
                rdyMsg.owner = 0;
                rdyMsg.rdy = 1;

                p_YW->NetBroadcastMessage(&rdyMsg, sizeof(rdyMsg), true);

                p_YW->_netDriver->FlushBroadcastBuffer();
            }
            else if ( r.code == 1209 )
            {
                uamessage_ready rdyMsg;

                rdyStart = false;

                int myIndex = p_YW->_netDriver->GetMyIndex();

                if (myIndex >= 0)
                    lobbyPlayers[myIndex].Ready = false;

                rdyMsg.msgID = UAMSG_READY;
                rdyMsg.owner = 0;
                rdyMsg.rdy = 0;

                p_YW->NetBroadcastMessage(&rdyMsg, sizeof(rdyMsg), true);

                p_YW->_netDriver->FlushBroadcastBuffer();
            }
            else if ( r.code == 1210 )
            {
//                if ( str17_NOT_FALSE )
//                {
//                    windd_dlgBox v316;
//                    memset(&v316, 0, sizeof(windd_dlgBox));
//
//                    v316.title = get_lang_string(422, "ENTER MESSAGE");
//                    v316.ok = get_lang_string(2, "OK");
//                    v316.cancel = get_lang_string(3, "CANCEL");
//                    v316.startText = netName;
//                    v316.timer_func = NULL;
//                    v316.maxLen = 64;
//
//                    windd->windd_func322(&v316);
//
//                    if ( v316.result )
//                    {
//                        strncpy(netName, v316.result, 64);
//                        netName[63] = 0;
//                    }
//                }

                if ( !netName.empty() )
                {
                    uamessage_message msgMsg;
                    msgMsg.msgID = UAMSG_MESSAGE;
                    msgMsg.owner = 0;

                    strncpy(msgMsg.message, netName.c_str(), 64);

                    p_YW->NetBroadcastMessage(&msgMsg, sizeof(msgMsg), true);

                    sub_4D0C24(p_YW, netPlayerName, msgMsg.message);

                    netName = "";
                    netNameCurPos = 0;

                    int v223 = strtol(msgMsg.message, NULL, 0);
                    if ( v223 > 0 )
                        sub_4D9550(p_YW, v223);
                }
            }
            else if ( r.code == 1250 )
            {
                p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSTRTSCR);
            }
            break;
        default:
            break;
        }
    }

    if ( EnvMode == ENVMODE_NETPLAY )
    {
        int a4 = network_button->getBTN_y();

        network_listvw.y = netListY + a4;

        network_listvw.InputHandle(p_YW, Input);

        if ( (network_listvw.listFlags & GuiList::GLIST_FLAG_IN_SELECT) || Input->KbdLastHit == Input::KC_UP || Input->KbdLastHit == Input::KC_DOWN )
        {
            netSel = network_listvw.selectedEntry;

            switch ( netSelMode )
            {
            case NETSCREEN_MODE_SELECT:
            case NETSCREEN_SESSION_SELECT:
                nInputMode = 0;
                break;
            case NETSCREEN_CHOOSE_MAP:
            {
                uint32_t playerCount = p_YW->_netDriver->GetPlayerCount();

                int filteredID = 0;

                for (const UserData::TMapDescription &desc : mapDescriptions)
                {
                    if ( playerCount <= 1 || playerCount <= p_YW->_globalMapRegions.MapRegions.at( desc.id ).RoboCount)
                    {
                        if ( filteredID == netSel )
                        {
                            netLevelName = desc.pstring;
                            netLevelID = desc.id;
                            break;
                        }

                        filteredID++;
                    }
                }
            }
            break;

            default:
                break;
            }

            netNameCurPos = netName.size();
        }

        network_listvw.Formate(p_YW);
    }

    if ( EnvMode == ENVMODE_NETPLAY )
    {
        if ( Input->KbdLastHit != Input::KC_NONE || Input->chr || Input->HotKeyID >= 0 )
        {
            if ( nInputMode )
            {

                uint32_t v233;

                if ( netSelMode == NETSCREEN_ENTER_NAME )
                    v233 = 32;
                else
                    v233 = 38;

                if ( netName.size() < v233 )
                {
                    if ( Input->chr > ' ' && Input->chr != '*' )
                    {
                        if (netNameCurPos <= (int)netName.size())
                        {
                            netName.insert(netNameCurPos, 1, Input->chr);
                            netNameCurPos++;
                        }
                    }
                }

                if ( Input->KbdLastHit == Input::KC_BACKSPACE && netNameCurPos > 0 )
                {
                    if (netNameCurPos > 0 && (int)netName.size() >= netNameCurPos)
                    {
                        netName.erase(netNameCurPos - 1, 1);
                        netNameCurPos--;
                    }
                }
                else if ( Input->KbdLastHit == Input::KC_LEFT )
                {
                    if ( netNameCurPos > 0 )
                        netNameCurPos--;
                }
                else if ( Input->KbdLastHit == Input::KC_RIGHT )
                {
                    if ( netNameCurPos < (int)netName.size() )
                        netNameCurPos++;
                }
                else if ( Input->KbdLastHit == Input::KC_DELETE && netNameCurPos < (int)netName.size() )
                {
                    if ( netNameCurPos < (int)netName.size() )
                        netName.erase(netNameCurPos, 1);
                }
            }

            if ( Input->KbdLastHit == Input::KC_RETURN )
            {
                switch ( netSelMode )
                {
                case NETSCREEN_MODE_SELECT:
                    yw_NetOKProvider();
                    break;

                case NETSCREEN_SESSION_SELECT:
                    if ( network_listvw.numEntries )
                    {
                        yw_JoinNetGame();
                    }
                    else
                    {
                        isHost = true;
                        netSel = -1;
                        netSelMode = NETSCREEN_CHOOSE_MAP;
                        network_listvw.firstShownEntries = 0;
                    }
                    break;

                case NETSCREEN_ENTER_NAME:
                    if ( !netName.empty() )
                    {
                        netPlayerName = netName;
                        netName = "";

                        p_YW->_netDriver->SetWantedName(netPlayerName);
                        switch ( p_YW->_netDriver->GetMode() )
                        {
                            case 1:
                                isHost = true;
                                netSel = -1;
                                network_listvw.firstShownEntries = 0;
                                netSelMode = NETSCREEN_CHOOSE_MAP;
                                p_YW->GuiWinOpen( &network_listvw );
                                break;

                            case 2:
                            {
                                netSelMode = NETSCREEN_ENTER_IP;
                                netName = "";
                                netNameCurPos = 0;
                            }
                                break;

                            default:
                                break;
                        }
                    }
                    break;

                case NETSCREEN_ENTER_IP:
                    {
                        if ( !netName.empty() )
                        {
                            std::string ip = netName;
                            netName = "";
                            ConnectToServer(ip);
                        }
                        else
                        {
                            netName = "127.0.0.1";
                            netNameCurPos = netName.size();
                        }
                    }
                    break;

                case NETSCREEN_CHOOSE_MAP:
                    AfterMapChoose();
                    break;
                case NETSCREEN_INSESSION:
                    if ( !netName.empty() )
                    {
                        uamessage_message msgMsg;
                        msgMsg.msgID = UAMSG_MESSAGE;
                        msgMsg.owner = 0;

                        strncpy(msgMsg.message, netName.c_str(), 64);

                        p_YW->NetBroadcastMessage(&msgMsg, sizeof(msgMsg), true);

                        sub_4D0C24(p_YW, netPlayerName, msgMsg.message);
                        netName.clear();
                        netNameCurPos = 0;

                        int v271 = strtol(msgMsg.message, NULL, 0);
                        if ( v271 > 0 )
                            sub_4D9550(p_YW, v271);
                    }
                    break;
                default:
                    break;
                }
            }
            else if ( Input->KbdLastHit == Input::KC_ESCAPE )
            {
                ExitFromNetworkToMain();
            }

            if ( Input->HotKeyID == 43 && !nInputMode )
            {
                switch ( netSelMode )
                {
                case NETSCREEN_MODE_SELECT:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELPROV);
                    break;
                case NETSCREEN_SESSION_SELECT:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELSESS);
                    break;
                case NETSCREEN_ENTER_NAME:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETNAME);
                    break;
                case NETSCREEN_CHOOSE_MAP:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSELLVL);
                    break;
                case NETSCREEN_INSESSION:
                    p_YW->_helpURL = Locale::Text::Help(Locale::HELP_NETSTRTSCR);
                    break;
                default:
                    break;
                }
            }

            if ( netSelMode == NETSCREEN_ENTER_IP && Input->KbdLastHit == Input::KC_V && Input::Engine.GetKeyState(Input::KC_CTRL) )
            {
                char * clpbrd = SDL_GetClipboardText();
                if (clpbrd)
                {
                    IPaddress tmp;
                    if (SDLNet_ResolveHost(&tmp, clpbrd, 0) == 0)
                    {
                        netName = clpbrd;
                        netNameCurPos = netName.size();
                    }
                    SDL_free(clpbrd);
                }
            }

            if ( netSel != -1 ){
                network_listvw.PosOnSelected(netSel);
            }

            Input->KbdLastHit = Input::KC_NONE;
        }
    }

    if ( isHost )
    {
        if ( netSelMode == NETSCREEN_INSESSION && envAction.action != EnvAction::ACTION_NETPLAY )
        {
            if ( p_YW->_netDriver->GetPlayerCount()   <   p_YW->_globalMapRegions.MapRegions[ netLevelID ].RoboCount )
            {
                if ( blocked )
                {
                    int v357 = 0;
                    p_YW->_netDriver->LockSession(&v357);

                    blocked = false;
                }
            }
            else if ( !blocked )
            {
                int v357 = 1;
                p_YW->_netDriver->LockSession(&v357);

                blocked = true;
            }
        }
    }

    v410.butID = 1201;
    network_button->Enable(&v410);

    v410.butID = 1205;
    network_button->Disable(&v410);

    v410.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
    network_button->Disable(&v410);

    v410.butID = 1203;
    network_button->Enable(&v410);

    v410.butID = 1225;
    network_button->Disable(&v410);

    v410.butID = 1226;
    network_button->Disable(&v410);

    v410.butID = 1227;
    network_button->Disable(&v410);

    network_button->SetText(1201, Locale::Text::Common(Locale::CMN_OK));

    v410.butID = 1220;
    network_button->Disable(&v410);

    v410.butID = 1206;
    network_button->Disable(&v410);

    v410.butID = 1207;
    network_button->Disable(&v410);

    v410.butID = 1208;
    network_button->Disable(&v410);

    v410.butID = 1209;
    network_button->Disable(&v410);

    v410.butID = 1219;
    network_button->Disable(&v410);

    v410.butID = 1221;
    network_button->Disable(&v410);

    v410.field_4 = 0;
    v410.butID = 1210;
    network_button->Disable(&v410);

    v410.butID = 1211;
    network_button->Disable(&v410);

    v410.butID = 1212;
    network_button->Disable(&v410);

    v410.butID = 1213;
    network_button->Disable(&v410);

    v410.butID = 1214;
    network_button->Disable(&v410);

    v410.butID = 1215;
    network_button->Disable(&v410);

    v410.butID = 1216;
    network_button->Disable(&v410);

    v410.butID = 1217;
    network_button->Disable(&v410);

    if ( (netSelMode != NETSCREEN_SESSION_SELECT || p_YW->_netDriver->GetProvType() != 3)
            && (netSelMode != NETSCREEN_SESSION_SELECT || modemAskSession != 1 || p_YW->_netDriver->GetProvType() != 4)
            && netSelMode != NETSCREEN_MODE_SELECT )
    {
        v410.butID = 1228;
        network_button->Disable(&v410);
    }
    else
    {
        std::string v280;

        if ( netSelMode != NETSCREEN_MODE_SELECT )
        {
            v280 = Locale::Text::Advanced(Locale::ADV_REFRESHSESS);
        }
        else
        {
            if ( !p_YW->_netTcpAddress.empty() )
            {
                v280 = fmt::sprintf("%s  %s",  Locale::Text::Advanced(Locale::ADV_YOURIP) , p_YW->_netTcpAddress);
            }
            else
                v280 = " ";
        }

        network_button->SetText(1228, v280);

        v410.butID = 1228;
        network_button->Enable(&v410);
    }

    if ( !nInputMode && (netSelMode != NETSCREEN_ENTER_NAME || netSelMode != NETSCREEN_ENTER_IP) )
    {
        v410.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
        network_button->Disable(&v410);
    }
    else
    {
        v410.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;
        network_button->Enable(&v410);

        v393.xpos = -1;
        v393.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX;

        if ( netSelMode == NETSCREEN_ENTER_NAME || netSelMode == NETSCREEN_ENTER_IP)
        {
            v393.width = dword_5A50B6;
            v393.ypos = 3 * (vertMenuSpace + p_YW->_fontH);
        }
        else
        {
            v393.width = dword_5A50B6 * 0.8;
            v393.ypos = 14 * (vertMenuSpace + p_YW->_fontH);
        }

        network_button->setXYWidth(&v393);

        std::string tmp = netName;
        if (tmp.size() >= (size_t)netNameCurPos)
            tmp.insert(netNameCurPos, 1, '_');

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXTBOX, tmp);
    }

    v393.xpos = -1;
    v393.width = -1;
    v393.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;

    if ( netSelMode == 2 )
    {
        v393.ypos = 4 * (vertMenuSpace + p_YW->_fontH);
    }
    else
    {
        v393.ypos = (vertMenuSpace + p_YW->_fontH) * 15.2;
    }

    network_button->setXYWidth(&v393);

    switch ( netSelMode )
    {
    case NETSCREEN_MODE_SELECT:
        v410.field_4 = 0;
        v410.butID = 1205;

        network_button->Disable(&v410);

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELPROV));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT2));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT3));
        break;

    case NETSCREEN_SESSION_SELECT:
    {
        if ( p_YW->_netDriver->GetProvType() != 4 || !modemAskSession )
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION, Locale::Text::Netdlg(Locale::NETDLG_NEW));

            v410.butID = UIWidgets::NETWORK_MENU_WIDGET_IDS::BTN_CREATE_SESSTION;
            network_button->Enable(&v410);
        }

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELSESS));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT5));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT6));

        windp_getNameMsg msg;
        msg.id = 0;

        if ( p_YW->_netDriver->GetSessionName(&msg) )
        {
            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_JOIN));
        }
        else if ( p_YW->_netDriver->GetProvType() != 4 || modemAskSession )
        {
            v410.butID = 1201;
            network_button->Disable(&v410);
        }
        else
        {
            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_CONNECT));
        }
    }
    break;

    case NETSCREEN_ENTER_NAME:
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_ENTERPL));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT11));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT12));
        break;

    case NETSCREEN_ENTER_IP:
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::OpenUA(Locale::OUA_NETWORK_SERVER_ADDRESS));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::OpenUA(Locale::OUA_NETWORK_ENTER_SERVER_IP));
        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, " ");
        break;

    case NETSCREEN_CHOOSE_MAP:
        if ( remoteMode )
        {
            v410.butID = 1205;
            network_button->Disable(&v410);
        }

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_SELLVL));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT8));

        network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT9));
        break;

    case NETSCREEN_INSESSION:
        v410.butID = 1225;
        network_button->Enable(&v410);

        network_button->SetText(1225, Locale::Text::Netdlg(Locale::NETDLG_SEND));

        v410.butID = 1226;
        network_button->Enable(&v410);

        v410.butID = 1227;
        network_button->Enable(&v410);

        if ( netLevelID )
            network_button->SetText(1226, netLevelName);
        else
            network_button->SetText(1226, " ");

        if ( isHost )
        {  // Change map
            v410.butID = 1205;
            //network_button->show(&v410);
        }

        if ( rdyStart )
        {
            if ( !isHost )
            {
                v410.butID = 1205;
                network_button->Disable(&v410);
            }
        }

        v410.butID = 1220;
        network_button->Enable(&v410);

        if ( netLevelID > 0 && netLevelID < 256 )
        {

            if ( !isHost && rdyStart )
            {
                v410.butID = 1220;
                network_button->Disable(&v410);
            }
            else
            {
                v410.butID = 1206;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_RESIST) )
                    network_button->Enable(&v410);
                else
                    network_button->Disable(&v410);

                v410.butID = 1207;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_GHOR) )
                    network_button->Enable(&v410);
                else
                    network_button->Disable(&v410);

                v410.butID = 1208;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_MYKO) )
                    network_button->Enable(&v410);
                else
                    network_button->Disable(&v410);

                v410.butID = 1209;
                if ( p_YW->_globalMapRegions.MapRegions[ netLevelID ].IsFraction(World::OWNER_TAER) )
                    network_button->Enable(&v410);
                else
                    network_button->Disable(&v410);
            }

            v408.butID = 0;

            switch ( SelectedFraction )
            {
            case World::OWNER_RESIST_BIT:
                v408.butID = 1206;
                break;
            case World::OWNER_GHOR_BIT:
                v408.butID = 1207;
                break;
                case World::OWNER_MYKO_BIT:
                v408.butID = 1208;
                break;
            case World::OWNER_TAER_BIT:
                v408.butID = 1209;
                break;

            default:
                break;
            }

            if ( v408.butID )
            {
                v408.field_4 = 1;
                network_button->SetState(&v408);
            }

            int FractionErrorMask = 0;

            netGameCanStart = true;
            isWelcmd = true;

            // First of all if fraction not allowed - fill
            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_RESIST) )
                FractionErrorMask |= World::OWNER_RESIST_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_GHOR) )
                FractionErrorMask |= World::OWNER_GHOR_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_MYKO) )
                FractionErrorMask |= World::OWNER_MYKO_BIT;

            if ( !p_YW->_globalMapRegions.MapRegions[netLevelID].IsFraction(World::OWNER_TAER) )
                FractionErrorMask |= World::OWNER_TAER_BIT;

            int FractionPlID[World::CVFractionsCount];

            for ( TDPPlayerData &p : p_YW->_netDriver->GetPlayersData() )
            {
                int fraction;

                if ( p.IsItMe() )
                    fraction = SelectedFraction;
                else
                    fraction = lobbyPlayers[p.Index].NetFraction;

                if ( fraction & FractionErrorMask )
                {
                    lobbyPlayers[p.Index].IsTrouble = true;
                    netGameCanStart = false;

                    switch ( fraction )
                    {
                    case World::OWNER_RESIST_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_RESIST] ].IsTrouble = true;
                        break;
                    case World::OWNER_GHOR_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_GHOR] ].IsTrouble = true;
                        break;
                    case World::OWNER_MYKO_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_MYKO] ].IsTrouble = true;
                        break;
                    case World::OWNER_TAER_BIT:
                        lobbyPlayers[ FractionPlID[World::OWNER_TAER] ].IsTrouble = true;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    lobbyPlayers[p.Index].IsTrouble = false;

                    switch ( fraction )
                    {
                    case World::OWNER_RESIST_BIT:
                        FractionPlID[World::OWNER_RESIST] = p.Index;
                        break;
                    case World::OWNER_GHOR_BIT:
                        FractionPlID[World::OWNER_GHOR] = p.Index;
                        break;
                    case World::OWNER_MYKO_BIT:
                        FractionPlID[World::OWNER_MYKO] = p.Index;
                        break;
                    case World::OWNER_TAER_BIT:
                        FractionPlID[World::OWNER_TAER] = p.Index;
                        break;
                    default:
                        break;
                    }

                }

                FractionErrorMask |= fraction;
            }
        }

        for ( TDPPlayerData &p : p_YW->_netDriver->GetPlayersData() )
        {
            if ( !p.IsItMe() )
            {
                if ( !lobbyPlayers[p.Index].Ready )
                    netGameCanStart = false;

                if ( !lobbyPlayers[p.Index].Welcomed )
                    isWelcmd = false;
            }
        }

        if ( isHost )
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_STARTOR));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT14));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT15));

            network_button->SetText(1201, Locale::Text::Netdlg(Locale::NETDLG_START));

            if ( !netGameCanStart )
            {
                v410.field_4 = 0;
                v410.butID = 1201;
                network_button->Disable(&v410);
            }
        }
        else
        {
            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_TITLE, Locale::Text::Netdlg(Locale::NETDLG_WAITOR));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE1, Locale::Text::Netdlg(Locale::NETDLG_TXT17));

            network_button->SetText(UIWidgets::NETWORK_MENU_WIDGET_IDS::TXT_MENU_DESCR_LINE2, Locale::Text::Netdlg(Locale::NETDLG_TXT18));

            if ( isWelcmd )
            {
                v410.butID = 1219;
                network_button->Enable(&v410);

                v410.butID = 1221;
                network_button->Enable(&v410);
            }

            v410.butID = 1201;
            network_button->Disable(&v410);
        }
        v410.butID = 1210;
        network_button->Enable(&v410);

        v410.butID = 1211;
        network_button->Enable(&v410);

        v410.butID = 1212;
        network_button->Enable(&v410);

        v410.butID = 1213;
        network_button->Enable(&v410);

        v410.butID = 1214;
        network_button->Enable(&v410);

        v410.butID = 1215;
        network_button->Enable(&v410);

        v410.butID = 1216;
        network_button->Enable(&v410);

        v410.butID = 1217;
        network_button->Enable(&v410);

        for (size_t i = 0; i < World::CVMaxNetPlayers; i++)
        {
            int v370;
            TDPPlayerData pData;
            bool v304 = p_YW->_netDriver->GetPlayerData(i, &pData);

            std::string name = " ";
            int btID;

            switch ( i )
            {
            case 0:
                v370 = 1214;
                btID = 1210;
                if ( v304 )
                    name = pData.name;
                break;

            case 1:
                v370 = 1215;
                btID = 1211;
                if ( v304 )
                    name = pData.name;
                break;

            case 2:
                v370 = 1216;
                btID = 1212;
                if ( v304 )
                    name = pData.name;
                break;

            case 3:
                v370 = 1217;
                btID = 1213;
                if ( v304 )
                    name = pData.name;
                break;

            default:
                break;
            }

            network_button->SetText(btID, name);

            std::string v339("     "); // 5 spaces

            if ( v304 )
            {
                int v305;

                if ( pData.IsItMe() )
                {
                    v305 = SelectedFraction;
                }
                else
                {
                    v305 = lobbyPlayers[pData.Index].NetFraction;
                }

                switch ( v305 )
                {
                case World::OWNER_RESIST_BIT:
                    v339[0] = 'P';
                    break;
                case World::OWNER_GHOR_BIT:
                    v339[0] = 'R';
                    break;
                case World::OWNER_MYKO_BIT:
                    v339[0] = 'T';
                    break;
                case World::OWNER_TAER_BIT:
                    v339[0] = 'V';
                    break;
                default:
                    break;
                }
                if ( lobbyPlayers[pData.Index].IsTrouble && ((GlobalTime / 300) & 1) )
                    v339[1] = 'f';

                if ( lobbyPlayers[pData.Index].Ready )
                    v339[2] = 'h';
            }

            network_button->SetText(v370, v339);
        }
        break;

    default:
        break;
    }

}

bool UserData::LoadSample(int sampleID, const std::string &file)
{
    std::string rsrc = Common::Env.SetPrefix("rsrc", "data:");

    NC_STACK_wav *wav = Nucleus::CInit<NC_STACK_wav>({{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(file)}});
    if ( !wav )
        return false;

    samples1[sampleID] = wav;
    samples1_info.Sounds[sampleID].PSample = wav->GetSampleData();

    if (sampleID == World::SOUND_ID_VOLUME ||
        sampleID == World::SOUND_ID_SLIDER ||
        sampleID == World::SOUND_ID_TEXTAPPEAR ||
        sampleID == World::SOUND_ID_TIMERCOUNT)
        samples1_info.Sounds[sampleID].SetLoop(true);

    Common::Env.SetPrefix("rsrc", rsrc);
    return true;
}

bool UserData::SaveBuildProtoState()
{
    return p_YW->SaveSettings(this, "settings.tmp", (World::SDF_BUDDY | World::SDF_PROTO | World::SDF_USER));
}

void UserData::SaveSettings()
{
    p_YW->SaveSettings(this, fmt::sprintf("%s/user.txt", UserName), World::SDF_ALL);

    FSMgr::FileHandle fil = uaOpenFile("env:user.def", "w");
    if ( fil.OK() )
        fil.write(UserName.c_str(), UserName.size());
}

bool UserData::ShellSoundsLoad()
{
    ScriptParser::HandlersList hndls {
        new World::Parsers::ShellSoundParser(this),
        new World::Parsers::ShellTracksParser(this),
    };

    // Current layout stores world.ini under data; env is kept as legacy fallback.
    if ( uaFileExist("data:world.ini") && ScriptParser::ParseFile("data:world.ini", hndls, 0) )
        return true;

    return uaFileExist("env:world.ini") && ScriptParser::ParseFile("env:world.ini", hndls, 0);
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

    static const std::array<int, 53> HOTKEY
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
        World::INPUT_BIND_TOGGLE_UFO_SPY_UI
    };

    if ( type == World::INPUT_BIND_TYPE_BUTTON && index < BUTTON.size())
        return BUTTON[index];
    else if ( type == World::INPUT_BIND_TYPE_SLIDER && index < SLIDER.size() )
        return SLIDER[index];
    else if ( type == World::INPUT_BIND_TYPE_HOTKEY && index < HOTKEY.size() )
        return HOTKEY[index];
    return -1;
}

bool UserData::IsHasSGM(const std::string &username, int id)
{
    return uaFileExist( fmt::sprintf("save:%s/%d.sgm", username, id) );
}

bool UserData::IsHasRestartForLevel(const std::string &username, int id)
{
    return uaFileExist( fmt::sprintf("save:%s/%d.rst", username, id) );
}
