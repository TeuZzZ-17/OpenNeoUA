// Production geometry, parser and actor probes. Run without starting a game.
#include "../collision_shape.h"
#include "../parsers.h"
#include "../../yw.h"
#include "../../ypamissile.h"
#include "../../ypatank.h"
#include "../../ypacar.h"
#include "../../ypaflyer.h"
#include "../../ypaufo.h"
#include "../../ypagun.h"
#include "../../yparobo.h"
#include "../../base.h"
#include "../../skeleton.h"
#include "../../system/fsmgr.h"
#include "../../system/inivals.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <random>
#undef main

namespace {
int checks=0, failures=0;
void Check(bool ok,const char *name) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",name); } }
Collision::Part Box(const vec3d &c,double x,double y,double z) {
    Collision::Part p;
    for(auto v : {vec3d(-x,-y,-z),vec3d(x,-y,-z),vec3d(x,y,-z),vec3d(-x,y,-z),
                  vec3d(-x,-y,z),vec3d(x,-y,z),vec3d(x,y,z),vec3d(-x,y,z)}) p.vertices.push_back(c+v);
    p.faces={{{0,2,1}},{{0,3,2}},{{4,5,6}},{{4,6,7}},{{0,1,5}},{{0,5,4}},
             {{3,7,6}},{{3,6,2}},{{0,4,7}},{{0,7,3}},{{1,2,6}},{{1,6,5}}};
    return p;
}
std::shared_ptr<Collision::Shape> Shape(std::initializer_list<Collision::Part> parts) {
    auto s=std::make_shared<Collision::Shape>(); s->parts=parts;
    std::string error; Check(Collision::Build(*s,&error),error.c_str()); return s;
}
struct TestWorld : NC_STACK_ypaworld {
    double wallFraction=-1;
    size_t world145=0;
    size_t GetSectorInfo(yw_130arg *arg) override { arg->pcell=&_cells.At(0); arg->CellId=Common::Point(0,0); return 1; }
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return world145; }
    void ypaworld_func149(ypaworld_arg136 *arg) override { arg->isect=wallFraction>=0; arg->tVal=wallFraction; }
    void ypaworld_func180(yw_arg180 *) override {} // No renderer palette in the native physics fixture.
};
template<class Base=NC_STACK_ypabact> struct Actor : Base {
    void Bind(TestWorld &w,int gid,vec3d pos,std::shared_ptr<Collision::Shape> shape={}) {
        this->_world=&w; this->_gid=gid; this->_owner=1; this->_bact_type=BACT_TYPES_BACT;
        this->_status=BACT_STATUS_NORMAL; this->_oflags=BACT_OFLAG_BACTCOLL;
        this->_rotation=mat3x3::Ident(); this->_position=pos; this->_old_pos=pos;
        this->_radius=2; this->_collisionShape=shape;
        this->_soundcarrier.Resize(7);
        this->_cellRef=w._cells.At(0).unitsList.push_back(this);
    }
};
struct Missile : Actor<NC_STACK_ypamissile> {
    void Emitter(NC_STACK_ypabact *actor) { _mislEmitter=actor; _mislRadiusHeli=1000; }
};
struct Robo : Actor<NC_STACK_yparobo> {
    int impactDamage=0;
    using NC_STACK_yparobo::wallow;
    using NC_STACK_yparobo::checkCollisions;
    void ChangeSectorEnergy(yw_arg129 *arg) override { impactDamage=arg->field_10; }
};
template<class Base> void ProbeMove(std::shared_ptr<Collision::Shape> cube,const char *name,int type=BACT_TYPES_BACT) {
    TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
    w._collisionScene.reset(new Collision::Scene(w));
    const vec3d centre(2400,0,-2400);
    Actor<> obstacle; obstacle.Bind(w,1,centre,cube);
    Actor<Base> mover; mover.Bind(w,2,centre+vec3d(-100,0,0),cube);
    mover._bact_type=type;
    mover._wrldSize=vec2d(9600,-9600); mover._mass=1;
    mover._force=100; mover._airconst=0; mover._airconst_static=1;
    mover._soundcarrier.Resize(7);
    mover._thraction=0; mover._fly_dir=vec3d(1,0,0); mover._fly_dir_length=2000;
    move_msg step; step.flag=0; step.field_0=1./60;
    mover.Move(&step);
    Check(mover._position.x < centre.x-19.7 && mover._position.x > centre.x-25 &&
          fabs((mover._fly_dir*mover._fly_dir_length).x)<.01,name);
    w._collisionScene.reset();
}
void Write(const Collision::Shape &s,const char *path) {
    std::ofstream f(path); f.precision(17);
    f<<"begin_collision_shape\nversion = 1\nscale = 1_1_1\nrotation = 0_0_0\n";
    for(auto &p:s.parts) {
        f<<"begin_hull\n";
        for(auto &v:p.vertices) f<<"vertex = "<<v.x<<'_'<<v.y<<'_'<<v.z<<'\n';
        for(auto &t:p.faces) f<<"face = "<<t[0]<<'_'<<t[1]<<'_'<<t[2]<<'\n';
        f<<"end\n";
    }
    f<<"end\n";
}
struct GroundWorld : TestWorld {
    UAskeleton::Data floorData;
    NC_STACK_skeleton floor;
    UAskeleton::Data sideData, crossData;
    NC_STACK_skeleton side, cross;
    TSubSectorDesc subsection;
    GroundWorld() {
        _mapSize=Common::Point(8,8); _cells.Resize(_mapSize);
        for(auto &sector:_secTypeArray)
            for(int x=0;x<3;++x) for(int y=0;y<3;++y) sector.SubSectors.At(x,y)=&subsection;
        floorData.POO={vec3d(-150,0,-150),vec3d(-150,0,150),vec3d(150,0,150),vec3d(150,0,-150)};
        UAskeleton::Polygon face; face.num_vertices=4;
        face.v[0]=0; face.v[1]=1; face.v[2]=2; face.v[3]=3; face.B=1;
        floorData.polygons.push_back(face); floor._resData=&floorData;
        _legoArray[0].CollisionSkelet=&floor; _legoArray[0].UseCollisionSkelet=&floor;
        sideData=floorData; side._resData=&sideData; _fillerSide=&side;
        crossData.POO=floorData.POO; crossData.POO.push_back(vec3d(0,0,0));
        for(int i=0;i<4;++i) {
            UAskeleton::Polygon triangle; triangle.num_vertices=3;
            triangle.v[0]=i; triangle.v[1]=(i+1)%4; triangle.v[2]=4;
            crossData.polygons.push_back(triangle);
        }
        cross._resData=&crossData; _fillerCross=&cross;
        _collisionScene.reset(new Collision::Scene(*this));
    }
    ~GroundWorld() { _collisionScene.reset(); _legoArray[0].CollisionSkelet=nullptr; _legoArray[0].UseCollisionSkelet=nullptr; _fillerSide=nullptr; _fillerCross=nullptr; }
    void ypaworld_func136(ypaworld_arg136 *arg) override {
        arg->isect=false;
        if(fabs(arg->vect.y)<1e-9) return;
        const double t=-arg->stPos.y/arg->vect.y;
        if(t<0 || t>1) return;
        arg->isect=true; arg->tVal=t; arg->isectPos=arg->stPos+arg->vect*t;
        arg->skel=&floorData; arg->polyID=0;
    }
};
struct BuildingWorld : GroundWorld {
    UAskeleton::Data buildingData;
    NC_STACK_skeleton building;
    TSubSectorDesc buildingSubsection;
    std::shared_ptr<Collision::Shape> solid;
    BuildingWorld(bool sloped=false) {
        _collisionScene.reset();
        for(auto &cell:_cells) cell.SectorType=1;
        floorData.POO={vec3d(-600,0,-600),vec3d(-600,0,600),vec3d(600,0,600),vec3d(600,0,-600)};
        buildingData=floorData;
        Collision::Part structure;
        if(sloped) {
            structure.vertices={vec3d(-180,0,-180),vec3d(-180,0,180),vec3d(180,0,180),
                                vec3d(180,0,-180),vec3d(0,-360,0)};
            structure.faces={{{0,1,4}},{{1,2,4}},{{2,3,4}},{{3,0,4}},{{0,2,1}},{{0,3,2}}};
        } else {
            structure=Box(vec3d(0,-90,0),140,90,140);
            for(auto &face:structure.faces) std::swap(face[1],face[2]);
        }
        solid=Shape({structure});
        buildingData.POO.insert(buildingData.POO.end(),structure.vertices.begin(),structure.vertices.end());
        for(const auto &face:structure.faces) {
            UAskeleton::Polygon polygon; polygon.num_vertices=3;
            for(int i=0;i<3;++i) polygon.v[i]=face[i]+4;
            const auto &a=buildingData.POO[polygon.v[0]];
            vec3d normal=(buildingData.POO[polygon.v[1]]-a)*(buildingData.POO[polygon.v[2]]-a);
            normal.normalise(); polygon.A=normal.x; polygon.B=normal.y; polygon.C=normal.z;
            polygon.D=-normal.dot(a); buildingData.polygons.push_back(polygon);
        }
        building._resData=&buildingData;
        _legoArray[1].CollisionSkelet=&building; _legoArray[1].UseCollisionSkelet=&building;
        buildingSubsection.HPModels.fill(1);
        for(int x=0;x<3;++x) for(int y=0;y<3;++y)
            _secTypeArray[1].SubSectors.At(x,y)=&buildingSubsection;
        _cells(2,2).type_id=1;
        _collisionScene.reset(new Collision::Scene(*this));
    }
    ~BuildingWorld() {
        _collisionScene.reset(); _legoArray[1].CollisionSkelet=nullptr; _legoArray[1].UseCollisionSkelet=nullptr;
    }
    void ypaworld_func136(ypaworld_arg136 *arg) override { NC_STACK_ypaworld::ypaworld_func136(arg); }
};
void GroundRegression(std::shared_ptr<Collision::Shape> tiger) {
    GroundWorld w;
    Actor<NC_STACK_ypatank> shaped,legacy;
    shaped.Bind(w,80,vec3d(2400,-13,-2400),tiger);
    legacy.Bind(w,81,vec3d(2400,-13,-3600));
    for(auto *a:{&shaped,&legacy}) {
        a->_bact_type=BACT_TYPES_TANK; a->_oflags=BACT_OFLAG_EXACTCOLL;
        a->_status_flg=BACT_STFLAG_LAND; a->_overeof=13;
        a->_wrldSize=vec2d(9600,-9600); a->_mass=1; a->_force=100;
        a->_airconst=0; a->_airconst_static=1; a->_soundcarrier.Resize(7);
        a->_fly_dir_length=20; a->_rotation=mat3x3::RotateX(.10);
        a->_fly_dir=a->_rotation.AxisZ();
    }
    const vec3d shapedStart=shaped._position,legacyStart=legacy._position;
    for(int frame=0;frame<20;++frame) {
        w._timeStamp+=20;
        for(auto *a:{&shaped,&legacy}) {
            const vec3d start=a->_position; const mat3x3 rotation=a->_rotation;
            a->ResolveShapeMovement(start,rotation);
            move_msg step; step.flag=0; step.field_0=.02;
            a->Move(&step);
            a->AlignVehicleAI(0,nullptr);
            a->ResolveShapeMovement(start,rotation,20);
        }
    }
    const double shapedTravel=(shaped._position-shapedStart).XZ().length();
    const double legacyTravel=(legacy._position-legacyStart).XZ().length();
    std::printf("GROUND shaped=%g legacy=%g y_shape=%g y_legacy=%g\n",shapedTravel,legacyTravel,shaped._position.y,legacy._position.y);
    Check(fabs(shapedTravel-legacyTravel)<.05,"grounded tank Move/support cycle retains vanilla progress on flat LEGO ground");
    Check(fabs(shaped._position.y-legacy._position.y)<.05,"grounded tank contact does not undo controller height/tip");
    shaped._position=vec3d(2400,-13,-2400); shaped._rotation=mat3x3::Ident();
    legacy._position=shaped._position+vec3d(15,0,0); legacy._collisionShape=tiger;
    shaped._oflags=legacy._oflags=BACT_OFLAG_BACTCOLL;
    shaped._fly_dir=vec3d(0,0,1); shaped._fly_dir_length=-20;
    w._timeStamp+=20;
    const double groundY=shaped._position.y;
    w._collisionScene->Resolve(&shaped,shaped._position,shaped._rotation,20);
    Collision::Contact contact;
    std::printf("GROUND_PAIR dy=%g signed_speed=%g dir_z=%g\n",shaped._position.y-groundY,shaped._fly_dir_length,shaped._fly_dir.z);
    Check(fabs(shaped._position.y-groundY)<.001,"overlapping grounded tanks separate in XZ rather than being lifted off support");
    Check(!w._collisionScene->PairContact(&shaped,&legacy,&contact) || contact.depth<=tiger->tolerance+.001,
          "grounded tank pair leaves no deep overlap after correction");
    Check(shaped._fly_dir_length<=0 && shaped._fly_dir.z>.99,
          "ground collision preserves the signed tank reverse speed and drive axis");
    for(double angle:{0.,.35,.8,1.57,2.4}) {
        shaped._position=vec3d(2400,-13,-2400); shaped._rotation=mat3x3::Ident();
        legacy._position=shaped._position+vec3d(15,0,5);
        legacy._rotation=mat3x3::RotateY(angle)*mat3x3::RotateX(.1);
        w._timeStamp+=20;
        w._collisionScene->Resolve(&shaped,shaped._position,shaped._rotation,0);
        Check(fabs(shaped._position.y-groundY)<.001,"rotated grounded pair keeps controller support height");
        Check(!w._collisionScene->PairContact(&shaped,&legacy,&contact) || contact.depth<=tiger->tolerance+.002,
              "rotated real Tiger hulls separate without persistent penetration");
    }
    legacy._collisionShape.reset(); legacy._radius=30;
    shaped._position=vec3d(2400,-13,-2400); legacy._position=shaped._position+vec3d(15,0,0);
    w._timeStamp+=20;
    w._collisionScene->Resolve(&shaped,shaped._position,shaped._rotation,0);
    Check(fabs(shaped._position.y-groundY)<.001 &&
          (!w._collisionScene->PairContact(&shaped,&legacy,&contact) || contact.depth<=tiger->tolerance+.002),
          "mixed shape/sphere ground contact separates horizontally");
    w._collisionScene.reset();
}
struct AITank : Actor<NC_STACK_ypatank> {
    // Keep the production movement controller while suppressing only combat
    // side effects that would require a populated gameplay sector.
    void FightWithSect(bact_arg75 *) override {}
    uint8_t CollisionFlags() const { return _tankCollisionFlag; }
    float CollisionAngle() const { return _tankCollisionAngle; }
};
void PrepareAITank(AITank &tank, TestWorld &w, int gid, const vec3d &position,
                  const vec3d &target, const std::shared_ptr<Collision::Shape> &shape) {
    tank.Bind(w,gid,position,shape);
    tank._bact_type=BACT_TYPES_TANK;
    tank._oflags=BACT_OFLAG_EXACTCOLL|BACT_OFLAG_BACTCOLL;
    tank._status=BACT_STATUS_NORMAL;
    tank._status_flg=BACT_STFLAG_LAND;
    tank._overeof=13;
    tank._wrldSize=vec2d(9600,-9600);
    tank._mass=10;
    tank._force=100;
    tank._airconst=2;
    tank._airconst_static=2;
    tank._maxrot=1;
    tank._radius=shape ? shape->radius : 0;
    tank._soundcarrier.Resize(7);
    tank._vp_normal=tank._vp_fire=tank._vp_wait=tank._vp_dead=tank._vp_megadeth=tank._vp_genesis=nullptr;
    tank._rotation=mat3x3::Ident();
    tank._fly_dir=tank._rotation.AxisZ();
    tank._fly_dir_length=20;
    tank._thraction=0;
    tank._primTtype=BACT_TGT_TYPE_CELL;
    tank._primTpos=target;
    tank._primT.pcell=nullptr;
    tank._target_vec=target-position;
}
bool StepAITank(AITank &tank, TestWorld &w, int frameTime=20,bool advanceWorld=true) {
    const vec3d oldPosition=tank._position;
    const mat3x3 oldRotation=tank._rotation;
    if(advanceWorld) w._timeStamp+=frameTime;
    tank._clock+=frameTime;
    tank.ResolveShapeMovement(oldPosition,oldRotation);
    const bool initialHeightPreserved=fabs(tank._position.y-oldPosition.y)<.01;
    update_msg update;
    update.gTime=w._timeStamp;
    update.frameTime=frameTime;
    tank.AI_layer3(&update);
    const double supportHeight=tank._position.y;
    tank.ResolveShapeMovement(oldPosition,oldRotation,frameTime);
    return initialHeightPreserved && fabs(tank._position.y-supportHeight)<.01;
}
void SpawnLandingRegression(std::shared_ptr<Collision::Shape> tiger) {
    GroundWorld w;
    std::vector<std::unique_ptr<AITank>> tanks;
    for (int i=0;i<5;++i) {
        tanks.emplace_back(new AITank);
        const vec3d start(2100+i*180,-160,-2400);
        PrepareAITank(*tanks.back(),w,400+i,start,start+vec3d(0,0,900),tiger);
        auto &tank=*tanks.back();
        tank._status_flg=0;
        tank._energy=tank._energy_max=100000;
        tank._fly_dir=vec3d(.1,1,0); tank._fly_dir.normalise();
        tank._fly_dir_length=10;
    }
    for(int frame=0;frame<300;++frame) {
        w._timeStamp+=20;
        for(auto &tank:tanks) {
            tank->_target_vec=tank->_primTpos-tank->_position;
            StepAITank(*tank,w,20,false);
        }
    }
    for(auto &tank:tanks) {
        std::printf("SPAWN gid=%d land=%d y=%g speed=%g dz=%g\n",tank->_gid,
                    !!(tank->_status_flg&BACT_STFLAG_LAND),tank->_position.y,
                    tank->_fly_dir_length,tank->_position.z+2400);
        Check(tank->_status_flg&BACT_STFLAG_LAND,"spawned Tiger reaches production LAND state without another unit pushing it");
        Check(tank->_position.z>-2200,"spawned Tiger follows movement order after landing");
    }
    for(auto &tank:tanks) { w._collisionScene->Forget(tank.get()); tank->_cellRef.Detach(); }
    tanks.clear();
    for(int i=0;i<5;++i) {
        tanks.emplace_back(new AITank);
        const vec3d start(2100+i*180,-160,-2400);
        PrepareAITank(*tanks.back(),w,440+i,start,start,tiger);
        auto &tank=*tanks.back(); tank._status_flg=0; tank._primTtype=BACT_TGT_TYPE_NONE;
        tank._energy=tank._energy_max=100000;
        tank._fly_dir=vec3d(.1,1,0); tank._fly_dir.normalise(); tank._fly_dir_length=10;
    }
    std::vector<vec3d> restingPositions;
    for(int frame=0;frame<300;++frame) {
        w._timeStamp+=20;
        for(auto &tank:tanks) StepAITank(*tank,w,20,false);
        if(frame==200) for(auto &tank:tanks) restingPositions.push_back(tank->_position);
    }
    for(size_t i=0;i<tanks.size();++i) {
        const auto &tank=*tanks[i];
        const double drift=(tank._position-restingPositions[i]).length();
        std::printf("SPAWN_IDLE gid=%d land=%d drift_last_2s=%.6f\n",tank._gid,
                    !!(tank._status_flg&BACT_STFLAG_LAND),drift);
        Check(tank._status_flg&BACT_STFLAG_LAND,"uncommanded spawned Tiger lands through the production controller");
        Check(drift<.01,"uncommanded spawned Tiger stops instead of skating indefinitely");
    }
    w._collisionScene.reset();
}
void AIWorldBuildingRegression(std::shared_ptr<Collision::Shape> tiger) {
    for(bool sloped:{false,true}) for(int dt:{20,100}) {
        BuildingWorld world(sloped);
        AITank tank;
        const vec3d start(3000,-13,-3400);
        PrepareAITank(tank,world,410,start,vec3d(3000,-13,-2100),tiger);
        const vec3d target=tank._primTpos;
        bool supported=true,outside=true; double maxDepth=0,maxStep=0;
        for(int frame=0;frame<12000/dt;++frame) {
            const vec3d previous=tank._position;
            tank._target_vec=target-tank._position;
            supported &= StepAITank(tank,world,dt);
            maxStep=std::max(maxStep,(tank._position-previous).length());
            Collision::Contact hit;
            if(Collision::ContactShapes(*tiger,tank.GetBodyPosition(),tank._rotation,
                                        *world.solid,vec3d(3000,0,-3000),mat3x3::Ident(),&hit)) {
                maxDepth=std::max(maxDepth,hit.depth);
                outside &= hit.depth<=tiger->tolerance+.01;
            }
            if(tank._position.z>-2660) break;
        }
        std::printf("AI_BUILDING slope=%d dt=%d progress=%.3f lateral=%.3f depth=%.5f max_step=%.3f flags=%u angle=%.3f\n",
            sloped,dt,tank._position.z-start.z,fabs(tank._position.x-start.x),maxDepth,maxStep,
            static_cast<unsigned>(tank.CollisionFlags()),tank.CollisionAngle());
        Check(tank._position.z>-2660,"production Tiger AI bypasses a LEGO building without another unit pushing it");
        Check(supported && outside,"LEGO building bypass preserves support and keeps the Tiger hull outside");
        Check(maxStep<45,"building avoidance has no large discontinuous relocation");
    }
}
void AirWorldBuildingRegression(const std::shared_ptr<Collision::Shape> &profile) {
    for(const vec3d direction:{vec3d(1,0,0),vec3d(-1,0,0),vec3d(0,0,1),vec3d(0,0,-1)})
        for(double angle:{0.,.6,1.4}) {
            BuildingWorld world;
            Actor<> flying;
            const vec3d start=vec3d(3000,-80,-3000)-direction*400;
            const mat3x3 rotation=mat3x3::RotateY(angle);
            flying.Bind(world,420,start,profile); flying._oflags=BACT_OFLAG_EXACTCOLL;
            flying._rotation=rotation; flying._fly_dir=direction; flying._fly_dir_length=40;
            world._timeStamp+=20;
            flying._position=start+direction*700;
            flying.ResolveShapeMovement(start,rotation,20);
            Collision::Contact hit;
            Check(!Collision::ContactShapes(*profile,flying.GetBodyPosition(),flying._rotation,
                *world.solid,vec3d(3000,0,-3000),mat3x3::Ident(),&hit) || hit.depth<=profile->tolerance+.01,
                "real airborne model stops outside each building wall");
            const vec3d reflected=flying._fly_dir*flying._fly_dir_length;
            Check(reflected.dot(direction)<-19.9 && fabs(reflected.length()-20)<.01,
                "real airborne model uses the vanilla bounce direction and speed attenuation");
            const vec3d touching=flying._position;
            world._timeStamp+=20;
            flying._position-=direction*15;
            flying.ResolveShapeMovement(touching,flying._rotation,20);
            Check((flying._position-(touching-direction*15)).length()<.01,
                "airborne model moves away from building contact without sticking");
        }
}
void CompoundWorldSweepRegression() {
    vec3d accepted;
    for(bool reverseOrder:{false,true}) {
        BuildingWorld world;
        auto profile=Shape({Box(vec3d(0,0,-40),10,10,10),Box(vec3d(0,0,40),10,10,10)});
        if(reverseOrder) {
            std::reverse(profile->parts.begin(),profile->parts.end());
            Check(Collision::Build(*profile),"reordered compound remains valid");
        }
        const vec3d start(3000,-80,-3500);
        Actor<> body; body.Bind(world,430,start,profile); body._oflags=BACT_OFLAG_EXACTCOLL;
        body._fly_dir=vec3d(0,0,1); body._fly_dir_length=40;
        world._timeStamp+=20; body._position.z+=700;
        body.ResolveShapeMovement(start,mat3x3::Ident(),20);
        Check(body._position.z>start.z+250 && body._position.z+50<=-3140+.01,
              "earliest world collision includes a leading hull even when it is listed last");
        if(reverseOrder)
            Check((body._position-accepted).length()<.01,"world sweep result does not depend on compound hull order");
        else accepted=body._position;
    }
}
void UnitResponseRegression(const std::shared_ptr<Collision::Shape> &cube) {
    const vec3d centre(2400,-80,-2400);
    for(bool shaped:{false,true}) {
        TestWorld world; world._mapSize=Common::Point(8,8); world._cells.Resize(world._mapSize);
        world._collisionScene.reset(new Collision::Scene(world));
        Actor<> moving, stopped;
        moving.Bind(world,610,centre+vec3d(-30,0,0),shaped?cube:nullptr);
        stopped.Bind(world,611,centre,shaped?cube:nullptr);
        moving._radius=stopped._radius=10;
        moving._pSector=stopped._pSector=&world._cells.At(0);
        moving._fly_dir=vec3d(1,0,0); moving._fly_dir_length=40;
        world._timeStamp+=20;
        const vec3d old=moving._position;
        moving._position=centre+vec3d(-15,0,0);
        if(shaped) moving.ResolveShapeMovement(old,moving._rotation);
        const bool response=moving.CollisionWithBact(20)!=0;
        const vec3d velocity=moving._fly_dir*moving._fly_dir_length;
        std::printf("UNIT_RESPONSE shape=%d response=%d bcrash=%d vx=%.4f\n",shaped,response,
            !!(moving._status_flg&BACT_STFLAG_BCRASH),velocity.x);
        Check(response,"unit impact reaches the existing gameplay collision response");
        Check(moving._status_flg&BACT_STFLAG_BCRASH,"unit impact activates vanilla crashvhcl event");
        Check(fabs(velocity.x+20)<.01,"unit impact matches vanilla reflected speed and direction");
        if(shaped) {
            const auto reflected=velocity;
            moving.ResolveShapeMovement(moving._position,moving._rotation,20);
            Check((moving._fly_dir*moving._fly_dir_length-reflected).length()<.01,
                "shared update does not apply the same unit rebound twice");
        }
        world._collisionScene.reset();
    }
}
void AirLandingRegression(const std::shared_ptr<Collision::Shape> &profile) {
    GroundWorld world;
    std::vector<std::unique_ptr<Actor<>>> actors;
    for(int i=0;i<5;++i) {
        actors.emplace_back(new Actor<>);
        auto &a=*actors.back();
        a.Bind(world,700+i,vec3d(2800+(i%3)*65,-250-i*35,-2800-(i/3)*65),profile);
        a._pSector=&world._cells.At(0);
        a._oflags=BACT_OFLAG_EXACTCOLL|BACT_OFLAG_BACTCOLL|BACT_OFLAG_LANDONWAIT;
        a._wrldSize=vec2d(9600,-9600); a._mass=1000; a._force=10000;
        a._airconst_static=500; a._maxrot=1; a._overeof=40;
        a._energy=a._energy_max=100000;
        a._vp_normal=a._vp_fire=a._vp_wait=a._vp_dead=a._vp_megadeth=a._vp_genesis=nullptr;
        a._fly_dir=vec3d(0,1,0); a._fly_dir_length=20;
    }
    int rebounds=0; double maximumStep=0,drift=0,depth=0;
    std::vector<vec3d> resting;
    for(int frame=0;frame<600;++frame) {
        world._timeStamp+=20;
        for(auto &a:actors) {
            const vec3d start=a->_position; const mat3x3 rotation=a->_rotation;
            a->ResolveShapeMovement(start,rotation);
            bact_arg86 landing; landing.field_one=0; landing.field_two=20;
            a->CrashOrLand(&landing);
            a->ResolveShapeMovement(start,rotation,20);
            if(a->_status_flg&BACT_STFLAG_BCRASH) ++rebounds;
            maximumStep=std::max(maximumStep,(a->_position-start).length());
        }
        if(frame==500) for(auto &a:actors) resting.push_back(a->_position);
    }
    int landed=0;
    for(size_t i=0;i<actors.size();++i) {
        if(actors[i]->_status_flg&BACT_STFLAG_LAND) ++landed;
        drift=std::max(drift,(actors[i]->_position-resting[i]).length());
        for(size_t j=i+1;j<actors.size();++j) {
            Collision::Contact hit;
            if(world._collisionScene->PairContact(actors[i].get(),actors[j].get(),&hit))
                depth=std::max(depth,hit.depth);
        }
    }
    std::printf("AIR_LANDING count=5 landed=%d rebound_events=%d max_step=%.5f final_depth=%.5f drift_last_2s=%.5f\n",
                landed,rebounds,maximumStep,depth,drift);
    Check(rebounds>0,"five nearby Air Prism reach the vanilla unit crash/rebound path during landing");
    Check(landed==5,"five nearby Air Prism settle on the production landing controller");
    Check(depth<=profile->tolerance+.02,"landed Air Prism do not remain piled through each other");
    Check(drift<.01,"landed Air Prism rest without continuing to skate");
    Check(maximumStep<100,"Air Prism landing has no large discontinuous relocation");
    world._collisionScene.reset();
}
void PlayerTankUnitRegression(const std::shared_ptr<Collision::Shape> &cube) {
    for(bool shaped:{false,true}) for(bool reverse:{false,true}) {
        GroundWorld world;
        Actor<NC_STACK_ypatank> moving, stopped;
        const vec3d centre(2400,-10,-2400),direction=vec3d(0,0,reverse?-1:1);
        moving.Bind(world,710,centre-direction*30,shaped?cube:nullptr);
        stopped.Bind(world,711,centre,shaped?cube:nullptr);
        for(auto *a:{&moving,&stopped}) {
            a->_bact_type=BACT_TYPES_TANK; a->_status_flg=BACT_STFLAG_LAND;
            a->_radius=10; a->_overeof=10; a->_mass=10;
            a->_pSector=&world._cells.At(0); a->_wrldSize=vec2d(9600,-9600);
        }
        moving._oflags|=BACT_OFLAG_USERINPT;
        moving._fly_dir=vec3d(0,0,1); moving._fly_dir_length=reverse?-40:40;
        world._timeStamp+=20;
        const auto start=moving._position, target=stopped._position;
        moving._position=centre-direction*15;
        if(shaped) moving.ResolveShapeMovement(start,moving._rotation);
        const bool response=moving.CollisionWithBact(20)!=0;
        const double push=(stopped._position-target).dot(direction);
        std::printf("PLAYER_TANK shape=%d reverse=%d response=%d crash=%d push=%.6f land=%d\n",
                    shaped,reverse,response,!!(moving._status_flg&BACT_STFLAG_BCRASH),push,
                    !!(stopped._status_flg&BACT_STFLAG_LAND));
        Check(response && (moving._status_flg&BACT_STFLAG_BCRASH),
              "player tank versus AI tank activates the vanilla unit crash response and sound event");
        Check(fabs(push-6.4)<.01,"player tank shove retains vanilla mass and speed calculation in forward/reverse");
        if(shaped) Check(stopped._status_flg&BACT_STFLAG_LAND,
                        "shape-constrained tank shove preserves ground support rather than restarting a fall");
        world._collisionScene.reset();
    }
    BuildingWorld world;
    Actor<NC_STACK_ypatank> player,target;
    const vec3d atWall(3000,-10,-3150.3);
    player.Bind(world,712,atWall-vec3d(0,0,30),cube);
    target.Bind(world,713,atWall,cube);
    for(auto *a:{&player,&target}) {
        a->_bact_type=BACT_TYPES_TANK; a->_status_flg=BACT_STFLAG_LAND;
        a->_oflags=BACT_OFLAG_BACTCOLL|BACT_OFLAG_EXACTCOLL;
        a->_mass=10; a->_overeof=10; a->_wrldSize=vec2d(9600,-9600);
    }
    player._oflags|=BACT_OFLAG_USERINPT;
    player._fly_dir=vec3d(0,0,1); player._fly_dir_length=40;
    world._timeStamp+=20;
    const auto start=player._position;
    player._position.z+=15;
    player.ResolveShapeMovement(start,player._rotation);
    player.CollisionWithBact(20);
    Collision::Contact hit;
    Check((player._status_flg&BACT_STFLAG_BCRASH) && target._position.z-atWall.z<.31,
          "player tank shove stops the target at the nearby LEGO building");
    Check(!Collision::ContactShapes(*cube,target.GetBodyPosition(),target._rotation,
                                   *world.solid,vec3d(3000,0,-3000),mat3x3::Ident(),&hit) || hit.depth<=cube->tolerance+.01,
          "restored player shove cannot push another tank through a building");
    world._collisionScene.reset();
}
void ContactPlaneRegression(const std::shared_ptr<Collision::Shape> &cube) {
    std::mt19937 random(17); std::uniform_real_distribution<double> angle(-3.14,3.14),offset(-12,12);
    bool clear=true,finite=true; int overlaps=0;
    for(int i=0;i<200;++i) {
        const auto a=mat3x3::RotateY(angle(random))*mat3x3::RotateX(angle(random));
        const auto b=mat3x3::RotateZ(angle(random))*mat3x3::RotateY(angle(random));
        const vec3d position(offset(random),offset(random),offset(random));
        Collision::Contact hit,after;
        if(!Collision::ContactShapes(*cube,position,a,*cube,vec3d(0,0,0),b,&hit)) continue;
        ++overlaps;
        finite &= std::isfinite(hit.depth) && hit.depth>=0 && fabs(hit.normal.length()-1)<1e-5;
        const auto separated=position+hit.normal*(hit.depth+.002);
        clear &= !Collision::ContactShapes(*cube,separated,a,*cube,vec3d(0,0,0),b,&after) || after.depth<.003;
    }
    Check(overlaps>150 && finite,"rotated face and edge contacts have finite normalized support planes");
    Check(clear,"refined GJK contacts clear the complete rotated convex bodies");
    // Independent OBB SAT checks detection, including separating edge axes;
    // the production support-plane refinement must not invent contacts.
    std::uniform_real_distribution<double> spacing(-35,35);
    bool detection=true;
    for(int i=0;i<300;++i) {
        const auto a=mat3x3::RotateY(angle(random))*mat3x3::RotateX(angle(random));
        const auto b=mat3x3::RotateZ(angle(random))*mat3x3::RotateY(angle(random));
        const vec3d position(spacing(random),spacing(random),spacing(random));
        const std::vector<vec3d> aa{a.AxisX(),a.AxisY(),a.AxisZ()},bb{b.AxisX(),b.AxisY(),b.AxisZ()};
        std::vector<vec3d> axes=aa; axes.insert(axes.end(),bb.begin(),bb.end());
        for(auto x:aa) for(auto y:bb) axes.push_back(x*y);
        bool expected=true; double smallest=1e30;
        for(auto axis:axes) {
            if(axis.normalise()<1e-9) continue;
            double extent=0;
            for(auto direction:aa) extent+=10*fabs(axis.dot(direction));
            for(auto direction:bb) extent+=10*fabs(axis.dot(direction));
            const double overlap=extent-fabs(axis.dot(position));
            smallest=std::min(smallest,overlap);
            expected &= overlap>0;
        }
        Collision::Contact hit;
        const bool actual=Collision::ContactShapes(*cube,position,a,*cube,vec3d(0,0,0),b,&hit);
        if(fabs(smallest)>.003) detection &= expected==actual;
    }
    Check(detection,"GJK hull detection agrees with independent face/edge OBB SAT for overlap and separation");
}
void PairBenchmark(const std::shared_ptr<Collision::Shape> &profile) {
    const int frames=std::getenv("NATIVE_BENCH_FRAMES")?std::max(6,std::atoi(std::getenv("NATIVE_BENCH_FRAMES"))):6;
    for(int count:{5,20}) {
        TestWorld world; world._mapSize=Common::Point(32,32); world._cells.Resize(world._mapSize);
        world._collisionScene.reset(new Collision::Scene(world));
        std::vector<std::unique_ptr<Actor<>>> actors;
        std::vector<vec3d> positions;
        const double spacing=profile->radius*.7;
        for(int i=0;i<count;++i) {
            const vec3d position(3000+(i%5)*spacing,-150,-3000-(i/5)*spacing);
            positions.push_back(position); actors.emplace_back(new Actor<>);
            actors.back()->Bind(world,650+i,position,profile);
        }
        double first=0,maximum=0,total=0;
        for(int frame=0;frame<frames;++frame) {
            world._timeStamp+=20;
            for(int i=0;i<count;++i) {
                actors[i]->_position=positions[i];
                actors[i]->_fly_dir=vec3d(1,0,0); actors[i]->_fly_dir_length=20;
            }
            const auto start=std::chrono::steady_clock::now();
            for(int i=0;i<count;++i) {
                actors[i]->_position.x+=2;
                actors[i]->ResolveShapeMovement(positions[i],actors[i]->_rotation,0);
            }
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(frame==0) first=ms; maximum=std::max(maximum,ms); total+=ms;
        }
        std::printf("BENCH unit_contacts=%d hulls=%zu first_ms=%.4f max_ms=%.4f mean_ms=%.4f\n",
                    count,profile->parts.size(),first,maximum,total/frames);
        world._collisionScene.reset();
    }
}
void ProfileLoadRegression(const char *path) {
    TestWorld world;
    world._collisionScene.reset(new Collision::Scene(world));
    const auto begin=std::chrono::steady_clock::now();
    auto profile=world._collisionScene->LoadShared(path);
    const auto loaded=std::chrono::steady_clock::now();
    bool shared=bool(profile);
    for(int i=0;i<1000;++i) shared &= world._collisionScene->LoadShared(path)==profile;
    const auto cached=std::chrono::steady_clock::now();
    std::printf("PROFILE_LOAD path=%s first_ms=%.5f cached_1000_ms=%.5f\n",path,
        std::chrono::duration<double,std::milli>(loaded-begin).count(),
        std::chrono::duration<double,std::milli>(cached-loaded).count());
    Check(shared,"mission actors share a collision profile instead of rebuilding it for each spawn");
}
void AIOrderRegression(std::shared_ptr<Collision::Shape> tiger) {
    const vec3d shapedStart(2400,-13,-2400), legacyStart(2400,-13,-3600);
    GroundWorld freeWorld;
    freeWorld.world145=0;
    AITank shaped,legacy;
    PrepareAITank(shaped,freeWorld,84,shapedStart,shapedStart+vec3d(0,0,900),tiger);
    PrepareAITank(legacy,freeWorld,85,legacyStart,legacyStart+vec3d(0,0,900),{});
    const vec3d shapedTarget=shaped._primTpos, legacyTarget=legacy._primTpos;
    for(int frame=0;frame<40;++frame) {
        shaped._target_vec=shapedTarget-shaped._position;
        legacy._target_vec=legacyTarget-legacy._position;
        StepAITank(shaped,freeWorld);
        StepAITank(legacy,freeWorld);
    }
    const double shapedTravel=(shaped._position-shapedStart).XZ().length();
    const double legacyTravel=(legacy._position-legacyStart).XZ().length();
    const bool finiteFree=std::isfinite(shaped._position.x)&&std::isfinite(shaped._position.y)&&
                          std::isfinite(shaped._position.z)&&std::isfinite(legacy._position.x)&&
                          std::isfinite(legacy._position.y)&&std::isfinite(legacy._position.z);
    std::printf("AI_FREE shaped=%.3f legacy=%.3f shape_pos=%.3f,%.3f,%.3f legacy_pos=%.3f,%.3f,%.3f\n",
                shapedTravel,legacyTravel,shaped._position.x,shaped._position.y,shaped._position.z,
                legacy._position.x,legacy._position.y,legacy._position.z);
    Check(finiteFree,"production tank AI movement stays finite on flat support");
    Check(shapedTravel>1.0&&legacyTravel>1.0,"production tank AI makes meaningful free-ground progress");
    Check(fabs(shapedTravel-legacyTravel)<.1,"shape tank AI keeps legacy free-ground progress");
    Check(fabs(shaped._position.y-legacy._position.y)<.1,"shape tank AI keeps legacy support height");

    GroundWorld obstacleWorld;
    obstacleWorld.world145=1;
    AITank mover, obstacle;
    const vec3d moverStart(2400,-13,-2400);
    double minZ=std::numeric_limits<double>::infinity(), maxZ=-std::numeric_limits<double>::infinity();
    for(const auto &part:tiger->parts) for(const auto &vertex:part.vertices) {
        minZ=std::min(minZ,vertex.z); maxZ=std::max(maxZ,vertex.z);
    }
    // Place the real Tiger just beyond the mover's forward support extent. It
    // is an idle grounded vehicle so the normal pass/bypass decision applies.
    const vec3d obstaclePosition=moverStart+vec3d(0,0,(maxZ-minZ)+24.0);
    const vec3d moverTarget=obstaclePosition+vec3d(0,0,900);
    PrepareAITank(mover,obstacleWorld,86,moverStart,moverTarget,tiger);
    PrepareAITank(obstacle,obstacleWorld,87,obstaclePosition,obstaclePosition,tiger);
    obstacle._status=BACT_STATUS_IDLE;
    obstacle._status_flg=BACT_STFLAG_LAND;
    obstacle._primTtype=BACT_TGT_TYPE_NONE;
    obstacle._target_vec=vec3d(0,0,0);
    obstacle._fly_dir_length=0;
    obstacle._thraction=0;
    obstacleWorld._collisionScene->UpdateActor(&obstacle);
    Collision::Contact initialContact;
    Check(!obstacleWorld._collisionScene->PairContact(&mover,&obstacle,&initialContact),
          "AI bypass fixture starts outside Tiger hull contact");
    bool noDeepOverlap = true, supported = true;
    double maxDepth=0, maxHeightError=0;
    // Allow the production controller to turn, drive its 80-unit bypass leg,
    // and return toward the order; a two-second run ends during the turn.
    for(int frame=0;frame<600;++frame) {
        mover._target_vec=moverTarget-mover._position;
        supported &= StepAITank(mover,obstacleWorld);
        Collision::Contact contact;
        const bool touched=obstacleWorld._collisionScene->PairContact(&mover,&obstacle,&contact);
        if(touched) maxDepth=std::max(maxDepth,contact.depth);
        maxHeightError=std::max(maxHeightError,fabs(mover._position.y-moverStart.y));
        noDeepOverlap &= !touched || contact.depth <= tiger->tolerance + .002;
    }
    const double bypassLateral=fabs(mover._position.x-moverStart.x);
    const double targetProgress=mover._position.z-moverStart.z;
    const bool finiteObstacle=std::isfinite(mover._position.x)&&std::isfinite(mover._position.y)&&
                              std::isfinite(mover._position.z);
    std::printf("AI_OBSTACLE progress_z=%.3f lateral_x=%.3f position=%.3f,%.3f,%.3f flags=%u angle=%.3f\n",
                targetProgress,bypassLateral,mover._position.x,mover._position.y,mover._position.z,
                static_cast<unsigned>(mover.CollisionFlags()),mover.CollisionAngle());
    std::printf("AI_CLEAR depth=%.5f height_error=%.5f tolerance=%.5f\n",maxDepth,maxHeightError,tiger->tolerance);
    Check(finiteObstacle,"production tank AI stays finite at a Tiger obstacle");
    Check(bypassLateral>10.0&&targetProgress>(obstaclePosition.z-moverStart.z)+20.0,
          "shape tank AI bypasses a grounded idle Tiger and continues toward its CELL target");
    Check(noDeepOverlap && supported && fabs(mover._position.y-moverStart.y)<.01,
          "AI bypass preserves controller support height without crossing the other hull");
    mover._oflags |= BACT_OFLAG_USERINPT;
    mover._status_flg |= BACT_STFLAG_MOVE;
    mover._fly_dir_length=20;
    const auto flags=mover.CollisionFlags(); const auto angle=mover.CollisionAngle();
    mover.HandleShapeUnitCollision(&obstacle,-mover._rotation.AxisZ());
    Check(mover._fly_dir_length==20 && (mover._status_flg&BACT_STFLAG_MOVE) &&
          mover.CollisionFlags()==flags && mover.CollisionAngle()==angle,
          "shape contact does not put a player-controlled tank into AI avoidance");
    GroundWorld fastWorld; fastWorld.world145=1;
    AITank fast, stopped;
    const vec3d fastStart(2400,-13,-4800), stoppedAt=fastStart+(obstaclePosition-moverStart);
    PrepareAITank(fast,fastWorld,88,fastStart,stoppedAt+vec3d(0,0,900),tiger);
    PrepareAITank(stopped,fastWorld,89,stoppedAt,stoppedAt,tiger);
    stopped._status=BACT_STATUS_IDLE; stopped._primTtype=BACT_TGT_TYPE_NONE;
    stopped._fly_dir_length=0; stopped._thraction=0;
    fast._fly_dir_length=2000;
    fastWorld._collisionScene->UpdateActor(&stopped);
    StepAITank(fast,fastWorld);
    Collision::Contact fastContact;
    Check(std::isfinite(fast._position.z) && fast._position.z<stoppedAt.z &&
          (!fastWorld._collisionScene->PairContact(&fast,&stopped,&fastContact) ||
           fastContact.depth<=tiger->tolerance+.002),
          "high-speed tank AI stops before crossing an idle Tiger hull");
    Check(fast.CollisionFlags()!=0 && fabs(fast._fly_dir_length)<.01,
          "high-speed geometric contact starts the existing AI bypass reaction");
    fastWorld._collisionScene.reset();
    obstacleWorld._collisionScene.reset();
    freeWorld._collisionScene.reset();
}
void BuildingControlsRegression(std::shared_ptr<Collision::Shape> tiger) {
    GroundWorld w; w._collisionScene.reset();
    for(auto &cell:w._cells) cell.SectorType=1;
    w.floorData.POO={vec3d(0,-200,-600),vec3d(0,0,-600),vec3d(0,0,600),vec3d(0,-200,600),
                     vec3d(-600,0,-600),vec3d(-600,0,600),vec3d(600,0,600),vec3d(600,0,-600)};
    UAskeleton::Polygon wall; wall.num_vertices=4; wall.A=1;
    for(int i=0;i<4;++i) wall.v[i]=i;
    UAskeleton::Polygon ground=wall; ground.A=0; ground.B=1;
    for(int i=0;i<4;++i) ground.v[i]=i+4;
    w.floorData.polygons={wall,ground};
    w._collisionScene.reset(new Collision::Scene(w));
    auto longBody=Shape({Box(vec3d(0,0,0),2,10,50)});
    auto wallBounds = [&](const NC_STACK_ypabact &actor) {
        double lo=1e30, hi=-1e30;
        for(const auto &part:actor._collisionShape->parts) for(const auto &vertex:part.vertices) {
            const double x=(actor.GetBodyPosition()+actor._rotation.Transpose().Transform(vertex)).x;
            lo=std::min(lo,x); hi=std::max(hi,x);
        }
        return std::make_pair(lo,hi);
    };
    Actor<NC_STACK_ypatank> tank;
    tank.Bind(w,82,vec3d(2990,-10,-3000),longBody);
    tank._bact_type=BACT_TYPES_TANK; tank._status_flg=BACT_STFLAG_LAND;
    tank._oflags=BACT_OFLAG_EXACTCOLL; tank._overeof=10;
    tank._wrldSize=vec2d(9600,-9600); tank._mass=1; tank._force=100;
    tank._airconst_static=1; tank._soundcarrier.Resize(7);
    const vec3d start=tank._position; const mat3x3 original=tank._rotation;
    w._timeStamp+=20;
    tank.ResolveShapeMovement(start,original);
    tank._rotation=mat3x3::RotateY(.8);
    move_msg step; step.flag=0; step.field_0=0;
    tank.Move(&step);
    const mat3x3 accepted=tank._rotation;
    const double angle=std::atan2(fabs(accepted.m02),accepted.m00);
    Check(angle>.1 && angle<=.8 && wallBounds(tank).second<=3000.01 &&
          (tank._position-start).length()<=10.01,
          "wall steering makes a safe partial turn without a large position jump");
    Check(tank._position.x<start.x && fabs(tank._position.y-start.y)<.01,
          "wall-limited ground steering moves outward while preserving support height");
    tank.ResolveShapeMovement(start,original,20);
    Check((tank._rotation.AxisZ()-accepted.AxisZ()).length()<.001,
          "end-of-update collision does not replay or cancel resolved steering");
    Check((tank._old_pos-start).length()<.001,"shape response preserves controller movement history");
    auto completeTurn = [&](NC_STACK_ypabact &actor,double wantedAngle,int frames) {
        bool outside=true,bounded=true;
        for(int frame=0;frame<frames;++frame) {
            w._timeStamp+=20;
            const vec3d from=actor._position; const mat3x3 rotation=actor._rotation;
            actor.ResolveShapeMovement(from,rotation);
            actor._rotation=mat3x3::RotateY(wantedAngle);
            actor.ResolveShapeMovement(from,rotation,20);
            outside &= wallBounds(actor).second<=3000.01;
            bounded &= (actor._position-from).length()<=10.01;
        }
        Check(outside && bounded,"repeated wall steering stays outside with bounded corrections per frame");
        return fabs(std::atan2(fabs(actor._rotation.m02),actor._rotation.m00)-wantedAngle)<.002;
    };
    Check(completeTurn(tank,.8,8),"ground tank completes its wall turn progressively without another vehicle pushing it");
    w._timeStamp+=20;
    const vec3d atWall=tank._position;
    tank.ResolveShapeMovement(atWall,tank._rotation);
    const mat3x3 turnAway=mat3x3::RotateY(-.04);
    tank._rotation=turnAway; tank.Move(&step);
    Check((tank._rotation.AxisZ()-turnAway.AxisZ()).length()<.001,
          "tank can turn away from a building contact");
    const vec3d beforeRetreat=tank._position;
    tank._position.x-=25;
    tank.ResolveShapeMovement(beforeRetreat,tank._rotation);
    Check(fabs(tank._position.x-beforeRetreat.x+25)<.01,"tank can leave building contact without sticking");
    w._collisionScene->Forget(&tank);
    tank._collisionShape=tiger; tank._overeof=13;
    tank._position=vec3d(2950,-13,-3000); tank._rotation=mat3x3::RotateY(-1.57079632679);
    tank._fly_dir=tank._rotation.AxisZ(); tank._fly_dir_length=-30;
    w._timeStamp+=20;
    const vec3d reverseStart=tank._position;
    step.field_0=.02; tank.Move(&step);
    Check(tank._fly_dir_length<0 && (tank._fly_dir-tank._rotation.AxisZ()).length()<.001 &&
          (tank._position-reverseStart).length()>1,
          "real Tiger retains signed reverse movement away from a wall");
    Actor<> flying;
    flying.Bind(w,83,vec3d(2990,-80,-3000),longBody);
    flying._soundcarrier.Resize(7);
    flying._oflags=BACT_OFLAG_EXACTCOLL;
    w._timeStamp+=20;
    const vec3d flyStart=flying._position;
    flying._rotation=mat3x3::RotateY(.8);
    flying.ResolveShapeMovement(flyStart,original);
    const double flyAngle=std::atan2(fabs(flying._rotation.m02),flying._rotation.m00);
    std::printf("AIR_CLEAR angle=%.5f pos=%.5f,%.5f,%.5f max_x=%.5f\n",flyAngle,flying._position.x,flying._position.y,flying._position.z,wallBounds(flying).second);
    Check(flyAngle>.1 && flyAngle<=.8 && fabs(flying._position.y-flyStart.y)<.01 &&
          wallBounds(flying).second<=3000.01 && (flying._position-flyStart).length()<=10.01,
          "airborne wall steering preserves altitude and limits its correction");
    Check(completeTurn(flying,.8,8),"airborne body completes its wall turn progressively");
    w._collisionScene->Forget(&flying);
    flying._position=flying._old_pos=flyStart; flying._rotation=original;
    flying._fly_dir=vec3d(1,0,0); flying._fly_dir_length=10;
    w._timeStamp+=20;
    flying._position.x+=5; flying._rotation=mat3x3::RotateY(.8);
    flying.ResolveShapeMovement(flyStart,original,20);
    Check(wallBounds(flying).second<=3000.01,
          "airborne impact recoil preserves the safe pose after simultaneous steering and translation");
    Check((flying._fly_dir*flying._fly_dir_length).x<0,
          "airborne building impact restores the vanilla reflected velocity");
    flying._fly_dir_length=0;
    const vec3d flyRetreat=flying._position;
    flying._position.x-=25;
    flying.ResolveShapeMovement(flyRetreat,flying._rotation);
    Check(fabs(flying._position.x-flyRetreat.x+25)<.01,"airborne body can retreat from a building contact");
    w._timeStamp+=20;
    flying._position=vec3d(2998,-80,-3000); flying._rotation=original;
    const vec3d touching=flying._position;
    flying._rotation=mat3x3::RotateY(3.0);
    flying.ResolveShapeMovement(touching,original);
    Check(completeTurn(flying,3.0,12) &&
          flying._position.x<=3000-std::sqrt(2504.)+.01 && wallBounds(flying).second<=3000.01,
          "exact-contact half-turn clears the interior swept extremum hidden by its end pose");
    // A second wall blocks the clearance move: an oversized rotation must
    // remain partial rather than forcing the body through the opposite wall.
    for(auto vertex:{vec3d(-55,-200,-600),vec3d(-55,0,-600),vec3d(-55,0,600),vec3d(-55,-200,600)})
        w.floorData.POO.push_back(vertex);
    UAskeleton::Polygon opposite=wall;
    for(int i=0;i<4;++i) opposite.v[i]=i+8;
    w.floorData.polygons.push_back(opposite);
    w._timeStamp+=20;
    flying._position=touching; flying._rotation=original;
    w._collisionScene->UpdateActor(&flying);
    flying._rotation=mat3x3::RotateY(1.57079632679);
    flying.ResolveShapeMovement(touching,original);
    const auto bounds=wallBounds(flying);
    Check(bounds.first>=2944.99 && bounds.second<=3000.01 &&
          fabs(flying._rotation.AxisZ().z)>.1,
          "rotation clearance blocked by a second wall remains a safe partial turn");
    w.floorData.polygons.pop_back(); w.floorData.POO.resize(8);
    w._collisionScene->Forget(&flying);
    flying._collisionShape=Shape({Box(vec3d(-6,0,-30),8,10,20),Box(vec3d(6,0,30),8,10,20)});
    flying._position=vec3d(2980,-80,-3000); flying._rotation=original;
    bool compoundOutside=true;
    for(int frame=0;frame<4;++frame) {
        w._timeStamp+=20;
        const vec3d from=flying._position; const mat3x3 rotation=flying._rotation;
        flying._rotation=mat3x3::RotateY(.8);
        flying.ResolveShapeMovement(from,rotation);
        compoundOutside &= wallBounds(flying).second<=3000.01;
    }
    Check(compoundOutside && fabs(std::atan2(fabs(flying._rotation.m02),flying._rotation.m00)-.8)<.002,
          "offset compound hulls complete repeated wall steering without crossing the surface");
    // Move applies its playable-box constraint before the collision service.
    // Clearance must also respect that same constraint near the map border.
    for(int i=0;i<4;++i) w.floorData.POO[i].x=-550; // wall at x=1250 in the x=1800 sector
    w._collisionScene->Forget(&tank);
    tank._collisionShape=longBody; tank._overeof=10;
    tank._position=vec3d(1240,-10,-3000); tank._rotation=original;
    tank._fly_dir_length=0; tank._thraction=0;
    w._timeStamp+=20;
    tank.ResolveShapeMovement(tank._position,original);
    tank._rotation=mat3x3::RotateY(1.57079632679);
    step.field_0=0; tank.Move(&step);
    std::printf("BORDER_CLEAR x=%.5f max_x=%.5f angle=%.5f\n",tank._position.x,wallBounds(tank).second,
                std::atan2(fabs(tank._rotation.m02),tank._rotation.m00));
    Check(tank._position.x>=1210-.01 && wallBounds(tank).second<=1250.01 &&
          fabs(tank._rotation.AxisZ().z)>.1,
          "clearance respects the existing playable border and stops an impossible full turn");
    w._collisionScene->Forget(&flying);
    flying._collisionShape=longBody; flying._wrldSize=vec2d(9600,-9600);
    flying._oflags=BACT_OFLAG_EXACTCOLL|BACT_OFLAG_VIEWER;
    flying._viewer_overeof=25;
    flying._position=vec3d(1240,-20,-3000); flying._rotation=original;
    const vec3d viewerStart=flying._position;
    w._timeStamp+=20;
    flying._rotation=mat3x3::RotateY(1.57079632679);
    flying.ResolveShapeMovement(viewerStart,original);
    Check(fabs(flying._position.y-viewerStart.y)<.01 && flying._position.x>=1210-.01 &&
          wallBounds(flying).second<=1250.01,
          "airborne viewer border clearance preserves altitude without the movement clamp ground snap");
    w._collisionScene.reset();
}
void RoboPositionRegression() {
    const vec3d centre(2400,-30,-2400);
    auto body=Shape({Box(vec3d(0,0,0),30,10,30)});
    {
        auto compound=Shape({Box(vec3d(-20,0,0),20,10,30),Box(vec3d(20,0,0),20,10,30)});
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        Robo host; host.Bind(w,707,centre,compound); host._bact_type=BACT_TYPES_ROBO;
        Actor<NC_STACK_ypatank> tank; tank.Bind(w,708,centre+vec3d(0,5,0),Shape({Box(vec3d(0,0,0),8,13,8)}));
        tank._bact_type=BACT_TYPES_TANK; tank._status_flg=BACT_STFLAG_LAND;
        host.ResolveShapeMovement(host._position,host._rotation);
        Collision::Contact contact;
        Check(fabs(host._position.y-centre.y)<.001 &&
              (!w._collisionScene->PairContact(&host,&tank,&contact) || contact.depth<=compound->tolerance+.001),
              "opposing Robo hull leaves use a consistent horizontal escape direction");
        w._collisionScene.reset();
    }
    {
        auto tall=Shape({Box(vec3d(0,5,0),10,20,10)});
        for(const auto &rotation:{mat3x3::Ident(),mat3x3::RotateX(.25),mat3x3::RotateZ(.3)}) {
            double expected=0;
            for(const auto &vertex:tall->parts[0].vertices)
                expected=std::max(expected,rotation.Transpose().Transform(vertex).y);
            Check(fabs(Collision::DownExtent(*tall,rotation)-expected)<.001,
                  "Robo lower extent follows the actual rotated hull support");
        }
        GroundWorld w; Robo host;
        host.Bind(w,709,vec3d(2400,-24.9,-2400),tall); host._bact_type=BACT_TYPES_ROBO;
        host._height=10; host._old_pos=host._position+vec3d(1,0,0);
        host.checkCollisions(.02);
        Check(host._target_dir.y<-.7 && (host._status_flg&BACT_STFLAG_UPWRD),
              "Robo hover controller does not request a height inside its taller shape");
        host._collisionShape.reset(); host._target_dir=vec3d(0,0,0);
        host.checkCollisions(.02);
        Check(host._target_dir.y>.7 && !(host._status_flg&BACT_STFLAG_UPWRD),
              "profile-free Robo retains its original authored height probe");
        w._collisionScene.reset();
    }
    {
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        Robo host; host.Bind(w,710,centre,body); host._bact_type=BACT_TYPES_ROBO;
        Actor<NC_STACK_ypatank> tank; tank.Bind(w,711,centre+vec3d(20,5,0),body);
        tank._bact_type=BACT_TYPES_TANK; tank._status_flg=BACT_STFLAG_LAND;
        host.ResolveShapeMovement(host._position,host._rotation);
        Check(fabs(host._position.y-centre.y)<.001,"Robo separates from grounded unit without a vertical teleport");
        Collision::Contact contact;
        Check(!w._collisionScene->PairContact(&host,&tank,&contact) || contact.depth<=body->tolerance+.001,
              "horizontal Robo contact still separates the physical hulls");
        contact.actor=&tank; contact.normal=vec3d(-1,0,0); contact.incomingVelocity=vec3d(20,0,0);
        tank._position=host._position+vec3d(20,5,0);
        host.HandleShapeUnitContact(contact,20);
        Check(fabs(host._fly_dir.y*host._fly_dir_length)<.001,
              "Robo side recoil does not convert centre height difference into lift");
        w._collisionScene.reset();
    }
    {
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        Robo host; host.Bind(w,712,centre,body); host._bact_type=BACT_TYPES_ROBO;
        host._wrldSize=vec2d(9600,-9600); host._pSector=&w._cells.At(0);
        Actor<> wall; wall.Bind(w,713,centre+vec3d(300,0,0),body);
        host.ResolveShapeMovement(host._position,host._rotation);
        const vec3d before=host._position; const mat3x3 rotation=host._rotation;
        bact_arg80 placement; placement.pos=centre+vec3d(600,0,0); placement.field_C=0;
        host.SetPosition(&placement);
        host.ResolveShapeMovement(before,rotation,20);
        Check((host._position-placement.pos).length()<.001,
              "native Robo placement does not sweep the teleport across intervening actors");
        wall._position=placement.pos;
        w._timeStamp+=20; w._collisionScene->UpdateActor(&wall);
        host.ResolveShapeMovement(host._position,host._rotation);
        Collision::Contact contact;
        Check(!w._collisionScene->PairContact(&host,&wall,&contact) || contact.depth<=body->tolerance+.001,
              "Robo placement still resolves a real overlap at the destination");
        w._collisionScene.reset();
    }
    {
        GroundWorld w;
        Robo host; host.Bind(w,714,vec3d(2400,-3,-2400),body); host._bact_type=BACT_TYPES_ROBO;
        host._oflags=BACT_OFLAG_EXACTCOLL; host._roboYPos=-3;
        w._timeStamp=20;
        host.ResolveShapeMovement(host._position,host._rotation);
        const vec3d before=host._position; const mat3x3 rotation=host._rotation;
        update_msg update{}; update.frameTime=20; update.gTime=20;
        host.wallow(&update);
        host.ResolveShapeMovement(before,rotation,20);
        host.UpdateUnitGuns(&update);
        const double phase=sin(update.gTime*C_PI/3000.0)*25.0;
        Check(fabs(host._roboYPos+phase-host._position.y)<.001,
              "Robo idle bob anchors to the accepted shape height after contact");
        double largestStep=0;
        for(int frame=1;frame<300;++frame) {
            w._timeStamp+=20; update.gTime=w._timeStamp;
            const vec3d old=host._position;
            host.ResolveShapeMovement(host._position,host._rotation);
            host.wallow(&update);
            host.ResolveShapeMovement(old,rotation,20);
            host.UpdateUnitGuns(&update);
            largestStep=std::max(largestStep,fabs(host._position.y-old.y));
        }
        Check(largestStep<.6,"Robo idle bob stays continuous over a complete contact cycle");
        w._collisionScene.reset();
    }
}
void RoboRealProfileContacts(const std::shared_ptr<Collision::Shape> &profile) {
    auto tankShape=Shape({Box(vec3d(0,0,0),25,13,25)});
    bool heightStable=true, clear=true;
    double maxStep=0,maxDepth=0;
    const double height=-Collision::DownExtent(*profile,mat3x3::Ident())-.1;
    for(int x=-120;x<=120;x+=30) for(int z=-120;z<=120;z+=30) {
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        Robo host; host.Bind(w,720,vec3d(3000,height,-3000),profile); host._bact_type=BACT_TYPES_ROBO;
        Actor<NC_STACK_ypatank> tank; tank.Bind(w,721,vec3d(3000+x,-13,-3000+z),tankShape);
        tank._bact_type=BACT_TYPES_TANK; tank._status_flg=BACT_STFLAG_LAND;
        host.ResolveShapeMovement(host._position,host._rotation);
        maxStep=std::max(maxStep,fabs(host._position.y-height));
        heightStable &= fabs(host._position.y-height)<.001;
        Collision::Contact contact;
        if(w._collisionScene->PairContact(&host,&tank,&contact)) {
            maxDepth=std::max(maxDepth,contact.depth);
            clear &= contact.depth<=profile->tolerance+.001;
        }
        w._collisionScene.reset();
    }
    std::printf("ROBO_REAL_CONTACTS cases=81 max_y_correction=%g max_depth=%g tolerance=%g\n",maxStep,maxDepth,profile->tolerance);
    Check(heightStable,"real Robo hull contacts retain controlled height in all 81 placements");
    Check(clear,"real Robo compound contacts clear opposing hull leaves without a residual trap");
}
void BuildingBenchmark(std::shared_ptr<Collision::Shape> profile) {
    for(int count:{10,50,100}) {
        GroundWorld w; w._collisionScene.reset();
        w._mapSize=Common::Point(32,32); w._cells.Resize(w._mapSize);
        for(auto &cell:w._cells) cell.SectorType=1;
        w.floorData.POO={vec3d(-150,-150,150),vec3d(150,-150,150),vec3d(150,0,150),vec3d(-150,0,150),
                         vec3d(-600,0,-600),vec3d(-600,0,600),vec3d(600,0,600),vec3d(600,0,-600)};
        w.floorData.polygons.clear();
        UAskeleton::Polygon wall; wall.num_vertices=4;
        for(int i=0;i<4;++i) wall.v[i]=i; wall.C=1;
        UAskeleton::Polygon ground=wall; ground.C=0; ground.B=1;
        for(int i=0;i<4;++i) ground.v[i]=i+4;
        w.floorData.polygons={wall,ground};
        w._collisionScene.reset(new Collision::Scene(w));
        std::vector<std::unique_ptr<Actor<>>> actors;
        for(int i=0;i<count;++i) {
            actors.emplace_back(new Actor<>);
            actors.back()->Bind(w,i+1,vec3d(600+(i%10)*1200,-80,-600-(i/10)*1200+250),profile);
            actors.back()->_oflags=BACT_OFLAG_EXACTCOLL;
        }
        auto start=std::chrono::steady_clock::now();
        for(int frame=0;frame<20;++frame) {
            w._timeStamp+=16;
            for(int i=0;i<count;++i) {
                auto *a=actors[i].get();
                const vec3d old(600+(i%10)*1200,-80,-600-(i/10)*1200+250);
                a->_position=old+vec3d(0,0,-150);
                w._collisionScene->Resolve(a,old,mat3x3::Ident(),0);
            }
        }
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
        std::printf("BENCH building_contacts=%d milliseconds_per_frame=%.4f\n",count,ms);
        w._collisionScene.reset();
    }
}
}
int main(int argc,char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const mat3x3 identity=mat3x3::Ident(); const vec3d zero(0,0,0);
    auto cube=Shape({Box(zero,10,10,10)});
    auto concave=Shape({Box(vec3d(-20,0,0),8,10,10),Box(vec3d(20,0,0),8,10,10)});
    ProbeMove<NC_STACK_ypabact>(cube,"production heli Move invokes swept response");
    ProbeMove<NC_STACK_ypatank>(cube,"production tank Move invokes swept response");
    ProbeMove<NC_STACK_ypacar>(cube,"production car Move inherits swept response");
    ProbeMove<NC_STACK_ypaflyer>(cube,"production flyer Move invokes swept response");
    ProbeMove<NC_STACK_ypaufo>(cube,"production UFO Move invokes swept response");
    ProbeMove<NC_STACK_yparobo>(cube,"production Robo Move invokes swept response",BACT_TYPES_ROBO);
    RoboPositionRegression();
    {
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        const vec3d centre(2400,0,-2400);
        Robo robo; robo.Bind(w,90,centre,cube); robo._bact_type=BACT_TYPES_ROBO;
        Actor<NC_STACK_ypagun> gun,sibling;
        gun.Bind(w,91,centre+vec3d(5,0,0),cube); gun._bact_type=BACT_TYPES_GUN;
        NC_STACK_base visual;
        gun._vp_normal=gun._vp_dead=gun._vp_fire=gun._vp_genesis=gun._vp_wait=gun._vp_megadeth=&visual;
        gun._wrldSize=vec2d(9600,-9600);
        sibling.Bind(w,92,gun._position,cube); sibling._bact_type=BACT_TYPES_GUN;
        gun.setGUN_roboGun(1); sibling.setGUN_roboGun(1);
        gun._parent=sibling._parent=&robo;
        Collision::Contact contact;
        Check(!w._collisionScene->PairContact(&robo,&gun,&contact),"native Robo gun excluded from owning host shape");
        Check(!w._collisionScene->PairContact(&gun,&sibling,&contact),"native Robo sibling guns do not collide");
        Actor<> follower; follower.Bind(w,93,centre,cube); follower._parent=&robo;
        Check(w._collisionScene->PairContact(&robo,&follower,&contact),"ordinary Robo squad follower remains a physical target");
        gun.setGUN_roboGun(0); gun._parent=nullptr;
        const vec3d anchor=gun._position; const mat3x3 aim=gun._rotation;
        Check(!w._collisionScene->Resolve(&gun,anchor,aim,20) && gun._position==anchor,
              "ground gun remains mounted despite overlapping geometry");
        Check(w._collisionScene->Trace(&gun,anchor+vec3d(0,0,-100),anchor+vec3d(0,0,100),0,&contact),
              "gun shape participates in production weapon trace");
        gun._rotation=mat3x3::RotateY(.7); gun._collisionShape=Shape({Box(zero,50,2,2)});
        w._collisionScene->Forget(&gun); w._collisionScene->UpdateActor(&gun);
        Check(w._collisionScene->Trace(&gun,anchor+vec3d(0,0,-100),anchor+vec3d(0,0,100),0,&contact),
              "gun shape follows aiming rotation");
        gun._rotation=mat3x3::RotateY(1.5707963267948966);
        w._collisionScene->Resolve(&gun,anchor,aim,20);
        auto aimedTargets=w._collisionScene->ShapeTargets(anchor+vec3d(-5,0,40),anchor+vec3d(5,0,40));
        Check(std::find(aimedTargets.begin(),aimedTargets.end(),&gun)!=aimedTargets.end(),
              "anchored gun broad phase follows current aiming rotation within frame");
        robo._position=sibling._position=centre+vec3d(0,0,500);
        w._collisionScene->UpdateActor(&robo); w._collisionScene->UpdateActor(&sibling);
        follower._position=centre+vec3d(-100,0,0);
        follower._fly_dir=vec3d(1,0,0); follower._fly_dir_length=200;
        const vec3d start=follower._position;
        follower._position=centre+vec3d(100,0,0);
        w._collisionScene->Resolve(&follower,start,follower._rotation,0);
        Check(follower._position.x<centre.x-6.7 && follower._position.x>centre.x-10 && gun._position==anchor,
              "approaching shaped vehicle stops without displacing mounted gun");
        contact.incomingVelocity=vec3d(10,0,0); contact.normal=vec3d(-1,0,0); contact.point=centre;
        robo.HandleShapeWorldCollision(contact);
        Check(robo.impactDamage==900000 && (robo._status_flg & BACT_STFLAG_UPWRD) &&
              fabs(robo._fly_dir_length-5)<.001 && robo._fly_dir.x<-.99,
              "Robo shape world response retains native recoil attenuation and building damage");
        World::TRoboGun mount; mount.gun_obj=&gun; mount.pos=vec3d(30,0,0);
        robo._roboGuns.push_back(mount);
        robo._position=centre+vec3d(0,-15,0);
        robo.UpdateUnitGuns(nullptr);
        Check(gun._position==robo._position+mount.pos,"native Robo gun follows shared final position correction");
        auto movedTargets=w._collisionScene->ShapeTargets(gun._position-vec3d(5,0,0),gun._position+vec3d(5,0,0));
        Check(std::find(movedTargets.begin(),movedTargets.end(),&gun)!=movedTargets.end(),
              "mounted gun placement updates query bounds within frame");
        robo._roboGuns.clear();
        robo._collisionShape.reset();
        const vec3d legacyPosition=robo._position;
        Check(!w._collisionScene->Resolve(&robo,legacyPosition,robo._rotation,20) && robo._position==legacyPosition,
              "profile-free Robo retains native movement solver");
        w._collisionScene.reset();
    }
    for(int type:{BACT_TYPES_GUN,BACT_TYPES_ROBO}) {
        TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
        w._collisionScene.reset(new Collision::Scene(w));
        const vec3d centre(2400,0,-2400);
        Actor<NC_STACK_ypagun> gun; Actor<NC_STACK_yparobo> host;
        NC_STACK_ypabact *victim;
        if(type==BACT_TYPES_GUN) { gun.Bind(w,95,centre,cube); victim=&gun; }
        else { host.Bind(w,95,centre,cube); victim=&host; }
        victim->_bact_type=type; victim->_radius=1000;
        Actor<> emitter; emitter.Bind(w,96,centre+vec3d(0,0,-200)); emitter._owner=2;
        Missile shot; shot.Bind(w,97,centre+vec3d(0,0,100));
        shot._bact_type=BACT_TYPES_MISSLE; shot._owner=2; shot.Emitter(&emitter);
        shot._old_pos=centre+vec3d(0,0,-100);
        NC_STACK_ypabact *target=nullptr;
        Check(shot.TubeCollisionTest(false,&target) && target==victim &&
              fabs(shot._position.z-(centre.z-10))<.1,"gun/Robo production projectile hits shape surface");
        shot._old_pos=centre+vec3d(100,0,-100); shot._position=centre+vec3d(100,0,100);
        Check(!shot.TubeCollisionTest(false,&target),"gun/Robo projectile ignores legacy radius outside valid shape");
        w._collisionScene.reset();
    }
    Collision::Contact hit;
    Check(!Collision::ContactShapes(*cube,zero,identity,*cube,vec3d(21,0,0),identity,&hit),"separated cubes");
    bool overlap=Collision::ContactShapes(*cube,zero,identity,*cube,vec3d(15,0,0),identity,&hit);
    std::printf("CONTACT depth=%g normal=%g,%g,%g\n",hit.depth,hit.normal.x,hit.normal.y,hit.normal.z);
    Check(overlap && fabs(hit.depth-5)<.03 && hit.normal.x<-.99,"penetration depth and outward normal");
    Check(Collision::SweepShapes(*cube,vec3d(-200,0,0),identity,vec3d(200,0,0),identity,*cube,zero,identity,zero,identity,&hit) && fabs(hit.fraction-.45)<.001,"continuous collision crosses entire body in one frame");
    Check(Collision::SweepShapes(*cube,vec3d(-100,0,0),identity,zero,identity,*cube,vec3d(100,0,0),identity,zero,identity,&hit) && fabs(hit.fraction-.9)<.001,"both bodies moving");
    std::mt19937 random(31);
    for(int i=0;i<80;++i) {
        double angle=(random()%6000)/1000.; mat3x3 rot=mat3x3::RotateY(angle);
        vec3d p=rot.Transpose().Transform(vec3d(15,0,0));
        Check(Collision::ContactShapes(*cube,zero,rot,*cube,p,rot,&hit),"rotated overlap");
        Check(!Collision::ContactShapes(*cube,zero,rot,*cube,rot.Transpose().Transform(vec3d(21,0,0)),rot,&hit),"rotated separation");
    }
    auto rod=Shape({Box(zero,50,2,2)});
    auto tiny=Shape({Box(zero,3,3,3)});
    const mat3x3 quarter=mat3x3::RotateY(1.5707963267948966);
    bool rotatingHit=false;
    for(auto p:{vec3d(35,0,35),vec3d(35,0,-35)})
        rotatingHit|=Collision::SweepShapes(*rod,zero,identity,zero,quarter,*tiny,p,identity,p,identity,&hit) && hit.fraction>0 && hit.fraction<1;
    Check(rotatingHit,"pure rotation CCD");
    Collision::Shape invalid; invalid.parts=cube->parts; invalid.parts[0].faces.pop_back();
    Check(!Collision::Build(invalid),"reject open hull");
    invalid.parts=cube->parts; invalid.parts[0].vertices[0].x=std::numeric_limits<double>::quiet_NaN();
    Check(!Collision::Build(invalid),"reject nonfinite coordinates");
    invalid.parts.assign(65,cube->parts[0]); Check(!Collision::Build(invalid),"reject hull cap");
    invalid.parts=cube->parts; invalid.parts[0].vertices.resize(129); Check(!Collision::Build(invalid),"reject vertex cap");
    Write(*cube,"fixture.collision"); FSMgr::iDir::setBaseDir("");
    std::string loadError; auto loaded=Collision::Load("fixture.collision",&loadError);
    if(!loaded) std::printf("LOAD_ERROR %s\n",loadError.c_str());
    Check(loaded!=nullptr,"production profile loader");
    std::ofstream("bad.collision")<<"begin_collision_shape\nversion = 2\nend\n";
    Check(!Collision::Load("bad.collision"),"unsupported format fallback");
    Check(!Collision::Load("missing.collision"),"missing profile fallback");
    {
        TestWorld spawn; spawn._mapSize=Common::Point(8,8); spawn._cells.Resize(spawn._mapSize);
        NC_STACK_base visual;
        spawn._vhclProtos.resize(3); spawn._vhclModels.resize(1,&visual);
        Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypagun>());
        Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_yparobo>());
        for(int type:{BACT_TYPES_GUN,BACT_TYPES_ROBO}) {
            auto &proto=spawn._vhclProtos[1]; proto.model_id=type;
            proto.weapon=-1;
            proto.scale_fx_pXX.fill(0);
            proto.collision_shape="fixture.collision";
            ypaworld_arg146 request; request.vehicle_id=1; request.pos=vec3d(2400,0,-2400);
            auto *actor=spawn.ypaworld_func146(&request);
            Check(actor && actor->_bact_type==type && actor->HasCollisionShape(),
                  "production spawn binds gun/Robo collision profile");
            if(actor) actor->Delete();
        }
        spawn._collisionScene.reset();
    }
    {
        TestWorld legacy; legacy._mapSize=Common::Point(8,8); legacy._cells.Resize(legacy._mapSize);
        const vec3d origin(1200,0,-1200);
        Actor<> victim,gunner; victim.Bind(legacy,10,origin); victim._radius=10;
        gunner.Bind(legacy,11,origin+vec3d(0,0,-200)); gunner._owner=2;
        Missile shot; shot.Bind(legacy,12,origin+vec3d(200,0,-600));
        shot._bact_type=BACT_TYPES_MISSLE; shot._owner=2; shot.Emitter(&gunner);
        shot._old_pos=origin+vec3d(200,0,-800);
        NC_STACK_ypabact *legacyTarget=nullptr;
        Check(shot.TubeCollisionTest(false,&legacyTarget) && legacyTarget==&victim,
              "profile-free production projectile retains legacy class tolerance");
        Check(!legacy._collisionScene,"profile-free world does not create new collision service");
    }
    std::vector<std::shared_ptr<Collision::Shape>> pilotProfiles;
    for(int i=1;i<std::min(argc,4);++i) {
        std::string error; auto s=Collision::Load(argv[i],&error); Check(s!=nullptr,error.c_str());
        if(s) { pilotProfiles.push_back(s); std::printf("PROFILE %s hulls=%zu radius=%.3f\n",argv[i],s->parts.size(),s->radius); }
    }
    if(argc>4) {
        ProfileLoadRegression(argv[4]);
        std::string error; auto authored=Collision::Load(argv[4],&error);
        Check(authored!=nullptr,error.c_str());
        if(authored) {
            std::printf("AUTHOR_PROFILE %s hulls=%zu source_set=%d\n",argv[4],authored->parts.size(),authored->assetSet);
            RoboRealProfileContacts(authored);
            BuildingBenchmark(authored);
            PairBenchmark(authored);
        }
    }
    if(!pilotProfiles.empty()) { SpawnLandingRegression(pilotProfiles.front()); GroundRegression(pilotProfiles.front()); AIOrderRegression(pilotProfiles.front()); AIWorldBuildingRegression(pilotProfiles.front()); BuildingControlsRegression(pilotProfiles.front()); BuildingBenchmark(pilotProfiles.front()); }
    if(pilotProfiles.size()>2) { AirWorldBuildingRegression(pilotProfiles[2]); AirLandingRegression(pilotProfiles[2]); }
    CompoundWorldSweepRegression();
    UnitResponseRegression(cube);
    PlayerTankUnitRegression(cube);
    ContactPlaneRegression(cube);
    if(!pilotProfiles.empty()) PairBenchmark(pilotProfiles.back());
    TestWorld world; world._mapSize=Common::Point(8,8); world._cells.Resize(world._mapSize);
    world._vhclProtos.resize(256); world._collisionScene.reset(new Collision::Scene(world));
    Actor<> a,b,emitter; const vec3d center(1200,0,-1200);
    a.Bind(world,1,center,concave); a._radius=1000;
    b.Bind(world,2,center); emitter.Bind(world,3,vec3d(1200,0,-1500)); emitter._owner=2;
    Check(!world._collisionScene->PairContact(&a,&b,&hit),"mixed sphere in genuine concavity misses despite legacy radius");
    b._position=center+vec3d(20,0,0); b._parent=&a;
    Check(world._collisionScene->PairContact(&a,&b,&hit),"ordinary squad follower still collides with its leader");
    b._isUnitGunChild=true;
    Check(!world._collisionScene->PairContact(&a,&b,&hit),"physical gun attachment is excluded from its owning body");
    b._isUnitGunChild=false; b._parent=nullptr; b._position=center;
    Check(!world._collisionScene->Trace(&a,center+vec3d(0,0,-100),center+vec3d(0,0,100),0,&hit),"ray traverses preserved empty concavity");
    Check(!world._collisionScene->Trace(&a,center+vec3d(0,0,-100),center+vec3d(0,0,100),2,&hit),"physical projectile radius misses concavity");
    Check(world._collisionScene->Trace(&a,center+vec3d(20,0,-100),center+vec3d(20,0,100),2,&hit) && fabs(hit.point.z-(center.z-10))<.1,"swept physical projectile hits actual surface");
    Check(!world._collisionScene->ShapeTargets(center+vec3d(20,0,-100),center+vec3d(20,0,100),2).empty(),"broadphase includes protruding model");
    world._collisionScene->Forget(&a); a._collisionShape=cube;
    b._position=center+vec3d(-100,0,0); world._timeStamp+=20; world._collisionScene->UpdateActor(&b);
    const vec3d old=b._position; b._position=center+vec3d(100,0,0); b._fly_dir=vec3d(1,0,0); b._fly_dir_length=50;
    bool moved=world._collisionScene->Resolve(&b,old,identity,0);
    std::printf("RESOLVE x=%g velocity=%g moved=%d\n",b._position.x,b._fly_dir_length,moved);
    Check(moved && b._position.x < center.x-11.8,"legacy sphere stops against shape across cell boundary");
    Check(b._fly_dir_length<.01,"entering velocity removed");
    auto &collisionDamage=System::IniConf::GameUnitFriendlyCollisionDamage;
    const auto savedCollisionDamage=collisionDamage.Value;
    const bool savedDamageWasSet=collisionDamage.WasSet;
    collisionDamage.Value=std::string("10"); collisionDamage.WasSet=true;
    a._energy=b._energy=1000; a._energy_max=b._energy_max=1000; a._shield=b._shield=0;
    b._position=center+vec3d(-100,0,0); world._timeStamp+=20; world._collisionScene->UpdateActor(&b);
    const vec3d damageOld=b._position; b._position=center+vec3d(100,0,0);
    world._collisionScene->Resolve(&b,damageOld,identity,0);
    Check(b._energy==1000,"movement contact defers gameplay damage to shared update");
    world._collisionScene->Resolve(&b,b._position,identity,20);
    Check(a._energy==990 && b._energy==990,"CCD contact retained after bodies stop at skin");
    world._collisionScene->Resolve(&b,b._position,identity,20);
    Check(a._energy==990 && b._energy==990,"collision damage is not repeated within actor frame");
    collisionDamage.Value=savedCollisionDamage; collisionDamage.WasSet=savedDamageWasSet;
    b._position=center+vec3d(-100,0,-20); world._timeStamp+=20; world._collisionScene->UpdateActor(&b);
    const vec3d slideOld=b._position; b._position=center+vec3d(100,0,40);
    Check(world._collisionScene->Resolve(&b,slideOld,identity,0) &&
          b._position.z>center.z+35 && b._position.x<center.x-11.8,"blocked movement continues along contact tangent");
    {
        Actor<> corner; corner.Bind(world,5,center+vec3d(-12,0,20),Shape({Box(zero,50,10,2)}));
        b._position=center+vec3d(-100,0,-20); world._timeStamp+=20; world._collisionScene->UpdateActor(&b);
        const vec3d cornerOld=b._position; b._position=center+vec3d(100,0,40);
        Check(world._collisionScene->Resolve(&b,cornerOld,identity,0) &&
              b._position.z<center.z+16.2 && b._position.x<center.x-11.8,"tangential slide stops at second wall of corner");
        world._collisionScene->Forget(&corner);
    }
    b._position=center+vec3d(5,0,0); world._timeStamp+=20;
    Check(world._collisionScene->Resolve(&b,b._position,identity,0) && fabs(b._position.x-center.x)>=11.8,"stationary initial overlap separated");
    Missile missile; missile.Bind(world,4,center+vec3d(0,0,100));
    missile._bact_type=BACT_TYPES_MISSLE; missile._owner=2; missile.Emitter(&emitter);
    missile._old_pos=center+vec3d(0,0,-100);
    b._position=center+vec3d(500,0,0); b._owner=2;
    NC_STACK_ypabact *target=nullptr;
    Check(missile.TubeCollisionTest(false,&target) && target==&a && fabs(missile._position.z-(center.z-10))<.1,"production projectile path uses physical radius and impact surface");
    missile._old_pos=center+vec3d(0,0,-100); missile._position=center+vec3d(0,0,100); world.wallFraction=.1;
    Check(!missile.TubeCollisionTest(false,&target),"world obstruction precedes shape hit"); world.wallFraction=-1;
    auto parse=[&](Engine::StringList lines) { ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)}; return ScriptParser::ParseStringList(lines,handlers,0); };
    Check(parse({"new_vehicle 1","model = heli","end"}) && world._vhclProtos[1].collision_shape.empty(),"absent parameter stays vanilla");
    Check(parse({"modify_vehicle 1","collision_shape = Data/Models/Collision/Pilot.collision","end"}) && world._vhclProtos[1].collision_shape=="Data/Models/Collision/Pilot.collision","production vehicle parser");
    Check(parse({"modify_vehicle 1","collision_shape = 0","end"}) && world._vhclProtos[1].collision_shape.empty(),"explicit zero stays vanilla");
    Check(parse({"modify_vehicle 1","collision_shape = ../escape.collision","end"}) && world._vhclProtos[1].collision_shape.empty(),"invalid path fallback");
    world._collisionScene->Forget(&a); a._collisionShape=concave; world._collisionScene->UpdateActor(&a);
    missile._old_pos=center+vec3d(0,0,-100); missile._position=center+vec3d(0,0,100);
    Check(!missile.TubeCollisionTest(false,&target),"production projectile ignores generous class radius through concavity");
    world._collisionScene->Forget(&a); a._collisionShape=cube; world._collisionScene->UpdateActor(&a);
    missile._old_pos=center; missile._position=center;
    Check(missile.TubeCollisionTest(false,&target) && target==&a,"stationary projectile initially inside shape hits");
    Check(world._collisionScene->Trace(&a,center,center+vec3d(0,0,100),0,&hit) && hit.fraction==0,"ray starts inside shape");
    Check(parse({"modify_vehicle 1","collision_shape = Data/Models/Collision/Pilot.collision","end"}) &&
          parse({"modify_vehicle 1","energy = 32000","end"}) &&
          !world._vhclProtos[1].collision_shape.empty(),"saved modifier preserves base profile binding");
    b._status=BACT_STATUS_DEAD; b._status_flg=BACT_STFLAG_DEATH1;
    b._position=center; b._vp_extra[0].flags=EVPROTO_FLAG_ACTIVE;
    b._scale_time=10000; b._energy_max=1000;
    a._energy=10; a._energy_max=1000; a._oflags=BACT_OFLAG_USERINPT|BACT_OFLAG_BACTCOLL;
    world._timeStamp+=20;
    world._collisionScene->Resolve(&a,a._position,identity,0);
    Check(b._scale_time<0 && a._energy>10,"shape vehicle retains plasma pickup without treating residue as solid");
    {
        auto &magnetRadius=System::IniConf::GamePlasmaDeathMagnetRadius;
        auto &magnetSpeed=System::IniConf::GamePlasmaDeathMagnetSpeed;
        const auto savedRadius=magnetRadius.Value;
        const auto savedSpeed=magnetSpeed.Value;
        const bool savedRadiusWasSet=magnetRadius.WasSet;
        const bool savedSpeedWasSet=magnetSpeed.WasSet;
        magnetRadius.Value=std::string("100"); magnetRadius.WasSet=true;
        magnetSpeed.Value=std::string("1000"); magnetSpeed.WasSet=true;

        a._status=BACT_STATUS_NORMAL; a._status_flg=0;
        a._position=center; a._energy=10; a._energy_max=1000;
        a._oflags=BACT_OFLAG_USERINPT|BACT_OFLAG_BACTCOLL;
        world._userUnit=&a;

        b._status=BACT_STATUS_DEAD; b._status_flg=BACT_STFLAG_DEATH1;
        b._position=center+vec3d(10,0,0); b._vp_extra[0].pos=b._position;
        b._vp_extra[0].flags=EVPROTO_FLAG_ACTIVE; b._scale_time=10000; b._energy_max=1000;
        b.UpdateDeathPlasmaMagnet(20);
        Check(b._scale_time<0 && a._energy>10,
              "plasma magnet completes pickup at player centre without collision-shape contact");

        a._energy=10;
        b._position=center; b._vp_extra[0].pos=center;
        b._vp_extra[0].flags=EVPROTO_FLAG_ACTIVE; b._scale_time=10000;
        b.UpdateDeathPlasmaMagnet(20);
        Check(b._scale_time<0 && a._energy>10,
              "plasma already at player centre is collected instead of remaining stuck");

        world._userUnit=nullptr;
        magnetRadius.Value=savedRadius; magnetRadius.WasSet=savedRadiusWasSet;
        magnetSpeed.Value=savedSpeed; magnetSpeed.WasSet=savedSpeedWasSet;
    }
    // Terrain uses the actual existing LEGO collision skeleton path.
    TSubSectorDesc subsection;
    for(auto &sector:world._secTypeArray)
        for(int x=0;x<3;++x) for(int y=0;y<3;++y) sector.SubSectors.At(x,y)=&subsection;
    UAskeleton::Data wallData; wallData.POO={vec3d(0,-200,-150),vec3d(0,200,-150),vec3d(0,200,150),vec3d(0,-200,150)};
    UAskeleton::Polygon face; face.num_vertices=4; face.v[0]=0; face.v[1]=1; face.v[2]=2; face.v[3]=3;
    wallData.polygons.push_back(face); NC_STACK_skeleton wall; wall._resData=&wallData;
    world._legoArray[0].CollisionSkelet=&wall;
    world._collisionScene->Forget(&a); a._collisionShape=cube;
    a._oflags=BACT_OFLAG_EXACTCOLL; a._position=vec3d(250,0,-300); world._timeStamp+=20;
    world._collisionScene->UpdateActor(&a); const vec3d wallOld=a._position; a._position.x=350;
    Check(world._collisionScene->Resolve(&a,wallOld,identity,0) && a._position.x<290.1,"swept shape stops at actual LEGO wall skeleton");
    a._position=vec3d(305,0,-300); world._timeStamp+=20;
    Check(world._collisionScene->Resolve(&a,a._position,identity,0) && fabs(a._position.x-300)>=9.8,"initial LEGO wall penetration separated");
    // No cached stale geometry after a building's skeleton changes.
    for(auto &v:wallData.POO) v.x+=100;
    a._position=vec3d(350,0,-300); world._timeStamp+=20;
    world._collisionScene->UpdateActor(&a); const vec3d changedWallOld=a._position; a._position.x=450;
    Check(world._collisionScene->Resolve(&a,changedWallOld,identity,0) && a._position.x<390.1,"changed building geometry invalidates terrain cache");
    world._legoArray[0].CollisionSkelet=nullptr;
    world._collisionScene.reset();
    for(int count : {20,100,200}) {
        TestWorld bench; bench._mapSize=Common::Point(32,32); bench._cells.Resize(bench._mapSize); bench._collisionScene.reset(new Collision::Scene(bench));
        std::vector<std::unique_ptr<Actor<>>> actors;
        for(int i=0;i<count;++i) { actors.emplace_back(new Actor<>); actors.back()->Bind(bench,i+1,vec3d(1200+(i%20)*50,0,-1200-(i/20)*50),cube); }
        auto start=std::chrono::steady_clock::now();
        for(int frame=0;frame<40;++frame) { bench._timeStamp+=16; for(auto &actor:actors) bench._collisionScene->Resolve(actor.get(),actor->_position,identity,0); }
        auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40;
        std::printf("BENCH actors=%d parts=1 milliseconds_per_frame=%.4f\n",count,ms);
        bench._collisionScene.reset();
    }
    if(!pilotProfiles.empty()) for(int count : {20,100,200}) {
        TestWorld bench; bench._mapSize=Common::Point(32,32); bench._cells.Resize(bench._mapSize);
        bench._collisionScene.reset(new Collision::Scene(bench));
        std::vector<std::unique_ptr<Actor<>>> actors;
        for(int i=0;i<count;++i) {
            actors.emplace_back(new Actor<>);
            actors.back()->Bind(bench,i+1,vec3d(1200+(i%20)*180,0,-1200-(i/20)*180),pilotProfiles[i%pilotProfiles.size()]);
        }
        auto start=std::chrono::steady_clock::now();
        for(int frame=0;frame<40;++frame) {
            bench._timeStamp+=16;
            for(auto &actor:actors) {
                const vec3d previous=actor->_position;
                actor->_position.x+=.5;
                bench._collisionScene->Resolve(actor.get(),previous,identity,0);
            }
        }
        auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40;
        std::printf("BENCH pilots=%d profiles=%zu moving milliseconds_per_frame=%.4f\n",count,pilotProfiles.size(),ms);
        bench._collisionScene.reset();
    }
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures); return failures?1:0;
}
