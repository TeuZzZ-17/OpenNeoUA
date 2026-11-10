// Checks that the built-in copy of OpenNeoUA.ini is embedded and applies like an ini layer.
#include "common/ini.h"
#include "openneoua_default_ini.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#undef main
int main()
{
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", what); } };

    Common::Ini::Key sprint("game.sprint_force_up", Common::Ini::KT_WORD, std::string("0"));
    Common::Ini::Key cursors("ui.retro_cursors", Common::Ini::KT_BOOL, false);
    Common::Ini::Key missing("game.not_a_real_key", Common::Ini::KT_WORD, std::string("vanilla"));
    Common::Ini::Key diagnostics("game.crash_diagnostics", Common::Ini::KT_BOOL, false);
    Common::Ini::Key craters("game.deformation_max_craters", Common::Ini::KT_LIMIT, (int32_t)128);
    Common::Ini::PKeyList keys{&sprint, &cursors, &missing, &diagnostics, &craters};

    check(std::strlen(OPENNEOUA_DEFAULT_INI) > 1000, "the ini is embedded");
    Common::Ini::ParseIniTextOverlay(OPENNEOUA_DEFAULT_INI, &keys);
    check(sprint.WasSet && sprint.Get<std::string>() == "60%", "a key that enables a feature is applied and marked as set");
    check(cursors.Get<bool>(), "a bool key is applied");
    check(!missing.WasSet && missing.Get<std::string>() == "vanilla", "unknown keys are untouched");
    check(!diagnostics.Get<bool>(), "local diagnostic opt-in is never embedded as the default");
    Common::Ini::ParseIniTextOverlay("game.crash_diagnostics = invalid\n", &keys);
    check(!diagnostics.Get<bool>(), "invalid diagnostic value keeps the safe default");
    Common::Ini::ParseIniTextOverlay("game.crash_diagnostics = yes\n", &keys);
    check(diagnostics.Get<bool>(), "explicit yes enables diagnostics");
    Common::Ini::ParseIniTextOverlay("game.crash_diagnostics = no\n", &keys);
    check(!diagnostics.Get<bool>(), "explicit no disables diagnostics");
    Common::Ini::ParseIniTextOverlay("game.deformation_max_craters = no_limit\n", &keys);
    check(craters.WasSet && craters.Get<int32_t>() == std::numeric_limits<int32_t>::max(),
          "no_limit is accepted by a limit key");
    Common::Ini::ParseIniTextOverlay("game.deformation_max_craters = 64\n", &keys);
    check(craters.Get<int32_t>() == 64, "a number still sets a limit key");
    Common::Ini::ParseIniTextOverlay("game.deformation_max_craters = banana\n", &keys);
    check(craters.Get<int32_t>() == 64, "an invalid limit value keeps the previous one");

    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
