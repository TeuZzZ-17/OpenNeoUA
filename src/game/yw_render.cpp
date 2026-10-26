// Rendering of the game view: sectors, fillers, sky, super waves, transient VPs and camera preparation.
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

void NC_STACK_ypaworld::RenderAdditionalBeeBox(Common::Point sect, TRenderingSector *sct, baseRender_msg *bs77)
{
    sct->dword8 = 0;
    sct->dword4 = 0;

    if ( IsSector( sect ) )
    {
        sct->dword4 = 1;
        sct->p_cell = &_cells( sect );

        vec3d pos = World::SectorIDToCenterPos3( sect );
        pos.y = sct->p_cell->height;

        _beeBox->SetPosition(pos);

        if ( _beeBox->Render(bs77, nullptr) )
        {
            sct->dword8 = 1;
        }
    }
}

void NC_STACK_ypaworld::RenderSector(TRenderingSector *sct, baseRender_msg *bs77)
{
    cellArea *pcell = sct->p_cell;

    // Buildings share the same render-message tint path as Vehicle/Weapon VPs.
    // Keep it strictly per draw because the BASE models and render message are
    // shared; leaking these values would tint unrelated sectors or actors.
    auto renderBuilding = [bs77](NC_STACK_base *base,
                                 NC_STACK_base::Instance *instance,
                                 const World::TVisualTint &tint)
    {
        if ( !base )
            return;

        const GFX::TGLColor oldTint = bs77->tint;
        const GFX::TGLColor oldParticleTint = bs77->particleTint;
        const bool oldColorizeTint = bs77->colorizeTint;
        const bool oldParticleColorizeTint = bs77->particleColorizeTint;
        const bool oldExcludeSectorTerrainFromTint = bs77->excludeSectorTerrainFromTint;

        if ( !tint.IsNeutral() )
        {
            const GFX::TGLColor renderTint(tint.r, tint.g, tint.b, tint.a);
            bs77->tint = renderTint;
            bs77->particleTint = renderTint;
            bs77->colorizeTint = tint.ColorizesRGB();
            bs77->particleColorizeTint = tint.ColorizesRGB();
            bs77->excludeSectorTerrainFromTint = true;
        }
        else
        {
            bs77->tint = GFX::TGLColor(1.0, 1.0, 1.0, 1.0);
            bs77->particleTint = GFX::TGLColor(1.0, 1.0, 1.0, 1.0);
            bs77->colorizeTint = false;
            bs77->particleColorizeTint = false;
            bs77->excludeSectorTerrainFromTint = false;
        }

        base->Render(bs77, instance);

        bs77->tint = oldTint;
        bs77->particleTint = oldParticleTint;
        bs77->colorizeTint = oldColorizeTint;
        bs77->particleColorizeTint = oldParticleColorizeTint;
        bs77->excludeSectorTerrainFromTint = oldExcludeSectorTerrainFromTint;
    };

    // The legacy sector proxy is centered on the terrain. At high altitude,
    // looking upward can move that proxy completely outside the camera frustum
    // even though the player cockpit body or a tall RAND border wall is still
    // visible. Keep ordinary sector culling unchanged, but allow those two
    // camera-relevant visuals to reach their own mesh clipping stage.
    const bool renderSectorContents = sct->dword8 != 0;
    const bool scanBorderWalls = !renderSectorContents && pcell->IsBorder() && !_hideMapBorderWalls;

    if ( renderSectorContents || scanBorderWalls )
    {
        int v22 = 0;
        World::TVisualTint buildingTint;
        bool hasBuildingVisual = pcell->PurposeType != cellArea::PT_NONE;

        vec3d scel;
        if ( pcell->PurposeType == cellArea::PT_CONSTRUCTING )
        {
            auto it = _inBuildProcess.find(pcell->Id);
            if (it != _inBuildProcess.end())
            {
                TConstructInfo &bldProc = it->second;

                scel = vec3d::OY((float)bldProc.Time / (float)bldProc.EndTime);

                const World::TBuildingProto &proto = _buildProtos[ bldProc.BuildID ];
                pcell->type_id = proto.SecType;
                pcell->SectorType = _secTypeArray[ pcell->type_id ].SectorType;
                buildingTint = proto.tint;

                v22 = 1;
            }
            else
            {
                hasBuildingVisual = false;
            }
        }
        else if ( hasBuildingVisual )
        {
            // sb_0x456384 stores this before special systems repurpose
            // PurposeIndex for a gate/gem/super-item index.
            buildingTint = pcell->BuildingTint;
        }

        int v17, v20;

        if ( pcell->SectorType == 1 )
        {
            v17 = 0;
            v20 = 1;
        }
        else
        {
            v17 = -1;
            v20 = 3;
        }

        for (int zz = 0; zz < v20; zz++)
        {
            for (int xx = 0; xx < v20; xx++)
            {
                vec3d pos = sct->p_cell->CenterPos + vec3d((v17 + xx) * 300.0, 0.0, (v17 + zz) * 300.0);

                if ( v22 )
                {
                    if ( !renderSectorContents )
                        continue;

                    NC_STACK_base *bld = _legoArray[ _secTypeArray[ pcell->type_id ].SubSectors.At(xx, zz)->HPModels[0] ].Base;

                    bld->SetStatic(false);

                    bld->SetScale(scel, NC_STACK_base::UF_Y); //Scale only Y
                    bld->SetPosition(pos);

                    NC_STACK_base::CheckOpts( &pcell->BldVPOpts.At(xx, zz), bld );

                    renderBuilding(bld, pcell->BldVPOpts.At(xx, zz), buildingTint);

                    bld->SetStatic(true);
                }
                else
                {
                    const int legoId = GetLegoBld(pcell, xx, zz);
                    NC_STACK_base *bld = _legoArray[ legoId ].Base;

                    const bool borderWallModel =
                        pcell->IsBorder() &&
                        legoId >= kMapBorderWallFirstLego &&
                        legoId <= kMapBorderWallLastLego;

                    if ( !renderSectorContents && !borderWallModel )
                        continue;

                    const bool hideBorderWall = _hideMapBorderWalls && borderWallModel;
                    const bool straightenBorderWall =
                        !hideBorderWall && _borderWallTopReady && borderWallModel;
                    const bool transformBorderWall = hideBorderWall || straightenBorderWall;

                    vec3d originalScale;
                    bool originalStatic = false;

                    if ( transformBorderWall )
                    {
                        originalScale = bld->GetScale();
                        originalStatic = bld->IsStatic();

                        vec3d wallScale = originalScale;
                        if ( hideBorderWall )
                        {
                            // RAND boundary BASEs contain both the vertical wall and
                            // their ground strip. Collapse only Y so wall polygons become
                            // degenerate while the XZ ground geometry remains rendered.
                            wallScale.y = 0.0f;
                        }
                        else
                        {
                            wallScale.y = (pcell->CenterPos.y - _borderWallTopY) /
                                          kMapBorderWallVanillaHeight;

                            // The target is selected from the highest vanilla wall,
                            // therefore this normally only extends shorter segments.
                            if ( !isfinite(wallScale.y) || wallScale.y < 1.0 )
                                wallScale.y = 1.0;
                        }

                        // Static BASE rendering ignores scale. Temporarily use the
                        // full transform and restore the shared model immediately.
                        bld->SetStatic(false);
                        bld->SetScale(wallScale, NC_STACK_base::UF_Y);
                    }

                    bld->SetPosition(pos);

                    NC_STACK_base::CheckOpts( &pcell->BldVPOpts.At(xx, zz), bld );

                    renderBuilding(bld, pcell->BldVPOpts.At(xx, zz),
                                   hasBuildingVisual ? buildingTint : World::TVisualTint());

                    if ( transformBorderWall )
                    {
                        bld->SetScale(originalScale, NC_STACK_base::UF_Y);
                        bld->SetStatic(originalStatic);
                    }
                }
            }
        }
    }

    // Dynamic actors must not inherit visibility from the terrain-centered
    // sector proxy. At high altitude that proxy can leave the frustum while an
    // aircraft in the same sector is still visible. Each actor keeps its own
    // VP/skeleton clipping, stealth and first-person body rules in Render().
    for ( NC_STACK_ypabact* &bact : pcell->unitsList )
        bact->Render(bs77);
}

