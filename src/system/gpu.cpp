// SDL_GPU renderer core: see gpu.h for the frame model.
#include "system/gpu.h"

#include <shaderc/shaderc.h>

#include <algorithm>
#include <cstring>
#include <map>

#include "common/log.h"

namespace GPU
{

namespace
{

struct TextureRec
{
    SDL_GPUTexture *gpu = nullptr;
    int w = 0, h = 0;
    Format format = Format::RGBA8;
    uint32_t usage = 0;
    uint64_t usedFrame = 0;     // frame that recorded a draw or copy with it
    int pendingUpload = -1;     // index in g_uploads of this frame's upload
    bool live = false;
};

struct BufferRec
{
    SDL_GPUBuffer *gpu = nullptr;
    uint32_t size = 0;
    uint32_t indexBytes = 0;
    uint64_t usedFrame = 0;
    int pendingUpload = -1;
    bool live = false;
};

struct ProgramRec
{
    SDL_GPUShader *vs = nullptr;
    SDL_GPUShader *fs = nullptr;
    bool live = false;
};

struct Upload
{
    SDL_GPUTexture *texture = nullptr;  // texture upload when set
    SDL_GPUBuffer *buffer = nullptr;
    uint32_t w = 0, h = 0;
    uint32_t stagingOffset = 0;
    uint32_t size = 0;
    bool skip = false;                  // replaced by a later upload of the same frame
};

struct DrawRec
{
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    Layout layout = Layout::Mesh;
    SDL_GPUBuffer *vertex = nullptr;    // nullptr = transient arena
    uint32_t pos = 0, color = 0, uv = 0, rest = 0;
    SDL_GPUBuffer *index = nullptr;
    bool index16 = false;
    uint32_t count = 0, first = 0;
    SDL_GPUTextureSamplerBinding samplers[3] = {};
    uint32_t numSamplers = 0;
    uint32_t vsUniform = 0, vsSize = 0, fsUniform = 0, fsSize = 0;  // in g_uniforms
    SDL_GPUViewport viewport = {};
    bool scissorOn = false;
    SDL_Rect scissor = {};
};

struct Step
{
    bool copy = false;
    // Render pass.
    SDL_GPUTexture *color = nullptr;
    SDL_GPUTexture *depth = nullptr;
    bool clearColor = false, clearDepth = false;
    SDL_FColor clear = {0, 0, 0, 0};
    size_t firstDraw = 0, numDraws = 0;
    // Texture copy.
    SDL_GPUTexture *src = nullptr, *dst = nullptr;
    uint32_t w = 0, h = 0;
};

SDL_GPUDevice *g_device = nullptr;
SDL_Window *g_window = nullptr;
std::string g_deviceName, g_driverName;
uint32_t g_vertexPitch = 0;
shaderc_compiler_t g_compiler = nullptr;

std::vector<TextureRec> g_textures;
std::vector<BufferRec> g_buffers;
std::vector<ProgramRec> g_programs;
std::vector<uint32_t> g_freeTextures, g_freeBuffers, g_freePrograms;

TextureRec g_backColor, g_backDepth;
// Backbuffer of the last presented frame. After a resize it is the old one, kept
// alive until the next Present releases it, so the readback still finds the image.
TextureRec g_lastShown;
TextureId g_white = 0;
BufferId g_quadIndex = 0;
SDL_GPUSampler *g_linear = nullptr;
SDL_GPUSampler *g_nearest = nullptr;

std::map<uint64_t, SDL_GPUGraphicsPipeline *> g_pipelines;

// This frame's recording.
uint64_t g_frame = 1;
std::vector<uint8_t> g_staging;
std::vector<Upload> g_uploads;
std::vector<uint8_t> g_transient;
std::vector<uint8_t> g_uniforms;
std::vector<DrawRec> g_draws;
std::vector<Step> g_steps;
std::vector<SDL_GPUTexture *> g_deadTextures;
std::vector<SDL_GPUBuffer *> g_deadBuffers;
bool g_stepOpen = false;          // last step is a pass that can still take draws
TextureId g_targetColor = BACKBUFFER;
TextureId g_targetDepth = BACKBUFFER_DEPTH;
SDL_GPUViewport g_viewport = {};
bool g_scissorOn = false;
SDL_Rect g_scissor = {};

SDL_GPUBuffer *g_transientGpu = nullptr;
uint32_t g_transientGpuSize = 0;
SDL_GPUTransferBuffer *g_transfer = nullptr;
uint32_t g_transferSize = 0;

SDL_GPUTextureFormat NativeFormat(Format format)
{
    switch (format)
    {
        case Format::BGRA8:   return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case Format::RGBA8:   return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case Format::RGBA16F: return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case Format::RGBA32F: return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        case Format::Depth:   return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
}

uint32_t BytesPerTexel(Format format)
{
    switch (format)
    {
        case Format::RGBA16F: return 8;
        case Format::RGBA32F: return 16;
        default:              return 4;
    }
}

SDL_GPUTexture *MakeTexture(int w, int h, Format format, uint32_t usage)
{
    SDL_GPUTextureCreateInfo info = {};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = NativeFormat(format);
    if ( usage & USAGE_SAMPLED )
        info.usage |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
    if ( usage & USAGE_TARGET )
        info.usage |= format == Format::Depth ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
                                              : SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = (uint32_t)std::max(1, w);
    info.height = (uint32_t)std::max(1, h);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(g_device, &info);
}

SDL_GPUBuffer *MakeBuffer(uint32_t size, uint32_t indexBytes)
{
    SDL_GPUBufferCreateInfo info = {};
    info.usage = indexBytes ? SDL_GPU_BUFFERUSAGE_INDEX : SDL_GPU_BUFFERUSAGE_VERTEX;
    info.size = std::max<uint32_t>(size, 4);
    return SDL_CreateGPUBuffer(g_device, &info);
}

TextureRec *FindTexture(TextureId id)
{
    if ( id == BACKBUFFER )
        return &g_backColor;
    if ( id == BACKBUFFER_DEPTH )
        return &g_backDepth;
    if ( id == 0 || id > g_textures.size() || !g_textures[id - 1].live )
        return nullptr;
    return &g_textures[id - 1];
}

BufferRec *FindBuffer(BufferId id)
{
    if ( id == 0 || id > g_buffers.size() || !g_buffers[id - 1].live )
        return nullptr;
    return &g_buffers[id - 1];
}

uint32_t Stage(const void *data, uint32_t size)
{
    const uint32_t offset = (uint32_t)g_staging.size();
    g_staging.resize(offset + size);
    if ( data && size )
        std::memcpy(g_staging.data() + offset, data, size);
    return offset;
}

uint32_t StoreUniforms(const void *data, uint32_t size)
{
    const uint32_t offset = (uint32_t)g_uniforms.size();
    if ( data && size )
        g_uniforms.insert(g_uniforms.end(), (const uint8_t *)data, (const uint8_t *)data + size);
    return offset;
}

// The backbuffer follows the window's pixel size; checked when a frame starts recording.
void EnsureBackbuffer()
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(g_window, &w, &h);
    w = std::max(1, w);
    h = std::max(1, h);
    if ( g_backColor.gpu && g_backColor.w == w && g_backColor.h == h )
        return;

    if ( g_backColor.gpu )
        g_deadTextures.push_back(g_backColor.gpu);
    if ( g_backDepth.gpu )
        g_deadTextures.push_back(g_backDepth.gpu);

    g_backColor.gpu = MakeTexture(w, h, Format::BGRA8, USAGE_SAMPLED | USAGE_TARGET);
    g_backColor.w = w;
    g_backColor.h = h;
    g_backColor.format = Format::BGRA8;
    g_backColor.usage = USAGE_SAMPLED | USAGE_TARGET;
    g_backColor.live = true;

    g_backDepth.gpu = MakeTexture(w, h, Format::Depth, USAGE_SAMPLED | USAGE_TARGET);
    g_backDepth.w = w;
    g_backDepth.h = h;
    g_backDepth.format = Format::Depth;
    g_backDepth.usage = USAGE_SAMPLED | USAGE_TARGET;
    g_backDepth.live = true;
}

Step &OpenPass()
{
    if ( !g_stepOpen )
    {
        // A window resized during this frame, before its first use of the
        // backbuffer: draw at the new size already.
        if ( (g_targetColor == BACKBUFFER || g_targetDepth == BACKBUFFER_DEPTH) && g_backColor.usedFrame != g_frame )
            EnsureBackbuffer();

        Step step;
        TextureRec *color = FindTexture(g_targetColor);
        TextureRec *depth = FindTexture(g_targetDepth);
        step.color = color ? color->gpu : nullptr;
        step.depth = depth ? depth->gpu : nullptr;
        step.firstDraw = g_draws.size();
        if ( color )
            color->usedFrame = g_frame;
        if ( depth )
            depth->usedFrame = g_frame;
        g_steps.push_back(step);
        g_stepOpen = true;
    }
    return g_steps.back();
}

SDL_GPUBlendFactor NativeBlend(Blend blend)
{
    switch (blend)
    {
        case Blend::Zero:             return SDL_GPU_BLENDFACTOR_ZERO;
        case Blend::One:              return SDL_GPU_BLENDFACTOR_ONE;
        case Blend::SrcAlpha:         return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        case Blend::OneMinusSrcAlpha: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    }
    return SDL_GPU_BLENDFACTOR_ONE;
}

SDL_GPUGraphicsPipeline *FindPipeline(const DrawState &state, SDL_GPUTextureFormat colorFormat, bool hasDepth)
{
    const uint64_t key = (uint64_t)state.program
                       | (uint64_t)state.layout << 20
                       | (uint64_t)state.blend << 24
                       | (uint64_t)state.src << 25
                       | (uint64_t)state.dst << 27
                       | (uint64_t)state.depthTest << 29
                       | (uint64_t)state.depthWrite << 30
                       | (uint64_t)state.colorWrite << 31
                       | (uint64_t)state.depthBias << 32
                       | (uint64_t)hasDepth << 33
                       | (uint64_t)colorFormat << 34;

    auto it = g_pipelines.find(key);
    if ( it != g_pipelines.end() )
        return it->second;

    if ( state.program == 0 || state.program > g_programs.size() || !g_programs[state.program - 1].live )
        return nullptr;
    const ProgramRec &program = g_programs[state.program - 1];

    // Attribute locations: 0 pos, 1 colour, 2 uv, 3 rest place; one stream per slot.
    SDL_GPUVertexBufferDescription buffers[4] = {};
    SDL_GPUVertexAttribute attributes[4] = {};
    uint32_t count = 0;
    const auto addStream = [&](uint32_t slot, uint32_t pitch, SDL_GPUVertexElementFormat format)
    {
        buffers[count].slot = slot;
        buffers[count].pitch = pitch;
        buffers[count].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        attributes[count].location = slot;
        attributes[count].buffer_slot = slot;
        attributes[count].format = format;
        attributes[count].offset = 0;
        ++count;
    };
    addStream(0, g_vertexPitch, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);
    if ( state.layout != Layout::Rest )
        addStream(1, g_vertexPitch, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4);
    addStream(2, state.layout == Layout::MeshCachedUV ? 8u : g_vertexPitch, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2);
    if ( state.layout == Layout::Rest )
        addStream(3, 12u, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3);

    SDL_GPUColorTargetDescription target = {};
    target.format = colorFormat;
    target.blend_state.enable_blend = state.blend;
    target.blend_state.src_color_blendfactor = NativeBlend(state.src);
    target.blend_state.dst_color_blendfactor = NativeBlend(state.dst);
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    // glBlendFunc applies the same factors to alpha.
    target.blend_state.src_alpha_blendfactor = NativeBlend(state.src);
    target.blend_state.dst_alpha_blendfactor = NativeBlend(state.dst);
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.enable_color_write_mask = true;
    target.blend_state.color_write_mask = state.colorWrite
        ? (SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A)
        : 0;

    SDL_GPUGraphicsPipelineCreateInfo info = {};
    info.vertex_shader = program.vs;
    info.fragment_shader = program.fs;
    info.vertex_input_state.vertex_buffer_descriptions = buffers;
    info.vertex_input_state.num_vertex_buffers = count;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = count;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    if ( state.depthBias )
    {
        info.rasterizer_state.enable_depth_bias = true;
        info.rasterizer_state.depth_bias_constant_factor = -1.0f;
        info.rasterizer_state.depth_bias_slope_factor = -1.0f;
    }
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = hasDepth && state.depthTest;
    info.depth_stencil_state.enable_depth_write = hasDepth && state.depthWrite;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = hasDepth;
    info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(g_device, &info);
    if ( !pipeline )
        log_d3dlog("GPU pipeline creation failed: %s\n", SDL_GetError());
    g_pipelines[key] = pipeline;
    return pipeline;
}

SDL_GPUShader *CompileStage(const std::string &source, bool fragment, uint32_t samplers,
                            const std::string &name, std::string *error)
{
    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
    shaderc_compilation_result_t result = shaderc_compile_into_spv(
        g_compiler, source.data(), source.size(),
        fragment ? shaderc_fragment_shader : shaderc_vertex_shader,
        name.c_str(), "main", options);
    shaderc_compile_options_release(options);

    SDL_GPUShader *shader = nullptr;
    if ( shaderc_result_get_compilation_status(result) == shaderc_compilation_status_success )
    {
        SDL_GPUShaderCreateInfo info = {};
        info.code = (const Uint8 *)shaderc_result_get_bytes(result);
        info.code_size = shaderc_result_get_length(result);
        info.entrypoint = "main";
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.stage = fragment ? SDL_GPU_SHADERSTAGE_FRAGMENT : SDL_GPU_SHADERSTAGE_VERTEX;
        info.num_samplers = fragment ? samplers : 0;
        info.num_uniform_buffers = 1;
        shader = SDL_CreateGPUShader(g_device, &info);
        if ( !shader && error )
            *error = SDL_GetError();
    }
    else if ( error )
        *error = shaderc_result_get_error_message(result);

    shaderc_result_release(result);
    return shader;
}

void ReleaseDead()
{
    for (SDL_GPUTexture *texture : g_deadTextures)
        SDL_ReleaseGPUTexture(g_device, texture);
    for (SDL_GPUBuffer *buffer : g_deadBuffers)
        SDL_ReleaseGPUBuffer(g_device, buffer);
    g_deadTextures.clear();
    g_deadBuffers.clear();
}

void ResetRecording()
{
    for (TextureRec &texture : g_textures)
        texture.pendingUpload = -1;
    for (BufferRec &buffer : g_buffers)
        buffer.pendingUpload = -1;
    g_staging.clear();
    g_uploads.clear();
    g_transient.clear();
    g_uniforms.clear();
    g_draws.clear();
    g_steps.clear();
    g_stepOpen = false;
    g_targetColor = BACKBUFFER;
    g_targetDepth = BACKBUFFER_DEPTH;
    g_scissorOn = false;
    ++g_frame;
    EnsureBackbuffer();
    g_viewport = {0.0f, 0.0f, (float)g_backColor.w, (float)g_backColor.h, 0.0f, 1.0f};
}

SDL_GPUPresentMode PresentModeFor(int vsync)
{
    if ( vsync == 0 && SDL_WindowSupportsGPUPresentMode(g_device, g_window, SDL_GPU_PRESENTMODE_IMMEDIATE) )
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    if ( vsync == 2 && SDL_WindowSupportsGPUPresentMode(g_device, g_window, SDL_GPU_PRESENTMODE_MAILBOX) )
        return SDL_GPU_PRESENTMODE_MAILBOX;
    return SDL_GPU_PRESENTMODE_VSYNC;
}

}

bool Init(SDL_Window *window, int vsync, uint32_t vertexPitch)
{
    if ( g_device )
        return true;

    g_window = window;
    g_vertexPitch = vertexPitch;
    g_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, nullptr);
    if ( !g_device )
    {
        log_d3dlog("SDL_GPU: no Vulkan device: %s\n", SDL_GetError());
        return false;
    }
    if ( !SDL_ClaimWindowForGPUDevice(g_device, window) )
    {
        log_d3dlog("SDL_GPU: cannot present to the window: %s\n", SDL_GetError());
        SDL_DestroyGPUDevice(g_device);
        g_device = nullptr;
        return false;
    }

