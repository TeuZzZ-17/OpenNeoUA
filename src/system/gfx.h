// GFXEngine: rendering back end (SDL_GPU, see gpu.h), render nodes, textures, 2D drawing.
#ifndef ENGINE_GFX_H_INCLUDED
#define ENGINE_GFX_H_INCLUDED

#include <array>
#include <deque>
#include <map>
#include <math.h>
#include <inttypes.h>

#include "system.h"
#include "common/ini.h"
#include "common/vectors.h"
#include "common/matrix.h"
#include "nucleus/IFFile.h"
#include "nucleus/nucleas.h"
#include "common/common.h"
#include "common/cachestor.h"
#include "common/line.h"
#include "decalshader.h"
#include "gpu.h"


#define GFX_MAX_VERTEX 12

class NC_STACK_bitmap;

typedef std::array<SDL_Color, 256> UA_PALETTE;

struct ResBitmap
{
    int16_t width = 0;
    int16_t height = 0;
    UA_PALETTE *palette = nullptr;
    SDL_Surface *swTex = nullptr;
    GPU::TextureId hwTex = 0;
};

typedef std::vector<uint8_t> CmdStream;
typedef std::vector< CmdStream* > CmdIncludes;

struct TileMap
{
    NC_STACK_bitmap *img = nullptr;
    std::array<Common::PointRect, 256> map;
    std::map<uint32_t, Common::PointRect> extendedGlyphs;
    // Optional second sheet (OpenNeoUA icons): its keys win over the same keys of img.
    NC_STACK_bitmap *extraImg = nullptr;
    std::map<uint32_t, Common::PointRect> extraGlyphs;
    const Common::PointRect &Glyph(uint32_t key) const;
    SDL_Surface *Page(uint32_t key) const; // surface that holds Glyph(key)
    int h = 0;

    TileMap();
    ~TileMap();

    void Draw(SDL_Surface *surface, const Common::Point &pos, uint8_t c);
    void Draw(SDL_Surface *surface, const Common::PointRect &posRect, uint8_t c);
    void Draw(SDL_Surface *surface, const Common::Rect &posRect, uint8_t c);

    void Fill(SDL_Surface *surface, const Common::Rect &rect, uint8_t c);
    void Fill(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c);

    //Special fill use 1pix column
    void FillColumn(SDL_Surface *surface, const Common::Rect &rect, uint8_t c);
    void FillColumn(SDL_Surface *surface, const Common::PointRect &rect, uint8_t c);

    int GetWidth(uint8_t c) const;
    int GetWidth(const std::string &str) const;
    Common::Point GetSize(uint8_t c) const;
};

struct w3d_a209
{
    char *cmdbuf = nullptr;
    char **includ = nullptr;
};

struct tUtV
{
    float tu = -1.0;
    float tv = -1.0;

    tUtV() = default;
    tUtV(float u, float v): tu(u), tv(v) {};
};


struct windd_arg256
{
    int sort_id;
    int width;
    int height;
    char name[32];
};



namespace GFX
{

constexpr float VIEW_ZOOM_MIN = 1.0f;
constexpr float VIEW_ZOOM_MAX = 8.0f;

#ifndef GFXSHORTINDEX
typedef uint32_t IndexType;
#else
typedef uint16_t IndexType;
#endif

enum RFLAGS
{
    RFLAGS_TEXTURED  =      (1 << 0),
    RFLAGS_DYNAMIC_TEXTURE = (1 << 1),
    RFLAGS_SHADED    =      (1 << 2),
    RFLAGS_FOG       =      (1 << 3),
    RFLAGS_ZEROTRACY =      (1 << 4),
    RFLAGS_LUMTRACY  =      (1 << 5),
    RFLAGS_SKY       =      (1 << 6),
    RFLAGS_COMPUTED_COLOR = (1 << 9),
    RFLAGS_DISABLE_ZWRITE = (1 << 10),
    RFLAGS_ALPHA_FOG =      (1 << 11),
    RFLAGS_ALPHABLEND =     (1 << 12),
    // World surface (terrain, buildings): drawn before the decals so it receives them.
    RFLAGS_DECAL_RECEIVER = (1 << 13),
};

// OpenNeoUA extended projection limits.
//
// The world far plane is unlocked natively instead of exposing another
// low-level INI knob. 65536 safely covers the automatic horizon-derived
// sector window up to its internal maximum while the sky keeps a larger pass.
constexpr float WORLD_FAR_CLIP = 65536.0f;
constexpr float SKY_FAR_CLIP = 262144.0f;

// RGBA colour in floats: vertex colours, tints, fog.
struct __attribute__((packed)) TColorF
{
    float r = 1.0;
    float g = 1.0;
    float b = 1.0;
    float a = 1.0;

