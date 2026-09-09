#ifndef SYSTEM_GAMETIME_H_INCLUDED
#define SYSTEM_GAMETIME_H_INCLUDED

#include <stdint.h>

namespace System
{

// OpenNeoUA master in-game clock.  Platform/input/network time remains real,
// but every local gameplay/render/audio time consumer can read this published
// domain instead of inventing its own scaling path.
class GameplayClock
{
public:
    void Reset(int32_t gameTime = 0);

    // Advances one local world frame and returns the canonical scaled delta.
    int32_t BeginFrame(int32_t realTime, int32_t realDelta,
                       int32_t currentGameTime, float scale,
                       bool frozen, bool forceAllTimeScaled);
    void SyncGameTime(int32_t gameTime) { _gameTime = gameTime; }

    bool IsActive() const { return _active; }
    bool IsFrozen() const { return _frozen; }
    bool ForceAllTimeScaled() const { return _forceAllTimeScaled; }

    int32_t RealTime() const { return _realTime; }
    int32_t RealDelta() const { return _realDelta; }
    int32_t Time() const { return _gameTime; }
    int32_t Delta() const { return _gameDelta; }
    int32_t VisualTime() const { return _visualTime; }
    float Scale() const { return _scale; }

private:
    bool _active = false;
    bool _frozen = false;
    bool _forceAllTimeScaled = false;
    int32_t _realTime = 0;
    int32_t _realDelta = 0;
    int32_t _gameTime = 0;
    int32_t _gameDelta = 0;
    int32_t _visualTime = 0;
    float _scale = 1.0f;
    double _deltaRemainder = 0.0;
};

extern GameplayClock GameClock;

} // namespace System

#endif
