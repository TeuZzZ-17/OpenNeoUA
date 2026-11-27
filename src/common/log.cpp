// Logging: log files, debug output and formatted log helpers.
#include <stdio.h>
#include <stdarg.h>
#include "common/log.h"
#include "system/fsmgr.h"
#include "common/utils.h"
#include "common/crashdiag.h"

// Every log line goes to the crash-diagnostics buffer (category) and is appended
// to its text file under Data/env. A va_list is consumed once, hence the copy.
static void AppendLog(const char *category, const char *file, const char *prefix,
                      const char *format, va_list args)
{
    va_list fileArgs;
    va_copy(fileArgs, args);
    CrashDiag::LogMessage(category, format, args);
    FSMgr::FileHandle *f = FSMgr::iDir::openFileAlloc(uaDataFirstResolvedWritePath(file), "a");
    if ( f )
    {
        if ( prefix )
            f->printf("%s", prefix);
        f->vprintf(format, fileArgs);
        delete f;
    }
    va_end(fileArgs);
}

// Truncates a log file and writes its header (called once at startup).
static void ResetLog(const char *file, const char *header)
{
    FSMgr::FileHandle *f = FSMgr::iDir::openFileAlloc(uaDataFirstResolvedWritePath(file), "w");
    if ( f )
    {
        f->printf("%s", header);
        delete f;
    }
}

void ypa_log__ypa_general_log()
{
    ResetLog("env/ypa_log.txt", "YPA General Log\n---------------\n");
}

void ypa_log_out(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    AppendLog("GAME_LOG", "env/ypa_log.txt", nullptr, format, args);
    va_end(args);
}

void init_d3dlog()
{
    ResetLog("env/d3dlog.txt", "YPA DD/D3D driver log\n---------------------\n");
}

void log_d3dlog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    AppendLog("GRAPHICS_LOG", "env/d3dlog.txt", nullptr, format, args);
    va_end(args);
}

void log_netlog(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    AppendLog("NETWORK_LOG", "env/ypa_log.txt", "netlog: ", format, args);
    va_end(args);
}
