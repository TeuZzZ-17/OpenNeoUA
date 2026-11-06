// Performance probe (not part of the game build). Linked into a test
// executable with ld --wrap, so the game sources stay untouched.
//
// It starts a real level, lets the real game run, and reports for every frame:
//   - where the CPU time goes (simulation, queue building, submission, swap);
//   - how long the GPU takes, with real timer queries;
//   - how many draw calls, state changes and triangles each frame costs;
//   - which frames stutter, and what was slow in them.
//
// Scenario is read from <CAPTURE_DIR>/../perf_cfg.txt: "level frames decals mode sweep".
//   decals: 0 none, N spread around the player, -N all piled on one spot
//   mode:   0 game settings, 1 no scene FBO, 2 legacy GL, 3 no vsync/limit, 4 plain vsync
//   sweep:  0 still, 1 turn the camera, 2 fly the player across the map
//
// Build and link exactly like decal_capture.cpp (see its header), with --wrap on
// the names listed at the bottom of this file. Run it from "UA Rising"; it writes
// <CAPTURE_DIR>/perf.log. Back up Data/Save first: starting a level rewrites it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_QUERY_RESULT 0x8866
#endif

#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/uacommon.h"
#include "gui/root.h"
#include "system/gfx.h"
#include "system/inpt.h"
#include "system/framepacer.h"
#include "system/inivals.h"
#include "system/system.h"
#include "system/tform.h"
#include "world/grounddecal_profile.h"
#include "world/nparticle.h"

