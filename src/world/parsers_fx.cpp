// Script parsers for sound and visual FX blocks shared by vehicles, weapons
// and super items, plus the Buff/Debuff and ground decal profiles.
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

TVhclSound *VhclProtoParser::GetSndFxByName(const std::string &sndname)
{
    struct SoundType
    {
        const std::string name;
        int id;
    };

    static const SoundType CmpVals[] = {
        {"normal",      0},
        {"fire",        1},
        {"wait",        2},
        {"genesis",     3},
        {"explode",     4},
        {"crashland",   5},
        {"crashvhcl",   6},
        {"goingdown",   7},
        {"cockpit",     8},
        {"beamin",      9},
        {"beamout",    10},
        {"build",      11},
        {"airexplode", 12},
    };

    for (const SoundType &t : CmpVals)
    {
        if ( !StriCmp(sndname, t.name) )
            return &_vhcl->sndFX[t.id];
    }

    return nullptr;
}

bool FxParser::ParseExtSampleDef(ScriptParser::Parser &parser, TVhclSound *sndfx, const std::string &p2)
{
    Stok stok(p2, "_");
    std::string pp1, pp2, pp3, pp4, pp5, pname;

    if ( !stok.GetNext(&pp1) || !stok.GetNext(&pp2) || !stok.GetNext(&pp3) || !stok.GetNext(&pp4) || !stok.GetNext(&pp5) || !stok.GetNext(&pname) )
        return false;

    sndfx->extS.emplace_back();
    sndfx->ExtSamples.emplace_back();

    sndfx->ExtSamples.back().Name = pname;

    TSampleParams &sndEx = sndfx->extS.back();
    sndEx.Sample = nullptr;
    sndEx.Loop = parser.stol(pp1, nullptr, 0);
    sndEx.Vol = parser.stol(pp2, nullptr, 0);
    sndEx.SampleRate = parser.stol(pp3, nullptr, 0);
    sndEx.Offset = parser.stol(pp4, nullptr, 0);
    sndEx.SampleCnt = parser.stol(pp5, nullptr, 0);

    return true;
}

int FxParser::ParseSndFX(ScriptParser::Parser &parser, const std::string &p1, const std::string &p2)
{
    std::string val;
    Stok stok(p1, "_");
    stok.GetNext(&val);

    int sndTP;
    if ( !StriCmp(val, "snd") )
        sndTP = 0;
    else if ( !StriCmp(val, "pal") )
        sndTP = 1;
    else if ( !StriCmp(val, "shk") )
        sndTP = 2;
    else
        return ScriptParser::RESULT_UNKNOWN;

    stok.GetNext(&val);
    const std::string soundEvent = val;

    TVhclSound *sndfx = GetSndFxByName(val);
    if (!sndfx)
        return ScriptParser::RESULT_UNKNOWN;

    stok.GetNext(&val);

    std::string suffix;
    if ( stok.GetNext(&suffix) )
        val += "_" + suffix;

    switch (sndTP)
    {
        case 0:
        {
            if ( !StriCmp(val, "sample") )
                sndfx->MainSample.Name = p2;
            else if ( !StriCmp(val, "volume") )
                sndfx->volume = parser.stol(p2, nullptr, 0);
            else if ( !StriCmp(val, "pitch") )
            {
                if ( !StriCmp(soundEvent, "genesis") && !StriCmp(p2, "auto") )
                {
                    // Automatic Genesis pitch is opt-in. Missing pitch keeps the normal default.
                    sndfx->auto_pitch = true;
                    sndfx->SetPitchRange(0, 0);
                }
                else
                {
                    // An explicit numeric value or range overrides a previous "auto".
                    sndfx->auto_pitch = false;
                    ParseSoundPitchRange(p2, *sndfx);
                }
            }
            else if ( !StriCmp(val, "radius") )
                sndfx->radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
            else if ( !StriCmp(val, "ext") )
            {
                if ( !ParseExtSampleDef(parser, sndfx, p2) )
                    return ScriptParser::RESULT_BAD_DATA;
            }
            else
                return ScriptParser::RESULT_UNKNOWN;
        }
        break;

        case 1:
        {
            if ( !StriCmp(val, "slot") )
                sndfx->sndPrm.slot = parser.stol(p2, nullptr, 0);
            else if ( !StriCmp(val, "mag0") )
                sndfx->sndPrm.mag0 = parser.stof(p2, 0);
            else if ( !StriCmp(val, "mag1") )
                sndfx->sndPrm.mag1 = parser.stof(p2, 0);
            else if ( !StriCmp(val, "time") )
                sndfx->sndPrm.time = parser.stol(p2, nullptr, 0);
            else if ( !StriCmp(val, "radius") )
                sndfx->sndPrm.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
            else
                return ScriptParser::RESULT_UNKNOWN;
        }
        break;

        case 2:
        {
            if ( !StriCmp(val, "slot") )
                sndfx->sndPrm_shk.slot = parser.stol(p2, nullptr, 0);
            else if ( !StriCmp(val, "mag0") )
                sndfx->sndPrm_shk.mag0 = parser.stof(p2, 0);
            else if ( !StriCmp(val, "mag1") )
                sndfx->sndPrm_shk.mag1 = parser.stof(p2, 0);
            else if ( !StriCmp(val, "time") )
                sndfx->sndPrm_shk.time = parser.stol(p2, nullptr, 0);
            else if ( !StriCmp(val, "radius") )
                sndfx->sndPrm_shk.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
            else if ( !StriCmp(val, "mute") )
                sndfx->sndPrm_shk.mute = parser.stof(p2, 0);
            else if ( !StriCmp(val, "x") )
                sndfx->sndPrm_shk.pos.x = parser.stof(p2, 0);
            else if ( !StriCmp(val, "y") )
                sndfx->sndPrm_shk.pos.y = parser.stof(p2, 0);
            else if ( !StriCmp(val, "z") )
                sndfx->sndPrm_shk.pos.z = parser.stof(p2, 0);
            else
                return ScriptParser::RESULT_UNKNOWN;
        }
        break;

        default:
            return ScriptParser::RESULT_UNKNOWN;
    }

    return ScriptParser::RESULT_OK;
}

void ResetVehicleScaleFX(TVhclProto *vhcl)
{
    vhcl->scale_fx_p0 = 0.0;
    vhcl->scale_fx_p1 = 0.0;
    vhcl->scale_fx_p2 = 0.0;
    vhcl->scale_fx_p3 = 0;
    vhcl->scale_fx_pXX.fill(0);
}

static float ParseVPScaleValue(ScriptParser::Parser &parser, const std::string &value)
{
    float scale = parser.stof(value, 0);
    return std::isfinite(scale) && scale > 0.0 ? scale : 1.0;
}

// Splits a compact "a_b_c" value (see World::SplitTupleValue). A wrong shape is
// logged and rejected, so the caller keeps the previous/default values.
bool ReadTuple(const std::string &key,
               const std::string &value,
               size_t count,
               std::string *parts)
{
    if ( World::SplitTupleValue(value, count, parts) )
        return true;

    ypa_log_out("WARNING: invalid %s '%s': expected %u values separated by '_'.\n",
                key.c_str(), value.c_str(), (unsigned)count);
    return false;
}

// <prefix>_scale_xyz = x_y_z
bool ParseVPScaleParam(ScriptParser::Parser &parser,
                       const std::string &prefix,
                       const std::string &p1,
                       const std::string &p2,
                       vec3d &scale)
{
    const std::string keyPrefix = prefix.empty() ? std::string() : prefix + "_";

    if ( StriCmp(p1, keyPrefix + "scale_xyz") )
        return false;

    std::string axis[3];
    if ( ReadTuple(p1, p2, 3, axis) )
    {
        scale.x = ParseVPScaleValue(parser, axis[0]);
        scale.y = ParseVPScaleValue(parser, axis[1]);
        scale.z = ParseVPScaleValue(parser, axis[2]);
    }
    return true;
}

