// Database screen: vehicle/weapon/building pages, text layout and entry media.
#include <inttypes.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/font.h"
#include "gui/button.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include <math.h>
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "system/system.h"
#include "nucleus/loaders.h"
#include "nucleus/obj3d.h"
#include "game/ypaflyer.h"
#include "common/utils.h"
#include "nucleus/IFFile.h"
#include "world/gunrotation.h"
#include "game/yw_shell_menus.h"

void UserData::ShowDatabaseMenu()
{
    if ( !database_button )
        return;

    titel_button->HideScreen();
    db_tab      = 0;
    db_page     = 0;
    db_selected = 0;
    PopulateDatabasePage();
    database_button->ShowScreen();
    EnvMode = ENVMODE_DATABASE;
}

// Truncate a name for display; never modifies the proto itself.
std::string db_trunc(const std::string &s, int max_len)
{
    if ((int)s.size() <= max_len)
        return s;
    return s.substr(0, max_len - 3) + "...";
}

// Collect valid prototype indices for the active tab.
void db_collect_valid(const UserData *usr, std::vector<int> &out)
{
    out.clear();
    const std::vector<World::TVhclProto>    &vhcls = usr->p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = usr->p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = usr->p_YW->GetBuildProtos();

    if (usr->db_tab == 0)
    {
        for (int i = 0; i < (int)vhcls.size(); i++)
            if (vhcls[i].Index >= 0 && i != usr->p_YW->GetSpectatorVehicleProtoID()) out.push_back(i);
    }
    else if (usr->db_tab == 1)
    {
        for (int i = 0; i < (int)wpns.size(); i++)
            if (!wpns[i].name.empty()) out.push_back(i);
    }
    else
    {
        for (int i = 0; i < (int)blds.size(); i++)
            if (!blds[i].Name.empty()) out.push_back(i);
    }
}

std::string db_weapon_display_name(const std::vector<World::TWeapProto> &wpns, int weaponId)
{
    if ( weaponId < 0 || weaponId >= (int)wpns.size() || wpns[weaponId].name.empty() )
        return "none";

    return wpns[weaponId].name;
}

int db_vehicle_flyer_type(const World::TVhclProto &p)
{
    IDVList::const_iterator it = p.initParams.find(NC_STACK_ypaflyer::FLY_ATT_TYPE);
    if ( it == p.initParams.end() )
        return -1;

    try
    {
        return nonstd::any_cast<int32_t>(it->second.Value);
    }
    catch ( const nonstd::bad_any_cast& )
    {
        return -1;
    }
}

std::string db_vehicle_model_display_name(const World::TVhclProto &p)
{
    if ( p.is_mimic )
        return "mimic";

    switch ( p.model_id )
    {
        case BACT_TYPES_BACT:
            return "heli";
        case BACT_TYPES_TANK:
            return "tank";
        case BACT_TYPES_ROBO:
            return "robo";
        case BACT_TYPES_MISSLE:
            return "missile";
        case BACT_TYPES_FLYER:
        {
            // Several script-facing models share the same runtime class.
            // Show the script-facing model name, not the internal class name
            // "flyer", because "flyer" is not a real SCR model keyword.
            switch ( db_vehicle_flyer_type(p) )
            {
                case 0: return "zeppelin";
                case 1: return "cruiser";
                case 2: return "glider";
                case 3: return "plane";
                default:return "plane";
            }
        }
        case BACT_TYPES_UFO:
            return "ufo";
        case BACT_TYPES_CAR:
            return "car";
        case BACT_TYPES_GUN:
            return "gun";
        default:
            return "unknown";
    }
}

std::string db_weapon_model_display_name(const World::TWeapProto &p)
{
    if ( p.IsKamikaze() )
        return "kamikaze";
    if ( p.IsLaser() )
        return "laser";
    if ( p.IsArtilleryShell() )
        return "artillery_shell";
    if ( p.IsHomingBomb() )
        return "homing_bomb";
    if ( p.IsArcGrenade() )
        return "arc_grenade";

    switch ( p._weaponFlags )
    {
        case World::TWeapProto::WEAPON_FLAGS_GRENADE: return "grenade";
        case World::TWeapProto::WEAPON_FLAGS_ROCKET:  return "rocket";
        case World::TWeapProto::WEAPON_FLAGS_MISSILE: return "missile";
        case World::TWeapProto::WEAPON_FLAGS_BOMB:    return "bomb";
        case World::TWeapProto::WEAPON_FLAGS_OBSAVOID:return "obsavoid";
        default:                                      return "unknown";
    }
}

std::string db_weapon_blast_energy_display(const World::TWeapProto &p)
{
    if ( p.blast_energy <= 0 )
        return Locale::Text::OpenUA(Locale::OUA_DB_NONE);

    // Keep the same player-facing damage scale used by the normal ATK row.
    return fmt::sprintf("%d", p.blast_energy / 100);
}

