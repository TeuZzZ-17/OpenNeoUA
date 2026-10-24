// Synthetic terrain fixture linked to the production world and decal builder.
#include "game/yw.h"
#include "nucleus/skeleton.h"
#include <cstdio>
#include <memory>
#undef main


namespace
{
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
    Check(world->SpawnGroundDecalInSphere(fx, center, 10), "production scanner accepts exact terrain tangency");
    Check(world->_groundDecals.size() == 1, "one configured decal emitted");
    if ( !world->_groundDecals.empty() )
        Check((world->_groundDecals.back().pos - vec3d(1800, 0, -1800)).length() < 1e-8,
              "decal anchored to nearest real terrain point");
    auto burning=fx;
    burning.ground_decal_fire=World::GroundDecalFireDefaults();
    burning.ground_decal_fire.enabled=true;burning.ground_decal_fire.chance=100;
    burning.fire_propagation_chance=100;burning.fire_propagation_count=2;
    burning.fire_propagation_radius=15;burning.fire_propagation_delay=1000;
    Check(world->SpawnGroundDecalInSphere(burning,center,10),"burning decal uses the actual terrain path");
    Check(world->_groundFireSpread.size()==2,"successful ignition schedules bounded child patches");
    Check(!world->_groundDecalSmoke.empty()&&world->_groundDecalSmoke.back().params.flame,"terrain decal creates flame emitter");
    world->_timeStamp=500;world->UpdateGroundDecalFire();
    Check(world->_groundFireSpread.size()==2,"propagation waits for the authored delay");
    const size_t beforeSpread=world->_groundDecals.size();
    world->_timeStamp=1500;world->UpdateGroundDecalFire();
    Check(world->_groundFireSpread.empty(),"child patches do not schedule another generation");
    Check(world->_groundDecals.size()==beforeSpread+2,"propagation projects two new decal patches onto the terrain");
    Check(!world->SpawnGroundDecalInSphere(fx, center, 9.999f), "production scanner rejects tiny gap");
    Check(world->SpawnGroundDecalInSphere(fx, center, 10.001f), "production scanner accepts tiny overlap");
    Check(!world->SpawnGroundDecalInSphere(fx, center, 0), "production scanner rejects zero radius");
    Check(!world->SpawnGroundDecalInSphere(fx, center, -1), "production scanner rejects negative radius");
    Check(!world->SpawnGroundDecalInSphere(fx, vec3d(1800, -1000, -1800), 10), "air explosion too high emits no decal");
    // A building's horizontal face is closer than the ground, but is ineligible.
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = -10;
    Check(!world->SpawnGroundDecalInSphere(fx, center, 10), "building surface cannot satisfy terrain rule");
    for (UAskeleton::Vertex &vertex : data.POO) vertex.y = 0;
    data.polygons[0].B = 0;
    data.polygons[0].A = 1;
    Check(!world->SpawnGroundDecalInSphere(fx, center, 10), "wall normal cannot satisfy terrain rule");
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
    world->_legoArray[0].CollisionSkelet = nullptr;
    world->_fillerSide = nullptr;
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
