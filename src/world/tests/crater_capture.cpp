// Capture driver for the terrain craters (not part of the game build). Same
// wrapping as decal_capture.cpp (see its header for the link line):
//   - starts a real level from the menu;
//   - digs craters with the real crater block of Weapon 38 (20 Tons Taerbomb)
//     through the real API, on real hits of the real collision;
//   - measures the collision before and after (the ground units stand on);
//   - saves what the real renderer drew, with and without the weapon decal;
//   - writes the craters to a save file and reads them back;
//   - times drawing and ray tests with the limit full of craters.
// Run from "UA Rising"; <folder>/../cap_cfg.txt holds "level mode plan".
// Mode 1 = no colour effects, 2 = old OpenGL path (no VBO). Plans:
//   (any other word)   everything above;
//   decals, decalsflat decals on dug and dented surfaces / the same with no crater;
//   userarea, userreal the user's test area, checker / real decal;
//   stress, stressreal the same area bombed fourteen times in a row, as in play;
//   cracks, cracksfree no gap may open between surfaces that touched before the
//                      hits, also when the buildings break (free: no INI limits);
//   tankbomb           a real bomb on a ground unit;
//   perfnear           the timings only, with the camera low over the dug ground.
// Back up Data/Save first: starting a level from the shell rewrites the player files.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL2/SDL_opengl.h>

#include "common/utils.h"
#include "game/yw.h"
#include "game/yw_internal.h"
#include "gui/uacommon.h"
#include "system/gfx.h"
#include "system/inivals.h"
#include "system/system.h"
#include "system/tform.h"
#include "world/saveparsers.h"
#include "game/ypagun.h"


namespace
{

struct Spot
{
    std::string name;
    Common::Point cell;
    std::vector<vec3d> digs;   // Where the bombs land (x, z; y found by the ray).
    vec3d eye[3];
    vec3d target[3];