    g_compiler = shaderc_compiler_initialize();
    SetVSync(vsync);

    const char *driver = SDL_GetGPUDeviceDriver(g_device);
    g_driverName = driver ? driver : "unknown";
    const SDL_PropertiesID props = SDL_GetGPUDeviceProperties(g_device);
    const char *name = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, nullptr);
    g_deviceName = name ? name : "unknown";

    SDL_GPUSamplerCreateInfo sampler = {};
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    g_linear = SDL_CreateGPUSampler(g_device, &sampler);
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    g_nearest = SDL_CreateGPUSampler(g_device, &sampler);

    ResetRecording();

    const uint32_t white = 0xFFFFFFFFu;
    g_white = CreateTexture(1, 1, Format::RGBA8, USAGE_SAMPLED);
    UploadTexture(g_white, &white, 4);

    const uint16_t quad[6] = {0, 1, 2, 0, 2, 3};
    g_quadIndex = CreateBuffer(sizeof(quad), 2);
    UploadBuffer(g_quadIndex, quad, sizeof(quad));
    return true;
}

void Shutdown()
{
    if ( !g_device )
        return;

    SDL_WaitForGPUIdle(g_device);
    for (auto &entry : g_pipelines)
        if ( entry.second )
            SDL_ReleaseGPUGraphicsPipeline(g_device, entry.second);
    g_pipelines.clear();
    for (TextureRec &texture : g_textures)
        if ( texture.live && texture.gpu )
            SDL_ReleaseGPUTexture(g_device, texture.gpu);
    for (BufferRec &buffer : g_buffers)
        if ( buffer.live && buffer.gpu )
            SDL_ReleaseGPUBuffer(g_device, buffer.gpu);
    for (ProgramRec &program : g_programs)
    {
        if ( program.vs )
            SDL_ReleaseGPUShader(g_device, program.vs);
        if ( program.fs )
            SDL_ReleaseGPUShader(g_device, program.fs);
    }
    g_textures.clear();
    g_buffers.clear();
    g_programs.clear();
    g_freeTextures.clear();
    g_freeBuffers.clear();
    g_freePrograms.clear();
    ReleaseDead();
    if ( g_backColor.gpu )
        SDL_ReleaseGPUTexture(g_device, g_backColor.gpu);
    if ( g_backDepth.gpu )
        SDL_ReleaseGPUTexture(g_device, g_backDepth.gpu);
    g_backColor = TextureRec();
    g_backDepth = TextureRec();
    g_lastShown = TextureRec();
    if ( g_transientGpu )
        SDL_ReleaseGPUBuffer(g_device, g_transientGpu);
    if ( g_transfer )
        SDL_ReleaseGPUTransferBuffer(g_device, g_transfer);
    g_transientGpu = nullptr;
    g_transfer = nullptr;
    g_transientGpuSize = g_transferSize = 0;
    SDL_ReleaseGPUSampler(g_device, g_linear);
    SDL_ReleaseGPUSampler(g_device, g_nearest);
    if ( g_compiler )
        shaderc_compiler_release(g_compiler);
    g_compiler = nullptr;
    SDL_ReleaseWindowFromGPUDevice(g_device, g_window);
    SDL_DestroyGPUDevice(g_device);
    g_device = nullptr;
    g_window = nullptr;
}

