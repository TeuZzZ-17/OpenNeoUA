#include "game/yw.h"
#include "world/database_catalog.h"
#include "../parsers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#undef main
namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *label) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", label); } }
struct WorldProbe : NC_STACK_ypaworld {
    WorldProbe() { _vhclProtos.resize(8); _weaponProtos.resize(8); _buildProtos.resize(8); _roboProtos.resize(1); }
};
bool Parse(WorldProbe &world, Engine::StringList lines, int category) {
    ScriptParser::HandlersList handlers;
    if (category == World::Database::Vehicle) handlers.push_back(new World::Parsers::VhclProtoParser(&world));
    else if (category == World::Database::Building) handlers.push_back(new World::Parsers::BuildProtoParser(&world));
    else handlers.push_back(new World::Parsers::WeaponProtoParser(&world));
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
const World::Database::Rating *Rating(const World::Database::Card &c, World::Database::Metric m) {
    const auto it = std::find_if(c.ratings.begin(), c.ratings.end(), [m](const auto &r) { return r.kind == m; });
    return it == c.ratings.end() ? nullptr : &*it;
}
void TestAuthoredParserFields() {
    WorldProbe w;
    Check(Parse(w, {"new_vehicle 1", "model = tank", "vehicle_description = Heavy probe", "purchase_cost = 12345", "shield = 16", "energy = 900", "radar = 4", "type_icon = A", "end"}, World::Database::Vehicle), "vehicle metadata parses");
    const auto &v = w._vhclProtos[1];
    Check(v.script_class == "tank" && v.description == "Heavy probe", "vehicle class and description persist");
    Check(v.purchase_cost == 12345 && v.shield_defined && v.energy_defined && v.radar_defined && v.type_icon_defined, "vehicle authored flags persist");
    Check(Parse(w, {"new_building 2", "model = building", "building_description = Relay", "energy = 77", "type_icon = B", "end"}, World::Database::Building), "building metadata parses");
    Check(w._buildProtos[2].script_class == "building" && w._buildProtos[2].description == "Relay" && w._buildProtos[2].energy_defined, "building metadata persists");
}
void TestCatalogEligibilityAndRatings() {
    WorldProbe w;
    auto &v = w._vhclProtos[1]; v.Index = 1; v.name = "Tank"; v.type_icon = 'A'; v.type_icon_defined = true; v.description = "desc"; v.script_class = "tank"; v.purchase_cost = 50; v.energy = 900; v.energy_defined = true; v.shield = 16; v.shield_defined = true; v.force = 10; v.airconst = 2; v.mass = 5; v.maxrot = 1; v.weapon = 1;
    auto &w1 = w._weaponProtos[1]; w1.energy = 100;
    auto &w2 = w._weaponProtos[2]; w2.energy = 25;
    v.extra_weapons[0] = 2;
    auto &hidden = w._vhclProtos[2]; hidden.Index = 2; hidden.type_icon = 'I'; hidden.type_icon_defined = true;
    auto &b = w._buildProtos[3]; b.Index = 3; b.TypeIcon = 'B'; b.type_icon_defined = true; b.Name = "Base"; b.Energy = 80; b.energy_defined = true;
    World::TBuildingProto::TGun gun; gun.VhclID = 1; b.Guns.push_back(gun); b.Guns.push_back(gun);
    World::Database::Catalog catalog; catalog.Build(w);
    Check(catalog.Entries(World::Database::Vehicle).size() == 1 && catalog.Entries(World::Database::Vehicle)[0] == 1, "catalog uses authored eligibility only");
    Check(catalog.Entries(World::Database::Building).size() == 1, "building eligibility uses authored icon");
    const auto card = catalog.Describe(w, World::Database::Vehicle, 1);
    const auto *attack = Rating(card, World::Database::Metric::Attack); const auto *defense = Rating(card, World::Database::Metric::Defense);
    Check(card.valid && card.description == "desc" && card.purchaseCost == 50 && attack && attack->value == 125, "vehicle card metadata and attack");
    Check(defense && defense->blocks == 2, "defense rounds 16 to two blocks");
    const auto building = catalog.Describe(w, World::Database::Building, 3);
    Check(building.valid && building.guns == 2 && building.weapons.size() == 2, "building counts repeated guns and linked weapons");
    Check(Rating(building,World::Database::Metric::Attack)->value==250,"building attack counts every mounted gun");
    Check(Rating(card,World::Database::Metric::Speed)->value==3,"speed uses the supplied formula and rounds the result");
    Check(catalog.Entries(World::Database::Weapon).empty(),"no standalone weapons category");
    for(int shield:{0,13,15,16,100})
    {
        v.shield=shield;
        const auto c=catalog.Describe(w,0,1);
        const int expected=shield==100?10:(shield==16?2:(shield==0?0:1));
        Check(Rating(c,World::Database::Metric::Defense)->blocks==expected,"defense block boundary");
    }
    Check(Parse(w,{"modify_vehicle 1","shield = 300","end"},0),"large shield does not abort parser");
    Check(!Rating(catalog.Describe(w,0,1),World::Database::Metric::Defense),"shield above 100 omitted before byte conversion");
    Check(Parse(w,{"modify_vehicle 1","purchase_cost = bad","end"},0)&&v.purchase_cost<0,"invalid purchase cost is omitted");
    Check(Parse(w,{"modify_vehicle 1","purchase_cost = 0","end"},0)&&v.purchase_cost==0,"zero purchase cost remains explicitly defined");
    Check(Parse(w,{"modify_vehicle 1","production_cost = 99","end"},0)&&v.purchase_cost==0,"production cost cannot change purchase cost");
    Check(catalog.Describe(w,0,1).versus.empty(),"implicit combat jobs do not invent VS rows");
    Check(Parse(w,{"modify_vehicle 1","job_fighttank = 7","job_fightplane = 11","spawn_count = 3","power = 40","end"},0),"optional metadata parses");
    const auto optional=catalog.Describe(w,0,1);
    Check(optional.versus.size()==1&&optional.versus[0].second==7,"only authored VS in range 1..10 shown");
    Check(optional.guardians==3&&optional.healingPower==40,"guardians and healing use authored values");
    World::TRoboGun mount;mount.robo_gun_type=3;
    auto &gunVehicle=w._vhclProtos[3];gunVehicle.Index=3;gunVehicle.weapon=1;
    v.unit_guns={mount,mount};
    World::TRoboGun cycle;cycle.robo_gun_type=1;gunVehicle.unit_guns={cycle};
    Check(Rating(catalog.Describe(w,0,1),World::Database::Metric::Attack)->value==325,"repeated mounts sum independently and cycles stop");
    gunVehicle.mgun_energy_set=true;gunVehicle.mgun_energy=30;
    Check(Rating(catalog.Describe(w,0,1),World::Database::Metric::Attack)->value==385,"standalone MGUN energy is counted for every mounted gun");
    v.airconst=0;
    Check(!Rating(catalog.Describe(w,0,1),World::Database::Metric::Speed),"invalid speed denominator omitted");
    v.type_icon=0;catalog.Build(w);
    Check(catalog.Entries(0).empty(),"null icon omitted");
    v.type_icon='A';v.type_icon_defined=false;catalog.Build(w);
    Check(catalog.Entries(0).empty(),"implicit icon omitted");
    Check(Parse(w,{"new_vehicle 1","model = tank","end"},0)&&!v.type_icon_defined&&v.purchase_cost<0&&v.description.empty(),"new definition resets optional metadata");

}
void TestScoreAndClassNames() {
    WorldProbe w;
    auto &low=w._vhclProtos[1];low.Index=1;low.type_icon='A';low.type_icon_defined=true;
    low.energy_defined=true;low.energy=100;low.shield_defined=true;low.shield=20;
    low.force=10;low.airconst=2;low.mass=5;low.maxrot=1;low.weapon=1;
    w._weaponProtos[1].energy=100;w._weaponProtos[2].energy=200;
    auto &high=w._vhclProtos[2];high=low;high.Index=2;high.energy=200;high.shield=40;high.force=20;high.weapon=2;
    auto &zero=w._vhclProtos[3];zero.Index=3;zero.type_icon='C';zero.type_icon_defined=true;zero.mass=0;zero.weapon=-1;
    auto &b=w._buildProtos[1];b.Index=1;b.TypeIcon='B';b.type_icon_defined=true;b.energy_defined=true;b.Energy=50;
    World::Database::Catalog catalog;catalog.Build(w);
    const auto weak=catalog.Describe(w,0,1),strong=catalog.Describe(w,0,2);
    Check(strong.score==10&&weak.score>=1&&weak.score<strong.score,"normalized score rewards stronger displayed attributes");
    Check(catalog.Describe(w,0,3).score==1,"zero strength still has the requested minimum one block");
    Check(catalog.Describe(w,2,1).score>=1&&catalog.Describe(w,2,1).score<=10,"buildings use the shared score scale");
    low.purchase_cost=999999;low.production_cost=999999;
    Check(catalog.Describe(w,0,1).score==weak.score,"costs do not alter strength score");
    Check(catalog.Describe(w,0,7).score==0,"invalid cards have no score");
    for(const auto &item:std::vector<std::pair<std::string,std::string>>{{"kraftwerk","Power Station"},{"defcenter","Flak Station"},{"radar","Radar Station"},{"building","building"}})
    {b.script_class=item.first;Check(catalog.Describe(w,2,1).className==item.second&&b.script_class==item.first,"building class uses display alias without changing its model");}
    auto &support=w._buildProtos[2];support=b;support.Index=2;support.Power=100;support.spawn_count_defined=true;support.spawn_count=4;support.radar_defined=true;support.radar=5;
    catalog.Build(w);
    Check(catalog.Describe(w,2,2).score>catalog.Describe(w,2,1).score,"healing guardians and radar contribute to score");
    WorldProbe empty;catalog.Build(empty);Check(catalog.Entries(0).empty(),"empty catalog resets score references safely");
}
}
int main() {
    TestAuthoredParserFields();
    TestCatalogEligibilityAndRatings();
    TestScoreAndClassNames();
    std::printf("database_catalog_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
