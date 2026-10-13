// Native probe linked to the production engine objects, with SDL_main renamed.
#include "game/yw.h"
#include "common/env.h"
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
    Check(PackEventPath(root + "/custom-pack/", 8) == root + "/custom-pack/enemy_host_found", "direct custom pack path");
    Check(PackEventPath("", 0).empty() && PackEventPath("0", 0).empty() &&
          PackEventPath(root, EventCount).empty(), "disabled pack inputs");
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
    Check(world._vhclProtos[1].speech_voicepack.empty(), "legacy defaults");
    Check(Parse(world, {"modify_vehicle 1", "speech_voicepack = Data/Sounds/Speech/custom-pack", "end"}), "direct pack parser");
    Check(world._vhclProtos[1].speech_voicepack == "Data/Sounds/Speech/custom-pack", "direct path stored");
    Check(Parse(world, {"modify_vehicle 1", "speech_voicepack = 0", "end"}), "disabled pack parser");
    Check(world._vhclProtos[1].speech_voicepack.empty(), "zero disabled");
    {
        ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
        Check(ScriptParser::ParseFile("data:scripts/Vehicles.cfg", handlers, 0), "live script parsed");
    }
    int attached = 0;
    for (const auto &proto : world._vhclProtos)
        if (!proto.speech_voicepack.empty())
        {
            ++attached;
            Check(proto.speech_voicepack.compare(0, root.size() + 1, root + "/") == 0, "live direct pack configuration");
        }
    Check(attached == 51, "all mobile units configured");

    NC_STACK_ypabact unit; unit._vehicleID = 1;
    world._userUnit = &unit; world._userRobo = &unit;
    auto &proto = world._vhclProtos[1];
    for (const std::string &category : categories)
        for (const char *faction : factions)
            for (int message : messages)
            {
                proto.speech_voicepack = root + "/" + faction + "-" + category;
                for (int owner : {1, 3})
                {
                    unit._owner = owner;
                    world._voiceMessage.Reset();
                    world.VoiceMessagePlayMsg(&unit, 10, message);
                    const std::string expected = root.substr(5) + "/" + std::string(faction) + "-" + category;
                    Check(world._voiceMessage.Sample != nullptr, "event WAV selected");
                    if (world._voiceMessage.Sample)
                        Check(world._voiceMessage.Sample->getRsrc_name().compare(0, expected.size(), expected) == 0,
                              "explicit pack selected independently of owner");
                }
            }
    proto.speech_voicepack.clear(); proto.vo_type = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("13231.wav") != std::string::npos, "legacy voice preserved");
    NC_STACK_sample *active = world._voiceMessage.Sample;
    Check(!world.VoiceMessagePlayResourceFile("Data/../invalid.wav", &unit, 20) && world._voiceMessage.Sample == active, "invalid path preserves active message");
    world._vhclProtos[2].speech_voicepack = root + "/taer-tank-heavy";
    unit._mimic_disguise_vehicleID = 2; unit._owner = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("taer-tank-heavy") != std::string::npos, "Mimic uses disguise pack");
    unit._mimic_disguise_vehicleID = 0;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 23);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Voicepacks") == std::string::npos, "strategic voice unaffected");
    world._voiceMessage.Reset();
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
