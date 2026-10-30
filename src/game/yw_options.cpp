// Options menus: graphics, atmosphere, graphic profiles, palette themes and menu fonts, with their INI storage.
#include <inttypes.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include "common/includes.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/font.h"
#include "gui/button.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include <math.h>
#include "gui/uamsgbox.h"
#include "common/env.h"
#include "common/utils.h"
#include "system/movie.h"
#include "system/inivals.h"
#include "system/system.h"
#include "nucleus/loaders.h"
#include "nucleus/obj3d.h"
#include "game/ypaflyer.h"
#include "common/utils.h"
#include "nucleus/IFFile.h"
#include "world/gunrotation.h"
#include "game/yw_shell_menus.h"
#include "system/framepacer.h"

std::string BlendingLabel(int v)
{
    switch (v)
    {
        case 1:  return Locale::Text::OpenUA(Locale::OUA_ADDITIVE);
        case 2:  return Locale::Text::OpenUA(Locale::OUA_SHARP);
        default: return Locale::Text::OpenUA(Locale::OUA_DEFAULT);
    }
}

int CycleBlending(int v)   { return (v == 0) ? 1 : (v == 1) ? 2 : 0; }

// OpenNeoUA: Genesis list order is stored as classic/energy text.
// Anything else normalizes to classic so vanilla sorting is preserved.
std::string NormalizeGenesisListOrder(const std::string &v)
{
    return !StriCmp(v, "energy") ? "energy" : "classic";
}

std::string CycleGenesisListOrder(const std::string &v)
{
    return NormalizeGenesisListOrder(v) == "energy" ? "classic" : "energy";
}

std::string GenesisListOrderLabel(const std::string &v)
{
    return Locale::Text::OpenUA(NormalizeGenesisListOrder(v) == "energy"
        ? Locale::OUA_GENESIS_ORDER_ENERGY
        : Locale::OUA_GENESIS_ORDER_CLASSIC);
}

int NormalizeGraphicsMode(int value)
{
    return value == 1 ? 1 : 0;
}

int NormalizeVideoFilterMode(int value)
{
    return NormalizeGraphicsMode(value);
}

std::string VideoFilterModeDisplayName(int value)
{
    return Locale::Text::OpenUA(
        NormalizeVideoFilterMode(value) == 1
            ? Locale::OUA_VIDEO_FILTER_ORIGINAL
            : Locale::OUA_VIDEO_FILTER_VHS);
}

int NormalizeHorizonFadeMode(int value)
{
    return NormalizeGraphicsMode(value);
}

std::string HorizonFadeModeDisplayName(int value)
{
    return Locale::Text::OpenUA(
        NormalizeHorizonFadeMode(value) == 1
            ? Locale::OUA_HORIZON_FADE_CLASSIC
            : Locale::OUA_HORIZON_FADE_SMOOTH);
}

// OpenNeoUA: the "Atmosphere" dropdown now selects a modern fullscreen visual filter from
// Data/Filters/*.pal (see GFXEngine::SetVisualFilter), NOT the legacy SET palette-theme.
// The existing paletteTheme* members/functions are reused as the filter selector to keep
// the menu infrastructure unchanged. An empty selection means "Standard" (no filter).
std::string NormalizePaletteThemeName(std::string theme)
{
    size_t first = theme.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    size_t last = theme.find_last_not_of(" \t\r\n");
    theme = theme.substr(first, last - first + 1);

    // "Standard" is the new sentinel; "Original" kept for backward compatibility.
    if (!StriCmp(theme, "Standard") || !StriCmp(theme, "Original"))
        return std::string();

    return theme;
}

std::string PaletteThemeStorageValue(const std::string &theme)
{
    return theme.empty() ? std::string("Standard") : theme;
}

std::string NormalizeMenuFontName(std::string fontName)
{
    size_t first = fontName.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string("Default");

    size_t last = fontName.find_last_not_of(" \t\r\n");
    fontName = fontName.substr(first, last - first + 1);

    if (!StriCmp(fontName, "Default"))
        return std::string("Default");

    return System::MenuFontDisplayValue(fontName);
}

std::string MenuFontDisplayName(const std::string &fontName)
{
    const std::string normalized = NormalizeMenuFontName(fontName);
    if (!StriCmp(normalized, "Default"))
        return Locale::Text::OpenUA(Locale::OUA_DEFAULT);
    return normalized;
}

void SavePaletteThemeCache(const std::string &theme)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc("env:visual_filter.txt", "w");
    if (!fil)
        return;

    fil->puts(PaletteThemeStorageValue(theme) + "\n");
    delete fil;
}

std::string VisualFilterStrengthStorageValue(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    return std::to_string(value / 100) + "."
         + (value % 100 < 10 ? "0" : "")
         + std::to_string(value % 100);
}

int VisualFilterStrengthPercentFromString(std::string s, int fallback)
{
    s = StrTrim(s);
    if (s.empty())
        return fallback;

    try
    {
        if (s.find(',') != std::string::npos)
            return fallback;

        size_t pos = 0;
        float st = std::stof(s, &pos);
        if (StrTrim(s.substr(pos)).size() != 0)
            return fallback;

        if (st < 0.0f) st = 0.0f;
        if (st > 1.0f) st = 1.0f;
        return (int)(st * 100.0f + 0.5f);
    }
    catch (...)
    {
        return fallback;
    }
}

int FloatHundredFromString(std::string s, int fallback, int minValue, int maxValue)
{
    s = StrTrim(s);
    if (s.empty())
        return fallback;

    try
    {
        if (s.find(',') != std::string::npos)
            return fallback;

        size_t pos = 0;
        float value = std::stof(s, &pos);
        if (StrTrim(s.substr(pos)).size() != 0 || !std::isfinite(value))
            return fallback;

        int scaled = (int)(value * 100.0f + (value >= 0.0f ? 0.5f : -0.5f));
        if (scaled < minValue) scaled = minValue;
        if (scaled > maxValue) scaled = maxValue;
        return scaled;
    }
    catch (...)
    {
        return fallback;
    }
}

int IntFromString(std::string s, int fallback, int minValue, int maxValue)
{
    s = StrTrim(s);
    if (s.empty())
        return fallback;

    try
    {
        size_t pos = 0;
        int value = std::stoi(s, &pos);
        if (StrTrim(s.substr(pos)).size() != 0)
            return fallback;
        if (value < minValue) value = minValue;
        if (value > maxValue) value = maxValue;
        return value;
    }
    catch (...)
    {
        return fallback;
    }
}

int SkyHorizonDistanceToSlider(int32_t distance)
{
    distance = std::max<int32_t>(YW_SKY_HORIZON_DISTANCE_MIN,
                                 std::min<int32_t>(YW_SKY_HORIZON_DISTANCE_MAX, distance));
    return (distance + YW_SKY_HORIZON_DISTANCE_UI_STEP / 2) /
           YW_SKY_HORIZON_DISTANCE_UI_STEP;
}

int32_t SkyHorizonSliderToDistance(int sliderValue)
{
    const int32_t minStep = YW_SKY_HORIZON_DISTANCE_MIN / YW_SKY_HORIZON_DISTANCE_UI_STEP;
    const int32_t maxStep = YW_SKY_HORIZON_DISTANCE_MAX / YW_SKY_HORIZON_DISTANCE_UI_STEP;
    sliderValue = std::max<int32_t>(minStep, std::min<int32_t>(maxStep, sliderValue));
    return sliderValue * YW_SKY_HORIZON_DISTANCE_UI_STEP;
}

std::string HundredStorageValue(int value)
{
    const bool negative = value < 0;
    int absValue = negative ? -value : value;
    std::string out = std::to_string(absValue / 100) + ".";
    if ((absValue % 100) < 10)
        out += "0";
    out += std::to_string(absValue % 100);
    return negative ? "-" + out : out;
}

