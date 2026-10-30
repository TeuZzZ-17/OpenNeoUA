// NC_STACK_wintimer: timer based on the operating system clock.
#include <inttypes.h>
#include "common/includes.h"
#include "nucleus/nucleas.h"
#include "platform/wintimer.h"
#include "common/utils.h"

// Returns the time since the previous call in game clock units (1024 per
// second). The clock is absolute, so whole units never drift or get lost.
int NC_STACK_wintimer::itimer_func64()
{
    const uint64_t freq = SDL_GetPerformanceFrequency();
    const uint64_t cnt = SDL_GetPerformanceCounter();
    // Split division: exact for any counter frequency, no 64-bit overflow.
    const uint64_t tik = (cnt / freq) * 1024 + (cnt % freq) * 1024 / freq;

    const int64_t period = (int64_t)(tik - ticks);
    ticks = tik;

    // First call, or a stall of about two seconds (level loading): report a
    // single unit instead of a huge jump.
    if ( period < 0 || period > 2000 )
        return 1;

    return (int)period;
}
