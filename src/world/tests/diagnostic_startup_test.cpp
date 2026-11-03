// Exercise the actual early-startup INI and filesystem path, before engines start.
#include "system/inivals.h"
#include "common/utils.h"
#include "common/crashdiag.h"
#include <cstdio>
#undef main

int main()
{
    System::IniConf::Init();
    FSMgr::iDir::setBaseDir("");
    const bool loaded = System::IniConf::ReadFromNucleusIni();
    const bool enabled = System::IniConf::GameCrashDiagnostics.Get<bool>();
    std::printf("loaded=%d enabled=%d nucleus=%s openneoua=%s output=%s\n",
                loaded, enabled, uaDataFirstNucleusIniPath().c_str(),
                uaDataFirstOpenNeoUAIniPath().c_str(),
                uaDataFirstResolvedWritePath("env/Diagnostics").c_str());
    CrashDiag::Init(enabled, "startup-integration-probe");
    CrashDiag::Breadcrumb("PROBE", "startup route reached");
    CrashDiag::Shutdown();
    return loaded && enabled ? 0 : 1;
}
