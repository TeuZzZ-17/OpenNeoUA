// Script parsers for world data: buildings, movies, backgrounds, colors,
// misc settings, atmospheric FX and super items.
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

bool BuildProtoParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (!StriCmp(word, "new_building"))
    {
        _isModify = false;
        _bldID = parser.stol(opt, nullptr, 0);

        _o._buildProtos[_bldID] = TBuildingProto();
        _bld = &_o._buildProtos[_bldID];
        _bld->Index = _bldID;
        _bld->Energy = 50000;
        _bld->TypeIcon = 65;
        _bld->SndFX.volume = 120;
        _bld->Guns.clear();
        return true;
    }
    else if (!StriCmp(word, "modify_building"))
    {
        _bldID = parser.stol(opt, nullptr, 0);

        if ( _bldID < 0 || (size_t)_bldID >= _o._buildProtos.size() )
        {
            ypa_log_out("WARNING: modify_building ignored invalid prototype ID %d.\n", _bldID);
            _bld = nullptr;
            _isModify = false;
            return false;
        }

        _bld = &_o._buildProtos[_bldID];
        _bld->Index = _bldID;
        _isModify = true;
        _o._upgradeBuildId = _bldID;
        return true;
    }

    return false;
}

int BuildProtoParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( !StriCmp(p1, "model") )
    {
        _bld->script_class = p2;
        if ( !StriCmp(p2, "building") )
        {
            _bld->ModelID = 0;
        }
        else if ( !StriCmp(p2, "kraftwerk") )
        {
            _bld->ModelID = 1;
        }
        else if ( !StriCmp(p2, "radar") )
        {
            _bld->ModelID = 2;
        }
        else if ( !StriCmp(p2, "defcenter") )
        {
            _bld->ModelID = 3;
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "enable") )
    {
        int fraction = parser.stol(p2, nullptr, 0);
        _bld->EnableMask |= 1 << fraction;
    }
    else if ( !StriCmp(p1, "disable") )
    {
        _bld->EnableMask &= ~(1 << parser.stol(p2, nullptr, 0));
    }
    else if ( !StriCmp(p1, "name") )
    {
        _bld->Name = p2;
        std::replace(_bld->Name.begin(), _bld->Name.end(), '_', ' ');
    }
    else if ( !StriCmp(p1, "building_description") )
        _bld->description = parser.FullLineValue();
    else if(!StriCmp(p1,"shield"))
    {
        const long value=parser.stol(p2,nullptr,0);
        _bld->shield=value>=0&&value<=100?value:-1;
    }
    else if(!StriCmp(p1,"radar"))
    {
        _bld->radar=std::max(0,parser.stoi(p2));
        _bld->radar_defined=true;
    }
    else if ( !StriCmp(p1, "power") )
    {
        _bld->Power = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "energy") )
    {
        _bld->Energy = parser.stol(p2, nullptr, 0);
        _bld->energy_defined = true;
    }
    else if ( !StriCmp(p1, "production_cost") )
    {
        // Displayed Genesis price. Zero or invalid keeps vanilla Energy-as-cost.
        const long value = parser.stol(p2, nullptr, 0);
        _bld->production_cost = value > 0 ? (int)value : 0;
        _bld->production_cost_defined = value >= 0;
    }
    else if ( ParseTintParam(parser, "tint", p1, p2, _bld->tint, true) )
    {
    }
    else if ( ParseDecorationFXParam(parser, p1, p2, _bld->DecorationFX) )
    {
    }
    else if ( !StriCmp(p1, "sec_type") )
    {
        _bld->SecType = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "type_icon") )
    {
        uint32_t glyph;
        if (GlyphKey::Parse(p2, glyph))
        {
            _bld->TypeIcon = glyph;
            _bld->type_icon_defined = true;
        }
        else
            ypa_log_out("WARNING: invalid type_icon; previous icon retained.\n");
    }
    else if ( !StriCmp(p1, "spawn_units") )
    {
        _bld->spawn_units = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "spawn_vehicle") )
    {
        _bld->spawn_vehicle = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "spawn_interval") )
    {
        int interval = parser.stol(p2, nullptr, 0);
        _bld->spawn_interval = interval > 0 ? interval : 0;
    }
    else if ( !StriCmp(p1, "spawn_trigger_radius") )
    {
        float radius = parser.stof(p2, 0);
        _bld->spawn_trigger_radius = radius > 0.0 ? radius : 0.0;
    }
    else if ( !StriCmp(p1, "spawn_random_pos") )
    {
        float radius = ParseFiniteFloatOrFallback(parser, p2, 340.0f);
        _bld->spawn_random_pos = radius >= 0.0 ? radius : 340.0f;
    }
    else if ( !StriCmp(p1, "spawn_offset_xz") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            _bld->spawn_offset.x = ParseFiniteFloatOrFallback(parser, axis[0], 37.0f);
            _bld->spawn_offset.z = ParseFiniteFloatOrFallback(parser, axis[1], -41.0f);
        }
    }
    else if ( !StriCmp(p1, "spawn_height") )
    {
        float minHeight = 650.0f;
        float maxHeight = 900.0f;
        if ( !ParseScriptFloatRange(p2, minHeight, maxHeight) ||
             minHeight < 0.0f || maxHeight < 0.0f )
        {
            minHeight = 650.0f;
            maxHeight = 900.0f;
        }
        _bld->spawn_height_min = minHeight;
        _bld->spawn_height_max = maxHeight;
    }
    else if ( !StriCmp(p1, "spawn_max_active") )
    {
        int maxActive = parser.stol(p2, nullptr, 0);
        _bld->spawn_max_active = maxActive > 0 ? maxActive : 0;
    }
    else if ( !StriCmp(p1, "spawn_count") )
    {
        int count = parser.stol(p2, nullptr, 0);
        _bld->spawn_count = count > 0 ? count : 1;
        _bld->spawn_count_defined = true;
    }
    else if ( !StriCmp(p1, "spawn_instant") )
    {
        _bld->spawn_instant = parser.stol(p2, nullptr, 0) ? 1 : 0;
    }
    else if ( !StriCmp(p1, "snd_normal_sample") )
    {
        _bld->SndFX.MainSample.Name = p2;
    }
    else if ( !StriCmp(p1, "snd_normal_volume") )
    {
        _bld->SndFX.volume = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "snd_normal_pitch") )
    {
        ParseSoundPitchRange(p2, _bld->SndFX);
    }
    else if ( !StriCmp(p1, "sbact_act") )
    {
        _gunID = parser.stol(p2, nullptr, 0);
        if (_gunID >= _bld->Guns.size())
            _bld->Guns.resize(_gunID + 1);
    }
    else
    {
        TBuildingProto::TGun &pGun = _bld->Guns.at(_gunID);
        if ( !StriCmp(p1, "sbact_vehicle") )
        {
            pGun.VhclID = parser.stol(p2, nullptr, 0);
        }
        else if ( !StriCmp(p1, "sbact_pos_x") )
        {
            pGun.Pos.x = parser.stof(p2, 0);
        }
        else if ( !StriCmp(p1, "sbact_pos_y") )
        {
            pGun.Pos.y = parser.stof(p2, 0);
        }
        else if ( !StriCmp(p1, "sbact_pos_z") )
        {
            pGun.Pos.z = parser.stof(p2, 0);
        }
        else if ( !StriCmp(p1, "sbact_dir_x") )
        {
            pGun.Dir.x = parser.stof(p2, 0);
        }
        else if ( !StriCmp(p1, "sbact_dir_y") )
        {
            pGun.Dir.y = parser.stof(p2, 0);
        }
        else if ( !StriCmp(p1, "sbact_dir_z") )
        {
            pGun.Dir.z = parser.stof(p2, 0);
        }
        else
            return ScriptParser::RESULT_UNKNOWN;
    }

    return ScriptParser::RESULT_OK;
}