    TColorF() = default;
    TColorF(float _r, float _g, float _b, float _a = 1.0)
        : r(_r), g(_g), b(_b), a(_a) {};
};

struct TBoundBox
{
    vec3d Min;
    vec3d Max;

    bool Inited = false;

    void Add(const vec3d &pt)
    {
        if (Inited)
        {
            if (pt.x < Min.x) Min.x = pt.x;
            if (pt.y < Min.y) Min.y = pt.y;
            if (pt.z < Min.z) Min.z = pt.z;
            if (pt.x > Max.x) Max.x = pt.x;
            if (pt.y > Max.y) Max.y = pt.y;
            if (pt.z > Max.z) Max.z = pt.z;
        }
        else
        {
            Inited = true;
            Min = pt;
            Max = pt;
        }
    }

    void Add(const TBoundBox &box)
    {
        Add(box.Min);
        Add(box.Max);
    }
};

struct TRenderParams
{
    NC_STACK_bitmap *TexSource = nullptr;
    ResBitmap *Tex = nullptr;

    uint32_t Flags = 0;
    TColorF Color;

    TRenderParams() = default;
    TRenderParams(const TRenderParams &) = default;
    TRenderParams(TRenderParams &&) = default;

    TRenderParams(uint32_t flags)
        : Flags(flags){};

    TRenderParams(NC_STACK_bitmap *tex, uint32_t flags)
        : TexSource(tex), Flags(flags){};

    TRenderParams(ResBitmap *tex, uint32_t flags)
        : Tex(tex), Flags(flags){};

    TRenderParams& operator=(const TRenderParams &) = default;
    TRenderParams& operator=(TRenderParams &&) = default;

    bool operator==(const TRenderParams &b);
};

struct TCoordsCache
{
    ResBitmap *Tex = nullptr;
    std::vector<tUtV> Coords;

    // Byte offset of these UVs inside the mesh vertex buffer.
    int32_t BufferPos = 0;
};

struct TVertex
{
    vec3f Pos;
    //vec3f Normal;
    tUtV  TexCoord;
    TColorF Color;
    TColorF ComputedColor;
    uint32_t TexCoordId = 0;

    TVertex() = default;
    TVertex(const vec3f &p): Pos(p) {};
    TVertex(const vec3f &p, const tUtV &uv): Pos(p), TexCoord(uv) {};
    TVertex(const vec3f &p, const tUtV &uv, const TColorF &clr): Pos(p), TexCoord(uv), Color(clr) {};
};

class TMesh
{
public:
    TRenderParams Mat;
    std::vector<TVertex> Vertexes;
    std::vector<IndexType> Indixes;

    std::vector<TCoordsCache> CoordsCache;

    // OpenNeoUA custom: where each vertex was before terrain craters and dents
    // moved it (same count as Vertexes). Empty on a mesh that was never dug.
    // The decals read it to stay painted on a surface that changed shape.
    std::vector<vec3f> Rest;
    int32_t RestBufferPos = 0;   // Byte offset of Rest inside gpuVertexBuf.

    TBoundBox BoundBox;

    TMesh() = default;
    ~TMesh();
    TMesh(const TMesh&);
    TMesh(TMesh&&) noexcept;

    TMesh &operator=(const TMesh&);
    TMesh &operator=(TMesh&&) noexcept;

    void RecalcBoundBox()
    {
        BoundBox = TBoundBox();
        for (const TVertex &v : Vertexes)
            BoundBox.Add(v.Pos);
    }

    // GPU buffers made by MeshMakeVBO: vertices, UV caches and rest places in one
    // vertex buffer, triangles in the index buffer. Copies never share them.
    GPU::BufferId gpuVertexBuf = 0;
    GPU::BufferId gpuIndexBuf = 0;
};

struct TRenderNode
{
    enum
    {
        TYPE_MESH,
        TYPE_PARTICLE
    };

    uint8_t Type = TYPE_MESH;

    TMesh *Mesh = nullptr;
    TMesh LocalMesh;

    int32_t coordsID = -1;

    ResBitmap *Tex = nullptr;

    uint32_t Flags = 0;
    TColorF Color;
    // OpenNeoUA custom: per-node visual target hue/alpha. Neutral (1,1,1,1) = no change.
    TColorF ColorMul = TColorF(1.0, 1.0, 1.0, 1.0);
    // Treat ColorMul.rgb as a replacement hue instead of a channel multiplier.
    bool Colorize = false;
    // OpenNeoUA custom: transient VP fade kept separate from ColorMul alpha so
    // additive (One/One) LUMTRACY materials can attenuate their RGB output.
    float VPFadeFactor = 1.0f;

