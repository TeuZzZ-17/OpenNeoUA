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
        _fire_x_random_seeded = false; _fire_x_random_state = 0; _fire_x_random_order.clear();
        _cellId = World::PositionToSectorID(pos);
        _pSector = &w.SectorAt(_cellId);
        _cellRef = w.SectorAt(World::PositionToSectorID(pos)).unitsList.push_back(this);
    }
};
struct FightProbe : Unit {
    int launches = 0;
    size_t LaunchMissile(bact_arg79 *) override { ++launches; return 1; }
};
struct SectorFightProbe : FightProbe {
    bact_arg79 lastLaunch{};
    int minigunCalls = 0, minigunStops = 0;
    size_t FireMinigun(bact_arg105 *) override { ++minigunCalls; return 1; }
    void SetState(setState_msg *arg) override {
        if (arg->unsetFlags & BACT_STFLAG_FIRE) ++minigunStops;
        _status_flg = (_status_flg & ~arg->unsetFlags) | arg->setFlags;
    }
    // Isolate fire control from the map's building layout; keep the selected part fixed.
    void GetBestSectorPart(vec3d *) override {}
    size_t LaunchMissile(bact_arg79 *arg) override {
        lastLaunch = *arg;
        return FightProbe::LaunchMissile(arg);
    }
};
struct MgunWallWorld : WorldProbe {
    void ypaworld_func149(ypaworld_arg136 *arg) override {
        arg->isect = true; arg->tVal = 0.25f;
        arg->isectPos = arg->stPos + arg->vect*arg->tVal;
    }
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 0; }
};
struct MgunDamageProbe : Unit {
    int buildingDamageCalls = 0;
    void ChangeSectorEnergy(yw_arg129 *) override { ++buildingDamageCalls; }
};
struct TankProbe : NC_STACK_ypatank {
    void Bind(WorldProbe &w) {
        _world = &w; _vehicleID = 0; _owner = 1;
        _position = vec3d(2400, 0, -2400); _rotation = mat3x3::Ident();
        _bact_type = BACT_TYPES_TANK; _oflags = _status_flg = 0;
        _status = BACT_STATUS_NORMAL; _fire_pos = vec3d();
        _primTtype = _secndTtype = BACT_TGT_TYPE_NONE;
        _weapon = 10; _mgun_set = true; _mgun = 1;
    }
};
struct CarProbe : NC_STACK_ypacar {
    void Bind(WorldProbe &w) { _world = &w; }
};
struct GunProbe : NC_STACK_ypagun {
    int launches = 0;
    int stateChanges = 0;
    bact_arg79 lastLaunch{};
    size_t LaunchMissile(bact_arg79 *arg) override {
        ++launches;
        lastLaunch = *arg;
        return 1;
    }
    void SetState(setState_msg *) override { ++stateChanges; }
    void Bind(WorldProbe &w, const World::TVhclProto &proto, int owner, int gid, const vec3d &pos) {
        _world = &w; _owner = owner; _gid = gid; _vehicleID = 20;
        _position = _old_pos = pos; _rotation = mat3x3::Ident();
        _bact_type = BACT_TYPES_GUN; _status = BACT_STATUS_NORMAL;
        _status_flg = 0; _oflags = 0; _radius = _viewer_radius = 30;
        _energy = _energy_max = 1000;
        _vp_normal = _vp_genesis = nullptr; _soundcarrier.Resize(7);
        _fire_pos = vec3d(0, 0, 0);
        _weapon = proto.weapon; _extra_weapons = proto.extra_weapons;
        _weapon_ai_switch_mode = proto.weapon_ai_switch_mode;
        _num_weapons = (uint8_t)proto.num_weapons_min;
        _weapon_projectile_counts[0] = (uint8_t)proto.num_weapons_min;
        _weapon_projectile_count_maxs[0] = (uint8_t)proto.num_weapons_max;
        for (size_t i = 0; i < proto.extra_num_weapons.size(); ++i) {
            const int minCount = proto.extra_num_weapons_min[i] > 0
                ? proto.extra_num_weapons_min[i] : proto.num_weapons_min;
            const int maxCount = proto.extra_num_weapons_max[i] > 0
                ? proto.extra_num_weapons_max[i] : proto.num_weapons_max;
            _weapon_projectile_counts[i + 1] = (uint8_t)minCount;
            _weapon_projectile_count_maxs[i + 1] = (uint8_t)maxCount;
        }
        _weapon_slot_index = 0;
        _gunType = GUN_TYPE_REAL; _gunFireTime = 600; _gunFireCount = 0;
        _gunRott = vec3d(0, 1, 0); _gunBasis = vec3d(0, 0, 1);
        _gunMaxUp = _gunMaxDown = C_PI_2; _gunMaxSide = C_PI;
        _cellRef = w.SectorAt(World::PositionToSectorID(pos)).unitsList.push_back(this);
    }
};

void TestBelowCompatibility(WorldProbe &w, const World::TWeapProto *wp) {
    for (const char *invalid : {"200_0_400", "200_-1_400", "200_nan_400", "200_inf_400",
                                "200_1200", "200_1200_400_1", "200_1200_0"}) {
        Check(ParseWeapons(w, {"new_weapon 50", "model = bomb",
                               std::string("ai_fire_trigger_box_below_xyz = ") + invalid, "end"}) &&
              w._weaponProtos[50].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE,
              "invalid or incomplete below-box depth/length preserves legacy behavior");
    }
    Check(ParseWeapons(w, {"modify_weapon 50", "ai_fire_trigger_box_below_xyz = 200_1200_400", "end",
                           "modify_weapon 50", "ai_fire_trigger_box_below_xyz = 0", "end"}) &&
          !w._weaponProtos[50].HasAIFireTriggerArea(), "single zero disables a previously valid finite below box");
    const vec3d P(2400, -300, -2400);
    Unit shooter; shooter.Bind(w, 1, 1, P);
    shooter._fire_pos = vec3d(10, 5, 20);

    TFireTriggerArea box;
    Check(!shooter.GetAIFireTriggerArea(wp[7], &box), "absent volume keeps legacy test");
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "below box built for bomb");
    const vec3d top = P + vec3d(0, 5, 20);
    Check(Near(box.top, top), "below box top is the fire point");
    Check(Near(box.minX, -110) && Near(box.maxX, 110) && Near(box.halfY, 200) && Near(box.depth, 1200), "below box extents and fire_x span");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(0, 500, 0), 0), "point below the fire point is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(0, -1, 0), 0), "point above the fire point is outside");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(105, 300, 190), 0), "point near below-box corner is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(0, 300, 201), 0), "point past below-box depth is outside");
    Check(!ypabact_FireTriggerAreaContains(box, top + vec3d(150, 300, 0), 35), "sphere short of below-box side is outside");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(150, 300, 0), 45), "sphere touching below-box side is inside");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(0, 1200, 0), 0) &&
          !ypabact_FireTriggerAreaContains(box, top + vec3d(0, 1200.1, 0), 0),
          "below box has an inclusive finite vertical cap");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(0, 1210, 0), 10) &&
          !ypabact_FireTriggerAreaContains(box, top + vec3d(0, 1210.1, 0), 10),
          "below box tests sphere contact at its depth cap");
    Check(ypabact_FireTriggerAreaContains(box, top + vec3d(116, 1206, 205), 10) &&
          !ypabact_FireTriggerAreaContains(box, top + vec3d(116, 1206, 206), 10),
          "below box uses sphere distance at its three-dimensional corner");

    Unit enemy; enemy.Bind(w, 2, 2, top + vec3d(0, 1211, 0)); enemy._radius = 10;
    bact_arg101 miss{}; miss.unkn = 1; miss.pos = top + vec3d(1000, 500, 0);
    size_t fire = 1;
    Check(shooter.CheckAIFireTriggerArea(wp[1], &miss, &fire) && fire == 0,
          "fallback does not acquire an enemy beyond the below box depth");
    enemy._position.y -= 1;
    Check(shooter.CheckAIFireTriggerArea(wp[1], &miss, &fire) && fire == 1 &&
          miss.fire_trigger_area_target == &enemy, "fallback acquires enemy sphere touching the below cap");
    const std::vector<NC_STACK_ypabact *> excluded;
    enemy._position = top + vec3d(0, 1200, 0);
    Check(ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, top, 2000, 0, &box, excluded),
          "vertical laser secondary selection shares finite below-box geometry");
    enemy._position.y += 11;
    Check(!ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, top, 2000, 0, &box, excluded),
          "vertical laser cannot select secondary targets beyond below-box cap");
    enemy._position = top + vec3d(100, 100, 0);
    Check(!ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, top, 120, 0, &box, excluded),
          "finite below-box secondary selection respects actual beam reach as well as volume depth");

    shooter._rotation = mat3x3::RotateY(C_PI_2);
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "below box built after yaw");
    vec3d nose = shooter._rotation.AxisZ().X0Z(); nose.normalise();
    vec3d side = shooter._rotation.AxisX().X0Z(); side.normalise();
    Check(Near(box.up, nose) && Near(box.forward, vec3d(0, 1, 0)) && Near(box.right, side), "below box follows heading");
    Check(ypabact_FireTriggerAreaContains(box, box.top + nose * 180 + vec3d(0, 300, 0), 0), "below box contains point along nose");
    Check(!ypabact_FireTriggerAreaContains(box, box.top + side * 180 + vec3d(0, 300, 0), 0), "below box excludes point along side");

    shooter._rotation = mat3x3::RotateX(0.5);
    Check(shooter.GetAIFireTriggerArea(wp[1], &box), "below box built while pitched");
    Check(Near(box.up.y, 0) && Near(box.right.y, 0), "below box remains vertical under pitch");
    Check(Near(box.top, P + shooter._rotation.Transpose().Transform(vec3d(0, 5, 20))), "below box top follows pitched fire point");
}

