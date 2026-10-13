// Replay recorder: frame capture, packing and playback.
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

int recorder_startrec(NC_STACK_ypaworld *yw)
{
    TGameRecorder *rcrd = yw->_replayRecorder;

    rcrd->do_record = 0;
    rcrd->field_40 = 0;
    rcrd->seqn++;
    rcrd->level_id = yw->_levelInfo.LevelID;
    rcrd->frame_id = 0;
    rcrd->time = 0;
    rcrd->bacts_count = 0;
    rcrd->ainf_size = 0;

    rcrd->mfile = IFFile::UAOpenIFFile(fmt::sprintf("env:snaps/m%02d%04d.raw", yw->_levelInfo.LevelID, rcrd->seqn), "wb");

    if ( !rcrd->mfile.OK() )
    {
        return 0;
    }

    rcrd->mfile.pushChunk(TAG_SEQN, TAG_FORM, -1);
    rcrd->mfile.pushChunk(0, TAG_SINF, 4);

    rcrd->mfile.writeU16L(rcrd->seqn);
    rcrd->mfile.writeU16L(rcrd->level_id);

    rcrd->mfile.popChunk();

    rcrd->do_record = 1;
    return 1;
}

void recorder_stoprec(NC_STACK_ypaworld *yw)
{
    TGameRecorder *rcrd = yw->_replayRecorder;
    rcrd->do_record = 0;

    rcrd->mfile.popChunk();

    rcrd->mfile.close();
}

void recorder_update_time(NC_STACK_ypaworld *yw, int dtime)
{
    yw->_replayRecorder->time += dtime;
    yw->_replayRecorder->field_40 -= dtime;
}

void NC_STACK_ypaworld::recorder_store_bact( TGameRecorder *rcrd, World::MissileList &bct_lst)
{
    for( NC_STACK_ypamissile * &bact : bct_lst )
    {
        if ( bact->_gid >= 0xFFFF || bact == _userRobo )
        {
            if ( rcrd->bacts_count < rcrd->bacts.size() )
            {
                rcrd->bacts[ rcrd->bacts_count ] = bact;
                rcrd->bacts_count++;
            }

            recorder_store_bact(rcrd, bact->_missiles_list);
            recorder_store_bact(rcrd, bact->_kidList);
        }
    }
}

void NC_STACK_ypaworld::recorder_store_bact( TGameRecorder *rcrd, World::RefBactList &bct_lst)
{
    for( NC_STACK_ypabact * &bact : bct_lst )
    {
        if ( bact->_gid >= 0xFFFF || bact == _userRobo )
        {
            if ( rcrd->bacts_count < rcrd->bacts.size() )
            {
                rcrd->bacts[ rcrd->bacts_count ] = bact;
                rcrd->bacts_count++;
            }

            recorder_store_bact(rcrd, bact->_missiles_list);
            recorder_store_bact(rcrd, bact->_kidList);
        }
    }
}

void NC_STACK_ypaworld::recorder_world_to_frame(TGameRecorder *rcrd)
{
    rcrd->bacts_count = 0;
    recorder_store_bact(rcrd, _unitsList);

    std::sort(rcrd->bacts.begin(), rcrd->bacts.begin() + rcrd->bacts_count, TGameRecorder::BactSortCompare);

    for (uint32_t i = 0; i < rcrd->bacts_count; i++)
    {
        NC_STACK_ypabact *bact = rcrd->bacts[i];

        trec_bct *oinf = &rcrd->oinf[i];

        oinf->bact_id = bact->_gid;
        oinf->pos = bact->_position;

        vec3d euler = bact->_rotation.GetEuler();

        oinf->rot_x = dround(euler.x * 127.0 / C_2PI);
        oinf->rot_y = dround(euler.y * 127.0 / C_2PI);
        oinf->rot_z = dround(euler.z * 127.0 / C_2PI);

        NC_STACK_base *a4 = bact->GetVP();

        if ( a4 == bact->_vp_normal )
        {
            oinf->vp_id = 1;
        }
        else if ( a4 == bact->_vp_fire )
        {
            oinf->vp_id = 2;
        }
        else if ( a4 == bact->_vp_wait )
        {
            oinf->vp_id = 3;
        }
        else if ( a4 == bact->_vp_dead )
        {
            oinf->vp_id = 4;
        }
        else if ( a4 == bact->_vp_megadeth )
        {
            oinf->vp_id = 5;
        }
        else if ( a4 == bact->_vp_genesis )
        {
            oinf->vp_id = 6;
        }
        else
        {
            oinf->vp_id = 0;
        }

        if (bact->_bact_type == BACT_TYPES_MISSLE)
            oinf->objType = TGameRecorder::OBJ_TYPE_MISSILE;
        else
            oinf->objType = TGameRecorder::OBJ_TYPE_VEHICLE;

        oinf->vhcl_id = bact->_vehicleID;

        TGameRecorder::TSndState &ssnd = rcrd->sound_status[i];
        ssnd.active = 0;

        for (int j = 0; j < 16; j++)
        {
            if (bact->_soundcarrier.Sounds[j].IsEnabled() ||
                bact->_soundcarrier.Sounds[j].IsPFxEnabled() ||
                bact->_soundcarrier.Sounds[j].IsShkEnabled())
                ssnd.active |= 1 << j;
        }

        ssnd.pitch = bact->_soundcarrier.Sounds[0].Pitch;
    }
}

