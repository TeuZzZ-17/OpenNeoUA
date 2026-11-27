// GFXEngine: rendering back end (SDL_GPU, see gpu.h), render nodes, textures, 2D drawing.
#include "../fmtlib/printf.h"
// Extended unit-icon keys retain the original 256-entry font table.
#include "common/glyph_key.h"

#include <queue>

#include "gfx.h"
#include "common/utils.h"
#include "common/env.h"
#include "common/common.h"
#include "common/ini.h"
#include "../gui/root.h"
#include "nucleus/bitmap.h"
#include "nucleus/ilbm.h"
#include "nucleus/loaders.h"
#include "common/log.h"
#include "gui/font.h"
#include "inivals.h"
#include "gametime.h"
#include "framepacer.h"
#include "world/locations.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace GFX
{
GFXEngine GFXEngine::Instance;

int GFXEngine::can_srcblend;
int GFXEngine::can_destblend;
int GFXEngine::can_stippling;

const SDL_PixelFormatDetails *GFXEngine::_pixfmt = nullptr;
GPU::Format GFXEngine::_texFormat = GPU::Format::RGBA8;
bool GFXEngine::_staticInited = false;


static int32_t GameplayAwareShaderTime()
{
    return System::GameClock.IsActive() ? System::GameClock.VisualTime()
                                        : (int32_t)SDL_GetTicks();
}

static int GameplayAwareShaderRandom()
{
    if ( !System::GameClock.IsActive() )
        return rand();

    // Visual noise is part of the rendered game domain too. Quantize to a
    // nominal ~60 Hz game-time tick so F4 slows changes and F6 freezes them.
    const uint32_t x = MixHash32((uint32_t)(System::GameClock.VisualTime() / 16));
    return (int)(x & 0x7fffffffU);
}

// Vanilla palette effects only. OpenNeoUA custom slots 8..16 are loaded from
// nucleus.ini and intentionally have no hardcoded fallback color.
const std::array<vec3d, 8> GFXEngine::_clrEff
{   vec3d(1.0,  1.0,  1.0)
,   vec3d(1.21, 0.0,  0.29)
,   vec3d(0.13, 0.43, 2.17)
,   vec3d(0.0,  1.60, 1.60)
,   vec3d(1.0,  1.0,  1.0)
,   vec3d(0.57, 0.59, 0.59)
,   vec3d(1.4,  1.08,  1.12)
,   vec3d(0.3,  0.60, 0.7)};

std::vector<TGFXDeviceInfo> GFXEngine::_devices
{
    TGFXDeviceInfo("Opengl", "<primary>")
};

struct HorizonFogConfig
{
    bool ClassicFade = false;
    bool FogEnable = true;
    bool FogStartOverride = false;
    bool FogLengthOverride = false;
    float FogStart = 0.0f;
    float FogLength = 0.0f;
    float FogStrength = 0.8f;
    TColorF FogColor = TColorF(150.0f / 255.0f, 155.0f / 255.0f, 160.0f / 255.0f, 1.0f);

    bool DarkEnable = true;
    bool DarkStartOverride = false;
    bool DarkLengthOverride = false;
    float DarkStart = 0.0f;
    float DarkLength = 0.0f;
    float DarkStrength = 0.65f;
    TColorF DarkColor = TColorF(0.0, 0.0, 0.0, 1.0);
};

static HorizonFogConfig gHorizonFogConfig;

static float HorizonClamp01(float v)
{
    if (v < 0.0f)
        return 0.0f;
    if (v > 1.0f)
        return 1.0f;
    return v;
}

static bool HorizonParseFloat(std::string s, float *out)
{
    s = StrTrim(s);
    if (s.empty())
        return false;

    for (char &c : s)
    {
        if (c == ',')
            c = '.';
    }

    try
    {
        size_t pos = 0;
        float v = std::stof(s, &pos);
        size_t rest = s.find_first_not_of(" \t\r\n", pos);
        if (rest != std::string::npos)
            return false;

        *out = v;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

static float HorizonReadStrength(const std::string &s, float fallback)
{
    float v = fallback;
    if (!HorizonParseFloat(s, &v))
        v = fallback;

    return HorizonClamp01(v);
}

static int HorizonClampColor(int v)
{
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return v;
}

static TColorF HorizonParseColor(std::string s, const TColorF &fallback)
{
    s = StrTrim(s);
    if (s.empty())
        return fallback;

    std::vector<std::string> parts = Stok::Split(s, "|, \t");
    if (parts.size() < 3)
        return fallback;

    try
    {
        int r = HorizonClampColor(std::stoi(parts[0]));
        int g = HorizonClampColor(std::stoi(parts[1]));
        int b = HorizonClampColor(std::stoi(parts[2]));

        return TColorF((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
    }
    catch (...)
    {
        return fallback;
    }
}

static bool ParseColorEffectRgb(std::string s, vec3d *out)
{
    s = StrTrim(s);
    if (s.empty() || !out)
        return false;

    std::vector<std::string> parts = Stok::Split(s, "|, \t");
    if (parts.size() != 3)
        return false;

    int component[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i)
    {
        try
        {
            size_t pos = 0;
            int value = std::stoi(parts[i], &pos, 10);
            if (pos != parts[i].size() || value < 0 || value > 255)
                return false;
            component[i] = value;
        }
        catch (...)
        {
            return false;
        }
    }

    *out = vec3d((double)component[0] / 255.0,
                 (double)component[1] / 255.0,
                 (double)component[2] / 255.0);
    return true;
}

static void HorizonLoadConfigFromIni()
{
    HorizonFogConfig cfg;
    cfg.ClassicFade = System::IniConf::GfxHorizonFadeMode.Get<int32_t>() == 1;

    cfg.FogEnable = System::IniConf::GfxHorizonFogEnable.Get<bool>();
    cfg.FogStartOverride = HorizonParseFloat(System::IniConf::GfxHorizonFogStart.Get<std::string>(), &cfg.FogStart);
    cfg.FogLengthOverride = HorizonParseFloat(System::IniConf::GfxHorizonFogLength.Get<std::string>(), &cfg.FogLength);
    cfg.FogStrength = HorizonReadStrength(System::IniConf::GfxHorizonFogStrength.Get<std::string>(), 0.8f);
    cfg.FogColor = HorizonParseColor(System::IniConf::GfxHorizonFogColor.Get<std::string>(), cfg.FogColor);

    cfg.DarkEnable = System::IniConf::GfxHorizonDarkEnable.Get<bool>();
    cfg.DarkStartOverride = HorizonParseFloat(System::IniConf::GfxHorizonDarkStart.Get<std::string>(), &cfg.DarkStart);
    cfg.DarkLengthOverride = HorizonParseFloat(System::IniConf::GfxHorizonDarkLength.Get<std::string>(), &cfg.DarkLength);
    cfg.DarkStrength = HorizonReadStrength(System::IniConf::GfxHorizonDarkStrength.Get<std::string>(), 0.65f);
    cfg.DarkColor = HorizonParseColor(System::IniConf::GfxHorizonDarkColor.Get<std::string>(), cfg.DarkColor);

    gHorizonFogConfig = cfg;
}

void GFXEngine::ReloadHorizonConfig()
{
    HorizonLoadConfigFromIni();
}


bool TRenderNode::CompareSolid(TRenderNode *a, TRenderNode *b)
{
    if ( !a->Mesh )
        return true;

    if ( !b->Mesh )
        return false;

    return a->Tex < b->Tex;
}

bool TRenderNode::CompareTransparent(TRenderNode* a, TRenderNode* b)
{
    return a->Distance > b->Distance;
}

static float HorizonDefaultAlphaFogStart(float fogStart, float fogLength)
{
    // OpenNeoUA: push the transparent horizon mist farther away from the player.
    // This keeps the atmosphere visible only in the far band instead of in mid-range.
    return fogStart + fogLength * 0.46f;
}

static float HorizonDefaultAlphaFogLength(float fogLength)
{
    // Keep the distant dissolve gradual after moving it farther back.
    return fogLength * 0.90f;
}

static float HorizonAlphaFogStart(float fogStart, float fogLength)
{
    return gHorizonFogConfig.FogStartOverride
         ? gHorizonFogConfig.FogStart
         : HorizonDefaultAlphaFogStart(fogStart, fogLength);
}

static float HorizonAlphaFogLength(float fogLength)
{
    return gHorizonFogConfig.FogLengthOverride
         ? gHorizonFogConfig.FogLength
         : HorizonDefaultAlphaFogLength(fogLength);
}

static float HorizonDarkFogStart(float fogStart, float fogLength)
{
    // Keep the black matte aligned with the far alpha band: less near fog, darker horizon.
    return gHorizonFogConfig.DarkStartOverride
         ? gHorizonFogConfig.DarkStart
         : HorizonDefaultAlphaFogStart(fogStart, fogLength);
}

static float HorizonDarkFogLength(float fogLength)
{
    // Reach black faster inside the far band, so the edge reads as dark horizon
    // instead of bright sky-colored mist.
    return gHorizonFogConfig.DarkLengthOverride
         ? gHorizonFogConfig.DarkLength
         : HorizonDefaultAlphaFogLength(fogLength) * 0.22f;
}

static bool HorizonAlphaFogEnabled(float fogLength)
{
    return gHorizonFogConfig.FogEnable
        && gHorizonFogConfig.FogStrength > 0.0f
        && HorizonAlphaFogLength(fogLength) > 0.0f;
}

static bool HorizonDarkFogEnabled(float fogLength)
{
    return gHorizonFogConfig.DarkEnable
        && gHorizonFogConfig.DarkStrength > 0.0f
        && HorizonDarkFogLength(fogLength) > 0.0f;
}

static float HorizonFogFactor(const vec3d &viewPos, float start, float length)
{
    if (length <= 0.0f)
        return 0.0f;

    // Classic restores the original linear camera-depth fade. Smooth mode
    // retains the radial horizon blend used by existing OpenNeoUA profiles.
    if (gHorizonFogConfig.ClassicFade)
        return HorizonClamp01(((float)viewPos.z - start) / length);
    const float radialDistance = (float)sqrt(viewPos.x * viewPos.x +
                                             viewPos.z * viewPos.z);
    float t = HorizonClamp01((radialDistance - start) / length);
    return t * t * (3.0f - 2.0f * t);
}

bool TRenderParams::operator==(const TRenderParams &b)
{
    if (Flags != b.Flags)
        return false;

    if (Flags & RFLAGS_DYNAMIC_TEXTURE)
        return TexSource->IsSameRes( b.TexSource );

    return Tex == b.Tex;
}

void GFXEngine::StaticInit()
{
    if (_staticInited)
        return;

    _staticInited = true;

    // Surfaces keep the desktop's byte order; textures use the matching GPU format.
    const SDL_DisplayMode *curr = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    switch(curr ? curr->format : SDL_PIXELFORMAT_UNKNOWN)
    {
        case SDL_PIXELFORMAT_XRGB8888:
        case SDL_PIXELFORMAT_ARGB8888:
            _pixfmt = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_ARGB8888 );
            _texFormat = GPU::Format::BGRA8;
            break;

        case SDL_PIXELFORMAT_XBGR8888:
        case SDL_PIXELFORMAT_ABGR8888:
        default:
            _pixfmt = SDL_GetPixelFormatDetails( SDL_PIXELFORMAT_ABGR8888 );
            _texFormat = GPU::Format::RGBA8;
            break;
    }
}

GFXEngine::GFXEngine()
{
    for(TileMap *&t : _tiles)
        t = nullptr;

    for(SDL_Color &c : _palette)
        c = {0, 0, 0, 0};

    _forcesoftcursor = 0;
    _field_38 = 0;
    _txt16bit = 0;
    _use_simple_d3d = 0;
    _disable_lowres = 0;
    _export_window_mode = 0;
    _flags = 0;

    _dither = 0;
    _filter = 0;
    _antialias = 0;
    _alpha = 255;
    _zbuf_when_tracy = 0;
    _colorkey = 0;

    _sceneBeginned = 0;

    _corrIW = _corrW = 1.0;
    _corrIH = _corrH = 1.0;

    _solidFont = true;

    _setFrustumClip(1.0f, WORLD_FAR_CLIP);

    _normClr = vec3d(1.0, 1.0, 1.0);
    _invClr = vec3d(0.0, 0.0, 0.0);
}

std::string read_guid(const std::string &filename)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(filename, "r");
    if ( !fil )
        return "";

    std::string guid;
    fil->ReadLine(&guid);
    delete fil;
    return guid;
}

bool out_guid_to_file(const std::string &filename, const std::string &name)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(filename, "w");
    if ( !fil )
        return false;

    fil->puts(name);
    delete fil;
    return true;
}

void out_yes_no_status(const char *filename, int val)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(filename, "w");
    if ( fil )
    {
        if ( val )
            fil->puts("yes");
        else
            fil->puts("no");
        delete fil;
    }
}

void GFXEngine::DrawTextEntry(const ScreenText *txt)
{
    if ( _font.ttfFont )
    {
        if ( txt->flag & 0x20 )
        {
            _font.r = txt->p1;
            _font.g = txt->p2;
            _font.b = txt->p3;
        }
        else
        {
            if (!txt->string.empty())
            {

                int cx = 0, cy = 0;

                if ( txt->flag & 0xE )
                {
                    TTF_GetStringSize(_font.ttfFont, txt->string.c_str(), 0, &cx, &cy);
                }

                int p1 = txt->p1;
                int p2 = txt->p2;
                int p3 = txt->p3;
                int p4 = txt->p4;

                if ( txt->flag & 8 )
                    p3 = cx * p3 / 100;

                SDL_Rect clipRect;

                clipRect.x = p1;
                clipRect.w = p3 + 4;
                clipRect.h = p4 + 1;
                clipRect.y = p2;

                if ( txt->flag & 2 )
                {
                    if ( txt->flag & 8 )
                    {
                        p1 -= cx;
                        clipRect.x = p1;
                        clipRect.w = p3 + 4;
                    }
                    else
                    {
                        p1 += (p3 - cx);
                    }
                }
                else if ( txt->flag & 4 )
                {
                    if ( txt->flag & 8 )
                    {
                        p1 -= cx / 2;
                        clipRect.x = p1;
                        clipRect.w = p3 + 4;
                    }
                    else
                    {
                        p1 += (p3 - cx) / 2;
                    }
                }

                SDL_SetSurfaceClipRect(Screen(), &clipRect);


                int v10 = ((p4 - _font.height) / 2) - 2 + p2;
                if ( txt->flag & 0x10 )
                {
                    v10++;
                    p1++;
                }

                SDL_Color clr;
                clr.a = 255;
                clr.r = 0;
                clr.g = 0;
                clr.b = 0;

                SDL_Surface *tmp;

                if (_solidFont)
                {
                    tmp = TTF_RenderText_Solid(_font.ttfFont, txt->string.c_str(), 0, clr);
                }
                else
                {
                    tmp = TTF_RenderText_Blended(_font.ttfFont, txt->string.c_str(), 0, clr);
                }

                if (!tmp)
                {
                    SDL_SetSurfaceClipRect(Screen(), nullptr);
                    return;
                }

                SDL_SetSurfaceBlendMode(tmp,
                    _solidFont ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);

                SDL_Rect want;
                want.w = tmp->w;
                want.h = tmp->h;
                want.x = p1 + 2;
                want.y = v10 + 1;

                SDL_BlitSurface(tmp, nullptr, Screen(), &want);

                clr.a = 255;
                clr.r = _font.r;
                clr.g = _font.g;
                clr.b = _font.b;

                if (_solidFont)
                {
                    SDL_SetPaletteColors(SDL_GetSurfacePalette(tmp), &clr, 1, 1);
                }
                else
                {
                    SDL_DestroySurface(tmp);
                    tmp = TTF_RenderText_Blended(_font.ttfFont, txt->string.c_str(), 0, clr);
                    if (!tmp)
                    {
                        SDL_SetSurfaceClipRect(Screen(), nullptr);
                        return;
                    }

                    SDL_SetSurfaceBlendMode(tmp, SDL_BLENDMODE_BLEND);
                }

                want.w = tmp->w;
                want.h = tmp->h;
                want.x = p1 + 1;
                want.y = v10;

                SDL_BlitSurface(tmp, nullptr, Screen(), &want);
                SDL_DestroySurface(tmp);

                SDL_SetSurfaceClipRect(Screen(), nullptr);
            }
        }
    }
}

void GFXEngine::AddScreenText(const std::string &string, int p1, int p2, int p3, int p4, int flag)
{
    ScreenText entry;
    entry.string = string;
    entry.p1 = p1;
    entry.p2 = p2;
    entry.p3 = p3;
    entry.p4 = p4;
    entry.flag = flag;

    _font.entries.push_back(std::move(entry));
}

void GFXEngine::DrawScreenText()
{
    _font.r = 255;
    _font.g = 255;
    _font.b = 0;

    for (const ScreenText &entry : _font.entries)
        DrawTextEntry(&entry);

    _font.entries.clear();
}

void GFXEngine::ClearLocationTitle()
{
    if (_locationTitle) SDL_DestroySurface(_locationTitle);
    if (_locationLabel) SDL_DestroySurface(_locationLabel);
    if (_locationFont) TTF_CloseFont(_locationFont);
    if (_locationLabelFont) TTF_CloseFont(_locationLabelFont);
    _locationTitle=nullptr; _locationLabel=nullptr;
    _locationFont=nullptr; _locationLabelFont=nullptr;
    _locationTitleName.clear();
    _locationTitleColor={0,0,0,0};
    _locationCanvasWidth=_locationCanvasHeight=0;
}

void GFXEngine::DrawLocationTitle(const std::string &name, const SDL_Color &color, float opacity)
{
    SDL_Surface *canvas=Screen();
    if (!canvas || opacity<=0) return;
    if (name!=_locationTitleName || color.r!=_locationTitleColor.r ||
        color.g!=_locationTitleColor.g || color.b!=_locationTitleColor.b ||
        canvas->w!=_locationCanvasWidth || canvas->h!=_locationCanvasHeight) {
        ClearLocationTitle();
        _locationTitleName=name;
        _locationTitleColor=color;
        _locationCanvasWidth=canvas->w; _locationCanvasHeight=canvas->h;
        const std::string display=World::LocationDisplayName(name);
        const int available=std::max(1,canvas->w-48);
        int size=std::max(14,int(canvas->h*.076));
        // Cache a fitted glyph surface; animation only changes its opacity.
        for (;size>=6;--size) {
            _locationFont=System::LoadFont("xolonium bold",size);
            if (!_locationFont) break;
            int width=0, height=0;
            if (TTF_GetStringSize(_locationFont, display.c_str(), 0, &width, &height) && width<=available) break;
            TTF_CloseFont(_locationFont); _locationFont=nullptr;
        }
        _locationLabelFont=System::LoadFont("xolonium bold",std::max(10,int(canvas->h*.023)));
        TTF_Font *titleFont=_locationFont ? _locationFont : _font.ttfFont;
        TTF_Font *labelFont=_locationLabelFont ? _locationLabelFont : _font.ttfFont;
        if (titleFont) _locationTitle=TTF_RenderText_Blended(titleFont, display.c_str(), 0, color);
        if (labelFont) _locationLabel=TTF_RenderText_Blended(labelFont, "LOCATION", 0, SDL_Color{144,185,194,255});
    }
    const uint8_t alpha=uint8_t(std::min(1.f,opacity)*255);
    const int top=int(canvas->h*.19);
    auto blit=[&](SDL_Surface *surface,int y,bool shadow) {
        if (!surface) return;
        SDL_SetSurfaceBlendMode(surface,SDL_BLENDMODE_BLEND);
        const float scale=std::min(1.f,float(std::max(1,canvas->w-48))/surface->w);
        const int width=std::max(1,int(surface->w*scale)), height=std::max(1,int(surface->h*scale));
        SDL_Rect target{(canvas->w-width)/2,y,width,height};
        if (shadow) {
            SDL_SetSurfaceColorMod(surface,0,0,0);
            SDL_SetSurfaceAlphaMod(surface,uint8_t(alpha*.63f));
            ++target.x; target.y+=2;
            SDL_BlitSurfaceScaled(surface, nullptr, canvas, &target, SDL_SCALEMODE_NEAREST);
            --target.x; target.y-=2;
            SDL_SetSurfaceColorMod(surface,255,255,255);
        }
        SDL_SetSurfaceAlphaMod(surface,alpha);
        SDL_BlitSurfaceScaled(surface, nullptr, canvas, &target, SDL_SCALEMODE_NEAREST);
    };
    blit(_locationLabel,top,false);
    blit(_locationTitle,top+28,true);
    const int y=top+94;
    DrawLine(canvas,Common::Line(int(canvas->w*.3),y,int(canvas->w*.48),y),128,175,183,alpha,_virtualUiPass);
    DrawLine(canvas,Common::Line(int(canvas->w*.52),y,int(canvas->w*.7),y),128,175,183,alpha,_virtualUiPass);
}

int GFXEngine::MeasureScreenTextWidth(const std::string &text) const
{
    int width = 0;
    int height = 0;

    if ( _font.ttfFont && TTF_GetStringSize(_font.ttfFont, text.c_str(), 0, &width, &height) )
        return width;

    return 0;
}

void GFXEngine::initPolyEngine()
{
    _states = GfxStates();
    _states.LinearFilter = (_filter != 0);

    SetRenderStates(1);
}

int GFXEngine::LoadFontByDescr(const std::string &fontname)
{
    std::vector<std::string> splt = Stok::Split(fontname, ",");

    std::string facename;
    std::string s_height;

    if (splt.size() > 0)
        facename = splt[0];

    if (splt.size() > 1)
        s_height = splt[1];

    int height;
    if ( !facename.empty() && !s_height.empty() )
    {
        height = std::stoi(s_height);
    }
    else
    {
        height = 12;
        facename = "MS Sans Serif";
    }

    if ( _font.ttfFont )
    {
        TTF_CloseFont(_font.ttfFont);
        _font.ttfFont = nullptr;
    }

    _font.height = height;
    _font.ttfFont = System::LoadFont(facename, height);

    if ( _font.ttfFont )
    {
        if (!_solidFont)
            TTF_SetFontHinting(_font.ttfFont, TTF_HINTING_LIGHT);
        else
            TTF_SetFontHinting(_font.ttfFont, TTF_HINTING_MONO);

        return 1;
    }

    printf("Can't load font\n");

    return 0;
}


size_t GFXEngine::windd_func0(IDVList &stak)
{
    int txt16bit_def = read_yes_no_status("env/txt16bit.def", 1);
    int drawprim_def = read_yes_no_status("env/drawprim.def", 0);
    _export_window_mode = System::IniConf::GfxExportWindowMode.Get<bool>();     // gfx.export_window_mode

    switch(System::IniConf::GfxBlending.Get<int>())
    {
        case 0:
        {
            can_srcblend = 1;
            can_destblend = 0;
            can_stippling = 0;
        }
        break;

        default:
        case 1:
        {
            can_srcblend = 1;
            can_destblend = 1;
            can_stippling = 0;
        }
        break;

        case 2:
        {
            can_srcblend = 0;
            can_destblend = 0;
            can_stippling = 1;
        }
        break;
    }

    SetResVariables( Common::Point(stak.Get<int32_t>(ATT_WIDTH, DEFAULT_WIDTH), stak.Get<int32_t>(ATT_HEIGHT, DEFAULT_HEIGHT)) );

    _forcesoftcursor = 0;
    _disable_lowres = System::IniConf::GfxDisableLowres.Get<bool>();
    _txt16bit = txt16bit_def;
    _use_simple_d3d = drawprim_def;

    _solidFont = System::IniConf::GfxSolidFont.Get<bool>();

    // 0 off, 1 on, 2 adaptive (mailbox when the display offers it).
    GPU::SetVSync(System::IniConf::GfxVsync.Get<int>());

    System::Frames.SetFrameRateLimit(System::NormalizeFrameRateLimit(System::IniConf::GfxMaxFps.Get<int32_t>()));

    LoadFontByDescr("MS Sans Serif,12,400,0");

    return 1;
}


bool GFXEngine::SetResVariables(Common::Point res)
{
    _resolution = res;   //stak.Get<int32_t>(ATT_WIDTH, 0);

    _clip = _resolution - Common::Point(1, 1);

    _field_54c = _resolution.x / 2;
    _field_550 = _resolution.y / 2;

    _field_554 = _resolution.x / 2;
    _field_558 = _resolution.y / 2;

    return true;
}

size_t GFXEngine::func0(IDVList &stak)
{
    System::IniConf::ReadFromNucleusIni();

    if ( !windd_func0(stak) )
        return 0;

    _dither = System::IniConf::GfxDither.Get<bool>();
    _filter = System::IniConf::GfxFilter.Get<bool>();
    _antialias = System::IniConf::GfxAntialias.Get<bool>();
    _zbuf_when_tracy = System::IniConf::GfxZbufWhenTracy.Get<bool>();
    _colorkey = System::IniConf::GfxColorkey.Get<bool>();

    if ( can_srcblend )
        _alpha = 192;
    else
        _alpha = 128;

    ApplyResolution();

    return 1;
}

void GFXEngine::ApplyResolution()
{
    if ( (float)_resolution.x / (float)_resolution.y >= 1.4 )
    {
        int half = (_resolution.x + _resolution.y) / 2;
        _corrW = (float)half * 1.1429 / (float)_resolution.x;
        _corrH = (float)half * 0.85715 / (float)_resolution.y;
        _corrIW = 1.0 / _corrW;
        _corrIH = 1.0 / _corrH;
    }
    else //No correction
    {
        _corrIW = _corrW = 1.0;
        _corrIH = _corrH = 1.0;
    }

    initPolyEngine();
}


size_t GFXEngine::raster_func198(const Common::FLine &arg)
{
    float tX = _field_554 - 1.0;
    float tY = _field_558 - 1.0;

    int y1 = (arg.y1 + 1.0) * tY;
    int y2 = (arg.y2 + 1.0) * tY;
    int x1 = (arg.x1 + 1.0) * tX;
    int x2 = (arg.x2 + 1.0) * tX;

    DrawLine(Screen(),
                      Common::Line(x1, y1, x2, y2),
                      _field_4.r,
                      _field_4.g,
                      _field_4.b,
                      _field_4.a,
                      _virtualUiPass);
    return 1;
}


size_t GFXEngine::raster_func199(const Common::Line &arg)
{
    DrawLine(Screen(),
                      Common::Line(_field_54c + arg.x1, _field_550 + arg.y1,
                                   _field_54c + arg.x2, _field_550 + arg.y2),
                      _field_4.r,
                      _field_4.g,
                      _field_4.b,
                      _field_4.a,
                      _virtualUiPass );

    return 1;
}

void GFXEngine::sub_420EDC(Common::Line line, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha)
{
    if ( line.ClipBy(_clip) )
    {
        Common::Line tmp2 = line;

        if ( _inverseClip.IsEmpty() || !tmp2.ClipBy(_inverseClip) )
        {
            DrawLine(Screen(), line, r, g, b, alpha, _virtualUiPass);
        }
        else
        {
            if ( tmp2.P2() != line.P2() )
            {
                if ( tmp2.P1() != line.P1() )
                {
                    DrawLine(Screen(), Common::Line(line.P1(), tmp2.P1()), r, g, b, alpha, _virtualUiPass);
                    DrawLine(Screen(), Common::Line(tmp2.P2(), line.P2()), r, g, b, alpha, _virtualUiPass);
                }
                else
                {
                    DrawLine(Screen(), Common::Line(tmp2.P2(), line.P2()), r, g, b, alpha, _virtualUiPass);
                }
            }
            else
            {
                DrawLine(Screen(), Common::Line(line.P1(), tmp2.P1()), r, g, b, alpha, _virtualUiPass);
            }
        }
    }
}

size_t GFXEngine::raster_func200(const Common::FLine &arg)
{
    float tX = _field_554 - 1.0;
    float tY = _field_558 - 1.0;

    sub_420EDC (Common::Line( (arg.x1 + 1.0) * tX
                            , (arg.y1 + 1.0) * tY
                            , (arg.x2 + 1.0) * tX
                            , (arg.y2 + 1.0) * tY),
                        _field_4.r,
                        _field_4.g,
                        _field_4.b,
                        _field_4.a);

    return 1;
}

size_t GFXEngine::raster_func201(const Common::Line &l)
{
    sub_420EDC( Common::Line( _field_54c + l.x1
                            , _field_550 + l.y1
                            , _field_54c + l.x2
                            , _field_550 + l.y2),
                        _field_4.r,
                        _field_4.g,
                        _field_4.b,
                        _field_4.a );

    return 1;
}

size_t GFXEngine::raster_func202(rstr_arg204 *arg)
{
    Common::Rect r1;
    r1.left   = (arg->float4.left   + 1.0) * (arg->pbitm->width / 2);
    r1.top    = (arg->float4.top    + 1.0) * (arg->pbitm->height / 2);
    r1.right  = (arg->float4.right  + 1.0) * (arg->pbitm->width / 2);
    r1.bottom = (arg->float4.bottom + 1.0) * (arg->pbitm->height / 2);

    Common::Rect r2;
    r2.left   = _field_554 * (arg->float14.left   + 1.0);
    r2.top    = _field_558 * (arg->float14.top    + 1.0);
    r2.right  = _field_554 * (arg->float14.right  + 1.0);
    r2.bottom = _field_558 * (arg->float14.bottom + 1.0);

    SDL_Rect src = r1;
    SDL_Rect dst = r2;

    SDL_BlitSurfaceScaled(arg->pbitm->swTex, &src, Screen(), &dst, SDL_SCALEMODE_NEAREST);

    return 1;
}

size_t GFXEngine::BlitPixelRect(ResBitmap *pbitm, Common::Rect r1, Common::Rect r2, uint8_t opacity)
{
    if ( !pbitm || !pbitm->swTex || r1.IsEmpty() || r2.IsEmpty() )
        return 0;

    if ( _clip.IsIntersects(r2) )
    {
        if ( r2.left < _clip.left )
        {
            r1.left += (_clip.left - r2.left) * r1.Width() / r2.Width();
            r2.left = _clip.left;
        }

        if ( r2.right > _clip.right )
        {
            r1.right += (_clip.right - r2.right) * r1.Width() / r2.Width();
            r2.right = _clip.right;
        }

        if ( r2.top < _clip.top )
        {
            r1.top += (_clip.top - r2.top) * r1.Height() / r2.Height();
            r2.top = _clip.top;
        }

        if ( r2.bottom > _clip.bottom )
        {
            r1.bottom += (_clip.bottom - r2.bottom) * r1.Height() / r2.Height();
            r2.bottom = _clip.bottom;
        }

        SDL_Rect src = r1;
        SDL_Rect dst = r2;
        if (opacity == 255)
        {
            SDL_BlitSurfaceScaled(pbitm->swTex, &src, Screen(), &dst, SDL_SCALEMODE_NEAREST);
            return 1;
        }

        uint8_t oldOpacity = 255;
        SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;
        SDL_GetSurfaceAlphaMod(pbitm->swTex, &oldOpacity);
        SDL_GetSurfaceBlendMode(pbitm->swTex, &oldBlendMode);
        SDL_SetSurfaceAlphaMod(pbitm->swTex, opacity);
        SDL_SetSurfaceBlendMode(pbitm->swTex, SDL_BLENDMODE_BLEND);
        SDL_BlitSurfaceScaled(pbitm->swTex, &src, Screen(), &dst, SDL_SCALEMODE_NEAREST);
        SDL_SetSurfaceAlphaMod(pbitm->swTex, oldOpacity);
        SDL_SetSurfaceBlendMode(pbitm->swTex, oldBlendMode);
    }

    return 1;
}

size_t GFXEngine::raster_func204(rstr_arg204 *arg)
{
    Common::Rect r1;
    r1.left   = (arg->float4.left   + 1.0) * (arg->pbitm->width / 2);
    r1.top    = (arg->float4.top    + 1.0) * (arg->pbitm->height / 2);
    r1.right  = (arg->float4.right  + 1.0) * (arg->pbitm->width / 2);
    r1.bottom = (arg->float4.bottom + 1.0) * (arg->pbitm->height / 2);

    Common::Rect r2;
    r2.left   = _field_554 * (arg->float14.left   + 1.0);
    r2.top    = _field_558 * (arg->float14.top    + 1.0);
    r2.right  = _field_554 * (arg->float14.right  + 1.0);
    r2.bottom = _field_558 * (arg->float14.bottom + 1.0);

    return BlitPixelRect(arg->pbitm, r1, r2, arg->opacity);
}

// Copies the render states the standard shader reads into its uniforms. Pipeline
// states (blend, depth, colour writes) are taken by each draw from _states.
void GFXEngine::SetRenderStates(int setAll)
{
    const GfxStates &s = (setAll < 2) ? _states : _lastStates;

    _stdParams.Textured = s.Tex ? 1 : 0;
    _stdParams.Flat = s.Shaded ? 0 : 1;
    _stdParams.ATest = s.AlphaTest ? 1 : 0;

    if (s.Fog)
    {
        _stdParams.Fog = 1.0;
        _stdParams.FogStart = s.FogStart;
        _stdParams.FogLength = s.FogLength;
        _stdParams.FogStrength = s.FogStrength;
        _stdParams.FogColor[0] = s.FogColor.r;
        _stdParams.FogColor[1] = s.FogColor.g;
        _stdParams.FogColor[2] = s.FogColor.b;
        _stdParams.FogColor[3] = 1.0;
    }
    else
        _stdParams.Fog = 0.0;

    if (s.AFog)
    {
        _stdParams.AFog = (float)s.AFog;
        _stdParams.AFogStart = s.AFogStart;
        _stdParams.AFogLength = s.AFogLength;
        _stdParams.AFogStrength = s.AFogStrength;
        _stdParams.AtmosphereColor[0] = s.AFogColor.r;
        _stdParams.AtmosphereColor[1] = s.AFogColor.g;
        _stdParams.AtmosphereColor[2] = s.AFogColor.b;
        _stdParams.AtmosphereColor[3] = 1.0;
    }
    else
        _stdParams.AFog = 0.0;

    if (setAll < 2)
        _lastStates = _states;
}

GPU::DrawState GFXEngine::StdDrawState(GPU::Layout layout) const
{
    GPU::DrawState state;
    state.program = _states.Prog;
    state.layout = layout;
    state.blend = _states.AlphaBlend;
    state.src = _states.SrcBlend;
    state.dst = _states.DstBlend;
    state.depthTest = _states.DepthTest;
    state.depthWrite = _states.Zwrite;
    state.colorWrite = _colorWrite;
    return state;
}

// Fog of a node: the same result for meshes and for the decals painted on them.
void GFXEngine::ApplyFogStates(uint32_t flags, float fogStart, float fogLength)
{
    if ( (flags & RFLAGS_FOG) && !(flags & RFLAGS_SKY) )
    {
        _states.Fog = true;
        _states.FogStart = fogStart;
        _states.FogLength = fogLength;
        _states.FogStrength = 1.0f;
        _states.FogColor = TColorF(0.0, 0.0, 0.0, 1.0);
    }

    if ((flags & RFLAGS_ALPHA_FOG) && (flags & RFLAGS_FOG) && !(flags & RFLAGS_SKY) && fogLength > 0.0f)
    {
        const bool atmosphereFog = HorizonAlphaFogEnabled(fogLength);
        const bool darkFog = HorizonDarkFogEnabled(fogLength);

        _states.Fog = false;
        _states.AFog = (atmosphereFog || darkFog)
                     ? (gHorizonFogConfig.ClassicFade ? 2 : 1) : 0;

        if (_states.AFog)
        {
            _states.AFogStart = HorizonAlphaFogStart(fogStart, fogLength);
            _states.AFogLength = HorizonAlphaFogLength(fogLength);
            _states.AFogStrength = atmosphereFog ? gHorizonFogConfig.FogStrength : 0.0f;
            _states.AFogColor = gHorizonFogConfig.FogColor;
        }

        if (darkFog)
        {
            _states.Fog = true;
            _states.FogStart = HorizonDarkFogStart(fogStart, fogLength);
            _states.FogLength = HorizonDarkFogLength(fogLength);
            _states.FogStrength = gHorizonFogConfig.DarkStrength;
            _states.FogColor = gHorizonFogConfig.DarkColor;
        }

        // Atmosphere V2 blends RGB and preserves the original alpha/depth.
        // Do not force transparent sorting/blending for opaque world geometry.
    }
}

void GFXEngine::RenderingMesh(TRenderNode *nod)
{
    if ( !_sceneBeginned )
        return;

    if (!nod)
        return;

    TMesh *mesh = nod->Mesh;

    if (!mesh || !mesh->gpuVertexBuf || !mesh->gpuIndexBuf)
        return;

    uint32_t flags = nod->Flags;

    _states.Shaded = false;
    _states.SrcBlend = GPU::Blend::One;
    _states.DstBlend = GPU::Blend::Zero;
    _states.AlphaBlend = false;
    _states.Zwrite = true;
    _states.Tex = 0;
    _states.LinearFilter = (_filter != 0);
    _states.Fog = false;
    _states.AlphaTest = false;
    _states.AFog = false;
    _states.Prog = _stdShaderProg;

    if ( flags & RFLAGS_TEXTURED )
    {
        if (nod->Tex)
            _states.Tex = nod->Tex->hwTex;
    }

    if ( flags & RFLAGS_SHADED )
    {
        _states.Shaded = true;
    }

    if ( flags & RFLAGS_LUMTRACY )
    {
        if ( !_zbuf_when_tracy )
            _states.Zwrite = false;

        if ( can_destblend )
        {
            _states.AlphaBlend = true;
            _states.SrcBlend = GPU::Blend::One;
            _states.DstBlend = GPU::Blend::One;
            _states.Shaded = false;
        }
        else if ( can_srcblend )
        {
            _states.AlphaBlend = true;
            _states.SrcBlend = GPU::Blend::SrcAlpha;
            _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
            _states.Shaded = false;
        }
        else if ( can_stippling )
        {
            _states.AlphaBlend = true;
            _states.SrcBlend = GPU::Blend::SrcAlpha;
            _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
            _states.Shaded = false;
        }
    }
    else if ( flags & RFLAGS_ZEROTRACY )
    {
        _states.AlphaTest = true;

        if ( _pixfmt->bytes_per_pixel != 1 )
        {
            _states.SrcBlend = GPU::Blend::SrcAlpha;
            _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
        }

        _states.AlphaBlend = true;
        _states.LinearFilter = false;
    }
    else if ( flags & RFLAGS_ALPHABLEND )
    {
        _states.AlphaBlend = true;
        _states.SrcBlend = GPU::Blend::SrcAlpha;
        _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
    }

    if (flags & RFLAGS_SKY)
        _states.Shaded = true;

    if (flags & RFLAGS_DISABLE_ZWRITE)
        _states.Zwrite = false;

    ApplyFogStates(flags, nod->FogStart, nod->FogLength);

    TColorF effectiveColorMul = nod->ColorMul;
    float vpFadeFactor = nod->VPFadeFactor;
    if ( !std::isfinite(vpFadeFactor) )
        vpFadeFactor = 1.0f;
    vpFadeFactor = std::max(0.0f, std::min(vpFadeFactor, 1.0f));

    // See RenderingMeshOld(): additive LUMTRACY ignores source alpha with
    // GPU::Blend::One/GPU::Blend::One, so only transient VP fades attenuate RGB in that path.
    if ( (flags & RFLAGS_LUMTRACY) && can_destblend && vpFadeFactor < 1.0f )
    {
        effectiveColorMul.r *= vpFadeFactor;
        effectiveColorMul.g *= vpFadeFactor;
        effectiveColorMul.b *= vpFadeFactor;
    }

    // OpenNeoUA custom VP tint: enable a local alpha blend when the tint lowers alpha.
    if ( effectiveColorMul.a < 1.0 && !_states.AlphaBlend )
    {
        _states.AlphaBlend = true;
        _states.SrcBlend = GPU::Blend::SrcAlpha;
        _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
    }

    SetRenderStates(0);

    SetModelViewMatrix(nod->TForm);

    // OpenNeoUA custom VP tint: the per-node colour multiplier of the shader.
    _stdParams.Colorize = nod->Colorize ? 1 : 0;
    _stdParams.ColorMul[0] = effectiveColorMul.r;
    _stdParams.ColorMul[1] = effectiveColorMul.g;
    _stdParams.ColorMul[2] = effectiveColorMul.b;
    _stdParams.ColorMul[3] = effectiveColorMul.a;

    GPU::Streams streams;
    streams.buffer = mesh->gpuVertexBuf;
    streams.pos = offsetof(TVertex, Pos);
    streams.color = (flags & RFLAGS_COMPUTED_COLOR) ? offsetof(TVertex, ComputedColor) : offsetof(TVertex, Color);
    streams.uv = offsetof(TVertex, TexCoord);

    GPU::Layout layout = GPU::Layout::Mesh;
    if ( (flags & RFLAGS_TEXTURED) && (flags & RFLAGS_DYNAMIC_TEXTURE) && nod->coordsID >= 0 )
    {
        layout = GPU::Layout::MeshCachedUV;
        streams.uv = mesh->CoordsCache.at( nod->coordsID ).BufferPos;
    }

    const GPU::Sampler sampler = {_states.Tex, _states.LinearFilter};
    GPU::Draw(StdDrawState(layout), streams, mesh->gpuIndexBuf, sizeof(IndexType) == 2,
              (uint32_t)mesh->Indixes.size(), 0, &sampler, 1,
              &_stdParams, sizeof(_stdParams), &_stdParams, sizeof(_stdParams));
}

void GFXEngine::RenderNode(TRenderNode *node)
{
    if (node && (node->Type == TRenderNode::TYPE_MESH || node->Type == TRenderNode::TYPE_PARTICLE))
        RenderingMesh(node);
}

void GFXEngine::QueueRenderMesh(TRenderNode *nod)
{
    if (!nod)
        return;

    TMesh *mesh = nod->Mesh;
    if (!mesh)
        return;

    uint32_t flags = nod->Flags;

    if (flags & RFLAGS_SKY)
        _renderSkyBoxList.push_back(nod);
    else if (flags & RFLAGS_ZEROTRACY)
        _renderZeroTracyList.push_back(nod);
    else if (flags & RFLAGS_ALPHABLEND)
        _renderLumaTracyList.push_back(nod);
    else if (flags & RFLAGS_LUMTRACY)
        _renderLumaTracyList.push_back(nod);
    else
        _renderSolidList.push_back(nod);
}

void GFXEngine::SetDecalView(const mat3x3 &worldToView, uint32_t renderFlags,
                             float fogStart, float fogLength)
{
    _decalWorldToView = worldToView;
    _decalRenderFlags = renderFlags;
    _decalFogStart = fogStart;
    _decalFogLength = fogLength;
}

void GFXEngine::QueueRenderDecal(const TDecalDraw &decal)
{
    _renderDecalList.push_back(decal);
}

// Copies the depth drawn so far (world surfaces only) for the decal shader.
bool GFXEngine::EnsureDecalDepthTexture(const Common::Point &scrSz)
{
    if (scrSz.x <= 0 || scrSz.y <= 0)
        return false;

    const GPU::TextureId depth = GPU::CurrentDepthTarget();
    int w = 0, h = 0;
    if (!GPU::TextureSize(depth, &w, &h))
        return false;

    if (!_decalDepthTex || _decalDepthTexSize != Common::Point(w, h))
    {
        GPU::DestroyTexture(_decalDepthTex);
        _decalDepthTex = GPU::CreateTexture(w, h, GPU::Format::Depth, GPU::USAGE_SAMPLED | GPU::USAGE_TARGET);
        _decalDepthTexSize = Common::Point(w, h);
    }
    if (!_decalDepthTex)
        return false;

    GPU::CopyTexture(depth, _decalDepthTex);
    return true;
}

// Screen rectangle of the box around a decal. False when the box is off screen.
static bool DecalScreenRect(const TDecalDraw &decal, const mat3x3 &worldToView,
                            const mat4x4f &projection, float nearZ,
                            const Common::Point &scrSz, Common::Rect *out)
{
    const float reach = 0.5f * std::sqrt(decal.SizeT * decal.SizeT + decal.SizeB * decal.SizeB);
    const vec3d origin(decal.Origin.x, decal.Origin.y, decal.Origin.z);
    // The box is around the decal as it was painted: a dug surface under it is
    // drawn up to decal.Moved away from there.
    const vec3d t = vec3d(decal.AxisT.x, decal.AxisT.y, decal.AxisT.z) * (reach + decal.Moved);
    const vec3d b = vec3d(decal.AxisB.x, decal.AxisB.y, decal.AxisB.z) * (reach + decal.Moved);
    // Small decals still reach the surface when collision and visuals differ a
    // little: same height limit of the shader.
    const vec3d n = vec3d(decal.AxisN.x, decal.AxisN.y, decal.AxisN.z) *
                    (std::max(reach, 12.0f) + decal.Moved);

    float left = 2.0f, right = -2.0f, bottom = 2.0f, top = -2.0f;
    bool behind = false;
    bool front = false;

    for (int corner = 0; corner < 8; ++corner)
    {
        const vec3d world = origin + t * ((corner & 1) ? 1.0 : -1.0)
                                   + b * ((corner & 2) ? 1.0 : -1.0)
                                   + n * ((corner & 4) ? 1.0 : -1.0);
        const vec3d view = worldToView.Transform(world);
        if (view.z <= nearZ)
        {
            behind = true;
            continue;
        }

        front = true;
        const float x = projection.m00 * (float)view.x / (float)view.z;
        const float y = projection.m11 * (float)view.y / (float)view.z;
        left = std::min(left, x);
        right = std::max(right, x);
        bottom = std::min(bottom, y);
        top = std::max(top, y);
    }

    if (!front)
        return false;

    // A box crossing the camera plane has no usable projection: use the full screen.
    if (behind)
    {
        *out = Common::Rect(0, 0, scrSz.x, scrSz.y);
        return true;
    }

    if (right <= -1.0f || left >= 1.0f || top <= -1.0f || bottom >= 1.0f)
        return false;

    const auto pixel = [](float ndc, int size)
    {
        return (std::max(-1.0f, std::min(ndc, 1.0f)) + 1.0f) * 0.5f * (float)size;
    };

    // Pixels with the origin at the top-left corner, like the scissor.
    out->left = (int)std::floor(pixel(left, scrSz.x));
    out->right = (int)std::ceil(pixel(right, scrSz.x));
    out->top = scrSz.y - (int)std::ceil(pixel(top, scrSz.y));
    out->bottom = scrSz.y - (int)std::floor(pixel(bottom, scrSz.y));
    return out->right > out->left && out->bottom > out->top;
}

// Inverse of the view rotation. The view may carry a scale, so no transpose.
static mat3x3 DecalInverse(const mat3x3 &m)
{
    const double c00 = m.m11 * m.m22 - m.m12 * m.m21;
    const double c01 = m.m12 * m.m20 - m.m10 * m.m22;
    const double c02 = m.m10 * m.m21 - m.m11 * m.m20;
    const double det = m.m00 * c00 + m.m01 * c01 + m.m02 * c02;
    const double inv = det != 0.0 ? 1.0 / det : 0.0;

    mat3x3 out;
    out.m00 = c00 * inv;
    out.m01 = (m.m02 * m.m21 - m.m01 * m.m22) * inv;
    out.m02 = (m.m01 * m.m12 - m.m02 * m.m11) * inv;
    out.m10 = c01 * inv;
    out.m11 = (m.m00 * m.m22 - m.m02 * m.m20) * inv;
    out.m12 = (m.m02 * m.m10 - m.m00 * m.m12) * inv;
    out.m20 = c02 * inv;
    out.m21 = (m.m01 * m.m20 - m.m00 * m.m21) * inv;
    out.m22 = (m.m00 * m.m11 - m.m01 * m.m10) * inv;
    return out;
}

// Draws the dug meshes of the frame (_decalDugList) once more into the rest
// texture: for each of their visible pixels, where its point was before the
// craters and dents, and the normal it had (see decalshader.h). The depth
// copied for the decals is the depth buffer of this pass, so only what is
// really visible is written. False when nothing was written.
// Draws the dug meshes of the frame (_decalDugList) once more into the rest
// texture: for each of their visible pixels, where its point was before the
// craters and dents, and the normal it had (see decalshader.h). The depth
// tested is the copy taken for the decals, so only what is really visible is
// written. False when nothing was written.
bool GFXEngine::DrawDecalRest(const Common::Point &scrSz, const float *viewToWorldColumns)
{
    if (_decalDugList.empty() || !_decalRestProg || !_decalDepthTex)
        return false;

    if (!_decalRestTex || _decalRestTexSize != _decalDepthTexSize)
    {
        GPU::DestroyTexture(_decalRestTex);
        // Full floats: the alpha carries a packed normal of 24 bits.
        _decalRestTex = GPU::CreateTexture(_decalDepthTexSize.x, _decalDepthTexSize.y, GPU::Format::RGBA32F,
                                           GPU::USAGE_SAMPLED | GPU::USAGE_TARGET);
        _decalRestTexSize = _decalDepthTexSize;
    }
    if (!_decalRestTex)
        return false;

    const GPU::TextureId sceneColor = GPU::CurrentColorTarget();
    const GPU::TextureId sceneDepth = GPU::CurrentDepthTarget();
    GPU::SetTarget(_decalRestTex, _decalDepthTex);
    GPU::Clear(true, false);

    TDecalRestParams params = {};
    std::memcpy(params.Proj, &_stdParams.Proj.m00, sizeof(params.Proj));
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            params.ViewToWorld[column * 4 + row] = viewToWorldColumns[column * 3 + row];
    params.ViewToWorld[15] = 1.0f;

    GPU::DrawState state;
    state.program = _decalRestProg;
    state.layout = GPU::Layout::Rest;
    state.depthTest = true;
    state.depthWrite = false;
    // The same triangles were drawn by another program: a small pull toward the
    // camera keeps them in front of their own depth.
    state.depthBias = true;

    for (TRenderNode *node : _decalDugList)
    {
        TMesh *mesh = node->Mesh;
        // Holes of a cut-out texture show what is behind: nothing is written there.
        const bool cut = (node->Flags & RFLAGS_ZEROTRACY) && node->Tex && node->Tex->hwTex;

        const mat4x4f view = mat4x4f(node->TForm).Transpose();
        std::memcpy(params.View, &view.m00, sizeof(params.View));
        params.Cut[0] = cut ? 1.0f : 0.0f;

        GPU::Streams streams;
        streams.buffer = mesh->gpuVertexBuf;
        streams.pos = offsetof(TVertex, Pos);
        streams.uv = offsetof(TVertex, TexCoord);
        streams.rest = mesh->RestBufferPos;

        const GPU::Sampler sampler = {cut ? node->Tex->hwTex : 0, false};
        GPU::Draw(state, streams, mesh->gpuIndexBuf, sizeof(IndexType) == 2,
                  (uint32_t)mesh->Indixes.size(), 0, &sampler, 1,
                  &params, sizeof(params), &params, sizeof(params));
    }

    GPU::SetTarget(sceneColor, sceneDepth);
    return true;
}

// Paints the queued decals over the world drawn so far. No unit is in the depth
// buffer yet, so they can never receive a decal.
// Paints the queued decals over the world drawn so far. No unit is in the depth
// buffer yet, so they can never receive a decal.
void GFXEngine::DrawDecalPass()
{
    if (_renderDecalList.empty())
        return;

    const Common::Point scrSz = System::GetResolution();
    if (!_decalProg || !EnsureDecalDepthTexture(scrSz))
    {
        _renderDecalList.clear();
        return;
    }

    const GfxStates saved = _states;

    const mat3x3 viewToWorld = DecalInverse(_decalWorldToView);
    const float viewToWorldColumns[9] = {
        (float)viewToWorld.m00, (float)viewToWorld.m10, (float)viewToWorld.m20,
        (float)viewToWorld.m01, (float)viewToWorld.m11, (float)viewToWorld.m21,
        (float)viewToWorld.m02, (float)viewToWorld.m12, (float)viewToWorld.m22};

    const bool restOn = DrawDecalRest(scrSz, viewToWorldColumns);

    _states.Fog = false;
    _states.AFog = 0;
    ApplyFogStates(_decalRenderFlags | RFLAGS_FOG, _decalFogStart, _decalFogLength);

    TDecalParams params = {};
    params.ScreenInv[0] = 1.0f / (float)_decalDepthTexSize.x;
    params.ScreenInv[1] = 1.0f / (float)_decalDepthTexSize.y;
    params.RestOn = restOn ? 1.0f : 0.0f;
    params.Project[0] = 1.0f / _frustum.m00;
    params.Project[1] = 1.0f / _frustum.m11;
    params.Project[2] = _frustum.m22;
    params.Project[3] = _frustum.m23;
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            params.ViewToWorld[column * 4 + row] = viewToWorldColumns[column * 3 + row];
    const float fog[4] = {_states.Fog ? 1.0f : 0.0f, _states.FogStart, _states.FogLength, _states.FogStrength};
    const float alphaFog[4] = {(float)_states.AFog, _states.AFogStart, _states.AFogLength, _states.AFogStrength};
    std::memcpy(params.Fog, fog, sizeof(fog));
    std::memcpy(params.AlphaFog, alphaFog, sizeof(alphaFog));
    params.FogColor[0] = _states.FogColor.r;
    params.FogColor[1] = _states.FogColor.g;
    params.FogColor[2] = _states.FogColor.b;
    params.AtmosphereColor[0] = _states.AFogColor.r;
    params.AtmosphereColor[1] = _states.AFogColor.g;
    params.AtmosphereColor[2] = _states.AFogColor.b;

    GPU::DrawState state;
    state.program = _decalProg;
    state.blend = true;
    state.src = GPU::Blend::SrcAlpha;
    state.dst = GPU::Blend::OneMinusSrcAlpha;
    state.depthTest = false;
    state.depthWrite = false;

    static const std::array<TVertex, 4> quad = {
        GFX::TVertex( vec3f(-1.0,  1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f(-1.0, -1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0, -1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0,  1.0, 0.0), tUtV(0.0, 0.0) )
    };
    // The quad never changes: once in the frame arena, only the draws repeat.
    const uint32_t base = GPU::PushTransient(quad.data(), sizeof(TVertex) * quad.size());
    GPU::Streams streams;
    streams.pos = base + offsetof(TVertex, Pos);
    streams.color = base + offsetof(TVertex, Color);
    streams.uv = base + offsetof(TVertex, TexCoord);

    // Oldest first: the newest impact ends on top.
    for (const TDecalDraw &decal : _renderDecalList)
    {
        Common::Rect rect;
        if (!decal.Shape ||
            !DecalScreenRect(decal, _decalWorldToView, _frustum, _frustumNear, scrSz, &rect))
            continue;

        const GPU::Rect scissor = {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
        GPU::SetScissor(&scissor);

        const GPU::TextureId image = decal.Tex ? decal.Tex->hwTex : 0;
        params.Textured = image ? 1.0f : 0.0f;
        params.Origin[0] = decal.Origin.x;
        params.Origin[1] = decal.Origin.y;
        params.Origin[2] = decal.Origin.z;
        params.AxisT[0] = decal.AxisT.x;
        params.AxisT[1] = decal.AxisT.y;
        params.AxisT[2] = decal.AxisT.z;
        params.AxisB[0] = decal.AxisB.x;
        params.AxisB[1] = decal.AxisB.y;
        params.AxisB[2] = decal.AxisB.z;
        params.AxisN[0] = decal.AxisN.x;
        params.AxisN[1] = decal.AxisN.y;
        params.AxisN[2] = decal.AxisN.z;
        params.Size[0] = decal.SizeT;
        params.Size[1] = decal.SizeB;
        params.EdgeFade = decal.EdgeFade;
        params.Colorize = decal.Colorize ? 1.0f : 0.0f;
        params.Tint[0] = decal.Tint.r;
        params.Tint[1] = decal.Tint.g;
        params.Tint[2] = decal.Tint.b;
        params.Tint[3] = decal.Tint.a;
        std::memcpy(params.Shape, decal.Shape, sizeof(params.Shape));

        const GPU::Sampler samplers[3] = {
            {image, _filter != 0},
            {_decalDepthTex, false},
            {restOn ? _decalRestTex : 0, false}};
        GPU::Draw(state, streams, GPU::QuadIndexBuffer(), true, 6, 0, samplers, 3,
                  nullptr, 0, &params, sizeof(params));
    }

    GPU::SetScissor(nullptr);
    _renderDecalList.clear();

    _states = saved;
    SetRenderStates(0);
}

void GFXEngine::Rasterize(uint32_t RasterEtapes)
{
    if (RasterEtapes & RASTER_SKY)
    {
        // OpenNeoUA: render the camera-following sky with its own extended
        // projection, then restore the unlocked world projection immediately.
        mat4x4f skyFrustum = mat4x4f::UAFrustum(_frustumNear, SKY_FAR_CLIP);
        skyFrustum.m00 *= _viewZoom;
        skyFrustum.m11 *= _viewZoom;
        SetProjectionMatrix(skyFrustum);

        std::stable_sort(_renderSkyBoxList.begin(), _renderSkyBoxList.end(),
                         TRenderNode::CompareSolid);

        for (size_t nodeIndex = 0; nodeIndex < _renderSkyBoxList.size(); nodeIndex++)
            RenderNode(_renderSkyBoxList[nodeIndex]);

        // The visible sky uses a much longer projection than world geometry, so
        // its depth values cannot safely be compared with the world's depth.
        // Render the same dome a second time with the normal world projection,
        // colour writes disabled and Z-write enabled. This restores the original
        // Urban Assault behaviour: the player-centered sky acts as a continuous
        // horizon mask, so already-prepared sectors are revealed smoothly instead
        // of popping in one whole sector at a time.
        SetProjectionMatrix(_frustum);
        _colorWrite = false;

        for (size_t nodeIndex = 0; nodeIndex < _renderSkyBoxList.size(); nodeIndex++)
        {
            TRenderNode *node = _renderSkyBoxList[nodeIndex];
            const uint32_t originalFlags = node->Flags;
            node->Flags &= ~RFLAGS_DISABLE_ZWRITE;
            RenderNode(node);
            node->Flags = originalFlags;
        }

        _colorWrite = true;
        _renderSkyBoxList.clear();

        // Every non-sky queue continues with the same normal world projection.
        SetProjectionMatrix(_frustum);
    }

    if (RasterEtapes & RASTER_SOLID)
        std::stable_sort(_renderSolidList.begin(), _renderSolidList.end(),
                         TRenderNode::CompareSolid);

    if (RasterEtapes & RASTER_ZEROTR)
        std::stable_sort(_renderZeroTracyList.begin(), _renderZeroTracyList.end(),
                         TRenderNode::CompareSolid);

    // With decals in the frame the world goes first, then the decals, then
    // everything else: the depth buffer they read holds world surfaces only.
    size_t receiverSolids = 0;
    size_t receiverZeroTracy = 0;

    if (!_renderDecalList.empty() && (RasterEtapes & RASTER_SOLID) && (RasterEtapes & RASTER_ZEROTR))
    {
        const auto receives = [](const TRenderNode *node)
        {
            return (node->Flags & RFLAGS_DECAL_RECEIVER) != 0;
        };

        receiverSolids = std::stable_partition(_renderSolidList.begin(), _renderSolidList.end(),
                                               receives) - _renderSolidList.begin();
        receiverZeroTracy = std::stable_partition(_renderZeroTracyList.begin(), _renderZeroTracyList.end(),
                                                  receives) - _renderZeroTracyList.begin();

        // Receivers dug by craters or dents also go in the rest pass of the decals.
        const auto drawReceiver = [this](TRenderNode *node)
        {
            RenderNode(node);
            if (node->Type == TRenderNode::TYPE_MESH && node->Mesh && !node->Mesh->Indixes.empty() &&
                node->Mesh->Rest.size() == node->Mesh->Vertexes.size())
                _decalDugList.push_back(node);
        };

        for (size_t nodeIndex = 0; nodeIndex < receiverSolids; nodeIndex++)
            drawReceiver(_renderSolidList[nodeIndex]);
        for (size_t nodeIndex = 0; nodeIndex < receiverZeroTracy; nodeIndex++)
            drawReceiver(_renderZeroTracyList[nodeIndex]);

        DrawDecalPass();
        _decalDugList.clear();
    }

    if (RasterEtapes & RASTER_SOLID)
    {
        for (size_t nodeIndex = receiverSolids; nodeIndex < _renderSolidList.size(); nodeIndex++)
            RenderNode(_renderSolidList[nodeIndex]);
        _renderSolidList.clear();
    }

    if (RasterEtapes & RASTER_ZEROTR)
    {
        for (size_t nodeIndex = receiverZeroTracy; nodeIndex < _renderZeroTracyList.size(); nodeIndex++)
            RenderNode(_renderZeroTracyList[nodeIndex]);
        _renderZeroTracyList.clear();
    }

    if (RasterEtapes & RASTER_LUMATR)
    {
        std::stable_sort(_renderLumaTracyList.begin(), _renderLumaTracyList.end(),
                         TRenderNode::CompareTransparent);

        for (size_t nodeIndex = 0; nodeIndex < _renderLumaTracyList.size(); nodeIndex++)
            RenderNode(_renderLumaTracyList[nodeIndex]);
        _renderLumaTracyList.clear();
    }
}


void GFXEngine::raster_func207(int id, TileMap *t)
{
    if (_tiles[id] != t)
        ClearUiAccentCache();

    _tiles[id] = t;
}

TileMap *GFXEngine::raster_func208(int id)
{
    return _tiles[id];
}

int GFXEngine::raster_func208(TileMap *t)
{
    if ( t )
    {
        for (int i = 0; i < 256; i++)
        {
            if (_tiles[i] == t)
                return i;
        }
    }
    return -1;
}

void GFXEngine::ClearUiAccentCache()
{
    for (auto &entry : _uiAccentSurfaces)
        SDL_DestroySurface(entry.second);

    _uiAccentSurfaces.clear();
    _uiAccentCacheValid = false;
}

bool GFXEngine::IsUiAccentTileset(uint8_t id)
{
    // Gameplay UI atlases use one vanilla source image. Non-Resistance factions
    // receive their owner colour at runtime, so no faction-specific copies are needed.
    return id == 0 || id == 2 || id == 3 || id == 4 || id == 5 || id == 6 || id == 8 ||
           (id >= 9 && id <= 15) || (id >= 21 && id <= 25) || id == 30 ||
           (id >= 40 && id <= 44);
}

bool GFXEngine::IsUiAccentNeutralHighlightTileset(uint8_t id)
{
    // Limit neutral recolouring to control atlases. Font atlases (notably 0
    // and 15) must keep their authored white highlights for legible resource
    // numbers and labels, especially with the Taerkasten yellow theme.
    return id >= 9 && id <= 14;
}

bool GFXEngine::IsFullFactionUiAtlas(uint8_t id)
{
    // These four atlases previously needed separate recoloured PNG files.
    // Recolour their complete visible artwork while keeping transparency intact.
    return (id >= 21 && id <= 23) || id == 30;
}

SDL_Color GFXEngine::RemapUiAccentColor(const SDL_Color &source, const SDL_Color &accent,
                                        bool includeNeutralHighlights,
                                        int neutralThreshold,
                                        bool tintAllNonDark)
{
    const int sourceMax = std::max(source.r, std::max(source.g, source.b));
    const int sourceMin = std::min(source.r, std::min(source.g, source.b));
    const bool tealAccent = source.g > source.r + 8 && source.b > source.r + 8;
    const bool neutralHighlight = includeNeutralHighlights &&
                                  sourceMax >= neutralThreshold &&
                                  sourceMax - sourceMin <= 36;

    // Dark teal pixels form the neutral panel fill in the original artwork.
    // Only medium/bright accents are themed. Very dark panel fills and grey
    // backgrounds remain neutral; bright UI highlights are admitted so
    // gauges, borders and buttons cannot remain patchy.
    const int darkThreshold = tintAllNonDark ? 48 : 72;
    if (sourceMax < darkThreshold ||
        (!tintAllNonDark && !tealAccent && !neutralHighlight))
    {
        return source;
    }

    const int accentValue = std::max(1, (int)std::max(accent.r,
                                                      std::max(accent.g, accent.b)));
    int themedValue = sourceMax;

    // Near-white faction accents need a small luminance lift; otherwise the
    // original teal value survives merely as grey despite the white target.
    const int accentMin = std::min(accent.r, std::min(accent.g, accent.b));
    if (accentMin >= 245)
        themedValue = std::min(255, sourceMax + 28);

    SDL_Color themed = source;
    themed.r = (uint8_t)std::min(255, accent.r * themedValue / accentValue);
    themed.g = (uint8_t)std::min(255, accent.g * themedValue / accentValue);
    themed.b = (uint8_t)std::min(255, accent.b * themedValue / accentValue);
    return themed;
}

SDL_Color GFXEngine::RemapFullFactionUiColor(const SDL_Color &source,
                                              const SDL_Color &accent)
{
    const int sourceValue = std::max(source.r, std::max(source.g, source.b));
    if (sourceValue <= 1)
        return source;

    const int accentValue = std::max(1, (int)std::max(accent.r,
                                                      std::max(accent.g, accent.b)));

    SDL_Color themed = source;
    themed.r = (uint8_t)std::min(255, accent.r * sourceValue / accentValue);
    themed.g = (uint8_t)std::min(255, accent.g * sourceValue / accentValue);
    themed.b = (uint8_t)std::min(255, accent.b * sourceValue / accentValue);
    return themed;
}

SDL_Surface *GFXEngine::GetUiAccentSurface(SDL_Surface *source, const SDL_Color &accent,
                                           uint8_t tilesetId)
{
    if (!source)
        return source;

    if (!_uiAccentCacheValid ||
        _uiAccentCacheColor.r != accent.r ||
        _uiAccentCacheColor.g != accent.g ||
        _uiAccentCacheColor.b != accent.b)
    {
        ClearUiAccentCache();
        _uiAccentCacheColor = accent;
        _uiAccentCacheValid = true;
    }

    const std::pair<SDL_Surface *, uint8_t> cacheKey(source, tilesetId);
    auto cached = _uiAccentSurfaces.find(cacheKey);
    if (cached != _uiAccentSurfaces.end())
        return cached->second;

    SDL_Surface *copy = SDL_DuplicateSurface(source);
    if (!copy)
        return source;

    Uint32 sourceColorKey = 0;
    const bool hasColorKey = SDL_GetSurfaceColorKey(source, &sourceColorKey);
    if (hasColorKey)
        SDL_SetSurfaceColorKey(copy, true, sourceColorKey);

    const bool fullFactionAtlas = IsFullFactionUiAtlas(tilesetId);
    const int bytesPerPixel = SDL_BYTESPERPIXEL(copy->format);
    if (!SDL_LockSurface(source))
    {
        SDL_DestroySurface(copy);
        return source;
    }
    if (bytesPerPixel >= 2 && bytesPerPixel <= 4 && SDL_LockSurface(copy))
    {
        for (int y = 0; y < copy->h; ++y)
        {
            uint8_t *row = (uint8_t *)copy->pixels + y * copy->pitch;
            // SDL_ConvertSurface turns colour-key pixels into zero-alpha pixels.
            // DrawFill tests the packed colour key, so preserve the source bytes
            // before remapping: transparent glyph pixels must not erase the UI below.
            memcpy(row, (uint8_t *)source->pixels + y * source->pitch,
                   source->w * bytesPerPixel);
            for (int x = 0; x < copy->w; ++x)
            {
                uint8_t *pixel = row + x * bytesPerPixel;
                uint32_t value = 0;

                if (bytesPerPixel == 2)
                    memcpy(&value, pixel, 2);
                else if (bytesPerPixel == 3)
                {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    value = pixel[0] << 16 | pixel[1] << 8 | pixel[2];
#else
                    value = pixel[0] | pixel[1] << 8 | pixel[2] << 16;
#endif
                }
                else
                    memcpy(&value, pixel, 4);

                if (hasColorKey && value == sourceColorKey)
                    continue;

                SDL_Color original;
                SDL_GetRGBA(value, SDL_GetPixelFormatDetails(copy->format), SDL_GetSurfacePalette(copy), &original.r, &original.g,
                            &original.b, &original.a);

                SDL_Color themed;
                if (fullFactionAtlas)
                {
                    themed = RemapFullFactionUiColor(original, accent);
                }
                else
                {
                    // Keep the dark frame/track ramp neutral for every faction.
                    // The normal accent remap still colours the authored scrollbar
                    // highlights, arrows and knob. In particular, do not tint the
                    // whole 11..13 map-control atlases for Ghorkov: that special
                    // case also coloured the outer map frame red.
                    const bool tintAllMapControls = tilesetId == 10;
                    const bool includeNeutralHighlights =
                        IsUiAccentNeutralHighlightTileset(tilesetId);
                    const int neutralThreshold = tilesetId == 10 ? 90 : 180;
                    themed = RemapUiAccentColor(
                        original, accent, includeNeutralHighlights, neutralThreshold,
                        tintAllMapControls);
                }
                if (themed.r == original.r && themed.g == original.g &&
                    themed.b == original.b)
                {
                    continue;
                }

                value = SDL_MapSurfaceRGBA(copy, themed.r, themed.g,
                                    themed.b, original.a);
                // A visible yellow accent must not become the transparent key.
                if (hasColorKey && value == sourceColorKey)
                    value ^= 1u << SDL_GetPixelFormatDetails(copy->format)->Bshift;
                if (bytesPerPixel == 2)
                    memcpy(pixel, &value, 2);
                else if (bytesPerPixel == 3)
                {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                    pixel[0] = (value >> 16) & 0xff;
                    pixel[1] = (value >> 8) & 0xff;
                    pixel[2] = value & 0xff;
#else
                    pixel[0] = value & 0xff;
                    pixel[1] = (value >> 8) & 0xff;
                    pixel[2] = (value >> 16) & 0xff;
#endif
                }
                else
                    memcpy(pixel, &value, 4);
            }
        }

        SDL_UnlockSurface(copy);
    }
    else
    {
        SDL_UnlockSurface(source);
        SDL_DestroySurface(copy);
        return source;
    }

    SDL_UnlockSurface(source);

    _uiAccentSurfaces[cacheKey] = copy;
    return copy;
}

static bool IsFactionAccentTextSource(uint8_t r, uint8_t g, uint8_t b)
{
    // The original gameplay/menu accent text is cyan/azure. Remap only that
    // chromatic family; authored white, grey and other semantic colours remain
    // untouched.
    return g >= 80 && b >= 80 &&
           g >= r + 18 && b >= r + 18 &&
           std::abs((int)g - (int)b) <= 96;
}

void GFXEngine::ProcessDrawSeq(const CmdStream &drawSeq, const CmdIncludes *includes,
                               const SDL_Color *uiAccent)
{
    struct CmdStkEntr
    {
        const CmdStream &seq;
        const int32_t pos;
    };

    int v11;

    int bytesPerColor = SDL_BYTESPERPIXEL(Screen()->format);

    int32_t curPos = 0;
    const CmdStream *curStream = &drawSeq;

    int w_pixels = Screen()->pitch / bytesPerColor;
    TileMap *tile = nullptr;
    uint8_t tileId = 0;
    uint8_t opacity = 255;

    int x_out = 0;
    int y_out = 0;

    int x_out_txt = 0;
    int y_out_txt = 0;
    int txt_flag = 0;

    int y_pos_line = 0;
    int x_pos_line = 0;

    const Common::Point drawResolution = _virtualUiPass ? _virtualUiResolution : _resolution;
    int halfWidth = drawResolution.x / 2;
    int halfHeight = drawResolution.y / 2;

    int line_width = 0;
    int line_height = 0;

    //if v11 = 0 - clone first column of tile  (count = line_width)
    //if v11 = 1 - normal copy of tile
    v11 = 1;


    int x_off = 0;
    int y_off = 0;

    std::stack<CmdStkEntr> Stack;

    while ( 1 )
    {
        uint32_t v13 = FontUA::get_u8(*curStream, &curPos);
        // Opcode 25 carries a wide glyph without changing legacy byte commands.
        if (!v13 && curPos < (int32_t)curStream->size() && (*curStream)[curPos] == 25)
        {
            ++curPos;
            v13 = (uint32_t)FontUA::get_u16(*curStream, &curPos) << 16;
            v13 |= FontUA::get_u16(*curStream, &curPos);
        }

        if ( v13 )
        {
            const Common::PointRect &chrr = tile->Glyph(v13);

            int cpy_width;

            if ( line_width )
                cpy_width = line_width - x_off;
            else
                cpy_width = chrr.w - x_off;

            int cpy_height = line_height - y_off;

            Common::Rect dstR(x_out, y_out, x_out + cpy_width, y_out + cpy_height);

            Common::Rect srcR;
            srcR.left = chrr.x + x_off;
            srcR.top = chrr.y + y_off;
            srcR.bottom = chrr.y + y_off + cpy_height;

            if (v11)
                srcR.right = chrr.x + x_off + cpy_width;
            else
                srcR.right = chrr.x + x_off + 1;

            SDL_Surface *source = tile->Page(v13);
            // TYPE_NS also contains unit icons and health bars. Only its frame
            // glyphs belong to the UI theme; the other symbols keep their colours.
            const bool squadFrameGlyph = tileId == 28 &&
                (v13 == '?' || v13 == '$' || v13 == '%' || v13 == '&' ||
                 v13 == '/' || v13 == '=' || v13 == '{' || v13 == '}');
            // Accent surfaces are built from the main page only.
            if (uiAccent && source == tile->img->GetSwTex() && (IsUiAccentTileset(tileId) || squadFrameGlyph))
                source = GetUiAccentSurface(source, *uiAccent, tileId);
            if (opacity == 255)
                DrawFill(source, srcR, Screen(), dstR);
            else
                DrawFillAlpha(source, srcR, Screen(), dstR, opacity);

            line_width = 0;
            x_off = 0;
            x_out += cpy_width;
            v11 = 1;
        }
        else // 0
        {
            int opcode = FontUA::get_u8(*curStream, &curPos);

            switch ( opcode )
            {
            case 0: // End

                if (Stack.empty())
                {
                    DrawScreenText();
                    return;
                }

                curPos = Stack.top().pos;
                curStream = &Stack.top().seq;

                Stack.pop();
                break;

            case 1: // x pos from center
                x_out = halfWidth + FontUA::get_s16(*curStream, &curPos);
                x_pos_line = x_out;

                y_pos_line = y_out;
                y_off = 0;

                line_height = tile->h;
                break;

            case 2: // y pos from center
                y_out = halfHeight + FontUA::get_s16(*curStream, &curPos);
                x_pos_line = x_out;

                y_pos_line = y_out;
                y_off = 0;

                line_height = tile->h;
                break;

            case 3: //xpos
                x_out = FontUA::get_s16(*curStream, &curPos);
                if ( x_out < 0 )
                    x_out += w_pixels;

                x_pos_line = x_out;
                y_pos_line = y_out;

                line_height = tile->h;
                y_off = 0;
                break;

            case 4: //ypos
                y_out = FontUA::get_s16(*curStream, &curPos);
                if ( y_out < 0 )
                    y_out += drawResolution.y;

                x_pos_line = x_out;
                y_pos_line = y_out;

                line_height = tile->h;
                y_off = 0;
                break;

            case 5: //add to x pos
                x_out += FontUA::get_s16(*curStream, &curPos);
                break;

            case 6: //add to y pos
                y_out += FontUA::get_s16(*curStream, &curPos);
                break;

            case 7: //next line
                y_out = (line_height - y_off) + y_pos_line;
                y_pos_line = y_out;
                x_out = x_pos_line;

                y_off = 0;
                line_height = tile->h;
                break;

            case 8: // Select tileset
                tileId = FontUA::get_u8(*curStream, &curPos);
                tile = _tiles[tileId];
                break;

            case 9: // Include another cmdlist source
            {
                int azaza = FontUA::get_u8(*curStream, &curPos);
                Stack.push( {*curStream, curPos} );
                curPos = 0;
                curStream = includes->at(azaza);
            }
            break;

            case 10:
                line_width = FontUA::get_u8(*curStream, &curPos);

                v11 = 0;
                x_off = 0;

                break;

            case 11:

                line_width = FontUA::get_u8(*curStream, &curPos);

                v11 = 0;
                x_off = 0;

                line_width -= (x_out - x_pos_line);
                break;

            case 12: // Set x offset
                x_off = FontUA::get_u8(*curStream, &curPos);
                break;

            case 13: // Set x width
                line_width = FontUA::get_u8(*curStream, &curPos);
                break;

            case 14: // Set y offset
                y_off = FontUA::get_u8(*curStream, &curPos);
                break;

            case 15: // Set y height
                line_height = FontUA::get_u8(*curStream, &curPos);
                break;

            case 16: // Full reset tileset
                tileId = FontUA::get_u8(*curStream, &curPos);
                tile = _tiles[tileId];
                line_height = tile->h;
                y_off = 0;
                break;

            case 17:
                line_width = FontUA::get_s16(*curStream, &curPos);
                v11 = 0;
                x_off = 0;
                line_width -= (x_out - x_pos_line);
                break;

            case 18: // Add text
            {
                int block_width = FontUA::get_s16(*curStream, &curPos);
                int flag = txt_flag | FontUA::get_u16(*curStream, &curPos);

                int32_t sz = FontUA::get_u16(*curStream, &curPos);

                std::string txt;
                txt.assign((const char *)(curStream->data()) + curPos, sz);


                curPos += sz + 1;
                AddScreenText(txt, x_out_txt, y_out_txt, block_width, tile->h, flag);
            }
            break;

            case 19: // Copy current x/y pos for text output
                x_out_txt = x_out;
                y_out_txt = y_out;
                break;

            case 20: // Add txtout flag
                txt_flag |= FontUA::get_u16(*curStream, &curPos);
                break;

            case 21: // Delete txtout flag
                txt_flag &= ~(FontUA::get_u16(*curStream, &curPos));
                break;

            case 22: // set colour for font, with selective faction accent
            {
                int r = FontUA::get_u16(*curStream, &curPos);
                int g = FontUA::get_u16(*curStream, &curPos);
                int b = FontUA::get_u16(*curStream, &curPos);

                if ( uiAccent && IsFactionAccentTextSource((uint8_t)r,
                                                           (uint8_t)g,
                                                           (uint8_t)b) )
                {
                    r = uiAccent->r;
                    g = uiAccent->g;
                    b = uiAccent->b;
                }

                AddScreenText("", r, g, b, 0, 0x20);
            }
            break;

            case 24: // set tile opacity
                opacity = FontUA::get_u16(*curStream, &curPos);
                break;
            }
        }
    }
}


void GFXEngine::raster_func210(const Common::FRect &arg)
{
    _clip = Common::Rect(   (arg.left + 1.0) * (_field_554 + -1.0),
                            (arg.top + 1.0) * (_field_558 + -1.0),
                            (arg.right + 1.0) * (_field_554 + -1.0),
                            (arg.bottom + 1.0) * (_field_558 + -1.0) );
    }

void GFXEngine::raster_func211(const Common::Rect &arg)
{
    _clip = Common::Rect(   _field_54c + arg.left,
                            _field_550 + arg.top,
                            _field_54c + arg.right,
                            _field_550 + arg.bottom );
}


void GFXEngine::BeginScene()
{
    SetRenderStates(2);

    SetProjectionMatrix( _frustum );
    SetModelViewMatrix( mat4x4f() );

    _sceneBeginned = 1;
}

// Draw transparent
void GFXEngine::EndScene()
{
    _sceneBeginned = 0;

    _renderSkyBoxList.clear();
    _renderSolidList.clear();
    _renderZeroTracyList.clear();
    _renderLumaTracyList.clear();
    _renderDecalList.clear();

    _renderNodesCache.Rewind();
}

size_t GFXEngine::raster_func217(SDL_Color color)
{
    _field_4 = color;

    return 0;
}

void GFXEngine::raster_func218(rstr_218_arg *arg)
{
    Common::Rect sRect( (arg->rect1.left + 1.0) * (arg->bitm_intern->width / 2),
                        (arg->rect1.top + 1.0) * (arg->bitm_intern->height / 2),
                        (arg->rect1.right + 1.0) * (arg->bitm_intern->width / 2),
                        (arg->rect1.bottom + 1.0) * (arg->bitm_intern->height / 2) );

    Common::Rect dRect( (arg->rect2.left + 1.0) * _field_554,
                        (arg->rect2.top + 1.0) * _field_558,
                        (arg->rect2.right + 1.0) * _field_554,
                        (arg->rect2.bottom + 1.0) * _field_558 );

    BlitScaleMasked(arg->bitm_intern->swTex, sRect, arg->bitm_intern2->swTex, arg->flg, Screen(), dRect);
}

void GFXEngine::raster_func221(const Common::Rect &arg)
{
    _inverseClip.left = _field_54c + arg.left;
    _inverseClip.top = _field_550 + arg.top;
    _inverseClip.right = _field_54c + arg.right;
    _inverseClip.bottom = _field_550 + arg.bottom;
}

// Which graphics chip really got the window. On a machine with two of them SDL_GPU
// picks the dedicated Vulkan device (and main.cpp exports the driver hints), but a
// system setting can still override that, so say plainly what is in use: a slow
// chip explains far more frame drops than any game setting. Written on the first
// frame because the log files are emptied while the graphics are starting.
void GFXEngine::ReportGraphicsChip()
{
    if ( _graphicsChipReported )
        return;
    _graphicsChipReported = true;

    ReadGraphicsChip();
    ypa_log_out("Graphics chip in use: %s (%s)\n", _gpuRenderer.c_str(), _gpuApi.c_str());

    const bool integrated = _gpuIntegrated;

    if ( integrated )
    {
#ifdef _WIN32
        ypa_log_out("WARNING: this looks like a built-in graphics chip, which is much slower "
                    "than a graphics card.\n"
                    "         If this computer has one, choose it for OpenNeoUA.exe in "
                    "Settings, System, Display, Graphics, and pick High performance.\n");
#else
        // Both commands also choose the Vulkan device on Linux.
        ypa_log_out("WARNING: this looks like a built-in graphics chip, which is much slower "
                    "than a graphics card.\n"
                    "         If this computer has one, start the game with DRI_PRIME=1 "
                    "(AMD or Intel card) or with prime-run (NVIDIA card).\n");
#endif
    }
}

// Reads the device names once and decides whether the chip is built into the processor.
void GFXEngine::ReadGraphicsChip()
{
    if ( _graphicsChipRead )
        return;
    _graphicsChipRead = true;

    _gpuRenderer = GPU::DeviceName();
    _gpuApi = GPU::DriverName();

    std::string name = _gpuRenderer;
    for (char &letter : name)
        letter = (char)std::tolower((unsigned char)letter);

    // Intel Arc is a real graphics card: do not confuse it with an Intel chip
    // built into the processor.
    _gpuIntegrated =
        (name.find("arc") == std::string::npos) &&
        (name.find("intel") != std::string::npos ||
         name.find("uhd graphics") != std::string::npos ||
         name.find("hd graphics") != std::string::npos ||
         name.find("iris") != std::string::npos ||
         name.find("radeon(tm) graphics") != std::string::npos ||
         name.find("vega") != std::string::npos ||
         name.find("llvmpipe") != std::string::npos ||
         name.find("softpipe") != std::string::npos ||
         name.find("microsoft basic") != std::string::npos);
}

void GFXEngine::BeginFrame()
{
    ReportGraphicsChip();

    setViewZoom(1.0f);
    SDL_FillSurfaceRect(Screen(), nullptr, SDL_MapSurfaceRGBA(Screen(), 0, 0, 0, 0) );

    Common::Point scrSz = System::GetResolution();

    if (_colorEffects)
    {
        // The window is cleared, then the 3D scene goes to its own target.
        GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
        GPU::Clear(true, true);
        CreateSceneTargets(scrSz);
        GPU::SetTarget(_sceneTex, _sceneDepth);
    }
    else
        GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);

    GPU::SetViewport(0, 0, scrSz.x, scrSz.y);
    GPU::Clear(true, true);

    _states.Prog = _stdShaderProg;
    SetRenderStates(0);
}

void GFXEngine::EndFrame()
{
    // The optional atmospheric pass must process the 3D scene only. When it is
    // active, resolve the scene FBO before composing the software/HW UI even
    // with the legacy gfx.color_effects = 1 ordering. With the feature absent
    // or disabled, the original ordering remains byte-for-byte equivalent.
    const bool drawWorldBeforeUi = (_colorEffects > 1) || _atmosphereActive;

    if (_vhsFilterActive)
    {
        if (drawWorldBeforeUi)
        {
            GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
            DrawFBO();
        }

        Gui::Root::Instance.Draw(Screen());
        DrawScreenSurface();
        DrawVirtualUISurface();
        Gui::Root::Instance.HwCompose();

        if (_colorEffects == 1 && !_atmosphereActive)
        {
            GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
            DrawFBO();
        }

        Common::Point scrSz = System::GetResolution();
        if (EnsureVhsFilterTexture(scrSz))
        {
            // The finished frame is copied, processed at the VHS size, then blended back.
            GPU::CopyTexture(GPU::BACKBUFFER, _vhsCopyTex);

            GPU::SetTarget(_vhsOutTex, 0);
            GPU::SetViewport(0, 0, _vhsOutTexSize.x, _vhsOutTexSize.y);
            GPU::Clear(true, false);
            DrawVhsEffect();

            GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
            GPU::SetViewport(0, 0, scrSz.x, scrSz.y);
            GPU::Clear(true, false);
            DrawVhsFilter();
        }

        GPU::Present();
        return;
    }
    else if (drawWorldBeforeUi)
    {
        GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
        DrawFBO();
    }

    Gui::Root::Instance.Draw(Screen());
    DrawScreenSurface();
    DrawVirtualUISurface();
    Gui::Root::Instance.HwCompose();

    if (_colorEffects == 1 && !_vhsFilterActive && !_atmosphereActive)
    {
        GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);
        DrawFBO();
    }

    GPU::Present();
}

void GFXEngine::SetColorEffectsPowers(const std::vector<ColorFx> &arg)
{
    if (arg.empty())
    {
        _normClr = vec3d(1.0, 1.0, 1.0);
        _invClr = vec3d(0.0, 0.0, 0.0);
        return;
    }

    _normClr = vec3d(0.0, 0.0, 0.0);
    _invClr = vec3d(0.0, 0.0, 0.0);
    bool applied = false;

    for (ColorFx fx : arg)
    {
        vec3d color;
        bool inverse = false;

        if (fx.Id >= 0 && fx.Id < (int)_clrEff.size())
        {
            color = _clrEff.at(fx.Id);
            inverse = (fx.Id == 4 || fx.Id == 5 || fx.Id == 7);
        }
        else if (fx.Id >= 8 && fx.Id < (int)_customClrEff.size() &&
                 _customClrEffDefined.at(fx.Id))
        {
            color = _customClrEff.at(fx.Id);
        }
        else
        {
            continue;
        }

        applied = true;
        if (inverse)
            _invClr += color * fx.Pwr;
        else
            _normClr += color * fx.Pwr;
    }

    // An undefined custom slot is a safe no-op, not a black-screen effect.
    if (!applied)
        _normClr = vec3d(1.0, 1.0, 1.0);
}

void GFXEngine::LoadCustomColorEffectsFromConfig()
{
    _customClrEff.fill(vec3d());
    _customClrEffDefined.fill(false);

    Common::Ini::Key *keys[] =
    {
        &System::IniConf::GfxColorEff8,
        &System::IniConf::GfxColorEff9,
        &System::IniConf::GfxColorEff10,
        &System::IniConf::GfxColorEff11,
        &System::IniConf::GfxColorEff12,
        &System::IniConf::GfxColorEff13,
        &System::IniConf::GfxColorEff14,
        &System::IniConf::GfxColorEff15,
        &System::IniConf::GfxColorEff16
    };

    for (int i = 0; i < 9; ++i)
    {
        const int slot = 8 + i;
        const std::string value = keys[i]->Get<std::string>();
        if (value.empty())
            continue;

        vec3d color;
        if (!ParseColorEffectRgb(value, &color))
        {
            ypa_log_out("WARNING: invalid custom palette color [gfx.color_eff[%d]=%s]; expected R_G_B with each component in 0..255. Slot disabled.\n",
                        slot, value.c_str());
            continue;
        }

        _customClrEff.at(slot) = color;
        _customClrEffDefined.at(slot) = true;
    }
}

bool GFXEngine::AllocTexture(ResBitmap *bitm)
{
    if (!bitm->swTex || bitm->hwTex)
        return true;

    SDL_Surface *source = bitm->swTex;
    SDL_Surface *converted = nullptr;
    if (source->format != _pixfmt->format)
    {
        converted = ConvertSDLSurface(source, _pixfmt->format);
        if (!converted)
            return false;
        source = converted;
    }

    bitm->hwTex = GPU::CreateTexture(source->w, source->h, _texFormat, GPU::USAGE_SAMPLED);
    if (bitm->hwTex)
    {
        SDL_LockSurface(source);
        GPU::UploadTexture(bitm->hwTex, source->pixels, source->pitch);
        SDL_UnlockSurface(source);
    }

    if (converted)
        SDL_DestroySurface(converted);
    return bitm->hwTex != 0;
}

void GFXEngine::FreeTexture(ResBitmap *bitm)
{
    if ( !bitm->hwTex )
        return;

    // Ids are reused: forget the cached binding of the old texture.
    if ( _states.Tex == bitm->hwTex )
        _states.Tex = 0;
    if ( _lastStates.Tex == bitm->hwTex )
        _lastStates.Tex = 0;

    GPU::DestroyTexture(bitm->hwTex);
    bitm->hwTex = 0;
}

void GFXEngine::display_func271(IDVPair *stak)
{

}

void GFXEngine::win3d_func274__sub0(FSMgr::FileHandle *fil)
{
    int bf_w = 0, bf_h = 0;

    uint8_t *buf = MakeScreenCopy(&bf_w, &bf_h);

    if (buf && bf_w && bf_h)
    {
        fil->printf("P6\n#YPA screenshot\n%d %d\n255\n", bf_w, bf_h);

        int lwidth = 3 * bf_w;

        for (int j = 0; j < bf_h; j++)
        {
            uint8_t *ln = buf + lwidth * (bf_h - 1 - j);

            for (int i = 0; i < bf_w; i++)
            {
                uint8_t *px = ln + i * 3;

                uint8_t r = px[0];
                uint8_t g = px[1];
                uint8_t b = px[2];

                fil->writeU8(r);
                fil->writeU8(g);
                fil->writeU8(b);
            }
        }

        free(buf);
    }
}

void GFXEngine::SaveScreenshot(const std::string & screenName)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(fmt::sprintf("%s.ppm", screenName), "wb");
    if ( fil )
    {
        win3d_func274__sub0(fil);
        delete fil;
    }
}


