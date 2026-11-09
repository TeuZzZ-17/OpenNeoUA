// Attached status FX silhouette probe (damaged/debuff/regen/drain % offsets).
// Uses real vanilla Set1 VP models (no collision_shape, as in vanilla scripts)
// and a real collision profile, through the production
// NC_STACK_ypaworld::SampleAttachedFXLocalPosition.
// Run from Data: python src/world/tests/run_native_probe.py src/world/tests/attached_fx_silhouette_test.cpp
#include "game/yw.h"
#include "game/yw_internal.h"
#include "game/ypabact.h"
#include "game/yw_impl.h"
#include "common/env.h"
#include "nucleus/amesh.h"
#include "nucleus/bmpAnm.h"
#include "nucleus/embed.h"
#include "nucleus/particle.h"
#include "../collision_shape.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#undef main

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char *label) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n", label); } }

struct TestWorld : NC_STACK_ypaworld {};
struct Unit : NC_STACK_ypabact { void SetWorld(NC_STACK_ypaworld *w) { _world = w; } };

struct Bounds { vec3d lo = vec3d(1e30, 1e30, 1e30), hi = vec3d(-1e30, -1e30, -1e30);
    void Add(const vec3d &p) { lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
                               hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z); } };

void ModelBounds(NC_STACK_base *base, const mat3x3 &rot, const vec3d &pos, Bounds *b)
{
    for (const GFX::TMesh &mesh : base->Meshes)
        for (const auto &v : mesh.Vertexes) b->Add(pos + rot.Transform(static_cast<vec3d>(v.Pos)));
    for (NC_STACK_base *kid : base->GetKidList())
        ModelBounds(kid, rot * kid->TForm().SclRot, pos + rot.Transform(kid->TForm().Pos), b);
}

// True when p lies inside (or within tol of) at least one convex hull.
bool InsideProfile(const Collision::Shape &shape, const vec3d &p, double tol)
{
    for (const Collision::Part &part : shape.parts)
    {
        vec3d c(0, 0, 0);
        for (const vec3d &v : part.vertices) c += v;
        c = c * (1.0 / part.vertices.size());
        bool inside = true;
        for (const auto &f : part.faces)
        {
            const vec3d &a = part.vertices[f[0]], &b = part.vertices[f[1]], &d = part.vertices[f[2]];
            vec3d n = (b - a) * (d - a);
            double len = n.length();
            if (len < 1e-9) continue;
            n = n * (1.0 / len);
            if ((c - a).dot(n) > 0) n = n * -1.0;
            if ((p - a).dot(n) > tol) { inside = false; break; }
        }
        if (inside) return true;
    }
    return false;
}
}