void TestTankElevation() {
    WorldProbe w;
    Check(ParseWeapons(w, {"new_weapon 10", "model = rocket", "ai_fire_trigger_box_forward_xyz = 60_40_900", "end",
                           "new_weapon 11", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_10_900", "end",
                           "new_weapon 12", "model = rocket", "end",
                           "new_weapon 13", "model = laser", "life_time = 6000", "ai_fire_trigger_box_forward_xyz = 60_40_900", "end",
                           "new_weapon 14", "model = rocket", "ai_fire_trigger_box_forward_xyz = 1000_1000_900", "end",
                           "new_weapon 15", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 170_170_900", "end",
                           "new_weapon 16", "model = missile", "multi_target = 4", "ai_fire_trigger_box_forward_xyz = 1000_1000_900", "end"}),
          "tank elevation weapon fixtures parsed");
    TankProbe tank; tank.Bind(w);
    Unit enemy; enemy.Bind(w, 2, 500, tank._position + vec3d(0, -300, 500));
    tank._primTtype = BACT_TGT_TYPE_UNIT; tank._primT.pbact = &enemy;
    const vec3d hullForward = tank._rotation.AxisZ();
    const vec3d mgunAim = ypabact_GetMinigunFireDir(&tank, hullForward);
    const float userAngle = tank._gun_angle_user;
    TFireTriggerArea box;
    bact_arg101 fire{}; fire.unkn = 2; fire.radius = 10; fire.pos = enemy._position;
    for (int weapon : {10, 11}) {
        fire.weapon = weapon;
        Check(tank.GetAIFireTriggerArea(w._weaponProtos[weapon], &box) && box.forward.y < -0.4f &&
              ypabact_FireTriggerAreaContains(box, enemy._position, 0),
              "tank debug forward volume raises to the combat target before fire");
        Check(tank.CheckFireAI(&fire) == 1, "real tank AI acquires elevated enemy through box and cone");
        fire.pos = tank._position + vec3d(0, 100, 500);
        Check(tank.CheckFireAI(&fire) == 1, "tank fire query lowers to its exact point independently of the stored target");
        fire.pos = enemy._position;
        w._vhclProtos[0].scope_angle_min_max_set = true;
        w._vhclProtos[0].scope_angle_min_max = {{-10, 10}};
        Check(tank.CheckFireAI(&fire) == 0, "tank volume cannot bypass the authored scope elevation limits");
        w._vhclProtos[0].scope_angle_min_max_set = false;
    }
    // Compare against the real player reticle rather than a copied angle formula.
    vec3d reticleLimits[2];
    tank._gun_angle_user = tank.USER_GROUND_AIM_MIN;
    reticleLimits[0] = tank.GetUserWeaponAimDirection(); reticleLimits[0].normalise();
    tank._gun_angle_user = tank.USER_GROUND_AIM_MAX;
    reticleLimits[1] = tank.GetUserWeaponAimDirection(); reticleLimits[1].normalise();
    tank._gun_angle_user = userAngle;
    for (int weapon : {10, 11, 14, 15, 16}) {
        fire.weapon = weapon;
        for (int end = 0; end < 2; ++end) {
            const double sign = end == 0 ? 1.0 : -1.0;
            for (double forward : {0.0, 0.01, 100.0}) {
                enemy._position = tank._position + vec3d(0, sign*600, forward);
                fire.pos = enemy._position;
                Check(tank.CheckFireAI(&fire) == 0,
                      "AI ground volumes reject vertical and near-vertical targets even with broad shapes and no authored scope");
                Check(tank.GetAIFireTriggerArea(w._weaponProtos[weapon], &box) && Near(box.forward, reticleLimits[end]),
                      "debug volume stops at the same upper/lower limit as the vanilla reticle");
                fire.unkn = 1;
                Check(tank.CheckFireAI(&fire) == 0, "sector targets cannot bypass the ground elevation limit");
                fire.unkn = 2;
            }
            enemy._position = tank._position + reticleLimits[end]*500;
            fire.pos = enemy._position;
            Check(tank.CheckFireAI(&fire) == 1, "AI ground forward volume accepts both inclusive reticle endpoints");
        }
    }
    w._vhclProtos[0].scope_angle_min_max_set = true;
    w._vhclProtos[0].scope_angle_min_max = {{-10, 10}};
    enemy._position = tank._position + vec3d(0, -600, 100);
    tank._gun_angle_user = tank.USER_GROUND_AIM_MAX*0.1f;
    vec3d restrictedAxis = tank.GetUserWeaponAimDirection(); restrictedAxis.normalise();
    tank._gun_angle_user = userAngle;
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[10], &box) && Near(box.forward, restrictedAxis),
          "authored scope narrows the debug elevation as well as firing permission");
    w._vhclProtos[0].scope_angle_min_max_set = false;

    tank._weapon = 16;
    bact_arg79 shot{}; shot.weapon = 16; shot.weapon_source_slot = 0; shot.g_time = 1000;
    shot.tgType = BACT_TGT_TYPE_UNIT; shot.target.pbact = &enemy; shot.direction = enemy._position-tank._position;
    tank._weapon_time = 100; tank._energy = 1000;
    std::srand(123); const int expectedRandom = std::rand(); std::srand(123);
    Check(tank.LaunchMissile(&shot) == 0 && tank._weapon_time == 100 && tank._energy == 1000 &&
          std::rand() == expectedRandom, "direct launch cannot bypass elevation or consume cadence, energy or random values");
    auto targets = ypabact_CollectMissileMultiTargets(&tank, &shot, w._weaponProtos[16], 4);
    Check(targets.empty(), "missile multi-target cannot select a vertically unreachable primary target");
    World::TWeapProto legacyMissile = w._weaponProtos[16]; legacyMissile.ai_fire_trigger_box_forward = vec3d();
    targets = ypabact_CollectMissileMultiTargets(&tank, &shot, legacyMissile, 4);
    Check(!targets.empty() && targets[0] == &enemy, "missile multi-target without a volume retains its legacy elevation rules");
    const std::vector<NC_STACK_ypabact *> excluded;
    vec3d laserAim = enemy._position-tank._position; laserAim.normalise();
    Check(ypabact_FindNearestLaserMultiTarget(&tank, tank._position, laserAim, 1000, false, excluded, true) == nullptr &&
          ypabact_FindNearestLaserMultiTarget(&tank, tank._position, laserAim, 1000, false, excluded) == &enemy,
          "laser secondary search enforces the bounded AI elevation only when requested by a forward volume");
    Check(ypabact_LaserAutoTarget(&tank, tank._position, laserAim, 1000, 0.4f, true) == nullptr &&
          ypabact_LaserAutoTarget(&tank, tank._position, laserAim, 1000, 0.4f) == &enemy,
          "AI laser auto-aim cannot acquire a target above its mechanical travel");
    CarProbe car; car.Bind(w); car._vehicleID = 0; car._bact_type = BACT_TYPES_CAR;
    car._position = tank._position; car._rotation = mat3x3::Ident(); car._oflags = 0; car._fire_pos = vec3d();
    fire.weapon = 10; fire.pos = enemy._position;
    Check(car.GetAIFireTriggerArea(w._weaponProtos[10], &box, &fire.pos) && Near(box.forward, reticleLimits[1]) &&
          car.CheckFireAI(&fire) == 0, "wheeled ground units share the bounded aim and cannot fire vertically");
    tank._rotation = mat3x3::RotateY(0.6)*mat3x3::RotateX(0.2)*mat3x3::RotateZ(0.1);
    enemy._position = tank._position + tank._rotation.Transpose().Transform(vec3d(0, -300, 500));
    vec3d raisedDirection = enemy._position - tank._position; raisedDirection.normalise();
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[10], &box) &&
          Near(box.forward, raisedDirection) &&
          Near(box.right, tank._rotation.AxisX()), "tank elevation preserves hull yaw, pitch and roll without extra horizontal aim");
    tank._rotation = mat3x3::Ident();
    enemy._position = tank._position + vec3d(0, -300, 500);
    fire.pos = enemy._position;
    fire.weapon = 12;
    Check(tank.CheckFireAI(&fire) == 1 && !tank.GetAIFireTriggerArea(w._weaponProtos[12], &box),
          "tank without a volume retains its existing elevated-target firing test");
    tank._oflags = BACT_OFLAG_USERINPT;
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[10], &box) && Near(box.forward, hullForward),
          "AI elevation does not change manually controlled weapon axes");
    tank._oflags = 0;
    tank._primTtype = BACT_TGT_TYPE_NONE;
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[10], &box) && Near(box.forward, hullForward),
          "tank volume returns to hull elevation when its combat target is cleared");
    tank._primTtype = BACT_TGT_TYPE_UNIT;
    enemy._position = tank._position + vec3d(300, -300, 500);
    fire.pos = enemy._position; fire.weapon = 10;
    Check(tank.CheckFireAI(&fire) == 0, "vertical tank aim cannot rotate a narrow box sideways to accept an unaligned target");
    enemy._position = tank._position + vec3d(0, -200, 500);
    fire.pos = enemy._position; fire.weapon = 13;
    tank._laser_active = true; tank._laser_weapon = 13;
    Check(tank.CheckFireAI(&fire) == 1 && ypabact_ShouldPersistAILaserFire(&tank, 13, &fire, &enemy),
          "tank continuous laser acquisition and retention use the same raised axis");
    enemy._position = tank._position + vec3d(0, -600, 100); fire.pos = enemy._position;
    Check(tank.CheckFireAI(&fire) == 0 && !ypabact_ShouldPersistAILaserFire(&tank, 13, &fire, &enemy),
          "an already active laser stops when its target exceeds the vanilla reticle elevation");
    tank.GetAIFireTriggerArea(w._weaponProtos[10], &box);
    Check(Near(tank._rotation.AxisZ(), hullForward) && Near(tank._gun_angle_user, userAngle) &&
          Near(ypabact_GetMinigunFireDir(&tank, hullForward), mgunAim),
          "primary volume elevation never changes hull, player reticle or independent MGUN direction");
    fire.weapon = -1; tank._weapon = -1;
    fire.pos = tank._position + vec3d(0, -100, 500);
    const size_t vanillaMgun = tank.CheckFireAI(&fire);
    Check(vanillaMgun == 1, "MGUN-only tank still accepts its normal vanilla forward target");
    w._weaponProtos[10].ai_fire_trigger_box_forward = vec3d();
    Check(tank.CheckFireAI(&fire) == vanillaMgun && tank.HasMinigun(),
          "MGUN-only fallback is identical with or without configured primary volumes");

    World::TVhclProto gunProto; gunProto.weapon = 14; gunProto.num_weapons_min = gunProto.num_weapons_max = 1;
    GunProbe gun; gun.Bind(w, gunProto, 1, 502, tank._position);
    gun._gunMaxUp = 0.4f; gun._gunMaxDown = 0.2f; gun._gunMaxSide = 0.3f;
    for (const vec3d &direction : {vec3d(0, -0.6, 1), vec3d(0, 0.4, 1), vec3d(0.6, 0, 1)}) {
        enemy._position = gun._position + direction*400;
        fire.pos = enemy._position; fire.weapon = 14;
        size_t fireAllowed = 1;
        Check(gun.CheckAIFireTriggerArea(w._weaponProtos[14], &fire, &fireAllowed) && fireAllowed == 0 &&
              !gun.TestTargetSector(&enemy), "wide turret volume cannot bypass native up, down or side stops");
    }
    enemy._position = gun._position + vec3d(0, -0.3, 1)*400;
    fire.pos = enemy._position;
    size_t fireAllowed = 0;
    Check(gun.CheckAIFireTriggerArea(w._weaponProtos[14], &fire, &fireAllowed) && fireAllowed == 1 &&
          gun.TestTargetSector(&enemy), "turret forward volume accepts targets within its existing mechanical stops");
    enemy._position = gun._position + vec3d(0, -0.1, 1)*4000;
    Check(gun.IsPrimaryWeaponElevationAllowed(enemy._position-gun._position, true) &&
          !gun.TestTargetSector(&enemy), "shared turret aim limits do not import its legacy target-search distance");
}