static bool ParseBuffParam(ScriptParser::Parser &parser,
                           const std::string &p1,
                           const std::string &p2,
                           TVehicleBuffConfig &buff)
{
    if ( !StriCmp(p1, "name") )
        buff.name = p2;
    else if ( !StriCmp(p1, "icon") )
        buff.icon = p2;
    else if ( !StriCmp(p1, "invisible") )
        buff.invisible = StrGetBool(p2);
    else if ( !StriCmp(p1, "invisible_reveal_vp") )
    {
        const long vp = parser.stol(p2, nullptr, 0);
        buff.invisible_reveal_vp = vp > 0 && vp <= std::numeric_limits<int16_t>::max()
            ? (int16_t)vp
            : 0;
    }
    else if ( !StriCmp(p1, "invisible_reveal_3ds") )
        buff.invisible_reveal_3ds = p2;
    else if ( !StriCmp(p1, "invisible_reveal_base") )
        buff.invisible_reveal_base = p2;
    else if ( !StriCmp(p1, "invulnerable") )
        buff.invulnerable = StrGetBool(p2);
    else if ( !StriCmp(p1, "deflect_charges") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        buff.deflect_charges = value > 0
            ? (int)std::min<long>(value, std::numeric_limits<int>::max())
            : 0;
    }
    else if ( !StriCmp(p1, "deflect_max_energy") )
    {
        const long value = parser.stol(p2, nullptr, 0);
        buff.deflect_max_energy = value > 0
            ? (int)std::min<long>(value, std::numeric_limits<int>::max())
            : 0;
    }
    else if ( !StriCmp(p1, "deflect_end_energy_reduction") )
    {
        TAuthoredScalar value;
        buff.deflect_end_energy_reduction = 0.0f;

        if ( ParseAuthoredScalar(p2, value) && value.percent && value.value >= 0.0f )
            buff.deflect_end_energy_reduction = std::min(value.value, 100.0f) / 100.0f;
        else
            ypa_log_out("WARNING: deflect_end_energy_reduction='%s' is invalid; expected an explicit percentage in range 0%%-100%%. Using 0%%.\n",
                        p2.c_str());
    }
    else if ( !StriCmp(p1, "deflect_vp") )
    {
        const long vp = parser.stol(p2, nullptr, 0);
        buff.deflect_vp = vp > 0 && vp <= std::numeric_limits<int16_t>::max()
            ? (int16_t)vp
            : 0;
    }
    else if ( !StriCmp(p1, "deflect_3ds") )
        buff.deflect_3ds = p2;
    else if ( !StriCmp(p1, "deflect_base") )
        buff.deflect_base = p2;
    else if ( !StriCmp(p1, "glow_intensity") )
    {
        const float value = parser.stof(p2, 0);
        buff.glow_intensity = std::isfinite(value)
            ? std::max(0.0f, std::min(value, 1.0f))
            : 0.0f;
    }
    else if ( ParseTintParam(parser, "glow_tint", p1, p2, buff.glow_tint, true) )
        return true;
    else if ( !StriCmp(p1, "glow_pulse_seconds") )
    {
        const float value = parser.stof(p2, 0);
        buff.glow_pulse_seconds = std::isfinite(value) && value > 0.0f
            ? std::min(value, 60.0f)
            : 0.0f;
    }
    else
        return false;

    return true;
}

static bool ParseDebuffParam(ScriptParser::Parser &parser,
                             const std::string &p1,
                             const std::string &p2,
                             TWeaponDebuffConfig &debuff)
{
    if ( !StriCmp(p1, "allow_on_host_station") )
        debuff.allow_on_host_station = parser.stol(p2, nullptr, 0) != 0;
    else if ( !StriCmp(p1, "allow_on_children") )
        debuff.allow_on_children = parser.stol(p2, nullptr, 0) != 0;
    else if ( !StriCmp(p1, "name") )
        debuff.name = p2;
    else if ( !StriCmp(p1, "energy") )
        ParseAbsoluteOrPercent(p2, debuff.energy, 100.0f);
    else if ( !StriCmp(p1, "mindcontrol") )
        debuff.mindcontrol = parser.stol(p2, nullptr, 0) != 0;
    else if ( !StriCmp(p1, "tick_time") )
    {
        int tickTime = parser.stol(p2, nullptr, 0);
        debuff.tick_time = tickTime > 0 ? tickTime : 1000;
        debuff.has_tick_time = true;
    }
    else if ( !StriCmp(p1, "duration") )
    {
        int duration = parser.stol(p2, nullptr, 0);
        debuff.duration = duration > 0 ? duration : 0;
    }
    else if ( !StriCmp(p1, "stun") )
        debuff.stun = parser.stol(p2, nullptr, 0) != 0;
    else if ( !StriCmp(p1, "stun_motion_level") )
        debuff.stun_motion_level = ParseNormalizedOrPercent(p2);
    else if ( !StriCmp(p1, "stun_unit_fire") )
        debuff.stun_unit_fire = parser.stol(p2, nullptr, 0) != 0;
    else if ( !StriCmp(p1, "force_malus") )
        debuff.force_malus = ParseMalusPercent(p2);
    else if ( !StriCmp(p1, "maxrot_malus") )
        debuff.maxrot_malus = ParseMalusPercent(p2);
    else if ( !StriCmp(p1, "shield_malus") )
        debuff.shield_malus = ParseMalusPercent(p2);
    else if ( !StriCmp(p1, "mgun_shot_time_malus") )
        debuff.mgun_shot_time_malus = ParseMalusPercent(p2);
    else if ( !StriCmp(p1, "shot_time_malus") )
        debuff.shot_time_malus = ParseMalusPercent(p2);
    else if ( !StriCmp(p1, "snd_pitch_malus") )
        debuff.snd_pitch_multiplier = ParseSignedPitchPercent(p2);
    else if ( ParseTintParam(parser, "target_tint", p1, p2,
                             debuff.target_tint, true) )
        return true;
    else if ( !StriCmp(p1, "vp") )
    {
        if ( !ParseSmartVPList(p2, debuff.vps) )
            debuff.vps.clear();
    }
    else if ( !StriCmp(p1, "3ds") )
        debuff.mesh3ds = p2;
    else if ( !StriCmp(p1, "scale") )
        debuff.visual_scale = ParseVPScaleValue(parser, p2);
    else if ( ParseTintParam(parser, "tint", p1, p2, debuff.tint) )
        return true;
    else if ( !StriCmp(p1, "random_max_offset") )
        ParseAbsoluteOrPercent(p2, debuff.random_max_offset, 100.0f);
    else if ( ParseTintParam(parser, "vp_trail_tint", p1, p2,
                             debuff.vp_trail_tint) )
    {
        debuff.has_vp_trail_tint = true;
        return true;
    }
    else if ( !StriCmp(p1, "icon") )
        debuff.icon = p2;
    else if ( !StriCmp(p1, "snd_sample") )
        debuff.tick_snd.MainSample.Name = p2;
    else if ( !StriCmp(p1, "snd_pitch") )
        ParseSoundPitchRange(p2, debuff.tick_snd);
    else if ( !StriCmp(p1, "snd_volume") )
        debuff.tick_snd.volume = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "snd_radius") )
        debuff.tick_snd.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "pal_slot") )
        debuff.tick_snd.sndPrm.slot = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "pal_mag0") )
        debuff.tick_snd.sndPrm.mag0 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, "pal_mag1") )
        debuff.tick_snd.sndPrm.mag1 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, "pal_time") )
        debuff.tick_snd.sndPrm.time = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "pal_radius") )
        debuff.tick_snd.sndPrm.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "shk_slot") )
        debuff.tick_snd.sndPrm_shk.slot = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "shk_mag0") )
        debuff.tick_snd.sndPrm_shk.mag0 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, "shk_mag1") )
        debuff.tick_snd.sndPrm_shk.mag1 = parser.stof(p2, 0);
    else if ( !StriCmp(p1, "shk_time") )
        debuff.tick_snd.sndPrm_shk.time = parser.stol(p2, nullptr, 0);
    else if ( !StriCmp(p1, "shk_radius") )
        debuff.tick_snd.sndPrm_shk.radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
    else if ( !StriCmp(p1, "shk_mute") )
        debuff.tick_snd.sndPrm_shk.mute = parser.stof(p2, 0);
    else
        return false;

    return true;
}

bool BuffProfileParser::IsScope(ScriptParser::Parser &parser,
                                const std::string &word,
                                const std::string &opt)
{
    if ( StriCmp(word, "new_buff") )
        return false;

    _profile = nullptr;
    _profileID = -1;

    if ( opt.empty() )
    {
        ypa_log_out("WARNING: new_buff requires a numeric id; profile ignored.\n");
        return true;
    }

    const long value = parser.stol(opt, nullptr, 0);
    if ( value < 0 || value > std::numeric_limits<int32_t>::max() )
    {
        ypa_log_out("WARNING: new_buff id '%s' is invalid; profile ignored.\n", opt.c_str());
        return true;
    }

    _profileID = (int32_t)value;
    uint32_t revision = 1;
    auto oldProfile = _profiles.find(_profileID);
    if ( oldProfile != _profiles.end() )
    {
        revision = oldProfile->second.revision + 1;
        ypa_log_out("WARNING: new_buff %d is defined more than once; replacing the previous profile.\n", _profileID);
    }

    TVehicleBuffConfig profile;
    profile.valid = true;
    profile.profile_id = _profileID;
    profile.revision = revision;
    _profiles[_profileID] = profile;
    _profile = &_profiles[_profileID];
    return true;
}

int BuffProfileParser::Handle(ScriptParser::Parser &parser,
                              const std::string &p1,
                              const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        _profile = nullptr;
        _profileID = -1;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !_profile )
        return ScriptParser::RESULT_OK;

    return ParseBuffParam(parser, p1, p2, *_profile)
        ? ScriptParser::RESULT_OK
        : ScriptParser::RESULT_UNKNOWN;
}

bool DebuffProfileParser::IsScope(ScriptParser::Parser &parser,
                                  const std::string &word,
                                  const std::string &opt)
{
    if ( StriCmp(word, "new_debuff") )
        return false;

    _profile = nullptr;
    _profileID = -1;

    if ( opt.empty() )
    {
        ypa_log_out("WARNING: new_debuff requires a numeric id; profile ignored.\n");
        return true;
    }

    const long value = parser.stol(opt, nullptr, 0);
    if ( value < 0 || value > std::numeric_limits<int32_t>::max() )
    {
        ypa_log_out("WARNING: new_debuff id '%s' is invalid; profile ignored.\n", opt.c_str());
        return true;
    }

    _profileID = (int32_t)value;
    uint32_t revision = 1;
    auto oldProfile = _profiles.find(_profileID);
    if ( oldProfile != _profiles.end() )
    {
        revision = oldProfile->second.revision + 1;
        ypa_log_out("WARNING: new_debuff %d is defined more than once; replacing the previous profile.\n", _profileID);
    }

    TWeaponDebuffConfig profile;
    profile.valid = true;
    profile.profile_id = _profileID;
    profile.revision = revision;
    _profiles[_profileID] = profile;
    _profile = &_profiles[_profileID];
    return true;
}