void NC_STACK_ypaworld::yw_renderSky(baseRender_msg *rndr_params)
{
    if ( _skyObject )
    {
        float v6 = rndr_params->maxZ;
        uint32_t flags = rndr_params->flags;

        const int32_t horizonDistance = GetSkyHorizonDistance();
        if ( _skyAppliedHorizonDistance != horizonDistance )
        {
            const float scale =
                (float)horizonDistance / (float)YW_SKY_HORIZON_DISTANCE_REFERENCE;
            _skyObject->SetScale(_skyBaseScale * scale,
                                 NC_STACK_base::UF_X |
                                 NC_STACK_base::UF_Y |
                                 NC_STACK_base::UF_Z);
            _skyAppliedHorizonDistance = horizonDistance;
        }

        // The sky remains centered on the controlled viewer exactly like the
        // original game; only its radius changes with gfx.skydistance.
        _skyObject->SetPosition( _viewerBact->_position + vec3d::OY(_skyHeight) );

        rndr_params->maxZ = GFX::SKY_FAR_CLIP;

        // The visible sky uses the extended sky projection and therefore must
        // not write incompatible depth values. Rasterize() follows it with an
        // invisible world-projection depth pass of the same dome, restoring the
        // continuous vanilla-style horizon mask without sacrificing modern sky range.
        rndr_params->flags = GFX::RFLAGS_SKY
                           | GFX::RFLAGS_COMPUTED_COLOR
                           | GFX::RFLAGS_DISABLE_ZWRITE;

        _skyObject->Render(rndr_params, nullptr);

        rndr_params->maxZ = v6;
        rndr_params->flags = flags;
    }
}

