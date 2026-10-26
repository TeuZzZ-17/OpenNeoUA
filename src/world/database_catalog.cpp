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

double Positive(double value)
{
    return std::isfinite(value) ? std::max(0.0,value) : 0.0;
}

double EffectiveDurability(double energy,double shield)
{
    // A 100% shield is bounded for ranking; it must never divide by zero.
    return Positive(energy)/std::max(0.01,1.0-std::min(100.0,Positive(shield))/100.0);
}

double WeaponQuality(const TWeapProto &w,double count)
{
    double targetMultiplier=0;
    for(int type=VEHICLE_COMBAT_CLASS_HELI;type<VEHICLE_COMBAT_CLASS_COUNT;++type)
    {
        float specific=1;
        if(!TryGetSpecificWeaponEnergy(w,static_cast<VehicleCombatClass>(type),&specific))
        {
            switch(type)
            {
            case VEHICLE_COMBAT_CLASS_HELI:
                if(w.energy_heli_defined)specific=w.energy_heli;
                break;
            case VEHICLE_COMBAT_CLASS_TANK:case VEHICLE_COMBAT_CLASS_CAR:
                if(w.energy_tank_defined)specific=w.energy_tank;
                break;
            case VEHICLE_COMBAT_CLASS_ROBO:
                if(w.energy_robo_defined)specific=w.energy_robo;
                break;
            case VEHICLE_COMBAT_CLASS_PLANE:case VEHICLE_COMBAT_CLASS_GLIDER:
            case VEHICLE_COMBAT_CLASS_ZEPPELIN:case VEHICLE_COMBAT_CLASS_UFO:
                if(w.energy_flyer_defined)specific=w.energy_flyer;
                break;
            }
        }
        targetMultiplier+=Positive(specific);
    }
    targetMultiplier/=VEHICLE_COMBAT_CLASS_COUNT-1;
    double interval=std::max(1,w.shot_time);
    if(w.salve_shots>0&&w.salve_delay>0)
        interval=((w.salve_shots-1.0)*interval+w.salve_delay)/w.salve_shots;
    if(w.IsLaser())
    {
        interval=std::max(1,w.laser_energy_tick_time);
        count=std::max(1,w.laser_beam_count);
    }
    if(w.IsArtilleryShell())
    {
        if(w.artillery_shell_barrage_shots<=0||w.artillery_shell_max_range<=0)return 0;
        interval=(Positive(w.artillery_shell_barrage_cooldown)+
                  (w.artillery_shell_barrage_shots-1.0)*Positive(w.artillery_shell_barrage_shot_delay))/w.artillery_shell_barrage_shots;
    }
    const double damage=Positive(w.energy)*targetMultiplier+Positive(w.blast_energy);
    // Reach is a bounded advantage, not a claim that every shot connects.
    const double speed=w.start_speed>0?w.start_speed:(w.airconst>0?w.force/w.airconst:0);
    const double lifetime=w.life_time_max>0?(w.life_time_min+w.life_time_max)*0.5:w.life_time;
    double reach=Positive(speed)*Positive(lifetime)/1000.0;
    if(w.IsArtilleryShell())reach=Positive(w.artillery_shell_max_range);
    const double reachBonus=1.0+0.5*reach/(1200.0+reach);
    return damage*Positive(count)*1000.0/std::max(1.0,interval)*reachBonus;
}

