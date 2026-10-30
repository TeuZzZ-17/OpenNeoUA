// Script parsers: small value helpers (numbers, ranges, percentages, clamps)
// shared by every parsers_*.cpp file. Declared in parsers_internal.h.
#include "parsers.h"
#include "../fmtlib/core.h"
#include "../fmtlib/printf.h"
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/ypaflyer.h"
#include "game/ypacar.h"
#include "common/glyph_key.h"
#include "common/log.h"
#include "common/utils.h"
#include "../system/inivals.h"
#include "spin.h"
#include "tools.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include "parsers_internal.h"

namespace World
{
namespace Parsers
{

int ParsePositiveIntOrZero(const std::string &value)
{
    try
    {
        size_t parsed = 0;
        long long result = std::stoll(value, &parsed, 0);
        if ( parsed != value.size() || result <= 0 ||
             result > std::numeric_limits<int>::max() )
            return 0;

        return (int)result;
    }
    catch (...)
    {
        return 0;
    }
}

void ParseSoundPitchRange(const std::string &value, TVhclSound &sound)
{
    int minPitch = 0;
    int maxPitch = 0;

    if ( !World::ParseIntRangeValue(value, minPitch, maxPitch) )
    {
        minPitch = 0;
        maxPitch = 0;
    }

    sound.SetPitchRange(minPitch, maxPitch);
}

float ParseNonNegativeIniFloatOrZero(const std::string &value)
{
    if ( value.empty() )
        return 0.0f;

    errno = 0;
    char *end = nullptr;
    const char *begin = value.c_str();
    float parsed = std::strtof(begin, &end);
    if ( end == begin || errno == ERANGE || *end != '\0' ||
         !std::isfinite(parsed) || parsed < 0.0f )
        return 0.0f;

    return parsed;
}

bool ParseAbsoluteOrPercent(const std::string &value,
                            TAbsoluteOrPercent &out,
                            float maxPercent)
{
    TAuthoredScalar parsed;
    out.Clear();
    if ( !ParseAuthoredScalar(value, parsed) || parsed.value < 0.0f )
        return false;

    if ( parsed.percent )
        parsed.value = std::min(parsed.value, maxPercent);

    out.value = parsed.value;
    out.percent = parsed.percent;
    out.defined = true;
    return true;
}

float ParseMalusPercent(const std::string &value)
{
    TAuthoredScalar parsed;
    if ( !ParseAuthoredScalar(value, parsed) || !parsed.percent ||
         parsed.value > 0.0f )
        return 0.0f;

    const float signedPercent = std::max(-100.0f, parsed.value);
    return -signedPercent / 100.0f;
}

float ParseSignedPitchPercent(const std::string &value)
{
    TAuthoredScalar parsed;
    if ( !ParseAuthoredScalar(value, parsed) || !parsed.percent )
        return 1.0f;

    const float signedPercent = std::max(-100.0f, parsed.value);
    const float multiplier = 1.0f + signedPercent / 100.0f;
    return std::isfinite(multiplier) && multiplier >= 0.0f ? multiplier : 1.0f;
}

float ParseNormalizedOrPercent(const std::string &value)
{
    TAuthoredScalar parsed;
    if ( !ParseAuthoredScalar(value, parsed) )
        return 0.0f;

    if ( parsed.percent )
        return std::max(0.0f, std::min(parsed.value, 100.0f)) / 100.0f;

    return std::max(0.0f, std::min(parsed.value, 1.0f));
}

bool ParseSmartVPList(const std::string &value, std::vector<int16_t> &out)
{
    return ParsePositiveInt16ListValue(value, out);
}

float Clamp01(float value)
{
    if ( value < 0.0f )
        return 0.0f;

    if ( value > 1.0f )
        return 1.0f;

    return value;
}

float NonNegativeFiniteOrZero(float value)
{
    return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}

float ClampPushIntensity(float value)
{
    if ( !std::isfinite(value) || value <= 0.0f )
        return 0.0f;

    return std::min(value, 10.0f);
}

int NonNegativeFiniteMilliseconds(ScriptParser::Parser &parser,
                                  const std::string &value)
{
    double milliseconds = parser.stod(value, 0);
    if ( !std::isfinite(milliseconds) || milliseconds <= 0.0 )
        return 0;

    const double maximum = (double)std::numeric_limits<int>::max();
    if ( milliseconds >= maximum )
        return std::numeric_limits<int>::max();

    return (int)std::lround(milliseconds);
}

float ClampRecoilMultiplier(float value)
{
    if ( !(value >= 0.0f) )
        return 0.0f;

    if ( value > 10.0f )
        return 10.0f;

    return value;
}

float ClampProjectileVisualMotionRadius(float value)
{
    if ( !std::isfinite(value) || value <= 0.0f )
        return 0.0f;

    // Lateral visual radius in model/world units. The upper bound still permits
    // intentionally extreme motion while preventing malformed scripts from throwing
    // the VP arbitrarily far from its physical collision path.
    return std::min(value, 1000.0f);
}

void InitStatusSoundFXDefaults(World::TVhclSound &snd, int defaultVolume)
{
    snd.volume = defaultVolume;
    snd.sndPrm.mag0 = 1.0;
    snd.sndPrm.time = 1000;
    snd.sndPrm_shk.mag0 = 1.0;
    snd.sndPrm_shk.time = 1000;
    snd.sndPrm_shk.mute = 0.02;
    snd.sndPrm_shk.pos.x = 0.2;
    snd.sndPrm_shk.pos.y = 0.2;
    snd.sndPrm_shk.pos.z = 0.2;
}

int ClampSectorPower(int power)
{
    if ( power < 0 )
        return 0;

    if ( power > 255 )
        return 255;

    return power;
}

bool IsUsableScriptText(const std::string &text)
{
    for (unsigned char ch : text)
    {
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' )
            return true;
    }

    return false;
}

}
}