    mat4x4 TForm;
    int32_t TimeStamp = 0;
    int32_t FrameTime = 0;

    float FogStart = 0.0;
    float FogLength = 0.0;

    float Distance = 0.0;

    float ParticleSize = 0.0;

    TRenderNode() = default;
    TRenderNode(TRenderNode &&) = default;
    TRenderNode(const TRenderNode &) = default;

    TRenderNode& operator=(const TRenderNode &) = default;
    TRenderNode& operator=(TRenderNode &&) = default;

    TRenderNode(uint8_t tp): Type(tp) {};

    static bool CompareSolid(TRenderNode *a, TRenderNode *b);
    static bool CompareTransparent(TRenderNode *a, TRenderNode *b);
};

enum
{
    DEFAULT_WIDTH = 640,
    DEFAULT_HEIGHT = 480,
};

// One projected decal of the current frame (see decalshader.h). Position and
// axes use world axes relative to the camera, so large maps keep their precision.
struct TDecalDraw
{
    vec3f Origin;
    vec3f AxisT;
    vec3f AxisB;
    vec3f AxisN;
    float SizeT = 0.0f;
    float SizeB = 0.0f;
    float EdgeFade = 0.0f;
    float Moved = 0.0f;   // How far craters and dents may have moved the surface under it.
    const float *Shape = nullptr;   // DECAL_SHAPE_SAMPLES radii, owned by the caller.
    ResBitmap *Tex = nullptr;
    TColorF Tint = TColorF(1.0, 1.0, 1.0, 1.0);
    bool Colorize = false;
};

// Render states of the next draws (see SetRenderStates and RenderingMesh).
struct GfxStates
{
    GPU::TextureId Tex = 0;
    bool Shaded = true;
    GPU::Blend SrcBlend = GPU::Blend::One;
    GPU::Blend DstBlend = GPU::Blend::Zero;
    bool AlphaBlend = false;
    bool Zwrite = true;
    bool LinearFilter = true;
    bool AlphaTest = false;
    bool Fog = false;
    float FogStart = 3496.0;
    float FogLength = 600.0;
    float FogStrength = 1.0;
    TColorF FogColor = TColorF(0.0, 0.0, 0.0, 1.0);
    int32_t AFog = 0; // 0: off, 1: radial smooth fade, 2: classic camera-depth fade
    float AFogStart = 3496.0;
    float AFogLength = 600.0;
    float AFogStrength = 1.0;
    TColorF AFogColor = TColorF(0.588235f, 0.607843f, 0.627451f, 1.0f);

    bool DepthTest = true;
    GPU::ProgramId Prog = 0;
};

// Uniforms of the post programs: PostParams of GFXEngine::_postPrelude (std140).
struct TPostParams
{
    float normclr[3];
    float filterStrength;
    float invclr[3];
    float vhsStrength;
    int32_t screenSize[2];
    int32_t randval;
    int32_t millisecs;
    float atmosphereStrength;
    float atmosphereExposure;
    float atmosphereContrast;
    float atmosphereSaturation;
    float atmosphereVignette;
    float pad[3];
};
static_assert(sizeof(TPostParams) == 80, "TPostParams must match PostParams of _postPrelude");

struct rstr_arg204
{
    ResBitmap *pbitm = nullptr;
    Common::FRect float4;
    Common::FRect float14;
    uint8_t opacity = 255;
};

struct rstr_218_arg
{
    ResBitmap *bitm_intern;
    ResBitmap *bitm_intern2;
    int flg;
    Common::FRect rect1;
    Common::FRect rect2;
};

struct ColorFx
{
    int Id = 0;
    float Pwr = 1.0;

    ColorFx() {};
    ColorFx(int id, float pwr) : Id(id), Pwr(pwr) {};
};

struct displ_arg263
{
    ResBitmap *bitm = nullptr;
    int pointer_id = 0;
};

struct windd_dlgBox
{
    const char *title;
    const char *ok;
    const char *cancel;
    const char *startText;
    int time;
    void (*timer_func)(int, int, int);
    void *timer_context;
    int replace;
    int maxLen;
    char *result;
};


struct TGFXDeviceInfo
{
    std::string name;
    std::string guid;
    bool isCurrent = false;

    TGFXDeviceInfo(const std::string &__name, const std::string &__guid)
    : name(__name), guid(__guid) {}
};

struct ScreenText
{
    std::string string;
    int p1;
    int p2;
    int p3;
    int p4;
    int flag;
};

struct ScreenFont
{
    TTF_Font *ttfFont = nullptr;
    int height = 0;
    uint8_t r = 0, g = 0, b = 0;

