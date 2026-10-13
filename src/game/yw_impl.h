#ifndef YW_IMPL_H_INCLUDED
#define YW_IMPL_H_INCLUDED

// Internal header of the NC_STACK_ypaworld implementation files (yw.cpp, yw_shell.cpp, yw_sprint.cpp ...).
// Not part of the public interface: helpers that used to be file-static and are shared between them.
// The public interface stays in yw.h.

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
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"

// ---- constants shared by the parts ----
static constexpr uint32_t GEM_NEW_UI_DEFAULT_DURATION_MS = 8000;
static constexpr uint32_t GAMEPLAY_TIME_SCALE_MAX_DURATION_MS = 600000;
static constexpr float GAMEPLAY_MIN_TIME_SCALE = 0.05f;
static constexpr float DEBUG_GAMEPLAY_TIME_SCALE = 0.20f;
static constexpr float DEBUG_GAMEPLAY_FAST_TIME_SCALE = 2.0f;
static constexpr float ROBO_DEATH_TIME_SCALE_MAX_DISTANCE_LIMIT = 1000000.0f;
static constexpr uint32_t PLASMA_CURRENCY_HUD_PULSE_MS = 350;

struct TimedGameplayScaleProfile
{
    float scale = 1.0f;
    uint32_t durationMs = 0;
};

struct PlayerSprintConfig
{
    bool complete = false;
    World::TAbsoluteOrPercent forceUp;
    float pitchUpPercent = 0.0f;
    int32_t rampTime = 0;
    World::TAbsoluteOrPercent energyCost;
};

struct yw_ModelPointCloud
{
    std::vector<vec3d> points;
    vec3d min;
    vec3d max;
    bool valid = false;
};

// ---- globals defined in the core file ----
extern yw_infolog info_log;
extern int vertMenuSpace;
extern int dword_5A50B2;
extern int dword_5A50B2_h;
extern int word_5A50AE;
extern int word_5A50BC;
extern int word_5A50BA;
extern int word_5A50BE;
extern int buttonsSpace;
extern int checkBoxWidth;
extern int dword_5A50B6;
extern int dword_5A50B6_h;
extern int scaledFontHeight;
extern int bottomButtonsY;
extern int button1LineWidth;
extern int bottomCenteredFirstBtnPosX;
extern int bottomCenteredSecondBtnPosX;
extern int bottomSecondBtnPosX;
extern int bottomThirdBtnPosX;
extern GuiList stru_5C91D0;
extern uint32_t bact_id;
extern uint32_t dword_5A7A80;
extern int PrevMouseX;
extern int PrevMouseY;

