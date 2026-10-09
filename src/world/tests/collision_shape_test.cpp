// Production geometry, parser and actor probes. Run without starting a game.
#include "../collision_shape.h"
#include "../parsers.h"
#include "../saveparsers.h"
#include "../../yw.h"
#include "../../yw_internal.h"
#include "../../env.h"
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
        this->_vp_genesis=nullptr; // Bind bypasses Init; missing visual models must be explicit in the fixture.
        this->_vp_normal=nullptr;
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
struct MovingRobo : Robo {
    void EnergyInteract(update_msg *) override {}
    void AI_layer3(update_msg *arg) override {
        // Exercise native Update/AI movement without mission economy or strategy.
        const double distance = _target_vec.length();
        _target_dir = distance > 0 ? _target_vec / distance : vec3d(0,0,0);
        if (!getBACT_bactCollisions() || !CollisionWithBact(arg->frameTime)) {
            checkCollisions(arg->frameTime * .001);
            AI_doMove(arg);
        }
    }
};
template<class Base> void ProbeMove(std::shared_ptr<Collision::Shape> cube,const char *name,int type=BACT_TYPES_BACT) {
    TestWorld w; w._mapSize=Common::Point(8,8); w._cells.Resize(w._mapSize);
    w._collisionScene.reset(new Collision::Scene(w));
    const vec3d centre(2400,0,-2400);
    Actor<> obstacle; obstacle.Bind(w,1,centre,cube);
    if (type==BACT_TYPES_ROBO) obstacle._bact_type=BACT_TYPES_ROBO;
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
void RoboUpdateRegression(const std::shared_ptr<Collision::Shape> &profile) {
    auto *savedDriver = SFXEngine::SFXe.digDriver;
    if (!savedDriver) SFXEngine::SFXe.digDriver = new waldev();
    for (int mode : {0,1,2}) {
        GroundWorld w; w._vhclProtos.resize(256); w._weaponProtos.resize(256);
        MovingRobo host; host.Bind(w,780,vec3d(3100,-600,-1900),profile);
        host._bact_type=BACT_TYPES_ROBO; host._owner=4;
        host._energy=host._energy_max=1500000;
        host._oflags=BACT_OFLAG_EXACTCOLL|BACT_OFLAG_BACTCOLL;
        host._wrldSize=vec2d(9600,-9600); host._mass=10000; host._force=40000;
        host._airconst=host._airconst_static=200; host._height=350;
        host._roboYPos=-600; host._roboWFlags=0; host._soundcarrier.Resize(17);
        Actor<> intruder; intruder.Bind(w,781,host._position,Shape({Box(vec3d(0,0,0),20,20,20)}));
        intruder._energy=1000;
        if (mode==0) intruder._position.x+=2000;
        if (mode==2) { intruder._host_station=&host; intruder._status=BACT_STATUS_CREATE; }
        double maxStep=0;
        for (int frame=0;frame<3000;++frame) {
            w._timeStamp+=20; update_msg update{};
            update.gTime=w._timeStamp; update.frameTime=20;
            const double oldY=host._position.y;
            host.Update(&update);
            maxStep=std::max(maxStep,fabs(host._position.y-oldY));
        }
        Check(maxStep<.001 && fabs(host._position.y+600)<.001,
              "native Robo Update stays at its hover height over 60 seconds with nearby normal/CREATE units");
        w._collisionScene.reset();
    }
    if (!savedDriver) { delete SFXEngine::SFXe.digDriver; SFXEngine::SFXe.digDriver=nullptr; }
}
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

// Birth-time placement probes for Scene::PlaceAboveTerrain.
// Call after setBaseDir("") and pass the loaded Myko-Scout profile.
void BirthPlacementRegression(const std::shared_ptr<Collision::Shape> &myko)
{
    Check(myko != nullptr, "real Myko-Scout profile loaded for birth placement");
    if (!myko) return;

    const vec3d centre(3000, 0, -3000);
    const mat3x3 identity = mat3x3::Ident();
    const double down = Collision::DownExtent(*myko, identity);

    {
        GroundWorld world;
        Actor<> body;
        body.Bind(world, 801, centre + vec3d(0, 200, 0), myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells.At(0);
        body._overeof = 5;
        Check(world._collisionScene->PlaceAboveTerrain(&body),
              "Myko below floor is raised from the real flat terrain mesh");
        const double expected = -std::max(5.0, down) - myko->tolerance;
        Check(std::fabs(body._position.y - expected) < .01,
              "Myko shape bottom, rather than smaller overeof, sets floor clearance");
        std::printf("BIRTH_GROUND_SHAPE y=%.3f expected=%.3f\n",
                    body._position.y, expected);
        Check(body._old_pos == body._position,
              "explicit birth placement updates the actor previous position");
        const double placedY = body._position.y;
        world._collisionScene->UpdateActor(&body);
        world._collisionScene->UpdateActor(&body);
        Check(std::fabs(body._position.y - placedY) < .001,
              "scene cache updates do not move a placed Myko on later frames");
    }

    {
        GroundWorld world;
        Actor<> body;
        body.Bind(world, 802, centre + vec3d(0, 200, 0), myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells.At(0);
        body._overeof = 60;
        Check(world._collisionScene->PlaceAboveTerrain(&body),
              "Myko below floor is raised when overeof exceeds its shape bottom");
        const double expected = -std::max(60.0, down) - myko->tolerance;
        Check(std::fabs(body._position.y - expected) < .01,
              "larger Myko overeof controls floor clearance");
        std::printf("BIRTH_GROUND_OVEREOF y=%.3f expected=%.3f\n",
                    body._position.y, expected);
    }

    {
        GroundWorld world;
        Actor<> body;
        const vec3d start = centre + vec3d(0, -300, 0);
        body.Bind(world, 803, start, myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells.At(0);
        body._overeof = 20;
        Check(!world._collisionScene->PlaceAboveTerrain(&body) && body._position == start,
              "Myko already above its clearance is left unchanged");
    }

    {
        GroundWorld world;
        Actor<> body;
        const mat3x3 tilt = mat3x3::RotateX(.35);
        body.Bind(world, 804, centre + vec3d(0, 200, 0), myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells.At(0);
        body._rotation = tilt;
        body._overeof = 5;
        const double tiltedDown = Collision::DownExtent(*myko, tilt);
        Check(world._collisionScene->PlaceAboveTerrain(&body),
              "tilted Myko below floor is raised");
        const double expected = -std::max(5.0, tiltedDown) - myko->tolerance;
        Check(std::fabs(body._position.y - expected) < .01,
              "tilted Myko clearance uses its rotated lower extent");
    }

    {
        GroundWorld world;
        world._cells.At(0).height = 120;
        world._legoArray[0].CollisionSkelet = nullptr;
        world._legoArray[0].UseCollisionSkelet = nullptr;
        world._fillerSide = nullptr;
        world._fillerCross = nullptr;
        Actor<> body;
        body.Bind(world, 805, centre + vec3d(0, 200, 0), myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells.At(0);
        body._overeof = 60;
        Check(world._collisionScene->PlaceAboveTerrain(&body),
              "missing terrain mesh uses the actor cell height fallback");
        const double expected = 120 - std::max(60.0, down) - myko->tolerance;
        Check(std::fabs(body._position.y - expected) < .01,
              "fallback height retains Myko bottom clearance");
    }

    {
        GroundWorld world;
        Actor<> zeppelin;
        zeppelin.Bind(world, 806, centre + vec3d(0, 100, 0));
        zeppelin._bact_type = BACT_TYPES_ZEPP;
        zeppelin._pSector = &world._cells.At(0);
        zeppelin._radius = 30;
        zeppelin._overeof = 45;
        Check(world._collisionScene->PlaceAboveTerrain(&zeppelin) &&
              std::fabs(zeppelin._position.y + 45) < .01,
              "legacy ZEPP uses overeof when it has no authored collision shape");
    }

    {
        BuildingWorld world;
        const vec3d origin = centre + vec3d(141, 0, 0);
        Actor<> body;
        body.Bind(world, 807, origin, myko);
        body._bact_type = BACT_TYPES_UFO;
        body._pSector = &world._cells(2, 2);
        body._overeof = 20;

        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        double minZ = std::numeric_limits<double>::infinity();
        double maxZ = -std::numeric_limits<double>::infinity();
        for (const auto &part : myko->parts)
            for (const auto &vertex : part.vertices)
            {
                const vec3d point = body._rotation.Transpose().Transform(vertex);
                minX = std::min(minX, point.x);
                maxX = std::max(maxX, point.x);
                minZ = std::min(minZ, point.z);
                maxZ = std::max(maxZ, point.z);
            }
        const bool centreOutside = origin.x > centre.x + 140;
        const bool footprintOverlaps = origin.x + minX <= centre.x + 140 &&
                                       origin.x + maxX >= centre.x - 140 &&
                                       origin.z + minZ <= centre.z + 140 &&
                                       origin.z + maxZ >= centre.z - 140;
        Check(centreOutside && footprintOverlaps,
              "Myko test origin is outside the roof while its actual footprint crosses the roof edge");
        Check(world._collisionScene->PlaceAboveTerrain(&body),
              "Myko overhanging a building roof edge is raised");
        const double expected = -180 - std::max(20.0, down) - myko->tolerance;
        Check(std::fabs(body._position.y - expected) < .02,
              "roof edge under the Myko footprint contributes its roof height");
        std::printf("BIRTH_ROOF_EDGE center_x=%.3f edge_x=%.3f origin_x=%.3f "
                    "bounds_x=[%.3f,%.3f] y=%.3f expected=%.3f\n",
                    centre.x, centre.x + 140, origin.x, minX, maxX,
                    body._position.y, expected);
    }

    std::printf("BIRTH_PLACEMENT myko_hulls=%zu radius=%.3f down=%.3f tolerance=%.3f\n",
                myko->parts.size(), myko->radius, down, myko->tolerance);
}

struct BirthCommandRobo : Robo
{
    using NC_STACK_yparobo::doUserCommands;
};

void GenesisFactoryBirthPlacementRegression()
{
    GroundWorld world;
    NC_STACK_base visual;
    world._vhclProtos.resize(68);
    world._vhclModels.resize(1, &visual);
    world._weaponProtos.resize(1);
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ypaufo>());

    auto &proto = world._vhclProtos[67];
    proto.model_id = BACT_TYPES_UFO;
    proto.weapon = -1;
    proto.energy = 6500;
    proto.max_active_at_once = 5;
    proto.overeof = 20;
    proto.scale_fx_pXX.fill(0);
    proto.collision_shape = "Models/Collision/Myko-Scout.collision";

    const vec3d start(3000, 200, -3000);
    BirthCommandRobo host;
    host.Bind(world, 808, vec3d(1800, 0, -3000));
    host._bact_type = BACT_TYPES_ROBO;
    host._owner = 1;
    host._roboEnergyLife = 100000;
    host._wrldSize = vec2d(9600, -9600);

    update_msg command{};
    command.user_action = World::DOACTION_ADD_UNIT1;
    command.protoID = 67;
    command.target_point = start;
    host.doUserCommands(&command);

    NC_STACK_ypabact *unit = host.GetKidList().empty() ? nullptr : host.GetKidList().front();
    Check(unit != nullptr, "native Host Station unit-add command creates the Myko");
    if (!unit) return;
    Check(unit->_bact_type == BACT_TYPES_UFO && unit->HasCollisionShape(),
          "native Myko factory binds its real collision profile");
    Check(unit->_status == BACT_STATUS_CREATE && unit->_host_station == &host,
          "native Host Station command keeps the spawned unit in CREATE under its host");
    Check(std::fabs(unit->_position.x - start.x) < .001 &&
          std::fabs(unit->_position.z - start.z) < .001 &&
          std::fabs(unit->_old_pos.x - start.x) < .001 &&
          std::fabs(unit->_old_pos.z - start.z) < .001,
          "terrain placement changes only Y on the factory's requested spawn point");
    Check(unit->_position.y <= -20.0 - unit->_collisionShape->tolerance + .01,
          "native Host Station factory raises a below-floor Myko above shape clearance");
    std::printf("BIRTH_FACTORY start_y=%.3f placed_y=%.3f xz_delta=%.3f,%.3f "
                "tolerance=%.3f\n",
                start.y, unit->_position.y, unit->_position.x-start.x,
                unit->_position.z-start.z, unit->_collisionShape->tolerance);
    const vec3d placed=unit->_position;
    double maxStep=0;
    for (int frame=0;frame<100 && unit->_status==BACT_STATUS_CREATE;++frame) {
        world._timeStamp+=20;
        update_msg update{}; update.frameTime=20; update.gTime=world._timeStamp;
        const vec3d old=unit->_position;
        const mat3x3 rotation=unit->_rotation;
        unit->ResolveShapeMovement(old,rotation);
        unit->CreationTimeUpdate(&update);
        unit->ResolveShapeMovement(old,rotation,20);
        maxStep=std::max(maxStep,(unit->_position-old).length());
    }
    Check(unit->_status==BACT_STATUS_NORMAL && maxStep<.001 &&
          (unit->_position-placed).length()<.001,
          "native CREATE completion retains the safe birth position without vertical corrections");
    unit->Delete();
    world._collisionScene.reset();
}

void RoboFluxGroundRegression()
{
    GroundWorld world;
    auto hostShape = Shape({Box(vec3d(0,0,0), 20, 10, 20)});
    auto gunShape = Shape({Box(vec3d(0,0,0), 6, 10, 6)});
    Robo host;
    const vec3d origin(2400, -40, -2400);
    host.Bind(world, 805, origin, hostShape);
    host._bact_type = BACT_TYPES_ROBO;
    host._roboWFlags = 1;
    host._wrldSize = vec2d(9600, -9600);
    host._mass = 1000; host._airconst = host._airconst_static = 100;
    host._force = 10000; host._roboFlotage = host._mass * 9.80665;
    host._height = 100;

    Actor<NC_STACK_ypagun> gun;
    gun.Bind(world, 806, origin + vec3d(0,35,0), gunShape);
    gun._bact_type = BACT_TYPES_GUN;
    gun._wrldSize = vec2d(9600, -9600);
    World::TRoboGun mount;
    mount.pos = vec3d(0,35,0);
    mount.gun_obj = &gun;
    host._roboGuns.push_back(mount);

    const double clearance = world._collisionScene->RoboGroundPenetration(&host);
    Check(fabs(clearance - (5.0 + gunShape->tolerance)) < .01,
          "flux floor includes the lowered attached gun, not only the Robo hull");
    host._height = 10;
    host._old_pos = origin + vec3d(1,0,0);
    host.checkCollisions(.02f);
    Check(host._target_dir.y < -0.7 && (host._status_flg & BACT_STFLAG_UPWRD),
          "flux height probe begins rising before the lower gun reaches the floor");
    host._height = 100;
    host._old_pos = origin;

    host._fly_dir = vec3d(0,1,0);
    host._fly_dir_length = 5;
    move_msg motion{};
    motion.field_0 = .02f;
    host.Move(&motion);
    update_msg update{};
    update.frameTime = 20;
    host.UpdateUnitGuns(&update);
    Check(fabs(world._collisionScene->RoboGroundPenetration(&host)) < .01 &&
          host._fly_dir.y < 0 && (host._status_flg & BACT_STFLAG_UPWRD),
          "flux Robo bounces upward without allowing its gun through the ground");
    Check(fabs(gun._position.y - (host._position.y + 35.0)) < .01,
          "mounted gun follows the corrected flux position in the same frame");

    host._roboWFlags = 0;
    host._position = origin;
    Check(world._collisionScene->RoboGroundPenetration(&host) > 5.0,
          "ground query is independent of the opt-in flux switch");
    // A shape-less gun still contributes its legacy physical radius.
    gun._collisionShape.reset();
    gun._radius = 12;
    Check(world._collisionScene->RoboGroundPenetration(&host) >= 7,
          "legacy mounted gun radius also constrains the Robo ground clearance");
    host.UpdateUnitGuns(&update);
    Check(host._position == origin,
          "flux floor correction does not change opt-out Host Stations");

    host._roboWFlags = 1;
    host._roboYPos = origin.y;
    update.gTime = 0;
    host.wallow(&update);
    host.UpdateUnitGuns(&update);
    Check(world._collisionScene->RoboGroundPenetration(&host) < .01 &&
          host._roboYPos < origin.y,
          "player-controlled flux wallow also stays above the mounted gun");
    world._collisionScene.reset();
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
        tank.ResolveShapeMovement(tank._position,tank._rotation);
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
        Check((host._position-centre).length()<.001,"ordinary vehicle overlap does not displace the native Robo body");
        tank.ResolveShapeMovement(tank._position,tank._rotation);
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
        wall._bact_type=BACT_TYPES_ROBO;
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
        tank.ResolveShapeMovement(tank._position,tank._rotation);
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
struct GenesisExitSaveParser : ScriptParser::DataHandler, World::Parsers::SaveBact {
    explicit GenesisExitSaveParser(NC_STACK_ypabact &actor) : actor(actor) {}
    int Handle(ScriptParser::Parser &parser, const std::string &key,
               const std::string &value) override {
        if (!StriCmp(key, "end")) return ScriptParser::RESULT_SCOPE_END;
        return SaveBactParser(parser, &actor, key, value)
            ? ScriptParser::RESULT_OK : ScriptParser::RESULT_UNKNOWN;
    }
    bool IsScope(ScriptParser::Parser &, const std::string &word,
                 const std::string &) override {
        return !StriCmp(word, "begin_genesis_probe");
    }
    NC_STACK_ypabact &actor;
};

void GenesisExitSaveParserRegression() {
    auto parse = [](NC_STACK_ypabact &actor, Engine::StringList lines) {
        ScriptParser::HandlersList handlers{new GenesisExitSaveParser(actor)};
        return ScriptParser::ParseStringList(lines, handlers, 0);
    };
    Actor<> restored;
    Check(parse(restored, {"begin_genesis_probe", "genesis_exit_pending = yes", "end"}) &&
          restored._genesisExitPending,
          "save parser restores genesis exit pending=yes");
    Check(parse(restored, {"begin_genesis_probe", "genesis_exit_pending = no", "end"}) &&
          !restored._genesisExitPending,
          "save parser restores genesis exit pending=no");
    Actor<> oldSave;
    Check(parse(oldSave, {"begin_genesis_probe", "mainstate = 1", "end"}) &&
          !oldSave._genesisExitPending,
          "old save without genesis exit field keeps default false");
}

void SetGenesisProbeVisuals(NC_STACK_ypabact &actor, NC_STACK_base &visual) {
    actor._vp_normal = actor._vp_fire = actor._vp_wait = actor._vp_dead =
        actor._vp_megadeth = actor._vp_genesis = &visual;
    actor._soundcarrier.Resize(17);
}

void InitGenesisProbeHost(Robo &host, GroundWorld &w, NC_STACK_base &visual,
                          const vec3d &position,
                          const std::shared_ptr<Collision::Shape> &shape) {
    host.Bind(w, 790, position, shape);
    host._bact_type = BACT_TYPES_ROBO;
    host._status = BACT_STATUS_NORMAL;
    host._status_flg = 0;
    host._oflags = BACT_OFLAG_EXACTCOLL | BACT_OFLAG_BACTCOLL;
    host._mass = 10000; host._force = 40000; host._maxrot = 1;
    host._airconst = host._airconst_static = 200;
    host._height = 250; host._overeof = 150; host._radius = 200;
    host._roboYPos = position.y;
    host._roboDockPos = vec3d(0, -450, 0);
    host._roboWFlags |= 1;
    host._wrldSize = vec2d(9600, -9600);
    host._energy = host._energy_max = 5000000;
    host._rotation = mat3x3::Ident(); host._old_pos = position;
    host._pSector = &w._cells.At(0); host._cellId = Common::Point(0, 0);
    SetGenesisProbeVisuals(host, visual);
    static const std::array<vec3d, 6> centers = {
        vec3d(0,-50,0), vec3d(0,-90,0), vec3d(0,-200,0),
        vec3d(0,-250,0), vec3d(0,-300,0), vec3d(0,-350,0)
    };
    static const std::array<float, 6> radii = {240,100,50,50,50,40};
    auto *coll = host.getBACT_collNodes();
    coll->roboColls.clear();
    for (size_t i = 0; i < centers.size(); ++i) {
        World::TRoboColl sphere;
        sphere.coll_pos = centers[i];
        sphere.field_10 = position + centers[i];
        sphere.robo_coll_radius = radii[i];
        coll->roboColls.push_back(sphere);
    }
    coll->field_0 = 0;
}

void SetGenesisProbeUnit(AITank &tank, Robo &host, NC_STACK_base &visual,
                         int commandId, double timerMs) {
    tank._host_station = &host;
    tank._isGenesisProduced = true;
    tank._energy = tank._energy_max = 1000;
    tank._status_flg &= ~(BACT_STFLAG_SCALE | BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2);
    SetGenesisProbeVisuals(tank, visual);
    setState_msg state{}; state.newStatus = BACT_STATUS_CREATE;
    tank.SetState(&state);
    tank._scale_time = timerMs;
    tank._commandID = commandId;
}

bool GenesisFootprintOverlap(const NC_STACK_ypabact &a, const Collision::Shape &as,
                             const NC_STACK_ypabact &b, const Collision::Shape &bs) {
    double ax = 0, az = 0, bx = 0, bz = 0;
    for (const auto &part : as.parts) for (const auto &v : part.vertices) {
        ax = std::max(ax, std::fabs(v.x)); az = std::max(az, std::fabs(v.z));
    }
    for (const auto &part : bs.parts) for (const auto &v : part.vertices) {
        bx = std::max(bx, std::fabs(v.x)); bz = std::max(bz, std::fabs(v.z));
    }
    return std::fabs(a._position.x - b._position.x) <= ax + bx &&
           std::fabs(a._position.z - b._position.z) <= az + bz;
}

void TickGenesisProbeHost(Robo &host, GroundWorld &w, int dt) {
    const vec3d old = host._position;
    const mat3x3 oldRotation = host._rotation;
    update_msg update{};
    update.frameTime = dt; update.gTime = w._timeStamp;
    host.ResolveShapeMovement(old, oldRotation);
    host.checkCollisions(dt * 0.001f);
    host.wallow(&update);
    host.ResolveShapeMovement(old, oldRotation, dt);
    host.UpdateUnitGuns(&update);
}

void GenesisHostExitCase(const std::shared_ptr<Collision::Shape> &hostShape,
                         const std::shared_ptr<Collision::Shape> &tankShape,
                         bool hostFirst, bool commanderParent) {
    GroundWorld w;
    const int dt = 20;
    const vec3d hostStart(2400, -250, -2400);
    const vec3d tankStart = hostStart + vec3d(0, 100, 0);
    const vec3d target = hostStart + vec3d(1600, 0, 0);
    NC_STACK_base visual;
    Robo host;
    InitGenesisProbeHost(host, w, visual, hostStart, hostShape);
    AITank tank;
    PrepareAITank(tank, w, 791, tankStart, target, tankShape);
    tank._pSector = &w._cells.At(0); tank._cellId = Common::Point(0, 0);
    tank._energy = 1000; tank._status_flg = BACT_STFLAG_LAND; tank._old_pos = tankStart;
    SetGenesisProbeVisuals(tank, visual);
    Actor<> commander;
    if (commanderParent) {
        commander.Bind(w, 792, vec3d(7200, -13, -7200));
        commander._wrldSize = vec2d(9600, -9600);
        commander._pSector = &w._cells.At(0); commander._cellId = Common::Point(0, 0);
        host.AddSubject(&commander); commander.AddSubject(&tank);
    } else {
        host.AddSubject(&tank);
    }
    SetGenesisProbeUnit(tank, host, visual, commanderParent ? 792 : 790, 100.0);
    w._collisionScene->UpdateActor(&host); w._collisionScene->UpdateActor(&tank);

    Collision::Contact geometric;
    const bool initialOverlap = Collision::ContactShapes(*hostShape, host.GetBodyPosition(), host._rotation,
        *tankShape, tank.GetBodyPosition(), tank._rotation, &geometric);
    Check(initialOverlap && tank._host_station == &host,
          "Genesis Tank begins geometrically overlapping its exact Host Station pointer");

    int createFrames = 0, releaseFrame = -1;
    double maxCreateStep = 0, maxHostCorrection = 0, maxHostStep = 0;
    vec3d previousTank = tank._position, previousHost = host._position;
    auto step = [&](bool advanceWorld) {
        if (advanceWorld) w._timeStamp += dt;
        TickGenesisProbeHost(host, w, dt);
        tank._target_vec = target - tank._position;
        StepAITank(tank, w, dt, false);
    };
    for (int frame = 0; frame < 8 && tank._status == BACT_STATUS_CREATE; ++frame) {
        if (hostFirst) step(true);
        else {
            tank._target_vec = target - tank._position;
            StepAITank(tank, w, dt, true);
            TickGenesisProbeHost(host, w, dt);
        }
        ++createFrames;
        maxCreateStep = std::max(maxCreateStep, (tank._position - previousTank).length());
        maxHostStep = std::max(maxHostStep, std::fabs(host._position.y - previousHost.y));
        const double wantedY = host._roboYPos + std::sin(w._timeStamp * C_PI / 3000.0) * 25.0;
        maxHostCorrection = std::max(maxHostCorrection, std::fabs(host._position.y - wantedY));
        previousTank = tank._position; previousHost = host._position;
        if (tank._status != BACT_STATUS_CREATE) releaseFrame = frame;
    }
    const bool releasedInsidePending = releaseFrame >= 0 &&
        GenesisFootprintOverlap(host, *hostShape, tank, *tankShape) && tank._genesisExitPending;
    int exitFrames = 0;
    for (; exitFrames < 600 && tank._genesisExitPending; ++exitFrames) {
        if (hostFirst) step(true);
        else {
            tank._target_vec = target - tank._position;
            StepAITank(tank, w, dt, true);
            TickGenesisProbeHost(host, w, dt);
        }
    }
    const bool outside = !GenesisFootprintOverlap(host, *hostShape, tank, *tankShape);
    Check(createFrames == 5 && releaseFrame == 4 && releasedInsidePending,
          "Genesis timer expires normally while own-host overlap remains temporarily non-solid");
    Check(maxCreateStep < 20.0,
          "Genesis Tank has no large relocation under either native update order");
    Check(maxHostCorrection < 1.0 && maxHostStep < 1.0,
          "Genesis overlap does not kick the Host Station vertically");
    Check(exitFrames < 600 && !tank._genesisExitPending && outside,
          "native Tank AI clears own-host grace after leaving the XZ footprint");

    if (!tank._genesisExitPending && outside) {
        tank._position = host._position; tank._old_pos = tank._position;
        w._timeStamp += dt; w._collisionScene->UpdateActor(&tank);
        Collision::Contact before, after;
        const bool ownHit = w._collisionScene->PairContact(&tank, &host, &before);
        const vec3d ownStart = tank._position;
        w._collisionScene->Resolve(&tank, ownStart, tank._rotation, dt);
        const bool ownStuck = w._collisionScene->PairContact(&tank, &host, &after) &&
                              after.depth > tankShape->tolerance + .01;
        Check(ownHit && (tank._position - ownStart).length() > 1.0 && !ownStuck,
              "re-entry into the former Host Station is solid after grace ends");

        Robo foreignHost;
        InitGenesisProbeHost(foreignHost, w, visual, tank._position, hostShape);
        w._collisionScene->UpdateActor(&foreignHost);
        const bool foreignHit = w._collisionScene->PairContact(&tank, &foreignHost, &before);
        const vec3d foreignStart = tank._position;
        w._collisionScene->Resolve(&tank, foreignStart, tank._rotation, dt);
        const bool foreignStuck = w._collisionScene->PairContact(&tank, &foreignHost, &after) &&
                                  after.depth > tankShape->tolerance + .01;
        Check(foreignHit && (tank._position - foreignStart).length() > 1.0 && !foreignStuck,
              "foreign Host Station remains solid after own-host grace ends");
        w._collisionScene->Forget(&foreignHost);
    }
    w._collisionScene.reset();
}

void GenesisForeignHostDuringGraceRegression(
    const std::shared_ptr<Collision::Shape> &hostShape,
    const std::shared_ptr<Collision::Shape> &tankShape) {
    GroundWorld w;
    const vec3d hostStart(2400, -250, -2400);
    const vec3d tankStart = hostStart + vec3d(0, 100, 0);
    NC_STACK_base visual;
    Robo host, foreignHost;
    InitGenesisProbeHost(host, w, visual, hostStart, hostShape);
    AITank tank;
    PrepareAITank(tank, w, 794, tankStart, hostStart + vec3d(1600, 0, 0), tankShape);
    tank._pSector = &w._cells.At(0); tank._cellId = Common::Point(0, 0);
    SetGenesisProbeUnit(tank, host, visual, 790, 100.0);
    host.AddSubject(&tank);
    w._collisionScene->UpdateActor(&host); w._collisionScene->UpdateActor(&tank);
    update_msg update{}; update.frameTime = 20; update.gTime = 20;
    tank.CreationTimeUpdate(&update);
    InitGenesisProbeHost(foreignHost, w, visual, tank._position, hostShape);
    w._collisionScene->UpdateActor(&foreignHost);
    Collision::Contact own, foreign;
    const bool ownHit = w._collisionScene->PairContact(&tank, &host, &own);
    const bool foreignHit = w._collisionScene->PairContact(&tank, &foreignHost, &foreign);
    Check(tank._status == BACT_STATUS_CREATE && tank._genesisExitPending && !ownHit &&
          foreignHit && foreign.depth > tankShape->tolerance,
          "Genesis grace excludes its own host but leaves a foreign host solid");
    w._collisionScene.reset();
}

void GenesisAboveHostDockRegression(const std::shared_ptr<Collision::Shape> &hostShape,
                                    const std::shared_ptr<Collision::Shape> &tankShape) {
    GroundWorld w;
    const int dt = 20;
    const vec3d hostStart(2400, -250, -2400);
    // Y increases downward; the native Resistance 56 dock offset is -450.
    const vec3d dock = hostStart + vec3d(0, -450, 0);
    const vec3d target = hostStart + vec3d(1600, 0, 0);
    NC_STACK_base visual;
    Robo host;
    InitGenesisProbeHost(host, w, visual, hostStart, hostShape);
    AITank tank;
    PrepareAITank(tank, w, 793, dock, target, tankShape);
    tank._pSector = &w._cells.At(0); tank._cellId = Common::Point(0, 0);
    tank._status_flg = 0;
    SetGenesisProbeUnit(tank, host, visual, 790, 100.0);
    host.AddSubject(&tank);
    w._collisionScene->UpdateActor(&host); w._collisionScene->UpdateActor(&tank);

    Collision::Contact contact;
    const bool shapeContact = Collision::ContactShapes(*hostShape, host.GetBodyPosition(), host._rotation,
        *tankShape, tank.GetBodyPosition(), tank._rotation, &contact);
    const bool footprint = GenesisFootprintOverlap(host, *hostShape, tank, *tankShape);
    Check(!shapeContact && footprint,
          "above-host dock starts 3D clear while projected XZ footprints overlap");
    int releaseFrame = -1;
    for (int frame = 0; frame < 8 && tank._status == BACT_STATUS_CREATE; ++frame) {
        w._timeStamp += dt;
        TickGenesisProbeHost(host, w, dt);
        tank._target_vec = target - tank._position;
        StepAITank(tank, w, dt, false);
        if (tank._status != BACT_STATUS_CREATE) releaseFrame = frame;
    }
    Collision::Contact atRelease;
    const bool releaseContact = Collision::ContactShapes(*hostShape, host.GetBodyPosition(), host._rotation,
        *tankShape, tank.GetBodyPosition(), tank._rotation, &atRelease);
    Check(releaseFrame == 4 && tank._genesisExitPending &&
          GenesisFootprintOverlap(host, *hostShape, tank, *tankShape) && !releaseContact,
          "CREATE timer ends at its normal duration while docked unit keeps exit grace");
    for (int frame = 0; frame < 160; ++frame) {
        w._timeStamp += dt;
        TickGenesisProbeHost(host, w, dt);
        tank._target_vec = target - tank._position;
        StepAITank(tank, w, dt, false);
    }
    Check(GenesisFootprintOverlap(host, *hostShape, tank, *tankShape) &&
          tank._genesisExitPending,
          "vertical dock separation alone does not clear own-host exit grace");
    for (int frame = 0; frame < 600 && tank._genesisExitPending; ++frame) {
        w._timeStamp += dt;
        TickGenesisProbeHost(host, w, dt);
        tank._target_vec = target - tank._position;
        StepAITank(tank, w, dt, false);
    }
    Check(!tank._genesisExitPending &&
          !GenesisFootprintOverlap(host, *hostShape, tank, *tankShape),
          "unit created at the native above-host dock lands and exits the host footprint");
    w._collisionScene.reset();
}

void GenesisHostExitRegression(const std::shared_ptr<Collision::Shape> &hostShape,
                               const std::shared_ptr<Collision::Shape> &tankShape) {
    for (bool hostFirst : {true, false})
        for (bool commanderParent : {false, true})
            GenesisHostExitCase(hostShape, tankShape, hostFirst, commanderParent);
    GenesisForeignHostDuringGraceRegression(hostShape, tankShape);
    GenesisAboveHostDockRegression(hostShape, tankShape);
}

struct GenesisModelSnapshot
{
    std::vector<vec3d> vertices;
    double minY = std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
};

double GenesisPeakScale()
{
    return NC_STACK_ypabact::GenesisScaleBase +
           NC_STACK_ypabact::GenesisScaleCurve / NC_STACK_ypabact::GenesisScaleCurve;
}

NC_STACK_base *GenesisSet1Model(NC_STACK_base *vpList, int id)
{
    if (!vpList || id < 0 || static_cast<size_t>(id) >= vpList->GetKidList().size())
        return nullptr;
    auto it = vpList->GetKidList().begin();
    std::advance(it, id);
    return *it;
}

void GenesisModelVertices(NC_STACK_base *node, const mat3x3 &parentBasis,
                          const vec3d &parentOffset, GenesisModelSnapshot &out)
{
    if (!node) return;
    const mat3x3 basis = parentBasis * node->TForm().SclRot;
    const vec3d offset = parentBasis.Transform(node->TForm().Pos) + parentOffset;
    if (auto *skeleton = node->GetSkeleton())
        if (auto *data = skeleton->GetSkelet())
            for (const auto &vertex : data->POO)
            {
                const vec3d point = basis.Transform(vertex) + offset;
                out.vertices.push_back(point);
                out.minY = std::min(out.minY, point.y);
                out.maxY = std::max(out.maxY, point.y);
            }
    for (auto *child : node->GetKidList())
        GenesisModelVertices(child, basis, offset, out);
}

double GenesisVisibleMaxY(const NC_STACK_ypabact &actor,
                          const GenesisModelSnapshot &model,
                          double scaleOverride = -1.0)
{
    const vec3d scale = scaleOverride >= 0.0 ? vec3d(scaleOverride) : actor._scale;
    const mat3x3 renderBasis = actor._rotation.Transpose() * mat3x3::Scale(scale);
    const mat3x3 visualScale = mat3x3::Scale(actor._vp_scale);
    double maximum = -std::numeric_limits<double>::infinity();
    for (const auto &vertex : model.vertices)
        maximum = std::max(maximum, (renderBasis.Transform(visualScale.Transform(vertex))).y);
    return actor.GetBodyPosition().y + maximum;
}

double GenesisTFormMaxY(const NC_STACK_ypabact &actor,
                        const GenesisModelSnapshot &model)
{
    const mat3x3 visualScale = mat3x3::Scale(actor._vp_scale);
    double maximum = -std::numeric_limits<double>::infinity();
    for (const auto &vertex : model.vertices)
        maximum = std::max(maximum,
            (actor._tForm.SclRot.Transform(visualScale.Transform(vertex)) + actor._tForm.Pos).y);
    return maximum;
}

struct GenesisProbeUfo : Actor<NC_STACK_ypaufo>
{
    bool moveDownDuringAI = false;
    void AI_layer1(update_msg *arg) override
    {
        if (moveDownDuringAI)
        {
            _position.y += 200.0;
            moveDownDuringAI = false;
        }
        NC_STACK_ypabact::AI_layer1(arg);
    }
};

struct GenesisProbeRobo : Robo
{
    using NC_STACK_yparobo::InitForce;
};

void GenesisBindUfo(GenesisProbeUfo &actor, GroundWorld &world, int gid,
                    const vec3d &position,
                    const std::shared_ptr<Collision::Shape> &shape,
                    NC_STACK_base *genesis, NC_STACK_base *normal,
                    float energy, float overeof)
{
    actor.Bind(world, gid, position, shape);
    actor._bact_type = BACT_TYPES_UFO;
    actor._pSector = &world._cells.At(0);
    actor._cellId = Common::Point(0, 0);
    actor._wrldSize = vec2d(9600, -9600);
    actor._mass = 400.0f;
    actor._energy = actor._energy_max = energy;
    actor._force = actor._base_force = 27000.0f;
    actor._height = 500.0f;
    actor._overeof = overeof;
    actor._radius = shape ? shape->radius : 30.0f;
    actor._scale = vec3d(1.0);
    actor._vp_scale = vec3d(1.0);
    actor._vp_spin_strength = vec3d(0.0);
    actor._vp_genesis = genesis;
    actor._vp_normal = normal;
}

void GenesisUpdateFrame(NC_STACK_ypabact &actor, GroundWorld &world, int frameTime)
{
    world._timeStamp += frameTime;
    update_msg update{};
    update.gTime = world._timeStamp;
    update.frameTime = frameTime;
    actor.Update(&update);
}

void GenesisFullAnimationProbe(const char *label,
                               const std::shared_ptr<Collision::Shape> &shape,
                               NC_STACK_base *genesis, NC_STACK_base *normal,
                               const GenesisModelSnapshot &genesisModel,
                               const GenesisModelSnapshot &normalModel,
                               float energy, float overeof, int expectedFrames)
{
    Check(!genesisModel.vertices.empty() && !normalModel.vertices.empty(),
          "Set1 Genesis and normal VP contain real skeleton vertices");
    if (genesisModel.vertices.empty() || normalModel.vertices.empty()) return;

    auto *savedDriver = SFXEngine::SFXe.digDriver;
    if (!savedDriver) SFXEngine::SFXe.digDriver = new waldev();

    GroundWorld world;
    world._vhclProtos.resize(256);
    world._weaponProtos.resize(256);
    GenesisProbeUfo actor;
    const vec3d start(3000, 200, -3000);
    GenesisBindUfo(actor, world, 920, start, shape, genesis, normal, energy, overeof);

    setState_msg create{};
    create.newStatus = BACT_STATUS_CREATE;
    actor.SetState(&create);
    actor._scale_time = energy * 0.2f;
    Check(actor._status == BACT_STATUS_CREATE,
          "real-asset unit enters native CREATE through SetState");
    const double expectedClearance = std::max({double(overeof),
        shape ? Collision::DownExtent(*shape, mat3x3::Ident()) : 0.0,
        genesisModel.maxY * GenesisPeakScale()}) +
        (shape ? shape->tolerance : 0.0);
    Check(std::fabs(actor._position.y + expectedClearance) < 0.05,
          "SetState birth placement includes the real Genesis max-scale support");
    const double firstVisualMaxY = GenesisTFormMaxY(actor, genesisModel);
    Check((actor._tForm.Pos - actor._position).length() < 0.01 &&
          (actor._tForm.Pos - actor.GetBodyPosition()).length() < 0.01,
          "SetState synchronizes the render origin to the placed actor position");
    Check(firstVisualMaxY <= 0.05,
          "real Genesis vertices are above the floor in the first pose before Update");
    std::printf("GENESIS_SETSTATE label=%s body_y=%.4f expected=%.4f create_ms=%.1f vp_y=[%.3f,%.3f] shape=%d\n",
                label, actor._position.y, -expectedClearance, double(actor._scale_time),
                genesisModel.minY, genesisModel.maxY, shape ? 1 : 0);

    // A loaded CREATE has no serialized ground-check bit. Recreate that state
    // after SetState, then let the real actor Update execute its first frame.
    actor._position.y = start.y;
    actor._old_pos = actor._position;
    actor._genesisGroundChecked = false;
    GenesisUpdateFrame(actor, world, 20);
    Check(actor._genesisGroundChecked && actor._position.y < 0.0,
          "first native Update checks a loaded CREATE before it can sink");
    Check((actor._tForm.Pos - actor.GetBodyPosition()).length() < 0.01 &&
          GenesisTFormMaxY(actor, genesisModel) <= 0.05,
          "first native Update renders the corrected Genesis pose above the floor");
    const auto visibleMaxY = [&](const GenesisModelSnapshot &visible) {
        const double renderScale = (actor._status_flg & BACT_STFLAG_SCALE) ? -1.0 : 1.0;
        return GenesisVisibleMaxY(actor, visible, renderScale);
    };
    double maxDepth = visibleMaxY(genesisModel);
    double maxStep = 0.0;
    int frames = 1;
    for (; frames < expectedFrames + 8 && actor._status == BACT_STATUS_CREATE; ++frames)
    {
        const vec3d old = actor._position;
        GenesisUpdateFrame(actor, world, 20);
        maxStep = std::max(maxStep, (actor._position - old).length());
        const auto &visible = (actor._status_flg & BACT_STFLAG_SCALE) ? genesisModel : normalModel;
        maxDepth = std::max(maxDepth, visibleMaxY(visible));
    }
    Check(actor._status == BACT_STATUS_NORMAL && frames >= expectedFrames - 1 &&
          frames <= expectedFrames + 2,
          "real Genesis animation finishes on the native energy-scaled timer");
    Check(maxDepth <= 0.05,
          "every sampled real Genesis skeleton vertex stays above the ground during CREATE");
    Check(maxStep < 0.05,
          "real CREATE rotation does not produce repeated vertical jumps");
    std::printf("GENESIS_ANIMATION label=%s frames=%d max_visible_y=%.5f max_step=%.5f end_y=%.4f scale=%.4f\n",
                label, frames, maxDepth, maxStep, actor._position.y, actor._scale.y);

    // Force a downward movement inside the same native Update after its start
    // pose is captured; the post-AI CREATE check must restore the floor pose.
    GroundWorld movedWorld;
    movedWorld._vhclProtos.resize(256);
    movedWorld._weaponProtos.resize(256);
    GenesisProbeUfo moved;
    GenesisBindUfo(moved, movedWorld, 921, start, shape, genesis, normal, energy, overeof);
    moved._scale_time = energy * 0.2f;
    moved.SetState(&create);
    moved._scale_time = energy * 0.2f;
    const double safeY = moved._position.y;
    moved._genesisGroundChecked = true;
    moved.moveDownDuringAI = true;
    GenesisUpdateFrame(moved, movedWorld, 20);
    Check(moved._genesisGroundChecked && std::fabs(moved._position.y - safeY) < 0.05,
          "native Update rechecks terrain after an external position change during CREATE");
    std::printf("GENESIS_MOVE label=%s safe_y=%.4f after_y=%.4f\n",
                label, safeY, moved._position.y);

    if (!savedDriver) SFXEngine::SFXe.digDriver = nullptr;
    world._collisionScene.reset();
    movedWorld._collisionScene.reset();
}

void GenesisTarantulFactoryGrid(NC_STACK_base *vpList,
                                const std::shared_ptr<Collision::Shape> &mykoShape,
                                const std::shared_ptr<Collision::Shape> &turantulShape,
                                const GenesisModelSnapshot &mykoGenesis)
{
    if (!mykoShape || !turantulShape) return;
    GroundWorld world;
    world._vhclProtos.resize(68);
    world._weaponProtos.resize(256);
    for (auto *model : vpList->GetKidList()) world._vhclModels.push_back(model);
    auto &myko = world._vhclProtos[67];
    myko.model_id = BACT_TYPES_UFO;
    myko.weapon = -1;
    myko.energy = 6500;
    myko.production_cost = 140;
    myko.max_active_at_once = 5;
    myko.mass = 400.0f;
    myko.force = 27000.0f;
    myko.maxrot = 1.8f;
    myko.airconst = 120.0f;
    myko.height = 500.0f;
    myko.radius = 30.0f;
    myko.overeof = 20.0f;
    myko.vp_normal = 200;
    myko.vp_genesis = 252;
    myko.collision_shape = "Data/Models/Collision/Myko-Scout.collision";
    myko.scale_fx_pXX.fill(0);

    GenesisProbeRobo host;
    const vec3d hostBase(1800, 0, -3000);
    host.Bind(world, 930, hostBase, turantulShape);
    host._bact_type = BACT_TYPES_ROBO;
    host._owner = 1;
    host._pSector = &world._cells.At(0);
    host._wrldSize = vec2d(9600, -9600);
    host._mass = 5000.0f;
    host._force = 10000.0f;
    host._maxrot = 2.0f;
    host._airconst = host._airconst_static = 100.0f;
    host._height = 200.0f;
    host._overeof = 50.0f;
    host._roboWFlags = 3; // Turantul_I: robo_does_twist + robo_does_flux.
    host._roboState = 0;
    host._roboDockPos = vec3d(0, 40, 0);
    host._roboDockEnerg = 100000;
    host._roboDockCnt = 0;
    host._energy = host._energy_max = 5000000;
    host._vp_normal = GenesisSet1Model(vpList, 116);
    host._vp_genesis = GenesisSet1Model(vpList, 121);

    GenesisProbeUfo commander;
    commander.Bind(world, 939, hostBase, nullptr);
    commander._vehicleID = 67;
    commander._commandID = 939;
    commander._owner = 1;
    commander._bact_type = BACT_TYPES_UFO;
    commander._energy = commander._energy_max = 6500;
    const double childClearance = std::max({20.0,
        Collision::DownExtent(*mykoShape, mat3x3::Ident()),
        mykoGenesis.maxY * GenesisPeakScale()}) +
        mykoShape->tolerance;
    int sample = 0;
    for (double hostY : {-100.0, 0.0, 120.0, 300.0})
    {
        host._position = vec3d(hostBase.x, hostY, hostBase.z);
        host._old_pos = host._position;
        host._roboDockTime = 0;
        host._roboDockEnerg = 100000;
        host._energy = host._energy_max = 5000000;
        host.InitForce(&commander);
        NC_STACK_ypabact *unit = commander.GetKidList().empty() ? nullptr : commander.GetKidList().back();
        Check(unit && unit->_host_station == &host && unit->_status == BACT_STATUS_CREATE,
              "native Tarantul InitForce creates a Myko at the configured dock point");
        if (unit)
        {
            Check(unit->HasCollisionShape() &&
                  std::fabs(unit->_position.x - hostBase.x) < 0.01 &&
                  std::fabs(unit->_position.z - hostBase.z) < 0.01,
                  "Tarantul production uses the real Myko hull and preserves dock XZ");
            Check(unit->_position.y <= -childClearance + 0.05,
                  "Tarantul dock birth is raised above the floor for every sampled host height");
            Check((unit->_tForm.Pos - unit->_position).length() < 0.01 &&
                  (unit->_tForm.Pos - unit->GetBodyPosition()).length() < 0.01 &&
                  GenesisTFormMaxY(*unit, mykoGenesis) <= 0.05,
                  "native InitForce factory returns with safe Genesis model transform");
            std::printf("TARANTUL_DOCK sample=%d host_y=%.1f dock_y=40 spawn_y=%.1f placed_y=%.4f clearance=%.4f\n",
                        sample, hostY, hostY + 40.0, unit->_position.y, childClearance);
            unit->Delete();
        }
        ++sample;
    }
    world._collisionScene.reset();
}

void GenesisCurrentSectorHeightProbe(const std::shared_ptr<Collision::Shape> &mykoShape,
                                     NC_STACK_base *mykoGenesis,
                                     const GenesisModelSnapshot &mykoModel)
{
    if (!mykoShape || !mykoGenesis) return;
    GroundWorld world;
    world._cells.At(0).height = 120.0f;
    world._cells(1, 1).height = 700.0f;
    world._legoArray[0].CollisionSkelet = nullptr;
    world._legoArray[0].UseCollisionSkelet = nullptr;
    world._fillerSide = nullptr;
    world._fillerCross = nullptr;
    world._collisionScene.reset();

    Actor<> actor;
    actor.Bind(world, 950, vec3d(3000, 500, -3000), mykoShape);
    actor._bact_type = BACT_TYPES_UFO;
    actor._status_flg = 0;
    actor._position = actor._old_pos = vec3d(3000, 500, -3000);
    actor._pSector = &world._cells(1, 1); // Deliberately stale relative to GetSectorInfo.
    actor._collisionShape = mykoShape;
    actor._overeof = 20.0f;
    actor._vp_genesis = mykoGenesis;
    actor._vp_scale = vec3d(1.0);
    actor.PlaceGenesisAboveTerrain();

    const double clearance = std::max({20.0,
        Collision::DownExtent(*mykoShape, mat3x3::Ident()),
        mykoModel.maxY * GenesisPeakScale()}) + mykoShape->tolerance;
    const double expected = 120.0 - clearance;
    Check(std::fabs(actor._position.y - expected) < 0.05 &&
          std::fabs(actor._position.y - (700.0 - clearance)) > 100.0,
          "terrain placement reads current-sector height instead of stale actor sector");
    Check((actor._tForm.Pos - actor.GetBodyPosition()).length() < 0.01,
          "current-sector terrain placement synchronizes first render origin");
    std::printf("GENESIS_CURRENT_SECTOR cell_height=120 stale_height=700 placed_y=%.4f expected=%.4f\n",
                actor._position.y, expected);
}

struct GenesisContactProbeUfo : GenesisProbeUfo
{
    vec3d positionAtAI;
    void AI_layer1(update_msg *arg) override
    {
        positionAtAI = _position;
        GenesisProbeUfo::AI_layer1(arg);
    }
};

void GenesisRoofPostContactProbe(const std::shared_ptr<Collision::Shape> &ghorShape,
                                 NC_STACK_base *ghorGenesis,
                                 NC_STACK_base *ghorNormal,
                                 const GenesisModelSnapshot &ghorModel)
{
    if (!ghorShape || !ghorGenesis) return;
    auto *savedDriver = SFXEngine::SFXe.digDriver;
    if (!savedDriver) SFXEngine::SFXe.digDriver = new waldev();

    BuildingWorld world;
    world._vhclProtos.resize(256);
    world._weaponProtos.resize(256);
    double genesisRadius = 0.0;
    for (const auto &vertex : ghorModel.vertices)
        genesisRadius = std::max(genesisRadius, vertex.length() * GenesisPeakScale());
    const double startX = std::max(3200.0, 3140.0 + genesisRadius + 10.0);
    GenesisContactProbeUfo ghor;
    const vec3d start(startX, 200, -3000);
    GenesisBindUfo(ghor, world, 960, start, ghorShape, ghorGenesis,
                   ghorNormal, 7000, 36);
    ghor._oflags |= BACT_OFLAG_EXACTCOLL;
    setState_msg create{};
    create.newStatus = BACT_STATUS_CREATE;
    ghor.SetState(&create);
    ghor._scale_time = 1400;
    const double clearance = std::max({36.0,
        Collision::DownExtent(*ghorShape, mat3x3::Ident()),
        ghorModel.maxY * GenesisPeakScale()}) + ghorShape->tolerance;
    const double spawnY = ghor._position.y;

    // A normal unit-sized box immediately to the Ghor's right overlaps its
    // real collision hull and pushes it toward the roof during initial resolve.
    auto blockerShape = Shape({Box(vec3d(0), 25, 25, 25)});
    Actor<> blocker;
    blocker.Bind(world, 961, vec3d(startX + 40.0, ghor._position.y, start.z), blockerShape);
    blocker._oflags |= BACT_OFLAG_EXACTCOLL;
    blocker._mass = 400.0f;

    GenesisUpdateFrame(ghor, world, 20);
    const double roofY = -180.0; // BuildingWorld's top face is at y=-180.
    const double aiX = ghor.positionAtAI.x;
    const double finalVisibleY = GenesisTFormMaxY(ghor, ghorModel);
    std::printf("GENESIS_ROOF start_x=%.3f genesis_radius=%.3f spawn_y=%.4f ai_x=%.3f final_x=%.3f final_y=%.4f roof_y=%.1f visible_max_y=%.4f blocker_x=%.3f\n",
                startX, genesisRadius, spawnY, aiX, ghor._position.x,
                ghor._position.y, roofY, finalVisibleY, blocker._position.x);
    Check(std::fabs(spawnY + clearance) < 0.05,
          "Ghor begins above flat ground with its full Genesis footprint outside the roof");
    Check(aiX < 3140.0 + genesisRadius && aiX < startX - 5.0,
          "normal blocker pushes the Ghor Genesis footprint onto the roof during initial resolve");
    Check(ghor._position.y < roofY - clearance + 0.05 && finalVisibleY <= roofY + 0.05,
          "final post-contact placement raises the Ghor above the newly covered building roof");

    world._collisionScene.reset();
    if (!savedDriver) { delete SFXEngine::SFXe.digDriver; SFXEngine::SFXe.digDriver = nullptr; }
}

void GenesisRealVisualTerrainRegression(NC_STACK_base *vpList)
{
    GenesisModelSnapshot ghorGenesis, ghorNormal, mykoGenesis, mykoNormal;
    GenesisModelVertices(GenesisSet1Model(vpList, 199), mat3x3::Ident(), vec3d(0), ghorGenesis);
    GenesisModelVertices(GenesisSet1Model(vpList, 197), mat3x3::Ident(), vec3d(0), ghorNormal);
    GenesisModelVertices(GenesisSet1Model(vpList, 252), mat3x3::Ident(), vec3d(0), mykoGenesis);
    GenesisModelVertices(GenesisSet1Model(vpList, 200), mat3x3::Ident(), vec3d(0), mykoNormal);
    std::printf("GENESIS_ASSETS ghor=199 vertices=%zu y=[%.3f,%.3f] normal=197 vertices=%zu y=[%.3f,%.3f] myko=252 vertices=%zu y=[%.3f,%.3f] normal=200 vertices=%zu y=[%.3f,%.3f]\n",
                ghorGenesis.vertices.size(), ghorGenesis.minY, ghorGenesis.maxY,
                ghorNormal.vertices.size(), ghorNormal.minY, ghorNormal.maxY,
                mykoGenesis.vertices.size(), mykoGenesis.minY, mykoGenesis.maxY,
                mykoNormal.vertices.size(), mykoNormal.minY, mykoNormal.maxY);
    std::string error;
    auto ghor = Collision::Load("Data/Models/Collision/Ghor-Scout.collision", &error);
    Check(ghor != nullptr, error.empty() ? "Ghor-Scout real collision profile" : error.c_str());
    error.clear();
    auto myko = Collision::Load("Data/Models/Collision/Myko-Scout.collision", &error);
    Check(myko != nullptr, error.empty() ? "Myko-Scout real collision profile" : error.c_str());
    error.clear();
    auto turantul = Collision::Load("Data/Models/Collision/Turantul_I.collision", &error);
    Check(turantul != nullptr, error.empty() ? "Turantul_I real collision profile" : error.c_str());
    if (ghor)
        GenesisFullAnimationProbe("Ghor-Scout", ghor, GenesisSet1Model(vpList, 199),
                                  GenesisSet1Model(vpList, 197), ghorGenesis,
                                  ghorNormal, 7000, 36, 70);
    if (myko)
        GenesisFullAnimationProbe("Myko-Scout", myko, GenesisSet1Model(vpList, 252),
                                  GenesisSet1Model(vpList, 200), mykoGenesis,
                                  mykoNormal, 6500, 20, 65);
    if (ghor)
        GenesisFullAnimationProbe("Ghor-Scout legacy-no-shape", {},
                                  GenesisSet1Model(vpList, 199), GenesisSet1Model(vpList, 197),
                                  ghorGenesis, ghorNormal, 7000, 36, 70);
    if (myko && turantul) GenesisTarantulFactoryGrid(vpList, myko, turantul, mykoGenesis);
    if (myko) GenesisCurrentSectorHeightProbe(myko, GenesisSet1Model(vpList, 252), mykoGenesis);
    if (ghor) GenesisRoofPostContactProbe(ghor, GenesisSet1Model(vpList,199), GenesisSet1Model(vpList,197), ghorGenesis);
}

void GenesisMountedGunExitAssociationCase(bool sharedGun, bool commanderParent,
        const std::shared_ptr<Collision::Shape> &hostShape,
        const std::shared_ptr<Collision::Shape> &gunShape,
        const std::shared_ptr<Collision::Shape> &unitShape) {
    GroundWorld w;
    NC_STACK_base visual;
    const vec3d hostPos(2400, -250, -2400);
    const vec3d gunPos = hostPos + vec3d(20, 0, 0);
    Robo host;
    InitGenesisProbeHost(host, w, visual, hostPos, hostShape);

    Actor<NC_STACK_ypagun> gun;
    gun.Bind(w, sharedGun ? 901 : 900, gunPos, gunShape);
    gun._bact_type = BACT_TYPES_GUN;
    gun._radius = 4;
    gun._wrldSize = vec2d(9600, -9600);
    gun._host_station = sharedGun ? nullptr : &host;
    gun._isUnitGunChild = sharedGun;
    gun.setGUN_roboGun(1);
    SetGenesisProbeVisuals(gun, visual);
    host.AddSubject(&gun);
    if (!sharedGun) {
        World::TRoboGun mount{};
        mount.gun_obj = &gun;
        mount.pos = gunPos - hostPos;
        host._roboGuns.push_back(mount);
    }

    AITank unit;
    PrepareAITank(unit, w, sharedGun ? 903 : 902,
                  hostPos + vec3d(21.5, 0, 0), hostPos + vec3d(1000, 0, 0), unitShape);
    unit._pSector = &w._cells.At(0);
    unit._cellId = Common::Point(0, 0);
    if (commanderParent) {
        Actor<> commander;
        commander.Bind(w, sharedGun ? 905 : 904, hostPos + vec3d(1500, 0, 0));
        commander._pSector = &w._cells.At(0);
        commander._cellId = Common::Point(0, 0);
        host.AddSubject(&commander);
        commander.AddSubject(&unit);
        SetGenesisProbeUnit(unit, host, visual, commander._gid, 20);

        Check(unit._parent == &commander && unit._host_station == &host &&
              unit._genesisExitPending,
              "Genesis gun regression preserves the exact Host Station under a commander parent");
        update_msg release{};
        release.frameTime = 20;
        release.gTime = w._timeStamp;
        unit.CreationTimeUpdate(&release);
        Check(unit._status == BACT_STATUS_NORMAL && unit._genesisExitPending,
              "CREATE timer expires normally while mounted attachments keep own-host grace active");

        Robo foreignHost;
        const vec3d foreignPos = hostPos + vec3d(100, 0, 0);
        InitGenesisProbeHost(foreignHost, w, visual, foreignPos, hostShape);
        foreignHost._gid = 906;
        Actor<NC_STACK_ypagun> foreignGun;
        const vec3d foreignGunPos = foreignPos + vec3d(20, 0, 0);
        foreignGun.Bind(w, 907, foreignGunPos, gunShape);
        foreignGun._bact_type = BACT_TYPES_GUN;
        foreignGun._radius = 4;
        foreignGun._host_station = &foreignHost;
        foreignGun.setGUN_roboGun(1);
        foreignHost.AddSubject(&foreignGun);
        SetGenesisProbeVisuals(foreignGun, visual);

        // The unit is 3D-clear of the host body but touches its protruding gun.
        Collision::Contact hostGeometry, gunGeometry;
        const bool hostOverlap = Collision::ContactShapes(*hostShape, host.GetBodyPosition(), host._rotation,
            *unitShape, unit.GetBodyPosition(), unit._rotation, &hostGeometry);
        const bool gunOverlap = Collision::ContactShapes(*gunShape, gun.GetBodyPosition(), gun._rotation,
            *unitShape, unit.GetBodyPosition(), unit._rotation, &gunGeometry);
        Collision::Contact gameContact;
        const bool gunPair = w._collisionScene->PairContact(&unit, &gun, &gameContact);
        Check(!hostOverlap && gunOverlap && !gunPair && unit._genesisExitPending,
              "pending Genesis ignores only its own protruding mounted gun despite real geometric overlap");
        const vec3d moveStart=unit._position;
        const vec3d expectedMove=moveStart+vec3d(1,0,0);
        w._timeStamp+=20;
        unit._position=expectedMove;
        unit.ResolveShapeMovement(moveStart,unit._rotation,20);
        Check((unit._position-expectedMove).length()<.001 && unit._genesisExitPending,
              "native swept movement through an overlapping own-host gun remains free during first exit");

        unit._position = foreignPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool foreignBodyDuringGrace = w._collisionScene->PairContact(&unit, &foreignHost, &gameContact);
        unit._position = foreignGunPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool foreignGunDuringGrace = w._collisionScene->PairContact(&unit, &foreignGun, &gameContact);
        Check(foreignBodyDuringGrace && foreignGunDuringGrace && unit._genesisExitPending,
              "pending grace remains specific to its own host and mounted guns");
        unit._position = hostPos + vec3d(21.5, 0, 0);
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);

        unit._position = hostPos + vec3d(13.5, 0, 0);
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool bodyGap = Collision::ContactShapes(*hostShape, host.GetBodyPosition(), host._rotation,
            *unitShape, unit.GetBodyPosition(), unit._rotation, nullptr);
        const bool gunGap = Collision::ContactShapes(*gunShape, gun.GetBodyPosition(), gun._rotation,
            *unitShape, unit.GetBodyPosition(), unit._rotation, nullptr);
        unit.UpdateGenesisHostExit();
        Check(!bodyGap && !gunGap && unit._genesisExitPending,
              "a clear gap between the host body and its gun stays inside the combined exit envelope");

        unit._position = hostPos + vec3d(40, 0, 0);
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        unit.UpdateGenesisHostExit();
        Check(!unit._genesisExitPending,
              "leaving the full host and mounted-gun footprint ends Genesis grace");

        unit._position = hostPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool ownHostHit = w._collisionScene->PairContact(&unit, &host, &gameContact);
        unit._position = gunPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool ownGunHit = w._collisionScene->PairContact(&unit, &gun, &gameContact);
        Check(ownHostHit && ownGunHit,
              "former Host Station and its gun are solid again after the first exit");

        unit._position = foreignPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool foreignBodyHit = w._collisionScene->PairContact(&unit, &foreignHost, &gameContact);
        unit._position = foreignGunPos;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        const bool foreignGunHit = w._collisionScene->PairContact(&unit, &foreignGun, &gameContact);
        Check(foreignBodyHit && foreignGunHit,
              "a different Host Station and its mounted gun remain solid");

        Actor<NC_STACK_ypatank> peer;
        const vec3d peerPos = hostPos + vec3d(190, 0, 0);
        peer.Bind(w, 908, peerPos, hostShape);
        peer._bact_type = BACT_TYPES_TANK;
        Actor<NC_STACK_ypagun> peerGun;
        peerGun.Bind(w, 909, peerPos + vec3d(20, 0, 0), gunShape);
        peerGun._bact_type = BACT_TYPES_GUN;
        peerGun._isUnitGunChild = true;
        peerGun._host_station = nullptr;
        peer.AddSubject(&peerGun);
        SetGenesisProbeVisuals(peerGun, visual);
        unit._position = peerGun._position;
        unit._old_pos = unit._position;
        w._collisionScene->UpdateActor(&unit);
        Check(w._collisionScene->PairContact(&unit, &peerGun, &gameContact),
              "another vehicle's shared unit gun remains solid");

        Missile shot;
        shot.Bind(w, 910, gunPos - vec3d(0, 0, 30));
        shot._bact_type = BACT_TYPES_MISSLE;
        shot._radius = 1;
        shot._old_pos = gunPos - vec3d(0, 0, 30);
        shot._position = gunPos + vec3d(0, 0, 30);
        Check(w._collisionScene->TraceProjectile(&shot, &gun, shot._rotation, &gameContact),
              "projectile sweep still hits a mounted gun during Genesis grace");

        w._collisionScene.reset();
    } else {
        host.AddSubject(&unit);
        SetGenesisProbeUnit(unit, host, visual, 902, 20);
        Check(unit._parent == &host && unit._host_station == &host &&
              unit._genesisExitPending,
              "Genesis gun regression preserves the exact Host Station under a direct parent");
        Collision::Contact geometry, gameContact;
        const bool overlap = Collision::ContactShapes(*gunShape, gun.GetBodyPosition(), gun._rotation,
            *unitShape, unit.GetBodyPosition(), unit._rotation, &geometry);
        Check(overlap && !w._collisionScene->PairContact(&unit, &gun, &gameContact),
              "legacy native Robo gun association is ignored during own-host grace");
        w._collisionScene.reset();
    }
}

void GenesisMountedGunExitLegacySphereCase() {
    GroundWorld w;
    w._collisionScene.reset(); // A legacy-only world has no persistent shape index.
    NC_STACK_base visual;
    const vec3d hostPos(2400, -250, -2400);
    Robo host;
    InitGenesisProbeHost(host, w, visual, hostPos, {});

    Actor<NC_STACK_ypagun> gun;
    gun.Bind(w, 920, hostPos + vec3d(260, 0, 0));
    gun._bact_type = BACT_TYPES_GUN;
    gun._radius = 8;
    gun._host_station = &host;
    gun.setGUN_roboGun(1);
    SetGenesisProbeVisuals(gun, visual);
    host.AddSubject(&gun);

    Actor<> dummy;
    dummy.Bind(w, 921, hostPos + vec3d(280, 0, 0));
    dummy._isDummy = true;
    dummy._radius = 5;
    gun.AddSubject(&dummy);

    AITank unit;
    PrepareAITank(unit, w, 922, hostPos + vec3d(260, 0, 0), hostPos, {});
    unit._pSector = &w._cells.At(0);
    unit._cellId = Common::Point(0, 0);
    unit._radius = 2;
    host.AddSubject(&unit);
    SetGenesisProbeUnit(unit, host, visual, 922, 20);
    update_msg update{};
    update.frameTime = 20;
    update.gTime = w._timeStamp;
    unit.CreationTimeUpdate(&update);

    vec3d selfCenter, otherCenter;
    float penetration = 0;
    const bool legacyPairAfterRelease = unit.GetUnitCollisionContact(&gun, &selfCenter, &otherCenter, &penetration);
    Check(unit._status == BACT_STATUS_NORMAL && unit._genesisExitPending &&
          !legacyPairAfterRelease,
          "profile-free legacy collision spheres skip the own mounted Robo gun during grace");
    unit._genesisExitPending = false;
    Check(unit.GetUnitCollisionContact(&gun, &selfCenter, &otherCenter, &penetration),
          "profile-free legacy collision contact remains active after grace");
    unit._genesisExitPending = true;
    unit.UpdateGenesisHostExit();
    Check(unit._genesisExitPending,
          "legacy sphere footprint keeps grace inside a gap spanning host and attachments");
    unit._position = hostPos + vec3d(290, 0, 0);
    unit._old_pos = unit._position;
    unit.UpdateGenesisHostExit();
    Check(!unit._genesisExitPending,
          "legacy sphere footprint releases grace after the nested gun attachment group is cleared");
    dummy._status=BACT_STATUS_DEAD;
    unit._radius=10; gun._radius=1;
    unit._genesisExitPending=true;
    unit._position=gun._position+vec3d(12,0,0);
    unit.UpdateGenesisHostExit();
    Check(unit._genesisExitPending,
          "legacy exit accounts for the mover-radius contact rule around a smaller mounted gun");
    unit._position=gun._position+vec3d(30,0,0);
    unit.UpdateGenesisHostExit();
    Check(!unit._genesisExitPending,
          "legacy first-exit grace ends once the native single-radius contact range is cleared");
    w._collisionScene.reset();
}

void GenesisMountedGunExitRegression() {
    auto hostShape = Shape({Box(vec3d(0, 0, 0), 10, 8, 10)});
    auto gunShape = Shape({Box(vec3d(0, 0, 0), 4, 4, 4)});
    auto unitShape = Shape({Box(vec3d(0, 0, 0), 2, 2, 2)});
    GenesisMountedGunExitAssociationCase(false, false, hostShape, gunShape, unitShape);
    GenesisMountedGunExitAssociationCase(true, true, hostShape, gunShape, unitShape);
    GenesisMountedGunExitLegacySphereCase();
    GenesisExitSaveParserRegression();
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
    if (argc>1 && std::string(argv[1])=="--host-first-exit") {
        FSMgr::iDir::setBaseDir("");
        GenesisMountedGunExitRegression();
        std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
        return failures ? 1 : 0;
    }
    if (argc>1 && std::string(argv[1])=="--genesis-visual-terrain") {
        extern int init_classesLists_and_variables();
        FSMgr::iDir::setBaseDir("..");
        Common::Env.SetPrefix("data", "Data");
        Common::Env.SetPrefix("rsrc", "data:set1");
        Common::Env.SetPrefix("scripts", "data:scripts");
        init_classesLists_and_variables();
        auto *set=load_set_base();
        Check(set && !set->GetKidList().empty(), "native active Set1 visual models load");
        if (set && !set->GetKidList().empty()) GenesisRealVisualTerrainRegression(set->GetKidList().front());
        if (set) set->Delete();
        std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
        return failures ? 1 : 0;
    }
    const mat3x3 identity=mat3x3::Ident(); const vec3d zero(0,0,0);
    auto cube=Shape({Box(zero,10,10,10)});
    auto concave=Shape({Box(vec3d(-20,0,0),8,10,10),Box(vec3d(20,0,0),8,10,10)});
    ProbeMove<NC_STACK_ypabact>(cube,"production heli Move invokes swept response");
    ProbeMove<NC_STACK_ypatank>(cube,"production tank Move invokes swept response");
    ProbeMove<NC_STACK_ypacar>(cube,"production car Move inherits swept response");
    ProbeMove<NC_STACK_ypaflyer>(cube,"production flyer Move invokes swept response");
    ProbeMove<NC_STACK_ypaufo>(cube,"production UFO Move invokes swept response");
    ProbeMove<NC_STACK_yparobo>(cube,"production Robo Move invokes swept response",BACT_TYPES_ROBO);
    RoboFluxGroundRegression();
    RoboPositionRegression();
    RoboUpdateRegression(cube);
    GenesisExitSaveParserRegression();
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
    if (argc>1 && std::string(argv[1])=="--birth-placement") {
        std::string error;
        auto myko=Collision::Load("Models/Collision/Myko-Scout.collision", &error);
        BirthPlacementRegression(myko);
        GenesisFactoryBirthPlacementRegression();
        std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
        return failures ? 1 : 0;
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
            RoboUpdateRegression(authored);
            if (!pilotProfiles.empty()) GenesisHostExitRegression(authored, pilotProfiles.front());
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
    Check(world._vhclProtos[1].briefing_radius == 0 && world._vhclProtos[1].BriefingRadius() == 25,
          "absent briefing radius uses vanilla radius");
    Check(parse({"modify_vehicle 1","radius = 80","coll_act = 0","coll_radius = 200","end"}) &&
          world._vhclProtos[1].BriefingRadius() == 80,"briefing fallback ignores compound collision size");
    Check(parse({"modify_vehicle 1","Briefing_radius = 120","end"}) &&
          world._vhclProtos[1].BriefingRadius() == 120 && world._vhclProtos[1].radius == 80 &&
          world._vhclProtos[1].coll.roboColls[0].robo_coll_radius == 200,"briefing override leaves physical radii unchanged");
    Check(parse({"modify_vehicle 1","energy = 32000","end"}) && world._vhclProtos[1].BriefingRadius() == 120,
          "unrelated modifier preserves briefing override");
    for (const char *value : {"0", "-1", "nan", "inf", "invalid", "120junk", "1e38", "1e100"})
        Check(parse({"modify_vehicle 1",std::string("briefing_radius = ")+value,"end"}) &&
              world._vhclProtos[1].briefing_radius == 0 && world._vhclProtos[1].BriefingRadius() == 80,
              "invalid or disabled briefing radius falls back safely");
    Check(parse({"modify_vehicle 1","briefing_radius = 0.5","end"}) && world._vhclProtos[1].BriefingRadius() == 0.5f,
          "fractional briefing radius supported");
    Check(parse({"new_vehicle 1","model = heli","end"}) && world._vhclProtos[1].briefing_radius == 0,
          "new vehicle resets briefing override");
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