    std::vector<ScreenText> entries;
};

struct GfxMode
{
    int w = 0;
    int h = 0;
    int bpp = 0;
    bool windowed = false;
    std::string name;

    GfxMode() {};
    GfxMode(GfxMode &&g);
    GfxMode(const GfxMode &g);
    GfxMode(const Common::Point &sz);

    GfxMode& operator=(const GfxMode &g);
    operator bool() const;

    bool operator==(const GfxMode &g) const;
    bool operator==(const Common::Point &g) const;
    bool operator!=(const GfxMode &g) const;
    bool operator!=(const Common::Point &g) const;

    GfxMode WithWindowed(bool state) const
    {
        GfxMode t = *this;
        t.windowed = state;
        return t;
    }

    operator Common::Point() const { return Common::Point(w, h); };

    static bool SortCompare(const GfxMode &a, const GfxMode &b);

    static std::string GenName(int w, int h);

};

enum WDD_ATT
{
    WDD_ATT_CURSOR = 0x80002000,
    WDD_ATT_TEXFILT = 0x80002001,
    WDD_ATT_DIS_LOWRES = 0x80002002,
    WDD_ATT_16BIT_TEX = 0x80002003,
    WDD_ATT_DRAW_PRIM = 0x80002004
};

enum DISP_ATT
{
    ATT_FGPEN       = 0x80001000,
    ATT_BGPEN       = 0x80001001,
    ATT_SHADE_RMP   = 0x80001002,
    ATT_TRACY_RMP   = 0x80001003,
    ATT_FGAPEN      = 0x80001004,
    ATT_WIDTH       = 0x80001005,
    ATT_HEIGHT      = 0x80001006,
    ATT_PALETTE     = 0x80001007,
    ATT_DISPLAY_ID  = 0x80001008,
    ATT_DISPLAY_INF = 0x80001009,
    ATT_DISPLAY_WIN = 0x8000100A,
};

enum class VirtualUIStyle
{
    RETRO = 0,
    SMOOTH = 1
};

enum RASTER
{
    RASTER_SOLID   = (1 << 0),
    RASTER_SKY     = (1 << 1),
    RASTER_ZEROTR  = (1 << 2),
    RASTER_LUMATR  = (1 << 3),
    RASTER_PARTICL = (1 << 4),

    RASTER_ALL = (RASTER_SOLID | RASTER_SKY | RASTER_ZEROTR | RASTER_LUMATR | RASTER_PARTICL)
};

class GFXEngine
{
public:
    static const std::array<vec3d, 8> _clrEff;

    static int can_srcblend;
    static int can_destblend;
    static int can_stippling;

protected:
    static const SDL_PixelFormatDetails *_pixfmt;
    static GPU::Format _texFormat;     // GPU texel format matching _pixfmt surfaces
    static bool _staticInited;

    static void StaticInit();

public:
    void Init();
    void Deinit();

    void RecreateScreenSurface();
    void DrawScreenSurface();
    void DrawVirtualUISurface();
    // OpenNeoUA: immediate solid-color rectangle on the virtual UI surface.
    // Drawing into the same surface as the rest of the gameplay UI preserves
    // normal window layering automatically instead of requiring a later overlay pass.
    void DrawVirtualUISolidRect(float left, float top, float right, float bottom,
                                const TColorF &color);
    Common::Point GetVirtualUIResolution() const;
    void BeginVirtualUI(const Common::Point &logicalSize);
    void EndVirtualUI();
    void SetVirtualUIStyle(VirtualUIStyle style) { _virtualUiStyle = style; }
    VirtualUIStyle GetVirtualUIStyle() const { return _virtualUiStyle; }
    bool IsVirtualUIPass() const { return _virtualUiPass; }

    // Text of a GLSL shader file of the game data (empty when missing).
    static std::string ReadShaderFile(const std::string &path);
    // Full-screen post program: built-in vertex shader + a Data/res fragment shader.
    GPU::ProgramId LoadPostProgram(const std::string &fragmentPath, const std::string &what);

    void SetFBOBlending(int mode);
    void DrawFBO();
    void DrawVhsFilter();
    void DrawVhsEffect();
    void UpdateFBOSizes();


    SDL_Surface *Screen();


    uint8_t *MakeScreenCopy(int *ow, int *oh);

    GfxMode windd_func0__sub0(const std::string &file);
    GfxMode sub_41F68C();

    int  GetGfxModeIndex(const Common::Point &res);

    void SetResolution(const Common::Point &res, bool windowed);
    void SetTracyRmp(ResBitmap *rmp);
    void SetShadeRmp(ResBitmap *rmp);

    void SetCursor(int curID, int force);
    GfxMode GetGfxMode();
    const std::vector<GfxMode> &GetAvailableModes();