void TestDownwardCone(WorldProbe &w) {
    Check(ParseWeapons(w, {
        "new_weapon 22", "ai_fire_trigger_cone_below_xyz = 90_60_500", "model = bomb", "end",
        "new_weapon 23", "model = homing_bomb", "ai_fire_trigger_cone_below_xyz = 90_60_500", "end",
        "new_weapon 24", "model = laser", "vertical_laser_enable = 1", "life_time = 2000",
        "ai_fire_trigger_cone_below_xyz = 90_60_500", "end",
        "new_weapon 25", "model = rocket", "ai_fire_trigger_cone_below_xyz = 90_60_500", "end",
        "new_weapon 26", "model = laser", "ai_fire_trigger_cone_below_xyz = 90_60_500", "end",
        "new_weapon 27", "model = bomb", "ai_fire_trigger_box_below_xyz = 200_1200_200",
        "ai_fire_trigger_cone_below_xyz = 90_60_500", "end",
        "new_weapon 28", "model = bomb", "ai_fire_trigger_cone_below_xyz = 180_60_500", "end",
        "new_weapon 29", "model = bomb", "ai_fire_trigger_cone_below_xyz = 90_nan_500", "end",
        "new_weapon 30", "model = bomb", "ai_fire_trigger_cone_below_xyz = 0", "end"
    }), "downward-cone parser fixtures loaded");
    for (int weapon : {22, 23, 24})
        Check(w._weaponProtos[weapon].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_CONE_BELOW,
              "bomb, homing bomb and vertical laser accept the downward cone");
    for (int weapon : {25, 26, 27, 28, 29, 30})
        Check(!w._weaponProtos[weapon].HasAIFireTriggerArea(),
              "unsupported, conflicting, invalid or disabled downward cone retains legacy control");
    Check(ParseWeapons(w, {"modify_weapon 27", "ai_fire_trigger_box_below_xyz = 0", "end"}) &&
          w._weaponProtos[27].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_CONE_BELOW,
          "clearing a box allows switching to the downward cone");

    Unit shooter; shooter.Bind(w, 1, 410, vec3d(2400, -500, -2400));
    shooter._fire_pos = vec3d(0, 5, 20);
    TFireTriggerArea area;
    const auto &weapon = w._weaponProtos[22];
    Check(shooter.GetAIFireTriggerArea(weapon, &area) && area.IsCone() && area.IsBelow(),
          "downward cone reuses the shared finite-cone representation");
    Check(Near(area.forward, vec3d(0, 1, 0)) && Near(area.depth, 500),
          "downward cone depth follows world-down rather than the nose");
    Check(ypabact_FireTriggerAreaContains(area, area.top + vec3d(0, 100, 0), 0),
          "point directly below the muzzle is inside the cone");
    Check(ypabact_FireTriggerAreaContains(area, area.top + area.forward*100 + area.right*60 + area.up*20, 0),
          "downward cone includes a point inside its elliptical cross section");
    Check(!ypabact_FireTriggerAreaContains(area, area.top + area.forward*100 + area.right*90 + area.up*50, 0),
          "downward cone excludes the rectangular corner outside its ellipse");
    Check(!ypabact_FireTriggerAreaContains(area, area.top - area.forward*0.01, 30),
          "enemy centre above the downward-cone origin does not trigger");
    Check(ypabact_FireTriggerAreaContains(area, area.top + area.forward*510, 10) &&
          !ypabact_FireTriggerAreaContains(area, area.top + area.forward*511, 10),
          "downward cone preserves sphere contact at its finite depth cap");
    shooter._rotation = mat3x3::RotateY(0.7)*mat3x3::RotateX(0.5)*mat3x3::RotateZ(0.3);
    Check(shooter.GetAIFireTriggerArea(weapon, &area) && Near(area.forward, vec3d(0, 1, 0)) &&
          Near(area.right.y, 0) && Near(area.up.y, 0), "yaw/pitch/roll never tilt the downward cone");
    Check(Near(area.top, shooter.GetBodyPosition() + shooter._rotation.Transpose().Transform(vec3d(0, 5, 20))),
          "tilted carrier still supplies the actual world-space muzzle origin");
    shooter._rotation = mat3x3::Ident();
    shooter.GetAIFireTriggerArea(weapon, &area);
    Unit enemy; enemy.Bind(w, 2, 411, area.top + area.forward*200 + area.right*20);
    bact_arg101 outside{}; outside.unkn = 1; outside.pos = area.top + area.right*1000;
    outside.launch_pos = area.top; outside.has_launch_pos = true;
    size_t fire = 0;
    Check(shooter.CheckAIFireTriggerArea(weapon, &outside, &fire) && fire == 1 &&
          outside.fire_trigger_area_target == &enemy, "downward cone finds an alternate enemy within its full volume");
    bact_arg79 shot{}; ypabact_AimAtFireTriggerAreaTarget(outside, &shot);
    Check(shot.tgType == BACT_TGT_TYPE_UNIT && shot.target.pbact == &enemy,
          "downward-cone fallback reaches the common launch target");

    const std::vector<NC_STACK_ypabact *> excluded;
    Check(ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, area.top, 1000, 0, &area, excluded),
          "vertical laser accepts secondary targets through the shared downward cone");
    Check(ypabact_FindNearestVerticalLaserTarget(&shooter, area.top, 1000, 0, &area, false, excluded) == &enemy,
          "configured downward cone replaces the legacy laser trigger radius during target search");
    enemy._position = area.top + area.forward*600;
    Check(!ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, area.top, 1000, 0, &area, excluded),
          "vertical laser does not select secondary targets past the cone cap");
    TFireTriggerArea wide = area; wide.coneX = 4;
    enemy._position = area.top + area.forward*100 + area.right*200;
    Check(!ypabact_IsVerticalLaserMultiCandidate(&shooter, &enemy, false, area.top, 150, 0, &wide, excluded),
          "downward-cone secondary target still respects the beam's physical reach");
    shooter._vertical_laser_active = true; shooter._vertical_laser_weapon = 24;
    bact_arg101 held{}; held.unkn = 1; held.pos = area.top + area.forward*100;
    Check(ypabact_ShouldPersistAILaserFire(&shooter, 24, &held, nullptr),
          "vertical laser remains active while its target is inside the downward cone");
    held.pos = area.top + area.forward*600;
    Check(!ypabact_ShouldPersistAILaserFire(&shooter, 24, &held, nullptr),
          "vertical laser releases when its target exits the downward cone");
}

void TestFrontVolumes(WorldProbe &w, const World::TWeapProto *wp) {
    Unit shooter; shooter.Bind(w, 1, 30, vec3d(2400, 0, -2400));
    TFireTriggerArea box;
    Check(shooter.GetAIFireTriggerArea(wp[10], &box), "forward box built from parsed shape");
    const vec3d top = box.top;
    Check(box.shape == World::TWeapProto::AI_TRIGGER_BOX_FORWARD &&
          Near(box.minX, -100) && Near(box.maxX, 100) && Near(box.halfY, 50) && Near(box.depth, 500),
          "forward box dimensions use full width and height");
    Check(ypabact_FireTriggerAreaContains(box, top + box.forward * 250, 0), "forward box contains axial centre");
    Check(ypabact_FireTriggerAreaContains(box, top + box.right * 100, 0), "forward box includes side boundary");
    Check(!ypabact_FireTriggerAreaContains(box, top + box.right * 100.1f, 0), "forward box excludes point past side");
    Check(ypabact_FireTriggerAreaContains(box, top + box.up * 50, 0) &&
          ypabact_FireTriggerAreaContains(box, top - box.up * 50, 0), "forward box includes both height boundaries");
    Check(ypabact_FireTriggerAreaContains(box, top + box.right * 110 + box.forward * 200, 10), "sphere touching forward box side is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + box.right * 110.1f + box.forward * 200, 10), "sphere beyond forward box side is outside");
    Check(ypabact_FireTriggerAreaContains(box, top + box.forward * 510, 10), "sphere touching forward box cap is inside");
    Check(!ypabact_FireTriggerAreaContains(box, top + box.forward * 510.1f, 10), "sphere beyond forward box cap is outside");
    Check(!ypabact_FireTriggerAreaContains(box, top - box.forward * 0.01f, 10), "front-volume centre behind muzzle remains outside");

    shooter._rotation = mat3x3::RotateY(0.7f) * mat3x3::RotateX(0.4f) * mat3x3::RotateZ(0.3f);
    Check(shooter.GetAIFireTriggerArea(wp[10], &box), "forward box built with yaw, pitch and roll");
    Check(Near(box.right, shooter._rotation.AxisX()) && Near(box.up, shooter._rotation.AxisY()) &&
          Near(box.forward, shooter._rotation.AxisZ()), "forward box axes preserve full unit rotation");
    Check(ypabact_FireTriggerAreaContains(box, box.top + box.forward * 250 + box.up * 20, 0), "rotated forward box contains along its local axes");
    Check(!ypabact_FireTriggerAreaContains(box, box.top + box.forward * 250 + box.right * 101, 0), "rotated forward box rejects local side overflow");

    shooter._rotation = mat3x3::Ident();
    Check(shooter.GetAIFireTriggerArea(wp[11], &box), "elliptic cone built from parsed shape");
    Check(box.shape == World::TWeapProto::AI_TRIGGER_CONE_FORWARD && Near(box.depth, 500), "cone stores axial depth");
    Check(Near((float)box.coneX, (float)std::tan(C_PI / 4.0)) &&
          Near((float)box.coneY, (float)std::tan(C_PI / 12.0)), "cone stores half-angle slopes from full degrees");
    const vec3d coneTop = box.top;
    const float z = 100.0f;
    const float edgeX = (float)box.coneX * z;
    const float edgeY = (float)box.coneY * z;
    Check(ypabact_FireTriggerAreaContains(box, coneTop, 0), "cone includes its apex");
    Check(ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.right * edgeX, 0), "cone includes horizontal ellipse boundary");
    Check(!ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.right * (edgeX * 1.01f), 0), "cone excludes point beyond horizontal boundary");
    Check(ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.up * edgeY * 0.65f + box.right * edgeX * 0.65f, 0), "cone includes anisotropic diagonal inside ellipse");
    Check(!ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.up * edgeY * 0.75f + box.right * edgeX * 0.75f, 0), "cone excludes anisotropic diagonal outside ellipse");
    Check(ypabact_FireTriggerAreaContains(box, coneTop + box.forward * 510, 10), "cone sphere touching cap is inside");
    Check(!ypabact_FireTriggerAreaContains(box, coneTop + box.forward * 510.1f, 10), "cone sphere beyond cap is outside");
    Check(!ypabact_FireTriggerAreaContains(box, coneTop - box.forward * 0.01f, 10), "cone excludes a centre behind the muzzle");
    const float sideTangent = (float)(box.coneX * z + 10.0 * std::sqrt(1.0 + box.coneX * box.coneX));
    Check(ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.right * sideTangent, 10), "cone sphere touching sloped side is inside");
    Check(!ypabact_FireTriggerAreaContains(box, coneTop + box.forward * z + box.right * (sideTangent + 0.3f), 10), "cone sphere beyond sloped side is outside");

    TFireTriggerArea capCorner = box;
    capCorner.shape = World::TWeapProto::AI_TRIGGER_CONE_FORWARD;
    capCorner.minX = capCorner.maxX = 0.0f;
    capCorner.coneX = capCorner.coneY = 1.0;
    capCorner.depth = 100.0f;
    const vec3d obliqueCapPoint = capCorner.top + capCorner.right * 300 +
                                  capCorner.up * 300 + capCorner.forward * 110;
    Check(!ypabact_FireTriggerAreaContains(capCorner, obliqueCapPoint, 320),
          "cone sphere near an oblique cap corner stays outside before true tangency");
    Check(ypabact_FireTriggerAreaContains(capCorner, obliqueCapPoint, 325),
          "cone sphere near an oblique cap corner includes true tangency");

    // Constructing and checking the volume must not advance salves, rack slots,
    // fire-X random state, projectile-count state or the global count RNG.
    struct FireXCase { int mode; bool advanced; float low; float high; };
    const FireXCase modes[] = {
        {World::TVhclProto::FIRE_X_MODE_VANILLA, false, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_SEQUENCE, false, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_RANDOM, false, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_SEQUENCE, true, -30, 30},
        {World::TVhclProto::FIRE_X_MODE_RANDOM, true, -30, 30},
        {World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE, false, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_SALVE_MIRROR, false, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE, true, -10, 10},
        {World::TVhclProto::FIRE_X_MODE_SALVE_MIRROR, true, -10, 10},
    };
    struct CountRange { int min; int max; };
    const CountRange counts[] = {{1, 1}, {2, 2}, {4, 4}, {4, 6}};
    struct ShapeCase { const World::TWeapProto *proto; float halfWidth; };
    const ShapeCase shapes[] = {
        {&wp[1], 100.0f},
        {&wp[10], 100.0f},
        {&wp[11], 0.0f},
        {&wp[22], 0.0f},
    };
    shooter._fire_pos.x = 10;
    shooter._fire_x_start = 30; shooter._fire_x_step = -20; shooter._fire_x_slots = 4;
    shooter._fire_x_slot_index = 2; shooter._fire_x_random_seeded = true;
    shooter._fire_x_random_state = 0x8badf00du; shooter._fire_x_random_order = {2, 0, 3, 1};
    shooter._weapon_slot_index = 2;
    std::srand(57311);
    const int nextRandom = std::rand();
    std::srand(57311);
    for (const FireXCase &mode : modes) {
        shooter._fire_x_mode = mode.mode; shooter._fire_x_advanced = mode.advanced;
        for (const CountRange &count : counts) {
            shooter._num_weapons = (uint8_t)count.min;
            shooter._weapon_projectile_counts.fill((uint8_t)count.min);
            shooter._weapon_projectile_count_maxs.fill((uint8_t)count.max);
            int minCount = 0, maxCount = 0;
            ypabact_GetWeaponProjectileCountRangeForSourceSlot(&shooter, 0, &minCount, &maxCount);
            Check(minCount == count.min && maxCount == count.max, "configured fixed or ranged count remains readable");
            for (const ShapeCase &shape : shapes) {
                TFireTriggerArea area;
                Check(shooter.GetAIFireTriggerArea(*shape.proto, &area), "fire-X trigger envelope builds for every shape");
                Check(Near(area.minX, mode.low - shape.halfWidth) &&
                      Near(area.maxX, mode.high + shape.halfWidth), "fire-X envelope covers the complete launch span");
                bact_arg101 probe;
                probe.unkn = 1;
                probe.pos = area.top + area.forward * 100;
                size_t shouldFire = 0;
                Check(shooter.CheckAIFireTriggerArea(*shape.proto, &probe, &shouldFire) && shouldFire == 1,
                      "all trigger shapes leave launch count selection to LaunchMissile");
                Check(shooter._fire_x_slot_index == 2 && shooter._fire_x_random_seeded &&
                      shooter._fire_x_random_state == 0x8badf00du &&
                      shooter._fire_x_random_order == std::vector<int>({2, 0, 3, 1}) &&
                      shooter._weapon_slot_index == 2, "trigger query does not consume fire-X or weapon slots");
                Check(shooter._weapon_projectile_counts[0] == count.min &&
                      shooter._weapon_projectile_count_maxs[0] == count.max && shooter._num_weapons == count.min,
                      "trigger query leaves fixed and ranged projectile-count state unchanged");
            }
        }
    }
    Check(std::rand() == nextRandom, "trigger queries do not consume the global projectile-count RNG");

    // A normal continuous laser uses the same front-volume test while held.
    Unit laser; laser.Bind(w, 1, 31, vec3d(2400, 0, -2400));
    laser._laser_active = true; laser._laser_weapon = 20;
    bact_arg101 held; held.unkn = 1; held.pos = laser._position + vec3d(20, 0, 100);
    Check(ypabact_ShouldPersistAILaserFire(&laser, 20, &held, nullptr), "active normal laser persists inside forward volume");
    held.pos = laser._position + vec3d(60, 0, 100);
    Check(!ypabact_ShouldPersistAILaserFire(&laser, 20, &held, nullptr), "active normal laser releases outside forward volume despite valid beam alignment");
}

