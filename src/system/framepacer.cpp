// Frame pacing for the main loop: frame-rate limit and measured FPS.
#include <thread>
#include <SDL2/SDL.h>
#include "framepacer.h"

#ifdef _WIN32
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

namespace System
{

FramePacer Frames;

namespace
{

const int FrameRateLimits[] = {60, 75, 90, 120, 144, 165, 200, 240};
const int FrameRateLimitCount = sizeof(FrameRateLimits) / sizeof(FrameRateLimits[0]);

// Sleeps roughly `ms` milliseconds and returns the precision the OS gives, so
// the caller knows how early it must wake up to spin out the remainder.
#ifdef _WIN32
// Windows 10 1803+ offers a waitable timer with ~0.5 ms precision. Older
// systems fall back to SDL_Delay (1 ms timer resolution set by SDL).
HANDLE HighResolutionTimer()
{
    static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr,
                                                 CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                 TIMER_ALL_ACCESS);
    return timer;
}

double CoarseSleep(double ms)
{
    if ( HANDLE timer = HighResolutionTimer() )
    {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)(ms * 10000.0); // Negative = relative, in 100 ns units.
        if ( SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) )
        {
            WaitForSingleObject(timer, INFINITE);
            return 0.5;
        }
    }

    SDL_Delay((Uint32)ms);
    return 1.5;
}
#else
double CoarseSleep(double ms)
{
    SDL_Delay((Uint32)ms);
    return 1.5;
}
#endif

} // namespace

int NormalizeFrameRateLimit(int value)
{
    for (int limit : FrameRateLimits)
    {
        if (value == limit)
            return value;
    }

    return 60;
}

int CycleFrameRateLimit(int value)
{
    value = NormalizeFrameRateLimit(value);

    for (int i = 0; i < FrameRateLimitCount; i++)
    {
        if (FrameRateLimits[i] == value)
            return FrameRateLimits[(i + 1) % FrameRateLimitCount];
    }

    return 60;
}

void FramePacer::SetFrameRateLimit(int fps)
{
    _period = (fps > 0 && fps <= 1000) ? SDL_GetPerformanceFrequency() / (uint64_t)fps : 0;
    _deadline = 0;
}

void FramePacer::SleepUntil(uint64_t counter) const
{
    const double ticksPerMs = (double)SDL_GetPerformanceFrequency() / 1000.0;
    double margin = 1.5;

    while ( true )
    {
        const int64_t remaining = (int64_t)(counter - SDL_GetPerformanceCounter());
        if ( remaining <= 0 )
            return;

        const double ms = (double)remaining / ticksPerMs;
        if ( ms > margin + 0.25 )
            margin = CoarseSleep(ms - margin);
        else
            std::this_thread::yield(); // Last fraction of a millisecond: spin politely.
    }
}

void FramePacer::EndFrame()
{
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    uint64_t now = SDL_GetPerformanceCounter();

    if ( _period )
    {
        // Resynchronize on the first frame and after a stall longer than one
        // frame; otherwise keep the fixed cadence.
        if ( _deadline == 0 || (int64_t)(now - _deadline) > (int64_t)_period )
            _deadline = now;
        else
            SleepUntil(_deadline);

        _deadline += _period;
        now = SDL_GetPerformanceCounter();
    }

    if ( _fpsWindowStart == 0 )
        _fpsWindowStart = now;

    _fpsWindowFrames++;
    const uint64_t elapsed = now - _fpsWindowStart;
    if ( elapsed >= frequency / 2 )
    {
        _measuredFps = (int)(((uint64_t)_fpsWindowFrames * frequency + elapsed / 2) / elapsed);
        _fpsWindowFrames = 0;
        _fpsWindowStart = now;
    }
}

} // namespace System
