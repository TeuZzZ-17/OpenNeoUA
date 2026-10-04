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

struct Variants
{
    std::vector<std::string> Files;
    size_t Last = static_cast<size_t>(-1);
};

int EventIndexFromMsgID(int message);
int EventIndexFromKey(const std::string &key);
const char *EventKey(size_t index);
std::string ParsePackName(const std::string &value);
int LegacyVoiceType(const std::string &voiceClass);
std::string PackEventPath(const std::string &root, const std::string &voiceClass, const std::string &faction, size_t event);
bool IsVariantFilename(const std::string &filename, const std::string &stem);
const std::string &ChooseVariant(Variants &variants, unsigned int random);
}
}

#endif