int DebuffProfileParser::Handle(ScriptParser::Parser &parser,
                                const std::string &p1,
                                const std::string &p2)
{
    if ( !StriCmp(p1, "end") )
    {
        _profile = nullptr;
        _profileID = -1;
        return ScriptParser::RESULT_SCOPE_END;
    }

    if ( !_profile )
        return ScriptParser::RESULT_OK;

    return ParseDebuffParam(parser, p1, p2, *_profile)
        ? ScriptParser::RESULT_OK
        : ScriptParser::RESULT_UNKNOWN;
}

bool ParseVPSpinParam(ScriptParser::Parser &parser,
                      const std::string &prefix,
                      const std::string &p1,
                      const std::string &p2,
                      vec3d &spin)
{
    const std::string keyPrefix = prefix.empty() ? std::string() : prefix + "_";

    if ( StriCmp(p1, keyPrefix + "spin_xyz") )
        return false;

    std::string axis[3];
    if ( ReadTuple(p1, p2, 3, axis) )
    {
        spin.x = Spin::ClampStrength(parser.stof(axis[0], 0));
        spin.y = Spin::ClampStrength(parser.stof(axis[1], 0));
        spin.z = Spin::ClampStrength(parser.stof(axis[2], 0));
    }
    return true;
}

float ParseFiniteFloatOrFallback(ScriptParser::Parser &parser,
                                 const std::string &value,
                                 float fallback)
{
    size_t parsed = 0;
    float result = parser.stof(value, &parsed);
    if ( parsed != value.size() || !std::isfinite(result) )
        return fallback;

    return result;
}

bool ParseVPRotationParam(ScriptParser::Parser &parser,
                          const std::string &prefix,
                          const std::string &p1,
                          const std::string &p2,
                          vec3d &rotation)
{
    const std::string keyPrefix = prefix.empty() ? std::string() : prefix + "_";

    if ( StriCmp(p1, keyPrefix + "rotation_xyz") )
        return false;

    std::string axis[3];
    if ( ReadTuple(p1, p2, 3, axis) )
    {
        rotation.x = ParseFiniteFloatOrFallback(parser, axis[0], 0.0f);
        rotation.y = ParseFiniteFloatOrFallback(parser, axis[1], 0.0f);
        rotation.z = ParseFiniteFloatOrFallback(parser, axis[2], 0.0f);
    }
    return true;
}

bool ParseExternalVisualParam(const std::string &p1,
                              const std::string &p2,
                              const std::string &prefix,
                              World::TExternalVisualSet &visuals)
{
    if ( !StriCmp(p1, prefix + "_normal") )
        visuals.normal = p2;
    else if ( !StriCmp(p1, prefix + "_fire") )
        visuals.fire = p2;
    else if ( !StriCmp(p1, prefix + "_megadeth") )
        visuals.megadeth = p2;
    else if ( !StriCmp(p1, prefix + "_wait") )
        visuals.wait = p2;
    else if ( !StriCmp(p1, prefix + "_dead") )
        visuals.dead = p2;
    else if ( !StriCmp(p1, prefix + "_genesis") )
        visuals.genesis = p2;
    else
        return false;

    return true;
}

bool ParseScriptIntRange(const std::string &value, int &minValue, int &maxValue)
{
    return World::ParseIntRangeValue(value, minValue, maxValue);
}

bool ParseScriptFloatRange(const std::string &value, float &minValue, float &maxValue)
{
    return World::ParseFloatRangeValue(value, minValue, maxValue);
}

bool ParseNumWeaponsRange(const std::string &value,
                          int &minCount,
                          int &maxCount)
{
    if ( !ParseScriptIntRange(value, minCount, maxCount) )
        return false;

    // Preserve the vanilla-compatible fixed zero value. Mixed zero ranges are
    // rejected instead of giving zero two different meanings inside one roll.
    if ( minCount == 0 && maxCount == 0 )
        return true;

    return minCount >= 1 && maxCount <= 255;
}

bool ParseMimicEnergyCostRange(const std::string &value,
                               int &minCost,
                               int &maxCost)
{
    if ( !ParseScriptIntRange(value, minCost, maxCost) )
        return false;

    if ( minCost == 0 && maxCost == 0 )
        return true;
    if ( minCost <= 0 || maxCost <= 0 )
        return false;

    return true;
}

