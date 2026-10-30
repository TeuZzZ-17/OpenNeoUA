// Prototype scripts, status profile links, super item profiles and display names of vehicles, weapons, buildings and levels.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include "common/includes.h"
#include "game/yw.h"
#include "game/database_session.h"
#include "game/yw_internal.h"
#include "nucleus/loaders.h"
#include "gui/button.h"
#include "nucleus/amesh.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "platform/windp.h"
#include "platform/wav.h"
#include "game/yw_net.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "locale/locale.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include "world/energyfx.h"
#include "world/tools.h"
#include "game/yw_impl.h"

bool yw_ParseOptionalInt(std::string s, int32_t *out)
{
    s = StrTrim(s);
    if (s.empty())
        return false;

    try
    {
        size_t pos = 0;
        int32_t v = (int32_t)std::stol(s, &pos, 0);
        size_t rest = s.find_first_not_of(" \t\r\n", pos);
        if (rest != std::string::npos)
            return false;

        *out = v;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool yw_ParseOptionalBool(std::string s, bool *out)
{
    s = StrTrim(s);
    if (s.empty())
        return false;

    *out = StrGetBool(s);
    return true;
}

bool yw_IsUsableGameplayName(const std::string &name)
{
    if ( name == "<NO NAME>" )
        return false;

    for (unsigned char ch : name)
    {
        if ( !std::isspace(ch) )
            return true;
    }

    return false;
}

std::string NC_STACK_ypaworld::GetVehicleName(uint32_t id) const
{
    return GetVehicleName(_vhclProtos[id]);
}

std::string NC_STACK_ypaworld::GetVehicleName(const World::TVhclProto &proto) const
{
    if ( yw_IsUsableGameplayName(proto.name) )
        return proto.name;

    if (proto.Index != -1)
        return Locale::Text::VehicleName(proto.Index, proto.name);

    return proto.name;
}

std::string NC_STACK_ypaworld::GetBuildingName(uint32_t id, bool net) const
{
    return GetBuildingName(_buildProtos[id], net);
}

std::string NC_STACK_ypaworld::GetBuildingName(const World::TBuildingProto &proto, bool net) const
{
    if ( yw_IsUsableGameplayName(proto.Name) )
        return proto.Name;

    if (proto.Index != -1)
    {
        if (net)
            return Locale::Text::NetBuildingName(proto.Index, proto.Name);
        else
            return Locale::Text::BuildingName(proto.Index, proto.Name);
    }

    return proto.Name;
}

std::string NC_STACK_ypaworld::GetLevelName(uint32_t id) const
{
    const TMapRegionInfo &region = _globalMapRegions.MapRegions[id];

    if ( yw_IsUsableGameplayName(region.MapName) )
        return region.MapName;

    return Locale::Text::LevelName(id, region.MapName);
}

std::string NC_STACK_ypaworld::GetLevelName(const TLevelInfo &lvl) const
{
    if ( yw_IsUsableGameplayName(lvl.MapName) )
        return lvl.MapName;

    return Locale::Text::LevelName(lvl.LevelID, lvl.MapName);
}

std::string NC_STACK_ypaworld::ResolveGameplayVehicleName(uint32_t id) const
{
    if ( id < _vhclProtos.size() )
        return ResolveGameplayVehicleName(_vhclProtos[id]);

    return std::string();
}

std::string NC_STACK_ypaworld::ResolveGameplayVehicleName(const World::TVhclProto &proto) const
{
    if ( yw_IsUsableGameplayName(proto.name) )
        return proto.name;

    return GetVehicleName(proto);
}

std::string NC_STACK_ypaworld::ResolveGameplayVehicleName(const NC_STACK_ypabact *bact, const World::TVhclProto &proto) const
{
    if ( bact && yw_IsUsableGameplayName(bact->_gunDisplayName) )
        return bact->_gunDisplayName;

    return ResolveGameplayVehicleName(proto);
}

std::string NC_STACK_ypaworld::ResolveGameplayWeaponName(uint32_t id) const
{
    if ( id < _weaponProtos.size() )
        return ResolveGameplayWeaponName(_weaponProtos[id]);

    return std::string();
}

std::string NC_STACK_ypaworld::ResolveGameplayWeaponName(const World::TWeapProto &proto) const
{
    if ( yw_IsUsableGameplayName(proto.name) )
        return proto.name;

    return std::string();
}

std::string NC_STACK_ypaworld::ResolveGameplayBuildingName(uint32_t id, bool net) const
{
    if ( id < _buildProtos.size() )
        return ResolveGameplayBuildingName(_buildProtos[id], net);

    return std::string();
}

std::string NC_STACK_ypaworld::ResolveGameplayBuildingName(const World::TBuildingProto &proto, bool net) const
{
    if ( yw_IsUsableGameplayName(proto.Name) )
        return proto.Name;

    return GetBuildingName(proto, net);
}

bool NC_STACK_ypaworld::LoadSpectatorVehicleProto()
{
    std::string spectatorScript = "data:scripts/spectator_vehicle.cfg";
    if ( !uaFileExist(spectatorScript) )
        spectatorScript = "data:scripts/spectator_vehicle.txt";

    if ( !uaFileExist(spectatorScript) )
    {
        ypa_log_out("WARNING: spectator vehicle file Data/Scripts/spectator_vehicle.cfg is missing. Spectator mode disabled for this level.\n");
        _spectatorVehicleProtoID = -1;
        return false;
    }

    int32_t targetID = _spectatorVehicleProtoID;
    if ( targetID <= 0 || targetID >= (int32_t)_vhclProtos.size() )
    {
        targetID = -1;
        for ( int32_t i = (int32_t)_vhclProtos.size() - 1; i > 0; --i )
        {
            if ( _vhclProtos[i].Index < 0 || _vhclProtos[i].model_id == BACT_TYPES_NOPE )
            {
                targetID = i;
                break;
            }
        }
    }

    if ( targetID < 0 )
    {
        ypa_log_out("WARNING: no free vehicle prototype slot is available for spectator mode.\n");
        _spectatorVehicleProtoID = -1;
        return false;
    }

    World::TVhclProto previous = _vhclProtos[targetID];
    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");
    ScriptParser::HandlersList parsers {
        new World::Parsers::VhclProtoParser(this, targetID)
    };

    bool parsed = ScriptParser::ParseFile(spectatorScript, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);
    Common::Env.SetPrefix("rsrc", oldRsrc);
    if ( parsed )
    {
        World::TVhclProto &spectatorProto = _vhclProtos[targetID];
        spectatorProto.buff = World::TVehicleBuffConfig();
        auto buffIt = _buffProfiles.find(spectatorProto.buff_id);
        if ( buffIt != _buffProfiles.end() )
            spectatorProto.buff = buffIt->second;
    }

    if ( !parsed ||
         _vhclProtos[targetID].Index != targetID ||
         _vhclProtos[targetID].model_id != BACT_TYPES_UFO ||
         !_vhclProtos[targetID].hidden ||
         !_vhclProtos[targetID].buff.valid ||
         !_vhclProtos[targetID].buff.invulnerable )
    {
        _vhclProtos[targetID] = previous;
        _spectatorVehicleProtoID = -1;
        ypa_log_out("WARNING: spectator vehicle file %s must define a valid model = ufo with hidden = yes and a buff_id linked to a Buff profile with invulnerable = 1. Spectator mode disabled for this level.\n", spectatorScript.c_str());
        return false;
    }

    _vhclProtos[targetID].disable_enable_bitmask = 0;
    _spectatorVehicleProtoID = targetID;
    ypa_log_out("Loaded OpenNeoUA spectator vehicle from %s into runtime prototype slot %d.\n", spectatorScript.c_str(), targetID);
    return true;
}

bool NC_STACK_ypaworld::LoadProtosScript(const std::string &filename)
{
    std::string buf = Common::Env.SetPrefix("rsrc", "data:");

    ScriptParser::HandlersList parsers {
        new World::Parsers::BuffProfileParser(&_buffProfiles),
        new World::Parsers::DebuffProfileParser(&_debuffProfiles),
        new World::Parsers::DecalProfileParser(&_decalProfiles),
        new World::Parsers::SuperItemProfileParser(&_superItemDefs),
        new World::Parsers::AtmosphericFXProfileParser(&_cloudProfiles),
        new World::Parsers::VhclProtoParser(this),
        new World::Parsers::WeaponProtoParser(this),
        new World::Parsers::BuildProtoParser(this)
    };

    bool res = ScriptParser::ParseFile(filename, parsers,
        ScriptParser::FLAG_NO_SCOPE_SKIP | ScriptParser::FLAG_SKIP_MISSING_INCLUDES);
    Common::Env.SetPrefix("rsrc", buf);

    if ( res )
        ResolveStatusProfileLinks();

    return res;
}

void NC_STACK_ypaworld::ResolveStatusProfileLinks()
{
    for (World::TVhclProto &proto : _vhclProtos)
    {
        if ( proto.buff_id < 0 )
        {
            if ( proto.buff.valid )
                proto.buff = World::TVehicleBuffConfig();
            continue;
        }

        auto it = _buffProfiles.find(proto.buff_id);
        if ( it == _buffProfiles.end() )
        {
            if ( proto.buff.valid )
                proto.buff = World::TVehicleBuffConfig();
            ypa_log_out("WARNING: vehicle %d references missing buff_id %d; Buff disabled.\n",
                        proto.Index, proto.buff_id);
            continue;
        }

        if ( proto.buff.valid &&
             proto.buff.profile_id == it->second.profile_id &&
             proto.buff.revision == it->second.revision )
            continue;

        proto.buff = it->second;
    }

    auto resolveWeaponDebuff = [this](size_t weaponId, int32_t profileId,
                                      int32_t firstValidId, const char *parameterName,
                                      World::TWeaponDebuffConfig &resolved)
    {
        if ( profileId < firstValidId )
        {
            if ( resolved.valid )
            {
                resolved.tick_snd.ClearSounds();
                resolved = World::TWeaponDebuffConfig();
            }
            return;
        }

        auto it = _debuffProfiles.find(profileId);
        if ( it == _debuffProfiles.end() )
        {
            if ( resolved.valid )
            {
                resolved.tick_snd.ClearSounds();
                resolved = World::TWeaponDebuffConfig();
            }
            ypa_log_out("WARNING: weapon %u references missing %s %d; Debuff disabled.\n",
                        (unsigned)weaponId, parameterName, profileId);
            return;
        }

        if ( resolved.valid &&
             resolved.profile_id == it->second.profile_id &&
             resolved.revision == it->second.revision )
            return;

        if ( resolved.valid )
            resolved.tick_snd.ClearSounds();
        resolved = it->second;
    };

    for (size_t i = 0; i < _weaponProtos.size(); ++i)
    {
        World::TWeapProto &proto = _weaponProtos[i];

        // debuff_id keeps its legacy rule where ID 0 is valid. The delayed
        // attachment parameter is explicitly opt-in, so 0 means disabled.
        resolveWeaponDebuff(i, proto.debuff_id, 0, "debuff_id", proto.debuff);
        resolveWeaponDebuff(i, proto.delay_time_debuff_id, 1,
                            "delay_time_debuff_id", proto.delay_time_debuff);
    }

    ResolveDecalProfileLinks();
}

// Builds the ground decals every vehicle (destroyed), weapon (impact_world) and MGUN
// (impact_world) spawns from its ground_decal_id / mgun_ground_decal_id. The decal size is not
// resolved here: it depends on the visual model of the unit and is measured when the decal spawns.
// Resolved decals are rebuilt from scratch every time, so changed profiles are always picked up.
void NC_STACK_ypaworld::ResolveDecalProfileLinks()
{
    const auto isGroundDecal = [](const World::TChainFXConfig &chain)
    {
        return chain.mode == World::TChainFXConfig::MODE_GROUND_DECAL;
    };

    const auto findProfile = [this](int32_t id, const char *owner, size_t index, const char *key)
        -> const World::TGroundDecalProfile *
    {
        if ( id < 0 )
            return nullptr;

        auto it = _decalProfiles.find(id);
        if ( it == _decalProfiles.end() )
        {
            ypa_log_out("WARNING: %s %u references missing %s %d; decal disabled.\n",
                        owner, (unsigned)index, key, id);
            return nullptr;
        }

        return &it->second;
    };

    for (World::TVhclProto &proto : _vhclProtos)
    {
        proto.chain_fx.erase(std::remove_if(proto.chain_fx.begin(), proto.chain_fx.end(), isGroundDecal),
                             proto.chain_fx.end());
        proto.mgun_decal_enable = false;
        proto.mgun_decal = World::TChainFXConfig();

        if ( const World::TGroundDecalProfile *profile = findProfile(proto.ground_decal_id, "vehicle",
                                                                       (size_t)proto.Index, "ground_decal_id") )
            proto.chain_fx.push_back(World::ResolveGroundDecal(
                *profile, World::TChainFXConfig::TRIGGER_DESTROYED, proto.ground_decal_size));

        // MGUN hits are sized from the vehicle that fires them.
        if ( const World::TGroundDecalProfile *profile = findProfile(proto.mgun_ground_decal_id, "vehicle",
                                                                       (size_t)proto.Index, "mgun_ground_decal_id") )
        {
            proto.mgun_decal = World::ResolveGroundDecal(
                *profile, World::TChainFXConfig::TRIGGER_IMPACT_WORLD, proto.mgun_ground_decal_size);
            proto.mgun_decal_enable = true;
        }
    }

    for (size_t i = 0; i < _weaponProtos.size(); ++i)
    {
        World::TWeapProto &proto = _weaponProtos[i];
        proto.chain_fx.erase(std::remove_if(proto.chain_fx.begin(), proto.chain_fx.end(), isGroundDecal),
                             proto.chain_fx.end());

        if ( const World::TGroundDecalProfile *profile = findProfile(proto.ground_decal_id, "weapon", i, "ground_decal_id") )
            proto.chain_fx.push_back(World::ResolveGroundDecal(
                *profile, World::TChainFXConfig::TRIGGER_IMPACT_WORLD, proto.ground_decal_size));
    }
}

bool NC_STACK_ypaworld::ProtosInit()
{
    _speechEventVariants.clear();
    _buffProfiles.clear();
    _debuffProfiles.clear();
    _decalProfiles.clear();
    _superItemDefs.clear();
    _cloudProfiles.clear();
    _vhclProtos.resize(NUM_VHCL_PROTO);
    _weaponProtos.resize(NUM_WEAPON_PROTO);
    _buildProtos.resize(NUM_BUILD_PROTO);

    _roboProtos.reserve(NUM_ROBO_PROTO);
    _roboProtos.resize(1);

    if ( !LoadProtosScript(_initScriptFilePath) )
        return false;

    DatabaseSession scripts;
    if(!scripts.LoadDefinitions(*this))return false;
    _vhclProtos.swap(scripts.definitions->_vhclProtos);
    _weaponProtos.swap(scripts.definitions->_weaponProtos);
    _buildProtos.swap(scripts.definitions->_buildProtos);
    ResolveStatusProfileLinks();
    return true;
}

// Copies one SuperItem definition of SuperItems.cfg for a level and checks what the level
// needs from it. Returns false when the bomb must use the vanilla behaviour.
bool NC_STACK_ypaworld::PrepareSuperItemProfile(int32_t id, World::TSuperItemProfile &outProfile)
{
    outProfile = World::TSuperItemProfile();

    const auto definition = _superItemDefs.find(id);
    if ( definition == _superItemDefs.end() )
    {
        ypa_log_out("WARNING: SuperItem profile %d is not defined in the scripts.\n", id);
        return false;
    }

    World::TSuperItemProfile profile = definition->second;
    profile.valid = true;
    if ( profile.name.empty() )
        profile.name = "superitem_" + std::to_string(id);

    profile.debuff = World::TWeaponDebuffConfig();
    if ( profile.debuff_id >= 0 )
    {
        auto debuffIt = _debuffProfiles.find(profile.debuff_id);
        if ( debuffIt != _debuffProfiles.end() )
            profile.debuff = debuffIt->second;
        else
            ypa_log_out("WARNING: SuperItem profile '%s' references missing debuff_id %d; Debuff disabled.\n",
                        profile.name.c_str(), profile.debuff_id);
    }

    if ( profile.wave_vp <= 0 && profile.wave_3ds.empty() && profile.wave_base.empty() )
    {
        ypa_log_out("WARNING: SuperItem profile '%s' has no wave_vp, wave_base or wave_3ds; using vanilla fallback.\n",
                    profile.name.c_str());
        return false;
    }

    const bool hasWaveSpeedTrio =
        profile.has_wave_start_speed &&
        profile.has_wave_speed_ramp_time &&
        profile.has_wave_end_speed;
    const bool waveSpeedTrioValid =
        hasWaveSpeedTrio &&
        std::isfinite(profile.wave_start_speed) && profile.wave_start_speed >= 0.0f &&
        std::isfinite(profile.wave_speed_ramp_time) && profile.wave_speed_ramp_time >= 0.0f &&
        std::isfinite(profile.wave_end_speed) && profile.wave_end_speed > 0.0f;

    if ( !waveSpeedTrioValid )
    {
        ypa_log_out("WARNING: SuperItem profile '%s' requires valid wave_start_speed, wave_speed_ramp_time and wave_end_speed; using vanilla fallback.\n",
                    profile.name.c_str());
        return false;
    }

    for (auto chainIt = profile.detonate_chain_fx.begin();
         chainIt != profile.detonate_chain_fx.end(); )
    {
        World::TChainFXConfig &chain = *chainIt;
        chain.visuals.erase(
            std::remove_if(chain.visuals.begin(), chain.visuals.end(),
                           [](const World::TChainFXVisual &visual) {
                               return visual.vp <= 0 && visual.mesh3ds.empty() && visual.basePath.empty();
                           }),
            chain.visuals.end());

        if ( chain.mode != World::TChainFXConfig::MODE_VISUAL ||
             chain.trigger != World::TChainFXConfig::TRIGGER_DETONATE ||
             chain.duration <= 0 || chain.visuals.empty() ||
             !std::isfinite(chain.start_size) || !std::isfinite(chain.end_size) ||
             !std::isfinite(chain.offset_min.x) || !std::isfinite(chain.offset_min.y) ||
             !std::isfinite(chain.offset_min.z) ||
             !std::isfinite(chain.offset_max.x) || !std::isfinite(chain.offset_max.y) ||
             !std::isfinite(chain.offset_max.z) ||
             !std::isfinite(chain.spin.x) || !std::isfinite(chain.spin.y) ||
             !std::isfinite(chain.spin.z) )
        {
            ypa_log_out("WARNING: invalid detonation Chain FX ignored in SuperItem profile '%s'.\n",
                        profile.name.c_str());
            chainIt = profile.detonate_chain_fx.erase(chainIt);
        }
        else
            ++chainIt;
    }

    // Keep loaded sample ownership out of temporary profiles. TVhclSound owns
    // raw sample objects, so moving a profile after LoadSamples() would copy the
    // owning MainSample pointer and leave the destination with a dangling sample.
    // InitSuperItems() loads sounds only after every profile is in stable storage.
    outProfile = std::move(profile);
    ypa_log_out("Prepared SuperItem profile %d '%s'.\n", id, outProfile.name.c_str());
    return true;
}

