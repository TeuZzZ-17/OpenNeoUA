// Script parser for weapon prototypes (new_weapon / modify_weapon).
#include "parsers.h"
#include "../fmtlib/core.h"
#include "../fmtlib/printf.h"
#include "game/yw.h"
#include "game/yparobo.h"
#include "game/ypaflyer.h"
#include "game/ypacar.h"
#include "common/glyph_key.h"
#include "common/log.h"
#include "common/utils.h"
#include "../system/inivals.h"
#include "spin.h"
#include "tools.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include "parsers_internal.h"

namespace World
{
namespace Parsers
{

TVhclSound *WeaponProtoParser::GetSndFxByName(const std::string &sndname)
{
    if ( !StriCmp(sndname, "normal") )
        return &_wpn->sndFXes[TWeapProto::SND_NORMAL];
    else if ( !StriCmp(sndname, "launch") )
        return &_wpn->sndFXes[TWeapProto::SND_LAUNCH];
    else if ( !StriCmp(sndname, "hit") )
        return &_wpn->sndFXes[TWeapProto::SND_HIT];

    return nullptr;
}

bool WeaponProtoParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (!StriCmp(word, "new_weapon"))
    {
        _isModify = false;
        _collID = -1;
        _wpnID = parser.stol(opt, nullptr, 0);
        _wpn = &_o._weaponProtos[_wpnID];

        *_wpn = TWeapProto();

        _wpn->unitID = 4;
        _wpn->name.clear();
        _wpn->energy = 10000;
        _wpn->blast_energy = 0;
        _wpn->blast_falloff = 0;
        _wpn->blast_push = 0.0f;
        _wpn->push = 0.0f;
        _wpn->armor_penetration_targets = 0;
        _wpn->armor_penetration_icon.clear();
        _wpn->recoil = 0.0;
        _wpn->mass = 50.0;
        _wpn->force = 5000.0;
        _wpn->airconst = 50.0;
        _wpn->maxrot = 2.0;
        _wpn->radius = 20.0;
        _wpn->trigger_radius = 0.0;
        _wpn->blast_radius = 0.0;
        _wpn->overeof = 10.0;
        _wpn->vwr_radius = 20.0;
        _wpn->vwr_overeof = 20.0;
        _wpn->energy_heli = 1.0;
        _wpn->energy_tank = 1.0;
        _wpn->energy_flyer = 1.0;
        _wpn->energy_robo = 1.0;
        _wpn->energy_plane = 1.0;
        _wpn->energy_cruiser = 1.0;
        _wpn->energy_glider = 1.0;
        _wpn->energy_zeppelin = 1.0;
        _wpn->energy_ufo = 1.0;
        _wpn->energy_car = 1.0;
        _wpn->energy_gun = 1.0;
        _wpn->radius_heli = 0;
        _wpn->radius_tank = 0;
        _wpn->radius_flyer = 0;
        _wpn->radius_robo = 0;
        _wpn->start_speed = 70.0;
        _wpn->grenade_arc_angle = 0.0f;
        _wpn->grenade_arc_gravity = 0.0f;
        _wpn->life_time = 20000;
        _wpn->life_time_min = 20000;
        _wpn->life_time_max = 20000;
        _wpn->life_time_nt = 0;
        _wpn->life_time_nt_min = 0;
        _wpn->life_time_nt_max = 0;
        _wpn->drive_time = 7000;
        _wpn->shot_time = 3000;
        _wpn->shot_time_user = 1000;
        _wpn->ramp_up_time = 0;
        _wpn->ramp_up_max_shot_time = 0;
        _wpn->ramp_up_max_weapon_spread_x = 0.0f;
        _wpn->ramp_up_max_weapon_spread_y = 0.0f;
        _wpn->ramp_up_overheat_time = 0;
        _wpn->ramp_up_overheat_hp_drain.Clear();
        _wpn->ramp_up_overheat_icon.clear();
        _wpn->salve_delay = 0;
        _wpn->salve_shots = 0;
        _wpn->multi_target = 0;
        // OpenNeoUA custom: model = laser defaults (vanilla-safe / disabled by default)
        _wpn->laser_energy_tick_time = 250;
        _wpn->laser_energy_tick_time_user = 150;
        _wpn->laser_energy_increment_rate = 0.0;
        _wpn->laser_max_energy = 0.0;
        _wpn->laser_spacing = 40.0;
        _wpn->laser_chain_allow = 0;
        _wpn->laser_chain_max_jumps = 0;
        _wpn->laser_chain_radius = 0.0;
        _wpn->laser_chain_energy_mult = 1.0;
        _wpn->laser_beam_count = 1;
        _wpn->vertical_laser_enable = false;
        _wpn->vertical_laser_ai_trigger_radius = 300.0f;
        _wpn->ai_fire_trigger_box_below = vec3d();
        _wpn->ai_fire_trigger_box_forward = vec3d();
        _wpn->ai_fire_trigger_cone_forward = vec3d();
        _wpn->ai_fire_trigger_cone_below = vec3d();
        _wpn->ai_fire_trigger_forward_max_inclination = 0.0f;
        _wpn->ai_fire_trigger_dead_zone = 0.0f;
        _wpn->fire_time_scale = 1.0f;
        _wpn->fire_time_scale_hp_drain.Clear();
        _wpn->snd_launch_at_muzzle = false;
        _wpn->vp_normal = 0;
        _wpn->vp_fire = 1;
        _wpn->vp_megadeth = 2;
        _wpn->vp_wait = 3;
        _wpn->vp_dead = 4;
        _wpn->vp_genesis = 5;
        _wpn->visual_3ds = TExternalVisualSet();
        _wpn->visual_base = TExternalVisualSet();
        _wpn->visual_scale = vec3d(1.0, 1.0, 1.0);
        _wpn->visual_rotation = vec3d(0.0, 0.0, 0.0);
        _wpn->visual_spin = vec3d(0.0, 0.0, 0.0);
        _wpn->collision_shape.clear();
        _wpn->spiral_speed = 0.0f;
        _wpn->spiral_radius = 0.0f;
        _wpn->chaos_factor = 0.0f;
        _wpn->chaos_radius = 0.0f;
        _wpn->visual_tint = TVisualTint();
        _wpn->vp_trail_scale = vec3d(1.0, 1.0, 1.0);
        _wpn->vp_trail_spin = vec3d(0.0, 0.0, 0.0);
        _wpn->vp_trail_tint = TVisualTint();
        _wpn->tracer = TWeaponTracerConfig();
        _wpn->laser_mesh = TWeapProto::TLaserMeshConfig();
        _wpn->type_icon = 65;
        _wpn->debuff_id = -1;
        _wpn->ground_decal_id = -1;
        _wpn->ground_decal_size = TDecalSizeOverride();
        _wpn->debuff = TWeaponDebuffConfig();
        _wpn->delay_time_debuff_id = 0;
        _wpn->delay_time_debuff = TWeaponDebuffConfig();
        _wpn->cluster = TWeaponClusterConfig();
        _wpn->cluster.snd.volume = 120;
        _wpn->cluster.snd.sndPrm.mag0 = 1.0;
        _wpn->cluster.snd.sndPrm.time = 1000;
        _wpn->cluster.snd.sndPrm_shk.mag0 = 1.0;
        _wpn->cluster.snd.sndPrm_shk.time = 1000;
        _wpn->cluster.snd.sndPrm_shk.mute = 0.02;
        _wpn->cluster.snd.sndPrm_shk.pos.x = 0.2;
        _wpn->cluster.snd.sndPrm_shk.pos.y = 0.2;
        _wpn->cluster.snd.sndPrm_shk.pos.z = 0.2;
        _wpn->chain = TWeaponChainConfig();

        for (TVhclSound &fx : _wpn->sndFXes)
        {
            fx.sndPrm.mag0 = 1.0;
            fx.sndPrm_shk.mag0 = 1.0;
            fx.sndPrm_shk.mute = 0.02;
            fx.sndPrm_shk.pos.x = 0.2;
            fx.sndPrm_shk.pos.y = 0.2;
            fx.sndPrm_shk.pos.z = 0.2;
            fx.volume = 120;
            fx.sndPrm.time = 1000;
            fx.sndPrm_shk.time = 1000;
        }

        // OpenNeoUA custom: player-only launch shake defaults. Slot 0 keeps the
        // feature disabled; mute 0 makes the local feedback independent from
        // third-person camera distance.
        _wpn->shk_launch_player.mag0 = 1.0;
        _wpn->shk_launch_player.time = 1000;
        _wpn->shk_launch_player.mute = 0.0;
        _wpn->shk_launch_player.pos.x = 0.2;
        _wpn->shk_launch_player.pos.y = 0.2;
        _wpn->shk_launch_player.pos.z = 0.2;

        _wpn->initParams.clear();
        return true;
    }
    else if (!StriCmp(word, "modify_weapon"))
    {
        _collID = -1;
        _wpnID = parser.stol(opt, nullptr, 0);

        if ( _wpnID < 0 || (size_t)_wpnID >= _o._weaponProtos.size() )
        {
            ypa_log_out("WARNING: modify_weapon ignored invalid prototype ID %d.\n", _wpnID);
            _wpn = nullptr;
            _isModify = false;
            return false;
        }

        _wpn = &_o._weaponProtos[_wpnID];
        _isModify = true;

        _o._upgradeWeaponId = _wpnID;
        return true;
    }

