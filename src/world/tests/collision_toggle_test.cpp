// Exercise the public parser and production collision gates without a mission.
#include "game/yw.h"
#include "game/ypagun.h"
#include "game/ypatank.h"
#include "game/ypamissile.h"
#include "game/ypabact_internal.h"
#include "nucleus/base.h"
#include "../parsers.h"
#include "../collision_shape.h"
#include <cstdio>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *label) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
struct WorldProbe : NC_STACK_ypaworld {
    int traces = 0, contacts = 0;
    bool worldHit = true;
    WorldProbe() {
        _mapSize = Common::Point(8, 8); _cells.Resize(_mapSize);
        _vhclProtos.resize(256); _weaponProtos.resize(256);
        _buildProtos.resize(256); _roboProtos.resize(1);
    }
    void ypaworld_func136(ypaworld_arg136 *q) override {
        ++traces; q->isect = worldHit; q->tVal = .5f;
    }
    void ypaworld_func149(ypaworld_arg136 *q) override { ypaworld_func136(q); }
    void ypaworld_func137(ypaworld_arg137 *q) override { ++contacts; q->coll_count = 1; }
    void ypaworld_func180(yw_arg180 *) override {}
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 0; }
};
bool Parse(WorldProbe &w, Engine::StringList lines) {
    ScriptParser::HandlersList handlers{
        new World::Parsers::VhclProtoParser(&w), new World::Parsers::WeaponProtoParser(&w)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
template<class Base = NC_STACK_ypabact> struct Actor : Base {
    void Bind(WorldProbe &w, int type = BACT_TYPES_BACT) {
        this->_world = &w; this->_bact_type = type; this->_status = BACT_STATUS_NORMAL;
        this->_status_flg = 0; this->_oflags = BACT_OFLAG_BACTCOLL | BACT_OFLAG_EXACTCOLL;
        this->_position = this->_old_pos = vec3d(2400, -100, -2400);
        this->_rotation = mat3x3::Ident(); this->_radius = this->_viewer_radius = 30;
        this->_gid = type; this->_owner = 1; this->_energy = this->_energy_max = 1000;
        this->_vp_normal = this->_vp_genesis = nullptr; this->_soundcarrier.Resize(7);
        this->_pSector = &w.SectorAt(World::PositionToSectorID(this->_position));
        this->_cellRef = this->_pSector->unitsList.push_back(this);
    }
};
struct DamageTarget : Actor<> {
    int hits = 0;
    void ModifyEnergy(bact_arg84 *) override { ++hits; }
};
std::shared_ptr<Collision::Shape> Cube() {
    auto shape = std::make_shared<Collision::Shape>();
    Collision::Part p;
    for (const auto &v : {vec3d(-20,-20,-20),vec3d(20,-20,-20),vec3d(20,20,-20),vec3d(-20,20,-20),
                         vec3d(-20,-20,20),vec3d(20,-20,20),vec3d(20,20,20),vec3d(-20,20,20)})
        p.vertices.push_back(v);
    p.faces = {{{0,2,1}},{{0,3,2}},{{4,5,6}},{{4,6,7}},{{0,1,5}},{{0,5,4}},
               {{3,7,6}},{{3,6,2}},{{0,4,7}},{{0,7,3}},{{1,2,6}},{{1,6,5}}};
    shape->parts.push_back(p); Check(Collision::Build(*shape), "native shape fixture");
    return shape;
}
}

int main() {
    WorldProbe w;
    Check(Parse(w, {"new_vehicle 1", "model = gun", "end"}), "absent gun and collision values parse");
    auto &gunProto = w._vhclProtos[1];
    Check(!gunProto.disable_collision && gunProto.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_LEVITATE,0) == 0,
          "absent defaults are zero");
    Check(Parse(w, {"modify_vehicle 1", "gun_levitate = 1", "disable_collision = 1", "end"}), "enable values parse");
    Check(gunProto.disable_collision && gunProto.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_LEVITATE,0) == 1,
          "gun and collision enabled independently");
    NC_STACK_ypagun gun;
    gun.SetParameters(gunProto.initParams);
    Check(gun.getGUN_levitate(), "parsed levitation reaches runtime setter");
    Check(Parse(w, {"modify_vehicle 1", "gun_levitate = 0", "disable_collision = 0", "end"}), "disable values parse");
    gun.SetParameters(gunProto.initParams);
    Check(!gun.getGUN_levitate() && !gunProto.disable_collision, "zero reverses prior enable");
    for (const char *value : {"2", "-1", "true", "1junk", "nan", ""}) {
        Check(Parse(w, {"modify_vehicle 1", std::string("gun_levitate = ")+value,
                       std::string("disable_collision = ")+value, "end"}), "invalid optional values parse safely");
        Check(!gunProto.disable_collision && gunProto.initParams.Get<int32_t>(NC_STACK_ypagun::GUN_ATT_LEVITATE,0) == 0,
              "only literal one enables switches");
    }
    Check(Parse(w, {"new_weapon 1", "model = rocket", "end"}) && !w._weaponProtos[1].disable_collision,
          "absent weapon collision value defaults off");
    Check(Parse(w, {"modify_weapon 1", "disable_collision = 1", "end"}) && w._weaponProtos[1].disable_collision,
          "weapon collision switch parses");
    Check(Parse(w, {"new_weapon 1", "model = rocket", "end"}) && !w._weaponProtos[1].disable_collision,
          "new definition resets collision default");

    // Exercise the real factory, including pooled actors and tank ground placement.
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypagun>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypatank>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypamissile>());
    NC_STACK_base visual;
    w._vhclModels.resize(256, &visual);
    for (int type : {BACT_TYPES_GUN, BACT_TYPES_TANK}) {
        auto &proto = w._vhclProtos[2]; proto.model_id = type; proto.weapon = -1;
        proto.scale_fx_pXX.fill(0);
        for (bool disabled : {true, false}) {
            proto.disable_collision = disabled;
            ypaworld_arg146 request; request.vehicle_id = 2; request.pos = vec3d(2400,-100,-2400);
            auto *spawned = w.ypaworld_func146(&request);
            Check(spawned && spawned->CollisionsDisabled()==disabled, "vehicle factory copies current collision switch");
            if (spawned) {
                if (type==BACT_TYPES_TANK && disabled) {
                    bact_arg80 placement; placement.pos=request.pos; placement.field_C=1;
                    Check(spawned->SetPosition(&placement)==1 && spawned->_position==request.pos,
                          "collision-free tank accepts authored position without requiring ground");
                }
                spawned->Delete();
            }
        }
    }
    for (bool disabled : {true,false}) {
        w._weaponProtos[1].disable_collision=disabled;
        ypaworld_arg146 request; request.vehicle_id=1; request.pos=vec3d(2400,-100,-2400);
        auto *spawned=w.ypaworld_func147(&request);
        Check(spawned && spawned->CollisionsDisabled()==disabled, "weapon factory copies current collision switch");
        if (spawned) spawned->Delete();
    }

    Actor<> a, b; a.Bind(w); b.Bind(w);
    std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
    a.GetCollisionSpheres(spheres,a._position,a._rotation);
    Check(!spheres.empty(), "ordinary legacy sphere exists");
    Check(a.GetUnitCollisionContact(&b,nullptr,nullptr,nullptr), "ordinary overlapping legacy actors contact");
    a._disableCollision = true;
    a.GetCollisionSpheres(spheres,a._position,a._rotation);
    Check(spheres.empty() && a.GetCollisionBroadRadius()==0, "disabled legacy volume removed");
    Check(!a.getBACT_exactCollisions() && !a.getBACT_bactCollisions(), "legacy collision getters respect switch");
    Check(!a.GetUnitCollisionContact(&b,nullptr,nullptr,nullptr) &&
          !b.GetUnitCollisionContact(&a,nullptr,nullptr,nullptr), "legacy contacts disabled symmetrically");
    ypaworld_arg136 ray; ray.isect = true;
    a.TraceWorldCollision(&ray); a.TraceWorldCollision(&ray,true);
    Check(!ray.isect && ray.tVal==2 && w.traces==0, "terrain and exact world traces bypassed");
    ypaworld_arg137 contacts{}; contacts.coll_count=7;
    a.QueryWorldCollisions(&contacts);
    Check(contacts.coll_count==0 && w.contacts==0, "world sphere contacts bypassed");
    const vec3d position=a._position;
    a.MaintainIdleGroundSupport(16); a.CorrectPositionOnLand(); a.FixSectorFall();
    bact_arg86 landing{};
    Check(a.CrashOrLand(&landing)==0 && a._position==position && w.traces==0, "no landing, snap or fall damage contact");
    a._disableCollision=false; a.TraceWorldCollision(&ray); a.QueryWorldCollisions(&contacts);
    Check(ray.isect && contacts.coll_count==1 && w.traces==1 && w.contacts==1, "zero restores original world queries");

    auto shape=Cube(); a._collisionShape=shape; b._collisionShape=shape;
    w._collisionScene.reset(new Collision::Scene(w)); Collision::Contact hit;
    Check(w._collisionScene->PairContact(&a,&b,&hit), "ordinary convex shapes contact");
    b._disableCollision=true;
    Check(!w._collisionScene->PairContact(&a,&b,&hit), "convex pair skips disabled target");
    Check(!w._collisionScene->Trace(&b,b._position,b._position,1,&hit), "hitscan cannot hit disabled convex target");
    Check(w._collisionScene->ShapeTargets(b._position,b._position,1).size()==1, "disabled shape omitted from target index");
    Check(!w._collisionScene->PlaceAboveTerrain(&b), "disabled body not snapped above terrain");
    b._disableCollision=false;
    Check(w._collisionScene->Trace(&b,b._position,b._position,1,&hit), "zero restores convex target");
    Actor<NC_STACK_ypamissile> missile; missile.Bind(w,BACT_TYPES_MISSLE); missile._disableCollision=true;
    Check(!missile.TubeCollisionTest(), "disabled projectile skips unit impact path");
    Check(!w._collisionScene->TraceProjectile(&missile,&b,missile._rotation,&hit), "disabled projectile skips convex sweep");
    Actor<NC_STACK_ypatank> tank; tank.Bind(w,BACT_TYPES_TANK); tank._disableCollision=true;
    Check(tank.CollisionWithBact(16)==0, "tank legacy override also bypasses contacts");
    Actor<NC_STACK_ypagun> ghostGun; ghostGun.Bind(w,BACT_TYPES_GUN); ghostGun._disableCollision=true;
    Check(ghostGun.CheckPedestal(), "collision-free gun has no pedestal contact death");
    w._collisionScene.reset();

    World::rbcolls compound; compound.roboColls.resize(2);
    compound.roboColls[0].robo_coll_radius=20;
    compound.roboColls[1].robo_coll_radius=30;
    tank.ApplyCompoundCollision(compound,true);
    tank.GetCollisionSpheres(spheres,tank._position,tank._rotation,true);
    Check(spheres.empty(), "compound and viewer spheres also removed");

    WorldProbe rays; rays.worldHit=false;
    Actor<> shooter; DamageTarget victim;
    shooter.Bind(rays); victim.Bind(rays); victim._owner=2;
    victim._position=shooter._position+vec3d(0,0,-100);
    World::TWeapProto laser; TLaserUnitHit laserHit;
    Check(ypabact_LaserHitscan(&shooter,laser,shooter._position,vec3d(0,0,-1),500,&laserHit)
          && laserHit.target==&victim, "ordinary laser hits legacy body");
    victim._disableCollision=true;
    Check(!ypabact_LaserHitscan(&shooter,laser,shooter._position,vec3d(0,0,-1),500,&laserHit),
          "laser passes through disabled legacy body");
    victim._disableCollision=false; laser.disable_collision=true;
    Check(!ypabact_LaserHitscan(&shooter,laser,shooter._position,vec3d(0,0,-1),500,&laserHit),
          "disabled laser weapon has no direct contact");
    shooter._mgun=-1; shooter._mgun_set=false; shooter._mgun_shot_time=100;
    shooter._mgun_energy_set=true; shooter._mgun_energy=500;
    shooter._mgun_angle_set=false; shooter._mgun_spread_x=shooter._mgun_spread_y=0;
    shooter._mgun_decal_enable=false; shooter._mgun_fire_energy_cost_defined=false;
    shooter._gun_radius=0;
    bact_arg105 shot{}; shot.field_0=vec3d(0,0,-1); shot.field_C=.02f; shot.field_10=1000;
    shooter.FireMinigun(&shot);
    Check(victim.hits==1, "normal MGUN still hits after tracer removal");
    victim._disableCollision=true; shot.field_10=1200; shooter.FireMinigun(&shot);
    Check(victim.hits==1, "MGUN skips disabled target");
    victim._disableCollision=false; shot.field_10=1400; shooter.FireMinigun(&shot);
    Check(victim.hits==2, "zero restores MGUN contact");

    // A mounted Unit Gun never hits its carrier or sibling guns; unrelated units are unaffected.
    Actor<> sibling, stranger;
    sibling.Bind(rays); stranger.Bind(rays);
    shooter._isUnitGunChild = true; shooter._parent = &victim; sibling._isUnitGunChild = true;
    sibling._parent = &victim;
    Check(shooter.IsMountedFamilyWith(&victim) && victim.IsMountedFamilyWith(&shooter),
          "carrier and mounted gun are one family");
    Check(shooter.IsMountedFamilyWith(&sibling), "guns of one carrier are one family");
    Check(!shooter.IsMountedFamilyWith(&stranger) && !shooter.IsMountedFamilyWith(&shooter) &&
          !shooter.IsMountedFamilyWith(nullptr), "unrelated, self and null are not family");
    shot.field_10 = 1600; shooter.FireMinigun(&shot);
    Check(victim.hits==2, "mounted gun MGUN skips its carrier");
    shooter._isUnitGunChild = false; shot.field_10 = 1800; shooter.FireMinigun(&shot);
    Check(victim.hits==3, "unmounted shooter still hits the same unit");

    // unit_gun_no_switch accepts only the literal 1 and defaults to 0.
    Check(Parse(w, {"new_vehicle 3", "model = tank", "unit_num_guns = 3",
                    "unit_act_gun = 0", "unit_gun_no_switch = 1",
                    "unit_act_gun = 1", "unit_gun_no_switch = 2",
                    "unit_act_gun = 2", "end"}), "unit_gun_no_switch parses");
    auto &mother = w._vhclProtos[3];
    Check(mother.unit_guns.size()==3 && mother.unit_guns[0].noSwitch==1 &&
          mother.unit_guns[1].noSwitch==0 && mother.unit_guns[2].noSwitch==0,
          "unit_gun_no_switch literal one enables, invalid and absent stay off");
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
    return failures ? 1 : 0;
}
