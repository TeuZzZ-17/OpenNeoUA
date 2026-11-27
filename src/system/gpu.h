// SDL_GPU renderer core (Vulkan through SDL3): device, resources, pipelines and the
// frame recorder used by GFXEngine.
//
// How a frame works: while the engine builds a frame it records targets, clears,
// draws and texture copies. Present() then uploads every buffer and texture that
// changed in one copy pass, replays the recorded steps in order and blits the
// backbuffer to the window. A draw captures the GPU objects it uses at record time,
// so re-uploading a buffer or texture later in the same frame gives it a fresh GPU
// object and earlier draws keep the old content (the OpenGL "orphaning" rule the
// engine was written for).
//
// Conventions kept from the OpenGL renderer: clip z in [-1, 1] (converted in the
// vertex shaders), counter-clockwise front faces, the colour of flat-shaded
// triangles taken from their last vertex (index buffers are rotated on upload).
#ifndef SYSTEM_GPU_H_INCLUDED
#define SYSTEM_GPU_H_INCLUDED

#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <vector>

namespace GPU
{

// Small integer handles (0 = none) so engine structs keep plain ids.
typedef uint32_t TextureId;
typedef uint32_t BufferId;
typedef uint32_t ProgramId;

// The backbuffer: the window-sized texture blitted to the window at Present().
constexpr TextureId BACKBUFFER = 0xFFFFFFFFu;
constexpr TextureId BACKBUFFER_DEPTH = 0xFFFFFFFEu;

enum class Format : uint8_t
{
    BGRA8,      // engine surfaces in ARGB8888 (memory B, G, R, A)
    RGBA8,      // engine surfaces in ABGR8888 (memory R, G, B, A), LUTs, movie frames
    RGBA16F,    // scene with colour effects: additive light may exceed 1 before post
    RGBA32F,    // decal rest places (alpha holds a packed 24-bit normal)
    Depth       // 32-bit float depth, sampleable by the decal shader
};

enum TextureUsage : uint32_t
{
    USAGE_SAMPLED = 1u << 0,
    USAGE_TARGET  = 1u << 1
};

enum class Blend : uint8_t { Zero, One, SrcAlpha, OneMinusSrcAlpha };

// Vertex streams a program reads (attribute locations 0..3: pos, colour, uv, rest).
enum class Layout : uint8_t
{
    Mesh,           // TVertex pos/colour/uv
    MeshCachedUV,   // TVertex pos/colour, uv from a packed tUtV array (animated UVs)
    Rest            // TVertex pos/uv plus a packed vec3 array (decal rest pass)
};

struct DrawState
{
    ProgramId program = 0;
    Layout layout = Layout::Mesh;
    bool blend = false;
    Blend src = Blend::One;
    Blend dst = Blend::Zero;
    bool depthTest = true;      // LEQUAL, like the OpenGL renderer
    bool depthWrite = true;
    bool colorWrite = true;
    bool depthBias = false;     // pull toward the camera (glPolygonOffset(-1, -1))
};

// Byte offsets of each vertex stream inside one buffer (0 = this frame's
// transient arena, see PushTransient).
struct Streams
{
    BufferId buffer = 0;
    uint32_t pos = 0;
    uint32_t color = 0;
    uint32_t uv = 0;
    uint32_t rest = 0;
};

struct Sampler
{
    TextureId texture = 0;      // 0 = a 1x1 white texture
    bool linear = true;
};

struct Rect
{
    int x = 0, y = 0, w = 0, h = 0;   // pixels, origin at the top-left corner
};

// Starts the device on the window. vsync: 0 off, 1 on, 2 adaptive (mailbox when
// available). vertexPitch = bytes of one engine vertex (TVertex). False when no
// Vulkan device can drive the window.
bool Init(SDL_Window *window, int vsync, uint32_t vertexPitch);
void Shutdown();
bool IsReady();
void SetVSync(int vsync);

// Graphics chip and API reported by the driver (for logs and the debug overlay).
const std::string &DeviceName();
const std::string &DriverName();

// GLSL 450 programs compiled to SPIR-V at runtime. The fragment shader may use
// up to fsSamplers samplers (set 2) and one uniform block (set 3, binding 0);
// the vertex shader one uniform block (set 1, binding 0). 0 on failure, with
// the compiler message in *error.
ProgramId CreateProgram(const std::string &vertexSource, const std::string &fragmentSource,
                        uint32_t fsSamplers, const std::string &name, std::string *error);
void DestroyProgram(ProgramId program);

TextureId CreateTexture(int w, int h, Format format, uint32_t usage);
// Whole-texture upload; pitch in bytes of one row of pixels.
void UploadTexture(TextureId texture, const void *pixels, int pitch);
void DestroyTexture(TextureId texture);
bool TextureSize(TextureId texture, int *w, int *h);

// indexBytes: 0 = vertex buffer, 2 or 4 = triangle-list index buffer of that size.
BufferId CreateBuffer(uint32_t size, uint32_t indexBytes);
// Replaces the whole buffer content (size must not exceed the created size).
// Index triangles are stored rotated (a b c -> c a b): same triangle and winding,
// with the OpenGL provoking (last) vertex first, as Vulkan expects.
void UploadBuffer(BufferId buffer, const void *data, uint32_t size);
void DestroyBuffer(BufferId buffer);

// Vertex bytes valid for this frame only; returns their offset in the arena.
uint32_t PushTransient(const void *data, uint32_t size);
// Triangle list for quads built from 4 transient vertices (0 1 2, 0 2 3).
BufferId QuadIndexBuffer();

// Recording. Draws go to the current target until another one is set.
void SetTarget(TextureId color, TextureId depth);
TextureId CurrentColorTarget();
TextureId CurrentDepthTarget();
void Clear(bool color, bool depth, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 0.0f);
void SetViewport(int x, int y, int w, int h);
void SetScissor(const Rect *rect);  // nullptr = whole target
void Draw(const DrawState &state, const Streams &streams, BufferId indexBuffer, bool index16,
          uint32_t indexCount, uint32_t firstIndex,
          const Sampler *samplers, uint32_t numSamplers,
          const void *vsUniforms, uint32_t vsSize, const void *fsUniforms, uint32_t fsSize);
// Copies src into dst (same size and kind) at this point of the frame.
void CopyTexture(TextureId src, TextureId dst);

// Runs the recorded frame and shows it.
void Present();
// Window size in pixels the backbuffer follows.
void BackbufferSize(int *w, int *h);
// RGB rows of the last presented frame, bottom row first (the screenshot order).
// w/h: wanted size in, size actually read out (never above the backbuffer).
bool ReadLastFrameRGB(std::vector<uint8_t> *rgb, int *w, int *h);

}

#endif // SYSTEM_GPU_H_INCLUDED
