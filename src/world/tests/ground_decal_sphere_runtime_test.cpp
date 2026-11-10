// Synthetic terrain fixture linked to the production world and decal builder.
#include "game/yw.h"
#include "nucleus/skeleton.h"
#include <cmath>
#include <cstdio>
#include <memory>
#undef main


namespace
{
class TerrainTexture : public NC_STACK_bitmap
{
public:
    const std::string getRsrc_name() const override { return "BODEN1"; }
};

int checks = 0, failures = 0;
void Check(bool result, const char *label)
{
    ++checks;
    if ( !result ) { ++failures; std::printf("FAIL: %s\n", label); }
}
}

int main()
{
    auto world = std::unique_ptr<NC_STACK_ypaworld>(new NC_STACK_ypaworld);
    NC_STACK_skeleton skeleton;
    UAskeleton::Data data;
    skeleton._resData = &data;
    data.POO = {vec3d(-600, 0, -600), vec3d(600, 0, -600),
                vec3d(600, 0, 600), vec3d(-600, 0, 600)};
    UAskeleton::Polygon floor;
    floor.num_vertices = 4;
    floor.v[0] = 0; floor.v[1] = 1; floor.v[2] = 2; floor.v[3] = 3;
    floor.B = 1;
    data.polygons.push_back(floor);
    world->_mapSize = Common::Point(4, 4);
    world->_cells.Resize(world->_mapSize);
    TSubSectorDesc sector;
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 3; ++x)
            world->_secTypeArray[0].SubSectors.At(x, y) = &sector;
    for (cellArea &cell : world->_cells)
    {
        cell.SectorType = 1;
        cell.type_id = 0;
        cell.height = 0;
    }
    world->_legoArray[0].CollisionSkelet = &skeleton;
    World::TChainFXConfig fx;
    fx.mode = World::TChainFXConfig::MODE_GROUND_DECAL;
    fx.trigger = World::TChainFXConfig::TRIGGER_DESTROYED;
    // Source supplies the manual world-space diameter.
    fx.ground_decal_size_min=fx.ground_decal_size_max=100;
    fx.ground_decal_permanent = true;
    const vec3d center(1800, -10, -1800);
    Check(world->SpawnGroundDecalInSphere(fx, center, 10), "production scanner accepts exact surface tangency");
    Check(world->_groundDecals.size() == 1, "one configured decal emitted");
    if ( !world->_groundDecals.empty() )
        Check((world->_groundDecals.back().pos - vec3d(1800, 0, -1800)).length() < 1e-8,
              "decal anchored to nearest real surface point");
    Check(!world->SpawnGroundDecalInSphere(fx, center, 9.999f), "production scanner rejects tiny gap");
    Check(world->SpawnGroundDecalInSphere(fx, center, 10.001f), "production scanner accepts tiny overlap");
    Check(!world->SpawnGroundDecalInSphere(fx, center, 0), "production scanner rejects zero radius");
    Check(!world->SpawnGroundDecalInSphere(fx, center, -1), "production scanner rejects negative radius");
    Check(!world->SpawnGroundDecalInSphere(fx, vec3d(1800, -1000, -1800), 10), "air explosion too high emits no decal");
    // Buildings and walls receive decals too: what shows them is decided per pixel.
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = -10;
    Check(world->SpawnGroundDecalInSphere(fx, center, 10), "building surface receives a decal");
    // A big decal on a roof also marks the ground in reach; a small one stays up there.
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = -100;
    const vec3d roofCenter(1800, -110, -1800);
    world->ClearGroundDecals();
    fx.ground_decal_size_min = fx.ground_decal_size_max = 1000;
    Check(world->SpawnGroundDecalInSphere(fx, roofCenter, 10), "big decal lands on the roof");
    Check(world->_groundDecals.size() == 2, "big decal on a roof also marks the ground");
    if ( world->_groundDecals.size() == 2 )
    {
        Check(std::fabs(world->_groundDecals.front().pos.y + 100) < 1e-6, "first decal stays on the roof");
        Check(std::fabs(world->_groundDecals.back().pos.y) < 1e-6, "second decal lies on the sector ground");
    }
    world->ClearGroundDecals();
    fx.ground_decal_size_min = fx.ground_decal_size_max = 100;
    Check(world->SpawnGroundDecalInSphere(fx, roofCenter, 10), "small decal lands on the roof");
    Check(world->_groundDecals.size() == 1, "small decal does not reach the ground");
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = -10;
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = 0;
    data.polygons[0].B = 0;
    data.polygons[0].A = 1;
    Check(world->SpawnGroundDecalInSphere(fx, center, 10), "wall receives a decal");
    if ( !world->_groundDecals.empty() )
        Check(std::fabs(world->_groundDecals.back().axisN.x) > 0.99, "decal plane follows the wall");
    data.polygons[0].B = 1;
    data.polygons[0].A = 0;
    NC_STACK_skeleton filler;
    UAskeleton::Data fillerData;
    filler._resData = &fillerData;
    fillerData.POO = {vec3d(-150, 0, -150), vec3d(150, 0, -150),
                      vec3d(150, 0, 150), vec3d(-150, 0, 150)};
    fillerData.polygons.push_back(floor);
    world->_fillerSide = &filler;
    for (int y = 0; y < 4; ++y) world->_cells.At(1, y).height = -300;
    const vec3d slopeCenter(2400, -160, -1800);
    ypaworld_arg136 contact;
    Check(world->FindWorldSurfaceInSphere(slopeCenter, 8, contact) &&
          contact.isect && contact.skel && contact.polyID >= 0 &&
          (contact.isectPos - vec3d(2395, -155, -1800)).length() < 1e-8,
          "shared contact provides position and collision polygon for deformation");
    Check(!world->FindWorldSurfaceInSphere(slopeCenter, 7, contact),
          "deformation contact rejects a surface outside the fuse radius");
    Check(world->SpawnGroundDecalInSphere(fx, slopeCenter, 8), "real terrain filler accepts side contact beyond vertical reach");
    if ( !world->_groundDecals.empty() )
        Check((world->_groundDecals.back().pos - vec3d(2395, -155, -1800)).length() < 1e-8,
              "shared filler restored and decal anchored to slope");
    Check(!world->SpawnGroundDecalInSphere(fx, slopeCenter, 7), "real terrain filler outside radius rejected");
    world->_isNetGame = true;
    Check(!world->SpawnGroundDecalInSphere(fx, center, 10), "existing network decal restriction retained");
    world->_isNetGame = false;
    fx.trigger = World::TChainFXConfig::TRIGGER_AIR_DESTROYED;
    Check(!world->SpawnGroundDecalInSphere(fx, center, 10), "only destroyed decal blocks accepted");

    // Two buildings share a sector. The wall overhangs the next slot, so its
    // actual collision origin is needed to identify which building was hit.
    world->ClearGroundDecals();
    cellArea &cell = world->_cells.At(1, 1);
    cell.CellId = Common::Point(1, 1);
    cell.SectorType = 0;
    cell.height = 0;
    cell.buildings_health.At(0, 1) = 255;
    cell.buildings_health.At(1, 1) = 255;
    TerrainTexture groundTexture;
    NC_STACK_base base;
    base.Meshes.emplace_back();
    GFX::TMesh &ground = base.Meshes.back();
    ground.Mat.TexSource = &groundTexture;
    ground.Vertexes.resize(3);
    ground.Vertexes[0].Pos = vec3f(-600, 0, -600);
    ground.Vertexes[1].Pos = vec3f(600, 0, -600);
    ground.Vertexes[2].Pos = vec3f(0, 0, 600);
    ground.Indixes = {0, 1, 2};
    world->_legoArray[0].Base = &base;
    world->_vhclModels.resize(2);
    world->_vhclModels[1] = &base;

    fx.trigger = World::TChainFXConfig::TRIGGER_IMPACT_WORLD;
    auto &smoke = fx.ground_decal_smoke;
    smoke.enabled = true;
    smoke.chance = 100;
    smoke.active_time_min = smoke.active_time_max = 3000;
    smoke.mode = World::TGroundDecalSmoke::MODE_GROUND_PULSE;
    smoke.pulse_vp = 1;
    smoke.pulse_time = 500;
    smoke.pulse_interval_min = smoke.pulse_interval_max = 500;
    World::TCraterConfig crater;
    crater.defined = true;
    crater.radius_min = crater.radius_max = 100;
    crater.depth_min = crater.depth_max = 30;

    ypaworld_arg136 impact;
    impact.isect = true;
    impact.hitCell = cell.CellId;
    impact.hitCollisionType = 1;
    impact.hitSkelPos = vec3d(1500, 0, -1800);
    impact.isectPos = vec3d(1740, -4, -1800);
    impact.vect = vec3d(1, 0, 0);
    impact.skel = &data;
    data.polygons[0].A = 1;
    data.polygons[0].B = 0;
    Check(world->SpawnGroundDecal(fx, impact) && world->SpawnTerrainCrater(crater, impact) &&
          world->_groundDecals.back().building.slot == 3 &&
          world->_terrainCraters.craters.back().building.slot == 3,
          "wall at ground height belongs to its collision piece, even across a slot boundary");

    impact.hitSkelPos.x = 1800;
    impact.isectPos = vec3d(1900, -100, -1800);
    Check(world->SpawnGroundDecal(fx, impact) && world->SpawnTerrainCrater(crater, impact),
          "neighboring building receives its own decal, smoke and dent");
    data.polygons[0].A = 0;
    data.polygons[0].B = 1;
    impact.hitSkelPos.x = 1500;
    impact.isectPos = vec3d(1500, 0, -1800);
    impact.vect = vec3d(0, 1, 0);
    Check(world->SpawnGroundDecal(fx, impact) && world->SpawnTerrainCrater(crater, impact) &&
          world->_groundDecals.back().building.slot == -1 &&
          world->_terrainCraters.craters.back().kind == World::Crater::Shape::GROUND,
          "ground inside a building slot has its own decal, smoke and crater");

    TF::TForm3D view;
    view.CalcPos = impact.isectPos;
    view.CalcSclRot = mat3x3::Ident();
    world->_normalVizLimit = 3000;
    auto *previousView = TF::Engine.GetViewPoint();
    TF::Engine.SetViewPoint(&view);
    baseRender_msg render{};
    world->RenderGroundDecalSmoke(&render);
    TF::Engine.SetViewPoint(previousView);
    Check(world->_transientVPs.size() == 3, "each smoke emitter has a visible pulse");

    yw_arg129 damage{};
    damage.OwnerID = World::OWNER_NOCHANGE;
    world->ApplyBuildingHealthChange(&cell, 0, 1, 50, &damage);
    Check(world->_groundDecals.size() == 3 && world->_groundDecalSmoke.size() == 3 &&
          world->_transientVPs.size() == 3 && world->TerrainCraterCount() == 3,
          "partial damage keeps all the impact effects");
    world->ApplyBuildingHealthChange(&cell, 0, 1, 0, &damage);
    Check(world->_groundDecals.size() == 2 && world->_groundDecalSmoke.size() == 2 &&
          world->_transientVPs.size() == 2 && world->TerrainCraterCount() == 2 &&
          world->_groundDecals.front().building.slot == 4 &&
          world->_groundDecals.back().building.slot == -1,
          "destruction clears only the matching decal, emitter, active pulse and dent");

    Engine::StringList save = {"begin_craters",
        "dent = 1740|-4|-1800|100|30|1|0|0 |1|1|3",
        "dent = 1740|-4|-1800|100|30|1|0|0 |1|1|4",
        "end"};
    ScriptParser::HandlersList handlers{new World::Parsers::SaveCraterParser(world.get())};
    Check(ScriptParser::ParseStringList(save, handlers, ScriptParser::FLAG_NO_SCOPE_SKIP) &&
          world->TerrainCraterCount() == 3 && world->_terrainCraters.craters.back().building.slot == 4,
          "save reader restores the exact live slot and skips a destroyed owner");
    world->ApplyBuildingHealthChange(&cell, 1, 1, 0, &damage);
    Check(world->TerrainCraterCount() == 1 && world->_groundDecals.size() == 1 &&
          world->_groundDecalSmoke.size() == 1 && world->_transientVPs.size() == 1,
          "ground effects survive destruction of both buildings");
    world->_transientVPs.clear();
    world->_vhclModels[1] = nullptr;
    world->_legoArray[0].Base = nullptr;
    world->_legoArray[0].CollisionSkelet = nullptr;
    world->_fillerSide = nullptr;
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