namespace
{

// ---------------------------------------------------------------- timing ---
double Now()
{
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

struct Stat
{
    std::vector<double> samples;

    void Add(double value) { samples.push_back(value); }

    double Percentile(double fraction) const
    {
        if (samples.empty())
            return 0.0;
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        size_t index = (size_t)(fraction * (double)(sorted.size() - 1) + 0.5);
        return sorted[std::min(index, sorted.size() - 1)];
    }

    double Mean() const
    {
        if (samples.empty())
            return 0.0;
        double total = 0.0;
        for (double v : samples)
            total += v;
        return total / (double)samples.size();
    }

    double Max() const
    {
        double best = 0.0;
        for (double v : samples)
            best = std::max(best, v);
        return best;
    }
};

// ------------------------------------------------------------ GPU queries ---
typedef void (APIENTRY *GenQueriesFn)(GLsizei, GLuint *);
typedef void (APIENTRY *DeleteQueriesFn)(GLsizei, const GLuint *);
typedef void (APIENTRY *BeginQueryFn)(GLenum, GLuint);
typedef void (APIENTRY *EndQueryFn)(GLenum);
typedef void (APIENTRY *GetQueryObjectuivFn)(GLuint, GLenum, GLuint *);
typedef void (APIENTRY *GetQueryObjectui64vFn)(GLuint, GLenum, GLuint64 *);

GenQueriesFn glGenQueries_ = nullptr;
BeginQueryFn glBeginQuery_ = nullptr;
EndQueryFn glEndQuery_ = nullptr;
GetQueryObjectuivFn glGetQueryObjectuiv_ = nullptr;
GetQueryObjectui64vFn glGetQueryObjectui64v_ = nullptr;

// Four queries in flight: reading the newest one back would stall the CPU and
// hide the very stutter we are looking for.
const int QUERY_SLOTS = 4;
GLuint gpuQuery[QUERY_SLOTS] = {0, 0, 0, 0};
bool gpuQueryBusy[QUERY_SLOTS] = {false, false, false, false};
int gpuSlot = 0;
bool gpuReady = false;

void InitGpuQueries()
{
    glGenQueries_ = (GenQueriesFn)SDL_GL_GetProcAddress("glGenQueries");
    glBeginQuery_ = (BeginQueryFn)SDL_GL_GetProcAddress("glBeginQuery");
    glEndQuery_ = (EndQueryFn)SDL_GL_GetProcAddress("glEndQuery");
    glGetQueryObjectuiv_ = (GetQueryObjectuivFn)SDL_GL_GetProcAddress("glGetQueryObjectuiv");
    glGetQueryObjectui64v_ = (GetQueryObjectui64vFn)SDL_GL_GetProcAddress("glGetQueryObjectui64v");
    gpuReady = glGenQueries_ && glBeginQuery_ && glEndQuery_ &&
               glGetQueryObjectuiv_ && glGetQueryObjectui64v_;
    if (gpuReady)
        glGenQueries_(QUERY_SLOTS, gpuQuery);
}

// --------------------------------------------------------------- scenario ---
struct Config
{
    int level = 1;
    int frames = 900;
    int decals = 0;
    int mode = 0;       // 1 = no scene FBO, 2 = legacy GL, 3 = no frame limit/vsync
    int sweep = 1;      // Turn the camera, so sector streaming is exercised.
};

Config cfg;
bool cfgLoaded = false;
FILE *logFile = nullptr;
std::string outDir;

void LoadConfig()
{
    if (cfgLoaded)
        return;
    cfgLoaded = true;
    outDir = CAPTURE_DIR;
    if (FILE *file = fopen((outDir + "/../perf_cfg.txt").c_str(), "r"))
    {
        if (fscanf(file, "%d %d %d %d %d", &cfg.level, &cfg.frames, &cfg.decals,
                   &cfg.mode, &cfg.sweep) < 4)
            cfg.frames = 900;
        fclose(file);
    }
}

// ------------------------------------------------------------- collection ---
enum Stage
{
    S_FRAME, S_SIM, S_RENDER_CPU, S_RASTER_CPU, S_SWAP, S_LIMITER,
    S_UI, S_GUI_SW, S_GUI_HW, S_SECTORS, S_FILLERS, S_DECALS, S_SMOKE, S_PARTICLES, S_GPU, S_COUNT
};

const char *stageName[S_COUNT] = {
    "frame (senza attesa)", "  simulazione", "  costruzione lista", "  invio alla GPU",
    "  attesa schermo", "attesa limite fps", "  interfaccia+post", "    gui software", "    gui hardware", "    settori", "    raccordi",
    "    decal", "    fumo decal", "    particelle", "GPU (frame intero)"};

Stat stats[S_COUNT];
double frameStage[S_COUNT];

Stat drawCalls, stateChanges, triangles, vboUploads, fillerRebuilds, queueNodes;
double currentDraw, currentState, currentTri, currentVbo, currentFiller, currentQueue;
double currentTexturePrepare, currentTextureMs;

// The worst frames, with their breakdown.
struct SlowFrame
{
    double total, sim, render, raster, swap, gpu, draws;
    int index;
};
std::vector<SlowFrame> slowFrames;

enum Phase { PH_MENU, PH_WAIT, PH_SETTLE, PH_RUN, PH_DONE };
Phase phase = PH_MENU;
int frames = 0;
int runFrames = 0;
double frameStart = 0.0;
double limiterStart = 0.0;
bool decalsPlaced = false;

NC_STACK_ypaworld *World_() { return Gui::UA::yw; }

void PlaceDecals(NC_STACK_ypaworld *w)
{
    if (!w->_viewerBact || cfg.decals == 0)
        return;

    World::TChainFXConfig fx;
    for (const World::TChainFXConfig &entry : w->_weaponProtos[38].chain_fx)
        if (entry.mode == World::TChainFXConfig::MODE_GROUND_DECAL)
            fx = entry;
    if (fx.mode != World::TChainFXConfig::MODE_GROUND_DECAL)
        return;

    fx.trigger = World::TChainFXConfig::TRIGGER_IMPACT_WORLD;
    fx.ground_decal_permanent = true;
    fx.ground_decal_size_min = fx.ground_decal_size_max = 220;
    // Half of them smoke, which is the authored worst case for the effects.
    const vec3d around = w->_viewerBact->_position;

    // A negative count piles them all on one spot: the worst case for fill rate.
    const bool pile = cfg.decals < 0;
    const int wanted = pile ? -cfg.decals : cfg.decals;

    int placed = 0;
    for (int ring = 0; ring < 40 && placed < wanted; ++ring)
    {
        for (int step = 0; step < 24 && placed < wanted; ++step)
        {
            const double angle = pile ? 0.0 : step * 0.2617993878 + ring * 0.11;
            const double radius = pile ? 300.0 : 200.0 + ring * 190.0;
            const vec3d target(around.x + std::cos(angle) * radius, around.y,
                               around.z + std::sin(angle) * radius);

            ypaworld_arg136 hit;
            for (double y = target.y - 1500.0; y < target.y + 1500.0 && !hit.isect; y += 200.0)
            {
                hit = ypaworld_arg136();
                hit.stPos = vec3d(target.x, y, target.z);
                hit.vect = vec3d(0.0, 200.0, 0.0);
                hit.flags = 0;
                w->ypaworld_func136(&hit);
            }
            if (!hit.isect)
                continue;

            fx.ground_decal_smoke.enabled = (placed % 2) == 0;
            if (w->SpawnGroundDecal(fx, hit))
                ++placed;
        }
    }
    fprintf(logFile, "decal piazzati=%d fumi=%d\n", placed, (int)w->_groundDecalSmoke.size());
    fflush(logFile);
}

void Report()
{
    fprintf(logFile, "\n=== SCENARIO livello=%d frame=%d decal=%d modo=%d sweep=%d\n",
            cfg.level, cfg.frames, cfg.decals, cfg.mode, cfg.sweep);
    fprintf(logFile, "%-24s %8s %8s %8s %8s\n", "fase (ms)", "media", "p50", "p99", "max");
    for (int i = 0; i < S_COUNT; ++i)
    {
        if (stats[i].samples.empty())
            continue;
        fprintf(logFile, "%-24s %8.2f %8.2f %8.2f %8.2f\n", stageName[i],
                stats[i].Mean(), stats[i].Percentile(0.5), stats[i].Percentile(0.99),
                stats[i].Max());
    }

    fprintf(logFile, "\n%-24s %8s %8s %8s\n", "per frame", "media", "p99", "max");
    struct { const char *name; Stat *stat; } counters[] = {
        {"mesh disegnate (draw call)", &queueNodes},
        {"upload VBO", &vboUploads}, {"ricostruz. raccordi", &fillerRebuilds}};
    for (const auto &counter : counters)
        fprintf(logFile, "%-24s %8.0f %8.0f %8.0f\n", counter.name, counter.stat->Mean(),
                counter.stat->Percentile(0.99), counter.stat->Max());

    const double budget60 = 1000.0 / 60.0;
    int over60 = 0, over30 = 0;
    for (double v : stats[S_FRAME].samples)
    {
        if (v > budget60) ++over60;
        if (v > 1000.0 / 30.0) ++over30;
    }
    fprintf(logFile, "\nframe oltre 16.7 ms: %d su %d (%.1f%%); oltre 33.3 ms: %d\n",
            over60, (int)stats[S_FRAME].samples.size(),
            stats[S_FRAME].samples.empty() ? 0.0 :
                100.0 * over60 / (double)stats[S_FRAME].samples.size(), over30);

    std::sort(slowFrames.begin(), slowFrames.end(),
              [](const SlowFrame &a, const SlowFrame &b) { return a.total > b.total; });
    fprintf(logFile, "\npeggiori frame (ms): %-8s %-8s %-8s %-8s %-8s %-8s %s\n",
            "totale", "sim", "lista", "invio", "scambio", "gpu", "disegni");
    for (size_t i = 0; i < slowFrames.size() && i < 10; ++i)
    {
        const SlowFrame &f = slowFrames[i];
        fprintf(logFile, "              #%-6d %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %-8.2f %.0f\n",
                f.index, f.total, f.sim, f.render, f.raster, f.swap, f.gpu, f.draws);
    }
    fflush(logFile);
}

void BeginFrameStats()
{
    for (int i = 0; i < S_COUNT; ++i)
        frameStage[i] = 0.0;
    currentDraw = currentState = currentTri = currentVbo = currentFiller = currentQueue = 0.0;
    currentTexturePrepare = currentTextureMs = 0.0;
}

void EndFrameStats()
{
    if (phase != PH_RUN)
        return;

    stats[S_FRAME].Add(frameStage[S_FRAME]);
    for (int i = S_SIM; i < S_COUNT; ++i)
        if (frameStage[i] > 0.0)
            stats[i].Add(frameStage[i]);

    drawCalls.Add(currentDraw);
    stateChanges.Add(currentState);
    triangles.Add(currentTri);
    vboUploads.Add(currentVbo);
    fillerRebuilds.Add(currentFiller);
    queueNodes.Add(currentQueue);

    if (frameStage[S_FRAME] > 16.0 && logFile)
    {
        NC_STACK_ypaworld *w = World_();
        fprintf(logFile, "FRAME LENTO #%d %.1f ms | gui_sw=%.1f gui_hw=%.1f post=%.1f "
                         "raster=%.1f lista=%.1f vbo=%.0f raccordi=%.0f settore=%d,%d\n",
                runFrames, frameStage[S_FRAME], frameStage[S_GUI_SW], frameStage[S_GUI_HW],
                frameStage[S_UI], frameStage[S_RASTER_CPU], frameStage[S_RENDER_CPU],
                currentVbo, currentFiller, currentTexturePrepare, currentTextureMs,
                w && w->_viewerBact ? w->_viewerBact->_cellId.x : -1,
                w && w->_viewerBact ? w->_viewerBact->_cellId.y : -1);
    }

    SlowFrame slow;
    slow.total = frameStage[S_FRAME];
    slow.sim = frameStage[S_SIM];
    slow.render = frameStage[S_RENDER_CPU];
    slow.raster = frameStage[S_RASTER_CPU];
    slow.swap = frameStage[S_SWAP];
    slow.gpu = frameStage[S_GPU];
    slow.draws = currentDraw;
    slow.index = runFrames;
    slowFrames.push_back(slow);
    if (slowFrames.size() > 400)
    {
        std::sort(slowFrames.begin(), slowFrames.end(),
                  [](const SlowFrame &a, const SlowFrame &b) { return a.total > b.total; });
        slowFrames.resize(200);
    }
}

}

// ------------------------------------------------------------------ wraps ---
extern "C" {

void __real__ZN5Input9INPEngine10QueryInputEP11TInputState(Input::INPEngine *, TInputState *);
int __real__ZN15NC_STACK_bitmap14PrepareTextureEb(void *, bool);

// Counts textures uploaded to the graphics card during a frame.
int __wrap__ZN15NC_STACK_bitmap14PrepareTextureEb(void *bitmap, bool flag)
{
    const double start = Now();
    const int result = __real__ZN15NC_STACK_bitmap14PrepareTextureEb(bitmap, flag);
    currentTexturePrepare += 1.0;
    currentTextureMs += Now() - start;
    return result;
}
void __real__ZN3Gui4Root4DrawEP11SDL_Surface(Gui::Root *, SDL_Surface *);
void __real__ZN3Gui4Root9HwComposeEv(Gui::Root *);

// Flying the player is the only way to make the engine stream new sectors,
// which is where a legacy engine usually stutters.
void __wrap__ZN5Input9INPEngine10QueryInputEP11TInputState(Input::INPEngine *engine, TInputState *state)
{
    __real__ZN5Input9INPEngine10QueryInputEP11TInputState(engine, state);
    if (phase == PH_RUN && cfg.sweep == 2 && state)
    {
        state->Sliders[2] = 1.0f;                                   // full throttle
        state->Sliders[0] = (float)std::sin(runFrames * 0.004) * 0.35f;  // long turns
    }
}

void __wrap__ZN3Gui4Root4DrawEP11SDL_Surface(Gui::Root *root, SDL_Surface *surface)
{
    const double start = Now();
    __real__ZN3Gui4Root4DrawEP11SDL_Surface(root, surface);
    frameStage[S_GUI_SW] += Now() - start;
}

void __wrap__ZN3Gui4Root9HwComposeEv(Gui::Root *root)
{
    const double start = Now();
    __real__ZN3Gui4Root9HwComposeEv(root);
    frameStage[S_GUI_HW] += Now() - start;
}

bool __real__ZN6System13ProcessEventsEv();
void __real__ZN6System4FlipEv();
void __real__ZN6System4InitEb(bool);
void __real__ZN17NC_STACK_ypaworld16ProcessGameShellEv(NC_STACK_ypaworld *);
void __real__ZN17NC_STACK_ypaworld7ProcessEP10base_64arg(NC_STACK_ypaworld *, base_64arg *);
void __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(NC_STACK_ypaworld *, base_64arg *, int);
void __real__ZN3GFX9GFXEngine9RasterizeEj(GFX::GFXEngine *, uint32_t);
void __real__ZN3GFX9GFXEngine8EndFrameEv(GFX::GFXEngine *);
void __real__ZN6System10FramePacer8EndFrameEv(System::FramePacer *);
void __real__ZN3GFX9GFXEngine10RenderNodeEPNS_11TRenderNodeE(GFX::GFXEngine *, GFX::TRenderNode *);
void __real__ZN3GFX9GFXEngine15SetRenderStatesEi(GFX::GFXEngine *, int);
void __real__ZN3GFX9GFXEngine15QueueRenderMeshEPNS_11TRenderNodeE(GFX::GFXEngine *, GFX::TRenderNode *);
void __real__ZN3GFX9GFXEngine11MeshMakeVBOEPNS_5TMeshE(GFX::GFXEngine *, GFX::TMesh *);
void __real__ZN17NC_STACK_ypaworld12RenderSectorEP16TRenderingSectorP14baseRender_msg(NC_STACK_ypaworld *, TRenderingSector *, baseRender_msg *);
void __real__ZN17NC_STACK_ypaworld13RenderFillersEP14baseRender_msg(NC_STACK_ypaworld *, baseRender_msg *);
void __real__ZN17NC_STACK_ypaworld18RenderGroundDecalsEP14baseRender_msg(NC_STACK_ypaworld *, baseRender_msg *);
void __real__ZN17NC_STACK_ypaworld22RenderGroundDecalSmokeEP14baseRender_msg(NC_STACK_ypaworld *, baseRender_msg *);
void __real__ZN5World14ParticleSystem12UpdateRenderEP11area_arg_65i(World::ParticleSystem *, area_arg_65 *, int);
void __real__ZN17NC_STACK_ypaworld13PrepareFillerEP8cellAreaS1_ffbP13TCellFillerChb(NC_STACK_ypaworld *, cellArea *, cellArea *, float, float, bool, TCellFillerCh *, bool);

void __wrap__ZN6System4InitEb(bool oldGL)
{
    LoadConfig();
    if (cfg.mode == 1)
        System::IniConf::GfxColorEffects.Value = (int32_t)0;
    else if (cfg.mode == 2)
    {
        System::IniConf::GfxVBO.Value = false;
        oldGL = true;
    }
    else if (cfg.mode == 3)
    {
        System::IniConf::GfxVsync.Value = (int32_t)0;
        System::IniConf::GfxMaxFps.Value = (int32_t)1000;
    }
    else if (cfg.mode == 4)   // vsync on, single refresh
        System::IniConf::GfxVsync.Value = (int32_t)1;
    __real__ZN6System4InitEb(oldGL);
}

// Frame boundary: everything between two of these calls is one frame.
bool __wrap__ZN6System13ProcessEventsEv()
{
    if (!logFile)
    {
        LoadConfig();
        logFile = fopen((outDir + "/perf.log").c_str(), "w");
        InitGpuQueries();
        const Common::Point screen = System::GetResolution();
        const char *renderer = (const char *)glGetString(GL_RENDERER);
        const char *vendor = (const char *)glGetString(GL_VENDOR);
        const char *version = (const char *)glGetString(GL_VERSION);
        SDL_DisplayMode mode;
        const int refresh = SDL_GetCurrentDisplayMode(0, &mode) == 0 ? mode.refresh_rate : 0;
        fprintf(logFile, "GPU in uso: %s | %s | GL %s\n", renderer ? renderer : "?",
                vendor ? vendor : "?", version ? version : "?");
        fprintf(logFile, "schermo %dx%d a %d Hz | vsync ini=%d, swap interval reale=%d | maxfps=%d"
                         " | vbo=%d color_effects=%d query GPU=%d\n",
                screen.x, screen.y, refresh, System::IniConf::GfxVsync.Get<int32_t>(),
                SDL_GL_GetSwapInterval(), System::IniConf::GfxMaxFps.Get<int32_t>(),
                System::IniConf::GfxVBO.Get<bool>() ? 1 : 0,
                System::IniConf::GfxColorEffects.Get<int32_t>(), gpuReady ? 1 : 0);
        fflush(logFile);
    }

    NC_STACK_ypaworld *w = World_();
    ++frames;

    if (phase == PH_RUN)
    {
        frameStage[S_FRAME] = Now() - frameStart;

        // Read back a GPU query from three frames ago: never stalls the CPU.
        if (gpuReady)
        {
            const int slot = (gpuSlot + 1) % QUERY_SLOTS;
            if (gpuQueryBusy[slot])
            {
                GLuint available = 0;
                glGetQueryObjectuiv_(gpuQuery[slot], GL_QUERY_RESULT_AVAILABLE, &available);
                if (available)
                {
                    GLuint64 elapsed = 0;
                    glGetQueryObjectui64v_(gpuQuery[slot], GL_QUERY_RESULT, &elapsed);
                    frameStage[S_GPU] = (double)elapsed / 1000000.0;
                    gpuQueryBusy[slot] = false;
                }
            }
        }

        EndFrameStats();
        if (++runFrames >= cfg.frames)
        {
            Report();
            fclose(logFile);
            _exit(0);
        }
    }

    switch (phase)
    {
    case PH_WAIT:
        if (w && w->_viewerBact)
        {
            phase = PH_SETTLE;
            frames = 0;
        }
        break;
    case PH_SETTLE:
        if (frames > 150)   // Let streaming and the AI settle before measuring.
        {
            if (w)
                w->_hideHudForScreenshots = false;
            if (!decalsPlaced)
            {
                decalsPlaced = true;
                PlaceDecals(w);
            }
            phase = PH_RUN;
            runFrames = 0;
        }
        break;
    default:
        break;
    }

    BeginFrameStats();
    frameStart = Now();
    return __real__ZN6System13ProcessEventsEv();
}

void __wrap__ZN17NC_STACK_ypaworld16ProcessGameShellEv(NC_STACK_ypaworld *w)
{
    __real__ZN17NC_STACK_ypaworld16ProcessGameShellEv(w);
    if (phase == PH_MENU && frames > 60 && w->_GameShell)
    {
        w->_GameShell->envAction.action = EnvAction::ACTION_PLAY;
        w->_GameShell->envAction.params[0] = cfg.level;
        w->_GameShell->envAction.params[1] = cfg.level;
        phase = PH_WAIT;
        frames = 0;
    }
}

void __wrap__ZN17NC_STACK_ypaworld7ProcessEP10base_64arg(NC_STACK_ypaworld *w, base_64arg *arg)
{
    // Turning the camera is what forces new sectors to be prepared.
    if (phase == PH_RUN && cfg.sweep == 1 && w->_viewerBact)
    {
        TF::TForm3D *view = TF::Engine.GetViewPoint();
        if (view)
            view->ay += 14;
    }

    const double start = Now();
    __real__ZN17NC_STACK_ypaworld7ProcessEP10base_64arg(w, arg);
    // Everything but the rendering already measured inside it.
    frameStage[S_SIM] = (Now() - start) - frameStage[S_RENDER_CPU] -
                        frameStage[S_RASTER_CPU] - frameStage[S_SWAP] - frameStage[S_UI];
}

void __wrap__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(NC_STACK_ypaworld *w, base_64arg *arg, int a2)
{
    if (gpuReady && phase == PH_RUN)
    {
        gpuSlot = (gpuSlot + 1) % QUERY_SLOTS;
        if (!gpuQueryBusy[gpuSlot])
        {
            glBeginQuery_(GL_TIME_ELAPSED, gpuQuery[gpuSlot]);
            gpuQueryBusy[gpuSlot] = true;
        }
        else
            gpuSlot = (gpuSlot + QUERY_SLOTS - 1) % QUERY_SLOTS;
    }

    const double start = Now();
    __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(w, arg, a2);
    frameStage[S_RENDER_CPU] = (Now() - start) - frameStage[S_RASTER_CPU];
}

void __wrap__ZN3GFX9GFXEngine9RasterizeEj(GFX::GFXEngine *engine, uint32_t stages)
{
    const double start = Now();
    __real__ZN3GFX9GFXEngine9RasterizeEj(engine, stages);
    frameStage[S_RASTER_CPU] += Now() - start;
}

void __wrap__ZN3GFX9GFXEngine8EndFrameEv(GFX::GFXEngine *engine)
{
    const double start = Now();
    __real__ZN3GFX9GFXEngine8EndFrameEv(engine);
    frameStage[S_UI] = (Now() - start) - frameStage[S_SWAP];
}

// The swap is where the wait for the screen happens: measured on its own.
void __wrap__ZN6System4FlipEv()
{
    if (gpuReady && phase == PH_RUN && gpuQueryBusy[gpuSlot])
        glEndQuery_(GL_TIME_ELAPSED);

    const double start = Now();
    __real__ZN6System4FlipEv();
    frameStage[S_SWAP] = Now() - start;
}

// The limiter sleep is not part of the frame cost: measured apart.
void __wrap__ZN6System10FramePacer8EndFrameEv(System::FramePacer *pacer)
{
    const double start = Now();
    __real__ZN6System10FramePacer8EndFrameEv(pacer);
    if (phase == PH_RUN)
        stats[S_LIMITER].Add(Now() - start);
}

void __wrap__ZN3GFX9GFXEngine10RenderNodeEPNS_11TRenderNodeE(GFX::GFXEngine *engine, GFX::TRenderNode *node)
{
    if (node && node->Mesh)
    {
        currentDraw += 1.0;
        currentTri += (double)(node->Mesh->Indixes.size() / 3);
    }
    __real__ZN3GFX9GFXEngine10RenderNodeEPNS_11TRenderNodeE(engine, node);
}

void __wrap__ZN3GFX9GFXEngine15SetRenderStatesEi(GFX::GFXEngine *engine, int setAll)
{
    currentState += 1.0;
    __real__ZN3GFX9GFXEngine15SetRenderStatesEi(engine, setAll);
}

void __wrap__ZN3GFX9GFXEngine15QueueRenderMeshEPNS_11TRenderNodeE(GFX::GFXEngine *engine, GFX::TRenderNode *node)
{
    currentQueue += 1.0;
    __real__ZN3GFX9GFXEngine15QueueRenderMeshEPNS_11TRenderNodeE(engine, node);
}

void __wrap__ZN3GFX9GFXEngine11MeshMakeVBOEPNS_5TMeshE(GFX::GFXEngine *engine, GFX::TMesh *mesh)
{
    currentVbo += 1.0;
    __real__ZN3GFX9GFXEngine11MeshMakeVBOEPNS_5TMeshE(engine, mesh);
}

void __wrap__ZN17NC_STACK_ypaworld12RenderSectorEP16TRenderingSectorP14baseRender_msg(NC_STACK_ypaworld *w, TRenderingSector *sector, baseRender_msg *arg)
{
    const double start = Now();
    __real__ZN17NC_STACK_ypaworld12RenderSectorEP16TRenderingSectorP14baseRender_msg(w, sector, arg);
    frameStage[S_SECTORS] += Now() - start;
}

void __wrap__ZN17NC_STACK_ypaworld13RenderFillersEP14baseRender_msg(NC_STACK_ypaworld *w, baseRender_msg *arg)
{
    const double start = Now();
    __real__ZN17NC_STACK_ypaworld13RenderFillersEP14baseRender_msg(w, arg);
    frameStage[S_FILLERS] += Now() - start;
}

void __wrap__ZN17NC_STACK_ypaworld18RenderGroundDecalsEP14baseRender_msg(NC_STACK_ypaworld *w, baseRender_msg *arg)
{
    const double start = Now();
    __real__ZN17NC_STACK_ypaworld18RenderGroundDecalsEP14baseRender_msg(w, arg);
    frameStage[S_DECALS] += Now() - start;
}

void __wrap__ZN17NC_STACK_ypaworld22RenderGroundDecalSmokeEP14baseRender_msg(NC_STACK_ypaworld *w, baseRender_msg *arg)
{
    const double start = Now();
    __real__ZN17NC_STACK_ypaworld22RenderGroundDecalSmokeEP14baseRender_msg(w, arg);
    frameStage[S_SMOKE] += Now() - start;
}

void __wrap__ZN5World14ParticleSystem12UpdateRenderEP11area_arg_65i(World::ParticleSystem *system, area_arg_65 *arg, int delta)
{
    const double start = Now();
    __real__ZN5World14ParticleSystem12UpdateRenderEP11area_arg_65i(system, arg, delta);
    frameStage[S_PARTICLES] += Now() - start;
}

void __wrap__ZN17NC_STACK_ypaworld13PrepareFillerEP8cellAreaS1_ffbP13TCellFillerChb(NC_STACK_ypaworld *w, cellArea *a, cellArea *b, float h1, float h2, bool vertical, TCellFillerCh *out, bool force)
{
    const double before = currentVbo;
    __real__ZN17NC_STACK_ypaworld13PrepareFillerEP8cellAreaS1_ffbP13TCellFillerChb(w, a, b, h1, h2, vertical, out, force);
    if (currentVbo > before)
        currentFiller += 1.0;
}

}