bool NC_STACK_ypaworld::IsVisibleMapPos(vec2d pos)
{
    Common::Point pt( ((pos.x + 150) / 300) / 4, ((-pos.y + 150) / 300) / 4 );

    if ( !IsGamePlaySector( pt ) || !_viewerBact )
        return false;

    Common::Point dist = _viewerBact->_cellId.AbsDistance( pt );
    const int visibleRadius = (_renderSectors - 1) / 2;

    return dist.x + dist.y <= visibleRadius;
}

void NC_STACK_ypaworld::PrepareFiller(cellArea *sct, cellArea *sct2, float v9h, float v8h, bool vertical, TCellFillerCh *out, bool force)
{
    int x = _secTypeArray[ sct->type_id ].SurfaceType;
    int y = _secTypeArray[ sct2->type_id ].SurfaceType;

    if (!force && (out->Id1 == x && out->Id2 == y &&
        out->Heights[0] == sct->height && out->Heights[1] == sct2->height &&
        out->Heights[2] == v8h && out->Heights[3] == v9h))
        return;

    NC_STACK_base *bs;
    if (vertical)
        bs = _fillersVertical(x, y);
    else
        bs = _fillersHorizontal(x, y);

    UAskeleton::Data *skel = bs->GetSkeleton()->GetSkelet();

    vec2d pos = World::SectorIDToCenterPos2( sct2->CellId );

    bs->SetPosition( vec3d::X0Z( pos ), NC_STACK_base::UF_XZ );

    for (int i = 0; i < 4; i++)
        skel->POO[i].y = sct->height;

    for (int i = 4; i < 8; i++)
        skel->POO[i].y = sct2->height;

    skel->POO[8].y = v8h;
    skel->POO[9].y = v9h;

    bs->RecalcInternal(true);
    bs->MakeCoordsCache();

    out->FreeVBO();

    bs->MakeCache(out);

    out->MakeVBO();

    out->Id1 = x;
    out->Id2 = y;
    out->Heights[0] = sct->height;
    out->Heights[1] = sct2->height;
    out->Heights[2] = v8h;
    out->Heights[3] = v9h;
}

void NC_STACK_ypaworld::PrepareAllFillers()
{
    for (int i = 0; i < _mapSize.x - 1; i++)
    {
        for (int j = 0; j < _mapSize.y - 2; j++)
        {
            cellArea *sct = &_cells(i, j);
            cellArea *sct2 = &_cells(i, j + 1);

            float h;
            if (i == _mapSize.x - 1)
                h = sct2->averg_height;
            else
                h = _cells(i + 1, j + 1).averg_height;

            PrepareFiller(sct, sct2, sct2->averg_height, h, false, &_cellsHFCache(i, j), true);
        }
    }

    for (int i = 0; i < _mapSize.x - 2; i++)
    {
        for (int j = 0; j < _mapSize.y - 1; j++)
        {
            cellArea *sct = &_cells(i, j);
            cellArea *sct2 = &_cells(i + 1, j);

            float h;
            if (i == _mapSize.x - 1)
                h = sct2->averg_height;
            else
                h = _cells(i + 1, j + 1).averg_height;

            PrepareFiller(sct, sct2, sct2->averg_height, h, true, &_cellsVFCache(i, j), true);
        }
    }
}

