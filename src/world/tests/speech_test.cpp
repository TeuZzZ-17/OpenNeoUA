// Native probe linked to the production engine objects, with SDL_main renamed.
#include "../../yw.h"
#include "../../env.h"
#include "../speech.h"
#include <cstdio>
#include <set>
#undef main

namespace
{
int failures = 0, checks = 0;
void Check(bool condition, const char *name)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL %s\n", name); }
}
bool Parse(NC_STACK_ypaworld &world, const Engine::StringList &lines)
{
    ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
    return ScriptParser::ParseStringList(lines, handlers, 0);
}
}

int main()
{
    using namespace World::Speech;
    const std::string categories[] = {"tank-light","tank-medium","tank-heavy","heli-light","heli-medium","heli-heavy","flyer-light","flyer-medium","flyer-heavy","ufo","car"};
    FSMgr::iDir::setBaseDir(".");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("rsrc", "data:");
    Common::Env.SetPrefix("scripts", "data:scripts");
    const std::string root = "Data/Sounds/Speech/Voicepacks";
    const int messages[] = {14,1,15,16,17,22,10,7,6,18,9,8,19,5,11,45};
    const char *factions[] = {"resistance", "taer", "myko", "sulg", "ghork", "blacksect"};
    std::set<int> events;
    for (int message : messages)
    {
        const int index = EventIndexFromMsgID(message);
        Check(index >= 0 && EventIndexFromKey(EventKey(index)) == index, "existing event round trip");
        events.insert(index);
    }
    Check(events.size() == EventCount && EventIndexFromKey("invented_event") == -1, "fixed existing event registry");
    Check(ParsePackName("TANK-LIGHT") == "tank-light" && ParsePackName("siegewalker") == "siegewalker" && ParsePackName("0").empty(), "dynamic class validation");
    Check(ParsePackName("ANCELLE") == "ancelle" && ParsePackName("order_7-blue") == "order_7-blue", "custom faction identifiers");
    for (const std::string invalid : {"", "0", "../myko", "myko/tank", "myko\\tank", "myko:other", "two words", "."})
        Check(ParsePackName(invalid).empty(), "invalid folder component disabled");
    Check(LegacyVoiceType("car") == 11, "car generic B compatibility");
    Check(PackEventPath(root + "/", "tank-medium", "ancelle", 8) == root + "/ancelle-tank-medium/enemy_host_found", "custom pack path");
    Check(PackEventPath("", "tank-medium", "ancelle", 0).empty() &&
          PackEventPath(root, "", "ancelle", 0).empty() &&
          PackEventPath(root, "tank-medium", "0", 0).empty() &&
          PackEventPath(root, "tank-medium", "ancelle", EventCount).empty(), "disabled pack inputs");
    Check(IsVariantFilename("UNIT_READY_103.WAV", "unit_ready") && !IsVariantFilename("unit_ready_00.wav", "unit_ready"), "numbered variants");
    Variants variants; variants.Files = {"one", "two", "three"};
    std::string previous;
    for (unsigned int i = 0; i < 30; ++i)
    {
        const std::string next = ChooseVariant(variants, i * 19);
        Check(next != previous, "no immediate file repeat"); previous = next;
    }

    NC_STACK_ypaworld world;
    world._vhclProtos.resize(256); world._weaponProtos.resize(256);
    world._buildProtos.resize(256); world._roboProtos.resize(1);
    Check(Parse(world, {"new_vehicle 1", "model = tank", "vo_type = 2", "end"}), "legacy parser");
    Check(world._vhclProtos[1].speech_faction.empty() && world._vhclProtos[1].speech_class == "", "legacy defaults");
    Check(Parse(world, {"modify_vehicle 1", "speech_class = tank-medium", "speech_faction = ancelle",
        "speech_voicepack = Data/Sounds/Speech/Voicepacks", "speech_event_not_real = ignored", "end"}), "custom faction parser");
    Check(world._vhclProtos[1].speech_faction == "ancelle", "custom name stored by actual parser");
    Check(Parse(world, {"modify_vehicle 1", "speech_class = 0", "speech_faction = ../bad",
        "speech_voicepack = 0", "speech_event_enemy_host_found = 0", "end"}), "invalid parser inputs");
    Check(world._vhclProtos[1].speech_class == "" && world._vhclProtos[1].speech_faction.empty() &&
          world._vhclProtos[1].speech_voicepack.empty(), "invalid and zero disabled");
    {
        ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
        Check(ScriptParser::ParseFile("data:scripts/Vehicles.cfg", handlers, 0), "live script parsed");
    }
    int attached = 0;
    for (const auto &proto : world._vhclProtos)
        if (!proto.speech_voicepack.empty())
        {
            ++attached;
            Check(proto.speech_voicepack == root && !proto.speech_faction.empty() && proto.speech_faction != "auto", "live explicit pack configuration");
        }
    Check(attached == 51, "all mobile units configured");

    NC_STACK_ypabact unit; unit._vehicleID = 1;
    world._userUnit = &unit; world._userRobo = &unit;
    auto &proto = world._vhclProtos[1];
    for (const std::string &category : categories)
        for (const char *faction : factions)
        {
            proto.speech_class = category;
            proto.speech_faction = faction; proto.speech_voicepack = root; proto.speech_events.fill("");
            for (int message : messages)
            {
                unit._owner = message % 7 + 1;
                world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, message, true);
                Check(world._voiceMessage.Sample && world._voiceMessage.Carrier.Sounds[0].IsEnabled(), "pack event load/start");
                Check(world._voiceMessage.Carrier.Sounds[0].IgnoreTimeScale, "time scale preserved");
                if (!world._voiceMessage.Sample) continue;
                const std::string expected = PackEventPath(root, proto.speech_class, faction, EventIndexFromMsgID(message)).substr(5);
                Check(world._voiceMessage.Sample->getRsrc_name().compare(0, expected.size(), expected) == 0, "manual faction pack selected regardless of owner");
                Check(world._voiceMessage.Carrier.Sounds[0].PSample == world._voiceMessage.Sample->GetSampleData() &&
                    world._voiceMessage.Carrier.Sounds[0].Pitch == 0, "WAV reproduced unchanged");
            }
        }
    proto.speech_class = "tank-medium"; proto.speech_faction = "myko";
    const int found = EventIndexFromMsgID(6);
    proto.speech_events[found] = root + "/taer-tank-heavy/enemy_host_found";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("taer-tank-heavy") != std::string::npos, "explicit event override wins");
    proto.speech_events[found] = "Data/../invalid";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("myko-tank-medium") != std::string::npos, "invalid override recovers selected pack");
    proto.speech_events.fill("");
    world._speechEventVariants[PackEventPath(root, "tank-medium", "myko", found)].Files.clear();
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("12231.wav") != std::string::npos, "missing pack event recovers vanilla directly");
    proto.speech_class = "ufo"; proto.speech_faction = "no-such-faction";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 19);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("1b311.wav") != std::string::npos, "UFO missing combat line recovers generic B");
    proto.speech_class = ""; proto.speech_voicepack.clear(); proto.speech_faction.clear(); proto.vo_type = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("13231.wav") != std::string::npos, "legacy vo_type remains functional");
    NC_STACK_sample *active = world._voiceMessage.Sample;
    Check(!world.VoiceMessagePlayResourceFile("Scripts/Startup.cfg", &unit, 20) && world._voiceMessage.Sample == active, "invalid WAV preserves active message");
    world.VoiceMessagePlayMsg(&unit, 9, 14);
    Check(world._voiceMessage.Sample == active, "priority arbitration");
    world._vhclProtos[2].speech_class = "tank-heavy";
    world._vhclProtos[2].speech_faction = "taer"; world._vhclProtos[2].speech_voicepack = root;
    unit._mimic_disguise_vehicleID = 2; unit._owner = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("taer-tank-heavy") != std::string::npos, "Mimic takes configured disguise voice");
    unit._mimic_disguise_vehicleID = 0;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 23);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Voicepacks") == std::string::npos, "strategic voice unaffected");
    Check(Parse(world, {"modify_vehicle 1", "speech_class = siegewalker", "speech_faction = ancelle",
        "speech_voicepack = Data/Sounds/Speech/_speech_probe", "end"}), "custom replacement parsed");
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("_speech_probe/ancelle-siegewalker") != std::string::npos, "custom faction, category and root loaded");
    if (world._voiceMessage.Sample)
    {
        const TSampleData *sample = world._voiceMessage.Sample->GetSampleData();
        Check(sample->SampleRate == 16000 && sample->Format == AL_FORMAT_MONO16 && sample->bufsz == 8000 &&
            world._voiceMessage.Carrier.Sounds[0].PSample == sample && world._voiceMessage.Carrier.Sounds[0].Pitch == 0, "custom WAV unchanged");
    }
    proto.speech_voicepack = "Data/Sounds/Speech/no-such-root"; proto.vo_type = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("13231.wav") != std::string::npos, "custom category uses configured legacy fallback");
    proto.vo_type = 0;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("1b231.wav") != std::string::npos, "custom category defaults to generic fallback");
    world._voiceMessage.Reset();
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
