#ifndef OPENNEOUA_GLYPH_KEY_H
#define OPENNEOUA_GLYPH_KEY_H

#include <cstdint>
#include <string>

namespace GlyphKey
{
inline bool Valid(uint32_t value)
{
    return value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff) && value != 0xfffd;
}

// Legacy single-byte keys keep their exact value. New keys use one UTF-8
// character or U+XXXX, which also represents script separators unambiguously.
inline bool Parse(const std::string &text, uint32_t &value, bool fontNumeric = false)
{
    if (text.empty()) return false;
    if ((text.size() > 2 && text[0] == 'U' && text[1] == '+') ||
        (fontNumeric && text.size() > 1 && text[0] == '#'))
    {
        const bool unicode = text[0] == 'U';
        const std::string digits = text.substr(unicode ? 2 : 1);
        if (digits.empty() || digits[0] == '-' || digits[0] == '+') return false;
        try
        {
            size_t used = 0;
            const unsigned long parsed = std::stoul(digits, &used, unicode ? 16 : 0);
            if (used != digits.size() || parsed > 0x10ffff || !Valid((uint32_t)parsed)) return false;
            value = (uint32_t)parsed;
            return true;
        }
        catch (...) { return false; }
    }
    const uint8_t first = (uint8_t)text[0];
    if (text.size() == 1 || first < 0x80)
    {
        value = first;
        return true;
    }
    const size_t count = first >= 0xc2 && first <= 0xdf ? 2 :
                         first >= 0xe0 && first <= 0xef ? 3 :
                         first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    if (!count || text.size() != count) return false;
    uint32_t parsed = first & (0x7f >> count);
    for (size_t i = 1; i < count; ++i)
    {
        const uint8_t next = (uint8_t)text[i];
        if ((next & 0xc0) != 0x80) return false;
        parsed = (parsed << 6) | (next & 0x3f);
    }
    if (parsed < (count == 2 ? 0x80u : count == 3 ? 0x800u : 0x10000u) || !Valid(parsed)) return false;
    value = parsed;
    return true;
}

// Allow a UTF-8 script to use symbols from an unchanged Windows-1252 font.
inline uint32_t LegacyAlias(uint32_t value)
{
    static const uint32_t cp1252[32] = {
        0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
        0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
        0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
        0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178
    };
    for (uint32_t i = 0; i < 32; ++i)
        if (cp1252[i] == value) return 0x80 + i;
    return value;
}
}
#endif
