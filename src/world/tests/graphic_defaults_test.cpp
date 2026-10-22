#include "game/yw_shell_menus.h"
#include "common/env.h"
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
    System::IniConf::Init();
    check(System::IniConf::ReadFromNucleusIni(), "startup configuration loads");
    UserData user{};
    NC_STACK_button emptyControls;
    user.atmosphere_button = &emptyControls;
    user.confPaletteTheme = "Moonlight_City.pal";
    const std::array<int, UserData::ATMOPT_COUNT> expected = {{
        45, 55, 130, 120, 120, 60, 77, -1000, 4381, 2000,
        40, 4619, 2000, 93, 5700, 70, 9000, 0, 0}};
    user.AtmosphereOptionsLoad();
    check(user.atmosphereValues == expected, "all startup graphics match Classic 1998");
    user.RefreshGraphicProfiles();
    user.DetectMatchingGraphicProfile();
    check(user.atmosphereGraphicProfileName == "Classic 1998", "settings identify Classic 1998");
    user.video_button = &emptyControls;
    user.ResetOptionsToDefaults();
    check(user.resetGraphicProfilePending, "graphics reset is staged until confirmation");
    check(!user.confHideMapBorderWalls && user.confRetroCursors &&
          user.confMoviePlayer && user.confPlayerRoboAIBehavior &&
          !user.confSpectatorMode && !user.confPlayAsOtherFactions &&
          user.confInterfaceStyle == GFX::VirtualUIStyle::RETRO,
          "reset stages the screenshot checkboxes");
    check(user.confBlending == 1 && user.confMaxFps == 240 &&
          user.confMenuFont == "Xolonium Regular" && user.confGenesisListOrder == "energy" &&
          user.confFxNumber == 16 && user.confSoundVolume == 127 &&
          user.confMusicVolume == 60 && user.confAmbientSoundVolume == 100,
          "reset stages the screenshot buttons and sliders");
    check((user.confGFXFlags & World::GFX_FLAG_SKYRENDER) &&
          !(user.confGFXFlags & World::GFX_FLAG_WINDOWED) &&
          (user.confSoundFlags & World::SF_CDSOUND), "reset stages sky, fullscreen and music");
    check(!OPTIONS_RESET_HIDE_MAP_BORDER_WALLS, "Reset Defaults shows border walls");
    check(OPTIONS_RESET_WIDTH == 800 && OPTIONS_RESET_HEIGHT == 600, "reset resolution");
    check(OPTIONS_RESET_BLENDING == 1 && OPTIONS_RESET_MAX_FPS == 240, "reset blending and frame limit");
    check(std::string(OPTIONS_RESET_MENU_FONT) == "Xolonium Regular", "reset font");
    check(OPTIONS_RESET_MOVIE_PLAYER && OPTIONS_RESET_PLAYER_ROBO_AI &&
          OPTIONS_RESET_RETRO_INTERFACE && OPTIONS_RESET_RETRO_CURSORS &&
          !OPTIONS_RESET_SPECTATOR && !OPTIONS_RESET_PLAY_AS, "reset checkboxes");
    check(OPTIONS_RESET_FX_NUMBER == 16 && OPTIONS_RESET_SOUND_VOLUME == 127 &&
          OPTIONS_RESET_MUSIC_VOLUME == 60 && OPTIONS_RESET_AMBIENT_VOLUME == 100, "reset sliders");
    check(std::string(OPTIONS_RESET_GENESIS_LIST_ORDER) == "energy", "reset Genesis order");
    check(!System::IniConf::GfxHideMapBorderWalls.Get<bool>(), "startup shows walls");
    check(System::IniConf::GfxVisualFilter.Get<std::string>() == "Moonlight_City.pal" &&
          System::IniConf::GfxVisualFilterStrength.Get<std::string>() == "0.45" &&
          System::IniConf::GfxAtmosphereExposure.Get<std::string>() == "1.30" &&
          System::IniConf::GfxHorizonDarkStart.Get<std::string>() == "4619", "startup screenshot graphics");
    check(Locale::Text::OpenUA(Locale::OUA_PRESET_SAVED) == "Preset saved", "English save confirmation");
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
