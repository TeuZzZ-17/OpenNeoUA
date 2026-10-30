// Voice messages of the in-game GUI.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <iterator>
#include <array>
#include <algorithm>
#include <map>
#include <vector>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "nucleus/loaders.h"
#include "gui/font.h"
#include "game/yparobo.h"
#include "game/ypagun.h"
#include "platform/windp.h"
#include "world/analyzer.h"
#include "system/inivals.h"
#include "game/yw_game_ui_internal.h"

namespace
{
std::map<std::string, bool> g_speechEventLoggedUsed;
std::map<std::string, bool> g_speechEventLoggedFallback;

// Logs a speech event message only once per key.
void SpeechEventLogOnce(std::map<std::string, bool> &cache, const std::string &key, const char *fmt, const std::string &path)
{
    if ( cache[key] )
        return;

    cache[key] = true;
    ypa_log_out(fmt, path.c_str());
}

// Picks one of the variants of a speech event file (cached per path).
std::string SpeechEventFindFile(std::map<std::string, World::Speech::Variants> &cache,
                                const std::string &path)
{
    std::string authored = path;
    if ( authored.compare(0, 5, "rsrc:") == 0 ) authored.erase(0, 5);
    std::string normalized;
    if ( !uaNormalizeDataAssetPath(authored, &normalized, true) ) return "";
    const std::string relative = normalized.substr(5);
    auto cached = cache.find(normalized);
    if ( cached == cache.end() )
    {
        World::Speech::Variants variants;
        const std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");
        if ( relative.size() >= 4 && !StriCmp(relative.substr(relative.size() - 4), ".wav") )
        {
            if ( uaFileExist("rsrc:" + relative) ) variants.Files.push_back(relative);
        }
        else
        {
            const size_t slash = relative.find_last_of('/');
            const std::string directory = slash == std::string::npos ? "" : relative.substr(0, slash + 1);
            const std::string stem = slash == std::string::npos ? relative : relative.substr(slash + 1);
            FSMgr::DirIter files = uaOpenDir("rsrc:" + directory);
            for (FSMgr::iNode *file = files.getNext(); file; file = files.getNext())
            {
                if ( file->getType() == FSMgr::iNode::NTYPE_FILE &&
                     World::Speech::IsVariantFilename(file->getName(), stem) )
                    variants.Files.push_back(directory + file->getName());
            }
            std::sort(variants.Files.begin(), variants.Files.end());
            if ( variants.Files.empty() && uaFileExist("rsrc:" + relative + ".wav") )
                variants.Files.push_back(relative + ".wav");
        }
        Common::Env.SetPrefix("rsrc", oldRsrc);
        cached = cache.emplace(normalized, std::move(variants)).first;
    }
    return World::Speech::ChooseVariant(cached->second, static_cast<unsigned int>(rand()));
}
}

bool NC_STACK_ypaworld::VoiceMessagePlayResourceFile(const std::string &filename, NC_STACK_ypabact *unit, int priority, bool ignoreTimeScale)
{
    if ( IsSpectatorControlled() )
        return false;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    if ( !uaFileExist("rsrc:" + filename) )
    {
        Common::Env.SetPrefix("rsrc", oldRsrc);
        return false;
    }

    NC_STACK_wav *v23 = Nucleus::CInit<NC_STACK_wav>( {{NC_STACK_rsrc::RSRC_ATT_NAME, filename}} );

    Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( !v23 || !v23->GetSampleData() || !v23->GetSampleData()->Data ||
         v23->GetSampleData()->bufsz <= 0 || v23->GetSampleData()->SampleRate <= 0 )
    {
        if ( v23 ) v23->Delete();
        return false;
    }

    _voiceMessage.Reset();

    _voiceMessage.Carrier.Sounds[0].Pitch = 0;
    _voiceMessage.Carrier.Sounds[0].Volume = priority + 500;
    _voiceMessage.Carrier.Sounds[0].PriorityBias = 512;
    _voiceMessage.Carrier.Sounds[0].IgnoreTimeScale = ignoreTimeScale;
    _voiceMessage.Carrier.Sounds[0].PSample = v23->GetSampleData();

    _voiceMessage.Priority = priority;
    _voiceMessage.Sample = v23;
    _voiceMessage.Unit = unit;

    VoiceMessageCalcPositionToUnit();

    _voiceMessage.Carrier.Vector = _userUnit->_fly_dir * _userUnit->_fly_dir_length;
    SFXEngine::SFXe.startSound(&_voiceMessage.Carrier, 0);

    return true;
}