std::string db_float_display(float value)
{
    std::string out = fmt::sprintf("%.2f", value);
    while ( out.size() > 1 && out.back() == '0' )
        out.pop_back();
    if ( !out.empty() && out.back() == '.' )
        out.pop_back();
    return out;
}

std::string db_optional_float_display(float value)
{
    if ( !std::isfinite(value) || value <= 0.0f )
        return Locale::Text::OpenUA(Locale::OUA_DB_NONE);

    return db_float_display(value);
}

bool db_static_gun_support_active(const World::TVhclProto &p)
{
    return p.model_id == BACT_TYPES_GUN &&
           p.initParams.find(NC_STACK_ypagun::GUN_ATT_NO_FALL) != p.initParams.end();
}

std::string db_vehicle_job_stars(int value)
{
    // Match the vanilla advanced unit-info scale: every two job points become
    // one marker, with a minimum visible rating of one marker.
    int stars = value / 2;
    if ( stars < 1 )
        stars = 1;

    return std::string((size_t)stars, '*');
}

bool db_vehicle_has_job_stats(const World::TVhclProto &p)
{
    return p.job_fightrobo_defined || p.job_fighttank_defined ||
           p.job_fightflyer_defined || p.job_fighthelicopter_defined ||
           p.job_fightplane_defined || p.job_fightcruiser_defined ||
           p.job_fightglider_defined ||
           p.job_fightzeppelin_defined || p.job_fightufo_defined ||
           p.job_fightcar_defined || p.job_fightgun_defined ||
           p.job_conquer_defined || p.job_reconnoitre_defined;
}

std::string db_compact_pair_line(const std::string &leftLabel,
                                        const std::string &leftValue,
                                        bool hasLeft,
                                        const std::string &rightLabel,
                                        const std::string &rightValue,
                                        bool hasRight)
{
    std::string line;
    if ( hasLeft )
        line = leftLabel + ": " + leftValue;
    if ( hasRight )
    {
        if ( !line.empty() )
            line += "  ";
        line += rightLabel + ": " + rightValue;
    }
    return db_trunc(line, 30);
}

void db_add_vehicle_job_lines(std::vector<std::string> *lines,
                                     int maxLines,
                                     const World::TVhclProto &p)
{
    if ( !lines || !db_vehicle_has_job_stats(p) )
        return;

    if ( (p.job_fightrobo_defined || p.job_fighttank_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_HOST), db_vehicle_job_stars(p.job_fightrobo), p.job_fightrobo_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_TANKS), db_vehicle_job_stars(p.job_fighttank), p.job_fighttank_defined));
    }
    const bool planeDefined = p.job_fightplane_defined || p.job_fightflyer_defined;
    const int planeJob = p.job_fightplane_defined ? p.job_fightplane : p.job_fightflyer;
    if ( (planeDefined || p.job_fighthelicopter_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_PLANES), db_vehicle_job_stars(planeJob), planeDefined,
            Locale::Text::OpenUA(Locale::OUA_DB_HELIS), db_vehicle_job_stars(p.job_fighthelicopter), p.job_fighthelicopter_defined));
    }
    if ( (p.job_fightcruiser_defined || p.job_fightglider_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_CRUISER), db_vehicle_job_stars(p.job_fightcruiser), p.job_fightcruiser_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), db_vehicle_job_stars(p.job_fightglider), p.job_fightglider_defined));
    }
    if ( (p.job_fightzeppelin_defined || p.job_fightufo_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), db_vehicle_job_stars(p.job_fightzeppelin), p.job_fightzeppelin_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_UFO), db_vehicle_job_stars(p.job_fightufo), p.job_fightufo_defined));
    }
    if ( (p.job_fightcar_defined || p.job_fightgun_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_CAR), db_vehicle_job_stars(p.job_fightcar), p.job_fightcar_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_GUN), db_vehicle_job_stars(p.job_fightgun), p.job_fightgun_defined));
    }
    if ( (p.job_conquer_defined || p.job_reconnoitre_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_CONQUER), db_vehicle_job_stars(p.job_conquer), p.job_conquer_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_RECON), db_vehicle_job_stars(p.job_reconnoitre), p.job_reconnoitre_defined));
    }
}

bool db_weapon_has_energy_multipliers(const World::TWeapProto &p)
{
    return p.energy_robo_defined || p.energy_tank_defined ||
           p.energy_flyer_defined || p.energy_heli_defined ||
           p.energy_plane_defined || p.energy_cruiser_defined ||
           p.energy_glider_defined ||
           p.energy_zeppelin_defined || p.energy_ufo_defined ||
           p.energy_car_defined || p.energy_gun_defined;
}

std::string db_weapon_energy_stars(float value)
{
    if ( value <= 0.0f )
        return "-";

    // Weapon class multipliers are commonly authored in 0.5 steps:
    // 0.5 = one marker, 1.0 = two markers, 1.5 = three markers.
    int stars = (int)(value * 2.0f + 0.5f);
    if ( stars < 1 )
        stars = 1;

    return std::string((size_t)stars, '*');
}

