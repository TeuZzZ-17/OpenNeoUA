#include <algorithm>
#include <cmath>
#include "gametime.h"

namespace System
{

GameplayClock GameClock;

void GameplayClock::Reset(int32_t gameTime)
{
    _active = false;
    _frozen = false;
    _forceAllTimeScaled = false;
    _realTime = 0;
    _realDelta = 0;
    _gameTime = gameTime;
    _gameDelta = 0;
    _visualTime = 0;
    _scale = 1.0f;
    _deltaRemainder = 0.0;
}

int32_t GameplayClock::BeginFrame(int32_t realTime, int32_t realDelta,
                                  int32_t currentGameTime, float scale,
                                  bool frozen, bool forceAllTimeScaled)
{
    _realTime = realTime;
    _realDelta = std::max(realDelta, 0);
    _frozen = frozen;
    _forceAllTimeScaled = forceAllTimeScaled;

    if ( !std::isfinite(scale) || scale <= 0.0f )
        scale = 1.0f;
    _scale = std::max(0.05f, std::min(scale, 2.0f));

    // Re-anchor only when entering the gameplay domain or when a load/seek
    // changed the canonical world timestamp behind the clock.
    const bool reanchor = !_active || currentGameTime != _gameTime;
    if ( reanchor )
    {
        _gameTime = currentGameTime;
        _visualTime = realTime;
        _deltaRemainder = 0.0;
        _active = true;
    }

    if ( frozen )
    {
        _gameDelta = 0;
        _deltaRemainder = 0.0;
        return 0;
    }

    double scaledExact = (double)_realDelta * (double)_scale + _deltaRemainder;
    // Float constants such as 1.8f can land just below an exact integer tick.
    // Snap only near-integers so 50ms at 1.8x is 90ms, not 89ms.
    const double nearestTick = std::round(scaledExact);
    if ( std::fabs(scaledExact - nearestTick) < 0.00001 )
        scaledExact = nearestTick;
    _gameDelta = (int32_t)std::floor(scaledExact);
    _deltaRemainder = scaledExact - (double)_gameDelta;

    // Running gameplay keeps the legacy minimum one-tick update.
    if ( _gameDelta < 1 && _realDelta > 0 )
    {
        _gameDelta = 1;
        _deltaRemainder = 0.0;
    }

    _gameTime += _gameDelta;
    if ( !reanchor )
        _visualTime += _gameDelta;
    return _gameDelta;
}

} // namespace System