void GFXEngine::windd_func320(IDVPair *)
{
}

void GFXEngine::windd_func321(IDVPair *)
{
}





char * GFXEngine::windd_func322__sub0(const char *box_title, const char *box_ok, const char *box_cancel, const char *box_startText, uint32_t timer_time, void (*timer_func)(int, int, int), void *timer_context, int replace, int maxLen)
{
    dprintf("MAKE ME %s\n","windd_func322__sub0");
    return nullptr;
}

//Show DLGBox with edit field and get entered value
void GFXEngine::windd_func322(windd_dlgBox *dlgBox)
{
    windd_func320(nullptr);

    dlgBox->result = windd_func322__sub0(
                         dlgBox->title,
                         dlgBox->ok,
                         dlgBox->cancel,
                         dlgBox->startText,
                         dlgBox->time,
                         dlgBox->timer_func,
                         dlgBox->timer_context,
                         dlgBox->replace,
                         dlgBox->maxLen);

    windd_func321(nullptr);
}



const std::vector<TGFXDeviceInfo>& GFXEngine::GetDevices()
{
    return _devices;
}

void GFXEngine::SetDeviceByGUID(const std::string &guid, bool writefile)
{
    std::string guidWrite = guid;

    bool found = false;
    for( TGFXDeviceInfo &dev: _devices )
    {
        if (dev.guid == guid)
        {
            dev.isCurrent = true;
            found = true;
        }
        else
            dev.isCurrent = false;
    }

    if ( !found )
    {
        for( TGFXDeviceInfo &dev: _devices )
        {
            if (dev.guid == "<primary>")
            {
                dev.isCurrent = true;
                found = true;
                guidWrite = "<primary>";
            }
        }

        for( TGFXDeviceInfo &dev: _devices )
        {
            if (dev.guid == "<software>")
            {
                dev.isCurrent = true;
                found = true;
                guidWrite = "<software>";
            }
        }

        if (!_devices.empty())
        {
            TGFXDeviceInfo &dev = _devices[0];
            found = true;
            dev.isCurrent = true;
            guidWrite = dev.guid;
        }
    }

    if (writefile)
    {
        if (guidWrite == "<primary>" || guidWrite == "<software>")
            out_guid_to_file("env/guid3d.def", guidWrite);
        else
            out_guid_to_file("env/guid3d.def", "<error>");
    }
}



