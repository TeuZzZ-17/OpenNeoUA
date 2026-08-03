#ifndef WORLD_ENERGYFX_H_INCLUDED
#define WORLD_ENERGYFX_H_INCLUDED

#include "protos.h"

namespace World
{
namespace EnergyFX
{

// Global, opt-in visual profile used while a unit is in the same regen/drain
// state that drives the corresponding automatic Status Icon.
struct Config
{
    int16_t vp = 0;
    float vp_scale = 1.0f;
    vec3d vp_spin = vec3d(0.0, 0.0, 0.0);
    TVisualTint vp_tint;
    int duration = 1000;
    int interval_min = 0;
    int interval_max = 0;
    int count_min = 0;
    int count_max = 0;
    float random_offset_percent = 25.0f;

    bool IsEnabled() const
    {
        return vp > 0 &&
               interval_min > 0 && interval_max > 0 &&
               count_min > 0 && count_max > 0;
    }
};

// Read and validate the global gfx.regen_decoration_fx_* and
// gfx.drain_decoration_fx_* profiles after Nucleus.ini has been parsed.
// Idempotent and safe to call again after a configuration reload.
void Init();

const Config &Regen();
const Config &Drain();

}
}

#endif // WORLD_ENERGYFX_H_INCLUDED
