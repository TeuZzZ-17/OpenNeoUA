// ypaworld game logic core: level loading, maps, units creation, sector ownership and per-frame world updates.
// Areas split into their own files:
//   yw_spectator.cpp ....... spectator mode and follow camera
//   yw_superitem.cpp ....... super items (custom waves, detonation, front)
//   yw_building_spawn.cpp .. buildings that spawn units
//   yw_mobile_power.cpp .... power stations, mobile power generators and sector energy
//   yw_fx.cpp .............. transient visuals, chain/decoration/atmospheric FX
//   yw_render.cpp .......... rendering of sectors, fillers, sky, transient VPs and the game view
//   yw_recorder.cpp ........ replay recorder
//   yw_debug.cpp ........... debug overlays and DPS meter
//   yw_artillery_markers.cpp artillery shell map markers
// Shared helpers are declared in yw_game_impl.h.
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
#include "world/collision_shape.h"
#include "game/ypatank.h"

TRenderingSector rendering_sectors[YW_RENDER_SECTORS_MAX + 1][ YW_RENDER_SECTORS_MAX + 1];

NC_STACK_bitmap * loadDisk_screen(NC_STACK_ypaworld *yw)
{
    SFXEngine::SFXe.StopMusicTrack();

    const char *v3;

    if ( yw->_screenSize.x <= 360 )
        v3 = "disk320.ilbm";
    else if ( yw->_screenSize.x > 600 )
        v3 = "disk640.ilbm";
    else
        v3 = "disk512.ilbm";

    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:mc2res");

    NC_STACK_bitmap *disk = Utils::ProxyLoadImage({
        {NC_STACK_rsrc::RSRC_ATT_NAME, std::string(v3)},
        {NC_STACK_bitmap::BMD_ATT_CONVCOLOR, (int32_t)1}});

    Common::Env.SetPrefix("rsrc", oldRsrc);

    return disk;
}

void draw_splashScreen(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen)
{
    if ( splashScreen )
    {
        GFX::rstr_arg204 a4;

        a4.pbitm = splashScreen->GetBitmap();

        a4.float4  = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        a4.float14 = Common::FRect(-1.0, -1.0, 1.0, 1.0);

        GFX::displ_arg263 v4;
        if (yw->_mousePointers[5])
            v4.bitm = yw->_mousePointers[5]->GetBitmap();
        v4.pointer_id = 6;

        GFX::Engine.SetCursor(v4.pointer_id, 0);

        GFX::Engine.BeginFrame();
        GFX::Engine.raster_func202(&a4);
        GFX::Engine.EndFrame();

        GFX::Engine.BeginFrame();
        GFX::Engine.raster_func202(&a4);
        GFX::Engine.EndFrame();
    }
}

void drawSplashScreenWithTOD(NC_STACK_ypaworld *yw, NC_STACK_bitmap *splashScreen, const std::string &text)
{
    if ( splashScreen )
    {
        GFX::rstr_arg204 a4;

        a4.pbitm = splashScreen->GetBitmap();

        a4.float4  = Common::FRect(-1.0, -1.0, 1.0, 1.0);
        a4.float14 = Common::FRect(-1.0, -1.0, 1.0, 1.0);

        GFX::displ_arg263 v4;
        if (yw->_mousePointers[5])
            v4.bitm = yw->_mousePointers[5]->GetBitmap();
        v4.pointer_id = 6;

        GFX::Engine.SetCursor(v4.pointer_id, 0);

        GFX::Engine.BeginFrame();
        GFX::Engine.raster_func202(&a4);
        splashScreen_OutText(yw, text, yw->_screenSize.x / 7, yw->_screenSize.y / 5);
        GFX::Engine.EndFrame();

        GFX::Engine.BeginFrame();
        GFX::Engine.raster_func202(&a4);
        splashScreen_OutText(yw, text, yw->_screenSize.x / 7, yw->_screenSize.y / 5);
        GFX::Engine.EndFrame();
    }
}

void sb_0x44ca90__sub2(NC_STACK_ypaworld *yw, TLevelDescription *mapp)
{
    if (!mapp->Palettes.empty())
    {
        if (!mapp->Palettes[0].empty())
        {
            NC_STACK_bitmap *ilbm = Utils::ProxyLoadImage({
                {NC_STACK_rsrc::RSRC_ATT_NAME, mapp->Palettes[0]},
                {NC_STACK_bitmap::BMD_ATT_HAS_COLORMAP, (int32_t)1}});

            if (ilbm)
            {
                GFX::Engine.SetPalette(*ilbm->getBMD_palette());
                ilbm->Delete();
            }
            else
            {
                ypa_log_out("WARNING: slot #%d [%s] init failed!\n", 0, mapp->Palettes[0].c_str());
            }
        }
    }
}

int NC_STACK_ypaworld::LevelCommonLoader(TLevelDescription *mapp, int levelID, int a5)
{
    if ( !mapp || levelID < 0 ||
            (size_t)levelID >= _globalMapRegions.MapRegions.size() )
        return 0;

    StopAmbientLevelSound();
    ResetLocations();
    StopAtmosphericFXLoopSound();
    ClearMobilePowerFXRuntime();

    int ok = 0;

    *mapp = TLevelDescription();

    _gameplayStats.fill( World::TPlayerStatus() );

    _debugBlastRings.clear();
    ResetPlasmaCurrencyRuntime();
    _timeStamp = 0;
    System::GameClock.Reset(_timeStamp);
    _msgTimestampHSReturn = 0;
    _msgTimestampEnemySector = 0;
    _msgTimestampGates = 0;
    _msgTimestampPSUnderAtk = 0;
    _framesElapsed = 0;

    _levelInfo.LevelID = levelID;
    _levelInfo.Mode = a5;
    _levelInfo.State = TLevelInfo::STATE_PLAYING;
    _levelInfo.OwnerMask = 0;
    _levelInfo.UserMask = 0;

    _cellOnMouse = 0;
    _bactOnMouse = nullptr;
    _damageHoverTargets.clear();
    _bactPrevClicked = 0;
    ClearArtilleryShellMarkers();
    _artilleryShellManualGid = 0;
    _artilleryShellManualRadius = 0.0f;
    _viewerBact = nullptr;
    _userRobo = nullptr;
    _userUnit = nullptr;
    _makingWaypointsMode = false;
    _gamePaused = false;
    _gamePausedTimeStamp = 0;
    _debugGameplaySlowMotion = false;
    _debugGameplayFrozen = false;
    _debugHostStationCheat = false;
    _debugGlobalInvulnerability = false;
    _joyIgnoreX = 1;
    _joyIgnoreY = 1;
    _joyIgnoreZ = 0;
    _helpURL.clear();
    _prevUnitId = 0;
    _fireBtnIsDown = false;
    _fireBtnDownHappen = false;
    _playerHSDestroyed = false;
    _vehicleSectorRatio = 0;
    _beamEnergyCurrent = 0;
    _invulnerable = 0;

    _levelInfo.Gates.clear();
    _levelInfo.SuperItems.clear();

    _techUpgrades.clear();

    _netEvent = TNetGameEvent();
    _countUnitsPerOwner.fill(0);

    _dbgTotalSquadCountMax = 0;
    _dbgTotalVehicleCountMax = 0;
    _dbgTotalFlakCountMax = 0;
    _dbgTotalWeaponCountMax = 0;
    _dbgTotalRoboCountMax = 0;

    _playerOwner = 0;

    /* Set hidden fractions to default world's*/
    _hiddenFractions = _worldHiddenFractions;

    if ( _gfxMode != GFX::Engine.GetGfxMode() || _GameShell->IsWindowedFlag() != GFX::Engine.GetGfxMode().windowed )
    {
        GFX::Engine.SetResolution(_gfxMode, _GameShell->IsWindowedFlag());

        GFX::Engine.setWDD_cursor( (_preferences & World::PREF_SOFTMOUSE) != 0 );

        const Common::Point physicalScreenSize = GFX::Engine.GetScreenSize();
        std::string fontStr = physicalScreenSize.x >= 512 ? Locale::Text::Font() : Locale::Text::SmallFont();
        fontStr = System::ResolveMenuFontDescr(fontStr);
        GFX::Engine.LoadFontByDescr(fontStr);
        Gui::UA::LoadFont(fontStr);
    }

    // Loading screens still use the real output resolution. The gameplay UI
    // switches to its 480-line logical canvas only after loading is complete.
    _screenSize = GFX::Engine.GetScreenSize();

    NC_STACK_bitmap *diskScreenImage = loadDisk_screen(this);

    if ( diskScreenImage )
        draw_splashScreen(this, diskScreenImage);


    std::string oldRsrc = Common::Env.SetPrefix("rsrc", "data:fonts");

    int v19 = load_fonts_and_icons();

    Common::Env.SetPrefix("rsrc", oldRsrc);

    if ( !v19 )
        return 0;

    int tod = loadTOD(this, "tod.def");

    int next_tod = tod + 1;

    if ( next_tod + 2490 > 2512 )
        next_tod = 0;

    writeTOD(this, "tod.def", next_tod);

    if ( diskScreenImage )
    {
        drawSplashScreenWithTOD(this, diskScreenImage, Locale::Text::ToD(tod, " "));
        diskScreenImage->Delete();
    }

    _screenSize = GFX::Engine.GetVirtualUIResolution();
    Input::Engine.SetPointerResolution(GFX::Engine.GetScreenSize(), _screenSize);
    Gui::Root::Instance.SetScreenSize(_screenSize);

    _profileFramesCount = 0;
    for (int i = 0; i < PFID_MAX; i++)
    {
        _profileVals[i] = 0;
        _profileMax[i] = 0;
        _profileMin[i] = 100000;
        _profileTotal[i] = 0;
    }

    _history.Clear();
    _historyLastIsTimeStamp = false;

    audio_volume = SFXEngine::SFXe.getMasterVolume();

    _voiceMessage.Reset();

    Common::Env.SetPrefix("rsrc", "data:");

    if ( sub_4DA41C(mapp, _globalMapRegions.MapRegions[_levelInfo.LevelID].MapDirectory) && mapp->IsOk() )
    {
        Common::DeleteAndNull(&_script);

        if (mapp->EventLoopID >= 1 && mapp->EventLoopID <= 3)
        {
            _script = new World::LuaEvents(this);
            _script->LoadFile(fmt::sprintf("lesson%d.lua", mapp->EventLoopID));
            _script->CallInit(_timeStamp);
        }
        else if (!_luaScriptName.empty())
        {
            _script = new World::LuaEvents(this);
            _script->LoadFile(_luaScriptName);
            _script->CallInit(_timeStamp);
        }

        _energyAccumMap.Clear();
        _nextPSForUpdate = 0;

        _inBuildProcess.clear();

        Common::Env.SetPrefix("rsrc", fmt::sprintf("data:set%d", mapp->SetID));

        sb_0x44ca90__sub2(this, mapp);

        if ( yw_LoadSet(mapp->SetID) )
        {
            if ( yw_loadSky(this, mapp->SkyStr) )
                ok = 1;
        }
    }

    FFeedback_Init();

    if ( ok )
    {
        StartAmbientLevelSound(*mapp);
        LoadAtmosphericFXProfile(*mapp);
    }
    else
    {
        ClearAtmosphericFXRuntime();
        ClearMobilePowerFXRuntime();
        _atmosphericFXProfile = World::TAtmosphericFXProfile();
    }

    return ok;
}

bool NC_STACK_ypaworld::LoadTypeMap(const std::string &mapName)
{
    if ( _lvlTypeMap.IsNull() )
        _lvlTypeMap = World::LoadMapDataFromImage(mapName);

    if ( _lvlTypeMap.IsNull() )
        return false;

    SetMapSize(_lvlTypeMap.Size());

    for(int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            const uint8_t sectorId = _lvlTypeMap(cell.CellId);
            TSectorDesc *sectp = &_secTypeArray[sectorId];

            cell.type_id = sectorId;
            cell.SectorType = sectp->SectorType;
            cell.energy_power = 0;

            const int maxBuildingX = sectp->SectorType == 1 ? 1 : 3;
            const int maxBuildingY = sectp->SectorType == 1 ? 1 : 3;
            for (int bldY = 0; bldY < maxBuildingY; bldY++)
            {
                for (int bldX = 0; bldX < maxBuildingX; bldX++)
                {
                    TSubSectorDesc *subSector = sectp->SubSectors.At(bldX, bldY);
                    if ( !subSector )
                    {
                        ypa_log_out(
                            "LoadTypeMap: invalid SET sector descriptor: map='%s' cell=[%d,%d] sector_id=%u sector_type=%d slot=[%d,%d].\n",
                            mapName.c_str(), x, y, (unsigned)sectorId,
                            sectp->SectorType, bldX, bldY);
                        CrashDiag::Breadcrumb(
                            "LEVEL_TYPEMAP_INVALID",
                            "map=%s cell=%d,%d sector_id=%u sector_type=%d slot=%d,%d",
                            mapName.c_str(), x, y, (unsigned)sectorId,
                            sectp->SectorType, bldX, bldY);
                        return false;
                    }

                    cell.buildings_health.At(bldX, bldY) = subSector->StartHealth;
                }
            }
        }
    }
    return true;
}

bool NC_STACK_ypaworld::LoadOwnerMap(const std::string &mapName)
{
    _countSectorsPerOwner.fill(0);

    if ( _lvlOwnMap.IsNull() )
        _lvlOwnMap = World::LoadMapDataFromImage(mapName);

    if ( _lvlOwnMap.IsNull() )
        return false;

    if ( _lvlOwnMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlOwnMap.Width(), _lvlOwnMap.Height(), _mapSize.x, _mapSize.y);
        _lvlOwnMap.Clear();
        return false;
    }


    for (uint32_t yy = 0; yy < _lvlOwnMap.Height(); yy++)
    {
        for (uint32_t xx = 0; xx < _lvlOwnMap.Width(); xx++)
        {
            Common::Point cellId(xx, yy);
            if ( IsGamePlaySector( cellId ) )
            {
                _cells(cellId).owner = _lvlOwnMap(cellId);
                _countSectorsPerOwner[ _lvlOwnMap(cellId) ]++;
            }
            else
            {
                _cells(cellId).owner = 0;
                _countSectorsPerOwner[ 0 ]++;
            }
        }
    }

    return true;
}

