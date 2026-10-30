// Script parsers for level (LDF) files: map sizes, robos, squads, gates,
// gems, super items, briefing and map planes.
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

Common::Point MapSizesParser::ParseSizes(ScriptParser::Parser &parser)
{
    std::string tmp;
    parser.ReadLine(&tmp);

    Stok stok(tmp, " \t\r\n");

    std::string sX, sY;
    stok.GetNext(&sX);
    stok.GetNext(&sY);

    int32_t x = parser.stol(sX, nullptr, 0);
    int32_t y = parser.stol(sY, nullptr, 0);

    for(int i = 0; i < y; i++)
        parser.ReadLine(&tmp);

    return Common::Point(x, y);
}

int MapSizesParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( !StriCmp(p1, "typ_map") )
    {
        _m.MapSize = ParseSizes(parser);
    }
    else if ( !StriCmp(p1, "own_map") || !StriCmp(p1, "hgt_map") || !StriCmp(p1, "blg_map") )
    {
        ParseSizes(parser);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelDataParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_level") )
        return false;

    _o._levelInfo.MapName = "<NO NAME>";
    _gotLocalizedTitle = false;
    _o._levelInfo.MovieStr.clear();
    _o._levelInfo.MovieWinStr.clear();
    _o._levelInfo.MovieLoseStr.clear();
    _o._vehicleSectorRatio = 0;
    _o._levelUnitLimit = _o._defaultUnitLimit;
    _o._allowMultiBuildLevel = _o._allowMultiBuildWorld;
    _o._levelUnitLimitType = _o._defaultUnitLimitType;
    _o._levelUnitLimitArg = _o._defaultUnitLimitArg;
    _o._luaScriptName = "";
    _m.CloudId = -1;
    return true;
}

int LevelDataParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( p1.find("title_") != std::string::npos )
    {
        std::string title_lang = std::string("title_") + Locale::Text::GetLocaleName();

        if ( !StriCmp(p1, title_lang) && IsUsableScriptText(p2) )
        {
                _o._levelInfo.MapName = p2;
                _gotLocalizedTitle = true;
        }
        else if ( !StriCmp(p1, "title_default") && !_gotLocalizedTitle && IsUsableScriptText(p2) )
        {
                _o._levelInfo.MapName = p2;
        }
    }
    else if ( !StriCmp(p1, "set") )
    {
        _m.SetID = parser.stol(p2, nullptr, 0);
        _m.ReadedPartsBits |= TLevelDescription::BIT_SET;
    }
    else if ( !StriCmp(p1, "sky") )
    {
        _m.SkyStr = p2;
        _m.ReadedPartsBits |= TLevelDescription::BIT_SKY;
    }
    else if ( !StriCmp(p1, "typ") )
    {
        _m.TypStr = p2;
        _m.ReadedPartsBits |= TLevelDescription::BIT_TYP;
    }
    else if ( !StriCmp(p1, "own") )
    {
        _m.OwnStr = p2;
        _m.ReadedPartsBits |= TLevelDescription::BIT_OWN;
    }
    else if ( !StriCmp(p1, "hgt") )
    {
        _m.HgtStr = p2;
        _m.ReadedPartsBits |= TLevelDescription::BIT_HGT;
    }
    else if ( !StriCmp(p1, "blg") )
    {
        _m.BlgStr = p2;
        _m.ReadedPartsBits |= TLevelDescription::BIT_BLG;
    }
    else if ( !StriCmp(p1, "palette") )
    {
        _m.Palettes[0] = p2;
    }
    else if ( !StriCmp(p1, "slot0") )
    {
        _m.Palettes[0] = p2;
    }
    else if ( !StriCmp(p1, "slot1") )
    {
        _m.Palettes[1] = p2;
    }
    else if ( !StriCmp(p1, "slot2") )
    {
        _m.Palettes[2] = p2;
    }
    else if ( !StriCmp(p1, "slot3") )
    {
        _m.Palettes[3] = p2;
    }
    else if ( !StriCmp(p1, "slot4") )
    {
        _m.Palettes[4] = p2;
    }
    else if ( !StriCmp(p1, "slot5") )
    {
        _m.Palettes[5] = p2;
    }
    else if ( !StriCmp(p1, "slot6") )
    {
        _m.Palettes[6] = p2;
    }
    else if ( !StriCmp(p1, "slot7") )
    {
        _m.Palettes[7] = p2;
    }
    else if ( !StriCmp(p1, "script") )
    {
        if ( !_o.LoadProtosScript(p2) )
            return ScriptParser::RESULT_BAD_DATA;
        return ScriptParser::RESULT_OK;
    }
    else if ( !StriCmp(p1, "ambiencetrack") )
    {
        _o._levelInfo.MusicTrackMinDelay = 0;
        _o._levelInfo.MusicTrackMaxDelay = 0;

        Stok stok(p2, " \t_\n");
        std::string tmp;
        stok.GetNext(&tmp);
        _o._levelInfo.MusicTrack = parser.stol(tmp, nullptr, 0);

        if ( stok.GetNext(&tmp) )
        {
            _o._levelInfo.MusicTrackMinDelay = parser.stol(tmp, nullptr, 0);

            if ( stok.GetNext(&tmp) )
                _o._levelInfo.MusicTrackMaxDelay = parser.stol(tmp, nullptr, 0);
        }
    }
    else if ( !StriCmp(p1, "ambient_sound") )
    {
        _m.AmbientSoundStr = p2;
    }
    else if ( !StriCmp(p1, "cloud_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _m.CloudId = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else if ( !StriCmp(p1, "movie") )
    {
        _o._levelInfo.MovieStr = p2;
    }
    else if ( !StriCmp(p1, "win_movie") )
    {
        _o._levelInfo.MovieWinStr = p2;
    }
    else if ( !StriCmp(p1, "lose_movie") )
    {
        _o._levelInfo.MovieLoseStr = p2;
    }
    else if ( !StriCmp(p1, "event_loop") )
    {
        _m.EventLoopID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "slow_connection") )
    {
        if ( StrGetBool(p2) )
        {
            _m.SlowConnection = true;
        }
        else
        {
            _m.SlowConnection = false;
        }
    }
    else if ( !StriCmp(p1, "vehicle_sector_ratio") )
    {
        _o._vehicleSectorRatio = parser.stof(p2, 0);
    }
    else if ( !StriCmp(p1, "unit_limit") )
    {
        _o._levelUnitLimit = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "unit_limit_type") )
    {
        _o._levelUnitLimitType = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "unit_limit_arg") )
    {
        _o._levelUnitLimitArg = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "lua_script") )
    {
        _o._luaScriptName = p2;
    }
    else if ( !StriCmp(p1, "multi_building") )
    {
        _o._allowMultiBuildLevel = StrGetBool(p2);
    }
    else if ( !StriCmp(p1, "hidden_fractions") )
    {
        _o._hiddenFractions = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool MapRobosParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (StriCmp(word, "begin_robo"))
        return false;

    _m.Robos.emplace_back(); // Construct new element
    _r = &_m.Robos.back();
    _r->MbStatus = 0;
    return true;
}

int MapRobosParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if (_m.Robos.size() == 1) //If it's first host station - save owner for brief
            _m.PlayerOwner = _r->Owner;

        _m.ReadedPartsBits |= TLevelDescription::BIT_END;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "owner") )
    {
        _r->Owner = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "vehicle") )
    {
        _r->VhclID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "sec_x") )
    {
        int tmp = parser.stol(p2, nullptr, 0);
        _r->Pos.y = -300;
        _r->Pos.x = tmp * CVSectorLength + CVSectorHalfLength;
    }
    else if ( !StriCmp(p1, "sec_y") )
    {
        int tmp = parser.stol(p2, nullptr, 0);
        _r->Pos.y = -300;
        _r->Pos.z = -(tmp * CVSectorLength + CVSectorHalfLength);
    }
    else if ( !StriCmp(p1, "pos_x") )
    {
        _r->Pos.x = parser.stof(p2, 0) + 0.3;
    }
    else if ( !StriCmp(p1, "pos_y") )
    {
        _r->Pos.y = parser.stof(p2, 0) + 0.3;
    }
    else if ( !StriCmp(p1, "pos_z") )
    {
        _r->Pos.z = parser.stof(p2, 0) + 0.3;
    }
    else if ( !StriCmp(p1, "energy") )
    {
        _r->Energy = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "con_budget") )
    {
        _r->ConBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rad_budget") )
    {
        _r->RadBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "pow_budget") )
    {
        _r->PowBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "def_budget") )
    {
        _r->DefBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "saf_budget") )
    {
        _r->SafBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rec_budget") )
    {
        _r->RecBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "cpl_budget") )
    {
        _r->CplBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rob_budget") )
    {
        _r->RobBudget = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "viewangle") )
    {
        _r->ViewAngle = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "saf_delay") )
    {
        _r->SafDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "pow_delay") )
    {
        _r->PowDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rad_delay") )
    {
        _r->RadDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "cpl_delay") )
    {
        _r->CplDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "def_delay") )
    {
        _r->DefDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "con_delay") )
    {
        _r->ConDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rec_delay") )
    {
        _r->RecDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "rob_delay") )
    {
        _r->RobDelay = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mb_status") )
    {
        if ( !StriCmp(p2, "known") )
        {
            _r->MbStatus = 0;
        }
        else if ( !StriCmp(p2, "unknown") )
        {
            _r->MbStatus = 1;
        }
        else if ( !StriCmp(p2, "hidden") )
        {
            _r->MbStatus = 2;
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "reload_const") )
    {
        _r->ReloadConst = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

int ShellSoundParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    struct ShellSoundNames
    {
        const std::string name;
        const int id;
    };

    static const ShellSoundNames block1[] =
    {
        {"volume", World::SOUND_ID_VOLUME},
        {"right", World::SOUND_ID_RIGHT},
        {"left", World::SOUND_ID_LEFT},
        {"button", World::SOUND_ID_BUTTON},
        {"quit", World::SOUND_ID_QUIT},
        {"slider", World::SOUND_ID_SLIDER},
        {"welcome", World::SOUND_ID_WELCOME},
        {"menuopen", World::SOUND_ID_MENUOPEN},
        {"overlevel", World::SOUND_ID_OVERLEVEL},
        {"levelselect", World::SOUND_ID_LEVELSEL},
        {"textappear", World::SOUND_ID_TEXTAPPEAR},
        {"objectappear", World::SOUND_ID_OBJAPPEAR},
        {"sectorconquered", World::SOUND_ID_SECTCONQ},
        {"vhcldestroyed", World::SOUND_ID_VHCLDESTR},
        {"bldgconquered", World::SOUND_ID_BLDGCONQ},
        {"timercount", World::SOUND_ID_TIMERCOUNT},
        {"select", World::SOUND_ID_SELECT},
        {"error", World::SOUND_ID_ERROR},
        {"attention", World::SOUND_ID_ATTEN},
        {"secret", World::SOUND_ID_SECRET},
        {"plasma", World::SOUND_ID_PLASMA}
    };

    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }
    else
    {
        std::string sm, tp;
        Stok stok(p1, "_");

        if (stok.GetNext(&sm) && stok.GetNext(&tp))
        {
            for (auto &t: block1)
            {
                if ( !StriCmp(t.name, sm) )
                {
                    if (!StriCmp("sample", tp))
                        return ( _o.LoadSample(t.id, p2) ? ScriptParser::RESULT_OK : ScriptParser::RESULT_BAD_DATA );
                    else if (!StriCmp("volume", tp))
                        _o.samples1_info.Sounds[t.id].Volume = parser.stoi(p2);
                    else if (!StriCmp("pitch", tp))
                        _o.samples1_info.Sounds[t.id].Pitch = parser.stoi(p2);
                    else
                        return ScriptParser::RESULT_UNKNOWN;

                    return ScriptParser::RESULT_OK;
                }
            }
        }

        return ScriptParser::RESULT_UNKNOWN;
    }

    return ScriptParser::RESULT_OK;
}

int ShellTracksParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    Stok stok(p2, " \t_\n");

    if ( !StriCmp(p1, "shelltrack") )
    {
        _o.shelltrack__adv.min_delay = 0;
        _o.shelltrack__adv.max_delay = 0;

        std::string val;
        stok.GetNext(&val);

        _o.shelltrack = parser.stol(val, nullptr, 0);
        if ( stok.GetNext(&val) )
        {
            _o.shelltrack__adv.min_delay = parser.stol(val, nullptr, 0);

            if ( stok.GetNext(&val) )
                _o.shelltrack__adv.max_delay = parser.stol(val, nullptr, 0);
        }
    }
    else if ( !StriCmp(p1, "missiontrack") )
    {
        _o.missiontrack__adv.min_delay = 0;
        _o.missiontrack__adv.max_delay = 0;

        std::string val;
        stok.GetNext(&val);

        _o.missiontrack = parser.stol(val, nullptr, 0);
        if ( stok.GetNext(&val) )
        {
            _o.missiontrack__adv.min_delay = parser.stol(val, nullptr, 0);

            if ( stok.GetNext(&val) )
                _o.missiontrack__adv.max_delay = parser.stol(val, nullptr, 0);
        }
    }
    else if ( !StriCmp(p1, "debriefingtrack") )
    {
        _o.debriefingtrack__adv.min_delay = 0;
        _o.debriefingtrack__adv.max_delay = 0;

        std::string val;
        stok.GetNext(&val);

        _o.debriefingtrack = parser.stol(val, nullptr, 0);
        if ( stok.GetNext(&val) )
        {
            _o.debriefingtrack__adv.min_delay = parser.stol(val, nullptr, 0);

            if ( stok.GetNext(&val) )
                _o.debriefingtrack__adv.max_delay = parser.stol(val, nullptr, 0);
        }
    }
    else if ( !StriCmp(p1, "loadingtrack") )
    {
        _o.loadingtrack__adv.min_delay = 0;
        _o.loadingtrack__adv.max_delay = 0;

        std::string val;
        stok.GetNext(&val);

        _o.loadingtrack = parser.stol(val, nullptr, 0);
        if ( stok.GetNext(&val) )
        {
            _o.loadingtrack__adv.min_delay = parser.stol(val, nullptr, 0);

            if ( stok.GetNext(&val) )
                _o.loadingtrack__adv.max_delay = parser.stol(val, nullptr, 0);
        }
    }
    else
        return ScriptParser::RESULT_UNKNOWN;
    return ScriptParser::RESULT_OK;
}

bool LevelSquadParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if (StriCmp(word, "begin_squad"))
        return false;

    _m.Squads.emplace_back();
    _s = &_m.Squads.back();
    return true;
}

int LevelSquadParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( !_s->VhclID )
        {
            ypa_log_out("Squad init: squad[%d]аno vehicle defined!\n", _m.Squads.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_s->Count )
        {
            ypa_log_out("Squad init: squad[%d] num of vehicles is 0!\n", _m.Squads.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( _s->X == 0.0 || _s->Z == 0.0 )
        {
            ypa_log_out("Squad init: squad[%d] no pos given!\n", _m.Squads.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "owner") )
    {
        _s->Owner = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "useable") )
    {
        _s->Useable = true;
    }
    else if ( !StriCmp(p1, "vehicle") )
    {
        _s->VhclID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "num") )
    {
        _s->Count = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "pos_x") )
    {
        _s->X = parser.stod(p2, 0) + 0.3;
    }
    else if ( !StriCmp(p1, "pos_z") )
    {
        _s->Z = parser.stod(p2, 0) + 0.3;
    }
    else if ( !StriCmp(p1, "mb_status") )
    {
        if ( !StriCmp(p2, "known") )
        {
            _s->MbStatus = 0;
        }
        else if ( !StriCmp(p2, "unknown") )
        {
            _s->MbStatus = 1;
        }
        else if ( !StriCmp(p2, "hidden") )
        {
            _s->MbStatus = 2;
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelLocationParser::IsScope(ScriptParser::Parser &, const std::string &word, const std::string &opt)
{
    if (StriCmp(word, "begin_location")) return false;
    if (_replace) { _values.clear(); _replace=false; }
    _value=World::Location();
    _valid=true;
    try {
        size_t used=0;
        _value.id=std::stoi(opt, &used);
        _valid=used==opt.size();
    } catch (...) { _valid=false; }
    return true;
}

int LevelLocationParser::Handle(ScriptParser::Parser &parser, const std::string &key, const std::string &text)
{
    if (!StriCmp(key,"end")) {
        bool duplicate=false;
        for (const auto &v:_values) if (v.id==_value.id) duplicate=true;
        if (_valid && !duplicate && _values.size()<256 && World::ValidLocation(_value)) {
            World::PrepareLocationBounds(_value);
            _values.push_back(_value);
        } else
            ypa_log_out("WARNING: ignored invalid or duplicate location %d.\n",_value.id);
        return ScriptParser::RESULT_SCOPE_END;
    }
    try {
        if (!StriCmp(key,"name")) _value.name=parser.FullLineValue();
        else if (!StriCmp(key,"color")) {
            _value.color=World::NormalizeLocationColor(parser.FullLineValue());
        } else if (!StriCmp(key,"sound")) {
            const std::string path=parser.FullLineValue();
            _value.sound.clear();
            if (!path.empty() && !uaNormalizeDataAssetPath(path,&_value.sound,false)) {
                _value.sound.clear();
                ypa_log_out("WARNING: ignored invalid location sound path.\n");
            }
        } else if (!StriCmp(key,"point")) {
            const size_t split=text.find('_');
            if (split==std::string::npos || _value.points.size()>=256) _valid=false;
            else {
                size_t usedX=0, usedZ=0;
                const double x=std::stod(text.substr(0,split),&usedX);
                const double z=std::stod(text.substr(split+1),&usedZ);
                if (usedX!=split || usedZ!=text.size()-split-1) _valid=false;
                else _value.points.push_back({x,z});
            }
        } else {
            size_t used=0;
            const int number=std::stoi(text,&used);
            if (used!=text.size()) _valid=false;
            if (!StriCmp(key,"fade_in_ms")) _value.fadeIn=number;
            else if (!StriCmp(key,"hold_ms")) _value.hold=number;
            else if (!StriCmp(key,"fade_out_ms")) _value.fadeOut=number;
            else _valid=false;
        }
    } catch (...) { _valid=false; }
    return ScriptParser::RESULT_OK;
}

bool LevelGatesParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_gate") )
        return false;

    _o._levelInfo.Gates.emplace_back();
    _g = &_o._levelInfo.Gates.back();
    _g->MbStatus = 0;
    return true;
}

int LevelGatesParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( !_g->ClosedBldID )
        {
            ypa_log_out("Gate init: gate[%d] no closed building defined!\n", _o._levelInfo.Gates.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_g->OpenBldID )
        {
            ypa_log_out("Gate init: gate[%d] no opened building defined!\n", _o._levelInfo.Gates.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( _g->CellId.x == 0 || _g->CellId.y == 0)
        {
            ypa_log_out("Gate init: gate[%d] no sector coords!\n", _o._levelInfo.Gates.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_g->PassToLevels.size() )
        {
            ypa_log_out("Gate init: gate[%d] no target levels defined!\n", _o._levelInfo.Gates.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "sec_x") )
    {
        _g->CellId.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "sec_y") )
    {
        _g->CellId.y = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "closed_bp") )
    {
        _g->ClosedBldID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "opened_bp") )
    {
        _g->OpenBldID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "target_level") )
    {
        _g->PassToLevels.push_back( parser.stol(p2, nullptr, 0) );
    }
    else if ( !StriCmp(p1, "keysec_x") )
    {
        _g->KeySectors.emplace_back();
        _g->KeySectors.back().CellId.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "keysec_y") )
    {
        _g->KeySectors.back().CellId.y = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mb_status") )
    {
        if ( !StriCmp(p2, "known") )
        {
            _g->MbStatus = 0;
        }
        else if ( !StriCmp(p2, "unknown") )
        {
            _g->MbStatus = 1;
        }
        else if ( !StriCmp(p2, "hidden") )
        {
            _g->MbStatus = 2;
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelBriefingTextParser::IsScope(ScriptParser::Parser &, const std::string &word, const std::string &)
{
    if (StriCmp(word, "begin_briefing_text")) return false;
    _m.BriefingText.clear();
    _first = true;
    return true;
}

int LevelBriefingTextParser::Handle(ScriptParser::Parser &, const std::string &raw, const std::string &)
{
    std::string line = raw;
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    const auto first = line.find_first_not_of(" \t");
    const auto last = line.find_last_not_of(" \t");
    if (first != std::string::npos && !StriCmp(line.substr(first, last-first+1), "end"))
    {
        if (_m.BriefingText.find_first_not_of(" \t\r\n") == std::string::npos)
            _m.BriefingText.clear();
        return ScriptParser::RESULT_SCOPE_END;
    }
    // Escape a literal end line or leading backslash with one leading backslash.
    if (!line.empty() && line.front() == '\\') line.erase(0, 1);
    if (!_first) _m.BriefingText += '\n';
    _first = false;
    _m.BriefingText += line;
    return ScriptParser::RESULT_OK;
}

bool LevelMbMapParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_mbmap") )
        return false;

    _m.Mbmaps.emplace_back();
    _d = &_m.Mbmaps.back();
    return true;
}

int LevelMbMapParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "name") )
    {
        _d->PicName = p2;
    }
    else if ( !StriCmp(p1, "size_x") )
    {
        _d->Size.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "size_y") )
    {
        _d->Size.y = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelGemParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_gem") )
        return false;

    _o._techUpgrades.emplace_back();
    _g = &_o._techUpgrades.back();
    _g->MbStatus = 0;
    return true;
}