void GFXEngine::setWDD_cursor(int mode)
{
}

void GFXEngine::setWDD_16bitTex(int arg)
{
    out_yes_no_status("env/txt16bit.def", arg);
}

void GFXEngine::setWDD_drawPrim(int arg)
{
    out_yes_no_status("env/drawprim.def", arg);
}



int GFXEngine::getWDD_16bitTex()
{
    return _txt16bit;
}

int GFXEngine::getWDD_drawPrim()
{
    return _use_simple_d3d;
}

void GFXEngine::setW3D_texFilt(int arg)
{
    _filter = arg;
}

void GFXEngine::SetPalette(UA_PALETTE &newPal)
{
    _palette = newPal;

    _normClr = vec3d(1.0, 1.0, 1.0);
    _invClr = vec3d(0.0, 0.0, 0.0);
}

void GFXEngine::SetPen(SDL_Color pen)
{
    _field_4 = pen;
}

UA_PALETTE *GFXEngine::GetPalette()
{
    return &_palette;
}




void GFXEngine::draw2DandFlush()
{
    if (_colorEffects)
        GPU::SetTarget(GPU::BACKBUFFER, GPU::BACKBUFFER_DEPTH);

    Gui::Root::Instance.Draw(Screen());
    if (_virtualUiPass)
    {
        // Briefing/debriefing performs an intermediate 2D flush while the
        // shell virtual-UI pass is still open. Present the current low-res
        // layer now, then let the caller continue drawing into the cleared
        // virtual surface for the final composition.
        _virtualUiPending = true;
        DrawVirtualUISurface();
    }
    else
    {
        DrawScreenSurface();
        DrawVirtualUISurface();
    }
    Gui::Root::Instance.HwCompose();

    SDL_FillSurfaceRect(Screen(), nullptr, SDL_MapSurfaceRGBA(Screen(), 0, 0, 0, 0) );

    if (_colorEffects)
        GPU::SetTarget(_sceneTex, _sceneDepth);
}

