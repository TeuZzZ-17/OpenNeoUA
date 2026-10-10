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
    check(clock.BeginFrame(1050, 50, 50, 1.8f, false, true) == 90, "fast 180 percent");
    check(clock.BeginFrame(1100, 50, 140, 0.2f, false, true) == 10, "slow 20 percent");
    check(clock.BeginFrame(1150, 50, 150, 1.8f, true, true) == 0, "freeze overrides fast");
    check(clock.BeginFrame(1200, 50, 150, 1.0f, false, false) == 50, "resume at normal speed");
    clock.Reset(0);
    int total = 0;
    for (int n=0; n<10; ++n)
        total += clock.BeginFrame(1000+n, 1, clock.Time(), 1.8f, false, true);
    check(total == 18, "fractional accumulation at 180 percent");
    clock.Reset(0);
    check(clock.BeginFrame(1000, 50, 0, 20.0f, false, true) == 90, "debug-safe upper bound");
    std::printf("debug_time_scale_test: %d failures\n", failures);
    return failures ? 1 : 0;
}
