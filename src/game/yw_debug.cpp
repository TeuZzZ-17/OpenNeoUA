// Debug overlays: unit counts, DPS meter, blast rings, collision spheres and info drawing.
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

void NC_STACK_ypaworld::debug_count_units()
{
    for (int i = 0; i < 8; i++)
    {
        _dbgSquadCounter[i] = 0;
        _dbgVehicleCounter[i] = 0;
        _dbgFlakCounter[i] = 0;
        _dbgRoboCounter[i] = 0;
        _dbgWeaponCounter[i] = 0;
    }

    _dbgTotalSquadCount = 0;
    _dbgTotalVehicleCount = 0;
    _dbgTotalFlakCount = 0;
    _dbgTotalWeaponCount = 0;
    _dbgTotalRoboCount = 0;

    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        _dbgRoboCounter[ robo->_owner ]++;

        if ( robo->_owner )
        {
            for ( NC_STACK_ypabact * &commander : robo->_kidList )
            {
                bool v5 = false;

                if ( commander->_bact_type == BACT_TYPES_GUN )
                {
                    v5 = true;
                    _dbgFlakCounter[ commander->_owner ]++;
                }
                else
                {
                    _dbgSquadCounter[ commander->_owner ]++;
                    _dbgVehicleCounter[ commander->_owner ]++;
                }

                _dbgWeaponCounter[ commander->_owner ] += commander->_missiles_list.size();

                for ( NC_STACK_ypabact * &unit : commander->_kidList )
                {
                    if ( v5 )
                        _dbgFlakCounter[ unit->_owner ]++;
                    else
                        _dbgVehicleCounter[ commander->_owner ]++;


                    _dbgWeaponCounter[ commander->_owner ] += unit->_missiles_list.size();
                }
            }
        }
    }

    for (int i = 0; i < 8; i++)
    {
        _dbgTotalSquadCount  += _dbgSquadCounter[i];
        _dbgTotalVehicleCount += _dbgVehicleCounter[i];
        _dbgTotalFlakCount  += _dbgFlakCounter[i];
        _dbgTotalWeaponCount  += _dbgWeaponCounter[i];
        _dbgTotalRoboCount += _dbgRoboCounter[i];
    }

    if ( _dbgTotalSquadCount > _dbgTotalSquadCountMax )
        _dbgTotalSquadCountMax = _dbgTotalSquadCount;

    if ( _dbgTotalVehicleCount > _dbgTotalVehicleCountMax )
        _dbgTotalVehicleCountMax = _dbgTotalVehicleCount;

    if ( _dbgTotalFlakCount > _dbgTotalFlakCountMax )
        _dbgTotalFlakCountMax = _dbgTotalFlakCount;

    if ( _dbgTotalWeaponCount > _dbgTotalWeaponCountMax )
        _dbgTotalWeaponCountMax = _dbgTotalWeaponCount;

    if ( _dbgTotalRoboCount > _dbgTotalRoboCountMax )
        _dbgTotalRoboCountMax = _dbgTotalRoboCount;
}

void NC_STACK_ypaworld::DebugResetDpsMeter()
{
    _debugDpsSamples.clear();
    _debugDpsSourceGid = 0;
    _debugDpsPeakRaw = 0;
    _debugDpsSessionDamageRaw = 0;
    _debugDpsSessionStartStamp = 0;
    _debugDpsLastDamageStamp = 0;
}

void NC_STACK_ypaworld::DebugUpdateDpsSource()
{
    if ( !_debugDpsEnabled || !_userUnit || IsSpectatorControlled() )
        return;

    // Weapon Cam can temporarily make a projectile the user unit. Keep the
    // last real controlled vehicle as the DPS source until control returns.
    if ( _userUnit->_bact_type == BACT_TYPES_MISSLE )
        return;

    // A level/replay transition can move the gameplay clock backwards. Never
    // carry a previous level's damage samples into the new one.
    if ( !_debugDpsSamples.empty() && _timeStamp < _debugDpsSamples.back().stamp )
    {
        _debugDpsSamples.clear();
        _debugDpsPeakRaw = 0;
        _debugDpsSessionDamageRaw = 0;
        _debugDpsSessionStartStamp = 0;
        _debugDpsLastDamageStamp = 0;
    }

    const uint32_t gid = _userUnit->_gid;
    if ( gid != _debugDpsSourceGid )
    {
        _debugDpsSourceGid = gid;
        _debugDpsSamples.clear();
        _debugDpsPeakRaw = 0;
        _debugDpsSessionDamageRaw = 0;
        _debugDpsSessionStartStamp = 0;
        _debugDpsLastDamageStamp = 0;
    }
}