bool NC_STACK_ypaworld::LoadHightMap(const std::string &mapName)
{
    _borderWallTopReady = false;

    if ( _lvlHeightMap.IsNull() )
        _lvlHeightMap = World::LoadMapDataFromImage(mapName);

    if ( _lvlHeightMap.IsNull() )
        return false;

    if ( _lvlHeightMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlHeightMap.Width(), _lvlHeightMap.Height(), _mapSize.x, _mapSize.y);
        _lvlHeightMap.Clear();
        return false;
    }

    for (int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            cellArea &cell = _cells(x, y);
            cell.height = (-100.0) * _lvlHeightMap( cell.CellId );
            cell.CenterPos = World::SectorIDToCenterPos3( cell.CellId );
            cell.CenterPos.y = cell.height;
        }
    }

    for (int y = 1; y < _mapSize.y; y++)
    {
        for (int x = 1; x < _mapSize.x; x++)
        {
            _cells(x, y).averg_height = (_cells(x    ,     y).height +
                                         _cells(x - 1,     y).height +
                                         _cells(x - 1, y - 1).height +
                                         _cells(x    , y - 1).height ) / 4.0;
        }
    }

    // The four vanilla RAND wall skeletons have a 2700-unit vertical span.
    // Their local ground stays at Y=0, so a per-cell Y scale can align only
    // the upper edge without moving the terrain strip at the map boundary.
    for (int y = 0; y < _mapSize.y; y++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            const bool corner = (x == 0 || x == _mapSize.x - 1) &&
                                (y == 0 || y == _mapSize.y - 1);
            const cellArea &cell = _cells(x, y);

            if ( !cell.IsBorder() || corner )
                continue;

            const float vanillaTopY = cell.height - kMapBorderWallVanillaHeight;
            if ( !_borderWallTopReady || vanillaTopY < _borderWallTopY )
            {
                _borderWallTopY = vanillaTopY;
                _borderWallTopReady = true;
            }
        }
    }

    return true;
}

bool NC_STACK_ypaworld::yw_createRobos(const std::vector<MapRobo> &Robos)
{
    if ( _levelInfo.Mode != 1 )
    {
        _levelInfo.OwnerMask = 0;
        int firstOwner = Robos.empty() ? 0 : Robos[0].Owner;
        _levelInfo.UserMask = (firstOwner >= 0 && firstOwner < (int)World::CVFractionsCount) ? (1 << firstOwner) : 0;

        if(IsPlasmaCurrencyEnabled())
            for(int id:_workshopOwned)
                if(id>=0&&id<(int)_vhclProtos.size()&&_vhclProtos[id].Index>=0&&
                   _vhclProtos[id].model_id!=BACT_TYPES_ROBO)
                    _vhclProtos[id].disable_enable_bitmask|=_levelInfo.UserMask;
        bool first = true;

        for ( const MapRobo &roboInf : Robos)
        {
            ypaworld_arg136 v14;
            v14.stPos = vec3d::X0Z(roboInf.Pos) - vec3d::OY(30000.0);
            v14.vect = vec3d::OY(50000.0);
            v14.flags = 0;

            ypaworld_arg146 v15;
            v15.vehicle_id = roboInf.VhclID;
            if(first&&IsPlasmaCurrencyEnabled()&&_workshopOwned.count(_workshopHost)&&
               _workshopHost>=0&&_workshopHost<(int)_vhclProtos.size()&&
               _vhclProtos[_workshopHost].model_id==BACT_TYPES_ROBO)
                v15.vehicle_id=_workshopHost;
            v15.pos = roboInf.Pos;

            ypaworld_func136(&v14);

            if ( v14.isect )
                v15.pos.y += v14.isectPos.y;

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>( ypaworld_func146(&v15) );

            if ( robo )
            {
                int v20 = 0;

                ypaworld_func134(robo);

                int v12;

                if ( !first )
                {
                    v12 = roboInf.Energy;
                }
                else
                {
                    v12 = roboInf.Energy / 4;

                    if ( v12 < _maxRoboEnergy )
                    {
                        v12 = _maxRoboEnergy;
                        v20 = _maxReloadConst;
                    }

                }

                robo->_owner = roboInf.Owner;
                robo->_energy = v12;
                robo->_energy_max = v12;

                if ( !v20 )
                {
                    if ( roboInf.ReloadConst )
                        v20 = roboInf.ReloadConst;
                    else
                        v20 = robo->_energy_max;
                }

                robo->_reload_const = v20;

                robo->setBACT_bactCollisions(1);
                robo->setROBO_fillMode(15);
                robo->setROBO_battVehicle(v12);
                robo->setROBO_battBeam(v12);

                _levelInfo.OwnerMask |= 1 << roboInf.Owner;

                robo->setROBO_epConquer(roboInf.ConBudget);
                robo->setROBO_epDefense(roboInf.DefBudget);
                robo->setROBO_epRadar(roboInf.RadBudget);
                robo->setROBO_epPower(roboInf.PowBudget);
                robo->setROBO_epSafety(roboInf.SafBudget);
                robo->setROBO_epChangeplace(roboInf.CplBudget);
                robo->setROBO_epRobo(roboInf.RobBudget);
                robo->setROBO_epReconnoitre(roboInf.RecBudget);
                robo->setROBO_viewAngle(roboInf.ViewAngle);
                robo->setROBO_safDelay(roboInf.SafDelay);
                robo->setROBO_powDelay(roboInf.PowDelay);
                robo->setROBO_cplDelay(roboInf.CplDelay);
                robo->setROBO_radDelay(roboInf.RadDelay);
                robo->setROBO_defDelay(roboInf.DefDelay);
                robo->setROBO_conDelay(roboInf.ConDelay);
                robo->setROBO_recDelay(roboInf.RecDelay);
                robo->setROBO_robDelay(roboInf.RobDelay);

                if ( first )
                {
                    robo->setBACT_viewer(true);
                    robo->setBACT_inputting(true);
                }
            }

            first = false;
        }
    }
    return true;
}

bool NC_STACK_ypaworld::LoadBlgMap(const std::string &mapName)
{
    if ( _lvlBuildingsMap.IsNull() )
        _lvlBuildingsMap = World::LoadMapDataFromImage(mapName);

    if ( _lvlBuildingsMap.IsNull() )
        return false;

    if ( _lvlBuildingsMap.Size() != _mapSize )
    {
        ypa_log_out("Mapsize mismatch %s: is [%d,%d], should be [%d,%d].\n", mapName.c_str(), _lvlBuildingsMap.Width(), _lvlBuildingsMap.Height(), _mapSize.x, _mapSize.y);
        _lvlBuildingsMap.Clear();
        return false;
    }

    for ( int y = 0; y < _mapSize.y; y++)
    {
        for ( int x = 0; x < _mapSize.x; x++)
        {
            int blg = _lvlBuildingsMap(x, y);
            cellArea &cell = _cells(x, y);

            if (blg && cell.owner)
            {
                ypaworld_arg148 arg148;

                arg148.owner = cell.owner;
                arg148.blg_ID = blg;
                arg148.field_C = 1;
                arg148.field_18 = 0;
                arg148.CellId = cell.CellId;

                ypaworld_func148(&arg148);
            }
        }
    }

    return true;
}

void NC_STACK_ypaworld::yw_InitSquads(const std::vector<MapSquad> &squads)
{
    if ( _levelInfo.Mode != 1 )
    {
        std::unique_ptr<Collision::Scene> localPlacementScene;
        Collision::Scene *placementScene = _collisionScene.get();
        if ( !placementScene )
        {
            localPlacementScene.reset(new Collision::Scene(*this));
            placementScene = localPlacementScene.get();
        }
        size_t i = 0;
        for ( const MapSquad &squad : squads )
        {
            NC_STACK_yparobo *robo = nullptr;

            for( NC_STACK_ypabact *unit : _unitsList )
            {
                if ( unit->_bact_type == BACT_TYPES_ROBO && unit->_owner == squad.Owner)
                {
                    robo = dynamic_cast<NC_STACK_yparobo *>(unit);
                    break;
                }
            }

            if ( !robo )
            {
                ypa_log_out("WARNING: yw_InitSquads(): no host robo for squad[%d], owner %d!\n", i, squad.Owner);
            }
            else
            {
                // yw_InitSquads runs after LoadBlgMap, so building geometry is
                // already available.  Capture this host's existing children so
                // only the freshly authored squad units are corrected; saved
                // units and unrelated runtime children keep their positions.
                std::set<NC_STACK_ypabact *> existingSquadUnits;
                const auto collectSquad = [&](const auto &visit,
                                              NC_STACK_ypabact *unit) -> void {
                    if ( !unit ) return;
                    existingSquadUnits.insert(unit);
                    for ( NC_STACK_ypabact *child : unit->GetKidList() )
                        visit(visit, child);
                };
                for ( NC_STACK_ypabact *child : robo->GetKidList() )
                    collectSquad(collectSquad, child);
                vec3d squadPos;

                ypaworld_arg136 arg136;
                arg136.stPos.x = squad.X;
                arg136.stPos.y = -50000.0;
                arg136.stPos.z = squad.Z;
                arg136.vect = vec3d::OY(100000.0);
                arg136.flags = 0;
                ypaworld_func136(&arg136);

                if ( arg136.isect )
                    squadPos = arg136.isectPos - vec3d::OY(50.0);
                else
                {
                    yw_130arg sect_info;
                    sect_info.pos_x = squad.X;
                    sect_info.pos_z = squad.Z;

                    if ( !GetSectorInfo(&sect_info) )
                    {
                        ypa_log_out("yw_InitSquads(): no valid position for squad[%d]!\n", i);
                        return;
                    }

                    squadPos.x = squad.X;
                    squadPos.y = sect_info.pcell->height;
                    squadPos.z = squad.Z;
                }
                // Create squad by robo method
                robo->MakeSquad( std::vector<int>(squad.Count, squad.VhclID), squadPos, squad.Useable); // yparobo_func133

                const auto placeFreshSquad = [&](const auto &visit,
                                                 NC_STACK_ypabact *unit) -> void {
                    if ( !unit ) return;
                    if ( !existingSquadUnits.count(unit) &&
                         (unit->_status == BACT_STATUS_NORMAL ||
                          unit->_status == BACT_STATUS_IDLE) &&
                         placementScene->PlaceAboveTerrain(unit) )
                    {
                        // An authored tank placed on a roof starts supported,
                        // using the ordinary ground controller rather than a fall.
                        if ( auto *tank=dynamic_cast<NC_STACK_ypatank *>(unit) )
                            if ( tank->AlignVehicleAI(0.0f,nullptr)==NC_STACK_ypatank::ALIGN_DONE )
                            {
                                unit->_status_flg|=BACT_STFLAG_LAND;
                                unit->_old_pos=unit->_position;
                                placementScene->ResetActorPose(unit);
                            }
                    }
                    for ( NC_STACK_ypabact *child : unit->GetKidList() )
                        visit(visit, child);
                };
                for ( NC_STACK_ypabact *child : robo->GetKidList() )
                {
                    placeFreshSquad(placeFreshSquad, child);
                }
            }

            i++;
        }
    }
}

void NC_STACK_ypaworld::InitBuddies()
{
    if ( !_levelInfo.Buddies.empty() )
    {
        int squad_sn = 0;

        std::vector<TMapBuddy> buds = _levelInfo.Buddies;
        while ( 1 )
        {
            std::vector<int> VhclIDS;
            int wrkID = -1;
            for (std::vector<TMapBuddy>::iterator it = buds.begin(); it != buds.end(); )
            {
                if (wrkID == -1 || wrkID == it->CommandID )
                {
                    wrkID = it->CommandID;
                    VhclIDS.push_back(it->Type);
                    it = buds.erase(it);
                }
                else
                    it++;
            }

            if ( wrkID == -1 )
                break;

            vec3d squadPos =    _userRobo->_position +
                                vec3d(  sin(squad_sn * 1.745) * 500.0,
                                        0.0,
                                        cos(squad_sn * 1.745) * 500.0 );

            ypaworld_arg136 arg136;
            arg136.stPos = squadPos.X0Z() + vec3d(0.5, -50000.0, 0.75);
            arg136.vect = vec3d::OY(100000.0);
            arg136.flags = 0;

            ypaworld_func136(&arg136);

            if ( arg136.isect )
                squadPos.y = arg136.isectPos.y + -100.0;

            NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(_userRobo);

            robo->MakeSquad(VhclIDS, squadPos, true); //robo 133 method

            squad_sn++;
        }
    }
}

void NC_STACK_ypaworld::yw_InitTechUpgradeBuildings()
{
    _upgradeTimeStamp = 0;
    _upgradeBuildId = 0;
    _upgradeMaxActiveVehicleId = 0;
    _upgradeVehicleId = 0;
    _upgradeWeaponId = 0;
    _upgradeId = -1;
    _gemUnlockSoundAttemptedPath.clear();

    for (size_t i = 0; i < _techUpgrades.size(); i++)
    {
        TMapGem &gem = _techUpgrades[i];
        cellArea &cell = _cells(gem.CellId);

        if (gem.BuildingID)
        {
            if ( cell.PurposeType != cellArea::PT_BUILDINGS || gem.BuildingID != cell.PurposeIndex )
            {
                ypaworld_arg148 arg148;
                arg148.owner = cell.owner;
                arg148.blg_ID = gem.BuildingID;
                arg148.CellId = gem.CellId;
                arg148.field_C = 1;
                arg148.field_18 = 0;

                ypaworld_func148(&arg148);
            }
        }

        cell.PurposeType = cellArea::PT_TECHUPGRADE;
        cell.PurposeIndex = i;
    }
}

void NC_STACK_ypaworld::InitGates()
{
    for (size_t i = 0; i < _levelInfo.Gates.size(); i++)
    {
        TMapGate &gate = _levelInfo.Gates[i];

        gate.PCell = &_cells(gate.CellId);

        ypaworld_arg148 arg148;
        arg148.owner = gate.PCell->owner;
        arg148.blg_ID = gate.ClosedBldID;
        arg148.field_C = 1;
        arg148.CellId = gate.CellId;
        arg148.field_18 = 0;

        ypaworld_func148(&arg148);

        gate.PCell->PurposeType = cellArea::PT_GATECLOSED;
        gate.PCell->PurposeIndex = i;

        for (TMapKeySector &ks : gate.KeySectors)
        {
            if ( IsGamePlaySector( ks.CellId ) )
            {
                ks.PCell = &_cells(ks.CellId);
            }
        }
    }
}

