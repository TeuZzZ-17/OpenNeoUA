#ifndef YW_SHELL_MENUS_H_INCLUDED
#define YW_SHELL_MENUS_H_INCLUDED

// Internal header of the game shell menus (yw_func2*.cpp, yw_options.cpp, yw_database.cpp, yw_input_config.cpp):
// helpers that used to be file-static. The public interface stays in yw.h.

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

// ---- constants shared by the parts ----
static constexpr int SETTINGS_CHANGE_PALETTE_THEME = 0x2000;
static constexpr int SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR = 0x4000;
static constexpr int SETTINGS_CHANGE_SPECTATOR_MODE = 0x8000;
static constexpr int SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS = 0x10000;
static constexpr int SETTINGS_CHANGE_AMBIENT_VOLUME = 0x20000;
static constexpr int SETTINGS_CHANGE_MAXFPS                 = 0x40000;
static constexpr int SETTINGS_CHANGE_BLENDING              = 0x80000;
static constexpr int SETTINGS_CHANGE_MOVIE_PLAYER          = 0x100000;
static constexpr int SETTINGS_CHANGE_MENU_FONT             = 0x200000;
static constexpr int SETTINGS_CHANGE_INTERFACE_STYLE       = 0x800000;
static constexpr int SETTINGS_CHANGE_HIDE_MAP_BORDER_WALLS = 0x1000000;
static constexpr int SETTINGS_CHANGE_RETRO_CURSORS         = 0x2000000;
static constexpr int SETTINGS_CHANGE_RESTART_REQUIRED_GRAPHICS =
    SETTINGS_CHANGE_BLENDING | SETTINGS_CHANGE_MENU_FONT;
static constexpr int MENU_MSGBOX_RESTORE_DEFAULT_KEYS = 1;
static constexpr int MENU_MSGBOX_INPUT_KEY_CONFLICT = 2;
static constexpr int OPTIONS_RESET_WIDTH = 800;
static constexpr int OPTIONS_RESET_HEIGHT = 600;
static constexpr int OPTIONS_RESET_BLENDING = 1;          // Additive
static constexpr int OPTIONS_RESET_MAX_FPS = 240;
static constexpr const char *OPTIONS_RESET_MENU_FONT = "Xolonium Regular";
static constexpr bool OPTIONS_RESET_MOVIE_PLAYER = true;  // Intro Movies
static constexpr bool OPTIONS_RESET_PLAYER_ROBO_AI = true;
static constexpr bool OPTIONS_RESET_SPECTATOR = false;
static constexpr bool OPTIONS_RESET_PLAY_AS = false;
static constexpr bool OPTIONS_RESET_RETRO_INTERFACE = true;
static constexpr bool OPTIONS_RESET_HIDE_MAP_BORDER_WALLS = false;
static constexpr bool OPTIONS_RESET_RETRO_CURSORS = true;
static constexpr const char *OPTIONS_RESET_GENESIS_LIST_ORDER = "energy";
static constexpr int OPTIONS_RESET_FX_NUMBER = 16;
static constexpr int OPTIONS_RESET_SOUND_VOLUME = 127;
static constexpr int OPTIONS_RESET_MUSIC_VOLUME = 60;
static constexpr int OPTIONS_RESET_AMBIENT_VOLUME = 100;

// ---- globals defined in the core file ----
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

// ---- functions that used to be file-static ----
std::string InputKeyDisplayTitle(int16_t keyCode);
std::string InputFixedShortcutTitle(int binding);
bool InputKeyUsesOtherSlot(const UserData *usr, int target, bool positiveSlot, int16_t keyCode);
std::string InputKeyConflictNames(const UserData *usr, int target, bool positiveSlot, int16_t keyCode);
void ClearInputKeyConflicts(UserData *usr, int target, bool positiveSlot, int16_t keyCode);
void ApplyCapturedInputKey(UserData *usr, int target, bool positiveSlot, int16_t keyCode);
void ClearPendingInputKey(UserData *usr);
void LayoutSaveLoadActionButtons(UserData *usr, bool deleteVisible);
std::string BlendingLabel(int v);
int CycleBlending(int v);
std::string NormalizeGenesisListOrder(const std::string &v);
std::string CycleGenesisListOrder(const std::string &v);
std::string GenesisListOrderLabel(const std::string &v);
int NormalizeGraphicsMode(int value);
int NormalizeVideoFilterMode(int value);
std::string VideoFilterModeDisplayName(int value);
int NormalizeHorizonFadeMode(int value);
std::string HorizonFadeModeDisplayName(int value);
std::string NormalizePaletteThemeName(std::string theme);
std::string PaletteThemeStorageValue(const std::string &theme);
std::string NormalizeMenuFontName(std::string fontName);
std::string MenuFontDisplayName(const std::string &fontName);
void SavePaletteThemeCache(const std::string &theme);
std::string VisualFilterStrengthStorageValue(int value);
int VisualFilterStrengthPercentFromString(std::string s, int fallback);
int FloatHundredFromString(std::string s, int fallback, int minValue, int maxValue);
int IntFromString(std::string s, int fallback, int minValue, int maxValue);
int SkyHorizonDistanceToSlider(int32_t distance);
int32_t SkyHorizonSliderToDistance(int sliderValue);
std::string HundredStorageValue(int value);
std::string PaletteThemeDisplayName(const std::string &fileName);
void ypaworld_func158__DrawVehicle(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc);
void yw_draw_input_list(NC_STACK_ypaworld *yw, UserData *usr);
int yw_loadSky(NC_STACK_ypaworld *yw, const std::string &skyname);
void listLocaleDir(UserData *usr, const char *dirname);
void ypaworld_func156__sub1(UserData *usr);
void sb_0x46ca74__sub0(const std::string &a1, const std::string &a2);
void sb_0x47f810(NC_STACK_ypaworld *yw);
void sub_44A1FC(NC_STACK_ypaworld *yw);
void sub_4D9550(NC_STACK_ypaworld *yw, int arg);
void sub_4D0C24(NC_STACK_ypaworld *yw, const std::string &a1, const std::string &a2);
void sub_4EDCD8(NC_STACK_ypaworld *yw);
void ypaworld_func158__sub0__sub9(NC_STACK_ypaworld *yw);
void ShowInputSettings2(NC_STACK_ypaworld *yw);
void ShowInputSettings1(NC_STACK_ypaworld *yw);
bool SaveIniKey(const std::string &iniPath, const std::string &key, const std::string &value);
bool RemoveIniKey(const std::string &iniPath, const std::string &key);
void sub_46D0F8(const std::string &path);
int ypaworld_func158__sub0__sub6(char a1);

#endif // YW_SHELL_MENUS_H_INCLUDED