void TestArcFallback() {
    WorldProbe w;
    Check(ParseWeapons(w, {"new_weapon 1", "model = arc_grenade", "start_speed = 10",
                           "ai_fire_trigger_box_forward_xyz = 200_200_600", "end"}), "arc fallback weapon parsed");
    Unit shooter; shooter.Bind(w, 1, 1, vec3d(2400, 0, -2400));
    const World::TWeapProto &wp = w._weaponProtos[1];
    TFireTriggerArea box;
    Check(shooter.GetAIFireTriggerArea(wp, &box), "arc-grenade forward volume built");
    Unit primary; primary.Bind(w, 2, 2, box.top + box.forward * 500);
    Unit alternate; alternate.Bind(w, 2, 3, box.top + box.forward * 30);
    vec3d aim;
    Check(!ypabact_TrySolveArcGrenadeDirection(box.top, primary._position, wp, &aim), "primary inside volume is outside arc-grenade reach");
    Check(ypabact_TrySolveArcGrenadeDirection(box.top, alternate._position, wp, &aim), "alternate inside volume is arc-grenade reachable");

    bact_arg101 fireTest;
    fireTest.unkn = 2; fireTest.pos = primary._position; fireTest.radius = 0;
    fireTest.launch_pos = box.top; fireTest.has_launch_pos = true;
    size_t fire = 0;
    Check(shooter.CheckAIFireTriggerArea(wp, &fireTest, &fire) && fire == 1 &&
          fireTest.fire_trigger_area_target == &alternate,
          "unreachable primary yields to reachable enemy inside the volume");
    bact_arg79 shot;
    shot.tgType = BACT_TGT_TYPE_CELL; shot.direction = box.forward;
    ypabact_AimAtFireTriggerAreaTarget(fireTest, &shot);
    Check(shot.tgType == BACT_TGT_TYPE_UNIT && shot.target.pbact == &alternate,
          "arc fallback retargets the launched shot to the reachable enemy");

    WorldProbe edgeWorld;
    Check(ParseWeapons(edgeWorld, {"new_weapon 2", "model = arc_grenade", "start_speed = 10",
                                  "ai_fire_trigger_box_forward_xyz = 2000_200_600", "end"}),
          "wide arc-grenade volume parsed for muzzle-origin check");
    Unit edgeShooter; edgeShooter.Bind(edgeWorld, 1, 10, vec3d(2400, 0, -2400));
    const World::TWeapProto &wideProto = edgeWorld._weaponProtos[2];
    TFireTriggerArea wideBox;
    Check(edgeShooter.GetAIFireTriggerArea(wideProto, &wideBox), "wide arc-grenade volume built");
    Unit edgeTarget; edgeTarget.Bind(edgeWorld, 2, 11,
                                     wideBox.top + wideBox.right * 500 + wideBox.forward);
    Check(!ypabact_TrySolveArcGrenadeDirection(wideBox.top, edgeTarget._position, wideProto, &aim),
          "wide edge target is unreachable from the actual centre muzzle");
    bact_arg101 edgeTest{};
    edgeTest.unkn = 2; edgeTest.pos = edgeTarget._position; edgeTest.radius = 0;
    edgeTest.launch_pos = wideBox.top; edgeTest.has_launch_pos = true;
    fire = 0;
    Check(edgeShooter.CheckAIFireTriggerArea(wideProto, &edgeTest, &fire) && fire == 0,
          "box half-width does not invent an arc-grenade muzzle origin");
}

void TestTurretSlotsAndCadence() {
    WorldProbe w;
    Check(ParseWeapons(w, {"new_weapon 10", "ai_fire_trigger_box_forward_xyz = 200_200_500", "model = rocket", "end",
                           "new_weapon 11", "model = rocket", "ai_fire_trigger_box_forward_xyz = 200_200_500", "end",
                           "new_weapon 12", "model = rocket", "ai_fire_trigger_box_forward_xyz = 200_200_500", "end",
                           "new_weapon 13", "model = rocket", "ai_fire_trigger_box_forward_xyz = 200_200_500", "end"}),
          "turret weapons parsed");
    Check(ParseVehicles(w, {"new_vehicle 20", "model = tank", "weapon = 10", "weapon_2 = 11",
                            "weapon_3 = 12", "weapon_4 = 13", "weapon_ai_switch_mode = sequence",
                            "num_weapons = 1_6", "num_weapons_2 = 2", "num_weapons_3 = 4",
                            "num_weapons_4 = 6", "end"}), "turret vehicle parsed");
    const World::TVhclProto &vehicle = w._vhclProtos[20];
    Check(vehicle.weapon == 10 && vehicle.extra_weapons[0] == 11 &&
          vehicle.extra_weapons[1] == 12 && vehicle.extra_weapons[2] == 13,
          "vehicle parser keeps weapon and weapon_2/3/4 slots");
    Check(vehicle.num_weapons_min == 1 && vehicle.num_weapons_max == 6 &&
          vehicle.extra_num_weapons_min[0] == 2 && vehicle.extra_num_weapons_max[0] == 2 &&
          vehicle.extra_num_weapons_min[1] == 4 && vehicle.extra_num_weapons_max[1] == 4 &&
          vehicle.extra_num_weapons_min[2] == 6 && vehicle.extra_num_weapons_max[2] == 6,
          "vehicle parser keeps fixed and ranged projectile counts per slot");

    GunProbe gun; gun.Bind(w, vehicle, 1, 100, vec3d(2400, 0, -2400));
    Unit enemy; enemy.Bind(w, 2, 101, gun._position + vec3d(0, 0, 100));
    std::srand(46721);
    const int nextRandom = std::rand();
    std::srand(46721);
    for (int sourceSlot = 1; sourceSlot <= 3; ++sourceSlot) {
        gun._weapon_slot_index = sourceSlot;
        gun._fire_x_mode = World::TVhclProto::FIRE_X_MODE_RANDOM;
        gun._fire_x_advanced = true; gun._fire_x_slots = 4;
        gun._fire_x_start = 40; gun._fire_x_step = -20;
        gun._fire_x_random_seeded = true; gun._fire_x_random_state = 0x13579bdfu;
        gun._fire_x_random_order = {1, 3, 0, 2};
        int countMin = 0, countMax = 0;
        ypabact_GetWeaponProjectileCountRangeForSourceSlot(&gun, sourceSlot, &countMin, &countMax);
        Check(countMin == sourceSlot * 2 && countMax == countMin,
              "secondary slot exposes its own projectile count");
        bact_arg75 fight{};
        fight.target.pbact = &enemy; fight.g_time = 16; fight.fperiod = 16.0f;
        gun.FightWithBact(&fight);
        Check(gun.launches == sourceSlot && gun.lastLaunch.weapon_source_slot == sourceSlot,
              "turret launch keeps exact weapon_2/3/4 source slot");
        Check(gun.lastLaunch.weapon == vehicle.weapon && gun.lastLaunch.target.pbact == &enemy &&
              (gun.lastLaunch.flags & BACT_ARG79_FLAG_AI_BALLISTIC_AIM),
              "turret launch keeps target and trigger-area ballistic aim");
        Check(gun._gunFireCount == gun._gunFireTime && gun.stateChanges == sourceSlot,
              "successful turret trigger launch starts its configured cadence");
        Check(gun._weapon_slot_index == sourceSlot && gun._fire_x_slot_index == 0 &&
              gun._fire_x_random_state == 0x13579bdfu &&
              gun._fire_x_random_order == std::vector<int>({1, 3, 0, 2}),
              "turret trigger does not consume rack or weapon selection slots");
    }
    Check(std::rand() == nextRandom, "turret trigger and intercepted launches do not roll projectile counts");

    enemy._position = gun._position + vec3d(0, 0, 600);
    gun._weapon_slot_index = 1; gun._gunFireCount = 0;
    bact_arg75 outside{};
    outside.target.pbact = &enemy; outside.g_time = 16; outside.fperiod = 16.0f;
    gun.FightWithBact(&outside);
    Check(gun.launches == 3 && gun._gunFireCount == 0, "turret fire gate blocks a target outside every configured volume");
}

struct LaunchScenario {
    int weapon = 0;
    int countMin = 1;
    int countMax = 1;
    int fireXMode = World::TVhclProto::FIRE_X_MODE_VANILLA;
    bool advancedRack = false;
    float arcX = 0.0f;
    float arcY = 0.0f;
    float coneXY = 0.0f;
    float spreadX = 0.0f;
    float spreadY = 0.0f;
    float spreadZ = 0.0f;
    bool playerInput = false;
    bool expectMultiTarget = false;
    std::vector<int> times;
};

struct ProjectileSnapshot {
    vec3d position;
    vec3d direction;
    float speed = 0.0f;
    int weapon = 0;
    int targetType = BACT_TGT_TYPE_NONE;
    int targetGid = 0;
    vec3d targetPosition;
};

struct LaunchSnapshot {
    std::vector<ProjectileSnapshot> projectiles;
    std::vector<int> launchReturns;
    std::vector<int> spawnedCounts;
    std::vector<bool> gateAccepted;
    int weaponTime = 0;
    int salveCounter = 0;
    int fireXSlot = 0;
    int weaponSlot = 0;
    int currentWeapon = 0;
    int currentWeaponSourceSlot = 0;
    int projectileCount = 0;
    int projectileCountMax = 0;
    uint32_t fireXRandomState = 0;
    bool fireXRandomSeeded = false;
    std::vector<int> fireXRandomOrder;
    int energy = 0;
    int nextGlobalRandom = 0;
};

