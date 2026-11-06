// Load the shipped BASE files with the production engine resource readers.
#include "game/yw.h"
#include "game/yw_internal.h"
#include "common/env.h"
#include "nucleus/amesh.h"
#include "nucleus/bmpAnm.h"
#include "nucleus/embed.h"
#include "nucleus/particle.h"
#include <cstdio>
#undef main

int main()
{
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
    Common::Env.SetPrefix("rsrc", "data:sets/set7");
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
        if (!ok) { ++failures; std::printf("FAIL %s\n", label); }
    };
    NC_STACK_base *set = load_set_base();
    check(set != nullptr, "active Set7 loads");
    if (set)
    {
        for (const char *path : {
            "Data/Models/Base/New/Hourglass/Hourglass_Dead/Hourglass_Dead.BASE",
            "Data/Models/Base/New/Static/Static_Dead/Static_Dead.BASE",
            "Data/Models/Base/New/Taer_Radar/Taer_Radar_Normal/Taer_Radar_Normal.BASE",
            "Data/Models/Base/New/Taer_Radar/Taer_Radar_Genesis/Taer_Radar_Genesis.BASE"})
        {
            NC_STACK_base *model = NC_STACK_base::LoadBaseFromFile(path);
            check(model && model->GetSkeleton(), path);
            if (model)
            {
                if (std::string(path).find("_Dead") != std::string::npos)
                {
                    int particles = 0;
                    for (const auto *ade : model->_ADES)
                        if (ade && ade->IsParticle()) ++particles;
                    check(particles == 2, "both Mykonian particle emitters load");
                }
                model->Delete();
            }
        }
        set->Delete();
    }
    std::printf("ENGINE MODEL LOAD FAILURES=%d\n", failures);
    return failures ? 1 : 0;
}