bool MovieParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_movies") )
        return false;

    for (std::string &movie : _o._movies)
        movie.clear();
    return true;
}

int MovieParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
	if ( !StriCmp(p1, "end") )
		return ScriptParser::RESULT_SCOPE_END;
	else if ( !StriCmp(p1, "game_intro") )
		_o._movies[World::MOVIE_INTRO] = p2;
	else if ( !StriCmp(p1, "win_extro") )
		_o._movies[World::MOVIE_WIN] = p2;
	else if ( !StriCmp(p1, "lose_extro") )
		_o._movies[World::MOVIE_LOSE] = p2;
	else if ( !StriCmp(p1, "user_intro") )
		_o._movies[World::MOVIE_USER] = p2;
	else if ( !StriCmp(p1, "kyt_intro") )
		_o._movies[World::MOVIE_KYT] = p2;
	else if ( !StriCmp(p1, "taer_intro") )
		_o._movies[World::MOVIE_TAER] = p2;
	else if ( !StriCmp(p1, "myk_intro") )
		_o._movies[World::MOVIE_MYK] = p2;
	else if ( !StriCmp(p1, "sulg_intro") )
		_o._movies[World::MOVIE_SULG] = p2;
	else if ( !StriCmp(p1, "black_intro") )
		_o._movies[World::MOVIE_BLACK] = p2;
	else
		return ScriptParser::RESULT_UNKNOWN;
	return ScriptParser::RESULT_OK;
}