void NC_STACK_ypaworld::DebugRecordDpsDamage(NC_STACK_ypabact *attacker,
                                              NC_STACK_ypabact *target,
                                              int rawDamage)
{
    if ( !_debugDpsEnabled || _isNetGame || !attacker || !target || rawDamage <= 0 )
        return;

    if ( !_debugDpsSourceGid || attacker->_gid != _debugDpsSourceGid )
        return;

    // Friendly units are valid debug dummies: F6/F8 can keep them alive while
    // the meter measures the damage this controlled unit would deal. Ignore only
    // true self-damage so suicide/kill-after-shot mechanics do not pollute DPS.
    if ( attacker == target )
        return;

    const int32_t now = _timeStamp;

    // Start a new AVG/PEAK firing session after three seconds without damage.
    // The rolling 1-second samples remain independent from this longer session.
    if ( _debugDpsLastDamageStamp <= 0 ||
         now < _debugDpsLastDamageStamp ||
         now - _debugDpsLastDamageStamp >= 3000 )
    {
        _debugDpsPeakRaw = 0;
        _debugDpsSessionDamageRaw = 0;
        _debugDpsSessionStartStamp = now;
    }

    if ( _debugDpsSessionStartStamp <= 0 )
        _debugDpsSessionStartStamp = now;

    _debugDpsSessionDamageRaw =
        std::min<int64_t>(_debugDpsSessionDamageRaw + (int64_t)rawDamage,
                          std::numeric_limits<int64_t>::max());
    _debugDpsLastDamageStamp = now;

    const int32_t cutoff = now - 1000;
    _debugDpsSamples.erase(
        std::remove_if(_debugDpsSamples.begin(), _debugDpsSamples.end(),
                       [cutoff](const DebugDpsSample &sample)
                       {
                           return sample.stamp < cutoff;
                       }),
        _debugDpsSamples.end());

    // Aggregate hits generated in the same frame. This keeps MGUN and shotgun
    // weapons cheap to profile even when they create many damage events.
    if ( !_debugDpsSamples.empty() && _debugDpsSamples.back().stamp == now )
    {
        const int64_t sum = (int64_t)_debugDpsSamples.back().rawDamage + rawDamage;
        _debugDpsSamples.back().rawDamage =
            (int32_t)std::min<int64_t>(sum, std::numeric_limits<int32_t>::max());
    }
    else
    {
        DebugDpsSample sample;
        sample.stamp = now;
        sample.rawDamage = rawDamage;
        _debugDpsSamples.push_back(sample);
    }
}

