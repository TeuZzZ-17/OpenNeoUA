#ifndef WORLD_BUILDING_PIECE_H_INCLUDED
#define WORLD_BUILDING_PIECE_H_INCLUDED

namespace World
{

// A destructible LEGO piece. Single-piece sectors use slot 0.
struct BuildingPiece
{
    int cellX = -1;
    int cellY = -1;
    int slot = -1;

    bool Matches(int x, int y, int index) const
    {
        return slot >= 0 && cellX == x && cellY == y && (index < 0 || slot == index);
    }
};

}

#endif
