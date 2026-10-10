// Standalone regression test: compile with system/gametime.cpp (no game assets needed).
#include "../../system/gametime.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check(bool ok, const char *name)
{
    if (!ok) { ++failures; std::printf("FAIL: %s\n", name); }
}

int main()
{
    System::GameplayClock clock;
    clock.Reset(0);
    check(clock.BeginFrame(1000, 50, 0, 1.0f, false, false) == 50, "vanilla speed");
    check(clock.BeginFrame(1050, 50, 50, 2.0f, false, true) == 100, "fast 200 percent");
    check(clock.BeginFrame(1100, 50, 150, 0.2f, false, true) == 10, "slow 20 percent");
    check(clock.BeginFrame(1150, 50, 160, 2.0f, true, true) == 0, "freeze overrides fast");
    check(clock.BeginFrame(1200, 50, 160, 1.0f, false, false) == 50, "resume at normal speed");
    clock.Reset(0);
    int total = 0;
    for (int n=0; n<10; ++n)
        total += clock.BeginFrame(1000+n, 1, clock.Time(), 2.0f, false, true);
    check(total == 20, "accumulation at 200 percent");
    clock.Reset(0);
    check(clock.BeginFrame(1000, 50, 0, 20.0f, false, true) == 100, "debug-safe upper bound");
    std::printf("debug_time_scale_test: %d failures\n", failures);
    return failures ? 1 : 0;
}