std::string PaletteThemeDisplayName(const std::string &fileName)
{
    if (fileName.empty())
        return Locale::Text::OpenUA(Locale::OUA_STANDARD);

    std::string name = fileName;
    if (name.size() >= 4 && !StriCmp(name.substr(name.size() - 4), ".pal"))
        name.resize(name.size() - 4);

    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}

int yw_loadSky(NC_STACK_ypaworld *yw, const std::string &skyname)
{
    struct SkyLooseScopeGuard
    {
        bool active = false;

        explicit SkyLooseScopeGuard(const std::string &name)
        {
            active = IFFile::BeginSkyLooseScope(name);
        }

        ~SkyLooseScopeGuard()
        {
            if ( active )
                IFFile::EndSkyLooseScope();
        }
    } skyLooseGuard(skyname);

    std::string tmprsrc = Common::Env.SetPrefix("rsrc", "data:");

    // "sky = Data/....BASE" loads an external BASE (canonical Data-first path, same resolver
    // as the other external models). Any other value keeps the legacy form, a path
    // relative to Data such as "objects/1998_04.bas".
    std::string skyfilename = fmt::sprintf("data:%s", skyname);
    std::string externalSky;
    if ( uaNormalizeDataAssetPath(skyname, &externalSky, false) &&
         externalSky.size() > 5 &&
         !StriCmp(externalSky.substr(externalSky.size() - 5), ".BASE") )
        skyfilename = externalSky;

    NC_STACK_base *sky = Utils::ProxyLoadBase(skyfilename);
    yw->_skyObject = sky;
    if ( !sky )
    {
        ypa_log_out("Couldn't create %s\n", skyfilename.c_str());
        Common::Env.SetPrefix("rsrc", tmprsrc);
        return 0;
    }

    Common::Env.SetPrefix("rsrc", tmprsrc);

    // Keep the full sky transform active. Static BASE rendering intentionally
    // ignores local scale, which would make gfx.skydistance visually ineffective.
    // The sky has no runtime local rotation, so using the full transform preserves
    // its fixed world orientation while allowing Sky Horizon Distance to resize it.
    sky->SetStatic(false);
    yw->_skyBaseScale = sky->GetScale();
    yw->_skyAppliedHorizonDistance = -1;
    sky->SetVizLimit(yw->_skyVizLimit);
    sky->SetFadeLength(yw->_skyFadeLength);
    sky->ComputeStaticFog();
    sky->MakeVBO();
    return 1;
}

void NC_STACK_ypaworld::SetFarView(bool farvw)
{
    if ( farvw )
    {
        setYW_visSectors(9);
        setYW_normVisLimit(3100);
        setYW_fadeLength(2100);
    }
    else
    {
        setYW_visSectors(5);
        setYW_normVisLimit(1400);
        setYW_fadeLength(600);
    }

    ApplyNucleusViewDistanceOverrides();
}

// Go to options menu
void UserData::ShowOptionsMenu()
{
    resetGraphicProfilePending = false;
    atmospherePageActive = false;
    if (atmosphere_button)
        atmosphere_button->HideScreen();
    titel_button->HideScreen();

    RefreshPaletteThemes();
    RefreshMenuFonts();
    confPaletteTheme = paletteTheme;
    confMenuFont = menuFont;
    confPlayerRoboAIBehavior = System::IniConf::GameRoboPlayerAIBehavior.Get<bool>();
    confSpectatorMode = System::IniConf::GameSpectatorMode.Get<bool>();
    confPlayAsOtherFactions = System::IniConf::GamePlayAsOtherFactions.Get<bool>();
    confHideMapBorderWalls = System::IniConf::GfxHideMapBorderWalls.Get<bool>();
    confRetroCursors = System::IniConf::UiRetroCursors.Get<bool>();
    confInterfaceStyle = interfaceStyle;
    confMaxFps = System::NormalizeFrameRateLimit(System::IniConf::GfxMaxFps.Get<int32_t>());
    confGenesisListOrder = NormalizeGenesisListOrder(System::IniConf::UiGenesisListOrder.Get<std::string>());
    ambientSoundVolume = p_YW->GetAmbientSoundGlobalVolume();
    confAmbientSoundVolume = ambientSoundVolume;
    UpdatePaletteThemeText();
    UpdateMenuFontText();
    UpdateGfxOptionTexts();
    UpdateGenesisListOrderText();

    NC_STACK_button::button_66arg state;
    state.butID = 1174;
    state.field_4 = (!confPlayerRoboAIBehavior) + 1;
    video_button->SetState(&state);

    state.butID = 1175;
    state.field_4 = (!confSpectatorMode) + 1;
    video_button->SetState(&state);

    state.butID = 1190;
    state.field_4 = (!confPlayAsOtherFactions) + 1;
    video_button->SetState(&state);

    state.butID = 1189;
    state.field_4 = (confInterfaceStyle == GFX::VirtualUIStyle::RETRO) ? 1 : 2;
    video_button->SetState(&state);

    state.butID = 1193;
    state.field_4 = confHideMapBorderWalls ? 1 : 2;
    video_button->SetState(&state);

    state.butID = 1196;
    state.field_4 = confRetroCursors ? 1 : 2;
    video_button->SetState(&state);

    if ( NC_STACK_button::Slider *ambientSlider = video_button->GetSliderData(1191) )
    {
        ambientSlider->value = confAmbientSoundVolume;
        video_button->Refresh(1191);
        video_button->SetText(1192, std::to_string(confAmbientSoundVolume));
    }


    video_button->ShowScreen();

    EnvMode = ENVMODE_SETTINGS;

    if ( video_listvw.IsOpen() )
    {
        p_YW->GuiWinClose( &video_listvw );
        p_YW->GuiWinOpen( &video_listvw );
    }

    video_listvw.selectedEntry = _gfxModeIndex;
}