void NC_STACK_ypaworld::CellSetOwner(cellArea *cell, uint8_t owner)
{
    if ( cell->owner != owner )
    {
        uint8_t oldOwner = cell->owner;

        HistoryEventAdd( World::History::Conq(cell->CellId.x, cell->CellId.y, owner) );

        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
            HistoryEventAdd( World::History::PowerST(cell->CellId.x, cell->CellId.y, owner) );

        _countSectorsPerOwner[cell->owner]--;
        _countSectorsPerOwner[owner]++;

        cell->owner = owner;

        // Building spawners belong to the original owner of the cell.
        // When the building/sector is conquered, stop the previous spawner state
        // and do not let the new owner inherit that spawner.
        if ( cell->BuildingSpawnInitialOwner == 0 )
            cell->BuildingSpawnInitialOwner = oldOwner;

        cell->BuildingSpawnLastTime = 0;
        cell->BuildingSpawnedGids.clear();
    }
}

void NC_STACK_ypaworld::CellSetNewOwner(cellArea *cell, NC_STACK_ypabact *a5, int newOwner)
{
    int energon[World::FRACTION_MAXCOUNT];

    if ( newOwner < World::OWNER_UNKNOW )
    {
        newOwner = cell->owner;

        for( int &e : energon )
            e = 0;

        for ( NC_STACK_ypabact* &nod : cell->unitsList )
        {
            // Sector ownership is decided by living occupants.  Dead BACTs
            // remain in the cell while their recoverable plasma is visible;
            // SetStateInternal(DEAD) also gives them the legacy -10000 energy
            // sentinel.  A long death-plasma lifetime must therefore not let
            // corpses/plasma subtract energy from their faction and recapture
            // a sector long after the actual fight ended.
            if ( !nod ||
                 nod->_owner <= World::OWNER_0 ||
                 nod->_owner >= World::FRACTION_MAXCOUNT ||
                 nod->_bact_type == BACT_TYPES_MISSLE ||
                 nod->_status == BACT_STATUS_DEAD ||
                 (nod->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) ||
                 nod->_energy <= 0 )
                continue;

            energon[nod->_owner] += nod->_energy;
        }

        // Vanilla projectiles carried the attacker's faction into the hit
        // sector.  Projectiles are now excluded from the occupant totals, so
        // retain a minimal ownership vote from the authoritative damage source.
        if ( a5 &&
             a5->_owner > World::OWNER_0 &&
             a5->_owner < World::FRACTION_MAXCOUNT &&
             energon[a5->_owner] == 0 )
            energon[a5->_owner] = 1;

        energon[0] = 0;

        for (int i = 0; i < World::FRACTION_MAXCOUNT; i++)
        {
            if ( energon[i] > energon[newOwner] )
                newOwner = i;
        }
    }

    if ( cell->owner != newOwner )
    {
        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
        {
            if ( _userRobo->_owner == newOwner )
            {
                if ( a5 )
                {
                    yw_arg159 v21;
                    v21.unit = a5;
                    v21.Priority = 78;
                    v21.MsgID = 45;

                    ypaworld_func159(&v21);
                }
            }
            else if ( cell->owner == _userRobo->_owner )
            {
                yw_arg159 v24;
                v24.unit = nullptr;
                v24.Priority = 78;
                v24.MsgID = 67;

                ypaworld_func159(&v24);
            }
        }
        else
        {
            for ( const TMapGate &gate : _levelInfo.Gates )
            {
                for ( const TMapKeySector &ks : gate.KeySectors )
                {
                    if ( cell == ks.PCell )
                    {
                        if ( _userRobo->_owner == newOwner )
                        {
                            yw_arg159 v23;
                            v23.unit = nullptr;
                            v23.Priority = 80;
                            v23.MsgID = 82;

                            ypaworld_func159(&v23);
                        }
                        else if ( _userRobo->_owner == cell->owner )
                        {
                            yw_arg159 v22;
                            v22.unit = nullptr;
                            v22.Priority = 80;
                            v22.MsgID = 81;

                            ypaworld_func159(&v22);
                        }
                    }
                }
            }
        }
    }

    CellSetOwner(cell, newOwner);
}

void NC_STACK_ypaworld::CellCheckHealth(cellArea *cell, int newOwner, NC_STACK_ypabact *a6)
{
    if ( cell->IsGamePlaySector() )
    {
        int helth = 0;

        for (auto h : cell->buildings_health)
            helth += h;

        if ( cell->PurposeType == cellArea::PT_POWERSTATION )
        {
            if ( helth )
            {
                auto psIt = _powerStations.find(cell->Id);
                if (psIt != _powerStations.end())
                {
                    TPowerStationInfo &psInf = psIt->second;
                    int effPower = (helth * psInf.Power) / 255 ;

                    if ( effPower < 0 )
                        effPower = 0;
                    else if ( effPower > 255 )
                        effPower = 255;

                    psInf.EffectivePower = effPower;
                }
            }
            else
            {
                PowerStationErase(cell);
            }
        }

        /* Fix for vanilla game bug that recalc sector owner after loading
         * saved game. */
        if (newOwner != World::OWNER_NOCHANGE)
        {
            if ( cell->SectorType == 1 )
            {
                if ( helth < 224 )
                    CellSetNewOwner(cell, a6, newOwner);
            }
            else if ( helth < 1728 )
            {
                CellSetNewOwner(cell, a6, newOwner);
            }
        }
    }
    else
    {
        CellSetOwner(cell, 0);
    }
}

TSectorCollision NC_STACK_ypaworld::sub_44DBF8(int _dx, int _dz, int _dxx, int _dzz, int flags)
{
    int v8 = flags;

    TSectorCollision tmp;
    tmp.sklt = nullptr;
    tmp.Flags = 0;
    tmp.CollisionType = 0;



    if ( _dxx > 0 && _dxx < 4 * _mapSize.x - 1 && _dzz > 0  &&  _dzz < 4 * _mapSize.y - 1)
    {
        tmp.Cell = Common::Point(_dxx / 4, _dzz / 4);

        cellArea &cell = _cells(tmp.Cell);

        if ( _dxx % 4 && _dzz % 4 )
        {
            tmp.CollisionType = 1;

            int v14, v16;

            if ( cell.SectorType == 1 )
            {
                v14 = 0;
                v16 = 0;

                if ( (_dxx / 4) == (_dx / 4)  &&  (_dz / 4) == (_dzz / 4) )
                    v8 = flags & ~1;

                tmp.pos = World::SectorIDToCenterPos3(tmp.Cell);
                tmp.pos.y = cell.height;
            }
            else
            {
                v16 = (_dxx % 4) - 1;
                v14 = 2 - ((_dzz % 4) - 1);

                if ( _dxx == _dx && _dzz == _dz )
                    v8 = flags & ~1;

                tmp.pos.z = -(_dzz * 300.0);
                tmp.pos.x = _dxx * 300.0;
                tmp.pos.y = cell.height;
            }

            tmp.Flags = v8;

            int model_id = GetLegoBld(&cell, v16, v14);

            if ( v8 & 1 )
                tmp.sklt = _legoArray[model_id].UseCollisionSkelet;
            else
                tmp.sklt = _legoArray[model_id].CollisionSkelet;
        }
        else
        {
            tmp.pos.y = 0;
            tmp.pos.x = _dxx * 300.0;
            tmp.pos.z = -(_dzz * 300.0);

            if ( _dxx == _dx && _dzz == _dz )
                v8 = flags & ~1;

            tmp.Flags = v8;

            if ( _dxx % 4 == 0 && _dzz % 4 == 0)
            {
                tmp.sklt = _fillerCross;
                tmp.CollisionType = 4;
            }
            else if ( _dxx % 4 == 0 && _dzz % 4 != 0 )
            {
                tmp.sklt = _fillerSide;
                tmp.CollisionType = 2;
            }
            else if ( _dxx % 4 != 0 && _dzz % 4 == 0 )
            {
                tmp.sklt = _fillerSide;
                tmp.CollisionType = 3;
            }
        }

        if ( tmp.CollisionType && !tmp.sklt )
        {
            ypa_log_out("yw_GetSklt: WARNING, not CZT_INVALID, but Sklt NULL!\n");

            const char *v17 = "UNKNOWN";

            switch ( tmp.CollisionType )
            {
            case 4:
                v17 = "czt_cross_slurp";
                break;
            case 2:
                v17 = "czt_vside_slurp";
                break;
            case 3:
                v17 = "czt_hside_slurp";
                break;
            case 1:
                v17 = "czt_lego";
                break;
            default:
                break;
            }
            ypa_log_out("    Type=%s, sec_x=%d, sec_y=%d.\n", v17, tmp.Cell.x, tmp.Cell.y);

            tmp.CollisionType = 0;
        }
    }

    return tmp;
}

void sub_44DF60(UAskeleton::Data *arg, int id)
{
    UAskeleton::Polygon &tr = arg->polygons[id];

    int vtx1 = arg->polygons[id].v[0];
    int vtx2 = arg->polygons[id].v[1];
    int vtx3 = arg->polygons[id].v[2];

    vec3d tmp  = arg->POO[vtx2] - arg->POO[vtx1];
    vec3d tmp2 = arg->POO[vtx3] - arg->POO[vtx2];

    vec3d tmp3 = tmp * tmp2;
    tmp3.normalise();

    tr.A = tmp3.x;
    tr.B = tmp3.y;
    tr.C = tmp3.z;

    tr.D = -tmp3.dot( arg->POO[vtx1] );
}

void NC_STACK_ypaworld::sub_44E07C(TSectorCollision &arg)
{
    if ( arg.CollisionType == 2 )
    {
        cellArea &cur = _cells(arg.Cell);
        cellArea &left = _cells(arg.Cell + Common::Point(-1, 0));

        if ( !(arg.Flags & 1) || fabs( (int)(cur.height - left.height)) < 500.0 )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = left.height;
            skel->POO[1].y = cur.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = left.height;

            sub_44DF60(skel, 0);
        }
        else
        {
            arg.sklt = _colsubSkeleton;

            if ( cur.height > left.height )
                arg.pos.y = cur.height;
            else
                arg.pos.y = left.height;
        }
    }
    else if ( arg.CollisionType == 3 )
    {
        cellArea &cur = _cells(arg.Cell);
        cellArea &up = _cells(arg.Cell + Common::Point(0, -1));

        if ( !(arg.Flags & 1) || fabs( (int)(cur.height - up.height)) < 500.0 )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = up.height;
            skel->POO[1].y = up.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = cur.height;

            sub_44DF60(skel, 0);
        }
        else
        {
            arg.sklt = _colsubSkeleton;

            if ( cur.height > up.height )
                arg.pos.y = cur.height;
            else
                arg.pos.y = up.height;
        }
    }
    else if ( arg.CollisionType == 4 )
    {

        int kk = 0;

        cellArea &cur = _cells(arg.Cell);
        cellArea &left = _cells(arg.Cell + Common::Point(-1, 0));
        cellArea &up = _cells(arg.Cell + Common::Point(0, -1));
        cellArea &leftup = _cells(arg.Cell + Common::Point(-1, -1));

        if ( arg.Flags & 1 )
        {
            float cs = cur.height;
            float ls = left.height;
            float us = up.height;
            float lus = leftup.height;

            float v15, v16, v17, v18;

            if ( cs >= ls )
                v15 = ls;
            else
                v15 = cs;

            if ( us >= lus )
                v16 = lus;
            else
                v16 = us;

            if ( v15 < v16 )
                v16 = v15;

            if ( cs <= ls )
                v17 = ls;
            else
                v17 = cs;

            if ( us <= lus )
                v18 = lus;
            else
                v18 = us;

            if ( v17 > v18 )
                v18 = v17;

            if ( fabs( (int)(v18 - v16) ) > 300.0 )
            {
                arg.sklt = _colsubSkeleton;
                arg.pos.y = v18;
                kk = 1;
            }
        }
        if ( !kk )
        {
            UAskeleton::Data *skel = arg.sklt->GetSkelet();
            skel->POO[0].y = leftup.height;
            skel->POO[1].y = up.height;
            skel->POO[2].y = cur.height;
            skel->POO[3].y = left.height;
            skel->POO[4].y = cur.averg_height;

            sub_44DF60(skel, 0);
            sub_44DF60(skel, 1);
            sub_44DF60(skel, 2);
            sub_44DF60(skel, 3);
        }
    }
}

int sub_44D36C(const vec3d &v, int id, NC_STACK_skeleton *skeleton)
{
    UAskeleton::Data *sklt = skeleton->GetSkelet();

    int v7 = 0;

    const UAskeleton::Polygon &tr = sklt->polygons[id];
    float nX = fabs(tr.A);
    float nY = fabs(tr.B);
    float nZ = fabs(tr.C);

    float maxAx = (nX <= nY ? nY : nX );
    maxAx = (maxAx <= nZ ? nZ : maxAx);

    if ( maxAx == nX )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.z <= v.z && v.z < cur.z) ||
                    (cur.z <= v.z && v.z < prv.z) ) &&
                    prv.y + (cur.y - prv.y) * (v.z - prv.z) / (cur.z - prv.z) > v.y )
            {
                v7 = v7 == 0;
            }

            prev = i;
        }
    }
    else if ( maxAx == nY )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.z <= v.z && v.z < cur.z) ||
                    (cur.z <= v.z && v.z < prv.z) ) &&
                    prv.x + (cur.x - prv.x) * (v.z - prv.z) / (cur.z - prv.z) > v.x )
            {
                v7 = v7 == 0;
            }

            prev = i;
        }
    }
    else if ( maxAx == nZ )
    {
        int prev = sklt->polygons[id].num_vertices - 1;

        for (int i = 0; i < sklt->polygons[id].num_vertices; i++)
        {
            const UAskeleton::Vertex &cur = sklt->POO[ sklt->polygons[id].v[i] ];
            const UAskeleton::Vertex &prv = sklt->POO[ sklt->polygons[id].v[prev] ];

            if ( ( (prv.y <= v.y && v.y < cur.y) ||
                    (cur.y <= v.y && v.y < prv.y) ) &&
                    prv.x + (cur.x - prv.x) * (v.y - prv.y) / (cur.y - prv.y) > v.x )
            {
                v7 = v7 == 0;
            }

            prev = i;
        }
    }
    return v7;
}

