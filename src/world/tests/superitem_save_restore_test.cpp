// Restore saved Super Items without emitting gameplay activation messages.
#include "game/yw.h"
#include "world/saveparsers.h"
#include "common/def_parser.h"
#include <cstdio>
#undef main
namespace {
int checks=0, failures=0;
void Check(bool ok,const char *name) {
    ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",name); }
}
struct RestoreWorld : NC_STACK_ypaworld {
    int messages=0, buildingUpdates=0, lastBuilding=-1, lastMessage=-1;
    RestoreWorld(int type) {
        _mapSize=Common::Point(5,5); _cells.Resize(_mapSize);
        _levelInfo.SuperItems.resize(1);
        auto &item=_levelInfo.SuperItems[0];
        item.CellId=Common::Point(2,2); item.PCell=&_cells(item.CellId);
        item.PCell->owner=1; item.Type=type;
        item.ActiveBldID=101; item.InactiveBldID=102; item.TriggerBldID=103;
        item.TimerValue=360000; _timeStamp=1234;
    }
    size_t ypaworld_func148(ypaworld_arg148 *arg) override {
        ++buildingUpdates; lastBuilding=arg->blg_ID; return 1;
    }
    void ypaworld_func159(yw_arg159 *arg) override { ++messages; lastMessage=arg->MsgID; }
};
}
int main() {
    for (int type : {TMapSuperItem::TYPE_BOMB,TMapSuperItem::TYPE_WAVE}) {
        for (int state : {1,2,3}) {
            RestoreWorld world(type);
            ScriptParser::HandlersList handlers{new World::Parsers::SaveSuperBombParser(&world)};
            Check(ScriptParser::ParseStringList({"begin_superbomb","num = 0",
                "status = "+std::to_string(state),"active_timestamp = 1","trigger_timestamp = 777",
                "activated_by = 4","countdown = 1048982","last_ten_sec = 102","last_sec = 9",
                "radius = 123","last_radius = 99","hit_unit = 456","end"},handlers,0),
                "production save parser restores Super Item state");
            const auto &item=world._levelInfo.SuperItems[0];
            Check(item.State==state && world.buildingUpdates==1 && world.lastBuilding==100+state,
                  "restoration updates the saved state and corresponding building");
            Check(!world._userUnit && world.messages==0,
                  "restoration is silent before player control is available");
            Check(item.ActiveTime==1 && item.TriggerTime==777 && item.ActivateOwner==4 &&
                  item.CountDown==1048982 && item.LastTenSec==102 && item.LastSec==9,
                  "restoration preserves the saved owner and timer fields");
            Check(item.CurrentRadius==123 && item.LastRadius==99 && item.CustomHitUnitGids.size()==1 &&
                  item.CustomHitUnitGids[0]==456,"restoration preserves wave progress and previously hit units");
        }
        RestoreWorld gameplay(type);
        gameplay.sub_4D12D8(0,0);
        Check(gameplay.messages==1 && gameplay.lastMessage==(type==TMapSuperItem::TYPE_BOMB?70:74),
              "normal gameplay activation retains its announcement");
        Check(gameplay._levelInfo.SuperItems[0].ActiveTime==1234 &&
              gameplay._levelInfo.SuperItems[0].CountDown==360000,
              "normal gameplay activation retains its fresh countdown");
        gameplay.sub_4D1594(0);
        Check(gameplay.messages==2 && gameplay.lastMessage==(type==TMapSuperItem::TYPE_BOMB?72:76),
              "normal gameplay freezing retains its announcement");
    }
    std::printf("CHECKS=%d FAILED=%d\n",checks,failures);
    return failures ? 1 : 0;
}
