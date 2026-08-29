#include "crashdiag.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#else
#include <unistd.h>
#endif

namespace CrashDiag
{
namespace
{

constexpr size_t BREADCRUMB_COUNT = 2048;
constexpr size_t BREADCRUMB_TEXT_SIZE = 512;
constexpr uint64_t WATCHDOG_POLL_MS = 100;
constexpr uint64_t HANG_THRESHOLD_MS = 2500;
constexpr uint64_t HANG_RECOVERED_MS = 500;

struct BreadcrumbSlot
{
    uint64_t sequence = 0;
    char text[BREADCRUMB_TEXT_SIZE] = {};
};

struct RuntimeState
{
    std::atomic<uint64_t> frameStarted{0};
    std::atomic<uint64_t> frameCompleted{0};
    std::atomic<uint32_t> gameTime{0};
    std::atomic<int> screenMode{0};
    std::atomic<int> levelId{-1};
    std::atomic<int> unitCount{0};
    std::atomic<int> userRoboGid{0};
    std::atomic<int> userUnitGid{0};
    std::atomic<int> viewerGid{0};

    std::atomic<uintptr_t> activePtr{0};
    std::atomic<int> activeGid{0};
    std::atomic<int> activeType{0};
    std::atomic<int> activeOwner{0};
    std::atomic<int> activeStatus{0};
    std::atomic<int> activeStatusFlags{0};
    std::atomic<int> activeEnergy{0};
};

std::atomic<bool> g_enabled{false};
std::atomic<bool> g_running{false};
std::atomic<bool> g_watchdogArmed{false};
std::atomic<bool> g_hangActive{false};
std::atomic<bool> g_checkpointPending{false};
std::atomic<uint64_t> g_lastHeartbeatMs{0};
std::atomic<uint64_t> g_sequence{0};
std::atomic<unsigned int> g_hangReportIndex{0};
std::atomic<unsigned int> g_checkpointIndex{0};
std::atomic<const char *> g_phase{"Disabled"};

RuntimeState g_state;
std::array<BreadcrumbSlot, BREADCRUMB_COUNT> g_breadcrumbs;
std::mutex g_ringMutex;
std::mutex g_fileMutex;
std::mutex g_checkpointMutex;
char g_checkpointReason[128] = {};

uint64_t g_startMs = 0;
std::thread g_watchdogThread;
std::string g_sessionId;
std::string g_sessionDirLogical;
std::string g_sessionDirPhysical;
std::string g_buildTag;
std::terminate_handler g_previousTerminate = nullptr;

#ifdef _WIN32
LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = nullptr;
std::atomic<bool> g_crashReportStarted{false};
#endif

uint64_t NowMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

unsigned long CurrentThreadIdValue()
{
#ifdef _WIN32
    return static_cast<unsigned long>(GetCurrentThreadId());
#else
    return static_cast<unsigned long>(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
}

std::string MakeTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm tmValue = {};
#ifdef _WIN32
    localtime_s(&tmValue, &now);
#else
    localtime_r(&now, &tmValue);
#endif

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d_%02d%02d%02d",
                  tmValue.tm_year + 1900,
                  tmValue.tm_mon + 1,
                  tmValue.tm_mday,
                  tmValue.tm_hour,
                  tmValue.tm_min,
                  tmValue.tm_sec);
    return buffer;
}

std::string JoinPath(const std::string &directory, const std::string &name)
{
    if ( directory.empty() )
        return name;

    const char last = directory[directory.size() - 1];
    if ( last == '/' || last == '\\' )
        return directory + name;

#ifdef _WIN32
    return directory + "\\" + name;
#else
    return directory + "/" + name;
#endif
}

void WriteRuntimeState(FILE *file)
{
    const char *phase = g_phase.load(std::memory_order_acquire);
    if ( !phase )
        phase = "Unknown";

    std::fprintf(file, "phase=%s\n", phase);
    std::fprintf(file, "frame_started=%llu\n",
                 static_cast<unsigned long long>(g_state.frameStarted.load()));
    std::fprintf(file, "frame_completed=%llu\n",
                 static_cast<unsigned long long>(g_state.frameCompleted.load()));
    std::fprintf(file, "game_time=%u\n", g_state.gameTime.load());
    std::fprintf(file, "screen_mode=%d\n", g_state.screenMode.load());
    std::fprintf(file, "level_id=%d\n", g_state.levelId.load());
    std::fprintf(file, "unit_count=%d\n", g_state.unitCount.load());
    std::fprintf(file, "user_robo_gid=%d\n", g_state.userRoboGid.load());
    std::fprintf(file, "user_unit_gid=%d\n", g_state.userUnitGid.load());
    std::fprintf(file, "viewer_gid=%d\n", g_state.viewerGid.load());
    std::fprintf(file, "active_ptr=0x%llx\n",
                 static_cast<unsigned long long>(g_state.activePtr.load()));
    std::fprintf(file, "active_gid=%d\n", g_state.activeGid.load());
    std::fprintf(file, "active_type=%d\n", g_state.activeType.load());
    std::fprintf(file, "active_owner=%d\n", g_state.activeOwner.load());
    std::fprintf(file, "active_status=%d\n", g_state.activeStatus.load());
    std::fprintf(file, "active_status_flags=0x%x\n", g_state.activeStatusFlags.load());
    std::fprintf(file, "active_energy=%d\n", g_state.activeEnergy.load());
}

void WriteBreadcrumbs(FILE *file)
{
    std::unique_lock<std::mutex> lock(g_ringMutex, std::defer_lock);
    if ( !lock.try_lock() )
    {
        std::fprintf(file, "\n[breadcrumbs unavailable: ring busy]\n");
        return;
    }

    const uint64_t newest = g_sequence.load(std::memory_order_acquire);
    const uint64_t oldest = newest > BREADCRUMB_COUNT ? newest - BREADCRUMB_COUNT + 1 : 1;

    std::fprintf(file, "\n[Breadcrumbs %llu..%llu]\n",
                 static_cast<unsigned long long>(oldest),
                 static_cast<unsigned long long>(newest));

    for (uint64_t seq = oldest; seq <= newest; ++seq)
    {
        const size_t index = static_cast<size_t>((seq - 1) % BREADCRUMB_COUNT);
        const BreadcrumbSlot &slot = g_breadcrumbs[index];
        if ( slot.sequence == seq )
            std::fprintf(file, "%s\n", slot.text);
    }
}

bool WriteSnapshotFile(const std::string &fileName,
                       const char *reportType,
                       const char *reason)
{
    std::lock_guard<std::mutex> fileLock(g_fileMutex);

    const std::string path = JoinPath(g_sessionDirPhysical, fileName);
    FILE *file = std::fopen(path.c_str(), "wb");
    if ( !file )
        return false;

    std::fprintf(file, "OpenNeoUA Diagnostics\n");
    std::fprintf(file, "report_type=%s\n", reportType ? reportType : "snapshot");
    std::fprintf(file, "reason=%s\n", reason ? reason : "none");
    std::fprintf(file, "session=%s\n", g_sessionId.c_str());
    std::fprintf(file, "build=%s\n", g_buildTag.c_str());
    std::fprintf(file, "elapsed_ms=%llu\n",
                 static_cast<unsigned long long>(NowMs() - g_startMs));
    std::fprintf(file, "thread_id=%lu\n", CurrentThreadIdValue());
    WriteRuntimeState(file);
    WriteBreadcrumbs(file);

    std::fclose(file);
    return true;
}

void WriteSessionInfo()
{
    const std::string path = JoinPath(g_sessionDirPhysical, "SessionInfo.txt");
    FILE *file = std::fopen(path.c_str(), "wb");
    if ( !file )
        return;

    std::fprintf(file, "OpenNeoUA crash diagnostics session\n");
    std::fprintf(file, "session=%s\n", g_sessionId.c_str());
    std::fprintf(file, "build=%s\n", g_buildTag.c_str());
    std::fprintf(file, "nucleus_ini=%s\n", uaDataFirstNucleusIniPath().c_str());
#ifdef _WIN32
    std::fprintf(file, "platform=Windows\n");
    std::fprintf(file, "process_id=%lu\n", static_cast<unsigned long>(GetCurrentProcessId()));
#else
    std::fprintf(file, "platform=non-Windows\n");
    std::fprintf(file, "process_id=%ld\n", static_cast<long>(getpid()));
#endif
    std::fprintf(file, "hang_threshold_ms=%llu\n",
                 static_cast<unsigned long long>(HANG_THRESHOLD_MS));
    std::fprintf(file, "breadcrumb_capacity=%zu\n", BREADCRUMB_COUNT);
    std::fclose(file);
}

void ConsumeCheckpoint()
{
    if ( !g_checkpointPending.exchange(false) )
        return;

    char reason[sizeof(g_checkpointReason)] = {};
    {
        std::lock_guard<std::mutex> lock(g_checkpointMutex);
        std::strncpy(reason, g_checkpointReason, sizeof(reason) - 1);
        g_checkpointReason[0] = '\0';
    }

    const unsigned int index = g_checkpointIndex.fetch_add(1) + 1;
    char fileName[64];
    std::snprintf(fileName, sizeof(fileName), "Checkpoint_%03u.txt", index);
    WriteSnapshotFile(fileName, "checkpoint", reason[0] ? reason : "requested");
}

void WatchdogLoop()
{
    while ( g_running.load(std::memory_order_acquire) )
    {
        ConsumeCheckpoint();

        if ( g_watchdogArmed.load(std::memory_order_acquire) &&
             g_state.screenMode.load(std::memory_order_acquire) == 2 )
        {
            const uint64_t now = NowMs();
            const uint64_t heartbeat = g_lastHeartbeatMs.load(std::memory_order_acquire);
            const uint64_t elapsed = now >= heartbeat ? now - heartbeat : 0;

            if ( elapsed >= HANG_THRESHOLD_MS && !g_hangActive.exchange(true) )
            {
                const char *phase = g_phase.load(std::memory_order_acquire);
                Breadcrumb("WATCHDOG", "hang detected elapsed=%llums phase=%s",
                           static_cast<unsigned long long>(elapsed),
                           phase ? phase : "Unknown");

                const unsigned int index = g_hangReportIndex.fetch_add(1) + 1;
                char fileName[64];
                std::snprintf(fileName, sizeof(fileName), "HangReport_%03u.txt", index);
                WriteSnapshotFile(fileName, "hang", "main heartbeat timeout");
            }
            else if ( elapsed <= HANG_RECOVERED_MS && g_hangActive.exchange(false) )
            {
                Breadcrumb("WATCHDOG", "main loop recovered after a reported hang");
                RequestCheckpoint("hang_recovered");
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(WATCHDOG_POLL_MS));
    }

    ConsumeCheckpoint();
}

void TerminateHandler()
{
    Breadcrumb("CRASH", "std::terminate invoked");
    WriteSnapshotFile("TerminateReport.txt", "terminate", "std::terminate");

    if ( g_previousTerminate && g_previousTerminate != &TerminateHandler )
        g_previousTerminate();

    std::abort();
}

#ifdef _WIN32
void WriteHandleText(HANDLE file, const char *text)
{
    if ( file == INVALID_HANDLE_VALUE || !text )
        return;

    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

void WriteHandleFormat(HANDLE file, const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    WriteHandleText(file, buffer);
}

void WriteExceptionStackTrace(HANDLE file, CONTEXT *context)
{
    if ( file == INVALID_HANDLE_VALUE || !context )
        return;

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT localContext = *context;
    STACKFRAME64 frame = {};
    DWORD machineType = 0;

#if defined(_M_X64) || defined(__x86_64__)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = localContext.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = localContext.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = localContext.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IX86) || defined(__i386__)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = localContext.Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = localContext.Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = localContext.Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#else
    WriteHandleText(file, "\r\n[stack trace unsupported on this architecture]\r\n");
    return;
#endif

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    if ( !SymInitialize(process, nullptr, TRUE) )
    {
        WriteHandleFormat(file, "\r\n[SymInitialize failed error=%lu]\r\n",
                          static_cast<unsigned long>(GetLastError()));
        return;
    }

    WriteHandleText(file, "\r\n[Stack trace]\r\n");
    for (unsigned int index = 0; index < 64; ++index)
    {
        if ( !StackWalk64(machineType, process, thread, &frame, &localContext,
                          nullptr, SymFunctionTableAccess64, SymGetModuleBase64,
                          nullptr) )
        {
            break;
        }

        const DWORD64 address = frame.AddrPC.Offset;
        if ( !address )
            break;

        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;

        const BOOL hasSymbol = SymFromAddr(process, address, &displacement, symbol);
        const BOOL hasLine = SymGetLineFromAddr64(process, address,
                                                  &lineDisplacement, &line);

        if ( hasSymbol && hasLine )
        {
            WriteHandleFormat(file, "#%02u 0x%llx %s+0x%llx %s:%lu\r\n",
                              index,
                              static_cast<unsigned long long>(address),
                              symbol->Name,
                              static_cast<unsigned long long>(displacement),
                              line.FileName ? line.FileName : "unknown",
                              static_cast<unsigned long>(line.LineNumber));
        }
        else if ( hasSymbol )
        {
            WriteHandleFormat(file, "#%02u 0x%llx %s+0x%llx\r\n",
                              index,
                              static_cast<unsigned long long>(address),
                              symbol->Name,
                              static_cast<unsigned long long>(displacement));
        }
        else
        {
            WriteHandleFormat(file, "#%02u 0x%llx\r\n", index,
                              static_cast<unsigned long long>(address));
        }
    }

    SymCleanup(process);
}

void WriteEmergencyBreadcrumbs(HANDLE file)
{
    std::unique_lock<std::mutex> lock(g_ringMutex, std::defer_lock);
    if ( !lock.try_lock() )
    {
        WriteHandleText(file, "\r\n[breadcrumbs unavailable: ring busy]\r\n");
        return;
    }

    const uint64_t newest = g_sequence.load(std::memory_order_acquire);
    const uint64_t oldest = newest > BREADCRUMB_COUNT ? newest - BREADCRUMB_COUNT + 1 : 1;
    WriteHandleFormat(file, "\r\n[Breadcrumbs %llu..%llu]\r\n",
                      static_cast<unsigned long long>(oldest),
                      static_cast<unsigned long long>(newest));

    for (uint64_t seq = oldest; seq <= newest; ++seq)
    {
        const size_t index = static_cast<size_t>((seq - 1) % BREADCRUMB_COUNT);
        const BreadcrumbSlot &slot = g_breadcrumbs[index];
        if ( slot.sequence == seq )
        {
            WriteHandleText(file, slot.text);
            WriteHandleText(file, "\r\n");
        }
    }
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS *exceptionInfo)
{
    if ( g_crashReportStarted.exchange(true) )
        return EXCEPTION_EXECUTE_HANDLER;

    const std::string reportPath = JoinPath(g_sessionDirPhysical, "CrashReport.txt");
    HANDLE report = CreateFileA(reportPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if ( report != INVALID_HANDLE_VALUE )
    {
        const EXCEPTION_RECORD *record = exceptionInfo ? exceptionInfo->ExceptionRecord : nullptr;
        const CONTEXT *context = exceptionInfo ? exceptionInfo->ContextRecord : nullptr;
        const void *address = record ? record->ExceptionAddress : nullptr;

        MEMORY_BASIC_INFORMATION memoryInfo = {};
        HMODULE module = nullptr;
        char modulePath[MAX_PATH] = {};
        uintptr_t moduleBase = 0;
        uintptr_t rva = 0;

        if ( address && VirtualQuery(address, &memoryInfo, sizeof(memoryInfo)) )
        {
            module = static_cast<HMODULE>(memoryInfo.AllocationBase);
            moduleBase = reinterpret_cast<uintptr_t>(memoryInfo.AllocationBase);
            rva = reinterpret_cast<uintptr_t>(address) - moduleBase;
            if ( module )
                GetModuleFileNameA(module, modulePath, MAX_PATH);
        }

        WriteHandleText(report, "OpenNeoUA Diagnostics\r\n");
        WriteHandleText(report, "report_type=unhandled_exception\r\n");
        WriteHandleFormat(report, "session=%s\r\n", g_sessionId.c_str());
        WriteHandleFormat(report, "build=%s\r\n", g_buildTag.c_str());
        WriteHandleFormat(report, "thread_id=%lu\r\n", static_cast<unsigned long>(GetCurrentThreadId()));
        WriteHandleFormat(report, "exception_code=0x%08lx\r\n",
                          record ? static_cast<unsigned long>(record->ExceptionCode) : 0UL);
        WriteHandleFormat(report, "exception_flags=0x%08lx\r\n",
                          record ? static_cast<unsigned long>(record->ExceptionFlags) : 0UL);
        WriteHandleFormat(report, "exception_address=0x%llx\r\n",
                          static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(address)));
        WriteHandleFormat(report, "module=%s\r\n", modulePath[0] ? modulePath : "unknown");
        WriteHandleFormat(report, "module_base=0x%llx\r\n",
                          static_cast<unsigned long long>(moduleBase));
        WriteHandleFormat(report, "module_rva=0x%llx\r\n",
                          static_cast<unsigned long long>(rva));

        if ( record && record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2 )
        {
            const char *access = "unknown";
            if ( record->ExceptionInformation[0] == 0 )
                access = "read";
            else if ( record->ExceptionInformation[0] == 1 )
                access = "write";
            else if ( record->ExceptionInformation[0] == 8 )
                access = "execute";

            WriteHandleFormat(report, "access_type=%s\r\n", access);
            WriteHandleFormat(report, "access_address=0x%llx\r\n",
                              static_cast<unsigned long long>(record->ExceptionInformation[1]));
        }

        const char *phase = g_phase.load(std::memory_order_acquire);
        WriteHandleFormat(report, "phase=%s\r\n", phase ? phase : "Unknown");
        WriteHandleFormat(report, "frame_started=%llu\r\n",
                          static_cast<unsigned long long>(g_state.frameStarted.load()));
        WriteHandleFormat(report, "frame_completed=%llu\r\n",
                          static_cast<unsigned long long>(g_state.frameCompleted.load()));
        WriteHandleFormat(report, "game_time=%u\r\n", g_state.gameTime.load());
        WriteHandleFormat(report, "screen_mode=%d\r\n", g_state.screenMode.load());
        WriteHandleFormat(report, "level_id=%d\r\n", g_state.levelId.load());
        WriteHandleFormat(report, "unit_count=%d\r\n", g_state.unitCount.load());
        WriteHandleFormat(report, "user_robo_gid=%d\r\n", g_state.userRoboGid.load());
        WriteHandleFormat(report, "user_unit_gid=%d\r\n", g_state.userUnitGid.load());
        WriteHandleFormat(report, "viewer_gid=%d\r\n", g_state.viewerGid.load());
        WriteHandleFormat(report, "active_ptr=0x%llx\r\n",
                          static_cast<unsigned long long>(g_state.activePtr.load()));
        WriteHandleFormat(report, "active_gid=%d\r\n", g_state.activeGid.load());
        WriteHandleFormat(report, "active_type=%d\r\n", g_state.activeType.load());
        WriteHandleFormat(report, "active_owner=%d\r\n", g_state.activeOwner.load());
        WriteHandleFormat(report, "active_status=%d\r\n", g_state.activeStatus.load());
        WriteHandleFormat(report, "active_status_flags=0x%x\r\n", g_state.activeStatusFlags.load());
        WriteHandleFormat(report, "active_energy=%d\r\n", g_state.activeEnergy.load());

#if defined(_M_X64) || defined(__x86_64__)
        if ( context )
        {
            WriteHandleFormat(report, "RIP=0x%llx RSP=0x%llx RBP=0x%llx\r\n",
                              static_cast<unsigned long long>(context->Rip),
                              static_cast<unsigned long long>(context->Rsp),
                              static_cast<unsigned long long>(context->Rbp));
            WriteHandleFormat(report, "RAX=0x%llx RBX=0x%llx RCX=0x%llx RDX=0x%llx\r\n",
                              static_cast<unsigned long long>(context->Rax),
                              static_cast<unsigned long long>(context->Rbx),
                              static_cast<unsigned long long>(context->Rcx),
                              static_cast<unsigned long long>(context->Rdx));
            WriteHandleFormat(report, "RSI=0x%llx RDI=0x%llx R8=0x%llx R9=0x%llx\r\n",
                              static_cast<unsigned long long>(context->Rsi),
                              static_cast<unsigned long long>(context->Rdi),
                              static_cast<unsigned long long>(context->R8),
                              static_cast<unsigned long long>(context->R9));
        }
#elif defined(_M_IX86) || defined(__i386__)
        if ( context )
        {
            WriteHandleFormat(report, "EIP=0x%lx ESP=0x%lx EBP=0x%lx\r\n",
                              static_cast<unsigned long>(context->Eip),
                              static_cast<unsigned long>(context->Esp),
                              static_cast<unsigned long>(context->Ebp));
            WriteHandleFormat(report, "EAX=0x%lx EBX=0x%lx ECX=0x%lx EDX=0x%lx\r\n",
                              static_cast<unsigned long>(context->Eax),
                              static_cast<unsigned long>(context->Ebx),
                              static_cast<unsigned long>(context->Ecx),
                              static_cast<unsigned long>(context->Edx));
        }
#endif

        WriteExceptionStackTrace(report, exceptionInfo ? exceptionInfo->ContextRecord : nullptr);
        WriteEmergencyBreadcrumbs(report);
        CloseHandle(report);
    }

    const std::string dumpPath = JoinPath(g_sessionDirPhysical, "CrashDump.dmp");
    HANDLE dump = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if ( dump != INVALID_HANDLE_VALUE )
    {
        MINIDUMP_EXCEPTION_INFORMATION exceptionData = {};
        exceptionData.ThreadId = GetCurrentThreadId();
        exceptionData.ExceptionPointers = exceptionInfo;
        exceptionData.ClientPointers = FALSE;

        const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs |
            MiniDumpWithHandleData |
            MiniDumpWithUnloadedModules |
            MiniDumpWithThreadInfo);

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump,
                          dumpType, exceptionInfo ? &exceptionData : nullptr,
                          nullptr, nullptr);
        CloseHandle(dump);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void Init(bool enabled, const std::string &buildTag)
{
    if ( !enabled || g_enabled.exchange(true) )
        return;

    g_buildTag = buildTag.empty() ? "unknown" : buildTag;
    g_sessionId = MakeTimestamp();
#ifdef _WIN32
    g_sessionId += "_" + std::to_string(static_cast<unsigned long>(GetCurrentProcessId()));
#else
    g_sessionId += "_" + std::to_string(static_cast<long>(getpid()));
#endif
    g_sessionDirLogical = "env/Diagnostics/" + g_sessionId;

    uaCreateDir("env/Diagnostics");
    if ( !uaCreateDir(g_sessionDirLogical) )
    {
        g_enabled.store(false);
        return;
    }

    g_sessionDirPhysical = uaDataFirstResolvedWritePath(g_sessionDirLogical);
    g_startMs = NowMs();
    g_lastHeartbeatMs.store(g_startMs);
    g_phase.store("Startup");

    WriteSessionInfo();

    g_previousTerminate = std::set_terminate(&TerminateHandler);
#ifdef _WIN32
    g_previousExceptionFilter = SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
#endif

    g_running.store(true);
    g_watchdogThread = std::thread(&WatchdogLoop);
    Breadcrumb("DIAG", "diagnostics enabled session=%s", g_sessionId.c_str());
}

void Shutdown()
{
    if ( !g_enabled.load() )
        return;

    g_watchdogArmed.store(false);
    g_running.store(false);
    if ( g_watchdogThread.joinable() )
        g_watchdogThread.join();

    Breadcrumb("DIAG", "diagnostics shutdown");
    WriteSnapshotFile("SessionEnd.txt", "shutdown", "normal shutdown");

#ifdef _WIN32
    SetUnhandledExceptionFilter(g_previousExceptionFilter);
#endif
    if ( g_previousTerminate )
        std::set_terminate(g_previousTerminate);

    g_phase.store("Disabled");
    g_enabled.store(false);
}

void DisarmWatchdog()
{
    if ( !g_enabled.load() )
        return;

    g_watchdogArmed.store(false);
    g_hangActive.store(false);
    Breadcrumb("WATCHDOG", "watchdog disarmed for shutdown");
}

bool Enabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

void SetPhase(const char *phase)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_phase.store(phase ? phase : "Unknown", std::memory_order_release);
}

void FrameBegin(uint32_t gameTime, int screenMode, int levelId)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_state.frameStarted.fetch_add(1);
    g_state.gameTime.store(gameTime);
    g_state.screenMode.store(screenMode);
    g_state.levelId.store(levelId);
    g_lastHeartbeatMs.store(NowMs(), std::memory_order_release);
    g_watchdogArmed.store(true, std::memory_order_release);
}

void FrameEnd(uint32_t gameTime, int screenMode, int levelId)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_state.frameCompleted.store(g_state.frameStarted.load());
    g_state.gameTime.store(gameTime);
    g_state.screenMode.store(screenMode);
    g_state.levelId.store(levelId);
    g_lastHeartbeatMs.store(NowMs(), std::memory_order_release);
}

