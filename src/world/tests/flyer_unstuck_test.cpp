// Focused test of the stuck-flyer escape: production ypaflyer AI_layer3 with a stubbed Move.
#include "game/yw.h"
#include "game/ypabact.h"
#include "game/ypaflyer.h"
#include "game/ypabact_internal.h"
#include "system/inivals.h"
#include "nucleus/skeleton.h"
#include "../collision_shape.h"
#include <cstdio>
#include <memory>
#undef main
namespace {
int checks=0, failures=0;
bool Near(float a,float b) { return std::fabs(a-b)<1e-3f; }
void Check(bool ok,const char *label) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",label); } }
struct TestWorld : NC_STACK_ypaworld {
    bool wallProbe=false;
    bool hasRoof=false; float roofY=0;
    UAskeleton::Data *probe=nullptr;
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 1; }
    // A wall 75 units ahead on every horizontal ray, like a facade in front of the nose.
    void ypaworld_func136(ypaworld_arg136 *arg) override {
        // A vertical ray from far above meets the roof of the building under it.
        if(hasRoof && std::fabs(arg->vect.x)<1e-8 && std::fabs(arg->vect.z)<1e-8 && arg->vect.y>10000) {
            arg->isect=true; arg->tVal=.5; arg->isectPos=vec3d(arg->stPos.x,roofY,arg->stPos.z); arg->skel=probe; arg->polyID=0;
            return;
        }
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
// Sets the two optional AI aircraft keys of OpenNeoUA.ini; "0" means absent.
void SetAiAirIni(const char *force, const char *bomber) {
    System::IniConf::GameAiAirVerticalForce.Value=std::string(force);
    System::IniConf::GameAiBomberExtraAltitude.Value=std::string(bomber);
    ypabact_ReloadAiAirConfig();
}
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

    // 10) Force 1 or absent is exactly vanilla: climb 2.5x weight, descent 0.7x, no extra vertical pursuit.
    Flyer lift; lift.Bind(*w,&enemy);
    const double weight=lift._mass*9.80665;
    for(const char *force : {"0","1.0"}) {
        SetAiAirIni(force,"0");
        lift._target_dir=vec3d(0,1,0); lift.ypaflyer_func70__sub0(.01f);
        Check(std::fabs(lift._flyerBoost-weight*.7)<.05,"force 1 or absent: the vanilla descent lift");
        lift._target_dir=vec3d(0,.3,.95); lift.ypaflyer_func70__sub0(.01f);
        Check(std::fabs(lift._flyerBoost-weight*.7)<.05,"force 1 or absent: a mild descent has the same vanilla lift");
        lift._target_dir=vec3d(0,-.5,.87); lift.ypaflyer_func70__sub0(.01f);
        Check(std::fabs(lift._flyerBoost-weight*2.5)<.05,"force 1 or absent: the vanilla climb boost");
        lift._target_dir=vec3d(0,0,1); lift.ypaflyer_func70__sub0(.01f);
        Check(std::fabs(lift._flyerBoost-weight)<.05,"force 1 or absent: level flight lift is the weight");
        Check(!lift.HasVerticalPursuitTarget(),"force 1 or absent: an airborne target gives no vertical pursuit");
    }

    // 11) Above 1 the lift moves away from level flight in both directions, which stays at the weight.
    SetAiAirIni("1.5","0");
    lift._target_dir=vec3d(0,-.5,.87); lift.ypaflyer_func70__sub0(.01f);
    Check(std::fabs(lift._flyerBoost-weight*3.25)<.05,"force 1.5: the climb boost is stronger than vanilla");
    lift._target_dir=vec3d(0,.5,.87); lift.ypaflyer_func70__sub0(.01f);
    Check(std::fabs(lift._flyerBoost-weight*.55)<.05,"force 1.5: the descent lift is lower than vanilla");
    lift._target_dir=vec3d(0,0,1); lift.ypaflyer_func70__sub0(.01f);
    Check(std::fabs(lift._flyerBoost-weight)<.05,"force 1.5: level flight keeps the weight as lift");
    lift._target_dir=vec3d(0,-.5,.87); lift._status_flg|=BACT_STFLAG_UPWRD; lift.ypaflyer_func70__sub0(.01f);
    Check(std::fabs(lift._flyerBoost-weight*3.25)<.05,"force 1.5: the obstacle climb uses the same boost");
    lift._status_flg&=~BACT_STFLAG_UPWRD;
    SetAiAirIni("5","0");
    lift._target_dir=vec3d(0,.5,.87); lift.ypaflyer_func70__sub0(.01f);
    Check(lift._flyerBoost>=0 && lift._flyerBoost<.05,"a very high force never makes the descent lift negative");
    SetAiAirIni("0.5","0");
    lift._target_dir=vec3d(0,-.5,.87); lift.ypaflyer_func70__sub0(.01f);
    Check(std::fabs(lift._flyerBoost-weight*1.75)<.05,"force below 1: a weaker climb");
    SetAiAirIni("1.5","0");
    Check(lift.HasVerticalPursuitTarget(),"force above 1: an airborne unit target allows vertical pursuit");
    Flyer idle; idle.Bind(*w,nullptr);
    Check(!idle.HasVerticalPursuitTarget(),"missing unit target keeps the cruise altitude rule");

    // 12) Bomber altitude: only AI aircraft with a bomb-like weapon, against aircraft and buildings.
    w->_weaponProtos[1]._weaponFlags=World::TWeapProto::WEAPON_FLAGS_BOMB;
    Flyer bomber; bomber.Bind(*w,&enemy);
    bomber._position=bomber._old_pos=vec3d(1800,-300,-1800); enemy._position=vec3d(1800,-300,-1200);
    SetAiAirIni("0","0");
    Check(!bomber.GetBomberAltitudeAim(),"absent extra altitude: the bomber keeps its normal altitude");
    SetAiAirIni("0","400");
    vec3d aim;
    Check(bomber.GetBomberAltitudeAim(&aim) && Near(aim.y,-300),"bomber follows an airborne unit target");
    bomber._target_dir=vec3d(0,0,1); bomber.ApplyBomberAltitude();
    Check(bomber._target_dir.y<-.2 && bomber._target_dir.z>0,"bomber at its target altitude climbs toward the point 400 above it");
    bomber._position=bomber._old_pos=vec3d(1800,-700,-1800);
    bomber._target_dir=vec3d(0,0,1); bomber.ApplyBomberAltitude();
    Check(std::fabs(bomber._target_dir.y)<.01,"bomber exactly 400 above its target flies level");
    bomber._position=bomber._old_pos=vec3d(1800,-1100,-1800);
    bomber._target_dir=vec3d(0,0,1); bomber.ApplyBomberAltitude();
    Check(bomber._target_dir.y>.2,"bomber too high descends toward the point 400 above its target");
    Check(bomber.HasVerticalPursuitTarget(),"an active bomber is allowed to leave the cruise altitude");
    bomber._position=bomber._old_pos=vec3d(1800,-300,-1800);
    enemy._bact_type=BACT_TYPES_TANK;
    Check(!bomber.GetBomberAltitudeAim(),"a ground unit target never changes the bomber altitude");
    enemy._bact_type=BACT_TYPES_FLYER;
    bomber._secndTtype=BACT_TGT_TYPE_CELL; bomber._sencdTpos=vec3d(1800,0,-1200);
    bomber._target_dir=vec3d(0,0,1);
    Check(bomber.GetBomberAltitudeAim(&aim) && Near(aim.y,0),"bomber aims above a building target");
    bomber.ApplyBomberAltitude();
    Check(bomber._target_dir.y<0,"bomber 300 above ground climbs toward 400 above the building");
    // The building target is its roof: roof at altitude 700 with extra 100 aims at altitude 800.
    SetAiAirIni("0","100");
    w->hasRoof=true; w->roofY=-700;
    Check(bomber.GetBomberAltitudeAim(&aim) && Near(aim.y,-700),"bomber aims at the roof of a building target");
    bomber._position=bomber._old_pos=vec3d(1800,-300,-1800); bomber._target_dir=vec3d(0,0,1); bomber.ApplyBomberAltitude();
    Check(bomber._target_dir.y<-.3,"bomber below the roof climbs toward roof + extra");
    bomber._position=bomber._old_pos=vec3d(1800,-800,-1800); bomber._target_dir=vec3d(0,0,1); bomber.ApplyBomberAltitude();
    Check(std::fabs(bomber._target_dir.y)<.01,"bomber at altitude 800 over a roof at 700 flies level");
    w->hasRoof=false;
    // The safety distance above the surface shrinks to the extra altitude only while closing on the target.
    Check(std::fabs(bomber.GetAiGroundClearance()-100)<.01,"bomber closing on its target keeps only the extra altitude above the surface");
    bomber._secndTtype=BACT_TGT_TYPE_NONE; bomber._primTtype=BACT_TGT_TYPE_NONE;
    Check(std::fabs(bomber.GetAiGroundClearance()-bomber._height)<.01,"without a bomber target the vanilla clearance stays");
    bomber._primTtype=BACT_TGT_TYPE_UNIT;
    SetAiAirIni("0","0");
    bomber._secndTtype=BACT_TGT_TYPE_CELL;
    Check(std::fabs(bomber.GetAiGroundClearance()-bomber._height)<.01,"absent extra altitude: the vanilla clearance");
    SetAiAirIni("0","400");
    Check(std::fabs(bomber.GetAiGroundClearance()-bomber._height)<.01,"an extra altitude above _height never lowers the clearance");
    SetAiAirIni("0","400");
    bomber._secndTtype=BACT_TGT_TYPE_NONE;
    bomber._bact_type=BACT_TYPES_TANK;
    Check(!bomber.GetBomberAltitudeAim(),"ground vehicles never use the bomber altitude");
    bomber._bact_type=BACT_TYPES_FLYER;
    w->_weaponProtos[1]._weaponFlags=0;
    Check(!bomber.GetBomberAltitudeAim(),"aircraft without a bomb-like weapon keep their altitude");
    SetAiAirIni("0","0");

    std::printf("flyer_unstuck_test: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