bool IsReady()
{
    return g_device != nullptr;
}

void SetVSync(int vsync)
{
    if ( g_device )
        SDL_SetGPUSwapchainParameters(g_device, g_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, PresentModeFor(vsync));
}

const std::string &DeviceName()
{
    return g_deviceName;
}

const std::string &DriverName()
{
    return g_driverName;
}

ProgramId CreateProgram(const std::string &vertexSource, const std::string &fragmentSource,
                        uint32_t fsSamplers, const std::string &name, std::string *error)
{
    if ( !g_device )
        return 0;

    ProgramRec rec;
    rec.vs = CompileStage(vertexSource, false, 0, name + ".vert", error);
    if ( rec.vs )
        rec.fs = CompileStage(fragmentSource, true, fsSamplers, name + ".frag", error);
    if ( !rec.vs || !rec.fs )
    {
        if ( rec.vs )
            SDL_ReleaseGPUShader(g_device, rec.vs);
        return 0;
    }
    rec.live = true;

    if ( !g_freePrograms.empty() )
    {
        const ProgramId id = g_freePrograms.back();
        g_freePrograms.pop_back();
        g_programs[id - 1] = rec;
        return id;
    }
    g_programs.push_back(rec);
    return (ProgramId)g_programs.size();
}

void DestroyProgram(ProgramId id)
{
    if ( id == 0 || id > g_programs.size() || !g_programs[id - 1].live )
        return;

    // Drop the pipelines built from it; recorded draws keep them until Present.
    SDL_WaitForGPUIdle(g_device);
    for (auto it = g_pipelines.begin(); it != g_pipelines.end();)
    {
        if ( (it->first & 0xFFFFF) == id )
        {
            if ( it->second )
                SDL_ReleaseGPUGraphicsPipeline(g_device, it->second);
            it = g_pipelines.erase(it);
        }
        else
            ++it;
    }
    ProgramRec &program = g_programs[id - 1];
    SDL_ReleaseGPUShader(g_device, program.vs);
    SDL_ReleaseGPUShader(g_device, program.fs);
    program = ProgramRec();
    g_freePrograms.push_back(id);
}