void db_add_weapon_energy_lines(std::vector<std::string> *lines,
                                       int maxLines,
                                       const World::TWeapProto &p)
{
    if ( !lines || !db_weapon_has_energy_multipliers(p) )
        return;

    if ( (p.energy_robo_defined || p.energy_tank_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_HOST), db_weapon_energy_stars(p.energy_robo), p.energy_robo_defined,
            Locale::Text::OpenUA(Locale::OUA_DB_TANKS), db_weapon_energy_stars(p.energy_tank), p.energy_tank_defined));
    }
    const bool planeDefined = p.energy_plane_defined || p.energy_flyer_defined;
    const float planeEnergy = p.energy_plane_defined ? p.energy_plane : p.energy_flyer;
    if ( (planeDefined || p.energy_heli_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_DB_VS_PLANES), db_weapon_energy_stars(planeEnergy), planeDefined,
            Locale::Text::OpenUA(Locale::OUA_DB_HELIS), db_weapon_energy_stars(p.energy_heli), p.energy_heli_defined));
    }
    if ( (p.energy_cruiser_defined || p.energy_glider_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_CRUISER), db_weapon_energy_stars(p.energy_cruiser), p.energy_cruiser_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_GLIDER), db_weapon_energy_stars(p.energy_glider), p.energy_glider_defined));
    }
    if ( (p.energy_zeppelin_defined || p.energy_ufo_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_ZEPPELIN), db_weapon_energy_stars(p.energy_zeppelin), p.energy_zeppelin_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_UFO), db_weapon_energy_stars(p.energy_ufo), p.energy_ufo_defined));
    }
    if ( (p.energy_car_defined || p.energy_gun_defined) &&
         (int)lines->size() < maxLines )
    {
        lines->push_back(db_compact_pair_line(
            Locale::Text::OpenUA(Locale::OUA_VS_CAR), db_weapon_energy_stars(p.energy_car), p.energy_car_defined,
            Locale::Text::OpenUA(Locale::OUA_VS_GUN), db_weapon_energy_stars(p.energy_gun), p.energy_gun_defined));
    }
}

void db_add_speciality_lines(std::vector<std::string> *lines,
                                    int maxLines,
                                    const std::vector<std::string> &items)
{
    if ( !lines || (int)lines->size() >= maxLines )
        return;

    lines->push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPECIALITIES));

    if ( items.empty() )
    {
        if ( (int)lines->size() < maxLines )
            lines->push_back("- " + Locale::Text::OpenUA(Locale::OUA_DB_NONE));
        return;
    }

    size_t shown = 0;
    for (const std::string &item : items)
    {
        if ( (int)lines->size() >= maxLines )
            break;

        lines->push_back("- " + db_trunc(item, 30));
        shown++;
    }

    if ( shown < items.size() && !lines->empty() )
        lines->back() = "- ...";
}

std::vector<std::string> db_weapon_specialties(const World::TWeapProto &p)
{
    std::vector<std::string> items;

    if ( p.cluster.enable || p.cluster.generations > 0 || p.cluster.count > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_CLUSTER_WEAPON));
    if ( p.chain.allow )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_CHAIN_WEAPON));
    if ( p.armor_penetration_targets > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_ARMOR_PENETRATION));
    if ( p.debuff.valid )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_DEBUFF));
    if ( p.multi_target > 1 &&
         (p._weaponFlags == World::TWeapProto::WEAPON_FLAGS_MISSILE ||
          p.IsHomingBomb()) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MULTI_TARGET));
    return items;
}

bool db_vehicle_has_mounted_kamikaze(
    const World::TVhclProto &vehicle,
    const std::vector<World::TWeapProto> &weapons)
{
    const int weaponIds[4] = {
        vehicle.weapon,
        vehicle.extra_weapons[0],
        vehicle.extra_weapons[1],
        vehicle.extra_weapons[2]
    };

    for (int slot = 0; slot < 4; slot++)
    {
        const int weaponId = weaponIds[slot];
        if ( ((slot == 0 && weaponId >= 0) ||
              (slot > 0 && weaponId > 0)) &&
             (size_t)weaponId < weapons.size() &&
             weapons[weaponId].IsKamikaze() )
        {
            return true;
        }
    }

    return false;
}

bool db_vehicle_has_kamikaze(
    const World::TVhclProto &vehicle,
    const std::vector<World::TWeapProto> &weapons,
    const std::vector<World::TVhclProto> &vehicles)
{
    if ( db_vehicle_has_mounted_kamikaze(vehicle, weapons) )
        return true;

    auto hasAttachedKamikaze = [&weapons, &vehicles](
        const std::vector<World::TRoboGun> &guns)
    {
        for (const World::TRoboGun &gun : guns)
        {
            const int vehicleId = gun.robo_gun_type;
            if ( vehicleId > 0 &&
                 (size_t)vehicleId < vehicles.size() &&
                 db_vehicle_has_mounted_kamikaze(vehicles[vehicleId], weapons) )
            {
                return true;
            }
        }

        return false;
    };

    if ( hasAttachedKamikaze(vehicle.unit_guns) )
        return true;

    return vehicle.RoboProto && hasAttachedKamikaze(vehicle.RoboProto->guns);
}