    void SetTileset(TileMap *tileset, int id);
    TileMap * GetTileset(int id);

    bool LoadPalette(const std::string &palette_ilbm);
    SDL_Cursor *LoadCursor(const std::string &name, bool retro = false);
    void LoadCursorSet(bool retro);

    // OpenNeoUA custom: modern fullscreen visual filter (Data/Filters/*.pal used as a
    // luminance-indexed LUT applied on the final post-process pass). Vanilla-safe:
    // "Standard"/empty/strength 0 produce no change at all.
    void SetVisualFilter(const std::string &filterName);
    void SetVisualFilterStrength(float strength);
    void ApplyVisualFilterFromConfig();
    void ApplyAtmosphereFromConfig();
    void ReloadHorizonConfig();
    void SetVhsFilterEnabled(bool enabled);
    void ApplyVhsFilterFromConfig();
    bool IsVisualFilterActive() const { return _visualFilterActive; }
    bool IsAtmosphereActive() const { return _atmosphereActive; }
    float GetVisualFilterStrength() const { return _visualFilterStrength; }
    const std::string &GetVisualFilterName() const { return _visualFilterName; }


    static const SDL_PixelFormatDetails *GetPixelFormat() { return _pixfmt; };
    static uint32_t GetPixelFormatU32() { return _pixfmt->format; };
    static GPU::Format GetTexFormat() { return _texFormat; };


    static void DrawLine(SDL_Surface *surface, const Common::Line &line, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t alpha = 255, bool preserveSurfaceAlpha = false);
    static void BlitScaleMasked(SDL_Surface *src, Common::Rect sRect, SDL_Surface *mask, uint8_t index, SDL_Surface *dst, Common::Rect dRect);
    static void DrawFill(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    static void DrawFillAlpha(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect, uint8_t opacity);

    static void Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::Point dPoint);
    static void Draw(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, Common::PointRect dRect);

    static SDL_Color Color(uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255);

protected:
    template <typename T>
    static void DrawFillIntCKey(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    template <typename T>
    static void DrawFillIntACpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);
    static void DrawFillIntCpy(SDL_Surface *src, const Common::Rect &sRect, SDL_Surface *dst, const Common::Rect &dRect);

private:
    GFXEngine();
    ~GFXEngine();

    void AddGfxMode(const GfxMode &md);
    uint32_t CursPix(uint8_t *data, int ofs, int bpp);

    static bool EventsWatcher(void *, SDL_Event *event);
    void CreateSceneTargets(const Common::Point &size);

public:
    virtual size_t func0(IDVList &stak);
    virtual size_t raster_func198(const Common::FLine &arg);
    virtual size_t raster_func199(const Common::Line &arg);
    virtual size_t raster_func200(const Common::FLine &arg);
    virtual size_t raster_func201(const Common::Line &r);
    virtual size_t raster_func202(rstr_arg204 *arg);
    virtual size_t raster_func204(rstr_arg204 *arg);
    size_t BlitPixelRect(ResBitmap *pbitm, Common::Rect r1, Common::Rect r2, uint8_t opacity = 255);
    virtual void raster_func207(int id, TileMap *tiles);
    virtual TileMap *raster_func208(int id);
    virtual int raster_func208(TileMap *tiles);

    virtual void ProcessDrawSeq(const CmdStream &cmdline, const CmdIncludes *arr = nullptr,
                                const SDL_Color *uiAccent = nullptr);

    virtual void raster_func210(const Common::FRect &arg);
    virtual void raster_func211(const Common::Rect &arg);
    virtual void BeginScene();
    virtual void EndScene();
    virtual size_t raster_func217(SDL_Color color);
    virtual void raster_func218(rstr_218_arg *arg);
    virtual void raster_func221(const Common::Rect &arg);
    virtual void BeginFrame();
    virtual void EndFrame();
    virtual void SetColorEffectsPowers(const std::vector<ColorFx> &arg);
    void LoadCustomColorEffectsFromConfig();

    virtual bool AllocTexture(ResBitmap *arg);
    virtual void FreeTexture(ResBitmap *arg);
    virtual void display_func271(IDVPair *stak);
    virtual void SaveScreenshot(const std::string & screenName);
    virtual void windd_func320(IDVPair *);
    virtual void windd_func321(IDVPair *);
    virtual void windd_func322(windd_dlgBox *dlgBox);

    virtual const std::vector<TGFXDeviceInfo>& GetDevices();
    virtual void SetDeviceByGUID(const std::string &guid, bool writefile = true);


    void MeshMakeVBO(TMesh *);
    void MeshFreeVBO(TMesh *);