TextureId CreateTexture(int w, int h, Format format, uint32_t usage)
{
    if ( !g_device )
        return 0;

    TextureRec rec;
    rec.gpu = MakeTexture(w, h, format, usage);
    if ( !rec.gpu )
    {
        log_d3dlog("SDL_GPU: texture %dx%d not created: %s\n", w, h, SDL_GetError());
        return 0;
    }
    rec.w = std::max(1, w);
    rec.h = std::max(1, h);
    rec.format = format;
    rec.usage = usage;
    rec.live = true;

    if ( !g_freeTextures.empty() )
    {
        const TextureId id = g_freeTextures.back();
        g_freeTextures.pop_back();
        g_textures[id - 1] = rec;
        return id;
    }
    g_textures.push_back(rec);
    return (TextureId)g_textures.size();
}

void UploadTexture(TextureId id, const void *pixels, int pitch)
{
    TextureRec *texture = FindTexture(id);
    if ( !texture || !pixels || id == BACKBUFFER || id == BACKBUFFER_DEPTH )
        return;

    // Already drawn this frame: earlier draws keep the old picture.
    if ( texture->usedFrame == g_frame )
    {
        g_deadTextures.push_back(texture->gpu);
        texture->gpu = MakeTexture(texture->w, texture->h, texture->format, texture->usage);
        texture->usedFrame = 0;
        texture->pendingUpload = -1;
    }

    const uint32_t row = (uint32_t)texture->w * BytesPerTexel(texture->format);
    const uint32_t size = row * (uint32_t)texture->h;
    const uint32_t offset = Stage(nullptr, size);
    for (int y = 0; y < texture->h; ++y)
        std::memcpy(g_staging.data() + offset + y * row, (const uint8_t *)pixels + (size_t)y * pitch, row);

    if ( texture->pendingUpload >= 0 )
        g_uploads[texture->pendingUpload].skip = true;
    Upload upload;
    upload.texture = texture->gpu;
    upload.w = (uint32_t)texture->w;
    upload.h = (uint32_t)texture->h;
    upload.stagingOffset = offset;
    upload.size = size;
    texture->pendingUpload = (int)g_uploads.size();
    g_uploads.push_back(upload);
}