    // Building hit: one shot along a ray (from above on a roof, from the side
    // on a wall) instead of the drops on the ground.
    bool building = false;
    int weapon = 38;
    vec3d rayStart, rayDir, focus;
};

enum Stage { STAGE_MENU, STAGE_WAIT_LEVEL, STAGE_SETTLE, STAGE_RUN, STAGE_BOMB, STAGE_GAP, STAGE_TURRET, STAGE_PERF,
             STAGE_TANKBOMB, STAGE_DONE };

// Camera of the stages that look at something moving (plan "tankbomb").
bool freeCamera = false;
vec3d freeEye, freeTarget;

Stage stage = STAGE_MENU;
int frames = 0;
std::vector<Spot> spots;
size_t spotIndex = 0;
int view = 0;
int pass = 0;          // 0 = no crater, 1 = crater, 2 = crater + weapon decal
int passFrames = 0;
bool capturePending = false;
std::string captureName;   // When set, the next frame is saved under this name.
std::string outDir;
FILE *logFile = nullptr;
TF::TForm3D camera;
int cfgLevel = 1;
int cfgMode = 0;
bool cfgLoaded = false;
double rasterMs = 0.0;
int rasterCount = 0;
double renderGameMs = 0.0;     // CPU time of the last RenderGame (crater copies are built there)
double renderGameMaxMs = 0.0;
double rebuildMaxMs = 0.0;
// Time of each piece and seam rebuild: the slowest one is logged at the end.
double slowestPieceMs = 0.0, slowestSeamMs = 0.0;
std::string slowestPiece;     // Slowest frame of crater rebuilds
int perfStep = 0;
int checksTotal = 0;
int checksFailed = 0;

void Check(bool ok, const std::string &label)
{
    ++checksTotal;
    if ( !ok )
        ++checksFailed;
    fprintf(logFile, "%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    fflush(logFile);
}

// Plan "decals": only the spots that show decals on dug or dented surfaces,
// checker decal on every weapon, then the run ends (no bomb, gap, turret, perf).
std::string cfgPlan;
// "decalsflat": the same run with the decals only, no crater (reference).
// "userarea": the user's test area left of the host station (see PlanSpots).
// "userreal": the same with the real weapon decal instead of the checker.
// "stress" / "stressreal": the user's area again, but bombed as in play: many
// hits one after the other, each decal landing on the craters already there.
bool Stress() { return cfgPlan == "stress" || cfgPlan == "stressreal"; }
bool UserArea() { return cfgPlan == "userarea" || cfgPlan == "userreal" || Stress(); }
bool DecalsPlan() { return cfgPlan == "decals" || cfgPlan == "decalsflat" || UserArea(); }

void LoadConfig()
{
    if (cfgLoaded)
        return;
    cfgLoaded = true;
    char plan[64] = {0};
    FILE *file = fopen(CAPTURE_DIR "/../cap_cfg.txt", "r");
    if (file)
    {
        if (fscanf(file, "%d %d %63s", &cfgLevel, &cfgMode, plan) < 2)
            cfgLevel = 1;
        fclose(file);
    }
    cfgPlan = plan;
}

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

// First real surface straight below a point, walked in short steps like a shell.
ypaworld_arg136 Drop(NC_STACK_ypaworld *w, double x, double z, double fromY)
{
    ypaworld_arg136 hit;
    for (double y = fromY - 2600.0; y < fromY + 1200.0 && !hit.isect; y += 200.0)
    {
        hit = ypaworld_arg136();
        hit.stPos = vec3d(x, y, z);
        hit.vect = vec3d(0.0, 200.0, 0.0);
        hit.flags = 0;
        w->ypaworld_func136(&hit);
    }
    return hit;
}

double GroundY(NC_STACK_ypaworld *w, double x, double z, double fromY)
{
    const ypaworld_arg136 hit = Drop(w, x, z, fromY);
    return hit.isect ? hit.isectPos.y : NAN;
}

// Weapon whose decal and crater are used: 38 (Taer bomb) unless a spot says otherwise.
int weaponId = 38;
bool checkerDecal = false;   // Pass 2 of the Pike spots: checker texture shows every cut.

const World::TCraterConfig &TaerCrater(NC_STACK_ypaworld *w)
{
    return w->_weaponProtos[weaponId].crater;
}

// First surface along a ray, walked in 200 unit steps like a shell.
ypaworld_arg136 Cast(NC_STACK_ypaworld *w, const vec3d &start, const vec3d &dir, double length)
{
    ypaworld_arg136 hit;
    for (double along = 0.0; along < length && !hit.isect; along += 200.0)
    {
        hit = ypaworld_arg136();
        hit.stPos = start + dir * along;
        hit.vect = dir * 200.0;
        hit.flags = 0;
        w->ypaworld_func136(&hit);
    }
    return hit;
}

double CastDistance(NC_STACK_ypaworld *w, const Spot &s)
{
    const ypaworld_arg136 hit = Cast(w, s.rayStart, s.rayDir, 3000.0);
    return hit.isect ? (hit.isectPos - s.rayStart).dot(s.rayDir) : NAN;
}

void SetViews(Spot &spot, NC_STACK_ypaworld *w)
{
    if (spot.weapon == 24)
    {
        const vec3d c = spot.building ? spot.focus : vec3d(spot.digs.front().x, w->_cells(spot.cell).height,
                                                            spot.digs.front().z);
        vec3d back(-spot.rayDir.x, 0.0, -spot.rayDir.z);
        if (!spot.building || back.length() < 0.1)
            back = vec3d(1.0, 0.0, 0.6);
        back.normalise();
        static const double views[3][3] = {{0.0, 260.0, 260.0}, {1.2, 230.0, 120.0}, {-1.0, 160.0, 40.0}};
        for (int i = 0; i < 3; ++i)
        {
            const double a = views[i][0];
            const vec3d dir(back.x * std::cos(a) - back.z * std::sin(a), 0.0,
                            back.x * std::sin(a) + back.z * std::cos(a));
            spot.eye[i] = c + dir * views[i][1] + vec3d(0.0, -views[i][2], 0.0);
            spot.target[i] = c;
        }
        return;
    }

    if (spot.building)
    {
        // Around the hit point, from the side the shot came from.
        vec3d back(-spot.rayDir.x, 0.0, -spot.rayDir.z);
        if (back.length() < 0.1)
            back = vec3d(1.0, 0.0, 0.6);
        back.normalise();
        static const double views[3][3] = {{0.0, 900.0, 450.0}, {0.9, 700.0, 250.0}, {-0.7, 520.0, 60.0}};
        for (int i = 0; i < 3; ++i)
        {
            const double a = views[i][0];
            const vec3d dir(back.x * std::cos(a) - back.z * std::sin(a), 0.0,
                            back.x * std::sin(a) + back.z * std::cos(a));
            spot.eye[i] = spot.focus + dir * views[i][1] + vec3d(0.0, -views[i][2], 0.0);
            spot.target[i] = spot.focus;
        }
        return;
    }

    const cellArea &area = w->_cells(spot.cell);
    const vec3d c = spot.digs.front();
    static const double views[3][4] = {{1.0, 0.6, 900.0, 700.0},
                                       {-0.8, 0.9, 700.0, 380.0},
                                       {0.7, -1.0, 650.0, 120.0}};
    for (int i = 0; i < 3; ++i)
    {
        vec3d dir(views[i][0], 0.0, views[i][1]);
        dir.normalise();
        spot.eye[i] = vec3d(c.x + dir.x * views[i][2], area.height - views[i][3], c.z + dir.z * views[i][2]);
        spot.target[i] = vec3d(c.x, area.height + 20.0, c.z);
    }
}

// A dig position is good when the crater code accepts it there.
bool Accepted(NC_STACK_ypaworld *w, double x, double z, double fromY)
{
    const ypaworld_arg136 hit = Drop(w, x, z, fromY);
    if ( !hit.isect )
        return false;
    const bool ok = w->SpawnTerrainCrater(TaerCrater(w), hit) &&
                    w->_terrainCraters.craters.back().kind == World::Crater::Shape::GROUND;
    w->ClearTerrainCraters();
    return ok;
}

// A building hit is good when the shot meets the building (not the ground) and
// the code makes a dent there.
bool BuildingShot(NC_STACK_ypaworld *w, const std::string &name, const Common::Point &cell,
                  const vec3d &start, const vec3d &dir)
{
    const ypaworld_arg136 hit = Cast(w, start, dir, 3000.0);
    if ( !hit.isect )
        return false;
    // A high natural rock is not a building roof. Select a point over visible
    // building geometry, using the same world footprint as the cap.
    const auto oldCap = System::IniConf::GameDeformationBuildingLimit.Value;
    System::IniConf::GameDeformationBuildingLimit.Value = std::string("0");
    const auto footprint = w->TerrainCraterLimits(hit.isectPos);
    System::IniConf::GameDeformationBuildingLimit.Value = oldCap;
    if (footprint.At(hit.isectPos.x, hit.isectPos.z, 1e6) != 0) return false;
    const bool ok = w->SpawnTerrainCrater(TaerCrater(w), hit) &&
                    w->_terrainCraters.craters.back().kind == World::Crater::Shape::DENT;
    w->ClearTerrainCraters();
    if ( !ok )
        return false;

    Spot s;
    s.name = name;
    s.cell = cell;
    s.building = true;
    s.rayStart = start;
    s.rayDir = dir;
    s.focus = hit.isectPos;
    s.digs.push_back(hit.isectPos);
    spots.push_back(s);
    const vec3d n = hit.skel->polygons[hit.polyID].Normal();
    fprintf(logFile, "%s cell=%d,%d type_id=%d purpose=%d hit=(%.0f %.0f %.0f) height=%.0f normal=(%.2f %.2f %.2f)\n",
            name.c_str(), cell.x, cell.y, (int)w->_cells(cell).type_id, (int)w->_cells(cell).PurposeType,
            hit.isectPos.x, hit.isectPos.y, hit.isectPos.z, w->_cells(cell).height - hit.isectPos.y, n.x, n.y, n.z);
    return true;
}

// A roof from above: the highest point among the centre and eight offsets.
bool RoofShot(NC_STACK_ypaworld *w, const std::string &name, const Common::Point &cell, double minHeight)
{
    const cellArea &area = w->_cells(cell);
    double best = minHeight;
    vec3d at;
    bool found = false;
    for (int k = 0; k < 9; ++k)
    {
        const double px = area.CenterPos.x + (k % 3 - 1) * 150.0, pz = area.CenterPos.z + (k / 3 - 1) * 150.0;
        const ypaworld_arg136 hit = Drop(w, px, pz, area.height);
        if (hit.isect && area.height - hit.isectPos.y > best)
        {
            best = area.height - hit.isectPos.y;
            at = vec3d(px, hit.isectPos.y, pz);
            found = true;
        }
    }
    return found && BuildingShot(w, name, cell, vec3d(at.x, at.y - 900.0, at.z), vec3d(0.0, 1.0, 0.0));
}

// A wall from the side, 100 above the ground, aimed at the sector centre.
bool WallShot(NC_STACK_ypaworld *w, const std::string &name, const Common::Point &cell)
{
    const cellArea &area = w->_cells(cell);
    static const double dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto &d : dirs)
    {
        const vec3d dir(-d[0], 0.0, -d[1]);
        const vec3d start(area.CenterPos.x + d[0] * 560.0, area.height - 100.0, area.CenterPos.z + d[1] * 560.0);
        const ypaworld_arg136 hit = Cast(w, start, dir, 600.0);
        if (!hit.isect || std::fabs(hit.skel->polygons[hit.polyID].B) > 0.3)
            continue;
        if (BuildingShot(w, name, cell, start, dir))
            return true;
    }
    return false;
}

void PlanSpots(NC_STACK_ypaworld *w)
{
    bool city = false, plain = false, seam = false, edge = false, roof = false, wall = false;
    bool station = false, special = false, gate = false, upgrade = false;

    for (int y = 2; y < w->_mapSize.y - 2; ++y)
    {
        for (int x = 2; x < w->_mapSize.x - 2; ++x)
        {
            const Common::Point id(x, y);
            const cellArea &area = w->_cells(id);
            const vec3d c = area.CenterPos;
            const float step = std::fabs(area.height - w->_cells(x + 1, y).height);

            // City sector: a piece of open ground among the buildings.
            if (!city && area.PurposeType == cellArea::PT_NONE && area.SectorType != 1)
            {
                for (int k = 0; k < 9 && !city; ++k)
                {
                    const double px = c.x + (k % 3 - 1) * 300.0, pz = c.z + (k / 3 - 1) * 300.0;
                    if (Accepted(w, px, pz, area.height))
                    {
                        city = true;
                        Spot s; s.name = "city"; s.cell = id; s.digs.push_back(vec3d(px, 0, pz));
                        spots.push_back(s);
                    }
                }
            }
            // Same, with the bomb on the line between two pieces (no crack allowed).
            if (!edge && city && area.PurposeType == cellArea::PT_NONE && area.SectorType != 1 &&
                (x + y) % 3 == 0)
            {
                for (int k = 0; k < 9 && !edge; ++k)
                {
                    const double px = c.x + (k % 3 - 1) * 300.0 + 150.0, pz = c.z + (k / 3 - 1) * 300.0;
                    if (Accepted(w, px, pz, area.height) && Accepted(w, px - 1.0, pz, area.height) &&
                        Accepted(w, px + 1.0, pz, area.height))
                    {
                        edge = true;
                        Spot s; s.name = "piece_edge"; s.cell = id; s.digs.push_back(vec3d(px, 0, pz));
                        spots.push_back(s);
                    }
                }
            }
            // One-piece sector: open plain, three overlapping hits.
            if (!plain && area.SectorType == 1 && area.PurposeType == cellArea::PT_NONE &&
                Accepted(w, c.x, c.z, area.height))
            {
                plain = true;
                Spot s; s.name = "plain_overlap"; s.cell = id;
                s.digs.push_back(vec3d(c.x, 0, c.z));
                s.digs.push_back(vec3d(c.x + 220.0, 0, c.z + 60.0));
                s.digs.push_back(vec3d(c.x + 90.0, 0, c.z - 200.0));
                spots.push_back(s);
            }
            // Seam between two sectors at different heights.
            if (!seam && step > 100.0f && area.PurposeType == cellArea::PT_NONE)
            {
                const double px = c.x + 600.0;
                if (Accepted(w, px, c.z, area.height))
                {
                    seam = true;
                    Spot s; s.name = "seam_slope"; s.cell = id; s.digs.push_back(vec3d(px, 0, c.z));
                    spots.push_back(s);
                }
            }
            // Buildings: a tall roof, a wall, and every kind of special building.
            if (!roof && area.PurposeType == cellArea::PT_NONE)
                roof = RoofShot(w, "building_roof", id, 300.0);
            if (!wall && roof && area.PurposeType == cellArea::PT_NONE && (x * 3 + y) % 2 == 0)
                wall = WallShot(w, "building_wall", id);
            if (!station && area.PurposeType == cellArea::PT_POWERSTATION)
                station = RoofShot(w, "powerstation", id, 40.0);
            if (!special && area.PurposeType == cellArea::PT_BUILDINGS)
                special = RoofShot(w, "special_building", id, 40.0);
            if (!gate && (area.PurposeType == cellArea::PT_GATECLOSED || area.PurposeType == cellArea::PT_GATEOPENED))
                gate = RoofShot(w, "beam_gate", id, 40.0);
            if (!upgrade && area.PurposeType == cellArea::PT_TECHUPGRADE)
                upgrade = RoofShot(w, "tech_upgrade", id, 40.0);
        }
    }

    {
        std::vector<Spot> pikes;
        for (const Spot &s : spots)
        {
            if (s.name != "city" && s.name != "building_wall" && s.name != "powerstation")
                continue;
            Spot p = s;
            p.name = "pike_" + s.name;
            p.weapon = 24;
            pikes.push_back(p);
            if (s.name == "city")
            {
                p.name = "pike_three_hits";
                p.digs = {s.digs.front(), s.digs.front() + vec3d(25, 0, 10), s.digs.front() + vec3d(-15, 0, 20)};
                pikes.push_back(p);
            }
        }
        spots.insert(spots.end(), pikes.begin(), pikes.end());
    }

    // Plan "userarea": where the user tests, the sectors left of the host
    // station (Taer bomb, three close hits as in play, buildings from above)
    // and a crater on the map border.
    if (UserArea())
    {
        spots.clear();
        const Common::Point robo = World::PositionToSectorID(w->_userRobo->_position);
        for (int dx = 1; dx <= 3; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
            {
                const Common::Point id(robo.x - dx, robo.y + dy);
                if (!w->IsGamePlaySector(id))
                    continue;
                const cellArea &area = w->_cells(id);
                const vec3d c = area.CenterPos;
                const std::string tag = std::to_string(id.x) + "_" + std::to_string(id.y);
                for (int k = 0; k < 9; ++k)
                {
                    const double px = c.x + (k % 3 - 1) * 300.0 + 75.0, pz = c.z + (k / 3 - 1) * 300.0 + 40.0;
                    if (Accepted(w, px, pz, area.height))
                    {
                        Spot s; s.name = "user_ground_" + tag; s.cell = id;
                        s.digs = {vec3d(px, 0, pz), vec3d(px + 120.0, 0, pz + 40.0), vec3d(px - 60.0, 0, pz + 90.0)};
                        if (Stress())
                        {
                            // Fourteen bombs scattered over the spot, some on the same hole.
                            uint32_t seed = 0x1234567u + (uint32_t)(id.x * 31 + id.y);
                            const auto roll = [&seed]()
                            {
                                seed = seed * 1664525u + 1013904223u;
                                return (double)((seed >> 8) & 0xffff) / 65535.0 * 2.0 - 1.0;
                            };
                            for (int n = 0; n < 11; ++n)
                                s.digs.push_back(vec3d(px + roll() * 420.0, 0, pz + roll() * 420.0));
                        }
                        spots.push_back(s);
                        break;
                    }
                }
                if (area.PurposeType != cellArea::PT_NONE)
                    RoofShot(w, "user_roof_" + tag, id, 40.0);
            }
        // Map border: the leftmost playable sector of the host station row.
        for (int x = 1; x < robo.x; ++x)
        {
            const Common::Point id(x, robo.y);
            if (!w->IsGamePlaySector(id))
                continue;
            const cellArea &area = w->_cells(id);
            for (double inset = 450.0; inset >= 150.0; inset -= 100.0)
            {
                const double px = area.CenterPos.x - inset, pz = area.CenterPos.z;
                if (Accepted(w, px, pz, area.height))
                {
                    Spot s; s.name = "user_border"; s.cell = id; s.digs = {vec3d(px, 0, pz)};
                    spots.push_back(s);
                    break;
                }
            }
            break;
        }
    }
    else if (DecalsPlan())
    {
        std::vector<Spot> kept;
        for (const Spot &s : spots)
            if (s.name == "city" || s.name == "building_wall" || s.name == "powerstation" ||
                s.name == "special_building" || s.name == "pike_city" || s.name == "pike_building_wall" ||
                s.name == "pike_powerstation")
                kept.push_back(s);
        spots.swap(kept);
    }

    for (Spot &s : spots)
        SetViews(s, w);
    Check(roof, "a tall roof was found and takes a dent");
    Check(wall, "a wall was found and takes a dent");
    Check(station, "a power station takes a dent");
    fprintf(logFile, "special=%d gate=%d upgrade=%d (only if the level has them)\n", special, gate, upgrade);

    fprintf(logFile, "map %dx%d spots=%d crater radius=%.0f~%.0f depth=%.0f~%.0f rim=%.0f~%.0f defined=%d\n",
            w->_mapSize.x, w->_mapSize.y, (int)spots.size(), TaerCrater(w).radius_min,
            TaerCrater(w).radius_max, TaerCrater(w).depth_min, TaerCrater(w).depth_max,
            TaerCrater(w).rim_min, TaerCrater(w).rim_max, TaerCrater(w).defined ? 1 : 0);
    Check(TaerCrater(w).defined, "Weapon 38 has its crater block");
    fflush(logFile);
}

// Collision before / after: what a unit standing there would touch.
// Points from the centre outwards along one of four directions.
std::vector<vec3d> Probes(const Spot &s, double radius, int direction)
{
    static const double dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    std::vector<vec3d> out;
    const vec3d c = s.digs.front();
    const double fractions[] = {0.0, 0.25, 0.5, 0.75, 1.0, 1.3, 2.0};
    for (double f : fractions)
        out.push_back(vec3d(c.x + f * radius * dirs[direction][0], 0.0, c.z + f * radius * dirs[direction][1]));
    return out;
}

int DigAll(NC_STACK_ypaworld *w, const Spot &s, bool withDecal)
{
    weaponId = s.weapon;
    checkerDecal = withDecal && cfgPlan != "userreal" && cfgPlan != "stressreal" && (s.weapon != 38 || DecalsPlan());
    const cellArea &area = w->_cells(s.cell);
    int made = 0;
    for (const vec3d &d : s.digs)
    {
        const ypaworld_arg136 hit = s.building ? Cast(w, s.rayStart, s.rayDir, 3000.0)
                                               : Drop(w, d.x, d.z, area.height);
        if ( !hit.isect )
            continue;
        if ( withDecal )
        {
            for (const World::TChainFXConfig &entry : w->_weaponProtos[weaponId].chain_fx)
                if (entry.mode == World::TChainFXConfig::MODE_GROUND_DECAL &&
                    entry.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD)
                {
                    World::TChainFXConfig fx = entry;
                    fx.ground_decal_smoke.enabled = false;  // Smoke would hide the ground.
                    if (checkerDecal)
                        fx.ground_decal_texture = "Graphics/GroundDecals/_capture_checker.png";
                    w->SpawnGroundDecal(fx, hit);
                }
        }
        // Plan "decalsflat": the same decals with no crater, as the reference.
        if ( cfgPlan != "decalsflat" )
            made += w->SpawnTerrainCrater(TaerCrater(w), hit) ? 1 : 0;
    }
    return made;
}

// Same frame, same time stamp: digging again gives the very same craters.
void Dig(NC_STACK_ypaworld *w, const Spot &s, bool withDecal, bool check)
{
    w->ClearTerrainCraters();
    w->ClearGroundDecals();
    const int made = DigAll(w, s, withDecal);
    // The decal pass also stresses accumulation with twenty identical impacts.
    if (withDecal && !UserArea()) {
        const auto initial = w->_terrainCraters.craters;
        for (int repeat = 0; repeat < 19; ++repeat)
            for (const auto &shape : initial) w->AddTerrainCrater(shape);
        if (s.building && !initial.empty()) {
            const auto &shape = initial.front();
            const auto limits = w->TerrainCraterLimits(vec3d(shape.x, shape.y, shape.z));
            const auto raw = World::Crater::SampleAt(w->_terrainCraters.craters, shape.x, shape.z, shape.y);
            const auto bounded = limits.Apply(raw, shape.x, shape.z);
            const double length = std::sqrt(bounded.OffsetX()*bounded.OffsetX() + bounded.OffsetY()*bounded.OffsetY() + bounded.OffsetZ()*bounded.OffsetZ());
            if (limits.building >= 0) {
                Check(std::fabs(limits.At(shape.x, shape.z, 1e6) - limits.building) < 1e-6,
                      s.name + ": real building footprint is classified for the cap");
                Check(length <= limits.building + 1e-6, s.name + ": twenty hits obey the building displacement cap");
            }
            fprintf(logFile, "%s repeated hits: raw movement %.2f, capped %.2f, limit %.2f\n", s.name.c_str(),
                std::sqrt(raw.OffsetX()*raw.OffsetX() + raw.OffsetY()*raw.OffsetY() + raw.OffsetZ()*raw.OffsetZ()), length, limits.building);
        }
    }
    if ( !check )
        return;

    // Bombs on open ground dig craters, however steep the pit already is: a
    // dent (pushed sideways) is only for what was steep before any hit.
    if ( Stress() && !s.building )
    {
        int dents = 0;
        for (const World::Crater::Shape &c : w->_terrainCraters.craters)
            dents += c.kind == World::Crater::Shape::DENT ? 1 : 0;
        fprintf(logFile, "%s: %d shapes, %d of them dents\n", s.name.c_str(),
                (int)w->_terrainCraters.craters.size(), dents);
        Check(dents * 3 <= (int)w->_terrainCraters.craters.size(),
              s.name + ": bombs on a pit keep digging craters, not dents");
    }

    Check(made == (int)s.digs.size(), s.name + ": every bomb dug a crater (" + std::to_string(made) + ")");
    if ( s.weapon != 38 )
        return;
    if ( w->_terrainCraters.craters.empty() )
        return;
    const World::Crater::Shape shape = w->_terrainCraters.craters.front();

    if ( s.building )
    {
        // The shot goes deeper into the dented building than into the intact one.
        const double dented = CastDistance(w, s);
        w->ClearTerrainCraters();
        const double intact = CastDistance(w, s);
        DigAll(w, s, false);
        char text[256];
        snprintf(text, sizeof(text), "%s: dent r=%.0f depth=%.0f dir=(%.2f %.2f %.2f), collision %.1f deeper",
                 s.name.c_str(), shape.radius, shape.depth, shape.nx, shape.ny, shape.nz, dented - intact);
        Check(shape.kind == World::Crater::Shape::DENT, std::string(text) + " [is a dent]");
        // A building moves no further than game.deformation_building_limit allows.
        const double cap = w->TerrainCraterLimits(vec3d(shape.x, shape.y, shape.z)).building;
        const double least = cap >= 0.0 ? std::min(0.25 * shape.depth, 0.8 * cap) : 0.25 * shape.depth;
        Check(dented - intact > least && dented - intact < 3.0 * shape.depth,
              std::string(text) + " [collision follows the dent]");
        return;
    }
    const double top = w->_cells(s.cell).height;

    // Measure along open ground: a direction that runs into a building would
    // measure its roof, which a crater does not touch.
    w->ClearTerrainCraters();
    std::vector<vec3d> probes;
    std::vector<double> flat;
    for (int direction = 0; direction < 4; ++direction)
    {
        std::vector<vec3d> candidate = Probes(s, shape.radius, direction);
        std::vector<double> heights;
        double worst = 0.0;
        for (const vec3d &p : candidate)
        {
            heights.push_back(GroundY(w, p.x, p.z, top));
            worst = std::max(worst, std::fabs(heights.back() - heights.front()));
        }
        if ( probes.empty() || worst < 3.0 )
        {
            probes = candidate;
            flat = heights;
            if ( worst < 3.0 )
            {
                fprintf(logFile, "%s probes along direction %d\n", s.name.c_str(), direction);
                break;
            }
        }
    }
    DigAll(w, s, false);

    std::vector<double> dug;
    for (const vec3d &p : probes)
        dug.push_back(GroundY(w, p.x, p.z, top));

    std::string line = s.name + " r=" + std::to_string((int)shape.radius) + " depth=" +
                       std::to_string((int)shape.depth) + " collision drop (+ = lower):";
    for (size_t i = 0; i < probes.size(); ++i)
    {
        char text[64];
        const double along = std::hypot(probes[i].x - probes[0].x, probes[i].z - probes[0].z);
        snprintf(text, sizeof(text), " %.0f%%r=%.1f", 100.0 * along / shape.radius, dug[i] - flat[i]);
        line += text;
    }
    fprintf(logFile, "%s\n", line.c_str());
    // The collision must have no hole: every probe finds ground before and after.
    for (size_t i = 0; i < probes.size(); ++i)
        if ( !std::isfinite(dug[i]) || !std::isfinite(flat[i]) )
        {
            // Same place a hair to the side: a ray exactly on the line between
            // two surfaces can miss both, with or without craters.
            const double beside = GroundY(w, probes[i].x + 0.013, probes[i].z + 0.017, top);
            fprintf(logFile, "%s probe %d at (%.3f %.3f): before=%.1f after=%.1f, a hair beside after=%.1f\n", s.name.c_str(),
                    (int)i, probes[i].x, probes[i].z, flat[i], dug[i], beside);
            Check(std::isfinite(beside) && (std::isfinite(dug[i]) || !std::isfinite(flat[i])),
                  s.name + ": the crater opened no hole in the collision");
            // Which dug surfaces lie under the probe, and whose they are.
            int dents = 0;
            for (const World::Crater::Shape &c : w->_terrainCraters.craters)
                dents += c.kind == World::Crater::Shape::DENT ? 1 : 0;
            fprintf(logFile, "  dents among the %d shapes: %d\n", (int)w->_terrainCraters.craters.size(), dents);
            for (const auto &entry : w->_terrainCraters.collision)
            {
                if (!entry.second || entry.second->empty)
                    continue;
                const double ox = (double)std::get<0>(entry.first), oz = (double)std::get<1>(entry.first);
                const UAskeleton::Data &data = entry.second->data;
                int covering = 0;
                for (const UAskeleton::Polygon &polygon : data.polygons)
                {
                    if (polygon.num_vertices != 3)
                        continue;
                    const double px = probes[i].x - ox, pz = probes[i].z - oz;
                    const vec3d &a0 = data.POO[polygon.v[0]], &b0 = data.POO[polygon.v[1]], &c0 = data.POO[polygon.v[2]];
                    const double d = (b0.x - a0.x) * (c0.z - a0.z) - (b0.z - a0.z) * (c0.x - a0.x);
                    if (std::fabs(d) < 1e-9)
                        continue;
                    const double w1 = ((b0.x - px) * (c0.z - pz) - (b0.z - pz) * (c0.x - px)) / d;
                    const double w2 = ((c0.x - px) * (a0.z - pz) - (c0.z - pz) * (a0.x - px)) / d;
                    if (w1 >= 0 && w2 >= 0 && w1 + w2 <= 1)
                        ++covering;
                }
                if (covering)
                    fprintf(logFile, "  copy at (%.0f %.0f) type %d: %d triangles over the probe (probe is %.0f %.0f from its centre)\n",
                            ox, oz, std::get<3>(entry.first), covering, probes[i].x - ox, probes[i].z - oz);
            }
        }

    const double centreDrop = dug[0] - flat[0];
    const double rimRise = flat[4] - dug[4];
    const double farDrop = dug[6] - flat[6];
    char text[256];
    snprintf(text, sizeof(text), "%s: centre lower by %.1f (depth %.1f), rim higher by %.1f, at 2r %.2f",
             s.name.c_str(), centreDrop, shape.depth, rimRise, farDrop);
    // Next to the map border the crater fades out on purpose (frozen boxes).
    const auto rimLimits = w->TerrainCraterLimits(probes[4]);
    const double frozenAtRim = World::Crater::DeformationLimits::Influence(rimLimits.frozen, probes[4].x, probes[4].z);
    fprintf(logFile, "%s frozen boxes %d, at rim %.2f\n", s.name.c_str(), (int)rimLimits.frozen.size(), frozenAtRim);
    for (const auto &box : rimLimits.frozen)
        fprintf(logFile, "  frozen %.0f %.0f .. %.0f %.0f\n", box.minX, box.minZ, box.maxX, box.maxZ);
    if (s.digs.size() == 1)
    {
        Check(std::fabs(centreDrop - shape.depth) < 0.08 * shape.depth + 2.0, std::string(text) + " [centre]");
        Check(rimRise > 0.0 || frozenAtRim > 0.0, std::string(text) + " [rim]");
        Check(std::fabs(farDrop) < 0.5, std::string(text) + " [outside untouched]");
    }
    else
    {
        double deepest = 0.0;
        for (const World::Crater::Shape &c : w->_terrainCraters.craters)
            deepest = std::max(deepest, c.depth);
        const auto centreLimits = w->TerrainCraterLimits(probes[0]);
        const auto expected = centreLimits.Apply(
            World::Crater::SampleAt(w->_terrainCraters.craters, probes[0].x, probes[0].z, flat[0]),
            probes[0].x, probes[0].z);
        fprintf(logFile, "%s overlap: deepest %.1f, field drop at centre %.1f, measured %.1f, craters %d\n",
                s.name.c_str(), deepest, expected.OffsetY(), centreDrop, (int)w->_terrainCraters.craters.size());
        // In the user's area the probe may land on a building slab (capped
        // apart): there the line above is the record, not a check.
        if (!UserArea())
            Check(centreDrop > shape.depth * 0.9 && centreDrop <= 1.75 * deepest + 1.0, std::string(text) + " [overlap capped]");
    }
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

void SaveRoundTrip(NC_STACK_ypaworld *w)
{
    const std::vector<World::Crater::Shape> kept = w->_terrainCraters.craters;
    const std::string path = "save:crater_roundtrip.tmp";   // Data/Save is backed up by the runner.
    FSMgr::FileHandle *file = uaOpenFileAlloc(path, "w");
    if (!file)
    {
        Check(false, "save file opened");
        return;
    }
    file->printf("; save test\n");
    w->WriteTerrainCraters(file);
    delete file;

    w->ClearTerrainCraters();
    ScriptParser::HandlersList parsers { new World::Parsers::SaveCraterParser(w) };
    const bool parsed = ScriptParser::ParseFile(path, parsers, ScriptParser::FLAG_NO_SCOPE_SKIP);

    bool same = w->_terrainCraters.craters.size() == kept.size();
    for (size_t i = 0; same && i < kept.size(); ++i)
    {
        const World::Crater::Shape &a = kept[i], &b = w->_terrainCraters.craters[i];
        same = std::fabs(a.x - b.x) < 0.01 && std::fabs(a.z - b.z) < 0.01 && std::fabs(a.y - b.y) < 0.01 &&
               std::fabs(a.radius - b.radius) < 0.01 && std::fabs(a.depth - b.depth) < 0.01 &&
               std::fabs(a.rim - b.rim) < 0.0001;
    }
    Check(parsed && same, "save and load keep all " + std::to_string(kept.size()) + " craters");
}

// Collision cost: many vertical rays across the dug area, as units on the ground do.
double RayMs(NC_STACK_ypaworld *w, const Spot &s)
{
    const cellArea &area = w->_cells(s.cell);
    const vec3d c = s.digs.front();
    const uint64_t start = SDL_GetPerformanceCounter();
    for (int i = 0; i < 4000; ++i)
    {
        const double x = c.x + ((i * 37) % 1200) - 600.0, z = c.z + ((i * 91) % 1200) - 600.0;
        ypaworld_arg136 hit;
        hit.stPos = vec3d(x, area.height - 400.0, z);
        hit.vect = vec3d(0.0, 600.0, 0.0);
        hit.flags = 0;
        w->ypaworld_func136(&hit);
    }
    return (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

// ---- Plan "cracks": does the drawn world stay in one piece? ---------------
// Every point that touched a polygon edge before the craters and dents (rest
// places, TMesh::Rest) must still touch it after them. The distance it ends up
// from the edge is a gap the player can look through.
struct CrackPoint
{
    vec3d now, rest;
    int piece = 0;      // cell x * 1000 + cell y * 10 + piece, for the log
    int mesh = 0;
    bool dug = false;
};

struct CrackEdge
{
    vec3d now[2], rest[2];
    int piece = 0, mesh = 0;
    bool dug = false;
};

void CrackCollect(NC_STACK_ypaworld *w, const Common::Point &centre, std::vector<CrackPoint> *points,
                  std::vector<CrackEdge> *edges)
{
    // As RenderGame does every frame: copies near a piece that changed model are made again.
    w->RefreshTerrainCraterFootprints();
    for (int cy = centre.y - 1; cy <= centre.y + 1; ++cy)
        for (int cx = centre.x - 1; cx <= centre.x + 1; ++cx)
        {
            const Common::Point id(cx, cy);
            if (!w->IsGamePlaySector(id))
                continue;
            const cellArea &area = w->_cells(id);
            const bool single = area.SectorType == 1;
            for (int zz = 0; zz < (single ? 1 : 3); ++zz)
                for (int xx = 0; xx < (single ? 1 : 3); ++xx)
                {
                    const int legoId = w->GetLegoBld(&area, xx, zz);
                    if (legoId < 0 || !w->_legoArray[legoId].Base)
                        continue;
                    const vec3d origin = area.CenterPos + (single ? vec3d() : vec3d((xx - 1) * 300.0, 0, (zz - 1) * 300.0));
                    // Outside the frame budget: every copy is made now.
                    w->_terrainCraters.rebuiltThisFrame = 0;
                    w->_terrainCraters.rebuildMs = 0.0;
                    std::list<GFX::TMesh> *copy = w->TerrainCraterPieceMeshes(area, xx, zz, legoId, origin);
                    const std::list<GFX::TMesh> &meshes = copy ? *copy : w->_legoArray[legoId].Base->Meshes;
                    int meshIndex = 0;
                    for (const GFX::TMesh &mesh : meshes)
                    {
                        const bool dug = mesh.Rest.size() == mesh.Vertexes.size();
                        const auto place = [&](GFX::IndexType i, vec3d *now, vec3d *rest)
                        {
                            const vec3f &p = mesh.Vertexes[i].Pos;
                            *now = origin + vec3d(p.x, p.y, p.z);
                            *rest = dug ? origin + vec3d(mesh.Rest[i].x, mesh.Rest[i].y, mesh.Rest[i].z) : *now;
                        };
                        for (size_t i = 0; i + 2 < mesh.Indixes.size(); i += 3)
                            for (int k = 0; k < 3; ++k)
                            {
                                CrackPoint point;
                                place(mesh.Indixes[i + k], &point.now, &point.rest);
                                point.piece = cx * 1000 + cy * 10 + (single ? 9 : xx + zz * 3);
                                point.mesh = meshIndex;
                                point.dug = dug;
                                points->push_back(point);

                                CrackEdge edge;
                                edge.now[0] = point.now;
                                edge.rest[0] = point.rest;
                                place(mesh.Indixes[i + (k + 1) % 3], &edge.now[1], &edge.rest[1]);
                                edge.piece = point.piece;
                                edge.mesh = meshIndex;
                                edge.dug = dug;
                                edges->push_back(edge);
                            }
                        ++meshIndex;
                    }
                }
        }
}

void CrackReport(NC_STACK_ypaworld *w, const Common::Point &centre, const std::string &label)
{
    std::vector<CrackPoint> points;
    std::vector<CrackEdge> edges;
    CrackCollect(w, centre, &points, &edges);

    // Points sorted in squares of their rest place (top view).
    const double square = 40.0;
    const auto key = [&](double x, double z)
    {
        return ((int64_t)std::floor(x / square) << 32) ^ (int64_t)(uint32_t)(int32_t)std::floor(z / square);
    };
    std::unordered_map<int64_t, std::vector<uint32_t>> grid;
    for (size_t i = 0; i < points.size(); ++i)
        grid[key(points[i].rest.x, points[i].rest.z)].push_back((uint32_t)i);

    struct Found { double gap; CrackPoint point; CrackEdge edge; double t; };
    std::vector<Found> worst;
    double largest = 0.0;
    int over1 = 0, over5 = 0;
    for (const CrackEdge &edge : edges)
    {
        const vec3d along = edge.rest[1] - edge.rest[0];
        const double length2 = along.dot(along);
        if (length2 < 1e-9)
            continue;
        const double minX = std::min(edge.rest[0].x, edge.rest[1].x) - 0.05, maxX = std::max(edge.rest[0].x, edge.rest[1].x) + 0.05;
        const double minZ = std::min(edge.rest[0].z, edge.rest[1].z) - 0.05, maxZ = std::max(edge.rest[0].z, edge.rest[1].z) + 0.05;
        for (double gx = std::floor(minX / square); gx <= std::floor(maxX / square); gx += 1.0)
            for (double gz = std::floor(minZ / square); gz <= std::floor(maxZ / square); gz += 1.0)
            {
                auto cell = grid.find(key(gx * square + 1.0, gz * square + 1.0));
                if (cell == grid.end())
                    continue;
                for (uint32_t index : cell->second)
                {
                    const CrackPoint &point = points[index];
                    double t = (point.rest - edge.rest[0]).dot(along) / length2;
                    if (t < -1e-6 || t > 1.0 + 1e-6)
                        continue;
                    t = std::max(0.0, std::min(1.0, t));
                    if ((edge.rest[0] + along * t - point.rest).length() > 0.02)
                        continue;
                    const double gap = (edge.now[0] + (edge.now[1] - edge.now[0]) * t - point.now).length();
                    if (gap > 1.0) ++over1;
                    if (gap > 5.0) ++over5;
                    if (gap > largest)
                    {
                        largest = gap;
                        worst.push_back({gap, point, edge, t});
                    }
                }
            }
    }
    fprintf(logFile, "CRACKS %s: triangles=%d largest gap=%.2f, pairs over 1 unit=%d, over 5 units=%d\n", label.c_str(),
            (int)(edges.size() / 3), largest, over1, over5);
    for (size_t i = worst.size() > 4 ? worst.size() - 4 : 0; i < worst.size(); ++i)
    {
        const Found &f = worst[i];
        fprintf(logFile, "  gap %.2f at rest (%.1f %.1f %.1f): point piece %d mesh %d dug=%d moved %.2f | edge piece %d mesh %d dug=%d "
                "t=%.3f length=%.1f ends moved %.2f %.2f\n", f.gap, f.point.rest.x, f.point.rest.y, f.point.rest.z,
                f.point.piece, f.point.mesh, f.point.dug ? 1 : 0, (f.point.now - f.point.rest).length(),
                f.edge.piece, f.edge.mesh, f.edge.dug ? 1 : 0, f.t, (f.edge.rest[1] - f.edge.rest[0]).length(),
                (f.edge.now[0] - f.edge.rest[0]).length(), (f.edge.now[1] - f.edge.rest[1]).length());
    }
    fflush(logFile);
}

void CrackPlan(NC_STACK_ypaworld *w)
{
    // Lego models with child objects: their meshes are not in the dug copy.
    int withKids = 0;
    for (const auto &lego : w->_legoArray)
        if (lego.Base && !lego.Base->_KIDS.empty())
            ++withKids;
    fprintf(logFile, "CRACKS lego models with child objects: %d of %d\n", withKids, (int)w->_legoArray.size());

    weaponId = 38;
    int done = 0;
    for (int y = 2; y < w->_mapSize.y - 2 && done < 8; ++y)
        for (int x = 2; x < w->_mapSize.x - 2 && done < 8; ++x)
        {
            const Common::Point id(x, y);
            cellArea &area = w->_cells(id);
            if (!w->IsGamePlaySector(id) || area.SectorType == 1)
                continue;
            // A sector with something tall on it.
            double tallest = 0.0;
            for (int k = 0; k < 9; ++k)
            {
                const ypaworld_arg136 hit = Drop(w, area.CenterPos.x + (k % 3 - 1) * 300.0, area.CenterPos.z + (k / 3 - 1) * 300.0, area.height);
                if (hit.isect)
                    tallest = std::max(tallest, (double)area.height - hit.isectPos.y);
            }
            if (tallest < 120.0)
                continue;
            ++done;

            w->ClearTerrainCraters();
            const std::string name = "cell " + std::to_string(x) + "," + std::to_string(y) + " purpose " +
                                     std::to_string((int)area.PurposeType) + " tallest " + std::to_string((int)tallest);
            CrackReport(w, id, name + " intact");

            uint32_t seed = 0x51ed27u + (uint32_t)(x * 131 + y);
            const auto roll = [&seed]()
            {
                seed = seed * 1664525u + 1013904223u;
                return (double)((seed >> 8) & 0xffff) / 65535.0 * 2.0 - 1.0;
            };
            int craters = 0, dents = 0;
            for (int n = 0; n < 10; ++n)
            {
                const ypaworld_arg136 hit = Drop(w, area.CenterPos.x + roll() * 520.0, area.CenterPos.z + roll() * 520.0, area.height);
                if (hit.isect && w->SpawnTerrainCrater(TaerCrater(w), hit))
                    (w->_terrainCraters.craters.back().kind == World::Crater::Shape::DENT ? dents : craters)++;
            }
            CrackReport(w, id, name + " after " + std::to_string(craters) + " craters " + std::to_string(dents) + " dents");

            // The building breaks: its pieces change model one after the other.
            std::vector<int16_t> saved(area.buildings_health.begin(), area.buildings_health.end());
            int step = 0;
            for (int16_t &h : area.buildings_health)
            {
                h = (step++ % 2) ? 60 : 0;
                if (step == 4)
                    CrackReport(w, id, name + " half damaged");
            }
            CrackReport(w, id, name + " all damaged");
            size_t i = 0;
            for (int16_t &h : area.buildings_health)
                h = saved[i++];
            w->ClearTerrainCraters();
        }
}

void Advance()
{
    NC_STACK_ypaworld *w = World_();
    ++frames;

    switch (stage)
    {
    case STAGE_MENU:
        break;

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
            if (cfgPlan == "cracks" || cfgPlan == "cracksfree")
            {
                // "cracksfree": without the limits of game.deformation_*_limit (an INI without them).
                if (cfgPlan == "cracksfree")
                {
                    System::IniConf::GameDeformationBuildingLimit.Value = std::string("");
                    System::IniConf::GameDeformationGroundLimit.Value = std::string("");
                }
                CrackPlan(w);
                stage = STAGE_DONE;
                break;
            }
            PlanSpots(w);
            // Plan "tankbomb": a real bomb on a ground unit, then the run ends.
            stage = spots.empty() ? STAGE_DONE : (cfgPlan == "tankbomb" ? STAGE_TANKBOMB : STAGE_RUN);
            // Plan "perfnear": the timings only, seen from low over the dug ground
            // (big decals all around the camera, as when flying over a battlefield).
            if (cfgPlan == "perfnear" && !spots.empty())
            {
                stage = STAGE_PERF;
                perfStep = 0;
            }
            frames = 0;
            spotIndex = 0;
            view = 0;
            pass = 0;
            passFrames = 0;
        }
        break;

    case STAGE_RUN:
        if (passFrames == 0)
        {
            if (pass == 0)
            {
                w->ClearTerrainCraters();
                w->ClearGroundDecals();
            }
            else
                Dig(w, spots[spotIndex], pass == 2, pass == 1 && view == 0);
        }
        else if (passFrames == (DecalsPlan() ? 40 : 4))
            capturePending = true;
        else if (passFrames > (DecalsPlan() ? 41 : 5))
        {
            passFrames = -1;
            if (++pass > 2)
            {
                pass = 0;
                if (++view > 2)
                {
                    view = 0;
                    if (++spotIndex >= spots.size() && DecalsPlan())
                    {
                        stage = STAGE_DONE;
                    }
                    else if (spotIndex >= spots.size())
                    {
                        // Leave the craters of every spot in the world for the save test.
                        w->ClearTerrainCraters();
                        w->ClearGroundDecals();
                        for (const Spot &s : spots)
                            for (const vec3d &d : s.digs)
                            {
                                const ypaworld_arg136 hit = Drop(w, d.x, d.z, w->_cells(s.cell).height);
                                if (hit.isect)
                                    w->SpawnTerrainCrater(TaerCrater(w), hit);
                            }
                        SaveRoundTrip(w);
                        stage = STAGE_BOMB;
                        frames = 0;
                    }
                }
            }
        }
        ++passFrames;
        break;

    case STAGE_BOMB:
    {
        // A real Weapon 38 bomb through the real game: created like a launch,
        // it falls, hits the ground and must dig exactly one crater there.
        static NC_STACK_ypamissile *bomb = nullptr;
        static vec3d target;
        const Spot &s = spots.front();
        if (frames == 1)
        {
            w->ClearTerrainCraters();
            w->ClearGroundDecals();
            target = s.digs.front();
            target.y = GroundY(w, target.x, target.z, w->_cells(s.cell).height);

            ypaworld_arg146 arg;
            arg.vehicle_id = 38;
            arg.pos = vec3d(target.x, target.y - 900.0, target.z);
            bomb = w->_userRobo ? w->ypaworld_func147(&arg) : nullptr;
            if (bomb)
            {
                bomb->SetLauncherBact(w->_userRobo);
                bomb->SetStartHeight(arg.pos.y);
                bomb->_owner = w->_userRobo->_owner;
                bomb->_fly_dir = vec3d(0.0, 1.0, 0.0);
                bomb->_fly_dir_length = 0.0;
                bomb->_kidRef.Detach();
                bomb->_parent = nullptr;
                w->_userRobo->_missiles_list.push_back(bomb);
            }
            Check(bomb != nullptr, "real Weapon 38 bomb created");
            if (!bomb)
            {
                stage = STAGE_PERF;
                perfStep = 0;
                frames = 0;
            }
        }
        else if (frames == 3)
            captureName = "perf_view_1_before_bomb";
        else if (!w->_terrainCraters.craters.empty() || frames > 2400)
        {
            // Let the dead bomb run a few more frames: no second crater allowed.
            static int settle = 0;
            if (++settle < 120 && frames <= 2400)
                break;
            const size_t count = w->_terrainCraters.craters.size();
            double distance = -1.0;
            if (count)
            {
                const World::Crater::Shape &c = w->_terrainCraters.craters.front();
                distance = std::hypot(c.x - target.x, c.z - target.z);
            }
            fprintf(logFile, "real bomb: frames=%d craters=%d distance from drop point=%.1f\n", frames,
                    (int)count, distance);
            Check(count == 1, "the real bomb dug exactly one crater");
            Check(count == 1 && distance < 60.0, "the crater is where the bomb landed");
            stage = STAGE_GAP;
            perfStep = 0;
            frames = 0;
        }
        break;
    }

    case STAGE_GAP:
    {
        // Damaged power station, then a dent on its roof: the collision must
        // stay on the drawn surface (a unit landing there must not float).
        static Common::Point cell(-1, -1);
        static std::vector<int16_t> saved;
        static std::vector<double> topBefore, hitBefore;
        if (frames == 1)
        {
            w->ClearTerrainCraters();
            w->ClearGroundDecals();
            for (const Spot &s : spots)
                if (s.name == "powerstation")
                    cell = s.cell;
            if (cell.x < 0)
            {
                stage = STAGE_TURRET;
                frames = 0;
                break;
            }
            cellArea &area = w->_cells(cell);
            saved.assign(area.buildings_health.begin(), area.buildings_health.end());
            for (int16_t &h : area.buildings_health)
                h = 60;
        }
        else if (frames == 10 || frames == 60)
        {
            if (frames == 10)
                fprintf(logFile, "GAP damaged power station, before the dent:\n");
            else
                fprintf(logFile, "GAP damaged power station, after the dent:\n");
            cellArea &area = w->_cells(cell);
            const bool single = area.SectorType == 1;
            double worstFloat = 0.0, worstSink = 0.0, worstSplit = 0.0, movedDrawn = 0.0, movedHit = 0.0;
            int samples = 0;
            size_t index = 0;
            if (frames == 10)
            {
                topBefore.clear();
                hitBefore.clear();
            }
            for (int zz = 0; zz < (single ? 1 : 3); ++zz)
                for (int xx = 0; xx < (single ? 1 : 3); ++xx)
                {
                    const int legoId = w->GetLegoBld(&area, xx, zz);
                    const vec3d origin = area.CenterPos + (single ? vec3d() : vec3d((xx - 1) * 300.0, 0, (zz - 1) * 300.0));
                    const std::list<GFX::TMesh> *meshes = &w->_legoArray[legoId].Base->Meshes;
                    auto found = w->_terrainCraters.pieces.find(World::Crater::PieceKey(cell.x, cell.y, single ? 9 : xx + zz * 3));
                    if (found != w->_terrainCraters.pieces.end() && !found->second->empty && found->second->legoId == legoId)
                        meshes = &found->second->meshes;
                    const double half = single ? 430.0 : 140.0;
                    for (double a = -half; a <= half; a += half / 6.0)
                        for (double b = -half; b <= half; b += half / 6.0)
                        {
                            const double x = origin.x + a, z = origin.z + b;
                            // Top of the drawn surface at x, z.
                            double top = 1e30;
                            for (const GFX::TMesh &mesh : *meshes)
                                for (size_t i = 0; i + 2 < mesh.Indixes.size(); i += 3)
                                {
                                    const vec3f &p0 = mesh.Vertexes[mesh.Indixes[i]].Pos;
                                    const vec3f &p1 = mesh.Vertexes[mesh.Indixes[i + 1]].Pos;
                                    const vec3f &p2 = mesh.Vertexes[mesh.Indixes[i + 2]].Pos;
                                    const double lx = x - origin.x, lz = z - origin.z;
                                    const double d = (p1.x - p0.x) * (p2.z - p0.z) - (p1.z - p0.z) * (p2.x - p0.x);
                                    if (std::fabs(d) < 1e-6)
                                        continue;
                                    const double w1 = ((p1.x - lx) * (p2.z - lz) - (p1.z - lz) * (p2.x - lx)) / d;
                                    const double w2 = ((p2.x - lx) * (p0.z - lz) - (p2.z - lz) * (p0.x - lx)) / d;
                                    const double w3 = 1.0 - w1 - w2;
                                    if (w1 < -1e-4 || w2 < -1e-4 || w3 < -1e-4)
                                        continue;
                                    top = std::min(top, origin.y + p0.y * w1 + p1.y * w2 + p2.y * w3);
                                }
                            const double hitY = GroundY(w, x, z, area.height);
                            if (frames == 10)
                            {
                                topBefore.push_back(top);
                                hitBefore.push_back(hitY);
                            }
                            else if (index < topBefore.size())
                            {
                                const size_t k = index++;
                                // Points where collision and drawing agreed before the dent.
                                if (topBefore[k] < 1e29 && std::isfinite(hitBefore[k]) && top < 1e29 &&
                                    std::isfinite(hitY) && std::fabs(hitBefore[k] - topBefore[k]) < 15.0)
                                {
                                    const double dt = top - topBefore[k], dh = hitY - hitBefore[k];
                                    worstSplit = std::max(worstSplit, std::fabs(dt - dh));
                                    movedDrawn = std::max(movedDrawn, dt);
                                    movedHit = std::max(movedHit, dh);
                                }
                            }
                            if (top > 1e29 || !std::isfinite(hitY))
                                continue;
                            ++samples;
                            // Y points down: collision above the drawn top = floating.
                            worstFloat = std::max(worstFloat, top - hitY);
                            worstSink = std::max(worstSink, hitY - top);
                        }
                }
            fprintf(logFile, "GAP samples=%d collision above drawn surface up to %.1f, below up to %.1f\n",
                    samples, worstFloat, worstSink);
            if (frames == 60)
                fprintf(logFile, "GAP where collision matched the drawing before: drawn moved up to %.1f, collision moved up to %.1f, worst difference %.1f\n",
                        movedDrawn, movedHit, worstSplit);
            if (frames == 10)
            {
                static double before = 0.0;
                before = worstFloat;
                for (const Spot &s : spots)
                    if (s.name == "powerstation")
                    {
                        weaponId = 38;
                        BuildingShot(w, "gap_dent", cell, s.rayStart, s.rayDir);  // checks only
                        const ypaworld_arg136 hit = Cast(w, s.rayStart, s.rayDir, 3000.0);
                        if (hit.isect)
                            w->SpawnTerrainCrater(TaerCrater(w), hit);
                    }
                fprintf(logFile, "GAP craters after the dent: %d\n", (int)w->_terrainCraters.craters.size());
                (void)before;
            }
            else
            {
                Check(worstSplit < 8.0, "damaged power station: collision follows the dented drawing (difference " +
                                        std::to_string((int)worstSplit) + ")");
                cellArea &area = w->_cells(cell);
                size_t i = 0;
                for (int16_t &h : area.buildings_health)
                    if (i < saved.size())
                        h = saved[i++];
                w->ClearTerrainCraters();
                stage = STAGE_TURRET;
                frames = 0;
            }
        }
        break;
    }

    case STAGE_TURRET:
    {
        // Vanilla rule: a ground turret with nothing under it dies
        // (NC_STACK_ypagun::CheckPedestal every 800 ms). A deep hole under one
        // turret must kill it; a scratch under another must not.
        static NC_STACK_ypagun *deep = nullptr, *shallow = nullptr;
        static int32_t startTime = 0;
        if (frames == 1)
        {
            w->ClearTerrainCraters();
            deep = shallow = nullptr;
            for (int y = 1; y < w->_mapSize.y - 1 && !shallow; ++y)
                for (int x = 1; x < w->_mapSize.x - 1 && !shallow; ++x)
                    for (NC_STACK_ypabact *bact : w->_cells(x, y).unitsList)
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>(bact);
                        if (!gun || gun->_status == BACT_STATUS_DEAD || gun->IsRoboGun() ||
                            gun->getGUN_levitate() || gun->_energy <= 0)
                            continue;
                        if (!deep)
                            deep = gun;
                        else if (std::fabs(gun->_position.x - deep->_position.x) +
                                 std::fabs(gun->_position.z - deep->_position.z) > 1500.0)
                        {
                            shallow = gun;
                            break;
                        }
                    }
            fprintf(logFile, "TURRET deep=%p shallow=%p\n", (void *)deep, (void *)shallow);
            if (!deep || !shallow)
            {
                Check(false, "two ground turrets found for the support test");
                stage = STAGE_PERF;
                perfStep = 0;
                frames = 0;
                break;
            }
            const auto dig = [&](NC_STACK_ypagun *gun, float radius, float depth, const char *label)
            {
                const ypaworld_arg136 below = Drop(w, gun->_position.x, gun->_position.z,
                                                  w->_cells(gun->_cellId).height);
                World::TCraterConfig config;
                config.defined = true;
                config.radius_min = config.radius_max = radius;
                config.depth_min = config.depth_max = depth;
                config.rim_min = config.rim_max = 0.0f;
                const bool made = below.isect && w->SpawnTerrainCrater(config, below);
                fprintf(logFile, "TURRET %s at (%.0f %.0f %.0f) height=%.1f support %.1f below it, %s made=%d\n",
                        label, gun->_position.x, gun->_position.y, gun->_position.z, gun->_height,
                        below.isect ? below.isectPos.y - gun->_position.y : -1.0,
                        made ? (w->_terrainCraters.craters.back().kind ? "dent" : "crater") : "-", made ? 1 : 0);
                return made;
            };
            // Without the building cap of game.deformation_building_limit, so the
            // roof can really sink past the turret's support ray.
            System::IniConf::GameDeformationBuildingLimit.Value = std::string("");
            System::IniConf::GameDeformationGroundLimit.Value = std::string("");
            Check(dig(deep, 600.0f, 300.0f, "deep") && dig(shallow, 60.0f, 3.0f, "shallow"),
                  "the turret holes were dug");
            const ypaworld_arg136 after = Drop(w, deep->_position.x, deep->_position.z, w->_cells(deep->_cellId).height);
            fprintf(logFile, "TURRET deep support after the hole: %.1f below\n",
                    after.isect ? after.isectPos.y - deep->_position.y : -1.0);
            startTime = w->_timeStamp;
        }
        else if (w->_timeStamp - startTime > 3000)
        {
            const bool deepDead = deep->_status == BACT_STATUS_DEAD || deep->_energy <= 0;
            const bool shallowAlive = shallow->_status != BACT_STATUS_DEAD && shallow->_energy > 0;
            fprintf(logFile, "TURRET after %d ms: deep status=%d energy=%d, shallow status=%d energy=%d\n",
                    w->_timeStamp - startTime, (int)deep->_status, (int)deep->_energy,
                    (int)shallow->_status, (int)shallow->_energy);
            Check(deepDead, "a turret with a deep crater under it loses its support and dies");
            Check(shallowAlive, "a turret on a scratch keeps standing");
            w->ClearTerrainCraters();
            stage = STAGE_PERF;
            perfStep = 0;
            frames = 0;
        }
        break;
    }

    case STAGE_PERF:
    {
        // Step 0: none. Step 1: the limit full of craters around the first spot.
        // Step 2: the same craters, each bomb with its real decal (the decal
        // pass over dug ground, as after a long fight).
        const Spot &s = spots.front();
        if (frames == 1)
        {
            w->ClearTerrainCraters();
            w->ClearGroundDecals();
            if (perfStep >= 1)
            {
                // The perf stage fills the limit on purpose; "no_limit" must not
                // ask a capture run for billions of craters.
                const int limit = std::min(
                    System::IniConf::GameDeformationMaxCraters.Get<int32_t>(), 2000);
                const uint64_t start = SDL_GetPerformanceCounter();
                int made = 0;
                for (int i = 0; i < limit * 4 && made < limit; ++i)
                {
                    const double x = s.digs.front().x + ((i * 173) % 2400) - 1200.0;
                    const double z = s.digs.front().z + ((i * 311) % 2400) - 1200.0;
                    const ypaworld_arg136 hit = Drop(w, x, z, w->_cells(s.cell).height);
                    if (!hit.isect)
                        continue;
                    if (perfStep == 2)
                        for (const World::TChainFXConfig &entry : w->_weaponProtos[38].chain_fx)
                            if (entry.mode == World::TChainFXConfig::MODE_GROUND_DECAL &&
                                entry.trigger == World::TChainFXConfig::TRIGGER_IMPACT_WORLD)
                            {
                                World::TChainFXConfig fx = entry;
                                fx.ground_decal_smoke.enabled = false;
                                w->SpawnGroundDecal(fx, hit);
                            }
                    if (w->SpawnTerrainCrater(TaerCrater(w), hit))
                        ++made;
                }
                fprintf(logFile, "PERF spawned %d craters in %.2f ms (kept %d), decals %d\n", made,
                        (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)SDL_GetPerformanceFrequency(),
                        (int)w->_terrainCraters.craters.size(), (int)w->_groundDecals.size());
            }
            rasterMs = 0.0;
            rasterCount = 0;
            renderGameMaxMs = 0.0;
            rebuildMaxMs = 0.0;
        }
        else if (frames == 3)
        {
            fprintf(logFile, "PERF step=%d first frames raster=%.3f ms, slowest RenderGame cpu=%.2f ms\n", perfStep,
                    rasterCount ? rasterMs / rasterCount : 0.0, renderGameMaxMs);
            renderGameMaxMs = 0.0;
        }
        else if (frames == 4)
        {
            rasterMs = 0.0;
            rasterCount = 0;
        }
        else if (frames == 64)
        {
            fprintf(logFile, "PERF step=%d slowest frame of crater rebuilds=%.2f ms\n", perfStep, rebuildMaxMs);
            fprintf(logFile, "PERF slowest piece %s, slowest seam %.2f ms%c", slowestPiece.c_str(), slowestSeamMs, 10);
            const double firstRays = RayMs(w, s);
            fprintf(logFile, "PERF step=%d craters=%d decals=%d raster=%.3f ms over %d frames, RenderGame cpu=%.2f ms (max %.2f), "
                    "4000 rays first=%.2f ms again=%.2f ms, copies pieces=%d collision=%d\n",
                    perfStep, (int)w->_terrainCraters.craters.size(), (int)w->_groundDecals.size(),
                    rasterCount ? rasterMs / rasterCount : 0.0,
                    rasterCount, renderGameMs, renderGameMaxMs, firstRays, RayMs(w, s),
                    (int)w->_terrainCraters.pieces.size(), (int)w->_terrainCraters.collision.size());
            if (perfStep == 0)
                captureName = "perf_view_2_after_bomb_no_craters";
            if (perfStep == 1)
                captureName = "perf_craters_no_decals";
            if (perfStep == 2)
            {
                capturePending = true;
                stage = STAGE_DONE;
            }
            else
            {
                ++perfStep;
                frames = 0;
            }
        }
        break;
    }

    case STAGE_TANKBOMB:
    {
        // A real Weapon 38 bomb on a ground unit (IsGroundUnit): the unit gets
        // neither crater nor decal, the ground right under it gets both.
        static NC_STACK_ypamissile *bomb = nullptr;
        static NC_STACK_ypabact *unit = nullptr;
        static vec3d last;
        static int settle = 0;
        const auto bombAlive = [&]()
        {
            if (!bomb || !w->_userRobo)
                return false;
            for (NC_STACK_ypabact *m : w->_userRobo->_missiles_list)
                if (m == bomb)
                    return bomb->_status != BACT_STATUS_DEAD;
            return false;
        };
        if (frames == 1)
        {
            w->ClearTerrainCraters();
            w->ClearGroundDecals();
            unit = nullptr;
            for (int y = 1; y < w->_mapSize.y - 1 && !unit; ++y)
                for (int x = 1; x < w->_mapSize.x - 1 && !unit; ++x)
                    for (NC_STACK_ypabact *bact : w->_cells(x, y).unitsList)
                        if (bact->IsGroundUnit() && bact->_status == BACT_STATUS_NORMAL && bact != w->_viewerBact)
                        {
                            unit = bact;
                            break;
                        }
            // None on the map: a tank of another owner on the first spot.
            for (size_t id = 0; !unit && id < w->_vhclProtos.size(); ++id)
            {
                if (w->_vhclProtos[id].model_id != BACT_TYPES_TANK)
                    continue;
                const vec3d at = spots.front().digs.front();
                ypaworld_arg146 request;
                request.vehicle_id = (int)id;
                request.pos = vec3d(at.x, GroundY(w, at.x, at.z, w->_cells(spots.front().cell).height) - 60.0, at.z);
                unit = w->ypaworld_func146(&request);
                if (unit)
                {
                    unit->_owner = w->_userRobo ? (w->_userRobo->_owner % 7) + 1 : 2;
                    w->ypaworld_func134(unit);
                    bact_arg80 placement;
                    placement.pos = request.pos;
                    placement.field_C = 1;
                    unit->SetPosition(&placement);
                }
            }
            Check(unit != nullptr, "a ground unit found for the bomb");
            if (!unit)
            {
                stage = STAGE_DONE;
                break;
            }
            last = unit->_position;
            ypaworld_arg146 arg;
            arg.vehicle_id = 38;
            arg.pos = vec3d(last.x, last.y - 700.0, last.z);
            bomb = w->_userRobo ? w->ypaworld_func147(&arg) : nullptr;
            if (bomb)
            {
                bomb->SetLauncherBact(w->_userRobo);
                bomb->SetStartHeight(arg.pos.y);
                bomb->_owner = w->_userRobo->_owner;
                bomb->_fly_dir = vec3d(0.0, 1.0, 0.0);
                bomb->_fly_dir_length = 0.0;
                bomb->_kidRef.Detach();
                bomb->_parent = nullptr;
                w->_userRobo->_missiles_list.push_back(bomb);
            }
            fprintf(logFile, "TANKBOMB unit type=%d vehicle=%d at %.0f %.0f %.0f bomb=%p\n", unit->_bact_type,
                    unit->_vehicleID, last.x, last.y, last.z, (void *)bomb);
            Check(bomb != nullptr, "real Weapon 38 bomb created over the ground unit");
            freeCamera = true;
            freeEye = last + vec3d(700.0, -450.0, 500.0);
            freeTarget = last;
        }
        else if (bombAlive() && frames < 2400)
        {
            // Keep the bomb over the unit: it may drive on.
            if (unit->_status != BACT_STATUS_DEAD)
                last = unit->_position;
            bomb->_position.x = last.x;
            bomb->_position.z = last.z;
            freeTarget = last;
        }
        else if (++settle == 60)
            captureName = "tankbomb_after";
        else if (settle > 62)
        {
            const size_t count = w->_terrainCraters.craters.size();
            double distance = -1.0, below = 0.0;
            int kind = -1;
            if (count)
            {
                const World::Crater::Shape &c = w->_terrainCraters.craters.front();
                distance = std::hypot(c.x - last.x, c.z - last.z);
                below = c.y - last.y;
                kind = c.kind;
            }
            fprintf(logFile, "TANKBOMB frames=%d craters=%d kind=%d distance=%.1f below unit=%.1f decals=%d\n",
                    frames, (int)count, kind, distance, below, (int)w->_groundDecals.size());
            Check(count == 1, "the bomb on the ground unit dug one crater");
            Check(count == 1 && kind == World::Crater::Shape::GROUND, "the crater is in the ground, not a dent");
            Check(count == 1 && distance < 80.0, "the crater is right under the unit");
            // The bomb decal (big), not the small one of the wrecked unit.
            bool bombDecal = false;
            for (const auto &decal : w->_groundDecals)
                if (decal.radius > 300.0f && std::hypot(decal.pos.x - last.x, decal.pos.z - last.z) < 80.0)
                    bombDecal = true;
            Check(bombDecal, "the ground under the unit got the bomb decal");
            stage = STAGE_DONE;
        }
        break;
    }

    case STAGE_DONE:
        if (capturePending)
            break;
        fprintf(logFile, "CHECKS %d FAILED %d\ndone glError=0x%x\n", checksTotal, checksFailed,
                (unsigned)glGetError());
        fclose(logFile);
        _exit(0);
    }
}

}

extern "C" std::list<GFX::TMesh> *__real__ZN17NC_STACK_ypaworld24TerrainCraterPieceMeshesB5cxx11ERK8cellAreaiiiRK6Tvec3dIdE(
    NC_STACK_ypaworld *, const cellArea &, int, int, int, const vec3d &);
extern "C" std::list<GFX::TMesh> *__wrap__ZN17NC_STACK_ypaworld24TerrainCraterPieceMeshesB5cxx11ERK8cellAreaiiiRK6Tvec3dIdE(
    NC_STACK_ypaworld *w, const cellArea &cell, int bldX, int bldY, int legoId, const vec3d &pos)
{
    const uint64_t start = SDL_GetPerformanceCounter();
    std::list<GFX::TMesh> *out = __real__ZN17NC_STACK_ypaworld24TerrainCraterPieceMeshesB5cxx11ERK8cellAreaiiiRK6Tvec3dIdE(
        w, cell, bldX, bldY, legoId, pos);
    const double ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    if (ms > slowestPieceMs)
    {
        slowestPieceMs = ms;
        size_t before = 0, after = 0, nearShapes = 0;
        for (const GFX::TMesh &m : w->_legoArray[legoId].Base->Meshes)
            before += m.Indixes.size() / 3;
        if (out)
            for (const GFX::TMesh &m : *out)
                after += m.Indixes.size() / 3;
        for (const World::Crater::Shape &s : w->_terrainCraters.craters)
            if (std::fabs(s.x - pos.x) < 700 && std::fabs(s.z - pos.z) < 700)
                ++nearShapes;
        char text[256];
        snprintf(text, sizeof(text), "%.2f ms cell=%d,%d piece=%d,%d lego=%d triangles %zu -> %zu, shapes near=%zu",
                 ms, cell.CellId.x, cell.CellId.y, bldX, bldY, legoId, before, after, nearShapes);
        slowestPiece = text;
    }
    return out;
}

extern "C" void __real__ZN17NC_STACK_ypaworld27ApplyTerrainCratersToFillerEP13TCellFillerCh(NC_STACK_ypaworld *, TCellFillerCh *);
extern "C" void __wrap__ZN17NC_STACK_ypaworld27ApplyTerrainCratersToFillerEP13TCellFillerCh(NC_STACK_ypaworld *w, TCellFillerCh *f)
{
    const uint64_t start = SDL_GetPerformanceCounter();
    __real__ZN17NC_STACK_ypaworld27ApplyTerrainCratersToFillerEP13TCellFillerCh(w, f);
    slowestSeamMs = std::max(slowestSeamMs, (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                                            (double)SDL_GetPerformanceFrequency());
}

extern "C" void __real__ZN6System4FlipEv();
extern "C" void __wrap__ZN6System4FlipEv()
{
    if (!captureName.empty())
    {
        SavePixels(outDir + "/" + captureName + ".ppm");
        captureName.clear();
    }
    if (capturePending)
    {
        capturePending = false;
        if (stage == STAGE_DONE)
            SavePixels(outDir + "/perf_full_limit.ppm");
        else
        {
            static const char *names[] = {"a_flat", "b_crater", "c_crater_decal"};
            SavePixels(outDir + "/" + spots[spotIndex].name + "_v" + std::to_string(view) + "_" +
                       names[pass] + ".ppm");
        }
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
    if ((stage != STAGE_RUN && stage != STAGE_BOMB && stage != STAGE_GAP && stage != STAGE_PERF && stage != STAGE_DONE &&
         stage != STAGE_TANKBOMB) ||
        spots.empty() || !w->_viewerBact)
    {
        __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(w, arg, a2);
        return;
    }

    const Spot *chosen = &spots.front();
    if (stage == STAGE_RUN)
        chosen = &spots[spotIndex];
    else if (stage == STAGE_GAP)
        for (const Spot &s : spots)
            if (s.name == "powerstation")
                chosen = &s;
    const Spot &spot = *chosen;
    const int v = stage == STAGE_RUN ? view : 0;
    TF::TForm3D *oldView = TF::Engine.GetViewPoint();
    const Common::Point oldCell = w->_viewerBact->_cellId;
    const vec3d oldPosition = w->_viewerBact->_position;

    vec3d eye = spot.eye[v], target = spot.target[v];
    if (freeCamera)
    {
        eye = freeEye;
        target = freeTarget;
    }
    else if (stage != STAGE_RUN && stage != STAGE_GAP)
    {
        // Wide view over the whole dug area.
        eye = spot.digs.front() + vec3d(1500.0, w->_cells(spot.cell).height - 1500.0, 1300.0);
        target = vec3d(spot.digs.front().x, w->_cells(spot.cell).height, spot.digs.front().z);
        if (cfgPlan == "perfnear")
        {
            eye = vec3d(spot.digs.front().x + 500.0, w->_cells(spot.cell).height - 260.0, spot.digs.front().z + 420.0);
            target = vec3d(spot.digs.front().x - 600.0, w->_cells(spot.cell).height + 80.0, spot.digs.front().z - 500.0);
        }
    }

    camera.Parent = nullptr;
    camera.flags = 0;
    camera.Pos = eye;
    camera.Scale = vec3d(1.0, 1.0, 1.0);
    camera.SclRot = LookAt(eye, target);
    TF::Engine.SetViewPoint(&camera);
    w->_viewerBact->_cellId = freeCamera ? World::PositionToSectorID(eye) : spot.cell;
    w->_viewerBact->_position = eye;

    const uint64_t start = SDL_GetPerformanceCounter();
    __real__ZN17NC_STACK_ypaworld10RenderGameEP10base_64argi(w, arg, a2);
    renderGameMs = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    renderGameMaxMs = std::max(renderGameMaxMs, renderGameMs);
    rebuildMaxMs = std::max(rebuildMaxMs, w->_terrainCraters.rebuildMs);

    w->_viewerBact->_cellId = oldCell;
    w->_viewerBact->_position = oldPosition;
    TF::Engine.SetViewPoint(oldView);
}

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
    if (stage != STAGE_PERF)
    {
        __real__ZN3GFX9GFXEngine9RasterizeEj(engine, stages);
        return;
    }

    glFinish();
    const uint64_t start = SDL_GetPerformanceCounter();
    __real__ZN3GFX9GFXEngine9RasterizeEj(engine, stages);
    glFinish();
    rasterMs += (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                (double)SDL_GetPerformanceFrequency();
    ++rasterCount;
}

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
