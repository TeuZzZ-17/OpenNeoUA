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
static constexpr bool OPTIONS_RESET_RETRO_CURSORS = false;
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
int NormalizeFrameRateLimit(int value);
int CycleFrameRateLimit(int value);
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
std::string TrimIniValue(std::string s);
int VisualFilterStrengthPercentFromString(std::string s, int fallback);
int FloatHundredFromString(std::string s, int fallback, int minValue, int maxValue);
int IntFromString(std::string s, int fallback, int minValue, int maxValue);
int SkyHorizonDistanceToSlider(int32_t distance);
int32_t SkyHorizonSliderToDistance(int sliderValue);
std::string HundredStorageValue(int value);
std::string PaletteThemeDisplayName(const std::string &fileName);
void sb_0x4eb94c__sub0(NC_STACK_ypaworld *yw, bool clockwise, int a3, vec3d *pos, baseRender_msg *arg);
void sb_0x4eb94c__sub1(NC_STACK_ypaworld *yw, bool clockwise, int rot, vec3d *pos, baseRender_msg *arg);
void sb_0x4eb94c(NC_STACK_ypaworld *yw, TBriefengScreen *brf, TInputState *struc, int a5);
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
std::string db_trunc(const std::string &s, int max_len);
void db_collect_valid(const UserData *usr, std::vector<int> &out);
std::string db_weapon_display_name(const std::vector<World::TWeapProto> &wpns, int weaponId);
int db_vehicle_flyer_type(const World::TVhclProto &p);
std::string db_vehicle_model_display_name(const World::TVhclProto &p);
std::string db_weapon_model_display_name(const World::TWeapProto &p);
std::string db_weapon_blast_energy_display(const World::TWeapProto &p);
std::string db_float_display(float value);
std::string db_optional_float_display(float value);
bool db_static_gun_support_active(const World::TVhclProto &p);
std::string db_vehicle_job_stars(int value);
bool db_vehicle_has_job_stats(const World::TVhclProto &p);
std::string db_compact_pair_line(const std::string &leftLabel, const std::string &leftValue, bool hasLeft, const std::string &rightLabel, const std::string &rightValue, bool hasRight);
void db_add_vehicle_job_lines(std::vector<std::string> *lines, int maxLines, const World::TVhclProto &p);
bool db_weapon_has_energy_multipliers(const World::TWeapProto &p);
std::string db_weapon_energy_stars(float value);
void db_add_weapon_energy_lines(std::vector<std::string> *lines, int maxLines, const World::TWeapProto &p);
void db_add_speciality_lines(std::vector<std::string> *lines, int maxLines, const std::vector<std::string> &items);
std::vector<std::string> db_weapon_specialties(const World::TWeapProto &p);
bool db_vehicle_has_mounted_kamikaze( const World::TVhclProto &vehicle, const std::vector<World::TWeapProto> &weapons);
bool db_vehicle_has_kamikaze( const World::TVhclProto &vehicle, const std::vector<World::TWeapProto> &weapons, const std::vector<World::TVhclProto> &vehicles);
std::vector<std::string> db_vehicle_specialties( const World::TVhclProto &p, const std::vector<World::TWeapProto> &weapons, const std::vector<World::TVhclProto> &vehicles);
std::vector<std::string> db_building_specialties(const World::TBuildingProto &p);
std::string db_building_model_display_name(const World::TBuildingProto &p);
std::string db_database_asset_path(const char *kind, const char *prefix, int id, const char *ext);
std::string db_database_flat_asset_path(const char *prefix, int id, const char *ext);
const char *db_asset_kind(int tab);
const char *db_asset_prefix(int tab);
std::string db_database_asset_path(int tab, int id, const char *ext);
std::string db_database_flat_asset_path(int tab, int id, const char *ext);
bool db_load_text_file(const std::string &path, std::string *out);
bool db_load_database_text(int tab, int id, std::string *out);
int db_text_width(TileMap *tiles, const std::string &txt);
std::string db_fit_text(TileMap *tiles, const std::string &txt, int max_width);
std::string db_centered_proto_id(int proto_id);
std::string db_make_name_row(const std::string &name, int proto_id, bool selected, int row_width);
bool db_text_fits(TileMap *tiles, const std::string &txt, int max_width);
int db_detail_text_columns(NC_STACK_ypaworld *yw);
void db_push_wrapped_line(TileMap *tiles, const std::string &line, int max_width, int max_lines, std::vector<std::string> *out);
std::vector<std::string> db_wrap_text_columns(const std::string &text, int max_columns, int max_lines);
NC_STACK_bitmap *db_load_database_image(int tab, int id);
void db_get_entry_image_rect(NC_STACK_ypaworld *yw, int *left, int *top, int *width, int *height);
Common::FRect db_screen_rect_to_ndc(NC_STACK_ypaworld *yw, int left, int top, int width, int height);
void db_draw_frame(SDL_Surface *scr, int left, int top, int width, int height, uint8_t r, uint8_t g, uint8_t b);
void db_fill_card_background(SDL_Surface *scr, int left, int top, int width, int height);
int ypaworld_func158__sub0__sub6(char a1);

#endif // YW_SHELL_MENUS_H_INCLUDED
