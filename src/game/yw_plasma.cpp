// Plasma currency: balance, HUD values, commits and restores.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"

bool NC_STACK_ypaworld::IsPlasmaCurrencyEnabled() const
{
    return !_isNetGame && System::IniConf::GamePlasmaCurrencyEnable.Get<bool>();
}

uint64_t NC_STACK_ypaworld::GetPlasmaCurrencyHudValue() const
{
    if ( _plasmaCurrencyHudTargetValue <= _plasmaCurrencyHudStartValue )
        return _plasmaCurrencyHudTargetValue;

    const uint32_t elapsed = (uint32_t)(_timeStamp - _plasmaCurrencyHudPulseStartTime);
    if ( elapsed >= PLASMA_CURRENCY_HUD_PULSE_MS )
        return _plasmaCurrencyHudTargetValue;

    double progress = (double)elapsed / (double)PLASMA_CURRENCY_HUD_PULSE_MS;
    progress = progress * progress * (3.0 - 2.0 * progress);

    const uint64_t delta = _plasmaCurrencyHudTargetValue - _plasmaCurrencyHudStartValue;
    const long double interpolated = (long double)delta * (long double)progress;
    return _plasmaCurrencyHudStartValue + (uint64_t)std::floor(interpolated);
}

uint8_t NC_STACK_ypaworld::GetPlasmaCurrencyHudOpacity() const
{
    if ( _plasmaCurrencyHudTargetValue <= _plasmaCurrencyHudStartValue )
        return 255;

    const uint32_t elapsed = (uint32_t)(_timeStamp - _plasmaCurrencyHudPulseStartTime);
    if ( elapsed >= PLASMA_CURRENCY_HUD_PULSE_MS )
        return 255;

    const double progress = (double)elapsed / (double)PLASMA_CURRENCY_HUD_PULSE_MS;
    return (uint8_t)(180 + (int)std::floor(75.0 * progress));
}

uint64_t NC_STACK_ypaworld::AddPlasmaCurrency(uint64_t amount, const vec3d &worldPos)
{
    if ( amount == 0 || !IsPlasmaCurrencyEnabled() )
        return 0;

    const uint64_t displayedBeforeCredit = GetPlasmaCurrencyHudValue();
    const uint64_t previous = _plasmaCurrency;
    const uint64_t available = std::numeric_limits<uint64_t>::max() - previous;
    const uint64_t delta = std::min(amount, available);
    if ( delta == 0 )
        return 0;

    _plasmaCurrency += delta;
    _plasmaCurrencyHudStartValue = std::min(displayedBeforeCredit, _plasmaCurrency);
    _plasmaCurrencyHudTargetValue = _plasmaCurrency;
    _plasmaCurrencyHudPulseStartTime = _timeStamp;

    if ( _mapLength.x > 0.0f && _mapLength.y > 0.0f )
    {
        int posX = dround(worldPos.x * 255.0f / _mapLength.x);
        int posY = dround(-worldPos.z * 255.0f / _mapLength.y);
        posX = std::max(0, std::min(posX, 255));
        posY = std::max(0, std::min(posY, 255));

        uint8_t owner = 0;
        if ( _userRobo )
            owner = _userRobo->_owner;
        else if ( _userUnit )
            owner = _userUnit->_owner;

        HistoryEventAdd(World::History::Plasma(
            delta, (uint8_t)posX, (uint8_t)posY, owner));
    }

    if ( _plasmaCurrencyPopups.size() >= 32 )
        _plasmaCurrencyPopups.erase(_plasmaCurrencyPopups.begin());

    TPlasmaCurrencyPopup popup;
    popup.worldPos = worldPos;
    popup.amount = delta;
    popup.startTime = _timeStamp;
    _plasmaCurrencyPopups.push_back(popup);
    return delta;
}

uint64_t NC_STACK_ypaworld::ParsePlasmaCurrencyAmount(const std::string &value)
{
    uint64_t amount = 0;
    for ( const char digit : value )
    {
        if ( digit < '0' || digit > '9' ||
             amount > (std::numeric_limits<uint64_t>::max() - (digit - '0')) / 10 )
            return 0;
        amount = amount * 10 + (digit - '0');
    }
    return amount;
}

void NC_STACK_ypaworld::RestorePlasmaCurrency(uint64_t amount)
{
    // Loading a save restores the counter without emitting pickup events.
    _plasmaCurrency = amount;
    _plasmaCurrencyHudStartValue = amount;
    _plasmaCurrencyHudTargetValue = amount;
    _plasmaCurrencyHudPulseStartTime = 0;
    _plasmaCurrencyPopups.clear();
}

void NC_STACK_ypaworld::ResetPlasmaCurrencyRuntime()
{
    RestorePlasmaCurrency(0);
}

void NC_STACK_ypaworld::CommitPlasmaCurrency()
{
    if ( _levelInfo.State != TLevelInfo::STATE_COMPLETED || !IsPlasmaCurrencyEnabled() )
        return;

    const uint64_t available = std::numeric_limits<uint64_t>::max() - _plasmaCurrencyBank;
    _plasmaCurrencyBank += std::min(_plasmaCurrency, available);
    ResetPlasmaCurrencyRuntime();
}
