// Wireframe/visual tint of the Black Sect faction.
#include <string>

#include "blacksecttint.h"
#include "game/ypabact.h"          // NC_STACK_ypabact, BACT_TYPES_*
#include "../system/inivals.h"
#include "common/log.h"

namespace World
{
namespace BlackSectTint
{

static TVisualTint s_tint;

static TVisualTint ParseTint(const std::string &str)
{
    TVisualTint tint;
    if ( !ParseVisualTint(str, &tint) )
    {
        ypa_log_out("Warning: invalid game.black_sect_units_tint '%s', using 140|140|140|255\n",
                    str.c_str());
        ParseVisualTint("140|140|140|255", &tint);
    }
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
