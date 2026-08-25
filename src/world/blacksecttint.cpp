#include <cerrno>
#include <cstdlib>
#include <string>

#include "blacksecttint.h"
#include "../ypabact.h"          // NC_STACK_ypabact, BACT_TYPES_*
#include "../system/inivals.h"
#include "../log.h"

namespace World
{
namespace BlackSectTint
{

static TVisualTint s_tint;

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

static bool ParseTintComponents(const std::string &str, int comp[4])
{
    const char *pos = str.c_str();

    for (int i = 0; i < 4; i++)
    {
        if ( !ParseTintComponent(pos, &comp[i]) )
            return false;

        if ( i < 3 )
        {
            if ( *pos != '_' )
                return false;

            pos++;
        }
    }

    return *pos == '\0';
}

static TVisualTint ParseTint(const std::string &str)
{
    int comp[4] = { 140, 140, 140, 255 };
    int parsed[4] = { 140, 140, 140, 255 };

    if ( ParseTintComponents(str, parsed) )
    {
        comp[0] = parsed[0];
        comp[1] = parsed[1];
        comp[2] = parsed[2];
        comp[3] = parsed[3];
    }
    else
    {
        ypa_log_out("Warning: invalid game.black_sect_units_tint '%s', using 140_140_140_255\n",
                    str.c_str());
    }

    auto clamp255 = [](int v) -> float
    {
        if ( v < 0 )   v = 0;
        if ( v > 255 ) v = 255;
        return (float)v / 255.0f;
    };

    TVisualTint tint;
    tint.r = clamp255(comp[0]);
    tint.g = clamp255(comp[1]);
    tint.b = clamp255(comp[2]);
    tint.a = clamp255(comp[3]);
    return tint;
}

void Init()
{
    s_tint = ParseTint(System::IniConf::GameBlackSectUnitsTint.Get<std::string>());
}

const TVisualTint &Tint()
{
    return s_tint;
}

bool IsTintedUnit(const NC_STACK_ypabact *bact)
{
    if ( !bact || bact->_owner != OWNER_BLACK_SECT )
        return false;

    switch ( bact->_bact_type )
    {
    case BACT_TYPES_BACT:
    case BACT_TYPES_TANK:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
    case BACT_TYPES_GUN:
        return true;

    default:
        return false;
    }
}

}
}
