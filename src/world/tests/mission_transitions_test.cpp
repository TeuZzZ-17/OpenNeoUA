// Mission transition probes use production routing and isolated save fixtures.
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/ypagun.h"
#include "common/env.h"
#include "system/fsmgr.h"
#include <cstdio>
#include <fstream>
#undef main

namespace {
int checks=0, failures=0;
void Check(bool ok, const char *name) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s\n",name); }
}
struct MissionWorld : NC_STACK_ypaworld {
    int freshLoads=0, finalLoads=0, restartSaves=0, settingsLoads=0, settingsSaves=0;
    bool settingsOK=true, saveOK=true;
    std::array<int,256> persisted{};
    MissionWorld() {
        _mapSize=Common::Point(8,8); _cells.Resize(_mapSize);
        for (int y=0; y<8; ++y) for (int x=0; x<8; ++x) {
            auto &cell=_cells(x,y); cell.CellId=Common::Point(x,y);
            cell.height=100; cell.PurposeType=cellArea::PT_NONE;
        }
    }
    size_t LoadSettings(const std::string &file, const std::string &, uint32_t mask,
                        bool updateShell, bool) override {
        ++settingsLoads;
        Check(file=="settings.tmp" && mask==(World::SDF_BUDDY|World::SDF_PROTO|World::SDF_USER) && !updateShell,
              "restart uses the existing pre-mission settings rollback");
        return settingsOK;
    }
    bool SaveSettings(UserData *, const std::string &, uint32_t mask) override {
        ++settingsSaves;
        Check(mask==World::SDF_ALL,"restart persists the restored campaign state");
        for (size_t i=0; i<persisted.size(); ++i) persisted[i]=_globalMapRegions.MapRegions[i].Status;
        return saveOK;
    }
    size_t ypaworld_func161(yw_arg161 *arg) override {
        ++freshLoads; _levelInfo.LevelID=arg->lvlID; return 1;
    }
    size_t LoadGame(const std::string &file) override {
        ++finalLoads;
        Check(file=="save:probe/7.fin","completed visit selects its own final save");
        _levelInfo.LevelID=7; return 1;
    }
    size_t SaveGame(const std::string &file) override {
        ++restartSaves;
        Check(file=="save:probe/7.rst","mission start snapshots the accepted new starting world");
        return 1;
    }
    void ypaworld_func136(ypaworld_arg136 *arg) override {
        arg->isect=true; arg->isectPos=vec3d(arg->stPos.x,100,arg->stPos.z);
    }
};
template<class Base> struct BoundActor : Base {
    NC_STACK_base visual;
    void Bind(MissionWorld &world, const vec3d &pos) {
        this->_world=&world; this->_owner=1; this->_status=BACT_STATUS_NORMAL;
        this->_status_flg=0; this->_oflags=0; this->_position=this->_old_pos=pos;
        this->_rotation=mat3x3::Ident(); this->_wrldSize=vec2d(9600,-9600);
        this->_mass=1000;
        this->_vp_normal=this->_vp_dead=this->_vp_fire=this->_vp_genesis=
            this->_vp_wait=this->_vp_megadeth=&visual;
        this->_cellId=World::PositionToSectorID(pos);
        this->_pSector=&world._cells(this->_cellId);
        this->_cellRef=this->_pSector->unitsList.push_back(this);
    }
};
void RestartRouting() {
    for (int previous : {TMapRegionInfo::STATUS_ENABLED,TMapRegionInfo::STATUS_COMPLETED})
        for (bool persisted : {false,true}) {
            UserData shell{}; shell.UserName="probe";
            MissionWorld world; world._GameShell=&shell;
            world._levelInfo.LevelID=7; world._levelInfo.State=TLevelInfo::STATE_DEBRIEFING;
            world._missionMapStatusSnapshotValid=true; world._missionMapStatusSnapshotLevel=7;
            world._missionMapStatusSnapshot[7]=previous;
            world._missionMapStatusSnapshot[8]=TMapRegionInfo::STATUS_DISABLED;
            world._missionMapStatusSnapshot[9]=TMapRegionInfo::STATUS_COMPLETED;
            world._globalMapRegions.MapRegions[7].Status=TMapRegionInfo::STATUS_COMPLETED;
            world._globalMapRegions.MapRegions[8].Status=TMapRegionInfo::STATUS_ENABLED;
            world._plasmaCurrencyBank=123; world.saveOK=persisted;
            Check(world.RestartCompletedMission(),"debrief restart is accepted for a valid single-player run");
            Check(shell.envAction.action==EnvAction::ACTION_RESTART_MISSION && shell.envAction.params[0]==7,
                  "debrief emits an explicit fresh restart request");
            Check(world.persisted[7]==TMapRegionInfo::STATUS_ENABLED &&
                  world.persisted[8]==TMapRegionInfo::STATUS_DISABLED &&
                  world.persisted[9]==TMapRegionInfo::STATUS_COMPLETED,
                  "restart resets only this completion and rolls back new unlocks");
            Check(world._plasmaCurrencyBank==123,"restart preserves the existing Plasma bank policy");
            Check(world._levelInfo.State==TLevelInfo::STATE_MENU && !world._missionMapStatusSnapshotValid,
                  "restart leaves debrief and consumes its snapshot once");
            Check(!world.RestartCompletedMission(),"the same debrief cannot restart twice");
            // Force completed status too: the explicit flag must still bypass .fin.
            world._globalMapRegions.MapRegions[7].Status=TMapRegionInfo::STATUS_COMPLETED;
            yw_arg161 start{}; start.lvlID=7; start.freshStart=true;
            Check(world.ypaworld_func183(&start)==1 && world.freshLoads==1 && world.finalLoads==0,
                  "fresh restart loads level data even when a completed final save exists");
        }
    UserData shell{}; shell.UserName="probe";
    MissionWorld world; world._GameShell=&shell;
    world._globalMapRegions.MapRegions[7].Status=TMapRegionInfo::STATUS_COMPLETED;
    yw_arg161 visit{}; visit.lvlID=7;
    Check(world.ypaworld_func183(&visit)==1 && world.finalLoads==1 && world.freshLoads==0,
          "ordinary completed visit keeps the final territory");
    world._levelInfo.State=TLevelInfo::STATE_DEBRIEFING;
    world._levelInfo.LevelID=7; world._missionMapStatusSnapshotLevel=7;
    world._missionMapStatusSnapshotValid=true; world.settingsOK=false;
    shell.envAction.action=EnvAction::ACTION_NONE;
    Check(!world.RestartCompletedMission() && shell.envAction.action==EnvAction::ACTION_NONE,
          "failed settings rollback does not start a new mission");
}
void CompletedHostPlacement() {
    for (bool atGate : {false,true}) for (bool correctOwner : {false,true}) {
        MissionWorld world;
        BoundActor<NC_STACK_yparobo> host; BoundActor<NC_STACK_ypagun> gun;
        const vec3d saved(3000,-600,-3000);
        host.Bind(world,saved); gun.Bind(world,saved+vec3d(20,0,0));
        host._bact_type=BACT_TYPES_ROBO; gun._bact_type=BACT_TYPES_GUN;
        host._beam_time=1000; host._fly_dir_length=20;
        host._roboState|=NC_STACK_yparobo::ROBOSTATE_MOVE;
        World::TRoboGun mount; mount.gun_obj=&gun; mount.pos=vec3d(20,0,0);
        host._roboGuns.push_back(mount); world._userRobo=&host;
        TMapGate gate; gate.CellId=atGate ? host._cellId : Common::Point(5,5);
        world._levelInfo.Gates.push_back(gate);
        world._cells(gate.CellId).PurposeType=cellArea::PT_GATEOPENED;
        MapRobo start; start.Owner=correctOwner ? 1 : 2; start.Pos=vec3d(4200,-500,-4200);
        const bool moved=world.RestoreCompletedMissionHostStart({start});
        Check(moved==(atGate&&correctOwner),"final-save host relocation requires its gate and authored owner");
        if (moved) {
            Check((host._position-vec3d(4200,-400,-4200)).length()<.001,
                  "completed visit restores the terrain-relative authored start");
            Check(host._old_pos==host._position && host._roboYPos==host._position.y,
                  "restored host movement and hover origins match the accepted position");
            Check((gun._position-host._position-vec3d(20,0,0)).length()<.001 && gun._cellId==host._cellId,
                  "native host SetPosition moves its mounted gun and sector reference");
            Check(host._beam_time==0 && host._fly_dir_length==0 &&
                  !(host._roboState&NC_STACK_yparobo::ROBOSTATE_MOVE),
                  "completed visit clears the previous gate departure state");
            world._levelInfo.State=TLevelInfo::STATE_PLAYING;
            world.ypaworld_func168(&host);
            Check(world._levelInfo.State==TLevelInfo::STATE_PLAYING,
                  "restored host does not immediately complete the mission again");
        } else {
            Check(host._position==saved && host._beam_time==1000,
                  "host outside a gate or without an authored owner is unchanged");
        }
        world._userRobo=nullptr;
    }
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::ofstream("Data/Build/probes/mission-fixtures/probe/7.fin")<<"isolated routing fixture\n";
    FSMgr::iDir::setBaseDir("");
    const std::string savePrefix=Common::Env.GetPrefix("save");
    Common::Env.SetPrefix("save","Data/Build/probes/mission-fixtures");
    RestartRouting();
    CompletedHostPlacement();
    Common::Env.SetPrefix("save",savePrefix);
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
    return failures ? 1 : 0;
}