static bool SetGemMbStatus(TMapGem *gem, const std::string &value)
{
    if ( !StriCmp(value, "known") )
        gem->MbStatus = 0;
    else if ( !StriCmp(value, "unknown") )
        gem->MbStatus = 1;
    else if ( !StriCmp(value, "hidden") )
        gem->MbStatus = 2;
    else
        return false;

    return true;
}

int LevelGemParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( !_g->BuildingID )
        {
            ypa_log_out("WStein init: gem[%d] no building defined!\n", _o._techUpgrades.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( _g->CellId.x == 0 || _g->CellId.y == 0 )
        {
            ypa_log_out("WStein init: gem[%d] sector pos wonky tonk!\n", _o._techUpgrades.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( p1.find("msg_") != std::string::npos )
    {
        std::string tmp = fmt::sprintf("msg_%s", Locale::Text::GetLocaleName());

        if ( !StriCmp(p1, "msg_default") || !StriCmp(p1, tmp) )
            _g->MsgDefault = p2;
    }
    else if ( !StriCmp(p1, "sec_x") )
    {
        _g->CellId.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "sec_y") )
    {
        _g->CellId.y = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "building") )
    {
        _g->BuildingID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "type") )
    {
        _g->Type = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "script") )
    {
        _g->ScriptFile = p2;

        FSMgr::FileHandle *tmp = uaOpenFileAlloc(_g->ScriptFile, "r");

        if ( !tmp )
            return ScriptParser::RESULT_BAD_DATA;

        delete tmp;
    }
    else if ( !StriCmp(p1, "mb_status") )
    {
        if ( !SetGemMbStatus(_g, p2) )
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "nw_vproto_num") )
    {
        Stok stok(p2, "_ \t");
        std::string tmp;
        if ( stok.GetNext(&tmp) )
        {
            _g->NwVprotoNum1 = parser.stol(tmp, nullptr, 0);
            if ( stok.GetNext(&tmp) )
            {
                _g->NwVprotoNum2 = parser.stol(tmp, nullptr, 0);
                if ( stok.GetNext(&tmp) )
                {
                    _g->NwVprotoNum3 = parser.stol(tmp, nullptr, 0);
                    if ( stok.GetNext(&tmp) )
                        _g->NwVprotoNum4 = parser.stol(tmp, nullptr, 0);
                }
            }
        }
    }
    else if ( !StriCmp(p1, "nw_bproto_num") )
    {
        Stok stok(p2, "_ \t");
        std::string tmp;
        if ( stok.GetNext(&tmp) )
        {
            _g->NwBprotoNum1 = parser.stol(tmp, nullptr, 0);
            if ( stok.GetNext(&tmp) )
            {
                _g->NwBprotoNum2 = parser.stol(tmp, nullptr, 0);
                if ( stok.GetNext(&tmp) )
                {
                    _g->NwBprotoNum3 = parser.stol(tmp, nullptr, 0);
                    if ( stok.GetNext(&tmp) )
                        _g->NwBprotoNum4 = parser.stol(tmp, nullptr, 0);
                }
            }
        }
    }
    else if ( !StriCmp(p1, "begin_action") )
    {
        std::string tmp;

        while( parser.ReadLine(&tmp) && (tmp.find("end_action") == std::string::npos) )
        {
            std::string actionLine = tmp;
            size_t commentPos = actionLine.find_first_of(";\n\r");
            if ( commentPos != std::string::npos )
                actionLine.erase(commentPos);

            Stok stok(actionLine, "= \t");
            std::string actionKey;
            std::string actionValue;
            if ( stok.GetNext(&actionKey) && !StriCmp(actionKey, "mb_status") )
            {
                if ( !stok.GetNext(&actionValue) || !SetGemMbStatus(_g, actionValue) )
                    return ScriptParser::RESULT_BAD_DATA;

                continue;
            }

            _g->ActionsList.push_back(tmp);
        }
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelEnableParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_enable") )
        return false;

    _fraction = parser.stol(opt, nullptr, 0);

    for (TVhclProto &vhcl : _o._vhclProtos)
        vhcl.disable_enable_bitmask &= ~(1 << _fraction);

    for (TBuildingProto &bld : _o._buildProtos)
        bld.EnableMask &= ~(1 << _fraction);

    return true;
}

int LevelEnableParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( !StriCmp(p1, "vehicle") )
    {
        size_t id = parser.stol(p2, nullptr, 0);
        if ( id >= 0 && id < _o._vhclProtos.size() ) //_o.ypaworld.VhclProtos.size() )
            _o._vhclProtos[id].disable_enable_bitmask |= (1 << _fraction);
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "building") )
    {
        size_t id = parser.stol(p2, nullptr, 0);
        if ( id >= 0 && id < _o._buildProtos.size() )
            _o._buildProtos[id].EnableMask |= (1 << _fraction);
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelDebMapParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_dbmap") )
        return false;

    _m.Dbmaps.emplace_back();
    _d = &_m.Dbmaps.back();
    return true;
}

int LevelDebMapParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "name") )
    {
        _d->PicName = p2;
    }
    else if ( !StriCmp(p1, "size_x") )
    {
        _d->Size.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "size_y") )
    {
        _d->Size.y = parser.stol(p2, nullptr, 0);
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

bool LevelSuperItemsParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_item") )
        return false;

    _o._levelInfo.SuperItems.emplace_back();

    _s = &_o._levelInfo.SuperItems.back();
    _s->Type = 0;
    _s->TimerValue = 60000; //1hour
    _s->State = TMapSuperItem::STATE_INACTIVE;
    _s->MbStatus = 0;
    return true;
}

int LevelSuperItemsParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        if ( _s->CellId.x == 0 || _s->CellId.y == 0)
        {
            ypa_log_out("Super item #%d: invalid sector coordinates!\n", _o._levelInfo.SuperItems.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_s->InactiveBldID )
        {
            ypa_log_out("Super item #%d: no <inactive_bp> defined!\n", _o._levelInfo.SuperItems.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_s->ActiveBldID )
        {
            ypa_log_out("Super item #%d: no <active_bp> defined!\n", _o._levelInfo.SuperItems.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( !_s->TriggerBldID )
        {
            ypa_log_out("Super item #%d: no <trigger_bp> defined!\n", _o._levelInfo.SuperItems.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        if ( _s->Type != TMapSuperItem::TYPE_BOMB && _s->Type != TMapSuperItem::TYPE_WAVE )
        {
            ypa_log_out("Super item #%d: no valid <type> defined!\n", _o._levelInfo.SuperItems.size() - 1);
            return ScriptParser::RESULT_BAD_DATA;
        }

        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !StriCmp(p1, "sec_x") )
    {
        _s->CellId.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "sec_y") )
    {
        _s->CellId.y = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "inactive_bp") )
    {
        _s->InactiveBldID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "active_bp") )
    {
        _s->ActiveBldID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "trigger_bp") )
    {
        _s->TriggerBldID = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "keysec_x") )
    {
        _s->KeySectors.emplace_back();
        _s->KeySectors.back().CellId.x = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "keysec_y") )
    {
        _s->KeySectors.back().CellId.y = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "mb_status") )
    {
        if ( !StriCmp(p2, "known") )
        {
            _s->MbStatus = 0;
        }
        else if ( !StriCmp(p2, "unknown") )
        {
            _s->MbStatus = 1;
        }
        else if ( !StriCmp(p2, "hidden") )
        {
            _s->MbStatus = 2;
        }
        else
            return ScriptParser::RESULT_BAD_DATA;
    }
    else if ( !StriCmp(p1, "type") )
    {
        _s->Type = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "countdown") )
    {
        _s->TimerValue = parser.stol(p2, nullptr, 0);
    }
    else if ( !StriCmp(p1, "superitem_id") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        _s->ProfileId = value >= 0 && value <= std::numeric_limits<int32_t>::max()
            ? (int32_t)value
            : -1;
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

Common::PlaneBytes MapAsPlaneBytes::ReadMapAsPlaneBytes(ScriptParser::Parser &parser)
{
    std::string buf;
    parser.ReadLine(&buf);

    std::string tmp;
    Stok stok(buf, " \t\r\n");
    stok.GetNext(&tmp);
    int w = parser.stol(tmp, nullptr, 0);
    stok.GetNext(&tmp);
    int h = parser.stol(tmp, nullptr, 0);

    if (w <= 0 || h <= 0)
        return Common::PlaneBytes();

    Common::PlaneBytes bmp(w, h);

    for (int j = 0; j < h; j++)
    {
        parser.ReadLine(&buf);
        stok = buf;

        uint8_t *ln = bmp.Line(j);

        for (int i = 0; i < w; i++)
        {
            stok.GetNext(&tmp);
            ln[i] = parser.stol(tmp, nullptr, 16);
        }
    }

    return bmp;
}

bool LevelMapsParser::IsScope(ScriptParser::Parser &parser, const std::string &word, const std::string &opt)
{
    if ( StriCmp(word, "begin_maps") )
        return false;

    _o._lvlTypeMap.Clear();
    _o._lvlOwnMap.Clear();
    _o._lvlHeightMap.Clear();
    _o._lvlBuildingsMap.Clear();

    return true;
}

int LevelMapsParser::Handle(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
        return ScriptParser::RESULT_SCOPE_END;

    if ( !StriCmp(p1, "typ_map") )
    {
        _o._lvlTypeMap = ReadMapAsPlaneBytes(parser);

        if ( !_o._lvlTypeMap.IsOk() )
            return ScriptParser::RESULT_BAD_DATA;

        _m.MapSize = _o._lvlTypeMap.Size();

        _m.ReadedPartsBits |= TLevelDescription::BIT_TYP;
    }
    else if ( !StriCmp(p1, "own_map") )
    {
        _o._lvlOwnMap = ReadMapAsPlaneBytes(parser);
        if ( !_o._lvlOwnMap.IsOk() )
            return ScriptParser::RESULT_BAD_DATA;

        _m.ReadedPartsBits |= TLevelDescription::BIT_OWN;
    }
    else if ( !StriCmp(p1, "hgt_map") )
    {
        _o._lvlHeightMap = ReadMapAsPlaneBytes(parser);
        if ( !_o._lvlHeightMap.IsOk() )
            return ScriptParser::RESULT_BAD_DATA;

        _m.ReadedPartsBits |= TLevelDescription::BIT_HGT;
    }
    else if ( !StriCmp(p1, "blg_map") )
    {
        _o._lvlBuildingsMap = ReadMapAsPlaneBytes(parser);
        if ( !_o._lvlBuildingsMap.IsOk() )
            return ScriptParser::RESULT_BAD_DATA;

        _m.ReadedPartsBits |= TLevelDescription::BIT_BLG;
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

}
}
