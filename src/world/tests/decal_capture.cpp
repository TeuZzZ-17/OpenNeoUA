// Capture driver for the projected decals (not part of the game build). It is
// linked only into a test executable with ld --wrap, so the game stays untouched:
//   - starts a real level from the menu;
//   - points a free camera at real buildings, power stations and terrain;
//   - casts rays on the real collision and spawns decals through the real API;
//   - saves what the real renderer drew on the real GPU.
//
// Build: compile with the game flags plus -fno-access-control and
// -DCAPTURE_DIR="<folder>", then link it BEFORE the game objects (main.cpp.obj
// included) with --wrap on _ZN6System4FlipEv, _ZN6System13ProcessEventsEv,
// _ZN6System4InitEb, _ZN3GFX9GFXEngine9RasterizeEj,
// _ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi and
// _ZN17NC_STACK_ypaworld16ProcessGameShellEv.
// Run from "UA Rising". <folder>/../cap_cfg.txt holds "level mode plan":
// mode 0 = default, 1 = no scene FBO, 2 = legacy GL (gfx.vbo off);
// plan "stations" = every power station type, "all" = also city, units, stress,
// "taer" = the real decal block of Weapon 38 (20 Tons Taerbomb) dropped from above.
// Needs Data/Interface/GroundDecals/_capture_checker.png (any checkerboard): the
// checker pass is what shows stretching. Back up Data/Save first: starting a
// level from the shell rewrites the player files. Remove the PNG afterwards.
// Use a build folder of your own: objects left by another build of a changed
// header make the executable crash in unrelated places.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/uacommon.h"
#include "system/gfx.h"
#include "system/inivals.h"
#include "system/system.h"
#include "system/tform.h"
#include "world/grounddecal_profile.h"


