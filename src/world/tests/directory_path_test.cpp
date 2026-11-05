#include "common/utils.h"
#include "system/fsmgr.h"
#include <cstdio>

#undef main

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;

    FSMgr::iDir::setBaseDir(argv[1]);
    auto child = uaOpenDir("OpenNeoUA.dir/C_/");
    FSMgr::iNode *node = nullptr;
    const bool correctChild = child && child.getNext(&node) && node &&
                              node->getName() == "inside.cfg" && !child.getNext(&node);
    const bool childFile = uaOpenFile("OpenNeoUA.dir/C_/inside.cfg", "r").OK();
    const bool longName = static_cast<bool>(uaOpenDir("assets.directory"));
    const bool legacyExtension = correctSeparatorAndExt("folder.dir/example.texture") ==
                                 "folder.dir\\example.tex";
    const bool extensionless = correctSeparatorAndExt("folder.dir/extensionless") ==
                               "folder.dir\\extensionless";
    const bool plainFile = correctSeparatorAndExt("example.texture") == "example.tex";
    const bool collision = correctSeparatorAndExt("folder.dir/example.collision") ==
                           "folder.dir\\example.collision";
    std::printf("Directory paths: child=%d, child file=%d, long name=%d, legacy extension=%d, extensionless=%d, plain file=%d, collision=%d\n",
                correctChild, childFile, longName, legacyExtension, extensionless, plainFile, collision);
    return correctChild && childFile && longName && legacyExtension && extensionless && plainFile && collision ? 0 : 1;
}
