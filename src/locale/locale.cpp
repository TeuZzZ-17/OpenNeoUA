#include "locale.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <vector>

#include "world/consts.h"
#include "system/fsmgr.h"
#include "log.h"
#include "utils.h"

namespace Locale {

static_assert(OPENUA_STRING_LIMIT <= static_cast<int>(World::CVLocaleStringsNumber),
              "OpenUA locale range exceeds the runtime catalogue capacity");
static_assert(OUA_STRING_MAX <= OPENUA_STRING_LIMIT,
              "OpenUA string IDs exceed their reserved range");

namespace {

struct PESection
{
    uint32_t virtualAddress = 0;
    uint32_t virtualSize = 0;
    uint32_t rawAddress = 0;
    uint32_t rawSize = 0;
    std::string name;
};

struct ResourceEntry
{
    uint32_t name = 0;
    uint32_t offset = 0;
};

static bool RangeOK(size_t offset, size_t size, size_t total)
{
    return offset <= total && size <= total - offset;
}

static bool ReadU16(const std::vector<uint8_t> &data, size_t offset, uint16_t *out)
{
    if (!out || !RangeOK(offset, 2, data.size()))
        return false;

    *out = static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
    return true;
}

static bool ReadU32(const std::vector<uint8_t> &data, size_t offset, uint32_t *out)
{
    if (!out || !RangeOK(offset, 4, data.size()))
        return false;

    *out = static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
    return true;
}

static bool ReadWholeFile(const std::string &filename, std::vector<uint8_t> *out)
{
    if (!out)
        return false;

    FSMgr::FileHandle file = uaOpenFile(filename, "rb");
    if (!file.OK())
        return false;

    if (file.seek(0, SEEK_END) != 0)
        return false;

    const size_t size = file.tell();
    if (size == 0 || size > static_cast<size_t>(std::numeric_limits<long>::max()))
        return false;

    if (file.seek(0, SEEK_SET) != 0)
        return false;

    out->assign(size, 0);
    return file.read(out->data(), size) == size;
}

static void AppendUTF8(std::string *out, uint32_t codepoint)
{
    if (!out)
        return;

    if (codepoint <= 0x7F)
    {
        out->push_back(static_cast<char>(codepoint));
    }
    else if (codepoint <= 0x7FF)
    {
        out->push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0xFFFF)
    {
        out->push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0x10FFFF)
    {
        out->push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out->push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

static std::string UTF16LEToUTF8(const std::vector<uint8_t> &data, size_t offset, size_t units)
{
    std::string result;
    result.reserve(units);

    for (size_t i = 0; i < units; ++i)
    {
        uint16_t first = 0;
        if (!ReadU16(data, offset + i * 2, &first))
            break;

        uint32_t codepoint = first;
        if (first >= 0xD800 && first <= 0xDBFF && i + 1 < units)
        {
            uint16_t second = 0;
            if (ReadU16(data, offset + (i + 1) * 2, &second) &&
                second >= 0xDC00 && second <= 0xDFFF)
            {
                codepoint = 0x10000 +
                            ((static_cast<uint32_t>(first) - 0xD800) << 10) +
                            (static_cast<uint32_t>(second) - 0xDC00);
                ++i;
            }
            else
            {
                codepoint = 0xFFFD;
            }
        }
        else if (first >= 0xDC00 && first <= 0xDFFF)
        {
            codepoint = 0xFFFD;
        }

        AppendUTF8(&result, codepoint);
    }

    return result;
}

static bool ReadResourceDirectory(const std::vector<uint8_t> &data,
                                  size_t resourceBase,
                                  size_t resourceSize,
                                  uint32_t relativeOffset,
                                  std::vector<ResourceEntry> *entries)
{
    if (!entries || !RangeOK(relativeOffset, 16, resourceSize))
        return false;

    const size_t directory = resourceBase + relativeOffset;
    uint16_t namedCount = 0;
    uint16_t idCount = 0;
    if (!ReadU16(data, directory + 12, &namedCount) ||
        !ReadU16(data, directory + 14, &idCount))
        return false;

    const size_t count = static_cast<size_t>(namedCount) + idCount;
    const size_t entriesOffset = static_cast<size_t>(relativeOffset) + 16;
    if (!RangeOK(entriesOffset, count * 8, resourceSize) ||
        !RangeOK(directory + 16, count * 8, data.size()))
        return false;

    entries->clear();
    entries->reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        ResourceEntry entry;
        if (!ReadU32(data, directory + 16 + i * 8, &entry.name) ||
            !ReadU32(data, directory + 20 + i * 8, &entry.offset))
            return false;
        entries->push_back(entry);
    }

    return true;
}

static bool RVAtoFileOffset(uint32_t rva,
                            const std::vector<PESection> &sections,
                            size_t fileSize,
                            size_t *offset)
{
    if (!offset)
        return false;

    for (const PESection &section : sections)
    {
        const uint32_t span = std::max(section.virtualSize, section.rawSize);
        if (rva >= section.virtualAddress && rva - section.virtualAddress < span)
        {
            const size_t result = static_cast<size_t>(section.rawAddress) +
                                  (rva - section.virtualAddress);
            if (result < fileSize)
            {
                *offset = result;
                return true;
            }
        }
    }

    return false;
}

static bool ParseIntegerID(const std::string &text, int32_t *out)
{
    if (!out || text.empty())
        return false;

    try
    {
        size_t consumed = 0;
        const long long value = std::stoll(text, &consumed, 0);
        if (consumed != text.size() ||
            value < std::numeric_limits<int32_t>::min() ||
            value > std::numeric_limits<int32_t>::max())
            return false;

        *out = static_cast<int32_t>(value);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool LoadLngRange(const std::string &filename,
                         std::vector<std::string> *strings,
                         int32_t minimumID,
                         int32_t maximumID)
{
    if (!strings || minimumID < 0 || maximumID <= minimumID)
        return false;

    FSMgr::FileHandle file = uaOpenFile(filename, "r");
    if (!file.OK())
        return false;

    bool multiline = false;
    int32_t currentID = -1;
    size_t lineNumber = 0;
    size_t loaded = 0;
    std::set<int32_t> seen;
    std::string line;

    while (file.ReadLine(&line))
    {
        ++lineNumber;
        if (lineNumber == 1 && line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line.erase(0, 3);
        }

        const size_t lineEnd = line.find_first_of("\n\r");
        if (lineEnd != std::string::npos)
            line.erase(lineEnd);

        if (multiline && currentID >= minimumID && currentID < maximumID)
        {
            const bool continues = !line.empty() && line.back() == '\\';
            std::string part = line;
            if (continues)
                part.pop_back();
            std::replace(part.begin(), part.end(), '\\', '\n');
            if (continues)
                part.push_back('\n');
            strings->at(currentID) += part;
            multiline = continues;
            continue;
        }

        multiline = false;
        currentID = -1;

        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#' || line[first] == ';')
            continue;

        const size_t separator = line.find_first_of("= \t", first);
        if (separator == std::string::npos)
        {
            ypa_log_out("Locale: ignored malformed line %d in %s\n",
                        static_cast<int>(lineNumber), filename.c_str());
            continue;
        }

        if (!ParseIntegerID(line.substr(first, separator - first), &currentID))
        {
            ypa_log_out("Locale: ignored invalid ID on line %d in %s\n",
                        static_cast<int>(lineNumber), filename.c_str());
            currentID = -1;
            continue;
        }

        if (currentID < minimumID || currentID >= maximumID ||
            currentID >= static_cast<int32_t>(strings->size()))
        {
            ypa_log_out("Locale: ignored out-of-range ID %d on line %d in %s\n",
                        currentID, static_cast<int>(lineNumber), filename.c_str());
            currentID = -1;
            continue;
        }

        const size_t equals = line.find('=', separator);
        if (equals == std::string::npos)
        {
            ypa_log_out("Locale: ignored line %d without '=' in %s\n",
                        static_cast<int>(lineNumber), filename.c_str());
            currentID = -1;
            continue;
        }

        // Legacy catalogues use one separator space after '='. Skip only that
        // single formatting character so intentional leading or whitespace-only
        // values remain representable (for example "427  =  ").
        size_t valueStart = equals + 1;
        if (valueStart < line.size() &&
            (line[valueStart] == ' ' || line[valueStart] == '\t'))
        {
            ++valueStart;
        }
        std::string value = valueStart < line.size()
                                ? line.substr(valueStart)
                                : std::string();

        const bool continues = !value.empty() && value.back() == '\\';
        if (continues)
            value.pop_back();
        std::replace(value.begin(), value.end(), '\\', '\n');
        if (continues)
            value.push_back('\n');

        if (!StriCmp(value, "<>"))
            value.clear();

        if (!seen.insert(currentID).second)
        {
            ypa_log_out("Locale: duplicate ID %d on line %d in %s; last value wins\n",
                        currentID, static_cast<int>(lineNumber), filename.c_str());
        }

        strings->at(currentID) = value;
        multiline = continues;
        ++loaded;
    }

    return loaded != 0;
}

} // namespace

std::string Text::_localeName;
std::vector<std::string> Text::_localeStrings(World::CVLocaleStringsNumber);

void Text::SetLangDefault()
{
    for (std::string &s : _localeStrings)
        s.clear();

    _localeName = "default";
}

std::string Text::Get(int32_t id, const std::string &def)
{
    if (id < 0 || id >= static_cast<int32_t>(_localeStrings.size()))
        return def;

    const std::string &value = _localeStrings[id];
    return value.empty() ? def : value;
}

std::string Text::OpenUA(uint32_t id)
{
    if (id < OPENUA_STRING_BASE || id >= OPENUA_STRING_LIMIT)
        return "[OpenUA text]";

    std::string fallback = std::string("[OpenUA ") + std::to_string(id) + "]";
    switch (id)
    {
        case OUA_INTERFACE_STYLE: fallback = "Interface Style"; break;
        case OUA_RETRO:           fallback = "Retro"; break;
        case OUA_SMOOTH:          fallback = "Smooth"; break;
        case OUA_RETRO_INTERFACE: fallback = "Retro Interface"; break;
        case OUA_RENDER_DISTANCE:   fallback = "Render Distance"; break;
        case OUA_SWITCH_WEAPON:        fallback = "Switch Weapon"; break;
        case OUA_CYCLE_TARGET:         fallback = "Cycle Target"; break;
        case OUA_ALTERNATIVE_VIEW:       fallback = "Alternative View"; break;
        case OUA_PLACE_MAP_MARKER:     fallback = "Map: Place Map Marker"; break;
        case OUA_KEY_CONFLICT_FORMAT:  fallback = "%s is already used by %s."; break;
        case OUA_KEY_CONFLICT_REASSIGN: fallback = "OK reassigns it. Cancel keeps bindings."; break;
        case OUA_ZOOM_IN:              fallback = "Zoom In"; break;
        case OUA_ZOOM_OUT:             fallback = "Zoom Out"; break;
        case OUA_ENABLE_PLAY_AS:        fallback = "Enable Play As"; break;
        case OUA_AMBIENT_VOLUME:        fallback = "Ambient Volume:"; break;
        default: break;
    }

    return Get(static_cast<int32_t>(id), fallback);
}

bool Text::OpenUALngFileLoad(const std::string &filename)
{
    return LoadLngRange(filename, &_localeStrings,
                        OPENUA_STRING_BASE, OPENUA_STRING_LIMIT);
}

bool Text::DllFileLoad(const std::string &filename)
{
    std::vector<uint8_t> data;
    if (!ReadWholeFile(filename, &data) || data.size() < 0x40)
        return false;

    if (data[0] != 'M' || data[1] != 'Z')
        return false;

    uint32_t peOffset = 0;
    if (!ReadU32(data, 0x3C, &peOffset) ||
        !RangeOK(peOffset, 24, data.size()) ||
        data[peOffset] != 'P' || data[peOffset + 1] != 'E' ||
        data[peOffset + 2] != 0 || data[peOffset + 3] != 0)
        return false;

    uint16_t sectionCount = 0;
    uint16_t optionalHeaderSize = 0;
    if (!ReadU16(data, peOffset + 6, &sectionCount) ||
        !ReadU16(data, peOffset + 20, &optionalHeaderSize))
        return false;

    const size_t optionalHeader = static_cast<size_t>(peOffset) + 24;
    if (!RangeOK(optionalHeader, optionalHeaderSize, data.size()))
        return false;

    uint16_t optionalMagic = 0;
    if (!ReadU16(data, optionalHeader, &optionalMagic))
        return false;

    size_t numberOfDirectoriesOffset = 0;
    size_t dataDirectoriesOffset = 0;
    if (optionalMagic == 0x10B) // PE32
    {
        numberOfDirectoriesOffset = optionalHeader + 92;
        dataDirectoriesOffset = optionalHeader + 96;
    }
    else if (optionalMagic == 0x20B) // PE32+
    {
        numberOfDirectoriesOffset = optionalHeader + 108;
        dataDirectoriesOffset = optionalHeader + 112;
    }
    else
    {
        return false;
    }

    uint32_t directoryCount = 0;
    uint32_t resourceRVA = 0;
    uint32_t resourceSize32 = 0;
    if (!ReadU32(data, numberOfDirectoriesOffset, &directoryCount) ||
        directoryCount <= 2 ||
        !ReadU32(data, dataDirectoriesOffset + 2 * 8, &resourceRVA) ||
        !ReadU32(data, dataDirectoriesOffset + 2 * 8 + 4, &resourceSize32) ||
        resourceRVA == 0 || resourceSize32 == 0)
        return false;

    const size_t sectionTable = optionalHeader + optionalHeaderSize;
    if (sectionCount == 0 ||
        !RangeOK(sectionTable, static_cast<size_t>(sectionCount) * 40, data.size()))
        return false;

    std::vector<PESection> sections;
    sections.reserve(sectionCount);
    for (uint16_t i = 0; i < sectionCount; ++i)
    {
        const size_t offset = sectionTable + static_cast<size_t>(i) * 40;
        PESection section;
        section.name.assign(reinterpret_cast<const char *>(&data[offset]), 8);
        const size_t nul = section.name.find('\0');
        if (nul != std::string::npos)
            section.name.erase(nul);

        if (!ReadU32(data, offset + 8, &section.virtualSize) ||
            !ReadU32(data, offset + 12, &section.virtualAddress) ||
            !ReadU32(data, offset + 16, &section.rawSize) ||
            !ReadU32(data, offset + 20, &section.rawAddress))
            return false;
        sections.push_back(section);
    }

    size_t resourceBase = 0;
    const size_t resourceSize = static_cast<size_t>(resourceSize32);
    if (!RVAtoFileOffset(resourceRVA, sections, data.size(), &resourceBase) ||
        !RangeOK(resourceBase, resourceSize, data.size()))
        return false;

    std::vector<ResourceEntry> rootEntries;
    if (!ReadResourceDirectory(data, resourceBase, resourceSize, 0, &rootEntries))
        return false;

    ResourceEntry stringType;
    bool foundStringType = false;
    for (const ResourceEntry &entry : rootEntries)
    {
        if ((entry.name & 0x80000000U) == 0 && (entry.name & 0xFFFFU) == 6)
        {
            stringType = entry;
            foundStringType = true;
            break;
        }
    }

    if (!foundStringType || (stringType.offset & 0x80000000U) == 0)
        return false;

    std::vector<ResourceEntry> blockEntries;
    if (!ReadResourceDirectory(data, resourceBase, resourceSize,
                               stringType.offset & 0x7FFFFFFFU, &blockEntries))
        return false;

    size_t loaded = 0;
    for (const ResourceEntry &block : blockEntries)
    {
        if ((block.name & 0x80000000U) != 0 ||
            (block.offset & 0x80000000U) == 0)
            continue;

        const uint32_t blockID = block.name & 0xFFFFU;
        if (blockID == 0)
            continue;

        std::vector<ResourceEntry> languageEntries;
        if (!ReadResourceDirectory(data, resourceBase, resourceSize,
                                   block.offset & 0x7FFFFFFFU, &languageEntries) ||
            languageEntries.empty())
            continue;

        const ResourceEntry *language = nullptr;
        for (const ResourceEntry &entry : languageEntries)
        {
            if ((entry.offset & 0x80000000U) == 0)
            {
                language = &entry;
                break;
            }
        }
        if (!language)
            continue;

        const uint32_t dataEntryRelative = language->offset & 0x7FFFFFFFU;
        if (!RangeOK(dataEntryRelative, 16, resourceSize))
            continue;

        const size_t dataEntry = resourceBase + dataEntryRelative;
        uint32_t stringsRVA = 0;
        uint32_t stringsSize = 0;
        if (!ReadU32(data, dataEntry, &stringsRVA) ||
            !ReadU32(data, dataEntry + 4, &stringsSize))
            continue;

        size_t stringsOffset = 0;
        if (!RVAtoFileOffset(stringsRVA, sections, data.size(), &stringsOffset) ||
            !RangeOK(stringsOffset, stringsSize, data.size()))
            continue;

        size_t cursor = stringsOffset;
        const size_t end = stringsOffset + stringsSize;
        for (uint32_t index = 0; index < 16 && cursor + 2 <= end; ++index)
        {
            uint16_t length = 0;
            if (!ReadU16(data, cursor, &length))
                break;
            cursor += 2;

            const size_t byteLength = static_cast<size_t>(length) * 2;
            if (!RangeOK(cursor, byteLength, end))
                break;

            const uint32_t stringID = (blockID - 1) * 16 + index;
            if (length != 0 && stringID < OPENUA_STRING_BASE &&
                stringID < _localeStrings.size())
            {
                std::string value = UTF16LEToUTF8(data, cursor, length);
                // The vanilla resource catalogue uses backslashes as display
                // line breaks. Decode them exactly like the legacy .lng path.
                std::replace(value.begin(), value.end(), '\\', '\n');
                if (!StriCmp(value, "<>"))
                    value.clear();
                _localeStrings[stringID] = value;
                ++loaded;
            }
            cursor += byteLength;
        }
    }

    return loaded != 0;
}

} // namespace Locale