void NC_STACK_ypaworld::sub_44D8B8(ypaworld_arg136 *arg, const TSectorCollision &loc)
{
    UAskeleton::Data *skel = loc.sklt->GetSkelet();
    for ( size_t i = 0; i < skel->polygons.size(); i++)
    {
        UAskeleton::Polygon &tr = skel->polygons[i];
        vec3d norm = tr.Normal();

        float v11 = norm.dot(arg->vect);
        if ( v11 > 0.0 )
        {
            float v19 = -(norm.dot( arg->stPos ) + tr.D) / v11;
            if ( v19 > 0.0 && v19 <= 1.0 && v19 < arg->tVal )
            {
                vec3d px = arg->vect * v19 + arg->stPos;

                if ( sub_44D36C(px, i, loc.sklt) )
                {
                    arg->isect = 1;
                    arg->tVal = v19;
                    arg->isectPos = loc.pos + px;
                    arg->polyID = i;
                    arg->skel = loc.sklt->GetSkelet();
                    arg->hitCell = loc.Cell;
                    arg->hitCollisionType = loc.CollisionType;
                    arg->hitSkelPos = loc.pos;
                }
            }
        }
    }
}

void NC_STACK_ypaworld::FFeedback_VehicleChanged()
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }

    if ( _userUnit )
    {
        _ffTimeStamp = _timeStamp;

        Input::Engine.ForceFeedback(Input::FF_STATE_STOP, Input::FF_TYPE_ALL);

        int effectType;
        float v13;
        float v14;
        float v15;
        float v16;
        float v17;
        float v18;
        float v19;
        float v22;
        float v24;
        float v25;


        switch ( _userUnit->_bact_type )
        {
        case BACT_TYPES_BACT:
            effectType = Input::FF_TYPE_HELIENGINE;
            v16 = 300.0;
            v13 = 800.0;
            v15 = 1.0;
            v14 = 2.0;
            v25 = 1.0;
            v18 = 0.0;
            v17 = 0.7;
            v24 = 0.3;
            v22 = 1.0;
            v19 = 0.5;
            break;

        case BACT_TYPES_TANK:
        case BACT_TYPES_CAR:
            effectType = Input::FF_TYPE_TANKENGINE;
            v16 = 200.0;
            v13 = 500.0;
            v15 = 0.6;
            v14 = 1.0;
            v25 = 1.0;
            v22 = 1.0;
            v17 = 0.1;
            v24 = 0.3;
            v19 = 0.4;
            v18 = 0.0;
            break;

        case BACT_TYPES_FLYER:
            effectType = Input::FF_TYPE_JETENGINE;
            v16 = 200.0;
            v13 = 500.0;
            v15 = 1.0;
            v14 = 2.0;
            v25 = 1.0;
            v18 = 0.0;
            v17 = 0.1;
            v24 = 0.3;
            v22 = 1.0;
            v19 = 0.75;
            break;

        default:
            effectType = -1;
            break;
        }

        if ( effectType != -1 )
        {
            float v4 = (_userUnit->_mass - v16) / (v13 - v16);
            float v5 = (_userUnit->_maxrot - v15) / (v14 - v15);

            float v21 = (v19 - v24) * v4 + v24;
            float v23 = (v18 - v22) * v4 + v22;

            float v20 = (v17 - v25) * v5 + v25;

            if ( v21 < v24)
                v21 = v24;
            else if (v21 > v19)
                v21 = v19;

            if ( v23 < v18 )
                v23 = v18;
            else if ( v23 > v22)
                v23 = v22;

            if ( v20 < v17)
                v20 = v17;
            else if ( v20 > v25)
                v20 = v25;

            _ffEffectType = effectType;
            _ffPeriod = v23;
            _ffMagnitude = v21;

            Input::Engine.ForceFeedback(Input::FF_STATE_START, effectType, v21, v23);
            Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_ROTDAMPER, v20);
        }
    }
}

NC_STACK_ypabact *NC_STACK_ypaworld::yw_createUnit( int model_id)
{
    std::array<const std::string, 10> unit_classes_names
    {
        "dummy.class",      // 0
        "ypabact.class",    // 1
        "ypatank.class",    // 2
        "yparobo.class",    // 3
        "ypamissile.class", // 4
        "ypazepp.class",    // 5
        "ypaflyer.class",   // 6
        "ypaufo.class",     // 7
        "ypacar.class",     // 8
        "ypagun.class"      // 9
    };

    if ( model_id < 0 || (size_t)model_id >= unit_classes_names.size() )
        return nullptr;

    NC_STACK_ypabact *bacto = nullptr;

    // Find dead units
    for ( NC_STACK_ypabact * &unit : _deadCacheList )
    {
        if (unit->_bact_type == model_id)
        {
            bacto = unit;
            break;
        }
    }

    if ( !bacto )
    {
        bacto = Nucleus::CTFInit<NC_STACK_ypabact>(unit_classes_names[model_id],
            {{NC_STACK_ypabact::BACT_ATT_WORLD, this}} );

        if ( !bacto )
            return nullptr;
    }

    bacto->Renew(); // Reset bact

    bacto->_gid = bact_id;
    bacto->_owner = 0;

    bacto->_rotation = mat3x3::Ident();

    bact_id++;

    return bacto;
}

NC_STACK_ypabact *yw_FindLiveMissileByGidInList(World::MissileList &list, int32_t gid)
{
    for (NC_STACK_ypamissile *missile : list)
    {
        if ( missile->_gid == gid )
        {
            if ( missile->_kidRef.IsListType(World::BLIST_CACHE) || missile->_status == BACT_STATUS_DEAD )
                return nullptr;

            return missile;
        }

        NC_STACK_ypabact *kid = yw_FindLiveBactByGidInList(missile->_kidList, gid);
        if ( kid )
            return kid;

        NC_STACK_ypabact *childMissile = yw_FindLiveMissileByGidInList(missile->_missiles_list, gid);
        if ( childMissile )
            return childMissile;
    }

    return nullptr;
}

NC_STACK_ypabact *yw_FindLiveBactByGidInList(World::RefBactList &list, int32_t gid)
{
    for (NC_STACK_ypabact *unit : list)
    {
        if ( unit->_gid == gid )
        {
            if ( unit->_kidRef.IsListType(World::BLIST_CACHE) || unit->_status == BACT_STATUS_DEAD )
                return nullptr;

            return unit;
        }

        NC_STACK_ypabact *kid = yw_FindLiveBactByGidInList(unit->_kidList, gid);
        if ( kid )
            return kid;

        NC_STACK_ypabact *missile = yw_FindLiveMissileByGidInList(unit->_missiles_list, gid);
        if ( missile )
            return missile;
    }

    return nullptr;
}

NC_STACK_ypabact *NC_STACK_ypaworld::FindLiveBactByGid(int32_t gid)
{
    if ( gid <= 0 )
        return nullptr;

    return yw_FindLiveBactByGidInList(_unitsList, gid);
}

void NC_STACK_ypaworld::sb_0x456384(const Common::Point &cellId, int ownerid2, int blg_id, int a7)
{
    uamessage_bldVhcl bvMsg;

    cellArea &cell = _cells(cellId);
    World::TBuildingProto *bld = &_buildProtos[ blg_id ];
    TSectorDesc *sectp = &_secTypeArray[ bld->SecType ];

    int v43 = 1;

    NC_STACK_yparobo *robo = nullptr;

    if ( cell.IsGamePlaySector() )
    {
        _lvlBuildingsMap(cellId) = blg_id;
        _lvlTypeMap(cellId) = bld->SecType;

        cell.type_id = bld->SecType;
        cell.energy_power = 0;
        cell.PurposeType = cellArea::PT_BUILDINGS;
        cell.SectorType = sectp->SectorType;
        cell.PurposeIndex = blg_id;
        cell.BuildingTint = bld->tint;
        cell.DecorationFX = bld->DecorationFX;
        cell.DecorationFXNextTime = 0;
        cell.DecorationFXPersistentId = 0;

        int v49;

        if ( sectp->SectorType == 1 )
        {
            cell.buildings_health.fill(0);

            v49 = 1;
        }
        else
        {
            v49 = 3;
        }

        for (int yy = 0; yy < v49; yy++)
        {
            for (int xx = 0; xx < v49; xx++)
                cell.buildings_health.At(xx, yy) = sectp->SubSectors.At(xx, yy)->StartHealth;
        }

        if ( bld->ModelID == 1 )
            SetupPowerStationInfo(&cell, bld->Power, blg_id);

        CellSetOwner(&cell, ownerid2);

        // Building spawner runtime ownership must be tied to the building that
        // has just been created, not to a previous owner stored when the sector
        // was conquered before construction.
        if ( bld->spawn_units )
        {
            cell.BuildingSpawnInitialOwner = ownerid2;
            cell.BuildingSpawnLastTime = 0;
            cell.BuildingSpawnedGids.clear();
        }
        else
        {
            cell.BuildingSpawnInitialOwner = 0;
            cell.BuildingSpawnLastTime = 0;
            cell.BuildingSpawnedGids.clear();
        }

        for( NC_STACK_ypabact * &unit: _unitsList )
        {
            if (unit->_bact_type == BACT_TYPES_ROBO && unit->_owner == ownerid2)
            {
                robo = dynamic_cast<NC_STACK_yparobo *>(unit);
                break;
            }
        }

        if ( _isNetGame )
        {
            if ( robo != _userRobo )
                v43 = 0;
        }

        if ( !a7 )
        {
            if ( robo && robo->_status != BACT_STATUS_DEAD && v43 )
            {
                NC_STACK_ypagun *commander = nullptr;

                int v39 = robo->getROBO_commCount();

                v39++;

                robo->setROBO_commCount(v39);

                for ( size_t i = 0; i < bld->Guns.size(); i++)
                {
                    World::TBuildingProto::TGun &GunProto = bld->Guns[i];

                    if ( !GunProto.VhclID )
                        break;

                    ypaworld_arg146 v33;
                    v33.vehicle_id = GunProto.VhclID;
                    v33.pos = GunProto.Pos + World::SectorIDToCenterPos3( cellId );

                    NC_STACK_ypabact *gun_obj = ypaworld_func146(&v33);
                    NC_STACK_ypagun *gunn = dynamic_cast<NC_STACK_ypagun *>(gun_obj);

                    if ( gun_obj )
                    {
                        gun_obj->_owner = ownerid2;

                        if (gunn)
                            gunn->ypagun_func128(GunProto.Dir, false);

                        setState_msg v34;
                        v34.newStatus = BACT_STATUS_CREATE;
                        v34.unsetFlags = 0;
                        v34.setFlags = 0;

                        gunn->SetStateInternal(&v34);

                        gun_obj->_scale_time = 500;
                        gun_obj->_scale = vec3d(1.0, 1.0, 1.0);

                        gun_obj->_host_station = robo;
                        gun_obj->_commandID = v39;

                        // A newly constructed building's gameplay gun/module
                        // components are children of the owner Host Station too.
                        // Keep debuff inheritance consistent with normal vehicle
                        // production so post-infection buildings cannot spawn
                        // clean attached actors.
                        gun_obj->InheritActiveDebuffFromParent(robo);

                        if ( _isNetGame && i < 8 )
                        {
                            gun_obj->_gid |= ownerid2 << 24;

                            bvMsg.vhcl[i].id = gun_obj->_gid;
                            bvMsg.vhcl[i].base = GunProto.Dir;
                            bvMsg.vhcl[i].pos = gun_obj->_position;
                            bvMsg.vhcl[i].protoID = GunProto.VhclID;
                        }

                        if ( commander )
                        {
                            commander->AddSubject(gunn);
                        }
                        else
                        {
                            commander = gunn;

                            robo->AddSubject(gunn);
                        }
                    }
                }

                if ( _isNetGame )
                {
                    bvMsg.msgID = UAMSG_BUILDINGVHCL;
                    bvMsg.tstamp = _timeStamp;
                    bvMsg.owner = ownerid2;

                    NetBroadcastMessage(&bvMsg, sizeof(bvMsg), true);
                }
            }
        }
    }
}

bool yw_IsBuildingTerrainCollisionPolygon(const UAskeleton::Data *skel,
                                                  const UAskeleton::Polygon &poly,
                                                  float terrainHalfExtent)
{
    if ( !skel || poly.num_vertices < 3 )
        return false;

    float minX =  1.0e30f;
    float maxX = -1.0e30f;
    float minZ =  1.0e30f;
    float maxZ = -1.0e30f;
    float maxAbsY = 0.0f;

    for (int i = 0; i < poly.num_vertices; ++i)
    {
        const int vertexID = poly.v[i];
        if ( vertexID < 0 || (size_t)vertexID >= skel->POO.size() )
            return false;

        const UAskeleton::Vertex &v = skel->POO[vertexID];
        minX = std::min(minX, (float)v.x);
        maxX = std::max(maxX, (float)v.x);
        minZ = std::min(minZ, (float)v.z);
        maxZ = std::max(maxZ, (float)v.z);
        maxAbsY = std::max(maxAbsY, std::fabs((float)v.y));
    }

    // Vanilla LEGO collision skeletons use one full-size, flat Y=0 polygon for
    // the sector surface (300x300 for a 3x3 LEGO slot, 900x900 for compact
    // single-LEGO sectors). Exclude exactly that polygon from the building
    // volume; foundations/ramps that merely touch Y=0 remain building geometry.
    constexpr float kGroundEpsilon = 0.5f;
    return maxAbsY <= kGroundEpsilon &&
           minX <= -terrainHalfExtent + kGroundEpsilon &&
           maxX >=  terrainHalfExtent - kGroundEpsilon &&
           minZ <= -terrainHalfExtent + kGroundEpsilon &&
           maxZ >=  terrainHalfExtent - kGroundEpsilon;
}

