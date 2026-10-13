#ifndef YPAMISSILE_INTERNAL_H_INCLUDED
#define YPAMISSILE_INTERNAL_H_INCLUDED

// Internal header of the NC_STACK_ypamissile implementation (ypamissile*.cpp): helpers that used to be file-static.

#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "game/yw.h"
#include "game/ypamissile.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "system/inivals.h"
#include "common/log.h"
#include <algorithm>
#include <math.h>

// ---- functions that used to be file-static ----
float ypamissile_Clamp01(float value);
bool ypamissile_WeaponTracerFinite(const vec3d &value);
bool ypamissile_WeaponTracerFinite(const World::TVisualTint &tint);
bool ypamissile_IsAliveForDeathPush(NC_STACK_ypabact *target);
NC_STACK_ypabact *ypamissile_ResolveDirectPushRecipient(NC_STACK_ypabact *target);
float ypamissile_GetPushAtDeathMultiplier();
float ypamissile_GetTargetPushMultiplier(NC_STACK_ypabact *target);
float ypamissile_SegmentSegmentDistanceSq(const vec3d &p1, const vec3d &q1, const vec3d &p2, const vec3d &q2);
bool ypamissile_GetCompoundImpactPoint(const vec3d &oldWeaponCenter, const vec3d &newWeaponCenter, float weaponRadius, const vec3d &targetCenter, float targetRadius, vec3d *impactPosition, float *travelFraction);
float ypamissile_BlastFalloffFactor(float distance, float radius, bool falloff);
int ypamissile_ScaleBlastEnergy(int energy, float factor);
bool ypamissile_NormalizeXZ(vec3d *dir);
bool ypamissile_GetDirectPushDir(NC_STACK_ypamissile *missile, NC_STACK_ypabact *target, const vec3d &fallbackDir, vec3d *outDir);
NC_STACK_ypabact *ypamissile_FindLiveBactByGid(World::RefBactList &list, int32_t gid);
bool ypamissile_IsHomingBombProto(const World::TWeapProto &wproto);
bool ypamissile_IsHomingBomb(NC_STACK_ypamissile *missile);
bool ypamissile_HasHomingBombTarget(NC_STACK_ypamissile *missile);
bool ypamissile_IsUsableSuicideReturn(NC_STACK_ypabact *candidate);

#endif // YPAMISSILE_INTERNAL_H_INCLUDED