// Restore the main page and stage the default profile for the advanced page.
// Values are staged exactly like ordinary UI edits: Back cancels them;
// Save Settings persists them through
// the existing USER.TXT/OpenNeoUA.ini paths.
void UserData::ResetOptionsToDefaults()
{
    resetGraphicProfilePending = true;
    const std::vector<GFX::GfxMode> &modes = GFX::GFXEngine::Instance.GetAvailableModes();

    // OpenNeoUA reset profile prefers 800x600. On an unusual display that does
    // not expose it, fall back safely to the engine's 640x480 default, then to
    // the first available mode rather than leaving an invalid selection.
    int defaultModeIndex = GFX::GFXEngine::Instance.GetGfxModeIndex(
        Common::Point(OPTIONS_RESET_WIDTH, OPTIONS_RESET_HEIGHT));
    if ( defaultModeIndex < 0 )
        defaultModeIndex = GFX::GFXEngine::Instance.GetGfxModeIndex(
            Common::Point(GFX::DEFAULT_WIDTH, GFX::DEFAULT_HEIGHT));
    if ( defaultModeIndex < 0 && !modes.empty() )
        defaultModeIndex = 0;

    if ( defaultModeIndex >= 0 && defaultModeIndex < (int)modes.size() )
    {
        _gfxModeIndex = defaultModeIndex;
        video_listvw.selectedEntry = defaultModeIndex;
        _gfxMode = modes.at(defaultModeIndex);
        video_button->SetText(1156, _gfxMode.name);
        _settingsChangeOptions |= 1;
    }

    // Preserve hidden legacy flags and reset only the graphics toggles visible
    // on this page: Sky on, Windowed Mode off.
    confGFXFlags = GFXFlags;
    confGFXFlags |= World::GFX_FLAG_SKYRENDER;
    confGFXFlags &= ~World::GFX_FLAG_WINDOWED;
    _settingsChangeOptions |= 8 | 0x400;

    // The visible Music checkbox maps to the legacy SF_CDSOUND profile flag.
    // Preserve hidden audio flags and restore the shipped checked state.
    confSoundFlags = soundFlags;
    confSoundFlags |= World::SF_CDSOUND;
    _settingsChangeOptions |= 0x200;

    confFxNumber = OPTIONS_RESET_FX_NUMBER;
    confSoundVolume = OPTIONS_RESET_SOUND_VOLUME;
    confMusicVolume = OPTIONS_RESET_MUSIC_VOLUME;
    _settingsChangeOptions |= 0x40 | 0x80 | 0x100;

    confBlending = OPTIONS_RESET_BLENDING;
    confMaxFps = System::NormalizeFrameRateLimit(OPTIONS_RESET_MAX_FPS);
    confGenesisListOrder = OPTIONS_RESET_GENESIS_LIST_ORDER;
    confMoviePlayer = OPTIONS_RESET_MOVIE_PLAYER;
    confMenuFont = OPTIONS_RESET_MENU_FONT;
    confPlayerRoboAIBehavior = OPTIONS_RESET_PLAYER_ROBO_AI;
    confSpectatorMode = OPTIONS_RESET_SPECTATOR;
    confPlayAsOtherFactions = OPTIONS_RESET_PLAY_AS;
    confHideMapBorderWalls = OPTIONS_RESET_HIDE_MAP_BORDER_WALLS;
    confRetroCursors = OPTIONS_RESET_RETRO_CURSORS;

    const bool defaultRetroInterface = OPTIONS_RESET_RETRO_INTERFACE;
    confInterfaceStyle = defaultRetroInterface ? GFX::VirtualUIStyle::RETRO : GFX::VirtualUIStyle::SMOOTH;

    confAmbientSoundVolume = OPTIONS_RESET_AMBIENT_VOLUME;

    _settingsChangeOptions |= SETTINGS_CHANGE_BLENDING |
                              SETTINGS_CHANGE_MAXFPS |
                              SETTINGS_CHANGE_MOVIE_PLAYER |
                              SETTINGS_CHANGE_MENU_FONT |
                              SETTINGS_CHANGE_PLAYER_ROBO_AI_BEHAVIOR |
                              SETTINGS_CHANGE_SPECTATOR_MODE |
                              SETTINGS_CHANGE_PLAY_AS_OTHER_FACTIONS |
                              SETTINGS_CHANGE_AMBIENT_VOLUME |
                              SETTINGS_CHANGE_INTERFACE_STYLE |
                              SETTINGS_CHANGE_HIDE_MAP_BORDER_WALLS |
                              SETTINGS_CHANGE_RETRO_CURSORS;

    NC_STACK_button::button_66arg state;

    state.butID = 1166; // Windowed Mode: default fullscreen/off.
    state.field_4 = 2;
    video_button->SetState(&state);

    state.butID = 1160; // Sky: visible by default.
    state.field_4 = 1;
    video_button->SetState(&state);

    state.butID = 1164; // Music: enabled by default.
    state.field_4 = 1;
    video_button->SetState(&state);

    state.butID = 1175;
    state.field_4 = (!confSpectatorMode) + 1;
    video_button->SetState(&state);

    state.butID = 1190;
    state.field_4 = (!confPlayAsOtherFactions) + 1;
    video_button->SetState(&state);

    state.butID = 1184;
    state.field_4 = (!confMoviePlayer) + 1;
    video_button->SetState(&state);

    state.butID = 1174;
    state.field_4 = (!confPlayerRoboAIBehavior) + 1;
    video_button->SetState(&state);

    state.butID = 1189;
    state.field_4 = defaultRetroInterface ? 1 : 2;
    video_button->SetState(&state);

    state.butID = 1193;
    state.field_4 = confHideMapBorderWalls ? 1 : 2;
    video_button->SetState(&state);

    state.butID = 1196;
    state.field_4 = confRetroCursors ? 1 : 2;
    video_button->SetState(&state);

    if ( NC_STACK_button::Slider *slider = video_button->GetSliderData(1159) )
    {
        slider->value = confFxNumber;
        video_button->Refresh(1159);
    }

    if ( NC_STACK_button::Slider *slider = video_button->GetSliderData(1152) )
    {
        slider->value = confSoundVolume;
        video_button->Refresh(1152);
    }

    if ( NC_STACK_button::Slider *slider = video_button->GetSliderData(1154) )
    {
        slider->value = confMusicVolume;
        video_button->Refresh(1154);
    }

    if ( NC_STACK_button::Slider *slider = video_button->GetSliderData(1191) )
    {
        slider->value = confAmbientSoundVolume;
        video_button->Refresh(1191);
        video_button->SetText(1192, std::to_string(confAmbientSoundVolume));
    }

    UpdateGfxOptionTexts();
    UpdateMenuFontText();
    UpdateGenesisListOrderText();
}

void UserData::AtmosphereOptionsLoad(bool saveSnapshot)
{
    atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxVisualFilterStrength.Get<std::string>(), 25);
    atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxAtmosphereStrength.Get<std::string>(), 50);
    atmosphereValues[ATMOPT_EXPOSURE] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereExposure.Get<std::string>(), 170, 25, 200);
    atmosphereValues[ATMOPT_CONTRAST] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereContrast.Get<std::string>(), 95, 50, 200);
    atmosphereValues[ATMOPT_SATURATION] =
        FloatHundredFromString(System::IniConf::GfxAtmosphereSaturation.Get<std::string>(), 80, 0, 200);
    atmosphereValues[ATMOPT_VIGNETTE] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxAtmosphereVignette.Get<std::string>(), 60);

    atmosphereValues[ATMOPT_SKY_HORIZON_DISTANCE] =
        SkyHorizonDistanceToSlider(System::IniConf::GfxSkyDistance.Get<int32_t>());

    const int32_t currentSkyHeight = p_YW ? p_YW->getYW_skyHeight() : YW_SKY_HEIGHT_DEFAULT;
    atmosphereValues[ATMOPT_SKY_HEIGHT] =
        IntFromString(System::IniConf::GfxSkyHeight.Get<std::string>(),
                      currentSkyHeight, YW_SKY_HEIGHT_MIN, YW_SKY_HEIGHT_MAX);
    if (p_YW)
        p_YW->setYW_skyHeight(atmosphereValues[ATMOPT_SKY_HEIGHT]);

    atmosphereValues[ATMOPT_FOG_START] =
        IntFromString(System::IniConf::GfxHorizonFogStart.Get<std::string>(), 4000, 0, 10000);
    atmosphereValues[ATMOPT_FOG_LENGTH] =
        IntFromString(System::IniConf::GfxHorizonFogLength.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_FOG_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxHorizonFogStrength.Get<std::string>(), 80);

    atmosphereValues[ATMOPT_DARK_START] =
        IntFromString(System::IniConf::GfxHorizonDarkStart.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_DARK_LENGTH] =
        IntFromString(System::IniConf::GfxHorizonDarkLength.Get<std::string>(), 2000, 0, 10000);
    atmosphereValues[ATMOPT_DARK_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxHorizonDarkStrength.Get<std::string>(), 65);

    atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE] =
        IntFromString(System::IniConf::GameWorldUiMaxDistance.Get<std::string>(), 5700, 100, 20000);

    atmosphereValues[ATMOPT_VHS_STRENGTH] =
        VisualFilterStrengthPercentFromString(System::IniConf::GfxVhsFilterStrength.Get<std::string>(), 60);

    atmosphereValues[ATMOPT_PARTICLE_LIMIT] =
        std::max<int32_t>(0, std::min<int32_t>(YW_PARTICLE_LIMIT_UI_MAX, System::IniConf::GfxParticlesLimit.Get<int32_t>()));

    atmosphereValues[ATMOPT_VIDEO_FILTER_MODE] =
        NormalizeVideoFilterMode(System::IniConf::GfxVideoFilterMode.Get<int32_t>());
    atmosphereValues[ATMOPT_HORIZON_FADE_MODE] =
        NormalizeHorizonFadeMode(System::IniConf::GfxHorizonFadeMode.Get<int32_t>());

    if (p_YW)
        p_YW->UpdateSkyHorizonRenderSectors();

    if (saveSnapshot)
        atmosphereSavedValues = atmosphereValues;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider)
        {
            slider->value = (int16_t)atmosphereValues[i];
            atmosphere_button->Refresh(1400 + i);
        }
    }
    UpdateAtmosphereOptionTexts();
}

