// Native set parser boundaries and extended model references; no game session.
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "nucleus/amesh.h"
#include "nucleus/bmpAnm.h"
#include "nucleus/embed.h"
#include "nucleus/particle.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#undef main

namespace
{
int checks = 0, failures = 0;
const char *fixture = "Data/Build/probes/sector_table_fixture.sdf";

void Check(bool value, const char *label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}

int Parse(NC_STACK_ypaworld *world, const std::string &text, bool sectors)
{
    { std::ofstream file(fixture); file << text; }
    FSMgr::FileHandle file(fixture, "r");
    if (!file.OK()) return -1;
    return sectors ? yw_parse_sektor(world, &file) : yw_parse_subSect(world, &file);
}
}

int main(int argc, char **argv)
{
    auto world = std::make_unique<NC_STACK_ypaworld>();
    if (argc == 3 && std::string(argv[1]) == "--real-set")
    {
        // Register asset readers directly; the full game initializer rewrites
        // the user's log files and is unnecessary for this loading probe.
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
        Common::Env.SetPrefix("rsrc", std::string("data:") + argv[2]);
        NC_STACK_base *set = load_set_base();
        Check(set && set->GetKidList().size() >= 2, "native engine loads generated set archive");
        if (set && set->GetKidList().size() >= 2)
        {
            auto lego = std::next(set->GetKidList().begin());
            std::unique_ptr<FSMgr::FileHandle> file(uaOpenFileAlloc("rsrc:scripts/set.sdf", "r"));
            Check(file && yw_parse_lego(world.get(), file.get(), *lego), "native engine loads extended SDF model rows");
            Check(file && yw_parse_subSect(world.get(), file.get()), "native engine loads generated damage states");
            Check(file && yw_parse_sektor(world.get(), file.get()), "native engine loads generated sector composition");
            world->LoadOverrideModels();
            Check(world->_legoArray[256].Base && world->_legoArray[256].Base->GetSkeleton(),
                  "native LEGO override supplies the new visual model");
            Check(world->_legoArray[256].CollisionSkelet && world->_legoArray[256].UseCollisionSkelet,
                  "new model keeps its collision skeleton");
            const auto *cell = world->_secTypeArray[1].SubSectors.At(1, 1);
            Check(cell && cell->HPModels[0] == 256 && cell->HPModels[3] == 256,
                  "generated sector retains model 256 in intact and destroyed states");
            world->FreeLegos();
            world->ClearOverrideModels();
            set->Delete();
        }
        std::printf("CHECKS %d FAILURES %d\n", checks, failures);
        return failures ? 1 : 0;
    }
    Check(world->_legoArray.size() == 4096 && world->_subSectorArray.size() == 4096,
          "extended tables contain 4096 slots");
    Check(world->_secTypeArray.size() == 256, "sector IDs retain the native byte range");
    Check(Parse(world.get(), "4095 300 256 0 1\n>\n", false) == 1,
          "model references above 255 load");
    Check(world->_subSectorArray[0].HPModels[0] == 4095 &&
          world->_subSectorArray[0].HPModels[1] == 300 &&
          world->_subSectorArray[0].HPModels[2] == 256,
          "model IDs are not truncated to a byte");
    Check(Parse(world.get(), "4096 0 0 0 1\n>\n", false) == 0, "model ID 4096 rejected");
    Check(Parse(world.get(), "-1 0 0 0 1\n>\n", false) == 0, "negative model rejected");
    Check(Parse(world.get(), "1bad 0 0 0 1\n>\n", false) == 0, "malformed model rejected");
    Check(Parse(world.get(), "0 1\n>\n", false) == 0, "incomplete state row rejected");
    std::ostringstream rows;
    for (size_t i = 0; i < SET_SUBSECTOR_LIMIT; ++i)
        rows << "4095 256 300 0 1\n";
    Check(Parse(world.get(), rows.str() + "; trailing comment\n>\n", false) == 1,
          "full table with trailing comments accepted");
    Check(world->_subSectorArray[4095].HPModels[0] == 4095, "last building row loaded");
    Check(Parse(world.get(), rows.str() + "0 0 0 0 0\n>\n", false) == 0,
          "building table overflow rejected");
    Check(Parse(world.get(), "5 1 0 73 4095\n>\n", true) == 1,
          "sector can reference extended building row");
    Check(world->_secTypeArray[5].SubSectors.At(0, 0) == &world->_subSectorArray[4095],
          "single-cell reference resolves exactly");
    Check(Parse(world.get(), "6 0 0 1 0 256 300 1 2 3 4095 4 5\n>\n", true) == 1,
          "nine-cell extended composition accepted");
    Check(world->_secTypeArray[6].SubSectors.At(2, 2) == &world->_subSectorArray[300] &&
          world->_secTypeArray[6].SubSectors.At(0, 0) == &world->_subSectorArray[4095],
          "native cell order retained");
    Check(Parse(world.get(), "256 1 0 1 0\n>\n", true) == 0, "sector ID 256 rejected");
    Check(Parse(world.get(), "-1 1 0 1 0\n>\n", true) == 0, "negative sector ID rejected");
    Check(Parse(world.get(), "5 1 0 1 4096\n>\n", true) == 0, "building ID 4096 rejected");
    NC_STACK_base container, child;
    for (size_t i = 0; i < SET_LEGO_LIMIT + 1; ++i)
        container.GetKidList().push_back(&child);
    { std::ofstream file(fixture); file << ">\n"; }
    FSMgr::FileHandle file(fixture, "r");
    Check(yw_parse_lego(world.get(), &file, &container) == 0,
          "oversized embedded LEGO table rejected before array access");
    container.GetKidList().clear();
    for (TLego &lego : world->_legoArray) lego.Base = nullptr;
    std::remove(fixture);
    std::printf("CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