std::vector<std::string> db_vehicle_specialties(
    const World::TVhclProto &p,
    const std::vector<World::TWeapProto> &weapons,
    const std::vector<World::TVhclProto> &vehicles)
{
    std::vector<std::string> items;

    if ( p.spawn_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWNER));
    if ( p.at_death_spawn_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWN_ON_DEATH));
    if ( p.power > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MOBILE_POWER_GENERATOR));
    if ( p.radar >= 2 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_RADAR_UNIT));
    if ( !p.unit_guns.empty() )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MOBILE_GUN_PLATFORM));
    if ( p.is_mimic )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MIMIC));
    if ( p.buff.valid && p.buff.invisible )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_INVISIBILITY));
    if ( p.buff.valid && p.buff.invulnerable )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_INVULNERABLE));
    if ( db_vehicle_has_kamikaze(p, weapons, vehicles) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_KAMIKAZE));
    if ( p.proximity_defense_enable )
        items.push_back(Locale::Text::OpenUA(p.proximity_defense_mode == 1 ? Locale::OUA_DB_PROXIMITY_DEFENSE_AT_DEATH : Locale::OUA_DB_PROXIMITY_DEFENSE));
    if ( p.extra_weapons[0] > 0 || p.extra_weapons[1] > 0 || p.extra_weapons[2] > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_MULTI_WEAPON));
    if ( db_static_gun_support_active(p) )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_STATIC_GUN_SUPPORT));

    return items;
}

std::vector<std::string> db_building_specialties(const World::TBuildingProto &p)
{
    std::vector<std::string> items;

    if ( p.Power > 0 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_POWER_GENERATOR));
    if ( p.spawn_units )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_SPAWNER));
    if ( !p.Guns.empty() )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_DEFENSIVE_GUNS));
    if ( p.ModelID == 2 )
        items.push_back(Locale::Text::OpenUA(Locale::OUA_DB_RADAR_BUILDING));

    return items;
}

std::string db_building_model_display_name(const World::TBuildingProto &p)
{
    switch ( p.ModelID )
    {
        case 0: return "building";
        case 1: return "kraftwerk";
        case 2: return "radar";
        case 3: return "defcenter";
        default:return "unknown";
    }
}

std::string db_database_asset_path(const char *kind, const char *prefix, int id, const char *ext)
{
    return fmt::sprintf("Database/%s/%s_%d.%s", kind, prefix, id, ext);
}

std::string db_database_flat_asset_path(const char *prefix, int id, const char *ext)
{
    return fmt::sprintf("Database/%s_%d.%s", prefix, id, ext);
}

const char *db_asset_kind(int tab)
{
    static const char *kinds[] = { "Units", "Weapons", "Buildings" };
    return kinds[tab >= 0 && tab < 3 ? tab : 0];
}

const char *db_asset_prefix(int tab)
{
    static const char *prefixes[] = { "vehicle", "weapon", "building" };
    return prefixes[tab >= 0 && tab < 3 ? tab : 0];
}

std::string db_database_asset_path(int tab, int id, const char *ext)
{
    return db_database_asset_path(db_asset_kind(tab), db_asset_prefix(tab), id, ext);
}

std::string db_database_flat_asset_path(int tab, int id, const char *ext)
{
    return db_database_flat_asset_path(db_asset_prefix(tab), id, ext);
}

bool db_load_text_file(const std::string &path, std::string *out)
{
    out->clear();

    FSMgr::FileHandle fil = uaOpenFile("data:" + path, "r");
    if ( !fil.OK() )
        return false;

    std::string line;
    while ( fil.ReadLine(&line) )
    {
        if ( out->size() + line.size() > 8192 )
            break;
        *out += line;
    }

    return true;
}

bool db_load_database_text(int tab, int id, std::string *out)
{
    if ( db_load_text_file(db_database_asset_path(tab, id, "txt"), out) )
        return true;

    return db_load_text_file(db_database_flat_asset_path(tab, id, "txt"), out);
}

int db_text_width(TileMap *tiles, const std::string &txt)
{
    if ( !tiles )
        return (int)txt.size() * 12;

    int width = 0;
    for (char ch : txt)
        width += tiles->map[(uint8_t)ch].w;
    return width;
}

std::string db_fit_text(TileMap *tiles, const std::string &txt, int max_width)
{
    if ( max_width <= 0 || txt.empty() )
        return std::string();

    if ( db_text_width(tiles, txt) <= max_width )
        return txt;

    const std::string ellipsis = "...";
    if ( db_text_width(tiles, ellipsis) > max_width )
    {
        std::string tiny;
        for (char ch : ellipsis)
        {
            std::string candidate = tiny + ch;
            if ( db_text_width(tiles, candidate) > max_width )
                break;
            tiny = candidate;
        }
        return tiny;
    }

    std::string out = txt;
    while ( !out.empty() && db_text_width(tiles, out + ellipsis) > max_width )
        out.pop_back();

    return out + ellipsis;
}

