#include "database_catalog.h"
#include "game/yw.h"
#include "world/protos.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace World { namespace Database {
namespace {

bool Valid(int id, size_t count) { return id >= 0 && static_cast<size_t>(id) < count; }

bool EligibleVehicle(const TVhclProto &p) { return p.Index >= 0 && p.type_icon_defined && p.type_icon != 0 && p.type_icon != static_cast<uint32_t>('I'); }
bool EligibleBuilding(const TBuildingProto &p) { return p.Index >= 0 && p.type_icon_defined && p.TypeIcon != 0 && p.TypeIcon != static_cast<uint32_t>('I'); }

void AddRating(Card &card, Metric kind, double value, int blocks = -1)
{
    if (std::isfinite(value)) card.ratings.push_back({kind, value, blocks});
}

void AddWeaponIds(const TVhclProto &v, std::vector<int> &out, size_t count,
                  const std::vector<TVhclProto> &vehicles, std::set<int> &visited)
{
    if (!visited.insert(static_cast<int>(&v - vehicles.data())).second) return;
    auto add = [&](int id) { if (Valid(id, count) && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id); };
    add(v.weapon);
    for (int id : v.extra_weapons) if (id > 0) add(id);
    if (v.mgun_set) add(v.mgun);
    if (v.proximity_defense_enable) add(v.proximity_defense_weapon);
    auto mounts = [&](const std::vector<TRoboGun> &guns) {
        for (const TRoboGun &gun : guns)
            if (Valid(gun.robo_gun_type, vehicles.size()))
                AddWeaponIds(vehicles[static_cast<size_t>(gun.robo_gun_type)], out, count, vehicles, visited);
    };
    mounts(v.unit_guns);
    if (v.RoboProto) mounts(v.RoboProto->guns);
}

void AddMountedGuns(const TVhclProto &v, int &count)
{
    count += static_cast<int>(v.unit_guns.size());
    if (v.RoboProto) count += static_cast<int>(v.RoboProto->guns.size());
}

int AttackForVehicle(int id, const std::vector<TVhclProto> &vehicles,
                     const std::vector<TWeapProto> &weapons, std::set<int> &stack)
{
    if (!Valid(id, vehicles.size()) || !stack.insert(id).second) return 0;
    const TVhclProto &v = vehicles[static_cast<size_t>(id)];
    long long total = 0;
    auto addWeapon = [&](int wid) { if (Valid(wid, weapons.size())) total += weapons[static_cast<size_t>(wid)].energy; };
    addWeapon(v.weapon);
    for (int wid : v.extra_weapons) if (wid > 0) addWeapon(wid);
    if (v.mgun_set&&Valid(v.mgun,weapons.size()))addWeapon(v.mgun);
    else if(v.mgun_energy_set)total+=std::max(0.0,(double)v.mgun_energy);
    if (v.proximity_defense_enable) addWeapon(v.proximity_defense_weapon);
    auto mounts = [&](const std::vector<TRoboGun> &guns) {
        for (const TRoboGun &gun : guns)
            if (Valid(gun.robo_gun_type, vehicles.size()))
                total += AttackForVehicle(gun.robo_gun_type, vehicles, weapons, stack);
    };
    mounts(v.unit_guns);
    if (v.RoboProto) mounts(v.RoboProto->guns);
    stack.erase(id);
    return total > 2147483647LL ? 2147483647 : static_cast<int>(std::max(0LL, total));
}

void AddVersus(Card &card, const TVhclProto &v)
{
    const std::pair<const char *, int> values[] = {
        {"tank", v.job_fighttank}, {"helicopter", v.job_fighthelicopter},
        {"flyer", v.job_fightflyer}, {"robo", v.job_fightrobo},
        {"plane", v.job_fightplane}, {"cruiser", v.job_fightcruiser},
        {"glider", v.job_fightglider}, {"zeppelin", v.job_fightzeppelin},
        {"ufo", v.job_fightufo}, {"car", v.job_fightcar}, {"gun", v.job_fightgun}
    };
    const bool defined[] = {v.job_fighttank_defined, v.job_fighthelicopter_defined,
        v.job_fightflyer_defined, v.job_fightrobo_defined, v.job_fightplane_defined,
        v.job_fightcruiser_defined, v.job_fightglider_defined, v.job_fightzeppelin_defined,
        v.job_fightufo_defined, v.job_fightcar_defined, v.job_fightgun_defined};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (defined[i] && values[i].second >= 1 && values[i].second <= 10)
            card.versus.push_back({values[i].first, values[i].second});
}

std::string DisplayClass(const std::string &name)
{
    if(!StriCmp(name,"robo"))return "Host Station";
    if(!StriCmp(name,"kraftwerk"))return "Power Station";
    if(!StriCmp(name,"defcenter"))return "Flak Station";
    if(!StriCmp(name,"radar"))return "Radar Station";
    return name;
}

void DescribeVehicle(Card &card, const TVhclProto &v, const std::vector<TVhclProto> &vehicles,
                     const std::vector<TWeapProto> &weapons)
{
    card.valid = true; card.name = v.name; card.description = v.description;
    card.className = DisplayClass(v.script_class); card.icon = v.type_icon; card.purchaseCost = v.purchase_cost;
    card.productionCost = v.production_cost_defined ? v.production_cost : -1;
    std::set<int> attackStack;
    AddRating(card, Metric::Attack, AttackForVehicle(static_cast<int>(&v - vehicles.data()), vehicles, weapons, attackStack), -2);
    if (v.shield_defined && v.shield <= 100) AddRating(card, Metric::Defense, v.shield, static_cast<int>(std::floor(v.shield / 10.0 + 0.499999)));
    if (v.energy_defined && v.energy >= 0) AddRating(card, Metric::Health, v.energy, -2);
    const double den = static_cast<double>(v.airconst);
    const double rotDen = 1.0 + static_cast<double>(v.maxrot);
    if (std::isfinite(v.force) && std::isfinite(v.airconst) && std::isfinite(v.mass) && std::isfinite(v.maxrot) &&
        v.airconst > 0.0 && v.mass > 0.0 && rotDen > 0.0)
        AddRating(card, Metric::Speed, std::round((v.force / den) * (1.0 - std::exp(-2.0 * v.airconst / v.mass)) *
                                      (0.8 + 0.4 * v.maxrot / rotDen)));
    std::set<int> visited; AddWeaponIds(v, card.weapons, weapons.size(), vehicles, visited);
    card.healingPower=std::max(0,v.power);
    card.guardians=v.spawn_count_defined?std::max(0,v.spawn_count):0;
    AddMountedGuns(v, card.guns); AddVersus(card, v); card.radarPower = v.radar_defined ? std::max(0, static_cast<int>(v.radar)) : 0;
}

void DescribeBuilding(Card &card, const TBuildingProto &b, const std::vector<TVhclProto> &vehicles,
                      const std::vector<TWeapProto> &weapons)
{
    card.valid = true; card.name = b.Name; card.description = b.description; card.className = DisplayClass(b.script_class);
    card.icon = b.TypeIcon; card.productionCost = b.production_cost_defined ? b.production_cost : -1;
    if(b.shield>=0&&b.shield<=100)AddRating(card,Metric::Defense,b.shield,(int)std::floor(b.shield/10.0+0.499999));
    card.radarPower=b.radar_defined?b.radar:0;
    if (b.energy_defined && b.Energy >= 0) AddRating(card, Metric::Health, b.Energy, -2);
    card.healingPower=b.Power;
    card.guardians=b.spawn_count_defined?std::max(0,b.spawn_count):0;
    for (const TBuildingProto::TGun &gun : b.Guns) {
        ++card.guns;
        if (Valid(gun.VhclID, vehicles.size())) {
            const TVhclProto &v = vehicles[static_cast<size_t>(gun.VhclID)];
            std::set<int> stack; const size_t before = card.weapons.size(); AddWeaponIds(v, card.weapons, weapons.size(), vehicles, stack);
            (void)before;
        }
    }
    int attack = 0;
    for (const TBuildingProto::TGun &gun : b.Guns)
        if (Valid(gun.VhclID, vehicles.size())) {
            std::set<int> stack;
            attack += AttackForVehicle(gun.VhclID, vehicles, weapons, stack);
        }
    AddRating(card, Metric::Attack, attack, -2);
}

Card RawCard(const NC_STACK_ypaworld &world,int category,int id)
{
    Card card;const auto &vehicles=world.GetVhclProtos();const auto &weapons=world.GetWeaponsProtos();const auto &buildings=world.GetBuildProtos();
    if(category==Vehicle&&Valid(id,vehicles.size())&&EligibleVehicle(vehicles[id]))DescribeVehicle(card,vehicles[id],vehicles,weapons);
    else if(category==Building&&Valid(id,buildings.size())&&EligibleBuilding(buildings[id]))DescribeBuilding(card,buildings[id],vehicles,weapons);
    return card;
}

std::array<double,9> ScoreValues(const Card &card)
{
    std::array<double,9> values{};
    for(const auto &rating:card.ratings)values[static_cast<size_t>(rating.kind)]=std::max(0.0,rating.value);
    values[4]=std::max(0,card.healingPower);values[5]=std::max(0,card.guardians);
    values[6]=std::max(0,card.guns);values[7]=card.radarPower>1?card.radarPower:0;
    // Treat the VS grid as one attribute, so eleven classes do not outweigh all other stats.
    for(const auto &versus:card.versus)values[8]+=versus.second;
    if(!card.versus.empty())values[8]/=card.versus.size();
    return values;
}

double ScoreSum(const Card &card,const std::array<double,9> &maxima)
{
    const auto values=ScoreValues(card);double sum=0;
    for(size_t i=0;i<values.size();++i)if(maxima[i]>0)sum+=std::min(1.0,values[i]/maxima[i]);
    return sum;
}

} // namespace