void UserData::UpdateAtmosphereOptionTexts()
{
    if (!atmosphere_button)
        return;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        std::string text;
        switch (i)
        {
            case ATMOPT_VISUAL_FILTER_STRENGTH:
            case ATMOPT_ATMOSPHERE_STRENGTH:
            case ATMOPT_VIGNETTE:
            case ATMOPT_FOG_STRENGTH:
            case ATMOPT_DARK_STRENGTH:
            case ATMOPT_VHS_STRENGTH:
                text = std::to_string(atmosphereValues[i]) + "%";
                break;
            case ATMOPT_VIDEO_FILTER_MODE:
                text = VideoFilterModeDisplayName(atmosphereValues[i]);
                break;
            case ATMOPT_HORIZON_FADE_MODE:
                text = HorizonFadeModeDisplayName(atmosphereValues[i]);
                break;
            case ATMOPT_EXPOSURE:
            case ATMOPT_CONTRAST:
            case ATMOPT_SATURATION:
                text = HundredStorageValue(atmosphereValues[i]);
                break;
            case ATMOPT_SKY_HORIZON_DISTANCE:
                text = std::to_string(SkyHorizonSliderToDistance(atmosphereValues[i]));
                break;
            default:
                text = std::to_string(atmosphereValues[i]);
                break;
        }
        atmosphere_button->SetText(1420 + i, text);
    }
}

void UserData::AtmosphereOptionsApplyLive()
{
    if (!atmosphere_button)
        return;

    bool changed = false;
    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider && atmosphereValues[i] != slider->value)
        {
            atmosphereValues[i] = slider->value;
            changed = true;
        }
    }

    if (!changed)
        return;

    UpdateAtmosphereOptionTexts();

    // Keep the framebuffer and world-only atmosphere path active internally.
    System::IniConf::GfxAtmosphereFx.Value = true;
    System::IniConf::GfxVisualFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH]);
    System::IniConf::GfxAtmosphereStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH]);
    System::IniConf::GfxAtmosphereExposure.Value = HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE]);
    System::IniConf::GfxAtmosphereContrast.Value = HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST]);
    System::IniConf::GfxAtmosphereSaturation.Value = HundredStorageValue(atmosphereValues[ATMOPT_SATURATION]);
    System::IniConf::GfxAtmosphereVignette.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE]);

    // The legacy gfx.skydistance key is now the live radius of the
    // player-centered sky horizon. The renderer reads it every frame.
    System::IniConf::GfxSkyDistance.Value =
        SkyHorizonSliderToDistance(atmosphereValues[ATMOPT_SKY_HORIZON_DISTANCE]);
    System::IniConf::GfxSkyHeight.Value =
        std::to_string(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    if (p_YW)
    {
        p_YW->UpdateSkyHorizonRenderSectors();
        p_YW->setYW_skyHeight(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    }

    System::IniConf::GfxHorizonFogEnable.Value = true;
    System::IniConf::GfxHorizonFogStart.Value = std::to_string(atmosphereValues[ATMOPT_FOG_START]);
    System::IniConf::GfxHorizonFogLength.Value = std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH]);
    System::IniConf::GfxHorizonFogStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH]);

    System::IniConf::GfxHorizonDarkEnable.Value = true;
    System::IniConf::GfxHorizonDarkStart.Value = std::to_string(atmosphereValues[ATMOPT_DARK_START]);
    System::IniConf::GfxHorizonDarkLength.Value = std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH]);
    System::IniConf::GfxHorizonDarkStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH]);

    System::IniConf::GameWorldUiMaxDistance.Value =
        std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE]);

    System::IniConf::GfxVhsFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH]);

    // Particle spawns read this key directly, so the new limit applies live.
    System::IniConf::GfxParticlesLimit.Value =
        (int32_t)atmosphereValues[ATMOPT_PARTICLE_LIMIT];
    System::IniConf::GfxVideoFilterMode.Value =
        (int32_t)NormalizeVideoFilterMode(atmosphereValues[ATMOPT_VIDEO_FILTER_MODE]);
    System::IniConf::GfxHorizonFadeMode.Value =
        (int32_t)NormalizeHorizonFadeMode(atmosphereValues[ATMOPT_HORIZON_FADE_MODE]);


    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    DetectMatchingGraphicProfile();
}