BkgParser::BkgParser(NC_STACK_ypaworld *o)
: _o(o->_globalMapRegions)
{}

int BkgParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        _o.NumSets++;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "background_map") )
    {
        _o.background_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "rollover_map") )
    {
        _o.rollover_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "finished_map") )
    {
        _o.finished_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "enabled_map") )
    {
        _o.enabled_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "mask_map") )
    {
        _o.mask_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "tut_background_map") )
    {
        _o.tut_background_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "tut_rollover_map") )
    {
        _o.tut_rollover_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "tut_mask_map") )
    {
        _o.tut_mask_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "menu_map") )
    {
        _o.menu_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "input_map") )
    {
        _o.input_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "settings_map") )
    {
        _o.settings_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "network_map") )
    {
        _o.network_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "locale_map") )
    {
        _o.locale_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "save_map") )
    {
        _o.save_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "about_map") )
    {
        _o.about_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "help_map") )
    {
        _o.help_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "brief_map") )
    {
        _o.brief_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "debrief_map") )
    {
        _o.debrief_map[_o.NumSets].PicName = p2;
    }
    else if ( !StriCmp(p1, "size_x") )
    {
        _o.background_map[_o.NumSets].Size.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "size_y") )
    {
        _o.background_map[_o.NumSets].Size.y = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

int ColorParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "owner_0") )
    {
        _o.ParseColorString(World::COLOR_OWNER_0, p2);
    }
    else if ( !StriCmp(p1, "owner_1") )
    {
        _o.ParseColorString(World::COLOR_OWNER_1, p2);
    }
    else if ( !StriCmp(p1, "owner_2") )
    {
        _o.ParseColorString(World::COLOR_OWNER_2, p2);
    }
    else if ( !StriCmp(p1, "owner_3") )
    {
        _o.ParseColorString(World::COLOR_OWNER_3, p2);
    }
    else if ( !StriCmp(p1, "owner_4") )
    {
        _o.ParseColorString(World::COLOR_OWNER_4, p2);
    }
    else if ( !StriCmp(p1, "owner_5") )
    {
        _o.ParseColorString(World::COLOR_OWNER_5, p2);
    }
    else if ( !StriCmp(p1, "owner_6") )
    {
        _o.ParseColorString(World::COLOR_OWNER_6, p2);
    }
    else if ( !StriCmp(p1, "owner_7") )
    {
        _o.ParseColorString(World::COLOR_OWNER_7, p2);
    }
    else if ( !StriCmp(p1, "map_direction") )
    {
        _o.ParseColorString(World::COLOR_MAP_DIRECTION, p2);
    }
    else if ( !StriCmp(p1, "map_primtarget") )
    {
        _o.ParseColorString(World::COLOR_MAP_PRIMTARGET, p2);
    }
    else if ( !StriCmp(p1, "map_sectarget") )
    {
        _o.ParseColorString(World::COLOR_MAP_SECTARGET, p2);
    }
    else if ( !StriCmp(p1, "map_commander") )
    {
        _o.ParseColorString(World::COLOR_MAP_COMMANDER, p2);
    }
    else if ( !StriCmp(p1, "map_dragbox") )
    {
        _o.ParseColorString(World::COLOR_MAP_DRAGBOX, p2);
    }
    else if ( !StriCmp(p1, "map_viewer") )
    {
        _o.ParseColorString(World::COLOR_MAP_VIEWER, p2);
    }
    else if ( !StriCmp(p1, "hud_weapon") )
    {
        _o.ParseColorString(World::COLOR_HUD_WEAPON_0, p2);
    }
    else if ( !StriCmp(p1, "hud_weapon_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_WEAPON_1, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_commandvec") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_CMDVEC_0, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_commandvec_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_CMDVEC_1, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_primtarget") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_PRIMTGT_0, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_primtarget_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_PRIMTGT_1, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_locktarget") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_LOCKTGT_0, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_locktarget_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_LOCKTGT_1, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_compass") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_0, p2);
    }
    else if ( !StriCmp(p1, "hud_compass_compass_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_COMPASS_1, p2);
    }
    else if ( !StriCmp(p1, "hud_vehicle") )
    {
        _o.ParseColorString(World::COLOR_HUD_VEHICLE_0, p2);
    }
    else if ( !StriCmp(p1, "hud_vehicle_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_VEHICLE_1, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_mg") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_MG_0, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_mg_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_MG_1, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_locked") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_LOCKED_0, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_locked_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_LOCKED_1, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_autonom") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_AUTONOM_0, p2);
    }
    else if ( !StriCmp(p1, "hud_visor_autonom_1") )
    {
        _o.ParseColorString(World::COLOR_HUD_VISOR_AUTONOM_1, p2);
    }
    else if ( !StriCmp(p1, "brief_lines") )
    {
        _o.ParseColorString(World::COLOR_BRIEF_LINES, p2);
    }
    else if ( !StriCmp(p1, "text_default") )
    {
        _o.ParseColorString(World::COLOR_TEXT_DEFAULT, p2);
    }
    else if ( !StriCmp(p1, "text_list") )
    {
        _o.ParseColorString(World::COLOR_TEXT_LIST, p2);
    }
    else if ( !StriCmp(p1, "text_list_sel") )
    {
        _o.ParseColorString(World::COLOR_TEXT_LIST_SEL, p2);
    }
    else if ( !StriCmp(p1, "text_tooltip") )
    {
        _o.ParseColorString(World::COLOR_TEXT_TOOLTIP, p2);
    }
    else if ( !StriCmp(p1, "text_message") )
    {
        _o.ParseColorString(World::COLOR_TEXT_MESSAGE, p2);
    }
    else if ( !StriCmp(p1, "text_hud") )
    {
        _o.ParseColorString(World::COLOR_TEXT_HUD, p2);
    }
    else if ( !StriCmp(p1, "text_briefing") )
    {
        _o.ParseColorString(World::COLOR_TEXT_BRIEFING, p2);
    }
    else if ( !StriCmp(p1, "text_debriefing") )
    {
        _o.ParseColorString(World::COLOR_TEXT_DEBRIEFING, p2);
    }
    else if ( !StriCmp(p1, "text_button") )
    {
        _o.ParseColorString(World::COLOR_TEXT_BUTTON, p2);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool MiscParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_misc") )
        return false;

    _o._beamEnergyStart = 500;
    _o._beamEnergyAdd = 100;
    _o._defaultUnitLimit = 512;
    _o._defaultUnitLimitType = 0;
    _o._defaultUnitLimitArg = 0;
    _o._easyCheatKeys = false;

    return true;
}