void GFXEngine::matrixAspectCorrection(mat3x3 &inout, bool invert)
{
    if (invert)
    {
        inout.m00 *= _corrIW;
        inout.m01 *= _corrIW;
        inout.m02 *= _corrIW;
        inout.m10 *= _corrIH;
        inout.m11 *= _corrIH;
        inout.m12 *= _corrIH;
    }
    else
    {
        inout.m00 *= _corrW;
        inout.m01 *= _corrW;
        inout.m02 *= _corrW;
        inout.m10 *= _corrH;
        inout.m11 *= _corrH;
        inout.m12 *= _corrH;
    }
}

void GFXEngine::getAspectCorrection(float &cW, float &cH, bool invert)
{
    if (invert)
    {
        cW = _corrIW;
        cH = _corrIH;
    }
    else
    {
        cW = _corrW;
        cH = _corrH;
    }
}

void GFXEngine::viewZoomCorrection(float &x, float &y, bool invert) const
{
    const float correction = invert ? 1.0f / _viewZoom : _viewZoom;
    x *= correction;
    y *= correction;
}

void GFXEngine::_setFrustumClip(float _near, float _far)
{
    //-z * frustum
    _frustumNear = _near;
    _frustumFar = _far;

    _frustum = mat4x4f::UAFrustum(_near, _far);
    _frustum.m00 *= _viewZoom;
    _frustum.m11 *= _viewZoom;
}