void ResetLaunchState(Unit &launcher, const LaunchScenario &scenario) {
    launcher._weapon = scenario.weapon;
    launcher._extra_weapons.fill(0);
    launcher._weapon_ai_switch_mode = World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
    launcher._weapon_slot_index = 0;
    launcher._current_weapon_id = scenario.weapon;
    launcher._current_weapon_source_slot = 0;
    launcher._num_weapons = (uint8_t)scenario.countMin;
    launcher._weapon_projectile_counts.fill((uint8_t)scenario.countMin);
    launcher._weapon_projectile_count_maxs.fill((uint8_t)scenario.countMax);
    launcher._weapon_arc_x = scenario.arcX;
    launcher._weapon_arc_y = scenario.arcY;
    launcher._weapon_cone_xy = scenario.coneXY;
    launcher._weapon_spread_x = scenario.spreadX;
    launcher._weapon_spread_y = scenario.spreadY;
    launcher._weapon_spread_z = scenario.spreadZ;
    launcher._fire_pos = vec3d(20, 0, 0);
    launcher._fire_x_mode = scenario.fireXMode;
    launcher._fire_x_advanced = scenario.advancedRack;
    launcher._fire_x_start = -30;
    launcher._fire_x_step = 20;
    launcher._fire_x_slots = 4;
    launcher._fire_x_slot_index = 1;
    launcher._fire_x_random_seeded = true;
    launcher._fire_x_random_state = 0x8badf00du;
    launcher._fire_x_random_order = {2, 0, 3, 1};
    launcher._oflags = scenario.playerInput ? BACT_OFLAG_USERINPT : 0;
    launcher._weapon_time = 0;
    launcher._salve_counter = 0;
    launcher._energy = launcher._energy_max = 100000;
    launcher._progressive_weapon_id = -1;
    launcher._progressive_weapon_level = 0.0f;
    launcher._progressive_weapon_requested = false;
    launcher._progressive_weapon_overheat_active = false;
}

LaunchSnapshot RunLaunchSequence(WorldProbe &world, Unit &launcher, Unit &primary,
                                 const World::TWeapProto &weapon,
                                 const LaunchScenario &scenario, bool withGate) {
    ResetLaunchState(launcher, scenario);
    constexpr unsigned int seed = 915723;
    std::srand(seed);
    LaunchSnapshot result;
    for (int time : scenario.times) {
        bact_arg79 shot{};
        shot.direction = primary._position - launcher._position;
        if ( shot.direction.normalise() <= 0.001f )
            shot.direction = launcher._rotation.AxisZ();
        shot.start_point = vec3d(20, 0, 0);
        shot.tgType = BACT_TGT_TYPE_UNIT;
        shot.target.pbact = &primary;
        shot.tgt_pos = primary._position;
        shot.weapon = scenario.weapon;
        shot.weapon_source_slot = 0;
        shot.g_time = time;

        if ( withGate ) {
            bact_arg101 fireTest{};
            fireTest.unkn = 2;
            fireTest.pos = primary._position;
            fireTest.radius = primary._radius;
            fireTest.launch_pos = launcher._position + launcher._rotation.Transpose().Transform(
                vec3d(0, launcher._fire_pos.y, launcher._fire_pos.z));
            fireTest.has_launch_pos = true;
            size_t shouldFire = 0;
            const bool hasGate = launcher.CheckAIFireTriggerArea(weapon, &fireTest, &shouldFire);
            const bool accepted = hasGate && shouldFire != 0;
            result.gateAccepted.push_back(accepted);
            if ( !accepted ) {
                result.launchReturns.push_back(0);
                result.spawnedCounts.push_back(0);
                continue;
            }
            ypabact_AimAtFireTriggerAreaTarget(fireTest, &shot);
        }

        const size_t before = launcher._missiles_list.size();
        const size_t launchResult = launcher.LaunchMissile(&shot);
        result.launchReturns.push_back((int)launchResult);
        result.spawnedCounts.push_back((int)(launcher._missiles_list.size() - before));

        auto missile = launcher._missiles_list.begin();
        std::advance(missile, before);
        for (; missile != launcher._missiles_list.end(); ++missile) {
            NC_STACK_ypamissile *projectile = *missile;
            ProjectileSnapshot snapshot;
            snapshot.position = projectile->_position;
            snapshot.direction = projectile->_fly_dir;
            snapshot.speed = projectile->_fly_dir_length;
            snapshot.weapon = projectile->_vehicleID;
            snapshot.targetType = projectile->_primTtype;
            snapshot.targetGid = projectile->_primTtype == BACT_TGT_TYPE_UNIT && projectile->_primT.pbact
                ? projectile->_primT.pbact->_gid : 0;
            snapshot.targetPosition = projectile->_primTpos;
            result.projectiles.push_back(snapshot);
        }
    }

    result.weaponTime = launcher._weapon_time;
    result.salveCounter = launcher._salve_counter;
    result.fireXSlot = launcher._fire_x_slot_index;
    result.weaponSlot = launcher._weapon_slot_index;
    result.currentWeapon = launcher._current_weapon_id;
    result.currentWeaponSourceSlot = launcher._current_weapon_source_slot;
    result.projectileCount = launcher._weapon_projectile_counts[0];
    result.projectileCountMax = launcher._weapon_projectile_count_maxs[0];
    result.fireXRandomState = launcher._fire_x_random_state;
    result.fireXRandomSeeded = launcher._fire_x_random_seeded;
    result.fireXRandomOrder = launcher._fire_x_random_order;
    result.energy = (int)launcher._energy;
    result.nextGlobalRandom = std::rand();
    return result;
}

bool SameLaunchSnapshot(const LaunchSnapshot &a, const LaunchSnapshot &b) {
    if ( a.projectiles.size() != b.projectiles.size() ||
         a.launchReturns != b.launchReturns || a.spawnedCounts != b.spawnedCounts ||
         a.weaponTime != b.weaponTime || a.salveCounter != b.salveCounter ||
         a.fireXSlot != b.fireXSlot || a.weaponSlot != b.weaponSlot ||
         a.currentWeapon != b.currentWeapon ||
         a.currentWeaponSourceSlot != b.currentWeaponSourceSlot ||
         a.projectileCount != b.projectileCount ||
         a.projectileCountMax != b.projectileCountMax ||
         a.fireXRandomState != b.fireXRandomState ||
         a.fireXRandomSeeded != b.fireXRandomSeeded ||
         a.fireXRandomOrder != b.fireXRandomOrder ||
         a.energy != b.energy || a.nextGlobalRandom != b.nextGlobalRandom )
        return false;

    for (size_t i = 0; i < a.projectiles.size(); ++i) {
        const ProjectileSnapshot &x = a.projectiles[i];
        const ProjectileSnapshot &y = b.projectiles[i];
        if ( !Near(x.position, y.position) || !Near(x.direction, y.direction) ||
             !Near(x.speed, y.speed) || x.weapon != y.weapon ||
             x.targetType != y.targetType || x.targetGid != y.targetGid ||
             !Near(x.targetPosition, y.targetPosition) )
            return false;
    }
    return true;
}

void TestVanillaSectorMinigun() {
    WorldProbe world;
    Check(ParseWeapons(world, {"new_weapon 80", "model = rocket", "end"}), "sector MGUN fixture parses");
    for (bool secondary : {false, true}) {
        for (bool timed : {false, true}) {
            SectorFightProbe unit;
            unit.Bind(world, 1, 620, vec3d(4200, -100, -4200));
            const vec3d target = unit._position + vec3d(0, 100, 500);
            auto *cell = &world.SectorAt(World::PositionToSectorID(target));
            unit._primTtype = secondary ? BACT_TGT_TYPE_NONE : BACT_TGT_TYPE_CELL;
            unit._secndTtype = secondary ? BACT_TGT_TYPE_CELL : BACT_TGT_TYPE_NONE;
            unit._primT.pcell = secondary ? nullptr : cell; unit._secndT.pcell = secondary ? cell : nullptr;
            unit._primTpos = unit._sencdTpos = target;
            unit._height = 100; unit._weapon = 80;
            unit._mgun = 1; unit._mgun_set = !timed; unit._mgun_shot_time = timed ? 100 : 0;
            unit._mgun_energy_set = timed; unit._mgun_energy = 1000;
            unit._status_flg = BACT_STFLAG_FIRE; unit._aggr = 100;
            unit._clock = unit._assess_time = 1000; unit._atk_ret = NC_STACK_ypabact::TA_FIGHT;
            unit._adist_sector = 800; unit._sdist_sector = 100; unit._wrldSize = vec2d(9600, -9600);
            bact_arg75 fight{}; fight.target.pcell = cell; fight.pos = target; fight.g_time = 1000;
            Check(unit.HasMinigun(), "sector MGUN fixture has a usable mounted MGUN");
            unit.FightWithSect(&fight);
            Check(unit.minigunCalls == 0 && unit.minigunStops == 1 && !(unit._status_flg & BACT_STFLAG_FIRE),
                  "primary and secondary sector orders stop MGUN fire without reactivating legacy or timed MGUNs");
            Check(unit.launches == 1, "sector order retains primary weapon fire after stopping the MGUN");
        }
    }
    MgunWallWorld walls;
    MgunDamageProbe shooter;
    shooter.Bind(walls, 1, 621, vec3d(4200, -100, -4200));
    shooter._mgun = -1; shooter._mgun_set = false;
    shooter._mgun_shot_time = 100; shooter._mgun_decal_enable = false;
    shooter._mgun_spread_x = shooter._mgun_spread_y = 0;
    shooter._gun_power = shooter._mgun_energy = 100000;
    shooter._energy = shooter._energy_max = 1000000;
    int clock = 1000;
    for (bool player : {false, true}) {
        for (bool timed : {false, true}) {
            shooter._oflags = player ? BACT_OFLAG_USERINPT : 0;
            shooter._mgun_energy_set = timed;
            bact_arg105 shot{}; shot.field_0 = vec3d(0, 0, 1); shot.field_C = 1.0f; shot.field_10 = clock;
            clock += 1000;
            Check(shooter.FireMinigun(&shot) == 1 && shooter.buildingDamageCalls == 0,
                  "actual AI/manual MGUN world impacts cause no sector damage in continuous or timed fire");
        }
    }
}