void Catalog::Build(NC_STACK_ypaworld &world)
{
    for (auto &v : entries_) v.clear();
    const auto &vehicles = world.GetVhclProtos(); const auto &buildings = world.GetBuildProtos();
    for (size_t i = 0; i < vehicles.size(); ++i) if (EligibleVehicle(vehicles[i])) entries_[Vehicle].push_back(static_cast<int>(i));
    for (size_t i = 0; i < buildings.size(); ++i) if (EligibleBuilding(buildings[i])) entries_[Building].push_back(static_cast<int>(i));
    // One shared scale for Database and Workshop; costs are not strength attributes.
    scoreMaxima_.fill(0);scorePeak_=0;
    std::vector<Card> cards;
    for(int category:{Vehicle,Building})for(int id:entries_[category])
    {
        cards.push_back(RawCard(world,category,id));const auto values=ScoreValues(cards.back());
        for(size_t i=0;i<values.size();++i)scoreMaxima_[i]=std::max(scoreMaxima_[i],values[i]);
    }
    for(const auto &card:cards)scorePeak_=std::max(scorePeak_,ScoreSum(card,scoreMaxima_));
}

const std::vector<int> &Catalog::Entries(int category) const { static const std::vector<int> empty; return category >= Vehicle && category <= Building ? entries_[category] : empty; }

Card Catalog::Describe(const NC_STACK_ypaworld &world, int category, int id) const
{
    Card card=RawCard(world,category,id);
    if(card.valid)card.score=scorePeak_>0?std::max(1,std::min(10,(int)std::lround(10*ScoreSum(card,scoreMaxima_)/scorePeak_))):1;
    return card;
}

}}