void UpdateWorldState(int levelId,
                      int unitCount,
                      int userRoboGid,
                      int userUnitGid,
                      int viewerGid)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_state.levelId.store(levelId);
    g_state.unitCount.store(unitCount);
    g_state.userRoboGid.store(userRoboGid);
    g_state.userUnitGid.store(userUnitGid);
    g_state.viewerGid.store(viewerGid);
}

void SetActiveBact(const void *ptr,
                   int gid,
                   int type,
                   int owner,
                   int status,
                   int statusFlags,
                   int energy)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_state.activePtr.store(reinterpret_cast<uintptr_t>(ptr));
    g_state.activeGid.store(gid);
    g_state.activeType.store(type);
    g_state.activeOwner.store(owner);
    g_state.activeStatus.store(status);
    g_state.activeStatusFlags.store(statusFlags);
    g_state.activeEnergy.store(energy);
}

void ClearActiveBact()
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    g_state.activePtr.store(0);
    g_state.activeGid.store(0);
    g_state.activeType.store(0);
    g_state.activeOwner.store(0);
    g_state.activeStatus.store(0);
    g_state.activeStatusFlags.store(0);
    g_state.activeEnergy.store(0);
}

ScopedActiveBact::ScopedActiveBact(const void *ptr,
                                       int gid,
                                       int type,
                                       int owner,
                                       int status,
                                       int statusFlags,
                                       int energy)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    _armed = true;
    _previousPtr = g_state.activePtr.load(std::memory_order_relaxed);
    _previousGid = g_state.activeGid.load(std::memory_order_relaxed);
    _previousType = g_state.activeType.load(std::memory_order_relaxed);
    _previousOwner = g_state.activeOwner.load(std::memory_order_relaxed);
    _previousStatus = g_state.activeStatus.load(std::memory_order_relaxed);
    _previousStatusFlags = g_state.activeStatusFlags.load(std::memory_order_relaxed);
    _previousEnergy = g_state.activeEnergy.load(std::memory_order_relaxed);

    SetActiveBact(ptr, gid, type, owner, status, statusFlags, energy);
}