bool ParseDecorationFXParam(ScriptParser::Parser &parser,
                            const std::string &p1,
                            const std::string &p2,
                            World::TDecorationFXConfig &config)
{
    if ( !StriCmp(p1, "decoration_fx_vp") )
    {
        int vp = parser.stol(p2, nullptr, 0);
        config.vp = vp > 0 ? vp : 0;
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_3ds") )
    {
        config.mesh3ds = p2;
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_base") )
    {
        config.basePath = p2;
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_mode") )
    {
        if ( !StriCmp(p2, "persistent") )
            config.mode = World::DECORATION_FX_PERSISTENT;
        else
            config.mode = World::DECORATION_FX_PERIODIC;

        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_interval") )
    {
        int intervalMin = 0;
        int intervalMax = 0;
        if ( ParseScriptIntRange(p2, intervalMin, intervalMax) )
        {
            config.interval_min = intervalMin;
            config.interval_max = intervalMax;
        }
        else
        {
            config.interval_min = 0;
            config.interval_max = 0;
        }
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_count") )
    {
        int countMin = 0;
        int countMax = 0;
        if ( ParseScriptIntRange(p2, countMin, countMax) )
        {
            config.count_min = std::max(0, std::min(countMin, 32));
            config.count_max = std::max(0, std::min(countMax, 32));
        }
        else
        {
            config.count_min = 0;
            config.count_max = 0;
        }
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_duration") )
    {
        int duration = parser.stol(p2, nullptr, 0);
        config.duration = duration > 0 ? duration : 1000;
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            config.fade_in = std::max(0, (int)parser.stol(fade[0], nullptr, 0));
            config.fade_out = std::max(0, (int)parser.stol(fade[1], nullptr, 0));
        }
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_vp_trail_fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            config.vp_trail_fade_in = std::max(0, (int)parser.stol(fade[0], nullptr, 0));
            config.vp_trail_fade_out = std::max(0, (int)parser.stol(fade[1], nullptr, 0));
        }
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_random_pos") )
    {
        float radius = parser.stof(p2, 0);
        config.random_pos = radius > 0.0 ? radius : 0.0;
        return true;
    }

    if ( !StriCmp(p1, "decoration_fx_offset_xyz") )
    {
        std::string axis[3];
        if ( ReadTuple(p1, p2, 3, axis) )
        {
            config.offset.x = parser.stof(axis[0], 0);
            config.offset.y = parser.stof(axis[1], 0);
            config.offset.z = parser.stof(axis[2], 0);
        }
        return true;
    }

    if ( ParseVPScaleParam(parser, "decoration_fx", p1, p2, config.scale) )
    {
        return true;
    }

    if ( ParseVPScaleParam(parser, "decoration_fx_vp_trail", p1, p2, config.vp_trail_scale) )
    {
        return true;
    }

    if ( ParseVPSpinParam(parser, "decoration_fx", p1, p2, config.spin) )
    {
        return true;
    }

    if ( ParseTintParam(parser, "decoration_fx_tint", p1, p2, config.tint) )
    {
        return true;
    }

    if ( ParseTintParam(parser, "decoration_fx_vp_trail_tint", p1, p2, config.vp_trail_tint) )
    {
        return true;
    }

    return false;
}

// OpenNeoUA custom: parse "*_tint = R_G_B_A" (each component 0..255).
// Alpha is optional and defaults to 255. Out-of-range values are clamped.
// RGB is stored as a normalized target hue; alpha remains multiplicative.
// Neutral white/opaque default = no change.
bool ParseTintParam(ScriptParser::Parser &parser,
                    const std::string &paramName,
                    const std::string &p1,
                    const std::string &p2,
                    TVisualTint &tint,
                    bool invalidFallsBackToNeutral)
{
    if ( StriCmp(p1, paramName) )
        return false;

    std::vector<std::string> parts = Stok::Split(p2, "_");

    long comp[4] = {255, 255, 255, 255};

    if ( invalidFallsBackToNeutral )
    {
        bool valid = parts.size() == 3 || parts.size() == 4;
        for (size_t i = 0; valid && i < parts.size(); i++)
        {
            const std::string &part = parts[i];
            size_t digit = 0;
            bool negative = false;
            if ( !part.empty() && (part[0] == '+' || part[0] == '-') )
            {
                negative = part[0] == '-';
                digit = 1;
            }

            if ( digit >= part.size() )
            {
                valid = false;
                break;
            }

            long value = 0;
            for (; digit < part.size(); digit++)
            {
                if ( part[digit] < '0' || part[digit] > '9' )
                {
                    valid = false;
                    break;
                }

                // Values outside 0..255 are clamped below. Saturating here avoids
                // integer overflow and parser error popups for absurdly long input.
                if ( value <= 255 )
                    value = value * 10 + (part[digit] - '0');
            }

            comp[i] = negative ? -value : value;
        }

        if ( !valid )
        {
            tint = TVisualTint();
            ypa_log_out("WARNING: invalid %s '%s', using neutral 255_255_255_255\n",
                        paramName.c_str(), p2.c_str());
            return true;
        }
    }
    else
    {
        for (size_t i = 0; i < parts.size() && i < 4; i++)
            comp[i] = parser.stol(parts[i], 0, 10);
    }

    auto clamp255 = [](long v) -> float
    {
        if ( v < 0 )
            v = 0;
        if ( v > 255 )
            v = 255;
        return (float)v / 255.0f;
    };

    tint.r = clamp255(comp[0]);
    tint.g = clamp255(comp[1]);
    tint.b = clamp255(comp[2]);
    tint.a = clamp255(comp[3]);
    return true;
}

bool ParseBoundedIntegerParam(const std::string &paramName,
                              const std::string &p1,
                              const std::string &p2,
                              int minValue,
                              int maxValue,
                              int fallback,
                              int &value)
{
    if ( StriCmp(p1, paramName) )
        return false;

    size_t digit = 0;
    bool negative = false;
    if ( !p2.empty() && (p2[0] == '+' || p2[0] == '-') )
    {
        negative = p2[0] == '-';
        digit = 1;
    }

    bool valid = digit < p2.size();
    long parsed = 0;
    for (; valid && digit < p2.size(); digit++)
    {
        if ( p2[digit] < '0' || p2[digit] > '9' )
        {
            valid = false;
            break;
        }

        if ( parsed <= (long)maxValue + 1 )
            parsed = parsed * 10 + (p2[digit] - '0');
    }

    if ( !valid )
    {
        value = fallback;
        ypa_log_out("WARNING: invalid %s '%s', using %d\n",
                    paramName.c_str(), p2.c_str(), fallback);
        return true;
    }

    if ( negative )
        parsed = -parsed;
    value = (int)std::max((long)minValue, std::min(parsed, (long)maxValue));
    return true;
}

float ParseBoundedPositiveFiniteOrZero(ScriptParser::Parser &parser,
                                        const std::string &value,
                                        float maximum)
{
    size_t parsed = 0;
    const float result = parser.stof(value, &parsed);
    if ( parsed != value.size() || !std::isfinite(result) || result <= 0.0f )
        return 0.0f;

    return result > maximum ? maximum : result;
}

static double ParseBoundedFiniteOrZero(ScriptParser::Parser &parser,
                                       const std::string &value,
                                       double maximum)
{
    size_t parsed = 0;
    const double result = parser.stod(value, &parsed);
    if ( parsed != value.size() || !std::isfinite(result) )
        return 0.0;

    return std::max(-maximum, std::min(maximum, result));
}

float ParseBoundedNonNegativeFiniteOrFallback(
    ScriptParser::Parser &parser, const std::string &value,
    float maximum, float fallback)
{
    size_t parsed = 0;
    const float result = parser.stof(value, &parsed);
    if ( parsed != value.size() || !std::isfinite(result) || result < 0.0f )
        return fallback;

    return result > maximum ? maximum : result;
}

static bool ParseMeshTracerUniformSize(ScriptParser::Parser &parser,
                                       const std::string &value,
                                       float &result)
{
    // head_size / tail_size are single scalar endpoint overrides. They control
    // the transverse X/Y section uniformly; size_z remains the sole owner of
    // longitudinal tracer length so tapering cannot introduce gaps.
    const float size = ParseBoundedNonNegativeFiniteOrFallback(
        parser, value, 100.0f, -1.0f);
    if ( size < 0.0f )
        return false;

    result = size;
    return true;
}

bool ParseMeshTracerParam(ScriptParser::Parser &parser,
                            const std::string &p1,
                            const std::string &p2,
                            World::TWeaponTracerConfig &config,
                            const std::string &prefix)
{
    const auto key = [&prefix](const char *suffix)
    {
        return prefix + suffix;
    };

    if ( !StriCmp(p1, key("enable")) )
    {
        config.enabled = p2 == "1";
        return true;
    }

    if ( ParseTintParam(parser, key("tint"), p1, p2, config.tint, true) )
        return true;

    if ( !StriCmp(p1, key("size_xyz")) )
    {
        std::string size[3];
        if ( ReadTuple(p1, p2, 3, size) )
        {
            config.size_x = ParseBoundedPositiveFiniteOrZero(parser, size[0], 100.0f);
            // A malformed Y inherits X.
            const float sizeY = ParseBoundedNonNegativeFiniteOrFallback(
                parser, size[1], 100.0f, -1.0f);
            config.has_size_y = sizeY >= 0.0f;
            config.size_y = config.has_size_y ? sizeY : 0.0f;
            config.size_z = ParseBoundedPositiveFiniteOrZero(parser, size[2], 6000.0f);
        }
        return true;
    }

    if ( !StriCmp(p1, key("pos_xyz")) )
    {
        std::string pos[3];
        if ( ReadTuple(p1, p2, 3, pos) )
        {
            config.pos.x = ParseBoundedFiniteOrZero(parser, pos[0], 6000.0);
            config.pos.y = ParseBoundedFiniteOrZero(parser, pos[1], 6000.0);
            config.pos.z = ParseBoundedFiniteOrZero(parser, pos[2], 6000.0);
        }
        return true;
    }

    if ( !StriCmp(p1, key("path")) )
    {
        config.mesh_path = p2;
        return true;
    }

    if ( ParseTintParam(parser, key("head_tint"), p1, p2, config.tint_head, true) )
    {
        config.has_tint_head = true;
        return true;
    }

    if ( ParseTintParam(parser, key("tail_tint"), p1, p2, config.tint_tail, true) )
    {
        config.has_tint_tail = true;
        return true;
    }

    if ( !StriCmp(p1, key("head_size")) )
    {
        float size = 0.0f;
        config.has_head_size = ParseMeshTracerUniformSize(parser, p2, size);
        config.head_size = config.has_head_size ? size : 0.0f;
        return true;
    }

    if ( !StriCmp(p1, key("tail_size")) )
    {
        float size = 0.0f;
        config.has_tail_size = ParseMeshTracerUniformSize(parser, p2, size);
        config.tail_size = config.has_tail_size ? size : 0.0f;
        return true;
    }

    if ( !StriCmp(p1, key("glow_rate")) )
    {
        config.glow_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, key("noise_rate")) )
    {
        config.noise_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, key("pulse_rate")) )
    {
        config.pulse_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, key("pulse_speed")) )
    {
        config.pulse_speed = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    return false;
}

bool ParseLaserMeshParam(
    ScriptParser::Parser &parser, const std::string &p1,
    const std::string &p2, World::TWeapProto::TLaserMeshConfig &config)
{
    if ( !StriCmp(p1, "laser_mesh_enable") )
    {
        // Only the exact value 1 enables the optional renderer.
        config.enabled = p2 == "1";
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_path") )
    {
        config.mesh_path = p2;
        return true;
    }

    if ( ParseTintParam(parser, "laser_mesh_tint", p1, p2, config.tint, true) )
        return true;

    if ( !StriCmp(p1, "laser_mesh_size_xy") )
    {
        std::string size[2];
        if ( ReadTuple(p1, p2, 2, size) )
        {
            // A non-positive or malformed primary size leaves the external mesh
            // renderer disabled for this weapon; no hidden geometry is substituted.
            config.size_x = ParseBoundedPositiveFiniteOrZero(parser, size[0], 100.0f);

            // The secondary axis is optional. Zero, negative and malformed values
            // deliberately clear the override so it inherits size_x.
            const float sizeY = ParseBoundedNonNegativeFiniteOrFallback(
                parser, size[1], 100.0f, -1.0f);
            config.has_size_y = sizeY > 0.0f;
            config.size_y = config.has_size_y ? sizeY : 0.0f;
        }
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_glow_rate") )
    {
        config.glow_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_pulse_rate") )
    {
        config.pulse_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_pulse_speed") )
    {
        config.pulse_speed = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_noise_rate") )
    {
        config.noise_rate = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 10.0f, 0.0f);
        return true;
    }

    if ( !StriCmp(p1, "laser_mesh_impact_fade_length") )
    {
        // World-space distance over which the laser body fades only when the
        // beam has a real unit/terrain/building contact. Zero/invalid disables.
        config.impact_fade_length = ParseBoundedNonNegativeFiniteOrFallback(
            parser, p2, 12000.0f, 0.0f);
        return true;
    }

    return false;
}

enum ChainFXParseContext
{
    CHAIN_FX_VEHICLE,
    CHAIN_FX_WEAPON,
    CHAIN_FX_SUPERITEM
};

static const char *ChainFXContextName(ChainFXParseContext context)
{
    if ( context == CHAIN_FX_WEAPON )
        return "weapon";
    if ( context == CHAIN_FX_SUPERITEM )
        return "SuperItem";
    return "vehicle";
}

static World::TChainFXConfig::Trigger ParseChainFXTrigger(const std::string &name,
                                                          ChainFXParseContext context)
{
    if ( context == CHAIN_FX_WEAPON )
    {
        if ( !StriCmp(name, "detonate") )
            return World::TChainFXConfig::TRIGGER_DETONATE;

        if ( !StriCmp(name, "impact_world") )
            return World::TChainFXConfig::TRIGGER_IMPACT_WORLD;

        // Weapon fired: spawns at the muzzle when the projectile or beam starts.
        if ( !StriCmp(name, "launch") )
            return World::TChainFXConfig::TRIGGER_LAUNCH;
    }
    else if ( context == CHAIN_FX_SUPERITEM )
    {
        if ( !StriCmp(name, "detonate") )
            return World::TChainFXConfig::TRIGGER_DETONATE;
    }
    else
    {
        // OpenNeoUA: destroyed = killed on the ground, air_destroyed = killed
        // in flight. The old crash/destroyed words are gone without aliases:
        // unknown triggers warn and the block is ignored.
        if ( !StriCmp(name, "air_destroyed") )
            return World::TChainFXConfig::TRIGGER_AIR_DESTROYED;

        if ( !StriCmp(name, "destroyed") )
            return World::TChainFXConfig::TRIGGER_DESTROYED;
    }

    return World::TChainFXConfig::TRIGGER_NONE;
}

static World::TChainFXConfig::Mode ParseChainFXMode(const std::string &name)
{
    if ( !StriCmp(name, "visual") )
        return World::TChainFXConfig::MODE_VISUAL;

    if ( !StriCmp(name, "physical") )
        return World::TChainFXConfig::MODE_PHYSICAL;

    return World::TChainFXConfig::MODE_VISUAL;
}

// Chain FX offset_xyz / launch_xyz = x_y_z. Every axis is a fixed value or an
// inclusive "min~max" range ("_" already separates the axes). Invalid text
// falls back to a fixed 0 on all axes so the block stays vanilla-safe and loadable.
static void ParseChainFXRangeTuple(const std::string &key,
                                   const std::string &value,
                                   vec3d &minValue,
                                   vec3d &maxValue)
{
    std::string axis[3];
    float parsedMin[3] = {0.0f, 0.0f, 0.0f};
    float parsedMax[3] = {0.0f, 0.0f, 0.0f};
    bool valid = World::SplitTupleValue(value, 3, axis);
    for (int i = 0; valid && i < 3; i++)
        valid = World::ParseFloatRangeValue(axis[i], parsedMin[i], parsedMax[i], '~');

    if ( valid )
    {
        minValue = vec3d(parsedMin[0], parsedMin[1], parsedMin[2]);
        maxValue = vec3d(parsedMax[0], parsedMax[1], parsedMax[2]);
        return;
    }

    minValue = vec3d(0.0, 0.0, 0.0);
    maxValue = vec3d(0.0, 0.0, 0.0);
    ypa_log_out("WARNING: invalid begin_fx %s '%s', using 0_0_0\n",
                key.c_str(), value.c_str());
}

class ChainFXPhysicalSoundParser : public FxParser
{
public:
    explicit ChainFXPhysicalSoundParser(World::TChainFXPhysical &physical) : _physical(physical) {}

    int Parse(ScriptParser::Parser &parser, const std::string &key, const std::string &value)
    {
        return ParseSndFX(parser, key, value);
    }

protected:
    TVhclSound *GetSndFxByName(const std::string &name) override
    {
        if ( !StriCmp(name, "normal") )
            return &_physical.sounds[TVhclProto::SND_NORMAL];
        if ( !StriCmp(name, "goingdown") )
            return &_physical.sounds[TVhclProto::SND_GODOWN];
        if ( !StriCmp(name, "explode") )
            return &_physical.sounds[TVhclProto::SND_EXPLODE];
        return nullptr;
    }

private:
    World::TChainFXPhysical &_physical;
};

// OpenNeoUA: smoke_* keys of a new_decal block. Returns true when p1 is a smoke key; any
// smoke key switches the smoke on (smoke_count = 0 switches it off again). A bad value keeps
// the default of that key (with a warning), so a typo never removes the whole smoke. Ranges
// use the shared fixed value / min_max syntax.
static bool ParseGroundDecalSmokeParam(ScriptParser::Parser &parser,
                                       const std::string &p1,
                                       const std::string &p2,
                                       World::TGroundDecalSmoke &smoke)
{
    const World::TGroundDecalSmoke defaults;

    const auto readIntRange = [&](const char *key, int lo, int hi,
                                  int defMin, int defMax, int &outMin, int &outMax)
    {
        int first = 0;
        int second = 0;
        if ( !World::ParseIntRangeValue(p2, first, second) )
        {
            ypa_log_out("WARNING: invalid begin_fx %s '%s', using %d_%d\n",
                        key, p2.c_str(), defMin, defMax);
            outMin = defMin;
            outMax = defMax;
            return;
        }

        outMin = std::max(lo, std::min(first, hi));
        outMax = std::max(lo, std::min(second, hi));
    };

    const auto readFloat = [&](const char *key, float lo, float hi, float def, float &out)
    {
        size_t parsed = 0;
        const float value = parser.stof(p2, &parsed);
        if ( parsed != p2.size() || !std::isfinite(value) )
        {
            ypa_log_out("WARNING: invalid begin_fx %s '%s', using %.3f\n", key, p2.c_str(), def);
            out = def;
            return;
        }

        out = std::max(lo, std::min(value, hi));
    };

    if ( ParseBoundedIntegerParam("smoke_chance", p1, p2, 0, 100, defaults.chance, smoke.chance) )
    {
    }
    else if ( !StriCmp(p1, "smoke_duration") )
        readIntRange("smoke_duration", 0, 3600000, defaults.duration_min, defaults.duration_max,
                     smoke.duration_min, smoke.duration_max);
    else if ( !StriCmp(p1, "smoke_fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            ParseBoundedIntegerParam(p1, p1, fade[0], 0, 3600000, defaults.fade_in, smoke.fade_in);
            ParseBoundedIntegerParam(p1, p1, fade[1], 0, 3600000, defaults.fade_out, smoke.fade_out);
        }
    }
    else if ( !StriCmp(p1, "smoke_count") )
        readIntRange("smoke_count", 0, 50, defaults.count_min, defaults.count_max,
                     smoke.count_min, smoke.count_max);
    else if ( !StriCmp(p1, "smoke_size") )
    {
        float first = 0.0f;
        float second = 0.0f;
        if ( World::ParseFloatRangeValue(p2, first, second) &&
             std::isfinite(first) && std::isfinite(second) && first > 0.0f && second > 0.0f )
        {
            smoke.size_min = std::max(0.5f, std::min(first, 200.0f));
            smoke.size_max = std::max(0.5f, std::min(second, 200.0f));
        }
        else
        {
            ypa_log_out("WARNING: invalid begin_fx smoke_size '%s', using %.1f_%.1f\n",
                        p2.c_str(), defaults.size_min, defaults.size_max);
            smoke.size_min = defaults.size_min;
            smoke.size_max = defaults.size_max;
        }
    }
    else if ( !StriCmp(p1, "smoke_growth") )
        readFloat("smoke_growth", 1.0f, 10.0f, defaults.growth, smoke.growth);
    else if ( !StriCmp(p1, "smoke_rise_speed") )
        readFloat("smoke_rise_speed", 0.0f, 200.0f, defaults.rise_speed, smoke.rise_speed);
    else if ( !StriCmp(p1, "smoke_puff_time") )
        readIntRange("smoke_puff_time", 200, 60000, defaults.puff_time_min, defaults.puff_time_max,
                     smoke.puff_time_min, smoke.puff_time_max);
    else if ( !StriCmp(p1, "smoke_drift") )
        readFloat("smoke_drift", 0.0f, 300.0f, defaults.drift, smoke.drift);
    else if ( !StriCmp(p1, "smoke_spread") )
        readFloat("smoke_spread", 0.0f, 100.0f, defaults.spread, smoke.spread);
    else if ( !StriCmp(p1, "smoke_intensity") )
        readFloat("smoke_intensity", 0.0f, 300.0f, defaults.intensity, smoke.intensity);
    else if ( ParseTintParam(parser, "smoke_tint", p1, p2, smoke.tint, true) )
    {
    }
    else
        return false;

    smoke.enabled = true;
    return true;
}

static int ParseChainFXBlock(ScriptParser::Parser &parser,
                             std::vector<World::TChainFXConfig> *out,
                             ChainFXParseContext context)
{
    World::TChainFXConfig::Mode mode = World::TChainFXConfig::MODE_VISUAL;
    float startSize = 1.0;
    float midSize = 0.0;
    float endSize = 0.0;
    bool hasMidSize = false;
    bool hasEndSize = false;
    int countMin = 1;
    int countMax = 1;
    // Offset accepts a fixed value or a min_max range rolled for each instance.
    vec3d offsetMin = vec3d(0.0, 0.0, 0.0);
    vec3d offsetMax = vec3d(0.0, 0.0, 0.0);
    vec3d launchMin = vec3d(0.0, 0.0, 0.0);
    vec3d launchMax = vec3d(0.0, 0.0, 0.0);
    vec3d spin = vec3d(0.0, 0.0, 0.0);
    int duration = 0;
    int fadeIn = 0;
    int fadeOut = 0;
    std::vector<World::TChainFXVisual> visuals;
    World::TVisualTint visualTint;
    bool hasVisualTint = false;
    int physicalVehicle = 0;
    bool hasPhysicalVehicleKey = false;
    std::shared_ptr<World::TChainFXPhysical> physical(new World::TChainFXPhysical());
    for (World::TVhclSound &sound : physical->sounds)
        InitStatusSoundFXDefaults(sound, 120);
    ChainFXPhysicalSoundParser physicalSounds(*physical);
    bool hasInlinePhysicalKey = false;
    bool hasOffset = false;
    bool invalidInlinePhysical = false;
    bool randomRotation = false;
    World::TChainFXConfig::Trigger trigger = World::TChainFXConfig::TRIGGER_NONE;
    bool hasTrigger = false;
    bool badTrigger = false;
    bool badMode = false;

    std::string p1;
    std::string p2;

    while ( parser.ReadLine(&p1) )
    {
        size_t line_start = p1.find(";#!");
        if (line_start != std::string::npos)
            p1 = p1.substr(line_start + 3);

        size_t line_end = p1.find_first_of(";\n\r");
        if (line_end != std::string::npos)
            p1.erase(line_end);

        Stok stok(p1, "= \t");
        if ( !stok.GetNext(&p1) )
            continue;

        p2.clear();
        stok.GetNext(&p2);

        if ( !StriCmp(p1, "end") )
        {
            if ( badTrigger || badMode )
                return ScriptParser::RESULT_OK;

            if ( !hasTrigger )
            {
                ypa_log_out("WARNING: begin_fx without trigger ignored for %s prototype\n",
                            ChainFXContextName(context));
                return ScriptParser::RESULT_OK;
            }

            if ( !hasEndSize )
                endSize = 0.0;

            if ( mode == World::TChainFXConfig::MODE_VISUAL )
            {
                if ( duration > 0 && !visuals.empty() )
                {
                    World::TChainFXConfig chain;
                    chain.mode = mode;
                    chain.trigger = trigger;
                    chain.random_rotation = randomRotation;
                    chain.count_min = countMin;
                    chain.count_max = countMax;
                    chain.offset_min = offsetMin;
                    chain.offset_max = offsetMax;
                    chain.spin = spin;
                    chain.start_size = startSize;
                    chain.mid_size = midSize;
                    chain.end_size = endSize;
                    chain.has_mid_size = hasMidSize;
                    chain.duration = duration;
                    chain.fade_in = fadeIn;
                    chain.fade_out = fadeOut;
                    // One tint applies to every visual, regardless of its position in the block.
                    for (World::TChainFXVisual &visual : visuals)
                    {
                        visual.tint = visualTint;
                        visual.has_tint = hasVisualTint;
                    }
                    chain.visuals = visuals;
                    out->push_back(chain);
                }
            }
            else if ( mode == World::TChainFXConfig::MODE_PHYSICAL )
            {
                const bool hasPositiveVP = std::find_if(
                    physical->vp_models.begin(), physical->vp_models.end(),
                    [](int16_t vp) { return vp > 0; }) != physical->vp_models.end();
                const bool vpPairsValid =
                    physical->vp_impacts.empty() ||
                    physical->vp_impacts.size() == physical->vp_models.size() ||
                    (physical->vp_models.empty() && physical->vp_impacts.size() == 1);

                if ( physicalVehicle > 0 && !hasInlinePhysicalKey )
                {
                    World::TChainFXConfig chain;
                    chain.mode = mode;
                    chain.trigger = trigger;
                    chain.random_rotation = randomRotation;
                    chain.count_min = countMin;
                    chain.count_max = countMax;
                    chain.offset_min = offsetMin;
                    chain.offset_max = offsetMax;
                    chain.physical_vehicle = physicalVehicle;
                    out->push_back(chain);
                }
                else if ( hasPhysicalVehicleKey || hasOffset )
                {
                    ypa_log_out("WARNING: begin_fx physical mixes legacy and inline keys; block ignored\n");
                }
                else if ( !vpPairsValid )
                {
                    ypa_log_out("WARNING: begin_fx physical vp_model/vp_impact list sizes differ; block ignored\n");
                }
                else if ( !invalidInlinePhysical &&
                          physical->mass > 0.0f && physical->radius > 0.0f &&
                          (hasPositiveVP || !physical->mesh3ds.empty() ||
                           !physical->base_model.empty()) )
                {
                    World::TChainFXConfig chain;
                    chain.mode = mode;
                    chain.trigger = trigger;
                    chain.random_rotation = randomRotation;
                    chain.count_min = countMin;
                    chain.count_max = countMax;
                    chain.launch_min = launchMin;
                    chain.launch_max = launchMax;
                    chain.physical_inline = physical;
                    out->push_back(chain);
                }
                else
                {
                    ypa_log_out("WARNING: incomplete or invalid begin_fx physical fragment ignored\n");
                }
            }

            return ScriptParser::RESULT_OK;
        }

        if ( badMode )
            continue;

        if ( !StriCmp(p1, "mode") )
        {
            if ( !StriCmp(p2, "visual") || !StriCmp(p2, "physical") )
            {
                mode = ParseChainFXMode(p2);
                if ( context == CHAIN_FX_SUPERITEM && mode != World::TChainFXConfig::MODE_VISUAL )
                {
                    ypa_log_out("WARNING: SuperItem begin_fx supports only visual mode; block ignored\n");
                    badMode = true;
                }
            }
            else
            {
                ypa_log_out("WARNING: Unknown begin_fx mode '%s' ignored\n", p2.c_str());
                badMode = true;
            }
        }
        else if ( !StriCmp(p1, "trigger") )
        {
            trigger = ParseChainFXTrigger(p2, context);
            hasTrigger = true;
            if ( trigger == World::TChainFXConfig::TRIGGER_NONE )
            {
                ypa_log_out("WARNING: Unknown or unsupported begin_fx trigger '%s' ignored\n", p2.c_str());
                badTrigger = true;
            }
        }
        else if ( !StriCmp(p1, "start_size") )
            startSize = parser.stof(p2, 0);
        else if ( !StriCmp(p1, "mid_size") )
        {
            size_t parsed = 0;
            const float value = parser.stof(p2, &parsed);
            if ( parsed == p2.size() && std::isfinite(value) && value >= 0.0f )
            {
                midSize = value;
                hasMidSize = true;
            }
            else
            {
                ypa_log_out("WARNING: Invalid begin_fx mid_size '%s'; using linear start_size -> end_size fallback\n",
                            p2.c_str());
                hasMidSize = false;
            }
        }
        else if ( !StriCmp(p1, "end_size") )
        {
            endSize = parser.stof(p2, 0);
            hasEndSize = true;
        }
        else if ( !StriCmp(p1, "duration") )
            duration = parser.stol(p2, nullptr, 0);
        else if ( !StriCmp(p1, "fade_inout") )
        {
            std::string fade[2];
            if ( ReadTuple(p1, p2, 2, fade) )
            {
                fadeIn = NonNegativeFiniteMilliseconds(parser, fade[0]);
                fadeOut = NonNegativeFiniteMilliseconds(parser, fade[1]);
            }
        }
        else if ( !StriCmp(p1, "count") )
        {
            if ( context == CHAIN_FX_SUPERITEM )
            {
                countMin = 1;
                countMax = 1;
                ypa_log_out("WARNING: SuperItem begin_fx does not support count; using 1\n");
            }
            else if ( !World::ParsePositiveIntRangeValue(p2, 32, countMin, countMax) )
            {
                countMin = 1;
                countMax = 1;
                ypa_log_out("WARNING: invalid begin_fx count '%s', using 1\n", p2.c_str());
            }
        }
        else if ( !StriCmp(p1, "offset_xyz") )
        {
            hasOffset = true;
            ParseChainFXRangeTuple(p1, p2, offsetMin, offsetMax);
        }
        else if ( !StriCmp(p1, "launch_xyz") )
        {
            hasInlinePhysicalKey = true;
            ParseChainFXRangeTuple(p1, p2, launchMin, launchMax);
        }
        else if ( ParseVPSpinParam(parser, "", p1, p2, spin) )
        {
        }
        else if ( !StriCmp(p1, "vp_model") )
        {
            hasInlinePhysicalKey = true;
            if ( mode == World::TChainFXConfig::MODE_PHYSICAL )
            {
                if ( p2 == "0" )
                    physical->vp_models.assign(1, 0);
                else if ( !World::ParsePositiveInt16ListValue(p2, physical->vp_models, 32) )
                {
                    physical->vp_models.clear();
                    invalidInlinePhysical = true;
                    ypa_log_out("WARNING: invalid begin_fx physical vp_model list '%s'\n",
                                p2.c_str());
                }
                continue;
            }

            const long model = parser.stol(p2, nullptr, 0);
            World::TChainFXVisual visual;
            visual.vp = model;
            visuals.push_back(visual);
        }
        else if ( !StriCmp(p1, "3ds_model") )
        {
            hasInlinePhysicalKey = true;
            physical->mesh3ds = p2;
            if ( !visuals.empty() &&
                 (visuals.back().vp > 0 || !visuals.back().basePath.empty()) &&
                 visuals.back().mesh3ds.empty() )
                visuals.back().mesh3ds = p2;
            else
            {
                World::TChainFXVisual visual;
                visual.mesh3ds = p2;
                visuals.push_back(visual);
            }
        }
        else if ( !StriCmp(p1, "base_model") )
        {
            hasInlinePhysicalKey = true;
            physical->base_model = p2;
            if ( !visuals.empty() && visuals.back().vp > 0 &&
                 visuals.back().mesh3ds.empty() && visuals.back().basePath.empty() )
                visuals.back().basePath = p2;
            else
            {
                World::TChainFXVisual visual;
                visual.basePath = p2;
                visuals.push_back(visual);
            }
        }
        else if ( !StriCmp(p1, "tint") )
        {
            if ( mode == World::TChainFXConfig::MODE_PHYSICAL )
            {
                hasInlinePhysicalKey = true;
                ParseTintParam(parser, "tint", p1, p2, physical->tint, true);
                continue;
            }
            ParseTintParam(parser, "tint", p1, p2, visualTint, true);
            hasVisualTint = true;
        }
        else if ( !StriCmp(p1, "vp_impact") )
        {
            hasInlinePhysicalKey = true;
            if ( !World::ParseNonNegativeInt16ListValue(p2, physical->vp_impacts, 32) )
            {
                physical->vp_impacts.clear();
                invalidInlinePhysical = true;
                ypa_log_out("WARNING: invalid begin_fx physical vp_impact list '%s'\n",
                            p2.c_str());
            }
        }
        else if ( !StriCmp(p1, "scale") )
        {
            hasInlinePhysicalKey = true;
            physical->visual_scale = ParseVPScaleValue(parser, p2);
        }
        else if ( !StriCmp(p1, "impact_energy") )
        {
            hasInlinePhysicalKey = true;
            const int damage = parser.stol(p2, nullptr, 0);
            physical->impact_energy = damage > 0 ? damage : 0;
        }
        else if ( !StriCmp(p1, "impact_energy_radius") )
        {
            hasInlinePhysicalKey = true;
            physical->impact_energy_radius = NonNegativeFiniteOrZero(parser.stof(p2, 0));
        }
        else if ( !StriCmp(p1, "lifetime") )
        {
            hasInlinePhysicalKey = true;
            int lifetimeMin = 0;
            int lifetimeMax = 0;
            if ( World::ParseIntRangeValue(p2, lifetimeMin, lifetimeMax) &&
                 lifetimeMin > 0 && lifetimeMax > 0 )
            {
                physical->lifetime_min = lifetimeMin;
                physical->lifetime_max = lifetimeMax;
            }
            else
            {
                physical->lifetime_min = 30000;
                physical->lifetime_max = 30000;
                ypa_log_out("WARNING: invalid begin_fx physical lifetime '%s'; using 30000 ms\n",
                            p2.c_str());
            }
        }
        else if ( !StriCmp(p1, "follow_carcass") )
        {
            hasInlinePhysicalKey = true;
            size_t parsed = 0;
            physical->follow_carcass = parser.stol(p2, &parsed, 0) == 1 && parsed == p2.size();
        }
        else if ( !StriCmp(p1, "mass") || !StriCmp(p1, "airconst") ||
                  !StriCmp(p1, "force") || !StriCmp(p1, "maxrot") ||
                  !StriCmp(p1, "radius") )
        {
            hasInlinePhysicalKey = true;
            size_t parsed = 0;
            const float value = parser.stof(p2, &parsed);
            const bool valid = parsed == p2.size() && std::isfinite(value) &&
                               (value > 0.0f ||
                                ((!StriCmp(p1, "airconst") || !StriCmp(p1, "force") ||
                                  !StriCmp(p1, "maxrot")) && value == 0.0f));
            if ( !valid )
            {
                if ( !StriCmp(p1, "mass") || !StriCmp(p1, "radius") )
                    invalidInlinePhysical = true;
                ypa_log_out("WARNING: invalid begin_fx physical %s '%s'\n",
                            p1.c_str(), p2.c_str());
            }
            else if ( !StriCmp(p1, "mass") )
                physical->mass = value;
            else if ( !StriCmp(p1, "airconst") )
                physical->airconst = value;
            else if ( !StriCmp(p1, "force") )
                physical->force = value;
            else if ( !StriCmp(p1, "maxrot") )
                physical->maxrot = value;
            else
                physical->radius = value;
        }
        else if ( p1.compare(0, 4, "snd_") == 0 ||
                  p1.compare(0, 4, "pal_") == 0 ||
                  p1.compare(0, 4, "shk_") == 0 )
        {
            hasInlinePhysicalKey = true;
            std::string soundValue = p2;
            if ( p1.size() >= 7 && p1.compare(p1.size() - 7, 7, "_sample") == 0 )
            {
                std::string normalized;
                if ( !uaNormalizeDataAssetPath(p2, &normalized, true) )
                {
                    ypa_log_out("WARNING: invalid begin_fx sound path '%s'; sample ignored\n",
                                p2.c_str());
                    continue;
                }
                // The existing sound loader already uses data: as its root.
                soundValue = normalized.substr(5);
            }
            const int result = physicalSounds.Parse(parser, p1, soundValue);
            if ( result != ScriptParser::RESULT_OK )
                return result;
        }
        else if ( !StriCmp(p1, "physical_vehicle") )
        {
            hasPhysicalVehicleKey = true;
            if ( context == CHAIN_FX_SUPERITEM )
            {
                ypa_log_out("WARNING: SuperItem begin_fx does not support physical_vehicle; block ignored\n");
                badMode = true;
            }
            else
                physicalVehicle = parser.stol(p2, nullptr, 0);
        }
        else if ( !StriCmp(p1, "random_rotation") )
            randomRotation = p2 == "1";
        else
        {
            if ( context == CHAIN_FX_SUPERITEM )
            {
                ypa_log_out("WARNING: Unknown SuperItem begin_fx parameter '%s'; block ignored\n", p1.c_str());
                badMode = true;
                continue;
            }
            return ScriptParser::RESULT_UNKNOWN;
        }
    }

    return ScriptParser::RESULT_UNEXP_EOF;
}

// OpenNeoUA: one axis of stretch_xz of a decal profile (fixed value or min~max, > 0).
static void ParseDecalStretch(const char *key, const std::string &value,
                              float &fixed, float &minValue, float &maxValue)
{
    float parsedMin = 1.0f;
    float parsedMax = 1.0f;
    if ( World::ParseFloatRangeValue(value, parsedMin, parsedMax, '~') &&
         std::isfinite(parsedMin) && std::isfinite(parsedMax) &&
         parsedMin > 0.0f && parsedMax > 0.0f )
    {
        fixed = parsedMin;
        minValue = parsedMin;
        maxValue = parsedMax;
        return;
    }

    fixed = 1.0f;
    minValue = 1.0f;
    maxValue = 1.0f;
    ypa_log_out("WARNING: invalid new_decal %s '%s', using 1.0\n", key, value.c_str());
}

// Edge fade 0-10, fixed or a whole-number min_max range rolled per decal.
static void ParseDecalEdgeFade(ScriptParser::Parser &parser, const std::string &value,
                               World::TChainFXConfig &fx)
{
    fx.ground_decal_edge_fade = 0.0f;
    fx.ground_decal_edge_fade_min = -1;
    fx.ground_decal_edge_fade_max = -1;

    if ( value.find('_') != std::string::npos )
    {
        // Ranged edge fade uses whole 0..10 steps so textured decals reuse a small
        // bounded set of baked fade textures.
        int fadeMin = 0;
        int fadeMax = 0;
        if ( World::ParseIntRangeValue(value, fadeMin, fadeMax) )
        {
            fx.ground_decal_edge_fade_min = std::max(0, std::min(fadeMin, 10));
            fx.ground_decal_edge_fade_max = std::max(0, std::min(fadeMax, 10));
            fx.ground_decal_edge_fade = (float)fx.ground_decal_edge_fade_min;
        }
        else
            ypa_log_out("WARNING: invalid new_decal edge_fade range '%s', using 0\n", value.c_str());
        return;
    }

    size_t parsed = 0;
    const float number = parser.stof(value, &parsed);
    if ( parsed == value.size() && std::isfinite(number) )
        fx.ground_decal_edge_fade = std::max(0.0f, std::min(number, 10.0f));
}

// Manual diameter, in world units. Zero suppresses the decal.
static bool ParseDecalSizeRange(const std::string &value, float &minValue, float &maxValue)
{
    float parsedMin = 0.0f;
    float parsedMax = 0.0f;
    if ( !World::ParseFloatRangeValue(value, parsedMin, parsedMax) ||
         !std::isfinite(parsedMin) || !std::isfinite(parsedMax) ||
         parsedMin < 0.0f || parsedMax < 0.0f ||
         parsedMin > 12000.0f || parsedMax > 12000.0f )
        return false;

    minValue = std::min(parsedMin, parsedMax);
    maxValue = std::max(parsedMin, parsedMax);
    return true;
}

// Invalid values disable the decal without changing its linked profile.
void ParseDecalSizeOverride(const char *key, const std::string &value,
                            TDecalSizeOverride &out)
{
    float minValue = 0.0f;
    float maxValue = 0.0f;
    if ( ParseDecalSizeRange(value, minValue, maxValue) )
    {
        out.defined = true;
        out.size_min = minValue;
        out.size_max = maxValue;
        return;
    }

    out = TDecalSizeOverride();
    ypa_log_out("WARNING: invalid %s '%s'; decal disabled\n", key, value.c_str());
}

bool DecalProfileParser::IsScope(ScriptParser::Parser &parser,
                                 const std::string &word,
                                 const std::string &opt)
{
    if ( StriCmp(word, "new_decal") )
        return false;

    _active = false;
    _profileID = -1;

    if ( opt.empty() )
    {
        ypa_log_out("WARNING: new_decal requires a numeric id; profile ignored.\n");
        return true;
    }

    const long value = parser.stol(opt, nullptr, 0);
    if ( value < 0 || value > std::numeric_limits<int32_t>::max() )
    {
        ypa_log_out("WARNING: new_decal id '%s' is invalid; profile ignored.\n", opt.c_str());
        return true;
    }

    _profileID = (int32_t)value;
    _work = TGroundDecalProfile();
    _work.profile_id = _profileID;
    _work.fx.mode = TChainFXConfig::MODE_GROUND_DECAL;
    _durationValid = false;
    _fadeIn = 0;
    _fadeOut = 0;
    _active = true;
    return true;
}

void DecalProfileParser::Finish()
{
    TChainFXConfig &fx = _work.fx;

    if ( !_durationValid || (!fx.ground_decal_permanent && fx.duration <= 0) ||
         fx.ground_decal_tint.a <= 0.0f )
    {
        ypa_log_out("WARNING: new_decal %d needs a valid duration and a tint alpha above 0; profile ignored.\n",
                    _profileID);
        return;
    }

    fx.ground_decal_random_rotation = fx.random_rotation;
    fx.fade_out = fx.ground_decal_permanent ? 0 : std::min(_fadeOut, fx.duration);
    fx.fade_in = fx.ground_decal_permanent ? _fadeIn : std::min(_fadeIn, fx.duration - fx.fade_out);

    if ( _profiles.find(_profileID) != _profiles.end() )
        ypa_log_out("WARNING: new_decal %d is defined more than once; replacing the previous profile.\n", _profileID);

    _work.valid = true;
    _profiles[_profileID] = _work;
}

int DecalProfileParser::Handle(ScriptParser::Parser &parser,
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

    TChainFXConfig &fx = _work.fx;

    if ( !StriCmp(p1, "name") )
        _work.name = p2;
    else if ( !StriCmp(p1, "texture") )
        fx.ground_decal_texture = p2;
    else if ( !StriCmp(p1, "stretch_xz") )
    {
        std::string axis[2];
        if ( ReadTuple(p1, p2, 2, axis) )
        {
            ParseDecalStretch("stretch_xz", axis[0], fx.ground_decal_stretch_x,
                              fx.ground_decal_stretch_x_min, fx.ground_decal_stretch_x_max);
            ParseDecalStretch("stretch_xz", axis[1], fx.ground_decal_stretch_z,
                              fx.ground_decal_stretch_z_min, fx.ground_decal_stretch_z_max);
        }
    }
    else if ( ParseTintParam(parser, "tint", p1, p2, fx.ground_decal_tint, true) )
    {
    }
    else if ( !StriCmp(p1, "random_rotation") )
        fx.random_rotation = p2 == "1";
    else if ( !StriCmp(p1, "edge_fade") )
        ParseDecalEdgeFade(parser, p2, fx);
    else if ( !StriCmp(p1, "duration") )
    {
        if ( !StriCmp(p2, "permanent") )
        {
            fx.duration = 0;
            fx.ground_decal_permanent = true;
            _durationValid = true;
        }
        else
        {
            size_t parsed = 0;
            fx.duration = parser.stol(p2, &parsed, 0);
            fx.ground_decal_permanent = false;
            _durationValid = parsed == p2.size() && fx.duration > 0;
        }
    }
    else if ( !StriCmp(p1, "fade_inout") )
    {
        std::string fade[2];
        if ( ReadTuple(p1, p2, 2, fade) )
        {
            _fadeIn = NonNegativeFiniteMilliseconds(parser, fade[0]);
            _fadeOut = NonNegativeFiniteMilliseconds(parser, fade[1]);
        }
    }
    else if ( ParseGroundDecalSmokeParam(parser, p1, p2, fx.ground_decal_smoke) )
    {
    }
    else
        return ScriptParser::RESULT_UNKNOWN;

    return ScriptParser::RESULT_OK;
}

int ParseVehicleChainFXBlock(ScriptParser::Parser &parser, TVhclProto *vhcl)
{
    return ParseChainFXBlock(parser,
                             &vhcl->chain_fx,
                             CHAIN_FX_VEHICLE);
}

int ParseWeaponChainFXBlock(ScriptParser::Parser &parser, TWeapProto *wpn)
{
    return ParseChainFXBlock(parser,
                             &wpn->chain_fx,
                             CHAIN_FX_WEAPON);
}

int ParseSuperItemChainFXBlock(ScriptParser::Parser &parser, TSuperItemProfile *profile)
{
    return ParseChainFXBlock(parser,
                             &profile->detonate_chain_fx,
                             CHAIN_FX_SUPERITEM);
}

bool IsMimicVehicleShellParam(const std::string &p1)
{
    return !StriCmp(p1, "model") ||
           !StriCmp(p1, "name") ||
           !StriCmp(p1, "type_icon") ||
           !StriCmp(p1, "mimic_energy_cost") ||
           !StriCmp(p1, "mimic_tint") ||
           !StriCmp(p1, "snd_mimic_sample") ||
           !StriCmp(p1, "snd_mimic_pitch") ||
           !StriCmp(p1, "snd_mimic_volume") ||
           !StriCmp(p1, "job_fightrobo") ||
           !StriCmp(p1, "job_fightflyer") ||
           !StriCmp(p1, "job_fighthelicopter") ||
           !StriCmp(p1, "job_fighttank") ||
           !StriCmp(p1, "job_fightplane") ||
           !StriCmp(p1, "job_fightcruiser") ||
           !StriCmp(p1, "job_fightglider") ||
           !StriCmp(p1, "job_fightzeppelin") ||
           !StriCmp(p1, "job_fightufo") ||
           !StriCmp(p1, "job_fightcar") ||
           !StriCmp(p1, "job_fightgun") ||
           !StriCmp(p1, "job_conquer") ||
           !StriCmp(p1, "job_reconnoitre") ||
           !StriCmp(p1, "at_death_spawn_units") ||
           !StriCmp(p1, "at_death_spawn_vehicle") ||
           !StriCmp(p1, "at_death_spawn_count") ||
           !StriCmp(p1, "at_death_spawn_random_pos") ||
           !StriCmp(p1, "at_death_spawn_instant") ||
           !StriCmp(p1, "at_death_spawn_immunity_time") ||
           !StriCmp(p1, "at_death_push_force") ||
           !StriCmp(p1, "at_death_push_radius") ||
           !StriCmp(p1, "at_death_push_falloff") ||
           !StriCmp(p1, "at_death_energy") ||
           !StriCmp(p1, "at_death_energy_radius") ||
           !StriCmp(p1, "at_death_energy_falloff");
}

float ParseFiniteCollisionFloat(ScriptParser::Parser &parser,
                                const std::string &value)
{
    float parsed = parser.stof(value, 0);
    return std::isfinite(parsed) ? parsed : 0.0f;
}

bool ParseCompoundCollisionParameter(
 ScriptParser::Parser &parser, const std::string &p1,
 const std::string &p2, rbcolls &coll, int32_t &collID)
{
    if ( !StriCmp(p1, "coll_num") )
    {
        int count = parser.stol(p2, nullptr, 0);
        count = std::max(0, std::min(count, (int)UNIT_COLL_MAX_COUNT));
        coll.roboColls.resize(count);
        if ( collID >= count )
            collID = count - 1;
        return true;
    }

    if ( !StriCmp(p1, "coll_act") )
    {
        collID = parser.stol(p2, nullptr, 0);
        collID = std::max(0, std::min(collID, (int)UNIT_COLL_MAX_COUNT - 1));
        if ( (size_t)collID >= coll.roboColls.size() )
            coll.roboColls.resize(collID + 1);
        return true;
    }

    if ( !StriCmp(p1, "coll_radius") || !StriCmp(p1, "coll_xyz") )
    {
        if ( collID < 0 || (size_t)collID >= coll.roboColls.size() )
            return true;

        TRoboColl &sphere = coll.roboColls.at(collID);
        if ( !StriCmp(p1, "coll_radius") )
        {
            sphere.robo_coll_radius = std::max(0.0f, ParseFiniteCollisionFloat(parser, p2));
            return true;
        }

        std::string pos[3];
        if ( ReadTuple(p1, p2, 3, pos) )
        {
            sphere.coll_pos.x = ParseFiniteCollisionFloat(parser, pos[0]);
            sphere.coll_pos.y = ParseFiniteCollisionFloat(parser, pos[1]);
            sphere.coll_pos.z = ParseFiniteCollisionFloat(parser, pos[2]);
        }
        return true;
    }

    return false;
}

}
}