    return false;
}

int WeaponProtoParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( !_wpn->HasAIFireTriggerArea() &&
             (_wpn->ai_fire_trigger_box_below.x > 0.0f || _wpn->ai_fire_trigger_box_forward.x > 0.0 ||
              _wpn->ai_fire_trigger_cone_forward.x > 0.0 || _wpn->ai_fire_trigger_cone_below.x > 0.0) )
            ypa_log_out("WARNING: weapon %d AI fire volume ignored: select one volume compatible with its weapon model. Existing fire control retained.\n", _wpnID);
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( ParseCompoundCollisionParameter(parser, p1, p2, _wpn->coll, _collID) )
        return ScriptParser::RESULT_OK;

    if ( !StriCmp(p1, "disable_collision") )
    {
        _wpn->disable_collision = p2 == "1";
        return ScriptParser::RESULT_OK;
    }
    if ( !StriCmp(p1, "collision_shape") )
    {
        _wpn->collision_shape.clear();
        if (p2.empty() || p2 == "0") return ScriptParser::RESULT_OK;
        if (!uaNormalizeDataAssetPath(p2, &_wpn->collision_shape, false))
            ypa_log_out("WARNING: invalid collision_shape %s; existing collision retained.\n", p2.c_str());
        return ScriptParser::RESULT_OK;
    }