void recorder_pack_soundstates(TGameRecorder *rcrd)
{
    uint8_t *in = (uint8_t *)rcrd->sound_status.data();
    int in_pos = 0;

    uint8_t *output = (uint8_t *)rcrd->ainf.data();
    int out_pos = 0;

    int max_bytes_count = 4 * rcrd->bacts_count;

    while ( in_pos < max_bytes_count )
    {
        if ( in_pos >= max_bytes_count - 1 || in[in_pos] != in[in_pos + 1] )
        {
            int ctrl_byte_pos = out_pos;
            int cnt_bytes = 0;

            while (cnt_bytes < 0x80)
            {
                if ( in_pos >= max_bytes_count )
                    break;
                else if ( in_pos < max_bytes_count - 2 && in[in_pos] == in[in_pos + 1] && in[in_pos] == in[in_pos + 2] )
                    break;

                output[out_pos] = in[in_pos];
                in_pos++;
                out_pos++;

                cnt_bytes++;
            }

            output[ctrl_byte_pos] = cnt_bytes - 1;
        }
        else
        {
            int cnt_bytes = 0;

            uint8_t smplbyte = in[in_pos];
            while ( in_pos < max_bytes_count )
            {
                if ( in[in_pos] != smplbyte )
                    break;

                if ( cnt_bytes >= 0x80 )
                    break;

                in_pos++;
                cnt_bytes++;
            }
            output[out_pos] = 0x101 - cnt_bytes;
            output[out_pos + 1] = smplbyte;
            out_pos += 2;
        }
    }

    rcrd->ainf_size = out_pos;
}

void recorder_unpack_soundstates(TGameRecorder *rcrd)
{
    uint8_t *out = (uint8_t *)rcrd->sound_status.data();
    uint8_t *in = (uint8_t *)rcrd->ainf.data();
    uint8_t *in_end = ((uint8_t *)rcrd->ainf.data()) + rcrd->ainf_size;

    while ( in < in_end )
    {
        uint8_t bt = *in;
        in++;

        if ( bt > 0x80 )
        {
            for (int i = 0; i < 0x101 - bt; i++)
            {
                *out = *in;
                out++;
            }

            in++;
        }
        else if ( bt < 0x80 )
        {
            bt += 1;

            memcpy(out, in, bt);

            out += bt;
            in += bt;
        }
    }
}