bool yw_PointCoveredByBuildingCollision(NC_STACK_skeleton *collision,
                                             const vec3d &localPoint,
                                             float terrainHalfExtent)
{
    if ( !collision )
        return false;

    UAskeleton::Data *skel = collision->GetSkelet();
    if ( !skel || skel->polygons.empty() )
        return false;

    bool hasBuildingGeometry = false;
    float minX =  1.0e30f;
    float maxX = -1.0e30f;
    float minY =  1.0e30f;
    float maxY = -1.0e30f;
    float minZ =  1.0e30f;
    float maxZ = -1.0e30f;

    for (const UAskeleton::Polygon &poly : skel->polygons)
    {
        if ( yw_IsBuildingTerrainCollisionPolygon(skel, poly, terrainHalfExtent) )
            continue;

        hasBuildingGeometry = true;
        for (int i = 0; i < poly.num_vertices; ++i)
        {
            const int vertexID = poly.v[i];
            if ( vertexID < 0 || (size_t)vertexID >= skel->POO.size() )
                continue;

            const UAskeleton::Vertex &v = skel->POO[vertexID];
            minX = std::min(minX, (float)v.x);
            maxX = std::max(maxX, (float)v.x);
            minY = std::min(minY, (float)v.y);
            maxY = std::max(maxY, (float)v.y);
            minZ = std::min(minZ, (float)v.z);
            maxZ = std::max(maxZ, (float)v.z);
        }
    }

    if ( !hasBuildingGeometry )
        return false;

    constexpr float kBoundsEpsilon = 0.25f;
    if ( localPoint.x < minX - kBoundsEpsilon || localPoint.x > maxX + kBoundsEpsilon ||
         localPoint.y < minY - kBoundsEpsilon || localPoint.y > maxY + kBoundsEpsilon ||
         localPoint.z < minZ - kBoundsEpsilon || localPoint.z > maxZ + kBoundsEpsilon )
        return false;

    // Building collision skeletons are not guaranteed to be closed solids.
    // The Stoudson/Superitem pyramid is a concrete vanilla example: a unit on
    // the ground in its central footprint has no horizontal +X wall crossing,
    // even though several roof tiers are directly above it.  Therefore the
    // primary test is "is authored building collision geometry overhead at
    // this X/Z and above this point?".  UA uses negative Y as up, so a smaller
    // local Y is above the unit.  Aircraft above the roof naturally fail this
    // test, and points outside the footprint have no polygon hit.
    vec3d verticalPoint = localPoint;
    verticalPoint.x += 0.03125f;
    verticalPoint.z += 0.0625f;

    constexpr float kPlaneEpsilon = 0.00001f;
    constexpr float kSurfaceEpsilon = 0.25f;

    for (size_t i = 0; i < skel->polygons.size(); ++i)
    {
        const UAskeleton::Polygon &poly = skel->polygons[i];
        if ( yw_IsBuildingTerrainCollisionPolygon(skel, poly, terrainHalfExtent) ||
             std::fabs(poly.B) <= kPlaneEpsilon )
            continue;

        const float y = -(poly.A * verticalPoint.x + poly.C * verticalPoint.z + poly.D) / poly.B;
        if ( y > verticalPoint.y + kSurfaceEpsilon )
            continue;

        const vec3d hitPoint(verticalPoint.x, y, verticalPoint.z);
        if ( sub_44D36C(hitPoint, (int)i, collision) )
            return true;
    }

    // Fallback for authored structures that have enclosing walls but no usable
    // roof/overhead polygon.  Closed collision shells still get the legacy
    // parity test, while the vertical test above covers open/non-manifold LEGO
    // skeletons such as the Superitem pyramid.
    vec3d rayPoint = localPoint;
    rayPoint.y += 0.03125f;
    rayPoint.z += 0.0625f;

    std::vector<float> hitX;
    hitX.reserve(8);

    constexpr float kDuplicateHitEpsilon = 0.05f;

    for (size_t i = 0; i < skel->polygons.size(); ++i)
    {
        const UAskeleton::Polygon &poly = skel->polygons[i];
        if ( yw_IsBuildingTerrainCollisionPolygon(skel, poly, terrainHalfExtent) ||
             std::fabs(poly.A) <= kPlaneEpsilon )
            continue;

        const float x = -(poly.B * rayPoint.y + poly.C * rayPoint.z + poly.D) / poly.A;
        if ( x <= rayPoint.x + kPlaneEpsilon )
            continue;

        const vec3d hitPoint(x, rayPoint.y, rayPoint.z);
        if ( !sub_44D36C(hitPoint, (int)i, collision) )
            continue;

        bool duplicate = false;
        for (float oldX : hitX)
        {
            if ( std::fabs(oldX - x) <= kDuplicateHitEpsilon )
            {
                duplicate = true;
                break;
            }
        }

        if ( !duplicate )
            hitX.push_back(x);
    }

    return (hitX.size() & 1u) != 0;
}

void NC_STACK_ypaworld::DestroyUnitsCoveredByBuilding(cellArea *cell, uint8_t buildingID)
{
    if ( !cell || buildingID >= _buildProtos.size() )
        return;

    const World::TBuildingProto &proto = _buildProtos[buildingID];
    if ( (size_t)proto.SecType >= _secTypeArray.size() )
        return;

    TSectorDesc &sector = _secTypeArray[proto.SecType];
    const int slotCount = sector.SectorType == 1 ? 1 : 3;
    const int slotOffset = sector.SectorType == 1 ? 0 : -1;
    const float terrainHalfExtent = sector.SectorType == 1 ? 450.0f : 150.0f;

    auto pointCovered = [&](const vec3d &worldPoint) -> bool
    {
        for (int y = 0; y < slotCount; ++y)
        {
            for (int x = 0; x < slotCount; ++x)
            {
                TSubSectorDesc *subSector = sector.SubSectors.At(x, y);
                if ( !subSector )
                    continue;

                const int legoID = subSector->HPModels[0];
                if ( legoID < 0 || (size_t)legoID >= _legoArray.size() )
                    continue;

                NC_STACK_skeleton *collision = _legoArray[legoID].CollisionSkelet;
                if ( !collision )
                    continue;

                vec3d legoPos = cell->CenterPos +
                        vec3d((slotOffset + x) * 300.0f, 0.0f,
                              (slotOffset + y) * 300.0f);
                legoPos.y = cell->height;

                if ( yw_PointCoveredByBuildingCollision(collision,
                                                     worldPoint - legoPos,
                                                     terrainHalfExtent) )
                    return true;
            }
        }

        return false;
    };

    // Snapshot/safe iterator is required because ModifyEnergy may immediately
    // remove a killed actor from the sector list.
    for (NC_STACK_ypabact *unit : cell->unitsList.safe_iter())
    {
        if ( !unit || unit->_status == BACT_STATUS_DEAD ||
             (unit->_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) ||
             unit->_bact_type == BACT_TYPES_MISSLE )
            continue;

        bool covered = pointCovered(unit->_position);

        if ( !covered )
        {
            std::vector<NC_STACK_ypabact::TCollisionSphereWorld> spheres;
            unit->GetCollisionSpheres(spheres, unit->_position, unit->_rotation, false);

            for (const NC_STACK_ypabact::TCollisionSphereWorld &sphere : spheres)
            {
                // Test authored collision-node centers, not their broad radius.
                // This catches large/compound units actually entering the future
                // building while avoiding helicopters or vehicles merely near it.
                if ( pointCovered(sphere.center) )
                {
                    covered = true;
                    break;
                }
            }
        }

        if ( !covered )
            continue;

        bact_arg84 kill;
        kill.energy = -22000000;
        kill.unit = nullptr;
        unit->ModifyEnergy(&kill);
    }
}

void NC_STACK_ypaworld::DestroyAllGunsInSector(cellArea *cell)
{
    /*
     * Destroy all GUN units in sector
     */

    // Safe iterator, because it will call ModifyEnergy->Die for units
    for ( NC_STACK_ypabact* itUnit : cell->unitsList.safe_iter() )
    {
        int v5 = 0;

        if ( _isNetGame )
        {
            // In netgame only destroy own units
            if ( itUnit->_owner == _userUnit->_owner )
            {
                if ( itUnit->_status != BACT_STATUS_DEAD && itUnit->_status != BACT_STATUS_BEAM && itUnit->_status != BACT_STATUS_CREATE )
                {
                    if ( itUnit->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( itUnit );

                        if (!gun->IsRoboGun())
                            v5 = 1;
                    }
                }
            }
        }
        else
        {
            if ( itUnit->_status != BACT_STATUS_DEAD && itUnit->_status != BACT_STATUS_BEAM && itUnit->_status != BACT_STATUS_CREATE )
            {
                if ( itUnit->_bact_type == BACT_TYPES_GUN )
                {
                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( itUnit );

                    if (!gun->IsRoboGun())
                        v5 = 1;
                }
            }
        }

        if ( v5 )
        {
            bact_arg84 v8;
            v8.energy = -22000000;
            v8.unit = nullptr;

            itUnit->ModifyEnergy(&v8);
        }
    }
}

bool NC_STACK_ypaworld::BuildingConstructBegin(cellArea *cell, uint8_t buildID, int owner, int cTime)
{
    if (!cell)
        return false;

    if (cell->IsBorder())
        return false;

    auto it = _inBuildProcess.find(cell->Id);
    if (it != _inBuildProcess.end())
        return false;

    TConstructInfo &bldProc = _inBuildProcess[cell->Id];
    bldProc.CellID = cell->CellId;
    bldProc.Time = 0;
    bldProc.EndTime = cTime;
    bldProc.BuildID = buildID;
    bldProc.Owner = owner;

    cell->PurposeType = cellArea::PT_CONSTRUCTING;

    for (NC_STACK_ypabact * &unit : _unitsList)
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && owner == unit->_owner )
        {
            SFXEngine::SFXe.startSound(&unit->_soundcarrier, 11);
            break;
        }
    }

    return true;
}

int ypaworld_func137__sub0__sub0(UAskeleton::Data *skl, int id, const vec3d &v, float r, vec3d &out)
{
    UAskeleton::Polygon &pg = skl->polygons[id];
    vec3d tmp(0.0, 0.0, 0.0);

    for (int i = 0; i < pg.num_vertices; i++)
    {
        int16_t idd = pg.v[i];
        tmp += static_cast<vec3d> (skl->POO[ idd ]);
    }

    vec3d tmp2 = tmp / pg.num_vertices - v;

    float v26 = tmp2.length();

    if ( v26 <= r )
        return 0;

    out = tmp2 * (r / v26) + v;

    return 1;
}

void NC_STACK_ypaworld::ypaworld_func137__sub0(ypaworld_arg137 *arg, const TSectorCollision &a2)
{
    for (size_t i = 0; i < a2.sklt->GetSkelet()->polygons.size(); i++)
    {
        const UAskeleton::Polygon &tria = a2.sklt->GetSkelet()->polygons[i];

        vec3d t0 = tria.Normal();

        float v9 = t0.dot( arg->pos2 );

        if ( v9 > 0.0 )
        {
            float v26 = -( t0.dot( arg->pos ) + tria.D) / ( t0.dot( t0 ) * arg->radius);

            if ( v26 > 0.0 && v26 <= 1.0 )
            {
                vec3d tx = arg->pos + t0 * (arg->radius * v26);

                int v27 = 0;

                vec3d v18;

                if ( ypaworld_func137__sub0__sub0(a2.sklt->GetSkelet(), i, tx, arg->radius, v18) )
                {
                    if ( sub_44D36C(v18, i, a2.sklt) )
                        v27 = 1;
                }
                else
                    v27 = 1;

                if ( v27 )
                {
                    if ( arg->coll_count < arg->coll_max )
                    {
                        int pos = arg->coll_count;

                        arg->collisions[pos].pos1 = a2.pos + tx;
                        arg->collisions[pos].pos2 = tria.Normal();

                        arg->coll_count++;
                    }
                }
            }
        }
    }
}

NC_STACK_ypabact * NC_STACK_ypaworld::FindBactByCmdOwn(uint32_t commandID, char owner)
{
    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        if ( robo->_bact_type == BACT_TYPES_ROBO && robo->_owner == owner)
        {
            if ( robo->_commandID == commandID )
            {
                if ( robo->_status == BACT_STATUS_DEAD )
                    return nullptr;
                else
                    return robo;
            }
            else
            {
                for ( NC_STACK_ypabact * &unit : robo->_kidList )
                {
                    if ( unit->_commandID == commandID )
                    {
                        if ( unit->_status == BACT_STATUS_DEAD )
                            return nullptr;
                        else
                            return unit;
                    }
                }
            }
        }
    }

    return nullptr;
}

void NC_STACK_ypaworld::BuildingConstructUpdate(int dtime)
{
    for(auto it  = _inBuildProcess.begin(); it != _inBuildProcess.end(); )
    {
        TConstructInfo &bldProc = it->second;
        bldProc.Time += dtime;

        if ( bldProc.Time >= bldProc.EndTime )
        {
            cellArea &rCell = _cells( bldProc.CellID );
            rCell.PurposeType = cellArea::PT_NONE;
            rCell.PurposeIndex = 0;

            // Resolve only actors that would actually be enclosed by the new
            // building collision volume. Do this before sb_0x456384 creates
            // attached building guns/modules so those new actors are untouched.
            DestroyUnitsCoveredByBuilding(&rCell, bldProc.BuildID);

            sb_0x456384(bldProc.CellID, bldProc.Owner, bldProc.BuildID, 0);

            if ( bldProc.Owner == _userRobo->_owner )
            {
                if ( _buildProtos[ bldProc.BuildID ].ModelID )
                {
                    yw_arg159 arg159;

                    arg159.unit = _userRobo;
                    arg159.Priority = 65;

                    switch( _buildProtos[ bldProc.BuildID ].ModelID )
                    {
                        case 1:
                        arg159.MsgID = 36;
                        break;

                        case 2:
                        arg159.MsgID = 38;
                        break;

                        case 3:
                        arg159.MsgID = 37;
                        break;

                        default:
                        arg159.MsgID = 0;
                        break;
                    }

                    ypaworld_func159(&arg159);
                }
            }
            it = _inBuildProcess.erase(it);
        }
        else
            ++it;
    }
}

bool NC_STACK_ypaworld::IsAnyBuildingProcess(int owner) const
{
    for(const auto &it : _inBuildProcess)
    {
        if (it.second.Owner == owner)
            return true;
    }
    return false;
}