    if ( !StriCmp(p1, "model") )
    {
        if ( !StriCmp(p2, "grenade") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_GRENADE;
        else if ( !StriCmp(p2, "arc_grenade") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_ARC_GRENADE;
        else if ( !StriCmp(p2, "rocket") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_ROCKET;
        else if ( !StriCmp(p2, "missile") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_MISSILE;
        else if ( !StriCmp(p2, "homing_bomb") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_HOMING_BOMB;
        else if ( !StriCmp(p2, "artillery_shell") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_ARTILLERY_SHELL;
        else if ( !StriCmp(p2, "laser") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_LASER;
        else if ( !StriCmp(p2, "kamikaze") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_KAMIKAZE;
        else if ( !StriCmp(p2, "bomb") || !StriCmp(p2, "special") )
            _wpn->_weaponFlags = TWeapProto::WEAPON_FLAGS_BOMB;
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "enable") )
    {
        _wpn->enable_mask |= 1 << parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "disable") )
    {
        _wpn->enable_mask &= ~(1 << parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "name") )
    {
        _wpn->name = p2;
        std::replace(_wpn->name.begin(), _wpn->name.end(), '_', ' ');
    }
    else if ( !StriCmp(p1, "energy") )
    {
        _wpn->energy = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "blast_energy") )
    {
        ParseBoundedIntegerParam("blast_energy", p1, p2, 0, 100000000, 0, _wpn->blast_energy);
    }

    else if ( !StriCmp(p1, "blast_falloff") )
    {
        _wpn->blast_falloff = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "blast_push") )
    {
        _wpn->blast_push = ClampPushIntensity(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "push") )
    {
        _wpn->push = ClampPushIntensity(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "armor_penetration_targets") )
    {
        _wpn->armor_penetration_targets = std::max(parser.stol(p2, nullptr, 0), 0L);
    }
    else if ( !StriCmp(p1, "armor_penetration_icon") )
    {
        std::string normalizedPath;
        if ( uaNormalizeDataAssetPath(p2, &normalizedPath, true) &&
             normalizedPath.size() > 5 )
        {
            // StatusIconLoad already resolves from the Data root. Keep its
            // internal value Data-relative after validating the authored path.
            _wpn->armor_penetration_icon = normalizedPath.substr(5);
        }
        else
            _wpn->armor_penetration_icon.clear();
    }
    else if ( !StriCmp(p1, "recoil") )
    {
        _wpn->recoil = ClampRecoilMultiplier(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "debuff_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _wpn->debuff_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "ground_decal_size") )
        ParseDecalSizeOverride("ground_decal_size", p2, _wpn->ground_decal_size);
    else if ( !StriCmp(p1, "ground_decal_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _wpn->ground_decal_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "energy_heli") )
    {
        _wpn->energy_heli = parser.stof(p2, 0);
        _wpn->energy_heli_defined = true;
    }
    else if ( !StriCmp(p1, "energy_tank") )
    {
        _wpn->energy_tank = parser.stof(p2, 0);
        _wpn->energy_tank_defined = true;
    }
    else if ( !StriCmp(p1, "energy_flyer") )
    {
        _wpn->energy_flyer = parser.stof(p2, 0);
        _wpn->energy_flyer_defined = true;
    }
    else if ( !StriCmp(p1, "energy_robo") )
    {
        _wpn->energy_robo = parser.stof(p2, 0);
        _wpn->energy_robo_defined = true;
    }
    else if ( !StriCmp(p1, "energy_plane") )
    {
        _wpn->energy_plane = parser.stof(p2, 0);
        _wpn->energy_plane_defined = true;
    }
    else if ( !StriCmp(p1, "energy_cruiser") )
    {
        _wpn->energy_cruiser = parser.stof(p2, 0);
        _wpn->energy_cruiser_defined = true;
    }
    else if ( !StriCmp(p1, "energy_glider") )
    {
        _wpn->energy_glider = parser.stof(p2, 0);
        _wpn->energy_glider_defined = true;
    }
    else if ( !StriCmp(p1, "energy_zeppelin") )
    {
        _wpn->energy_zeppelin = parser.stof(p2, 0);
        _wpn->energy_zeppelin_defined = true;
    }
    else if ( !StriCmp(p1, "energy_ufo") )
    {
        _wpn->energy_ufo = parser.stof(p2, 0);
        _wpn->energy_ufo_defined = true;
    }
    else if ( !StriCmp(p1, "energy_car") )
    {
        _wpn->energy_car = parser.stof(p2, 0);
        _wpn->energy_car_defined = true;
    }
    else if ( !StriCmp(p1, "energy_gun") )
    {
        _wpn->energy_gun = parser.stof(p2, 0);
        _wpn->energy_gun_defined = true;
    }
    else if ( !StriCmp(p1, "mass") )
    {
        _wpn->mass = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "force") )
    {
        _wpn->force = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "maxrot") )
    {
        _wpn->maxrot = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "airconst") )
    {
        _wpn->airconst = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "radius") )
    {
        _wpn->radius = parser.stof(p2, 0);
        _wpn->radius_defined = true;
    }
    else if ( !StriCmp(p1, "trigger_radius") )
    {
        // Canonical model=kamikaze fuse value. Zero, absent, negative,
        // malformed and non-finite values all mean physical contact.
        _wpn->trigger_radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "fire_time_scale") )
    {
        float scale = parser.stof(p2, 0);
        if ( !std::isfinite(scale) || scale <= 0.0f || scale >= 1.0f )
            _wpn->fire_time_scale = 1.0f;
        else
            _wpn->fire_time_scale = std::max(0.05f, scale);
    }
    else if ( !StriCmp(p1, "fire_time_scale_hp_drain") )
    {
        ParseAbsoluteOrPercent(p2, _wpn->fire_time_scale_hp_drain, 100.0f);
    }
    else if ( !StriCmp(p1, "snd_launch_at_muzzle") )
    {
        _wpn->snd_launch_at_muzzle = p2 == "1";
    }
    else if ( !StriCmp(p1, "blast_radius") )
    {
        _wpn->blast_radius = ParseBoundedNonNegativeFiniteOrFallback(parser, p2, 12000.0f, 0.0f);
    }

    else if ( !StriCmp(p1, "radius_heli") )
    {
        _wpn->radius_heli = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "radius_tank") )
    {
        _wpn->radius_tank = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "radius_flyer") )
    {
        _wpn->radius_flyer = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "radius_robo") )
    {
        _wpn->radius_robo = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "overeof") )
    {
        _wpn->overeof = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "vwr_radius") )
    {
        _wpn->vwr_radius = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "vwr_overeof") )
    {
        _wpn->vwr_overeof = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "start_speed") )
    {
        _wpn->start_speed = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "grenade_arc_angle") )
    {
        // model = arc_grenade only: one-time launch elevation in degrees.
        _wpn->grenade_arc_angle = std::min(ParseNonNegativeIniFloatOrZero(p2), 89.0f);
    }
    else if ( !StriCmp(p1, "grenade_arc_gravity") )
    {
        // model = arc_grenade only: downward acceleration. Zero/absent/invalid
        // selects the engine-standard gravity in the runtime.
        _wpn->grenade_arc_gravity = std::min(ParseNonNegativeIniFloatOrZero(p2), 1000.0f);
    }
    else if ( !StriCmp(p1, "cluster_enable") )
    {
        _wpn->cluster.enable = parser.stol(p2, nullptr, 0) != 0;
    }
    else if ( !StriCmp(p1, "cluster_generations") )
    {
        int generations = parser.stol(p2, nullptr, 0);
        _wpn->cluster.generations = generations > 0 ? generations : 0;
    }
    else if ( !StriCmp(p1, "cluster_count") )
    {
        int countMin = 0;
        int countMax = 0;
        if ( World::ParsePositiveIntRangeValue(
                 p2, (int)TWeaponClusterConfig::MAX_COUNT,
                 countMin, countMax) )
        {
            _wpn->cluster.count = countMin;
            _wpn->cluster.count_max = countMax;
        }
        else
        {
            _wpn->cluster.count = 0;
            _wpn->cluster.count_max = 0;
        }
    }
    else if ( !StriCmp(p1, "cluster_weapon_id") )
    {
        int weaponId = parser.stol(p2, nullptr, 0);
        _wpn->cluster.weapon_id = weaponId > 0 ? weaponId : 0;
    }
    else if ( !StriCmp(p1, "cluster_trigger_time") )
    {
        int triggerTime = 0;
        int triggerTimeMax = 0;
        _wpn->cluster.trigger_time =
            ParseScriptIntRange(p2, triggerTime, triggerTimeMax) &&
            triggerTime == triggerTimeMax && triggerTime > 0
                ? triggerTime
                : 0;
    }
    else if ( !StriCmp(p1, "cluster_horizontal_angle") )
    {
        float angleMin = 0.0f;
        float angleMax = 0.0f;
        _wpn->cluster.horizontal_angle_set =
            ParseScriptFloatRange(p2, angleMin, angleMax);
        _wpn->cluster.horizontal_angle_min =
            _wpn->cluster.horizontal_angle_set ? angleMin : 0.0f;
        _wpn->cluster.horizontal_angle_max =
            _wpn->cluster.horizontal_angle_set ? angleMax : 0.0f;
    }
    else if ( !StriCmp(p1, "cluster_vertical_angle") )
    {
        float angleMin = 0.0f;
        float angleMax = 0.0f;
        _wpn->cluster.vertical_angle_set =
            ParseScriptFloatRange(p2, angleMin, angleMax);
        _wpn->cluster.vertical_angle_min =
            _wpn->cluster.vertical_angle_set ? angleMin : 0.0f;
        _wpn->cluster.vertical_angle_max =
            _wpn->cluster.vertical_angle_set ? angleMax : 0.0f;
    }
    else if ( !StriCmp(p1, "cluster_spread_xy") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            float spreadX = parser.stof(axis[0], 0);
            float spreadY = parser.stof(axis[1], 0);
            _wpn->cluster.spread_x = spreadX > 0.0 ? spreadX : 0.0;
            _wpn->cluster.spread_y = spreadY > 0.0 ? spreadY : 0.0;
        }
    }
    else if ( !StriCmp(p1, "cluster_vp") )
    {
        int vp = parser.stol(p2, nullptr, 0);
        _wpn->cluster.vp = vp > 0 ? vp : 0;
    }
    else if ( !StriCmp(p1, "cluster_3ds") )
    {
        _wpn->cluster.mesh3ds = p2;
    }
    else if ( !StriCmp(p1, "cluster_base") )
    {
        _wpn->cluster.basePath = p2;
    }
    else if ( !StriCmp(p1, "snd_cluster_sample") )
    {
        _wpn->cluster.snd.MainSample.Name = p2;
    }
    else if ( !StriCmp(p1, "snd_cluster_pitch") )
    {
        ParseSoundPitchRange(p2, _wpn->cluster.snd);
    }
    else if ( !StriCmp(p1, "snd_cluster_volume") )
    {
        _wpn->cluster.snd.volume = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "snd_cluster_radius") )
    {
        _wpn->cluster.snd.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "pal_cluster_slot") )
    {
        _wpn->cluster.snd.sndPrm.slot = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "pal_cluster_mag0") )
    {
        _wpn->cluster.snd.sndPrm.mag0 = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "pal_cluster_mag1") )
    {
        _wpn->cluster.snd.sndPrm.mag1 = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "pal_cluster_time") )
    {
        _wpn->cluster.snd.sndPrm.time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "pal_cluster_radius") )
    {
        _wpn->cluster.snd.sndPrm.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "shk_cluster_slot") )
    {
        _wpn->cluster.snd.sndPrm_shk.slot = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "shk_cluster_mag0") )
    {
        _wpn->cluster.snd.sndPrm_shk.mag0 = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "shk_cluster_mag1") )
    {
        _wpn->cluster.snd.sndPrm_shk.mag1 = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "shk_cluster_time") )
    {
        _wpn->cluster.snd.sndPrm_shk.time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "shk_cluster_radius") )
    {
        _wpn->cluster.snd.sndPrm_shk.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "shk_cluster_mute") )
    {
        _wpn->cluster.snd.sndPrm_shk.mute = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "shk_cluster_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _wpn->cluster.snd.sndPrm_shk.pos.x = parser.stof(axis[0], 0);
            _wpn->cluster.snd.sndPrm_shk.pos.y = parser.stof(axis[1], 0);
            _wpn->cluster.snd.sndPrm_shk.pos.z = parser.stof(axis[2], 0);
        }
    }
    else if ( !StriCmp(p1, "chain_allow") )
    {
        _wpn->chain.allow = parser.stol(p2, nullptr, 0) != 0;
    }
    else if ( !StriCmp(p1, "chain_max_jumps") )
    {
        int maxJumps = parser.stol(p2, nullptr, 0);
        _wpn->chain.max_jumps = maxJumps > 0 ? maxJumps : 0;
    }
    else if ( !StriCmp(p1, "chain_radius") )
    {
        float radius = parser.stof(p2, 0);
        _wpn->chain.radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "chain_energy_mult") )
    {
        float mult = parser.stof(p2, 0);
        _wpn->chain.energy_mult = mult > 0.0 ? mult : 0.0;
    }
    else if ( !StriCmp(p1, "chain_jump_delay") )
    {
        int delay = parser.stol(p2, nullptr, 0);
        _wpn->chain.jump_delay = delay > 0 ? delay : 0;
    }
    else if ( !StriCmp(p1, "life_time") )
    {
        int minLifeTime = 0;
        int maxLifeTime = 0;
        if ( ParseScriptIntRange(p2, minLifeTime, maxLifeTime) )
        {
            _wpn->life_time = minLifeTime;
            _wpn->life_time_min = minLifeTime;
            _wpn->life_time_max = maxLifeTime;
        }
        else if ( p2.find('_') == std::string::npos )
        {
            _wpn->life_time = parser.stol(p2, nullptr, 0);
            _wpn->life_time_min = _wpn->life_time;
            _wpn->life_time_max = _wpn->life_time;
        }
        else
        {
            // Do not let stol() silently accept only the first endpoint of a
            // malformed range such as 2500_3000_3500.
            _wpn->life_time = 0;
            _wpn->life_time_min = 0;
            _wpn->life_time_max = 0;
        }
    }
    else if ( !StriCmp(p1, "life_time_nt") )
    {
        int minLifeTime = 0;
        int maxLifeTime = 0;
        if ( p2.find('_') != std::string::npos )
        {
            // Range min_max; a malformed range disables the override (0).
            if ( !ParseScriptIntRange(p2, minLifeTime, maxLifeTime) )
                minLifeTime = maxLifeTime = 0;
        }
        else
        {
            minLifeTime = maxLifeTime = parser.stol(p2, nullptr, 0);
        }
        _wpn->life_time_nt = minLifeTime;
        _wpn->life_time_nt_min = minLifeTime;
        _wpn->life_time_nt_max = maxLifeTime;
    }
    else if ( !StriCmp(p1, "drive_time") )
    {
        _wpn->drive_time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "delay_time") )
    {
        _wpn->delay_time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "delay_time_debuff_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _wpn->delay_time_debuff_id = value > 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : 0;
    }
    else if ( !StriCmp(p1, "shot_time") )
    {
        _wpn->shot_time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "shot_time_user") )
    {
        _wpn->shot_time_user = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "weapon_energy_cost") )
    {
        TAuthoredScalar value;
        _wpn->weapon_energy_cost = 0.0f;
        _wpn->weapon_energy_cost_defined = false;

        if ( ParseAuthoredScalar(p2, value) && value.percent &&
             std::isfinite(value.value) && value.value >= 0.0f )
        {
            _wpn->weapon_energy_cost = std::min(value.value, 100.0f);
            _wpn->weapon_energy_cost_defined = true;
        }
        else
        {
            ypa_log_out("WARNING: weapon %d weapon_energy_cost='%s' is invalid; expected an explicit percentage in range 0%%-100%%. Falling back to legacy weapon energy drain.\n",
                        _wpnID, p2.c_str());
        }
    }
    else if ( !StriCmp(p1, "ramp_up_time") )
    {
        _wpn->ramp_up_time = NonNegativeFiniteMilliseconds(parser, p2);
    }
    else if ( !StriCmp(p1, "ramp_up_max_shot_time") )
    {
        _wpn->ramp_up_max_shot_time = NonNegativeFiniteMilliseconds(parser, p2);
    }
    else if ( !StriCmp(p1, "ramp_up_max_weapon_spread_xy") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            _wpn->ramp_up_max_weapon_spread_x =
                NonNegativeFiniteOrZero(parser.stof(axis[0], 0));
            _wpn->ramp_up_max_weapon_spread_y =
                NonNegativeFiniteOrZero(parser.stof(axis[1], 0));
        }
    }
    else if ( !StriCmp(p1, "ramp_up_overheat_time") )
    {
        _wpn->ramp_up_overheat_time = NonNegativeFiniteMilliseconds(parser, p2);
    }
    else if ( !StriCmp(p1, "ramp_up_overheat_hp_drain") )
    {
        ParseAbsoluteOrPercent(p2, _wpn->ramp_up_overheat_hp_drain, 100.0f);
    }
    else if ( !StriCmp(p1, "ramp_up_overheat_icon") )
    {
        _wpn->ramp_up_overheat_icon = p2;
    }
    else if ( !StriCmp(p1, "shk_launch_player_slot") )
    {
        const int slot = parser.stol(p2, nullptr, 0);
        _wpn->shk_launch_player.slot = slot > 0 ? slot : 0;
    }
    else if ( !StriCmp(p1, "shk_launch_player_time") )
    {
        const int time = parser.stol(p2, nullptr, 0);
        _wpn->shk_launch_player.time = time > 0 ? time : 0;
    }
    else if ( !StriCmp(p1, "shk_launch_player_mag0") )
    {
        const float mag = parser.stof(p2, 0);
        _wpn->shk_launch_player.mag0 = std::isfinite(mag) && mag >= 0.0f ? mag : 0.0f;
    }
    else if ( !StriCmp(p1, "shk_launch_player_mag1") )
    {
        const float mag = parser.stof(p2, 0);
        _wpn->shk_launch_player.mag1 = std::isfinite(mag) && mag >= 0.0f ? mag : 0.0f;
    }
    else if ( !StriCmp(p1, "salve_shots") )
    {
        _wpn->salve_shots = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "salve_delay") )
    {
        _wpn->salve_delay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "multi_target") )
    {
        int maxTargets = parser.stol(p2, nullptr, 0);
        _wpn->multi_target = maxTargets > 0 ? maxTargets : 0;
    }
    else if ( !StriCmp(p1, "guide_time") )
    {
        _wpn->guide_time = ParsePositiveIntOrZero(p2);
    }
    // ---- OpenNeoUA custom: model = laser parameters ----
    else if ( !StriCmp(p1, "laser_energy_tick_time") )
    {
        int tickTime = parser.stol(p2, nullptr, 0);
        _wpn->laser_energy_tick_time = tickTime > 0 ? tickTime : 250;
    }
    else if ( !StriCmp(p1, "laser_energy_tick_time_user") )
    {
        int tickTime = parser.stol(p2, nullptr, 0);
        _wpn->laser_energy_tick_time_user = tickTime > 0 ? tickTime : 150;
    }
    else if ( !StriCmp(p1, "laser_energy_increment_rate") )
    {
        float rate = parser.stof(p2, 0);
        _wpn->laser_energy_increment_rate = rate > 0.0 ? rate : 0.0;
    }
    else if ( !StriCmp(p1, "laser_max_energy") )
    {
        float maxEnergy = parser.stof(p2, 0);
        _wpn->laser_max_energy = maxEnergy > 0.0 ? maxEnergy : 0.0;
    }
    else if ( !StriCmp(p1, "laser_spacing") )
    {
        float spacing = parser.stof(p2, 0);
        if ( spacing <= 0.0 )
            spacing = 40.0;
        if ( spacing < 20.0 )
            spacing = 20.0;
        if ( spacing > 500.0 )
            spacing = 500.0;
        _wpn->laser_spacing = spacing;
    }
    else if ( !StriCmp(p1, "laser_chain_allow") )
    {
        _wpn->laser_chain_allow = parser.stol(p2, nullptr, 0) != 0 ? 1 : 0;
    }
    else if ( !StriCmp(p1, "laser_chain_max_jumps") )
    {
        int maxJumps = parser.stol(p2, nullptr, 0);
        _wpn->laser_chain_max_jumps = maxJumps > 0 ? maxJumps : 0;
    }
    else if ( !StriCmp(p1, "laser_chain_radius") )
    {
        float radius = parser.stof(p2, 0);
        _wpn->laser_chain_radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "laser_chain_energy_mult") )
    {
        float mult = parser.stof(p2, 0);
        _wpn->laser_chain_energy_mult = mult > 0.0 ? mult : 1.0;
    }
    else if ( !StriCmp(p1, "laser_beam_count") )
    {
        int maxTargets = parser.stol(p2, nullptr, 0);
        _wpn->laser_beam_count = maxTargets > 1 ? maxTargets : 1;
    }
    else if ( !StriCmp(p1, "vertical_laser_enable") )
    {
        _wpn->vertical_laser_enable = parser.stol(p2, nullptr, 0) == 1;
    }
    else if ( !StriCmp(p1, "vertical_laser_ai_trigger_radius") )
    {
        float radius = parser.stof(p2, 0);
        _wpn->vertical_laser_ai_trigger_radius = std::isfinite(radius) && radius > 0.0f ? radius : 300.0f;
    }
    else if ( !StriCmp(p1, "ai_fire_trigger_box_below_xyz") ||
              !StriCmp(p1, "ai_fire_trigger_box_forward_xyz") ||
              !StriCmp(p1, "ai_fire_trigger_cone_forward_xyz") ||
              !StriCmp(p1, "ai_fire_trigger_cone_below_xyz") )
    {
        const bool below = !StriCmp(p1, "ai_fire_trigger_box_below_xyz");
        const bool coneBelow = !StriCmp(p1, "ai_fire_trigger_cone_below_xyz");
        const bool cone = coneBelow || !StriCmp(p1, "ai_fire_trigger_cone_forward_xyz");
        vec3d &size = below ? _wpn->ai_fire_trigger_box_below :
                      coneBelow ? _wpn->ai_fire_trigger_cone_below :
                      cone ? _wpn->ai_fire_trigger_cone_forward : _wpn->ai_fire_trigger_box_forward;
        size = vec3d();
        if ( p2 == "0" )
            return ScriptParser::RESULT_OK;

        const int count = 3;
        std::string axis[3];
        bool valid = ReadTuple(p1, p2, count, axis);
        float values[3] = {};
        for (int i = 0; valid && i < count; ++i)
        {
            try
            {
                size_t used = 0;
                values[i] = std::stof(axis[i], &used);
                valid = used == axis[i].size() && std::isfinite(values[i]) && values[i] > 0.0f;
            }
            catch (const std::exception &) { valid = false; }
        }
        if ( cone && (values[0] >= 180.0f || values[1] >= 180.0f) )
            valid = false;
        if ( valid )
            size = vec3d(values[0], values[1], values[2]);
        else
            ypa_log_out("WARNING: weapon %d %s='%s' is invalid; expected %d positive finite values (cone angles < 180). Volume disabled.\n",
                        _wpnID, p1.c_str(), p2.c_str(), count);
    }
    else if ( !StriCmp(p1, "ai_fire_trigger_forward_max_inclination") ||
              !StriCmp(p1, "ai_fire_trigger_dead_zone") )
    {
        const bool tilt = !StriCmp(p1, "ai_fire_trigger_forward_max_inclination");
        float &valueOut = tilt ? _wpn->ai_fire_trigger_forward_max_inclination : _wpn->ai_fire_trigger_dead_zone;
        valueOut = 0.0f;
        bool valid = false;
        try
        {
            size_t used = 0;
            const float value = std::stof(p2, &used);
            valid = used == p2.size() && std::isfinite(value) && value >= 0.0f &&
                    (tilt ? value < 90.0f : value <= 30000.0f);
            if ( valid )
                valueOut = value;
        }
        catch (const std::exception &) {}
        if ( !valid )
            ypa_log_out("WARNING: weapon %d %s='%s' is invalid; value disabled.\n",
                        _wpnID, p1.c_str(), p2.c_str());
    }
    else if ( !StriCmp(p1, "add_energy") )
    {
        _wpn->energy += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_energy_heli") )
    {
        _wpn->energy_heli += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_energy_tank") )
    {
        _wpn->energy_tank += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_energy_flyer") )
    {
        _wpn->energy_flyer += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_energy_Robo") )
    {
        _wpn->energy_robo += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_energy_plane") )
    {
        if ( !_wpn->energy_plane_defined )
            _wpn->energy_plane = _wpn->energy_flyer;
        _wpn->energy_plane += parser.stol(p2, nullptr, 0);
        _wpn->energy_plane_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_cruiser") )
    {
        if ( !_wpn->energy_cruiser_defined )
            _wpn->energy_cruiser = _wpn->energy_flyer;
        _wpn->energy_cruiser += parser.stol(p2, nullptr, 0);
        _wpn->energy_cruiser_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_glider") )
    {
        if ( !_wpn->energy_glider_defined )
            _wpn->energy_glider = _wpn->energy_flyer;
        _wpn->energy_glider += parser.stol(p2, nullptr, 0);
        _wpn->energy_glider_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_zeppelin") )
    {
        if ( !_wpn->energy_zeppelin_defined )
            _wpn->energy_zeppelin = _wpn->energy_flyer;
        _wpn->energy_zeppelin += parser.stol(p2, nullptr, 0);
        _wpn->energy_zeppelin_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_ufo") )
    {
        if ( !_wpn->energy_ufo_defined )
            _wpn->energy_ufo = _wpn->energy_flyer;
        _wpn->energy_ufo += parser.stol(p2, nullptr, 0);
        _wpn->energy_ufo_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_car") )
    {
        if ( !_wpn->energy_car_defined )
            _wpn->energy_car = _wpn->energy_tank;
        _wpn->energy_car += parser.stol(p2, nullptr, 0);
        _wpn->energy_car_defined = true;
    }
    else if ( !StriCmp(p1, "add_energy_gun") )
    {
        if ( !_wpn->energy_gun_defined )
            _wpn->energy_gun = 1.0f;
        _wpn->energy_gun += parser.stol(p2, nullptr, 0);
        _wpn->energy_gun_defined = true;
    }
    else if ( !StriCmp(p1, "add_shot_time") )
    {
        _wpn->shot_time += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_shot_time_user") )
    {
        _wpn->shot_time_user += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_normal") )
    {
        _wpn->vp_normal = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_fire") )
    {
        _wpn->vp_fire = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_megadeth") )
    {
        _wpn->vp_megadeth = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_wait") )
    {
        _wpn->vp_wait = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_dead") )
    {
        _wpn->vp_dead = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_genesis") )
    {
        _wpn->vp_genesis = parser.stol(p2, nullptr, 0);
    }
    else if ( ParseExternalVisualParam(p1, p2, "3ds", _wpn->visual_3ds) )
    {
    }
    else if ( ParseExternalVisualParam(p1, p2, "base", _wpn->visual_base) )
    {
    }
    else if ( ParseVPScaleParam(parser, "", p1, p2, _wpn->visual_scale) )
    {
    }
    else if ( ParseTintParam(parser, "tint", p1, p2, _wpn->visual_tint) )
    {
    }
    else if ( ParseVPRotationParam(parser, "", p1, p2, _wpn->visual_rotation) )
    {
    }
    else if ( ParseVPSpinParam(parser, "", p1, p2, _wpn->visual_spin) )
    {
    }
    else if ( !StriCmp(p1, "spiral_speed") )
    {
        _wpn->spiral_speed = (float)World::Spin::ClampStrength(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "spiral_radius") )
    {
        _wpn->spiral_radius = ClampProjectileVisualMotionRadius(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "chaos_factor") )
    {
        _wpn->chaos_factor = (float)World::Spin::ClampStrength(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "chaos_radius") )
    {
        _wpn->chaos_radius = ClampProjectileVisualMotionRadius(parser.stof(p2, 0));
    }
    else if ( ParseVPScaleParam(parser, "vp_trail", p1, p2, _wpn->vp_trail_scale) )
    {
    }
    else if ( ParseVPSpinParam(parser, "vp_trail", p1, p2, _wpn->vp_trail_spin) )
    {
    }
    else if ( ParseTintParam(parser, "vp_trail_tint", p1, p2, _wpn->vp_trail_tint) )
    {
    }
    else if ( ParseMeshTracerParam(parser, p1, p2, _wpn->tracer, "mesh_tracer_") )
    {
    }
    else if ( ParseLaserMeshParam(parser, p1, p2, _wpn->laser_mesh) )
    {
    }
    else if ( ParseDecorationFXParam(parser, p1, p2, _wpn->decoration_fx) )
    {
    }
    else if ( !StriCmp(p1, "type_icon") )
    {
        uint32_t glyph;
        if (GlyphKey::Parse(p2, glyph))
            _wpn->type_icon = glyph;
        else
            ypa_log_out("WARNING: invalid type_icon; previous icon retained.\n");
    }
    else if ( !StriCmp(p1, "wireframe") )
    {
        if ( _wpn->wireframe )
            _wpn->wireframe->Delete();

        _wpn->wireframe = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "dest_fx") )
    {
        Stok stok(p2, " _");
        std::string fx_type, pp1, pp2, pp3, pp4;

        if ( stok.GetNext(&fx_type) && stok.GetNext(&pp1) && stok.GetNext(&pp2) && stok.GetNext(&pp3) && stok.GetNext(&pp4) )
        {
            _wpn->dfx.emplace_back();
            DestFX &dfx = _wpn->dfx.back();
            dfx.Type = DestFX::ParseTypeName(fx_type);

            if (dfx.Type == DestFX::FX_NONE)
                return ScriptParser::RESULT_BAD_DATA;

            dfx.ModelID = parser.stol(pp1, nullptr, 0);
            dfx.Pos.x = parser.stof(pp2, 0);
            dfx.Pos.y = parser.stof(pp3, 0);
            dfx.Pos.z = parser.stof(pp4, 0);

            std::string pp5;
            if ( stok.GetNext(&pp5) )
            {
                if (parser.stol(pp5, nullptr, 0) != 0 )
                    dfx.Accel = true;
                else
                    dfx.Accel = false;
            }
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "ext_dest_fx") || !StriCmp(p1, "extended_dest_fx") )
    {
        Stok stok(p2, " _");
        std::string fx_type, pp1, pp2, pp3, pp4;

        if ( stok.GetNext(&fx_type) && stok.GetNext(&pp1) && stok.GetNext(&pp2) && stok.GetNext(&pp3) && stok.GetNext(&pp4) )
        {
            _wpn->ExtDestroyFX.emplace_back();

            DestFX &dfx = _wpn->ExtDestroyFX.back();

            dfx.Type = DestFX::ParseTypeName(fx_type);

            if (dfx.Type == DestFX::FX_NONE)
                return ScriptParser::RESULT_BAD_DATA;

            dfx.ModelID = parser.stol(pp1, nullptr, 0);
            dfx.Pos.x = parser.stof(pp2, 0);
            dfx.Pos.y = parser.stof(pp3, 0);
            dfx.Pos.z = parser.stof(pp4, 0);

            std::string pp5;
            if ( stok.GetNext(&pp5) )
            {
                if (parser.stol(pp5, nullptr, 0) != 0 )
                    dfx.Accel = true;
                else
                    dfx.Accel = false;
            }
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "artillery_shell_mode") )
    {
        if ( !StriCmp(p2, "ballistic") )
            _wpn->artillery_shell_mode = TWeapProto::ARTILLERY_SHELL_MODE_BALLISTIC;
        else if ( !StriCmp(p2, "vertical_barrage") )
            _wpn->artillery_shell_mode = TWeapProto::ARTILLERY_SHELL_MODE_VERTICAL_BARRAGE;
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "artillery_shell_fall_delay") )
    {
        int v = parser.stol(p2, nullptr, 0);
        _wpn->artillery_shell_fall_delay = v > 0 ? v : 0;
    }
    else if ( !StriCmp(p1, "artillery_shell_range_minmax") )
    {
        std::string range[2];
        if ( ReadTuple(p1, p2, 2, range) )
        {
            float minRange = parser.stof(range[0], 0);
            float maxRange = parser.stof(range[1], 0);
            _wpn->artillery_shell_min_range = minRange > 0.0 ? minRange : 0.0;
            _wpn->artillery_shell_max_range = maxRange > 0.0 ? maxRange : 0.0;
        }
    }
    else if ( !StriCmp(p1, "artillery_shell_requires_radar") )
    {
        _wpn->artillery_shell_requires_radar = parser.stol(p2, nullptr, 0) != 0 ? 1 : 0;
    }
    else if ( !StriCmp(p1, "artillery_shell_manual_mode_only") )
    {
        _wpn->artillery_shell_manual_mode_only = parser.stol(p2, nullptr, 0) != 0 ? 1 : 0;
    }
    else if ( !StriCmp(p1, "artillery_shell_barrage_radius") )
    {
        float v = parser.stof(p2, 0);
        _wpn->artillery_shell_barrage_radius = v > 0.0 ? v : 0.0;
    }
    else if ( !StriCmp(p1, "artillery_shell_barrage_shots") )
    {
        int v = parser.stol(p2, nullptr, 0);
        _wpn->artillery_shell_barrage_shots = v > 0 ? v : 0;
    }
    else if ( !StriCmp(p1, "artillery_shell_barrage_shot_delay") )
    {
        _wpn->artillery_shell_barrage_shot_delay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "artillery_shell_barrage_cooldown") )
    {
        _wpn->artillery_shell_barrage_cooldown = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "artillery_shell_arc_height") )
    {
        float v = parser.stof(p2, 0);
        _wpn->artillery_shell_arc_height = v > 0.0 ? v : 0.0;
    }
    else if ( !StriCmp(p1, "artillery_shell_speed") )
    {
        float v = parser.stof(p2, 0);
        _wpn->artillery_shell_speed = v > 0.0 ? v : 0.0;
    }
    else if ( !StriCmp(p1, "artillery_shell_vertical_spread_xz") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            float x = parser.stof(axis[0], 0);
            float z = parser.stof(axis[1], 0);
            _wpn->artillery_shell_vertical_spread_x = (std::isfinite(x) && x > 0.0f) ? std::min(x, 45.0f) : 0.0f;
            _wpn->artillery_shell_vertical_spread_z = (std::isfinite(z) && z > 0.0f) ? std::min(z, 45.0f) : 0.0f;
        }
    }
    else if ( !StriCmp(p1, "artillery_shell_airburst") )
    {
        _wpn->artillery_shell_airburst = parser.stol(p2, nullptr, 0) != 0 ? 1 : 0;
    }
    else if ( !StriCmp(p1, "artillery_shell_marker_path") )
    {
        // Author relative to Data/Interface/Map/Markers. Empty/invalid/missing
        // assets are handled by the UI loader with the canonical classic SVG fallback.
        _wpn->artillery_shell_marker_path = p2;
    }
    else if ( !StriCmp(p1, "begin_fx") )
    {
        return ParseWeaponChainFXBlock(parser, _wpn);
    }
    else
        return ParseSndFX(parser, p1, p2);

    return ScriptParser::RESULT_OK;
}

}
}
