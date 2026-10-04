#ifndef WORLD_SPEECH_H_INCLUDED
#define WORLD_SPEECH_H_INCLUDED

#include <cstddef>
#include <string>
#include <vector>

namespace World
{
namespace Speech
{
constexpr size_t EventCount = 16;

enum class Class
{
    None, TankLight, TankMedium, TankHeavy,
    HeliLight, HeliMedium, HeliHeavy,
    FlyerLight, FlyerMedium, FlyerHeavy, Ufo, Car
};

enum class Faction
{
    None, Auto, Resistance, Taer, Myko, Sulg, Ghork, BlackSect
};

struct Variants
{
    std::vector<std::string> Files;
    size_t Last = static_cast<size_t>(-1);
};

int EventIndexFromMsgID(int message);
int EventIndexFromKey(const std::string &key);
const char *EventKey(size_t index);
Class ParseClass(const std::string &value);
const char *ClassName(Class value);
int LegacyVoiceType(Class value);
Faction ParseFaction(const std::string &value);
Faction ResolveFaction(Faction configured, int owner);
const char *FactionName(Faction faction);
std::string PackEventPath(const std::string &root, Class voiceClass, Faction faction, size_t event);
std::string ClassEventPath(Class voiceClass, size_t event);
bool IsVariantFilename(const std::string &filename, const std::string &stem);
const std::string &ChooseVariant(Variants &variants, unsigned int random);
}
}

#endif