void NC_STACK_ypaworld::RenderFillers(baseRender_msg *arg)
{
    //Render landscape linking parts
    for (int i = 0; i < _renderSectors; i++)
    {
        for (int j = 0; j < _renderSectors - 1; j++)
        {
            TRenderingSector &sct = rendering_sectors[j][i];
            TRenderingSector &sct2 = rendering_sectors[j + 1][i];

            if (sct.dword4 == 1 && sct2.dword4 == 1 &&
                (sct.dword8 == 1 || sct2.dword8 == 1))
            {
                float h;
                if (rendering_sectors[j + 1][i + 1].dword4 == 1)
                    h = rendering_sectors[j + 1][i + 1].p_cell->averg_height;
                else
                    h = sct2.p_cell->averg_height;

                TCellFillerCh &filler = _cellsVFCache( sct2.p_cell->CellId.x - 1, sct2.p_cell->CellId.y );
                PrepareFiller(sct.p_cell, sct2.p_cell, sct2.p_cell->averg_height, h, true, &filler);
                filler.Render(arg);
            }
        }
    }

    for (int i = 0; i < _renderSectors - 1; i++)
    {
        for (int j = 0; j < _renderSectors; j++)
        {
            TRenderingSector &sct = rendering_sectors[j][i];
            TRenderingSector &sct2 = rendering_sectors[j][i + 1];

            if (sct.dword4 == 1 && sct2.dword4 == 1 &&
                (sct.dword8 == 1 || sct2.dword8 == 1))
            {
                float h;
                if (rendering_sectors[j + 1][i + 1].dword4 == 1)
                    h = rendering_sectors[j + 1][i + 1].p_cell->averg_height;
                else
                    h = sct2.p_cell->averg_height;

                TCellFillerCh &filler = _cellsHFCache( sct2.p_cell->CellId.x, sct2.p_cell->CellId.y - 1 );
                PrepareFiller(sct.p_cell, sct2.p_cell, sct2.p_cell->averg_height, h, false, &filler);
                filler.Render(arg);
            }
        }
    }
}

void NC_STACK_ypaworld::RenderGame(base_64arg *bs64, int a2)
{
    if ( !_viewerBact )
        return;

    TF::TForm3D *v5 = TF::Engine.GetViewPoint();

    if ( v5 )
        v5->CalcGlobal();

    baseRender_msg rndrs;

    rndrs.flags = (_skyRender && _skyObject) ? GFX::RFLAGS_ALPHA_FOG : 0;
    rndrs.frameTime = bs64->DTime;
    // Render-side animation belongs to the same gameplay clock as physics,
    // AI and timers.  Using the platform timestamp here let animated VP
    // textures and particle emitters continue at real speed during GEM
    // slowdown even though their spawn intervals and lifetimes were scaled.
    rndrs.globTime = GetGameplayRenderTimeStamp();
    rndrs.adeCount = 0;

    rndrs.minZ = 1.0;

    if ( _renderSectors == 5 )
        rndrs.maxZ = (float)_normalVizLimit + 100.0;
    else
        rndrs.maxZ = (float)_normalVizLimit + 400.0;

    const bool skyHorizonActive = _skyRender && _skyObject;
    const int32_t skyHorizonDistance = GetSkyHorizonDistance();

    // Keep CPU-side object preparation one sector beyond the visible horizon.
    // The sky depth mask performs the continuous final reveal; this budget only
    // prevents distant objects from being prepared unnecessarily early.
    if ( skyHorizonActive )
        rndrs.maxZ = std::min(rndrs.maxZ,
                              (float)skyHorizonDistance + World::CVSectorLength);

    int v6 = _renderSectors - 1;

    for (int j = 0; j < v6; j++)
    {
        for (int i = 0; i < v6; i++)
        {
            rendering_sectors[j][i].dword4 = 0;
            rendering_sectors[j][i].dword8 = 0;
        }
    }

    const int v29 = v6 / 2;

    // _renderSectors is already calculated from Sky Horizon Distance with
    // hidden safety sectors behind the dome. Do not clamp it again here:
    // doing so exposes black gaps between the terrain and the sky.
    const int renderRadius = v29;

    for (int i = 0; i <= renderRadius; i++)
    {
        int v28 = renderRadius - i;

        for (int j = -i; j <= i; j++)
        {
            TRenderingSector *sct = &rendering_sectors[v29 + j][v29 - v28];

            RenderAdditionalBeeBox( _viewerBact->_cellId + Common::Point(j, -v28),
                                    sct, &rndrs);

            if ( sct->dword4 )
                RenderSector(sct, &rndrs);

        }

        if ( -v28 != v28 )
        {
            for (int j = -i; j <= i; j++)
            {
                TRenderingSector *sct = &rendering_sectors[v29 + j][v29 + v28];

                RenderAdditionalBeeBox( _viewerBact->_cellId + Common::Point(j, v28),
                                        sct, &rndrs);

                if ( sct->dword4 )
                    RenderSector(sct, &rndrs);
            }
        }
    }


    RenderSuperItems(&rndrs);

    RenderFillers(&rndrs);

    RenderProceduralEnergyFX(&rndrs);

    RenderGroundDecals(&rndrs);

    RenderGroundDecalSmoke(&rndrs);

    yw_RenderTransientVPs(this, &_transientVPs, &rndrs);

    bs64->field_C = rndrs.adeCount;

    _polysCount = rndrs.adeCount;
    _polysDraw = 7777;


    area_arg_65 rrg;
    rrg.timeStamp = GetGameplayRenderTimeStamp();
    rrg.frameTime = bs64->DTime;
    rrg.minZ = 1.0;
    // Particles outlive their emitter's visibility; use the visible limit,
    // not the extra sectors prepared behind the horizon.
    rrg.maxZ = (float)_normalVizLimit;
    rrg.fadeStart = (float)(_normalVizLimit - _normalFadeLength);
    rrg.fadeLength = (float)_normalFadeLength;
    rrg.ViewTForm = TF::Engine.GetViewPoint();
    rrg.OwnerTForm = nullptr;
    rrg.flags = 0;


    ParticleSystem().UpdateRender(&rrg, bs64->DTime);

    GFX::Engine.BeginScene();

    if ( _skyRender )
        yw_renderSky(&rndrs);

    GFX::Engine.Rasterize();

    GFX::Engine.EndScene();

    if ( a2 )
    {
        uint32_t tpm = profiler_begin();
        sb_0x4d7c08__sub0(this);
        yw_FinalizePriorityGameplayUi(this);
        _profileVals[PFID_NEWGUITIME] = profiler_end(tpm);
    }
}

