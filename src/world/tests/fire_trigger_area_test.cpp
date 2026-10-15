// Fire trigger area: ai_fire_trigger_area_below_xz parsing, the AI fire volume of bomb-like weapons
// (geometry, fire_x span, enemy scan, shot retargeting) and the legacy
// fallback when the parameter is absent or invalid.
#include "game/ypabact_internal.h"
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
bool Near(const vec3d &a, const vec3d &b) { return (a - b).length() < 1e-3f; }
bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

struct WorldProbe : NC_STACK_ypaworld {
    WorldProbe() {
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
struct Unit : NC_STACK_ypabact {
    void Bind(WorldProbe &w, int owner, int gid, const vec3d &pos) {
        _world = &w; _owner = owner; _gid = gid; _vehicleID = 0;
        _position = _old_pos = pos; _rotation = mat3x3::Ident();
        _bact_type = BACT_TYPES_FLYER; _status = BACT_STATUS_NORMAL;
        _status_flg = 0; _oflags = 0; _radius = _viewer_radius = 30;
        _energy = _energy_max = 1000;
        _vp_normal = _vp_genesis = nullptr; _soundcarrier.Resize(7);
        _fire_pos = vec3d(0, 0, 0);
        _fire_x_mode = World::TVhclProto::FIRE_X_MODE_VANILLA; _fire_x_advanced = false;
        _fire_x_start = 0; _fire_x_step = 0; _fire_x_slots = 0; _fire_x_slot_index = 0;
        _cellRef = w.SectorAt(World::PositionToSectorID(pos)).unitsList.push_back(this);
    }
};
}

int main() {
    WorldProbe w;
    Check(Parse(w, {"new_weapon 1", "model = bomb", "ai_fire_trigger_area_below_xz = 200_400", "end",
                    "new_weapon 2", "model = homing_bomb", "ai_fire_trigger_area_below_xz = 100_50", "end",
                    "new_weapon 3", "model = rocket", "ai_fire_trigger_area_below_xz = 200_400", "end",
                    "new_weapon 4", "model = bomb", "ai_fire_trigger_area_below_xz = 200", "end",
                    "new_weapon 5", "model = bomb", "ai_fire_trigger_area_below_xz = -1_100", "end",
                    "new_weapon 6", "model = bomb", "ai_fire_trigger_area_below_xz = nan_100", "end",
                    "new_weapon 7", "model = bomb", "end",
                    "new_weapon 8", "model = laser", "vertical_laser_enable = 1", "ai_fire_trigger_area_below_xz = 300_300", "end",
                    "new_weapon 9", "model = laser", "ai_fire_trigger_area_below_xz = 300_300", "end"}),
          "weapons parse");
    const auto &wp = w._weaponProtos;
    Check(wp[1].ai_fire_trigger_area_x == 200.0f && wp[1].ai_fire_trigger_area_z == 400.0f && wp[1].HasAIFireTriggerArea(), "bomb fire trigger area parsed");
    Check(wp[2].HasAIFireTriggerArea(), "homing_bomb accepts the fire trigger area");
    Check(!wp[3].HasAIFireTriggerArea(), "rocket ignores the fire trigger area");
    Check(!wp[4].HasAIFireTriggerArea() && wp[4].ai_fire_trigger_area_x == 0.0f, "wrong value count disables the box");
    Check(!wp[5].HasAIFireTriggerArea() && wp[5].ai_fire_trigger_area_x == 0.0f, "negative size disables the box");
    Check(!wp[6].HasAIFireTriggerArea(), "invalid size disables the box");
    Check(!wp[7].HasAIFireTriggerArea() && wp[7].ai_fire_trigger_area_x == 0.0f, "absent parameter keeps the box off");
    Check(wp[8].HasAIFireTriggerArea(), "vertical laser accepts the fire trigger area");
    Check(!wp[9].HasAIFireTriggerArea(), "normal laser ignores the fire trigger area");

    const vec3d P(2400, -300, -2400);
    Unit shooter; shooter.Bind(w, 1, 1, P);
    shooter._fire_pos = vec3d(10, 5, 20);

    // Geometry: top on the fire point at X = 0, X span +-fire_x plus half width.
    TFireTriggerArea box;
    Check(!shooter.GetAIFireTriggerArea(wp[7], &box), "no box without ai_fire_trigger_area_below_xz");
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "box built for the bomb");
    const vec3d top = P + vec3d(0, 5, 20);
    Check(Near(box.top, top), "box top is the fire point");
    Check(Near(box.minX, -110) && Near(box.maxX, 110) && Near(box.halfZ, 200), "box extents");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(0, 500, 0), 0), "point under the fire point is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(0, -1, 0), 0), "point above the fire point is outside");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(105, 300, 190), 0), "point near a corner is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(0, 300, 201), 0), "point past the length is outside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(150, 300, 0), 35), "footprint short of the side is outside");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(150, 300, 0), 45), "footprint touching the side is inside");

    // Heading: the box turns with the unit and its forward axis is the unit nose.
    shooter._rotation = mat3x3::RotateY(C_PI_2);
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "box built after a yaw");
    vec3d nose = shooter._rotation.AxisZ().X0Z(); nose.normalise();
    vec3d side = shooter._rotation.AxisX().X0Z(); side.normalise();
    Check(Near(box.forward, nose) && Near(box.right, side), "box axes follow the heading");
    Check(ypabact_FireTriggerAreaContains(box, box.top + nose * 180 + vec3d(0, 300, 0), 0), "inside along the nose");
    Check(!ypabact_FireTriggerAreaContains(box, box.top + side * 180 + vec3d(0, 300, 0), 0), "outside along the side");

    // Pitch: the box stays vertical, only its top follows the fire point.
    shooter._rotation = mat3x3::RotateX(0.5);
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "box built while pitched");
    Check(Near(box.forward.y, 0) && Near(box.right.y, 0), "pitched box stays vertical");
    Check(Near(box.top, P + shooter._rotation.Transpose().Transform(vec3d(0, 5, 20))), "pitched top is the real fire point");
    shooter._rotation = mat3x3::Ident();

    // fire_x span: vanilla +-fire_x, rack for advanced sequence/random only.
    float minX = 0, maxX = 0;
    ypabact_GetFireXSpan(&shooter, &minX, &maxX);
    Check(Near(minX, -10) && Near(maxX, 10), "vanilla span is +-fire_x");
    shooter._fire_x_advanced = true; shooter._fire_x_mode = World::TVhclProto::FIRE_X_MODE_SEQUENCE;
    shooter._fire_x_start = 30; shooter._fire_x_step = -20; shooter._fire_x_slots = 4;
    ypabact_GetFireXSpan(&shooter, &minX, &maxX);
    Check(Near(minX, -30) && Near(maxX, 30), "rack span covers every slot");
    shooter._fire_x_mode = World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE;
    ypabact_GetFireXSpan(&shooter, &minX, &maxX);
    Check(Near(minX, -10) && Near(maxX, 10), "salve modes keep +-fire_x");
    shooter._fire_x_advanced = false; shooter._fire_x_mode = World::TVhclProto::FIRE_X_MODE_VANILLA;

    // Fire gate: sector point, unit target, then any enemy inside the box.
    shooter.GetAIFireTriggerArea(wp[1], &box);
    size_t fire = 7;
    bact_arg101 sector;
    sector.unkn = 1; sector.pos = top + vec3d(0, 300, 0);
    Check(shooter.CheckAIFireTriggerArea(wp[1], &sector, &fire) && fire == 1 && !sector.fire_trigger_area_target,
          "sector point inside fires on the sector");
    Check(!shooter.CheckAIFireTriggerArea(wp[3], &sector, &fire), "rocket keeps the legacy fire test");
    bact_arg101 outside;
    outside.unkn = 2; outside.radius = 10; outside.pos = top + vec3d(1000, 300, 0);
    Check(shooter.CheckAIFireTriggerArea(wp[1], &outside, &fire) && fire == 0, "no enemy inside: no fire");
    bact_arg101 edge;
    edge.unkn = 2; edge.radius = 45; edge.pos = top + vec3d(150, 300, 0);
    Check(shooter.CheckAIFireTriggerArea(wp[1], &edge, &fire) && fire == 1 && !edge.fire_trigger_area_target,
          "unit target touching the box fires on the target");

    Unit enemy; enemy.Bind(w, 2, 2, top + vec3d(50, 300, 50));
    Check(shooter.CheckAIFireTriggerArea(wp[1], &outside, &fire) && fire == 1 && outside.fire_trigger_area_target == &enemy,
          "enemy inside the box triggers the shot");
    outside.launch_pos = top;
    bact_arg79 shot;
    shot.tgType = BACT_TGT_TYPE_CELL; shot.direction = vec3d(1, 0, 0);
    ypabact_AimAtFireTriggerAreaTarget(outside, &shot);
    vec3d toEnemy = enemy._position - top; toEnemy.normalise();
    Check(shot.tgType == BACT_TGT_TYPE_UNIT && shot.target.pbact == &enemy && Near(shot.direction, toEnemy),
          "shot is aimed at the enemy inside the box");
    enemy._owner = 1;
    Check(shooter.CheckAIFireTriggerArea(wp[1], &outside, &fire) && fire == 0, "friendly unit does not trigger");
    enemy._owner = 0;
    Check(shooter.CheckAIFireTriggerArea(wp[1], &outside, &fire) && fire == 0, "neutral unit does not trigger");
    enemy._owner = 2;
    enemy._position = top + vec3d(50, -20, 50);
    Check(shooter.CheckAIFireTriggerArea(wp[1], &outside, &fire) && fire == 0, "enemy above the fire point does not trigger");

    // Vertical laser zone: legacy cylinder without a box, the box when present.
    const vec3d origin = top;
    Check(ypabact_IsInVerticalLaserFireZone(nullptr, origin, 300, origin + vec3d(0, 10, 250), 0) &&
          !ypabact_IsInVerticalLaserFireZone(nullptr, origin, 300, origin + vec3d(0, 10, 350), 0) &&
          !ypabact_IsInVerticalLaserFireZone(nullptr, origin, 300, origin + vec3d(0, -10, 0), 0),
          "vertical laser keeps the legacy cylinder without a box");
    Check(shooter.GetAIFireTriggerArea(wp[8], &box), "vertical laser box built");
    Check(ypabact_IsInVerticalLaserFireZone(&box, origin, 300, top + vec3d(0, 10, 140), 0) &&
          !ypabact_IsInVerticalLaserFireZone(&box, origin, 300, top + vec3d(0, 10, 250), 0),
          "vertical laser uses the box when present");

    std::printf("CHECKS=%d FAILED=%d\n", checks, failures);
    return failures ? 1 : 0;
}