std::array<double,4> VehicleQuality(int id,const std::vector<TVhclProto> &vehicles,
                                  const std::vector<TWeapProto> &weapons,std::set<int> &stack)
{
    std::array<double,4> result{};
    if(!Valid(id,vehicles.size())||!stack.insert(id).second)return result;
    const auto &v=vehicles[id];
    double primary=0;int slots=0;
    auto weapon=[&](int wid,double count) {return Valid(wid,weapons.size())?WeaponQuality(weapons[wid],count):0.0;};
    const double count=std::max(1.0,v.num_weapons_max>0?
        (v.num_weapons_min+v.num_weapons_max)*0.5:(double)v.num_weapons);
    auto addPrimary=[&](int wid,double amount) {
        if(!Valid(wid,weapons.size()))return;
        const double value=weapon(wid,amount);
        primary=v.weapon_ai_switch_mode==TVhclProto::WEAPON_AI_SWITCH_MODE_SMART?std::max(primary,value):primary+value;
        ++slots;
    };
    addPrimary(v.weapon,count);
    for(size_t i=0;i<v.extra_weapons.size();++i)if(v.extra_weapons[i]>0)
    {
        const double extra=v.extra_num_weapons_max[i]>0?
            (v.extra_num_weapons_min[i]+v.extra_num_weapons_max[i])*0.5:
            (v.extra_num_weapons[i]>0?v.extra_num_weapons[i]:count);
        addPrimary(v.extra_weapons[i],extra);
    }
    // Alternative slots share a firing channel. Mounted guns fire independently.
    if(slots>0&&v.weapon_ai_switch_mode!=TVhclProto::WEAPON_AI_SWITCH_MODE_SMART)primary/=slots;
    result[0]=primary;
    const bool hasMgun=v.mgun_set?Valid(v.mgun,weapons.size()):v.mgun_shot_time>0;
    if(hasMgun)
    {
        const int cadence=v.mgun_shot_time>0?v.mgun_shot_time:weapons[v.mgun].shot_time;
        // MGUN damage comes from the vehicle, not the referenced visual weapon.
        result[0]+=v.mgun_energy_set?Positive(v.mgun_energy)*1000.0/std::max(1,cadence):Positive(v.gun_power);
    }
    if(v.proximity_defense_enable&&Valid(v.proximity_defense_weapon,weapons.size()))
    {
        const auto &defense=weapons[v.proximity_defense_weapon];
        const double shots=std::max(1,v.proximity_defense_shots);
        const double cycle=std::max(1.0,std::max((double)v.proximity_defense_interval,
            shots*Positive(v.proximity_defense_sequence_delay)));
        result[0]+=weapon(v.proximity_defense_weapon,shots)*std::max(1,defense.shot_time)/cycle;
    }
    result[1]=EffectiveDurability(v.energy,v.shield);
    if(v.model_id!=BACT_TYPES_GUN&&v.airconst>0&&v.mass>0)
        result[2]=Positive(v.force/v.airconst)*(1.0-std::exp(-2.0*v.airconst/v.mass))*
                  (0.8+0.4*Positive(v.maxrot)/(1.0+Positive(v.maxrot)));
    if(v.power>0&&v.power_radius>0)result[3]+=v.power*std::sqrt(v.power_radius/1200.0);
    result[3]+=std::max(0,(int)v.radar-1)*25.0;
    auto mounts=[&](const std::vector<TRoboGun> &guns) {
        for(const auto &gun:guns)if(gun.robo_gun_type>0)
        {
            const auto mounted=VehicleQuality(gun.robo_gun_type,vehicles,weapons,stack);
            result[0]+=mounted[0];result[3]+=mounted[3];
            if(gun.protect)result[1]+=mounted[1];
        }
    };
    mounts(v.unit_guns);if(v.RoboProto)mounts(v.RoboProto->guns);
    if(v.spawn_units&&v.spawn_count>0&&v.spawn_vehicle>0&&v.spawn_interval>0)
    {
        const auto spawned=VehicleQuality(v.spawn_vehicle,vehicles,weapons,stack);
        result[3]+=std::sqrt(spawned[0]*spawned[1])*v.spawn_count*1000.0/v.spawn_interval;
    }
    stack.erase(id);
    for(auto &value:result)value=Positive(value);
    return result;
}

void SetQuality(Card &card,const NC_STACK_ypaworld &world,int category,int id)
{
    std::set<int> stack;
    const auto &vehicles=world.GetVhclProtos();const auto &weapons=world.GetWeaponsProtos();
    if(category==Vehicle)card.quality=VehicleQuality(id,vehicles,weapons,stack);
    else
    {
        const auto &b=world.GetBuildProtos()[id];
        card.quality[1]=EffectiveDurability(b.Energy,b.shield);
        card.quality[3]=Positive(b.Power)+std::max(0,b.radar-1)*25.0;
        for(const auto &gun:b.Guns)card.quality[0]+=VehicleQuality(gun.VhclID,vehicles,weapons,stack)[0];
        if(b.spawn_units&&b.spawn_count>0&&b.spawn_vehicle>0&&b.spawn_interval>0)
        {
            const auto spawned=VehicleQuality(b.spawn_vehicle,vehicles,weapons,stack);
            card.quality[3]+=std::sqrt(spawned[0]*spawned[1])*b.spawn_count*1000.0/b.spawn_interval;
        }
    }
}

double ScoreSum(const Card &card,const std::array<double,4> &maxima)
{
    std::array<double,4> normalized{};
    for(size_t i=0;i<normalized.size();++i)if(maxima[i]>0)
        normalized[i]=std::sqrt(std::min(1.0,Positive(card.quality[i])/maxima[i]));
    // Combat and support are alternative roles. Empty guns or authored AI
    // preferences earn nothing; health and shield form one durability measure.
    return 0.5*std::max(normalized[0],normalized[3])+0.3*normalized[1]+0.2*normalized[2]+
           0.1*std::min(normalized[0],normalized[3]);
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
        cards.push_back(RawCard(world,category,id));SetQuality(cards.back(),world,category,id);
    }
    // The upper decile is the reference. A single extreme modded prototype
    // must not compress every ordinary unit into the first score block.
    for(size_t i=0;i<scoreMaxima_.size();++i)
    {
        std::vector<double> values;
        for(const auto &card:cards)if(card.quality[i]>0)values.push_back(card.quality[i]);
        if(values.empty())continue;
        std::sort(values.begin(),values.end());
        scoreMaxima_[i]=values[(size_t)std::ceil(0.9*(values.size()-1))];
    }
    for(const auto &card:cards)scorePeak_=std::max(scorePeak_,ScoreSum(card,scoreMaxima_));
}

const std::vector<int> &Catalog::Entries(int category) const { static const std::vector<int> empty; return category >= Vehicle && category <= Building ? entries_[category] : empty; }

Card Catalog::Describe(const NC_STACK_ypaworld &world, int category, int id) const
{
    Card card=RawCard(world,category,id);
    if(card.valid)SetQuality(card,world,category,id);
    if(card.valid)card.score=scorePeak_>0?std::max(1,std::min(10,(int)std::lround(10*ScoreSum(card,scoreMaxima_)/scorePeak_))):1;
    return card;
}

}}
