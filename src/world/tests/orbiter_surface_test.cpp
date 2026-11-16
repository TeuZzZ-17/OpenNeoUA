// Orbiter strikes happen on the besieged unit's collision surface (here: its collision
// radius, the legacy sphere), not at its centre. Production helpers, no game session.
// Link production objects with main.cpp's SDL_main renamed (see run_runtime_probe.py).
#include "game/yw.h"
#include "game/ypatank.h"
#include "game/ypamissile_internal.h"
#include <cmath>
#include <cstdio>
#undef main

namespace {
struct TestWorld : NC_STACK_ypaworld {
    cellArea cell;
    size_t GetSectorInfo(yw_130arg *arg) override { arg->pcell = &cell; arg->CellId = Common::Point(0, 0); return 1; }
};
struct TestTank : NC_STACK_ypatank { void BindWorld(NC_STACK_ypaworld &world) { _world = &world; } };

int checks = 0, failures = 0;
void Check(bool value, const char *name) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
bool Near(const vec3d &a, const vec3d &b) { return (a - b).length() < 0.01; }
}

int main() {
    TestWorld world;
    TestTank target;
    target.BindWorld(world);
    target._bact_type = BACT_TYPES_TANK;
    target._rotation = mat3x3::Ident();
    target._position = vec3d(0, 0, 0);
    target._radius = 100.0f;

    vec3d hit;
    // A dive from the side meets the surface 100 units from the centre, not the centre.
    Check(ypamissile_OrbiterSurfaceHit(&target, vec3d(400, 0, 0), vec3d(0, 0, 0), &hit) && Near(hit, vec3d(100, 0, 0)),
          "dive meets the collision surface");
    // A path through the body enters at the near surface...
    Check(ypamissile_OrbiterSurfaceHit(&target, vec3d(0, 0, 400), vec3d(0, 0, -400), &hit) && Near(hit, vec3d(0, 0, 100)),
          "pass-through enters at the near surface");
    // ...and the reversed path finds the exit surface on the far side.
    Check(ypamissile_OrbiterSurfaceHit(&target, vec3d(0, 0, -400), vec3d(0, 0, 400), &hit) && Near(hit, vec3d(0, 0, -100)),
          "reversed path finds the far (exit) surface");
    Check(!ypamissile_OrbiterSurfaceHit(&target, vec3d(400, 0, 300), vec3d(400, 0, -300), &hit), "a path beside the body misses");
    Check(!ypamissile_OrbiterSurfaceHit(&target, vec3d(400, 0, 0), vec3d(200, 0, 0), &hit), "a path that stops short misses");

    Check(ypamissile_OrbiterInsideTarget(&target, vec3d(60, 0, 0)), "point inside the collision");
    Check(!ypamissile_OrbiterInsideTarget(&target, vec3d(140, 0, 0)), "point outside the collision");

    // The collision follows the unit, not the world origin.
    target._position = vec3d(1000, -50, 500);
    Check(ypamissile_OrbiterSurfaceHit(&target, vec3d(1400, -50, 500), target._position, &hit) &&
          Near(hit, vec3d(1100, -50, 500)), "surface follows the unit position");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
