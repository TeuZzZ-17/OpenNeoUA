// Focused tests linked to production mount, guidance, fuse and collision code.
#include "game/yw.h"
#include "game/ypabact.h"
#include "game/ypaflyer.h"
#include "game/ypaufo.h"
#include "game/ypagun.h"
#include "game/ypamissile.h"
#include "nucleus/skeleton.h"
#include "../collision_shape.h"
#include "../kamikaze_contact.h"
#include <cstdio>
#include <memory>
#undef main
namespace {
int checks=0, failures=0;
void Check(bool ok,const char *label) { ++checks; if(!ok) { ++failures; std::printf("FAIL %s\n",label); } }
struct TestWorld : NC_STACK_ypaworld {
    int detonations=0;
    vec3d explosion;
    bool groundProbe=false, wallProbe=false;
    UAskeleton::Data *probe=nullptr;
    NC_STACK_ypamissile *ypaworld_func147(ypaworld_arg146 *arg) override {
        ++detonations; explosion=arg->pos; return nullptr;
    }
    size_t ypaworld_func145(NC_STACK_ypabact *) override { return 1; }
    void ypaworld_func136(ypaworld_arg136 *arg) override {
        const bool wall=wallProbe && std::fabs(arg->vect.y)<1e-8 && arg->vect.XZ().length()>1;
        arg->isect=wall || (groundProbe && arg->vect.y>0);
        if(arg->isect) { arg->tVal=wall?.25:.5; arg->skel=probe; arg->polyID=0; }
    }
};
template<class Base=NC_STACK_ypabact> struct Actor : Base {
    vec3d moveDirection;
    int moves=0;
    void Bind(TestWorld &w,vec3d pos,int owner=1) {
        this->_world=&w; this->_owner=owner; this->_gid=owner;
        this->_bact_type=BACT_TYPES_FLYER; this->_status=BACT_STATUS_NORMAL;
        this->_energy=this->_energy_max=15000; this->_radius=40;
        this->_position=this->_old_pos=pos; this->_rotation=mat3x3::Ident();
        this->_weapon=1; this->_extra_weapons.fill(-1);
        this->_oflags=0; this->_status_flg=BACT_STFLAG_ATTACK;
        this->_atk_ret=NC_STACK_ypabact::TA_FIGHT;
        this->_pSector=&w._cells.At(1,1); this->_cellId=Common::Point(1,1);
        this->_primTtype=this->_secndTtype=BACT_TGT_TYPE_NONE;
        this->_mass=400; this->_force=7000; this->_maxrot=.9; this->_height=300;
        this->_fly_dir=vec3d::OZ(1); this->_fly_dir_length=0;
    }
    void Target(NC_STACK_ypabact *target) {
        this->_primTtype=BACT_TGT_TYPE_UNIT; this->_primT.pbact=target;
    }
    void Move(move_msg *) override { moveDirection=this->_target_dir; ++moves; }
    void FightWithBact(bact_arg75 *) override {}
    void FightWithSect(bact_arg75 *) override {}
    void Recoil(bact_arg88 *) override {}
    size_t SetPosition(bact_arg80 *arg) override { this->_position=arg->pos; return 1; }
};
Collision::Part Box(double x,double y,double z) {
    Collision::Part p;
    for(const auto &v : {vec3d(-x,-y,-z),vec3d(x,-y,-z),vec3d(x,y,-z),vec3d(-x,y,-z),
                         vec3d(-x,-y,z),vec3d(x,-y,z),vec3d(x,y,z),vec3d(-x,y,z)}) p.vertices.push_back(v);
    p.faces={{{0,2,1}},{{0,3,2}},{{4,5,6}},{{4,6,7}},{{0,1,5}},{{0,5,4}},
             {{3,7,6}},{{3,6,2}},{{0,4,7}},{{0,7,3}},{{1,2,6}},{{1,6,5}}};
    return p;
}
struct BlastProbe : Actor<NC_STACK_ypamissile> {
    using NC_STACK_ypamissile::ApplyBlastToBuildings;
    using NC_STACK_ypamissile::ApplyBlastToSectors;
    std::vector<yw_arg129> damage;
    void ChangeSectorEnergy(yw_arg129 *arg) override { damage.push_back(*arg); }
    void IgnoreBuildings(bool value) {
        if(value) _mislFlags|=FLAG_MISL_IGNOREBUILDS;
        else _mislFlags&=~FLAG_MISL_IGNOREBUILDS;
    }
};
}
int main() {
    auto w=std::unique_ptr<TestWorld>(new TestWorld);
    w->_mapSize=Common::Point(6,6); w->_cells.Resize(w->_mapSize);
    for(auto &cell:w->_cells) { cell.height=0; cell.owner=2; cell.SectorType=1; cell.type_id=0; cell.AddToViewMask(1); }
    w->_weaponProtos.resize(2); w->_weaponProtos[1]._weaponFlags=World::TWeapProto::WEAPON_FLAGS_KAMIKAZE;
    w->_weaponProtos[1].trigger_radius=360;
    Actor<> carrier, enemy;
    const vec3d anchor(1800,0,-1800);
    carrier.Bind(*w,anchor); enemy.Bind(*w,anchor+vec3d(400,0,0),2); carrier.Target(&enemy);
    const auto fuse=[&](const char *label,bool expected,const vec3d *old=nullptr) {
        w->detonations=0; carrier.UpdateKamikaze(nullptr,old);
        Check((w->detonations==1)==expected,label);
    };
    fuse("legacy enemy surface exact tangency at trigger 360",true);
    enemy._position.x=anchor.x+400.001; fuse("legacy enemy tiny gap",false);
    enemy._position.x=anchor.x+399.999; fuse("legacy enemy tiny overlap",true);
    enemy._owner=1; fuse("friendly does not detonate",false); enemy._owner=2;
    enemy._status=BACT_STATUS_DEAD; fuse("dead enemy does not detonate",false); enemy._status=BACT_STATUS_NORMAL;
    enemy._bact_type=BACT_TYPES_MISSLE; fuse("projectile does not detonate",false); enemy._bact_type=BACT_TYPES_FLYER;
    enemy._position=anchor+vec3d(0,-400,0); fuse("XYZ fuse touches airborne enemy",true);
    enemy._position=anchor+vec3d(0,-400.001,0); fuse("vertical gap rejected",false);
    enemy._position=anchor;
    const vec3d old=anchor-vec3d(1000,0,0); carrier._position=anchor+vec3d(1000,0,0);
    fuse("fast carrier cannot skip entire fuse",true,&old);
    Check((w->explosion-(anchor-vec3d(400,0,0))).length()<1e-8,"fast explosion at first contact");
    carrier._position=anchor; enemy._position=anchor+vec3d(400,0,0);
    auto shape=std::make_shared<Collision::Shape>(); shape->parts.push_back(Box(40,40,40));
    std::string error; Check(Collision::Build(*shape,&error),"build real collision hull"); enemy._collisionShape=shape;
    fuse("authored hull exact surface tangency",true);
    enemy._position.x=anchor.x+400.01; fuse("authored hull positive gap",false);
    enemy._position.x=anchor.x+399.99; fuse("authored hull small overlap",true);
    enemy._position=anchor; carrier._position=anchor+vec3d(1000,0,0);
    fuse("fast carrier crosses real hull fuse",true,&old);
    Check(std::fabs(w->explosion.x-(anchor.x-400))<.1,"real hull sweep stops at fuse entry");
    enemy._collisionShape.reset(); enemy._position=anchor+vec3d(400,0,0); carrier._position=anchor;
    for(int type : {BACT_TYPES_BACT,BACT_TYPES_TANK,BACT_TYPES_CAR,BACT_TYPES_FLYER,BACT_TYPES_UFO,BACT_TYPES_ROBO}) {
        carrier._bact_type=type; fuse("mounted kamikaze fuse shared across carrier classes",true);
    }
    carrier._bact_type=BACT_TYPES_FLYER;
    World::rbcolls compound; World::TRoboColl sphere; sphere.coll_pos=vec3d(-400,0,0); sphere.robo_coll_radius=40;
    compound.roboColls.push_back(sphere); enemy.ApplyCompoundCollision(compound,false);
    enemy._position=anchor+vec3d(800,0,0); fuse("offset compound sphere surface contact",true);
    enemy.ApplyCompoundCollision(World::rbcolls(),true); enemy._position=anchor+vec3d(400,0,0);
    Actor<NC_STACK_ypagun> module; module.Bind(*w,anchor); module._bact_type=BACT_TYPES_GUN;
    carrier._weapon=-1; World::TRoboGun mount; mount.gun_obj=&module; carrier._unitGuns.push_back(mount);
    fuse("attached module supplies kamikaze radius",true); carrier._unitGuns.clear(); carrier._weapon=1;

    NC_STACK_skeleton skeleton; UAskeleton::Data geometry; skeleton._resData=&geometry;
    geometry.POO={vec3d(-600,-600,-600),vec3d(600,-600,-600),vec3d(600,-600,600),vec3d(-600,-600,600)};
    UAskeleton::Polygon roof; roof.num_vertices=4; roof.v[0]=0; roof.v[1]=1; roof.v[2]=2; roof.v[3]=3; roof.B=1;
    geometry.polygons.push_back(roof); w->probe=&geometry;
    w->_legoArray[0].CollisionSkelet=&skeleton;
    TSubSectorDesc sector; for(int y=0;y<3;++y) for(int x=0;x<3;++x) w->_secTypeArray[0].SubSectors.At(x,y)=&sector;
    carrier._primTtype=BACT_TGT_TYPE_CELL; carrier._primT.pcell=&w->_cells.At(1,1);
    carrier._primTpos=anchor; carrier._position=anchor+vec3d(0,-960,0); carrier._aggr=100;
    carrier._status_flg=BACT_STFLAG_ATTACK;
    fuse("maximum aggression sector fuse touches roof above target origin",true);
    carrier._position.y=-960.01; fuse("building roof positive gap",false);
    carrier._position.y=-959.99; fuse("building roof small overlap",true);
    carrier._position.y=-960; carrier._status_flg=0; fuse("sector traversal without active attack does not detonate",false);
    carrier._status_flg=BACT_STFLAG_ATTACK; carrier._primTpos=anchor+vec3d(1200,0,0);
    fuse("unrelated sector cannot satisfy active target fuse",false); carrier._primTpos=anchor;
    w->_cells.At(1,1).owner=1; w->_cells.At(1,1).buildings_health.At(0,0)=255;
    fuse("maximum aggression can attack own intact sector",true);
    carrier._aggr=99; fuse("lower aggression does not attack own sector",false);
    carrier._aggr=100; w->_cells.At(1,1).buildings_health.At(0,0)=0;
    fuse("razed own sector does not detonate",false); w->_cells.At(1,1).owner=2;
    const vec3d roofFrom=anchor+vec3d(0,-1500,0); carrier._position=anchor+vec3d(0,-300,0);
    fuse("sector sweep catches roof passed during movement",true,&roofFrom);
    Check(std::fabs(w->explosion.y+960)<.1,"sector explosion placed at first roof contact");
    w->_legoArray[0].CollisionSkelet=nullptr;

    carrier._position=anchor; carrier.Target(&enemy); enemy._position=anchor+vec3d(0,500,-1000);
    carrier._status_flg=BACT_STFLAG_ATTACK|BACT_STFLAG_APPROACH|BACT_STFLAG_DODGE_LEFT;
    carrier._ai_attack_range=500; carrier._ai_retreat_start_range=200; carrier._ai_retreat_stop_range=450;
    Check(carrier.ApplyKamikazeRammingGuidance(),"armed carrier has active guidance");
    const vec3d desired=enemy._position-carrier._position;
    Check((carrier._target_vec-desired).length()<1e-8,"guidance refreshes the live enemy destination");
    Check(!(carrier._status_flg&BACT_STFLAG_APPROACH),"combat retreat cleared during charge");
    Check(carrier._status_flg&BACT_STFLAG_DODGE_LEFT,"guidance preserves pending obstacle avoidance");
    Check(carrier._status_flg&BACT_STFLAG_MOVE,"charge keeps moving inside ordinary combat ranges");
    enemy._position.y=-500; Check(carrier.ApplyKamikazeRammingGuidance() && carrier._target_vec.y<0,"guidance follows target above");
    carrier._weapon=-1; carrier._primTtype=BACT_TGT_TYPE_CELL; carrier._primTpos=anchor;
    Check(!carrier.ApplyKamikazeRammingGuidance(),"ordinary vehicle sector attack never uses kamikaze guidance");
    carrier._weapon=1; carrier._oflags=BACT_OFLAG_USERINPT; carrier.Target(&enemy);
    Check(!carrier.ApplyKamikazeRammingGuidance(),"manual control retains player steering"); carrier._oflags=0;
    Actor<NC_STACK_ypaflyer> flyer; flyer.Bind(*w,anchor+vec3d(0,-300,0)); enemy._position=anchor+vec3d(0,0,-500); flyer.Target(&enemy);
    w->groundProbe=true; update_msg frame; frame.frameTime=16; flyer.AI_layer3(&frame);
    Check(flyer.moves==1 && flyer.moveDirection.y<0,"native flyer retains terrain clearance during charge");
    w->groundProbe=false; flyer.AI_layer3(&frame);
    Check(flyer.moveDirection.y>0,"native flyer descends toward lower enemy when route is clear");
    Actor<NC_STACK_ypaufo> ufo; ufo.Bind(*w,anchor+vec3d(0,-300,0)); ufo.Target(&enemy);
    enemy._position=anchor+vec3d(500,0,500);
    const vec2d toEnemy=(enemy._position-ufo._position).XZ();
    const double oldAlignment=toEnemy.dot(ufo._rotation.AxisZ().XZ());
    ufo.AI_layer3(&frame);
    Check(ufo.moves==1 && ufo.moveDirection.y>0,"native UFO charges toward lower enemy");
    Check(toEnemy.dot(ufo._rotation.AxisZ().XZ())>oldAlignment,"native UFO turns toward enemy instead of away");
    w->groundProbe=false;
    geometry.polygons[0].A=0; geometry.polygons[0].B=0; geometry.polygons[0].C=-1;
    Actor<NC_STACK_ypaflyer> blockedFlyer; blockedFlyer.Bind(*w,anchor+vec3d(0,-300,0));
    enemy._position=blockedFlyer._position+vec3d(0,0,1000); blockedFlyer.Target(&enemy);
    w->wallProbe=true; blockedFlyer.AI_layer3(&frame);
    Check((blockedFlyer._status_flg&BACT_STFLAG_DODGE_LEFT) && blockedFlyer.moveDirection.x<-.5,
          "native flyer steers sideways around intervening building");
    blockedFlyer.AI_layer3(&frame);
    Check((blockedFlyer._status_flg&BACT_STFLAG_DODGE_LEFT) && blockedFlyer.moveDirection.x<-.5,
          "native flyer keeps avoidance across consecutive frames");
    w->wallProbe=false; blockedFlyer.AI_layer3(&frame);
    Check(!(blockedFlyer._status_flg&(BACT_STFLAG_DODGE_LEFT|BACT_STFLAG_DODGE_RIGHT)) &&
          blockedFlyer.moveDirection.z>.9,
          "native flyer resumes charge after obstacle clears");
    Actor<> blockedHeli; blockedHeli.Bind(*w,anchor+vec3d(0,-300,0));
    blockedHeli._bact_type=BACT_TYPES_BACT;
    enemy._position=blockedHeli._position+vec3d(0,0,1000); blockedHeli.Target(&enemy);
    w->wallProbe=true; blockedHeli.AI_layer3(&frame);
    Check((blockedHeli._status_flg&BACT_STFLAG_DODGE_LEFT) && blockedHeli.moveDirection.x<-.5,
          "shared helicopter controller retains native obstacle steering");
    w->wallProbe=false; blockedHeli.AI_layer3(&frame);
    Check(!(blockedHeli._status_flg&(BACT_STFLAG_DODGE_LEFT|BACT_STFLAG_DODGE_RIGHT)) &&
          blockedHeli.moveDirection.z>.9,
          "shared helicopter controller resumes charge after obstacle clears");
    Actor<NC_STACK_ypaufo> blockedUfo; blockedUfo.Bind(*w,anchor+vec3d(0,-300,0));
    blockedUfo._ufoTogo=1200;
    enemy._position=blockedUfo._position+vec3d(0,0,1000); blockedUfo.Target(&enemy);
    w->wallProbe=true; blockedUfo.AI_layer3(&frame);
    Check((blockedUfo._ufoFlags&9)==9,"native UFO starts obstacle turning maneuver");
    const double pendingTurn=std::fabs(blockedUfo._ufoProcAngle);
    blockedUfo.AI_layer3(&frame);
    Check(std::fabs(blockedUfo._ufoProcAngle)<pendingTurn && (blockedUfo._ufoFlags&1),
          "native UFO advances pending avoidance turn instead of resetting it");
    w->wallProbe=false;
    for(int i=0;i<150 && (blockedUfo._ufoFlags&1);++i) blockedUfo.AI_layer3(&frame);
    Check(!(blockedUfo._ufoFlags&1) && std::fabs(blockedUfo._ufoProcAngle)<1e-5,
          "native UFO completes obstacle maneuver after route clears");
    blockedUfo.AI_layer3(&frame);
    Check((blockedUfo._status_flg&BACT_STFLAG_ATTACK) && (blockedUfo._status_flg&BACT_STFLAG_MOVE),
          "native UFO continues attack after avoiding building");
    double fraction=0;
    Check(World::KamikazeSphereSweep(vec3d(-10,1,0),vec3d(10,1,0),vec3d(0,0,0),1,&fraction) && std::fabs(fraction-.5)<1e-12,"swept sphere edge tangency included");
    Check(!World::KamikazeSphereSweep(vec3d(-10,1.001,0),vec3d(10,1.001,0),vec3d(0,0,0),1,&fraction),"swept sphere tiny miss rejected");
    Check(!World::KamikazeSphereSweep(vec3d(-10,0,0),vec3d(10,0,0),vec3d(0,0,0),0,&fraction),"zero radius geometry inactive");
    // The fuse reaches a roof 600 units above the historical damage point.
    // The real blast scan must reach that same geometry without changing
    // ordinary projectile blast semantics or damaging unrelated slots.
    for(int y=0;y<6;++y) for(int x=0;x<6;++x) {
        auto &cell=w->_cells.At(x,y); cell.CellId=Common::Point(x,y);
        cell.buildings_health.At(0,0)=0;
    }
    geometry.polygons[0].A=0; geometry.polygons[0].B=1; geometry.polygons[0].C=0;
    geometry.POO={vec3d(-40,-600,-40),vec3d(40,-600,-40),vec3d(40,-600,40),vec3d(-40,-600,40)};
    w->_legoArray[0].CollisionSkelet=&skeleton;
    auto &damagedCell=w->_cells.At(1,1);
    damagedCell.buildings_health.At(0,0)=255;
    damagedCell.PurposeType=cellArea::PT_NONE;
    BlastProbe blast; blast.Bind(*w,anchor+vec3d(0,-900,0));
    blast._bact_type=BACT_TYPES_MISSLE; blast._vehicleID=1;
    blast.SetLauncherBact(&carrier); blast.SetBlast(400,150000,0);
    blast.ApplyBlastToSectors();
    Check(blast.damage.size()==1 && blast.damage[0].field_10==150000,
          "kamikaze area damage reaches tall ordinary sector roof");
    blast.damage.clear(); damagedCell.PurposeType=cellArea::PT_POWERSTATION;
    blast.ApplyBlastToBuildings();
    Check(blast.damage.size()==1 && blast.damage[0].field_10==150000,
          "kamikaze area damage reaches tall functional building roof");
    blast.damage.clear(); blast.SetBlast(400,150000,1); blast.ApplyBlastToBuildings();
    Check(blast.damage.size()==1 && std::abs(blast.damage[0].field_10-74487)<=2,
          "kamikaze blast falloff uses surface distance");
    blast.damage.clear(); blast.SetBlast(299,150000,0); blast.ApplyBlastToBuildings();
    Check(blast.damage.empty(),"building surface outside blast radius receives no damage");
    blast.SetBlast(400,150000,0); blast.IgnoreBuildings(true); blast.ApplyBlastToBuildings();
    Check(blast.damage.empty(),"ignore-buildings rule remains authoritative"); blast.IgnoreBuildings(false);
    w->_weaponProtos[1]._weaponFlags=World::TWeapProto::WEAPON_FLAG_PROJECTILE;
    blast.ApplyBlastToBuildings();
    Check(blast.damage.empty(),"ordinary projectile retains historical building blast distance");
    w->_weaponProtos[1]._weaponFlags=World::TWeapProto::WEAPON_FLAGS_KAMIKAZE;
    damagedCell.SectorType=0;
    for(int y=0;y<3;++y) for(int x=0;x<3;++x) damagedCell.buildings_health.At(x,y)=255;
    blast._position=anchor+vec3d(-300,-900,-300); blast.SetBlast(310,150000,0);
    blast.ApplyBlastToBuildings();
    Check(blast.damage.size()==1 &&
          (blast.damage[0].pos-(anchor+vec3d(-300,0,-300))).length()<1e-8,
          "blast damages only the reached building slot, not all nine sector slots");
    blast.damage.clear(); damagedCell.PurposeType=cellArea::PT_NONE;
    blast.ApplyBlastToSectors();
    Check(blast.damage.size()==1,"ordinary sector architecture also uses single-slot surface contact");
    w->_legoArray[0].CollisionSkelet=nullptr;
    w->_collisionScene.reset();
    std::printf("CHECKS %d FAILURES %d\n",checks,failures); return failures?1:0;
}
