// Runs the real diagnostic service in a disposable process, without the game.
#include "common/crashdiag.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <direct.h>
#include <windows.h>

// Only the filesystem routing is replaced: each probe owns its working directory.
std::string uaDataFirstResolvedWritePath(const std::string &path) { return path; }
std::string uaDataFirstNucleusIniPath() { return "Nucleus.ini"; }
std::string uaDataFirstOpenNeoUAIniPath() { return "OpenNeoUA.ini"; }
bool uaCreateDir(const std::string &path) { return _mkdir(path.c_str()) == 0 || errno == EEXIST; }

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--openneoua-crash-dump") == 0)
    {
        if (std::getenv("OPENNEOUA_PROBE_WRITER_HANG"))
            Sleep(30000);
        if (std::getenv("OPENNEOUA_PROBE_WRITER_ERROR"))
            return 1234;
    }
    const int dumpWriter = CrashDiag::RunDumpWriter(argc, argv);
    if (dumpWriter >= 0)
        return dumpWriter;
    const std::string mode = argc > 1 ? argv[1] : "normal";
    if (mode == "writer_timeout")
        _putenv_s("OPENNEOUA_PROBE_WRITER_HANG", "yes");
    if (mode == "writer_error")
        _putenv_s("OPENNEOUA_PROBE_WRITER_ERROR", "yes");
    _mkdir("env");
    CrashDiag::Init(mode != "disabled", "diagnostics-test");
    CrashDiag::FrameBegin(1234, 2, 17);
    CrashDiag::UpdateWorldState(17, 42, 100, 101, 102);
    CrashDiag::SetPhase("ProbeSimulation");
    CrashDiag::SetActiveBact(reinterpret_cast<void *>(0x1234), 101, 3, 1, 2, 4, 500,
                             149, 10, -20, 30);
    {
        CrashDiag::ScopedActiveBact child(reinterpret_cast<void *>(0x5678), 202, 4, 2, 1, 8, 100,
                                           150, 40, -50, 60);
        CrashDiag::Breadcrumb("PROBE", "nested scope");
    }
    for (int i = 0; i < 2200; ++i)
        CrashDiag::Breadcrumb("PROBE", "event=%d", i);
    CrashDiag::Breadcrumb("PROBE", "long=%s", std::string(500, 'x').c_str());
    CrashDiag::RequestCheckpoint("probe_checkpoint");
    if (mode == "rotation")
    {
        const std::string detail(300, 'x');
        for (int i = 0; i < 60000; ++i)
        {
            CrashDiag::Breadcrumb("ROTATION", "event=%d %s", i, detail.c_str());
            if (i % 128 == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    std::fclose(std::fopen("Ready.txt", "wb"));

    if (mode == "av" || mode == "writer_timeout" || mode == "writer_error")
        *reinterpret_cast<volatile int *>(static_cast<uintptr_t>(1)) = 1;
    if (mode == "abort")
        std::abort();
    if (mode == "terminate")
        std::terminate();
    if (mode == "hang")
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(1400));
    if (mode == "forced")
        TerminateProcess(GetCurrentProcess(), 99);
    CrashDiag::FrameEnd(1250, 2, 17);
    CrashDiag::Shutdown();
    return 0;
}