void DestroyTexture(TextureId id)
{
    TextureRec *texture = FindTexture(id);
    if ( !texture || id == BACKBUFFER || id == BACKBUFFER_DEPTH )
        return;

    if ( texture->pendingUpload >= 0 )
        g_uploads[texture->pendingUpload].skip = true;
    // SDL keeps a released texture alive while submitted work uses it; recorded
    // but not yet submitted draws need it until Present.
    g_deadTextures.push_back(texture->gpu);
    *texture = TextureRec();
    g_freeTextures.push_back(id);
}

bool TextureSize(TextureId id, int *w, int *h)
{
    TextureRec *texture = FindTexture(id);
    if ( !texture )
        return false;
    if ( w )
        *w = texture->w;
    if ( h )
        *h = texture->h;
    return true;
}

BufferId CreateBuffer(uint32_t size, uint32_t indexBytes)
{
    if ( !g_device )
        return 0;

    BufferRec rec;
    rec.gpu = MakeBuffer(size, indexBytes);
    if ( !rec.gpu )
        return 0;
    rec.size = std::max<uint32_t>(size, 4);
    rec.indexBytes = indexBytes;
    rec.live = true;

    if ( !g_freeBuffers.empty() )
    {
        const BufferId id = g_freeBuffers.back();
        g_freeBuffers.pop_back();
        g_buffers[id - 1] = rec;
        return id;
    }
    g_buffers.push_back(rec);
    return (BufferId)g_buffers.size();
}

