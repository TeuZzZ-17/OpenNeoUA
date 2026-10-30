#ifndef YW_GAME_UI_INTERNAL_H_INCLUDED
#define YW_GAME_UI_INTERNAL_H_INCLUDED

// Internal header of the in-game GUI implementation (yw_game_ui*.cpp).
// Not part of the public interface: helpers that used to be file-static and are shared between the
// GUI files. The public interface stays in yw.h.

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

// ---- constants shared by the parts ----
static constexpr int kWorldSelectionXHalfArm = 2;
static constexpr int kWorldSelectionXBossHalfArm = 5;
static constexpr int kWorldSelectionXShadowOffset = 1;
static constexpr int kWorldSelectionXUiGap = 2;
static constexpr int STATUS_ICON_MAX_COUNT = 8;
static constexpr int STATUS_ICON_SIZE = 16;
static constexpr int STATUS_ICON_SPACING = 2;
static constexpr uint32_t STATUS_ICON_BLINK_PHASE_MS = 200;
static constexpr int STATUS_ICON_BLINK_MAX_COUNT = 10;
static constexpr uint32_t STATUS_ICON_BLINK_STATE_TTL_MS = 10000;
static constexpr uint32_t STATUS_ICON_BLINK_PRUNE_INTERVAL_MS = 5000;
static constexpr uint32_t ENEMY_INDICATOR_VISIBILITY_FADE_MS = 300;
static constexpr uint32_t ENEMY_INDICATOR_VISIBILITY_STATE_TTL_MS = 1500;
static constexpr uint32_t ENEMY_INDICATOR_VISIBILITY_PRUNE_INTERVAL_MS = 500;
static constexpr uint8_t kSquadronManagerDragOpacity = 128;
static constexpr int kSquadronManagerDefaultShownEntries = 6;
static constexpr int kSquadronManagerExpandedShownEntries = 10;
static constexpr int kSquadronManagerExpandedIconSlots = 5;
static constexpr int kSquadronManagerTitleLabelSummaryGap = 6;
static constexpr int kSquadronManagerTitleIconTextGap = 2;
static constexpr int kSquadronManagerTitleCompactGap = 4;
static constexpr int kSquadronManagerTitleTextPadding = 4;
static constexpr int kSquadronManagerStatusCountGap = 2;
static constexpr int kSquadronManagerRowCountIconGap = 6;
static constexpr int kSquadronManagerSingleDigitCenterNudge = 1;
static constexpr uint32_t kMapToolbarZoomFlashMs = 160;
static constexpr int kRoboMapScrollbarVisualSize = GuiList::kThinScrollbarVisualSize;
static constexpr int PLASMA_CURRENCY_HUD_FONT = 31;

struct StatusIconCacheEntry
{
    NC_STACK_bitmap *bitmap = nullptr;
};

enum StatusIconBlinkMode
{
    STATUS_ICON_BLINK_STEADY = 0,
    STATUS_ICON_BLINK_IN,
    STATUS_ICON_BLINK_OUT
};

struct StatusIconBlinkEntry
{
    std::string path;
    StatusIconBlinkMode mode = STATUS_ICON_BLINK_STEADY;
    uint32_t transitionStart = 0;
    bool desired = false;
};

struct StatusIconBlinkState
{
    NC_STACK_ypaworld *world = nullptr;
    uint32_t gid = 0;
    uint32_t lastTouched = 0;
    std::vector<StatusIconBlinkEntry> entries;
};

struct StatusIconBlinkRenderEntry
{
    std::string path;
    bool visible = true;
};

struct EnemyIndicatorVisibilityState
{
    NC_STACK_ypaworld *world = nullptr;
    uint32_t gid = 0;
    uint32_t lastTouched = 0;
    float opacity = 0.0f;
    vec3d lastVisiblePosition;
};

struct YwLoadedUiBitmap
{
    NC_STACK_bitmap *bitmap = nullptr;
    bool ownerTint = false;
};

struct YWProceduralStatusBarConfig
{
    bool hpEnabled = false;
    GFX::TGLColor hpFullTint = GFX::TGLColor(0.0f, 217.0f / 255.0f, 81.0f / 255.0f, 1.0f);
    GFX::TGLColor hpLowTint = GFX::TGLColor(1.0f, 0.0f, 0.0f, 1.0f);
    GFX::TGLColor hpEmptyTint = GFX::TGLColor(1.0f, 0.0f, 0.0f, 0.0f);
    bool shieldEnabled = false;
    // Same teal as the classic Shield squares in MAPMISC, so the world Shield
    // bar matches the cockpit Shield display.
    GFX::TGLColor shieldTint = GFX::TGLColor(0.0f, 137.0f / 255.0f, 170.0f / 255.0f, 1.0f);
};

struct TPlasmaCurrencyHudLayout
{
    std::string suffix;
    int suffixX = 0;
    int textY = 0;
    int iconLeft = 0;
    int iconTop = 0;
    int iconSize = 0;
    bool showIcon = false;
};

struct SquadronDisplaySummary
{
    NC_STACK_ypabact *highestHpMember = nullptr;
    int visibleMemberCount = 0;
};

enum YwOrderTemplateId
{
    YW_ORDER_TEMPLATE_MOVE = 0,
    YW_ORDER_TEMPLATE_ATTACK,
    YW_ORDER_TEMPLATE_ROBO_MOVE,
    YW_ORDER_TEMPLATE_MAP_MARKER
};

struct aab
{
    int a;
    int b;
};

using StatusIconList = std::array<std::string, STATUS_ICON_MAX_COUNT>;