void NC_STACK_ypaworld::debug_info_draw(TInputState *inpt)
{
    if ( _showDebugMode != 0 )
    {
        CmdStream dbg_txt;
        dbg_txt.reserve(4096);
        char buf_sprintf[2048];

        FontUA::select_tileset(&dbg_txt, 15);
        FontUA::set_xpos(&dbg_txt, 8);
        FontUA::set_ypos(&dbg_txt, 16);

        int v104 = 0;

        if ( _showDebugMode == 1 )
        {
            debug_count_units();

            if ( !_buildDate.empty() )
            {
                sub_445654(this, &dbg_txt, buf_sprintf, "build id: %s", _buildDate.c_str());

                FontUA::next_line(&dbg_txt);
            }

            int this_time = _timeStamp / 1024;
            int all_time;

            if ( _isNetGame )
                all_time = 0;
            else
                all_time = (_timeStamp + _playersStats[1].ElapsedTime) / 1024;

            sub_445654(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "time: (this: %02d:%02d:%02d) (all: %02d:%02d:%02d)",
                      this_time / 60 / 60,
                      this_time / 60 % 60,
                      this_time % 60,
                      all_time / 60 / 60,
                      all_time / 60 % 60,
                      all_time % 60 );

            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof all: %d", _profileVals[PFID_FRAMETIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof fprint: %d", _profileVals[PFID_MARKTIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof gui: %d", _profileVals[PFID_GUITIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof ai: %d", _profileVals[PFID_UPDATETIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof rend: %d", _profileVals[PFID_RENDERTIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof 2d rend: %d", _profileVals[PFID_NEWGUITIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "prof net: %d", _profileVals[PFID_NETTIME]);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "sec type/wtype: %d/%d", _userUnit->_pSector->type_id, _userUnit->_pSector->PurposeType);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "beam energy: %d", _beamEnergyCapacity);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "num sqd: %d,%d", _dbgTotalSquadCount, _dbgTotalSquadCountMax);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "num vhcl: %d,%d", _dbgTotalVehicleCount, _dbgTotalVehicleCountMax);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "num flk: %d,%d", _dbgTotalFlakCount, _dbgTotalFlakCountMax);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "num robo: %d,%d", _dbgTotalRoboCount, _dbgTotalRoboCountMax);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "num wpn: %d,%d", _dbgTotalWeaponCount, _dbgTotalWeaponCountMax);
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "reload const: %d", _userRobo->_reload_const);
            FontUA::next_line(&dbg_txt);

            sub_445654(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "num all vhcl: %d,%d,%d,%d,%d,%d,%d,%d",
                      _countUnitsPerOwner[0],
                      _countUnitsPerOwner[1],
                      _countUnitsPerOwner[2],
                      _countUnitsPerOwner[3],
                      _countUnitsPerOwner[4],
                      _countUnitsPerOwner[5],
                      _countUnitsPerOwner[6],
                      _countUnitsPerOwner[7]);
            FontUA::next_line(&dbg_txt);

            sub_445654(
                      this,
                      &dbg_txt,
                      buf_sprintf,
                      "rld ratio: %8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f,%8.2f",
                      _reloadRatioPositive[0],
                      _reloadRatioPositive[1],
                      _reloadRatioPositive[2],
                      _reloadRatioPositive[3],
                      _reloadRatioPositive[4],
                      _reloadRatioPositive[5],
                      _reloadRatioPositive[6],
                      _reloadRatioPositive[7]);
            FontUA::next_line(&dbg_txt);

            if ( _invulnerable )
                sub_445654(this, &dbg_txt, buf_sprintf, "invulnerable: %s", "YES");
            else
                sub_445654(this, &dbg_txt, buf_sprintf, "invulnerable: %s", "NO");
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "global invulnerability: %s",
                       _debugGlobalInvulnerability ? "YES" : "NO");
            FontUA::next_line(&dbg_txt);
        }
        else if ( _showDebugMode == 2 )
        {
            if ( _GameShell && _isNetGame )
            {
                FontUA::next_line(&dbg_txt);
                FontUA::next_line(&dbg_txt);

                for (UserData::TNetPlayerData &pl : _GameShell->netPlayers)
                {
                    if ( pl.Status )
                    {
                        const char *v35;
                        const char *v36;

                        switch ( pl.Owner )
                        {
                        case 1:
                            v35 = "Resistance";
                            break;

                        case 3:
                            v35 = "Mykonier  ";
                            break;

                        case 4:
                            v35 = "Taerkasten";
                            break;

                        case 6:
                            v35 = "Ghorkov   ";
                            break;

                        default:
                            v35 = "Hae?!     ";
                            break;
                        }

                        switch ( pl.Status )
                        {
                        case 1:
                            v36 = "OK";
                            break;

                        case 2:
                            v36 = "makes trouble";
                            break;

                        case 3:
                            v36 = "left the game";
                            break;

                        case 4:
                            v36 = "Removed";
                            break;

                        default:
                            v36 = "Hae?!     ";
                            break;
                        }

                        sub_445654(this, &dbg_txt, buf_sprintf, "%s status: %s latency: %d", v35, v36, pl.Latency);

                        FontUA::next_line(&dbg_txt);
                    }
                }

                FontUA::next_line(&dbg_txt);

                sub_445654(this, &dbg_txt, buf_sprintf, "net send: %d bytes/sec", _GameShell->netsend_speed);
                FontUA::next_line(&dbg_txt);

                sub_445654(this, &dbg_txt, buf_sprintf, "net rcv: %d bytes/sec", _GameShell->netrecv_speed);
                FontUA::next_line(&dbg_txt);

                sub_445654(this, &dbg_txt, buf_sprintf, "packet: %d bytes", _GameShell->net_packet_size);
                FontUA::next_line(&dbg_txt);

                if ( _netInfoOverkill )
                    sub_445654(this, &dbg_txt, buf_sprintf, "WARNING: INFO OVERKILL");

                FontUA::next_line(&dbg_txt);

                if ( _netDriver )
                {
                    int v100[7];
                    _netDriver->GetStats(v100);

                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "thread send list now: %d", v100[0]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "thread recv list now: %d", v100[1]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "thread send list max: %d", v100[3]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "thread recv list max: %d", v100[2]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "send call now: %d", v100[4]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "send call max: %d", v100[5]);
                    FontUA::next_line(&dbg_txt);

                    sub_445654(this, &dbg_txt, buf_sprintf, "send bugs: %d", v100[6]);
                    FontUA::next_line(&dbg_txt);
                }
            }
            else
            {
                sub_445654(this, &dbg_txt, buf_sprintf, "not a network game");
                FontUA::next_line(&dbg_txt);
            }
        }
        else if ( _showDebugMode == 3 )
        {
            for (int i = 0; i < 17; i++)
            {
                sub_445654(this, &dbg_txt, buf_sprintf, "slider[%d] = %f", i, inpt->Sliders[i]);
                FontUA::next_line(&dbg_txt);
            }

            std::string buf;

            for (int i = 0; i < 32; i++)
            {
                if ( inpt->Buttons.Is(i) )
                    buf += 'O';
                else
                    buf += '_';
            }

            sub_445654(this, &dbg_txt, buf_sprintf, buf.c_str());
            FontUA::next_line(&dbg_txt);

            sub_445654(this, &dbg_txt, buf_sprintf, "keycode = %d", inpt->KbdLastDown);
            FontUA::next_line(&dbg_txt);
        }
        else
        {
            int v109 = 0;
            int v110 = 0;

            for ( NC_STACK_ypabact * &bact : _unitsList )
            {
                if (v109)
                    break;

                if ( bact->_bact_type == BACT_TYPES_ROBO && bact->_owner != 1 ) // FIXME owner
                {
                    v110++;

                    if ( _showDebugMode - 3 <= v110 )
                    {
                        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bact);

                        v109 = 1;

                        sub_445654(this, &dbg_txt, buf_sprintf, "robo owner %d with energy %d / %d / %d / %d", robo->_owner, robo->_energy, robo->_roboBuildSpare, robo->_roboVehicleSpare, robo->_energy_max);
                        FontUA::next_line(&dbg_txt);

                        const char *v71;
                        const char *v73;

                        switch (robo->_roboBuildingDuty)
                        {
                        case 0x10:
                            v71 = "radar";
                            break;

                        case 0x20:
                            v71 = "powerstation";
                            break;

                        case 0x100:
                            v71 = "flak";
                            break;

                        case 0x20000:
                            v71 = "location";
                            break;

                        default:
                            v71 = "nothing";
                            break;
                        }

                        switch (robo->_roboVehicleDuty)
                        {
                        case 0:
                            v73 = "nothing";
                            break;

                        case 0x40:
                            v73 = "conquer";
                            break;

                        case 0x80:
                            v73 = "defense";
                            break;

                        case 0x80000:
                            v73 = "recon";
                            break;

                        case 0x200000:
                            v73 = "robo";
                            break;

                        default:
                            v73 = "powerstation";
                            break;
                        }

                        sub_445654(this, &dbg_txt, buf_sprintf, "    do build job   >%s<   and vhcl job   >%s<", v71, v73);
                        FontUA::next_line(&dbg_txt);

                        sub_445654(this, &dbg_txt, buf_sprintf, "    wait power %d, radar %d, flak %d, location %d",
                                         robo->_roboPowerDelay / 1000,
                                         robo->_roboRadarDelay / 1000,
                                         robo->_roboSafetyDelay / 1000,
                                         robo->_roboPositionDelay / 1000);

                        FontUA::next_line(&dbg_txt);

                        sub_445654(this, &dbg_txt, buf_sprintf, "    wait conquer %d, defense %d, recon %d, robo %d",
                                         robo->_roboConqDelay / 1000,
                                         robo->_roboEnemyDelay / 1000,
                                         robo->_roboExploreDelay / 1000,
                                         robo->_roboDangerDelay / 1000);
                        FontUA::next_line(&dbg_txt);

                        sub_445654(this, &dbg_txt, buf_sprintf, "    values  ");
                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboPowerDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "power -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "power %d, ", robo->_roboPowerValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboRadarDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "radar -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "radar %d, ", robo->_roboRadarValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboSafetyDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "flak -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "flak %d, ", robo->_roboSafetyValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboPositionDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "power -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "power %d, ", robo->_roboPositionValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboEnemyDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "defense -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "defense %d, ", robo->_roboEnemyValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboConqDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "conquer -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "conquer %d, ", robo->_roboConqValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboExploreDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "recon -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "recon %d, ", robo->_roboExploreValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboDangerDelay > 0 )
                            sub_445654(this, &dbg_txt, buf_sprintf, "robo -1, ");
                        else
                            sub_445654(this, &dbg_txt, buf_sprintf, "robo %d, ", robo->_roboDangerValue);

                        FontUA::next_line(&dbg_txt);

                        if ( robo->_roboState & NC_STACK_yparobo::ROBOSTATE_DOCKINUSE )
                            sub_445654(this, &dbg_txt, buf_sprintf, "dock energy %d time %d", robo->_roboDockEnerg, robo->_roboDockTime);
                    }
                }
            }

            if ( !v109 )
                v104 = 1;
        }


        FontUA::next_line(&dbg_txt);

        sub_445654(this, &dbg_txt, buf_sprintf, "fps: %d", _FPS);
        FontUA::next_line(&dbg_txt);

        sub_445654(this, &dbg_txt, buf_sprintf, "polys: %d,%d", _polysCount, _polysDraw);
        FontUA::next_line(&dbg_txt);

        FontUA::set_end(&dbg_txt);

        GFX::Engine.ProcessDrawSeq(dbg_txt);


        if ( v104 )
            _showDebugMode = 0;
    }

    if ( _debugDpsEnabled )
    {
        const int32_t cutoff = _timeStamp - 1000;
        _debugDpsSamples.erase(
            std::remove_if(_debugDpsSamples.begin(), _debugDpsSamples.end(),
                           [cutoff](const DebugDpsSample &sample)
                           {
                               return sample.stamp < cutoff;
                           }),
            _debugDpsSamples.end());

        int64_t rawDamage = 0;
        for ( const DebugDpsSample &sample : _debugDpsSamples )
            rawDamage += sample.rawDamage;

        // DPS PEAK is the best rolling 1-second value reached during the current
        // firing burst. DPS AVG covers the whole burst from its first recorded hit.
        // Three seconds without damage starts a new burst for both values.
        if ( _debugDpsLastDamageStamp > 0 &&
             _timeStamp >= _debugDpsLastDamageStamp &&
             _timeStamp - _debugDpsLastDamageStamp >= 3000 )
        {
            _debugDpsPeakRaw = 0;
            _debugDpsSessionDamageRaw = 0;
            _debugDpsSessionStartStamp = 0;
            _debugDpsLastDamageStamp = 0;
        }
        else if ( rawDamage > _debugDpsPeakRaw )
        {
            _debugDpsPeakRaw = rawDamage;
        }

        // Player-facing combat values use the same /100 scale as the DMG row.
        const double dps = (double)rawDamage / 100.0;
        const double peakDps = (double)_debugDpsPeakRaw / 100.0;

        double avgDps = 0.0;
        if ( _debugDpsSessionStartStamp > 0 &&
             _timeStamp >= _debugDpsSessionStartStamp &&
             _debugDpsSessionDamageRaw > 0 )
        {
            // Clamp the first second to one full second so the first hit does not
            // create an artificial huge AVG value from a near-zero denominator.
            const int32_t elapsedMs =
                std::max<int32_t>(1000, _timeStamp - _debugDpsSessionStartStamp);
            avgDps = ((double)_debugDpsSessionDamageRaw / 100.0) *
                     (1000.0 / (double)elapsedMs);
        }

        const std::string dpsText = fmt::sprintf("DPS: %.1f", dps);
        const std::string avgText = fmt::sprintf("DPS AVG: %.1f", avgDps);
        const std::string peakText = fmt::sprintf("DPS PEAK: %.1f", peakDps);

        CmdStream dpsDraw;
        dpsDraw.reserve(256);
        FontUA::select_tileset(&dpsDraw, 15);

        // Keep the diagnostics in the upper-centre area, below the top HUD bars.
        // Use a screen-relative anchor so the placement remains consistent across resolutions.
        const int32_t dpsBlockCenterY = std::max<int32_t>(3 * _fontH, (_screenSize.y * 17) / 100);
        const int16_t dpsBlockCenterOffset =
            (int16_t)(dpsBlockCenterY - (_screenSize.y / 2));

        FontUA::set_txtColor(&dpsDraw, 90, 255, 170);
        FontUA::set_xpos(&dpsDraw, 0);
        FontUA::set_center_ypos(&dpsDraw, dpsBlockCenterOffset - _fontH);
        if ( _guiTiles[15] )
            FontUA::FormateCenteredSkipableItem(_guiTiles[15], &dpsDraw, dpsText, _screenSize.x);
        else
            FontUA::add_txt(&dpsDraw, _screenSize.x, 1, dpsText);

        FontUA::set_txtColor(&dpsDraw, 110, 210, 255);
        FontUA::set_xpos(&dpsDraw, 0);
        FontUA::set_center_ypos(&dpsDraw, dpsBlockCenterOffset);
        if ( _guiTiles[15] )
            FontUA::FormateCenteredSkipableItem(_guiTiles[15], &dpsDraw, avgText, _screenSize.x);
        else
            FontUA::add_txt(&dpsDraw, _screenSize.x, 1, avgText);

        FontUA::set_txtColor(&dpsDraw, 255, 80, 80);
        FontUA::set_xpos(&dpsDraw, 0);
        FontUA::set_center_ypos(&dpsDraw, dpsBlockCenterOffset + _fontH);
        if ( _guiTiles[15] )
            FontUA::FormateCenteredSkipableItem(_guiTiles[15], &dpsDraw, peakText, _screenSize.x);
        else
            FontUA::add_txt(&dpsDraw, _screenSize.x, 1, peakText);

        FontUA::set_end(&dpsDraw);
        GFX::Engine.ProcessDrawSeq(dpsDraw);
    }

    bool openUADebug = System::IniConf::IsGameNewDebugEnabled();
    if ( !openUADebug )
    {
        _showCollDebug = false;
        _hideHudForScreenshots = false;
        _debugGameplaySlowMotion = false;
        _debugGameplayFastMotion = false;
        _debugGameplayFrozen = false;
        _debugHostStationCheat = false;
        _debugGlobalInvulnerability = false;
        _debugDpsEnabled = false;
        DebugResetDpsMeter();
    }
    else
    {
        // F9 collision debug overlay: direct key check, no RMB/easy-cheat helper.
        if ( inpt && inpt->KbdLastHit == Input::KC_F9 )
        {
            inpt->HotKeyID = -1;
            _showCollDebug = !_showCollDebug;

            yw_arg159 infoMsg;
            infoMsg.txt = _showCollDebug ?
                          "Collision Debug ON" :
                          "Collision Debug OFF";
            infoMsg.unit = nullptr;
            infoMsg.Priority = 100;
            infoMsg.MsgID = 0;
            ypaworld_func159(&infoMsg);
        }

        // F10 screenshot mode: hide the gameplay HUD without enabling debug overlays.
        if ( inpt && inpt->KbdLastHit == Input::KC_F10 )
        {
            inpt->HotKeyID = -1;
            _hideHudForScreenshots = !_hideHudForScreenshots;

            yw_arg159 infoMsg;
            infoMsg.txt = _hideHudForScreenshots ?
                          "HUD Hidden" :
                          "HUD Visible";
            infoMsg.unit = nullptr;
            infoMsg.Priority = 100;
            infoMsg.MsgID = 0;
            ypaworld_func159(&infoMsg);
        }
    }

    ExpireDebugBlastRings();

    if ( _showCollDebug )
        debug_draw_coll_spheres();

    // OpenNeoUA custom: keep strategic-map artillery shell markers from lingering after expiry.
    // Manual artillery shell control is handled entirely by 2D-map clicks (no key trigger);
    // see NC_STACK_ypaworld::HandleArtilleryShellMapClick().
    ExpireArtilleryShellMarkers();
}