int main(int argc, char **argv)
{
    // Optional: --dump <dir> writes every silhouette and model vertex as CSV for visual checks.
    const char *dumpDir = (argc > 2 && std::string(argv[1]) == "--dump") ? argv[2] : nullptr;
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_nucleus>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_rsrc>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_bitmap>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_skeleton>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ilbm>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_sklt>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_ade>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_area>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_base>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_bmpanim>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_amesh>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_particle>());
    Nucleus::ClassList.push_back(Nucleus::MakeClassDescr<NC_STACK_embed>());
    FSMgr::iDir::setBaseDir(".");
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("rsrc", "data:sets/set1");
    std::srand(1);

    NC_STACK_base *set = load_set_base();
    Check(set && !set->GetKidList().empty(), "Set1 loads");
    if (!set) { std::printf("checks=%d failures=%d\n", checks, failures); return 1; }
    std::vector<NC_STACK_base *> vps(set->GetKidList().begin(), set->GetKidList().end());
    vps = std::vector<NC_STACK_base *>(vps[0]->GetKidList().begin(), vps[0]->GetKidList().end());

    // Vanilla VP numbers taken from vp_normal in Vehicles.cfg.
    struct { int vp; const char *label; } models[] = {
        {30, "tank 1"}, {146, "tank 3 (Tiger)"}, {169, "car 11"}, {71, "plane 4"}, {77, "plane 8"},
        {83, "heli 6"}, {48, "heli 10"}, {24, "heli 14"}, {42, "ufo 9"}, {115, "robo 56"}};

    World::TAbsoluteOrPercent full, core;
    full.defined = core.defined = true;
    full.percent = core.percent = true;
    full.value = 100.0f;
    core.value = 25.0f;

    for (const auto &m : models)
    {
        if ((size_t)m.vp >= vps.size() || !vps[m.vp]) { Check(false, m.label); continue; }
        TestWorld world;
        Unit unit;
        unit.SetWorld(&world);
        unit._vp_normal = vps[m.vp];
        unit._vp_scale = vec3d(1, 1, 1);
        unit._vp_rotation = vec3d(0, 0, 0);

        Bounds model;
        ModelBounds(vps[m.vp], mat3x3::Ident(), vec3d(0, 0, 0), &model);
        const double height = model.hi.y - model.lo.y;

        vec3d p;
        auto t0 = std::chrono::steady_clock::now();
        bool ok = world.SampleAttachedFXLocalPosition(&unit, full, &p);
        double buildMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        Check(ok && world._attachedFXGeometryCache.size() == 1 &&
              !world._attachedFXGeometryCache[0].cells.empty(), m.label);
        if (!ok || world._attachedFXGeometryCache.empty()) continue;
        const auto &cache = world._attachedFXGeometryCache[0];
        const double cell = cache.cellSize;

        Bounds samples;
        int top10 = 0, outside = 0;
        double maxFull = 0;
        for (int i = 0; i < 4000; i++)
        {
            world.SampleAttachedFXLocalPosition(&unit, full, &p);
            samples.Add(p);
            maxFull = std::max(maxFull, (p - cache.centre).length());
            if (p.y < model.lo.y + height * 0.1) ++top10; // y down: low y = top
            if (p.x < model.lo.x - cell || p.x > model.hi.x + cell || p.y < model.lo.y - cell ||
                p.y > model.hi.y + cell || p.z < model.lo.z - cell || p.z > model.hi.z + cell) ++outside;
        }
        Check(outside == 0, "100% samples stay inside the model box");
        Check(world._attachedFXGeometryCache.size() == 1, "silhouette cached once");

        double maxCore = 0;
        for (int i = 0; i < 4000; i++)
        {
            world.SampleAttachedFXLocalPosition(&unit, core, &p);
            maxCore = std::max(maxCore, (p - cache.centre).length());
        }
        Check(maxCore <= maxFull * 0.25 + cell, "25% stays in the central core");

        if (dumpDir)
        {
            std::string path = std::string(dumpDir) + "/vp" + std::to_string(m.vp) + ".csv";
            if (FILE *out = std::fopen(path.c_str(), "w"))
            {
                for (const vec3d &c : cache.cells) std::fprintf(out, "c,%f,%f,%f\n", c.x, c.y, c.z);
                std::vector<yw_AttachedFXTriangle> tris;
                yw_CollectAttachedFXTriangles(vps[m.vp], mat3x3::Ident(), vec3d(0, 0, 0), &tris);
                for (const auto &t : tris) for (const vec3d &v : t) std::fprintf(out, "m,%f,%f,%f\n", v.x, v.y, v.z);
                std::fclose(out);
            }
        }

        std::printf("%-15s cells=%4zu cell=%6.2f build=%6.2fms model y[%7.1f..%7.1f] centre(%6.1f,%6.1f,%6.1f) "
                    "top10%%=%5.1f%% maxR100=%6.1f maxR25=%6.1f\n",
                    m.label, cache.cells.size(), cell, buildMs, model.lo.y, model.hi.y,
                    cache.centre.x, cache.centre.y, cache.centre.z, 100.0 * top10 / 4000, maxFull, maxCore);
    }

    // Collision profile wins over the visual model when present.
    {
        std::string error;
        auto shape = Collision::Load("Data/Collisions/Tiger.collision", &error);
        Check(shape != nullptr, "Tiger profile loads");
        if (shape)
        {
            TestWorld world;
            Unit unit;
            unit.SetWorld(&world);
            unit._vp_normal = vps[146];
            unit._vp_scale = shape->visualScale;
            unit._vp_rotation = shape->visualRotation;
            unit._collisionShape = shape;
            vec3d p;
            Check(world.SampleAttachedFXLocalPosition(&unit, full, &p), "profile sample");
            Check(world._attachedFXGeometryCache.size() == 1 &&
                  world._attachedFXGeometryCache[0].source == shape.get(), "profile used, visual not built");
            const double cell = world._attachedFXGeometryCache[0].cellSize;
            int outside = 0;
            for (int i = 0; i < 4000; i++)
            {
                world.SampleAttachedFXLocalPosition(&unit, full, &p);
                if (!InsideProfile(*shape, p, cell * 1.75)) ++outside; // voxel resolution: cell centre + jitter
            }
            Check(outside == 0, "profile samples stay inside the hulls");
            std::printf("Tiger profile   cells=%4zu cell=%6.2f outside=%d\n",
                        world._attachedFXGeometryCache[0].cells.size(), cell, outside);

            // An empty profile falls back to the visual model.
            auto empty = std::make_shared<Collision::Shape>();
            unit._collisionShape = empty;
            TestWorld fallbackWorld;
            unit.SetWorld(&fallbackWorld);
            Check(fallbackWorld.SampleAttachedFXLocalPosition(&unit, full, &p) &&
                  fallbackWorld._attachedFXGeometryCache.size() == 1 &&
                  fallbackWorld._attachedFXGeometryCache[0].source == unit._vp_normal, "empty profile falls back to visual");
            unit._collisionShape.reset();
        }
    }

    // Absolute offsets keep the old radial sphere; no geometry is built.
    {
        TestWorld world;
        Unit unit;
        unit.SetWorld(&world);
        unit._vp_normal = vps[30];
        World::TAbsoluteOrPercent absolute;
        absolute.defined = true;
        absolute.value = 40.0f;
        vec3d p;
        double maxR = 0;
        for (int i = 0; i < 2000; i++) { world.SampleAttachedFXLocalPosition(&unit, absolute, &p); maxR = std::max(maxR, p.length()); }
        Check(maxR <= 40.0 + 1e-3 && world._attachedFXGeometryCache.empty(), "absolute offset unchanged");
    }

    std::printf("checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