void GFXEngine::setViewZoom(float zoom)
{
    if (!std::isfinite(zoom))
        zoom = 1.0f;

    if (zoom < VIEW_ZOOM_MIN)
        zoom = VIEW_ZOOM_MIN;
    else if (zoom > VIEW_ZOOM_MAX)
        zoom = VIEW_ZOOM_MAX;

    if (std::fabs(_viewZoom - zoom) < 0.0001f)
        return;

    _viewZoom = zoom;
    _frustum = mat4x4f::UAFrustum(_frustumNear, _frustumFar);
    _frustum.m00 *= _viewZoom;
    _frustum.m11 *= _viewZoom;
}

void GFXEngine::ConvAlphaPalette(UA_PALETTE *dst, const UA_PALETTE &src, bool transp)
{
    for (uint16_t i = 0; i < dst->size(); i++)
    {
        SDL_Color &c = (*dst)[i];
        c = src[i];
        c.a = 255;

        if (c.r == 255 && c.g == 255 && c.b == 0)
        {
            c.a = 0;
            c.r = 0;
            c.g = 0;
            c.b = 0;
        }
        else
        {
            if (!can_destblend && can_srcblend && transp)
            {
                int mx = (c.r >= c.g) ? (c.r > c.b ? c.r: c.b) : (c.g > c.b ? c.g : c.b);

                if (mx <= 8)
                {
                    c.r = 0;
                    c.g = 0;
                    c.b = 0;
                    c.a = 0;
                }
                else
                {
                    float prm = mx;
                    c.r = 255.0 * (c.r / prm);
                    c.g = 255.0 * (c.g / prm);
                    c.b = 255.0 * (c.b / prm);
                    c.a = mx;
                }
            }
            else
            {
                c.a = 255;
            }
        }
    }
}

SDL_Surface *GFXEngine::CreateSurfaceScreenFormat(int width, int height)
{
    return SDL_CreateSurface(width, height, _pixfmt->format);
}

SDL_Surface *GFXEngine::ConvertToScreenFormat(SDL_Surface *src)
{
    return ConvertSDLSurface(src, _pixfmt->format);
}

SDL_Surface * GFXEngine::ConvertSDLSurface(SDL_Surface *src, SDL_PixelFormat fmt)
{
    return SDL_ConvertSurface(src, fmt);
}


}


const Common::PointRect &TileMap::Glyph(uint32_t key) const
{
    if (extraImg)
    {
        const auto extra = extraGlyphs.find(key);
        if (extra != extraGlyphs.end()) return extra->second;
    }
    if (key < map.size()) return map[key];
    const auto found = extendedGlyphs.find(key);
    if (found != extendedGlyphs.end()) return found->second;
    const uint32_t legacy = GlyphKey::LegacyAlias(key);
    return map[legacy < map.size() ? legacy : 0];
}

SDL_Surface *TileMap::Page(uint32_t key) const
{
    if (extraImg && extraGlyphs.count(key))
        return extraImg->GetSwTex();
    return img->GetSwTex();
}

TileMap::TileMap()
{
    img = nullptr;
    h = 0;
}

TileMap::~TileMap()
{
    if (img)
        img->Delete();
    if (extraImg)
        extraImg->Delete();
}