void UserData::AtmosphereOptionsSave()
{
    AtmosphereOptionsApplyLive();

    paletteTheme = confPaletteTheme;
    System::IniConf::GfxVisualFilter.Value = PaletteThemeStorageValue(paletteTheme);
    SavePaletteThemeCache(paletteTheme);
    GFX::Engine.SetVisualFilter(PaletteThemeStorageValue(paletteTheme));
    if (!SaveKeyToOpenNeoUAIni("gfx.palette_filter", PaletteThemeStorageValue(paletteTheme)))
        ypa_log_out("WARNING: Could not save gfx.palette_filter to OpenNeoUA.ini\n");

    const std::array<std::pair<const char *, std::string>, 19> values =
    {{
        {"gfx.palette_filter_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH])},
        {"gfx.atmosphere_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH])},
        {"gfx.atmosphere_exposure", HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE])},
        {"gfx.atmosphere_contrast", HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST])},
        {"gfx.atmosphere_saturation", HundredStorageValue(atmosphereValues[ATMOPT_SATURATION])},
        {"gfx.atmosphere_vignette", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE])},
        {"gfx.skydistance", std::to_string(SkyHorizonSliderToDistance(atmosphereValues[ATMOPT_SKY_HORIZON_DISTANCE]))},
        {"gfx.sky_height", std::to_string(atmosphereValues[ATMOPT_SKY_HEIGHT])},
        {"gfx.horizon_fog_start", std::to_string(atmosphereValues[ATMOPT_FOG_START])},
        {"gfx.horizon_fog_length", std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH])},
        {"gfx.horizon_fog_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH])},
        {"gfx.horizon_dark_start", std::to_string(atmosphereValues[ATMOPT_DARK_START])},
        {"gfx.horizon_dark_length", std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH])},
        {"gfx.horizon_dark_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH])},
        {"game.world_ui_max_distance", std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE])},
        {"gfx.vhs_filter_strength", VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH])},
        {"gfx.particles.limit", std::to_string(atmosphereValues[ATMOPT_PARTICLE_LIMIT])},
        {"gfx.video_filter_mode", std::to_string(NormalizeVideoFilterMode(atmosphereValues[ATMOPT_VIDEO_FILTER_MODE]))},
        {"gfx.horizon_fade_mode", std::to_string(NormalizeHorizonFadeMode(atmosphereValues[ATMOPT_HORIZON_FADE_MODE]))}
    }};

    for (const auto &entry : values)
    {
        if (!SaveKeyToOpenNeoUAIni(entry.first, entry.second))
            ypa_log_out("WARNING: Could not save %s to OpenNeoUA.ini\n", entry.first);
    }

    // These are infrastructure switches now enabled by safe internal defaults.
    // Removing them keeps OpenNeoUA.ini focused on artistic values while preserving
    // backward compatibility for users who deliberately add the keys again.
    RemoveKeyFromOpenNeoUAIni("gfx.color_effects");
    RemoveKeyFromOpenNeoUAIni("gfx.atmosphere_fx");
    // Retired: sector count is now derived automatically from gfx.skydistance.
    RemoveKeyFromOpenNeoUAIni("gfx.render_sectors");

    System::IniConf::GfxAtmosphereFx.Value = true;
    System::IniConf::GfxHorizonFogEnable.Value = true;
    System::IniConf::GfxHorizonDarkEnable.Value = true;
    SaveKeyToOpenNeoUAIni("gfx.horizon_fog_enable", "yes");
    SaveKeyToOpenNeoUAIni("gfx.horizon_dark_enable", "yes");

    // Force the final saved state into the current session even when the user
    // opened the page and pressed Save without moving a slider.
    System::IniConf::GfxParticlesLimit.Value =
        (int32_t)atmosphereValues[ATMOPT_PARTICLE_LIMIT];
    System::IniConf::GfxVideoFilterMode.Value =
        (int32_t)NormalizeVideoFilterMode(atmosphereValues[ATMOPT_VIDEO_FILTER_MODE]);
    System::IniConf::GfxHorizonFadeMode.Value =
        (int32_t)NormalizeHorizonFadeMode(atmosphereValues[ATMOPT_HORIZON_FADE_MODE]);
    System::IniConf::GfxSkyDistance.Value =
        SkyHorizonSliderToDistance(atmosphereValues[ATMOPT_SKY_HORIZON_DISTANCE]);
    System::IniConf::GfxSkyHeight.Value =
        std::to_string(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    if (p_YW)
    {
        p_YW->UpdateSkyHorizonRenderSectors();
        p_YW->setYW_skyHeight(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    }

    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    atmosphereSavedValues = atmosphereValues;
    atmosphereSavedGraphicProfileName = atmosphereGraphicProfileName;
    _settingsChangeOptions &= ~SETTINGS_CHANGE_PALETTE_THEME;
    atmospherePageActive = false;
    atmosphere_button->HideScreen();
    video_button->ShowScreen();
}

void UserData::AtmosphereOptionsCancel()
{
    atmosphereValues = atmosphereSavedValues;

    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400 + i);
        if (slider)
        {
            slider->value = (int16_t)atmosphereValues[i];
            atmosphere_button->Refresh(1400 + i);
        }
    }

    // Force one live re-apply of the saved snapshot.
    if (ATMOPT_COUNT > 0)
    {
        NC_STACK_button::Slider *slider = atmosphere_button->GetSliderData(1400);
        if (slider)
            slider->value = (int16_t)atmosphereValues[0];
    }
    UpdateAtmosphereOptionTexts();

    // Apply snapshot directly.
    System::IniConf::GfxVisualFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH]);
    System::IniConf::GfxAtmosphereStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_ATMOSPHERE_STRENGTH]);
    System::IniConf::GfxAtmosphereExposure.Value = HundredStorageValue(atmosphereValues[ATMOPT_EXPOSURE]);
    System::IniConf::GfxAtmosphereContrast.Value = HundredStorageValue(atmosphereValues[ATMOPT_CONTRAST]);
    System::IniConf::GfxAtmosphereSaturation.Value = HundredStorageValue(atmosphereValues[ATMOPT_SATURATION]);
    System::IniConf::GfxAtmosphereVignette.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VIGNETTE]);
    System::IniConf::GfxSkyDistance.Value =
        SkyHorizonSliderToDistance(atmosphereValues[ATMOPT_SKY_HORIZON_DISTANCE]);
    System::IniConf::GfxSkyHeight.Value =
        std::to_string(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    if (p_YW)
    {
        p_YW->UpdateSkyHorizonRenderSectors();
        p_YW->setYW_skyHeight(atmosphereValues[ATMOPT_SKY_HEIGHT]);
    }
    System::IniConf::GfxHorizonFogStart.Value = std::to_string(atmosphereValues[ATMOPT_FOG_START]);
    System::IniConf::GfxHorizonFogLength.Value = std::to_string(atmosphereValues[ATMOPT_FOG_LENGTH]);
    System::IniConf::GfxHorizonFogStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_FOG_STRENGTH]);
    System::IniConf::GfxHorizonDarkStart.Value = std::to_string(atmosphereValues[ATMOPT_DARK_START]);
    System::IniConf::GfxHorizonDarkLength.Value = std::to_string(atmosphereValues[ATMOPT_DARK_LENGTH]);
    System::IniConf::GfxHorizonDarkStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_DARK_STRENGTH]);
    System::IniConf::GameWorldUiMaxDistance.Value =
        std::to_string(atmosphereValues[ATMOPT_WORLD_UI_MAX_DISTANCE]);
    System::IniConf::GfxVhsFilterStrength.Value =
        VisualFilterStrengthStorageValue(atmosphereValues[ATMOPT_VHS_STRENGTH]);
    System::IniConf::GfxParticlesLimit.Value =
        (int32_t)atmosphereValues[ATMOPT_PARTICLE_LIMIT];
    System::IniConf::GfxVideoFilterMode.Value =
        (int32_t)NormalizeVideoFilterMode(atmosphereValues[ATMOPT_VIDEO_FILTER_MODE]);
    System::IniConf::GfxHorizonFadeMode.Value =
        (int32_t)NormalizeHorizonFadeMode(atmosphereValues[ATMOPT_HORIZON_FADE_MODE]);

    GFX::Engine.SetVisualFilterStrength(atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    confPaletteTheme = paletteTheme;
    System::IniConf::GfxVisualFilter.Value = PaletteThemeStorageValue(confPaletteTheme);
    GFX::Engine.SetVisualFilter(PaletteThemeStorageValue(confPaletteTheme));
    _settingsChangeOptions &= ~SETTINGS_CHANGE_PALETTE_THEME;
    UpdatePaletteThemeText();

    atmosphereGraphicProfileName = atmosphereSavedGraphicProfileName;
    UpdateGraphicProfileText();

    atmospherePageActive = false;
    atmosphere_button->HideScreen();
    video_button->ShowScreen();
}

void UserData::ShowAtmosphereOptionsMenu()
{
    if (!atmosphere_button)
        return;

    RefreshPaletteThemes();
    RefreshGraphicProfiles();
    confPaletteTheme = paletteTheme;
    AtmosphereOptionsLoad();
    UpdatePaletteThemeText();
    DetectMatchingGraphicProfile();
    atmosphereSavedGraphicProfileName = atmosphereGraphicProfileName;
    atmospherePageActive = true;
    if (resetGraphicProfilePending)
    {
        const std::string name = System::DEFAULT_GRAPHIC_PROFILE_NAME;
        ApplyGraphicProfile({name, "Data/Scripts/Graphic_Profiles/" + name + ".cfg"});
        resetGraphicProfilePending = false;
    }
    video_button->HideScreen();
    atmosphere_button->ShowScreen();
}

void  UserData::UpdateSelected3DDevFromList()
{
    std::string name;
    std::string guid;

    const std::vector<GFX::TGFXDeviceInfo> &devices = GFX::Engine.GetDevices();

    if ((size_t)d3d_listvw.selectedEntry < devices.size())
    {
        const GFX::TGFXDeviceInfo &dev = devices.at(d3d_listvw.selectedEntry);
        if ( !StriCmp(dev.name, "software") )
            name = Locale::Text::Advanced(Locale::ADV_SOFTWARE);
        else
            name = dev.name;

        guid = dev.guid;
    }

    conf3DName = name;
    conf3DGuid = guid;

    video_button->SetText(1172, name);
}