void TestMinimumDepth() {
    WorldProbe w;
    Check(ParseWeapons(w, {
        "new_weapon 80", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", "ai_fire_trigger_min_depth = 150", "end",
        "new_weapon 81", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 40_40_1000", "ai_fire_trigger_min_depth = 150", "end",
        "new_weapon 82", "model = bomb", "ai_fire_trigger_box_below_xyz = 80_1000_400", "ai_fire_trigger_min_depth = 150", "end",
        "new_weapon 83", "model = bomb", "ai_fire_trigger_cone_below_xyz = 40_40_1000", "ai_fire_trigger_min_depth = 150", "end",
        "new_weapon 84", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", "ai_fire_trigger_min_depth = -1", "end",
        "new_weapon 85", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", "ai_fire_trigger_min_depth = 1200", "end"}),
        "minimum depth fixtures parse");
    Unit shooter; shooter.Bind(w, 1, 1, vec3d(2400, -300, -2400));
    for (int id : {80, 81, 82, 83}) {
        TFireTriggerArea area;
        Check(shooter.GetAIFireTriggerArea(w._weaponProtos[id], &area) && Near(area.minDepth, 150),
              "minimum depth applies to all four shapes");
        Check(!ypabact_FireTriggerAreaContains(area, area.top + area.forward * 100, 0),
              "point inside the near dead zone cannot trigger fire");
        Check(ypabact_FireTriggerAreaContains(area, area.top + area.forward * 150, 0),
              "near boundary starts the active volume");
        Check(ypabact_FireTriggerAreaContains(area, area.top + area.forward * 140, 10),
              "a unit touching the near boundary can trigger fire");
    }
    TFireTriggerArea area;
    Check(shooter.GetAIFireTriggerArea(w._weaponProtos[84], &area) && Near(area.minDepth, 0),
          "invalid minimum leaves the vanilla near boundary");
    Check(shooter.GetAIFireTriggerArea(w._weaponProtos[85], &area) && Near(area.minDepth, 0),
          "minimum beyond the far end cannot disable the volume");
}

// One max inclination key applies to either forward shape, in degrees from 0 to below 90.
void TestMaxInclinationParse() {
    WorldProbe w;
    Check(ParseWeapons(w, {
        "new_weapon 90", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000",
        "ai_fire_trigger_forward_max_inclination = 25", "end",
        "new_weapon 91", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_20_1000",
        "ai_fire_trigger_forward_max_inclination = 12.5", "end",
        "new_weapon 92", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_20_1000",
        "ai_fire_trigger_forward_max_inclination = 25", "end",
        "new_weapon 93", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", "end",
        "new_weapon 94", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000",
        "ai_fire_trigger_forward_max_inclination = 0", "end",
        "new_weapon 95", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000",
        "ai_fire_trigger_forward_max_inclination = 90", "end",
        "new_weapon 96", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000",
        "ai_fire_trigger_forward_max_inclination = -5", "end",
        "new_weapon 97", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000",
        "ai_fire_trigger_forward_max_inclination = steep", "end",
        "new_weapon 98", "model = bomb", "ai_fire_trigger_cone_below_xyz = 30_30_1500",
        "ai_fire_trigger_forward_max_inclination = 25", "end"}),
        "max inclination fixtures parse");
    const auto &wp = w._weaponProtos;
    Check(Near(wp[90].GetAIFireTriggerMaxInclination(), 25.0f), "box forward volume reads its max inclination");
    Check(Near(wp[91].GetAIFireTriggerMaxInclination(), 12.5f), "cone forward volume reads its max inclination");
    Check(Near(wp[92].GetAIFireTriggerMaxInclination(), 25.0f), "the shared key applies to a cone volume");
    Check(Near(wp[93].GetAIFireTriggerMaxInclination(), 0.0f), "absent key: no tilt limit configured");
    for (int id : {94, 95, 96, 97})
        Check(Near(wp[id].GetAIFireTriggerMaxInclination(), 0.0f), "zero, 90 or more, negative or malformed value leaves the tilt off");
    Check(wp[95].HasAIFireTriggerArea(), "an invalid max inclination keeps the volume itself");
    Check(Near(wp[98].GetAIFireTriggerMaxInclination(), 0.0f), "below volumes never use a max inclination");
}

// Without a max_inclination on the weapon, aircraft volumes stay on the hull axes for every target.
void TestAirTiltAbsent() {
    WorldProbe world;
    Check(ParseWeapons(world, {"new_weapon 71", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", "end",
                               "new_weapon 72", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_20_1000", "end"}),
          "absent-tilt fixtures parse");
    Unit aircraft;
    aircraft.Bind(world, 1, 620, vec3d(4200, -250, -4200));
    aircraft._height = 250;
    aircraft._primTtype = BACT_TGT_TYPE_CELL; aircraft._secndTtype = BACT_TGT_TYPE_NONE;
    aircraft._primTpos = aircraft._position + vec3d(0, 250, 700);
    TankProbe groundFoe; groundFoe.Bind(world); groundFoe._position = aircraft._primTpos;
    for (int type : {BACT_TYPES_BACT, BACT_TYPES_FLYER, BACT_TYPES_UFO}) {
        aircraft._bact_type = type;
        for (int weapon : {71, 72}) {
            TFireTriggerArea area;
            bact_arg101 fire{}; fire.weapon = weapon; fire.unkn = 1; fire.pos = aircraft._primTpos;
            Check(aircraft.GetAIFireTriggerArea(world._weaponProtos[weapon], &area, &fire.pos, true) &&
                  Near(area.forward, aircraft._rotation.AxisZ()),
                  "absent max inclination: aircraft volume stays on the hull axis");
            Check(aircraft.CheckFireAI(&fire) == 0, "absent max inclination: a sector far below the nose is outside the volume");
            fire.unkn = 2; fire.target = &groundFoe;
            Check(aircraft.CheckFireAI(&fire) == 0, "absent max inclination: a ground unit far below the nose is outside the volume");
        }
    }
}

// The optional weapon max inclination also caps ground units, on top of their native scope limits.
void TestTankTiltCap() {
    WorldProbe w;
    Check(ParseWeapons(w, {"new_weapon 10", "model = rocket", "ai_fire_trigger_box_forward_xyz = 60_40_900", "end",
                           "new_weapon 11", "model = rocket", "ai_fire_trigger_box_forward_xyz = 60_40_900",
                           "ai_fire_trigger_forward_max_inclination = 10", "end"}),
          "tank tilt-cap fixture parses");
    TankProbe tank; tank.Bind(w);
    Unit enemy; enemy.Bind(w, 2, 500, tank._position + vec3d(0, -300, 500));
    tank._primTtype = BACT_TGT_TYPE_UNIT; tank._primT.pbact = &enemy;
    TFireTriggerArea box;
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[10], &box) && box.forward.y < -0.4f,
          "absent max inclination: ground unit volume follows only its native limits");
    Check(tank.GetAIFireTriggerArea(w._weaponProtos[11], &box) &&
          box.forward.y >= -std::sin(10.0 * C_PI / 180.0) - 1e-3 && box.forward.y < -0.1f,
          "max inclination caps the volume of a ground unit at 10 degrees");
}

void TestAirSectorElevation() {
    WorldProbe world;
    const char *BoxTilt = "ai_fire_trigger_forward_max_inclination = 25";
    const char *ConeTilt = "ai_fire_trigger_forward_max_inclination = 25";
    Check(ParseWeapons(world, {
        "new_weapon 70", "model = rocket", "end",
        "new_weapon 71", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_1000", BoxTilt, "end",
        "new_weapon 72", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_20_1000", ConeTilt, "end",
        "new_weapon 73", "model = missile", "ai_fire_trigger_box_forward_xyz = 80_80_1000", BoxTilt, "end",
        "new_weapon 74", "model = missile", "ai_fire_trigger_cone_forward_xyz = 20_20_1000", ConeTilt, "end",
        "new_weapon 75", "model = rocket", "ai_fire_trigger_box_forward_xyz = 80_80_3000", BoxTilt, "end",
        "new_weapon 76", "model = laser", "life_time = 5000", "ai_fire_trigger_box_forward_xyz = 80_80_1000", BoxTilt, "end",
        "new_weapon 77", "model = arc_grenade", "start_speed = 100", "grenade_arc_gravity = 35",
        "ai_fire_trigger_box_forward_xyz = 80_80_1000", BoxTilt, "end",
        "new_weapon 78", "model = rocket", "ai_fire_trigger_box_forward_xyz = 0", "end",
        "new_weapon 79", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 20_bad_1000", "end"
    }), "air-sector elevation fixtures parse");
    Unit aircraft;
    aircraft.Bind(world, 1, 610, vec3d(4200, -250, -4200));
    aircraft._height = 250;
    aircraft._primTtype = BACT_TGT_TYPE_CELL;
    aircraft._secndTtype = BACT_TGT_TYPE_NONE;
    aircraft._primTpos = aircraft._position + vec3d(0, 250, 700);
    bact_arg101 fire{};
    fire.unkn = 1; fire.pos = aircraft._primTpos;
    for (int type : {BACT_TYPES_BACT, BACT_TYPES_FLYER, BACT_TYPES_UFO}) {
        aircraft._bact_type = type;
        fire.weapon = 70;
        Check(aircraft.CheckFireAI(&fire) == 1,
              "vanilla helicopter and fixed-wing sector fire allows downward weapon aim without pitching the hull");
        for (int weapon : {78, 79}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 1,
                  "zero or malformed aerial forward volumes preserve vanilla sector aiming");
        }
        for (int weapon : {71, 72, 73, 74, 76, 77}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 1,
                  "forward box and cone retain vanilla aerial downward sector aim");
            TFireTriggerArea area;
            Check(aircraft.GetAIFireTriggerArea(world._weaponProtos[weapon], &area) &&
                  ypabact_FireTriggerAreaContains(area, fire.pos, 0),
                  "aerial debug volume follows the same sector elevation as the firing query");
            const vec3d originalForward = aircraft._rotation.AxisZ();
            fire.unkn = 2;
            Check(aircraft.CheckFireAI(&fire) == 0,
                  "unit queries without a ground target do not inherit the aerial elevation allowance");
            fire.unkn = 1;
            aircraft._laser_active = true; aircraft._laser_weapon = 76;
            if (weapon == 76)
                Check(ypabact_ShouldPersistAILaserFire(&aircraft, weapon, &fire, nullptr),
                      "aerial continuous-laser sector retention shares the acquisition elevation");
            Check(Near(aircraft._rotation.AxisZ(), originalForward),
                  "aerial sector elevation leaves the hull and its independent MGUN axes untouched");
        }

        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            for (const vec3d &offset : {vec3d(300, 250, 700), vec3d(0, 250, -700), vec3d(0, 250, 1100)}) {
                fire.pos = aircraft._position + offset;
                Check(aircraft.CheckFireAI(&fire) == 0,
                      "aerial sector elevation cannot bypass horizontal width, rear rejection or finite depth");
            }
        }
        fire.pos = aircraft._position + vec3d(0, 250, 1800);
        fire.weapon = 70;
        Check(aircraft.CheckFireAI(&fire) == 0, "unconfigured aerial sector fire retains its vanilla distance cap");
        fire.weapon = 75;
        Check(aircraft.CheckFireAI(&fire) == 1, "aerial sector pitch allowance does not import the vanilla range cap into configured depth");
        fire.pos = aircraft._primTpos;
        aircraft._height = 700;
        fire.weapon = 70;
        Check(aircraft.CheckFireAI(&fire) == 0,
              "unconfigured aerial sector fire keeps the vanilla 24-degree allowance");
        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 1,
                  "aerial sector volumes tilt to any point ahead, independently of the vanilla allowance");
        }
        aircraft._height = 250;
        fire.pos = aircraft._position + vec3d(0, 250, 450);
        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 1,
                  "aircraft fires at a sector point just beyond the vanilla 24-degree allowance");
        }
        fire.weapon = 70;
        Check(aircraft.CheckFireAI(&fire) == 0, "the same point stays outside the unconfigured vanilla allowance");
        fire.pos = aircraft._position + vec3d(0, 250, 60);
        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 0,
                  "aircraft almost above the sector does not fire down: the tilt stops at 25 degrees");
            TFireTriggerArea area;
            Check(aircraft.GetAIFireTriggerArea(world._weaponProtos[weapon], &area, &fire.pos, true) &&
                  std::fabs(area.forward.dot(aircraft._rotation.AxisZ()) - std::cos(25.0 * C_PI / 180.0)) < 1e-3,
                  "aerial ground tilt is clamped to 25 degrees below the nose");
        }
        fire.pos = aircraft._position + vec3d(0, 250, -60);
        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 0, "aerial sector volumes never tilt toward a point behind the nose");
        }
        fire.pos = aircraft._primTpos;
        {
            TankProbe groundFoe; groundFoe.Bind(world); groundFoe._position = fire.pos;
            Unit airFoe; airFoe._bact_type = BACT_TYPES_FLYER;
            bact_arg101 unitFire{}; unitFire.unkn = 2; unitFire.radius = 0; unitFire.pos = fire.pos;
            for (int weapon : {71, 72}) {
                unitFire.weapon = weapon;
                unitFire.target = &groundFoe;
                Check(aircraft.CheckFireAI(&unitFire) == 1, "aerial volumes tilt down toward a ground unit ahead");
                unitFire.target = &airFoe;
                Check(aircraft.CheckFireAI(&unitFire) == 0, "air unit targets keep the untilted forward volume");
            }
            const auto savedTarget = aircraft._primT;
            aircraft._primTtype = BACT_TGT_TYPE_UNIT; aircraft._primT.pbact = &groundFoe;
            TFireTriggerArea area;
            Check(aircraft.GetAIFireTriggerArea(world._weaponProtos[71], &area) &&
                  ypabact_FireTriggerAreaContains(area, groundFoe._position, 0),
                  "aerial debug volume tilts toward its ground unit target");
            aircraft._primTtype = BACT_TGT_TYPE_CELL; aircraft._primT = savedTarget;
        }

        aircraft._oflags = BACT_OFLAG_USERINPT;
        TFireTriggerArea area;
        Check(aircraft.GetAIFireTriggerArea(world._weaponProtos[71], &area) && Near(area.forward, aircraft._rotation.AxisZ()),
              "manual aerial volume orientation retains its original hull axes");
        aircraft._oflags = 0;
        aircraft._height = 450;
        aircraft._rotation = mat3x3::RotateY(0.4)*mat3x3::RotateX(-0.3)*mat3x3::RotateZ(0.1);
        fire.pos = aircraft._position + aircraft._rotation.Transpose().Transform(vec3d(0, 250, 700));
        aircraft._primTpos = fire.pos;
        fire.weapon = 70;
        Check(aircraft.CheckFireAI(&fire) == 1, "pitched and rolled aircraft fixture lies within vanilla sector aim allowance");
        for (int weapon : {71, 72}) {
            fire.weapon = weapon;
            Check(aircraft.CheckFireAI(&fire) == 1 && aircraft.GetAIFireTriggerArea(world._weaponProtos[weapon], &area) &&
                  Near(area.right, aircraft._rotation.AxisX()) && ypabact_FireTriggerAreaContains(area, fire.pos, 0),
                  "aerial sector elevation preserves yaw and roll while tilting only the weapon axis");
        }
        aircraft._height = 250; aircraft._rotation = mat3x3::Ident();
        aircraft._primTpos = aircraft._position + vec3d(0, 250, 700); fire.pos = aircraft._primTpos;

        SectorFightProbe legacy, configured, cone;
        for (auto *unit : {&legacy, &configured, &cone}) {
            unit->Bind(world, 1, 611, aircraft._position);
            unit->_bact_type = type;
            unit->_primTtype = BACT_TGT_TYPE_CELL; unit->_secndTtype = BACT_TGT_TYPE_NONE;
            unit->_primT.pcell = &world.SectorAt(World::PositionToSectorID(aircraft._primTpos));
            unit->_secndT.pcell = nullptr;
            unit->_primTpos = aircraft._primTpos;
            unit->_height = 250; unit->_aggr = 100;
            unit->_clock = unit->_assess_time = 1000; unit->_atk_ret = NC_STACK_ypabact::TA_FIGHT;
            unit->_unifiedAICombatDistance = true;
            unit->_ai_attack_range = 250; unit->_ai_retreat_start_range = 100; unit->_ai_retreat_stop_range = 220;
            unit->_wrldSize = vec2d(9600, -9600);
            unit->_fly_dir = vec3d(0, 0, 1);
        }
        legacy._weapon = 70; configured._weapon = 71; cone._weapon = 72;
        for (float distance : {700.f, 450.f, 99.f, 150.f, 219.f, 220.f, 700.f}) {
            for (auto *unit : {&legacy, &configured, &cone}) {
                unit->_position = unit->_primTpos - vec3d(0, 250, distance);
                bact_arg75 fight{}; fight.target.pcell = unit->_primT.pcell;
                fight.pos = unit->_primTpos; fight.g_time = 1000;
                unit->FightWithSect(&fight);
            }
            // ATTACK only reports whether the last shot was possible; movement uses APPROACH and target_vec.
            const int moveFlags = ~BACT_STFLAG_ATTACK;
            Check((legacy._status_flg & moveFlags) == (configured._status_flg & moveFlags) &&
                  Near(legacy._target_vec, configured._target_vec),
                  "sector fight keeps vanilla retreat/attack movement with forward box enabled");
            Check((legacy._status_flg & moveFlags) == (cone._status_flg & moveFlags) &&
                  Near(legacy._target_vec, cone._target_vec),
                  "sector fight keeps vanilla retreat/attack movement with forward cone enabled");
        }
        Check(legacy.launches == 2, "unconfigured sector fight fires only outside the vanilla close-range dead zone");
        Check(configured.launches == 3 && configured.lastLaunch.tgType == BACT_TGT_TYPE_CELL &&
              configured.lastLaunch.direction.y > 0.0f,
              "forward box also fires at mid range, but not steeply down from above the sector");
        Check(cone.launches == 3 && cone.lastLaunch.tgType == BACT_TGT_TYPE_CELL && cone.lastLaunch.direction.y > 0.0f,
              "forward cone also fires at mid range, but not steeply down from above the sector");
    }
}