bool yw_DebugIsLiveBact(NC_STACK_ypabact *unit)
{
    return unit &&
           !unit->_kidRef.IsListType(World::BLIST_CACHE) &&
           unit->_status != BACT_STATUS_NOPE &&
           unit->_status != BACT_STATUS_DEAD &&
           unit->_status != BACT_STATUS_CREATE &&
           unit->_status != BACT_STATUS_BEAM &&
           !(unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2));
}

bool yw_DebugIsCurrentControlledBact(NC_STACK_ypaworld *world, NC_STACK_ypabact *unit)
{
    return world &&
           unit &&
           (unit == world->_userUnit ||
            (unit == world->_viewerBact && unit->getBACT_inputting()));
}

void NC_STACK_ypaworld::debug_draw_coll_spheres()
{
    SDL_Surface *scr = GFX::Engine.Screen();
    int screenW = _screenSize.x;
    int screenH = _screenSize.y;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if (!view)
        return;

    const mat4x4f &Proj = GFX::Engine.GetProjectionMatrix();
    float nearZ = GFX::Engine.GetProjectionNear();

    auto project = [&](const vec3d &worldPos, int &sx, int &sy) -> bool {
        vec3d cam = view->CalcSclRot.Transform(worldPos - view->CalcPos);
        if (cam.z < nearZ)
            return false;
        vec3d p = Proj.Transform(cam);
        float w = Proj.CalcW(cam);
        if (w <= 0.001f)
            return false;
        sx = (int)((p.x / w * 0.5f + 0.5f) * screenW);
        sy = (int)((1.0f - (p.y / w * 0.5f + 0.5f)) * screenH);
        return sx >= -64 && sx <= screenW + 64 && sy >= -64 && sy <= screenH + 64;
    };

    const int SEGS = 12;
    auto drawRing = [&](const vec3d &center, float radius, int axis, uint8_t r, uint8_t g, uint8_t b) {
        if (radius < 0.01f)
            return;

        int px0 = 0, py0 = 0;
        bool hasPrev = false;
        for (int i = 0; i <= SEGS; i++)
        {
            float a = 2.0f * M_PI * i / SEGS;
            float ca = cosf(a) * radius;
            float sa = sinf(a) * radius;
            vec3d p;
            if (axis == 0)
                p = center + vec3d(ca, sa, 0.0);
            else if (axis == 1)
                p = center + vec3d(ca, 0.0, sa);
            else
                p = center + vec3d(0.0, ca, sa);

            int sx = 0, sy = 0;
            if (project(p, sx, sy))
            {
                if (hasPrev)
                    GFX::GFXEngine::DrawLine(scr, Common::Line(px0, py0, sx, sy), r, g, b);
                px0 = sx;
                py0 = sy;
                hasPrev = true;
            }
            else
            {
                hasPrev = false;
            }
        }
    };

    // Single horizontal (XZ) ground ring. Used for large gameplay range radii so the
    // screen does not get cluttered with full wire spheres for every effect.
    auto drawFlatRing = [&](const vec3d &center, float radius, uint8_t r, uint8_t g, uint8_t b) {
        if (radius < 0.01f)
            return;

        int px0 = 0, py0 = 0;
        bool hasPrev = false;
        for (int i = 0; i <= SEGS; i++)
        {
            float a = 2.0f * M_PI * i / SEGS;
            vec3d p = center + vec3d(cosf(a) * radius, 0.0, sinf(a) * radius);
            int sx = 0, sy = 0;
            if (project(p, sx, sy))
            {
                if (hasPrev)
                    GFX::GFXEngine::DrawLine(scr, Common::Line(px0, py0, sx, sy), r, g, b);
                px0 = sx;
                py0 = sy;
                hasPrev = true;
            }
            else
            {
                hasPrev = false;
            }
        }
    };

    auto drawBeamTube = [&](const vec3d &start, const vec3d &end, float radius, uint8_t r, uint8_t g, uint8_t b) {
        if (radius < 0.01f)
            return;

        vec3d axis = end - start;
        float len = axis.length();
        if (len < 1.0f)
            return;
        axis /= len;

        vec3d ref = fabs(axis.y) < 0.9f ? vec3d(0.0f, 1.0f, 0.0f) : vec3d(1.0f, 0.0f, 0.0f);
        vec3d side = ref * axis;
        if (side.normalise() < 0.001f)
            return;

        vec3d up = axis * side;
        if (up.normalise() < 0.001f)
            return;

        auto drawOrientedRing = [&](const vec3d &center) {
            int px0 = 0, py0 = 0;
            bool hasPrev = false;
            for (int i = 0; i <= SEGS; i++)
            {
                float a = 2.0f * M_PI * i / SEGS;
                vec3d p = center + side * (cosf(a) * radius) + up * (sinf(a) * radius);
                int sx = 0, sy = 0;
                if (project(p, sx, sy))
                {
                    if (hasPrev)
                        GFX::GFXEngine::DrawLine(scr, Common::Line(px0, py0, sx, sy), r, g, b);
                    px0 = sx;
                    py0 = sy;
                    hasPrev = true;
                }
                else
                {
                    hasPrev = false;
                }
            }
        };

        int sections = (int)(len / 600.0f) + 2;
        if (sections < 2) sections = 2;
        if (sections > 12) sections = 12;

        for (int i = 0; i <= sections; i++)
        {
            float t = (float)i / (float)sections;
            drawOrientedRing(start + axis * (len * t));
        }

        for (int i = 0; i < 4; i++)
        {
            vec3d offset;
            if (i == 0) offset = side * radius;
            else if (i == 1) offset = side * -radius;
            else if (i == 2) offset = up * radius;
            else offset = up * -radius;

            int sx0 = 0, sy0 = 0, sx1 = 0, sy1 = 0;
            if (project(start + offset, sx0, sy0) && project(end + offset, sx1, sy1))
                GFX::GFXEngine::DrawLine(scr, Common::Line(sx0, sy0, sx1, sy1), r, g, b);
        }
    };

    // Optional near-object labels. There is deliberately no top-left fallback.
    CmdStream labels;
    labels.reserve(4096);
    FontUA::select_tileset(&labels, 15);
    int labelCount = 0;
    const int MAX_LABELS = 40;
    auto drawLabel = [&](const vec3d &worldPos, const char *txt, uint8_t r, uint8_t g, uint8_t b) {
        if (labelCount >= MAX_LABELS)
            return;

        int sx = 0, sy = 0;
        if (!project(worldPos, sx, sy))
            return;

        // Keep labels away from the HUD/top-left/top bars. If it cannot be near the object cleanly, skip it.
        if (sx < 80 || sx > screenW - 180 || sy < 80 || sy > screenH - 80)
            return;

        FontUA::set_xpos(&labels, sx + 6);
        FontUA::set_ypos(&labels, sy - 6);
        // op19: add_txt (op18) outputs at x_out_txt/y_out_txt, NOT at x_out/y_out.
        // Without this copy those stay at 0,0 and every label lands top-left over the HUD.
        FontUA::copy_position(&labels);
        FontUA::set_txtColor(&labels, r, g, b);
        FontUA::add_txt(&labels, 220, 1, txt);
        labelCount++;
    };

    std::vector<NC_STACK_ypabact *> objs;
    objs.reserve(512);

    auto addObj = [&](NC_STACK_ypabact *unit) {
        if (!unit)
            return;
        if (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2 | BACT_STFLAG_CLEAN))
            return;
        // OpenNeoUA invisible: cloaked stealth units are excluded from the F9 collision/
        // radius debug overlay (radius rings + labels) just like any other UI.
        if (unit->IsInvisibleUnrevealed())
            return;
        for (NC_STACK_ypabact *old : objs)
            if (old == unit)
                return;
        objs.push_back(unit);
    };

    // _unitsList contains major/root BACTs. Cell lists contain live sector occupants, including many normal units/projectiles.
    for (NC_STACK_ypabact *unit : _unitsList)
        addObj(unit);

    for (cellArea &cell : _cells)
    {
        for (NC_STACK_ypabact *unit : cell.unitsList.safe_iter())
            addObj(unit);
    }

    vec3d camPos = view->CalcPos;
    const float RING_MAX_DIST = 8000.0f;
    const float LABEL_MAX_DIST = 3000.0f;

    for (NC_STACK_ypabact *unit : objs)
    {
        float dist = (unit->_position - camPos).length();
        if (dist > RING_MAX_DIST)
            continue;

        bool isCurrentControlled = yw_DebugIsCurrentControlledBact(this, unit);
        vec3d pos = unit->_position;
        World::rbcolls *colls = unit->getBACT_collNodes();
        // Units with disable_collision have no active collision volumes, so none are drawn.
        const bool collisionActive = !unit->CollisionsDisabled();
        bool legacyRadiusCollisionEnabled = collisionActive && !unit->HasCollisionShape() && unit->UsesLegacyRadiusCollision();
        if (collisionActive && unit->HasCollisionShape() && !isCurrentControlled)
        {
            const mat3x3 rotation = unit->_rotation.Transpose();
            const vec3d origin = unit->GetBodyPosition();
            for (const auto &part : unit->_collisionShape->parts)
            {
                struct ProjectedVertex { int x = 0, y = 0; bool visible = false; };
                std::vector<ProjectedVertex> projected(part.vertices.size());
                for (size_t i = 0; i < part.vertices.size(); ++i)
                    projected[i].visible = project(origin + rotation.Transform(part.vertices[i]),
                                                   projected[i].x, projected[i].y);
                for (const auto &edge : part.debugEdges)
                {
                    const auto &from = projected[edge[0]], &to = projected[edge[1]];
                    if (from.visible && to.visible)
                        GFX::GFXEngine::DrawLine(scr, Common::Line(from.x,from.y,to.x,to.y), 50,220,220);
                }
            }
        }

        // Red legacy radius. Manual coll_* suppresses the default radius only
        // when the script did not explicitly author radius. Native Robo volumes
        // and explicitly authored hybrid radius + coll_* keep the red sphere.
        // Hide collision geometry for the unit currently controlled by the player
        // so its own hitbox does not obstruct the cockpit/debug view.
        if (!isCurrentControlled && legacyRadiusCollisionEnabled)
        {
            float R = unit->_radius;
            if (R > 0.01f)
            {
                drawRing(pos, R, 0, 220, 60, 60);
                drawRing(pos, R, 1, 220, 60, 60);
                drawRing(pos, R, 2, 220, 60, 60);

                if (dist <= LABEL_MAX_DIST)
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "radius=%.0f", R);
                    drawLabel(pos + vec3d::OY(R + 15.0f), buf, 220, 60, 60);
                }
            }
        }

        // Compound collision spheres: green (same color as the Studio) for vehicles and weapons.
        // Compound slot/radius/offset labels are intentionally omitted;
        // only the legacy radius keeps a numeric label.
        if (collisionActive && !isCurrentControlled && colls && !unit->HasCollisionShape())
        {
            mat3x3 rotT = unit->_rotation.Transpose();
            const uint8_t sphereR = 60;
            const uint8_t sphereG = 220;
            const uint8_t sphereB = 60;
            for (const World::TRoboColl &cs : colls->roboColls)
            {
                if (!cs.debug_visible || cs.robo_coll_radius < 0.01f)
                    continue;

                vec3d sphWorld = pos + rotT.Transform(cs.coll_pos);
                float debugRadius = cs.robo_coll_radius;
                drawRing(sphWorld, debugRadius, 0, sphereR, sphereG, sphereB);
                drawRing(sphWorld, debugRadius, 1, sphereR, sphereG, sphereB);
                drawRing(sphWorld, debugRadius, 2, sphereR, sphereG, sphereB);
            }
        }

        // --- GAMEPLAY RANGE RADII (single horizontal ring, distinct colors) ---
        // Drawn flat to keep the overlay readable. Each only appears if its value is set.

        // Blue = power_radius (mobile/static power influence). Read from the vehicle proto.
        if ( unit->_bact_type != BACT_TYPES_MISSLE &&
             unit->_bact_type != BACT_TYPES_ROBO &&
             unit->_bact_type != BACT_TYPES_GUN &&
             (size_t)(unit->_mimic_disguise_vehicleID ? unit->_mimic_disguise_vehicleID : unit->_vehicleID) < _vhclProtos.size() )
        {
            uint8_t protoId = unit->_mimic_disguise_vehicleID ? unit->_mimic_disguise_vehicleID : unit->_vehicleID;
            const World::TVhclProto &proto = _vhclProtos[protoId];
            if ( proto.power > 0 && proto.power_radius > 0.01f )
                drawFlatRing(pos, proto.power_radius, 60, 120, 230);
        }

        // Yellow = spawn trigger radius.
        if ( unit->_spawn_trigger_radius > 0.01f )
            drawFlatRing(pos, unit->_spawn_trigger_radius, 230, 220, 60);

        // Magenta = proximity defense trigger radius.
        if ( unit->_proximity_defense_trigger_radius > 0.01f )
            drawFlatRing(pos, unit->_proximity_defense_trigger_radius, 230, 60, 200);

        // Cyan = model=kamikaze XYZ trigger sphere. Contact mode uses the
        // effective carrier collision bound; no flat/XZ-only fuse is implied.
        float kamikazeDebugRadius = 0.0f;
        if ( unit->GetKamikazeDebugSphere(&kamikazeDebugRadius) )
        {
            drawRing(pos, kamikazeDebugRadius, 0, 60, 220, 220);
            drawRing(pos, kamikazeDebugRadius, 1, 60, 220, 220);
            drawRing(pos, kamikazeDebugRadius, 2, 60, 220, 220);
        }

        // Red tube = active model=laser beam thickness, using the weapon radius.
        if ( unit->_laser_active &&
             unit->_laser_weapon >= 0 &&
             (size_t)unit->_laser_weapon < _weaponProtos.size() )
        {
            float laserRadius = _weaponProtos[unit->_laser_weapon].radius;
            if ( laserRadius < 1.0f )
                laserRadius = 1.0f;

            if ( !unit->_laser_beams.empty() )
            {
                for (const NC_STACK_ypabact::TLaserBeamRuntime &beam : unit->_laser_beams)
                    drawBeamTube(beam.start, beam.end, laserRadius, 255, 60, 60);
            }
            else
            {
                drawBeamTube(unit->_laser_beam_start, unit->_laser_beam_end, laserRadius, 255, 60, 60);
            }

            if (dist <= LABEL_MAX_DIST)
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "laser radius=%.0f", laserRadius);
                vec3d labelPos = unit->_laser_beams.empty()
                               ? (unit->_laser_beam_start + unit->_laser_beam_end) * 0.5f
                               : (unit->_laser_beams[0].start + unit->_laser_beams[0].end) * 0.5f;
                drawLabel(labelPos, buf, 255, 60, 60);
            }
        }

        // Orange = ai_fire_trigger_area_below_xz AI fire volume of each bomb-like weapon slot, from the
        // fire point down to the terrain. Bright for the weapon the AI fires now.
        if ( !isCurrentControlled && unit->_bact_type != BACT_TYPES_MISSLE )
        {
            const int currentWeapon = unit->GetCurrentWeaponId();
            const int slots[4] = { unit->_weapon, unit->_extra_weapons[0],
                                   unit->_extra_weapons[1], unit->_extra_weapons[2] };
            for (int s = 0; s < 4; s++)
            {
                const int weaponId = slots[s];
                if ( weaponId < 0 || (s > 0 && weaponId == 0) ||
                     (size_t)weaponId >= _weaponProtos.size() )
                    continue;

                const World::TWeapProto &wproto = _weaponProtos[weaponId];
                TFireTriggerArea box;
                if ( !unit->GetAIFireTriggerArea(wproto, &box) )
                    continue;

                // A vertical ray keeps the same sector from start to end, so one
                // long probe finds the terrain reliably.
                ypaworld_arg136 ground;
                ground.stPos = box.top;
                ground.vect = vec3d(0.0, 20000.0, 0.0);
                ground.flags = 0;
                ypaworld_func136(&ground);
                const float bottomY = ground.isect ? ground.isectPos.y
                                                   : box.top.y + World::CVSectorLength;

                const bool active = weaponId == currentWeapon;
                const uint8_t r = active ? 255 : 160;
                const uint8_t g = active ? 150 : 100;
                const uint8_t b = active ? 30 : 40;

                vec3d corners[8];
                for (int i = 0; i < 4; i++)
                {
                    const float x = (i & 1) ? box.maxX : box.minX;
                    const float z = (i & 2) ? box.halfZ : -box.halfZ;
                    corners[i] = box.top + box.right * x + box.forward * z;
                    corners[i + 4] = vec3d(corners[i].x, bottomY, corners[i].z);
                }

                static const int edges[12][2] = {
                    {0, 1}, {1, 3}, {3, 2}, {2, 0},
                    {4, 5}, {5, 7}, {7, 6}, {6, 4},
                    {0, 4}, {1, 5}, {2, 6}, {3, 7} };
                for (const auto &edge : edges)
                {
                    int x0, y0, x1, y1;
                    if ( project(corners[edge[0]], x0, y0) && project(corners[edge[1]], x1, y1) )
                        GFX::GFXEngine::DrawLine(scr, Common::Line(x0, y0, x1, y1), r, g, b);
                }

                if ( dist <= LABEL_MAX_DIST )
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "fire trigger area %.0fx%.0f",
                             wproto.ai_fire_trigger_area_x, wproto.ai_fire_trigger_area_z);
                    drawLabel(box.top, buf, r, g, b);
                }
            }
        }
    }

    if (_collisionScene)
        for (const Collision::Contact &contact : _collisionScene->lastContacts)
        {
            if ((contact.point - camPos).length() > RING_MAX_DIST) continue;
            int x0,y0,x1,y1;
            if (project(contact.point,x0,y0) &&
                project(contact.point + contact.normal * std::max(20.0,contact.depth),x1,y1))
                GFX::GFXEngine::DrawLine(scr,Common::Line(x0,y0,x1,y1),255,220,30);
        }

    // --- TRANSIENT DEBUG RADII (lifetime is maintained independently of rendering) ---
    for (const DebugBlastRing &ring : _debugBlastRings)
    {
        if ((ring.pos - camPos).length() > RING_MAX_DIST)
            continue;

        if ( ring.sphere3d )
        {
            drawRing(ring.pos, ring.radius, 0, ring.r, ring.g, ring.b);
            drawRing(ring.pos, ring.radius, 1, ring.r, ring.g, ring.b);
            drawRing(ring.pos, ring.radius, 2, ring.r, ring.g, ring.b);
        }
        else
        {
            drawFlatRing(ring.pos, ring.radius, ring.r, ring.g, ring.b);
        }
    }

    FontUA::reset_tileset(&labels, 15);
    FontUA::set_end(&labels);
    GFX::Engine.ProcessDrawSeq(labels);
}