ScopedActiveBact::~ScopedActiveBact()
{
    if ( !_armed )
        return;

    g_state.activePtr.store(_previousPtr, std::memory_order_relaxed);
    g_state.activeGid.store(_previousGid, std::memory_order_relaxed);
    g_state.activeType.store(_previousType, std::memory_order_relaxed);
    g_state.activeOwner.store(_previousOwner, std::memory_order_relaxed);
    g_state.activeStatus.store(_previousStatus, std::memory_order_relaxed);
    g_state.activeStatusFlags.store(_previousStatusFlags, std::memory_order_relaxed);
    g_state.activeEnergy.store(_previousEnergy, std::memory_order_relaxed);
}

void Breadcrumb(const char *category, const char *format, ...)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    char detail[352];
    va_list args;
    va_start(args, format);
    std::vsnprintf(detail, sizeof(detail), format ? format : "", args);
    va_end(args);
    detail[sizeof(detail) - 1] = '\0';

    const uint64_t elapsed = NowMs() - g_startMs;

    std::lock_guard<std::mutex> lock(g_ringMutex);
    const uint64_t seq = g_sequence.fetch_add(1) + 1;

    char line[BREADCRUMB_TEXT_SIZE];
    std::snprintf(line, sizeof(line), "[%06llu][+%llums][T%lu][%s] %s",
                  static_cast<unsigned long long>(seq),
                  static_cast<unsigned long long>(elapsed),
                  CurrentThreadIdValue(),
                  category ? category : "GENERAL",
                  detail);
    line[sizeof(line) - 1] = '\0';

    BreadcrumbSlot &slot = g_breadcrumbs[static_cast<size_t>((seq - 1) % BREADCRUMB_COUNT)];
    slot.sequence = seq;
    std::strncpy(slot.text, line, sizeof(slot.text) - 1);
    slot.text[sizeof(slot.text) - 1] = '\0';
}

void RequestCheckpoint(const char *reason)
{
    if ( !g_enabled.load(std::memory_order_relaxed) )
        return;

    {
        std::lock_guard<std::mutex> lock(g_checkpointMutex);
        std::strncpy(g_checkpointReason,
                     reason ? reason : "requested",
                     sizeof(g_checkpointReason) - 1);
        g_checkpointReason[sizeof(g_checkpointReason) - 1] = '\0';
    }
    g_checkpointPending.store(true, std::memory_order_release);
}

} // namespace CrashDiag