std::string db_centered_proto_id(int proto_id)
{
    // Fixed-width slot: names keep the same X, while two-digit IDs no longer
    // look glued to the left side of the brackets.
    if ( proto_id >= 0 && proto_id < 10 )
        return fmt::sprintf("[ %d  ]", proto_id);
    if ( proto_id >= 10 && proto_id < 100 )
        return fmt::sprintf("[ %d ]", proto_id);
    if ( proto_id >= 100 && proto_id < 1000 )
        return fmt::sprintf("[%d]", proto_id);

    return fmt::sprintf("[%d]", proto_id);
}

std::string db_make_name_row(const std::string &name,
                                    int proto_id,
                                    bool selected,
                                    int row_width)
{
    const char *sel = selected ? ">" : " ";
    std::string prefix = std::string(sel) + db_centered_proto_id(proto_id) + " ";

    TileMap *tiles = GFX::Engine.GetTileset(16);
    int name_width = row_width - db_text_width(tiles, prefix) - 8;
    if ( name_width < 24 )
        name_width = 24;

    return prefix + db_fit_text(tiles, name, name_width);
}

bool db_text_fits(TileMap *tiles, const std::string &txt, int max_width)
{
    return db_text_width(tiles, txt) <= max_width;
}

int db_detail_text_columns(NC_STACK_ypaworld *yw)
{
    // The database screen uses the old shell font through button captions.
    // TileMap pixel metrics under-report its final on-screen width, so pixel
    // wrapping still lets long Italian lore lines get clipped by the widget.
    // Use a conservative character budget scaled from the tested 1536-wide
    // shell instead. This favors readable wrapping over edge-hugging text.
    if ( !yw || yw->_screenSize.x <= 0 )
        return 38;

    int columns = (yw->_screenSize.x * 40) / 1536;
    if ( columns < 30 ) columns = 30;
    if ( columns > 52 ) columns = 52;
    return columns;
}

