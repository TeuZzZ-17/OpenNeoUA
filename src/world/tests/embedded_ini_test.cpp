// Checks that the built-in copy of OpenNeoUA.ini is embedded and applies like an ini layer.
#include "common/ini.h"
#include "openneoua_default_ini.h"
#include <cstdio>
#include <cstring>
#undef main
int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what); } };

    Common::Ini::Key sprint("game.sprint_force_up", Common::Ini::KT_WORD, std::string("0"));
    Common::Ini::Key cursors("ui.retro_cursors", Common::Ini::KT_BOOL, false);
    Common::Ini::Key missing("game.not_a_real_key", Common::Ini::KT_WORD, std::string("vanilla"));
    Common::Ini::PKeyList keys{&sprint, &cursors, &missing};

    check(std::strlen(OPENNEOUA_DEFAULT_INI) > 1000, "the ini is embedded");
    Common::Ini::ParseIniTextOverlay(OPENNEOUA_DEFAULT_INI, &keys);
    check(sprint.WasSet && sprint.Get<std::string>() == "60%", "a key that enables a feature is applied and marked as set");
    check(cursors.Get<bool>(), "a bool key is applied");
    check(!missing.WasSet && missing.Get<std::string>() == "vanilla", "unknown keys are untouched");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