void TestVolumeDistanceAndMovement() {
    WorldProbe world;
    Check(ParseWeapons(world, {
        "new_weapon 40", "model = rocket", "ai_fire_trigger_box_forward_xyz = 100_100_5000", "end",
        "new_weapon 41", "model = missile", "ai_fire_trigger_cone_forward_xyz = 30_20_5000", "end",
        "new_weapon 42", "model = laser", "life_time = 100", "ai_fire_trigger_box_forward_xyz = 100_100_5000", "end",
        "new_weapon 43", "model = rocket", "end",
        "new_weapon 44", "model = rocket", "ai_fire_trigger_box_forward_xyz = 0", "end",
        "new_weapon 45", "model = rocket", "ai_fire_trigger_box_forward_xyz = 100_100_bad", "end",
        "new_weapon 46", "model = rocket", "ai_fire_trigger_box_forward_xyz = 100_100_5000",
        "ai_fire_trigger_cone_forward_xyz = 30_20_5000", "end"}), "distance/movement fixtures parsed");
    FightProbe shooter; shooter.Bind(world, 1, 400, vec3d(2400, 0, -7200));
    Unit enemy; enemy.Bind(world, 2, 401, shooter._position + vec3d(0, 0, 4000));
    for (int weapon : {40, 41}) {
        bact_arg101 test{};
        test.unkn = 2; test.pos = enemy._position; test.radius = enemy._radius; test.weapon = weapon;
        Check(shooter.CheckFireAI(&test) == 1, "configured volume fires past the former 1200/2400 limits");
        test.pos = shooter._position + vec3d(700, 0, 0);
        Check(shooter.CheckFireAI(&test) == 1 && test.fire_trigger_area_target == &enemy,
              "full-depth sector scan finds an alternate enemy beyond the former search cap");
    }
    bact_arg101 laser{};
    laser.unkn = 2; laser.pos = enemy._position; laser.radius = 0; laser.weapon = 42;
    Check(shooter.CheckFireAI(&laser) == 0, "long trigger does not extend the laser's actual reach");
    laser.weapon = 43;
    Check(shooter.CheckFireAI(&laser) == 0, "absent volume preserves the legacy distance gate");

    shooter._weapon = 40; shooter._extra_weapons.fill(0);
    shooter._weapon_ai_switch_mode = World::TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
    shooter._weapon_slot_index = 0;
    shooter._primTtype = BACT_TGT_TYPE_UNIT; shooter._primT.pbact = &enemy;
    shooter._secndTtype = BACT_TGT_TYPE_NONE;
    shooter._unifiedAICombatDistance = true;
    shooter._ai_attack_range = 900; shooter._ai_retreat_start_range = 100; shooter._ai_retreat_stop_range = 600;
    shooter._wrldSize = vec2d(9600, -9600);
    shooter._clock = shooter._assess_time = 1000; shooter._atk_ret = NC_STACK_ypabact::TA_FIGHT;
    bact_arg75 fight{}; fight.target.pbact = &enemy; fight.g_time = 1000; fight.fperiod = 16;
    shooter.FightWithBact(&fight);
    Check(shooter.launches == 1 && !(shooter._status_flg & BACT_STFLAG_ATTACK),
          "volume authorizes fire beyond attack range while movement remains outside its attack band");
    enemy._position = shooter._position + vec3d(0, 0, 50);
    shooter.FightWithBact(&fight);
    Check(shooter.launches == 2 && (shooter._status_flg & BACT_STFLAG_APPROACH),
          "volume authorizes fire during the existing retreat maneuver");
    enemy._position = shooter._position + vec3d(0, 0, 400);
    shooter.FightWithBact(&fight);
    Check(shooter.launches == 3 && (shooter._status_flg & BACT_STFLAG_APPROACH),
          "retreat hysteresis changes movement without suppressing volume fire");
    enemy._position = shooter._position + vec3d(0, 0, 700);
    shooter.FightWithBact(&fight);
    Check(shooter.launches == 4 && !(shooter._status_flg & BACT_STFLAG_APPROACH) &&
          (shooter._status_flg & BACT_STFLAG_ATTACK), "normal re-attack after retreat remains intact");
    enemy._position = shooter._position + vec3d(0, 0, 4000);
    for (int weapon : {43, 44, 45, 46}) {
        shooter._weapon = weapon;
        shooter.FightWithBact(&fight);
        Check(shooter.launches == 4,
              "movement settings alone never enable long-range volume fire: absent/zero/invalid/conflicting volume");
    }
    enemy._position = shooter._position + vec3d(0, 0, 700);
    for (int weapon : {43, 44, 45, 46}) {
        const int before = shooter.launches;
        shooter._weapon = weapon;
        shooter.FightWithBact(&fight);
        Check(shooter.launches == before+1,
              "absent/zero/invalid/conflicting volume still permits the normal legacy shot");
    }
}

void TestDebugSegmentClipping() {
    const mat4x4f projection = mat4x4f::UAFrustum(1.0f, 10000.0f);
    Common::Line line;
    Check(yw_ProjectDebugSegment(vec3d(-5, 0, 2), vec3d(5, 0, 2), projection, 1, 320, 200, &line) &&
          line.x1 == 0 && line.x2 == 319 && line.y1 == 100 && line.y2 == 100,
          "debug edge crossing viewport survives with both endpoints off screen");
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 320, 200, 32, SDL_PIXELFORMAT_ARGB8888);
    Check(surface != nullptr, "offscreen debug drawing surface created");
    if ( surface ) {
        SDL_FillRect(surface, nullptr, 0);
        GFX::GFXEngine::DrawLine(surface, line, 255, 150, 30);
        const uint32_t orange = SDL_MapRGB(surface->format, 255, 150, 30);
        const uint32_t *pixels = (const uint32_t *)surface->pixels;
        const int row = 100*surface->pitch/4;
        Check(pixels[row] == orange && pixels[row+160] == orange && pixels[row+319] == orange,
              "production DrawLine draws the clipped edge through both screen borders");
        SDL_FreeSurface(surface);
    }
    Check(yw_ProjectDebugSegment(vec3d(0.1, 0, -2), vec3d(0.2, 0, 5), projection, 1, 320, 200, &line),
          "debug edge crossing near plane keeps its visible part");
    Check(yw_ProjectDebugSegment(vec3d(0.2, 0, 5), vec3d(0.1, 0, -2), projection, 1, 320, 200, &line),
          "near-plane clipping is independent of endpoint order");
    Check(!yw_ProjectDebugSegment(vec3d(0, 0, -5), vec3d(0, 0, 0.5), projection, 1, 320, 200, &line),
          "debug edge entirely behind near plane stays hidden");
    Check(!yw_ProjectDebugSegment(vec3d(-5, 0, 2), vec3d(-3, 0, 2), projection, 1, 320, 200, &line),
          "debug edge entirely outside one screen side stays hidden");
    Check(yw_ProjectDebugSegment(vec3d(0, -5, 2), vec3d(0, 5, 2), projection, 1, 320, 200, &line) &&
          line.y1 == 0 && line.y2 == 199, "debug edge clips correctly at top and bottom borders");
}

