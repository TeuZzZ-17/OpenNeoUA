// Terrain craters: what the world keeps for them.
//
// A sector piece or a collision surface is shared by every cell that uses the
// same model, so a crater never edits it. The cells it reaches get their own
// copy, cut and dug (see terraincrater_core.h), used in place of the shared one
// only there. Copies are made when first needed and thrown away when a crater
// that reaches them comes or goes; a thrown away copy lives one more frame,
// because a collision result taken from it may still be in use.
#ifndef WORLD_TERRAINCRATER_H_INCLUDED
#define WORLD_TERRAINCRATER_H_INCLUDED

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "terraincrater_core.h"
#include "nucleus/skeleton.h"
#include "system/gfx.h"

namespace World
{
namespace Crater
{

// A sector piece drawn with its craters. `empty` means no crater reaches it, so
// the shared model is drawn as it is. `stale` means a crater came or went: it is
// still drawn until its new copy is made (see the rebuild budget).
struct PieceMeshes
{
    int legoId = -1;
    bool empty = true;
    bool stale = false;
    std::list<GFX::TMesh> meshes;

    PieceMeshes() = default;
    PieceMeshes(const PieceMeshes &) = delete;
    PieceMeshes &operator=(const PieceMeshes &) = delete;
    ~PieceMeshes();
};

// A collision surface with its craters, used in place of the shared one.
struct CollisionCopy
{
    bool empty = true;
    UAskeleton::Data data;
    // Where each point of data.POO was before the craters and dents moved it.
    std::vector<vec3d> rest;
    NC_STACK_skeleton skeleton;
    PolygonGrid grid;   // Which polygons lie under a short ray (see sub_44D8B8).

    CollisionCopy() { skeleton._resData = &data; }
    CollisionCopy(const CollisionCopy &) = delete;
    CollisionCopy &operator=(const CollisionCopy &) = delete;
    ~CollisionCopy() { skeleton._resData = nullptr; }
};

// Piece: cell x, cell y, piece 0..8 (9 = the single model of a one-piece sector).
typedef std::tuple<int, int, int> PieceKey;
// Collision: surface position (it tells the cell piece or seam), shared model, type.
typedef std::tuple<int64_t, int64_t, const void *, int> CollisionKey;

struct State
{
    std::vector<Shape> craters;     // Oldest first.
    std::map<PieceKey, std::unique_ptr<PieceMeshes>> pieces;
    std::map<CollisionKey, std::unique_ptr<CollisionCopy>> collision;
    std::vector<std::unique_ptr<PieceMeshes>> retiredPieces;
    std::vector<std::unique_ptr<CollisionCopy>> retiredCollision;

    // Pieces rebuilt in the current frame and the time spent on them.
    int rebuiltThisFrame = 0;
    double rebuildMs = 0.0;

    // Goes up whenever a crater or dent comes or goes.
    uint32_t version = 0;

    // The models standing on each sector when its copies were last checked
    // (see RefreshTerrainCraterFootprints).
    std::vector<uint32_t> cellModels;

    // Squares of 300 units next to a dent (see TerrainCraterSpill): how many
    // squares around a ray must also look, 0 where no dent is near.
    std::vector<uint8_t> spill;
    int spillWidth = 0;
    uint32_t spillVersion = 0xffffffffu;
};

}
}

#endif // WORLD_TERRAINCRATER_H_INCLUDED
