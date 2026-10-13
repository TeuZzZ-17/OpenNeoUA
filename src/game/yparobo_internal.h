#ifndef YPAROBO_INTERNAL_H_INCLUDED
#define YPAROBO_INTERNAL_H_INCLUDED

// Internal header of the NC_STACK_yparobo implementation (yparobo*.cpp): helpers that used to be file-static.

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "common/log.h"
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "fmtlib/printf.h"
#include "system/inivals.h"
#include "common/crashdiag.h"
#include "world/tools.h"
#include <math.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>
#include <limits>

// ---- constants shared by the parts ----
static const std::array<Common::Point, 8> NearDxy
{{
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
    { 1,  0}, {-1, 1}, {0,  1}, { 1, 1}
}};
static const int PLAYER_ROBO_RESOURCE_TREND_WINDOW_TIME = 500;
static const int PLAYER_ROBO_RESOURCE_TREND_HOLD_TIME = 600;
static const int PLAYER_ROBO_RESOURCE_TREND_THRESHOLD = 2;

// ---- globals defined in the core file ----
extern int dword_5B1128;
extern robo_t2 stru_5B0628[100];
extern int dword_515138[8];

// ---- functions that used to be file-static ----
int ScalePlayerMobileMoveSecondaryCost(int cost, float mult);
float yparobo_ReadMobileEnergyCostMultiplier(Common::Ini::Key &key, float fallback);
float ScalePlayerMobileMovePitchEffect(float speedPitchScale);
int PlayerMobileMoveScaledPitch(TSoundSource &snd, int basePitch, float speedPitchScale);
bool yparobo_IsActiveProducedVehicle(NC_STACK_ypabact *unit);
int yparobo_CountActiveProducedVehicleRecursive(NC_STACK_ypabact *unit, int owner, int vehicleId, std::unordered_set<NC_STACK_ypabact *> &visited);
int yparobo_CountActiveProducedVehicle(NC_STACK_ypaworld *world, int owner, int vehicleId);
NC_STACK_ypabact *yparobo_TryCreateGenesisUnit(NC_STACK_yparobo *robo, ypaworld_arg146 *arg);
int yparobo_GetVehicleProductionCost(NC_STACK_ypaworld *world, int vehicleId);
int yparobo_GetUnitProductionCost(NC_STACK_ypabact *unit);
void yparobo_AdvanceMimicProductionCost(NC_STACK_ypaworld *world, int vehicleId);

#endif // YPAROBO_INTERNAL_H_INCLUDED
