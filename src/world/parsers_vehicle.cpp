// Script parser for vehicle prototypes (new_vehicle / modify_vehicle).
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

int VhclProtoParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "disable_collision") )
    {
        _vhcl->disable_collision = p2 == "1";
        return ScriptParser::RESULT_OK;
    }
    if ( !StriCmp(p1, "weapon_use_vehicle_fire") )
    {
        _vhcl->weapon_use_vehicle_fire = p2 == "1";
        return ScriptParser::RESULT_OK;
    }
    if ( !StriCmp(p1, "disable_genesis") )
    {
        _vhcl->disable_genesis = p2 == "1";
        return ScriptParser::RESULT_OK;
    }
    if ( !StriCmp(p1, "collision_shape") )
    {
        _vhcl->collision_shape.clear();
        if (p2.empty() || p2 == "0") return ScriptParser::RESULT_OK;
        if (!uaNormalizeDataAssetPath(p2, &_vhcl->collision_shape, false))
            ypa_log_out("WARNING: invalid collision_shape %s; existing collision retained.\n", p2.c_str());
        return ScriptParser::RESULT_OK;
    }
    TRoboProto *robo = _vhcl->RoboProto;

    if (!robo)
        robo = &_roboTmp;

    auto getUnitGun = [this, robo]() -> TRoboGun *
    {
        // On Host Stations the unit_* family is a parser alias of the native
        // Robo gun list. This keeps one runtime attachment system (_roboGuns)
        // while allowing the richer Unit Gun fields (icon/protect) to be used.
        if ( _vhcl->model_id == BACT_TYPES_ROBO )
        {
            if ( _unitGunID < 0 || (size_t)_unitGunID >= robo->guns.size() )
                return nullptr;

            return &robo->guns.at(_unitGunID);
        }

        if ( _unitGunID < 0 || (size_t)_unitGunID >= _vhcl->unit_guns.size() )
            return nullptr;

        return &_vhcl->unit_guns.at(_unitGunID);
    };

    auto getRoboColl = [this, robo]() -> TRoboColl *
    {
        if ( _collID < 0 || (size_t)_collID >= robo->coll.roboColls.size() )
            return nullptr;

        return &robo->coll.roboColls.at(_collID);
    };

    if ( !StriCmp(p1, "end") )
    {
        bool fireXAdvancedAuthored = _vhcl->fire_x_start_defined ||
                                     _vhcl->fire_x_step_defined ||
                                     _vhcl->fire_x_slots_defined;
        bool fireXSlotsValid = _vhcl->fire_x_slots_defined &&
                               _vhcl->fire_x_slots > 0 &&
                               _vhcl->fire_x_slots <= TVhclProto::FIRE_X_MAX_SLOTS;
        bool fireXAdvancedValid = _vhcl->fire_x_start_defined &&
                                  _vhcl->fire_x_step_defined &&
                                  fireXSlotsValid &&
                                  std::isfinite(_vhcl->fire_x_start) &&
                                  std::isfinite(_vhcl->fire_x_step);

        if ( fireXAdvancedValid )
        {
            double fireXLast = (double)_vhcl->fire_x_start +
                               (double)(_vhcl->fire_x_slots - 1) * (double)_vhcl->fire_x_step;
            fireXAdvancedValid = std::isfinite(fireXLast) &&
                                 std::abs(fireXLast) <= (double)std::numeric_limits<float>::max();
        }

        bool fireXAdvancedMode = _vhcl->fire_x_mode == TVhclProto::FIRE_X_MODE_SEQUENCE ||
                                 _vhcl->fire_x_mode == TVhclProto::FIRE_X_MODE_RANDOM;
        if ( fireXAdvancedMode )
        {
            _vhcl->fire_x_advanced = fireXAdvancedValid;

            if ( fireXAdvancedAuthored && !fireXAdvancedValid )
            {
                ypa_log_out("WARNING: vehicle %d fire_x advanced rack ignored; require finite fire_x_start/fire_x_step, a finite final slot, and fire_x_slots in range 1-%d. Falling back to simple fire_x mode.\n",
                            _vhclID, TVhclProto::FIRE_X_MAX_SLOTS);
            }
        }
        else
            _vhcl->fire_x_advanced = false;

        const bool weaponArcXActive = _vhcl->weapon_arc_x > 0.0f;
        const bool weaponArcYActive = _vhcl->weapon_arc_y > 0.0f;
        const bool weaponConeActive = _vhcl->weapon_cone_xy > 0.0f;
        if ( (weaponArcXActive || weaponArcYActive) && weaponConeActive )
        {
            ypa_log_out("WARNING: vehicle %d weapon Arc/Cone conflict (arc_x=%.3f, arc_y=%.3f, cone_xy=%.3f); Arc and Cone disabled, normal launch direction retained for every weapon slot.\n",
                        _vhclID, _vhcl->weapon_arc_x, _vhcl->weapon_arc_y,
                        _vhcl->weapon_cone_xy);
        }
        else if ( weaponArcXActive && weaponArcYActive )
        {
            const int weaponIds[4] = {_vhcl->weapon, _vhcl->extra_weapons[0],
                                      _vhcl->extra_weapons[1], _vhcl->extra_weapons[2]};

            for (int weaponSlot = 0; weaponSlot < 4; weaponSlot++)
            {
                if ( (weaponSlot == 0 && weaponIds[weaponSlot] < 0) ||
                     (weaponSlot > 0 && weaponIds[weaponSlot] <= 0) )
                    continue;

                int minWeaponCount = 1;
                int maxWeaponCount = 1;
                _vhcl->GetWeaponProjectileCountRange(
                    weaponSlot, minWeaponCount, maxWeaponCount);

                bool hasInvalidArcCount = false;
                for (int weaponCount = minWeaponCount;
                     weaponCount <= maxWeaponCount; ++weaponCount)
                {
                    if ( weaponCount > 1 &&
                         weaponCount % 4 != 0 && weaponCount % 4 != 1 )
                    {
                        hasInvalidArcCount = true;
                        break;
                    }
                }

                if ( hasInvalidArcCount )
                {
                    ypa_log_out("WARNING: vehicle %d weapon slot %d Arc cross requires every effective projectile count to be 4k or 4k+1 (configured %d_%d); Arc is disabled on incompatible random rolls.\n",
                                _vhclID, weaponSlot + 1,
                                minWeaponCount, maxWeaponCount);
                }
            }
        }

        if ( _vhcl->model_id == BACT_TYPES_ROBO )
        {
            if (!_vhcl->RoboProto)
                _vhcl->RoboProto = new TRoboProto(_roboTmp);

            _vhcl->initParams.Add(NC_STACK_yparobo::ROBO_ATT_PROTO, _vhcl->RoboProto);
        }

        if ( _vhcl->model_id == BACT_TYPES_BACT )
            _vhcl->field_1D6F = (_vhcl->force * 0.6) / _vhcl->airconst;
        else
            _vhcl->field_1D6F = (_vhcl->force) / _vhcl->airconst;

        _vhcl->field_1D6D = (_vhcl->field_1D6F / 10) * 1200.0;

        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( ParseCompoundCollisionParameter(parser, p1, p2, _vhcl->coll, _collID) )
        return ScriptParser::RESULT_OK;

    if ( !StriCmp(p1, "model") )
    {
        _vhcl->script_class = p2;
        _vhcl->is_mimic = 0;

        if ( !StriCmp(p2, "heli") )
        {
            _vhcl->model_id = BACT_TYPES_BACT;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_HELI;
        }
        else if ( !StriCmp(p2, "tank") )
        {
            _vhcl->model_id = BACT_TYPES_TANK;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_TANK;
        }
        else if ( !StriCmp(p2, "robo") )
        {
            _vhcl->model_id = BACT_TYPES_ROBO;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_ROBO;

            *robo = TRoboProto();
            robo->matrix = mat3x3::Ident();

            // If unit_* gun fields appeared before model = robo, fold them into
            // the native Robo list now. Script order must not create a second
            // attachment runtime on Host Stations.
            if ( !_vhcl->unit_guns.empty() )
            {
                robo->guns = _vhcl->unit_guns;
                _vhcl->unit_guns.clear();
                _gunID = _unitGunID;
            }
        }
        else if ( !StriCmp(p2, "ufo") )
        {
            _vhcl->model_id = BACT_TYPES_UFO;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_UFO;

            // UFO control does not use the generic handbrake. Avoid loading its
            // global sample for this class; Space/Stop is owned by Spy UI instead.
            _vhcl->sndFX[TVhclProto::SND_HANDBRAKE].MainSample.Name.clear();
        }
        else if ( !StriCmp(p2, "car") )
        {
            _vhcl->model_id = BACT_TYPES_CAR;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_CAR;
        }
        else if ( !StriCmp(p2, "gun") || !StriCmp(p2, "module") )
        {
            // OpenNeoUA: model = module is a semantic alias of the existing gun
            // runtime. Behaviour is selected by gun_type; no parallel actor
            // class or attachment system is introduced.
            _vhcl->model_id = BACT_TYPES_GUN;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_GUN;
        }
        else if ( !StriCmp(p2, "plane") )
        {
            _vhcl->model_id = BACT_TYPES_FLYER;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_PLANE;

            _vhcl->initParams.Add(NC_STACK_ypaflyer::FLY_ATT_TYPE, (int32_t)3);
        }
        else if ( !StriCmp(p2, "cruiser") )
        {
            // OpenNeoUA: expose the unused Flyer bit-combination already handled
            // by ypaflyer: pitch follows vertical motion, lateral banking stays rigid.
            _vhcl->model_id = BACT_TYPES_FLYER;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_CRUISER;
            _vhcl->initParams.Add(NC_STACK_ypaflyer::FLY_ATT_TYPE, (int32_t)1);
        }
        else if ( !StriCmp(p2, "glider") )
        {
            _vhcl->model_id = BACT_TYPES_FLYER;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_GLIDER;
            _vhcl->initParams.Add(NC_STACK_ypaflyer::FLY_ATT_TYPE, (int32_t)2);
        }
        else if ( !StriCmp(p2, "zeppelin") )
        {
            _vhcl->model_id = BACT_TYPES_FLYER;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_ZEPPELIN;
            _vhcl->initParams.Add(NC_STACK_ypaflyer::FLY_ATT_TYPE, (int32_t)0);
        }
        else if ( !StriCmp(p2, "mimic") )
        {
            // OpenNeoUA custom: runtime shell that copies one vehicle enabled by the
            // current level, then keeps this proto's at_death_spawn_* reveal data.
            _vhcl->model_id = BACT_TYPES_TANK;
            _vhcl->combat_class = VEHICLE_COMBAT_CLASS_UNKNOWN;
            _vhcl->is_mimic = 1;
            _vhcl->job_fightrobo = 6;
            _vhcl->job_fightflyer = 6;
            _vhcl->job_fighthelicopter = 6;
            _vhcl->job_fighttank = 6;
            _vhcl->job_conquer = 6;
            _vhcl->job_reconnoitre = 6;
        }
        else
        {
            return ScriptParser::RESULT_BAD_DATA;
        }
    }
    else if ( _vhcl->is_mimic && !IsMimicVehicleShellParam(p1) )
    {
        return ScriptParser::RESULT_OK;
    }
    else if ( !StriCmp(p1, "enable") )
    {
        int fraction = parser.stol(p2, nullptr, 0);
        _vhcl->disable_enable_bitmask |= 1 << fraction;
    }
    else if ( !StriCmp(p1, "disable") )
    {
        _vhcl->disable_enable_bitmask &= ~(1 << parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "name") )
    {
        _vhcl->name = p2;
        std::replace(_vhcl->name.begin(), _vhcl->name.end(), '_', ' ');
    }
    else if (!StriCmp(p1,"purchase_cost"))
    {
        const auto value=parser.FullLineValue();
        if(!value.empty()&&value.find_first_not_of("0123456789")==std::string::npos)
        {
            try { _vhcl->purchase_cost=std::stoll(value); }
            catch(const std::exception &) { _vhcl->purchase_cost=-1; }
        }
        else _vhcl->purchase_cost=-1;
    }
    else if ( !StriCmp(p1, "vehicle_description") )
        _vhcl->description = parser.FullLineValue();
    else if ( !StriCmp(p1, "energy") )
    {
        _vhcl->energy = parser.stol(p2, nullptr, 0);
        _vhcl->energy_defined = true;
    }
    else if ( !StriCmp(p1, "production_cost") )
    {
        // Displayed Genesis price. Zero or invalid keeps vanilla energy-as-cost.
        const long value = parser.stol(p2, nullptr, 0);
        _vhcl->production_cost = value > 0 ? (int)value : 0;
        _vhcl->production_cost_defined = value >= 0;
    }
    else if ( !StriCmp(p1, "mimic_energy_cost") )
    {
        int minCost = 0;
        int maxCost = 0;
        if ( ParseMimicEnergyCostRange(p2, minCost, maxCost) )
        {
            _vhcl->mimic_energy_cost_min = minCost;
            _vhcl->mimic_energy_cost_max = maxCost;
            _vhcl->RollMimicProductionCost();
        }
        else
        {
            _vhcl->mimic_energy_cost = 0;
            _vhcl->mimic_energy_cost_min = 0;
            _vhcl->mimic_energy_cost_max = 0;
        }
    }
    else if ( !StriCmp(p1, "shield") )
    {
        const long shield=parser.stol(p2, nullptr, 0);
        _vhcl->shield=shield;
        _vhcl->shield_defined=shield>=0&&shield<=100;
    }
    else if ( !StriCmp(p1, "buff_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _vhcl->buff_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "ground_decal_size") )
        ParseDecalSizeOverride("ground_decal_size", p2, _vhcl->ground_decal_size);
    else if ( !StriCmp(p1, "ground_decal_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _vhcl->ground_decal_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "mass") )
    {
        _vhcl->mass = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "force") )
    {
        _vhcl->force = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "maxrot") )
    {
        _vhcl->maxrot = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "airconst") )
    {
        _vhcl->airconst = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "height") )
    {
        _vhcl->height = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "radius") )
    {
        _vhcl->radius = parser.stof(p2, 0);
        _vhcl->radius_defined = true;
    }
    else if ( !StriCmp(p1, "briefing_xyz") )
    {
        std::string axis[3];
        if(ReadTuple(p1,p2,3,axis))
        {
            double offset[3];bool valid=true;
            for(int i=0;i<3;++i)
            {
                offset[i]=ParseFiniteFloatOrFallback(parser,axis[i],std::numeric_limits<float>::quiet_NaN());
                valid=valid&&std::isfinite(offset[i])&&std::abs(offset[i])<=100000.0;
            }
            if(valid)_vhcl->briefing_xyz=vec3d(offset[0],offset[1],offset[2]);
        }
    }
    else if ( !StriCmp(p1, "briefing_size") )
    {
        // Valid sizes are 1-10000. Anything else keeps the vanilla radius framing.
        const float size = ParseNonNegativeIniFloatOrZero(p2);
        _vhcl->briefing_size = size >= 1.0f && size <= 10000.0f ? size : 0.0f;
    }
    else if ( !StriCmp(p1, "overeof") )
    {
        _vhcl->overeof = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "vwr_radius") )
    {
        _vhcl->vwr_radius = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "vwr_overeof") )
    {
        _vhcl->vwr_overeof = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "adist_sector") )
    {
        _vhcl->adist_sector = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "adist_bact") )
    {
        _vhcl->adist_bact = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "sdist_sector") )
    {
        _vhcl->sdist_sector = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "sdist_bact") )
    {
        _vhcl->sdist_bact = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "ai_attack_range") )
    {
        _vhcl->ai_attack_range = parser.stof(p2, 0);
        _vhcl->ai_combat_distance_mask |= TVhclProto::AI_COMBAT_ATTACK_DEFINED;
    }
    else if ( !StriCmp(p1, "ai_retreat_start_range") )
    {
        _vhcl->ai_retreat_start_range = parser.stof(p2, 0);
        _vhcl->ai_combat_distance_mask |= TVhclProto::AI_COMBAT_RETREAT_START_DEFINED;
    }
    else if ( !StriCmp(p1, "ai_retreat_stop_range") )
    {
        _vhcl->ai_retreat_stop_range = parser.stof(p2, 0);
        _vhcl->ai_combat_distance_mask |= TVhclProto::AI_COMBAT_RETREAT_STOP_DEFINED;
    }
    else if ( !StriCmp(p1, "radar") )
    {
        _vhcl->radar = parser.stol(p2, nullptr, 0);
        _vhcl->radar_defined = true;
    }
    else if ( !StriCmp(p1, "push_resistance") )
    {
        _vhcl->push_resistance = Clamp01(parser.stof(p2, 0));
        _vhcl->has_push_resistance = true;
    }
    else if ( !StriCmp(p1, "at_death_push_force") )
    {
        _vhcl->at_death_push_force = ClampPushIntensity(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "at_death_push_radius") )
    {
        _vhcl->at_death_push_radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "at_death_push_falloff") )
    {
        _vhcl->at_death_push_falloff = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "at_death_energy") )
    {
        const int damage = parser.stol(p2, nullptr, 0);
        _vhcl->at_death_energy = damage > 0 ? damage : 0;
    }
    else if ( !StriCmp(p1, "at_death_energy_radius") )
    {
        _vhcl->at_death_energy_radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    }
    else if ( !StriCmp(p1, "at_death_energy_falloff") )
    {
        _vhcl->at_death_energy_falloff = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "add_energy") )
    {
        _vhcl->energy += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_shield") )
    {
        _vhcl->shield += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_radar") )
    {
        _vhcl->radar += parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "add_max_active_at_once") )
    {
        const int increment = ParsePositiveIntOrZero(p2);
        if ( increment > 0 && _vhcl->max_active_at_once > 0 )
        {
            const int previousLimit = _vhcl->max_active_at_once;
            if ( _vhcl->max_active_at_once > std::numeric_limits<int>::max() - increment )
                _vhcl->max_active_at_once = std::numeric_limits<int>::max();
            else
                _vhcl->max_active_at_once += increment;

            if ( _isModify && _vhcl->max_active_at_once != previousLimit )
                _o._upgradeMaxActiveVehicleId = _vhclID;
        }
    }
    else if ( !StriCmp(p1, "vp_normal") )
    {
        _vhcl->vp_normal = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_fire") )
    {
        _vhcl->vp_fire = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_megadeth") )
    {
        _vhcl->vp_megadeth = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_wait") )
    {
        _vhcl->vp_wait = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_dead") )
    {
        _vhcl->vp_dead = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vp_genesis") )
    {
        _vhcl->vp_genesis = parser.stol(p2, nullptr, 0);
    }
    else if ( ParseExternalVisualParam(p1, p2, "3ds", _vhcl->visual_3ds) )
    {
    }
    else if ( ParseExternalVisualParam(p1, p2, "base", _vhcl->visual_base) )
    {
    }
    else if ( !StriCmp(p1, "damaged_fx_vp") )
    {
        if ( !ParseSmartVPList(p2, _vhcl->damaged_fx.vps) )
            _vhcl->damaged_fx.vps.clear();
    }
    else if ( !StriCmp(p1, "damaged_fx_3ds") )
    {
        if ( !p2.empty() )
            _vhcl->damaged_fx.meshes3ds.push_back(p2);
    }
    else if ( !StriCmp(p1, "damaged_fx_scale") )
    {
        float scaleMin = 1.0f;
        float scaleMax = 1.0f;
        if ( ParseScriptFloatRange(p2, scaleMin, scaleMax) &&
             scaleMin > 0.0f && scaleMax > 0.0f )
        {
            _vhcl->damaged_fx.scale = scaleMin;
            _vhcl->damaged_fx.scale_max = scaleMax;
        }
        else
        {
            _vhcl->damaged_fx.scale = 1.0f;
            _vhcl->damaged_fx.scale_max = 1.0f;
        }
    }
    else if ( !StriCmp(p1, "damaged_fx_threshold") )
    {
        ParseAbsoluteOrPercent(p2, _vhcl->damaged_fx.threshold, 100.0f);
    }
    else if ( !StriCmp(p1, "damaged_fx_count") )
    {
        int countMin = 0;
        int countMax = 0;
        if ( ParseScriptIntRange(p2, countMin, countMax) )
        {
            _vhcl->damaged_fx.count_min = std::max(0, std::min(countMin, 32));
            _vhcl->damaged_fx.count_max = std::max(0, std::min(countMax, 32));
        }
        else
        {
            _vhcl->damaged_fx.count_min = 0;
            _vhcl->damaged_fx.count_max = 0;
        }
    }
    else if ( !StriCmp(p1, "damaged_fx_interval") )
    {
        int intervalMin = 0;
        int intervalMax = 0;
        if ( ParseScriptIntRange(p2, intervalMin, intervalMax) )
        {
            _vhcl->damaged_fx.interval_min = intervalMin;
            _vhcl->damaged_fx.interval_max = intervalMax;
        }
        else
        {
            _vhcl->damaged_fx.interval_min = 0;
            _vhcl->damaged_fx.interval_max = 0;
        }
    }
    else if ( !StriCmp(p1, "damaged_fx_random_max_offset") )
    {
        ParseAbsoluteOrPercent(p2, _vhcl->damaged_fx.random_max_offset, 100.0f);
    }
    else if ( !StriCmp(p1, "damaged_fx_trail_only") )
    {
        _vhcl->damaged_fx.trail_only = parser.stol(p2, nullptr, 0) != 0;
    }
    else if ( ParseDecorationFXParam(parser, p1, p2, _vhcl->decoration_fx) )
    {
    }
    else if ( !StriCmp(p1, "regen_icon") ||
              !StriCmp(p1, "drain_icon") ||
              !StriCmp(p1, "damaged_icon") ||
              !StriCmp(p1, "spawn_icon") ||
              !StriCmp(p1, "radar_icon") ||
              !StriCmp(p1, "power_icon") )
    {
        // Legacy no-op: vehicle capability icons are assigned automatically.
        // Keep accepting old script keys so vanilla/OpenNeoUA scripts still load.
    }
    else if ( !StriCmp(p1, "unit_gun_icon") )
    {
        if (TRoboGun *gun = getUnitGun())
            gun->icon = p2;
        else
            _vhcl->unit_gun_icon = p2;
    }
    else if ( !StriCmp(p1, "power") )
    {
        _vhcl->power = ClampSectorPower(parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "power_radius") )
    {
        float radius = parser.stof(p2, 0);
        _vhcl->power_radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "power_falloff") )
    {
        _vhcl->power_falloff = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "power_cloud_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _vhcl->power_cloud_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "spy_ui_radius") )
    {
        // Vehicle-side and intentionally runtime-active only for model = ufo.
        // Parse independently of declaration order so scripts may place model
        // before or after this key without changing the result.
        float radius = parser.stof(p2, 0);
        _vhcl->spy_ui_radius =
            std::isfinite(radius) && radius > 0.0f ? radius : 0.0f;
    }
    else if ( !StriCmp(p1, "spy_ui_radius_sound") )
    {
        // Optional one-shot used by both enable and disable transitions.
        _vhcl->sndFX[TVhclProto::SND_SPY_UI].MainSample.Name = p2;
    }
    else if ( !StriCmp(p1, "zoom_steps") )
    {
        // -1 means the parameter is absent/invalid and preserves the current
        // OpenNeoUA UFO zoom range. Zero intentionally disables optical zoom.
        size_t parsed = 0;
        const long steps = parser.stol(p2, &parsed, 0);
        _vhcl->zoom_steps = parsed == p2.size() && steps >= 0
            ? (int)std::min<long>(steps, 10)
            : -1;
    }
    else if ( !StriCmp(p1, "zoom_step_sound") )
    {
        // Optional one-shot played whenever an actual UFO optical zoom step is applied.
        _vhcl->sndFX[TVhclProto::SND_ZOOM_STEP].MainSample.Name = p2;
    }
    else if ( !StriCmp(p1, "damaged_force_malus") )
    {
        _vhcl->damaged_force_malus = ParseMalusPercent(p2);
    }
    else if ( !StriCmp(p1, "damaged_maxrot_malus") )
    {
        _vhcl->damaged_maxrot_malus = ParseMalusPercent(p2);
    }
    else if ( !StriCmp(p1, "damaged_mgun_shot_time_malus") )
    {
        _vhcl->damaged_mgun_shot_time_malus = ParseMalusPercent(p2);
    }
    else if ( !StriCmp(p1, "damaged_shot_time_malus") )
    {
        _vhcl->damaged_shot_time_malus = ParseMalusPercent(p2);
    }
    else if ( !StriCmp(p1, "damaged_snd_pitch") )
    {
        _vhcl->damaged_snd_pitch_multiplier = ParseSignedPitchPercent(p2);
    }
    else if ( !StriCmp(p1, "spawn_units") )
    {
        _vhcl->spawn_units = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "spawn_vehicle") )
    {
        int vehicleId = parser.stol(p2, nullptr, 0);
        _vhcl->spawn_vehicle = vehicleId > 0 ? vehicleId : 0;
    }
    else if ( !StriCmp(p1, "spawn_interval") )
    {
        int interval = parser.stol(p2, nullptr, 0);

        if ( interval <= 0 )
            interval = 5000;
        else if ( interval < 1000 )
            interval = 1000;

        _vhcl->spawn_interval = interval;
    }
    else if ( !StriCmp(p1, "spawn_trigger_radius") )
    {
        float radius = parser.stof(p2, 0);
        _vhcl->spawn_trigger_radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "spawn_random_pos") )
    {
        float radius = parser.stof(p2, 0);
        _vhcl->spawn_random_pos = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "spawn_offset_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _vhcl->spawn_offset.x = ParseFiniteFloatOrFallback(parser, axis[0], 0.0f);
            _vhcl->spawn_offset.y = ParseFiniteFloatOrFallback(parser, axis[1], 0.0f);
            _vhcl->spawn_offset.z = ParseFiniteFloatOrFallback(parser, axis[2], 0.0f);
        }
    }
    else if ( !StriCmp(p1, "spawn_max_active") )
    {
        int maxActive = parser.stol(p2, nullptr, 0);
        _vhcl->spawn_max_active = maxActive > 0 ? maxActive : 0;
    }
    else if ( !StriCmp(p1, "spawn_count") )
    {
        int count = parser.stol(p2, nullptr, 0);

        if ( count <= 0 )
            count = 1;
        else if ( count > 8 )
            count = 8;

        _vhcl->spawn_count = count;
        _vhcl->spawn_count_defined = true;
    }
    else if ( !StriCmp(p1, "spawn_instant") )
    {
        _vhcl->spawn_instant = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "at_death_spawn_units") )
    {
        _vhcl->at_death_spawn_units = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "at_death_spawn_vehicle") )
    {
        int vehicleId = parser.stol(p2, nullptr, 0);
        _vhcl->at_death_spawn_vehicle = vehicleId > 0 ? vehicleId : 0;
    }
    else if ( !StriCmp(p1, "at_death_spawn_count") )
    {
        int count = parser.stol(p2, nullptr, 0);

        if ( count <= 0 )
            count = 1;
        else if ( count > 8 )
            count = 8;

        _vhcl->at_death_spawn_count = count;
    }
    else if ( !StriCmp(p1, "at_death_spawn_random_pos") )
    {
        float radius = parser.stof(p2, 0);
        _vhcl->at_death_spawn_random_pos = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "at_death_spawn_instant") )
    {
        _vhcl->at_death_spawn_instant = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "at_death_spawn_immunity_time") )
    {
        int time = parser.stol(p2, nullptr, 0);
        _vhcl->at_death_spawn_immunity_time = time > 0 ? time : 0;
    }
    else if ( !StriCmp(p1, "snd_mimic_sample") )
    {
        _vhcl->snd_mimic.MainSample.Name = p2;
    }
    else if ( !StriCmp(p1, "snd_mimic_pitch") )
    {
        ParseSoundPitchRange(p2, _vhcl->snd_mimic);
    }
    else if ( !StriCmp(p1, "snd_mimic_volume") )
    {
        _vhcl->snd_mimic.volume = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "proximity_defense_enable") )
    {
        _vhcl->proximity_defense_enable = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "proximity_defense_weapon") )
    {
        int weaponId = parser.stol(p2, nullptr, 0);
        _vhcl->proximity_defense_weapon = weaponId > 0 ? weaponId : 0;
    }
    else if ( !StriCmp(p1, "proximity_defense_trigger_radius") )
    {
        float radius = parser.stof(p2, 0);
        _vhcl->proximity_defense_trigger_radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "proximity_defense_interval") )
    {
        int interval = parser.stol(p2, nullptr, 0);
        _vhcl->proximity_defense_interval = interval > 0 ? interval : 1000;
    }
    else if ( !StriCmp(p1, "proximity_defense_shots") )
    {
        int shots = parser.stol(p2, nullptr, 0);
        _vhcl->proximity_defense_shots = shots > 0 ? shots : 1;
    }
    else if ( !StriCmp(p1, "proximity_defense_vp_launch") )
    {
        int vp = parser.stol(p2, nullptr, 0);
        _vhcl->proximity_defense_vp_launch = vp > 0 ? vp : -1;
    }
    else if ( !StriCmp(p1, "proximity_defense_3ds_launch") )
    {
        _vhcl->proximity_defense_3ds_launch = p2;
    }
    else if ( !StriCmp(p1, "proximity_defense_base_launch") )
    {
        _vhcl->proximity_defense_base_launch = p2;
    }
    else if ( !StriCmp(p1, "proximity_defense_fire_mode") )
    {
        if ( !StriCmp(p2, "sequential") )
            _vhcl->proximity_defense_fire_mode = 1;
        else
            _vhcl->proximity_defense_fire_mode = 0;
    }
    else if ( !StriCmp(p1, "proximity_defense_sequence_delay") )
    {
        int delay = parser.stol(p2, nullptr, 0);
        _vhcl->proximity_defense_sequence_delay = delay > 0 ? delay : 100;
    }
    else if ( !StriCmp(p1, "proximity_defense_mode") )
    {
        _vhcl->proximity_defense_mode = !StriCmp(p2, "at_death") ? 1 : 0;
    }
    else if ( !StriCmp(p1, "proximity_defense_horizontal_angle") )
    {
        float angleMin = 0.0f;
        float angleMax = 360.0f;
        _vhcl->proximity_defense_horizontal_angle_set =
            ParseScriptFloatRange(p2, angleMin, angleMax);
        _vhcl->proximity_defense_horizontal_angle_min = angleMin;
        _vhcl->proximity_defense_horizontal_angle_max = angleMax;
    }
    else if ( !StriCmp(p1, "proximity_defense_vertical_angle") )
    {
        float angleMin = -10.0f;
        float angleMax = 45.0f;
        _vhcl->proximity_defense_vertical_angle_set =
            ParseScriptFloatRange(p2, angleMin, angleMax);
        _vhcl->proximity_defense_vertical_angle_min = angleMin;
        _vhcl->proximity_defense_vertical_angle_max = angleMax;
    }
    else if ( !StriCmp(p1, "max_active_at_once") )
    {
        _vhcl->max_active_at_once = ParsePositiveIntOrZero(p2);
    }
    else if ( ParseVPScaleParam(parser, "", p1, p2, _vhcl->visual_scale) )
    {
    }
    else if ( ParseTintParam(parser, "tint", p1, p2, _vhcl->visual_tint) )
    {
    }
    else if ( ParseTintParam(parser, "mimic_tint", p1, p2, _vhcl->mimic_tint) )
    {
    }
    else if ( ParseVPRotationParam(parser, "", p1, p2, _vhcl->visual_rotation) )
    {
    }
    else if ( ParseVPSpinParam(parser, "", p1, p2, _vhcl->visual_spin) )
    {
    }
    else if ( !StriCmp(p1, "type_icon") )
    {
        uint32_t glyph;
        if (GlyphKey::Parse(p2, glyph))
        {
            _vhcl->type_icon = glyph;
            _vhcl->type_icon_defined = true;
        }
        else
            ypa_log_out("WARNING: invalid type_icon; previous icon retained.\n");
    }
    else if ( !StriCmp(p1, "dest_fx") )
    {
        Stok stok(p2, " _");
        std::string fx_type, pp1, pp2, pp3, pp4;

        if ( stok.GetNext(&fx_type) && stok.GetNext(&pp1) && stok.GetNext(&pp2) && stok.GetNext(&pp3) && stok.GetNext(&pp4) )
        {
            _vhcl->dest_fx.emplace_back();
            DestFX &dfx = _vhcl->dest_fx.back();
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
        {
            return ScriptParser::RESULT_BAD_DATA;
        }
    }
    else if ( !StriCmp(p1, "ext_dest_fx") || !StriCmp(p1, "extended_dest_fx") )
    {
        Stok stok(p2, " _");
        std::string fx_type, pp1, pp2, pp3, pp4;

        if ( stok.GetNext(&fx_type) && stok.GetNext(&pp1) && stok.GetNext(&pp2) && stok.GetNext(&pp3) && stok.GetNext(&pp4) )
        {
            _vhcl->ExtDestroyFX.emplace_back();

            DestFX &dfx = _vhcl->ExtDestroyFX.back();

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
    else if ( !StriCmp(p1, "weapon") )
    {
        _vhcl->weapon = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "weapon_2") )
    {
        int weapon = parser.stol(p2, nullptr, 0);
        _vhcl->extra_weapons[0] = weapon > 0 ? weapon : 0;
    }
    else if ( !StriCmp(p1, "weapon_3") )
    {
        int weapon = parser.stol(p2, nullptr, 0);
        _vhcl->extra_weapons[1] = weapon > 0 ? weapon : 0;
    }
    else if ( !StriCmp(p1, "weapon_4") )
    {
        int weapon = parser.stol(p2, nullptr, 0);
        _vhcl->extra_weapons[2] = weapon > 0 ? weapon : 0;
    }
    else if ( !StriCmp(p1, "weapon_player_switch_mode") )
    {
        if ( !StriCmp(p2, "random") )
            _vhcl->weapon_player_switch_mode = TVhclProto::WEAPON_PLAYER_SWITCH_MODE_RANDOM;
        else if ( !StriCmp(p2, "manual") )
            _vhcl->weapon_player_switch_mode = TVhclProto::WEAPON_PLAYER_SWITCH_MODE_MANUAL;
        else if ( !StriCmp(p2, "sequence") )
            _vhcl->weapon_player_switch_mode = TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE;
        else
        {
            _vhcl->weapon_player_switch_mode = TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE;
            ypa_log_out("WARNING: vehicle %d unknown weapon_player_switch_mode '%s'; sequence used.\n",
                        _vhclID, p2.c_str());
        }
    }
    else if ( !StriCmp(p1, "weapon_ai_switch_mode") )
    {
        if ( !StriCmp(p2, "random") )
            _vhcl->weapon_ai_switch_mode = TVhclProto::WEAPON_AI_SWITCH_MODE_RANDOM;
        else if ( !StriCmp(p2, "smart") )
            _vhcl->weapon_ai_switch_mode = TVhclProto::WEAPON_AI_SWITCH_MODE_SMART;
        else if ( !StriCmp(p2, "sequence") )
            _vhcl->weapon_ai_switch_mode = TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
        else
        {
            _vhcl->weapon_ai_switch_mode = TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
            ypa_log_out("WARNING: vehicle %d unknown weapon_ai_switch_mode '%s'; sequence used.\n",
                        _vhclID, p2.c_str());
        }
    }
    else if ( !StriCmp(p1, "mgun") )
    {
        _vhcl->mgun = parser.stol(p2, nullptr, 0);
        _vhcl->mgun_set = true;
    }
    else if ( !StriCmp(p1, "mgun_shot_time") )
    {
        _vhcl->mgun_shot_time = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mgun_shot_time_user") )
    {
        _vhcl->mgun_shot_time_user = parser.stol(p2, nullptr, 0);
        if ( _vhcl->mgun_shot_time_user < 0 )
            _vhcl->mgun_shot_time_user = 0;
    }
    else if ( !StriCmp(p1, "mgun_name") )
    {
        _vhcl->mgun_name = p2;
    }
    else if ( !StriCmp(p1, "mgun_wireframe") )
    {
        if ( _vhcl->mgun_wireframe )
            _vhcl->mgun_wireframe->Delete();

        _vhcl->mgun_wireframe = Nucleus::CInit<NC_STACK_sklt>(
            {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "mgun_ground_decal_size") )
        ParseDecalSizeOverride("mgun_ground_decal_size", p2, _vhcl->mgun_ground_decal_size);
    else if ( !StriCmp(p1, "mgun_ground_decal_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _vhcl->mgun_ground_decal_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "mgun_vp_dead") )
    {
        _vhcl->mgun_vp_dead = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mgun_vp_megadeth") )
    {
        _vhcl->mgun_vp_megadeth = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mgun_3ds_dead") )
    {
        _vhcl->mgun_3ds_dead = p2;
    }
    else if ( !StriCmp(p1, "mgun_3ds_megadeth") )
    {
        _vhcl->mgun_3ds_megadeth = p2;
    }
    else if ( !StriCmp(p1, "mgun_base_dead") )
    {
        _vhcl->mgun_base_dead = p2;
    }
    else if ( !StriCmp(p1, "mgun_base_megadeth") )
    {
        _vhcl->mgun_base_megadeth = p2;
    }
    else if ( !StriCmp(p1, "mgun_cockpit_camera_recoil") )
    {
        // First-person MGUN shake intensity, 0-10. Invalid values disable it.
        float intensity = parser.stof(p2, 0);
        if ( !std::isfinite(intensity) || intensity < 0.0f )
            intensity = 0.0f;
        else if ( intensity > 10.0f )
            intensity = 10.0f;
        _vhcl->mgun_cockpit_camera_recoil = intensity;
    }
    else if ( !StriCmp(p1, "mgun_energy") )
    {
        // Energy removed by each MGUN shot. Zero or invalid keeps the vanilla
        // continuous gun_power MGUN.
        const float mgunEnergy = parser.stof(p2, 0);
        _vhcl->mgun_energy = std::isfinite(mgunEnergy) && mgunEnergy > 0.0f ? mgunEnergy : 0.0f;
        _vhcl->mgun_energy_set = _vhcl->mgun_energy > 0.0f;
    }
    else if ( !StriCmp(p1, "mgun_angle") )
    {
        _vhcl->mgun_angle = parser.stof(p2, 0);
        _vhcl->mgun_angle_set = true;
    }
    else if ( !StriCmp(p1, "weapon_spread_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _vhcl->weapon_spread_x = parser.stof(axis[0], 0);
            _vhcl->weapon_spread_y = parser.stof(axis[1], 0);
            float value = parser.stof(axis[2], 0);
            _vhcl->weapon_spread_z = std::isfinite(value) && value > 0.0f ? value : 0.0f;
        }
    }
    else if ( !StriCmp(p1, "weapon_arc_xy") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            float x = parser.stof(axis[0], 0);
            float y = parser.stof(axis[1], 0);
            _vhcl->weapon_arc_x = std::isfinite(x) && x > 0.0f ? x : 0.0f;
            _vhcl->weapon_arc_y = std::isfinite(y) && y > 0.0f ? y : 0.0f;
        }
    }
    else if ( !StriCmp(p1, "weapon_cone_xy") )
    {
        float value = parser.stof(p2, 0);
        _vhcl->weapon_cone_xy = std::isfinite(value) && value > 0.0f ? value : 0.0f;
    }
    else if ( !StriCmp(p1, "mgun_spread_xy") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            _vhcl->mgun_spread_x = parser.stof(axis[0], 0);
            _vhcl->mgun_spread_y = parser.stof(axis[1], 0);
        }
    }
    else if ( !StriCmp(p1, "fire_x") )
    {
        _vhcl->fire_x = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "fire_y") )
    {
        _vhcl->fire_y = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "fire_z") )
    {
        _vhcl->fire_z = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "fire_x_mode") )
    {
        if ( !StriCmp(p2, "sequence") )
            _vhcl->fire_x_mode = TVhclProto::FIRE_X_MODE_SEQUENCE;
        else if ( !StriCmp(p2, "random") )
            _vhcl->fire_x_mode = TVhclProto::FIRE_X_MODE_RANDOM;
        else if ( !StriCmp(p2, "salve_sequence") )
            _vhcl->fire_x_mode = TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE;
        else if ( !StriCmp(p2, "salve_mirror") )
            _vhcl->fire_x_mode = TVhclProto::FIRE_X_MODE_SALVE_MIRROR;
        else
        {
            _vhcl->fire_x_mode = TVhclProto::FIRE_X_MODE_VANILLA;
            ypa_log_out("WARNING: vehicle %d unknown fire_x_mode '%s'; vanilla fire_x selection used.\n",
                        _vhclID, p2.c_str());
        }
    }
    else if ( !StriCmp(p1, "fire_x_start") )
    {
        size_t parsed = 0;
        float value = parser.stof(p2, &parsed);
        _vhcl->fire_x_start = (parsed == p2.size() && std::isfinite(value))
                            ? value
                            : std::numeric_limits<float>::quiet_NaN();
        _vhcl->fire_x_start_defined = true;
    }
    else if ( !StriCmp(p1, "fire_x_step") )
    {
        size_t parsed = 0;
        float value = parser.stof(p2, &parsed);
        _vhcl->fire_x_step = (parsed == p2.size() && std::isfinite(value))
                           ? value
                           : std::numeric_limits<float>::quiet_NaN();
        _vhcl->fire_x_step_defined = true;
    }
    else if ( !StriCmp(p1, "fire_x_slots") )
    {
        size_t parsed = 0;
        long value = parser.stol(p2, &parsed, 0);
        _vhcl->fire_x_slots = (parsed == p2.size() &&
                               value >= std::numeric_limits<int>::min() &&
                               value <= std::numeric_limits<int>::max())
                              ? (int)value
                              : 0;
        _vhcl->fire_x_slots_defined = true;
    }
    else if ( !StriCmp(p1, "cockpit_camera_offset_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _vhcl->cockpit_camera_offset.x = parser.stof(axis[0], 0);
            _vhcl->cockpit_camera_offset.y = parser.stof(axis[1], 0);
            _vhcl->cockpit_camera_offset.z = parser.stof(axis[2], 0);
        }
    }
    else if ( !StriCmp(p1, "cockpit_gun_camera_recoil") )
    {
        _vhcl->cockpit_gun_camera_recoil = ParseBoundedPositiveFiniteOrZero(
            parser, p2, 5.0f);
    }
    else if ( !StriCmp(p1, "cockpit_camera_hide_vehicle") )
    {
        _vhcl->cockpit_camera_hide_vehicle = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "gun_radius") )
    {
        _vhcl->gun_radius = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "gun_power") )
    {
        _vhcl->gun_power = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "gun_angle") )
    {
        _vhcl->gun_angle = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "scope_angle_min_max") )
    {
        float minimum = 0.0f, maximum = 0.0f, unused = 0.0f;
        const size_t separator = p2.find('_');
        const bool valid = separator != std::string::npos &&
            p2.find('_', separator + 1) == std::string::npos &&
            World::ParseFloatRangeValue(p2.substr(0, separator), minimum, unused) &&
            World::ParseFloatRangeValue(p2.substr(separator + 1), maximum, unused) &&
            minimum >= -100.0f && maximum <= 100.0f && minimum <= maximum;
        if ( !valid )
        {
            ypa_log_out("ERROR: vehicle %d %s='%s' must be min_max references with -100 <= min <= max <= 100.\n",
                        _vhclID, p1.c_str(), p2.c_str());
            return ScriptParser::RESULT_BAD_DATA;
        }
        _vhcl->scope_angle_min_max = {{minimum, maximum}};
        _vhcl->scope_angle_min_max_set = true;
    }
    else if ( !StriCmp(p1, "num_weapons") )
    {
        int minCount = 0;
        int maxCount = 0;

        if ( !ParseNumWeaponsRange(p2, minCount, maxCount) )
        {
            ypa_log_out("WARNING: vehicle %d num_weapons='%s' is invalid; expected 0 or an inclusive value/min_max in range 1-255. Keeping the previous value.\n",
                        _vhclID, p2.c_str());
            return ScriptParser::RESULT_OK;
        }

        _vhcl->num_weapons = minCount;
        _vhcl->num_weapons_min = minCount;
        _vhcl->num_weapons_max = maxCount;
    }
    else if ( !StriCmp(p1, "mgun_fire_energy_cost") )
    {
        TAuthoredScalar value;
        _vhcl->mgun_fire_energy_cost = 0.0f;
        _vhcl->mgun_fire_energy_cost_defined = false;

        if ( ParseAuthoredScalar(p2, value) && value.percent &&
             std::isfinite(value.value) && value.value >= 0.0f )
        {
            _vhcl->mgun_fire_energy_cost = std::min(value.value, 100.0f);
            _vhcl->mgun_fire_energy_cost_defined = true;
        }
        else
        {
            ypa_log_out("WARNING: vehicle %d mgun_fire_energy_cost='%s' is invalid; expected an explicit percentage in range 0%%-100%%. Falling back to legacy MGUN energy drain.\n",
                        _vhclID, p2.c_str());
        }
    }
    else if ( !StriCmp(p1, "num_weapons_snd_events") )
    {
        int value = parser.stol(p2, nullptr, 0);
        _vhcl->num_weapons_snd_events =
            value > 0 ? std::min(value, 255) : 0;
    }
    else if ( !StriCmp(p1, "num_weapons_2") ||
              !StriCmp(p1, "num_weapons_3") ||
              !StriCmp(p1, "num_weapons_4") )
    {
        int sourceSlot = p1.back() - '2';
        int minCount = 0;
        int maxCount = 0;

        if ( !ParseNumWeaponsRange(p2, minCount, maxCount) )
        {
            ypa_log_out("WARNING: vehicle %d %s='%s' is invalid; expected 0/inherit or an inclusive value/min_max in range 1-255. Keeping the previous value.\n",
                        _vhclID, p1.c_str(), p2.c_str());
        }
        else
        {
            _vhcl->extra_num_weapons[sourceSlot] = minCount;
            _vhcl->extra_num_weapons_min[sourceSlot] = minCount;
            _vhcl->extra_num_weapons_max[sourceSlot] = maxCount;
        }
    }
    else if ( !StriCmp(p1, "kill_after_shot") )
    {
        _vhcl->kill_after_shot = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "job_fighthelicopter") )
    {
        _vhcl->job_fighthelicopter = parser.stoi(p2);
        _vhcl->job_fighthelicopter_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightflyer") )
    {
        _vhcl->job_fightflyer = parser.stoi(p2);
        _vhcl->job_fightflyer_defined = true;
    }
    else if ( !StriCmp(p1, "job_fighttank") )
    {
        _vhcl->job_fighttank = parser.stoi(p2);
        _vhcl->job_fighttank_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightrobo") )
    {
        _vhcl->job_fightrobo = parser.stoi(p2);
        _vhcl->job_fightrobo_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightplane") )
    {
        _vhcl->job_fightplane = parser.stoi(p2);
        _vhcl->job_fightplane_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightcruiser") )
    {
        _vhcl->job_fightcruiser = parser.stoi(p2);
        _vhcl->job_fightcruiser_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightglider") )
    {
        _vhcl->job_fightglider = parser.stoi(p2);
        _vhcl->job_fightglider_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightzeppelin") )
    {
        _vhcl->job_fightzeppelin = parser.stoi(p2);
        _vhcl->job_fightzeppelin_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightufo") )
    {
        _vhcl->job_fightufo = parser.stoi(p2);
        _vhcl->job_fightufo_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightcar") )
    {
        _vhcl->job_fightcar = parser.stoi(p2);
        _vhcl->job_fightcar_defined = true;
    }
    else if ( !StriCmp(p1, "job_fightgun") )
    {
        _vhcl->job_fightgun = parser.stoi(p2);
        _vhcl->job_fightgun_defined = true;
    }
    else if ( !StriCmp(p1, "job_reconnoitre") )
    {
        _vhcl->job_reconnoitre = parser.stoi(p2);
        _vhcl->job_reconnoitre_defined = true;
    }
    else if ( !StriCmp(p1, "job_conquer") )
    {
        _vhcl->job_conquer = parser.stoi(p2);
        _vhcl->job_conquer_defined = true;
    }
    else if ( !StriCmp(p1, "gun_side_angle") )
    {
        _vhcl->initParams.Add(NC_STACK_ypagun::GUN_ATT_SIDEANGLE, (int32_t)parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "gun_up_angle") )
    {
        _vhcl->initParams.Add(NC_STACK_ypagun::GUN_ATT_UPANGLE, (int32_t)parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "gun_down_angle") )
    {
        _vhcl->initParams.Add(NC_STACK_ypagun::GUN_ATT_DOWNANGLE, (int32_t)parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "gun_type") )
    {
        int gun_type = NC_STACK_ypagun::GUN_TYPE_REAL;
        bool recognized = true;
        if ( !StriCmp(p2, "flak") )
        {
            gun_type = NC_STACK_ypagun::GUN_TYPE_REAL;
        }
        else if ( !StriCmp(p2, "mg") )
        {
            gun_type = NC_STACK_ypagun::GUN_TYPE_PROTO;
        }
        else if ( !StriCmp(p2, "dummy") )
        {
            gun_type = NC_STACK_ypagun::GUN_TYPE_DUMMY;
        }
        else if ( !StriCmp(p2, "radar") )
        {
            gun_type = NC_STACK_ypagun::GUN_TYPE_RADAR;
        }
        else if ( !StriCmp(p2, "power") )
        {
            gun_type = NC_STACK_ypagun::GUN_TYPE_POWER;
        }
        else
        {
            recognized = false;
        }

        if ( recognized )
            _vhcl->initParams.Add(NC_STACK_ypagun::GUN_ATT_FIRETYPE, (int32_t)gun_type);
    }
    else if ( !StriCmp(p1, "gun_levitate") )
    {
        if ( _vhcl->model_id == BACT_TYPES_GUN )
            _vhcl->initParams.Add(NC_STACK_ypagun::GUN_ATT_LEVITATE, (int32_t)(p2 == "1"));
    }
    else if ( !StriCmp(p1, "kamikaze") )
    {
        _vhcl->initParams.Add(NC_STACK_ypacar::CAR_ATT_KAMIKAZE, (int32_t)1);

        _vhcl->initParams.Add(NC_STACK_ypacar::CAR_ATT_BLAST, (int32_t)parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "wireframe") )
    {
        if ( _vhcl->wireframe )
            _vhcl->wireframe->Delete();

        _vhcl->wireframe = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "hud_wireframe") )
    {
        if ( _vhcl->hud_wireframe )
            _vhcl->hud_wireframe->Delete();

        _vhcl->hud_wireframe = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "mg_wireframe") )
    {
        if ( _vhcl->mg_wireframe )
            _vhcl->mg_wireframe->Delete();

        _vhcl->mg_wireframe = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "wpn_wireframe_1") )
    {
        if ( _vhcl->wpn_wireframe_1 )
            _vhcl->wpn_wireframe_1->Delete();

        _vhcl->wpn_wireframe_1 = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "wpn_wireframe_2") )
    {
        if ( _vhcl->wpn_wireframe_2 )
            _vhcl->wpn_wireframe_2->Delete();

        _vhcl->wpn_wireframe_2 = Nucleus::CInit<NC_STACK_sklt>( {{NC_STACK_rsrc::RSRC_ATT_NAME, std::string(p2)}} );
    }
    else if ( !StriCmp(p1, "vo_type") )
    {
        _vhcl->vo_type = parser.stol(p2, nullptr, 16);
    }
    else if ( !StriCmp(p1, "speech_voicepack") )
    {
        if ( p2.empty() )
            return ScriptParser::RESULT_BAD_DATA;
        _vhcl->speech_voicepack = p2 == "0" ? "" : p2;
    }

    else if ( !StriCmp(p1, "max_pitch") )
    {
        _vhcl->max_pitch = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "scale_fx") )
    {
        Stok stok(p2, "_");
        std::string pp0, pp1, pp2, pp3;

        if ( stok.GetNext(&pp0) && stok.GetNext(&pp1) && stok.GetNext(&pp2) && stok.GetNext(&pp3) )
        {
            ResetVehicleScaleFX(_vhcl);

            _vhcl->scale_fx_p0 = parser.stof(pp0, 0);
            _vhcl->scale_fx_p1 = parser.stof(pp1, 0);
            _vhcl->scale_fx_p2 = parser.stof(pp2, 0);
            _vhcl->scale_fx_p3 = parser.stol(pp3, nullptr, 0);

            int tmp = 0;
            while ( stok.GetNext(&pp0) )
            {
                _vhcl->scale_fx_pXX[tmp] = parser.stol(pp0, nullptr, 0);
                tmp++;
            }
        }
    }
    else if ( !StriCmp(p1, "begin_fx") )
    {
        return ParseVehicleChainFXBlock(parser, _vhcl);
    }
    else if ( !StriCmp(p1, "robo_data_slot") )
    {
    }
    else if ( !StriCmp(p1, "robo_num_guns") )
    {
        int cnt = parser.stol(p2, nullptr, 0);

        if ( cnt < 0 )
            cnt = 0;
        else if ( cnt > (int)ROBO_GUN_MAX_COUNT )
            cnt = ROBO_GUN_MAX_COUNT;

        robo->guns.resize(cnt);
        if(_gunID<0&&cnt>0)_gunID=0;

        if ( _vhcl->model_id == BACT_TYPES_ROBO && _unitGunID >= cnt )
            _unitGunID = cnt - 1;
    }
    else if ( !StriCmp(p1, "robo_act_gun") )
    {
        _gunID = parser.stol(p2, nullptr, 0);

        // robo_* and unit_* select the same native Robo gun slot when parsing
        // a Host Station, so mixed syntax remains deterministic: later fields
        // overwrite the same TRoboGun entry instead of creating duplicates.
        if ( _vhcl->model_id == BACT_TYPES_ROBO )
            _unitGunID = _gunID;
    }
    else if ( !StriCmp(p1, "robo_gun_pos_x") )
    {
        robo->guns[ _gunID ].pos.x = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_pos_y") )
    {
        robo->guns[ _gunID ].pos.y = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_pos_z") )
    {
        robo->guns[ _gunID ].pos.z = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_dir_x") )
    {
        robo->guns[ _gunID ].dir.x = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_dir_y") )
    {
        robo->guns[ _gunID ].dir.y = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_dir_z") )
    {
        robo->guns[ _gunID ].dir.z = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_type") )
    {
        robo->guns[ _gunID ].robo_gun_type = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "robo_gun_name") )
    {
        robo->guns[ _gunID ].robo_gun_name = p2;
    }
    else if ( !StriCmp(p1, "unit_num_guns") )
    {
        int cnt = parser.stol(p2, nullptr, 0);

        if ( cnt < 0 )
            cnt = 0;
        else if ( cnt > (int)ROBO_GUN_MAX_COUNT )
            cnt = ROBO_GUN_MAX_COUNT;

        if ( _vhcl->model_id == BACT_TYPES_ROBO )
        {
            robo->guns.resize(cnt);

            if ( _gunID >= cnt )
                _gunID = cnt - 1;
        }
        else
        {
            _vhcl->unit_guns.resize(cnt);
        }

        if ( _unitGunID >= cnt )
            _unitGunID = cnt - 1;
    }
    else if ( !StriCmp(p1, "unit_act_gun") )
    {
        _unitGunID = parser.stol(p2, nullptr, 0);

        if ( _unitGunID < 0 )
            _unitGunID = 0;

        if ( _unitGunID >= (int)ROBO_GUN_MAX_COUNT )
            _unitGunID = ROBO_GUN_MAX_COUNT - 1;

        if ( _vhcl->model_id == BACT_TYPES_ROBO )
        {
            _gunID = _unitGunID;

            if ( (size_t)_unitGunID >= robo->guns.size() )
                robo->guns.resize(_unitGunID + 1);
        }
        else if ( (size_t)_unitGunID >= _vhcl->unit_guns.size() )
        {
            _vhcl->unit_guns.resize(_unitGunID + 1);
        }
    }
    else if ( !StriCmp(p1, "unit_gun_pos_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            if (TRoboGun *gun = getUnitGun())
            {
                gun->pos.x = parser.stof(axis[0], 0);
                gun->pos.y = parser.stof(axis[1], 0);
                gun->pos.z = parser.stof(axis[2], 0);
            }
        }
    }
    else if ( !StriCmp(p1, "unit_gun_dir_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            if (TRoboGun *gun = getUnitGun())
            {
                gun->dir.x = parser.stof(axis[0], 0);
                gun->dir.y = parser.stof(axis[1], 0);
                gun->dir.z = parser.stof(axis[2], 0);
            }
        }
    }
    else if ( !StriCmp(p1, "unit_gun_type") )
    {
        if (TRoboGun *gun = getUnitGun())
            gun->robo_gun_type = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "unit_gun_name") )
    {
        if (TRoboGun *gun = getUnitGun())
            gun->robo_gun_name = p2;
    }
    else if ( !StriCmp(p1, "unit_gun_protect") )
    {
        if (TRoboGun *gun = getUnitGun())
            gun->protect = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "unit_gun_no_switch") )
    {
        if (TRoboGun *gun = getUnitGun())
            gun->noSwitch = parser.stol(p2, nullptr, 0) == 1 ? 1 : 0;
    }
    else if ( !StriCmp(p1, "robo_dock_x") )
    {
        robo->dock.x = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_dock_y") )
    {
        robo->dock.y = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_dock_z") )
    {
        robo->dock.z = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_coll_num") )
    {
        int cnt = parser.stol(p2, nullptr, 0);

        if ( cnt < 0 )
            cnt = 0;
        else if ( cnt > (int)UNIT_COLL_MAX_COUNT )
            cnt = UNIT_COLL_MAX_COUNT;

        robo->coll.roboColls.resize(cnt);

        if ( _collID >= cnt )
            _collID = cnt - 1;
    }
    else if ( !StriCmp(p1, "robo_coll_act") )
    {
        _collID = parser.stol(p2, nullptr, 0);

        if ( _collID < 0 )
            _collID = 0;

        if ( _collID >= (int)UNIT_COLL_MAX_COUNT )
            _collID = UNIT_COLL_MAX_COUNT - 1;

        if ( (size_t)_collID >= robo->coll.roboColls.size() )
            robo->coll.roboColls.resize(_collID + 1);
    }
    else if ( !StriCmp(p1, "robo_coll_radius") )
    {
        if (TRoboColl *c = getRoboColl())
            c->robo_coll_radius = std::max(0.0f, ParseFiniteCollisionFloat(parser, p2));
    }
    else if ( !StriCmp(p1, "robo_coll_x") )
    {
        if (TRoboColl *c = getRoboColl())
            c->coll_pos.x = ParseFiniteCollisionFloat(parser, p2);
    }
    else if ( !StriCmp(p1, "robo_coll_y") )
    {
        if (TRoboColl *c = getRoboColl())
            c->coll_pos.y = ParseFiniteCollisionFloat(parser, p2);
    }
    else if ( !StriCmp(p1, "robo_coll_z") )
    {
        if (TRoboColl *c = getRoboColl())
            c->coll_pos.z = ParseFiniteCollisionFloat(parser, p2);
    }
    else if ( !StriCmp(p1, "robo_viewer_x") )
    {
        robo->viewer.x = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_viewer_y") )
    {
        robo->viewer.y = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_viewer_z") )
    {
        robo->viewer.z = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_viewer_max_up") )
    {
        robo->robo_viewer_max_up = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_viewer_max_down") )
    {
        robo->robo_viewer_max_down = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_viewer_max_side") )
    {
        robo->robo_viewer_max_side = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "robo_does_twist") )
    {
        _vhcl->initParams.Add(NC_STACK_yparobo::ROBO_ATT_WAIT_ROTATE, (int32_t)1);
    }
    else if ( !StriCmp(p1, "robo_does_flux") )
    {
        _vhcl->initParams.Add(NC_STACK_yparobo::ROBO_ATT_WAIT_SWAY, (int32_t)1);
    }
    else if ( !StriCmp(p1, "hidden") )
    {
        _vhcl->hidden = StrGetBool(p2);
    }
    else if ( !StriCmp(p1, "unhide_radar") )
    {
        _vhcl->unhideRadar = parser.stol(p2, nullptr, 0);

        if (_vhcl->unhideRadar < 0)
            _vhcl->unhideRadar = 0;
        else if (_vhcl->unhideRadar > _vhcl->radar)
            _vhcl->unhideRadar = _vhcl->radar + 1;
    }
    else if ( !StriCmp(p1, "add_unhide_radar") )
    {
        _vhcl->unhideRadar += parser.stol(p2, nullptr, 0);

        if (_vhcl->unhideRadar < 0)
            _vhcl->unhideRadar = 0;
        else if (_vhcl->unhideRadar > _vhcl->radar)
            _vhcl->unhideRadar = _vhcl->radar + 1;
    }
    else
        return ParseSndFX(parser, p1, p2);

    return ScriptParser::RESULT_OK;
}

bool VhclProtoParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( !StriCmp(word, "new_vehicle") )
    {
        _isModify = false;
        _roboTmp = TRoboProto();
        _gunID = -1;
        _unitGunID = -1;
        _collID = -1;
        _vhclID = _forcedVhclID >= 0 ? _forcedVhclID : parser.stol(opt, nullptr, 0);
        _vhcl = &_o._vhclProtos.at(_vhclID);

        *_vhcl = TVhclProto();

        _vhcl->Index = _vhclID;

        _vhcl->model_id = BACT_TYPES_TANK;
        _vhcl->weapon = -1;
        _vhcl->extra_weapons = {0, 0, 0};
        _vhcl->extra_num_weapons = {0, 0, 0};
        _vhcl->extra_num_weapons_min = {0, 0, 0};
        _vhcl->extra_num_weapons_max = {0, 0, 0};
        _vhcl->weapon_player_switch_mode = TVhclProto::WEAPON_PLAYER_SWITCH_MODE_SEQUENCE;
        _vhcl->weapon_ai_switch_mode = TVhclProto::WEAPON_AI_SWITCH_MODE_SEQUENCE;
        _vhcl->mgun = -1;
        _vhcl->mgun_set = false;
        _vhcl->mgun_shot_time = 0;
        _vhcl->mgun_shot_time_user = 0;
        _vhcl->mgun_decal_enable = false;
        _vhcl->mgun_decal = World::TChainFXConfig();
        _vhcl->mgun_ground_decal_id = -1;
        _vhcl->mgun_ground_decal_size = TDecalSizeOverride();
        _vhcl->mgun_vp_dead = 0;
        _vhcl->mgun_vp_megadeth = 0;
        _vhcl->mgun_energy = 0.0;
        _vhcl->mgun_angle = 0.0;
        _vhcl->mgun_energy_set = false;
        _vhcl->mgun_cockpit_camera_recoil = 0.0f;
        _vhcl->mgun_angle_set = false;
        _vhcl->mgun_3ds_dead.clear();
        _vhcl->mgun_3ds_megadeth.clear();
        _vhcl->mgun_base_dead.clear();
        _vhcl->mgun_base_megadeth.clear();
        _vhcl->weapon_spread_x = 0.0;
        _vhcl->weapon_spread_y = 0.0;
        _vhcl->weapon_spread_z = 0.0;
        _vhcl->weapon_arc_x = 0.0;
        _vhcl->weapon_arc_y = 0.0;
        _vhcl->weapon_cone_xy = 0.0;
        _vhcl->mgun_spread_x = 0.0;
        _vhcl->mgun_spread_y = 0.0;
        _vhcl->type_icon = 65;
        _vhcl->vp_normal = 0;
        _vhcl->vp_fire = 1;
        _vhcl->vp_megadeth = 2;
        _vhcl->vp_wait = 3;
        _vhcl->vp_dead = 4;
        _vhcl->vp_genesis = 5;
        _vhcl->visual_3ds = TExternalVisualSet();
        _vhcl->visual_base = TExternalVisualSet();
        _vhcl->visual_scale = vec3d(1.0, 1.0, 1.0);
        _vhcl->collision_shape.clear();
        _vhcl->visual_rotation = vec3d(0.0, 0.0, 0.0);
        _vhcl->visual_spin = vec3d(0.0, 0.0, 0.0);
        _vhcl->visual_tint = TVisualTint();
        _vhcl->damaged_fx = TDamagedFXConfig();
        _vhcl->unit_gun_icon.clear();
        _vhcl->power = 0;
        _vhcl->power_radius = 0.0;
        _vhcl->power_falloff = 1;
        _vhcl->power_cloud_id = -1;
        _vhcl->spy_ui_radius = 0.0f;
        _vhcl->zoom_steps = -1;
        _vhcl->damaged_force_malus = 0.0;
        _vhcl->damaged_maxrot_malus = 0.0;
        _vhcl->damaged_mgun_shot_time_malus = 0.0;
        _vhcl->damaged_shot_time_malus = 0.0;
        _vhcl->damaged_snd_pitch_multiplier = 1.0;
        _vhcl->spawn_units = 0;
        _vhcl->spawn_vehicle = 0;
        _vhcl->spawn_interval = 5000;
        _vhcl->spawn_trigger_radius = 0.0;
        _vhcl->spawn_random_pos = 0.0;
        _vhcl->spawn_offset = vec3d(0.0, 0.0, 0.0);
        _vhcl->spawn_max_active = 0;
        _vhcl->spawn_count = 1;
        _vhcl->spawn_instant = 0;
        _vhcl->at_death_spawn_units = 0;
        _vhcl->at_death_spawn_vehicle = 0;
        _vhcl->at_death_spawn_count = 1;
        _vhcl->at_death_spawn_random_pos = 0.0;
        _vhcl->at_death_spawn_instant = 0;
        _vhcl->at_death_spawn_immunity_time = 0;
        _vhcl->proximity_defense_enable = 0;
        _vhcl->proximity_defense_weapon = 0;
        _vhcl->proximity_defense_trigger_radius = 0.0;
        _vhcl->proximity_defense_interval = 1000;
        _vhcl->proximity_defense_shots = 12;
        _vhcl->proximity_defense_vp_launch = -1;
        _vhcl->proximity_defense_3ds_launch.clear();
        _vhcl->proximity_defense_base_launch.clear();
        _vhcl->proximity_defense_fire_mode = 0;
        _vhcl->proximity_defense_sequence_delay = 100;
        _vhcl->proximity_defense_mode = 0;
        _vhcl->proximity_defense_horizontal_angle_set = false;
        _vhcl->proximity_defense_horizontal_angle_min = 0.0;
        _vhcl->proximity_defense_horizontal_angle_max = 360.0;
        _vhcl->proximity_defense_vertical_angle_set = false;
        _vhcl->proximity_defense_vertical_angle_min = -10.0;
        _vhcl->proximity_defense_vertical_angle_max = 45.0;
        _vhcl->max_active_at_once = 0;
        _vhcl->shield = 50;
        _vhcl->buff_id = -1;
        _vhcl->ground_decal_id = -1;
        _vhcl->ground_decal_size = TDecalSizeOverride();
        _vhcl->buff = TVehicleBuffConfig();
        _vhcl->energy = 10000;
        _vhcl->mimic_energy_cost = 0;
        _vhcl->mimic_energy_cost_min = 0;
        _vhcl->mimic_energy_cost_max = 0;
        _vhcl->production_cost = 0;
        _vhcl->adist_sector = 800.0;
        _vhcl->adist_bact = 650.0;
        _vhcl->sdist_sector = 200.0;
        _vhcl->sdist_bact = 100.0;
        _vhcl->radar = 1;
        _vhcl->kill_after_shot = 0;
        _vhcl->push_resistance = 0.0;
        _vhcl->has_push_resistance = false;
        _vhcl->at_death_push_force = 0.0f;
        _vhcl->at_death_push_radius = 0.0f;
        _vhcl->at_death_push_falloff = 0;
        _vhcl->at_death_energy = 0;
        _vhcl->at_death_energy_radius = 0.0f;
        _vhcl->at_death_energy_falloff = 0;
        _vhcl->mass = 400.0;
        _vhcl->force = 5000.0;
        _vhcl->airconst = 80.0;
        _vhcl->maxrot = 0.8;
        _vhcl->height = 150.0;
        _vhcl->radius = 25.0;
        _vhcl->radius_defined = false;
        _vhcl->briefing_size = 0.0f;
        _vhcl->briefing_xyz = vec3d();
        _vhcl->overeof = 25.0;
        _vhcl->vwr_radius = 30.0;
        _vhcl->vwr_overeof = 30.0;
        _vhcl->cockpit_camera_offset = vec3d(0.0, 0.0, 0.0);
        _vhcl->cockpit_gun_camera_recoil = 0.0f;
        _vhcl->cockpit_camera_hide_vehicle = false;
        _vhcl->gun_power = 4000.0;
        _vhcl->gun_radius = 5.0;
        _vhcl->max_pitch = -1.0;
        _vhcl->job_fightflyer = 0;
        _vhcl->job_fighthelicopter = 0;
        _vhcl->job_fightrobo = 0;
        _vhcl->job_fighttank = 0;
        _vhcl->job_reconnoitre = 0;
        _vhcl->job_conquer = 0;

        for (auto &x : _vhcl->sndFX)
        {
            x.sndPrm.mag0 = 1.0;
            x.sndPrm_shk.mag0 = 1.0;
            x.sndPrm_shk.mute = 0.02;
            x.sndPrm_shk.pos.x = 0.2;
            x.sndPrm_shk.pos.y = 0.2;
            x.sndPrm_shk.pos.z = 0.2;
            x.volume = 120;
            x.sndPrm.time = 1000;
            x.sndPrm_shk.time = 1000;
        }

        // Pickup audio is global in Nucleus.ini rather than authored per Vehicle.
        // The event remains attached to the collecting unit so playback stays positional.
        TVhclSound &pickupSnd = _vhcl->sndFX[TVhclProto::SND_PICKUP];
        pickupSnd.MainSample.Name =
            System::IniConf::GamePlasmaSndPickupSample.Get<std::string>();
        const int pickupVolume =
            System::IniConf::GamePlasmaSndPickupVolume.Get<int32_t>();
        pickupSnd.volume = pickupVolume >= 0 ? pickupVolume : 90;
        ParseSoundPitchRange(
            System::IniConf::GamePlasmaSndPickupPitch.Get<std::string>(), pickupSnd);
        // Radius remains 0 so SFXEngine uses the classic legacy distance attenuation.

        _vhcl->sndFX[TVhclProto::SND_HANDBRAKE].MainSample.Name =
            System::IniConf::GameHandBrakeSound.Get<std::string>();

        _vhcl->initParams.clear();
        _vhcl->is_mimic = 0;
        _vhcl->mimic_tint = TVisualTint();
        _vhcl->snd_mimic = TVhclSound();
        _vhcl->coll = rbcolls();
        return true;
    }
    else if ( !StriCmp(word, "modify_vehicle") )
    {
        _gunID = -1;
        _unitGunID = -1;
        _collID = -1;
        _vhclID = parser.stol(opt, nullptr, 0);

        if ( _vhclID < 0 || (size_t)_vhclID >= _o._vhclProtos.size() )
        {
            ypa_log_out("WARNING: modify_vehicle ignored invalid prototype ID %d.\n", _vhclID);
            _vhcl = nullptr;
            _isModify = false;
            return false;
        }

        _vhcl = &_o._vhclProtos.at(_vhclID);
        _isModify = true;

        _vhcl->Index = _vhclID;

        _o._upgradeVehicleId = _vhclID;
        return true;
    }

    return false;
}

}
}
