// Reusable ground decal profiles (new_decal <id>) and their per-unit resolution.
#ifndef WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
#define WORLD_GROUNDDECAL_PROFILE_H_INCLUDED

#include <string>

#include "protos.h"

class NC_STACK_base;
class NC_STACK_ypaworld;

namespace World
{

// One new_decal block. `fx` is the decal template (texture, stretch, tint, fades,
// smoke and fire). Each source supplies its own manual diameter range.
struct TGroundDecalProfile
{
    bool valid = false;
    int32_t profile_id = -1;
    std::string name;
    TChainFXConfig fx;
};

// Builds the ground decal Chain FX one vehicle/weapon spawns, from its decal profile.
// `trigger` is decided by the user of the profile: destroyed for vehicles,
// impact_world for weapons and MGUN hits. A size override written in the unit's own
// script defines the diameter; a missing size disables the decal.
inline TChainFXConfig ResolveGroundDecal(const TGroundDecalProfile &profile,
                                         TChainFXConfig::Trigger trigger,
                                         const TDecalSizeOverride &sizeOverride = TDecalSizeOverride())
{
    TChainFXConfig chain = profile.fx;
    chain.mode = TChainFXConfig::MODE_GROUND_DECAL;
    chain.trigger = trigger;
    if ( sizeOverride.defined )
    {
        chain.ground_decal_size_min = sizeOverride.size_min;
        chain.ground_decal_size_max = sizeOverride.size_max;
    }
    return chain;
}

}

#endif // WORLD_GROUNDDECAL_PROFILE_H_INCLUDED