void NC_STACK_ypaworld::CameraPrepareRender(TGameRecorder *rcrd, NC_STACK_ypabact *bact, TInputState *inpt)
{
    extern tehMap robo_map;
    extern squadMan squadron_manager;

    float fperiod = inpt->Period / 1000.0;

    if ( inpt->ClickInf.flag & TClickBoxInf::FLAG_RM_DOWN )
    {

        if ( _mouseGrabbed )
            _mouseGrabbed = false;
        else if ( inpt->ClickInf.selected_btn != &robo_map  &&  inpt->ClickInf.selected_btn != &squadron_manager )
            _mouseGrabbed = true;
    }

    if ( inpt->Buttons.Is(0) )
        rcrd->rotation_matrix = mat3x3::Ident();

    ypaworld_func163__sub2__sub1(this, fperiod, inpt);

    if ( _mouseGrabbed )
        ypaworld_func163__sub2__sub0(this, fperiod, inpt);

    if ( rcrd->field_80 == 16 )
    {
        recorder_set_bact_pos(bact, rcrd->field_44);
        bact->_rotation = rcrd->rotation_matrix;
    }
    else if ( rcrd->field_80 == 18 )
    {
        for ( NC_STACK_ypabact * &unit : _userRobo->GetKidList() )
        {
            if ( rcrd->field_84 == unit->_gid )
            {
                vec3d v35 = unit->_position + unit->_rotation.Transpose().Transform(rcrd->field_44);
                recorder_set_bact_pos(bact, v35);

                bact->_rotation = rcrd->rotation_matrix * unit->_rotation;
                break;
            }
        }
    }
    else if ( rcrd->field_80 == 20 )
    {
        for ( NC_STACK_ypabact * &unit : _userRobo->GetKidList() )
        {
            if ( rcrd->ctrl_bact_id == unit->_gid )
            {
                vec3d a3a = unit->_position + unit->_rotation.Transpose().Transform(rcrd->field_44);
                recorder_set_bact_pos(bact, a3a);

                bact->_rotation = rcrd->rotation_matrix * unit->_rotation;
                break;
            }
        }
    }

    bact->_fly_dir = bact->_old_pos - bact->_position;

    float v39 = bact->_fly_dir.length();
    if ( v39 <= 0.0 )
    {
        bact->_fly_dir = vec3d::OX(1.0);
        bact->_fly_dir_length = 0;
    }
    else
    {
        bact->_fly_dir /= v39;

        if ( fperiod <= 0.0 )
            bact->_fly_dir_length = 0;
        else
            bact->_fly_dir_length = (v39 / fperiod) / 6.0;
    }

    bact->_tForm.Pos = bact->_position;
    bact->_tForm.SclRot = bact->_rotation;
}