    virtual void setWDD_cursor(int arg);
    virtual void setWDD_16bitTex(int arg);
    virtual void setWDD_drawPrim(int arg);

    virtual int getWDD_16bitTex();
    virtual int getWDD_drawPrim();



    virtual void SetPalette(UA_PALETTE &pal);
    virtual void SetPen(SDL_Color pen);
    virtual void setRSTR_BGpen(uint32_t pen) {};
    virtual void setRSTR_shdRmp(ResBitmap *rmp) {};
    virtual void setRSTR_trcRmp(ResBitmap *rmp) {};
    virtual void setRSTR_FGApen(uint32_t pen) {};

    virtual UA_PALETTE *GetPalette();

    virtual int16_t GetScreenW() const { return _resolution.x; }
    virtual int16_t GetScreenH() const { return _resolution.y; }
    virtual Common::Point GetScreenSize() const { return _resolution; };




    // windd methods
    size_t windd_func0(IDVList &stak);

    virtual void setW3D_texFilt(int arg);

    void draw2DandFlush();

    void DrawVtxQuad(const std::array<GFX::TVertex, 4> &vtx);
    // Pipeline states of a standard-shader draw, from _states.
    GPU::DrawState StdDrawState(GPU::Layout layout) const;
    // Full-screen quad of a post program; image and aux are its two samplers.
    void DrawPostQuad(GPU::ProgramId program, GPU::TextureId image, GPU::TextureId aux,
                      bool blend, GPU::Blend src, GPU::Blend dst, const TPostParams &params);
    TPostParams PostParamsBase(const Common::Point &scrSz) const;

    int LoadFontByDescr(const std::string &fontname);
    int MeasureScreenTextWidth(const std::string &text) const;
    void DrawLocationTitle(const std::string &name, const SDL_Color &color, float opacity);
    void matrixAspectCorrection(mat3x3 &inout, bool invert);
    void getAspectCorrection(float &cW, float &cH, bool invert);
    void viewZoomCorrection(float &x, float &y, bool invert = false) const;

    void setViewZoom(float zoom);
    float getViewZoom() const { return _viewZoom; }

    virtual void ConvAlphaPalette(UA_PALETTE *dst, const UA_PALETTE &src, bool transp);

    virtual SDL_Surface *CreateSurfaceScreenFormat(int width, int height);
    virtual SDL_Surface *ConvertToScreenFormat(SDL_Surface *src);

    static SDL_Surface *ConvertSDLSurface(SDL_Surface *src, SDL_PixelFormat fmt);


    float GetColorEffectPower(int id);

    Common::Point ConvertPosTo2DStuff(const Common::Point &pos);

    void RenderingMesh(TRenderNode *mesh);
    void RenderNode(TRenderNode *node);
    void Rasterize(uint32_t RasterEtapes = RASTER_ALL);

    void QueueRenderMesh(TRenderNode *mesh);
    // Projected decals of this frame, painted over the world before the units.
    void SetDecalView(const mat3x3 &worldToView, uint32_t renderFlags,
                      float fogStart, float fogLength);
    void QueueRenderDecal(const TDecalDraw &decal);
    TRenderNode &AllocRenderNode() { return _renderNodesCache.GetNext(); };

    float GetAlpha() const { return _alpha / 255.0; };

    GfxStates& States() { return _states; };
    const GfxStates& GetLastStates() { return _lastStates; };

    GPU::ProgramId GetStdShaderProg() { return _stdShaderProg; };

    void SetRenderStates(int arg);

    void SetProjectionMatrix(const mat4x4f &mat);
    void SetModelViewMatrix(const mat4x4f &mat);

    const mat4x4f &GetProjectionMatrix() const { return _frustum; };
    float GetProjectionNear() const { return _frustumNear; };
    float GetProjectionFar() const { return _frustumFar; };

protected:
    void initPolyEngine();

    void DrawScreenText();
    void AddScreenText(const std::string &string, int p1, int p2, int p3, int p4, int flag);
    void DrawTextEntry(const ScreenText *txt);


    void win3d_func274__sub0(FSMgr::FileHandle *fil);

    static SDL_Cursor *wrapLoadCursor(const char *name);

    void _setFrustumClip(float near, float far);

    void sub_420EDC(Common::Line line, uint8_t r, uint8_t g, uint8_t b, uint8_t alpha = 255);
    char * windd_func322__sub0(const char *box_title, const char *box_ok, const char *box_cancel, const char *box_startText, uint32_t timer_time, void (*timer_func)(int, int, int), void *timer_context, int replace, int maxLen);

    void ApplyResolution();

