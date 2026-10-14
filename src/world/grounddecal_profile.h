// Reusable ground decal profiles (new_decal <id>) and their per-unit resolution.
#ifndef WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
#define WORLD_GROUNDDECAL_PROFILE_H_INCLUDED

#include <algorithm>
#include <cmath>
#include <string>

#include "protos.h"

namespace World
{

// One new_decal block. `fx` is the decal template (texture, stretch, tint, fades,
// smoke); the size is filled in for every vehicle/weapon by ResolveGroundDecal(),
// because it can depend on that unit's explosion or silhouette.
struct TGroundDecalProfile
{
    bool valid = false;
    int32_t profile_id = -1;
    std::string name;
    TChainFXConfig fx;

    // size = min_max is an absolute size in world units and wins over the rules below.
    bool has_absolute_size = false;
    float size_min = 0.0f;
    float size_max = 0.0f;

    // Percentage ranges of a diameter (keys size_explosion and size_silhouette) and the
    // smallest decal the percentage rules may produce (key size_min).
    float explosion_percent_min = 70.0f;
    float explosion_percent_max = 80.0f;
    float silhouette_percent_min = 150.0f;
    float silhouette_percent_max = 200.0f;
    float floor_size = 30.0f;
};

// Decal size for a unit with the given silhouette and explosion diameters (0 = none).
// An absolute size is returned as written. Otherwise the explosion diameter is the
// base; a unit without explosion falls back to its silhouette so every decal stays
// visible. The floor keeps tiny projectiles from producing invisible decals.
inline void ResolveGroundDecalSize(const TGroundDecalProfile &profile,
                                   float silhouetteDiameter, float explosionDiameter,
                                   float *outMin, float *outMax)
{
    if ( profile.has_absolute_size )
    {
        *outMin = profile.size_min;
        *outMax = profile.size_max;
        return;
    }

    const bool useExplosion = std::isfinite(explosionDiameter) && explosionDiameter > 0.0f;
    const float base = useExplosion ? explosionDiameter
                                    : (std::isfinite(silhouetteDiameter) && silhouetteDiameter > 0.0f
                                           ? silhouetteDiameter : 0.0f);
    const float lowPercent = useExplosion ? profile.explosion_percent_min : profile.silhouette_percent_min;
    const float highPercent = useExplosion ? profile.explosion_percent_max : profile.silhouette_percent_max;

    *outMin = std::max(profile.floor_size, base * lowPercent * 0.01f);
    *outMax = std::max(profile.floor_size, base * highPercent * 0.01f);
}

// Builds the ground decal Chain FX one vehicle/weapon spawns, from its decal profile.
// `trigger` is decided by the user of the profile: destroyed for vehicles,
// impact_world for weapons and MGUN hits.
inline TChainFXConfig ResolveGroundDecal(const TGroundDecalProfile &profile,
                                         TChainFXConfig::Trigger trigger,
                                         float silhouetteDiameter, float explosionDiameter)
{
    TChainFXConfig chain = profile.fx;
    chain.mode = TChainFXConfig::MODE_GROUND_DECAL;
    chain.trigger = trigger;

    float sizeMin = 0.0f;
    float sizeMax = 0.0f;
    ResolveGroundDecalSize(profile, silhouetteDiameter, explosionDiameter, &sizeMin, &sizeMax);
    chain.ground_decal_size = sizeMin;
    chain.ground_decal_size_min = sizeMin;
    chain.ground_decal_size_max = sizeMax;
    return chain;
}

}

#endif // WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