using StatusIconBlinkRenderList = std::array<StatusIconBlinkRenderEntry, STATUS_ICON_MAX_COUNT>;

typedef int (*mapFunc)(NC_STACK_ypaworld *yw, int x, int y);

// ---- globals defined in the core file ----
extern uint32_t bact_id;
extern GuiBase *dword_5BAFAC;
extern std::string dword_5BAF98;
extern int dword_5BAF9C;
extern GuiList lstvw2;
extern int dword_5BAFA0;
extern int dword_5BAFA4;
extern int dword_5BAFA8;
extern GuiList exit_menu;
extern int dword_5C8B78;
extern int dword_5C8B7C;
extern int dword_5C8B80;
extern int dword_5C8B84;
extern int dword_5C8B88;
extern squadMan squadron_manager;
extern yw_infolog info_log;
extern int dword_516510;
extern int dword_516514;
extern int dword_516518;
extern int dword_51651C;
extern float flt_5164F4;
extern float flt_516504;
extern float flt_51650C;
extern float flt_516524;
extern float flt_51652C;
extern float flt_516528;
extern float flt_516530;
extern tehMap robo_map;
extern energPanel up_panel;
extern GuiList gui_lstvw;
extern bzd bzda;

// ---- functions that used to be file-static ----
uint8_t yw_GetConfiguredUiOpacity(Common::Ini::Key &key, int fallback);
uint8_t yw_GetMapOpacity();
uint8_t yw_GetSquadronManagerOpacity();
bool StatusIconResourceExists(const std::string &path);
const std::string &StatusIconConfiguredPath(Common::Ini::Key &key);
const std::string &StatusIconRegenPath();
const std::string &StatusIconDrainPath();
const std::string &StatusIconDamagedPath();
const std::string &StatusIconSpawnPath();
const std::string &StatusIconRadarPath();
const std::string &StatusIconPowerPath();
const std::string &StatusIconSprintPath();
const std::string &StatusIconHandbrakePath();
int yw_GetUiOwner(const NC_STACK_ypaworld *yw);
const std::string &StatusIconPlasmaPath(const NC_STACK_ypaworld *yw);
bool StatusIconIsNonRoboGun(NC_STACK_ypabact *bact);
NC_STACK_ypabact *StatusIconGetRoboGunHostStation(NC_STACK_ypabact *bact);
bool StatusIconCanUseVehicleCapabilityUnit(NC_STACK_ypabact *bact);
bool StatusIconAdd(StatusIconList &icons, int &count, const std::string &path);
bool StatusIconListContains(const StatusIconList &icons, int count, const std::string &path);
int StatusIconFindBlinkEntry(const std::vector<StatusIconBlinkEntry> &entries, const std::string &path);
int StatusIconGetBlinkCount();
void StatusIconPrepareBlinkStates(NC_STACK_ypaworld *yw, uint32_t now);
bool StatusIconBlinkTransitionComplete(const StatusIconBlinkEntry &entry, int blinkCount, uint32_t now);
bool StatusIconBlinkPhaseVisible(const StatusIconBlinkEntry &entry, uint32_t now);
int StatusIconBuildBlinkRenderList(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const StatusIconList &desiredIcons, int desiredCount, StatusIconBlinkRenderList &renderIcons);
uint8_t ApplyWireframeTintComponent(uint8_t value, float tint);
SDL_Color ApplyWireframeTint(SDL_Color color, const World::TVisualTint *tint);
int ResolveUnitDisplayVehicleProtoId(NC_STACK_ypaworld *yw, const NC_STACK_ypabact *bact);
bool StatusIconIsSvgPath(const std::string &path);
NC_STACK_bitmap *StatusIconLoad(const std::string &path, int width = 0, int height = 0);
void StatusIconRenderBitmap(NC_STACK_ypaworld *yw, NC_STACK_bitmap *bitmap, int left, int top, int size, uint8_t opacity = 255);
SDL_Color yw_GetUiOwnerTintColor(NC_STACK_ypaworld *yw, int owner);
void StatusIconRenderBitmapTinted(NC_STACK_ypaworld *yw, NC_STACK_bitmap *bitmap, int left, int top, int size, const SDL_Color *tint, uint8_t opacity = 255);
YwLoadedUiBitmap yw_LoadNeutralOwnerTintSvg(const std::string &path, int width = 0, int height = 0);
bool StatusIconCanUseMobilePowerUnit(NC_STACK_ypabact *bact);
void StatusIconCollectMountedUnitGunIcons(const std::vector<World::TRoboGun> &guns, const std::string &fallbackIcon, StatusIconList &icons, int &iconCount);
int StatusIconCollect(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, StatusIconList &icons);
void StatusIconRenderBlinkList(NC_STACK_ypaworld *yw, const StatusIconBlinkRenderList &icons, int iconCount, int left, int top, int size, uint8_t opacity = 255);
void StatusIconRenderWorld(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, int barLeft, int barTop, int barWidth, int barHeight, uint8_t opacity, int bottomReserve, int *markerCenterX);
GFX::TGLColor yw_ReadStatusBarTint(Common::Ini::Key &key, const GFX::TGLColor &fallback);
const YWProceduralStatusBarConfig &yw_GetProceduralStatusBarConfig();
GFX::TGLColor yw_StatusBarTintLerp(const GFX::TGLColor &lowTint, const GFX::TGLColor &fullTint, float ratio, uint8_t opacity);
void yw_GetProceduralHpStripRect(int top, int cellHeight, int *outTop, int *outHeight);
bool yw_RenderProceduralHpBar(int left, int top, int squareCount, int value, int maxValue, int cellWidth, int cellHeight, uint8_t opacity, bool hideFilled = false, bool hideEmpty = false);
void yw_GetWorldShieldBarRect(int hpLeft, int hpTop, int hpWidth, int cellHeight, bool proceduralHp, int *outLeft, int *outTop, int *outWidth, int *outHeight);
void yw_RenderProceduralShieldBar(int left, int top, int width, int height, int value, int maxValue, uint8_t opacity);
void yw_RenderUnitSquareBar(NC_STACK_ypaworld *yw, CmdStream *cur, int left, int top, int squareCount, int value, int maxValue, uint8_t filledTile, uint8_t emptyTile, uint8_t opacity = 255);
bool yw_ShouldRenderArtilleryShellCooldownBar(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_RenderArtilleryShellCooldownBar(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact, int left, int top, int squareCount);
bool StatusIconCanRenderCockpitUnit(NC_STACK_ypabact *bact);
void StatusIconRenderCockpit(NC_STACK_ypaworld *yw, sklt_wis *wis, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float hudX, float hudY);
void create_squad_man(NC_STACK_ypaworld *yw);
void sub_4C3A54(NC_STACK_ypaworld *yw);
void sb_0x451034__sub8(NC_STACK_ypaworld *yw);
Common::Point sub_4F681C(vec2d in);
void sub_4F68FC(float a3, float a4, float a5, float a6, SDL_Color a7);
SDL_Color yw_MapToolbarColor(NC_STACK_ypaworld *yw, bool active);
void yw_DrawMapToolbarLine(int x0, int y0, int x1, int y1);
void yw_DrawMapToolbarStroke(int x0, int y0, int x1, int y1, int thickness = 1);
void yw_DrawMapToolbarRect(int left, int top, int right, int bottom, int thickness = 1);
void yw_DrawMapToolbarCircle(int cx, int cy, int radius, int segments = 16);
const char *yw_MapToolbarIconName(int buttonId);
bool yw_IsMapToolbarZoomFlashActive(int buttonId);
void yw_FlashMapToolbarZoomButton(int buttonId);
YwLoadedUiBitmap yw_LoadFactionToolbarIcon(int owner, const char *assetRoot, const char *iconName, bool active, int iconSize);
bool yw_RenderFactionToolbarIcon(NC_STACK_ypaworld *yw, int windowX, int windowY, int buttonId, bool active, const ButtonBox &box);
bool yw_RenderSquadronManagerIcon(NC_STACK_ypaworld *yw, int windowX, int windowY, const char *iconName, bool active, const ButtonBox &box);
void yw_DrawFactionCloseFallback(NC_STACK_ypaworld *yw, int windowX, int windowY, const ButtonBox &box);
void yw_DrawMapToolbarGlyph(NC_STACK_ypaworld *yw, int buttonId, bool active);
void yw_RenderMapTitleToolbar(NC_STACK_ypaworld *yw);
void yw_SetFullScreenUiClip(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerTitleButtonWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerTextWidth(NC_STACK_ypaworld *yw, const std::string &text);
int yw_GetSquadronManagerStatusCellWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerCompactEntryWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerDefaultEntryWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerExpandedEntryWidth(NC_STACK_ypaworld *yw);
bool yw_IsSquadronManagerCompactSize(NC_STACK_ypaworld *yw);
void yw_SetSquadronManagerSize(NC_STACK_ypaworld *yw, int entryWidth, int shownEntries);
void yw_ToggleSquadronManagerCompact(NC_STACK_ypaworld *yw);
void yw_ApplySquadronManagerSavedRect(NC_STACK_ypaworld *yw, const Common::PointRect &rect);
int yw_GetSquadronManagerCompactButtonId();
void yw_UpdateSquadronManagerTitleButtons(NC_STACK_ypaworld *yw);
void yw_DrawSquadronManagerCompactFallback(NC_STACK_ypaworld *yw, const ButtonBox &box, bool active);
bool yw_IsSquadronManagerRowSelected(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
NC_STACK_ypabact *yw_GetSquadronManagerSelectedCommander( NC_STACK_ypaworld *yw);
void yw_DrawSquadronManagerSelectionSummary( NC_STACK_ypaworld *yw, const ButtonBox &compactBox);
void yw_PreDrawSquadronManager(NC_STACK_ypaworld *yw);
void yw_PostDrawSquadronManager(NC_STACK_ypaworld *yw);
int yw_RoboMapMarkerSizePx();
int yw_RoboMapMarkerOwner(NC_STACK_ypaworld *yw);
YwLoadedUiBitmap yw_LoadRoboMapMarkerBitmap(NC_STACK_ypaworld *yw, int markerSize);
void yw_RenderCustomMapMarkers(NC_STACK_ypaworld *yw);
void yw_RenderCustomHudRadarMarkers(NC_STACK_ypaworld *yw);
void yw_RenderArtilleryShellCooldownRadarBar(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, int iconWidth, int iconHeight);
void yw_RenderArtilleryShellCooldownRadarBars(NC_STACK_ypaworld *yw, int cellX0, int cellY0, int cellX1, int cellY1);
const char *yw_ArtilleryShellDefaultMarkerFile();
std::string yw_ResolveArtilleryShellMarkerAsset(const std::string &authored);
NC_STACK_bitmap *yw_LoadArtilleryShellMarkerSvg(const std::string &authored, int width, int height);
bool yw_RenderArtilleryShellMarkerSvg(NC_STACK_ypaworld *yw, const vec3d &pos, float radius, const std::string &markerPath, const SDL_Color &color);
bool yw_IsArtilleryShellMarkerVisible(NC_STACK_ypaworld *yw, const NC_STACK_ypaworld::ArtilleryShellMarker &marker, const Common::Point &cellId);
bool yw_GetArtilleryShellMapCursorTarget(NC_STACK_ypaworld *yw, vec3d *outTarget);
bool yw_ReadSurfacePixel(SDL_Surface *surface, int x, int y, Uint32 *out);
SDL_Color yw_GetMapProjectileOwnerColor(NC_STACK_ypaworld *yw, int owner, int tilesetId);
bool GetPlayerRoboAIBehaviorMapTarget(NC_STACK_ypaworld *yw, vec3d *target);
void sub_4F6980(CmdStream *cur, float a1, float a2, uint32_t a3, int a4, int a5, int screenYOffset = 0);
bool yw_IsDirectPlayerControlledUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void sub_4F72E8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void sb_0x4f8f64__sub1__sub0(NC_STACK_ypaworld *yw);
void sb_0x4f8f64__sub1(NC_STACK_ypaworld *yw);
void sb_0x4f8f64__sub2__sub0(CmdStream *cur, float a1, float a2, char a3, int a4, int a5);
bool yw_IsStrategicMapCellVisible(NC_STACK_ypaworld *yw, const cellArea *cell);
void sb_0x4f8f64__sub2(NC_STACK_ypaworld *yw, CmdStream *cur);
int yw_RoboMapVehicleTileset();
void sub_4F6DFC(NC_STACK_ypaworld *yw, CmdStream *cur, int height, int width, NC_STACK_ypabact *bact, int a6);
NC_STACK_ypabact * sub_4D3C3C(NC_STACK_ypabact *bact);
void sub_4F7BE8(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact, int a2, int a4, int a5, int a6);
void sb_0x4f8f64__sub3__sub1(NC_STACK_ypaworld *yw, const std::string &labl, int tileset_id, CmdStream *cur, int a4, int a5);
void sb_0x4f8f64__sub3(NC_STACK_ypaworld *yw, CmdStream *cur);
void sb_0x4f8f64__sub0(NC_STACK_ypaworld *yw, CmdStream *cur);
void yw_RenderLegacyStrategicMap(NC_STACK_ypaworld *yw);
SDL_Color yw_GetMapTitleBackgroundColor(NC_STACK_ypaworld *yw);
void yw_RenderMapBackground(NC_STACK_ypaworld *yw);
void sb_0x4f8f64(NC_STACK_ypaworld *yw);
void sub_4C157C(NC_STACK_ypaworld *yw);
void yw_RoboMapStoreTrimmedVerticalGlyph(CmdStream *cmd, uint8_t glyph, int trim);
void yw_RoboMapStoreTrimmedHorizontalGlyph(CmdStream *cmd, uint8_t glyph, int trim);
void sub_4C0C00(NC_STACK_ypaworld *yw);
void sub_4C0FEC(NC_STACK_ypaworld *yw);
void sub_4C1214(NC_STACK_ypaworld *yw);
void sub_4F6114(NC_STACK_ypaworld *yw, int a2, CmdStream *cmdbuf, int a3, int a4, int a5, int a6, int a7, mapFunc ffunc, int xpos, int ypos);
int sub_4F6048(NC_STACK_ypaworld *yw, int x, int y);
int sub_4F5FE0(NC_STACK_ypaworld *yw, int x, int y);
int sub_4F5CEC(NC_STACK_ypaworld *yw, int x, int y);
void sb_0x4f6650__sub0(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int xpos, int ypos, int w, int h);
void sb_0x4f6650(NC_STACK_ypaworld *yw, CmdStream *cmdbuf, int x, int y);
void sub_4C1814(NC_STACK_ypaworld *yw, int a2, int a3);
void MapRoboUpdateMapInt(NC_STACK_ypaworld *yw);
void yw_RebuildRoboMapLayout(NC_STACK_ypaworld *yw);
void yw_ToggleRoboMapFullscreen(NC_STACK_ypaworld *yw);
vec2d yw_RoboMapButtonPosToWorld(const Common::Point &btnPos);
bool yw_IsRoboMapMarkerPositionValid(NC_STACK_ypaworld *yw, const vec2d &worldPos);
bool yw_RoboMapMarkerOverlaps(const vec2d &worldPos, int ignoreIndex = -1);
int yw_FindRoboMapMarkerAt(const Common::Point &btnPos);
int yw_AddRoboMapMarker(NC_STACK_ypaworld *yw, const Common::Point &btnPos);
bool yw_PlaceRoboMapMarker(NC_STACK_ypaworld *yw, const Common::Point &btnPos);
bool yw_RemoveNearestRoboMapMarker(const Common::Point &btnPos);
bool yw_MoveRoboMapMarker(NC_STACK_ypaworld *yw, int markerIndex, const Common::Point &btnPos);
void sb_0x451034__sub2(NC_STACK_ypaworld *yw);
void sub_4C32A8(NC_STACK_ypaworld *yw, TMFWinStatus *status);
void sub_4C1970(NC_STACK_ypaworld *yw, int a2);
int sb_0x451034__sub3(NC_STACK_ypaworld *yw);
void create_info_log(NC_STACK_ypaworld *yw);
void create_exit_menu(NC_STACK_ypaworld *yw);
void sb_0x451034__sub5(NC_STACK_ypaworld *yw);
void sb_0x451034__sub9(NC_STACK_ypaworld *yw);
void sub_4DA874(NC_STACK_ypaworld *yw);
void sub_46E16C(NC_STACK_ypaworld *yw);
void sub_4C706C(NC_STACK_ypaworld *yw);
void sub_4C39A4(NC_STACK_ypaworld *yw);
void sub_4E1D24();
void sub_4E2B24(NC_STACK_ypaworld *yw);
void sub_4C8524(NC_STACK_ypaworld *yw);
int sb_0x451034(NC_STACK_ypaworld *yw);
void sb_0x4d7c08__sub0__sub1(const SDL_Color *uiAccent);
bool yw_IsTopLevelExitMenuFlowWindow(const GuiBase *window);
bool yw_IsVisiblePriorityGameplayWindow(const GuiBase *window);
bool yw_IsPriorityGameplayUiOpen();
bool yw_IsPriorityGameplayUiOcclusionOpen();
void yw_RenderGameplayGuiWindow(NC_STACK_ypaworld *yw, GuiBase *window, const SDL_Color *uiAccent);
void yw_ClearVirtualUiWindowRect(NC_STACK_ypaworld *yw, const GuiBase *window);
void yw_ClearPriorityGameplayUiUnderlay(NC_STACK_ypaworld *yw);
void yw_RenderPriorityGameplayWindows(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent);
void yw_ResetVirtualUiClipToFullScreen(NC_STACK_ypaworld *yw);
void yw_RenderTopLevelExitMenuFlow(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent);
void yw_RenderAlwaysVisibleGameplayBars(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent);
void yw_RenderForegroundGameplayGuiWindows(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent);
void yw_FinalizePriorityGameplayUi(NC_STACK_ypaworld *yw);
void sb_0x4d7c08__sub0(NC_STACK_ypaworld *yw);
void yw_SetGuiListBackgroundOpacity(CmdStream *cur, uint8_t opacity);
int yw_GetGuiListScrollbarGapFillWidth(const GuiList *lstvw, NC_STACK_ypaworld *yw);
void yw_StoreGuiListRowBackground(const GuiList *lstvw, CmdStream *cur, int fillTileset, int fillWidth, int opacityOverride = -1);
int yw_GetGenesisScreenTextWidth(NC_STACK_ypaworld *yw, const std::string &text);
std::string yw_FormatGenesisVehicleName(NC_STACK_ypaworld *yw, int normalTextWidth, int selectedTextWidth, const std::string &name, int remaining);
void buy_list_update_sub(NC_STACK_ypaworld *yw, int a2, GuiList *lstvw, CmdStream *cur, uint32_t a5, const std::string &a6, int a7, int remaining);
void gui_update_create_btn__sub0(NC_STACK_ypaworld *yw);
void gui_update_create_btn(NC_STACK_ypaworld *yw, CmdStream *cur);
void gui_update_map_squad_btn(NC_STACK_ypaworld *yw, CmdStream *cur);
void gui_update_player_panel(NC_STACK_ypaworld *yw, CmdStream *cur);
void gui_update_tools(NC_STACK_ypaworld *yw, CmdStream *cur);
void sub_449970(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5, int a6, int a7);
void ypaworld_func64__sub7__sub2__sub1__sub0(NC_STACK_ypaworld *yw, CmdStream *cur);
void ypaworld_func64__sub7__sub2__sub1(NC_STACK_ypaworld *yw);
NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub6(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub2__sub7(NC_STACK_ypaworld *yw);
int ypaworld_func64__sub7__sub2__sub3(NC_STACK_ypaworld *yw, TInputState *inpt);
void ypaworld_func64__sub7__sub2__sub2(NC_STACK_ypaworld *yw);
NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub5(NC_STACK_ypaworld *yw);
NC_STACK_ypabact * ypaworld_func64__sub7__sub2__sub4(NC_STACK_ypaworld *yw);
int sub_4C885C();
void ypaworld_func64__sub7__sub2__sub8(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub2__sub9(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub2__sub0();
void sb_0x4c66f8__sub0(NC_STACK_ypaworld *yw);
void sb_0x4c66f8(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2);
void ypaworld_func64__sub7__sub2(NC_STACK_ypaworld *yw, TInputState *inpt);
void sub_4E1D6C(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, uint8_t icon, uint8_t icon2, uint8_t icon3, float a6, float a7, const std::string &a8);
void ypaworld_func64__sub7__sub7__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, int a3, int a4, int a5, int a6, float a7);
void ypaworld_func64__sub7__sub7__sub0__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7);
void ypaworld_func64__sub7__sub7__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur, int a3, int a2, char a4, int a5, int a6, int a7);
void ypaworld_func64__sub7__sub7__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur, int x, int y, char a4, int a5, int a6, int a7);
bool yw_GetPlasmaCurrencyHudLayout(NC_STACK_ypaworld *yw, uint64_t value, TPlasmaCurrencyHudLayout *layout);
void yw_RenderPlasmaCurrencyHud(NC_STACK_ypaworld *yw, CmdStream *cur);
void yw_RenderPlasmaCurrencyHudIcon(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub7__sub0(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub7(NC_STACK_ypaworld *yw, TInputState *inpt);
uint32_t sub_4C7134(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_IsSquadronDisplayMember(NC_STACK_ypabact *bact);
SquadronDisplaySummary yw_GetSquadronDisplaySummary(NC_STACK_ypabact *leader);
std::vector<NC_STACK_ypabact *> yw_GetSquadronDisplayUnits( NC_STACK_ypabact *leader);
char yw_GetSquadronManagerStatusIcon(NC_STACK_ypabact *bact);
uint32_t yw_GetSquadronManagerUnitIcon(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, int blink);
uint8_t yw_GetSquadronManagerHealthGlyph(NC_STACK_ypabact *bact);
bool yw_IsSquadronManagerRowHovered(NC_STACK_ypabact *bact);
int yw_GetSquadronManagerScrollbarGapFillWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerBackgroundFillWidth(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerCountReserveWidth(NC_STACK_ypaworld *yw);
void yw_UpdateSquadronManagerColumnLayout(NC_STACK_ypaworld *yw);
int yw_GetSquadronManagerUnitSlotCount(NC_STACK_ypaworld *yw);
void yw_UpdateSquadronManagerDynamicLayout(NC_STACK_ypaworld *yw);
NC_STACK_ypabact *yw_GetSquadronAggressionCommander( NC_STACK_ypaworld *yw);
int yw_GetSquadronAggressionIndexAtX(NC_STACK_ypaworld *yw, int x);
void yw_RenderSquadronAggressionStrip(NC_STACK_ypaworld *yw, CmdStream *cur);
void sub_4C7950(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3);
void ypaworld_func64__sub7__sub3__sub0__sub2(NC_STACK_ypaworld *yw, CmdStream *cur);
void yw_RenderSquadronManagerSquadContent(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur, uint8_t opacity);
void yw_RenderSquadronManagerWholeSquadDragGhost( NC_STACK_ypaworld *yw, CmdStream *cur, int firstIconX, int rowY);
void ypaworld_func64__sub7__sub3__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur);
void ypaworld_func64__sub7__sub3__sub0__sub0(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, CmdStream *cur);
void ypaworld_func64__sub7__sub3__sub0__sub3(NC_STACK_ypaworld *yw, CmdStream *cur);
void ypaworld_func64__sub7__sub3__sub0(NC_STACK_ypaworld *yw, TInputState *inpt);
void sub_4C707C(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub3__sub3(NC_STACK_ypaworld *yw, TClickBoxInf *winpt);
bool yw_IsSquadronManagerWholeSquadDragArea( NC_STACK_ypaworld *yw, int squadIndex, int mouseX);
void sub_4C1FBC();
void ypaworld_func64__sub7__sub1__sub0(NC_STACK_ypaworld *yw);
void yw_ResetWorldSelectionDrag(NC_STACK_ypaworld *yw);
SDL_Color yw_GetFactionSelectionColor(NC_STACK_ypaworld *yw);
SDL_Color yw_GetNeutralSelectionShadowColor();
const SDL_Color *yw_GetFactionUiAccent(NC_STACK_ypaworld *yw, SDL_Color *accent);
SDL_Color yw_GetFactionUiTextColor(NC_STACK_ypaworld *yw);
float yw_GetWorldUiMaxDistance();
bool yw_ShouldHideControlledUnitWorldUi(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_IsActiveSquadronSelectionUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
float yw_GetWorldUiFadeStart(float maxDistance);
uint8_t yw_GetWorldUiOpacity(NC_STACK_ypaworld *yw, const vec3d &worldPos);
int yw_GetWorldUiSectorRadius(float maxDistance);
void yw_PrepareEnemyIndicatorVisibilityStates(NC_STACK_ypaworld *yw);
uint8_t yw_GetEnemyIndicatorVisibilityOpacity(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, bool currentlyVisible, vec3d *renderPosition);
bool yw_ProjectWorldSelectionPoint(NC_STACK_ypaworld *yw, const vec3d &worldPos, Common::Point *screenPos);
void yw_RenderPlasmaCurrencyPopups(NC_STACK_ypaworld *yw, CmdStream *cur);
bool yw_IsWorldSelectionUnitValid(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_TryAddClickedUnitToActiveSquad(NC_STACK_ypaworld *yw, TInputState *inpt);
void yw_SelectWorldUnitsInDrag(NC_STACK_ypaworld *yw);
bool yw_CanStartWorldSelectionDrag(NC_STACK_ypaworld *yw, TInputState *inpt);
Common::Point yw_ClampWorldSelectionPoint(NC_STACK_ypaworld *yw, Common::Point point);
void yw_WorldSelectionDragInput(NC_STACK_ypaworld *yw, TInputState *inpt);
void yw_RenderWorldSelectionDrag(NC_STACK_ypaworld *yw);
int yw_GetOrderIconOwner(NC_STACK_ypaworld *yw);
std::string yw_OrderTemplateCanonicalPath(YwOrderTemplateId templateId);
YwLoadedUiBitmap yw_LoadOrderTemplateBitmap(NC_STACK_ypaworld *yw, YwOrderTemplateId templateId);
void yw_RenderOrderTemplateAt(NC_STACK_ypaworld *yw, const Common::Point &point, int radius, uint8_t worldUiOpacity, YwOrderTemplateId templateId);
void yw_RenderRoboRelocationMarker(NC_STACK_ypaworld *yw);
void yw_RenderCustomWorldMapMarkers(NC_STACK_ypaworld *yw);
void yw_RenderMoveOrderFeedback(NC_STACK_ypaworld *yw);
uint64_t yw_AttackOrderSquadKey(uint32_t commandID, uint8_t owner);
TAttackOrderFeedbackEntry *yw_FindAttackOrderFeedback( NC_STACK_ypaworld *yw, uint32_t commandID, uint8_t owner);
void yw_SetAttackOrderFeedback(NC_STACK_ypaworld *yw, NC_STACK_ypabact *commander, NC_STACK_ypabact *target);
void yw_ClearAttackOrderFeedback(NC_STACK_ypaworld *yw, uint32_t commandID, uint8_t owner);
bool yw_IsActiveAttackOrderSource(NC_STACK_ypabact *unit);
bool yw_IsEnemyAttackOrderTarget(NC_STACK_ypaworld *yw, NC_STACK_ypabact *target);
void yw_AddAttackOrderTarget(NC_STACK_ypaworld *yw, NC_STACK_ypabact *target, std::vector<NC_STACK_ypabact *> *targets);
bool yw_IsUnitTargetingAttackOrderTarget(NC_STACK_ypabact *unit, NC_STACK_ypabact *target);
bool yw_IsSquadTargetingAttackOrderTarget(NC_STACK_ypabact *commander, NC_STACK_ypabact *target);
void yw_CollectAlliedAttackTargetsFromUnit( NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit, uint8_t playerOwner, bool trustedPlayerHierarchy, std::vector<NC_STACK_ypabact *> *visited, std::vector<NC_STACK_ypabact *> *targets);
void yw_CollectAlliedAttackTargetsFromSectorUnits( NC_STACK_ypaworld *yw, uint8_t playerOwner, std::vector<NC_STACK_ypabact *> *visited, std::vector<NC_STACK_ypabact *> *targets);
void yw_CollectAlliedAttackTargets( NC_STACK_ypaworld *yw, std::vector<NC_STACK_ypabact *> *targets);
void yw_AddManualAttackOrderGraceTargets( NC_STACK_ypaworld *yw, std::vector<NC_STACK_ypabact *> *targets);
bool yw_ProjectAttackOrderTargetPoint( NC_STACK_ypaworld *yw, const vec3d &worldPos, Common::Point *screenPos);
void yw_RenderAttackOrderTargetMarker( NC_STACK_ypaworld *yw, NC_STACK_ypabact *target, const YwLoadedUiBitmap &bitmap);
void yw_RenderAttackOrderFeedback(NC_STACK_ypaworld *yw);
bool yw_GuiListOwnsWheelAtPointer(const GuiList &list, const TClickBoxInf &click);
void RoboMap_InputHandle(NC_STACK_ypaworld *yw, TInputState *inpt);
void ypaworld_func64__sub7__sub0__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int id);
void ypaworld_func64__sub7__sub0__sub0(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub7__sub0(NC_STACK_ypaworld *yw, TInputState *inpt);
GuiBase * ypaworld_func64__sub7__sub6__sub1();
void sub_451714_with_yoffset(TileMap *, CmdStream *cur, const std::string &txt, int a2, uint8_t a4, int textYOffset);
void sub_451714(TileMap *tiles, CmdStream *cur, const std::string &txt, int a2, uint8_t a4);
void sub_4DA8DC(NC_STACK_ypaworld *yw, CmdStream *cur, int a4, int a3, const std::string &a5);
void ypaworld_func64__sub7__sub6__sub3(NC_STACK_ypaworld *yw, int a2, int a4);
void sub_4C8534(NC_STACK_ypaworld *yw, CmdStream *cur, const std::string &a2);
void ypaworld_func64__sub7__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3);
bool sub_4C3E34(NC_STACK_ypabact* const &a1, NC_STACK_ypabact* const &a2);
void ypaworld_func64__sub14(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub8(NC_STACK_ypaworld *yw);
bool yw_IsGenesisEnergyOrder();
int sub_4C3C88(const void *a1, const void *a2);
void ypaworld_func64__sub16(NC_STACK_ypaworld *yw);
int sub_4C3D6C(const void *a1, const void *a2);
void ypaworld_func64__sub17(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub15(NC_STACK_ypaworld *yw);
void sb_0x4d7c08__sub0__sub0(NC_STACK_ypaworld *yw);
void wis_color(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2);
void yw_RenderVector2D(NC_STACK_ypaworld *yw, UAskeleton::Data *wire, float posX, float posY, float m00, float m01, float m10, float m11, float scaleX, float scaleY, SDL_Color coloooor, wis_color_func color_func, wis_color_func color_func2, bool aspectCorrection, const World::TVisualTint *wireframeTint);
void yw_RenderInfoVehicleWire(NC_STACK_ypaworld *yw, sklt_wis *wis, World::TVhclProto *vhcl, float a4, float a5, float a6, int owner);
void sub_4E4F80(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float x, float y, int value, int maxval, int valCH, int valBG, const std::string &txt1, const std::string &txt2, int flag = 0);
void yw_RenderInfoLifebar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos);
void yw_RenderInfoShieldbar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos);
void yw_RenderInfoVehicleName(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, const std::string &name, float xpos, float ypos);
void yw_RenderInfoWeaponName(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, const std::string &name, float xpos, float ypos);
void yw_RenderInfoWeaponLikeWire(NC_STACK_ypaworld *yw, sklt_wis *wis, NC_STACK_skeleton *wireframe, const World::TVisualTint *wireframeTint, float xpos, float ypos);
void yw_RenderInfoWeaponWire(NC_STACK_ypaworld *yw, sklt_wis *wis, World::TWeapProto *wpn, float xpos, float ypos, int owner);
void yw_RenderInfoReloadbar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TWeapProto *wpn, float xpos, float ypos);
bool yw_HasInfoMagazineBar(const World::TWeapProto *wpn);
void yw_RenderInfoMagazineBar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TWeapProto *wpn, float xpos, float ypos);
std::string yw_BuildInfoWeaponDamageText(NC_STACK_ypabact *bact, World::TVhclProto *vhcl, World::TWeapProto *weap, int effectiveEnergy);
int yw_GetInfoHudTextWidth(NC_STACK_ypaworld *yw, const std::string &text);
int yw_GetInfoWeaponEffectIcons(NC_STACK_ypaworld *yw, World::TVhclProto *vhcl, World::TWeapProto *weap, int iconSize, std::array<NC_STACK_bitmap *, 2> *bitmaps);
int yw_GetInfoWeaponIconSize(NC_STACK_ypaworld *yw);
int yw_GetInfoWeaponRightContentWidth(NC_STACK_ypaworld *yw, World::TVhclProto *vhcl, World::TWeapProto *weap, const std::string &damageText, int *iconCountOut = nullptr);
float yw_GetInfoDamageWireX(NC_STACK_ypaworld *yw, sklt_wis *wis, float vanillaX, World::TVhclProto *vhcl, World::TWeapProto *weap, const std::string &damageText);
void yw_RenderInfoWeaponEffectIcons(NC_STACK_ypaworld *yw, sklt_wis *wis, World::TVhclProto *vhcl, World::TWeapProto *weap, const std::string &damageText, float xpos, float ypos);
void yw_RenderInfoWeaponInf(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, World::TWeapProto *weap, float xpos, float ypos);
bool yw_IsMgunWeaponInfoAvailable(NC_STACK_ypabact *bact, World::TVhclProto *vhcl);
void yw_RenderInfoMgunReloadbar(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos);
std::string yw_BuildInfoMgunDamageText(int effectivePower);
void yw_RenderInfoMgunDamage(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, NC_STACK_ypabact *bact, World::TVhclProto *vhcl, float xpos, float ypos);
void yw_RenderHUDInfo(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float xpos, float ypos, NC_STACK_ypabact *bact, int vhclid, int flag);
void sb_0x4d7c08__sub0__sub0__sub0__sub0(NC_STACK_ypaworld *yw, sklt_wis *wis, CmdStream *cur, float a4, float a5, int bldId, int a7);
int sb_0x4d7c08__sub0__sub0__sub0(NC_STACK_ypaworld *yw);
void wis_color2(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2);
void yw_RenderHUDCompass(NC_STACK_ypaworld *yw, sklt_wis *wis);
void yw_RenderHUDVectorGFX(NC_STACK_ypaworld *yw, CmdStream *cur);
void sb_0x4d7c08__sub0__sub4__sub0__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact);
bool yw_IsUfoSpyHudActive(NC_STACK_ypaworld *yw);
uint8_t yw_GetUfoSpyWorldOpacity(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_ShouldRenderUfoSpyWorldArrow(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
bool yw_ShouldRenderUfoSpyWorldStatus(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void yw_RenderUnitLifeBar(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact);
bool yw_IsExplicitWorldStatusTarget(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const std::vector<NC_STACK_ypabact *> &damageHoverTargets);
bool yw_IsExplicitWorldCursorTarget( NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const std::vector<NC_STACK_ypabact *> &damageHoverTargets);
void yw_RenderUfoSpyWorldStatus(NC_STACK_ypaworld *yw, CmdStream *cur, NC_STACK_ypabact *bact, const std::vector<NC_STACK_ypabact *> &damageHoverTargets);
void yw_RenderUfoSpyWorldArrows( NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const std::vector<NC_STACK_ypabact *> &damageHoverTargets);
void yw_DrawWorldSelectionX(int centerX, int centerY, int halfArm, SDL_Color color);
bool yw_GetWorldSelectionXAnchor(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const Common::Point &point, int *centerXOut, int *centerYOut);
void yw_DrawWorldKillMarks(int centerX, int tipY, uint8_t marks, SDL_Color color);
bool yw_IsWorldBossMarkerUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
int yw_GetWorldSelectionXHalfArm(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void yw_RenderOverlayCursors( NC_STACK_ypaworld *yw, CmdStream *cur, const std::vector<NC_STACK_ypabact *> &damageHoverTargets);
void sb_0x4d7c08__sub0__sub4(NC_STACK_ypaworld *yw);
void sub_4E3B80(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2);
void sub_4E3D98(NC_STACK_ypaworld *yw, float x1, float y1, float x2, float y2, SDL_Color *out1, SDL_Color *out2);
bool yw_ProjectHUDMissileLockTarget(NC_STACK_ypaworld *yw, NC_STACK_ypabact *target, float *outX, float *outY);
void yw_RenderHUDWeaponLockMarker(NC_STACK_ypaworld *yw, sklt_wis *wis, UAskeleton::Data *wpn_wure, UAskeleton::Data *wpn_wure2, bool locked, float fromX, float fromY, float toX, float toY, float hudAppearProgress, const World::TVhclProto *vhcl);
void yw_RenderHUDTarget(NC_STACK_ypaworld *yw, sklt_wis *wis);
void yw_RenderCursorOverUnitWithOpacity(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact, const vec3d &renderPosition, uint8_t visibilityOpacity);
void yw_RenderCursorOverUnit(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact);
void sb_0x4d7c08__sub0__sub4__sub2__sub0(NC_STACK_ypaworld *yw);
void yw_RenderHUDRadare(NC_STACK_ypaworld *yw);
int sub_47F360(const void *a1, const void *a2);
void sb_0x4d7c08__sub0__sub2__sub1(NC_STACK_ypaworld *yw, CmdStream *cur, const std::array<World::TPlayerStatus, World::CVFractionsCount> &statuses, int a3);
void sb_0x4d7c08__sub0__sub2__sub0(NC_STACK_ypaworld *yw, CmdStream *cur, int a3);
void sb_0x4d7c08__sub0__sub2(NC_STACK_ypaworld *yw);
void ypaworld_func2__sub0__sub1(NC_STACK_ypaworld *yw, NC_STACK_ypabact *bact1, NC_STACK_ypabact *bact2);
int sub_4D3C80(NC_STACK_ypaworld *yw);

#endif // YW_GAME_UI_INTERNAL_H_INCLUDED
