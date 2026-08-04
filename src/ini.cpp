#include <inttypes.h>
#include <limits>

#include "ini.h"
#include "env.h"
#include "log.h"
#include "utils.h"

namespace Common {
namespace Ini {

Key::Key(const std::string &k, KEYTYPE t, nonstd::any v)
: Name(k), Type(t), Value(v), DefaultValue(v)
{}


Key::Key(const std::string &k, KEYTYPE t)
: Name(k), Type(t)
{
    switch(t)
    {
        default:
        case KT_DIGIT:
            Value = (int32_t)0;
            break;

        case KT_WORD:
        case KT_STRING:
            Value = std::string();
            break;

        case KT_BOOL:
            Value = false;
            break;
    }

    DefaultValue = Value;
}

void Key::Reset()
{
    Value = DefaultValue;
    WasSet = false;
}

inline Key& Deref(Key &a)
{
    return a;
}

inline Key& Deref(Key *a)
{
    return *a;
}

static bool ParseInt32(const std::string &str, int32_t *out)
{
    try
    {
        size_t pos = 0;
        long long value = std::stoll(str, &pos, 0);

        if ( pos != str.size() ||
             value < std::numeric_limits<int32_t>::min() ||
             value > std::numeric_limits<int32_t>::max() )
            return false;

        *out = (int32_t)value;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static bool ParseBool(const std::string &str, bool *out)
{
    if ( !StriCmp(str, "yes") || !StriCmp(str, "true") ||
         !StriCmp(str, "on") || !StriCmp(str, "1") )
    {
        *out = true;
        return true;
    }

    if ( !StriCmp(str, "no") || !StriCmp(str, "false") ||
         !StriCmp(str, "off") || !StriCmp(str, "0") )
    {
        *out = false;
        return true;
    }

    return false;
}

static void WarnInvalidValue(const Key &key, const std::string &value)
{
    ypa_log_out("WARNING: invalid Nucleus.ini value [%s=%s]; using the default value.\n",
                key.Name.c_str(), value.empty() ? "<missing>" : value.c_str());
}

template <typename T>
void ResetKeys(std::vector<T> *lst)
{
    for ( T &v : *lst )
        Deref(v).Reset();
}

template <typename T>
void ParseLine(std::string line, std::vector<T> *lst)
{
    size_t endp = line.find_first_of(";\r\n");

    if (endp != std::string::npos)
        line.erase(endp);

    Stok splt(line, "= \t");
    std::string token;

    if ( splt.GetNext(&token) )
    {
        for ( T &v : *lst )
        {
            Key &key = Deref(v);

            if ( !StriCmp(key.Name, token) )
            {
                std::string tmp;
                bool parsed = false;

                switch ( key.Type )
                {
                case KT_DIGIT:
                {
                    int32_t value = 0;
                    if ( splt.GetNext(&tmp) && ParseInt32(tmp, &value) )
                    {
                        key.Value = value;
                        parsed = true;
                    }
                    break;
                }

                case KT_BOOL:
                {
                    bool value = false;
                    if ( splt.GetNext(&tmp) && ParseBool(tmp, &value) )
                    {
                        key.Value = value;
                        parsed = true;
                    }
                    break;
                }

                case KT_WORD:
                    if ( splt.GetNext(&tmp) )
                    {
                        key.Value = std::string(tmp);
                        parsed = true;
                    }
                    break;

                case KT_STRING:
                    if ( splt.GetNext(&tmp, "=") )
                        key.Value = std::string(tmp);
                    else
                        key.Value = std::string();
                    parsed = true;
                    break;

                default:
                    break;
                }

                if ( parsed )
                    key.WasSet = true;
                else if ( key.Type == KT_DIGIT || key.Type == KT_BOOL || key.Type == KT_WORD )
                    WarnInvalidValue(key, tmp);
            }
        }
    }
}

bool ParseIniFile(std::string iniFile, KeyList *lst)
{
    if ( iniFile.empty() )
        return false;

    FSMgr::FileHandle *fil = FSMgr::iDir::openFileAlloc(iniFile, "r");

    if ( !fil )
        return false;

    ResetKeys(lst);

    std::string buf;
    while ( fil->ReadLine(&buf) )
        ParseLine(buf, lst);

    delete fil;

    for( const std::string &str : Env._predefinedIniKeys )
        ParseLine(str, lst);

    return true;
}

bool ParseIniFile(std::string iniFile, PKeyList *lst)
{
    if ( iniFile.empty() )
        return false;

    FSMgr::FileHandle *fil = FSMgr::iDir::openFileAlloc(iniFile, "r");

    if ( !fil )
        return false;

    ResetKeys(lst);

    std::string buf;
    while ( fil->ReadLine(&buf) )
        ParseLine(buf, lst);

    delete fil;

    for( const std::string &str : Env._predefinedIniKeys )
        ParseLine<Key *>(str, lst);

    return true;
}

}
}
