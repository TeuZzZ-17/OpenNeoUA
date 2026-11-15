// AI fire volumes: parsing, launch selection and projectile-side compatibility.
#include "game/ypabact_internal.h"
#include "game/yw_internal.h"
#include "game/ypatank.h"
#include "game/ypacar.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
bool Near(const vec3d &a, const vec3d &b) { return (a - b).length() < 1e-3f; }
bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

struct WorldProbe : NC_STACK_ypaworld {
    NC_STACK_base visual;
    WorldProbe() {
        _vhclModels.resize(256, &visual);
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    // The fixture contains units but no terrain or building assets.
    void ypaworld_func136(ypaworld_arg136 *arg) override { arg->isect = false; }
    void ypaworld_func180(yw_arg180 *) override {}
    void ypaworld_func149(ypaworld_arg136 *arg) override { arg->isect = false; }
};
bool ParseWeapons(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::WeaponProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
bool ParseVehicles(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
bool ParseBuildings(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::BuildProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
struct Unit : NC_STACK_ypabact {
    bool Debuffed() const { return _active_debuff.active; }
    void ModifyEnergy(bact_arg84 *arg) override { _energy += arg->energy; }
    void Bind(WorldProbe &w, int owner, int gid, const vec3d &pos) {
        _world = &w; _owner = owner; _gid = gid; _vehicleID = 0;
        _wrldSize = vec2d(9600, -9600);
        _position = _old_pos = pos; _rotation = mat3x3::Ident();
        _bact_type = BACT_TYPES_FLYER; _status = BACT_STATUS_NORMAL;
        _status_flg = 0; _oflags = 0; _radius = _viewer_radius = 30;
        _energy = _energy_max = 1000;
        _vp_normal = _vp_genesis = nullptr; _soundcarrier.Resize(7);
        _fire_pos = vec3d(0, 0, 0);
        _fire_x_mode = World::TVhclProto::FIRE_X_MODE_VANILLA; _fire_x_advanced = false;
        _fire_x_start = 0; _fire_x_step = 0; _fire_x_slots = 0; _fire_x_slot_index = 0;
        _fire_x_random_seeded = false; _fire_x_random_state = 0; _fire_x_random_order.clear();
        _cellId = World::PositionToSectorID(pos);
        _pSector = &w.SectorAt(_cellId);
        _cellRef = w.SectorAt(World::PositionToSectorID(pos)).unitsList.push_back(this);
    }
};

} // namespace
int main() {
    WorldProbe w;
    Check(ParseVehicles(w, {"new_vehicle 1", "model = plane", "num_targets = 3", "num_weapons = 4", "end",
                            "new_vehicle 2", "model = plane", "num_targets = -1", "end"}), "vehicle target capacity parsed");
    Check(w._vhclProtos[1].num_targets == 3 && w._vhclProtos[2].num_targets == 0, "target capacity default and invalid value");
    Check(ParseWeapons(w, {"new_weapon 1", "model = laser", "energy = 100", "laser_chain_energy_mult = 1.25",
                          "ai_fire_trigger_box_forward_xyz = 400|400|1200", "end"}), "laser fixture parsed");
    auto &proto = w._weaponProtos[1];
    Check(Near(proto.laser_chain_energy_mult, 1.25f), "chain multiplier accepts amplification");
    Check(Near(ypabact_LaserNominalTickEnergy(proto, 0, 1.25f*1.25f), 156.25f), "chain amplification compounds");
    Unit shooter; shooter.Bind(w, 1, 101, vec3d(2400, -300, -2400));
    Unit enemy; enemy.Bind(w, 2, 102, shooter._position + vec3d(0, 0, 300));
    Unit other; other.Bind(w, 2, 103, shooter._position + vec3d(150, 0, 500));
    shooter._weapon = 1; shooter._num_weapons = 4;
    shooter._weapon_projectile_counts[0] = shooter._weapon_projectile_count_maxs[0] = 4;
    shooter._num_targets = 1; shooter._fire_pos = vec3d(30, 0, 10);
    bact_arg79 shot{}; shot.start_point = shooter._fire_pos; shot.tgType = BACT_TGT_TYPE_UNIT;
    shot.target.pbact = &enemy; shot.direction = vec3d(0, 0, 1);
    shooter.RequestLaserFire(1, &shot);
    Check(shooter._laser_requests.size() == 4, "num_weapons creates four beams");
    Check(Near(shooter._laser_requests[0].start, shooter._position+vec3d(-30,0,10)) &&
          Near(shooter._laser_requests[3].start, shooter._position+vec3d(30,0,10)), "beams span real fire positions");
    auto requests = shooter._laser_requests;
    ypabact_AddLaserMultiTargetRequests(&shooter, proto, &requests, 1200, false, false);
    Check(requests.size() == 4 && requests[0].target == &enemy && requests[3].target == &enemy,
          "one target concentrates every beam");
    shooter._num_targets = 2;
    ypabact_AddLaserMultiTargetRequests(&shooter, proto, &requests, 1200, false, false);
    Check(requests[0].target == &enemy && requests[1].target == &other && requests[2].target == &enemy &&
          requests[3].target == &other, "four beams distribute across two targets without changing origins");
    other._position = shooter._position+vec3d(2000,0,100);
    requests = shooter._laser_requests;
    ypabact_AddLaserMultiTargetRequests(&shooter, proto, &requests, 1200, false, false);
    Check(requests[1].target == &enemy && requests[3].target == &enemy, "missing secondary target reuses primary");
    other._position = shooter._position+vec3d(150,0,500);
    shooter._shield = enemy._shield = other._shield = 0;
    shooter._clock = 100;
    update_msg update{};
    shooter.UpdateLaser(&update);
    Check(shooter._laser_beams.size() == 4, "runtime updates all four muzzle beams");
    Check(enemy._energy == 800 && other._energy == 800, "two beams per target apply independent damage ticks");
    const auto sources = shooter._laser_muzzle_x;
    shooter._laser_requests.clear(); shooter._laser_active = true;
    shooter.RequestLaserFire(1, &shot);
    Check(shooter._laser_muzzle_x == sources, "held beams keep their chosen sources");
    shooter.StopLaser();
    shooter._fire_x_mode = World::TVhclProto::FIRE_X_MODE_SEQUENCE;
    shooter.RequestLaserFire(1, &shot);
    Check(shooter._laser_muzzle_x == std::vector<float>({-30,30,-30,30}), "laser reuses sequence fire_x");
    shooter.StopLaser();
    shooter._weapon_projectile_counts[1] = shooter._weapon_projectile_count_maxs[1] = 2;
    shooter.RequestLaserFire(1, &shot, 1);
    Check(shooter._laser_requests.size() == 2, "secondary slot uses its own num_weapons count");
    shooter.StopLaser();
    proto.ai_fire_trigger_box_forward = vec3d(); proto.ai_fire_trigger_box_below = vec3d(400,1200,400);
    shot.tgType = BACT_TGT_TYPE_DRCT; shot.direction = vec3d(0,0,1);
    shooter.RequestLaserFire(1, &shot);
    Check(Near(shooter._laser_requests[0].dir, vec3d(0,1,0)), "below volume selects world-down free fire");
    shooter.StopLaser();
    enemy._position = shooter._position + vec3d(0,300,0);
    other._position = shooter._position + vec3d(100,500,0);
    shot.tgType = BACT_TGT_TYPE_UNIT; shot.target.pbact = &enemy;
    shooter._clock = 200;
    shooter.RequestLaserFire(1, &shot);
    shooter.UpdateLaser(&update);
    Check(shooter._laser_beams.size() == 4 && shooter._laser_beams[0].target_gid == enemy._gid &&
          shooter._laser_beams[1].target_gid == other._gid, "downward beams share muzzle count and target distribution");
    shooter.StopLaser();
    proto.energy = 0; proto.debuff.valid = true; proto.debuff.duration = 1000;
    NC_STACK_ypabact::TLaserBeamRuntime beam;
    shooter._clock = 100;
    const int energy = enemy._energy;
    ypabact_ApplyLaserUnitTick(&shooter, proto, &enemy, beam, false, 1.0f);
    Check(enemy.Debuffed() && enemy._energy == energy, "zero-damage laser applies direct debuff");
    Unit armored; armored.Bind(w, 2, 105, enemy._position); armored._shield = 100;
    proto.energy = 100; beam = {};
    ypabact_ApplyLaserUnitTick(&shooter, proto, &armored, beam, false, 1.0f);
    Check(armored.Debuffed() && armored._energy == 1000, "full armor blocks damage but not eligible debuffs");
    Unit ally; ally.Bind(w, 1, 104, enemy._position); beam = {};
    ypabact_ApplyLaserUnitTick(&shooter, proto, &ally, beam, false, 1.0f);
    Check(!ally.Debuffed(), "AI debuff laser preserves allies");
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
    return failures ? 1 : 0;
}
