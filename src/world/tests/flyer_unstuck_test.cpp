// Focused test of the stuck-flyer escape: production ypaflyer AI_layer3 with a stubbed Move.
#include "game/yw.h"
#include "game/ypabact.h"
#include "game/ypaflyer.h"
#include "nucleus/skeleton.h"
#include "../collision_shape.h"
#include <cstdio>
#include <memory>
#undef main
namespace {
int checks=0, failures=0;
void Check(bool ok,const char *label) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",label); } }
struct TestWorld : NC_STACK_ypaworld {
    bool wallProbe=false;
    UAskeleton::Data *probe=nullptr;
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 1; }
    // A wall 75 units ahead on every horizontal ray, like a facade in front of the nose.
    void ypaworld_func136(ypaworld_arg136 *arg) override {
        arg->isect=wallProbe && std::fabs(arg->vect.y)<1e-8 && arg->vect.XZ().length()>1;
        if(arg->isect) { arg->tVal=.25; arg->skel=probe; arg->polyID=0; }
    }
};
struct Flyer : NC_STACK_ypaflyer {
    void Move(move_msg *) override {}
    void FightWithBact(bact_arg75 *) override {}
    void FightWithSect(bact_arg75 *) override {}
    void Recoil(bact_arg88 *) override {}
    size_t SetPosition(bact_arg80 *arg) override { _position=arg->pos; return 1; }
    void Bind(TestWorld &w,NC_STACK_ypabact *target) {
        _world=&w; _owner=1; _gid=1; _bact_type=BACT_TYPES_FLYER; _status=BACT_STATUS_NORMAL;
        _energy=_energy_max=15000; _radius=40; _position=_old_pos=vec3d(1800,-300,-1800);
        _rotation=mat3x3::Ident(); _weapon=1; _extra_weapons.fill(-1); _oflags=0;
        _status_flg=BACT_STFLAG_ATTACK; _atk_ret=NC_STACK_ypabact::TA_FIGHT;
        _pSector=&w._cells.At(1,1); _cellId=Common::Point(1,1);
        _mass=400; _force=7000; _airconst=_airconst_static=350; _maxrot=.9; _height=300;
        _fly_dir=vec3d::OZ(1); _fly_dir_length=0; _clock=100000;
        _target_vec=vec3d(0,0,1000); // Enemy straight ahead; the real target refresh is outside this probe.
        _primTtype=BACT_TGT_TYPE_UNIT; _primT.pbact=target; _secndTtype=BACT_TGT_TYPE_NONE;
    }
};
Collision::Part Box(double x,double y,double z) {
    Collision::Part p;
    for(const auto &v : {vec3d(-x,-y,-z),vec3d(x,-y,-z),vec3d(x,y,-z),vec3d(-x,y,-z),
                         vec3d(-x,-y,z),vec3d(x,-y,z),vec3d(x,y,z),vec3d(-x,y,z)}) p.vertices.push_back(v);
    p.faces={{{0,2,1}},{{0,3,2}},{{4,5,6}},{{4,6,7}},{{0,1,5}},{{0,5,4}},
             {{3,7,6}},{{3,6,2}},{{0,4,7}},{{0,7,3}},{{1,2,6}},{{1,6,5}}};
    return p;
}
std::shared_ptr<Collision::Shape> MakeShape() {
    auto shape=std::make_shared<Collision::Shape>(); shape->parts.push_back(Box(40,40,40));
    std::string error; Collision::Build(*shape,&error); return shape;
}
// One AI frame of 100 ms.
void Step(Flyer &f,int ms=100) {
    update_msg frame; frame.frameTime=ms; f._clock+=ms; f.AI_layer3(&frame);
}
void RunMs(Flyer &f,int ms) { for(int t=0;t<ms;t+=100) Step(f); }
}
int main() {
    auto w=std::unique_ptr<TestWorld>(new TestWorld);
    w->_mapSize=Common::Point(6,6); w->_cells.Resize(w->_mapSize);
    for(auto &cell:w->_cells) { cell.height=0; cell.owner=2; cell.SectorType=1; cell.type_id=0; cell.AddToViewMask(1); }
    w->_weaponProtos.resize(2);
    NC_STACK_skeleton skeleton; UAskeleton::Data geometry; skeleton._resData=&geometry;
    geometry.POO={vec3d(-600,-600,-600),vec3d(600,-600,-600),vec3d(600,-600,600),vec3d(-600,-600,600)};
    UAskeleton::Polygon wall; wall.num_vertices=4; wall.v[0]=0; wall.v[1]=1; wall.v[2]=2; wall.v[3]=3;
    wall.A=0; wall.B=0; wall.C=-1; geometry.polygons.push_back(wall); w->probe=&geometry;

    Flyer enemy; enemy.Bind(*w,nullptr); enemy._position=vec3d(1800,-300,-800); enemy._primTtype=BACT_TGT_TYPE_NONE;
    auto shape=MakeShape();
    Flyer f; f.Bind(*w,&enemy); f._collisionShape=shape;
    w->wallProbe=true;

    // 1) Before the detection time nothing changes: plain vanilla dodge.
    RunMs(f,2800);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE,"no escape before the detection time");
    Check(f._status_flg&(BACT_STFLAG_DODGE_LEFT|BACT_STFLAG_DODGE_RIGHT),"normal dodge is active while blocked");
    Check(f._thraction>0,"forward thrust before the detection time");

    // 2) Stuck for more than 3 s: reverse, climb.
    RunMs(f,400);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_REVERSE,"reverse phase starts after the timer");
    Check(f._thraction<0,"reverse phase pushes backwards");
    Check(f._flyerBoost>f._mass*9.80665*2.0,"reverse phase climbs hard");
    Check(f._target_dir.y<0,"reverse phase aims upwards");

    // 3) Wide turn: sideways heading, full forward thrust, strong climb.
    RunMs(f,1100);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_TURN,"turn phase follows the reverse phase");
    Check(f._thraction>0,"turn phase thrusts forward again");
    Check(std::fabs(f._target_dir.x)>.5 && f._target_dir.y<-.5,"turn phase heads sideways and up");
    const double firstSide=f._target_dir.x;
    Check(f._flyerBoost>f._mass*9.80665*2.0,"turn phase climbs hard");

    // 4) After the manoeuvre the normal AI controls again.
    RunMs(f,2600);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE,"escape ends and returns to the normal controller");
    w->wallProbe=false; RunMs(f,200);
    Check(!(f._status_flg&(BACT_STFLAG_DODGE_LEFT|BACT_STFLAG_DODGE_RIGHT)) && f._thraction>0 && f._target_dir.z>.9,
          "clear route resumes the straight charge");

    // 5) Stuck again soon after: a longer manoeuvre on the other side.
    // The previous turn rotated the plane; face the wall again before the second incident.
    f._rotation=mat3x3::Ident(); w->wallProbe=true; RunMs(f,3300);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_REVERSE && f._unstuckLevel==2,"repeat within the window escalates the level");
    RunMs(f,1600);
    Check(f._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_TURN && f._target_dir.x*firstSide<0,"escalated turn uses the opposite side");

    // 6) A plane that really moves is never "stuck", even with an obstacle ahead.
    Flyer moving; moving.Bind(*w,&enemy); moving._collisionShape=shape;
    for(int t=0;t<10000;t+=100) { moving._position.x+=60; Step(moving); }
    Check(moving._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE && moving._unstuckLevel==0,"progress resets the timer");

    // 7) Obstacle gone for more than a second resets the timer.
    Flyer gap; gap.Bind(*w,&enemy); gap._collisionShape=shape;
    RunMs(gap,2500); w->wallProbe=false; RunMs(gap,1500); w->wallProbe=true; RunMs(gap,2500);
    Check(gap._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE,"a clear gap restarts the detection time");

    // 8) Without a collision profile the vanilla behaviour is untouched.
    Flyer legacy; legacy.Bind(*w,&enemy);
    RunMs(legacy,12000);
    Check(legacy._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE && legacy._thraction>0,"profile-free flyer never runs the escape");

    // 9) Kamikaze ramming is intentional contact, not a stuck state.
    w->_weaponProtos[1]._weaponFlags=World::TWeapProto::WEAPON_FLAGS_KAMIKAZE; w->_weaponProtos[1].trigger_radius=360;
    Flyer ram; ram.Bind(*w,&enemy); ram._collisionShape=shape;
    RunMs(ram,6000);
    Check(ram._unstuckPhase==NC_STACK_ypaflyer::UNSTUCK_NONE,"kamikaze flyer is excluded");
    w->_weaponProtos[1]._weaponFlags=0;

    std::printf("flyer_unstuck_test: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
