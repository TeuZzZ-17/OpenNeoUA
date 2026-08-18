#ifndef WORLD_ENERGYFX_H_INCLUDED
#define WORLD_ENERGYFX_H_INCLUDED

#include "protos.h"

namespace World
{
namespace EnergyFX
{

// Global visual profile used while a unit is in the same regen/drain state
// that drives the corresponding automatic Status Icon.
//
// No public mode selector is needed: vp > 0 uses the existing VP path;
// vp <= 0 uses the procedural +/- mesh path.
struct Config
{
    int16_t vp = 0;
    float vp_scale = 1.0f;
    vec3d vp_spin = vec3d(0.0, 0.0, 0.0);
    TVisualTint tint;

    int duration = 1000;
    int interval_min = 0;
    int interval_max = 0;
    int count_min = 0;
    int count_max = 0;
    float random_offset_percent = 25.0f;

    float size = 30.0f;
    float thickness = 5.0f;
    float rise_speed = 100.0f;
    int fade_in = 150;
    int fade_out = 300;

    bool IsProcedural() const
    {
        return vp <= 0;
    }

    bool IsEnabled() const
    {
        if ( interval_min <= 0 || interval_max <= 0 ||
             count_min <= 0 || count_max <= 0 )
            return false;

        if ( IsProcedural() )
            return size > 0.0f && thickness > 0.0f;

        return vp > 0;
    }
};

// Read and validate the global gfx.regen_fx_*/gfx.regen_mesh_* and
// gfx.drain_fx_*/gfx.drain_mesh_* profiles after Nucleus.ini has been parsed.
// Idempotent and safe to call again after a configuration reload.
void Init();

const Config &Regen();
const Config &Drain();

}
}

#endif // WORLD_ENERGYFX_H_INCLUDED
