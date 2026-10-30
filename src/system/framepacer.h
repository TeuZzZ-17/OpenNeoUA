// Frame pacing for the main loop: frame-rate limit and measured FPS.
#ifndef SYSTEM_FRAMEPACER_H_INCLUDED
#define SYSTEM_FRAMEPACER_H_INCLUDED

#include <stdint.h>

namespace System
{

// Holds the main loop to the gfx.maxfps cadence with sub-millisecond
// precision. Deadlines advance by a fixed period, so the average rate is exact
// (240 means 240, not 250 as with whole-millisecond delays). A frame that
// arrives late is not "repaid" with a burst of fast frames.
class FramePacer
{
public:
    // Frames per second; 0 or anything above 1000 removes the limit.
    void SetFrameRateLimit(int fps);

    // Called once per frame after rendering. Sleeps until the next deadline
    // and updates the FPS measurement.
    void EndFrame();

    // Frames per second averaged over the last half second.
    int MeasuredFps() const { return _measuredFps; }

private:
    void SleepUntil(uint64_t counter) const;

    uint64_t _period = 0;        // Performance-counter ticks per frame; 0 = no limit.
    uint64_t _deadline = 0;      // Counter value at which the next frame may start.
    uint64_t _fpsWindowStart = 0;
    int _fpsWindowFrames = 0;
    int _measuredFps = 0;
};

extern FramePacer Frames;

// FPS limits offered in Options (gfx.maxfps). Anything else becomes 60.
int NormalizeFrameRateLimit(int value);
// Next limit of the Options list, wrapping from the last to the first.
int CycleFrameRateLimit(int value);

} // namespace System

#endif