void NC_STACK_ypaworld::ypaworld_func64__sub6__sub0()
{
    for(int i = 0; i < 8; i++)
        _countUnitsPerOwner[i] = 0;

    for ( NC_STACK_ypabact * &robo : _unitsList )
    {
        if ( robo->_bact_type == BACT_TYPES_ROBO && robo->_status != BACT_STATUS_DEAD && robo->_status != BACT_STATUS_BEAM )
        {
            for( NC_STACK_ypabact * &comnd : robo->_kidList )
            {
                if ( comnd->_status != BACT_STATUS_DEAD && comnd->_status != BACT_STATUS_BEAM )
                {
                    if ( IsSpectatorBact(comnd) )
                        continue;

                    bool a4 = false;

                    if ( comnd->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( comnd );
                        a4 = gun->IsRoboGun();
                    }

                    if ( !a4 )
                    {
                        _countUnitsPerOwner[ comnd->_owner ]++;

                        for( NC_STACK_ypabact * &unit : comnd->_kidList )
                        {
                            if ( unit->_status != BACT_STATUS_DEAD && unit->_status != BACT_STATUS_BEAM && !IsSpectatorBact(unit) )
                                _countUnitsPerOwner[ unit->_owner ]++;
                        }
                    }
                }
            }
        }
    }
}

void ypaworld_func64__sub6(NC_STACK_ypaworld *yw)
{
    int v13[8];

    for (int i = 0; i < 8; i++)
        v13[i] = 0;

    for (const auto &ps : yw->_powerStations)
    {
        if (ps.second.pCell)
            v13[ ps.second.pCell->owner ] += ps.second.EffectivePower;
    }

    yw->ypaworld_func64__sub6__sub0();

    for (int i = 0; i < 8; i++)
    {
        v13[i] /= 2;

        if ( v13[i] <= 0 )
        {
            yw->_reloadRatioPositive[i] = 0;
            yw->_reloadRatioClamped[i] = 0;
        }
        else
        {
            int v15 = yw->_countSectorsPerOwner[i];

            if ( v15 < 0 )
                v15 = 0;

            yw->_reloadRatioClamped[i] = (float)v15 / (float)v13[i];
            yw->_reloadRatioPositive[i] = (float)v15 / (float)v13[i];

            if ( yw->_isNetGame )
            {
                if ( yw->_levelUnitLimitType == 1 )
                {
                    int v16 = yw->_countUnitsPerOwner[yw->_userRobo->_owner] - yw->_levelUnitLimit;

                    if ( v16 > 0 )
                    {
                        int v10 = (float)yw->_levelUnitLimitArg * 0.01 * (float)v16;

                        yw->_reloadRatioClamped[i] -= v10;
                        yw->_reloadRatioPositive[i] -= v10;
                    }
                }
            }

            if ( yw->_reloadRatioClamped[i] > 1.0 )
                yw->_reloadRatioClamped[i] = 1.0;
            else if ( yw->_reloadRatioClamped[i] < 0.0 )
                yw->_reloadRatioClamped[i] = 0;

            if ( yw->_reloadRatioPositive[i] < 0.0 )
                yw->_reloadRatioPositive[i] = 0;
        }
    }
}

void NC_STACK_ypaworld::RecalcSectorsPowerForPS(const TPowerStationInfo &ps)
{
    int pwrTmp = ps.EffectivePower;
    int powsCount = 0;

    while (pwrTmp > 0)
    {
        pwrTmp >>= 1;
        powsCount++;
    }

    int sdx = -powsCount;
    int edx = powsCount + 1;

    int sdy = -powsCount;
    int edy = powsCount + 1;

    if ( ps.CellId.x + sdx < 1 )
        sdx = 1 - ps.CellId.x;

    if ( ps.CellId.y + sdy < 1 )
        sdy = 1 - ps.CellId.y;

    if ( ps.CellId.x + edx >= _mapSize.x )
        edx = _mapSize.x - ps.CellId.x - 1;

    if ( ps.CellId.y + edy >= _mapSize.y )
        edy = _mapSize.y - ps.CellId.y - 1;

    for (int dy = sdy; dy < edy; dy++)
    {
        for (int dx = sdx; dx < edx; dx++)
        {
            int v17 = ps.EffectivePower  >>  _sqrtTable(abs(dx), abs(dy));

            EnergyAccum &accum = _energyAccumMap(dx + ps.CellId.x, dy + ps.CellId.y);

            if ( accum.Owner == ps.pCell->owner )
            {
                accum.Energy += v17; // Add power to this cell

                if ( accum.Energy > 255 )
                    accum.Energy = 255;
            }

        }
    }
}

void NC_STACK_ypaworld::sub_4D12D8(int id, int a3, bool restoring)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.State = TMapSuperItem::STATE_ACTIVE;
    sitem.TriggerTime = 0;
    sitem.ActivateOwner = sitem.PCell->owner;

    if ( !a3 )
    {
        sitem.ActiveTime = _timeStamp;
        sitem.LastTenSec = 0;
        sitem.LastSec = 0;
        sitem.CountDown = sitem.TimerValue;
    }

    ypaworld_arg148 arg148;
    arg148.owner = sitem.PCell->owner;
    arg148.blg_ID = sitem.ActiveBldID;
    arg148.field_C = 1;
    arg148.CellId = sitem.CellId;
    arg148.field_18 = 0;

    ypaworld_func148(&arg148);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    // Loading restores the building state before player control exists.
    // It must not replay an activation announcement.
    if ( restoring )
        return;

    yw_arg159 arg159;
    arg159.unit = 0;
    arg159.Priority = 94;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBACT);
        arg159.MsgID = 70;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEACT);
        arg159.MsgID = 74;
    }
    else
    {
        arg159.MsgID = 0;
        arg159.txt = Locale::Text::OpenUA(Locale::OUA_INTERNAL_CANNOT_HAPPEN);
    }

    ypaworld_func159(&arg159);
}

void NC_STACK_ypaworld::sub_4D1594(int id, bool restoring)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.State = TMapSuperItem::STATE_STOPPED;

    ypaworld_arg148 arg148;
    arg148.owner = sitem.PCell->owner;
    arg148.blg_ID = sitem.InactiveBldID;
    arg148.field_C = 1;
    arg148.CellId = sitem.CellId;
    arg148.field_18 = 0;

    ypaworld_func148(&arg148);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    if ( restoring )
        return;

    yw_arg159 arg159;
    arg159.unit = 0;
    arg159.Priority = 93;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBFROZ);
        arg159.MsgID = 72;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEFROZ);
        arg159.MsgID = 76;
    }
    else
    {
        arg159.MsgID = 0;
        arg159.txt = Locale::Text::OpenUA(Locale::OUA_INTERNAL_CANNOT_HAPPEN);
    }

    ypaworld_func159(&arg159);
}

void NC_STACK_ypaworld::sub_4D1444(int id, bool restoring)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];
    sitem.State = TMapSuperItem::STATE_TRIGGED;
    if ( !restoring )
        sitem.TriggerTime = _timeStamp;

    ypaworld_arg148 arg148;
    arg148.owner = sitem.PCell->owner;
    arg148.blg_ID = sitem.TriggerBldID;
    arg148.field_C = 1;
    arg148.CellId = sitem.CellId;
    arg148.field_18 = 0;

    ypaworld_func148(&arg148);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    if ( !restoring )
    {
        const bool customSuperItem = IsCustomSuperItem(sitem);
        sitem.CurrentRadius = 0;
        sitem.LastRadius = 0;
        sitem.CustomHitUnitGids.clear();
        if ( customSuperItem )
            sitem.CustomHitBuildingSlots.assign(_cells.size() * 9, 0);
        else
            sitem.CustomHitBuildingSlots.clear();

        if ( sitem.WaveTransientVPId > 0 )
            RemoveTransientVP(sitem.WaveTransientVPId);
        sitem.WaveTransientVPId = 0;
        sitem.WavePalEffectStarted = false;
        sitem.WaveShakeEffectStarted = false;

        if ( customSuperItem )
            StartCustomSuperItemDetonation(id);
    }

    if ( restoring )
        return;

    yw_arg159 arg159;
    arg159.Priority = 95;
    arg159.unit = 0;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBTRIG);
        arg159.MsgID = 71;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVETRIG);
        arg159.MsgID = 75;
    }
    else
    {
        arg159.MsgID = 0;
        arg159.txt = Locale::Text::OpenUA(Locale::OUA_INTERNAL_CANNOT_HAPPEN);
    }

    ypaworld_func159(&arg159);
}

NC_STACK_ypabact * NC_STACK_ypaworld::sb_0x47b028__sub0(uint32_t bactid)
{
    for ( NC_STACK_ypabact * &station : _unitsList )
    {
        if ( bactid == station->_gid )
            return station;

        for ( NC_STACK_ypabact * &commander : station->_kidList )
        {
            if ( bactid == commander->_gid )
                return station; // CHECK IT

            for ( NC_STACK_ypabact * &slave : commander->_kidList )
            {
                if ( bactid == slave->_gid )
                    return station;  // CHECK IT
            }
        }
    }

    return nullptr;
}

void NC_STACK_ypaworld::RefreshUnitPRT(NC_STACK_ypabact *unit, NC_STACK_ypabact *robo, bool isRobo)
{
    bact_arg80 arg80;
    arg80.pos = unit->_position;

    if ( unit->_bact_type == BACT_TYPES_GUN )
        arg80.field_C = 4;
    else
        arg80.field_C = 0;

    unit->SetPosition(&arg80);

    if ( unit->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *guno = (NC_STACK_ypagun *)unit;

        guno->ypagun_func128(guno->_gunBasis, false);
    }

    setState_msg arg78;
    arg78.newStatus = unit->_status;
    arg78.setFlags = 0;
    arg78.unsetFlags = 0;
    unit->SetState(&arg78);

    if ( unit->_status_flg & BACT_STFLAG_DEATH2 )
    {
        arg78.newStatus = BACT_STATUS_NOPE;
        arg78.unsetFlags = 0;
        arg78.setFlags = BACT_STFLAG_DEATH2;
        unit->SetState(&arg78);
    }

    if ( unit->_status_flg & BACT_STFLAG_FIRE )
    {
        arg78.newStatus = BACT_STATUS_NOPE;
        arg78.unsetFlags = 0;
        arg78.setFlags = BACT_STFLAG_FIRE;
        unit->SetState(&arg78);
    }

    if ( !isRobo )
    {
        unit->_host_station = dynamic_cast<NC_STACK_yparobo *>(robo);
        unit->_owner = robo->_owner;
    }

    if ( unit->_primTtype == BACT_TGT_TYPE_UNIT )
    {
        unit->_primTtype = BACT_TGT_TYPE_NONE;

        setTarget_msg arg67;
        arg67.tgt.pbact = sb_0x47b028__sub0((int)(size_t)unit->_primT.pbact);
        arg67.tgt_type = BACT_TGT_TYPE_UNIT_IND;
        arg67.priority = 0;
        unit->SetTarget(&arg67);
    }

    if ( unit->_primTtype == BACT_TGT_TYPE_CELL )
    {
        unit->_primTtype = BACT_TGT_TYPE_NONE;

        setTarget_msg arg67_1;
        arg67_1.tgt_type = BACT_TGT_TYPE_CELL_IND;
        arg67_1.tgt_pos = unit->_primTpos;
        arg67_1.priority = 0;

        unit->SetTarget(&arg67_1);
    }
}

int ypaworld_func64__sub4(NC_STACK_ypaworld *yw, base_64arg *arg)
{
    if ( yw->_isNetGame )
        return 0;

    if ( !yw->_gamePaused )
    {
        if ( arg->field_8->HotKeyID == 32 || arg->field_8->KbdLastHit == Input::KC_PAUSE )
        {
            yw->_gamePaused = true;
            yw->_gamePausedTimeStamp = arg->TimeStamp;
        }
        return 0;
    }

    if ( arg->field_8->KbdLastHit != Input::KC_NONE )
    {
        yw->_gamePaused = false;
        arg->TimeStamp = yw->_gamePausedTimeStamp;
    }
    else
    {
        GFX::Engine.BeginFrame();


        vec3d a2a = yw->_viewerPosition + vec3d::OY(50000.0);

        SFXEngine::SFXe.sub_423EFC(1, a2a, vec3d(0.0), mat3x3::Ident());

        if ( arg->TimeStamp / 500 & 1 )
        {
            const std::string v6 = Locale::Text::Common(Locale::CMN_PAUSED);

            CmdStream v10;
            v10.reserve(256);

            FontUA::select_tileset(&v10, 15);

            FontUA::set_xpos(&v10, 0);
            FontUA::set_center_ypos(&v10, -yw->_fontH / 2);

            FontUA::FormateCenteredSkipableItem(yw->_guiTiles[15], &v10, v6, yw->_screenSize.x);

            FontUA::set_end(&v10);

            GFX::Engine.ProcessDrawSeq(v10);
        }

        SFXEngine::SFXe.sb_0x424c74();

        GFX::Engine.EndFrame();
    }
    return 1;
}

void ypaworld_func64__sub2(NC_STACK_ypaworld *yw)
{
    yw->_playerInHSGun = false;

    if ( yw->_userRobo != yw->_userUnit )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(yw->_userRobo);

        for (World::TRoboGun &gun : robo->GetGuns())
        {
            if ( yw->_userUnit == gun.gun_obj )
                yw->_playerInHSGun = true;
        }
    }
}

