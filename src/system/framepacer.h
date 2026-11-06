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

    // The limit last given to SetFrameRateLimit (0 = unlimited).
    int FrameRateLimit() const { return _limitFps; }

    // Frame-time statistics over the last FrameHistory frames (debug overlay).
    struct FrameStats
    {
        int frames = 0;          // Samples used; 0 = nothing measured yet.
        double lastMs = 0.0;     // Most recent frame time.
        double avgMs = 0.0;      // Mean frame time.
        double worstMs = 0.0;    // Slowest frame.
        double low1PercentMs = 0.0; // Mean of the slowest 1% of frames ("1% low").
    };
    FrameStats GetFrameStats() const;

private:
    void SleepUntil(uint64_t counter) const;

    uint64_t _period = 0;        // Performance-counter ticks per frame; 0 = no limit.
    uint64_t _deadline = 0;      // Counter value at which the next frame may start.
    uint64_t _fpsWindowStart = 0;
    uint64_t _lastFrameEnd = 0;  // Counter value at the previous EndFrame.
    int _limitFps = 0;
    static const int FrameHistory = 240;
    float _frameMs[FrameHistory] = {};
    int _frameMsCount = 0;       // Samples stored (up to FrameHistory).
    int _frameMsNext = 0;        // Ring-buffer write position.
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