bool UserData::SaveGraphicProfile(const std::string &name, std::string *error)
{
    auto fail = [&](Locale::OPENUA_STRING id) {
        if (error) *error = Locale::Text::OpenUA(id);
        return false;
    };
    // Keep the supplied name inside the profile folder and valid on Windows.
    if (name.empty() || name.size() > 32 || name.front() == ' ' ||
        name.back() == ' ' || name.back() == '.' || !StriCmp(name, "Custom"))
        return fail(Locale::OUA_PRESET_INVALID_NAME);
    for (unsigned char c : name)
        if (c < 32 || c == 127 || std::string("/\\:*?\"<>|").find(c) != std::string::npos)
            return fail(Locale::OUA_PRESET_INVALID_NAME);
    const std::string stem = name.substr(0, name.find('.'));
    if (!StriCmp(stem, "CON") || !StriCmp(stem, "PRN") ||
        !StriCmp(stem, "AUX") || !StriCmp(stem, "NUL") ||
        (stem.size() == 4 && stem[3] >= '1' && stem[3] <= '9' &&
         (!StriCmp(stem.substr(0, 3), "COM") || !StriCmp(stem.substr(0, 3), "LPT"))))
        return fail(Locale::OUA_PRESET_INVALID_NAME);

    RefreshGraphicProfiles();
    for (const TGraphicProfile &profile : atmosphereGraphicProfiles)
        if (!StriCmp(profile.Name, name))
            return fail(Locale::OUA_PRESET_EXISTS);

    // Serialize the values shown in the menu, including edits not yet saved to INI.
    const std::array<const char *, ATMOPT_COUNT> keys = {{
        "gfx.palette_filter_strength", "gfx.atmosphere_strength",
        "gfx.atmosphere_exposure", "gfx.atmosphere_contrast", "gfx.atmosphere_saturation",
        "gfx.atmosphere_vignette", "gfx.skydistance", "gfx.sky_height",
        "gfx.horizon_fog_start", "gfx.horizon_fog_length", "gfx.horizon_fog_strength",
        "gfx.horizon_dark_start", "gfx.horizon_dark_length", "gfx.horizon_dark_strength",
        "game.world_ui_max_distance", "gfx.vhs_filter_strength", "gfx.particles.limit",
        "gfx.video_filter_mode", "gfx.horizon_fade_mode"
    }};
    std::string text = "gfx.palette_filter = " + PaletteThemeStorageValue(confPaletteTheme) + "\n";
    for (int i = 0; i < ATMOPT_COUNT; ++i)
    {
        std::string value;
        switch (i)
        {
        case ATMOPT_VISUAL_FILTER_STRENGTH: case ATMOPT_ATMOSPHERE_STRENGTH:
        case ATMOPT_VIGNETTE: case ATMOPT_FOG_STRENGTH: case ATMOPT_DARK_STRENGTH:
        case ATMOPT_VHS_STRENGTH:
            value = VisualFilterStrengthStorageValue(atmosphereValues[i]);
            break;
        case ATMOPT_EXPOSURE: case ATMOPT_CONTRAST: case ATMOPT_SATURATION:
            value = HundredStorageValue(atmosphereValues[i]);
            break;
        case ATMOPT_SKY_HORIZON_DISTANCE:
            value = std::to_string(SkyHorizonSliderToDistance(atmosphereValues[i]));
            break;
        default:
            value = std::to_string(atmosphereValues[i]);
            break;
        }
        text += std::string(keys[i]) + " = " + value + "\n";
    }

    const std::string folder = "data:Scripts/Graphic_Profiles";
    if (!uaOpenDir(folder) && !uaCreateDir(folder))
        return fail(Locale::OUA_PRESET_WRITE_FAILED);
    const std::string path = folder + "/" + name + ".cfg";
    auto file = uaOpenFile(path, "wb");
    if (!file.OK())
        return fail(Locale::OUA_PRESET_WRITE_FAILED);
    const bool written = file.write(text.data(), text.size()) == text.size();
    file.close();
    // Read back after close so a buffered write failure cannot report success.
    auto saved = uaOpenFile(path, "rb");
    std::string actual(text.size() + 1, '\0');
    const size_t count = saved.read(&actual[0], actual.size());
    saved.close();
    actual.resize(count);
    if (!written || actual != text)
    {
        uaDeleteFile(path);
        return fail(Locale::OUA_PRESET_WRITE_FAILED);
    }
    RefreshGraphicProfiles();
    atmosphereGraphicProfileName = name;
    UpdateGraphicProfileText();
    if (error) error->clear();
    return true;
}

void UserData::HandleGraphicPresetDialog(TInputState *input)
{
    const auto result = confirm_button->ProcessWidgetsEvents(input);
    if (result.code == 1351 || input->KbdLastHit == Input::KC_ESCAPE)
    {
        sub_46D960();
        graphicPresetInput.clear();
    }
    else if (result.code == 1350 || input->KbdLastHit == Input::KC_RETURN)
    {
        if (confirmMode == 8)
            sub_46D960();
        else
        {
            std::string error;
            if (SaveGraphicProfile(graphicPresetInput, &error))
            {
                sub_46D960();
                ShowConfirmDialog(8, Locale::Text::OpenUA(Locale::OUA_PRESET_SAVED), "", 1);
                graphicPresetInput.clear();
            }
            else
                confirm_button->SetText(1302, error);
        }
    }
    else if (confirmMode == 7)
    {
        if (input->KbdLastHit == Input::KC_BACKSPACE && !graphicPresetInput.empty())
            graphicPresetInput.pop_back();
        else if (input->chr >= ' ' && graphicPresetInput.size() < 32)
            graphicPresetInput.push_back(input->chr);
        confirm_button->SetText(1303, graphicPresetInput + "_");
    }
}

void UserData::RefreshGraphicProfiles()
{
    const std::string currentName = atmosphereGraphicProfileName;
    atmosphereGraphicProfiles.clear();

    // Presets are discovered at runtime. .cfg is the native OpenNeoUA profile
    // extension; .txt remains accepted for compatibility with older mods.
    FSMgr::DirIter dir = uaOpenDir("data:Scripts/Graphic_Profiles");
    FSMgr::iNode *node = nullptr;

    while (dir.getNext(&node))
    {
        if (!node || node->getType() != FSMgr::iNode::NTYPE_FILE)
            continue;

        const std::string fileName = node->getName();
        const size_t dot = fileName.find_last_of('.');
        if (dot == std::string::npos)
            continue;

        const std::string extension = fileName.substr(dot);
        const bool isCfg = !StriCmp(extension, ".cfg");
        const bool isLegacyTxt = !StriCmp(extension, ".txt");
        if (!isCfg && !isLegacyTxt)
            continue;

        TGraphicProfile profile;
        profile.Name = fileName.substr(0, dot);
        profile.Path = node->getVPath();

        bool handled = false;
        for (TGraphicProfile &existing : atmosphereGraphicProfiles)
        {
            if (StriCmp(existing.Name, profile.Name))
                continue;

            handled = true;
            if (isCfg)
                existing = profile;
            break;
        }

        if (!handled)
            atmosphereGraphicProfiles.push_back(profile);
    }

    std::sort(atmosphereGraphicProfiles.begin(), atmosphereGraphicProfiles.end(),
        [](const TGraphicProfile &a, const TGraphicProfile &b)
        {
            return StriCmp(a.Name, b.Name) < 0;
        });

    if (!currentName.empty())
    {
        bool found = false;
        for (const TGraphicProfile &profile : atmosphereGraphicProfiles)
        {
            if (!StriCmp(profile.Name, currentName))
            {
                atmosphereGraphicProfileName = profile.Name;
                found = true;
                break;
            }
        }

        if (!found)
            atmosphereGraphicProfileName.clear();
    }
}