void UploadBuffer(BufferId id, const void *data, uint32_t size)
{
    BufferRec *buffer = FindBuffer(id);
    if ( !buffer || !data || !size )
        return;

    // Growing or already drawn this frame: a new GPU buffer takes the data.
    if ( size > buffer->size || buffer->usedFrame == g_frame )
    {
        g_deadBuffers.push_back(buffer->gpu);
        buffer->size = std::max(size, buffer->size);
        buffer->gpu = MakeBuffer(buffer->size, buffer->indexBytes);
        buffer->usedFrame = 0;
        buffer->pendingUpload = -1;
    }

    const uint32_t offset = Stage(data, size);
    if ( buffer->indexBytes )
    {
        // a b c -> c a b: Vulkan takes flat colours from the first vertex, OpenGL from the last.
        uint8_t *bytes = g_staging.data() + offset;
        const uint32_t stride = buffer->indexBytes;
        uint8_t tmp[4];
        for (uint32_t tri = 0; tri + 3 * stride <= size; tri += 3 * stride)
        {
            std::memcpy(tmp, bytes + tri + 2 * stride, stride);
            std::memmove(bytes + tri + stride, bytes + tri, 2 * stride);
            std::memcpy(bytes + tri, tmp, stride);
        }
    }

    if ( buffer->pendingUpload >= 0 )
        g_uploads[buffer->pendingUpload].skip = true;
    Upload upload;
    upload.buffer = buffer->gpu;
    upload.stagingOffset = offset;
    upload.size = size;
    buffer->pendingUpload = (int)g_uploads.size();
    g_uploads.push_back(upload);
}

void DestroyBuffer(BufferId id)
{
    BufferRec *buffer = FindBuffer(id);
    if ( !buffer )
        return;

    if ( buffer->pendingUpload >= 0 )
        g_uploads[buffer->pendingUpload].skip = true;
    g_deadBuffers.push_back(buffer->gpu);
    *buffer = BufferRec();
    g_freeBuffers.push_back(id);
}

uint32_t PushTransient(const void *data, uint32_t size)
{
    // 16-byte alignment keeps every vertex stream offset aligned.
    const uint32_t offset = (uint32_t)((g_transient.size() + 15) & ~(size_t)15);
    g_transient.resize(offset + size);
    std::memcpy(g_transient.data() + offset, data, size);
    return offset;
}

BufferId QuadIndexBuffer()
{
    return g_quadIndex;
}

void SetTarget(TextureId color, TextureId depth)
{
    if ( color == g_targetColor && depth == g_targetDepth )
        return;
    g_targetColor = color;
    g_targetDepth = depth;
    g_stepOpen = false;
    g_scissorOn = false;
}

TextureId CurrentColorTarget()
{
    return g_targetColor;
}

TextureId CurrentDepthTarget()
{
    return g_targetDepth;
}

void Clear(bool color, bool depth, float r, float g, float b, float a)
{
    if ( !g_device || (!color && !depth) )
        return;

    // A clear starts the pass of the current target again with clear load ops.
    if ( g_stepOpen && g_steps.back().numDraws != 0 )
        g_stepOpen = false;
    Step &step = OpenPass();
    if ( color )
    {
        step.clearColor = true;
        step.clear = {r, g, b, a};
    }
    if ( depth )
        step.clearDepth = true;
}