void db_push_wrapped_line(TileMap *tiles, const std::string &line, int max_width, int max_lines, std::vector<std::string> *out)
{
    auto push_word = [&](std::string word)
    {
        if ( word.empty() )
            return;

        if ( !out->empty() && !out->back().empty() )
        {
            std::string candidate = out->back() + " " + word;
            if ( db_text_fits(tiles, candidate, max_width) )
            {
                out->back() = candidate;
                return;
            }
        }

        while ( !word.empty() && (int)out->size() < max_lines )
        {
            int cut = (int)word.size();
            while ( cut > 1 && !db_text_fits(tiles, word.substr(0, cut), max_width) )
                cut--;

            out->push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
    };

    size_t pos = 0;
    while ( pos < line.size() && (int)out->size() < max_lines )
    {
        while ( pos < line.size() && (line[pos] == ' ' || line[pos] == '\t') )
            pos++;

        size_t start = pos;
        while ( pos < line.size() && line[pos] != ' ' && line[pos] != '\t' )
            pos++;

        push_word(line.substr(start, pos - start));
    }
}

std::vector<std::string> db_wrap_text_columns(const std::string &text, int max_columns, int max_lines)
{
    std::vector<std::string> out;
    if ( max_columns < 8 )
        max_columns = 8;

    auto push_word = [&](std::string word)
    {
        if ( word.empty() || (int)out.size() >= max_lines )
            return;

        while ( !word.empty() && (int)out.size() < max_lines )
        {
            if ( !out.empty() && !out.back().empty() && out.back() != " " )
            {
                std::string candidate = out.back() + " " + word;
                if ( (int)candidate.size() <= max_columns )
                {
                    out.back() = candidate;
                    return;
                }
            }

            if ( (int)word.size() <= max_columns )
            {
                out.push_back(word);
                return;
            }

            int cut = max_columns;
            while ( cut > 1 && cut < (int)word.size() && ((uint8_t)word[cut] & 0xC0) == 0x80 )
                cut--; // avoid splitting in the middle of a UTF-8 continuation byte
            out.push_back(word.substr(0, cut));
            word.erase(0, cut);
        }
    };

    size_t start = 0;
    while ( start <= text.size() && (int)out.size() < max_lines )
    {
        size_t end = text.find('\n', start);
        std::string line = (end == std::string::npos) ? text.substr(start) : text.substr(start, end - start);
        while ( !line.empty() && (line.back() == '\r' || line.back() == '\n') )
            line.pop_back();

        if ( line.empty() )
        {
            out.push_back(" ");
        }
        else
        {
            size_t pos = 0;
            while ( pos < line.size() && (int)out.size() < max_lines )
            {
                while ( pos < line.size() && (line[pos] == ' ' || line[pos] == '\t') )
                    pos++;

                size_t wordStart = pos;
                while ( pos < line.size() && line[pos] != ' ' && line[pos] != '\t' )
                    pos++;

                push_word(line.substr(wordStart, pos - wordStart));
            }
        }

        if ( end == std::string::npos )
            break;
        start = end + 1;
    }

    return out;
}

NC_STACK_bitmap *db_load_database_image(int tab, int id)
{
    std::string path = db_database_asset_path(tab, id, "png");
    if ( !uaFileExist("data:" + path) )
        path = db_database_flat_asset_path(tab, id, "png");

    if ( !uaFileExist("data:" + path) )
        return nullptr;

    std::string oldRsrc = Common::Env.GetPrefix("rsrc");
    Common::Env.SetPrefix("rsrc", "data:");

    NC_STACK_bitmap *img = Utils::ProxyLoadImage({
        {NC_STACK_rsrc::RSRC_ATT_NAME, path},
        {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});

    if ( !oldRsrc.empty() )
        Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( img && img->GetBitmap() )
        return img;

    if ( img )
        img->Delete();
    return nullptr;
}

void UserData::ReleaseDatabaseEntryImage()
{
    if ( db_entry_image )
    {
        db_entry_image->Delete();
        db_entry_image = nullptr;
    }
}

void db_get_entry_image_rect(NC_STACK_ypaworld *yw, int *left, int *top, int *width, int *height)
{
    const int lh = yw->_fontH + vertMenuSpace;
    const int rx = (yw->_screenSize.x * 36) / 100;
    const int rw = yw->_screenSize.x - rx;
    const int panelH = yw->_screenSize.y;
    int nav_y = panelH - lh - vertMenuSpace;
    if (nav_y < 10 * lh) nav_y = 10 * lh;

    const int detailLines = 14;
    int imgW = std::max(120, (rw * 70) / 100);
    int imgX = rx + (rw - imgW) / 2;
    int imgY = (detailLines + 3) * lh;
    int imgH = nav_y - imgY - vertMenuSpace;

    if (imgH < lh * 3) imgH = lh * 3;
    if (imgX + imgW > yw->_screenSize.x) imgW = yw->_screenSize.x - imgX;
    if (imgY + imgH > nav_y) imgH = nav_y - imgY - vertMenuSpace;
    if (imgW < 1) imgW = 1;
    if (imgH < 1) imgH = 1;

    *left = imgX;
    *top = imgY;
    *width = imgW;
    *height = imgH;
}

Common::FRect db_screen_rect_to_ndc(NC_STACK_ypaworld *yw, int left, int top, int width, int height)
{
    const float halfW = (float)yw->_screenSize.x * 0.5f;
    const float halfH = (float)yw->_screenSize.y * 0.5f;

    return Common::FRect(
        ((float)left / halfW) - 1.0f,
        ((float)top / halfH) - 1.0f,
        ((float)(left + width) / halfW) - 1.0f,
        ((float)(top + height) / halfH) - 1.0f);
}

void db_draw_frame(SDL_Surface *scr, int left, int top, int width, int height, uint8_t r, uint8_t g, uint8_t b)
{
    if ( !scr || width <= 0 || height <= 0 )
        return;

    int right = left + width - 1;
    int bottom = top + height - 1;

    GFX::GFXEngine::DrawLine(scr, Common::Line(left, top, right, top), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(right, top, right, bottom), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(right, bottom, left, bottom), r, g, b);
    GFX::GFXEngine::DrawLine(scr, Common::Line(left, bottom, left, top), r, g, b);
}

void db_fill_card_background(SDL_Surface *scr, int left, int top, int width, int height)
{
    if ( !scr || width <= 2 || height <= 2 )
        return;

    SDL_Rect rect;
    rect.x = left + 1;
    rect.y = top + 1;
    rect.w = width - 2;
    rect.h = height - 2;

    // Solid dark matte: this makes the image preview read as an intentional UI
    // card instead of a floating PNG inside a mismatched legacy border.
    SDL_FillRect(scr, &rect, SDL_MapRGB(scr->format, 5, 12, 14));
}

void UserData::RenderDatabaseEntryMedia()
{
    if ( !p_YW || db_tab < 0 || db_tab > 2 )
        return;

    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    db_get_entry_image_rect(p_YW, &left, &top, &width, &height);

    SDL_Surface *scr = GFX::Engine.Screen();
    uint8_t r = p_YW->_iniColors[68].r;
    uint8_t g = p_YW->_iniColors[68].g;
    uint8_t b = p_YW->_iniColors[68].b;

    bool hasImage = (db_entry_has_image && db_entry_image && db_entry_image->GetBitmap());

    if ( hasImage && scr )
        db_fill_card_background(scr, left, top, width, height);

    int drawX = left;
    int drawY = top;
    int drawW = width;
    int drawH = height;

    if ( hasImage )
    {
        ResBitmap *bitm = db_entry_image->GetBitmap();
        const int pad = std::max(8, p_YW->_fontH / 3);
        int maxW = std::max(1, width - pad * 2);
        int maxH = std::max(1, height - pad * 2);
        float scale = std::min((float)maxW / (float)bitm->width, (float)maxH / (float)bitm->height);
        drawW = std::max(1, (int)((float)bitm->width * scale));
        drawH = std::max(1, (int)((float)bitm->height * scale));
        drawX = left + (width - drawW) / 2;
        drawY = top + (height - drawH) / 2;

        GFX::rstr_arg204 arg;
        arg.pbitm = bitm;
        arg.float4 = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        arg.float14 = db_screen_rect_to_ndc(p_YW, drawX, drawY, drawW, drawH);
        GFX::Engine.raster_func204(&arg);
    }

    if ( scr )
    {
        // Outer frame: the stable preview card. Missing-image placeholders keep
        // using this full box so the centered "No image" caption remains visible.
        db_draw_frame(scr, left, top, width, height, r, g, b);

        // Inner frame: only when an actual PNG is drawn. It follows the real
        // aspect-fit image rectangle, so non-matching aspect ratios look
        // deliberate instead of visually offset.
        if ( hasImage )
        {
            uint8_t ir = (uint8_t)std::max(32, (int)r / 2);
            uint8_t ig = (uint8_t)std::max(32, (int)g / 2);
            uint8_t ib = (uint8_t)std::max(32, (int)b / 2);
            db_draw_frame(scr, drawX - 1, drawY - 1, drawW + 2, drawH + 2, ir, ig, ib);
        }
    }
}

void UserData::PopulateDatabasePage()
{
    if ( !database_button )
        return;

    // Fill the available vertical space; rows stay name-only and the right pane
    // carries the database text/image entry.
    // Keep this in sync with CreateDatabaseControls().
    static const int LINES = 22;

    const std::vector<World::TVhclProto>    &vhcls = p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = p_YW->GetBuildProtos();

    std::vector<int> valid;
    db_collect_valid(this, valid);
    int total = (int)valid.size();
    int total_pages = (total == 0) ? 1 : (total + LINES - 1) / LINES;

    if (db_page < 0) db_page = 0;
    if (db_page >= total_pages) db_page = total_pages - 1;

    int entries_on_page = total - db_page * LINES;
    if (entries_on_page > LINES) entries_on_page = LINES;
    if (entries_on_page < 0)    entries_on_page = 0;

    if (db_selected >= entries_on_page) db_selected = (entries_on_page > 0) ? entries_on_page - 1 : 0;
    if (db_selected < 0) db_selected = 0;

    const std::string tabLabels[] = {
        Locale::Text::OpenUA(Locale::OUA_DB_UNITS),
        Locale::Text::OpenUA(Locale::OUA_DB_WEAPONS),
        Locale::Text::OpenUA(Locale::OUA_DB_BUILDINGS)
    };
    std::string page_lbl;
    if (total == 0)
        page_lbl = fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_NO_PROTOTYPE_FORMAT), tabLabels[db_tab]);
    else
        page_lbl = fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PAGE_FORMAT),
            tabLabels[db_tab], db_page + 1, total_pages, total);
    database_button->SetText(UIWidgets::DB_LABEL_PAGE, page_lbl);

    int start = db_page * LINES;
    for (int k = 0; k < LINES; k++)
    {
        int btn_id = UIWidgets::DB_LINE_0 + k;
        int idx    = start + k;
        std::string line;

        if (idx < total)
        {
            int pid = valid[idx];
            const int listWidth = (p_YW->_screenSize.x * 34) / 100;

            if (db_tab == 0)
            {
                const World::TVhclProto &p = vhcls[pid];
                line = db_make_name_row(p.name, pid, k == db_selected, listWidth);
            }
            else if (db_tab == 1)
            {
                const World::TWeapProto &p = wpns[pid];
                line = db_make_name_row(p.name, pid, k == db_selected, listWidth);
            }
            else
            {
                const World::TBuildingProto &p = blds[pid];
                line = db_make_name_row(p.Name, pid, k == db_selected, listWidth);
            }
        }
        else
        {
            line = " ";
        }

        database_button->SetText(btn_id, line);
    }

    PopulateDetailPane();
}

