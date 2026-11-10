// Shared reading of gfx.ground_decal_limit (maximum active ground decals).
#ifndef WORLD_GROUNDDECAL_LIMIT_H_INCLUDED
#define WORLD_GROUNDDECAL_LIMIT_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <limits>

#include "../system/inivals.h"

namespace World
{

// 0 disables the decals; a negative value falls back to 256; the maximum is 10000.
// "no_limit" keeps every decal (stored as INT32_MAX).
inline int GroundDecalLimit()
{
    int value = System::IniConf::GfxGroundDecalLimit.Get<int32_t>();
    if ( value == std::numeric_limits<int32_t>::max() )
        return value;
    if ( value < 0 )
        value = 256;

    return std::min(value, 10000);
}

}

#endif // WORLD_GROUNDDECAL_LIMIT_H_INCLUDED