void TestRealLaunchGateParity() {
    WorldProbe world;
    Check(ParseWeapons(world, {
        "new_weapon 30", "model = rocket", "start_speed = 100",
        "ai_fire_trigger_box_forward_xyz = 240_240_600", "shot_time = 100",
        "shot_time_user = 200", "salve_shots = 2", "salve_delay = 300", "end",
        "new_weapon 31", "model = rocket", "start_speed = 100",
        "ai_fire_trigger_cone_forward_xyz = 80_40_600", "end",
        "new_weapon 32", "model = missile", "start_speed = 100", "multi_target = 3",
        "ai_fire_trigger_cone_forward_xyz = 80_40_600", "end",
        "new_weapon 33", "model = homing_bomb", "multi_target = 3",
        "ai_fire_trigger_box_below_xyz = 400_1200_400", "end",
        "new_weapon 34", "model = homing_bomb", "multi_target = 3",
        "ai_fire_trigger_cone_below_xyz = 80_60_600", "end"}),
        "real-launch fixture weapons parsed");
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypamissile>());

    std::vector<LaunchScenario> scenarios(9);
    scenarios[0].weapon = 30;
    scenarios[0].times = {1000};
    scenarios[1].weapon = 31; scenarios[1].countMin = scenarios[1].countMax = 2;
    scenarios[1].fireXMode = World::TVhclProto::FIRE_X_MODE_SEQUENCE;
    scenarios[1].advancedRack = true; scenarios[1].arcX = 8; scenarios[1].arcY = 5;
    scenarios[1].spreadX = 6; scenarios[1].spreadY = 4;
    scenarios[1].times = {1000};
    scenarios[2].weapon = 32; scenarios[2].countMin = 4; scenarios[2].countMax = 6;
    scenarios[2].fireXMode = World::TVhclProto::FIRE_X_MODE_RANDOM;
    scenarios[2].advancedRack = true; scenarios[2].coneXY = 0.25f;
    scenarios[2].spreadX = 8; scenarios[2].spreadY = 5;
    scenarios[2].expectMultiTarget = true; scenarios[2].times = {1000};
    scenarios[3].weapon = 33; scenarios[3].countMin = scenarios[3].countMax = 4;
    scenarios[3].fireXMode = World::TVhclProto::FIRE_X_MODE_SALVE_MIRROR;
    scenarios[3].spreadX = 8; scenarios[3].spreadZ = 8;
    scenarios[3].expectMultiTarget = true; scenarios[3].times = {1000};
    scenarios[4].weapon = 30; scenarios[4].times = {100, 200, 300, 400, 500, 600};
    scenarios[5] = scenarios[4]; scenarios[5].playerInput = true;
    scenarios[6] = scenarios[1]; scenarios[6].countMin = scenarios[6].countMax = 4;
    scenarios[6].spreadX = scenarios[6].spreadY = 0;
    scenarios[7] = scenarios[3]; scenarios[7].fireXMode = World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE;
    scenarios[8] = scenarios[3]; scenarios[8].weapon = 34;

    int gid = 200;
    for (const LaunchScenario &scenario : scenarios) {
        Unit launcher; launcher.Bind(world, 1, gid++, vec3d(2400, 0, -2400));
        ResetLaunchState(launcher, scenario);
        const World::TWeapProto &weapon = world._weaponProtos[scenario.weapon];
        TFireTriggerArea area;
        Check(launcher.GetAIFireTriggerArea(weapon, &area), "real-launch weapon exposes its configured trigger shape");
        vec3d primaryPosition = area.top + area.forward * 100;
        Unit primary; primary.Bind(world, 2, gid++, primaryPosition);
        Unit alternate1; alternate1.Bind(world, 2, gid++, area.top + area.forward * 180 + area.right * 10);
        Unit alternate2; alternate2.Bind(world, 2, gid++, area.top + area.forward * 260 - area.right * 10);

        LaunchSnapshot gated = RunLaunchSequence(world, launcher, primary, weapon, scenario, true);
        LaunchSnapshot plain = RunLaunchSequence(world, launcher, primary, weapon, scenario, false);
        Check(gated.gateAccepted.size() == scenario.times.size() &&
              std::find(gated.gateAccepted.begin(), gated.gateAccepted.end(), false) == gated.gateAccepted.end(),
              "every real-launch gate query accepts the in-volume primary target");
        Check(SameLaunchSnapshot(gated, plain), "gate query leaves actual LaunchMissile results and state unchanged");

        if ( scenario.expectMultiTarget ) {
            std::vector<int> targetGids;
            for (const ProjectileSnapshot &projectile : gated.projectiles) {
                if ( projectile.targetGid &&
                     std::find(targetGids.begin(), targetGids.end(), projectile.targetGid) == targetGids.end() )
                    targetGids.push_back(projectile.targetGid);
            }
            Check(targetGids.size() >= 2, "real missile or homing-bomb salvo distributes to multiple live targets");
        }
        if ( scenario.weapon == 31 && scenario.countMin == 4 && scenario.arcX > 0.0f && scenario.arcY > 0.0f ) {
            bool hasHorizontalArc = false;
            bool hasVerticalArc = false;
            for (const ProjectileSnapshot &projectile : gated.projectiles) {
                hasHorizontalArc = hasHorizontalArc || std::fabs(projectile.direction.x) > 0.01f;
                hasVerticalArc = hasVerticalArc || std::fabs(projectile.direction.y) > 0.01f;
            }
            Check(hasHorizontalArc && hasVerticalArc,
                  "real four-projectile launch applies both weapon_arc_x and weapon_arc_y");
        }
        if ( scenario.fireXMode == World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE )
            Check(gated.fireXSlot == 0, "real salve-sequence launch advances its side slot");
        if ( scenario.times.size() > 1 ) {
            const std::vector<int> expected = scenario.playerInput
                ? std::vector<int>({1, 0, 1, 0, 0, 1})
                : std::vector<int>({1, 1, 0, 0, 1, 1});
            Check(gated.launchReturns == expected, "real launch preserves shot_time, shot_time_user and salve_delay cadence");
            Check(gated.weaponTime == 600, "real cadence launch records the last permitted shot time");
            Check(gated.salveCounter == (scenario.playerInput ? 1 : 2),
                  "real cadence launch preserves salve counter across the structural pause");
        }
    }
}
}

int main() {
    WorldProbe w;
    Check(ParseWeapons(w, {"new_weapon 1", "model = bomb", "ai_fire_trigger_box_below_xyz = 200_1200_400", "end",
                           "new_weapon 2", "model = homing_bomb", "ai_fire_trigger_box_below_xyz = 100_1200_50", "end",
                           "new_weapon 3", "model = rocket", "ai_fire_trigger_box_below_xyz = 200_1200_400", "end",
                           "new_weapon 4", "model = bomb", "ai_fire_trigger_box_below_xyz = 200", "end",
                           "new_weapon 5", "model = bomb", "ai_fire_trigger_box_below_xyz = -1_1200_100", "end",
                           "new_weapon 6", "model = bomb", "ai_fire_trigger_box_below_xyz = nan_1200_100", "end",
                           "new_weapon 7", "model = bomb", "end",
                           "new_weapon 8", "model = laser", "vertical_laser_enable = 1", "ai_fire_trigger_box_below_xyz = 300_1200_300", "end",
                           "new_weapon 9", "model = laser", "ai_fire_trigger_box_below_xyz = 300_1200_300", "end",
                           "new_weapon 10", "ai_fire_trigger_box_forward_xyz = 200_100_500", "model = rocket", "end",
                           "new_weapon 11", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 90_30_500", "end",
                           "new_weapon 12", "model = bomb", "ai_fire_trigger_box_forward_xyz = 200_100_500", "end",
                           "new_weapon 13", "model = artillery_shell", "ai_fire_trigger_cone_forward_xyz = 90_30_500", "end",
                           "new_weapon 14", "model = kamikaze", "ai_fire_trigger_box_forward_xyz = 200_100_500", "end",
                           "new_weapon 15", "model = rocket", "ai_fire_trigger_box_forward_xyz = 200_100_500", "ai_fire_trigger_cone_forward_xyz = 90_30_500", "end",
                           "new_weapon 16", "model = rocket", "ai_fire_trigger_box_forward_xyz = 200_100_500junk", "end",
                           "new_weapon 17", "model = rocket", "ai_fire_trigger_cone_forward_xyz = 180_30_500", "end",
                           "new_weapon 18", "model = rocket", "ai_fire_trigger_box_forward_xyz = 0", "end",
                           "new_weapon 19", "model = bomb", "ai_fire_trigger_box_below_xyz = 200_1200_400tail", "end",
                           "new_weapon 20", "model = laser", "life_time = 2000", "ai_fire_trigger_box_forward_xyz = 100_100_500", "end",
                           "new_weapon 21", "model = arc_grenade", "start_speed = 10", "ai_fire_trigger_box_forward_xyz = 200_200_600", "end"}),
          "weapon parser fixtures loaded");
    const auto &wp = w._weaponProtos;
    Check(wp[1].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_BOX_BELOW && wp[1].ai_fire_trigger_box_below.x == 200.0f &&
          wp[1].ai_fire_trigger_box_below.z == 400.0f, "bomb below shape parsed");
    Check(wp[2].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_BOX_BELOW, "homing bomb keeps below shape");
    Check(wp[3].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "rocket rejects below shape");
    Check(wp[4].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE && wp[4].ai_fire_trigger_box_below.x == 0,
          "malformed below tuple disables shape");
    Check(wp[5].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE && wp[5].ai_fire_trigger_box_below.x == 0,
          "negative below dimension disables shape");
    Check(wp[6].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "non-finite below dimension disables shape");
    Check(wp[7].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "absent shape keeps legacy fire control");
    Check(wp[8].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_BOX_BELOW, "vertical laser accepts below shape");
    Check(wp[9].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "normal laser rejects below shape");
    Check(wp[10].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_BOX_FORWARD, "forward shape before model is accepted at block end");
    Check(wp[11].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_CONE_FORWARD, "cone shape after model is accepted");
    Check(wp[12].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "bomb rejects forward shape");
    Check(wp[13].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "artillery shell rejects front shape");
    Check(wp[14].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "kamikaze rejects front shape");
    Check(wp[15].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "two positive shapes conflict to legacy behavior");
    Check(wp[16].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "forward tuple with trailing text is invalid");
    Check(wp[17].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "cone angle at 180 degrees is invalid");
    Check(wp[18].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE, "single zero disables forward shape");
    Check(wp[19].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_NONE && wp[19].ai_fire_trigger_box_below.x == 0,
          "below tuple with trailing text is invalid");
    Check(wp[20].GetAIFireTriggerShape() == World::TWeapProto::AI_TRIGGER_BOX_FORWARD, "normal laser accepts forward hold shape");

    TestBelowCompatibility(w, wp.data());
    TestTankElevation();
    TestDownwardCone(w);
    TestFrontVolumes(w, wp.data());
    TestArcFallback();
    TestTurretSlotsAndCadence();
    TestRealLaunchGateParity();
    TestVanillaSectorMinigun();
    TestMaxInclinationParse();
    TestMinimumDepth();
    TestAirTiltAbsent();
    TestTankTiltCap();
    TestAirSectorElevation();
    TestVolumeDistanceAndMovement();
    TestDebugSegmentClipping();

    std::printf("CHECKS=%d FAILED=%d\n", checks, failures);
    return failures ? 1 : 0;
}