bool NC_STACK_ypaworld::VoiceMessagePlayFile(const std::string &flname, NC_STACK_ypabact *unit, int priority, bool ignoreTimeScale)
{
    if ( IsSpectatorControlled() )
        return false;

    _voiceMessage.Reset();

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    std::string filename = "sounds/speech/";
    std::string locName = Locale::Text::GetLocaleName();

    if ( !locName.empty() )
        filename += locName + "/";
    else
        filename += "language/";

    filename += flname;

    if ( !uaFileExist("rsrc:" + filename) )
    {
        filename = "sounds/speech/language/";
        filename += flname;
    }

    Common::Env.SetPrefix("rsrc", oldRsrc);
    return VoiceMessagePlayResourceFile(filename, unit, priority, ignoreTimeScale);
}

void NC_STACK_ypaworld::VoiceMessagePlayMsg(NC_STACK_ypabact *unit, int priority, int msgID, bool ignoreTimeScale)
{
    if ( IsSpectatorControlled() )
        return;

    if ( _voiceMessage.Priority < 0 || _voiceMessage.Priority < priority )
    {

        struct TMsgVals
        {
            int v1 = 0;
            int type = 0;
            int v3 = 0;
            int v4 = 0;
            int num = 0;

            TMsgVals() {};

            TMsgVals(int p1, int t, int p2, int p3, int n)
            : v1(p1), type(t), v3(p2), v4(p3), num(n) {};

        } msgvals;

        switch ( msgID )
        {
        case 1:
            msgvals = TMsgVals(1, 0, 1, 2, 2);
            break;
        case 5:
            msgvals = TMsgVals(1, 0, 3, 2, 2);
            break;
        case 6:
            msgvals = TMsgVals(1, 0, 2, 3, 1);
            break;
        case 7:
            msgvals = TMsgVals(1, 0, 2, 2, 2);
            break;
        case 8:
            msgvals = TMsgVals(1, 0, 2, 6, 2);
            break;
        case 9:
            msgvals = TMsgVals(1, 0, 2, 5, 1);
            break;
        case 10:
            msgvals = TMsgVals(1, 0, 2, 1, 1);
            break;
        case 11:
            msgvals = TMsgVals(1, 0, 3, 3, 1);
            break;
        case 12:
            msgvals = TMsgVals(2, 1, 2, 6, 2);
            break;
        case 13:
            msgvals = TMsgVals(2, 1, 2, 1, 1);
            break;
        case 14:
            msgvals = TMsgVals(1, 0, 1, 1, 2);
            break;
        case 15:
            msgvals = TMsgVals(1, 0, 1, 3, 2);
            break;
        case 16:
            msgvals = TMsgVals(1, 0, 1, 4, 2);
            break;
        case 17:
            msgvals = TMsgVals(1, 0, 1, 5, 1);
            break;
        case 18:
            msgvals = TMsgVals(1, 0, 2, 4, 1);
            break;
        case 19:
            msgvals = TMsgVals(1, 0, 3, 1, 1);
            break;
        case 22:
            msgvals = TMsgVals(1, 0, 1, 6, 1);
            break;
        case 23:
            msgvals = TMsgVals(2, 1, 1, 1, 1);
            break;
        case 24:
            msgvals = TMsgVals(2, 1, 1, 2, 1);
            break;
        case 25:
            msgvals = TMsgVals(2, 1, 1, 3, 1);
            break;
        case 26:
            msgvals = TMsgVals(2, 1, 1, 4, 1);
            break;
        case 27:
            msgvals = TMsgVals(2, 1, 1, 5, 1);
            break;
        case 28:
            msgvals = TMsgVals(2, 1, 1, 6, 1);
            break;
        case 29:
            msgvals = TMsgVals(2, 1, 1, 7, 1);
            break;
        case 30:
            msgvals = TMsgVals(2, 1, 1, 8, 1);
            break;
        case 31:
            msgvals = TMsgVals(2, 2, 1, 8, 1);
            break;
        case 32:
            msgvals = TMsgVals(2, 3, 1, 8, 1);
            break;
        case 33:
            msgvals = TMsgVals(2, 1, 1, 9, 1);
            break;
        case 34:
            msgvals = TMsgVals(2, 2, 1, 9, 1);
            break;
        case 35:
            msgvals = TMsgVals(2, 3, 1, 9, 1);
            break;
        case 36:
            msgvals = TMsgVals(2, 1, 1, 10, 1);
            break;
        case 37:
            msgvals = TMsgVals(2, 2, 1, 10, 1);
            break;
        case 38:
            msgvals = TMsgVals(2, 3, 1, 10, 1);
            break;
        case 39:
            msgvals = TMsgVals(3, 0, 1, 7, 1);
            break;
        case 40:
            msgvals = TMsgVals(2, 1, 2, 3, 1);
            break;
        case 41:
            msgvals = TMsgVals(2, 1, 2, 4, 2);
            break;
        case 42:
            msgvals = TMsgVals(2, 1, 2, 5, 2);
            break;
        case 43:
            msgvals = TMsgVals(2, 1, 2, 7, 3);
            break;
        case 44:
            msgvals = TMsgVals(2, 1, 2, 8, 2);
            break;
        case 45:
            msgvals = TMsgVals(1, 0, 3, 4, 1);
            if ( !unit )
                return;
            break;
        case 46:
            msgvals = TMsgVals(2, 4, 1, 1, 1);
            break;
        case 47:
            msgvals = TMsgVals(3, 0, 0, 1, 1);
            priority++;
            break;
        case 48:
            msgvals = TMsgVals(3, 0, 0, 2, 2);
            priority += 2;
            break;
        case 49:
            msgvals = TMsgVals(3, 0, 0, 3, 3);
            priority += 3;
            break;
        case 50:
            msgvals = TMsgVals(3, 0, 0, 5, 1);
            priority++;
            break;
        case 51:
            msgvals = TMsgVals(3, 0, 0, 7, 1);
            priority += 3;
            break;
        case 52:
            msgvals = TMsgVals(3, 0, 0, 8, 1);
            priority += 4;
            break;
        case 53:
            msgvals = TMsgVals(3, 0, 0, 9, 1);
            priority += 5;
            break;
        case 54:
            msgvals = TMsgVals(3, 0, 1, 1, 1);
            priority++;
            break;
        case 55:
            msgvals = TMsgVals(3, 0, 1, 2, 1);
            priority += 2;
            break;
        case 56:
            msgvals = TMsgVals(3, 0, 1, 3, 1);
            priority += 3;
            break;
        case 59:
            msgvals = TMsgVals(3, 0, 1, 8, 1);
            priority++;
            break;
        case 60:
            msgvals = TMsgVals(3, 0, 0, 4, 1);
            priority += 4;
            break;
        case 61:
            msgvals = TMsgVals(3, 0, 1, 0, 1);
            priority += 6;
            break;
        case 63:
            msgvals = TMsgVals(3, 0, 3, 0, 1);
            priority += 13;
            break;
        case 64:
            msgvals = TMsgVals(3, 0, 0, 6, 1);
            priority += 2;
            break;
        case 65:
            msgvals = TMsgVals(3, 0, 1, 4, 1);
            priority += 4;
            break;
        case 66:
            msgvals = TMsgVals(3, 0, 1, 6, 1);
            priority += 5;
            break;
        case 67:
            msgvals = TMsgVals(2, 1, 1, 11, 1);
            break;
        case 68:
            msgvals = TMsgVals(2, 2, 1, 8, 1);
            break;
        case 69:
            msgvals = TMsgVals(2, 3, 1, 8, 1);
            break;
        case 70:
            msgvals = TMsgVals(2, 4, 1, 5, 1);
            break;
        case 71:
            msgvals = TMsgVals(2, 4, 1, 8, 1);
            break;
        case 72:
            msgvals = TMsgVals(2, 4, 1, 6, 1);
            break;
        case 73:
            msgvals = TMsgVals(2, 4, 1, 7, 1);
            break;
        case 74:
            msgvals = TMsgVals(2, 4, 1, 9, 1);
            break;
        case 75:
            msgvals = TMsgVals(2, 4, 1, 12, 1);
            break;
        case 76:
            msgvals = TMsgVals(2, 4, 1, 10, 1);
            break;
        case 77:
            msgvals = TMsgVals(2, 4, 1, 11, 1);
            break;
        case 78:
            msgvals = TMsgVals(2, 1, 1, 12, 1);
            break;
        case 79:
            msgvals = TMsgVals(2, 4, 1, 2, 1);
            break;
        case 80:
            msgvals = TMsgVals(2, 1, 1, 13, 1);
            break;
        case 81:
            msgvals = TMsgVals(2, 4, 1, 3, 1);
            break;
        case 82:
            msgvals = TMsgVals(2, 4, 1, 4, 1);
            break;
        case 83:
            msgvals = TMsgVals(2, 5, 1, 2, 1);
            break;
        case 84:
            msgvals = TMsgVals(2, 5, 1, 1, 1);
            break;
        case 85:
            msgvals = TMsgVals(2, 5, 1, 3, 1);
            break;
        case 86:
            msgvals = TMsgVals(2, 5, 1, 4, 1);
            break;
        case 87:
            msgvals = TMsgVals(2, 5, 1, 7, 1);
            break;
        case 88:
            msgvals = TMsgVals(2, 5, 1, 6, 1);
            break;
        case 89:
            msgvals = TMsgVals(2, 5, 1, 8, 1);
            break;
        case 90:
            msgvals = TMsgVals(2, 5, 1, 9, 1);
            break;
        case 91:
            msgvals = TMsgVals(2, 5, 1, 5, 1);
            break;
        case 92:
            msgvals = TMsgVals(2, 5, 1, 10, 1);
            break;
        case 93:
            msgvals = TMsgVals(4, 1, 1, 1, 1);
            break;
        case 94:
            msgvals = TMsgVals(3, 0, 1, 9, 1);
            priority += 2;
            break;
        case 95:
            msgvals = TMsgVals(3, 0, 2, 0, 1);
            priority += 3;
            break;
        case 96:
            msgvals = TMsgVals(3, 0, 2, 1, 1);
            priority += 4;
            break;
        case 97:
            msgvals = TMsgVals(3, 0, 2, 2, 1);
            priority += 5;
            break;
        case 98:
            msgvals = TMsgVals(3, 0, 2, 3, 1);
            priority += 6;
            break;
        case 99:
            msgvals = TMsgVals(3, 0, 2, 4, 1);
            priority += 7;
            break;
        case 100:
            msgvals = TMsgVals(3, 0, 2, 5, 1);
            priority += 8;
            break;
        case 101:
            msgvals = TMsgVals(3, 0, 2, 6, 1);
            priority += 9;
            break;
        case 102:
            msgvals = TMsgVals(3, 0, 2, 7, 1);
            priority += 10;
            break;
        case 103:
            msgvals = TMsgVals(3, 0, 2, 8, 1);
            priority += 11;
            break;
        case 104:
            msgvals = TMsgVals(3, 0, 2, 9, 1);
            priority += 12;
            break;

        default:
            break;
        }

        if ( msgvals.v1 )
        {
            if ( !unit )
                unit = _userRobo;

            int vo_type = msgvals.type;
            bool packVoice = false;

            if ( msgvals.v1 == 1 )
            {
                uint8_t protoId = unit->_mimic_disguise_vehicleID ? unit->_mimic_disguise_vehicleID : unit->_vehicleID;
                World::TVhclProto &vhclProto = _vhclProtos[protoId];
                packVoice = !vhclProto.speech_voicepack.empty();
                const int speechEvent = World::Speech::EventIndexFromMsgID(msgID);

                if ( speechEvent >= 0 )
                {
                    const std::string paths[] = {
                        World::Speech::PackEventPath(vhclProto.speech_voicepack, speechEvent)
                    };
                    for (const std::string &path : paths)
                    {
                        if ( path.empty() ) continue;
                        const std::string speechFile = SpeechEventFindFile(_speechEventVariants, path);
                        if ( !speechFile.empty() && VoiceMessagePlayResourceFile(speechFile, unit, priority, ignoreTimeScale) )
                        {
                            SpeechEventLogOnce(g_speechEventLoggedUsed, speechFile,
                                               "Speech event: using %s\n", speechFile);
                            return;
                        }
                        SpeechEventLogOnce(g_speechEventLoggedFallback, path,
                                           "Speech event: missing or failed to load %s, trying the next voice source\n", path);
                    }
                }

                vo_type = vhclProto.vo_type;
                if ( !vo_type )
                    vo_type = 11;
            }

            int v16 = 1;

            if ( msgvals.num > 1 )
                v16 = rand() % msgvals.num + 1;

            if ( !VoiceMessagePlayFile(fmt::sprintf("%x%x%x%x%x.wav", msgvals.v1, vo_type, msgvals.v3, msgvals.v4, v16), unit, priority, ignoreTimeScale) &&
                 packVoice && vo_type != 11 )
            {
                // A configured pack with a missing combat event can use the
                // existing generic unit voice after its vanilla voice fails.
                VoiceMessagePlayFile(fmt::sprintf("1b%x%x%x.wav", msgvals.v3, msgvals.v4, v16), unit, priority, ignoreTimeScale);
            }
        }
    }
}
