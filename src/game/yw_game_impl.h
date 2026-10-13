#ifndef YW_GAME_IMPL_H_INCLUDED
#define YW_GAME_IMPL_H_INCLUDED

// Internal header of the ypaworld game-logic files (yw_game*.cpp).
// Not part of the public interface: helpers that used to be file-static and are shared between the files.
// The public interface stays in yw.h.

#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include "nucleus/loaders.h"
#include "game/yparobo.h"
#include "gui/font.h"
#include "gui/uacommon.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "world/spin.h"
#include "common/crashdiag.h"

// ---- constants shared by the parts ----
static constexpr int kMapBorderWallFirstLego = 210;
static constexpr int kMapBorderWallLastLego = 213;
static constexpr float kMapBorderWallVanillaHeight = 2700.0f;

// ---- globals defined in the core file ----
extern uint32_t bact_id;
extern TRenderingSector rendering_sectors[YW_RENDER_SECTORS_MAX + 1][ YW_RENDER_SECTORS_MAX + 1];

// ---- functions that used to be file-static ----
NC_STACK_bitmap * loadDisk_screen(NC_STACK_ypaworld *yw);
void draw_splashScreen(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen);
void drawSplashScreenWithTOD(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen, const std::string &text);
void sb_0x44ca90__sub2(NC_STACK_ypaworld *yw, TLevelDescription *mapp);
bool yw_IsBactInSpectatorFollowTree(NC_STACK_ypabact *node, NC_STACK_ypabact *target);
bool yw_IsUsableSpectatorBact(NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit);
NC_STACK_ypabact *yw_FindSpectatorBactInList(NC_STACK_ypaworld *yw, World::RefBactList &list);
void yw_ApplySpectatorObserverSafety(NC_STACK_ypabact *spectator);
float yw_ClampSpectatorCoord(float value, float minValue, float maxValue);
vec3d yw_GetSpectatorSpawnPosition(NC_STACK_ypaworld *yw);
void sub_44DF60(UAskeleton::Data *arg, int id);
int sub_44D36C(const vec3d &v, int id, NC_STACK_skeleton *skeleton);
void yw_ConfigureTransientVPFade(NC_STACK_ypaworld::TTransientVP &effect, int fadeIn, int fadeOut, double duration, double elapsed = -1.0);
int yw_RandomInRange(int minValue, int maxValue);
float yw_RandomFloatSigned(float magnitude);
bool yw_IsAtmosphericFXInsideVolume(const NC_STACK_ypaworld::TTransientVP &fx, const vec3d &viewerPos, const World::TAtmosphericFXProfile &profile);
bool yw_IsInsideSuperItemWaveXZ(const vec3d &pos, const vec3d &waveCenter, float waveRadius);
NC_STACK_ypabact *yw_FindLiveMissileByGidInList(World::MissileList &list, int32_t gid);
NC_STACK_ypabact *yw_FindLiveBactByGidInList(World::RefBactList &list, int32_t gid);
bool yw_IsAliveBuildingSpawnedUnit(NC_STACK_ypabact *unit);
int yw_CountBuildingSpawnedUnits(NC_STACK_ypaworld *world, cellArea &cell);
bool yw_IsBuildingSpawnEnemy(uint8_t owner, NC_STACK_ypabact *unit);
bool yw_IsActiveBuildingSpawnerCell(cellArea &cell);
bool yw_BuildingHasEnemyNearbyInUnitTree(uint8_t owner, const vec3d &centerPos, float radiusSq, NC_STACK_ypabact *unit);
bool yw_BuildingHasEnemyNearby(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto);
bool yw_IsBuildingSpawnCandidateCell(cellArea &buildingCell, cellArea *spawnCell, const char **failReason);
bool yw_AdjustBuildingSpawnHeight(NC_STACK_ypaworld *world, const World::TBuildingProto &proto, vec3d *pos, const char **failReason);
bool yw_IsBuildingSpawnPositionClear(NC_STACK_ypaworld *world, cellArea &buildingCell, const World::TBuildingProto &proto, vec3d *pos, const char **failReason);
void yw_LogBuildingSpawnPositionFailure(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, const char *reason);
NC_STACK_yparobo *yw_FindBuildingSpawnOwnerRobo(NC_STACK_ypaworld *world, uint8_t owner);
void yw_LogBuildingSpawnOwnerFailure(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto);
bool yw_FindBuildingSpawnPosition(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, vec3d *outPos);
NC_STACK_ypabact *yw_CreateBuildingSpawnedUnit(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto, const vec3d &pos);
void yw_UpdateBuildingSpawner(NC_STACK_ypaworld *world, cellArea &cell, const World::TBuildingProto &proto);
mat3x3 yw_BuildTransientVPRotationMatrix(const vec3d &degrees);
bool yw_HasTransientVPRotation(const vec3d &rotation);
void yw_RenderTransientVPs(NC_STACK_ypaworld *world, std::list<NC_STACK_ypaworld::TTransientVP> *effects, baseRender_msg *arg);
bool yw_IsBuildingTerrainCollisionPolygon(const UAskeleton::Data *skel, const UAskeleton::Polygon &poly, float terrainHalfExtent);
bool yw_PointCoveredByBuildingCollision(NC_STACK_skeleton *collision, const vec3d &localPoint, float terrainHalfExtent);
int ypaworld_func137__sub0__sub0(UAskeleton::Data *skl, int id, const vec3d &v, float r, vec3d &out);
void ypaworld_func64__sub6(NC_STACK_ypaworld *yw);
bool yw_IsValidMobilePowerGenerator(NC_STACK_ypaworld *yw, NC_STACK_ypabact *unit);
vec3d yw_RandomPointInDisk(float radius);
void yw_AddMobilePowerInfluenceFromGenerator(NC_STACK_ypaworld *yw, NC_STACK_ypabact *target, NC_STACK_ypabact *generator, TMobilePowerInfluence &influence);
bool yw_IsCustomSuperItemPushTarget(const NC_STACK_ypabact *target);
double yw_SuperItemMapRadius(const vec2d &mapLength);
double yw_SuperItemWaveRadiusAtElapsed(const World::TSuperItemProfile &profile, double elapsedMilliseconds);
double yw_SuperItemWaveTravelTimeMs(const World::TSuperItemProfile &profile, double radius);
int ypaworld_func64__sub4(NC_STACK_ypaworld *yw, base_64arg *arg);
void ypaworld_func64__sub2(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub9(NC_STACK_ypaworld *yw);
void ypaworld_func64__sub19__sub3(NC_STACK_ypaworld *yw, int id);
uint32_t yw_SuperItemBuildingHash(const World::TSuperItemProfile &profile, const TMapSuperItem &sitem, const cellArea &cell, int bldX, int bldY);
void ypaworld_func64__sub3(NC_STACK_ypaworld *yw);
int recorder_startrec(NC_STACK_ypaworld *yw);
void recorder_stoprec(NC_STACK_ypaworld *yw);
void sb_0x447720(NC_STACK_ypaworld *yw, TInputState *inpt);
void recorder_update_time(NC_STACK_ypaworld *yw, int dtime);
void recorder_pack_soundstates(TGameRecorder *rcrd);
void recorder_unpack_soundstates(TGameRecorder *rcrd);
int recorder_open_replay(TGameRecorder *rcrd);
void recorder_read_framedata(TGameRecorder *rcrd);
void ypaworld_func163__sub2__sub1(NC_STACK_ypaworld *yw, float fperiod, TInputState *inpt);
void ypaworld_func163__sub2__sub0(NC_STACK_ypaworld *yw, float fperiod, TInputState *inpt);
void sub_445654(NC_STACK_ypaworld *yw, CmdStream *in, char *buf, const char *fmt, ...);
bool yw_DebugIsLiveBact(NC_STACK_ypabact *unit);
bool yw_DebugIsCurrentControlledBact(NC_STACK_ypaworld *world, NC_STACK_ypabact *unit);

#endif // YW_GAME_IMPL_H_INCLUDED