void SetViewport(int x, int y, int w, int h)
{
    g_viewport = {(float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f};
}

void SetScissor(const Rect *rect)
{
    g_scissorOn = rect != nullptr;
    if ( rect )
        g_scissor = {rect->x, rect->y, rect->w, rect->h};
}

void Draw(const DrawState &state, const Streams &streams, BufferId indexBuffer, bool index16,
          uint32_t indexCount, uint32_t firstIndex,
          const Sampler *samplers, uint32_t numSamplers,
          const void *vsUniforms, uint32_t vsSize, const void *fsUniforms, uint32_t fsSize)
{
    if ( !g_device || indexCount == 0 )
        return;

    TextureRec *colorTarget = FindTexture(g_targetColor);
    if ( !colorTarget )
        return;
    TextureRec *depthTarget = FindTexture(g_targetDepth);

    DrawRec draw;
    draw.pipeline = FindPipeline(state, NativeFormat(colorTarget->format), depthTarget != nullptr);
    if ( !draw.pipeline )
        return;

    BufferRec *index = FindBuffer(indexBuffer);
    if ( !index )
        return;
    index->usedFrame = g_frame;
    draw.index = index->gpu;
    draw.index16 = index16;
    draw.count = indexCount;
    draw.first = firstIndex;

    if ( streams.buffer )
    {
        BufferRec *vertex = FindBuffer(streams.buffer);
        if ( !vertex )
            return;
        vertex->usedFrame = g_frame;
        draw.vertex = vertex->gpu;
    }
    draw.layout = state.layout;
    draw.pos = streams.pos;
    draw.color = streams.color;
    draw.uv = streams.uv;
    draw.rest = streams.rest;

    draw.numSamplers = std::min<uint32_t>(numSamplers, 3);
    for (uint32_t i = 0; i < draw.numSamplers; ++i)
    {
        TextureRec *texture = FindTexture(samplers[i].texture);
        if ( !texture )
            texture = FindTexture(g_white);
        texture->usedFrame = g_frame;
        draw.samplers[i].texture = texture->gpu;
        draw.samplers[i].sampler = samplers[i].linear ? g_linear : g_nearest;
    }

    draw.vsUniform = StoreUniforms(vsUniforms, vsSize);
    draw.vsSize = vsSize;
    draw.fsUniform = StoreUniforms(fsUniforms, fsSize);
    draw.fsSize = fsSize;
    draw.viewport = g_viewport;
    draw.scissorOn = g_scissorOn;
    draw.scissor = g_scissor;

    Step &step = OpenPass();
    g_draws.push_back(draw);
    ++step.numDraws;
}

void CopyTexture(TextureId srcId, TextureId dstId)
{
    TextureRec *src = FindTexture(srcId);
    TextureRec *dst = FindTexture(dstId);
    if ( !src || !dst )
        return;

    Step step;
    step.copy = true;
    step.src = src->gpu;
    step.dst = dst->gpu;
    step.w = (uint32_t)std::min(src->w, dst->w);
    step.h = (uint32_t)std::min(src->h, dst->h);
    src->usedFrame = g_frame;
    dst->usedFrame = g_frame;
    g_steps.push_back(step);
    g_stepOpen = false;
}

void Present()
{
    if ( !g_device )
        return;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if ( !cmd )
    {
        log_d3dlog("SDL_GPU: no command buffer: %s\n", SDL_GetError());
        ResetRecording();
        return;
    }

    // 1. Every upload of the frame, plus the transient vertices, in one copy pass.
    const uint32_t transientSize = (uint32_t)g_transient.size();
    const uint32_t transferNeed = (uint32_t)g_staging.size() + transientSize;
    if ( transientSize > g_transientGpuSize )
    {
        if ( g_transientGpu )
            g_deadBuffers.push_back(g_transientGpu);
        g_transientGpuSize = std::max<uint32_t>(transientSize * 2, 64 * 1024);
        g_transientGpu = MakeBuffer(g_transientGpuSize, 0);
    }
    if ( transferNeed )
    {
        if ( transferNeed > g_transferSize )
        {
            if ( g_transfer )
                SDL_ReleaseGPUTransferBuffer(g_device, g_transfer);
            g_transferSize = std::max<uint32_t>(transferNeed * 2, 1024 * 1024);
            SDL_GPUTransferBufferCreateInfo info = {};
            info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            info.size = g_transferSize;
            g_transfer = SDL_CreateGPUTransferBuffer(g_device, &info);
        }

        uint8_t *mapped = (uint8_t *)SDL_MapGPUTransferBuffer(g_device, g_transfer, true);
        if ( mapped )
        {
            if ( !g_staging.empty() )
                std::memcpy(mapped, g_staging.data(), g_staging.size());
            if ( transientSize )
                std::memcpy(mapped + g_staging.size(), g_transient.data(), transientSize);
            SDL_UnmapGPUTransferBuffer(g_device, g_transfer);

            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            for (const Upload &upload : g_uploads)
            {
                if ( upload.skip )
                    continue;
                if ( upload.texture )
                {
                    SDL_GPUTextureTransferInfo source = {g_transfer, upload.stagingOffset, upload.w, upload.h};
                    SDL_GPUTextureRegion region = {};
                    region.texture = upload.texture;
                    region.w = upload.w;
                    region.h = upload.h;
                    region.d = 1;
                    SDL_UploadToGPUTexture(copy, &source, &region, false);
                }
                else
                {
                    SDL_GPUTransferBufferLocation source = {g_transfer, upload.stagingOffset};
                    SDL_GPUBufferRegion region = {upload.buffer, 0, upload.size};
                    SDL_UploadToGPUBuffer(copy, &source, &region, false);
                }
            }
            if ( transientSize )
            {
                SDL_GPUTransferBufferLocation source = {g_transfer, (uint32_t)g_staging.size()};
                SDL_GPUBufferRegion region = {g_transientGpu, 0, transientSize};
                SDL_UploadToGPUBuffer(copy, &source, &region, false);
            }
            SDL_EndGPUCopyPass(copy);
        }
    }

    // 2. The recorded passes and copies, in order.
    for (const Step &step : g_steps)
    {
        if ( step.copy )
        {
            SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
            SDL_GPUTextureLocation src = {step.src, 0, 0, 0, 0, 0};
            SDL_GPUTextureLocation dst = {step.dst, 0, 0, 0, 0, 0};
            SDL_CopyGPUTextureToTexture(copy, &src, &dst, step.w, step.h, 1, false);
            SDL_EndGPUCopyPass(copy);
            continue;
        }
        if ( !step.color )
            continue;

        SDL_GPUColorTargetInfo color = {};
        color.texture = step.color;
        color.load_op = step.clearColor ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        color.clear_color = step.clear;
        SDL_GPUDepthStencilTargetInfo depth = {};
        depth.texture = step.depth;
        depth.clear_depth = 1.0f;
        depth.load_op = step.clearDepth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, step.depth ? &depth : nullptr);
        if ( !pass )
            continue;

        SDL_GPUGraphicsPipeline *bound = nullptr;
        for (size_t i = step.firstDraw; i < step.firstDraw + step.numDraws; ++i)
        {
            const DrawRec &draw = g_draws[i];
            if ( draw.pipeline != bound )
            {
                SDL_BindGPUGraphicsPipeline(pass, draw.pipeline);
                bound = draw.pipeline;
            }
            SDL_SetGPUViewport(pass, &draw.viewport);
            if ( draw.scissorOn )
                SDL_SetGPUScissor(pass, &draw.scissor);
            else
            {
                const SDL_Rect whole = {(int)draw.viewport.x, (int)draw.viewport.y,
                                        (int)draw.viewport.w, (int)draw.viewport.h};
                SDL_SetGPUScissor(pass, &whole);
            }

            SDL_GPUBuffer *vertex = draw.vertex ? draw.vertex : g_transientGpu;
            SDL_GPUBufferBinding streams[4] = {
                {vertex, draw.pos}, {vertex, draw.color}, {vertex, draw.uv}, {vertex, draw.rest}};
            if ( draw.layout == Layout::Rest )
            {
                SDL_BindGPUVertexBuffers(pass, 0, &streams[0], 1);
                SDL_BindGPUVertexBuffers(pass, 2, &streams[2], 2);
            }
            else
                SDL_BindGPUVertexBuffers(pass, 0, streams, 3);

            const SDL_GPUBufferBinding index = {draw.index, 0};
            SDL_BindGPUIndexBuffer(pass, &index, draw.index16 ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                                                              : SDL_GPU_INDEXELEMENTSIZE_32BIT);
            if ( draw.numSamplers )
                SDL_BindGPUFragmentSamplers(pass, 0, draw.samplers, draw.numSamplers);
            if ( draw.vsSize )
                SDL_PushGPUVertexUniformData(cmd, 0, g_uniforms.data() + draw.vsUniform, draw.vsSize);
            if ( draw.fsSize )
                SDL_PushGPUFragmentUniformData(cmd, 0, g_uniforms.data() + draw.fsUniform, draw.fsSize);
            SDL_DrawGPUIndexedPrimitives(pass, draw.count, 1, draw.first, 0, 0);
        }
        SDL_EndGPURenderPass(pass);
    }

    // 3. Backbuffer to the window (no swapchain while minimized).
    SDL_GPUTexture *swapchain = nullptr;
    Uint32 swapW = 0, swapH = 0;
    if ( SDL_WaitAndAcquireGPUSwapchainTexture(cmd, g_window, &swapchain, &swapW, &swapH) && swapchain )
    {
        SDL_GPUBlitInfo blit = {};
        blit.source.texture = g_backColor.gpu;
        blit.source.w = (uint32_t)g_backColor.w;
        blit.source.h = (uint32_t)g_backColor.h;
        blit.destination.texture = swapchain;
        blit.destination.w = swapW;
        blit.destination.h = swapH;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(cmd, &blit);
    }

    if ( !SDL_SubmitGPUCommandBuffer(cmd) )
        log_d3dlog("SDL_GPU: frame submit failed: %s\n", SDL_GetError());

    ReleaseDead();
    g_lastShown = g_backColor;
    ResetRecording();
}

