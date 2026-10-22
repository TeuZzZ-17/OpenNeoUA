// Saves in a temporary data tree and checks discovery and exact setting recovery.
#include "game/yw.h"
#include "common/env.h"
#include "common/utils.h"
#include <cstdio>
#undef main

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FSMgr::iDir::setBaseDir(argv[1]);
    Common::Env.SetPrefix("data", "Data");
    int checks = 0, failures = 0;
    auto check = [&](bool ok, const char *what) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL %s\n", what); }
    };
    UserData user{};
    std::string error;
    user.atmosphereValues = {{55, 50, 170, 95, 80, 60, 77, -400,
                             4333, 2000, 35, 5142, 2000, 90, 5700, 60, 9000, 1, 1}};
    for (const char *name : {"", "../escape", "a/b", "a\\b", "CON", "nul.cfg",
                             "Preset.", " Preset", "Preset ", "bad:name", "Custom",
                             "bad\nname", "123456789012345678901234567890123"})
        check(!user.SaveGraphicProfile(name, &error) && !error.empty(), "invalid filename rejected");
    check(user.SaveGraphicProfile("Night Mission", &error), "save and create missing profile directory");
    check(uaFileExist("data:Scripts/Graphic_Profiles/Night Mission.cfg"), "file in native profile folder");
    check(user.atmosphereGraphicProfileName == "Night Mission", "saved profile selected");
    check(user.atmosphereGraphicProfiles.size() == 1, "new file immediately discovered");
    check(!user.SaveGraphicProfile("night mission", &error), "duplicate rejected ignoring case");
    UserData reopened{};
    reopened.atmosphereValues = user.atmosphereValues;
    reopened.RefreshGraphicProfiles();
    reopened.DetectMatchingGraphicProfile();
    check(reopened.atmosphereGraphicProfileName == "Night Mission", "all values round trip through native parser");
    reopened.atmosphereValues[UserData::ATMOPT_EXPOSURE] = 100;
    reopened.DetectMatchingGraphicProfile();
    check(reopened.atmosphereGraphicProfileName.empty(), "changed settings remain Custom");
    user.confPaletteTheme = "Taerkasten_Stone_Rite.pal";
    user.atmosphereValues = {{0, 100, 25, 200, 0, 100,
        YW_SKY_HORIZON_DISTANCE_MIN / YW_SKY_HORIZON_DISTANCE_UI_STEP,
        YW_SKY_HEIGHT_MIN, 0, 10000, 100, 10000, 0, 0, 100, 0, 0, 0, 0}};
    const auto snapshot = user.atmosphereSavedValues;
    check(user.SaveGraphicProfile("Limits and theme", &error), "save limit values and named atmosphere");
    reopened.confPaletteTheme = user.confPaletteTheme;
    reopened.atmosphereValues = user.atmosphereValues;
    reopened.RefreshGraphicProfiles();
    reopened.DetectMatchingGraphicProfile();
    check(reopened.atmosphereGraphicProfileName == "Limits and theme", "limits and atmosphere round trip");
    check(user.atmosphereSavedValues == snapshot, "save preserves the menu cancel snapshot");
    auto legacy = uaOpenFile("data:Scripts/Graphic_Profiles/Legacy.txt", "w");
    legacy.puts("gfx.palette_filter = Standard\n");
    legacy.close();
    check(!user.SaveGraphicProfile("Legacy", &error), "legacy txt name protected");
    Common::Env.SetPrefix("data", "Missing/Data");
    check(!user.SaveGraphicProfile("Unwritable", &error), "write failure reported");
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
