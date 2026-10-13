// Artillery shell markers on the map (add, remove, expire).
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include "nucleus/loaders.h"
#include "game/yparobo.h"
#include "gui/font.h"
#include "gui/uacommon.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "world/spin.h"
#include "common/crashdiag.h"
#include "game/yw_game_impl.h"

// OpenNeoUA custom: register/refresh an artillery shell map zone. A firing cycle keeps
// one immutable active target centre per platform. Pending follow-up orders are also unique
// per platform: retargeting moves that one disabled-grey pending marker instead of creating
// another active bombardment zone.
void NC_STACK_ypaworld::AddArtilleryShellMarker(const vec3d &pos, float radius, int owner, uint32_t sourceGid, const std::string &markerPath, bool pending, int lingerMs)
{
    if ( radius < 0.01f )
        return;

    ExpireArtilleryShellMarkers();

    if ( lingerMs < 0 )
        lingerMs = 0;

    int32_t expire = _timeStamp + (int32_t)((int64_t)lingerMs * 1024 / 1000); // 1024 ticks = 1s

    for (ArtilleryShellMarker &m : _artilleryShellMarkers)
    {
        if ( m.sourceGid != sourceGid || m.pending != pending )
            continue;

        // A pending order has exactly one authoritative target per artillery platform.
        // Updating it must replace the old drawing immediately. Active markers instead
        // only merge shells belonging to the same target centre, allowing a previous
        // zone to remain visible while its already-fired shells are still in flight.
        const float sameZoneTolerance = std::max(1.0f, radius * 0.01f);
        if ( pending || (m.pos.XZ() - pos.XZ()).length() <= sameZoneTolerance )
        {
            m.pos = pos;
            m.radius = radius;
            m.owner = (uint8_t)owner;
            m.markerPath = markerPath;
            if ( pending || expire > m.expireStamp )
                m.expireStamp = expire;
            return;
        }
    }

    ArtilleryShellMarker marker;
    marker.pos = pos;
    marker.radius = radius;
    marker.expireStamp = expire;
    marker.sourceGid = sourceGid;
    marker.owner = (uint8_t)owner;
    marker.markerPath = markerPath;
    marker.pending = pending;
    _artilleryShellMarkers.push_back(marker);

    if ( _artilleryShellMarkers.size() > 64 )
        _artilleryShellMarkers.erase(_artilleryShellMarkers.begin());
}

void NC_STACK_ypaworld::RemovePendingArtilleryShellMarker(uint32_t sourceGid)
{
    for (size_t i = 0; i < _artilleryShellMarkers.size(); )
    {
        const ArtilleryShellMarker &marker = _artilleryShellMarkers[i];
        if ( marker.pending && marker.sourceGid == sourceGid )
        {
            _artilleryShellMarkers[i] = _artilleryShellMarkers.back();
            _artilleryShellMarkers.pop_back();
        }
        else
            i++;
    }
}

// OpenNeoUA custom: expire old bombardment markers without drawing them in the 3D world.
void NC_STACK_ypaworld::ExpireArtilleryShellMarkers()
{
    for (size_t i = 0; i < _artilleryShellMarkers.size(); )
    {
        if ( _timeStamp >= _artilleryShellMarkers[i].expireStamp )
        {
            _artilleryShellMarkers[i] = _artilleryShellMarkers.back();
            _artilleryShellMarkers.pop_back();
        }
        else
            i++;
    }
}

void NC_STACK_ypaworld::ClearArtilleryShellMarkers()
{
    _artilleryShellMarkers.clear();
}
