// Focused integration probe against the production parser and actor methods.
// Link production objects with main.cpp's SDL_main renamed; no game session needed.
#include "../../yw.h"
#include "../../ypatank.h"
#include "../../ypacar.h"
#include "../../ypaflyer.h"
#include "../../ypagun.h"
#include "../parsers.h"
#include <cmath>
#include <cstdio>
#include <limits>
#undef main

namespace {
struct TestWorld : NC_STACK_ypaworld {
    cellArea targetCell;
    size_t GetSectorInfo(yw_130arg *arg) override {
        arg->pcell = &targetCell;
        arg->CellId = Common::Point(0, 0);
        return 1;
    }
    void ypaworld_func149(ypaworld_arg136 *arg) override { arg->isect = false; }
};
int checks = 0, failures = 0;
void Check(bool value, const char *name) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", name); }
}
bool Parse(NC_STACK_ypaworld &world, const Engine::StringList &lines) {
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
vec3d Aim(double percent) {
    float cw, ch;
    GFX::Engine.getAspectCorrection(cw, ch, true);
    // Independent oracle: half-down = -0.15 offset, three-quarter-up = +0.6.
    const double offset = percent * (percent < 0 ? 0.003 : 0.008);
    return vec3d(0.0, -offset * ch, 1.0);
}
template<class Actor> struct TestActor : Actor { void BindWorld(NC_STACK_ypaworld &world) { this->_world = &world; } };
template<class Actor> void Configure(Actor &actor, NC_STACK_ypaworld &world, int type) {
    actor.BindWorld(world);
    actor._vehicleID = 1;
    actor._bact_type = type;
    actor._rotation = mat3x3::Ident();
    actor._gun_angle_user = 0.0;
    actor._gun_leftright = 0.0;
}
}
int main() {
    TestWorld world;
    world._vhclProtos.resize(256);
    Check(Parse(world, {"new_vehicle 1", "model = tank", "end"}), "vanilla parser fixture");
    TestActor<NC_STACK_ypatank> tank;
    TestActor<NC_STACK_ypacar> car;
    TestActor<NC_STACK_ypaflyer> flyer;
    TestActor<NC_STACK_ypagun> gun;
    Configure(tank, world, BACT_TYPES_TANK);
    Configure(car, world, BACT_TYPES_CAR);
    Configure(flyer, world, BACT_TYPES_FLYER);
    Configure(gun, world, BACT_TYPES_GUN);
    auto &proto = world._vhclProtos[1];
    Check(!proto.scope_angle_min_max_set, "absent scope preserves vanilla");
    Check(tank.IsPrimaryWeaponElevationAllowed(Aim(-150)) && tank.IsPrimaryWeaponElevationAllowed(Aim(150)), "absent preserves actual vanilla AI beyond reticle travel");
    Check(Parse(world, {"modify_vehicle 1", "scope_angle_min_max = -50_75", "end"}), "single interval parser");
    for (auto *actor : {static_cast<NC_STACK_ypabact*>(&tank), static_cast<NC_STACK_ypabact*>(&car)}) {
        Check(actor->IsPrimaryWeaponElevationAllowed(Aim(-50)), "inclusive lower boundary");
        Check(actor->IsPrimaryWeaponElevationAllowed(Aim(75)), "inclusive upper boundary");
        Check(actor->IsPrimaryWeaponElevationAllowed(Aim(0)), "inside window");
        Check(!actor->IsPrimaryWeaponElevationAllowed(Aim(-50.01)), "below lower boundary");
        Check(!actor->IsPrimaryWeaponElevationAllowed(Aim(75.01)), "above upper boundary");
        for (const mat3x3 &rotation : {mat3x3::RotateX(0.45), mat3x3::RotateX(-0.45),
                                      mat3x3::RotateY(1.2), mat3x3::RotateZ(0.4),
                                      mat3x3::RotateZ(0.4)*mat3x3::RotateY(1.2)*mat3x3::RotateX(0.45)}) {
            actor->_rotation = rotation;
            Check(actor->IsPrimaryWeaponElevationAllowed(rotation.Transpose().Transform(Aim(50))), "hull frame on slope yaw roll");
            Check(!actor->IsPrimaryWeaponElevationAllowed(rotation.Transpose().Transform(Aim(80))), "hull frame blocks high on slope yaw roll");
        }
        actor->_rotation = mat3x3::Ident();
        actor->_gun_angle_user = 0.8;
        Check(!actor->IsPlayerPrimaryWeaponElevationAllowed() && actor->_gun_angle_user == 0.8f,
              "free upper reticle remains movable outside window");
        actor->_gun_angle_user = -0.3;
        Check(!actor->IsPlayerPrimaryWeaponElevationAllowed() && actor->_gun_angle_user == -0.3f,
              "free lower reticle remains movable outside window");
        actor->_oflags = BACT_OFLAG_USERINPT;
        actor->_energy = 777;
        actor->_weapon_time = 123;
        actor->_salve_counter = 2;
        actor->_laser_fire_request = false;
        actor->_vertical_laser_fire_request = false;
        const auto missileCount = actor->_missiles_list.size();
        bact_arg79 request{};
        request.direction = Aim(0); // Player permission must follow the reticle, not this request.
        actor->LaunchMissile(&request);
        Check(actor->_energy == 777 && actor->_weapon_time == 123 && actor->_salve_counter == 2 &&
              !actor->_laser_fire_request && !actor->_vertical_laser_fire_request && actor->_missiles_list.size() == missileCount,
              "blocked player fire has no projectile energy cadence or laser side effects");
        actor->_oflags = 0;
        actor->_gun_angle_user = 0.0;
        request.direction = Aim(80);
        actor->_laser_active = true;
        actor->LaunchMissile(&request);
        Check(actor->_energy == 777 && actor->_weapon_time == 123 && !actor->_laser_fire_request &&
              actor->_missiles_list.size() == missileCount, "active AI laser cannot bypass central elevation gate");
        actor->_laser_active = false;
    }
    tank._gun_angle_user = -0.15f;
    tank._gun_leftright = 0.8f;
    Check(tank.IsPlayerPrimaryWeaponElevationAllowed(), "half-down boundary with lateral reticle");
    tank._gun_angle_user = -0.1501f;
    Check(!tank.IsPlayerPrimaryWeaponElevationAllowed(), "below half-down with lateral reticle");
    tank._gun_angle_user = 0.6f;
    Check(tank.IsPlayerPrimaryWeaponElevationAllowed(), "75 reference up boundary with lateral reticle");
    tank._gun_angle_user = 0.6001f;
    Check(!tank.IsPlayerPrimaryWeaponElevationAllowed(), "above 75 reference up with lateral reticle");
    tank._gun_leftright = 0.0f;
    Check(flyer.IsPrimaryWeaponElevationAllowed(Aim(80)) && gun.IsPrimaryWeaponElevationAllowed(Aim(-80)), "air and mounted guns excluded");
    for (const char *bad : {"nan_20", "-20_inf", "-101_20", "-20_101", "10junk_20",
                            "10_20_30", "bad", "50", "0%_20%", "25_-10", "_20", "20_"}) {
        const auto range = proto.scope_angle_min_max;
        const bool wasSet = proto.scope_angle_min_max_set;
        Check(!Parse(world, {"modify_vehicle 1", std::string("scope_angle_min_max = ")+bad, "end"}) &&
              proto.scope_angle_min_max == range && proto.scope_angle_min_max_set == wasSet,
              "invalid interval rejected without fallback or state changes");
    }
    Check(Parse(world, {"modify_vehicle 1", "scope_angle_min_max = 0_0", "end"}) &&
          tank.IsPrimaryWeaponElevationAllowed(Aim(0)) && !tank.IsPrimaryWeaponElevationAllowed(Aim(1)) &&
          !tank.IsPrimaryWeaponElevationAllowed(Aim(-1)), "zero interval permits hull-parallel aim only");
    Check(Parse(world, {"modify_vehicle 1", "scope_angle_min_max = 20_80", "end"}) &&
          tank.IsPrimaryWeaponElevationAllowed(Aim(20)) && tank.IsPrimaryWeaponElevationAllowed(Aim(80)) &&
          !tank.IsPrimaryWeaponElevationAllowed(Aim(0)) && !tank.IsPrimaryWeaponElevationAllowed(Aim(-20)),
          "positive interval excludes hull-parallel and downward aim");
    Check(Parse(world, {"modify_vehicle 1", "scope_angle_min_max = -80_-20", "end"}) &&
          tank.IsPrimaryWeaponElevationAllowed(Aim(-80)) && tank.IsPrimaryWeaponElevationAllowed(Aim(-20)) &&
          !tank.IsPrimaryWeaponElevationAllowed(Aim(0)) && !tank.IsPrimaryWeaponElevationAllowed(Aim(20)),
          "negative interval excludes hull-parallel and upward aim");
    Check(Parse(world, {"new_vehicle 1", "model = tank", "end"}) &&
          !proto.scope_angle_min_max_set, "new_vehicle clears previous scope configuration");
    Check(Parse(world, {"new_vehicle 1", "model = tank", "scope_angle_min_max = -100_100", "end"}) &&
          tank.IsPrimaryWeaponElevationAllowed(Aim(-100)) && tank.IsPrimaryWeaponElevationAllowed(Aim(100)) &&
          !tank.IsPrimaryWeaponElevationAllowed(Aim(-100.01)) && !tank.IsPrimaryWeaponElevationAllowed(Aim(100.01)),
          "explicit full interval covers entire unchanged vanilla reticle travel");
    Check(Parse(world, {"modify_vehicle 1", "scope_angle_min_max = -50.5_75.25", "end"}) &&
          proto.scope_angle_min_max[0] == -50.5f && proto.scope_angle_min_max[1] == 75.25f &&
          tank.IsPrimaryWeaponElevationAllowed(Aim(-50.5)) && tank.IsPrimaryWeaponElevationAllowed(Aim(75.25)),
          "fractional reference interval includes endpoints");
    proto.scope_angle_min_max = {{-50, 75}};
    tank._gun_angle_user = 0.8;
    tank._gun_leftright = 0.8;
    const vec3d actual = tank.GetUserWeaponAimDirection();
    float cw, ch;
    GFX::Engine.getAspectCorrection(cw, ch, true);
    Check(actual == tank._rotation.AxisZ() - tank._rotation.AxisY()*(tank._gun_angle_user*ch) - tank._rotation.AxisX()*(tank._gun_leftright*cw), "shared tank direction preserves vanilla lateral aim");
    car._gun_angle_user = 0.8;
    car._gun_leftright = 0.8;
    Check(car.GetUserWeaponAimDirection() == car._rotation.AxisZ() - car._rotation.AxisY()*(car._gun_angle_user*ch), "car preserves no lateral offset");
    Check(!tank.IsPrimaryWeaponElevationAllowed(vec3d(0, std::numeric_limits<double>::quiet_NaN(), 1)), "invalid aim blocked only with active valid window");
    for (auto *actor : {static_cast<NC_STACK_ypabact*>(&tank), static_cast<NC_STACK_ypabact*>(&car)}) {
        for (float offset : {-0.3f, 0.8f}) {
            actor->_oflags = BACT_OFLAG_USERINPT;
            actor->_gun_angle_user = offset;
            actor->_mgun_set = true;
            actor->_mgun = 0;
            actor->_userHomingPrimaryTargetGid = 123;
            actor->_userHomingTargetCycleRequested = true;
            setTarget_msg target{};
            target.tgt_type = BACT_TGT_TYPE_UNIT;
            target.priority = 1;
            target.tgt.pbact = &flyer;
            actor->SetTarget(&target);
            world._guiVisor.field_18 = &flyer;
            world._hudMissileMultiLockTargets = {&flyer, &gun};
            bact_arg106 targeting{};
            targeting.field_4 = Aim(0); // Permission must still follow the reticle.
            targeting.ret_bact = &flyer;
            Check(actor->UserTargeting(&targeting) == 0 && targeting.ret_bact == NULL &&
                  world._guiVisor.field_18 == NULL && world._hudMissileMultiLockTargets.empty(),
                  "outside either bound releases HUD and multi-target lock");
            Check(actor->_secndTtype == BACT_TGT_TYPE_NONE && actor->_secndT.pbact == NULL &&
                  actor->_userHomingPrimaryTargetGid == 0 && !actor->_userHomingTargetCycleRequested,
                  "outside bound clears retained and pending lock targets");
            Check(actor->UserTargeting(&targeting) == 0 && targeting.ret_bact == NULL,
                  "outside bound prevents reacquisition on following frame");
            Check(!actor->RequestHomingTargetCycle() && !actor->_userHomingTargetCycleRequested,
                  "outside bound rejects manual target cycling");
            Check(actor->HasMinigun() && world._guiVisor.field_0 == 1 &&
                  world._guiVisor.field_C == -offset,
                  "disabled primary lock preserves independent MG reticle");
        }
    }
    world.GetWeaponsProtos().resize(2);
    world.GetWeaponsProtos()[1]._weaponFlags = World::TWeapProto::WEAPON_FLAGS_MISSILE;
    proto.scope_angle_min_max = {{-10, 10}};
    flyer._status = BACT_STATUS_NORMAL;
    flyer._status_flg = 0;
    flyer._owner = 2;
    flyer._energy = flyer._energy_max = 1000;
    flyer._cellRef = world.targetCell.unitsList.push_back(&flyer);
    for (auto *actor : {static_cast<NC_STACK_ypabact*>(&tank), static_cast<NC_STACK_ypabact*>(&car)}) {
        actor->_rotation = mat3x3::Ident();
        actor->_gun_leftright = actor->_gun_angle_user = 0;
        actor->_owner = 1;
        actor->_old_pos = actor->_position;
        actor->_weapon = actor->_current_weapon_id = 1;
        actor->_oflags = BACT_OFLAG_USERINPT;
        bact_arg106 targeting{};
        targeting.field_0 = 5;
        targeting.field_4 = actor->GetUserWeaponAimDirection();
        flyer._position = actor->_position + Aim(20) * 400;
        Check(actor->UserTargeting(&targeting) == 0 && world._guiVisor.field_18 == NULL,
              "valid reticle cannot acquire an out-of-scope enemy");
        flyer._position = actor->_position + Aim(5) * 400;
        Check(actor->UserTargeting(&targeting) == 1 && targeting.ret_bact == &flyer &&
              world._guiVisor.field_18 == &flyer, "in-scope enemy still acquires normal lock");
        flyer._position = actor->_position + Aim(20) * 400;
        Check(actor->UserTargeting(&targeting) == 0 && actor->_secndTtype == BACT_TGT_TYPE_NONE &&
              world._guiVisor.field_18 == NULL, "moving enemy releases lock after leaving scope");
        actor->_oflags = 0;
        bact_arg101 ai{};
        ai.unkn = 2;
        ai.weapon = 1;
        for (double reference : {-20.0, 20.0}) {
            ai.pos = actor->_position + Aim(reference) * 400;
            Check(actor->CheckFireAI(&ai) == 0, "AI primary respects both scope limits");
            ai.weapon = 0;
            Check(actor->CheckFireAI(&ai) == 1, "AI MG fallback ignores scope and keeps normal firing checks");
            ai.weapon = 1;
        }
        ai.pos = actor->_position + Aim(5) * 400;
        Check(actor->CheckFireAI(&ai) == 1, "AI primary still approves an in-scope enemy");
        actor->_oflags = BACT_OFLAG_USERINPT;
        flyer._position = actor->_position + Aim(20) * 400;
        bact_arg79 blocked{};
        blocked.tgType = BACT_TGT_TYPE_UNIT;
        blocked.target.pbact = &flyer;
        blocked.direction = Aim(0);
        const int energy = actor->_energy;
        const auto missiles = actor->_missiles_list.size();
        Check(actor->LaunchMissile(&blocked) == 0 && actor->_energy == energy &&
              actor->_missiles_list.size() == missiles, "target direction cannot bypass valid player reticle scope");
    }
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