void BackbufferSize(int *w, int *h)
{
    if ( w )
        *w = g_backColor.w;
    if ( h )
        *h = g_backColor.h;
}

bool ReadLastFrameRGB(std::vector<uint8_t> *rgb, int *outW, int *outH)
{
    if ( !g_device || !rgb || !outW || !outH || *outW <= 0 || *outH <= 0 || !g_lastShown.gpu )
        return false;

    // Never more than the frame holds; the size really read goes back.
    const int w = std::min(*outW, g_lastShown.w);
    const int h = std::min(*outH, g_lastShown.h);
    *outW = w;
    *outH = h;
    const uint32_t size = (uint32_t)w * (uint32_t)h * 4;

    SDL_GPUTransferBufferCreateInfo info = {};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = size;
    SDL_GPUTransferBuffer *download = SDL_CreateGPUTransferBuffer(g_device, &info);
    SDL_GPUCommandBuffer *cmd = download ? SDL_AcquireGPUCommandBuffer(g_device) : nullptr;
    if ( !cmd )
    {
        if ( download )
            SDL_ReleaseGPUTransferBuffer(g_device, download);
        return false;
    }

    // Nothing of the current frame is submitted yet: the last shown image is intact.
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region = {};
    region.texture = g_lastShown.gpu;
    region.w = (uint32_t)w;
    region.h = (uint32_t)h;
    region.d = 1;
    SDL_GPUTextureTransferInfo dest = {download, 0, (uint32_t)w, (uint32_t)h};
    SDL_DownloadFromGPUTexture(copy, &region, &dest);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    bool ok = false;
    if ( fence )
    {
        SDL_WaitForGPUFences(g_device, true, &fence, 1);
        SDL_ReleaseGPUFence(g_device, fence);
        const uint8_t *bgra = (const uint8_t *)SDL_MapGPUTransferBuffer(g_device, download, false);
        if ( bgra )
        {
            rgb->resize((size_t)w * h * 3);
            for (int y = 0; y < h; ++y)
            {
                const uint8_t *src = bgra + (size_t)(h - 1 - y) * w * 4;
                uint8_t *dst = rgb->data() + (size_t)y * w * 3;
                for (int x = 0; x < w; ++x)
                {
                    dst[x * 3 + 0] = src[x * 4 + 2];
                    dst[x * 3 + 1] = src[x * 4 + 1];
                    dst[x * 3 + 2] = src[x * 4 + 0];
                }
            }
            SDL_UnmapGPUTransferBuffer(g_device, download);
            ok = true;
        }
    }
    SDL_ReleaseGPUTransferBuffer(g_device, download);
    return ok;
}

}