bool AtmosphericFXProfileParser::IsScope(ScriptParser::Parser &parser,
                                         const std::string &word,
                                         const std::string &opt)
{
    if ( StriCmp(word, "new_cloud") )
        return false;

    _active = false;
    _profileID = -1;

    if ( opt.empty() )
    {
        ypa_log_out("WARNING: new_cloud requires a numeric id; profile ignored.\n");
        return true;
    }

    const long value = parser.stol(opt, nullptr, 0);
    if ( value < 0 || value > std::numeric_limits<int32_t>::max() )
    {
        ypa_log_out("WARNING: new_cloud id '%s' is invalid; profile ignored.\n", opt.c_str());
        return true;
    }

    _profileID = (int32_t)value;
    _work = TAtmosphericFXProfile();
    _work.profile_id = _profileID;
    _active = true;
    return true;
}

// Checks the finished block with the same rules the old single-profile files had, so only
// usable clouds end up in the table.
void AtmosphericFXProfileParser::Finish()
{
    TAtmosphericFXProfile &profile = _work;

    if ( profile.mesh3ds.empty() || profile.count <= 0 )
    {
        ypa_log_out("WARNING: new_cloud %d is missing mesh_3ds or has count <= 0; profile ignored.\n", _profileID);
        return;
    }

    if ( profile.count > 512 )
    {
        ypa_log_out("WARNING: new_cloud %d requests count=%d; clamping to 512.\n", _profileID, profile.count);
        profile.count = 512;
    }

    if ( profile.lifetime_min <= 0 && profile.lifetime_max <= 0 )
        profile.lifetime_min = profile.lifetime_max = 1000;
    else if ( profile.lifetime_min <= 0 )
        profile.lifetime_min = profile.lifetime_max;
    else if ( profile.lifetime_max <= 0 )
        profile.lifetime_max = profile.lifetime_min;

    if ( profile.lifetime_max < profile.lifetime_min )
        std::swap(profile.lifetime_min, profile.lifetime_max);

    const auto finiteOr = [](double value, double fallback) { return std::isfinite(value) ? value : fallback; };
    profile.spawn_offset = vec3d(finiteOr(profile.spawn_offset.x, 0.0), finiteOr(profile.spawn_offset.y, 0.0),
                                 finiteOr(profile.spawn_offset.z, 0.0));
    profile.velocity = vec3d(finiteOr(profile.velocity.x, 0.0), finiteOr(profile.velocity.y, 0.0),
                             finiteOr(profile.velocity.z, 0.0));
    profile.spin = vec3d(finiteOr(profile.spin.x, 0.0), finiteOr(profile.spin.y, 0.0), finiteOr(profile.spin.z, 0.0));
    const auto positiveOr1 = [](double value) { return (std::isfinite(value) && value > 0.0) ? value : 1.0; };
    profile.scale = vec3d(positiveOr1(profile.scale.x), positiveOr1(profile.scale.y), positiveOr1(profile.scale.z));
    profile.tint.Clamp();

    profile.loop_sound_volume = std::max(0, std::min(profile.loop_sound_volume, 127));
    profile.valid = true;

    if ( _profiles.find(_profileID) != _profiles.end() )
        ypa_log_out("WARNING: new_cloud %d is defined more than once; replacing the previous profile.\n", _profileID);
    _profiles[_profileID] = profile;
}

