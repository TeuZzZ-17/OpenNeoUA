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

const char *LegacyClasses[] = {"", "tank-light", "tank-medium", "tank-heavy",
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

std::string ParsePackName(const std::string &value)
{
    if (value.empty() || value == "0") return "";
    // A category or faction names one folder component, including custom names.
    for (unsigned char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return "";
    return Lower(value);
}

int LegacyVoiceType(const std::string &voiceClass)
{
    // This table selects only the vanilla fallback; it does not limit pack names.
    const std::string name = ParsePackName(voiceClass);
    for (size_t i = 1; i < sizeof(LegacyClasses) / sizeof(LegacyClasses[0]); ++i)
        if (name == LegacyClasses[i]) return static_cast<int>(i);
    return 0;
}

std::string PackEventPath(const std::string &root, const std::string &voiceClass, const std::string &faction, size_t event)
{
    const std::string category = ParsePackName(voiceClass);
    const std::string name = ParsePackName(faction);
    if (root.empty() || category.empty() || name.empty() || event >= EventCount)
        return "";
    // The existing file resolver validates and normalizes this Data/... path.
    const size_t end = root.find_last_not_of("/\\");
    if (end == std::string::npos) return "";
    return root.substr(0, end + 1) + "/" + name + "-" + category + "/" + EventKey(event);
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