    bool SetResVariables(Common::Point res);
    bool LoadAtmosphereShader();
    void FreeAtmosphereShader();
    bool LoadVhsFilterShader();
    void FreeVhsFilterShader();
    bool EnsureVhsFilterTexture(const Common::Point &scrSz);
    void ApplyFogStates(uint32_t flags, float fogStart, float fogLength);
    void ReportGraphicsChip();
    void ReadGraphicsChip();
    bool EnsureDecalDepthTexture(const Common::Point &scrSz);
    bool DrawDecalRest(const Common::Point &scrSz, const float *viewToWorldColumns);
    void DrawDecalPass();


public:
    //Data
    //static constexpr const char * __ClassName = ;

    // Graphics chip in use, the API that drives it and whether it is a chip built
    // into the processor (debug overlay and the startup log).
    const std::string &GpuRenderer() { ReadGraphicsChip(); return _gpuRenderer; }
    const std::string &GpuApi() { ReadGraphicsChip(); return _gpuApi; }
    bool GpuIsIntegrated() { ReadGraphicsChip(); return _gpuIntegrated; }


public:
    static GFXEngine Instance;

private:
    std::vector<GfxMode> graphicsModes;
    std::array<SDL_Cursor *, 11> cursors{};
    int CurrentCursorID = -1;
    GfxMode GfxSelectedMode;

    SDL_Surface *ScreenSurface = nullptr;
    SDL_Surface *VirtualUISurface = nullptr;
    GPU::TextureId screenTex = 0;
    GPU::TextureId virtualUiTex = 0;

    bool _virtualUiPass = false;
    bool _virtualUiPending = false;
    VirtualUIStyle _virtualUiStyle = VirtualUIStyle::RETRO;
    Common::Point _virtualUiResolution;
    Common::Point _virtualUiTextureSize;
    Common::Rect _virtualUiSavedClip;
    Common::Rect _virtualUiSavedInverseClip;
    int _virtualUiSavedField54c = 0;
    int _virtualUiSavedField550 = 0;
    float _virtualUiSavedField554 = 0.0f;
    float _virtualUiSavedField558 = 0.0f;

    static const std::string _stdPShaderText;
    static const std::string _stdVShaderText;
    static const std::string _postVShaderText;
    static const std::string _postPrelude;

public:
    int _forcesoftcursor;
    int _field_38;
    int _txt16bit;
    int _use_simple_d3d;
    int _disable_lowres;
    int _export_window_mode;
    int _flags;

    // From bigdata
    // \From bigdata

    int _dither;
    int _filter;
    int _antialias;
    int _alpha;
    int _zbuf_when_tracy;
    int _colorkey;



    std::vector<TRenderNode *> _renderSolidList;
    std::vector<TRenderNode *> _renderSkyBoxList;
    std::vector<TRenderNode *> _renderZeroTracyList;
    std::vector<TRenderNode *> _renderLumaTracyList;
    std::vector<TDecalDraw> _renderDecalList;

    Common::CacheStorage<TRenderNode> _renderNodesCache;

    int _sceneBeginned;

    ScreenFont _font;

    float _corrW, _corrIW;
    float _corrH, _corrIH;
    bool _solidFont;


    // Display class
    SDL_Color _field_4 = {0}; // Color?
    Common::Rect _clip;
    Common::Rect _inverseClip;
    std::array<TileMap *, 256> _tiles;
    std::map<std::pair<SDL_Surface *, uint8_t>, SDL_Surface *> _uiAccentSurfaces;
    SDL_Color _uiAccentCacheColor = {0};
    bool _uiAccentCacheValid = false;
    int _field_54c   = 0;
    int _field_550   = 0;
    float _field_554 = 0;
    float _field_558 = 0;

    UA_PALETTE _palette;

    void ClearUiAccentCache();
    void ClearLocationTitle();
    TTF_Font *_locationFont=nullptr;
    TTF_Font *_locationLabelFont=nullptr;
    SDL_Surface *_locationTitle=nullptr, *_locationLabel=nullptr;
    std::string _locationTitleName;
    SDL_Color _locationTitleColor{0,0,0,0};
    int _locationCanvasWidth=0, _locationCanvasHeight=0;
    SDL_Surface *GetUiAccentSurface(SDL_Surface *source, const SDL_Color &accent,
                                    uint8_t tilesetId);
    static bool IsUiAccentTileset(uint8_t id);
    static bool IsUiAccentNeutralHighlightTileset(uint8_t id);
    static bool IsFullFactionUiAtlas(uint8_t id);
    static SDL_Color RemapUiAccentColor(const SDL_Color &source, const SDL_Color &accent,
                                        bool includeNeutralHighlights = false,
                                        int neutralThreshold = 180,
                                        bool tintAllNonDark = false);
    static SDL_Color RemapFullFactionUiColor(const SDL_Color &source,
                                              const SDL_Color &accent);

protected:
    // Display class
    Common::Point _resolution;