static float ParseAtmosphericFXPositiveScale(ScriptParser::Parser &parser,
                                             const std::string &param,
                                             const std::string &value)
{
    float scale = parser.stof(value, 0);
    if ( std::isfinite(scale) && scale > 0.0f )
        return scale;

    ypa_log_out("WARNING: new_cloud parameter '%s' must be finite and greater than zero; using 1.0\n",
                param.c_str());
    return 1.0f;
}

int AtmosphericFXProfileParser::Handle(ScriptParser::Parser &parser,
                                       const std::string &p1,
                                       const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( _active )
            Finish();
        _active = false;
        _profileID = -1;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !_active )
        return ScriptParser::RESULT_OK;

    TAtmosphericFXProfile &_profile = _work;

    if ( !StriCmp(p1, "name") )
        _profile.name = p2;
    else if ( !StriCmp(p1, "loop_sound") )
        _profile.loop_sound = parser.FullLineValue(); // Whole line: file names may contain spaces.
    else if ( !StriCmp(p1, "loop_sound_volume") )
        _profile.loop_sound_volume = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "mesh_3ds") )
        _profile.mesh3ds = parser.FullLineValue(); // Whole line: file names may contain spaces.
    else if ( !StriCmp(p1, "count") )
        _profile.count = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "spawn_radius_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.spawn_radius.x = NonNegativeFiniteOrZero(parser.stof(axis[0], 0));
            _profile.spawn_radius.y = NonNegativeFiniteOrZero(parser.stof(axis[1], 0));
            _profile.spawn_radius.z = NonNegativeFiniteOrZero(parser.stof(axis[2], 0));
        }
    }
    else if ( !StriCmp(p1, "spawn_offset_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.spawn_offset.x = parser.stof(axis[0], 0);
            _profile.spawn_offset.y = parser.stof(axis[1], 0);
            _profile.spawn_offset.z = parser.stof(axis[2], 0);
        }
    }
    else if ( !StriCmp(p1, "lifetime_minmax") )
    {
        std::string lifetime[2];
        if ( ReadTuple(p1, p2, 2, lifetime) )
        {
            _profile.lifetime_min = NonNegativeFiniteMilliseconds(parser, lifetime[0]);
            _profile.lifetime_max = NonNegativeFiniteMilliseconds(parser, lifetime[1]);
        }
    }
    else if ( !StriCmp(p1, "velocity_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.velocity.x = parser.stof(axis[0], 0);
            _profile.velocity.y = parser.stof(axis[1], 0);
            _profile.velocity.z = parser.stof(axis[2], 0);
        }
    }
    else if ( !StriCmp(p1, "velocity_random_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.velocity_random.x = NonNegativeFiniteOrZero(parser.stof(axis[0], 0));
            _profile.velocity_random.y = NonNegativeFiniteOrZero(parser.stof(axis[1], 0));
            _profile.velocity_random.z = NonNegativeFiniteOrZero(parser.stof(axis[2], 0));
        }
    }
    else if ( !StriCmp(p1, "scale_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.scale.x = ParseAtmosphericFXPositiveScale(parser, p1, axis[0]);
            _profile.scale.y = ParseAtmosphericFXPositiveScale(parser, p1, axis[1]);
            _profile.scale.z = ParseAtmosphericFXPositiveScale(parser, p1, axis[2]);
        }
    }
    else if ( !StriCmp(p1, "spin_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile.spin.x = parser.stof(axis[0], 0);
            _profile.spin.y = parser.stof(axis[1], 0);
            _profile.spin.z = parser.stof(axis[2], 0);
        }
    }
    else if ( !StriCmp(p1, "fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            _profile.fade_in = NonNegativeFiniteMilliseconds(parser, fade[0]);
            _profile.fade_out = NonNegativeFiniteMilliseconds(parser, fade[1]);
        }
    }
    else if ( !StriCmp(p1, "tint") )
        ParseTintParam(parser, "tint", p1, p2, _profile.tint, true);
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

int MiscParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }
    else if ( !StriCmp(p1, "one_game_res") )
    {
        _o._oneGameRes = StrGetBool(p2);
    }
    else if ( !StriCmp(p1, "shell_default_res") )
    {
		Stok stok(p2, "_ \t");
		std::string pp1, pp2;
        if ( stok.GetNext(&pp1) && stok.GetNext(&pp2) )
        {
            _o._shellDefaultRes = Common::Point(parser.stol(pp1, nullptr, 0), parser.stol(pp2, nullptr, 0));
            _o._shellGfxMode = Common::Point(parser.stol(pp1, nullptr, 0), parser.stol(pp2, nullptr, 0));
        }
    }
    else if ( !StriCmp(p1, "game_default_res") )
    {
		Stok stok(p2, "_ \t");
        std::string pp1, pp2;
        if ( stok.GetNext(&pp1) && stok.GetNext(&pp2) )
        {
            _o._gameDefaultRes = Common::Point(parser.stol(pp1, nullptr, 0), parser.stol(pp2, nullptr, 0));
            _o._gfxMode = Common::Point(parser.stol(pp1, nullptr, 0), parser.stol(pp2, nullptr, 0));
        }
    }
    else if ( !StriCmp(p1, "max_impulse") )
    {
        _o._maxImpulse = parser.stof(p2);
    }
    else if ( !StriCmp(p1, "unit_limit") )
    {
        _o._defaultUnitLimit = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "unit_limit_type") )
    {
        _o._defaultUnitLimitType = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "unit_limit_arg") )
    {
        _o._defaultUnitLimitArg = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "beam_energy_start") )
    {
        _o._beamEnergyStart = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "beam_energy_add") )
    {
        _o._beamEnergyAdd = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "easy_cheat_keys") )
    {
        _o._easyCheatKeys = parser.stol(p2, nullptr, 0) != 0;
    }
    else if ( !StriCmp(p1, "multi_building") )
    {
        _o._allowMultiBuildWorld = StrGetBool(p2);
    }
    else if ( !StriCmp(p1, "hidden_fractions") )
    {
        _o._worldHiddenFractions = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool SuperItemParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_superitem") )
        return false;

    _o._stoudsonWaveVehicleId = 0;
    _o._stoudsonCenterVehicleId = 0;
    return true;
}

int SuperItemParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }
	else if ( !StriCmp(p1, "superbomb_center_vproto") )
    {
        _o._stoudsonCenterVehicleId = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "superbomb_wall_vproto") )
    {
        _o._stoudsonWaveVehicleId = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;
    return ScriptParser::RESULT_OK;
}

bool SuperItemProfileParser::IsScope(ScriptParser::Parser &parser,
                                     const std::string &word,
                                     const std::string &opt)
{
    if ( StriCmp(word, "new_superitem") )
        return false;

    _profile = nullptr;
    _discard = TSuperItemProfile();

    if ( opt.empty() )
    {
        ypa_log_out("WARNING: new_superitem requires a numeric id; profile ignored.\n");
        _profile = &_discard;
        return true;
    }

    const long value = parser.stol(opt, nullptr, 0);
    if ( value < 0 || value > std::numeric_limits<int32_t>::max() )
    {
        ypa_log_out("WARNING: new_superitem id '%s' is invalid; profile ignored.\n", opt.c_str());
        _profile = &_discard;
        return true;
    }

    if ( _profiles.find((int32_t)value) != _profiles.end() )
        ypa_log_out("WARNING: new_superitem %d is defined more than once; replacing the previous profile.\n", (int)value);

    TSuperItemProfile profile;
    profile.profile_id = (int32_t)value;
    _profiles[(int32_t)value] = profile;
    _profile = &_profiles[(int32_t)value];
    return true;
}

static float ParseSuperItemScale(ScriptParser::Parser &parser,
                                 const std::string &param,
                                 const std::string &value)
{
    float scale = parser.stof(value, 0);
    if ( std::isfinite(scale) && scale > 0.0f )
        return scale;

    ypa_log_out("WARNING: SuperItem profile %s must be finite and greater than zero; using 1.0\n",
                param.c_str());
    return 1.0f;
}

static bool ParseSuperItemSoundEventParam(ScriptParser::Parser &parser,
                                          const std::string &eventName,
                                          const std::string &p1,
                                          const std::string &p2,
                                          TVhclSound &sound)
{
    const std::string sndPrefix = "snd_" + eventName + "_";
    const std::string palPrefix = "pal_" + eventName + "_";
    const std::string shkPrefix = "shk_" + eventName + "_";

    if ( !StriCmp(p1, sndPrefix + "sample") )
        sound.MainSample.Name = p2;
    else if ( !StriCmp(p1, sndPrefix + "volume") )
        sound.volume = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, sndPrefix + "pitch") )
        ParseSoundPitchRange(p2, sound);
    else if ( !StriCmp(p1, sndPrefix + "radius") )
        sound.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, palPrefix + "slot") )
        sound.sndPrm.slot = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, palPrefix + "time") )
        sound.sndPrm.time = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, palPrefix + "radius") )
        sound.sndPrm.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, palPrefix + "mag0") )
        sound.sndPrm.mag0 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, palPrefix + "mag1") )
        sound.sndPrm.mag1 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "slot") )
        sound.sndPrm_shk.slot = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, shkPrefix + "time") )
        sound.sndPrm_shk.time = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, shkPrefix + "radius") )
        sound.sndPrm_shk.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, shkPrefix + "mag0") )
        sound.sndPrm_shk.mag0 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "mag1") )
        sound.sndPrm_shk.mag1 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "mute") )
        sound.sndPrm_shk.mute = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "x") )
        sound.sndPrm_shk.pos.x = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "y") )
        sound.sndPrm_shk.pos.y = parser.stof(p2, 0);
    else if ( !StriCmp(p1, shkPrefix + "z") )
        sound.sndPrm_shk.pos.z = parser.stof(p2, 0);
    else
        return false;

    return true;
}