void NC_STACK_ypaworld::ExpireDebugBlastRings()
{
    for (size_t i = 0; i < _debugBlastRings.size(); )
    {
        const DebugBlastRing &ring = _debugBlastRings[i];
        if ( _timeStamp < ring.createdStamp || _timeStamp >= ring.expireStamp )
        {
            _debugBlastRings[i] = _debugBlastRings.back();
            _debugBlastRings.pop_back();
            continue;
        }
        i++;
    }
}

void NC_STACK_ypaworld::DebugAddBlastRing(const vec3d &pos, float radius, uint8_t r, uint8_t g, uint8_t b)
{
    // Only record while the F9 overlay is active, so there is zero cost when off.
    if ( !_showCollDebug || radius < 0.01f )
        return;

    DebugBlastRing ring;
    ring.pos = pos;
    ring.radius = radius;
    ring.r = r;
    ring.g = g;
    ring.b = b;
    ring.sphere3d = false;
    ring.createdStamp = _timeStamp;
    ring.expireStamp = _timeStamp + 1536; // ~1.5s (1024 ticks = 1s)

    _debugBlastRings.push_back(ring);
    if ( _debugBlastRings.size() > 256 )
        _debugBlastRings.erase(_debugBlastRings.begin());
}

void NC_STACK_ypaworld::DebugAddAtDeathSphere(const vec3d &pos, float radius)
{
    // F9 only: preserve the exact at-death effect volume after the source disappears.
    if ( !_showCollDebug || radius < 0.01f )
        return;

    DebugBlastRing ring;
    ring.pos = pos;
    ring.radius = radius;
    ring.r = 185;
    ring.g = 80;
    ring.b = 255;
    ring.sphere3d = true;
    ring.createdStamp = _timeStamp;
    ring.expireStamp = _timeStamp + 3072; // ~3.0s (1024 ticks = 1s)

    _debugBlastRings.push_back(ring);
    if ( _debugBlastRings.size() > 256 )
        _debugBlastRings.erase(_debugBlastRings.begin());
}