void UserData::DetectMatchingGraphicProfile()
{
    atmosphereGraphicProfileName.clear();

    // Auto-identification is intentionally strict: a profile is named only when
    // it defines every control currently exposed by this page and all values
    // match. Partial or hand-edited configurations therefore remain Custom.
    for (const TGraphicProfile &profile : atmosphereGraphicProfiles)
    {
        Common::Ini::Key visualFilter("gfx.palette_filter", Common::Ini::KT_WORD, std::string("Standard"));
        Common::Ini::Key visualFilterStrength("gfx.palette_filter_strength", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key atmosphereStrength("gfx.atmosphere_strength", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key exposure("gfx.atmosphere_exposure", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key contrast("gfx.atmosphere_contrast", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key saturation("gfx.atmosphere_saturation", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key vignette("gfx.atmosphere_vignette", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key skyDistance("gfx.skydistance", Common::Ini::KT_DIGIT,
                                     (int32_t)YW_SKY_HORIZON_DISTANCE_REFERENCE);
        Common::Ini::Key skyHeight("gfx.sky_height", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key fogStart("gfx.horizon_fog_start", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key fogLength("gfx.horizon_fog_length", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key fogStrength("gfx.horizon_fog_strength", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key darkStart("gfx.horizon_dark_start", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key darkLength("gfx.horizon_dark_length", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key darkStrength("gfx.horizon_dark_strength", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key worldUiMaxDistance("game.world_ui_max_distance", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key vhsStrength("gfx.vhs_filter_strength", Common::Ini::KT_WORD, std::string());
        Common::Ini::Key particleLimit("gfx.particles.limit", Common::Ini::KT_DIGIT, (int32_t)0);
        Common::Ini::Key videoFilterMode("gfx.video_filter_mode", Common::Ini::KT_DIGIT, (int32_t)0);
        Common::Ini::Key horizonFadeMode("gfx.horizon_fade_mode", Common::Ini::KT_DIGIT, (int32_t)0);

        Common::Ini::PKeyList keys =
        {
            &visualFilter, &visualFilterStrength, &atmosphereStrength,
            &exposure, &contrast, &saturation, &vignette, &skyDistance, &skyHeight,
            &fogStart, &fogLength, &fogStrength,
            &darkStart, &darkLength, &darkStrength,
            &worldUiMaxDistance, &vhsStrength, &particleLimit, &videoFilterMode, &horizonFadeMode
        };

        if (!Common::Ini::ParseIniFileOverlay(profile.Path, &keys))
            continue;

        bool complete = true;
        for (Common::Ini::Key *key : keys)
        {
            if (!key->WasSet)
            {
                complete = false;
                break;
            }
        }
        if (!complete)
            continue;

        if (StriCmp(NormalizePaletteThemeName(visualFilter.Get<std::string>()), confPaletteTheme))
            continue;

        std::array<int, ATMOPT_COUNT> profileValues =
        {{
            VisualFilterStrengthPercentFromString(visualFilterStrength.Get<std::string>(), -1),
            VisualFilterStrengthPercentFromString(atmosphereStrength.Get<std::string>(), -1),
            FloatHundredFromString(exposure.Get<std::string>(), -1, 25, 200),
            FloatHundredFromString(contrast.Get<std::string>(), -1, 50, 200),
            FloatHundredFromString(saturation.Get<std::string>(), -1, 0, 200),
            VisualFilterStrengthPercentFromString(vignette.Get<std::string>(), -1),
            SkyHorizonDistanceToSlider(skyDistance.Get<int32_t>()),
            IntFromString(skyHeight.Get<std::string>(), YW_SKY_HEIGHT_DEFAULT,
                          YW_SKY_HEIGHT_MIN, YW_SKY_HEIGHT_MAX),
            IntFromString(fogStart.Get<std::string>(), -1, 0, 10000),
            IntFromString(fogLength.Get<std::string>(), -1, 0, 10000),
            VisualFilterStrengthPercentFromString(fogStrength.Get<std::string>(), -1),
            IntFromString(darkStart.Get<std::string>(), -1, 0, 10000),
            IntFromString(darkLength.Get<std::string>(), -1, 0, 10000),
            VisualFilterStrengthPercentFromString(darkStrength.Get<std::string>(), -1),
            IntFromString(worldUiMaxDistance.Get<std::string>(), -1, 100, 20000),
            VisualFilterStrengthPercentFromString(vhsStrength.Get<std::string>(), -1),
            std::max<int32_t>(0, std::min<int32_t>(YW_PARTICLE_LIMIT_UI_MAX, particleLimit.Get<int32_t>())),
            NormalizeVideoFilterMode(videoFilterMode.Get<int32_t>()),
            NormalizeHorizonFadeMode(horizonFadeMode.Get<int32_t>())
        }};

        if (profileValues == atmosphereValues)
        {
            atmosphereGraphicProfileName = profile.Name;
            break;
        }
    }

    UpdateGraphicProfileText();
}

void UserData::UpdateGraphicProfileText()
{
    if (!atmosphere_button)
        return;

    atmosphere_button->SetText(1389,
        atmosphereGraphicProfileName.empty()
            ? Locale::Text::OpenUA(Locale::OUA_CUSTOM)
            : atmosphereGraphicProfileName);
}

void UserData::MarkAtmosphereGraphicProfileCustom()
{
    if (atmosphereGraphicProfileName.empty())
        return;

    atmosphereGraphicProfileName.clear();
    UpdateGraphicProfileText();
}

bool UserData::ApplyGraphicProfile(const TGraphicProfile &profile)
{
    Common::Ini::PKeyList keys =
    {
        &System::IniConf::GfxVisualFilter,
        &System::IniConf::GfxVisualFilterStrength,
        &System::IniConf::GfxAtmosphereStrength,
        &System::IniConf::GfxAtmosphereExposure,
        &System::IniConf::GfxAtmosphereContrast,
        &System::IniConf::GfxAtmosphereSaturation,
        &System::IniConf::GfxAtmosphereVignette,
        &System::IniConf::GfxSkyDistance,
        &System::IniConf::GfxSkyHeight,
        &System::IniConf::GfxHorizonFogStart,
        &System::IniConf::GfxHorizonFogLength,
        &System::IniConf::GfxHorizonFogStrength,
        &System::IniConf::GfxHorizonDarkStart,
        &System::IniConf::GfxHorizonDarkLength,
        &System::IniConf::GfxHorizonDarkStrength,
        &System::IniConf::GameWorldUiMaxDistance,
        &System::IniConf::GfxVhsFilterStrength,
        &System::IniConf::GfxParticlesLimit
    };

    Common::Ini::Key videoFilterMode("gfx.video_filter_mode", Common::Ini::KT_DIGIT, (int32_t)0);
    Common::Ini::Key horizonFadeMode("gfx.horizon_fade_mode", Common::Ini::KT_DIGIT, (int32_t)0);
    keys.push_back(&videoFilterMode);
    keys.push_back(&horizonFadeMode);

    // Profiles written before these mode keys existed must start in their
    // safe defaults instead of inheriting the previous profile selection.
    if (!Common::Ini::ParseIniFileOverlay(profile.Path, &keys))
    {
        ypa_log_out("WARNING: Could not load graphics profile %s\n", profile.Path.c_str());
        return false;
    }

    System::IniConf::GfxVideoFilterMode.Value =
        (int32_t)NormalizeVideoFilterMode(videoFilterMode.Get<int32_t>());
    System::IniConf::GfxHorizonFadeMode.Value =
        (int32_t)NormalizeHorizonFadeMode(horizonFadeMode.Get<int32_t>());

    // gfx.palette_filter is the only non-slider value on this page. If a profile
    // references a missing PAL, fall back to Standard without blocking the rest.
    const std::string requestedTheme =
        NormalizePaletteThemeName(System::IniConf::GfxVisualFilter.Get<std::string>());
    bool themeFound = requestedTheme.empty();
    for (const std::string &theme : paletteThemes)
    {
        if (!StriCmp(theme, requestedTheme))
        {
            themeFound = true;
            break;
        }
    }

    if (!themeFound)
        ypa_log_out("WARNING: Graphics profile %s references missing visual filter %s; using Standard.\n",
                    profile.Name.c_str(), requestedTheme.c_str());

    confPaletteTheme = themeFound ? requestedTheme : std::string();
    System::IniConf::GfxVisualFilter.Value = PaletteThemeStorageValue(confPaletteTheme);

    // Reuse the existing menu conversion/clamping path. The saved snapshot is
    // deliberately untouched so Back still restores the pre-preset settings.
    AtmosphereOptionsLoad(false);
    UpdatePaletteThemeText();

    // Infrastructure switches stay enabled by the same safe internal defaults
    // used by manual Advanced Graphics Settings editing.
    System::IniConf::GfxAtmosphereFx.Value = true;
    System::IniConf::GfxHorizonFogEnable.Value = true;
    System::IniConf::GfxHorizonDarkEnable.Value = true;

    GFX::Engine.SetVisualFilter(PaletteThemeStorageValue(confPaletteTheme));
    GFX::Engine.SetVisualFilterStrength(
        atmosphereValues[ATMOPT_VISUAL_FILTER_STRENGTH] / 100.0f);
    GFX::Engine.ApplyAtmosphereFromConfig();
    GFX::Engine.ReloadHorizonConfig();
    GFX::Engine.SetVhsFilterEnabled(true);

    atmosphereGraphicProfileName = profile.Name;
    _settingsChangeOptions |= SETTINGS_CHANGE_PALETTE_THEME;
    UpdateGraphicProfileText();
    return true;
}

void UserData::CycleGraphicProfile()
{
    RefreshGraphicProfiles();
    if (atmosphereGraphicProfiles.empty())
    {
        MarkAtmosphereGraphicProfileCustom();
        return;
    }

    size_t next = 0;
    if (!atmosphereGraphicProfileName.empty())
    {
        for (size_t i = 0; i < atmosphereGraphicProfiles.size(); ++i)
        {
            if (!StriCmp(atmosphereGraphicProfiles[i].Name, atmosphereGraphicProfileName))
            {
                next = (i + 1) % atmosphereGraphicProfiles.size();
                break;
            }
        }
    }

    ApplyGraphicProfile(atmosphereGraphicProfiles[next]);
}

void UserData::RefreshPaletteThemes()
{
    paletteThemes.clear();
    paletteThemes.push_back(std::string());

    // OpenNeoUA: enumerate modern fullscreen filters from Data/Filters/*.pal.
    // Missing folder simply yields the single "Standard" (none) entry -> identical rendering.
    FSMgr::DirIter dir = uaOpenDir("data:Filters");
    FSMgr::iNode *node = nullptr;

    while (dir.getNext(&node))
    {
        if (!node || node->getType() != FSMgr::iNode::NTYPE_FILE)
            continue;

        std::string name = node->getName();
        if (name.size() < 4 || StriCmp(name.substr(name.size() - 4), ".pal"))
            continue;

        paletteThemes.push_back(name);
    }

    std::sort(paletteThemes.begin() + 1, paletteThemes.end(),
        [](const std::string &a, const std::string &b) { return StriCmp(a, b) < 0; });

    // gfx.palette_filter is global: OpenNeoUA.ini remains authoritative across
    // restarts and player-profile creation/switching.
    std::string currentTheme =
        NormalizePaletteThemeName(System::IniConf::GfxVisualFilter.Get<std::string>());

    bool found = currentTheme.empty();
    for (const std::string &theme : paletteThemes)
    {
        if ( !StriCmp(theme, currentTheme) )
        {
            found = true;
            break;
        }
    }

    paletteTheme = found ? currentTheme : std::string();
    confPaletteTheme = paletteTheme;
}

void UserData::UpdatePaletteThemeText()
{
    if (atmosphere_button)
        atmosphere_button->SetText(1392, PaletteThemeDisplayName(confPaletteTheme));
}

void UserData::CyclePaletteTheme()
{
    if (paletteThemes.empty())
        RefreshPaletteThemes();

    size_t next = 0;
    for (size_t i = 0; i < paletteThemes.size(); i++)
    {
        if (!StriCmp(paletteThemes[i], confPaletteTheme))
        {
            next = (i + 1) % paletteThemes.size();
            break;
        }
    }

    confPaletteTheme = paletteThemes[next];
    _settingsChangeOptions |= SETTINGS_CHANGE_PALETTE_THEME;
    if (atmospherePageActive)
        DetectMatchingGraphicProfile();
    else
        UpdateGraphicProfileText();
    UpdatePaletteThemeText();
}

void UserData::RefreshMenuFonts()
{
    menuFonts.clear();
    menuFonts.push_back("Default");

    System::RescanFonts();

    std::vector<std::string> scannedFonts = System::GetScannedFontNames();
    menuFonts.insert(menuFonts.end(), scannedFonts.begin(), scannedFonts.end());

    // Use the committed in-memory value first. After OK this is updated by the
    // same Options commit path as gfx.blending, so reopening Options in the same
    // session shows the saved choice immediately. On first startup menuFont is
    // empty, so we read the parsed OpenNeoUA.ini value.
    std::string currentFont = NormalizeMenuFontName(menuFont.empty() ? System::GetConfiguredMenuFontName() : menuFont);
    bool found = !StriCmp(currentFont, "Default");

    if (!found)
    {
        for (const std::string &fontName : menuFonts)
        {
            if (!StriCmp(fontName, currentFont))
            {
                currentFont = fontName;
                found = true;
                break;
            }
        }
    }

    menuFont = found ? currentFont : std::string("Default");
    confMenuFont = menuFont;
}

void UserData::UpdateMenuFontText()
{
    video_button->SetText(1186, MenuFontDisplayName(confMenuFont));
}

void UserData::CycleMenuFont()
{
    if (menuFonts.empty())
        RefreshMenuFonts();

    if (menuFonts.size() <= 1)
    {
        confMenuFont = "Default";
        UpdateMenuFontText();
        return;
    }

    size_t next = 0;
    for (size_t i = 0; i < menuFonts.size(); i++)
    {
        if (!StriCmp(menuFonts[i], confMenuFont))
        {
            next = (i + 1) % menuFonts.size();
            break;
        }
    }

    confMenuFont = menuFonts[next];
    _settingsChangeOptions |= SETTINGS_CHANGE_MENU_FONT;
    UpdateMenuFontText();
}

// Vanilla settings still persist in Nucleus.ini.
bool UserData::SaveKeyToNucleusIni(const std::string &key, const std::string &value)
{
    return SaveIniKey(uaDataFirstNucleusIniPath(), key, value);
}

// OpenNeoUA-only settings persist in the separate custom layer.
bool UserData::SaveKeyToOpenNeoUAIni(const std::string &key, const std::string &value)
{
    return SaveIniKey(uaDataFirstOpenNeoUAIniPath(), key, value);
}

bool UserData::RemoveKeyFromOpenNeoUAIni(const std::string &key)
{
    return RemoveIniKey(uaDataFirstOpenNeoUAIniPath(), key);
}

// OpenNeoUA: refresh the captions of the modern graphics cycle-buttons.
void UserData::UpdateGfxOptionTexts()
{
    video_button->SetText(1183, BlendingLabel(confBlending));
    video_button->SetText(1187, std::to_string(System::NormalizeFrameRateLimit(confMaxFps)));
}

// OpenNeoUA: refresh the Genesis List Order cycle-button caption.
void UserData::UpdateGenesisListOrderText()
{
    video_button->SetText(1188, GenesisListOrderLabel(confGenesisListOrder));
}
