// Texture lookup order of an external BASE: set folder, BASE folder, subfolders (any set),
// shared set textures, active SET.BAS, Set1 SET.BAS. Same order as OpenNeoUAStudio.
// Run from Data: python src/world/tests/run_native_probe.py src/world/tests/external_texture_order_test.cpp
// (cwd is the game root "UA Rising"; part B needs the installed Set1/Set2 SET.BAS).
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "nucleus/ilbm.h"
#include "nucleus/embed.h"
#include "nucleus/bmpAnm.h"
#include "nucleus/particle.h"
#include "nucleus/amesh.h"
#include <cstdio>
#include <direct.h>
#include <fstream>
#include <string>
#include <vector>
#undef main

static int failures = 0;
static void check(bool ok, const char *label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    failures += !ok;
}

// Throw-away fixture files (C++14 build: no std::filesystem).
static std::vector<std::string> createdFiles, createdDirs;
static void touch(const std::string &path)
{
    size_t slash = 0;
    while ((slash = path.find('/', slash + 1)) != std::string::npos)
    {
        const std::string dir = path.substr(0, slash);
        if (_mkdir(dir.c_str()) == 0)
            createdDirs.push_back(dir);
    }
    std::ofstream(path, std::ios::binary) << "x";
    createdFiles.push_back(path);
}
static void cleanup()
{
    for (const std::string &file : createdFiles)
        std::remove(file.c_str());
    for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
        _rmdir(it->c_str());
}

static NC_STACK_ilbm *load(const std::string &name)
{
    NC_STACK_ilbm *bitmap = new NC_STACK_ilbm;
    IDVList args{{NC_STACK_rsrc::RSRC_ATT_NAME, name}, {NC_STACK_ilbm::ATT_ALPHAPALETTE, int32_t(0)}};
    if (bitmap->Init(args))
        return bitmap;
    delete bitmap;
    return nullptr;
}

int main()
{
    // ---- Part A: path order on a throw-away folder ------------------------------------------------
    const std::string fixture = "Data/Build/probes/texture_order_fixture";
    const std::string model = "Data/Models/Model/Model_Normal.BASE";
    const std::string root = fixture + "/Data/Models/Model";
    // The virtual file system scans the folder when the base directory is set: create the files first.
    touch(fixture + "/Data/Models/Sector/Set3/Textures/POOL.ILBM");
    touch(root + "/Textures/Set2/SUB.ILBM");
    touch(root + "/Parts/A/SUB2.ILBM");
    touch(root + "/Parts/Set3/SUB2.ILBM");
    touch(root + "/Parts/Z/SUB2.ILBM");
    touch(root + "/Parts/A/SUB3.ILBM");
    touch(root + "/Parts/B/SUB3.ILBM");
    touch(root + "/Textures/DEF.ILBM");
    touch(root + "/BESIDE.ILBM");
    touch(root + "/Textures/BESIDE.ILBM");
    touch(root + "/Textures/Set3/ALL.ILBM");
    touch(root + "/ALL.ILBM");
    FSMgr::iDir::setBaseDir(fixture);
    Common::Env.SetPrefix("data", "Data");
    Common::Env.SetPrefix("rsrc", "data:set3");
    IFFile::BeginExternalBaseResourceScope(model);

    auto resolve = [](const char *name) { return IFFile::ResolveExternalTexture(name); };
    using Src = IFFile::ExternalTexture;

    check(resolve("NONE.ILBM").source == Src::NONE, "missing texture resolves to nothing");

    check(resolve("POOL.ILBM").source == Src::SHARED_POOL, "shared set textures are the last folder step");

    auto sub = resolve("SUB.ILBM");
    check(sub.source == Src::SUBFOLDER && sub.otherSet && sub.candidates == 1,
          "another set's folder is searched as a subfolder and flagged");

    auto pick = resolve("SUB2.ILBM");
    check(pick.source == Src::SUBFOLDER && pick.candidates == 3 && !pick.otherSet &&
          pick.path.find("Parts/Set3/") != std::string::npos,
          "several subfolder files: the active set's folder wins");

    auto alpha = resolve("SUB3.ILBM");
    check(alpha.candidates == 2 && alpha.path.find("Parts/A/") != std::string::npos,
          "several subfolder files without the active set: alphabetical order");

    check(resolve("DEF.ILBM").source == Src::MODEL_FOLDER, "Textures/ default file");

    auto beside = resolve("BESIDE.ILBM");
    check(beside.source == Src::MODEL_FOLDER && beside.path == "Data/Models/Model/BESIDE.ILBM",
          "file beside the BASE comes before Textures/");

    check(resolve("ALL.ILBM").source == Src::SET_FOLDER, "Textures/Set<N> of the active set comes first");

    Common::Env.SetPrefix("rsrc", "data:set1");
    check(resolve("ALL.ILBM").source == Src::MODEL_FOLDER, "with Set1 active the Set3 folder is skipped, the BASE folder is used");
    IFFile::EndExternalBaseResourceScope();
    cleanup();

    // ---- Part B: archive order with the real SET.BAS files ----------------------------------------
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
    Common::Env.SetPrefix("rsrc", "data:set2");
    NC_STACK_base *set = load_set_base();
    check(set != nullptr, "Set2 SET.BAS loads");
    if (!set)
        return 1;

    IFFile::BeginExternalBaseResourceScope("Data/Models/Missing/Missing_Normal.BASE");
    auto *own = load("FBAGGER.ILBM");
    check(own != nullptr, "a texture of the active SET.BAS is found");
    auto *fallback = load("SCHROTT.ILBM");   // exists in Set1 only
    check(fallback != nullptr, "a texture missing from the active set comes from the Set1 SET.BAS");
    auto *again = load("SCHROTT.ILBM");
    check(fallback && again && fallback->IsSameRes(again), "the Set1 copy is shared by later requests");
    auto *nowhere = load("NOWHERE_AT_ALL.ILBM");
    check(nowhere == nullptr, "a texture found nowhere still fails");
    IFFile::EndExternalBaseResourceScope();

    for (auto *bitmap : {own, fallback, again})
        if (bitmap)
            bitmap->Delete();
    std::printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures;
}