void ypaworld_func64__sub9(NC_STACK_ypaworld *yw)
{
    for ( size_t i = 0; i < yw->_levelInfo.Gates.size(); i++ )
    {
        const TMapGate &gate = yw->_levelInfo.Gates[i];
        int gateState = cellArea::PT_GATEOPENED;

        if ( gate.PCell->owner == yw->_userRobo->_owner )
        {
            for( const TMapKeySector &ks : gate.KeySectors )
            {
                if (ks.PCell)
                {
                    if (ks.PCell->owner != yw->_userRobo->_owner)
                    {
                        gateState = cellArea::PT_GATECLOSED;
                        break;
                    }
                }
            }
        }
        else
        {
            gateState = cellArea::PT_GATECLOSED;
        }

        if ( gate.PCell->PurposeType != gateState )
        {
            ypaworld_arg148 arg148;
            arg148.owner = gate.PCell->owner;
            arg148.field_C = 1;
            arg148.CellId = gate.CellId;
            arg148.field_18 = 0;

            if ( gateState == cellArea::PT_GATEOPENED )
            {
                arg148.blg_ID = gate.OpenBldID;
            }
            else
            {
                arg148.blg_ID = gate.ClosedBldID;

                yw_arg159 arg159;
                arg159.unit = 0;
                arg159.Priority = 65;
                arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATECLOSE);
                arg159.MsgID = 24;

                yw->ypaworld_func159(&arg159);
            }

            yw->ypaworld_func148(&arg148);

            gate.PCell->PurposeType = gateState;
            gate.PCell->PurposeIndex = i;
        }

        if ( gateState == cellArea::PT_GATEOPENED )
        {
            int energ = 0;

            for ( NC_STACK_ypabact* &v8 : gate.PCell->unitsList )
            {
                if ( v8->_status != BACT_STATUS_DEAD && v8->_status != BACT_STATUS_BEAM )
                {
                    if ( v8->_bact_type != BACT_TYPES_ROBO && v8->_bact_type != BACT_TYPES_MISSLE && v8->_bact_type != BACT_TYPES_GUN )
                        energ += (v8->_energy_max + 99) / 100;
                }
            }

            if ( energ <= yw->_beamEnergyCapacity )
            {
                if ( yw->_timeStamp - yw->_msgTimestampGates > 60000 )
                {
                    yw_arg159 arg159_1;
                    arg159_1.unit = 0;
                    arg159_1.Priority = 49;
                    arg159_1.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATEOPEN);
                    arg159_1.MsgID = 23;

                    yw->ypaworld_func159(&arg159_1);
                    yw->_msgTimestampGates = yw->_timeStamp;
                }
            }
            else
            {
                if ( yw->_timeStamp - yw->_msgTimestampGates > 40000 )
                {
                    yw_arg159 arg159_2;
                    arg159_2.unit = 0;
                    arg159_2.Priority = 10;
                    arg159_2.txt = Locale::Text::Feedback(Locale::FEEDBACK_GATEFULL);
                    arg159_2.MsgID = 46;

                    yw->ypaworld_func159(&arg159_2);
                    yw->_msgTimestampGates = yw->_timeStamp;
                }
            }
        }
    }
}

bool NC_STACK_ypaworld::sub_4D11C0(int id, int owner)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( sitem.PCell->owner != owner )
        return false;

    if ( sitem.KeySectors.empty() )
        return true;

    for ( const TMapKeySector &ks : sitem.KeySectors )
    {
        if ( ks.PCell->owner != owner )
            return false;
    }
    return true;
}

bool NC_STACK_ypaworld::sub_4D12A0(int owner)
{
    for ( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && owner == unit->_owner )
            return true;
    }

    return false;
}

void NC_STACK_ypaworld::sub_4D16C4(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    StopCustomSuperItemWaveEffects(id);

    sitem.State = TMapSuperItem::STATE_INACTIVE;
    sitem.ActiveTime = 0;
    sitem.TriggerTime = 0;
    sitem.ActivateOwner = 0;
    sitem.CountDown = 0;

    ypaworld_arg148 arg148;
    arg148.owner = sitem.PCell->owner;
    arg148.blg_ID = sitem.InactiveBldID;
    arg148.field_C = 1;
    arg148.CellId = sitem.CellId;
    arg148.field_18 = 0;

    ypaworld_func148(&arg148);

    sitem.PCell->PurposeType = cellArea::PT_STOUDSON;
    sitem.PCell->PurposeIndex = id;

    yw_arg159 arg159;
    arg159.unit = nullptr;
    arg159.Priority = 92;

    if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_BOMBDEACT);
        arg159.MsgID = 73;
    }
    else if ( sitem.Type == TMapSuperItem::TYPE_WAVE )
    {
        arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_WAVEDEACT);
        arg159.MsgID = 77;
    }
    else
    {
        arg159.MsgID = 0;
        arg159.txt = Locale::Text::OpenUA(Locale::OUA_INTERNAL_CANNOT_HAPPEN);
    }

    ypaworld_func159(&arg159);
}

void NC_STACK_ypaworld::ypaworld_func64__sub19__sub0(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    for( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && sitem.ActivateOwner == unit->_owner )
        {
            if ( sub_4D11C0(id, sitem.ActivateOwner) )
            {
                if ( sitem.CountDown > 0 )
                    sitem.CountDown = sitem.CountDown - _frameTime;
                else
                    sub_4D1444(id);
            }
            else if ( !sitem.KeySectors.empty() )
            {
                sub_4D1594(id);
            }
            else
            {
                sub_4D12D8(id, 0);
            }
            return;
        }
    }

    sub_4D16C4(id);
}

void ypaworld_func64__sub19__sub3(NC_STACK_ypaworld *yw, int id)
{
    if ( yw->_GameShell )
    {
        TMapSuperItem &sitem = yw->_levelInfo.SuperItems[id];

        int v4 = sitem.CountDown / 1024;

        if ( v4 < 10 && v4 != sitem.LastSec )
        {
            SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            sitem.LastSec = v4;
        }

        int v5 = v4 / 10;
        if ( v5 != sitem.LastTenSec )
        {
            SFXEngine::SFXe.startSound(&yw->_GameShell->samples1_info, 3);
            sitem.LastTenSec = v5;
        }
    }
}

bool NC_STACK_ypaworld::sub_4D1230(int id, int a3)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( sitem.PCell->owner == a3 )
        return false;

    if ( sitem.KeySectors.empty() )
        return true;

    for ( const TMapKeySector &ks : sitem.KeySectors )
    {
        if (ks.PCell->owner == a3)
            return false;
    }

    return true;
}

void NC_STACK_ypaworld::ypaworld_func64__sub19__sub1(int id)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    for ( NC_STACK_ypabact * &unit : _unitsList )
    {
        if ( unit->_bact_type == BACT_TYPES_ROBO && sitem.ActivateOwner == unit->_owner )
        {
            if ( sub_4D11C0(id, sitem.ActivateOwner) )
                sub_4D12D8(id, 1);
            else if ( sub_4D1230(id, sitem.ActivateOwner) )
                sub_4D16C4(id);
            return;
        }
    }

    sub_4D16C4(id);
}

