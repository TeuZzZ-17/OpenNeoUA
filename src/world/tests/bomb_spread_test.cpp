// Bomb spread: production LaunchMissile() must give bombs and homing bombs a
// ground-plane push from the X (left/right) and Z (forward/back) values of
// weapon_spread_xyz, ignore its Y value for them, and leave absent/zero spread
// and every other weapon class unchanged.
#include "game/yw.h"
#include "game/ypamissile.h"
#include "nucleus/base.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
struct WorldProbe : NC_STACK_ypaworld {
    NC_STACK_base visual;
    WorldProbe() {
        _vhclModels.resize(256, &visual);
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    void ypaworld_func180(yw_arg180 *) override {}
};
bool Parse(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::WeaponProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
struct Launcher : NC_STACK_ypabact {
    void Bind(WorldProbe &w) {
        _world = &w; _bact_type = BACT_TYPES_FLYER; _status = BACT_STATUS_NORMAL;
        _status_flg = 0; _oflags = 0;
        _position = _old_pos = vec3d(2400, -300, -2400);
        _rotation = mat3x3::Ident(); _radius = _viewer_radius = 30;
        _gid = 1; _owner = 1; _energy = _energy_max = 100000;
        _vp_normal = _vp_genesis = nullptr; _soundcarrier.Resize(7);
        _pSector = &w.SectorAt(World::PositionToSectorID(_position));
    }
    // Fires one weapon and returns the projectile it created (nullptr on failure).
    NC_STACK_ypamissile *Fire(int weapon, float x, float y, float z, float carrierSpeed) {
        _weapon = weapon; _weapon_spread_x = x; _weapon_spread_y = y; _weapon_spread_z = z;
        _fly_dir_length = carrierSpeed; _weapon_time = 0; _salve_counter = 0;
        bact_arg79 arg{};
        arg.direction = vec3d(0, 0, 0);
        arg.tgType = BACT_TGT_TYPE_DRCT;
        arg.weapon = weapon; arg.g_time = 1000;
        const size_t before = _missiles_list.size();
        LaunchMissile(&arg);
        return _missiles_list.size() > before ? _missiles_list.back() : nullptr;
    }
};
vec3d Velocity(NC_STACK_ypamissile *m) { return m->_fly_dir * m->_fly_dir_length; }
}

int main() {
    WorldProbe w;
    Check(Parse(w, {"new_weapon 1", "model = bomb", "start_speed = 0", "end",
                    "new_weapon 2", "model = homing_bomb", "start_speed = 0", "end",
                    "new_weapon 3", "model = rocket", "start_speed = 100", "end"}),
          "bomb, homing bomb and rocket parse");
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypamissile>());

