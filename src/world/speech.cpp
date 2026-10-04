#include "speech.h"
#include <algorithm>
#include <cctype>

namespace World
{
namespace Speech
{
namespace
{
struct EventDefinition
{
    int Message;
    const char *Key;
};

// These are the existing unit messages. Strategic/announcer messages keep
// their original voice and never enter the unit voicepack resolver.
const EventDefinition Events[EventCount] = {
    {14, "unit_ready"}, {1, "unit_idle"},
    {15, "order_ack_friendly"}, {16, "order_ack_enemy"},
    {17, "unit_control_entered"}, {22, "enemy_sector_entered"},
    {10, "unit_recovered"}, {7, "enemy_unit_spotted"},
    {6, "enemy_host_found"}, {18, "request_support"},
    {9, "unit_retreating"}, {8, "unit_lost"},
    {19, "enemy_unit_engaged"}, {5, "enemy_unit_destroyed"},
    {11, "enemy_host_destroyed"}, {45, "powerstation_captured"}
};

const char *Classes[] = {"", "tank-light", "tank-medium", "tank-heavy",
    "heli-light", "heli-medium", "heli-heavy", "flyer-light",
    "flyer-medium", "flyer-heavy", "ufo", "car"};

std::string Lower(const std::string &value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

}

int EventIndexFromMsgID(int message)
{
    for (size_t i = 0; i < EventCount; ++i)
        if (Events[i].Message == message)
            return static_cast<int>(i);
    return -1;
}

int EventIndexFromKey(const std::string &key)
{
    const std::string lower = Lower(key);
    for (size_t i = 0; i < EventCount; ++i)
        if (lower == Events[i].Key)
            return static_cast<int>(i);
    return -1;
}

const char *EventKey(size_t index)
{
    return index < EventCount ? Events[index].Key : "";
}

Class ParseClass(const std::string &value)
{
    const std::string lower = Lower(value);
    for (size_t i = 1; i < sizeof(Classes) / sizeof(Classes[0]); ++i)
        if (lower == Classes[i])
            return static_cast<Class>(i);
    return Class::None;
}

const char *ClassName(Class value)
{
    const size_t index = static_cast<size_t>(value);
    return index < sizeof(Classes) / sizeof(Classes[0]) ? Classes[index] : "";
}

int LegacyVoiceType(Class value)
{
    // Class IDs match the original 1..A codes; cars used the generic B voice.
    return ClassName(value)[0] ? static_cast<int>(value) : 0;
}

Faction ParseFaction(const std::string &value)
{
    const std::string lower = Lower(value);
    if (lower == "auto") return Faction::Auto;
    if (lower == "resistance") return Faction::Resistance;
    if (lower == "taer") return Faction::Taer;
    if (lower == "myko") return Faction::Myko;
    if (lower == "sulg") return Faction::Sulg;
    if (lower == "ghork") return Faction::Ghork;
    if (lower == "blacksect") return Faction::BlackSect;
    return Faction::None;
}

Faction ResolveFaction(Faction configured, int owner)
{
    if (configured != Faction::Auto)
        return configured;
    // Owner IDs are the engine's existing faction IDs, including borrowed units.
    switch (owner)
    {
    case 1: return Faction::Resistance;
    case 2: return Faction::Sulg;
    case 3: return Faction::Myko;
    case 4: return Faction::Taer;
    case 5: return Faction::BlackSect;
    case 6: return Faction::Ghork;
    default: return Faction::None;
    }
}


const char *FactionName(Faction faction)
{
    switch (faction)
    {
    case Faction::Resistance: return "resistance";
    case Faction::Taer: return "taer";
    case Faction::Myko: return "myko";
    case Faction::Sulg: return "sulg";
    case Faction::Ghork: return "ghork";
    case Faction::BlackSect: return "blacksect";
    default: return "";
    }
}

std::string PackEventPath(const std::string &root, Class voiceClass, Faction faction, size_t event)
{
    const std::string category = ClassName(voiceClass);
    const std::string name = FactionName(faction);
    if (root.empty() || category.empty() || name.empty() || event >= EventCount)
        return "";
    // The existing file resolver validates and normalizes this Data/... path.
    const size_t end = root.find_last_not_of("/\\");
    if (end == std::string::npos) return "";
    return root.substr(0, end + 1) + "/" + name + "-" + category + "/" + EventKey(event);
}

std::string ClassEventPath(Class voiceClass, size_t event)
{
    const std::string name = ClassName(voiceClass);
    if (name.empty() || event >= EventCount)
        return "";
    // Keep historical pack paths intact: some old heli labels contain flyer audio.
    return "Data/Sounds/Speech/Voicepacks/Classes/" + name + "/" + EventKey(event);
}

bool IsVariantFilename(const std::string &filename, const std::string &stem)
{
    const std::string lower = Lower(filename);
    const std::string prefix = Lower(stem) + "_";
    if (lower.size() < prefix.size() + 6 || lower.compare(0, prefix.size(), prefix) ||
        lower.compare(lower.size() - 4, 4, ".wav"))
        return false;
    bool positive = false;
    for (size_t i = prefix.size(); i < lower.size() - 4; ++i)
    {
        if (lower[i] < '0' || lower[i] > '9') return false;
        positive |= lower[i] != '0';
    }
    return positive;
}

const std::string &ChooseVariant(Variants &variants, unsigned int random)
{
    static const std::string empty;
    const size_t count = variants.Files.size();
    if (!count) return empty;
    size_t chosen = random % count;
    if (count > 1 && variants.Last < count)
    {
        chosen = random % (count - 1);
        if (chosen >= variants.Last) ++chosen;
    }
    variants.Last = chosen;
    return variants.Files[chosen];
}

}
}
