#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "energyfx.h"
#include "spin.h"
#include "../log.h"
#include "../system/inivals.h"

namespace World
{
namespace EnergyFX
{

static Config s_regen;
static Config s_drain;

static bool ParseLongStrict(const std::string &value, long *out)
{
    if ( !out || value.empty() )
        return false;

    errno = 0;
    char *end = NULL;
    const char *begin = value.c_str();
    long parsed = std::strtol(begin, &end, 10);

    if ( end == begin || errno == ERANGE )
        return false;

    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
        end++;

    if ( *end != '\0' )
        return false;

    *out = parsed;
    return true;
}

static bool ParseFloatStrict(const std::string &value, float *out)
{
    if ( !out || value.empty() )
        return false;

    errno = 0;
    char *end = NULL;
    const char *begin = value.c_str();
    float parsed = std::strtof(begin, &end);

    if ( end == begin || errno == ERANGE || !std::isfinite(parsed) )
        return false;

    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
        end++;

    if ( *end != '\0' )
        return false;

    *out = parsed;
    return true;
}

static int ReadInt(Common::Ini::Key &key, int fallback, int minValue, int maxValue)
{
    long parsed = fallback;
    const std::string value = key.Get<std::string>();

    if ( !ParseLongStrict(value, &parsed) )
    {
        if ( key.WasSet )
            ypa_log_out("Warning: invalid %s '%s', using %d\n", key.Name.c_str(), value.c_str(), fallback);
        parsed = fallback;
    }

    if ( parsed < minValue )
        parsed = minValue;
    if ( parsed > maxValue )
        parsed = maxValue;
    return (int)parsed;
}

static float ReadFloat(Common::Ini::Key &key, float fallback)
{
    float parsed = fallback;
    const std::string value = key.Get<std::string>();

    if ( !ParseFloatStrict(value, &parsed) )
    {
        if ( key.WasSet )
            ypa_log_out("Warning: invalid %s '%s', using %.3f\n", key.Name.c_str(), value.c_str(), fallback);
        parsed = fallback;
    }

    return parsed;
}

static bool ParseTintComponent(const char *&pos, int *out)
{
    errno = 0;
    char *end = NULL;
    long value = std::strtol(pos, &end, 10);

    if ( end == pos || errno == ERANGE )
        return false;

    *out = (int)value;
    pos = end;
    return true;
}

static TVisualTint ReadTint(Common::Ini::Key &key)
{
    const std::string value = key.Get<std::string>();
    const char *pos = value.c_str();
    int component[4] = {255, 255, 255, 255};
    bool valid = true;

    for (int i = 0; i < 3; i++)
    {
        if ( !ParseTintComponent(pos, &component[i]) )
        {
            valid = false;
            break;
        }

        if ( i < 2 )
        {
            if ( *pos != '_' )
            {
                valid = false;
                break;
            }
            pos++;
        }
    }

    if ( valid && *pos == '_' )
    {
        pos++;
        valid = ParseTintComponent(pos, &component[3]);
    }

    if ( valid )
        valid = *pos == '\0';

    if ( !valid )
    {
        if ( key.WasSet )
            ypa_log_out("Warning: invalid %s '%s', using 255_255_255_255\n", key.Name.c_str(), value.c_str());
        component[0] = component[1] = component[2] = component[3] = 255;
    }

    auto clamp255 = [](int v) -> float
    {
        if ( v < 0 ) v = 0;
        if ( v > 255 ) v = 255;
        return (float)v / 255.0f;
    };

    TVisualTint tint;
    tint.r = clamp255(component[0]);
    tint.g = clamp255(component[1]);
    tint.b = clamp255(component[2]);
    tint.a = clamp255(component[3]);
    return tint;
}

static Config BuildConfig(Common::Ini::Key &vp,
                          Common::Ini::Key &scale,
                          Common::Ini::Key &spinX,
                          Common::Ini::Key &spinY,
                          Common::Ini::Key &spinZ,
                          Common::Ini::Key &tint,
                          Common::Ini::Key &duration,
                          Common::Ini::Key &intervalMin,
                          Common::Ini::Key &intervalMax,
                          Common::Ini::Key &countMin,
                          Common::Ini::Key &countMax,
                          Common::Ini::Key &randomOffsetPercent)
{
    Config config;
    config.vp = (int16_t)ReadInt(vp, 0, 0, std::numeric_limits<int16_t>::max());

    config.vp_scale = ReadFloat(scale, 1.0f);
    if ( config.vp_scale <= 0.0f )
        config.vp_scale = 1.0f;

    config.vp_spin.x = Spin::ClampStrength(ReadFloat(spinX, 0.0f));
    config.vp_spin.y = Spin::ClampStrength(ReadFloat(spinY, 0.0f));
    config.vp_spin.z = Spin::ClampStrength(ReadFloat(spinZ, 0.0f));
    config.vp_tint = ReadTint(tint);

    config.duration = ReadInt(duration, 1000, 0, std::numeric_limits<int>::max() / 4);
    if ( config.duration <= 0 )
        config.duration = 1000;

    config.interval_min = ReadInt(intervalMin, 0, 0, std::numeric_limits<int>::max() / 4);
    config.interval_max = ReadInt(intervalMax, 0, 0, std::numeric_limits<int>::max() / 4);
    config.count_min = ReadInt(countMin, 0, 0, 32);
    config.count_max = ReadInt(countMax, 0, 0, 32);

    config.random_offset_percent = ReadFloat(randomOffsetPercent, 25.0f);
    if ( config.random_offset_percent < 0.0f )
        config.random_offset_percent = 0.0f;
    else if ( config.random_offset_percent > 100.0f )
        config.random_offset_percent = 100.0f;

    if ( config.interval_max < config.interval_min )
        std::swap(config.interval_min, config.interval_max);
    if ( config.count_max < config.count_min )
        std::swap(config.count_min, config.count_max);

    return config;
}

void Init()
{
    s_regen = BuildConfig(System::IniConf::GfxRegenDecorationFXVP,
                          System::IniConf::GfxRegenDecorationFXVPScale,
                          System::IniConf::GfxRegenDecorationFXVPSpinX,
                          System::IniConf::GfxRegenDecorationFXVPSpinY,
                          System::IniConf::GfxRegenDecorationFXVPSpinZ,
                          System::IniConf::GfxRegenDecorationFXVPTint,
                          System::IniConf::GfxRegenDecorationFXDuration,
                          System::IniConf::GfxRegenDecorationFXIntervalMin,
                          System::IniConf::GfxRegenDecorationFXIntervalMax,
                          System::IniConf::GfxRegenDecorationFXCountMin,
                          System::IniConf::GfxRegenDecorationFXCountMax,
                          System::IniConf::GfxRegenDecorationFXRandomOffsetPercent);

    s_drain = BuildConfig(System::IniConf::GfxDrainDecorationFXVP,
                          System::IniConf::GfxDrainDecorationFXVPScale,
                          System::IniConf::GfxDrainDecorationFXVPSpinX,
                          System::IniConf::GfxDrainDecorationFXVPSpinY,
                          System::IniConf::GfxDrainDecorationFXVPSpinZ,
                          System::IniConf::GfxDrainDecorationFXVPTint,
                          System::IniConf::GfxDrainDecorationFXDuration,
                          System::IniConf::GfxDrainDecorationFXIntervalMin,
                          System::IniConf::GfxDrainDecorationFXIntervalMax,
                          System::IniConf::GfxDrainDecorationFXCountMin,
                          System::IniConf::GfxDrainDecorationFXCountMax,
                          System::IniConf::GfxDrainDecorationFXRandomOffsetPercent);
}

const Config &Regen()
{
    return s_regen;
}

const Config &Drain()
{
    return s_drain;
}

}
}
