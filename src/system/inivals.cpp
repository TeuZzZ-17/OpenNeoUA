#include <inttypes.h>
#include "inivals.h"
#include "utils.h"

namespace System
{
Common::Ini::PKeyList IniConf::_varList;

// Win3d Class
Common::Ini::Key IniConf::GfxDither("gfx.dither", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxFilter("gfx.filter", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxAntialias("gfx.antialias", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxAlpha("gfx.alpha", Common::Ini::KT_DIGIT, (int32_t)192);
Common::Ini::Key IniConf::GfxZbufWhenTracy("gfx.zbuf_when_tracy", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxColorkey("gfx.colorkey", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxForceEmul("gfx.force_emul", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxForceSoftCursor("gfx.force_soft_cursor", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxAllModes("gfx.all_modes", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxMoviePlayer("gfx.movie_player", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::GfxForceAlphaTex("gfx.force_alpha_textures", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxUseDrawPrimitive("gfx.use_draw_primitive", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxDisableLowres("gfx.disable_lowres", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxExportWindowMode("gfx.export_window_mode", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GfxBlending("gfx.blending", Common::Ini::KT_DIGIT, (int32_t) 0);
Common::Ini::Key IniConf::GfxSolidFont("gfx.solidfont", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GfxVsync("gfx.vsync", Common::Ini::KT_DIGIT, (int32_t)1);
Common::Ini::Key IniConf::GfxMaxFps("gfx.maxfps", Common::Ini::KT_DIGIT, (int32_t)60);
Common::Ini::Key IniConf::GfxNewSky("gfx.newsky", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GfxSkyDistance("gfx.skydistance", Common::Ini::KT_DIGIT, (int32_t)3000);
Common::Ini::Key IniConf::GfxSkyLength("gfx.skylength", Common::Ini::KT_DIGIT, (int32_t)500);
Common::Ini::Key IniConf::GfxHorizonFogEnable("gfx.horizon_fog_enable", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::GfxHorizonFogStart("gfx.horizon_fog_start", Common::Ini::KT_WORD, std::string("4000"));
Common::Ini::Key IniConf::GfxHorizonFogLength("gfx.horizon_fog_length", Common::Ini::KT_WORD, std::string("2000"));
Common::Ini::Key IniConf::GfxHorizonFogStrength("gfx.horizon_fog_strength", Common::Ini::KT_WORD, std::string("0.80"));
// OpenNeoUA Horizon Atmosphere V2: RGB colour used to blend opaque world geometry
// into the distant atmosphere. The effect preserves depth and object alpha.
Common::Ini::Key IniConf::GfxHorizonFogColor("gfx.horizon_fog_color", Common::Ini::KT_WORD, std::string("150_155_160"));
Common::Ini::Key IniConf::GfxHorizonDarkEnable("gfx.horizon_dark_enable", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::GfxHorizonDarkStart("gfx.horizon_dark_start", Common::Ini::KT_WORD, std::string("2000"));
Common::Ini::Key IniConf::GfxHorizonDarkLength("gfx.horizon_dark_length", Common::Ini::KT_WORD, std::string("2000"));
Common::Ini::Key IniConf::GfxHorizonDarkStrength("gfx.horizon_dark_strength", Common::Ini::KT_WORD, std::string("0.65"));
Common::Ini::Key IniConf::GfxHorizonDarkColor("gfx.horizon_dark_color", Common::Ini::KT_WORD, std::string("0_0_0"));
Common::Ini::Key IniConf::GfxRenderSectors("gfx.render_sectors", Common::Ini::KT_WORD, std::string());
Common::Ini::Key IniConf::GfxSkyHeight("gfx.sky_height", Common::Ini::KT_WORD, std::string());
Common::Ini::Key IniConf::GfxSkyRender("gfx.sky_render", Common::Ini::KT_WORD, std::string());
Common::Ini::Key IniConf::GfxAdditionalModes("gfx.custommodes", Common::Ini::KT_STRING);

// Gfx Engine
Common::Ini::Key IniConf::GfxMode("gfx.mode", Common::Ini::KT_DIGIT);
Common::Ini::Key IniConf::GfxXRes("gfx.xres", Common::Ini::KT_DIGIT);
Common::Ini::Key IniConf::GfxYRes("gfx.yres", Common::Ini::KT_DIGIT);
Common::Ini::Key IniConf::GfxPalette("gfx.palette", Common::Ini::KT_WORD);
// OpenNeoUA custom: modern fullscreen visual filter (replaces the legacy palette-theme remap).
Common::Ini::Key IniConf::GfxVisualFilter("gfx.visual_filter", Common::Ini::KT_WORD, std::string("Black_Wadi.pal"));
Common::Ini::Key IniConf::GfxVisualFilterStrength("gfx.visual_filter_strength", Common::Ini::KT_WORD, std::string("0.25"));
Common::Ini::Key IniConf::GfxAtmosphereFx("gfx.atmosphere_fx", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::GfxAtmosphereStrength("gfx.atmosphere_strength", Common::Ini::KT_WORD, std::string("0.50"));
Common::Ini::Key IniConf::GfxAtmosphereExposure("gfx.atmosphere_exposure", Common::Ini::KT_WORD, std::string("1.70"));
Common::Ini::Key IniConf::GfxAtmosphereContrast("gfx.atmosphere_contrast", Common::Ini::KT_WORD, std::string("0.95"));
Common::Ini::Key IniConf::GfxAtmosphereSaturation("gfx.atmosphere_saturation", Common::Ini::KT_WORD, std::string("0.80"));
Common::Ini::Key IniConf::GfxAtmosphereVignette("gfx.atmosphere_vignette", Common::Ini::KT_WORD, std::string("0.60"));
Common::Ini::Key IniConf::GfxVhsFilterShader("gfx.vhs_filter_shader", Common::Ini::KT_STRING, std::string("res/ua_cinematic_1998_vhs.ps"));
Common::Ini::Key IniConf::GfxVhsFilterShaderVbo("gfx.vhs_filter_shader_vbo", Common::Ini::KT_STRING, std::string("res/ua_cinematic_1998_vhs_vbo.ps"));
Common::Ini::Key IniConf::GfxVhsFilterStrength("gfx.vhs_filter_strength", Common::Ini::KT_WORD, std::string("0.60"));
Common::Ini::Key IniConf::GfxDisplay("gfx.display", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::GfxDisplay2("gfx.display2", Common::Ini::KT_WORD);

Common::Ini::Key IniConf::GfxColorEffects("gfx.color_effects", Common::Ini::KT_DIGIT, (int32_t)1);
Common::Ini::Key IniConf::GfxColorEffPower1("gfx.color_eff_pwr[1]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower2("gfx.color_eff_pwr[2]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower3("gfx.color_eff_pwr[3]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower4("gfx.color_eff_pwr[4]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower5("gfx.color_eff_pwr[5]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower6("gfx.color_eff_pwr[6]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower7("gfx.color_eff_pwr[7]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower8("gfx.color_eff_pwr[8]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower9("gfx.color_eff_pwr[9]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower10("gfx.color_eff_pwr[10]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower11("gfx.color_eff_pwr[11]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower12("gfx.color_eff_pwr[12]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower13("gfx.color_eff_pwr[13]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower14("gfx.color_eff_pwr[14]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower15("gfx.color_eff_pwr[15]", Common::Ini::KT_DIGIT, (int32_t)100);
Common::Ini::Key IniConf::GfxColorEffPower16("gfx.color_eff_pwr[16]", Common::Ini::KT_DIGIT, (int32_t)100);

Common::Ini::Key IniConf::GfxVBO("gfx.vbo", Common::Ini::KT_BOOL, true);

// Stored as a single token when saved by Options, e.g. Liberation_Mono_Regular.
// KT_STRING is intentionally kept so older test builds with spaces still parse.
Common::Ini::Key IniConf::UiMenuFont("ui.menu_font", Common::Ini::KT_STRING, std::string("Default"));
// OpenNeoUA: default/current virtual UI scaling style. yes = nearest/Retro, no = linear/Smooth.
Common::Ini::Key IniConf::UiRetroInterface("ui.retro_interface", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::UiMapMarkerSound("ui.map_marker_sound", Common::Ini::KT_STRING, std::string());
Common::Ini::Key IniConf::UiMoveOrderTemplate(
    "ui.move_order_template", Common::Ini::KT_STRING,
    std::string("Interface/Actions/MoveOrder/owner_{owner}/move_order_01.svg"));
Common::Ini::Key IniConf::UiAttackOrderTemplate(
    "ui.attack_order_template", Common::Ini::KT_STRING, std::string());
Common::Ini::Key IniConf::UiRoboMoveOrderTemplate(
    "ui.robo.move_order_template", Common::Ini::KT_STRING,
    std::string("Interface/Actions/MoveOrder/owner_{owner}/move_order_01.svg"));


// Input Engine
Common::Ini::Key IniConf::InputDebug("input.debug", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::InputTimer("input.timer", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputWimp("input.wimp", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputKeyboard("input.keyboard", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputButton0("input.button[0]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton1("input.button[1]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton2("input.button[2]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton3("input.button[3]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton4("input.button[4]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton5("input.button[5]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton6("input.button[6]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton7("input.button[7]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton8("input.button[8]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton9("input.button[9]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton10("input.button[10]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton11("input.button[11]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton12("input.button[12]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton13("input.button[13]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton14("input.button[14]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton15("input.button[15]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton16("input.button[16]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton17("input.button[17]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton18("input.button[18]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton19("input.button[19]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton20("input.button[20]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton21("input.button[21]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton22("input.button[22]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton23("input.button[23]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton24("input.button[24]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton25("input.button[25]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton26("input.button[26]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton27("input.button[27]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton28("input.button[28]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton29("input.button[29]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton30("input.button[30]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputButton31("input.button[31]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider0("input.slider[0]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider1("input.slider[1]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider2("input.slider[2]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider3("input.slider[3]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider4("input.slider[4]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider5("input.slider[5]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider6("input.slider[6]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider7("input.slider[7]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider8("input.slider[8]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider9("input.slider[9]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider10("input.slider[10]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider11("input.slider[11]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider12("input.slider[12]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider13("input.slider[13]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider14("input.slider[14]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider15("input.slider[15]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider16("input.slider[16]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider17("input.slider[17]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider18("input.slider[18]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider19("input.slider[19]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider20("input.slider[20]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider21("input.slider[21]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider22("input.slider[22]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider23("input.slider[23]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider24("input.slider[24]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider25("input.slider[25]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider26("input.slider[26]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider27("input.slider[27]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider28("input.slider[28]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider29("input.slider[29]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider30("input.slider[30]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputSlider31("input.slider[31]", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::InputHotkey0("input.hotkey[0]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey1("input.hotkey[1]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey2("input.hotkey[2]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey3("input.hotkey[3]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey4("input.hotkey[4]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey5("input.hotkey[5]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey6("input.hotkey[6]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey7("input.hotkey[7]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey8("input.hotkey[8]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey9("input.hotkey[9]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey10("input.hotkey[10]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey11("input.hotkey[11]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey12("input.hotkey[12]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey13("input.hotkey[13]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey14("input.hotkey[14]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey15("input.hotkey[15]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey16("input.hotkey[16]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey17("input.hotkey[17]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey18("input.hotkey[18]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey19("input.hotkey[19]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey20("input.hotkey[20]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey21("input.hotkey[21]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey22("input.hotkey[22]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey23("input.hotkey[23]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey24("input.hotkey[24]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey25("input.hotkey[25]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey26("input.hotkey[26]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey27("input.hotkey[27]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey28("input.hotkey[28]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey29("input.hotkey[29]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey30("input.hotkey[30]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey31("input.hotkey[31]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey32("input.hotkey[32]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey33("input.hotkey[33]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey34("input.hotkey[34]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey35("input.hotkey[35]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey36("input.hotkey[36]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey37("input.hotkey[37]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey38("input.hotkey[38]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey39("input.hotkey[39]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey40("input.hotkey[40]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey41("input.hotkey[41]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey42("input.hotkey[42]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey43("input.hotkey[43]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey44("input.hotkey[44]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey45("input.hotkey[45]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey46("input.hotkey[46]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey47("input.hotkey[47]", Common::Ini::KT_WORD);
Common::Ini::Key IniConf::InputHotkey48("input.hotkey[48]", Common::Ini::KT_WORD);

// Audio Engine
Common::Ini::Key IniConf::AudioChannels("audio.channels", Common::Ini::KT_DIGIT, (int32_t)64);
Common::Ini::Key IniConf::AudioVolume("audio.volume",   Common::Ini::KT_DIGIT, (int32_t)127);
Common::Ini::Key IniConf::AudioNumPalfx("audio.num_palfx", Common::Ini::KT_DIGIT, (int32_t)4);
Common::Ini::Key IniConf::AudioRevStereo("audio.rev_stereo", Common::Ini::KT_BOOL);

// Tform Engine
Common::Ini::Key IniConf::TformBackplane("tform.backplane",  Common::Ini::KT_DIGIT, (int32_t)4096);
Common::Ini::Key IniConf::TformFrontplane("tform.frontplane", Common::Ini::KT_DIGIT, (int32_t)16);
Common::Ini::Key IniConf::TformZoomx("tform.zoomx", Common::Ini::KT_DIGIT, (int32_t)320);
Common::Ini::Key IniConf::TformZoomy("tform.zoomy", Common::Ini::KT_DIGIT, (int32_t)200);

// Windp keys
Common::Ini::Key IniConf::NetGmode("net.gmode",  Common::Ini::KT_DIGIT);
Common::Ini::Key IniConf::NetVersionCheck("net.versioncheck", Common::Ini::KT_BOOL, true);

Common::Ini::Key IniConf::GameDebug("game.debug", Common::Ini::KT_BOOL);
Common::Ini::Key IniConf::GameNewDebug("game.new.debug", Common::Ini::KT_WORD, std::string("no"));
Common::Ini::Key IniConf::GameCrashDiagnostics("game.crash_diagnostics", Common::Ini::KT_BOOL, false);
// OpenNeoUA custom: choose the original VP model preview or the text-only briefing preview.
// The default keeps the current text mode for vanilla-safe behavior.
Common::Ini::Key IniConf::GameBriefingVPRender("game.briefing_vp_render", Common::Ini::KT_BOOL, false);

// Yparobo keys
Common::Ini::Key IniConf::GameNewAI("game.newai",    Common::Ini::KT_BOOL, true);
// OpenNeoUA: frame-rate independent gameplay timing is always enabled.
// This multiplier remains configurable for tank ground-pose response.
// 2.0 is near vanilla alignment; 5.5 is the balanced default; 10.0 is very reactive.
Common::Ini::Key IniConf::GameFixedTickTankGroundPoseMult("game.fixed_tick_tank_ground_pose_mult", Common::Ini::KT_WORD, std::string("5.5"));
// OpenNeoUA custom: player-only tank coast braking after releasing forward/reverse.
// Value is the approximate stop time from canonical top speed, in milliseconds.
// Missing, zero or negative keeps the vanilla endless-coast behavior.
Common::Ini::Key IniConf::GamePlayerTankBrakeTime("game.player_tank_brake_time", Common::Ini::KT_DIGIT, (int32_t)0);
// OpenNeoUA custom: maximum altitude above the current sector terrain for
// player-controlled aerial units. 1600.0 preserves the vanilla limiter.
Common::Ini::Key IniConf::GamePlayerMaxAltitudeAboveGround("game.player_max_altitude_above_ground", Common::Ini::KT_WORD, std::string("1600.0"));
// OpenNeoUA custom: optional absolute AI aerial ceiling above the current sector
// terrain. Missing, zero, negative or invalid preserves vanilla AI behavior.
Common::Ini::Key IniConf::GameAiMaxAltitudeAboveGround("game.ai_max_altitude_above_ground", Common::Ini::KT_WORD, std::string("0.0"));
// OpenNeoUA custom: the player Sprint exists only when all three Sprint values are
// explicitly present in Nucleus.ini. Missing any one of them disables Sprint.
Common::Ini::Key IniConf::GameSprintForceUpPercent("game.sprint_force_up_percent", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameSprintPitchUpPercent("game.sprint_pitch_up_percent", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameSprintRampTime("game.sprint_ramp_time", Common::Ini::KT_WORD, std::string("0"));
// OpenNeoUA custom: one shared percentage of nominal weapon damage converted into
// shooter-side energy cost for normal projectiles, MGUNs and laser (including vertical mode).
// 0 disables the configured drain; missing or invalid preserves each weapon type's
// previous fallback. The shooter's effective shield attenuates the final cost.
Common::Ini::Key IniConf::GameWeaponEnergyCostPercent("game.weapon_energy_cost_percent", Common::Ini::KT_WORD, std::string());
// OpenNeoUA custom: percentage of the unit's maximum energy consumed per second
// while player Sprint is active. Zero keeps Sprint free.
Common::Ini::Key IniConf::GameSprintEnergyCostPercent("game.sprint_energy_cost_percent", Common::Ini::KT_WORD, std::string("0"));
// OpenNeoUA custom: application interval for continuous Sprint energy drain.
// Zero applies the accumulated drain as soon as a whole energy unit is ready.
Common::Ini::Key IniConf::GameSprintEnergyDrainIntervalMs("game.sprint_energy_drain_interval_ms", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameTimeLine("game.timeline", Common::Ini::KT_DIGIT, (int32_t)600000);
Common::Ini::Key IniConf::GameRoboPlayerAIBehavior("game.robo_player_ai_behavior", Common::Ini::KT_BOOL, false);
// OpenNeoUA custom: independent multipliers for the three Host Station resources
// consumed by physical player relocation. These defaults preserve the current
// hardcoded balance when the Nucleus keys are absent.
Common::Ini::Key IniConf::GameRoboMobileMoveEnergyCostMultiplier("game.robo_mobile_move_energy_cost_multiplier", Common::Ini::KT_WORD, std::string("1.0"));
Common::Ini::Key IniConf::GameRoboMobileMainEnergyCostMultiplier("game.robo_mobile_main_energy_cost_multiplier", Common::Ini::KT_WORD, std::string("0.33"));
Common::Ini::Key IniConf::GameRoboMobileBuildEnergyCostMultiplier("game.robo_mobile_build_energy_cost_multiplier", Common::Ini::KT_WORD, std::string("0.33"));
Common::Ini::Key IniConf::GameSpectatorMode("game.spectator_mode", Common::Ini::KT_BOOL, false);
// OpenNeoUA custom: exposes the briefing Play As selector. Disabled by default so
// the Resistance campaign keeps its original presentation and start faction.
Common::Ini::Key IniConf::GamePlayAsOtherFactions("game.play_as_other_factions", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GameWeaponWeaponCollision("game.weapon_weapon_collision", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GameRoboBuildingCollisionDamagePercent("game.robo_building_collision_damage_percent", Common::Ini::KT_DIGIT, (int32_t)0);
// OpenNeoUA custom: raw max-energy percentage exchanged once when two enemy
// non-neutral units begin a physical collision. The target's effective shield
// reduces the final damage. Zero preserves current behavior.
Common::Ini::Key IniConf::GameUnitEnemyCollisionDamagePercent("game.unit_enemy_collision_damage_percent", Common::Ini::KT_DIGIT, (int32_t)0);
// OpenNeoUA custom: same shield-aware collision damage for allied non-neutral units.
// Zero preserves current behavior.
Common::Ini::Key IniConf::GameUnitFriendlyCollisionDamagePercent("game.unit_friendly_collision_damage_percent", Common::Ini::KT_DIGIT, (int32_t)0);
// OpenNeoUA custom: multiplier for the vanilla power-station sector energy effect.
// game.powerstation_energy_multiplier = 1.0 keeps existing power-energy behaviour; higher values scale static and mobile recharge/drain.
Common::Ini::Key IniConf::GamePowerStationEnergyMultiplier("game.powerstation_energy_multiplier", Common::Ini::KT_WORD, std::string("1.0"));
// OpenNeoUA custom: when explicitly present, replaces vanilla fall damage with a
// shield-aware percentage of the unit's maximum energy. Missing/invalid keeps
// the vanilla fall-damage calculation.
Common::Ini::Key IniConf::GameFallDamagePercent("game.fall_damage_percent", Common::Ini::KT_WORD, std::string());
// OpenNeoUA custom: lethal-hit push multiplier, parsed and clamped to 0..10.
Common::Ini::Key IniConf::GamePushAtDeathMultiplier("game.push_at_death_mult", Common::Ini::KT_WORD, std::string("1.0"));
// OpenNeoUA: single Hand Brake intensity for braking strength and the normalized
// recoil/random-spread reduction. Zero disables all three effects; values above
// one may strengthen braking while weapon modifiers remain capped at 100%.
Common::Ini::Key IniConf::GameHandBrakePower("game.handbrake_power", Common::Ini::KT_WORD, std::string("1.0"));
// OpenNeoUA: linear per-session unit stat bonus derived from the existing 0..4 kill marks.
// The runtime clamps the configured per-mark value and never mutates shared prototypes.
Common::Ini::Key IniConf::GameUnitKillStatBonusPercent("game.unit_kill_stat_bonus_percent", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameHandBrakeSound("game.handbrake_sound", Common::Ini::KT_STRING, std::string("sounds/new/handbrake.wav"));
Common::Ini::Key IniConf::GameGemUnlockNewUI("game.gem_unlock_new_ui", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GameGemUnlockSound("game.gem_unlock_sound", Common::Ini::KT_STRING, std::string());
// OpenNeoUA: optional global mission ambience. A level may override both the
// sample/folder and its volume through begin_level. A directory selects one
// supported audio file randomly at each level start. Empty disables ambience.
Common::Ini::Key IniConf::GameAmbientSound("game.ambient_sound", Common::Ini::KT_STRING, std::string());
Common::Ini::Key IniConf::GameAmbientSoundVolume("game.ambient_sound_volume", Common::Ini::KT_WORD, std::string("100"));
// OpenNeoUA: display lifetime of the new GEM-unlock popup only. GEM unlocks no
// longer alter gameplay time. Missing/invalid values preserve 8000 ms.
Common::Ini::Key IniConf::GameGemUnlockDuration("game.gem_unlock_duration", Common::Ini::KT_WORD, std::string("8000"));
// OpenNeoUA: Host Station destruction slowdown. Defaults disable the feature and
// preserve existing gameplay when scale/duration are absent. A zero maximum
// distance keeps the previous unlimited-range behavior.
Common::Ini::Key IniConf::GameRoboDeathTimeScale("game.robo_death_time_scale", Common::Ini::KT_WORD, std::string("1.0"));
Common::Ini::Key IniConf::GameRoboDeathTimeScaleDuration("game.robo_death_time_scale_duration", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameRoboDeathTimeScaleMaxDistance("game.robo_death_time_scale_max_distance", Common::Ini::KT_WORD, std::string("0"));
// OpenNeoUA: single-player multiplier for the lifetime of recoverable death plasma.
// 1.0, missing, zero, negative or malformed preserves the vanilla duration.
// Netgames deliberately remain vanilla to avoid client-side gameplay divergence.
Common::Ini::Key IniConf::GamePlasmaDeathDurationMult("game.plasma_death_duration_mult", Common::Ini::KT_WORD, std::string("1.0"));
// OpenNeoUA: single-player-only automatic attraction of recoverable death plasma
// toward the directly controlled player. Zero or invalid values disable it.
Common::Ini::Key IniConf::GamePlasmaDeathMagnetRadius("game.plasma_death_magnet_radius", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GamePlasmaDeathMagnetSpeed("game.plasma_death_magnet_speed", Common::Ini::KT_WORD, std::string("0"));
// OpenNeoUA: single-player-only Plasma currency accounting. Missing, zero or
// invalid values keep the feature disabled and preserve the vanilla pickup.
Common::Ini::Key IniConf::GamePlasmaCurrencyEnable("game.plasma_currency_enable", Common::Ini::KT_BOOL, false);
// Percentage of each valid residual-plasma reward that is actually credited to
// the global currency reserve. 100 preserves the existing currency scale.
Common::Ini::Key IniConf::GamePlasmaCurrencyGainPercent("game.plasma_currency_gain_percent", Common::Ini::KT_WORD, std::string("100"));
// OpenNeoUA: one positional pickup sound shared by every pickup-capable vehicle.
// An empty or unloadable sample falls back to the vanilla World.ini plasma sample.
// Distance attenuation deliberately stays on the legacy sound path (Radius == 0).
Common::Ini::Key IniConf::GamePlasmaSndPickupSample("game.plasma_snd_pickup_sample", Common::Ini::KT_STRING, std::string());
Common::Ini::Key IniConf::GamePlasmaSndPickupVolume("game.plasma_snd_pickup_volume", Common::Ini::KT_DIGIT, (int32_t)90);
Common::Ini::Key IniConf::GamePlasmaSndPickupPitch("game.plasma_snd_pickup_pitch", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameWorldUiMaxDistance("game.world_ui_max_distance", Common::Ini::KT_WORD, std::string("5700"));
// OpenNeoUA custom: optional global distance for automatic AI target acquisition.
// Zero or an invalid value preserves the vanilla acquisition behavior.
Common::Ini::Key IniConf::GameAiTargetRange("game.ai_target_range", Common::Ini::KT_WORD, std::string("0"));
// OpenNeoUA custom: global MGUN hitscan and AI engagement range. 1000 is vanilla.
Common::Ini::Key IniConf::GameMgunRange("game.mgun_range", Common::Ini::KT_WORD, std::string("1000"));
Common::Ini::Key IniConf::GameMgunAiFireAlignment("game.mgun_ai_fire_alignment", Common::Ini::KT_WORD, std::string("0.85"));
// OpenNeoUA: global temporal envelopes are limited to SHK and PAL.
// Event-specific fade keys are intentionally unsupported; missing, malformed,
// negative or zero means no additional fade. SND keeps its legacy behavior.
Common::Ini::Key IniConf::GameGlobalShkFadeIn("game.global_shk_fade_in", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameGlobalShkFadeOut("game.global_shk_fade_out", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameGlobalPalFadeIn("game.global_pal_fade_in", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GameGlobalPalFadeOut("game.global_pal_fade_out", Common::Ini::KT_WORD, std::string("0"));

// OpenNeoUA custom: procedural HP bar shared by world/HUD. When enabled, the
// world-space Shield bar is omitted while the personal cockpit Shield keeps
// the classic MAPMISC squares. Missing/disabled HP mesh keys preserve the
// full vanilla HP/Shield paths. Active HP tint converges from full_tint toward low_tint as HP decreases.
Common::Ini::Key IniConf::GfxMeshHpBarEnable("gfx.mesh_hp_bar_enable", Common::Ini::KT_BOOL, false);
Common::Ini::Key IniConf::GfxMeshHpBarFullTint("gfx.mesh_hp_bar_full_tint", Common::Ini::KT_WORD, std::string("0_217_81_255"));
Common::Ini::Key IniConf::GfxMeshHpBarLowTint("gfx.mesh_hp_bar_low_tint", Common::Ini::KT_WORD, std::string("255_0_0_255"));
Common::Ini::Key IniConf::GfxMeshHpBarEmptyTint("gfx.mesh_hp_bar_empty_tint", Common::Ini::KT_WORD, std::string("255_0_0_0"));

// OpenNeoUA custom: opt-in regen/drain unit FX. Shared state/VP controls use
// gfx.*_fx_* while procedural-only geometry controls use gfx.*_mesh_*.
// All numeric values use KT_WORD so
// malformed user input can be validated safely by World::EnergyFX instead of
// throwing while Nucleus.ini is parsed. VP/interval/count default to zero, so
// an absent or incomplete profile is fully disabled.
Common::Ini::Key IniConf::GfxRegenFXVP("gfx.regen_fx_vp", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXVPScale("gfx.regen_fx_vp_scale", Common::Ini::KT_WORD, std::string("1.0"));
Common::Ini::Key IniConf::GfxRegenFXVPSpinX("gfx.regen_fx_vp_spin_x", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXVPSpinY("gfx.regen_fx_vp_spin_y", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXVPSpinZ("gfx.regen_fx_vp_spin_z", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXTint("gfx.regen_fx_tint", Common::Ini::KT_WORD, std::string("255_255_255_255"));
Common::Ini::Key IniConf::GfxRegenFXDuration("gfx.regen_fx_duration", Common::Ini::KT_WORD, std::string("1000"));
Common::Ini::Key IniConf::GfxRegenFXInterval("gfx.regen_fx_interval", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXCount("gfx.regen_fx_count", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxRegenFXRandomOffsetPercent("gfx.regen_fx_random_offset_percent", Common::Ini::KT_WORD, std::string("25"));
Common::Ini::Key IniConf::GfxRegenMeshSize("gfx.regen_mesh_size", Common::Ini::KT_WORD, std::string("30"));
Common::Ini::Key IniConf::GfxRegenMeshThickness("gfx.regen_mesh_thickness", Common::Ini::KT_WORD, std::string("5"));
Common::Ini::Key IniConf::GfxRegenMeshRiseSpeed("gfx.regen_mesh_rise_speed", Common::Ini::KT_WORD, std::string("100"));
Common::Ini::Key IniConf::GfxRegenMeshFadeIn("gfx.regen_mesh_fade_in", Common::Ini::KT_WORD, std::string("150"));
Common::Ini::Key IniConf::GfxRegenMeshFadeOut("gfx.regen_mesh_fade_out", Common::Ini::KT_WORD, std::string("300"));

Common::Ini::Key IniConf::GfxDrainFXVP("gfx.drain_fx_vp", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXVPScale("gfx.drain_fx_vp_scale", Common::Ini::KT_WORD, std::string("1.0"));
Common::Ini::Key IniConf::GfxDrainFXVPSpinX("gfx.drain_fx_vp_spin_x", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXVPSpinY("gfx.drain_fx_vp_spin_y", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXVPSpinZ("gfx.drain_fx_vp_spin_z", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXTint("gfx.drain_fx_tint", Common::Ini::KT_WORD, std::string("255_255_255_255"));
Common::Ini::Key IniConf::GfxDrainFXDuration("gfx.drain_fx_duration", Common::Ini::KT_WORD, std::string("1000"));
Common::Ini::Key IniConf::GfxDrainFXInterval("gfx.drain_fx_interval", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXCount("gfx.drain_fx_count", Common::Ini::KT_WORD, std::string("0"));
Common::Ini::Key IniConf::GfxDrainFXRandomOffsetPercent("gfx.drain_fx_random_offset_percent", Common::Ini::KT_WORD, std::string("25"));
Common::Ini::Key IniConf::GfxDrainMeshSize("gfx.drain_mesh_size", Common::Ini::KT_WORD, std::string("30"));
Common::Ini::Key IniConf::GfxDrainMeshThickness("gfx.drain_mesh_thickness", Common::Ini::KT_WORD, std::string("5"));
Common::Ini::Key IniConf::GfxDrainMeshRiseSpeed("gfx.drain_mesh_rise_speed", Common::Ini::KT_WORD, std::string("100"));
Common::Ini::Key IniConf::GfxDrainMeshFadeIn("gfx.drain_mesh_fade_in", Common::Ini::KT_WORD, std::string("150"));
Common::Ini::Key IniConf::GfxDrainMeshFadeOut("gfx.drain_mesh_fade_out", Common::Ini::KT_WORD, std::string("300"));

// OpenNeoUA custom: opt-in Data/-relative paths used by the automatic status-icon
// renderer. Missing, empty or invalid paths disable only that icon category.
Common::Ini::Key IniConf::UiStatusIconRegen("ui.status_icon_regen", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconDrain("ui.status_icon_drain", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconDamaged("ui.status_icon_damaged", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconSpawn("ui.status_icon_spawn", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconRadar("ui.status_icon_radar", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconPower("ui.status_icon_power", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconKamikaze("ui.status_icon_kamikaze", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconInvisible("ui.status_icon_invisible", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconProximityDefense("ui.status_icon_proximity_defense", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconSprint("ui.status_icon_sprint", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconHandbrake("ui.status_icon_handbrake", Common::Ini::KT_STRING);
Common::Ini::Key IniConf::UiStatusIconPlasma(
    "ui.status_icon_plasma", Common::Ini::KT_STRING,
    std::string("Interface/Plasma/owner_{owner}/plasma.png"));
// Number of complete 200 ms on/off blink cycles used both when a dynamic
// status icon appears and when it disappears. Zero keeps the previous
// immediate behavior; runtime clamps the value to 0..10.
Common::Ini::Key IniConf::UiStatusIconBlinkCount("ui.status_icon_blink_count", Common::Ini::KT_DIGIT, (int32_t)0);

// OpenNeoUA custom: render-only tint for Black Sect combat units (owner/faction 5).
// game.black_sect_units_tint = 140_140_140_255 ; R_G_B_A, each component 0..255
Common::Ini::Key IniConf::GameBlackSectUnitsTint("game.black_sect_units_tint", Common::Ini::KT_WORD, std::string("140_140_140_255"));

// Ypaworld keys
Common::Ini::Key IniConf::NetGameExclusiveGem("netgame.exclusivegem", Common::Ini::KT_BOOL, true);
Common::Ini::Key IniConf::NetWaitStart("net.waitstart", Common::Ini::KT_DIGIT, (int32_t)150000);
Common::Ini::Key IniConf::NetKickoff("net.kickoff", Common::Ini::KT_DIGIT, (int32_t)20000);

// Graphics particles
Common::Ini::Key IniConf::GfxParticlesLimit("gfx.particles.limit", Common::Ini::KT_DIGIT, (int32_t)5000);
// OpenNeoUA custom: bounded world-side terrain decals. Zero disables the system;
// the generated mesh uses a fixed internal triangle cap.
Common::Ini::Key IniConf::GfxGroundDecalLimit("gfx.ground_decal_limit", Common::Ini::KT_DIGIT, (int32_t)256);

Common::Ini::Key IniConf::MenuWindowed("menu.windowed", Common::Ini::KT_BOOL, false);

void IniConf::Init()
{
    _varList = {
          &GfxDither
        , &GfxFilter
        , &GfxAntialias
        , &GfxAlpha
        , &GfxZbufWhenTracy
        , &GfxColorkey
        , &GfxForceEmul
        , &GfxForceSoftCursor
        , &GfxAllModes
        , &GfxMoviePlayer
        , &GfxForceAlphaTex
        , &GfxUseDrawPrimitive
        , &GfxDisableLowres
        , &GfxExportWindowMode
        , &GfxBlending
        , &GfxSolidFont
        , &GfxVsync
        , &GfxMaxFps
        , &GfxNewSky
        , &GfxSkyDistance
        , &GfxSkyLength
        , &GfxHorizonFogEnable
        , &GfxHorizonFogStart
        , &GfxHorizonFogLength
        , &GfxHorizonFogStrength
        , &GfxHorizonFogColor
        , &GfxHorizonDarkEnable
        , &GfxHorizonDarkStart
        , &GfxHorizonDarkLength
        , &GfxHorizonDarkStrength
        , &GfxHorizonDarkColor
        , &GfxRenderSectors
        , &GfxSkyHeight
        , &GfxSkyRender
        , &GfxMode
        , &GfxXRes
        , &GfxYRes
        , &GfxPalette
        , &GfxVisualFilter
        , &GfxVisualFilterStrength
        , &GfxAtmosphereFx
        , &GfxAtmosphereStrength
        , &GfxAtmosphereExposure
        , &GfxAtmosphereContrast
        , &GfxAtmosphereSaturation
        , &GfxAtmosphereVignette
        , &GfxDisplay
        , &GfxDisplay2

        , &InputDebug
        , &InputTimer
        , &InputWimp
        , &InputKeyboard
        , &InputButton0
        , &InputButton1
        , &InputButton2
        , &InputButton3
        , &InputButton4
        , &InputButton5
        , &InputButton6
        , &InputButton7
        , &InputButton8
        , &InputButton9
        , &InputButton10
        , &InputButton11
        , &InputButton12
        , &InputButton13
        , &InputButton14
        , &InputButton15
        , &InputButton16
        , &InputButton17
        , &InputButton18
        , &InputButton19
        , &InputButton20
        , &InputButton21
        , &InputButton22
        , &InputButton23
        , &InputButton24
        , &InputButton25
        , &InputButton26
        , &InputButton27
        , &InputButton28
        , &InputButton29
        , &InputButton30
        , &InputButton31
        , &InputSlider0
        , &InputSlider1
        , &InputSlider2
        , &InputSlider3
        , &InputSlider4
        , &InputSlider5
        , &InputSlider6
        , &InputSlider7
        , &InputSlider8
        , &InputSlider9
        , &InputSlider10
        , &InputSlider11
        , &InputSlider12
        , &InputSlider13
        , &InputSlider14
        , &InputSlider15
        , &InputSlider16
        , &InputSlider17
        , &InputSlider18
        , &InputSlider19
        , &InputSlider20
        , &InputSlider21
        , &InputSlider22
        , &InputSlider23
        , &InputSlider24
        , &InputSlider25
        , &InputSlider26
        , &InputSlider27
        , &InputSlider28
        , &InputSlider29
        , &InputSlider30
        , &InputSlider31
        , &InputHotkey0
        , &InputHotkey1
        , &InputHotkey2
        , &InputHotkey3
        , &InputHotkey4
        , &InputHotkey5
        , &InputHotkey6
        , &InputHotkey7
        , &InputHotkey8
        , &InputHotkey9
        , &InputHotkey10
        , &InputHotkey11
        , &InputHotkey12
        , &InputHotkey13
        , &InputHotkey14
        , &InputHotkey15
        , &InputHotkey16
        , &InputHotkey17
        , &InputHotkey18
        , &InputHotkey19
        , &InputHotkey20
        , &InputHotkey21
        , &InputHotkey22
        , &InputHotkey23
        , &InputHotkey24
        , &InputHotkey25
        , &InputHotkey26
        , &InputHotkey27
        , &InputHotkey28
        , &InputHotkey29
        , &InputHotkey30
        , &InputHotkey31
        , &InputHotkey32
        , &InputHotkey33
        , &InputHotkey34
        , &InputHotkey35
        , &InputHotkey36
        , &InputHotkey37
        , &InputHotkey38
        , &InputHotkey39
        , &InputHotkey40
        , &InputHotkey41
        , &InputHotkey42
        , &InputHotkey43
        , &InputHotkey44
        , &InputHotkey45
        , &InputHotkey46
        , &InputHotkey47
        , &InputHotkey48

        , &AudioChannels
        , &AudioVolume
        , &AudioNumPalfx
        , &AudioRevStereo

        , &TformBackplane
        , &TformFrontplane
        , &TformZoomx
        , &TformZoomy

        , &NetGmode
        , &NetVersionCheck

        , &GameDebug
        , &GameNewDebug
        , &GameCrashDiagnostics
        , &GameBriefingVPRender

        , &GameNewAI
        , &GameFixedTickTankGroundPoseMult
        , &GamePlayerTankBrakeTime
        , &GamePlayerMaxAltitudeAboveGround
        , &GameAiMaxAltitudeAboveGround
        , &GameSprintForceUpPercent
        , &GameSprintPitchUpPercent
        , &GameSprintRampTime
        , &GameWeaponEnergyCostPercent
        , &GameSprintEnergyCostPercent
        , &GameSprintEnergyDrainIntervalMs
        , &GameTimeLine
        , &GameRoboPlayerAIBehavior
        , &GameRoboMobileMoveEnergyCostMultiplier
        , &GameRoboMobileMainEnergyCostMultiplier
        , &GameRoboMobileBuildEnergyCostMultiplier
        , &GameSpectatorMode
        , &GamePlayAsOtherFactions
        , &GameWeaponWeaponCollision
        , &GameRoboBuildingCollisionDamagePercent
        , &GameUnitEnemyCollisionDamagePercent
        , &GameUnitFriendlyCollisionDamagePercent
        , &GamePowerStationEnergyMultiplier
        , &GameFallDamagePercent
        , &GamePushAtDeathMultiplier
        , &GameHandBrakePower
        , &GameUnitKillStatBonusPercent
        , &GameHandBrakeSound
        , &GameGemUnlockNewUI
        , &GameGemUnlockSound
        , &GameAmbientSound
        , &GameAmbientSoundVolume
        , &GameGemUnlockDuration
        , &GameRoboDeathTimeScale
        , &GameRoboDeathTimeScaleDuration
        , &GameRoboDeathTimeScaleMaxDistance
        , &GamePlasmaDeathDurationMult
        , &GamePlasmaDeathMagnetRadius
        , &GamePlasmaDeathMagnetSpeed
        , &GamePlasmaCurrencyEnable
        , &GamePlasmaCurrencyGainPercent
        , &GamePlasmaSndPickupSample
        , &GamePlasmaSndPickupVolume
        , &GamePlasmaSndPickupPitch
        , &GameWorldUiMaxDistance
        , &GameAiTargetRange
        , &GameMgunRange
        , &GameMgunAiFireAlignment
        , &GameGlobalShkFadeIn
        , &GameGlobalShkFadeOut
        , &GameGlobalPalFadeIn
        , &GameGlobalPalFadeOut
        , &GfxMeshHpBarEnable
        , &GfxMeshHpBarFullTint
        , &GfxMeshHpBarLowTint
        , &GfxMeshHpBarEmptyTint
        , &GfxRegenFXVP
        , &GfxRegenFXVPScale
        , &GfxRegenFXVPSpinX
        , &GfxRegenFXVPSpinY
        , &GfxRegenFXVPSpinZ
        , &GfxRegenFXTint
        , &GfxRegenFXDuration
        , &GfxRegenFXInterval
        , &GfxRegenFXCount
        , &GfxRegenFXRandomOffsetPercent
        , &GfxRegenMeshSize
        , &GfxRegenMeshThickness
        , &GfxRegenMeshRiseSpeed
        , &GfxRegenMeshFadeIn
        , &GfxRegenMeshFadeOut
        , &GfxDrainFXVP
        , &GfxDrainFXVPScale
        , &GfxDrainFXVPSpinX
        , &GfxDrainFXVPSpinY
        , &GfxDrainFXVPSpinZ
        , &GfxDrainFXTint
        , &GfxDrainFXDuration
        , &GfxDrainFXInterval
        , &GfxDrainFXCount
        , &GfxDrainFXRandomOffsetPercent
        , &GfxDrainMeshSize
        , &GfxDrainMeshThickness
        , &GfxDrainMeshRiseSpeed
        , &GfxDrainMeshFadeIn
        , &GfxDrainMeshFadeOut
        , &UiStatusIconRegen
        , &UiStatusIconDrain
        , &UiStatusIconDamaged
        , &UiStatusIconSpawn
        , &UiStatusIconRadar
        , &UiStatusIconPower
        , &UiStatusIconKamikaze
        , &UiStatusIconInvisible
        , &UiStatusIconProximityDefense
        , &UiStatusIconSprint
        , &UiStatusIconHandbrake
        , &UiStatusIconPlasma
        , &UiStatusIconBlinkCount
        , &GameBlackSectUnitsTint

        , &NetGameExclusiveGem
        , &NetWaitStart
        , &NetKickoff

        , &GfxColorEffects
        , &GfxColorEffPower1
        , &GfxColorEffPower2
        , &GfxColorEffPower3
        , &GfxColorEffPower4
        , &GfxColorEffPower5
        , &GfxColorEffPower6
        , &GfxColorEffPower7
        , &GfxColorEffPower8
        , &GfxColorEffPower9
        , &GfxColorEffPower10
        , &GfxColorEffPower11
        , &GfxColorEffPower12
        , &GfxColorEffPower13
        , &GfxColorEffPower14
        , &GfxColorEffPower15
        , &GfxColorEffPower16

        , &GfxParticlesLimit
        , &GfxGroundDecalLimit

        , &MenuWindowed

        , &GfxAdditionalModes
        , &GfxVBO
        , &GfxVhsFilterShader
        , &GfxVhsFilterShaderVbo
        , &GfxVhsFilterStrength

        , &UiMenuFont
        , &UiRetroInterface
        , &UiMapMarkerSound
        , &UiMoveOrderTemplate
        , &UiAttackOrderTemplate
        , &UiRoboMoveOrderTemplate
    };
}

bool IniConf::ReadFromNucleusIni()
{
    return Common::Ini::ParseIniFile(uaDataFirstNucleusIniPath(), &_varList);
}

bool IniConf::ReadFromIni(const std::string &fname)
{
    return Common::Ini::ParseIniFile(fname, &_varList);
}

bool IniConf::IsGameNewDebugEnabled()
{
    return !StriCmp(GameNewDebug.Get<std::string>(), "yes");
}


}
