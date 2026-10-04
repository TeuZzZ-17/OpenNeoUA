// Integration probe: link the production objects with main.cpp's SDL_main
// renamed, then run from the game root. No audio device or game session needed.
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
    FSMgr::iDir::setBaseDir(".");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("rsrc", "data:");
    Common::Env.SetPrefix("scripts", "data:scripts");

    std::set<int> eventIDs;
    const int messages[] = {14,1,15,16,17,22,10,7,6,18,9,8,19,5,11,45};
    for (int message : messages)
    {
        const int index = EventIndexFromMsgID(message);
        Check(index >= 0 && EventIndexFromKey(EventKey(index)) == index, "existing event round trip");
        eventIDs.insert(index);
    }
    Check(eventIDs.size() == EventCount, "unique event definitions");
    Check(EventIndexFromMsgID(12) == -1 && EventIndexFromKey("invented_event") == -1, "no invented events");
    Check(ParseClass("TANK-LIGHT") == Class::TankLight && ParseClass("Ufo") == Class::Ufo, "case independent class");
    Check(ParseClass("0") == Class::None && ParseClass("tank-superheavy") == Class::None, "invalid class disabled");
    Check(LegacyVoiceType(Class::Car) == 11, "car original generic B voice");
    Check(ParseFaction("0") == Faction::None && ParseFaction("unknown") == Faction::None, "invalid faction disabled");
    Check(ResolveFaction(Faction::Auto, 5) == Faction::BlackSect && ResolveFaction(Faction::Auto, 7) == Faction::None, "owner faction selection");
    Check(ResolveFaction(Faction::Taer, 1) == Faction::Taer, "fixed faction override");
    Check(IsVariantFilename("UNIT_READY_103.WAV", "unit_ready") && !IsVariantFilename("unit_ready_extra.wav", "unit_ready") &&
          !IsVariantFilename("unit_ready_00.wav", "unit_ready"), "numbered variants only");
    Variants variants; variants.Files = {"one", "two", "three"};
    std::string previous;
    for (unsigned int i = 0; i < 30; ++i)
    {
        const std::string next = ChooseVariant(variants, i * 19);
        Check(next != previous, "no immediate variant repeat"); previous = next;
    }
    Variants missing; Check(ChooseVariant(missing, 1).empty(), "empty pack fallback");

    NC_STACK_ypaworld world;
    world._vhclProtos.resize(256);
    world._weaponProtos.resize(256);
    world._buildProtos.resize(256);
    world._roboProtos.resize(1);
    Check(Parse(world, {"new_vehicle 1", "model = tank", "vo_type = 2", "end"}), "legacy parser fixture");
    Check(world._vhclProtos[1].speech_class == Class::None && world._vhclProtos[1].speech_faction == Faction::None &&
          world._vhclProtos[1].vo_type == 2, "legacy defaults preserved");
    Check(Parse(world, {"modify_vehicle 1", "speech_class = tank-medium", "speech_faction = auto", "speech_voicepack = Data/Sounds/Speech/Voicepacks",
        "speech_event_enemy_host_found = Data/Sounds/Speech/Voicepacks/Classes/tank-heavy/enemy_host_found",
        "speech_event_not_real = ignored", "end"}), "modern parser fixture");
    Check(world._vhclProtos[1].speech_class == Class::TankMedium && world._vhclProtos[1].speech_faction == Faction::Auto &&
          world._vhclProtos[1].speech_voicepack == "Data/Sounds/Speech/Voicepacks",
          "modern parser values");
    Check(Parse(world, {"modify_vehicle 1", "speech_class = 0", "speech_faction = invalid", "speech_voicepack = 0",
        "speech_event_enemy_host_found = 0", "end"}), "invalid values parser fixture");
    Check(world._vhclProtos[1].speech_class == Class::None && world._vhclProtos[1].speech_faction == Faction::None &&
          world._vhclProtos[1].speech_events[EventIndexFromMsgID(6)].empty() && world._vhclProtos[1].speech_voicepack.empty(), "disable and clear values");
    {
        ScriptParser::HandlersList handlers{new World::Parsers::VhclProtoParser(&world)};
        Check(ScriptParser::ParseFile("data:scripts/Vehicles.cfg", handlers, 0), "all live Vehicles.cfg parsed");
    }

    NC_STACK_ypabact unit;
    unit._vehicleID = 1;
    world._userUnit = &unit; world._userRobo = &unit;
    const std::string packRoot = "Data/Sounds/Speech/Voicepacks";
    int attached = 0, configured = 0;
    for (const auto &proto : world._vhclProtos)
    {
        if (!proto.speech_voicepack.empty())
        {
            ++attached;
            Check(proto.speech_voicepack == packRoot && proto.speech_class != Class::None, "live script pack attached");
        }
        if (proto.speech_faction == Faction::Auto) ++configured;
    }
    Check(attached == 51 && configured == 82, "all live script assignments");
    Check(PackEventPath(packRoot + "/", Class::TankMedium, Faction::Taer, EventIndexFromMsgID(6)) ==
        packRoot + "/taer-tank-medium/enemy_host_found", "faction pack path and trailing separator");
    Check(PackEventPath("", Class::TankMedium, Faction::Taer, 0).empty() &&
        PackEventPath(packRoot, Class::None, Faction::Taer, 0).empty() &&
        PackEventPath(packRoot, Class::TankMedium, Faction::None, 0).empty() &&
        PackEventPath(packRoot, Class::TankMedium, Faction::Taer, EventCount).empty(), "disabled pack inputs");
    for (int voiceClass = 1; voiceClass <= static_cast<int>(Class::Car); ++voiceClass)
    {
        for (int owner = 1; owner <= 6; ++owner)
        {
            auto &prototype = world._vhclProtos[1];
            prototype.speech_class = static_cast<Class>(voiceClass);
            prototype.speech_faction = Faction::Auto;
            prototype.speech_voicepack = packRoot;
            prototype.speech_events.fill("");
            unit._owner = owner;
            for (int message : messages)
            {
                world._voiceMessage.Reset();
                world.VoiceMessagePlayMsg(&unit, 10, message, true);
                Check(world._voiceMessage.Sample && world._voiceMessage.Carrier.Sounds[0].IsEnabled(), "class event load/start");
                Check(world._voiceMessage.Carrier.Sounds[0].IgnoreTimeScale, "voice time scale choice preserved");
                if (!world._voiceMessage.Sample) continue;
                TSampleData *original = world._voiceMessage.Sample->GetSampleData();
                TSampleData *actual = world._voiceMessage.Carrier.Sounds[0].PSample;
                Check(actual == original && actual->Data == original->Data &&
                      world._voiceMessage.Carrier.Sounds[0].Pitch == 0, "all factions preserve original PCM and playback rate");
                const std::string expected = PackEventPath(packRoot, static_cast<Class>(voiceClass),
                    ResolveFaction(Faction::Auto, owner), EventIndexFromMsgID(message)).substr(5);
                Check(world._voiceMessage.Sample->getRsrc_name().compare(0, expected.size(), expected) == 0, "actual faction class event selected");
            }
        }
    }
    auto &prototype = world._vhclProtos[1];
    prototype.speech_class = Class::TankMedium; prototype.speech_faction = Faction::Auto;
    const int hostFound = EventIndexFromMsgID(6);
    prototype.speech_events[hostFound] = "Data/Sounds/Speech/Voicepacks/Classes/tank-heavy/enemy_host_found";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("tank-heavy") != std::string::npos, "explicit override wins");
    prototype.speech_events[hostFound] = "Data/Sounds/Speech/no-such-event";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("tank-medium") != std::string::npos, "missing override faction fallback");
    prototype.speech_events[hostFound] = "Data/../no-such-event";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("tank-medium") != std::string::npos, "invalid path class fallback");
    const Faction ownerFaction = ResolveFaction(Faction::Auto, unit._owner);
    world._speechEventVariants[PackEventPath(packRoot, Class::TankMedium, ownerFaction, hostFound)].Files.clear();
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Classes/tank-medium") != std::string::npos,
          "missing faction pack recovers unprocessed category pack");
    world._speechEventVariants[ClassEventPath(Class::TankMedium, hostFound)].Files.clear();
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("12231.wav") != std::string::npos, "missing class original voice fallback");
    Check(world._voiceMessage.Carrier.Sounds[0].PSample == world._voiceMessage.Sample->GetSampleData() &&
        world._voiceMessage.Carrier.Sounds[0].Pitch == 0, "fallback recording unprocessed");
    prototype.speech_class = Class::Ufo; prototype.speech_events.fill("");
    const int engaged = EventIndexFromMsgID(19);
    world._speechEventVariants[PackEventPath(packRoot, Class::Ufo, ownerFaction, engaged)].Files.clear();
    world._speechEventVariants[ClassEventPath(Class::Ufo, engaged)].Files.clear();
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 19);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("1b311.wav") != std::string::npos,
          "UFO absent original combat line recovers generic event");
    prototype.speech_class = Class::None; prototype.speech_faction = Faction::None; prototype.speech_events.fill(""); prototype.vo_type = 3;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 6);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("13231.wav") != std::string::npos &&
          world._voiceMessage.Carrier.Sounds[0].PSample == world._voiceMessage.Sample->GetSampleData(), "legacy voice and PCM exact");
    NC_STACK_sample *active = world._voiceMessage.Sample;
    Check(!world.VoiceMessagePlayResourceFile("Scripts/Startup.cfg", &unit, 20) && world._voiceMessage.Sample == active,
          "invalid audio rejected without replacing active voice");
    world.VoiceMessagePlayMsg(&unit, 9, 14);
    Check(world._voiceMessage.Sample == active, "priority arbitration preserved");
    prototype.speech_class = Class::TankMedium; prototype.speech_faction = Faction::Taer;
    unit._owner = 1;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("taer-tank-medium") != std::string::npos &&
        world._voiceMessage.Carrier.Sounds[0].Pitch == 0,
          "fixed faction overrides controlling owner in playback");
    world._vhclProtos[2].speech_class = Class::TankHeavy;
    world._vhclProtos[2].speech_faction = Faction::Auto;
    world._vhclProtos[2].speech_voicepack = packRoot;
    unit._mimic_disguise_vehicleID = 2; unit._owner = 5;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("blacksect-tank-heavy") != std::string::npos &&
          world._voiceMessage.Carrier.Sounds[0].Pitch == 0,
          "Mimic disguised class with controlling faction");
    unit._mimic_disguise_vehicleID = 0;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 23);
    Check(world._voiceMessage.Sample && world._voiceMessage.Carrier.Sounds[0].PSample == world._voiceMessage.Sample->GetSampleData(), "strategic announcement unchanged");
    world._voiceMessage.Reset();

    // A replacement recording with a distinct format/rate must be loaded directly.
    prototype.speech_class = Class::TankMedium; prototype.speech_faction = Faction::Taer;
    prototype.speech_voicepack = packRoot + "/_speech_probe";
    world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("_speech_probe/taer-tank-medium") != std::string::npos,
        "edited faction recording selected");
    if (world._voiceMessage.Sample)
    {
        const TSampleData *sample = world._voiceMessage.Sample->GetSampleData();
        Check(sample->SampleRate == 16000 && sample->Format == AL_FORMAT_MONO16 && sample->bufsz == 8000 &&
            world._voiceMessage.Carrier.Sounds[0].PSample == sample && world._voiceMessage.Carrier.Sounds[0].Pitch == 0,
            "replacement WAV format, rate and PCM preserved");
    }
    prototype.speech_voicepack = "Data/../invalid";
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Classes/tank-medium") != std::string::npos,
        "invalid voicepack root recovers category");
    prototype.speech_voicepack = packRoot; prototype.speech_faction = Faction::None;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Classes/tank-medium") != std::string::npos,
        "disabled faction recovers category");
    prototype.speech_faction = Faction::Auto; unit._owner = 7;
    world._voiceMessage.Reset(); world.VoiceMessagePlayMsg(&unit, 10, 14);
    Check(world._voiceMessage.Sample && world._voiceMessage.Sample->getRsrc_name().find("Classes/tank-medium") != std::string::npos,
        "unknown owner recovers category");
    world._voiceMessage.Reset();
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