void NC_STACK_ypaworld::recorder_write_frame()
{
    TGameRecorder *rcrd = _replayRecorder;

    if ( rcrd->field_40 < 0 )
    {
        recorder_world_to_frame(rcrd);
        rcrd->ctrl_bact_id = _userUnit->_gid;
        recorder_pack_soundstates(rcrd);


        int frame_size = 24;
        int oinf_size = 22 * rcrd->bacts_count;
        int v5 = 0;//16 * rcrd->field_34;

        if ( oinf_size )
        {
            frame_size = oinf_size + 32;

            if ( frame_size & 1 )
                frame_size++;
        }

        if ( rcrd->ainf_size )
        {
            frame_size += rcrd->ainf_size + 8;

            if ( frame_size & 1 )
                frame_size++;
        }

        if ( v5 )
        {
            frame_size += v5 + 8;

            if ( frame_size & 1 )
                frame_size++;
        }
        rcrd->mfile.pushChunk(TAG_FRAM, TAG_FORM, frame_size);
        rcrd->mfile.pushChunk(0, TAG_FINF, 12);

        rcrd->mfile.writeS32L(rcrd->frame_id);
        rcrd->mfile.writeS32L(rcrd->time);
        rcrd->mfile.writeU32L(rcrd->ctrl_bact_id);

        rcrd->mfile.popChunk();

        if ( oinf_size )
        {
            rcrd->mfile.pushChunk(0, TAG_OINF, oinf_size);

            for (uint32_t i = 0; i < rcrd->bacts_count; i++)
            {
                trec_bct *oinf = &rcrd->oinf[i];

                rcrd->mfile.writeU32L(oinf->bact_id);
                TF::Engine.Vec3dWriteIFF(oinf->pos, &rcrd->mfile, false);
                rcrd->mfile.writeS8(oinf->rot_x);
                rcrd->mfile.writeS8(oinf->rot_y);
                rcrd->mfile.writeS8(oinf->rot_z);
                rcrd->mfile.writeU8(oinf->vp_id);
                rcrd->mfile.writeU8(oinf->objType);
                rcrd->mfile.writeU8(oinf->vhcl_id);
            }

            rcrd->mfile.popChunk();
        }

        if ( rcrd->ainf_size )
        {
            rcrd->mfile.pushChunk(0, TAG_AINF, rcrd->ainf_size);
            rcrd->mfile.write(rcrd->ainf.data(), rcrd->ainf_size);
            rcrd->mfile.popChunk();
        }

        if ( v5 )
        {
        }

        rcrd->mfile.popChunk();

        rcrd->field_40 += 250;
        rcrd->frame_id += 1;
    }
}

int recorder_open_replay(TGameRecorder *rcrd)
{
    rcrd->mfile = IFFile( uaOpenFile(rcrd->filename, "rb") );

    if ( !rcrd->mfile.OK() )
    {
        return 0;
    }

    if ( rcrd->mfile.parse() != IFFile::IFF_ERR_OK )
    {
        rcrd->mfile.close();
        return 0;
    }

    if ( rcrd->mfile.GetCurrentChunk().Is(TAG_FORM, TAG_SEQN) )
        return 1;

    return 0;
}

bool NC_STACK_ypaworld::recorder_create_camera()
{
    NC_STACK_ypabact *bacto = Nucleus::CInit<NC_STACK_ypabact>( {{NC_STACK_ypabact::BACT_ATT_WORLD, this}} );

    if ( !bacto )
        return false;

    bacto->Renew();

    bacto->_gid = 0;
    bacto->_owner = 1;

    bacto->_rotation = mat3x3::Ident();

    bacto->_soundcarrier.Clear();

    ypaworld_func134(bacto);

    bacto->setBACT_viewer(true);
    bacto->setBACT_inputting(true);

    _userRobo = bacto;

    TF::Engine.SetViewPoint(&bacto->_tForm);

    return true;
}

void recorder_read_framedata(TGameRecorder *rcrd)
{
    while ( rcrd->mfile.parse() != IFFile::IFF_ERR_EOC )
    {
        const IFFile::Context &v3 = rcrd->mfile.GetCurrentChunk();

        switch ( v3.TAG )
        {
        case TAG_FLSH:
            rcrd->field_78 |= 1;
            rcrd->mfile.parse();
            break;

        case TAG_FINF:
            rcrd->frame_id = rcrd->mfile.readS32L();
            rcrd->time = rcrd->mfile.readS32L();
            rcrd->ctrl_bact_id = rcrd->mfile.readU32L();
            rcrd->mfile.parse();
            break;

        case TAG_OINF:
        {
            rcrd->bacts_count = v3.TAG_SIZE / 22;

            for (uint32_t i = 0; i < rcrd->bacts_count; i++)
            {
                trec_bct *oinf = &rcrd->oinf[i];

                oinf->bact_id = rcrd->mfile.readU32L();
                TF::Engine.Vec3dReadIFF(&oinf->pos, &rcrd->mfile, false);
                oinf->rot_x = rcrd->mfile.readS8();
                oinf->rot_y = rcrd->mfile.readS8();
                oinf->rot_z = rcrd->mfile.readS8();

                oinf->vp_id = rcrd->mfile.readU8();
                oinf->objType = rcrd->mfile.readU8();
                oinf->vhcl_id = rcrd->mfile.readU8();
            }

            rcrd->mfile.parse();
        }
        break;

        case TAG_AINF:
            rcrd->mfile.read(rcrd->ainf.data(), v3.TAG_SIZE);
            rcrd->ainf_size = v3.TAG_SIZE;

            recorder_unpack_soundstates(rcrd);

            rcrd->mfile.parse();
            break;

        case TAG_MODE:

            rcrd->mfile.skipChunk();
            break;

        default:
            rcrd->mfile.skipChunk();
            break;
        }
    }
}

