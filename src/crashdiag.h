#ifndef OPENUA_CRASHDIAG_H_INCLUDED
#define OPENUA_CRASHDIAG_H_INCLUDED

#include <cstdint>
#include <string>

namespace CrashDiag
{

// Permanent OpenUA diagnostics. The service is inert until
// game.crash_diagnostics is enabled in Nucleus.ini.
void Init(bool enabled, const std::string &buildTag);
void Shutdown();
void DisarmWatchdog();

bool Enabled();

// Phase names must be static string literals. They are read by the watchdog
// and the unhandled-exception handler without allocating memory.
void SetPhase(const char *phase);

void FrameBegin(uint32_t gameTime, int screenMode, int levelId);
void FrameEnd(uint32_t gameTime, int screenMode, int levelId);

void UpdateWorldState(int levelId,
                      int unitCount,
                      int userRoboGid,
                      int userUnitGid,
                      int viewerGid);

void SetActiveBact(const void *ptr,
                   int gid,
                   int type,
                   int owner,
                   int status,
                   int statusFlags,
                   int energy);
void ClearActiveBact();

// Tracks the deepest BACT currently executing while preserving any parent
// scope. Disabled diagnostics reduce this to a cheap no-op.
class ScopedActiveBact
{
public:
    ScopedActiveBact(const void *ptr,
                     int gid,
                     int type,
                     int owner,
                     int status,
                     int statusFlags,
                     int energy);
    ~ScopedActiveBact();

    ScopedActiveBact(const ScopedActiveBact &) = delete;
    ScopedActiveBact &operator=(const ScopedActiveBact &) = delete;

private:
    bool _armed = false;
    uintptr_t _previousPtr = 0;
    int _previousGid = 0;
    int _previousType = 0;
    int _previousOwner = 0;
    int _previousStatus = 0;
    int _previousStatusFlags = 0;
    int _previousEnergy = 0;
};

void Breadcrumb(const char *category, const char *format, ...);

// Requests a snapshot from the background diagnostic worker. This avoids
// synchronous disk I/O in sensitive gameplay paths such as Host Station death.
void RequestCheckpoint(const char *reason);

}

#endif // OPENUA_CRASHDIAG_H_INCLUDED
