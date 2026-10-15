// Reusable ground decal profiles (new_decal <id>) and their per-unit resolution.
#ifndef WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
#define WORLD_GROUNDDECAL_PROFILE_H_INCLUDED

#include <string>

#include "protos.h"

class NC_STACK_base;

namespace World
{

// One new_decal block. `fx` is the decal template (texture, stretch, tint, fades,
// smoke, size_by_model_%). The decal size itself is measured when the decal spawns,
// from the visual model of the unit that spawns it (see GroundDecalModelDiameter).
struct TGroundDecalProfile
{
    bool valid = false;
    int32_t profile_id = -1;
    std::string name;
    TChainFXConfig fx;
};

// Diameter of the bounding sphere of a visual model, with the unit's visual scale
// applied. The sphere is centered on the middle of the model's bounding box, so it
// does not depend on the model rotation. Returns 0 when the model has no geometry.
float GroundDecalModelDiameter(NC_STACK_base *model, const vec3d &scale);

// Builds the ground decal Chain FX one vehicle/weapon spawns, from its decal profile.
// `trigger` is decided by the user of the profile: destroyed for vehicles,
// impact_world for weapons and MGUN hits. A size override written in the unit's own
// script replaces the profile's size_by_model_%.
inline TChainFXConfig ResolveGroundDecal(const TGroundDecalProfile &profile,
                                         TChainFXConfig::Trigger trigger,
                                         const TDecalSizeOverride &sizeOverride = TDecalSizeOverride())
{
    TChainFXConfig chain = profile.fx;
    chain.mode = TChainFXConfig::MODE_GROUND_DECAL;
    chain.trigger = trigger;
    if ( sizeOverride.defined )
    {
        chain.ground_decal_model_percent_min = sizeOverride.percent_min;
        chain.ground_decal_model_percent_max = sizeOverride.percent_max;
    }
    return chain;
}

}

#endif // WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