NC_STACK_ypabact *NC_STACK_ypaworld::recorder_newObject(trec_bct *oinf)
{
    NC_STACK_ypabact *bacto = nullptr;

    if ( oinf->objType == TGameRecorder::OBJ_TYPE_VEHICLE )
    {
        if ( oinf->vhcl_id )
        {
            ypaworld_arg146 arg146;
            arg146.vehicle_id = oinf->vhcl_id;
            arg146.pos = vec3d(0.0, 0.0, 0.0);

            World::TVhclProto *prot = &_vhclProtos[ oinf->vhcl_id ];

            int v6 = prot->model_id;

            prot->model_id = BACT_TYPES_BACT;

            bacto = ypaworld_func146(&arg146);

            _vhclProtos[oinf->vhcl_id].model_id = v6;
        }
        else
        {

            bacto = Nucleus::CInit<NC_STACK_ypabact>( {{NC_STACK_ypabact::BACT_ATT_WORLD, this}} );
            if ( bacto )
            {
                bacto->Renew();

                bacto->_gid = 0;
                bacto->_owner = 1;

                bacto->_rotation = mat3x3::Ident();
            }
        }
    }
    else
    {
        ypaworld_arg146 arg147;
        arg147.vehicle_id = oinf->vhcl_id;
        arg147.pos = vec3d(0.0, 0.0, 0.0);

        bacto = ypaworld_func147(&arg147);
    }

    if ( bacto )
    {
        bacto->_kidRef.Detach();

        bacto->_gid = oinf->bact_id;
        bacto->_host_station = (NC_STACK_yparobo *)_userUnit;
        bacto->_parent = _userUnit;
    }

    return bacto;
}

void NC_STACK_ypaworld::recorder_set_bact_pos(NC_STACK_ypabact *bact, const vec3d &pos)
{
    yw_130arg arg130;
    arg130.pos_x = pos.x;
    arg130.pos_z = pos.z;

    if ( GetSectorInfo(&arg130) )
    {
        if ( bact->_pSector )
            bact->_cellRef.Detach();

        bact->_cellRef = arg130.pcell->unitsList.push_back(bact);

        bact->_pSector = arg130.pcell;
        bact->_old_pos = bact->_position;
        bact->_position = pos;
        bact->_cellId = arg130.CellId;
    }
}

void NC_STACK_ypaworld::recorder_updateObject(NC_STACK_ypabact *bact, trec_bct *oinf, TGameRecorder::TSndState *ssnd, float a5, float a6)
{
    vec3d bct_pos;
    bct_pos = (oinf->pos - bact->_position) * a5 + bact->_position;

    recorder_set_bact_pos(bact, bct_pos);

    bact->_fly_dir = bact->_position - bact->_old_pos;

    float ln = bact->_fly_dir.length();
    if ( ln > 0.0 )
    {
        bact->_fly_dir /= ln;

        if ( a6 <= 0.0 )
            bact->_fly_dir_length = 0;
        else
            bact->_fly_dir_length = (ln / a6) / 6.0;
    }
    else
    {
        bact->_fly_dir = vec3d::OX(1.0);

        bact->_fly_dir_length = 0;
    }

    mat3x3 tmp = mat3x3::Euler( vec3d(oinf->rot_x, oinf->rot_y, oinf->rot_z) / 127.0 * C_2PI );

    vec3d axisX = (tmp.AxisX() - bact->_rotation.AxisX()) * a5 + bact->_rotation.AxisX();

    if ( axisX.normalise() == 0.0 )
        axisX = vec3d::OX(1.0);

    vec3d axisY = (tmp.AxisY() - bact->_rotation.AxisY()) * a5 + bact->_rotation.AxisY();

    if ( axisY.normalise() == 0.0 )
        axisY = vec3d::OY(1.0);

    vec3d axisZ = (tmp.AxisZ() - bact->_rotation.AxisZ()) * a5 + bact->_rotation.AxisZ();

    if ( axisZ.normalise() == 0.0 )
        axisZ = vec3d::OZ(1.0);

    bact->_rotation = mat3x3::Basis(axisX, axisY, axisZ);

    switch ( oinf->vp_id )
    {
    case 1:
        bact->SetVP(bact->_vp_normal);
        break;

    case 2:
        bact->SetVP(bact->_vp_fire);
        break;

    case 3:
        bact->SetVP(bact->_vp_wait);
        break;

    case 4:
        bact->SetVP(bact->_vp_dead);
        break;

    case 5:
        bact->SetVP(bact->_vp_megadeth);
        break;

    case 6:
        bact->SetVP(bact->_vp_genesis);
        break;

    default:
        break;
    }

    for(int i = 0; i < 16; i++)
    {
        int v48 = 1 << i;
        if ( v48 & ssnd->active )
        {
            if ( !(bact->_soundFlags & v48) )
            {
                bact->_soundFlags |= v48;
                SFXEngine::SFXe.startSound(&bact->_soundcarrier, i);
            }
        }
        else
        {
            if ( bact->_soundFlags & v48 )
            {
                bact->_soundFlags &= ~v48;

                if ( bact->_soundcarrier.Sounds[i].IsLoop() )
                    SFXEngine::SFXe.sub_424000(&bact->_soundcarrier, i);
            }
        }
    }

    // Replay owns the recorded runtime pitch for SND_NORMAL. startSound() may
    // roll an authored range while restoring active channels, so reapply the
    // captured value after channel state has been rebuilt.
    bact->_soundcarrier.Sounds[0].Pitch = ssnd->pitch;
    bact->_soundcarrier.Sounds[0].PitchBase = ssnd->pitch;
}

