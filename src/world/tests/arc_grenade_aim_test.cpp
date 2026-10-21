// Arc Grenade player aim: ground-point solution of the crosshair ray.
#include "game/ypabact_internal.h"
#include "game/yw_internal.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

// Flat ground at y = 0: a ray that goes down meets it, anything else flies free.
struct FlatWorld : NC_STACK_ypaworld {
    NC_STACK_base visual;
    FlatWorld() {
        _vhclModels.resize(256, &visual);
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    void ypaworld_func180(yw_arg180 *) override {}
    void ypaworld_func149(ypaworld_arg136 *arg) override {
        arg->isect = false;
        if ( arg->vect.y <= 0.0f )
            return;
        const float t = (0.0f - arg->stPos.y) / arg->vect.y;
        if ( t < 0.0f || t > 1.0f )
            return;
        arg->isect = true;
        arg->tVal = t;
        arg->isectPos = arg->stPos + arg->vect * t;
    }
};

bool ParseWeapons(FlatWorld &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::WeaponProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}

struct Shooter : NC_STACK_ypabact {
    void Bind(FlatWorld &w, const vec3d &pos) {
        _world = &w; _owner = 1; _gid = 1; _vehicleID = 0;
        _position = _old_pos = pos; _rotation = mat3x3::Ident();
        _bact_type = BACT_TYPES_TANK; _status = BACT_STATUS_NORMAL;
        _status_flg = 0; _oflags = 0;
        _fire_pos = vec3d(0, 0, 0);
        _soundcarrier.Resize(7);
    }
};

// Same motion as the runtime: gravity on the velocity, position moves by velocity * time * 6.
vec3d Fly(vec3d pos, const vec3d &dir, float speed, float gravity) {
    vec3d velocity = dir * speed;
    const float dt = 0.002f;
    for (int i = 0; i < 2000000 && pos.y < 0.0f + 1e9f; i++) {
        velocity.y += gravity * dt;
        vec3d next = pos + velocity * (dt * 6.0f);
        if ( next.y >= 0.0f && velocity.y > 0.0f )
            return next;
        pos = next;
    }
    return pos;
}
}

int main() {
    FlatWorld w;
    Check(ParseWeapons(w, {
        "new_weapon 1", "model = arc_grenade", "start_speed = 90", "grenade_arc_angle = 50",
        "grenade_arc_gravity = 35", "end"}),
          "arc grenade fixture parses");
    Check(w._weaponProtos[1].IsArcGrenade(), "the fixture is an arc grenade");

    const World::TWeapProto &wp = w._weaponProtos[1];
    Shooter tank;
    tank.Bind(w, vec3d(4000, -100, -4000));
    const vec3d muzzle = tank._position;

    // 1. Aim at the ground ahead: the shell has to land on that point, at any distance.
    // The ground is y = 0 and the muzzle 100 units above it, so a ray of slope "down"
    // meets the ground 100 / down units ahead.
    float previousRange = 0.0f;
    for (float down : {0.30f, 0.15f, 0.08f}) {
        vec3d aim(0.0f, down, 1.0f);
        vec3d dir;
        Check(ypabact_ComputeArcGrenadeGroundAim(&tank, wp, muzzle, aim, &dir),
              "ground aim is computed");
        const float expectedRange = 100.0f / down;
        const vec3d hit = Fly(muzzle, dir, wp.start_speed, 35.0f);
        const float dx = hit.x - muzzle.x;
        const float dz = hit.z - (muzzle.z + expectedRange);
        const float miss = std::sqrt(dx * dx + dz * dz);
        Check(miss < 0.01f * expectedRange + 2.0f,
              "the simulated flight comes down on the aimed ground point");
        Check(expectedRange > previousRange, "aiming higher lands the shell farther away");
        previousRange = expectedRange;
    }

    // 2. Sky and unreachable ground: 45 degrees, maximum range, same heading.
    const float maxRange = 90.0f * 90.0f / 35.0f * 6.0f;
    for (vec3d aim : {vec3d(0.0f, -0.4f, 1.0f), vec3d(0.0f, 0.03f, 1.0f)}) {
        vec3d dir;
        Check(ypabact_ComputeArcGrenadeGroundAim(&tank, wp, muzzle, aim, &dir),
              "sky or out of reach still gives a launch direction");
        Check(std::fabs(dir.y + 0.70710678f) < 1e-3f && std::fabs(dir.x) < 1e-3f && dir.z > 0.0f,
              "unreachable aim fires at 45 degrees along the same heading");
        const vec3d hit = Fly(muzzle, dir, wp.start_speed, 35.0f);
        Check(hit.z - muzzle.z > 0.9f * maxRange && hit.z - muzzle.z < 1.1f * maxRange,
              "unreachable aim lands near the maximum range");
    }

    // 3. A weapon that is not an arc grenade, or has no speed, never gets a ground aim.
    {
        vec3d dir;
        Check(ParseWeapons(w, {"new_weapon 5", "model = rocket", "end"}), "rocket fixture parses");
        Check(!ypabact_ComputeArcGrenadeGroundAim(&tank, w._weaponProtos[5], muzzle, vec3d(0, 0.1f, 1), &dir),
              "only arc grenades use the ground aim");
    }

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
