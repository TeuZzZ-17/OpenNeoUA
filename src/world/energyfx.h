#ifndef WORLD_ENERGYFX_H_INCLUDED
#define WORLD_ENERGYFX_H_INCLUDED

#include "protos.h"

namespace World
{
namespace EnergyFX
{

enum Mode
{
    MODE_VP = 0,
    MODE_PROCEDURAL = 1
};

// Global, opt-in visual profile used while a unit is in the same regen/drain
// state that drives the corresponding automatic Status Icon.
struct Config
{
    uint8_t mode = MODE_VP;

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

    // Procedural alternative to the VP representation. It deliberately reuses
    // duration, interval/count, random offset and vp_tint from the same profile.
    float procedural_size = 30.0f;
    float procedural_thickness = 5.0f;
    float procedural_rise_speed = 100.0f;
    int procedural_fade_in = 150;
    int procedural_fade_out = 300;

    bool IsProcedural() const
    {
        return mode == MODE_PROCEDURAL;
    }

    bool IsEnabled() const
    {
        if ( interval_min <= 0 || interval_max <= 0 ||
             count_min <= 0 || count_max <= 0 )
            return false;

        if ( IsProcedural() )
            return procedural_size > 0.0f && procedural_thickness > 0.0f;

        return vp > 0;
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