int SuperItemProfileParser::Handle(ScriptParser::Parser &parser,
                                   const std::string &p1,
                                   const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        _profile = nullptr;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !_profile )
        return ScriptParser::RESULT_OK;

    if ( !StriCmp(p1, "begin_fx") )
        return ParseSuperItemChainFXBlock(parser, _profile);

    if ( !StriCmp(p1, "name") )
        _profile->name = p2;
    else if ( !StriCmp(p1, "wave_vp") )
        _profile->wave_vp = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "wave_3ds") )
        _profile->wave_3ds = p2;
    else if ( !StriCmp(p1, "wave_base") )
        _profile->wave_base = p2;
    else if ( !StriCmp(p1, "fallout_cloud_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _profile->fallout_cloud_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "wave_scale_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            _profile->wave_axis_scale.x = ParseSuperItemScale(parser, p1, axis[0]);
            _profile->wave_axis_scale.y = ParseSuperItemScale(parser, p1, axis[1]);
            _profile->wave_axis_scale.z = ParseSuperItemScale(parser, p1, axis[2]);
        }
    }
    else if ( !StriCmp(p1, "wave_tint") )
        ParseTintParam(parser, "wave_tint", p1, p2, _profile->wave_tint);
    else if ( !StriCmp(p1, "wave_start_speed") )
    {
        _profile->wave_start_speed = parser.stof(p2, 0);
        _profile->has_wave_start_speed = true;
    }
    else if ( !StriCmp(p1, "wave_speed_ramp_time") )
    {
        _profile->wave_speed_ramp_time = parser.stof(p2, 0);
        _profile->has_wave_speed_ramp_time = true;
    }
    else if ( !StriCmp(p1, "wave_end_speed") )
    {
        _profile->wave_end_speed = parser.stof(p2, 0);
        _profile->has_wave_end_speed = true;
    }
    else if ( !StriCmp(p1, "push_force") )
        _profile->push_force = ClampPushIntensity(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "push_radius") )
        _profile->push_radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "push_falloff") )
    {
        const int falloff = parser.stol(p2, nullptr, 0);
        _profile->push_falloff = std::max(0, std::min(falloff, 1));
    }
    else if ( !StriCmp(p1, "wave_push_force") )
        _profile->wave_push_force = ClampPushIntensity(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "wave_push_radius") ||
              !StriCmp(p1, "wave_push_falloff") )
    {
        // Compatibility only: older profiles remain loadable, but the wave
        // contact push now uses wave_push_force alone.
        ypa_log_out("WARNING: SuperItem parameter '%s' is deprecated and ignored; wave_push_force now acts on wave contact.\n",
                    p1.c_str());
    }
    else if ( !StriCmp(p1, "fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            _profile->fade_in = NonNegativeFiniteMilliseconds(parser, fade[0]);
            _profile->fade_out = NonNegativeFiniteMilliseconds(parser, fade[1]);
        }
    }
    else if ( !StriCmp(p1, "wave_unit_energy") )
    {
        int damage = parser.stol(p2, nullptr, 0);
        _profile->wave_unit_energy = damage > 0 ? damage : 0;
    }
    else if ( !StriCmp(p1, "wave_building_total_destruction") )
    {
        TAuthoredScalar value;
        _profile->wave_building_total_destruction = 0;
        if ( ParseAuthoredScalar(p2, value) && value.percent && value.value >= 0.0f )
            _profile->wave_building_total_destruction =
                (int)std::lround(std::min(value.value, 100.0f));
    }
    else if ( !StriCmp(p1, "debuff_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _profile->debuff_id = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
        _profile->debuff = TWeaponDebuffConfig();
    }
    else if ( ParseSuperItemSoundEventParam(parser, "detonate", p1, p2,
                                             _profile->detonate_snd) )
    {}
    else if ( ParseSuperItemSoundEventParam(parser, "wave", p1, p2,
                                             _profile->wave_snd) )
    {}
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

}
}