void UserData::PopulateDetailPane()
{
    if ( !database_button )
        return;

    // Must match PopulateDatabasePage()/CreateDatabaseControls().
    // If this differs, selected rows on page 2+ show the wrong detail entry.
    static const int LINES = 22;
    static const int DETAIL_LINES = 14;

    const std::vector<World::TVhclProto>    &vhcls = p_YW->GetVhclProtos();
    const std::vector<World::TWeapProto>    &wpns  = p_YW->GetWeaponsProtos();
    const std::vector<World::TBuildingProto>&blds   = p_YW->GetBuildProtos();

    std::vector<int> valid;
    db_collect_valid(this, valid);
    int total = (int)valid.size();

    database_button->SetText(UIWidgets::DB_DETAIL_HEADER, Locale::Text::OpenUA(Locale::OUA_DATABASE));
    database_button->SetText(UIWidgets::DB_IMAGE_TEXT, " ");
    database_button->SetText(UIWidgets::DB_STATS_HEADER, " ");
    for (int k = 0; k < 12; k++)
        database_button->SetText(UIWidgets::DB_STATS_0 + k, " ");

    int abs_idx = db_page * LINES + db_selected;
    if (abs_idx >= total)
    {
        for (int k = 0; k < DETAIL_LINES; k++)
            database_button->SetText(UIWidgets::DB_DETAIL_0 + k, " ");
        database_button->SetText(UIWidgets::DB_STATS_HEADER, " ");
        for (int k = 0; k < 12; k++)
            database_button->SetText(UIWidgets::DB_STATS_0 + k, " ");
        ReleaseDatabaseEntryImage();
        db_entry_tab = -1;
        db_entry_id = -1;
        db_entry_has_image = false;
        return;
    }

    int pid = valid[abs_idx];
    std::string lines[DETAIL_LINES];
    for (int i = 0; i < DETAIL_LINES; i++) lines[i] = " ";

    std::string entryName;
    if (db_tab == 0)
    {
        entryName = vhcls[pid].name;
    }
    else if (db_tab == 1)
    {
        entryName = wpns[pid].name;
    }
    else
    {
        entryName = blds[pid].Name;
    }

    database_button->SetText(UIWidgets::DB_DETAIL_HEADER, db_trunc(entryName, 28));

    static const int DB_STATS_LINES = 12;
    std::vector<std::string> statLines;
    statLines.reserve(DB_STATS_LINES);
    std::string statHeader = Locale::Text::OpenUA(Locale::OUA_DB_STATS);
    if ( db_tab == 0 )
    {
        const World::TVhclProto &p = vhcls[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_vehicle_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_HP_FORMAT), p.energy));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_DEF_FORMAT), p.shield));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_WEAPON_FORMAT), db_trunc(db_weapon_display_name(wpns, p.weapon), 20)));
        statLines.push_back(p.has_push_resistance && p.push_resistance > 0.0 ?
                            fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PUSH_RESISTANCE_FORMAT), p.push_resistance) :
                            Locale::Text::OpenUA(Locale::OUA_DB_PUSH_RESISTANCE_NONE));
        db_add_vehicle_job_lines(&statLines, DB_STATS_LINES, p);
        db_add_speciality_lines(&statLines, DB_STATS_LINES,
                                db_vehicle_specialties(p, wpns, vhcls));
    }
    else if ( db_tab == 1 )
    {
        const World::TWeapProto &p = wpns[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_weapon_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_ATK_FORMAT), p.energy / 100));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_BLAST_ENERGY_FORMAT), db_weapon_blast_energy_display(p)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_PUSH_FORMAT), db_optional_float_display(p.push)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_BLAST_PUSH_FORMAT), db_optional_float_display(p.blast_push)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_WEAPON_RECOIL_FORMAT),
                            p.recoil > 0.0 ? db_float_display(p.recoil) : Locale::Text::OpenUA(Locale::OUA_DB_NONE)));
        db_add_weapon_energy_lines(&statLines, DB_STATS_LINES, p);
        db_add_speciality_lines(&statLines, DB_STATS_LINES, db_weapon_specialties(p));
    }
    else
    {
        const World::TBuildingProto &p = blds[pid];
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_MODEL_FORMAT), db_trunc(db_building_model_display_name(p), 18)));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_HP_FORMAT), p.Energy));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_POWER_FORMAT), p.Power));
        statLines.push_back(fmt::sprintf(Locale::Text::OpenUA(Locale::OUA_DB_GUNS_FORMAT), (int)p.Guns.size()));
        db_add_speciality_lines(&statLines, DB_STATS_LINES, db_building_specialties(p));
    }

    database_button->SetText(UIWidgets::DB_STATS_HEADER, statHeader);
    for (int k = 0; k < DB_STATS_LINES; k++)
        database_button->SetText(UIWidgets::DB_STATS_0 + k, k < (int)statLines.size() ? statLines[k] : " ");

    std::string body;
    if ( !db_load_database_text(db_tab, pid, &body) || body.empty() )
        body = Locale::Text::OpenUA(Locale::OUA_DB_NO_ENTRY);

    const int wrapColumns = db_detail_text_columns(p_YW);
    std::vector<std::string> wrapped = db_wrap_text_columns(body, wrapColumns, DETAIL_LINES);
    for (int i = 0; i < DETAIL_LINES && i < (int)wrapped.size(); i++)
        lines[i] = wrapped[i].empty() ? std::string(" ") : wrapped[i];

    if ( db_entry_tab != db_tab || db_entry_id != pid )
    {
        ReleaseDatabaseEntryImage();
        db_entry_image = db_load_database_image(db_tab, pid);
        db_entry_tab = db_tab;
        db_entry_id = pid;
        db_entry_has_image = (db_entry_image && db_entry_image->GetBitmap());
    }

    if ( !db_entry_has_image )
        database_button->SetText(UIWidgets::DB_IMAGE_TEXT, Locale::Text::OpenUA(Locale::OUA_DB_NO_IMAGE));

    for (int k = 0; k < DETAIL_LINES; k++)
        database_button->SetText(UIWidgets::DB_DETAIL_0 + k, lines[k]);
}