void NC_STACK_ypaworld::recorder_updateObjectList(TGameRecorder *rcrd, float a5, int period)
{
    float fperiod = period / 1000.0;
    World::RefBactList::iterator it = _userUnit->_kidList.begin();

    uint32_t i = 0;

    while ( i < rcrd->bacts_count )
    {
        trec_bct *oinf = &rcrd->oinf[i];
        TGameRecorder::TSndState &ssnd = rcrd->sound_status[i];

        if ( it != _userUnit->_kidList.end() )
        {
            NC_STACK_ypabact *bact = *it;

            if ( oinf->bact_id > bact->_gid )
            {
                it++;

                ypaworld_func144(bact);
            }
            else if ( oinf->bact_id < bact->_gid )
            {
                NC_STACK_ypabact *v10 = recorder_newObject(oinf);

                if ( v10 )
                {
                    recorder_updateObject(v10, oinf, &ssnd, 1.0, fperiod);

                    v10->_kidRef = _userUnit->_kidList.insert(it, v10);

                    i++;
                }
            }
            else // ==
            {
                recorder_updateObject(bact, oinf, &ssnd, a5, fperiod);
                it++;

                i++;
            }
        }
        else
        {
            NC_STACK_ypabact *v13 = recorder_newObject(oinf);

            if ( v13 )
            {
                recorder_updateObject(v13, oinf, &ssnd, 1.0, fperiod);

                v13->_kidRef = _userUnit->_kidList.push_back(v13);
                it = v13->_kidRef;
                it++;

                i++;
            }
        }
    }

    while ( it != _userUnit->_kidList.end() )
    {
        NC_STACK_ypabact *bact = *it;
        it++;

        ypaworld_func144(bact);
    }
}

int NC_STACK_ypaworld::recorder_go_to_frame(TGameRecorder *rcrd, int wanted_frame_id)
{
    int frame_id = wanted_frame_id;
    int cur_frame_id = 0;

    if ( frame_id >= 0 )
    {
        if ( frame_id >= rcrd->field_74 )
            frame_id = rcrd->field_74 - 1;
    }
    else
    {
        frame_id = 0;
    }

    rcrd->mfile.close();

    if ( recorder_open_replay(rcrd) )
    {
        while ( rcrd->mfile.parse() != IFFile::IFF_ERR_EOC )
        {
            if ( rcrd->mfile.GetCurrentChunk().Is(TAG_FORM, TAG_FRAM) )
            {
                if ( cur_frame_id == frame_id )
                {
                    recorder_read_framedata(rcrd);

                    _timeStamp = rcrd->time;

                    recorder_updateObjectList(rcrd, 1.0, 0);
                    return 1;
                }

                cur_frame_id++;
                rcrd->mfile.skipChunk();
            }
            else
            {
                rcrd->mfile.skipChunk();
            }
        }
    }
    return 0;
}