void NC_STACK_ypaworld::ypaworld_func64__sub19__sub2__sub0__sub0(uint8_t activate, float a5, float a6, float a7)
{
    for(cellArea &cell : _cells)
    {
        for( NC_STACK_ypabact* &bct : cell.unitsList )
        {
            int v9 = 1;

            if ( _isNetGame )
            {
                if ( bct->_owner != _userUnit->_owner || bct->_owner == activate || bct->_status == BACT_STATUS_DEAD )
                    v9 = 0;
            }
            else if ( bct->_owner == activate || bct->_status == BACT_STATUS_DEAD )
            {
                v9 = 0;
            }

            if ( v9 )
            {
                float v10 = a5 - bct->_position.x;
                float v11 = a6 - bct->_position.z;

                if ( sqrt(POW2(v10) + POW2(v11)) < a7 )
                {
                    bact_arg84 arg84;
                    arg84.energy = -22000000;
                    arg84.unit = nullptr;

                    bct->ModifyEnergy(&arg84);
                }
            }
        }

    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub19__sub2__sub0(int id)
{
    TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    sitem.CurrentRadius = (_timeStamp - sitem.TriggerTime) * World::CVSectorLength / 2400.0;

    vec2d tmp = World::SectorIDToCenterPos2( sitem.CellId );

    float v19 = sqrt(POW2(_mapLength.x) + POW2(_mapLength.y));

    if ( sitem.CurrentRadius > 300 && sitem.CurrentRadius - sitem.LastRadius > 200 && sitem.CurrentRadius < v19 )
    {
        float v9 = (2 * sitem.CurrentRadius) * C_PI / 150.0;

        sitem.LastRadius = sitem.CurrentRadius;

        if ( v9 > 2.0 )
        {
            for (float v25 = 0.0; v25 < 6.283 ; v25 += 6.283 / v9 )
            {
                float v10 = sitem.CurrentRadius;
                float v26 = cos(v25) * v10 + tmp.x;
                float v21 = sin(v25) * v10 + tmp.y;

                if ( v26 > 600.0 && v21 < -600.0 && v26 < _mapLength.x - 600.0 && v21 > -(_mapLength.y - 600.0) )
                {
                    int v12 = _fxLimit;

                    _fxLimit = 2;

                    yw_arg129 arg129;
                    arg129.pos.x = v26;
                    arg129.pos.y = sitem.PCell->height;
                    arg129.pos.z = v21;
                    arg129.field_10 = 200000;
                    arg129.OwnerID = sitem.ActivateOwner;
                    arg129.unit = 0;

                    ypaworld_func129(&arg129);

                    _fxLimit = v12;
                }
            }
        }
    }

    // The superbomb itself does not request a slowdown. If this damage pass
    // actually destroys a Host Station, the normal Host Station death path
    // decides whether to start the shared time-scale event using the configured
    // scale, duration and maximum distance.
    ypaworld_func64__sub19__sub2__sub0__sub0(
        sitem.ActivateOwner, tmp.x, tmp.y, sitem.CurrentRadius);
}

void NC_STACK_ypaworld::ypaworld_func64__sub19__sub2(int id)
{
    const TMapSuperItem &sitem = _levelInfo.SuperItems[id];

    if ( !sub_4D1230(id, sitem.ActivateOwner) && sub_4D12A0(sitem.ActivateOwner) )
    {
        if ( sitem.Type == TMapSuperItem::TYPE_BOMB )
        {
            if ( IsCustomSuperItem(sitem) )
                UpdateCustomSuperItem(id);
            else
                ypaworld_func64__sub19__sub2__sub0(id);
        }
    }
    else
    {
        sub_4D16C4(id);
    }
}

void NC_STACK_ypaworld::ypaworld_func64__sub19()
{
    for (size_t i = 0; i < _levelInfo.SuperItems.size(); i++)
    {
        const TMapSuperItem &sitem = _levelInfo.SuperItems[i];

        if ( sitem.Type != 0 )
        {
            switch ( sitem.State )
            {
            case TMapSuperItem::STATE_INACTIVE:
                if ( sub_4D11C0(i, sitem.PCell->owner) )
                {
                    if ( sub_4D12A0(sitem.PCell->owner) )
                        sub_4D12D8(i, 0);
                }
                break;

            case TMapSuperItem::STATE_ACTIVE:
                ypaworld_func64__sub19__sub0(i);
                ypaworld_func64__sub19__sub3(this, i);
                break;

            case TMapSuperItem::STATE_STOPPED:
                ypaworld_func64__sub19__sub1(i);
                break;

            case TMapSuperItem::STATE_TRIGGED:
                ypaworld_func64__sub19__sub2(i);
                break;

            default:
                break;
            }
        }
    }

    UpdateSuperItemFalloutAtmosphericFX();
    UpdateMobilePowerFX();

    for (const std::unique_ptr<TSndCarrier> &carrier : _superItemSoundCarriers)
    {
        if ( carrier )
            SFXEngine::SFXe.UpdateSoundCarrier(carrier.get());
    }

    for (const std::unique_ptr<TSndCarrier> &carrier : _superItemWaveSoundCarriers)
    {
        if ( carrier )
            SFXEngine::SFXe.UpdateSoundCarrier(carrier.get());
    }
}

void NC_STACK_ypaworld::VoiceMessageCalcPositionToUnit()
{
    if ( _voiceMessage.Unit == _userRobo )
    {
        _voiceMessage.Carrier.Position = _userUnit->_position;
    }
    else
    {
        vec3d tmp = _voiceMessage.Unit->_position - _userUnit->_position;

        float v11 = tmp.length();

        if ( v11 > 0.0 )
            tmp *= (100.0 / v11);

        _voiceMessage.Carrier.Position = _userUnit->_position + tmp;
    }
}

void NC_STACK_ypaworld::VoiceMessageUpdate()
{
    if ( IsSpectatorControlled() )
    {
        _voiceMessage.Reset();
        return;
    }

    if ( _voiceMessage.Priority >= 0 )
    {
        if ( _voiceMessage.Unit->_status != BACT_STATUS_DEAD )
        {
            VoiceMessageCalcPositionToUnit();

            _voiceMessage.Carrier.Vector = _userUnit->_fly_dir * _userUnit->_fly_dir_length;
        }

        if ( _voiceMessage.Carrier.Sounds[0].IsEnabled() )
        {
            SFXEngine::SFXe.UpdateSoundCarrier(&_voiceMessage.Carrier);
        }
        else
            _voiceMessage.Reset();
    }
}

void ypaworld_func64__sub3(NC_STACK_ypaworld *yw)
{
    if ( yw->IsSpectatorControlled() )
        return;

    if ( yw->_userUnit->_pSector->owner != yw->_userRobo->_owner )
    {
        if ( yw->_userUnit->_pSector->owner )
        {
            if ( yw->_ownerOldCellUserUnit == yw->_userRobo->_owner || !yw->_ownerOldCellUserUnit )
            {
                if ( yw->_timeStamp - yw->_msgTimestampEnemySector > 10000 )
                {
                    yw_arg159 arg159;
                    arg159.unit = yw->_userUnit;
                    arg159.Priority = 24;
                    arg159.txt = Locale::Text::Feedback(Locale::FEEDBACK_ESECTENTER);
                    arg159.MsgID = 22;

                    yw->ypaworld_func159(&arg159);
                }

                yw->_msgTimestampEnemySector = yw->_timeStamp;
            }
        }
    }
}

void NC_STACK_ypaworld::ProfileCalcValues()
{
    _profileFramesCount++;

    if ( _profileFramesCount >= 5 )
    {
        for (size_t i = 0; i < PFID_MAX; i++)
        {
            if ( _profileVals[i] != 0 )
            {
                if ( _profileVals[i] < _profileMin[i] )
                    _profileMin[i] = _profileVals[i];

                if ( _profileVals[i] > _profileMax[i] )
                    _profileMax[i] = _profileVals[i];

                _profileTotal[i] += _profileVals[i];
            }
        }
    }
}

int NC_STACK_ypaworld::yw_RestoreVehicleData()
{
    std::string buf = fmt::sprintf("save:%s/%d.rst", _GameShell->UserName, _levelInfo.LevelID);

    ScriptParser::HandlersList parsers {
        new World::Parsers::VhclProtoParser(this),
        new World::Parsers::WeaponProtoParser(this),
        new World::Parsers::BuildProtoParser(this)
    };

    const bool parsed = ScriptParser::ParseFile(buf, parsers, 0);
    if ( parsed )
        ResolveStatusProfileLinks();
    return parsed;
}

void NC_STACK_ypaworld::EnableLevelPasses()
{
    if ( _levelInfo.State == TLevelInfo::STATE_COMPLETED )
    {
        TMapGate &gate = _levelInfo.Gates[ _levelInfo.GateCompleteID ];

        _globalMapRegions.MapRegions[ _levelInfo.LevelID ].Status = TMapRegionInfo::STATUS_COMPLETED;

        for (int lvl : gate.PassToLevels)
        {
            if ( _globalMapRegions.MapRegions[ lvl ].Status == TMapRegionInfo::STATUS_DISABLED )
                _globalMapRegions.MapRegions[ lvl ].Status = TMapRegionInfo::STATUS_ENABLED;
        }
    }
    else if ( _levelInfo.State == TLevelInfo::STATE_ABORTED && !yw_RestoreVehicleData() )
    {
        ypa_log_out("yw_RestoreVehicleData() failed.\n");
    }
}

void NC_STACK_ypaworld::NetReleaseMissiles(NC_STACK_ypabact *bact)
{
    while(!bact->_missiles_list.empty())
    {
        NC_STACK_ypamissile *misl = bact->_missiles_list.front();
        bact->_missiles_list.pop_front();

        if ( misl->_primTtype == BACT_TGT_TYPE_UNIT )
        {
            misl->_primT.pbact->DeleteAttacker(misl, 0);
            misl->_primTtype = BACT_TGT_TYPE_NONE;
        }

        misl->CleanAttackersTarget();

        misl->_parent = nullptr;

        ypaworld_func144(misl);

        misl->_status_flg |= BACT_STFLAG_DEATH1;
    }
}

void NC_STACK_ypaworld::sub_4F1BE8(NC_STACK_ypabact *bct)
{
    if ( bct->_bact_type == BACT_TYPES_GUN )
    {
        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );

        if ( gun->IsRoboGun() )
        {
            for (World::TRoboGun &pgun : bct->_host_station->GetGuns())
            {
                if ( bct == pgun.gun_obj )
                    pgun.gun_obj = nullptr;
            }
        }
    }
}

void NC_STACK_ypaworld::NetRemove(NC_STACK_ypabact *bct)
{
    while(!bct->_kidList.empty())
    {
        NC_STACK_ypabact *cmnder = bct->_kidList.front();

        while ( !cmnder->_kidList.empty() )
        {
            NC_STACK_ypabact *slave = cmnder->_kidList.front();

            NetReleaseMissiles(slave);
            slave->CleanAttackersTarget();
            sub_4F1BE8(slave);

            slave->_status_flg |= BACT_STFLAG_DEATH1;
            slave->_status = BACT_STATUS_DEAD;

            ypaworld_func144(slave);
        }

        NetReleaseMissiles(cmnder);
        cmnder->CleanAttackersTarget();
        sub_4F1BE8(cmnder);

        cmnder->_status_flg |= BACT_STFLAG_DEATH1;
        cmnder->_status = BACT_STATUS_DEAD;

        ypaworld_func144(cmnder);
    }

    if ( bct->_bact_type == BACT_TYPES_ROBO )
    {
        NC_STACK_yparobo *robo = dynamic_cast<NC_STACK_yparobo *>(bct);

        for (World::TRoboGun &gun : robo->GetGuns())
            gun.gun_obj = nullptr;
    }

    bct->CleanAttackersTarget();
    NetReleaseMissiles(bct);

    bct->_status = BACT_STATUS_DEAD;

    ypaworld_func144(bct);

    bct->_status_flg |= BACT_STFLAG_DEATH1;
}

void NC_STACK_ypaworld::ProtosFreeSounds()
{
    SFXEngine::SFXe.StopPlayingSounds();

    for (World::TVhclProto &vhcl : _vhclProtos)
    {
        for (World::TVhclSound &sfx : vhcl.sndFX)
            sfx.ClearSounds();
    }

    for (World::TWeapProto &wep : _weaponProtos)
    {
        for (World::TVhclSound &fx : wep.sndFXes)
            fx.ClearSounds();

        wep.debuff.tick_snd.ClearSounds();
        wep.delay_time_debuff.tick_snd.ClearSounds();
    }

    for (World::TBuildingProto &proto : _buildProtos)
        proto.SndFX.ClearSounds();
}

void NC_STACK_ypaworld::FFeedback_Update()
{
    if ( _shellConfIsParsed )
    {
        if ( _preferences & (World::PREF_JOYDISABLE | World::PREF_FFDISABLE) )
            return;
    }

    if ( _userUnit )
    {
        if ( _timeStamp - _ffTimeStamp > 250 )
        {
            _ffTimeStamp = _timeStamp;

            if ( _userUnit->_status == BACT_STATUS_DEAD )
            {
                Input::Engine.ForceFeedback(Input::FF_STATE_UPDATE, _ffEffectType);
            }
            else
            {
                if ( _ffEffectType != -1 )
                {
                    float a1 = POW2(_userUnit->_force) - POW2(_userUnit->_mass) * 100.0;

                    if (a1 < 0.0)
                        a1 = 0.0;

                    float v17 = fabs(_userUnit->_fly_dir_length) / ( sqrt(a1) / _userUnit->_airconst_static );
                    if ( v17 >= 1.0 )
                        v17 = 1.0;
                    else if (v17 < 0.0)
                        v17 = 0.0;

                    Input::Engine.ForceFeedback(Input::FF_STATE_UPDATE, _ffEffectType,
                                                        _ffMagnitude, _ffPeriod * v17);
                }
            }
        }

        TSoundSource *top = SFXEngine::SFXe.SndGetTopShake();
        if ( top )
        {
            if ( top->StartTime == SFXEngine::SFXe.currentTime )
            {
                float p1 = top->ShkMag;
                if ( p1 > 1.0 )
                    p1 = 1.0;

                vec3d tmp = top->PCarrier->Position - _userUnit->_position;

                float p2 = top->PShkFx->time;
                float p3 = _userUnit->_rotation.AxisX().dot( tmp );
                float p4 = -_userUnit->_rotation.AxisZ().dot( tmp );

                if ( p2 > 0.0 )
                {
                    Input::Engine.ForceFeedback(Input::FF_STATE_START, Input::FF_TYPE_SHAKE,
                                                        p1, p2, p3, p4);
                }
            }
        }
    }
}

void sb_0x447720(NC_STACK_ypaworld *yw, TInputState *inpt)
{
    if ( inpt->KbdLastHit == Input::KC_NUMMUL && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
    {
        sub_4476AC(yw);

        yw_arg159 info_msg;
        info_msg.txt = Locale::Text::OpenUA(Locale::OUA_SCREENSHOT_SAVED);
        info_msg.unit = nullptr;
        info_msg.Priority = 100;
        info_msg.MsgID = 0;

        yw->ypaworld_func159(&info_msg);
    }


    if ( yw->_screenShotSeq )
    {
        if ( inpt->KbdLastHit == Input::KC_NUMDIV && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
        {
            yw->_screenShotSeq = false;

            yw_arg159 info_msg;
            info_msg.txt = Locale::Text::OpenUA(Locale::OUA_SCREENSHOTTING_STOPPED);
            info_msg.unit = nullptr;
            info_msg.Priority = 100;
            info_msg.MsgID = 0;

            yw->ypaworld_func159(&info_msg);
        }

        GFX::Engine.SaveScreenshot( fmt::sprintf("env:snaps/s%d_%04d", yw->_screenShotSeqId, yw->_screenShotSeqFrame) );

        yw->_screenShotSeqFrame++;
    }
    else if ( inpt->KbdLastHit == Input::KC_NUMDIV && (inpt->ClickInf.flag & 0x100 || yw->_easyCheatKeys) )
    {
        yw->_screenShotSeqFrame = 0;
        yw->_screenShotSeq = true;
        yw->_screenShotSeqId++;

        yw_arg159 info_msg;
        info_msg.txt = Locale::Text::OpenUA(Locale::OUA_SCREENSHOTTING_STARTED);
        info_msg.unit = nullptr;
        info_msg.Priority = 100;
        info_msg.MsgID = 0;

        yw->ypaworld_func159(&info_msg);
    }

    if ( yw->_replayRecorder->do_record )
    {
        if ( inpt->KbdLastHit == Input::KC_NUMMINUS && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
        {
            recorder_stoprec(yw);

            yw_arg159 info_msg;
            info_msg.txt = Locale::Text::OpenUA(Locale::OUA_REPLAY_RECORDING_STOPPED);
            info_msg.unit = nullptr;
            info_msg.Priority = 100;
            info_msg.MsgID = 0;

            yw->ypaworld_func159(&info_msg);
        }

    }
    else
    {
        if ( inpt->KbdLastHit == Input::KC_NUMMINUS && (inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_HOLD || yw->_easyCheatKeys) )
        {
            recorder_startrec(yw);

            yw_arg159 info_msg;
            info_msg.txt = Locale::Text::OpenUA(Locale::OUA_REPLAY_RECORDING_STARTED);
            info_msg.unit = nullptr;
            info_msg.Priority = 100;
            info_msg.MsgID = 0;

            yw->ypaworld_func159(&info_msg);
        }
    }
}

void NC_STACK_ypaworld::ypaworld_func163__sub1(TGameRecorder *rcrd, int dTime)
{
    if ( dTime )
    {
        rcrd->field_78 &= 0xFFFFFFFE;

        while ( rcrd->field_74 - 1 != rcrd->frame_id  &&  (int32_t)(dTime + _timeStamp) > rcrd->time )
        {
            if ( rcrd->mfile.parse() != IFFile::IFF_ERR_EOF )
            {
                if ( rcrd->mfile.GetCurrentChunk().Is(TAG_FORM, TAG_FRAM) )
                    recorder_read_framedata(rcrd);
            }
        }


        if ( rcrd->field_74 - 1 == rcrd->frame_id )
        {
            recorder_go_to_frame(rcrd, 0);
        }
        else
        {
            if ( rcrd->field_78 & 1 )
            {
                _timeStamp = rcrd->time;
                recorder_updateObjectList(rcrd, 1.0, dTime);
            }
            else
            {
                float v9 = (float)dTime / (float)(rcrd->time - _timeStamp);

                _timeStamp += dTime;

                recorder_updateObjectList(rcrd, v9, dTime);
            }
        }
    }
}

void ypaworld_func163__sub2__sub1(NC_STACK_ypaworld *yw, float fperiod, TInputState *inpt)
{
    TGameRecorder *rcrd = yw->_replayPlayer;

    float v20 = rcrd->rotation_matrix.m20;
    float v18 = rcrd->rotation_matrix.m22;

    float v13 = inpt->Sliders[0] * 250.0 * fperiod;
    float v14 = -inpt->Sliders[2] * 250.0 * fperiod;
    float v15 = -inpt->Sliders[1] * 150.0 * fperiod;

    float v17 = sqrt( POW2(v20) + POW2(v18) );
    if ( v17 > 0.0 )
    {
        v20 /= v17;
        v18 /= v17;
    }

    rcrd->field_44.z += v15 * v18;
    rcrd->field_44.x += v15 * v20;

    float v21 = rcrd->rotation_matrix.m00;
    float v19 = rcrd->rotation_matrix.m02;

    float v16 = sqrt( POW2(v21) + POW2(v19) );
    if ( v16 > 0.0 )
    {
        v21 /= v16;
        v19 /= v16;
    }

    rcrd->field_44.y += v14;
    rcrd->field_44.z += v19 * v13;
    rcrd->field_44.x += v21 * v13;
}

void ypaworld_func163__sub2__sub0(NC_STACK_ypaworld *yw, float fperiod, TInputState *inpt)
{
    float v3 = inpt->Sliders[10] * 2.5 * fperiod;

    if ( fabs(v3) > 0.001 )
        yw->_replayPlayer->rotation_matrix = mat3x3::RotateY(-v3) * yw->_replayPlayer->rotation_matrix;

    float v5 = inpt->Sliders[11] * 2.5 * fperiod;

    if ( fabs(v5) > 0.001 )
    {
        yw->_replayPlayer->rotation_matrix = mat3x3::RotateX(v5) * yw->_replayPlayer->rotation_matrix;
    }
}

void sub_445654(NC_STACK_ypaworld *yw, CmdStream *in, char *buf, const char *fmt, ...)
{
    FontUA::copy_position(in);

    va_list va;
    va_start(va, fmt);

    vsprintf(buf, fmt, va);

    va_end(va);

    FontUA::add_txt(in, yw->_screenSize.x, 1, buf);
}

void NC_STACK_ypaworld::HistoryAktCreate(NC_STACK_ypabact *bact)
{
    HistoryEventAdd( World::History::VhclCreate(bact->_owner, bact->_vehicleID, bact->_position.x * 256.0 / bact->_wrldSize.x, bact->_position.z * 256.0 / bact->_wrldSize.y) );
}

void NC_STACK_ypaworld::HistoryAktKill(NC_STACK_ypabact *bact)
{
    if ( bact->_killer )
    {
        uint8_t owners = (bact->_killer->_owner << 3) | bact->_owner;

        if ( bact->_killer->getBACT_viewer() || (bact->_killer->_status_flg & BACT_STFLAG_ISVIEW) )
            owners |= 0x80;

        if ( bact->getBACT_viewer() || (bact->_status_flg & BACT_STFLAG_ISVIEW) )
            owners |= 0x40;

        uint16_t vp = bact->_vehicleID;

        if ( bact->_bact_type == BACT_TYPES_ROBO )
            vp |= 0x8000;

        HistoryEventAdd( World::History::VhclKill(owners, vp, bact->_position.x * 256.0 / bact->_wrldSize.x, bact->_position.z * 256.0 / bact->_wrldSize.y) );
    }
}