// ---- functions that used to be file-static ----
int yw_GetAutomaticGenesisPitch(int energy);
float yw_ReadGameplayTimeScale(Common::Ini::Key &key, float fallback);
uint32_t yw_ReadNonNegativeDuration(Common::Ini::Key &key, uint32_t fallback);
float yw_ReadNonNegativeDistance(Common::Ini::Key &key, float fallback);
uint32_t yw_GetGemUnlockDuration();
TimedGameplayScaleProfile yw_GetRoboDeathScaleProfile();
World::TAbsoluteOrPercent yw_ParseSprintAbsoluteOrPercent(Common::Ini::Key &key, float maximumPercent);
float yw_ParseSprintExplicitPercent(Common::Ini::Key &key, float maximum);
int32_t yw_ParseSprintTime(Common::Ini::Key &key);
PlayerSprintConfig yw_GetPlayerSprintConfig();
bool yw_IsKamikazeFireHeld(NC_STACK_ypaworld *yw, TInputState *inpt);
void yw_UpdateKamikazeFireLatch(NC_STACK_ypaworld *yw, TInputState *inpt);
bool yw_GetActiveKamikazeFireTimeScale(NC_STACK_ypaworld *yw, float *outScale, float *outHpDrainPerSecond);
float yw_GetActiveGameplayTimeScale(NC_STACK_ypaworld *yw);
void yw_ApplyDebugHostStationResources(NC_STACK_ypaworld *yw);
void yw_UpdateKamikazeFireTimeScaleHpDrain(NC_STACK_ypaworld *yw, int32_t frameTime);
int yw_SelectMimicVehicleID(const std::vector<World::TVhclProto> &protos, int shellVehicleId);
mat3x3 yw_BuildVPRotationMatrix(const vec3d &degrees);
vec3d yw_SafeVPScale(const vec3d &scale);
void yw_AddModelPoint(yw_ModelPointCloud *cloud, const vec3d &point);
vec3d yw_Cross3(const vec3d &a, const vec3d &b);
bool yw_IsFinitePoint(const vec3d &point);
void yw_AddAttachedFXTriangle(NC_STACK_ypaworld::TAttachedFXGeometryCache *cache, yw_ModelPointCloud *bounds, const vec3d &a, const vec3d &b, const vec3d &c);
void yw_CollectAttachedFXTriangles(NC_STACK_base *base, const mat3x3 &rot, const vec3d &pos, NC_STACK_ypaworld::TAttachedFXGeometryCache *cache, yw_ModelPointCloud *bounds);
int yw_GetWeldedVertexID(const vec3d &point, double weld, std::map<std::tuple<int64_t, int64_t, int64_t>, int> *vertices);
bool yw_IsClosedAttachedFXMesh(const NC_STACK_ypaworld::TAttachedFXGeometryCache &cache, const yw_ModelPointCloud &bounds);
bool yw_RayIntersectsAttachedFXTriangle(const vec3d &origin, const vec3d &direction, const NC_STACK_ypaworld::TAttachedFXTriangle &triangle);
bool yw_IsPointInsideAttachedFXMesh(const vec3d &point, const NC_STACK_ypaworld::TAttachedFXGeometryCache &cache);
float yw_NextAttachedFXCacheRandom(uint32_t *state);
void yw_BuildAttachedFXVolumeCache(NC_STACK_ypaworld::TAttachedFXGeometryCache *cache, const yw_ModelPointCloud &bounds);
bool yw_SameAttachedFXTransform(const vec3d &a, const vec3d &b);
std::string yw_TrimConfigValue(std::string s);
bool yw_ParseOptionalInt(std::string s, int32_t *out);
bool yw_ParseOptionalBool(std::string s, bool *out);
int32_t yw_ClampRenderSectors(int32_t sectors);
int32_t yw_RenderSectorsToNormalVizLimit(int32_t sectors);
void yw_ApplyNucleusViewDistanceOverrides(NC_STACK_ypaworld *yw);
bool yw_IsUsableGameplayName(const std::string &name);
std::string yw_SuperItemProfilePathName(const std::string &path);
int yw_InitSceneRecorder(NC_STACK_ypaworld *yw);
void yw_setInitScriptLoc(NC_STACK_ypaworld *yw);
void sub_445230(NC_STACK_ypaworld *yw);
void yw_UpdateUfoSpyUiToggle(NC_STACK_ypaworld *yw, TInputState *inpt);
void yw_UpdateHideVehicleToggle(NC_STACK_ypaworld *yw, TInputState *inpt);
bool yw_VehicleReferencesWeapon(const World::TVhclProto &vehicle, int32_t weaponId);
bool yw_PlayerSprintAcceleratorPressed(const NC_STACK_ypabact *bact, const TInputState *inpt);
bool yw_PlayerSprintCanRun(const NC_STACK_ypabact *bact, const TInputState *inpt);
void yw_ClampTankSprintExcessSpeed(NC_STACK_ypabact *bact, float sprintFactor, const World::TAbsoluteOrPercent &forceUp);
bool yw_IsAmbientAudioFile(const std::string &name);
std::string yw_NormalizeAmbientPath(const std::string &path);
std::vector<std::string> yw_GetAmbientSoundCandidates(const std::string &configuredPath);
void sub_47C1EC(NC_STACK_ypaworld *yw, TMapGem *gemProt, int *a3, int *a4);
void sub_47C29C(NC_STACK_ypaworld *yw, cellArea *cell, int a3);
void ypaworld_func129__sub1(NC_STACK_ypaworld *yw, cellArea *cell, int a3);
void sub_44FD6C(NC_STACK_ypaworld *yw, const cellArea &cell, int bldX, int bldY);
void ypaworld_func129__sub0(NC_STACK_ypaworld *yw, const cellArea &cell, yw_arg129 *arg);
NC_STACK_bitmap *yw_LoadDatabaseBackground();
void sb_0x4e75e8__sub1(NC_STACK_ypaworld *yw, int mode);
void sb_0x4e75e8__sub0(NC_STACK_ypaworld *yw);
void draw_tooltip(NC_STACK_ypaworld *yw, const SDL_Color *uiAccent);
void sub_4476AC(NC_STACK_ypaworld *yw);
int ypaworld_func183__sub0(int lvlID, const char *userName);

#endif // YW_IMPL_H_INCLUDED