    // Vehicle parameter: absent/invalid Z in weapon_spread_xyz means 0, valid values are kept.
    auto ParseVehicle = [&](Engine::StringList lines) {
        ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&w)};
        return ScriptParser::ParseStringList(lines, handlers, 0);
    };
    ParseVehicle({"new_vehicle 1", "model = tank", "weapon_spread_xyz = 0_0_12.5", "end"});
    Check(w._vhclProtos[1].weapon_spread_z == 12.5f, "weapon_spread_xyz parses Z");
    for (const char *bad : {"-3", "nan", "inf", "junk", ""}) {
        ParseVehicle({"new_vehicle 1", "model = tank", std::string("weapon_spread_xyz = 0_0_") + bad, "end"});
        Check(w._vhclProtos[1].weapon_spread_z == 0.0f, "invalid weapon_spread_xyz Z becomes 0");
    }
    ParseVehicle({"new_vehicle 1", "model = tank", "weapon_spread_xyz = 0_0_9", "end"});
    ParseVehicle({"new_vehicle 1", "model = tank", "end"});
    Check(w._vhclProtos[1].weapon_spread_z == 0.0f, "absent weapon_spread_xyz is 0");

    // Pure helper: no spread keeps the velocity; only the ground plane is touched.
    const vec3d velocity(1, 2, 3);
    const mat3x3 level = mat3x3::Ident();
    Check((World::ApplyBombSpreadKick(velocity, level, 0, 0) - velocity).length() < 1e-6f &&
          (World::ApplyBombSpreadKick(velocity, level, -4, NAN) - velocity).length() < 1e-6f,
          "zero, negative or invalid spread keeps the launch velocity");
    const float maxPush = std::tan(25.0f * 0.0174532925f) * World::BOMB_SPREAD_REFERENCE_SPEED;
    bool xOnly = true, zOnly = true, pitched = true, bounded = true;
    for (int i = 0; i < 200; ++i) {
        const vec3d px = World::ApplyBombSpreadKick(velocity, level, 25, 0) - velocity;
        const vec3d pz = World::ApplyBombSpreadKick(velocity, level, 0, 25) - velocity;
        // Nose pitched 60 degrees down: the push must stay horizontal.
        const vec3d pp = World::ApplyBombSpreadKick(velocity, mat3x3::RotateX(1.05), 25, 25) - velocity;
        xOnly = xOnly && std::fabs(px.y) < 1e-5f && std::fabs(px.z) < 1e-5f;
        zOnly = zOnly && std::fabs(pz.y) < 1e-5f && std::fabs(pz.x) < 1e-5f;
        pitched = pitched && std::fabs(pp.y) < 1e-5f;
        bounded = bounded && std::fabs(px.x) <= maxPush + 1e-4f && std::fabs(pz.z) <= maxPush + 1e-4f;
    }
    Check(xOnly, "weapon_spread_xyz X pushes left/right only");
    Check(zOnly, "weapon_spread_xyz Z pushes forward/back only");
    Check(pitched, "push stays on the ground plane when the carrier is pitched");
    Check(bounded, "push stays within the spread angle scale");

    Launcher shooter; shooter.Bind(w);
    for (int weapon : {1, 2}) {
        // Absent/zero spread keeps vanilla: a stationary carrier drops a bomb with no speed;
        // a moving carrier gives exactly 20% of its speed straight ahead.
        auto *still = shooter.Fire(weapon, 0, 0, 0, 0.0f);
        Check(still && still->_fly_dir_length == 0.0f, "zero spread, stationary carrier: no launch speed");
        auto *moving = shooter.Fire(weapon, 0, 0, 0, 50.0f);
        Check(moving && std::fabs(moving->_fly_dir_length - 10.0f) < 1e-4f &&
              (Velocity(moving) - vec3d(0, 0, 10)).length() < 1e-4f,
              "zero spread, moving carrier: vanilla 20% forward speed");
        // Y of weapon_spread_xyz alone no longer does anything to a bomb.
        auto *yOnly = shooter.Fire(weapon, 0, 25, 0, 0.0f);
        Check(yOnly && yOnly->_fly_dir_length == 0.0f, "weapon_spread_xyz Y alone leaves the bomb unchanged");

        int sideways = 0, forwardBack = 0;
        bool vertical = true;
        for (int i = 0; i < 200; ++i) {
            // y is set on purpose: it must stay ignored while x and z act.
            auto *bomb = shooter.Fire(weapon, 25, 25, 25, 0.0f);
            if (!bomb) { vertical = false; break; }
            const vec3d v = Velocity(bomb);
            if (std::fabs(v.x) > 0.5f) ++sideways;
            if (std::fabs(v.z) > 0.5f) ++forwardBack;
            if (std::fabs(v.y) > 1e-4f) vertical = false;
        }
        Check(sideways > 150 && forwardBack > 150, "x and z disperse bombs from a stationary carrier");
        Check(vertical, "bombs get no vertical push");
        auto *onlyZ = shooter.Fire(weapon, 0, 0, 25, 0.0f);
        Check(onlyZ && std::fabs(Velocity(onlyZ).x) < 1e-4f, "z alone does not push sideways");
    }

    // Other projectile classes keep the old rotation spread, including the Y of weapon_spread_xyz.
    auto *rocket = shooter.Fire(3, 25, 25, 25, 0.0f);
    Check(rocket && std::fabs(rocket->_fly_dir_length - 100.0f) < 1e-3f, "rocket speed unchanged by spread");
    bool rocketVaries = false;
    for (int i = 0; i < 50 && !rocketVaries; ++i) {
        auto *r = shooter.Fire(3, 0, 25, 0, 0.0f);
        rocketVaries = r && std::fabs(r->_fly_dir.y) > 0.01f;
    }
    Check(rocketVaries, "rocket still uses weapon_spread_xyz Y");

    std::printf("CHECKS=%d FAILED=%d\n", checks, failures);
    return failures ? 1 : 0;
}