namespace
{

struct Shot
{
    std::string name;
    Common::Point cell;
    vec3d eye;
    vec3d target;
    float size = 120.0f;
    int columns = 5;
    int rows = 4;
    bool drop = false;      // One real bomb falling on dropPos instead of the ray grid.
    vec3d dropPos;
};

enum Stage { STAGE_MENU, STAGE_WAIT_LEVEL, STAGE_SETTLE, STAGE_RUN, STAGE_DONE };

Stage stage = STAGE_MENU;
int frames = 0;
std::vector<Shot> shots;
size_t shotIndex = 0;
int pass = 0;
int passFrames = 0;
bool capturePending = false;
std::string outDir;
FILE *logFile = nullptr;
TF::TForm3D camera;
int cfgLevel = 1;
int cfgMode = 0;
std::string cfgPlan = "stations";
bool cfgLoaded = false;

void LoadConfig()
{
    if (cfgLoaded)
        return;
    cfgLoaded = true;
    char plan[64] = {0};
    FILE *file = fopen(CAPTURE_DIR "/../cap_cfg.txt", "r");
    if (file)
    {
        if (fscanf(file, "%d %d %63s", &cfgLevel, &cfgMode, plan) == 3)
            cfgPlan = plan;
        fclose(file);
    }
}
double rasterMs[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
int rasterCount[5] = {0, 0, 0, 0, 0};

NC_STACK_ypaworld *World_() { return Gui::UA::yw; }

mat3x3 LookAt(const vec3d &eye, const vec3d &target)
{
    vec3d forward = target - eye;
    forward.normalise();
    vec3d right = vec3d(0.0, 1.0, 0.0) * forward;
    right.normalise();
    const vec3d down = forward * right;

    mat3x3 out;
    out.m00 = right.x;   out.m01 = right.y;   out.m02 = right.z;
    out.m10 = down.x;    out.m11 = down.y;    out.m12 = down.z;
    out.m20 = forward.x; out.m21 = forward.y; out.m22 = forward.z;
    return out;
}

void AddOrbit(const std::string &name, NC_STACK_ypaworld *w, const Common::Point &cell, float size,
              float distance, float height, float aimHeight)
{
    const cellArea &area = w->_cells(cell);
    const vec3d center = area.CenterPos;
    static const double angles[3][2] = {{1.0, 0.6}, {-0.8, 0.9}, {0.5, -1.0}};
    for (int view = 0; view < (cfgPlan == "all" ? 3 : 2); ++view)
    {
        Shot shot;
        shot.name = name + "_v" + std::to_string(view);
        shot.cell = cell;
        vec3d dir(angles[view][0], 0.0, angles[view][1]);
        dir.normalise();
        shot.eye = vec3d(center.x + dir.x * distance, area.height - height, center.z + dir.z * distance);
        shot.target = vec3d(center.x, area.height - aimHeight, center.z);
        shot.size = size;
        shots.push_back(shot);
    }
}

// Real weapon decal, dropped like the bomb falls: high views plus one from the ground.
void AddBomb(const std::string &name, NC_STACK_ypaworld *w, const Common::Point &cell, double offsetX)
{
    const cellArea &area = w->_cells(cell);
    const vec3d center = area.CenterPos;
    static const double views[3][4] = {{1.0, 0.6, 1750.0, 1150.0},
                                       {-0.8, 0.9, 1750.0, 1150.0},
                                       {0.7, -1.0, 1250.0, 170.0}};
    for (int view = 0; view < 3; ++view)
    {
        Shot shot;
        shot.name = name + "_v" + std::to_string(view);
        shot.cell = cell;
        vec3d dir(views[view][0], 0.0, views[view][1]);
        dir.normalise();
        shot.eye = vec3d(center.x + dir.x * views[view][2], area.height - views[view][3],
                         center.z + dir.z * views[view][2]);
        shot.target = vec3d(center.x, area.height - (view == 2 ? 120.0 : 60.0), center.z);
        shot.drop = true;
        shot.dropPos = vec3d(center.x + offsetX, area.height, center.z);
        shots.push_back(shot);
    }
}

void PlanBombShots(NC_STACK_ypaworld *w)
{
    bool station = false;
    int city = 0;
    int slope = 0;

    for (int y = 2; y < w->_mapSize.y - 2; ++y)
    {
        for (int x = 2; x < w->_mapSize.x - 2; ++x)
        {
            const Common::Point id(x, y);
            const cellArea &area = w->_cells(id);
            const float step = std::fabs(area.height - w->_cells(x + 1, y).height) +
                               std::fabs(area.height - w->_cells(x, y + 1).height);

            if (!station && area.PurposeType == cellArea::PT_POWERSTATION && area.type_id == 201)
            {
                station = true;
                AddBomb("taer_ps201_centro", w, id, 0.0);
                AddBomb("taer_ps201_fianco", w, id, 430.0);
            }
            if (city < 2 && area.PurposeType == cellArea::PT_NONE && area.SectorType != 1 &&
                step < 1.0f && ((x * 5 + y * 3) % 11) == 0)
            {
                ++city;
                AddBomb("taer_city" + std::to_string(city), w, id, 0.0);
            }
            if (slope < 1 && step > 150.0f && ((x * 7 + y) % 4) == 0)
            {
                ++slope;
                AddBomb("taer_slope", w, id, 0.0);
            }
        }
    }
    // Tallest buildings under the sector centre: the bomb lands on a real roof.
    std::vector<std::pair<double, Common::Point>> tall;
    for (int y = 2; y < w->_mapSize.y - 2; ++y)
    {
        for (int x = 2; x < w->_mapSize.x - 2; ++x)
        {
            const cellArea &area = w->_cells(x, y);
            if (area.PurposeType != cellArea::PT_NONE)
                continue;
            ypaworld_arg136 hit;
            for (double top = area.height - 2600.0; top < area.height + 800.0 && !hit.isect; top += 200.0)
            {
                hit = ypaworld_arg136();
                hit.stPos = vec3d(area.CenterPos.x, top, area.CenterPos.z);
                hit.vect = vec3d(0.0, 200.0, 0.0);
                hit.flags = 0;
                w->ypaworld_func136(&hit);
            }
            const double height = hit.isect ? area.height - hit.isectPos.y : 0.0;
            if (height > 150.0 && height < 900.0)
                tall.push_back(std::make_pair(height, Common::Point(x, y)));
        }
    }
    std::sort(tall.begin(), tall.end(), [](const std::pair<double, Common::Point> &a,
                                           const std::pair<double, Common::Point> &b)
              { return a.first > b.first; });
    if (!tall.empty())
    {
        AddBomb("taer_torre_alta", w, tall.front().second, 0.0);
        AddBomb("taer_torre_media", w, tall[tall.size() / 2].second, 0.0);
        fprintf(logFile, "tall buildings=%d highest=%.0f middle=%.0f\n", (int)tall.size(),
                tall.front().first, tall[tall.size() / 2].first);
    }

    fprintf(logFile, "map %dx%d shots=%d decalProg=%u\n", w->_mapSize.x, w->_mapSize.y,
            (int)shots.size(), GFX::Engine._decalProg.ID);
    fflush(logFile);
}

void PlanShots(NC_STACK_ypaworld *w)
{
    if (cfgPlan == "taer")
    {
        PlanBombShots(w);
        return;
    }

    std::vector<int> stationTypes;
    int plain = 0;
    int city = 0;

    for (int y = 1; y < w->_mapSize.y - 1; ++y)
    {
        for (int x = 1; x < w->_mapSize.x - 1; ++x)
        {
            const Common::Point id(x, y);
            const cellArea &area = w->_cells(id);
            const float step = std::fabs(area.height - w->_cells(x + 1, y).height) +
                               std::fabs(area.height - w->_cells(x, y + 1).height);

            if (area.PurposeType == cellArea::PT_POWERSTATION &&
                std::find(stationTypes.begin(), stationTypes.end(), (int)area.type_id) == stationTypes.end())
            {
                stationTypes.push_back(area.type_id);
                const std::string name = "ps" + std::to_string((int)area.type_id);
                AddOrbit(name, w, id, 170.0f, 820.0f, 430.0f, 90.0f);
                AddOrbit(name + "small", w, id, 60.0f, 700.0f, 380.0f, 90.0f);
                fprintf(logFile, "station type_id=%d cell=%d,%d owner=%d height=%.0f set=%d\n",
                        (int)area.type_id, x, y, (int)area.owner, area.height, w->_setId);
            }

            if (cfgPlan == "all" && city < 2 && area.PurposeType == cellArea::PT_NONE &&
                area.SectorType != 1 && step < 1.0f && ((x * 5 + y * 3) % 11) == 0)
            {
                ++city;
                AddOrbit("city" + std::to_string(city), w, id, 200.0f, 800.0f, 420.0f, 60.0f);
            }

            if (cfgPlan == "all" && plain < 1 && area.SectorType == 1 && step < 1.0f)
            {
                ++plain;
                AddOrbit("plain", w, id, 180.0f, 620.0f, 360.0f, 0.0f);
            }
        }
    }

    // Decals around real ground units: the units must stay clean.
    int units = cfgPlan == "all" ? 0 : 3;
    for (int y = 1; y < w->_mapSize.y - 1 && units < 3; ++y)
    {
        for (int x = 1; x < w->_mapSize.x - 1 && units < 3; ++x)
        {
            for (NC_STACK_ypabact *bact : w->_cells(x, y).unitsList)
            {
                if ((bact->_bact_type != BACT_TYPES_TANK && bact->_bact_type != BACT_TYPES_CAR) ||
                    bact->_status == BACT_STATUS_DEAD)
                    continue;

                Shot shot;
                shot.name = "unit" + std::to_string(++units);
                shot.cell = Common::Point(x, y);
                shot.target = bact->_position;
                shot.eye = bact->_position + vec3d(190.0, -170.0, 150.0);
                shot.size = 240.0f;
                shots.push_back(shot);
                break;
            }
        }
    }

    // Worst case of the default limit: 256 decals on screen at once.
    if (plain > 0)
    {
        for (const Shot &shot : shots)
        {
            if (shot.name == "plain_v0")
            {
                Shot stress = shot;
                stress.name = "stress256";
                stress.columns = 16;
                stress.rows = 16;
                stress.size = 90.0f;
                shots.push_back(stress);
                break;
            }
        }
    }

    fprintf(logFile, "map %dx%d shots=%d decalProg=%u\n", w->_mapSize.x, w->_mapSize.y,
            (int)shots.size(), GFX::Engine._decalProg.ID);
    fflush(logFile);
}

// Pass 0: no decals. Pass 1: real image. Pass 2: checkerboard, shows any stretch.
std::vector<std::pair<int, int>> savedHealth;

// Pass 3 and 4 keep the decals of pass 2 and only change the building state.
void SetSectorHealth(NC_STACK_ypaworld *w, const Shot &shot, int health)
{
    cellArea &area = w->_cells(shot.cell);
    savedHealth.clear();
    int index = 0;
    for (int16_t &value : area.buildings_health)
    {
        savedHealth.push_back(std::make_pair(index++, value));
        value = health;
    }
}

void RestoreSectorHealth(NC_STACK_ypaworld *w, const Shot &shot)
{
    cellArea &area = w->_cells(shot.cell);
    size_t index = 0;
    for (int16_t &value : area.buildings_health)
    {
        if (index < savedHealth.size())
            value = savedHealth[index].second;
        ++index;
    }
}

void PrepareDecals(NC_STACK_ypaworld *w, const Shot &shot, int currentPass)
{
    if (currentPass == 3)
    {
        SetSectorHealth(w, shot, 60);
        return;
    }
    if (currentPass == 4)
    {
        RestoreSectorHealth(w, shot);
        SetSectorHealth(w, shot, 0);
        return;
    }

    w->ClearGroundDecals();
    if (currentPass == 0)
        return;

    World::TChainFXConfig fx;
    fx.mode = World::TChainFXConfig::MODE_GROUND_DECAL;
    fx.trigger = World::TChainFXConfig::TRIGGER_IMPACT_WORLD;
    fx.ground_decal_texture = currentPass == 1 ? "Interface/GroundDecals/Impact_01.png"
                                               : "Interface/GroundDecals/_capture_checker.png";
    fx.ground_decal_permanent = true;
    fx.ground_decal_size_min = fx.ground_decal_size_max = shot.size;
    fx.ground_decal_tint = World::TVisualTint();
    fx.ground_decal_random_rotation = false;
    fx.ground_decal_edge_fade = 3.0f;
    fx.duration = 100000;

    if (shot.drop)
    {
        for (const World::TChainFXConfig &entry : w->_weaponProtos[38].chain_fx)
        {
            if (entry.mode == World::TChainFXConfig::MODE_GROUND_DECAL &&
                entry.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD)
            {
                fx = entry;
                break;
            }
        }
        fx.ground_decal_smoke.enabled = false;  // Smoke would only hide the decal.
        if (currentPass == 2)
            fx.ground_decal_texture = "Interface/GroundDecals/_capture_checker.png";

        ypaworld_arg136 hit;
        for (double y = shot.dropPos.y - 2600.0; y < shot.dropPos.y + 800.0 && !hit.isect; y += 200.0)
        {
            hit = ypaworld_arg136();
            hit.stPos = vec3d(shot.dropPos.x, y, shot.dropPos.z);
            hit.vect = vec3d(0.0, 200.0, 0.0);
            hit.flags = 0;
            w->ypaworld_func136(&hit);
        }

        const bool ok = hit.isect && w->SpawnGroundDecal(fx, hit);
        if (ok)
        {
            const auto &decal = w->_groundDecals.back();
            fprintf(logFile, "%s pass=%d texture=%s size_cfg=%.0f~%.0f size=%.0fx%.0f hit=(%.0f %.0f %.0f) n=(%.2f %.2f %.2f) ground=%.0f\n",
                    shot.name.c_str(), currentPass, fx.ground_decal_texture.c_str(),
                    fx.ground_decal_size_min, fx.ground_decal_size_max, decal.sizeT, decal.sizeB,
                    hit.isectPos.x, hit.isectPos.y, hit.isectPos.z,
                    decal.axisN.x, decal.axisN.y, decal.axisN.z, shot.dropPos.y);
        }
        else
            fprintf(logFile, "%s pass=%d NO DECAL isect=%d\n", shot.name.c_str(), currentPass, hit.isect ? 1 : 0);
        fflush(logFile);
        return;
    }

    vec3d forward = shot.target - shot.eye;
    forward.normalise();
    vec3d right = vec3d(0.0, 1.0, 0.0) * forward;
    right.normalise();
    const vec3d down = forward * right;

    int accepted = 0, rejected = 0, missed = 0;
    for (int row = 0; row < shot.rows; ++row)
    {
        for (int column = 0; column < shot.columns; ++column)
        {
            const double sx = (column + 0.5) / shot.columns * 1.2 - 0.6;
            const double sy = (row + 0.5) / shot.rows * 1.0 - 0.5;
            vec3d dir = forward + right * sx + down * sy;
            dir.normalise();

            // The world ray test only walks short segments, like a flying shell.
            ypaworld_arg136 hit;
            for (double along = 0.0; along < 3500.0 && !hit.isect; along += 200.0)
            {
                hit = ypaworld_arg136();
                hit.stPos = shot.eye + dir * along;
                hit.vect = dir * 200.0;
                hit.flags = 0;
                w->ypaworld_func136(&hit);
            }
            if (!hit.isect)
            {
                ++missed;
                continue;
            }

            const bool ok = w->SpawnGroundDecal(fx, hit);
            ok ? ++accepted : ++rejected;
            if (currentPass == 1)
                fprintf(logFile, "  %s hit (%.1f %.1f %.1f) type=%d ok=%d n=(%.2f %.2f %.2f)\n",
                        shot.name.c_str(), hit.isectPos.x, hit.isectPos.y, hit.isectPos.z,
                        (int)hit.hitCollisionType, ok ? 1 : 0,
                        w->_groundDecals.empty() ? 0.0 : w->_groundDecals.back().axisN.x,
                        w->_groundDecals.empty() ? 0.0 : w->_groundDecals.back().axisN.y,
                        w->_groundDecals.empty() ? 0.0 : w->_groundDecals.back().axisN.z);
        }
    }
    fprintf(logFile, "%s pass=%d accepted=%d rejected=%d missed=%d\n", shot.name.c_str(),
            currentPass, accepted, rejected, missed);
    fflush(logFile);
}

void SavePixels(const std::string &path)
{
    const Common::Point res = System::GetResolution();
    std::vector<uint8_t> pixels((size_t)res.x * res.y * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, res.x, res.y, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    FILE *file = fopen(path.c_str(), "wb");
    if (!file)
        return;
    fprintf(file, "P6\n%d %d\n255\n", res.x, res.y);
    for (int y = res.y - 1; y >= 0; --y)
        fwrite(&pixels[(size_t)y * res.x * 3], 3, res.x, file);
    fclose(file);
}

void Advance()
{
    NC_STACK_ypaworld *w = World_();
    ++frames;

    switch (stage)
    {
    case STAGE_MENU:
        break;   // Handled right after the shell frame, see the wrapper below.

    case STAGE_WAIT_LEVEL:
        if (w->_viewerBact)
        {
            stage = STAGE_SETTLE;
            frames = 0;
        }
        break;

    case STAGE_SETTLE:
        if (frames > 90)
        {
            w->_hideHudForScreenshots = true;
            PlanShots(w);
            stage = shots.empty() ? STAGE_DONE : STAGE_RUN;
            shotIndex = 0;
            pass = 0;
            passFrames = 0;
        }
        break;

    case STAGE_RUN:
        if (passFrames == 0)
        {
            PrepareDecals(w, shots[shotIndex], pass);
            rasterMs[pass] = 0.0;
            rasterCount[pass] = 0;
        }
        else if (passFrames == 4)
            capturePending = true;
        else if (passFrames > (shots[shotIndex].name == "stress256" ? 64 : 5))
        {
            if (shots[shotIndex].name == "stress256" && rasterCount[pass] > 0)
                fprintf(logFile, "TIMING stress256 pass=%d raster=%.3f ms over %d frames decals=%d\n",
                        pass, rasterMs[pass] / rasterCount[pass], rasterCount[pass],
                        (int)w->_groundDecals.size());
            passFrames = -1;
            if (++pass > 4)
            {
                RestoreSectorHealth(w, shots[shotIndex]);
                pass = 0;
                if (++shotIndex >= shots.size())
                    stage = STAGE_DONE;
            }
        }
        ++passFrames;
        break;

    case STAGE_DONE:
        fprintf(logFile, "done glError=0x%x\n", (unsigned)glGetError());
        fclose(logFile);
        _exit(0);
    }
}

}

extern "C" void __real__ZN6System4FlipEv();
extern "C" void __wrap__ZN6System4FlipEv()
{
    if (capturePending)
    {
        capturePending = false;
        static const char *names[] = {"a_none", "b_decal", "c_checker", "d_damaged", "e_destroyed"};
        SavePixels(outDir + "/" + shots[shotIndex].name + "_" + names[pass] + ".ppm");
    }
    __real__ZN6System4FlipEv();
}

extern "C" bool __real__ZN6System13ProcessEventsEv();
extern "C" bool __wrap__ZN6System13ProcessEventsEv()
{
    if (!logFile)
    {
        outDir = CAPTURE_DIR;
        logFile = fopen((outDir + "/capture.log").c_str(), "w");
    }
    Advance();
    return __real__ZN6System13ProcessEventsEv();
}

extern "C" void __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(NC_STACK_ypaworld *, base_64arg *, int);
extern "C" void __wrap__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(NC_STACK_ypaworld *w, base_64arg *arg, int a2)
{
    if (stage != STAGE_RUN || shots.empty() || !w->_viewerBact)
    {
        __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(w, arg, a2);
        return;
    }

    const Shot &shot = shots[shotIndex];
    TF::TForm3D *oldView = TF::Engine.GetViewPoint();
    const Common::Point oldCell = w->_viewerBact->_cellId;
    const vec3d oldPosition = w->_viewerBact->_position;

    camera.Parent = nullptr;
    camera.flags = 0;
    camera.Pos = shot.eye;
    camera.Scale = vec3d(1.0, 1.0, 1.0);
    camera.SclRot = LookAt(shot.eye, shot.target);
    TF::Engine.SetViewPoint(&camera);
    w->_viewerBact->_cellId = shot.cell;
    w->_viewerBact->_position = shot.eye;

    __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(w, arg, a2);

    w->_viewerBact->_cellId = oldCell;
    w->_viewerBact->_position = oldPosition;
    TF::Engine.SetViewPoint(oldView);
}

// The shell clears its pending action at the start of every frame, so the level
// request must be written after the shell ran, before main.cpp reads it.
extern "C" void __real__ZN17NC_STACK_ypaworld16ProcessGameShellEv(NC_STACK_ypaworld *);
extern "C" void __wrap__ZN17NC_STACK_ypaworld16ProcessGameShellEv(NC_STACK_ypaworld *w)
{
    __real__ZN17NC_STACK_ypaworld16ProcessGameShellEv(w);
    if (stage == STAGE_MENU && frames > 60 && w->_GameShell)
    {
        w->_GameShell->envAction.action = EnvAction::ACTION_PLAY;
        w->_GameShell->envAction.params[0] = cfgLevel;
        w->_GameShell->envAction.params[1] = cfgLevel;
        stage = STAGE_WAIT_LEVEL;
        frames = 0;
    }
}

extern "C" void __real__ZN3GFX9GFXEngine9RasterizeEj(GFX::GFXEngine *, uint32_t);
extern "C" void __wrap__ZN3GFX9GFXEngine9RasterizeEj(GFX::GFXEngine *engine, uint32_t stages)
{
    if (stage != STAGE_RUN)
    {
        __real__ZN3GFX9GFXEngine9RasterizeEj(engine, stages);
        return;
    }

    glFinish();
    const uint64_t start = SDL_GetPerformanceCounter();
    __real__ZN3GFX9GFXEngine9RasterizeEj(engine, stages);
    glFinish();
    rasterMs[pass] += (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                      (double)SDL_GetPerformanceFrequency();
    ++rasterCount[pass];
}

// CAPTURE_MODE 1 = no scene FBO (depth read from the window), 2 = legacy GL path.
extern "C" void __real__ZN6System4InitEb(bool);
extern "C" void __wrap__ZN6System4InitEb(bool oldGL)
{
    LoadConfig();
    if (cfgMode == 1)
        System::IniConf::GfxColorEffects.Value = (int32_t)0;
    else if (cfgMode == 2)
    {
        System::IniConf::GfxVBO.Value = false;
        oldGL = true;
    }
    __real__ZN6System4InitEb(oldGL);
}
