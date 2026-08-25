#ifndef WORLD_BLACKSECTTINT_H_INCLUDED
#define WORLD_BLACKSECTTINT_H_INCLUDED

#include "protos.h" // for World::TVisualTint

class NC_STACK_ypabact;

namespace World
{
namespace BlackSectTint
{
    constexpr int OWNER_BLACK_SECT = 5;

    // Read game.black_sect_units_tint after nucleus.ini has been parsed.
    // The tint is render-only and never mutates actor or prototype state.
    void Init();

    const TVisualTint &Tint();

    // True for Black Sect combat units. Host Stations and projectiles are excluded.
    bool IsTintedUnit(const NC_STACK_ypabact *bact);
}
}

#endif // WORLD_BLACKSECTTINT_H_INCLUDED