void TileMap::Draw(SDL_Surface *surface, const Common::Point &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Draw(SDL_Surface *surface, const Common::PointRect &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    if (src.w > pos.w)
        src.w = pos.w;
    if (src.h > pos.h)
        src.h = pos.h;
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Draw(SDL_Surface *surface, const Common::Rect &pos, uint8_t c)
{
    SDL_Rect src = map[c];
    if (src.w > pos.Width())
        src.w = pos.Width();
    if (src.h > pos.Height())
        src.h = pos.Height();
    SDL_Rect dst = pos;
    SDL_BlitSurface(img->GetSwTex(), &src, surface, &dst);
}

void TileMap::Fill(SDL_Surface *surface, const Common::Rect &rect, uint8_t c)
{
    GFX::Engine.DrawFill(img->GetSwTex(), map[c], surface, rect);
}

void TileMap::Fill(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c)
{
    GFX::Engine.DrawFill(img->GetSwTex(), map[c], surface, rect);
}

void TileMap::FillColumn(SDL_Surface *surface, const Common::Rect &rect, uint8_t c)
{
    Common::PointRect sPRect = map[c];
    sPRect.w = 1;
    GFX::Engine.DrawFill(img->GetSwTex(), sPRect, surface, rect);
}

void TileMap::FillColumn(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c)
{
    Common::PointRect sPRect = map[c];
    sPRect.w = 1;
    GFX::Engine.DrawFill(img->GetSwTex(), sPRect, surface, rect);
}

int TileMap::GetWidth(uint8_t c) const
{
    return map[c].w;
}

int TileMap::GetWidth(const std::string &str) const
{
    int wdth = 0;
    for (uint8_t c : str)
        wdth += map[c].w;
    return wdth;
}

Common::Point TileMap::GetSize(uint8_t c) const
{
    return Common::Point(map[c].w, h);
}












namespace GFX
{

GfxMode::GfxMode(GfxMode &&g)
{
    w = std::move(g.w);
    h = std::move(g.h);
    bpp = std::move(g.bpp);
    name = std::move(g.name);
    windowed = std::move(g.windowed);
}

GfxMode::GfxMode(const GfxMode &g)
{
    w = g.w;
    h = g.h;
    bpp = g.bpp;
    name = g.name;
    windowed = g.windowed;
}

GfxMode::GfxMode(const Common::Point &sz)
: w(sz.x), h(sz.y)
{
    name = GenName(w, h);
}

GfxMode& GfxMode::operator=(const GfxMode &g)
{
    w = g.w;
    h = g.h;
    bpp = g.bpp;
    name = g.name;
    windowed = g.windowed;
    return *this;
}

GfxMode::operator bool() const
{
    if (w == 0 || h == 0)
        return false;
    return true;
}

bool GfxMode::operator==(const GfxMode &g) const
{
    return (w == g.w && h == g.h);
}

bool GfxMode::operator==(const Common::Point &g) const
{
    return (w == g.x && h == g.y);
}

bool GfxMode::operator!=(const GfxMode &g) const
{
    return w != g.w || h != g.h;
}

bool GfxMode::operator!=(const Common::Point &g) const
{
    return w != g.x || h != g.y;
}

bool GfxMode::SortCompare(const GfxMode &a, const GfxMode &b)
{
    if (a.w > b.w)
        return true;
    else if (a.w == b.w && a.h > b.h)
        return true;
    return false;
}

std::string GfxMode::GenName(int w, int h)
{
    return fmt::sprintf("%d x %d", w, h);
}




void GFXEngine::AddGfxMode(const GfxMode &md)
{
    for (const GfxMode &m : graphicsModes)
    {
        if ( m.w == md.w && m.h == md.h )
            return;
    }

    graphicsModes.push_back(md);
}


uint32_t GFXEngine::CursPix(uint8_t *data, int ofs, int bpp)
{
    switch (bpp)
    {
    case 1:
        return (data[ofs / 8] >> (7 - ofs % 8)) & 1;
    case 2:
        return (data[ofs / 4] >> ((3 - ofs % 4) << 1)) & 3;
    case 4:
        return (data[ofs / 2] >> ((1 - ofs % 2) << 2)) & 15;
    case 8:
        return data[ofs];
    case 16:
        return data[2 * ofs] | data[2 * ofs + 1] << 8;
    case 24:
        return data[3 * ofs] | data[3 * ofs + 1] << 8 | data[ 3 * ofs + 2] << 16;
    case 32:
        return data[4 * ofs] | data[4 * ofs + 1] << 8 | data[4 * ofs + 2] << 16 | data[4 * ofs + 3] << 24;
    }

    return 0;
}

// Loads the 11 mouse cursors from the user's game data.
void GFXEngine::LoadCursorSet(bool retro)
{
    static const char *const names[11] =
    {
        "Pointer", "Cancel", "Select", "Attack", "Goto", "Disk",
        "New", "Add", "Control", "Beam", "Build"
    };

    const std::array<SDL_Cursor *, 11> previous = cursors;

    for (size_t i = 0; i < cursors.size(); i++)
    {
        cursors[i] = LoadCursor(names[i], retro);
        if ( !cursors[i] )
            cursors[i] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    }

    if ( CurrentCursorID > 0 )
        SetCursor(CurrentCursorID, 1);

    for (SDL_Cursor *old : previous)
        if ( old )
            SDL_DestroyCursor(old);
}

SDL_Cursor *GFXEngine::LoadCursor(const std::string &name, bool retro)
{
    FSMgr::iFileHandle *fil = uaOpenFileAlloc(
        fmt::sprintf(retro ? "Res/Cursors/Retro/%s.cur" : "Res/Cursors/Classic/%s.cur", name), "rb");
    if (!fil && !retro)
    {
        const std::string previousPrefix = Common::Env.SetPrefix("rsrc", "data:mc2res");
        std::string originalName = name;
        std::transform(originalName.begin(), originalName.end(), originalName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        NC_STACK_bitmap *bitmap = Utils::ProxyLoadImage({
            {NC_STACK_rsrc::RSRC_ATT_NAME, fmt::sprintf("pointers/%s.ilbm", originalName)},
            {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});
        Common::Env.SetPrefix("rsrc", previousPrefix);
        SDL_Cursor *original = nullptr;
        if (bitmap)
        {
            ResBitmap *image = bitmap->GetBitmap();
            if (image && image->swTex)
                original = SDL_CreateColorCursor(image->swTex, 0, 0);
            bitmap->Delete();
        }
        return original;
    }

    UA_PALETTE pal;

    if (!fil)
        return nullptr;

    fil->readU16L();
    if (fil->readU16L() != 2)
    {
        delete fil;
        return nullptr;
    }

    fil->readU16L(); //count

    //Only first entry
    fil->readU8(); //w
    fil->readU8(); //h
    fil->readU8(); //color count
    fil->readU8(); //reserved
    int hotX = fil->readU16L();
    int hotY = fil->readU16L();
    fil->readU32L(); //size
    int off = fil->readU32L();

    //seek to cursor
    fil->seek(off, SEEK_SET);

    //read InfoHeader
    fil->readU32L(); //header size
    int bmpw = fil->readS32L();
    int bmph = fil->readS32L();
    fil->readU16L(); //planes
    int bitcount = fil->readU16L();
    fil->readU32L(); //compression
    fil->readU32L(); //imagesize
    fil->readU32L(); //XpixelsPerM
    fil->readU32L(); //YpixelsPerM
    int clrused = fil->readU32L(); //ColorsUsed
    fil->readU32L(); //ColorsImportant

    // read pallete
    int palcnt = 0;
    if (clrused == 0 || bitcount < 16)
    {
        palcnt = clrused != 0 ? clrused : 1 << bitcount;

        for (int i = 0; i < palcnt; i++)
        {
            pal[i].r = fil->readU8();
            pal[i].g = fil->readU8();
            pal[i].b = fil->readU8();
            fil->readU8(); //reserved
        }
    }

    int width = bmpw;
    int height = abs(bmph)/2;

    int imgsz = height * width * bitcount / 8;
    int mask_size = height * width / 8;

    uint8_t *data = (uint8_t *)malloc(imgsz);
    uint8_t *mask = (uint8_t *)malloc(mask_size);

    fil->read(data, imgsz);
    fil->read(mask, mask_size);

    delete fil;


    SDL_Surface *cursr = SDL_CreateSurface(width, height, SDL_GetPixelFormatForMasks(32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000));

    for (int y = 0; y < height; y++)
    {
        int invY = height - 1 - y;
        uint8_t *row = (uint8_t *)cursr->pixels + y * cursr->pitch;

        for (int x = 0; x < width; x++)
        {
            if (palcnt > 0)
            {
                int idx = CursPix(data, invY * width + x, bitcount);
                int alph = CursPix(mask, invY * width + x, 1);
                row[x * 4 + 0] = pal[ idx ].r;
                row[x * 4 + 1] = pal[ idx ].g;
                row[x * 4 + 2] = pal[ idx ].b;
                row[x * 4 + 3] = (1 - alph) * 255;
            }
            else
            {
                uint32_t clr = CursPix(data, invY * width + x, bitcount);
                int alph = CursPix(mask, invY * width + x, 1);
                row[x * 4 + 0] = clr & 0xFF;
                row[x * 4 + 1] = (clr >> 8) & 0xFF;
                row[x * 4 + 2] = (clr >> 16) & 0xFF;
                row[x * 4 + 3] = (1 - alph) * 255;
            }
        }
    }

    free(data);
    free(mask);

    SDL_Cursor* cursor = SDL_CreateColorCursor(cursr, hotX, hotY);

    SDL_DestroySurface(cursr);

    return cursor;
}

void GFXEngine::Init()
{
    StaticInit();

    if (!GPU::Init(System::GetWindow(), System::IniConf::GfxVsync.Get<int>(), sizeof(TVertex)))
    {
        // No OpenGL fallback: SDL_GPU needs a Vulkan driver for this window.
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenNeoUA",
            "No graphics card with Vulkan support was found.\n"
            "Update the graphics driver, then start OpenNeoUA again.", System::GetWindow());
        std::exit(1);
    }

    SetDeviceByGUID( read_guid("env/guid3d.def") );

    System::EventsAddHandler(EventsWatcher);

    System::IniConf::ReadFromNucleusIni();
    HorizonLoadConfigFromIni();
    LoadCustomColorEffectsFromConfig();

    _colorEffects = System::IniConf::GfxColorEffects.Get<int32_t>();

    std::array<Common::Point, 17> checkModes
    {{
        {640, 480},     {800, 600},     {1024, 768},    {1280, 1024},
        {1440, 1050},   {1600, 1200},   {720, 480},     {852, 480},
        {1280, 720},    {1366, 768},    {1600, 900},    {1920, 1080},
        {1920, 1200},   {2560, 1080},   {2560, 1440},   {3440, 1440},
        {3840, 2160}
     }};

    graphicsModes.reserve(checkModes.size());

    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    for(Common::Point m : checkModes)
    {
        SDL_DisplayMode closest;
        if ( SDL_GetClosestFullscreenDisplayMode(display, m.x, m.y, 0.0f, false, &closest) )
        {
            GfxMode mode;
            mode.w = closest.w;
            mode.h = closest.h;
            mode.bpp = _pixfmt->bytes_per_pixel;
            mode.name = GfxMode::GenName(mode.w, mode.h);

            AddGfxMode(mode);
        }
    }

    // Force to add custom resolutions
    std::vector<std::string> customModes = Stok::Split(System::IniConf::GfxAdditionalModes.Get<std::string>(), ",");
    for (std::string mod : customModes)
    {
        std::vector<std::string> vals = Stok::Split(mod, ":-x \t");
        if (vals.size() >= 2)
        {
            SDL_DisplayMode closest;

            GfxMode mode;
            mode.w = std::stoi(vals[0]);
            mode.h = std::stoi(vals[1]);
            mode.bpp = _pixfmt->bytes_per_pixel;
            mode.name = GfxMode::GenName(mode.w, mode.h);

            if ( SDL_GetClosestFullscreenDisplayMode(display, mode.w, mode.h, 0.0f, false, &closest) )
                mode.bpp = SDL_BYTESPERPIXEL(closest.format) * 8;

            AddGfxMode(mode);
        }
    }



    std::sort(graphicsModes.begin(), graphicsModes.end(), GfxMode::SortCompare);

    LoadCursorSet(System::IniConf::UiRetroCursors.Get<bool>());

    IDVList stak { {ATT_WIDTH,  (int32_t)640},
            {ATT_HEIGHT, (int32_t)480} };
    func0( stak );

    RecreateScreenSurface();
    Gui::Instance.SetScreenSize(GetScreenSize());

    LoadPalette(System::IniConf::GfxPalette.Get<std::string>());

    std::string error;
    _stdShaderProg = GPU::CreateProgram(_stdVShaderText, _stdPShaderText, 1, "standard", &error);
    if (!_stdShaderProg)
    {
        log_d3dlog("Standard shader failed to compile: %s\n", error.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenNeoUA",
                                 "The standard shader could not be built (see env/d3dlog.txt).",
                                 System::GetWindow());
        std::exit(1);
    }

    _decalProg = GPU::CreateProgram(DecalVertexShaderText(), DecalFragmentShaderText(), 3, "decal", &error);
    if (!_decalProg)
        log_d3dlog("Decal shader failed to compile: ground decals are disabled\n%s\n", error.c_str());
    else
    {
        _decalRestProg = GPU::CreateProgram(DecalRestVertexShaderText(), DecalRestFragmentShaderText(), 1,
                                            "decal_rest", &error);
        if (!_decalRestProg)
            log_d3dlog("Decal rest shader failed to compile: decals do not follow craters\n%s\n", error.c_str());
    }

    if (_colorEffects > 0)
    {
        CreateSceneTargets(System::GetResolution());
        _colorEffectsShaderProg = LoadPostProgram("res/clreff.ps", "Colour effects");
        // Without its post program the scene is drawn straight to the window.
        if (!_colorEffectsShaderProg)
            _colorEffects = 0;
    }

    // OpenNeoUA custom: load the fullscreen visual filter selected in nucleus.ini.
    // Safe no-op when "Standard"/empty or when the file is missing.
    ApplyVisualFilterFromConfig();

    // OpenNeoUA custom: world-only atmospheric color pass. When enabled, it uses
    // the dedicated shader; if that shader is absent or cannot be linked, the
    // renderer falls back to the existing DrawFBO path without changing vanilla.
    ApplyAtmosphereFromConfig();

    // OpenNeoUA: VHS pass enabled by default, loaded from its own INI shader path.
    ApplyVhsFilterFromConfig();
}

void GFXEngine::Deinit()
{
    ClearUiAccentCache();
    ClearLocationTitle();

    if ( _font.ttfFont )
    {
        TTF_CloseFont(_font.ttfFont);
        _font.ttfFont = nullptr;
    }

    if (ScreenSurface)
        SDL_DestroySurface(ScreenSurface);

    ScreenSurface = nullptr;

    if (VirtualUISurface)
        SDL_DestroySurface(VirtualUISurface);

    VirtualUISurface = nullptr;
    _virtualUiPass = false;
    _virtualUiPending = false;
    _virtualUiResolution = Common::Point();
    _virtualUiTextureSize = Common::Point();

    GPU::DestroyTexture(screenTex);
    GPU::DestroyTexture(virtualUiTex);
    screenTex = virtualUiTex = 0;

    GPU::DestroyProgram(_stdShaderProg);
    GPU::DestroyProgram(_decalProg);
    GPU::DestroyProgram(_decalRestProg);
    GPU::DestroyTexture(_decalDepthTex);
    GPU::DestroyTexture(_decalRestTex);
    _stdShaderProg = _decalProg = _decalRestProg = 0;
    _decalDepthTex = _decalRestTex = 0;
    _decalDepthTexSize = _decalRestTexSize = Common::Point();

    FreeAtmosphereShader();
    _atmosphereEnabled = false;
    _atmosphereActive = false;

    GPU::DestroyProgram(_colorEffectsShaderProg);
    _colorEffectsShaderProg = 0;
    GPU::DestroyTexture(_sceneTex);
    GPU::DestroyTexture(_sceneDepth);
    _sceneTex = _sceneDepth = 0;
    _sceneSize = Common::Point();

    // OpenNeoUA custom: free the visual filter LUT texture
    GPU::DestroyTexture(_visualFilterLut);
    _visualFilterLut = 0;
    _visualFilterActive = false;

    FreeVhsFilterShader();
    GPU::DestroyTexture(_vhsCopyTex);
    GPU::DestroyTexture(_vhsOutTex);
    _vhsCopyTex = _vhsOutTex = 0;
    _vhsCopyTexSize = Common::Point();
    _vhsOutTexSize = Common::Point();
    _vhsFilterActive = false;

    GPU::Shutdown();
}

GFXEngine::~GFXEngine()
{
    Deinit();
}

bool GFXEngine::EventsWatcher(void *, SDL_Event *event)
{
    if ( event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED )
        Instance.UpdateFBOSizes();

    return true; // This event can be passed to another event watcher
}

void GFXEngine::SetCursor(int curID, int force)
{
    int sett = 0;

    if ( force )
        sett = 1;
    else if ( curID != CurrentCursorID )
        sett = 1;

    if ( sett )
    {
        if ( curID == 0 )
            SDL_HideCursor();
        else if ( curID <= 11 )
        {
            if ( cursors[curID - 1] )
                SDL_SetCursor( cursors[curID - 1] );

            if (!CurrentCursorID)
                SDL_ShowCursor();
        }

    }

    CurrentCursorID = curID;
}

GfxMode GFXEngine::sub_41F68C()
{
    for (const GfxMode &m : graphicsModes)
    {
        if (m.w == 640 && m.h == 480)
            return m;
    }

    return graphicsModes.front();
}


GfxMode GFXEngine::windd_func0__sub0(const std::string &file)
{
    FSMgr::FileHandle *fil = uaOpenFileAlloc(file, "r");

    if ( fil )
    {
        std::string line;
        if ( fil->ReadLine(&line) )
        {
            size_t pos = line.find_first_of("\n\r");

            if (pos != std::string::npos)
                line.erase(pos);

            bool windowed = false;
            pos = line.find("Windowed");

            if (pos != std::string::npos && pos >= 1)
            {
                windowed = true;
                line.erase(pos - 1);
            }

            for (const GfxMode &m : graphicsModes)
            {
                if ( StriCmp(m.name, line) == 0 )
                {
                    GfxMode tmp = m;
                    tmp.windowed = windowed;
                    return tmp;
                }
            }
        }
        delete fil;
    }

    return sub_41F68C();
}

int GFXEngine::GetGfxModeIndex(const Common::Point &res)
{
    int i = 0;
    for (const GfxMode &m : graphicsModes)
    {
        if (m == res)
            return i;
        ++i;
    }
    return -1;
}

void GFXEngine::SetResolution(const Common::Point &res, bool windowed)
{
    if (GfxSelectedMode == res && GfxSelectedMode.windowed == windowed)
        return;

    UA_PALETTE *screen_palette = GetPalette();

    UA_PALETTE palette_copy;

    if ( screen_palette )
        palette_copy = *screen_palette; // Copy palette
    else
    {
        for(auto &x : palette_copy)
        {
            x.r = 0;
            x.g = 0;
            x.b = 0;
            x.a = 0;
        }
    }

    EndFrame();


    GfxMode picked;
    if ( res )
    {
        for (const GfxMode &m : graphicsModes)
        {
            if ( m.w == res.x && m.h == res.y )
            {
                picked = m;
                picked.windowed = windowed;
                break;
            }
        }

        if ( !picked )
            picked = sub_41F68C();
    }
    else
    {
        picked = windd_func0__sub0("env/vid.def");
    }

    System::SetVideoMode(Common::Point(picked.w, picked.h), !picked.windowed);

    SetResVariables(picked);
    ApplyResolution();

    RecreateScreenSurface();

    BeginFrame();
    SetPalette(palette_copy);

    GfxSelectedMode = picked;

    FSMgr::FileHandle *fil = uaOpenFileAlloc("env/vid.def", "w");
    if ( fil )
    {
        if (picked.windowed)
            fil->printf("%s Windowed\n", picked.name.c_str());
        else
            fil->printf("%s\n", picked.name.c_str());

        delete fil;
    }

    Gui::Instance.SetScreenSize(GetScreenSize());

    UpdateFBOSizes();
}

const std::vector<GfxMode> &GFXEngine::GetAvailableModes()
{
    return graphicsModes;
}




void GFXEngine::SetTracyRmp(ResBitmap *rmp)
{
    setRSTR_trcRmp(rmp);
}

void GFXEngine::SetShadeRmp(ResBitmap *rmp)
{
    setRSTR_shdRmp(rmp);
}

GfxMode GFXEngine::GetGfxMode()
{
    return GfxSelectedMode;
}

TileMap * GFXEngine::GetTileset(int id)
{
    return raster_func208(id);
}

void GFXEngine::SetTileset(TileMap *tileset, int id)
{
    raster_func207(id, tileset);
}


bool GFXEngine::LoadPalette(const std::string &palette_ilbm)
{
    NC_STACK_bitmap *ilbm = Nucleus::CInit<NC_STACK_ilbm>( {
        {NC_STACK_rsrc::RSRC_ATT_NAME, palette_ilbm},
        {NC_STACK_bitmap::BMD_ATT_HAS_COLORMAP, (int32_t)1}} );

    if (!ilbm)
        return false;

    SetPalette( *ilbm->getBMD_palette() );

    ilbm->Delete();

    return true;
}

// OpenNeoUA custom: read gfx.palette_filter_strength ("0.0".."1.0") with a safe default.
// NUCLEUS.INI is the single source of truth; missing/empty/invalid values only fall
// back in memory and are not rewritten unless the user saves Options.
static float ParseVisualFilterStrength(std::string s, float fallback)
{
    if (s.empty())
        return fallback;

    try
    {
        if (s.find(',') != std::string::npos)
            return fallback;

        size_t pos = 0;
        float strength = std::stof(s, &pos);
        size_t rest = s.find_first_not_of(" \t\r\n", pos);
        if (rest != std::string::npos)
            return fallback;

        if (strength < 0.0f) strength = 0.0f;
        if (strength > 1.0f) strength = 1.0f;
        return strength;
    }
    catch (...)
    {
        return fallback;
    }
}

static float ReadVisualFilterStrength()
{
    const float defaultStrength = 0.25f; // default if missing/invalid

    return ParseVisualFilterStrength(System::IniConf::GfxVisualFilterStrength.Get<std::string>(), defaultStrength);
}

// OpenNeoUA custom: select the fullscreen visual filter.
// filterName == "Standard"/"None"/"Original"/empty disables the filter (no visual change).
// Otherwise loads Data/Filters/<name>.pal as a 256-entry RGB LUT (read with the normal
// ILBM/CMAP loader) and uploads it to a 256x1 GL texture used by the post-process shader.
void GFXEngine::SetVisualFilter(const std::string &filterName)
{
    // Trim whitespace
    std::string name = filterName;
    size_t a = name.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        name.clear();
    else
    {
        size_t b = name.find_last_not_of(" \t\r\n");
        name = name.substr(a, b - a + 1);
    }

    float strength = ReadVisualFilterStrength();

    bool disable = name.empty()
                || !StriCmp(name, "Standard")
                || !StriCmp(name, "Original")
                || !StriCmp(name, "None");

    if (disable)
    {
        _visualFilterActive = false;
        _visualFilterStrength = 0.0f;
        _visualFilterName = "Standard";
        ypa_log_out("Visual filter: disabled (Standard / no fullscreen filter, rendering unchanged)\n");
        return;
    }

    // Ensure ".pal" extension
    std::string file = name;
    if (file.size() < 4 || StriCmp(file.substr(file.size() - 4), ".pal"))
        file += ".pal";

    const std::string loadPath = "Filters/" + file;

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:");

    NC_STACK_bitmap *ilbm = Nucleus::CInit<NC_STACK_ilbm>( {
        {NC_STACK_rsrc::RSRC_ATT_NAME, loadPath},
        {NC_STACK_bitmap::BMD_ATT_HAS_COLORMAP, (int32_t)1}} );

    Common::Env.SetPrefix("rsrc", oldRsrc);

    UA_PALETTE *pal = ilbm ? ilbm->getBMD_palette() : nullptr;
    if (!pal)
    {
        ypa_log_out("WARNING: Visual filter [%s] could not be loaded from [data:%s]; filter disabled.\n",
                    name.c_str(), loadPath.c_str());
        if (ilbm)
            ilbm->Delete();
        _visualFilterActive = false;
        _visualFilterStrength = 0.0f;
        _visualFilterName = "Standard";
        return;
    }

    // OpenNeoUA: build a SMOOTH luminance grade from the palette.
    // A UA .pal CMAP is an arbitrary indexed game palette. Even sorted by luminance it has
    // harsh chroma jumps (colors of similar luminance but very different hue) which show up
    // as red/blue speckle. So we:
    //   1) sort the 256 colors by perceived luminance,
    //   2) average them into a few equal-count buckets (non-empty by construction),
    //   3) linearly interpolate the bucket averages back into a 256-entry ramp,
    //   4) run a small moving-average smoothing pass.
    // The result is a smooth color-grade ramp, not indexed-palette noise.
    struct LumColor { float r, g, b, lum; };
    std::array<LumColor, 256> sorted;
    for (int i = 0; i < 256; i++)
    {
        const SDL_Color &c = (*pal)[i];
        sorted[i].r = c.r;
        sorted[i].g = c.g;
        sorted[i].b = c.b;
        sorted[i].lum = 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
    }

    std::sort(sorted.begin(), sorted.end(),
              [](const LumColor &a, const LumColor &b) { return a.lum < b.lum; });

    // Equal-count buckets averaged into control colors.
    const int kBuckets = 16;
    std::array<float, kBuckets> br, bg, bb;
    for (int k = 0; k < kBuckets; k++)
    {
        int start = (k * 256) / kBuckets;
        int end   = ((k + 1) * 256) / kBuckets;
        if (end <= start)
            end = start + 1;

        float sr = 0, sg = 0, sb = 0;
        for (int j = start; j < end; j++)
        {
            sr += sorted[j].r;
            sg += sorted[j].g;
            sb += sorted[j].b;
        }
        float n = float(end - start);
        br[k] = sr / n;
        bg[k] = sg / n;
        bb[k] = sb / n;
    }

    // Interpolate the bucket control colors into a continuous 256-entry ramp.
    float rampR[256], rampG[256], rampB[256];
    for (int i = 0; i < 256; i++)
    {
        float t = (i / 255.0f) * (kBuckets - 1);
        int lo = (int)t;
        int hi = lo + 1;
        if (hi > kBuckets - 1)
            hi = kBuckets - 1;
        float f = t - lo;
        rampR[i] = br[lo] + (br[hi] - br[lo]) * f;
        rampG[i] = bg[lo] + (bg[hi] - bg[lo]) * f;
        rampB[i] = bb[lo] + (bb[hi] - bb[lo]) * f;
    }

    // Moving-average smoothing pass (removes any residual chroma steps).
    auto clamp8 = [](float v) -> uint8_t
    {
        if (v < 0.0f)   v = 0.0f;
        if (v > 255.0f) v = 255.0f;
        return (uint8_t)(v + 0.5f);
    };

    std::vector<uint8_t> lut(256 * 3); // tightly packed r,g,b,r,g,b...
    const int radius = 3;
    for (int i = 0; i < 256; i++)
    {
        float sr = 0, sg = 0, sb = 0;
        int cnt = 0;
        for (int d = -radius; d <= radius; d++)
        {
            int j = i + d;
            if (j < 0)   j = 0;
            if (j > 255) j = 255;
            sr += rampR[j];
            sg += rampG[j];
            sb += rampB[j];
            cnt++;
        }
        lut[i * 3 + 0] = clamp8(sr / cnt);
        lut[i * 3 + 1] = clamp8(sg / cnt);
        lut[i * 3 + 2] = clamp8(sb / cnt);
    }

    ilbm->Delete();

    // 256x1 RGBA ramp, sampled with linear filtering and clamped edges.
    std::array<uint8_t, 256 * 4> rgba;
    for (int i = 0; i < 256; i++)
    {
        rgba[i * 4 + 0] = lut[i * 3 + 0];
        rgba[i * 4 + 1] = lut[i * 3 + 1];
        rgba[i * 4 + 2] = lut[i * 3 + 2];
        rgba[i * 4 + 3] = 255;
    }
    if (!_visualFilterLut)
        _visualFilterLut = GPU::CreateTexture(256, 1, GPU::Format::RGBA8, GPU::USAGE_SAMPLED);
    GPU::UploadTexture(_visualFilterLut, rgba.data(), 256 * 4);

    _visualFilterActive = true;
    _visualFilterStrength = strength;
    _visualFilterName = name;

    const bool postFxOn = (_colorEffects > 0);
    ypa_log_out("Visual filter: ENABLED name=[%s] path=[data:%s] strength=%.2f post_process=%s%s\n",
                name.c_str(), loadPath.c_str(), strength,
                postFxOn ? "on" : "off",
                postFxOn ? "" : " (WARNING: gfx.color_effects=0 -> post-process pass disabled, filter not visible)");
}

void GFXEngine::SetVisualFilterStrength(float strength)
{
    if (!std::isfinite(strength))
        strength = 0.0f;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    _visualFilterStrength = strength;
}

// OpenNeoUA custom: apply the visual filter selected in nucleus.ini (gfx.palette_filter).
void GFXEngine::ApplyVisualFilterFromConfig()
{
    SetVisualFilter(System::IniConf::GfxVisualFilter.Get<std::string>());
}

static float ParseVhsFilterStrength(std::string s, float fallback)
{
    if (s.empty())
        return fallback;

    try
    {
        size_t pos = 0;
        float strength = std::stof(s, &pos);
        if ( s.find_first_not_of(" \t\r\n", pos) != std::string::npos ||
             !std::isfinite(strength) )
            return fallback;

        if (strength < 0.0f) strength = 0.0f;
        if (strength > 1.0f) strength = 1.0f;
        return strength;
    }
    catch (...)
    {
        return fallback;
    }
}

static float ParseAtmosphereValue(std::string s, float fallback, float minValue, float maxValue)
{
    s = StrTrim(s);
    if (s.empty() || s.find(',') != std::string::npos)
        return fallback;

    try
    {
        size_t pos = 0;
        float value = std::stof(s, &pos);
        if (s.find_first_not_of(" \t\r\n", pos) != std::string::npos || !std::isfinite(value))
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

void GFXEngine::FreeAtmosphereShader()
{
    GPU::DestroyProgram(_atmosphereShaderProg);
    _atmosphereShaderProg = 0;
}

bool GFXEngine::LoadAtmosphereShader()
{
    if (_atmosphereShaderProg)
        return true;

    _atmosphereShaderProg = LoadPostProgram("res/atmosphere.ps", "Atmospheric pass");
    if (!_atmosphereShaderProg)
        ypa_log_out("WARNING: Atmospheric pass unavailable; continuing with the existing renderer.\n");
    return _atmosphereShaderProg != 0;
}

void GFXEngine::ApplyAtmosphereFromConfig()
{
    _atmosphereEnabled = System::IniConf::GfxAtmosphereFx.Get<bool>();
    _atmosphereActive = false;

    if (!_atmosphereEnabled)
    {
        FreeAtmosphereShader();
        ypa_log_out("Atmospheric pass: disabled (gfx.atmosphere_fx=no; rendering order unchanged)\n");
        return;
    }

    _atmosphereStrength = ParseAtmosphereValue(
        System::IniConf::GfxAtmosphereStrength.Get<std::string>(), 0.5f, 0.0f, 1.0f);
    _atmosphereExposure = ParseAtmosphereValue(
        System::IniConf::GfxAtmosphereExposure.Get<std::string>(), 1.7f, 0.25f, 2.0f);
    _atmosphereContrast = ParseAtmosphereValue(
        System::IniConf::GfxAtmosphereContrast.Get<std::string>(), 0.95f, 0.50f, 2.0f);
    _atmosphereSaturation = ParseAtmosphereValue(
        System::IniConf::GfxAtmosphereSaturation.Get<std::string>(), 0.8f, 0.0f, 2.0f);
    _atmosphereVignette = ParseAtmosphereValue(
        System::IniConf::GfxAtmosphereVignette.Get<std::string>(), 0.6f, 0.0f, 1.0f);

    if (_colorEffects <= 0)
    {
        ypa_log_out("WARNING: Atmospheric pass requested but gfx.color_effects=0 disables the scene framebuffer; continuing with the existing renderer.\n");
        return;
    }

    if (!LoadAtmosphereShader())
        return;

    _atmosphereActive = true;
    ypa_log_out("Atmospheric pass: ENABLED strength=%.2f exposure=%.2f contrast=%.2f saturation=%.2f vignette=%.2f scope=world_before_ui\n",
                _atmosphereStrength,
                _atmosphereExposure,
                _atmosphereContrast,
                _atmosphereSaturation,
                _atmosphereVignette);
}

// Blend of the VHS image over the finished frame (post shader body, see _postPrelude).
static const char *VhsBlendShaderText()
{
    return
        "void main()\n"
        "{\n"
        "    vec4 base = sampleScreen(uTexture, texCoords);\n"
        "    vec4 vhs = sampleScreen(uAux, texCoords);\n"
        "    fragColor = mix(base, vhs, clamp(vhsStrength, 0.0, 1.0));\n"
        "}\n";
}

void GFXEngine::SetVhsFilterEnabled(bool enabled)
{
    _originalVideoFilter = System::IniConf::GfxVideoFilterMode.Get<int32_t>() == 1;
    _vhsFilterStrength = ParseVhsFilterStrength(System::IniConf::GfxVhsFilterStrength.Get<std::string>(), 0.60f);

    if (!enabled)
    {
        _vhsFilterActive = false;
        return;
    }

    if (_vhsFilterStrength <= 0.0f)
    {
        _vhsFilterActive = false;
        ypa_log_out("VHS filter: disabled because gfx.vhs_filter_strength is 0\n");
        return;
    }

    if (_colorEffects <= 0)
    {
        _vhsFilterActive = false;
        ypa_log_out("WARNING: VHS filter requested but gfx.color_effects=0 disables the post-process framebuffer; continuing without VHS.\n");
        return;
    }

    if (!LoadVhsFilterShader())
    {
        _vhsFilterActive = false;
        return;
    }

    _vhsFilterActive = true;
    ypa_log_out("VHS filter: ENABLED shader=[%s] strength=%.2f pass=final_frame_after_visual_filter\n",
                _vhsFilterShaderPath.c_str(),
                _vhsFilterStrength);
}

void GFXEngine::ApplyVhsFilterFromConfig()
{
    SetVhsFilterEnabled(true);
}

void GFXEngine::FreeVhsFilterShader()
{
    GPU::DestroyProgram(_vhsFilterProg);
    GPU::DestroyProgram(_vhsBlendProg);
    _vhsFilterProg = 0;
    _vhsBlendProg = 0;
}

bool GFXEngine::LoadVhsFilterShader()
{
    std::string shaderPath = StrTrim(System::IniConf::GfxVhsFilterShader.Get<std::string>());
    // Both modes share the final-frame effect and strength blend.
    if (_originalVideoFilter)
        shaderPath = "res/original_video.ps";

    if (_vhsFilterProg && !StriCmp(shaderPath, _vhsFilterShaderPath))
        return true;

    FreeVhsFilterShader();
    _vhsFilterShaderPath = shaderPath;

    _vhsFilterProg = LoadPostProgram(shaderPath, "VHS filter");
    if (!_vhsFilterProg)
    {
        ypa_log_out("WARNING: VHS filter unavailable; continuing without VHS.\n");
        return false;
    }

    std::string error;
    _vhsBlendProg = GPU::CreateProgram(_postVShaderText, _postPrelude + VhsBlendShaderText(), 2,
                                       "vhs_blend", &error);
    if (!_vhsBlendProg)
    {
        ypa_log_out("WARNING: VHS filter blend shader failed to compile; continuing without VHS.\n%s\n",
                    error.c_str());
        FreeVhsFilterShader();
        return false;
    }
    return true;
}

uint8_t *GFXEngine::MakeScreenCopy(int *ow, int *oh)
{
    Common::Point res = System::GetResolution();

    // power of 2
    res.x &= ~1;
    res.y &= ~1;

    // RGB of the last shown frame, bottom row first.
    std::vector<uint8_t> rgb;
    int w = res.x, h = res.y;
    if (!GPU::ReadLastFrameRGB(&rgb, &w, &h) || w <= 0 || h <= 0)
    {
        *ow = 0;
        *oh = 0;
        return nullptr;
    }

    uint8_t *buf = (uint8_t *)malloc(rgb.size());
    std::memcpy(buf, rgb.data(), rgb.size());
    *ow = w;
    *oh = h;
    return buf;
}

SDL_Surface *GFXEngine::Screen()
{
    if (_virtualUiPass && VirtualUISurface)
        return VirtualUISurface;

    return ScreenSurface;
}



// Draw line Bresenham's algorithm
void GFXEngine::DrawLine(SDL_Surface *surface, const Common::Line &line, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t alpha, bool preserveSurfaceAlpha)
{
    if ((line.Width() == 0 && line.Height() == 0) ||
         !Common::Rect(surface->w, surface->h).IsIn(line.P1()) ||
         !Common::Rect(surface->w, surface->h).IsIn(line.P2()) )
        return;

    int rilWidth = surface->pitch / SDL_BYTESPERPIXEL(surface->format);

    int xCount = line.Width();
    int yCount = line.Height();

    uint32_t color = SDL_MapSurfaceRGBA(surface, cr, cg, cb, 255);

    // The virtual UI surface is transparent and composited over the 3D world
    // later. Pre-blending a partially transparent line against its cleared
    // black RGB turns the fade into an opaque black silhouette. Preserve the
    // destination alpha here so the final GPU composition performs the
    // actual fade against the world framebuffer.
    auto blendPixel = [&](uint32_t dstPixel) -> uint32_t
    {
        if (!preserveSurfaceAlpha)
        {
            if (alpha == 255)
                return color;

            uint8_t dr, dg, db;
            SDL_GetRGB(dstPixel, SDL_GetPixelFormatDetails(surface->format), SDL_GetSurfacePalette(surface), &dr, &dg, &db);
            return SDL_MapSurfaceRGB(surface,
                (uint8_t)((cr * alpha + dr * (255 - alpha)) / 255),
                (uint8_t)((cg * alpha + dg * (255 - alpha)) / 255),
                (uint8_t)((cb * alpha + db * (255 - alpha)) / 255));
        }

        if (alpha == 0)
            return dstPixel;
        if (alpha == 255)
            return color;

        uint8_t dr, dg, db, da;
        SDL_GetRGBA(dstPixel, SDL_GetPixelFormatDetails(surface->format), SDL_GetSurfacePalette(surface), &dr, &dg, &db, &da);

        const uint32_t invAlpha = 255u - alpha;
        const uint32_t outAlpha = alpha + (da * invAlpha + 127u) / 255u;
        if (outAlpha == 0)
            return SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0);

        const uint32_t denominator = outAlpha * 255u;
        const uint32_t outR = (cr * alpha * 255u + dr * da * invAlpha + denominator / 2u) / denominator;
        const uint32_t outG = (cg * alpha * 255u + dg * da * invAlpha + denominator / 2u) / denominator;
        const uint32_t outB = (cb * alpha * 255u + db * da * invAlpha + denominator / 2u) / denominator;

        return SDL_MapSurfaceRGBA(surface,
                           (uint8_t)outR, (uint8_t)outG, (uint8_t)outB,
                           (uint8_t)outAlpha);
    };

    int stepAdd, stepOdd;
    int steps, subSteps;

    if ( xCount <= yCount )
    {
        if ( line.y2 <= line.y1 )
            stepAdd = -rilWidth;
        else
            stepAdd = rilWidth;

        if ( line.x2 <= line.x1 )
            stepOdd = -1;
        else
            stepOdd = 1;

        steps = yCount;
        subSteps = xCount;
    }
    else
    {
        if ( line.x2 <= line.x1 )
            stepAdd = -1;
        else
            stepAdd = 1;

        if ( line.y2 <= line.y1 )
            stepOdd = -rilWidth;
        else
            stepOdd = rilWidth;

        steps = xCount;
        subSteps = yCount;
    }

    int incr1 = 2 * subSteps;
    int t = 2 * subSteps - steps;
    int incr2 = 2 * (subSteps - steps);

    SDL_LockSurface(surface);

    void *surfPos = (void *) ((uint8_t *) surface->pixels
                    + line.y1 * surface->pitch
                    + line.x1 * SDL_BYTESPERPIXEL(surface->format) );

    switch(SDL_BYTESPERPIXEL(surface->format))
    {
        case 1:
        {
            uint8_t *surf = (uint8_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                if (alpha >= 128)
                    *surf = color;
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;

        case 2:
        {
            uint16_t *surf = (uint16_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                *surf = (uint16_t)blendPixel(*surf);
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;

        case 4:
        {
            uint32_t *surf = (uint32_t *)surfPos;

            for (int i = 0; i <= steps; i++) // Verify i bound
            {
                *surf = blendPixel(*surf);
                if ( t > 0 )
                {
                    t += incr2;
                    surf += stepOdd;
                }
                else
                    t += incr1;

                surf += stepAdd;
            }
        }
        break;

        default:
        break;
    }

    SDL_UnlockSurface(surface);
}

void GFXEngine::BlitScaleMasked(SDL_Surface *src, Common::Rect sRect, SDL_Surface *mask, uint8_t index, SDL_Surface *dst, Common::Rect dRect)
{
    if (SDL_BITSPERPIXEL(mask->format) != 8)
        return;

    if (src->w != mask->w || src->h != mask->h)
        return;

    if (sRect.IsEmpty() || !sRect.IsValid())
        sRect = Common::Rect(src->w, src->h);
    else if (!Common::Rect(src->w, src->h).IsIn(sRect))
        return;

    if (dRect.IsEmpty() || !dRect.IsValid())
        dRect = Common::Rect(dst->w, dst->h);
    else if (!Common::Rect(dst->w, dst->h).IsIn(dRect))
        return;

    // Try fast
    if (src->format == dst->format)
    {
        switch(SDL_BYTESPERPIXEL(src->format))
        {
            case 2:
            {
                SDL_LockSurface(src);
                SDL_LockSurface(mask);
                SDL_LockSurface(dst);

                int32_t dY = (sRect.Height() << 16) / dRect.Height();
                int32_t dX = (sRect.Width()  << 16) / dRect.Width();

                int32_t srcY  = sRect.top << 16;
                for (int y = dRect.top; y < dRect.bottom; y++)
                {
                    uint16_t *dBuf = (uint16_t *)((uint8_t *)dst->pixels + y * dst->pitch) + dRect.left;
                    uint16_t *sBuf = (uint16_t *)((uint8_t *)src->pixels + (srcY >> 16) * src->pitch) + sRect.left;
                    uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;

                    int32_t xx = 0;
                    for (int x = dRect.left; x < dRect.right; x++)
                    {
                        if (mBuf[xx >> 16] == index)
                            *dBuf = sBuf[xx >> 16];
                        dBuf++;
                        xx += dX;
                    }
                    srcY += dY;
                }

                SDL_UnlockSurface(dst);
                SDL_UnlockSurface(mask);
                SDL_UnlockSurface(src);
            }
            break;

            case 4:
            {
                SDL_LockSurface(src);
                SDL_LockSurface(mask);
                SDL_LockSurface(dst);

                int32_t dY = (sRect.Height() << 16) / dRect.Height();
                int32_t dX = (sRect.Width()  << 16) / dRect.Width();

                int32_t srcY  = sRect.top << 16;
                for (int y = dRect.top; y < dRect.bottom; y++)
                {
                    uint32_t *dBuf = (uint32_t *)((uint8_t *)dst->pixels + y * dst->pitch) + dRect.left;
                    uint32_t *sBuf = (uint32_t *)((uint8_t *)src->pixels + (srcY >> 16) * src->pitch) + sRect.left;
                    uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;

                    int32_t xx = 0;
                    for (int x = dRect.left; x < dRect.right; x++)
                    {
                        if (mBuf[xx >> 16] == index)
                            *dBuf = sBuf[xx >> 16];
                        dBuf++;
                        xx += dX;
                    }
                    srcY += dY;
                }

                SDL_UnlockSurface(dst);
                SDL_UnlockSurface(mask);
                SDL_UnlockSurface(src);
            }
            break;

            default:
            break;
        }
    }
    else // Slow
    {
        SDL_LockSurface(src);
        SDL_LockSurface(mask);
        SDL_LockSurface(dst);

        uint8_t sbpp = SDL_BYTESPERPIXEL(src->format);
        uint8_t dbpp = SDL_BYTESPERPIXEL(dst->format);

        int32_t dY = (sRect.Height() << 16) / dRect.Height();
        int32_t dX = (sRect.Width()  << 16) / dRect.Width();

        int32_t srcY  = sRect.top << 16;
        for (int y = dRect.top; y < dRect.bottom; y++)
        {
            uint8_t *dBuf = (uint8_t *)dst->pixels + y * dst->pitch + dRect.left * dbpp;
            uint8_t *sBuf = (uint8_t *)src->pixels + (srcY >> 16) * src->pitch + sRect.left * sbpp;
            uint8_t  *mBuf = (uint8_t *)mask->pixels + (srcY >> 16) * mask->pitch + sRect.left;

            int32_t xx = 0;
            for (int x = dRect.left; x < dRect.right; x++)
            {
                if (mBuf[xx >> 16] == index)
                {
                    uint8_t r,g,b;
                    uint32_t clr = 0;

                    uint8_t *spix = sBuf + (xx >> 16) * sbpp;
                    for(int i = 0; i < sbpp; i++)
                        clr |= spix[i] << (i * 8);

                    SDL_GetRGB(clr, SDL_GetPixelFormatDetails(src->format), SDL_GetSurfacePalette(src), &r, &g, &b);
                    clr = SDL_MapSurfaceRGB(dst, r, g, b);

                    for(int i = 0; i < dbpp; i++)
                        dBuf[i] = (clr >> (i * 8)) & 0xFF;
                }
                dBuf += dbpp;
                xx += dX;
            }
            srcY += dY;
        }

        SDL_UnlockSurface(dst);
        SDL_UnlockSurface(mask);
        SDL_UnlockSurface(src);
    }
}

void GFXEngine::DrawFillIntCpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();

    SDL_LockSurface(src);
    SDL_LockSurface(dst);

    if (dw > sw)
    {
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int32_t nit = dw / sw;
        const int32_t nit2 = (dw % sw) * SDL_BYTESPERPIXEL(dst->format);
        const int32_t cpsz = sw * SDL_BYTESPERPIXEL(dst->format);
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format) + nit2;

        int sptch = src->pitch;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            for(int n = nit; n > 0; --n)
            {
                memcpy(pdst, psrcl, cpsz);
                pdst += cpsz;
            }

            memcpy(pdst, psrcl, nit2);
            pdst += skipdst;

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
        }
    }
    else
    {
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int32_t cpsz = dw * SDL_BYTESPERPIXEL(dst->format);
        const int32_t skipdst = dst->pitch;

        int sptch = src->pitch;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            memcpy(pdst, psrcl, cpsz);
            pdst += skipdst;

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;
        }
    }

    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

template <typename T>
void GFXEngine::DrawFillIntCKey(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();

    SDL_LockSurface(src);
    SDL_LockSurface(dst);

    uint32_t clrKey = 0;
    SDL_GetSurfaceColorKey(src, &clrKey);

    if (dw > sw)
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int32_t nit = dw / sw;
        const int32_t nit2 = dw % sw;

        const int sz = SDL_BYTESPERPIXEL(dst->format);

        int sptch = src->pitch;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {

            for(int n = nit; n > 0; --n)
            {
                const uint8_t *lsrc = psrcl;
                for (int iw = sw; iw > 0; --iw)
                {
                    if (*(T *)lsrc != clrKey)
                        *(T *)pdst = *(T *)lsrc;

                    lsrc += sz;
                    pdst += sz;
                }
            }

            uint8_t *rsrc = psrcl;
            for(int iw = nit2; iw > 0; --iw)
            {
                if (*(T *)rsrc != clrKey)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;

            pdst += skipdst;
        }
    }
    else
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int sz = SDL_BYTESPERPIXEL(dst->format);

        int sptch = src->pitch;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            uint8_t *rsrc = psrcl;
            for(int iw = dw; iw > 0; --iw)
            {
                if (*(T *)rsrc != clrKey)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;

            pdst += skipdst;
        }
    }

    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

template <typename T>
void GFXEngine::DrawFillIntACpy(SDL_Surface* src, const Common::Rect& sRect, SDL_Surface* dst, const Common::Rect& dRect)
{
    const int32_t sh = sRect.Height();
    const int32_t sw = sRect.Width();
    const int32_t dh = dRect.Height();
    const int32_t dw = dRect.Width();

    SDL_LockSurface(src);
    SDL_LockSurface(dst);

    if (dw > sw)
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int32_t nit = dw / sw;
        const int32_t nit2 = dw % sw;
        uint32_t amask = SDL_GetPixelFormatDetails(src->format)->Amask;

        const int sz = SDL_BYTESPERPIXEL(dst->format);

        int sptch = src->pitch;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {

            for(int n = nit; n > 0; --n)
            {
                const uint8_t *lsrc = psrcl;
                for (int iw = sw; iw > 0; --iw)
                {
                    if (*(T *)lsrc & amask)
                        *(T *)pdst = *(T *)lsrc;

                    lsrc += sz;
                    pdst += sz;
                }
            }

            uint8_t *rsrc = psrcl;
            for(int iw = nit2; iw > 0; --iw)
            {
                if (*(T *)rsrc & amask)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;

            pdst += skipdst;
        }
    }
    else
    {
        const int32_t skipdst = dst->pitch - dw * SDL_BYTESPERPIXEL(dst->format);
        uint8_t *psrc = (uint8_t *)src->pixels + sRect.top * src->pitch + sRect.left * SDL_BYTESPERPIXEL(src->format);
        uint8_t *pdst = (uint8_t *)dst->pixels + dRect.top * dst->pitch + dRect.left * SDL_BYTESPERPIXEL(dst->format);

        const int sz = SDL_BYTESPERPIXEL(dst->format);

        int sptch = src->pitch;
        uint32_t amask = SDL_GetPixelFormatDetails(src->format)->Amask;

        int ln = 0;
        uint8_t *psrcl = psrc;
        for(int ih = dh; ih > 0; --ih)
        {
            uint8_t *rsrc = psrcl;
            for(int iw = dw; iw > 0; --iw)
            {
                if (*(T *)rsrc & amask)
                    *(T *)pdst = *(T *)rsrc;

                rsrc += sz;
                pdst += sz;
            }

            ++ln;
            if (ln >= sh)
            {
                ln = 0;
                psrcl = psrc;
            }
            else
                psrcl += sptch;

            pdst += skipdst;
        }
    }

    SDL_UnlockSurface(dst);
    SDL_UnlockSurface(src);
}

void GFXEngine::DrawFill(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect)
{
    if (sRect.IsEmpty() || dRect.IsEmpty())
        return;

    if (src->format == dst->format)
    {
#if SDL_VERSION_ATLEAST(2, 0, 9)
        if (SDL_SurfaceHasColorKey(src))
#else
        uint32_t tmp;
        if (SDL_GetSurfaceColorKey(src, &tmp))
#endif
        {
            switch(SDL_BYTESPERPIXEL(src->format))
            {

                case 2:
                    DrawFillIntCKey<uint16_t>(src, sRect, dst, dRect);
                    break;

                case 4:
                    DrawFillIntCKey<uint32_t>(src, sRect, dst, dRect);
                    break;

                default:
                break;
            }
        }
        else
        {
            if (SDL_GetPixelFormatDetails(src->format)->Amask)
            {
                switch(SDL_BYTESPERPIXEL(src->format))
                {

                    case 2:
                        DrawFillIntACpy<uint16_t>(src, sRect, dst, dRect);
                        break;

                    case 4:
                        DrawFillIntACpy<uint32_t>(src, sRect, dst, dRect);
                        break;

                    default:
                    break;
                }
            }
            else
                DrawFillIntCpy(src, sRect, dst, dRect);
        }
    }
    else
    {
        SDL_Rect lsrc = sRect;
        SDL_Rect ldst;
        for(ldst.y = dRect.top; ldst.y < dRect.bottom; ldst.y += lsrc.h)
        {
            if (dRect.bottom - ldst.y < lsrc.h)
                lsrc.h = dRect.bottom - ldst.y;

            lsrc.w = sRect.Width();

            for(ldst.x = dRect.left; ldst.x < dRect.right; ldst.x += lsrc.w)
            {
                if (dRect.right - ldst.x < lsrc.w)
                    lsrc.w = dRect.right - ldst.x;

                SDL_BlitSurface(src, &lsrc, dst, &ldst);
            }
        }
    }
}

void GFXEngine::DrawFillAlpha(SDL_Surface *src, const Common::Rect &sRect,
                              SDL_Surface *dst, const Common::Rect &dRect,
                              uint8_t opacity)
{
    if (opacity == 0 || sRect.IsEmpty() || dRect.IsEmpty())
        return;
    if (opacity == 255)
    {
        DrawFill(src, sRect, dst, dRect);
        return;
    }

    uint8_t oldOpacity = 255;
    SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetSurfaceAlphaMod(src, &oldOpacity);
    SDL_GetSurfaceBlendMode(src, &oldBlendMode);
    SDL_SetSurfaceAlphaMod(src, opacity);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_BLEND);

    SDL_Rect source = sRect;
    SDL_Rect target;
    for (target.y = dRect.top; target.y < dRect.bottom; target.y += sRect.Height())
    {
        source.h = std::min(sRect.Height(), dRect.bottom - target.y);
        source.w = sRect.Width();
        for (target.x = dRect.left; target.x < dRect.right; target.x += sRect.Width())
        {
            source.w = std::min(sRect.Width(), dRect.right - target.x);
            SDL_BlitSurface(src, &source, dst, &target);
        }
    }

    SDL_SetSurfaceAlphaMod(src, oldOpacity);
    SDL_SetSurfaceBlendMode(src, oldBlendMode);
}

void GFXEngine::Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::Point dPoint)
{
    SDL_Rect Ssrc = sRect;
    SDL_Rect Sdst = dPoint;
    SDL_BlitSurface(src, &Ssrc, dst, &Sdst);
}

void GFXEngine::Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::PointRect dRect)
{
    SDL_Rect Ssrc = sRect;
    if (Ssrc.w > dRect.w)
        Ssrc.w = dRect.w;
    if (Ssrc.h > dRect.h)
        Ssrc.h = dRect.h;
    SDL_Rect Sdst = dRect;
    SDL_BlitSurface(src, &Ssrc, dst, &Sdst);
}

Common::Point GFXEngine::GetVirtualUIResolution() const
{
    if (_resolution.x <= 0 || _resolution.y <= 0)
        return Common::Point(640, 480);

    // Urban Assault's in-game interface was authored around a 480-line
    // canvas. Keep that visual size at modern resolutions while extending the
    // logical width to the current aspect ratio, so widescreen layouts reach
    // both edges without stretching the original 4:3 geometry.
    if (_resolution.y <= 480)
        return _resolution;

    const int logicalHeight = 480;
    const int logicalWidth = std::max(640,
                                      (_resolution.x * logicalHeight + _resolution.y / 2) /
                                      _resolution.y);
    return Common::Point(logicalWidth, logicalHeight);
}

void GFXEngine::BeginVirtualUI(const Common::Point &logicalSize)
{
    if (_virtualUiPass || logicalSize.x <= 0 || logicalSize.y <= 0 || !ScreenSurface)
        return;

    if (!VirtualUISurface ||
        VirtualUISurface->w != logicalSize.x ||
        VirtualUISurface->h != logicalSize.y ||
        VirtualUISurface->format != ScreenSurface->format)
    {
        if (VirtualUISurface)
            SDL_DestroySurface(VirtualUISurface);

        VirtualUISurface = SDL_CreateSurface(logicalSize.x, logicalSize.y, SDL_GetPixelFormatForMasks(SDL_BITSPERPIXEL(ScreenSurface->format), SDL_GetPixelFormatDetails(ScreenSurface->format)->Rmask, SDL_GetPixelFormatDetails(ScreenSurface->format)->Gmask, SDL_GetPixelFormatDetails(ScreenSurface->format)->Bmask, SDL_GetPixelFormatDetails(ScreenSurface->format)->Amask));
        if (!VirtualUISurface)
            return;

        SDL_SetSurfaceBlendMode(VirtualUISurface, SDL_BLENDMODE_BLEND);
    }

    SDL_FillSurfaceRect(VirtualUISurface, nullptr,
                 SDL_MapSurfaceRGBA(VirtualUISurface, 0, 0, 0, 0));

    _virtualUiSavedClip = _clip;
    _virtualUiSavedInverseClip = _inverseClip;
    _virtualUiSavedField54c = _field_54c;
    _virtualUiSavedField550 = _field_550;
    _virtualUiSavedField554 = _field_554;
    _virtualUiSavedField558 = _field_558;

    _virtualUiResolution = logicalSize;
    _virtualUiPending = false;
    _virtualUiPass = true;

    _clip = logicalSize - Common::Point(1, 1);
    _inverseClip = Common::Rect();
    _field_54c = logicalSize.x / 2;
    _field_550 = logicalSize.y / 2;
    _field_554 = logicalSize.x / 2;
    _field_558 = logicalSize.y / 2;
}

void GFXEngine::EndVirtualUI()
{
    if (!_virtualUiPass)
        return;

    _virtualUiPass = false;

    _clip = _virtualUiSavedClip;
    _inverseClip = _virtualUiSavedInverseClip;
    _field_54c = _virtualUiSavedField54c;
    _field_550 = _virtualUiSavedField550;
    _field_554 = _virtualUiSavedField554;
    _field_558 = _virtualUiSavedField558;

    _virtualUiPending = VirtualUISurface != nullptr;
}

void GFXEngine::RecreateScreenSurface()
{
    if ( ScreenSurface )
        SDL_DestroySurface(ScreenSurface);

    ScreenSurface = SDL_CreateSurface(_resolution.x, _resolution.y, _pixfmt->format);

    GPU::DestroyTexture(screenTex);
    screenTex = GPU::CreateTexture(ScreenSurface->w, ScreenSurface->h, _texFormat, GPU::USAGE_SAMPLED);
}

// Two triangles with the current states and the standard shader uniforms.
void GFXEngine::DrawVtxQuad(const std::array<GFX::TVertex, 4> &vtx)
{
    // OpenNeoUA custom VP tint: screen/HUD/UI quads must never inherit a mesh tint.
    _stdParams.ColorMul[0] = 1.0;
    _stdParams.ColorMul[1] = 1.0;
    _stdParams.ColorMul[2] = 1.0;
    _stdParams.ColorMul[3] = 1.0;
    _stdParams.Colorize = 0;

    const uint32_t base = GPU::PushTransient(vtx.data(), sizeof(TVertex) * vtx.size());
    GPU::Streams streams;
    streams.pos = base + offsetof(TVertex, Pos);
    streams.color = base + offsetof(TVertex, Color);
    streams.uv = base + offsetof(TVertex, TexCoord);

    const GPU::Sampler sampler = {_states.Tex, _states.LinearFilter};
    GPU::Draw(StdDrawState(GPU::Layout::Mesh), streams, GPU::QuadIndexBuffer(), true, 6, 0, &sampler, 1,
              &_stdParams, sizeof(_stdParams), &_stdParams, sizeof(_stdParams));
}

// Full-screen pass of a post program (see _postPrelude).
void GFXEngine::DrawPostQuad(GPU::ProgramId program, GPU::TextureId image, GPU::TextureId aux,
                             bool blend, GPU::Blend src, GPU::Blend dst, const TPostParams &params)
{
    if (!program)
        return;

    static const std::array<TVertex, 4> quad = {
        GFX::TVertex( vec3f(-1.0,  1.0, 0.0) ),
        GFX::TVertex( vec3f(-1.0, -1.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0, -1.0, 0.0) ),
        GFX::TVertex( vec3f( 1.0,  1.0, 0.0) )
    };
    const uint32_t base = GPU::PushTransient(quad.data(), sizeof(TVertex) * quad.size());
    GPU::Streams streams;
    streams.pos = base + offsetof(TVertex, Pos);
    streams.color = base + offsetof(TVertex, Color);
    streams.uv = base + offsetof(TVertex, TexCoord);

    GPU::DrawState state;
    state.program = program;
    state.blend = blend;
    state.src = src;
    state.dst = dst;
    state.depthTest = false;
    state.depthWrite = false;

    const GPU::Sampler samplers[2] = {{image, true}, {aux, true}};
    GPU::Draw(state, streams, GPU::QuadIndexBuffer(), true, 6, 0, samplers, 2,
              nullptr, 0, &params, sizeof(params));
}

// Uniforms every post pass shares: palette flash, screen size, random and clock.
TPostParams GFXEngine::PostParamsBase(const Common::Point &scrSz) const
{
    TPostParams params = {};
    params.normclr[0] = _normClr.x;
    params.normclr[1] = _normClr.y;
    params.normclr[2] = _normClr.z;
    params.invclr[0] = _invClr.x;
    params.invclr[1] = _invClr.y;
    params.invclr[2] = _invClr.z;
    params.screenSize[0] = scrSz.x;
    params.screenSize[1] = scrSz.y;
    params.randval = GameplayAwareShaderRandom();
    params.millisecs = GameplayAwareShaderTime();
    params.vhsStrength = _vhsFilterStrength;
    return params;
}

void GFXEngine::DrawScreenSurface()
{
    GfxStates save = _states;

    Common::Point scrSz = System::GetResolution();
    GPU::SetViewport(0, 0, scrSz.x, scrSz.y);

    SetProjectionMatrix( mat4x4f() );
    SetModelViewMatrix( mat4x4f() );

    _states.DepthTest = false;
    _states.Zwrite = false;
    _states.AlphaBlend = true;
    _states.SrcBlend = GPU::Blend::SrcAlpha;
    _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
    _states.Tex = screenTex;
    _states.Prog = _stdShaderProg;
    _states.AlphaTest = false;
    _states.Shaded = true;
    _states.LinearFilter = true;

    SetRenderStates(0);

    SDL_LockSurface(ScreenSurface);
    GPU::UploadTexture(screenTex, ScreenSurface->pixels, ScreenSurface->pitch);
    SDL_UnlockSurface(ScreenSurface);

    static std::array<TVertex, 4> vtx = {
        GFX::TVertex( vec3f(-1.0,  1.0, 0.0), tUtV(0.0, 0.0) ),
        GFX::TVertex( vec3f(-1.0, -1.0, 0.0), tUtV(0.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0, -1.0, 0.0), tUtV(1.0, 1.0) ),
        GFX::TVertex( vec3f( 1.0,  1.0, 0.0), tUtV(1.0, 0.0) )
    };

    DrawVtxQuad(vtx);

    _states = save;
}

void GFXEngine::DrawVirtualUISolidRect(float left, float top, float right, float bottom,
                                        const TColorF &color)
{
    if ( !_virtualUiPass || !VirtualUISurface ||
         color.a <= 0.0f || right <= left || bottom <= top )
    {
        return;
    }

    const int x0 = std::max(0, std::min((int)std::floor(left), VirtualUISurface->w));
    const int x1 = std::max(0, std::min((int)std::ceil(right), VirtualUISurface->w));
    const int y0 = std::max(0, std::min((int)std::floor(top), VirtualUISurface->h));
    const int y1 = std::max(0, std::min((int)std::ceil(bottom), VirtualUISurface->h));
    if ( x1 <= x0 || y1 <= y0 )
        return;

    const uint8_t r = (uint8_t)std::max(0, std::min(255, (int)std::lround(color.r * 255.0f)));
    const uint8_t g = (uint8_t)std::max(0, std::min(255, (int)std::lround(color.g * 255.0f)));
    const uint8_t b = (uint8_t)std::max(0, std::min(255, (int)std::lround(color.b * 255.0f)));
    const uint8_t a = (uint8_t)std::max(0, std::min(255, (int)std::lround(color.a * 255.0f)));
    if ( a == 0 )
        return;

    // DrawLine ignores zero-length lines, so a 1 px wide rect would vanish row by row.
    // A single pixel is filled directly; a 1 px wide column is one vertical (blended) line.
    if ( x1 - x0 == 1 )
    {
        if ( y1 - y0 == 1 )
        {
            SDL_Rect pixel = { x0, y0, 1, 1 };
            SDL_FillSurfaceRect(VirtualUISurface, &pixel, SDL_MapSurfaceRGBA(VirtualUISurface, r, g, b, a));
        }
        else
        {
            DrawLine(VirtualUISurface, Common::Line(x0, y0, x0, y1 - 1), r, g, b, a, true);
        }
        return;
    }

    for ( int y = y0; y < y1; ++y )
    {
        DrawLine(VirtualUISurface, Common::Line(x0, y, x1 - 1, y),
                 r, g, b, a, true);
    }
}

void GFXEngine::DrawVirtualUISurface()
{
    if (!_virtualUiPending || !VirtualUISurface)
        return;

    GfxStates save = _states;

    Common::Point scrSz = System::GetResolution();
    GPU::SetViewport(0, 0, scrSz.x, scrSz.y);

    SetProjectionMatrix(mat4x4f());
    SetModelViewMatrix(mat4x4f());

    _states.DepthTest = false;
    _states.Zwrite = false;
    _states.AlphaBlend = true;
    _states.SrcBlend = GPU::Blend::SrcAlpha;
    _states.DstBlend = GPU::Blend::OneMinusSrcAlpha;
    _states.Prog = _stdShaderProg;
    _states.AlphaTest = false;
    _states.Shaded = true;
    _states.LinearFilter = (_virtualUiStyle == VirtualUIStyle::SMOOTH);

    const Common::Point surfaceSize(VirtualUISurface->w, VirtualUISurface->h);
    if (!virtualUiTex || _virtualUiTextureSize != surfaceSize)
    {
        GPU::DestroyTexture(virtualUiTex);
        virtualUiTex = GPU::CreateTexture(surfaceSize.x, surfaceSize.y, _texFormat, GPU::USAGE_SAMPLED);
        _virtualUiTextureSize = surfaceSize;
    }
    _states.Tex = virtualUiTex;
    SetRenderStates(0);

    SDL_LockSurface(VirtualUISurface);
    GPU::UploadTexture(virtualUiTex, VirtualUISurface->pixels, VirtualUISurface->pitch);
    SDL_UnlockSurface(VirtualUISurface);

    static std::array<TVertex, 4> vtx = {
        GFX::TVertex(vec3f(-1.0,  1.0, 0.0), tUtV(0.0, 0.0)),
        GFX::TVertex(vec3f(-1.0, -1.0, 0.0), tUtV(0.0, 1.0)),
        GFX::TVertex(vec3f( 1.0, -1.0, 0.0), tUtV(1.0, 1.0)),
        GFX::TVertex(vec3f( 1.0,  1.0, 0.0), tUtV(1.0, 0.0))
    };

    DrawVtxQuad(vtx);

    _states = save;
    _virtualUiPending = false;
}

std::string GFXEngine::ReadShaderFile(const std::string &fl)
{
    FSMgr::FileHandle *f = FSMgr::iDir::openFileAlloc(uaDataFirstResolvedReadPath(fl), "rb");
    if (!f)
        return std::string();

    f->seek(0, SEEK_END);
    const size_t sz = f->tell();
    f->seek(0, SEEK_SET);

    std::string text(sz, '\0');
    f->read(&text[0], sz);
    delete f;
    return text;
}

GPU::ProgramId GFXEngine::LoadPostProgram(const std::string &fragmentPath, const std::string &what)
{
    std::string text = ReadShaderFile(fragmentPath);
    if (text.empty())
    {
        ypa_log_out("WARNING: %s shader [%s] is missing.\n", what.c_str(), fragmentPath.c_str());
        return 0;
    }

    // The prelude brings its own #version; a #version line in the file would break it.
    if (text.compare(0, 8, "#version") == 0)
    {
        const size_t eol = text.find('\n');
        text.erase(0, eol == std::string::npos ? text.size() : eol + 1);
    }

    std::string error;
    const GPU::ProgramId program = GPU::CreateProgram(_postVShaderText, _postPrelude + text, 2, fragmentPath, &error);
    if (!program)
        ypa_log_out("WARNING: %s shader [%s] failed to compile:\n%s\n", what.c_str(), fragmentPath.c_str(),
                    error.c_str());
    return program;
}

void GFXEngine::DrawFBO()
{
    GPU::ProgramId postProg = _colorEffectsShaderProg;
    if (_atmosphereActive && _atmosphereShaderProg)
        postProg = _atmosphereShaderProg;
    if (!postProg || !_sceneTex)
        return;

    Common::Point scrSz = System::GetResolution();
    GPU::SetViewport(0, 0, scrSz.x, scrSz.y);

    TPostParams params = PostParamsBase(scrSz);
    if (postProg == _atmosphereShaderProg)
    {
        params.atmosphereStrength = _atmosphereStrength;
        params.atmosphereExposure = _atmosphereExposure;
        params.atmosphereContrast = _atmosphereContrast;
        params.atmosphereSaturation = _atmosphereSaturation;
        params.atmosphereVignette = _atmosphereVignette;
    }
    // OpenNeoUA custom: fullscreen visual filter LUT. Strength 0 => passthrough.
    params.filterStrength = (_visualFilterActive && _visualFilterLut) ? _visualFilterStrength : 0.0f;

    GPU::Blend src = _states.SrcBlend;
    GPU::Blend dst = _states.DstBlend;
    if (_fboBlend == 0)
    {
        src = GPU::Blend::One;
        dst = GPU::Blend::OneMinusSrcAlpha;
    }
    else if (_fboBlend == 1)
    {
        src = GPU::Blend::SrcAlpha;
        dst = GPU::Blend::OneMinusSrcAlpha;
    }

    DrawPostQuad(postProg, _sceneTex, _visualFilterLut, true, src, dst, params);
}

bool GFXEngine::EnsureVhsFilterTexture(const Common::Point &scrSz)
{
    Common::Point outputSize = scrSz;
    if (_originalVideoFilter && scrSz.y > 240)
    {
        // Original tutorial footage has 240 lines. Keep the display aspect
        // ratio and never upscale an already smaller source during reduction.
        outputSize.y = 240;
        outputSize.x = std::max(1, (int)std::lround((double)scrSz.x * 240.0 / scrSz.y));
    }

    // The copy has the backbuffer format: a texture copy needs equal formats.
    if (!_vhsCopyTex || _vhsCopyTexSize != scrSz)
    {
        GPU::DestroyTexture(_vhsCopyTex);
        _vhsCopyTex = GPU::CreateTexture(scrSz.x, scrSz.y, GPU::Format::BGRA8, GPU::USAGE_SAMPLED);
        _vhsCopyTexSize = scrSz;
    }
    if (!_vhsOutTex || _vhsOutTexSize != outputSize)
    {
        GPU::DestroyTexture(_vhsOutTex);
        _vhsOutTex = GPU::CreateTexture(outputSize.x, outputSize.y, GPU::Format::BGRA8,
                                        GPU::USAGE_SAMPLED | GPU::USAGE_TARGET);
        _vhsOutTexSize = outputSize;
    }

    if (!_vhsCopyTex || !_vhsOutTex)
    {
        ypa_log_out("WARNING: VHS filter textures could not be created; disabling VHS and keeping the visual filter path.\n");
        _vhsFilterActive = false;
        return false;
    }
    return true;
}

void GFXEngine::DrawVhsEffect()
{
    if (!_vhsFilterActive || !_vhsFilterProg || _vhsFilterStrength <= 0.0f)
        return;

    // The effect target is set by EndFrame; screenSize stays the window size.
    const TPostParams params = PostParamsBase(System::GetResolution());
    DrawPostQuad(_vhsFilterProg, _vhsCopyTex, 0, false, GPU::Blend::One, GPU::Blend::Zero, params);
}

void GFXEngine::DrawVhsFilter()
{
    if (!_vhsFilterActive || !_vhsBlendProg || _vhsFilterStrength <= 0.0f)
        return;

    const TPostParams params = PostParamsBase(System::GetResolution());
    DrawPostQuad(_vhsBlendProg, _vhsCopyTex, _vhsOutTex, false, GPU::Blend::One, GPU::Blend::Zero, params);
}

void GFXEngine::SetFBOBlending(int mode)
{
    _fboBlend = mode;
}

void GFXEngine::UpdateFBOSizes()
{
    if (_colorEffects)
        CreateSceneTargets(System::GetResolution());
}

// Colour and depth targets of the 3D scene, at the window size.
void GFXEngine::CreateSceneTargets(const Common::Point &size)
{
    if (_sceneTex && _sceneSize == size)
        return;

    GPU::DestroyTexture(_sceneTex);
    GPU::DestroyTexture(_sceneDepth);
    _sceneTex = GPU::CreateTexture(size.x, size.y, GPU::Format::RGBA16F, GPU::USAGE_SAMPLED | GPU::USAGE_TARGET);
    _sceneDepth = GPU::CreateTexture(size.x, size.y, GPU::Format::Depth, GPU::USAGE_SAMPLED | GPU::USAGE_TARGET);
    _sceneSize = size;
}

SDL_Color GFXEngine::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    SDL_Color tmp;
    tmp.a = a;
    tmp.r = r;
    tmp.g = g;
    tmp.b = b;
    return tmp;
}

float GFXEngine::GetColorEffectPower(int id)
{
    int32_t pwr = 0;
    switch(id)
    {
        case 0:
            return 1.0f;

        case 1:
            pwr = System::IniConf::GfxColorEffPower1.Get<int32_t>();
            break;

        case 2:
            pwr = System::IniConf::GfxColorEffPower2.Get<int32_t>();
            break;

        case 3:
            pwr = System::IniConf::GfxColorEffPower3.Get<int32_t>();
            break;

        case 4:
            pwr = System::IniConf::GfxColorEffPower4.Get<int32_t>();
            break;

        case 5:
            pwr = System::IniConf::GfxColorEffPower5.Get<int32_t>();
            break;

        case 6:
            pwr = System::IniConf::GfxColorEffPower6.Get<int32_t>();
            break;

        case 7:
            pwr = System::IniConf::GfxColorEffPower7.Get<int32_t>();
            break;

        default:
            if (id >= 8 && id < (int)_customClrEffDefined.size())
                return _customClrEffDefined.at(id) ? 1.0f : 0.0f;
            return 0.0f;
    }

    if (pwr < 0)
        pwr = 0;

    if (pwr > 100)
        pwr = 100;

    return (float)pwr / 100.0f;
}

Common::Point GFXEngine::ConvertPosTo2DStuff(const Common::Point &pos)
{
    Common::Point real = System::GetResolution();

    if (real == _resolution)
        return pos;

    Common::Point t( pos.x * _resolution.x / real.x,
                     pos.y * _resolution.y / real.y );

    if (t.x < 0)
        t.x = 0;
    if (t.x >= _resolution.x)
        t.x = _resolution.x - 1;

    if (t.y < 0)
        t.y = 0;
    if (t.y >= _resolution.y)
        t.y = _resolution.y - 1;

    return t;
}

TMesh::TMesh(const TMesh &b)
{
    Mat = b.Mat;
    Vertexes = b.Vertexes;
    Indixes = b.Indixes;
    CoordsCache = b.CoordsCache;
    Rest = b.Rest;
    BoundBox = b.BoundBox;

    gpuVertexBuf = 0;
    gpuIndexBuf = 0;
}

TMesh::TMesh(TMesh &&b) noexcept
{
    Mat = std::move(b.Mat);
    Vertexes = std::move(b.Vertexes);
    Indixes = std::move(b.Indixes);
    CoordsCache = std::move(b.CoordsCache);
    Rest = std::move(b.Rest);
    RestBufferPos = b.RestBufferPos;
    BoundBox = std::move(b.BoundBox);

    gpuVertexBuf = b.gpuVertexBuf;
    gpuIndexBuf = b.gpuIndexBuf;
    b.gpuVertexBuf = 0;
    b.gpuIndexBuf = 0;
}

TMesh &TMesh::operator=(const TMesh& b)
{
    if (this == &b)
        return *this;

    GFX::Engine.MeshFreeVBO(this);

    Mat = b.Mat;
    Vertexes = b.Vertexes;
    Indixes = b.Indixes;
    CoordsCache = b.CoordsCache;
    Rest = b.Rest;
    BoundBox = b.BoundBox;

    gpuVertexBuf = 0;
    gpuIndexBuf = 0;

    return *this;
}

TMesh &TMesh::operator=(TMesh &&b) noexcept
{
    if (this == &b)
        return *this;

    GFX::Engine.MeshFreeVBO(this);

    Mat = std::move(b.Mat);
    Vertexes = std::move(b.Vertexes);
    Indixes = std::move(b.Indixes);
    CoordsCache = std::move(b.CoordsCache);
    Rest = std::move(b.Rest);
    RestBufferPos = b.RestBufferPos;
    BoundBox = std::move(b.BoundBox);

    gpuVertexBuf = b.gpuVertexBuf;
    gpuIndexBuf = b.gpuIndexBuf;
    b.gpuVertexBuf = 0;
    b.gpuIndexBuf = 0;

    return *this;
}

TMesh::~TMesh()
{
    GFX::Engine.MeshFreeVBO(this);

    // Just notify about still existing buffers
    if (gpuVertexBuf || gpuIndexBuf)
        printf("TMesh still has buffers!\n");
}

// One vertex buffer per mesh: the vertices, then each UV cache, then the rest
// places of a dug mesh (for the decals); the triangles in the index buffer.
void GFXEngine::MeshMakeVBO(TMesh *mesh)
{
    const uint32_t vtxDataSz = mesh->Vertexes.size() * sizeof(TVertex);
    const uint32_t coordDataSz = mesh->Vertexes.size() * sizeof(tUtV);
    const uint32_t restDataSz = mesh->Rest.size() * sizeof(vec3f);

    std::vector<uint8_t> data(vtxDataSz + mesh->CoordsCache.size() * coordDataSz + restDataSz);
    if (vtxDataSz)
        std::memcpy(data.data(), mesh->Vertexes.data(), vtxDataSz);

    uint32_t off = vtxDataSz;
    for (TCoordsCache &cch : mesh->CoordsCache)
    {
        std::memcpy(data.data() + off, cch.Coords.data(), std::min<size_t>(coordDataSz, cch.Coords.size() * sizeof(tUtV)));
        cch.BufferPos = off;
        off += coordDataSz;
    }

    // OpenNeoUA custom: rest places of a dug mesh, for the decals.
    mesh->RestBufferPos = off;
    if (restDataSz)
        std::memcpy(data.data() + off, mesh->Rest.data(), restDataSz);

    if (!mesh->gpuVertexBuf)
        mesh->gpuVertexBuf = GPU::CreateBuffer(data.size(), 0);
    GPU::UploadBuffer(mesh->gpuVertexBuf, data.data(), data.size());

    const uint32_t indexSz = mesh->Indixes.size() * sizeof(IndexType);
    if (!mesh->gpuIndexBuf)
        mesh->gpuIndexBuf = GPU::CreateBuffer(indexSz, sizeof(IndexType));
    GPU::UploadBuffer(mesh->gpuIndexBuf, mesh->Indixes.data(), indexSz);
}

void GFXEngine::MeshFreeVBO(TMesh *mesh)
{
    GPU::DestroyBuffer(mesh->gpuVertexBuf);
    GPU::DestroyBuffer(mesh->gpuIndexBuf);
    mesh->gpuVertexBuf = 0;
    mesh->gpuIndexBuf = 0;
}

void GFXEngine::SetProjectionMatrix(const mat4x4f &mat)
{
    _stdParams.Proj = mat.Transpose();
}

void GFXEngine::SetModelViewMatrix(const mat4x4f &mat)
{
    _stdParams.View = mat.Transpose();
}

}