    // Win3d
    mat4x4f _frustum;
    float _frustumNear;
    float _frustumFar;
    float _viewZoom = 1.0f;

    GPU::ProgramId _stdShaderProg = 0;

    // Uniforms of the standard shader (std140, same layout in both stages):
    // matrices for the vertex stage, fog and material switches for the fragment one.
    struct
    {
        mat4x4f Proj;
        mat4x4f View;
        float   Fog = 0.0;
        float   FogStart = 0.0;
        float   FogLength = 0.0;
        float   FogStrength = 1.0;
        float   AFog = 0.0;
        float   AFogStart = 0.0;
        float   AFogLength = 0.0;
        float   AFogStrength = 1.0;
        int32_t Textured = 0;
        int32_t Flat = 0;
        int32_t ATest = 0;
        int32_t Colorize = 0;
        float   ColorMul[4] = {1.0, 1.0, 1.0, 1.0};
        float   FogColor[4] = {0.0, 0.0, 0.0, 1.0};
        float   AtmosphereColor[4] = {0.588235f, 0.607843f, 0.627451f, 1.0f};
    } _stdParams;

    GfxStates _states;
    GfxStates _lastStates;
    bool _colorWrite = true;      // false while the sky writes only depth

    vec3d _normClr;
    vec3d _invClr;
    std::array<vec3d, 17> _customClrEff{};
    std::array<bool, 17> _customClrEffDefined{};

    int32_t _colorEffects = 0;

    // The 3D scene is drawn here when colour effects are on, then composed by the
    // post program (DrawFBO). RGBA16F keeps additive light above 1 until then.
    GPU::TextureId _sceneTex = 0;
    GPU::TextureId _sceneDepth = 0;
    Common::Point _sceneSize;
    int32_t _fboBlend = 0;

    GPU::ProgramId _colorEffectsShaderProg = 0;

    // OpenNeoUA custom: optional world-only atmospheric colour pass, drawn by its
    // own post program in place of the colour effects one.
    GPU::ProgramId _atmosphereShaderProg = 0;
    bool _atmosphereEnabled = false;
    bool _atmosphereActive = false;
    float _atmosphereStrength = 1.0f;
    float _atmosphereExposure = 1.0f;
    float _atmosphereContrast = 1.0f;
    float _atmosphereSaturation = 1.0f;
    float _atmosphereVignette = 0.0f;

    // OpenNeoUA custom: fullscreen visual filter state
    GPU::TextureId _visualFilterLut = 0;  // 256x1 RGBA LUT
    float _visualFilterStrength = 0.0f;   // effective blend strength (0 => off)
    bool _visualFilterActive = false;     // true only when a real filter LUT is loaded
    std::string _visualFilterName;        // selected filter file name ("Standard" when none)

    // OpenNeoUA experimental: optional VHS post-process stacked after the visual filter.
    GPU::TextureId _vhsCopyTex = 0;       // copy of the finished frame
    GPU::TextureId _vhsOutTex = 0;        // VHS effect output (240 lines for original video)
    Common::Point _vhsCopyTexSize;
    Common::Point _vhsOutTexSize;
    bool _originalVideoFilter = false;
    GPU::ProgramId _vhsFilterProg = 0;
    GPU::ProgramId _vhsBlendProg = 0;

    bool _graphicsChipReported = false;

    // Filled once by ReadGraphicsChip (debug overlay and the startup log).
    bool _graphicsChipRead = false;
    std::string _gpuRenderer, _gpuApi;
    bool _gpuIntegrated = false;

    GPU::ProgramId _decalProg = 0;
    GPU::TextureId _decalDepthTex = 0;    // copy of the world depth, read by the decals
    Common::Point _decalDepthTexSize;
    // Rest pass: its program, its float texture and the dug meshes of the frame.
    GPU::ProgramId _decalRestProg = 0;
    GPU::TextureId _decalRestTex = 0;
    Common::Point _decalRestTexSize;
    std::vector<TRenderNode *> _decalDugList;
    mat3x3 _decalWorldToView;
    uint32_t _decalRenderFlags = 0;
    float _decalFogStart = 0.0f;
    float _decalFogLength = 0.0f;
    bool _vhsFilterActive = false;
    float _vhsFilterStrength = 0.0f;
    std::string _vhsFilterShaderPath;

    static std::vector<TGFXDeviceInfo> _devices;
};

static constexpr GFXEngine &Engine = GFXEngine::Instance;







}

#endif // ENGINE_GFX_H_INCLUDED
