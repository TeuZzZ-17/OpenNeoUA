// AI layers, target assessment, path finding, group organisation and combat decisions.
#include "game/ypabact_internal.h"

float ypabact_GetAiMaxAltitudeAboveGround()
{
    static const float altitude = []()
    {
        float parsed = ypabact_ReadNonNegativeFloatIni(
            System::IniConf::GameAiMaxAltitudeAboveGround, 0.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return 0.0f;

        return parsed;
    }();

    return altitude;
}

const TAiTargetRangeConfig &ypabact_GetAiTargetRangeConfig()
{
    static const TAiTargetRangeConfig config = []()
    {
        constexpr float VanillaRange = 1800.0f;
        constexpr float MaxRange = 2160.0f;

        float parsed = ypabact_ReadNonNegativeFloatIni(System::IniConf::GameAiTargetRange, 0.0f);
        if ( !isfinite(parsed) || parsed <= 0.0f )
            return TAiTargetRangeConfig{VanillaRange, false};

        return TAiTargetRangeConfig{std::min(parsed, MaxRange), true};
    }();

    return config;
}

bool ypabact_HasEnemyNearby(NC_STACK_ypabact *carrier, float radius)
{
    NC_STACK_ypaworld *world = carrier->getBACT_pWorld();
    if ( !world )
        return false;

    if ( radius <= 0.0 )
        return false;

    float radiusSq = radius * radius;
    int sectorRadius = (int)(radius / World::CVSectorLength) + 2;
    Common::Point center = World::PositionToSectorID(carrier->_position);

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);

            if ( !world->IsSector(cellId) )
                continue;

            cellArea &cell = world->SectorAt(cellId);

            for (NC_STACK_ypabact *unit : cell.unitsList)
            {
                if ( !ypabact_IsCarrierSpawnEnemy(carrier, unit) )
                    continue;

                if ( (unit->_position.XZ() - carrier->_position.XZ()).square() <= radiusSq )
                    return true;
            }
        }
    }

    return false;
}

void NC_STACK_ypabact::AI_layer1(update_msg *arg)
{
    setTarget_msg v36;

    if ( _mass == 1.0 )
    {
        if ( _status_flg & BACT_STFLAG_DEATH2 )
        {
            _yls_time -= arg->frameTime;

            if ( _yls_time < 0 )
                _world->ypaworld_func144(this);
        }
        else
        {
            setState_msg v37;
            v37.newStatus = BACT_STATUS_NOPE;
            v37.unsetFlags = 0;
            v37.setFlags = BACT_STFLAG_DEATH2;

            SetState(&v37);

            _yls_time = 6000;
        }
        return;
    }

    if ( _bact_type != BACT_TYPES_MISSLE )
        EnergyInteract(arg);

    if ( _status == BACT_STATUS_DEAD )
    {
        if ( _status_flg & BACT_STFLAG_LAND )
            _yls_time -= arg->frameTime;
    }

    _airconst = _airconst_static;

    _soundcarrier.Sounds[0].Pitch = _soundcarrier.Sounds[0].PitchBase;
    _soundcarrier.Sounds[0].Volume = _volume;

    if ( _clock - _AI_time1 < 250 ||
         _primTtype == BACT_TGT_TYPE_DRCT ||
         _bact_type == BACT_TYPES_GUN ||
         _status == BACT_STATUS_DEAD ||
         _status == BACT_STATUS_BEAM ||
         _status == BACT_STATUS_CREATE )
    {
        AI_layer2(arg);
        return;
    }

    _AI_time1 = _clock;
    _target_vec = vec3d(0.0, 0.0, 0.0);

    if ( _clock - _brkfr_time > 5000 )
    {
        _brkfr_time = _clock;

        StuckFree(arg);
    }

    // Keep the common timing/stuck bookkeeping above, but do not let the
    // directly controlled unit execute AI target propagation or order recovery.
    // AI_layer2 routes it straight to the class-specific User_layer.
    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        AI_layer2(arg);
        return;
    }

    if ( _status == BACT_STATUS_NORMAL && _primTtype != BACT_TGT_TYPE_NONE )
    {
        if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            _target_vec = _primT.pbact->_position - _position;

            if ( _primT.pbact->_status != BACT_STATUS_DEAD)
                _primTpos = _primT.pbact->_position;
        }
        else
        {
            _target_vec = _primTpos - _position;
        }

        if ( _target_vec.length() > 2000.0 )
            _target_vec.y = 0;

        if ( IsParentMyRobo() &&
             (_oflags & BACT_OFLAG_VIEWER) )
        {
            bool doFight = _target_vec.length() < 800.0;

            int unitId = 0;
            for (NC_STACK_ypabact* &node : _kidList)
            {
                if ( node->_status != BACT_STATUS_DEAD )
                {
                    if ( doFight )
                    {
                        v36.tgt_type = _primTtype;
                        v36.priority = 0;
                        v36.tgt.pbact = _primT.pbact;
                        v36.tgt_pos = _primTpos;
                    }
                    else
                    {
                        bact_arg94 v35;
                        v35.field_0 = unitId;
                        GetFormationPosition(&v35);

                        v36.tgt_type = BACT_TGT_TYPE_FRMT;
                        v36.priority = 0;
                        v36.tgt_pos = v35.pos1;
                    }

                    node->SetTarget(&v36);
                }
                unitId++;
            }
        }
    }

    if ( _primTtype == BACT_TGT_TYPE_NONE)
    {
        if ( _host_station && _primT_cmdID )
        {
            v36.priority = _primT_cmdID;

            if ( _host_station->yparobo_func132(&v36) )
            {
                v36.priority = 0;
            }
            else
            {
                _primT_cmdID = 0;

                v36.tgt_type = BACT_TGT_TYPE_CELL;
                v36.priority = 0;
                v36.tgt_pos = _primTpos;
            }

            SetTarget(&v36);
        }
    }

    if ( _vp_active == 6 && _status == BACT_STATUS_NORMAL )
    {
        setState_msg v38;
        v38.newStatus = BACT_STATUS_NORMAL;
        v38.setFlags = 0;
        v38.unsetFlags = 0;
        SetState(&v38);
    }

    AI_layer2(arg);
}

void NC_STACK_ypabact::AI_layer2(update_msg *arg)
{
    // The directly controlled unit normally uses player targeting and input.
    // Bypass opportunistic enemy scans, AI speech/engagement messages and AI
    // secondary-target assignment; only the active single-player stun
    // movement controller below temporarily replaces direct driving.
    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        // OpenNeoUA debuff stun: while movement stunation is active, the
        // directly controlled unit temporarily runs the same erratic movement
        // controller already used by AI units. This keeps one shared source of
        // truth for phases, traction and floor safety instead of duplicating the
        // behavior inside every vehicle-specific User_layer().
        const bool playerMovementStunEnabled = _world && !_world->_isNetGame;
        if ( playerMovementStunEnabled && IsActiveDebuffStunning() )
        {
            ReleaseHandBrake();
            _world->ResetPlayerSprint();
            RunAIWithActiveDebuffStun(arg);
        }
        else
        {
            // Fire-only stun keeps normal steering while the shared fire policy
            // applies to the directly controlled unit too. Random movement stays
            // single-player-only because that path has not been network-audited.
            UpdateActiveDebuffStunFire(arg);
            ypabact_RunPlayerUserLayer(this, arg);
        }
        return;
    }

    constexpr float CurSectrLength = 1.05 * World::CVSectorLength;

    if ( (_clock - _AI_time2) < 250
       || _owner == 0
       || _secndTtype == BACT_TGT_TYPE_DRCT
       || _status == BACT_STATUS_CREATE
       || _status == BACT_STATUS_DEAD
       || _status == BACT_STATUS_BEAM )
    {
        RunAIWithActiveDebuffStun(arg);
        return;
    }

    _AI_time2 = _clock;

    if ( _status_flg & BACT_STFLAG_ESCAPE &&
         _target_vec.XZ().length() > 3600.0 )
    {
        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_NONE;
        arg67.priority = 1;

        for(NC_STACK_ypabact* &nod : _kidList)
            nod->SetTarget(&arg67);

        SetTarget(&arg67);

        RunAIWithActiveDebuffStun(arg);
        return;
    }

    NC_STACK_ypabact *wee = _world->getYW_userHostStation();

    if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
    {
        if ( _clock - _search_time1 > 500 )
        {
            _search_time1 = _clock;

            NC_STACK_ypabact *enemy = GetSectorTarget(_cellId);
            if ( enemy )
            {
                if ( enemy->_bact_type != BACT_TYPES_ROBO && IsParentMyRobo() && _host_station == wee && enemy->_commandID != _fe_cmdID && _clock - _fe_time > 45000 )
                {
                    bool isRoboGun = false;
                    if ( enemy->_bact_type == BACT_TYPES_GUN )
                    {
                        NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( enemy );
                        isRoboGun = gun->IsRoboGun();
                    }

                    if ( !isRoboGun )
                    {
                        _fe_cmdID = enemy->_commandID;
                        _fe_time = _clock;

                        robo_arg134 arg134;
                        arg134.field_4 = 7;
                        arg134.field_8 = enemy->_commandID;
                        arg134.field_C = 0;
                        arg134.field_10 = 0;
                        arg134.unit = this;
                        arg134.field_14 = 46;

                        _host_station->placeMessage(&arg134);
                    }
                }
            }

            if ( _status == BACT_STATUS_IDLE ||
                 (  _aggr >= 50 &&
                  !(_status_flg & BACT_STFLAG_ESCAPE) &&
                     (_secndTtype == BACT_TGT_TYPE_NONE ||
                      _secndTtype == BACT_TGT_TYPE_CELL ||
                      _secndTtype == BACT_TGT_TYPE_FRMT) ) )
            {
                if ( enemy )
                {
                    _secndT_cmdID = enemy->_commandID;

                    setTarget_msg arg67;
                    arg67.tgt_type = BACT_TGT_TYPE_UNIT;
                    arg67.priority = 1;
                    arg67.tgt.pbact = enemy;

                    SetTarget(&arg67);
                }

                if ( (_clock - _search_time2) > 2000 &&
                     _aggr == 75 &&
                    !(_oflags & BACT_OFLAG_VIEWER) &&
                     IsParentMyRobo() &&
                     (_secndTtype == BACT_TGT_TYPE_FRMT ||
                      _secndTtype == BACT_TGT_TYPE_NONE) )
                {
                    if (  _position.x > CurSectrLength &&
                          _position.x < _wrldSize.x + -CurSectrLength &&
                          _position.z < -CurSectrLength &&
                          _position.z > _wrldSize.y + CurSectrLength )
                    {
                        _search_time2 = _clock;

                        if ( _owner != _pSector->owner )
                        {
                            setTarget_msg arg67;
                            arg67.priority = 1;
                            arg67.tgt_type = BACT_TGT_TYPE_CELL;
                            arg67.tgt_pos.x = _position.x;
                            arg67.tgt_pos.z = _position.z;

                            SetTarget(&arg67);
                        }
                    }
                }

                if ( IsParentMyRobo() && _secndTtype == BACT_TGT_TYPE_CELL )
                {
                    for(NC_STACK_ypabact* &nod : _kidList)
                    {
                        if ( nod->_secndTtype == BACT_TGT_TYPE_NONE || nod->_secndTtype == BACT_TGT_TYPE_FRMT )
                        {
                            setTarget_msg arg67;
                            arg67.tgt_type = BACT_TGT_TYPE_CELL;
                            arg67.tgt_pos = _sencdTpos;
                            arg67.priority = 1;
                            nod->SetTarget(&arg67);
                        }
                    }
                }
            }
        }

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
            _target_vec = _secndT.pbact->_position - _position;
        else if ( _secndTtype == BACT_TGT_TYPE_CELL)
            _target_vec = _sencdTpos - _position;

        if ( _target_vec.length() > 2000.0 )
            _target_vec.y = 0;
    }

    RunAIWithActiveDebuffStun(arg);
}

void AI_layer3__sub1(NC_STACK_ypabact *bact, update_msg *arg)
{
    bact->_thraction = bact->_force;

    float v39 = arg->frameTime / 1000.0;

    float top = -bact->_target_dir.y;

    if ( top == 1.0 )
        top = 0.99998999;

    if ( top == -1.0 )
        top = -0.99998999;

    float weight = bact->_mass * 9.80665;
    float thraction = bact->_thraction;

    if ( thraction == 0.0 )
        thraction = 0.1;

    float v5 = sqrt( (1.0 - POW2(top)) ) * (top * -0.5);

    float v6 = (1.0 - 0.25 * POW2(top) + 0.25 * POW2(top) * POW2(top)) * (POW2(weight) / POW2(thraction));

    float v58 = sqrt( (1.0 - v6) ) + (weight * v5 / thraction);

    vec3d tmp;
    tmp.y = -cos( clp_asin(v58) );

    if ( bact->_target_dir.z != 0.0 )
    {
        float v62 = (1.0 - POW2(tmp.y)) / (POW2(bact->_target_dir.x) / POW2(bact->_target_dir.z) + 1.0);

        if ( v62 < 0.0 )
            v62 = 0.0;

        tmp.z = sqrt(v62);

        if ( bact->_target_dir.z < 0.0 )
            tmp.z = -tmp.z;
    }
    else
    {
        tmp.z = 0.0;
    }

    if ( bact->_target_dir.x != 0.0 )
    {
        float v57 = (1.0 - POW2(tmp.y)) / (POW2(bact->_target_dir.z) / POW2(bact->_target_dir.x) + 1.0);

        if ( v57 < 0.0 )
            v57 = 0.0;

        tmp.x = sqrt(v57);

        if ( bact->_target_dir.x < 0.0 )
            tmp.x = -tmp.x;
    }
    else
    {
        tmp.x = 0.0;
    }

    vec3d vaxis = (-bact->_rotation.AxisY()) * tmp;;

    if ( vaxis.normalise() != 0.0 )
    {
        float maxrot = bact->_maxrot * v39;

        float v56 = clp_acos( tmp.dot( -bact->_rotation.AxisY() ) );

        if ( v56 > maxrot )
            v56 = maxrot;

        if ( fabs(v56) > BACT_MIN_ANGLE )
            bact->_rotation *= mat3x3::AxisAngle(vaxis, v56);
    }
}

void AI_layer3__sub0(NC_STACK_ypabact *bact, int a2)
{
    if ( clp_acos(bact->_rotation.m11) > 0.003 && (bact->_fly_dir.z != 0.0 || bact->_fly_dir.x != 0.0) && bact->_fly_dir_length > 0.0 )
    {
        float v11 = a2 / 1000.0;

        vec2d flydir = bact->_fly_dir.XZ();
        vec2d axisZ = bact->_rotation.AxisZ().XZ();

        float tmpsq = flydir.length();
        float v18 = 0.0;

        if ( isnormal(tmpsq) ) // Not nullptr, NAN, INF
            v18 = flydir.dot(axisZ) / tmpsq;

        tmpsq = axisZ.length();

        if ( isnormal(tmpsq) ) // Not nullptr, NAN, INF
            v18 /= tmpsq;
        else
            v18 = 0.0;

        float v20 = clp_acos( v18 );

        float v13 = bact->_maxrot * v11 * (fabs(v20) * 0.8 + 0.2);

        if ( v20 > v13 )
            v20 = v13;

        if ( bact->_fly_dir.x * bact->_rotation.m22 - bact->_rotation.m20 * bact->_fly_dir.z < 0.0 )
            v20 = -v20;

        bact->_rotation *= mat3x3::RotateY(-v20);
    }
}

void NC_STACK_ypabact::AI_layer3(update_msg *arg)
{
    float v75 = arg->frameTime / 1000.0;

    const bool kamikazeRamming = ApplyKamikazeRammingGuidance();

    float v77 = _target_vec.length();

    if ( v77 > 0.0 )
        _target_dir = _target_vec / v77;

    if ( IsActiveDebuffStunning() )
    {
        UpdateActiveDebuffStunMoveIntent();
        v77 = 1200.0f;
    }

    int v82 = _oflags & BACT_OFLAG_VIEWER;
    int v70 = _oflags & BACT_OFLAG_EXACTCOLL;

    int v80 = _world->ypaworld_func145(this);

    int v79;

    if ( v82 )
        v79 = _viewer_radius;
    else
        v79 = _radius;

    switch ( _status )
    {
    case BACT_STATUS_NORMAL:
    {
        if ( _oflags & BACT_OFLAG_BACTCOLL )
        {
            if ( (v80 || (_secndTtype == BACT_TGT_TYPE_NONE && v77 < World::CVSectorLength)) && !(_status_flg & BACT_STFLAG_LAND) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        if ( !_primTtype && !_secndTtype && !IsActiveDebuffStunning() )
        {
            _status = BACT_STATUS_IDLE;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg arg78;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.setFlags = 0;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
            break;
        }

        ypaworld_arg136 arg136;

        arg136.isect = false;
        arg136.stPos = _old_pos;
        arg136.vect = _position - _old_pos;
        arg136.vect.y = 0;

        float len = arg136.vect.length();

        if ( len > 0.0 )
            arg136.vect *= 300.0 / len;
        else
            arg136.vect = _rotation.AxisZ() * 300.0;

        arg136.isect = false;
        arg136.flags = 0;

        ypaworld_arg136 arg136_1;
        arg136_1.isect = false;
        arg136_1.flags = 0;

        if ( v82 || (_status_flg & BACT_STFLAG_DODGE_RIGHT) || (v80 && v70) )
        {
            arg136_1.stPos = _old_pos;
            arg136_1.vect.x = arg136.vect.x * 0.93969 - arg136.vect.z * 0.34202;
            arg136_1.vect.y = arg136.vect.y;
            arg136_1.vect.z = arg136.vect.z * 0.93969 + arg136.vect.x * 0.34202;

            TraceWorldCollision(&arg136_1);
        }

        ypaworld_arg136 arg136_2;
        arg136_2.isect = false;
        arg136_2.flags = 0;

        if ( v82 || (_status_flg & BACT_STFLAG_DODGE_LEFT) || (v80 && v70) )
        {
            arg136_2.stPos = _old_pos;
            arg136_2.vect.x = arg136.vect.x * 0.93969 + arg136.vect.z * 0.34202;
            arg136_2.vect.y = arg136.vect.y;
            arg136_2.vect.z = arg136.vect.z * 0.93969 - arg136.vect.x * 0.34202;

            TraceWorldCollision(&arg136_2);
        }

        if ( v82 || !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) || (v80 && v70) )
            TraceWorldCollision(&arg136);

        int v18 = 0;

        bact_arg88 arg88;
        arg88.pos1 = vec3d(0.0, 0.0, 0.0);

        if ( arg136.isect )
        {
            if ( len + v79 > arg136.tVal * 300.0 )
            {
                arg88.pos1 = arg136.skel->polygons[arg136.polyID].Normal();
                v18++;
            }
        }

        if ( arg136_1.isect )
        {
            if ( len + v79 > arg136_1.tVal * 300.0 )
            {
                arg88.pos1 += arg136_1.skel->polygons[arg136_1.polyID].Normal();
                v18++;
            }
        }

        if ( arg136_2.isect )
        {
            if ( len + v79 > arg136_2.tVal * 300.0 )
            {
                arg88.pos1 += arg136_2.skel->polygons[arg136_2.polyID].Normal();
                v18++;
            }
        }

        if ( !arg136.isect && !arg136_1.isect && !arg136_2.isect )
        {
            _status_flg &= ~(BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT | BACT_STFLAG_MOVE);
            _status_flg |= BACT_STFLAG_MOVE;
        }

        if ( !(_status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT)) )
        {

            if ( arg136_1.isect == 1 && arg136_2.isect == 1 )
            {
                if ( arg136_1.tVal >= arg136_2.tVal )
                    _status_flg |= BACT_STFLAG_DODGE_LEFT;
                else
                    _status_flg |= BACT_STFLAG_DODGE_RIGHT;
            }

            if ( arg136_1.isect == 1 && !arg136_2.isect )
                _status_flg |= BACT_STFLAG_DODGE_RIGHT;

            if ( !arg136_1.isect && arg136_2.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;

            if ( !arg136_1.isect && !arg136_2.isect && arg136.isect == 1 )
                _status_flg |= BACT_STFLAG_DODGE_LEFT;
        }

        float v21 = _mass * 9.80665;

        if ( v21 <= _force )
            v21 = _force;

        float v88;

        if ( _airconst >= 10.0 )
            v88 = _airconst;
        else
            v88 = 10.0;


        ypaworld_arg136 arg136_3;

        arg136_3.vect.x = (_fly_dir.x * 200.0 * _fly_dir_length) / (v21 / v88);

        if ( arg136_3.vect.x < -200.0 )
            arg136_3.vect.x = -200.0;

        if ( arg136_3.vect.x > 200.0 )
            arg136_3.vect.x = 200.0;

        arg136_3.vect.y = _height;

        arg136_3.vect.z = (_fly_dir.z * 200.0 * _fly_dir_length) / (v21 / v88);

        if ( arg136_3.vect.z < -200.0 )
            arg136_3.vect.z = -200.0;

        if ( arg136_3.vect.z > 200.0 )
            arg136_3.vect.z = 200.0;

        arg136_3.stPos = _old_pos;
        arg136_3.flags = 0;

        TraceWorldCollision(&arg136_3);

        if ( arg136_3.isect )
        {
            _target_dir.y = -(1.0 - arg136_3.tVal);
        }
        else
        {
            if ( (!kamikazeRamming && !HasLocalPlayerForceVerticalPursuitTarget()) ||
                 _target_dir.y >= -0.01 )
            {
                if ( _target_dir.y < 0.15 )
                    _target_dir.y = 0.15;
            }
        }

        if ( _status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT) )
            _target_dir.y = -0.2;

        if ( arg136_3.isect )
        {
            if ( arg136_3.tVal * _height < _radius && _fly_dir.y > 0.0 )
            {
                arg88.pos1 += arg136_3.skel->polygons[arg136_3.polyID].Normal();

                v18++;
            }
        }

        if ( v18 )
        {
            float v29 = v18;

            arg88.pos1 /= v29;

            Recoil(&arg88);
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_LCRASH;
        }

        if ( _target_dir.y != 0.0 )
            _target_dir.normalise();

        float tmpsq = arg136.vect.XZ().length();
        if (isnormal(tmpsq)) // not nullptr, NAN, INF
        {
            if ( _status_flg & BACT_STFLAG_DODGE_LEFT )
            {
                _target_dir.x = -arg136.vect.z / tmpsq;
                _target_dir.z = arg136.vect.x / tmpsq;
            }
            else if ( _status_flg & BACT_STFLAG_DODGE_RIGHT )
            {
                _target_dir.x = arg136.vect.z / tmpsq;
                _target_dir.z = -arg136.vect.x / tmpsq;
            }
        }
        else // emulate watcom div 0.0
        {
            if ( _status_flg & (BACT_STFLAG_DODGE_LEFT | BACT_STFLAG_DODGE_RIGHT) )
            {
                _target_dir.x = 0.0;
                _target_dir.z = 0.0;
            }
        }

        if ( !kamikazeRamming )
            ApplyAiMaxAltitudeAboveGround();

        AI_layer3__sub1(this, arg);

        /*if ( bact->status_flg & BACT_STFLAG_DODGE ) //Unused flag
            bact->fly_dir_length *= 0.95;*/

        _thraction = (0.85 - _target_dir.y) * _force;
        _thraction = GetActiveDebuffStunTraction(_thraction, false);

        move_msg arg74;
        arg74.flag = 0;
        arg74.field_0 = arg->frameTime / 1000.0;

        Move(&arg74);

        AI_layer3__sub0(this, arg->frameTime);

        bact_arg75 arg75;

        arg75.fperiod = v75;
        arg75.g_time = _clock;

        if ( _secndTtype == BACT_TGT_TYPE_UNIT )
        {
            arg75.target.pbact = _secndT.pbact;
            arg75.prio = 1;

            FightWithBact(&arg75);
        }
        else if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            arg75.pos = _sencdTpos;
            arg75.target.pcell = _secndT.pcell;
            arg75.prio = 1;

            FightWithSect(&arg75);
        }
        else if ( _primTtype == BACT_TGT_TYPE_UNIT )
        {
            arg75.target.pbact = _primT.pbact;
            arg75.prio = 0;

            FightWithBact(&arg75);
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            arg75.pos = _primTpos;
            arg75.target.pcell = _primT.pcell;
            arg75.prio = 0;

            FightWithSect(&arg75);
        }
        else
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg arg78;
                arg78.unsetFlags = BACT_STFLAG_FIRE;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.setFlags = 0;

                SetState(&arg78);
            }

            _status_flg &= ~(BACT_STFLAG_FIGHT_P | BACT_STFLAG_FIGHT_S);
        }
    }
    break;

    case BACT_STATUS_DEAD:
        DeadTimeUpdate(arg);
        break;

    case BACT_STATUS_IDLE:

        if ( _clock - _newtarget_time > 500 )
        {
            _newtarget_time = _clock;

            bact_arg110 arg110;
            arg110.tgType = _secndTtype;
            arg110.priority = 1;

            int v46 = TargetAssess(&arg110);

            arg110.priority = 0;
            arg110.tgType = _primTtype;
            int v48 = TargetAssess(&arg110);

            if ( v46 != TA_IGNORE || v48 != TA_IGNORE )
            {
                setTarget_msg arg67;

                if ( v46 == TA_CANCEL )
                {
                    arg67.tgt_type = BACT_TGT_TYPE_NONE;
                    arg67.priority = 1;
                    SetTarget(&arg67);
                }

                if ( v48 == TA_CANCEL )
                {
                    arg67.tgt_type = BACT_TGT_TYPE_CELL;
                    arg67.tgt_pos.x = _position.x;
                    arg67.tgt_pos.z = _position.z;
                    arg67.priority = 0;
                    SetTarget(&arg67);
                }

                if ( _primTtype || _secndTtype )
                {
                    setState_msg arg78;
                    arg78.unsetFlags = BACT_STFLAG_LAND;
                    arg78.setFlags = 0;
                    arg78.newStatus = BACT_STATUS_NORMAL;
                    SetState(&arg78);
                    break;
                }
            }
        }

        if ( _oflags & BACT_OFLAG_LANDONWAIT )
        {
            _thraction = 0;

            if ( _status_flg & BACT_STFLAG_LAND )
            {
                setState_msg arg78;
                arg78.unsetFlags = 0;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_IDLE;
                SetState(&arg78);

                MaintainIdleGroundSupport(arg->frameTime);
            }
            else
            {
                bact_arg86 v65;
                v65.field_one = 0;
                v65.field_two = arg->frameTime;

                CrashOrLand(&v65);
            }
        }
        break;

    case BACT_STATUS_CREATE:
        CreationTimeUpdate(arg);
        break;

    case BACT_STATUS_BEAM:
        BeamingTimeUpdate(arg);
        break;
    }
}

bool NC_STACK_ypabact::ApplyUnifiedAICombatDistance(float distance, bool *startedRetreat)
{
    if ( startedRetreat )
        *startedRetreat = false;

    if ( !_unifiedAICombatDistance || !std::isfinite(distance) )
        return false;

    if ( _status_flg & BACT_STFLAG_APPROACH )
    {
        _status_flg &= ~BACT_STFLAG_ATTACK;

        if ( distance >= _ai_reengage_range )
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;
            if ( distance <= _ai_engage_range )
                _status_flg |= BACT_STFLAG_ATTACK;
        }
        else
        {
            _AI_time2 = _clock;
            _AI_time1 = _clock;
        }

        return true;
    }

    if ( distance < _ai_retreat_range )
    {
        _status_flg &= ~BACT_STFLAG_ATTACK;
        _status_flg |= BACT_STFLAG_APPROACH;
        _AI_time2 = _clock;
        _AI_time1 = _clock;
        if ( startedRetreat )
            *startedRetreat = true;
        return true;
    }

    if ( distance <= _ai_engage_range )
        _status_flg |= BACT_STFLAG_ATTACK;
    else
        _status_flg &= ~BACT_STFLAG_ATTACK;

    return true;
}

void NC_STACK_ypabact::FightWithBact(bact_arg75 *arg)
{
    constexpr float CurSectrLen = 1.1 * World::CVSectorLength;

    arg->pos = arg->target.pbact->_position;

    vec3d v40 = arg->target.pbact->_position - _position;
    float v45 = v40.normalise();

    bact_arg110 arg110;

    vec3d *foePos;
    bool isSecTarget = false;
    bool isPrimTarget = 0;

    if ( _secndT.pbact == arg->target.pbact )
    {
        foePos = &_secndT.pbact->_position;
        arg110.priority = 1;
        isSecTarget = true;
    }
    else
    {
        foePos = &_primT.pbact->_position;
        arg110.priority = 0;
        isPrimTarget = true;
    }

    NC_STACK_ypabact *a4 = _world->getYW_userHostStation();

    if ( _clock - _assess_time > 500 || _clock < 500 )
    {
        _assess_time = _clock;

        arg110.tgType = BACT_TGT_TYPE_UNIT;
        _atk_ret = TargetAssess(&arg110);
    }

    if ( _atk_ret == TA_FIGHT )
    {
        float foeDistance = ( foePos->XZ() - _position.XZ() ).length();
        bool kamikazeArmed = ypabact_IsKamikazeArmed(this);

        if ( kamikazeArmed )
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;
            _status_flg |= BACT_STFLAG_ATTACK;
            _target_vec = *foePos - _position;
        }
        else if ( _unifiedAICombatDistance )
        {
            const bool atMovementBoundary =
                _position.x < CurSectrLen || _position.z > -CurSectrLen ||
                _position.x > _wrldSize.x - CurSectrLen ||
                _position.z < _wrldSize.y + CurSectrLen;

            if ( atMovementBoundary )
            {
                // Preserve the existing world-bound escape: do not repeatedly
                // re-enter APPROACH while there is no room to increase range.
                _status_flg &= ~BACT_STFLAG_APPROACH;
                if ( foeDistance >= _ai_retreat_range &&
                     foeDistance <= _ai_engage_range )
                    _status_flg |= BACT_STFLAG_ATTACK;
                else
                    _status_flg &= ~BACT_STFLAG_ATTACK;
            }
            else
            {
                bool startedRetreat = false;
                ApplyUnifiedAICombatDistance(foeDistance, &startedRetreat);
                if ( startedRetreat )
                {
                    _target_vec.x = -_fly_dir.x;
                    _target_vec.z = -_fly_dir.z;
                }
            }
        }
        else if ( _status_flg & BACT_STFLAG_APPROACH )
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            if ( (_position.x < CurSectrLen || _position.z > -CurSectrLen || _position.x > _wrldSize.x - CurSectrLen || _position.z < _wrldSize.y + CurSectrLen) || _adist_bact < foeDistance )
            {
                _status_flg &= ~BACT_STFLAG_APPROACH;
            }
            else
            {
                _AI_time2 = _clock;
                _AI_time1 = _clock;
            }
        }
        else
        {
            if ( _sdist_bact <= foeDistance )
            {
                if ( _adist_sector <= foeDistance )
                    _status_flg &= ~BACT_STFLAG_ATTACK;
                else
                    _status_flg |= BACT_STFLAG_ATTACK;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_ATTACK;

                {
                    _target_vec.x = -_fly_dir.x;
                    _target_vec.z = -_fly_dir.z;
                }

                _AI_time2 = _clock;
                _AI_time1 = _clock;
                _status_flg |= BACT_STFLAG_APPROACH;
            }
        }
    }
    else
    {
        _status_flg &= ~(BACT_STFLAG_APPROACH | BACT_STFLAG_ATTACK);
    }

    switch( _atk_ret )
    {
        case TA_CANCEL:
        {
            if ( isPrimTarget )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                setTarget_msg arg67;
                arg67.priority = 0;
                arg67.tgt_type = BACT_TGT_TYPE_CELL;
                arg67.tgt_pos = _primTpos;

                SetTarget(&arg67);
            }

            if ( isSecTarget )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg arg67;
                arg67.priority = 1;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;

                SetTarget(&arg67);
            }

            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg arg78;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
        }
        break;

        case TA_MOVE:
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg arg78;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
        }
        break;

        case TA_FIGHT:
        {
            bact_arg101 arg101;
            arg101.pos = arg->target.pbact->_position;
            arg101.unkn = 2;
            arg101.radius = arg->target.pbact->GetCollisionBroadRadius();
            int aiWeaponSourceSlot = -1;
            arg101.weapon = ypabact_SelectAIPrimaryWeaponSlotWithLaserHold(
                this, arg->target.pbact, &aiWeaponSourceSlot);

            vec3d aiStartPoint;
            aiStartPoint.x = (arg->g_time & 1) ? _fire_pos.x : -_fire_pos.x;
            aiStartPoint.y = _fire_pos.y;
            aiStartPoint.z = _fire_pos.z;
            arg101.launch_pos =
                _position + _rotation.Transpose().Transform(aiStartPoint);
            arg101.has_launch_pos = true;

            const bool keepContinuousLaser =
                ypabact_ShouldPersistAILaserFire(this, arg101.weapon, &arg101, arg->target.pbact);

            if ( CheckFireAI(&arg101) || keepContinuousLaser )
            {
                if ( isSecTarget )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                bact_arg79 arg79;
                // CheckFireAI already approved this target. Fire along the
                // actual target vector; using only the vehicle nose caused a
                // small angular error to become a guaranteed long-range miss.
                arg79.direction = v40;
                arg79.tgType = BACT_TGT_TYPE_UNIT;
                arg79.target.pbact = arg->target.pbact;
                arg79.tgt_pos = arg->pos;
                arg79.weapon = _weapon;
                arg79.weapon_source_slot = aiWeaponSourceSlot;
                arg79.g_time = _clock;

                arg79.start_point = aiStartPoint;
                arg79.flags = BACT_ARG79_FLAG_AI_BALLISTIC_AIM;
                ypabact_AimAtFireTriggerAreaTarget(arg101, &arg79);

                LaunchMissile(&arg79);
            }
            else
            {
                if ( ypabact_IsKamikazeArmed(this) )
                    _status_flg |= BACT_STFLAG_ATTACK;
                else
                    _status_flg &= ~BACT_STFLAG_ATTACK;
            }

            bool minigunHasLineOfSight = false;
            if ( v45 < ypabact_GetMinigunRange() && HasMinigun() &&
                 v40.dot(_rotation.AxisZ()) > ypabact_GetMinigunAiFireAlignment() )
            {
                ypaworld_arg136 sight;
                sight.stPos = _position;
                sight.vect = arg->target.pbact->_position - _position;
                sight.flags = 0;
                // The short world trace only samples the start/end collision
                // cells and can miss an intermediate building on a long MGUN
                // line. Use the existing full stepped trace so AI fire is
                // gated by every terrain/building cell between shooter and
                // target.
                _world->ypaworld_func149(&sight);
                minigunHasLineOfSight = !sight.isect;
            }

            if ( minigunHasLineOfSight )
            {
                if ( isSecTarget )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                if ( !UsesVehicleMinigunTiming() && !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg arg78;
                    arg78.unsetFlags = 0;
                    arg78.newStatus = BACT_STATUS_NOPE;
                    arg78.setFlags = BACT_STFLAG_FIRE;

                    SetState(&arg78);
                }

                bact_arg105 arg105;

                arg105.field_C = arg->fperiod;
                arg105.field_10 = _clock;
                arg105.field_0 = v40;

                FireMinigun(&arg105);
            }
            else if ( !UsesVehicleMinigunTiming() && (_status_flg & BACT_STFLAG_FIRE) )
            {
                setState_msg arg78;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
        }
        break;

        case TA_IGNORE:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                setState_msg arg78;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }

            if ( _secndT.pbact == arg->target.pbact )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;
                arg67.priority = 1;

                SetTarget(&arg67);

                isSecTarget = 0;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                if ( (IsParentMyRobo() && _host_station == a4) && _status != BACT_STATUS_IDLE && !(_status_flg & BACT_STFLAG_ESCAPE) )
                {
                    robo_arg134 arg134;
                    arg134.unit = this;
                    arg134.field_4 = 1;
                    arg134.field_10 = 0;
                    arg134.field_C = 0;
                    arg134.field_8 = 0;
                    arg134.field_14 = 32;

                    _host_station->placeMessage(&arg134);
                }

                setState_msg arg78;
                arg78.unsetFlags = 0;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NORMAL;

                SetState(&arg78);

                _status = BACT_STATUS_IDLE;
            }
        }
        break;

        default:
        break;
    }
}

void NC_STACK_ypabact::FightWithSect(bact_arg75 *arg)
{
    constexpr float CurSectrLen = 1.1 * World::CVSectorLength;

    int v64 = 0;
    int v68 = 0;

    vec3d *cellPos;

    bact_arg110 arg110;

    if ( _secndT.pcell == arg->target.pcell )
    {
        cellPos = &_sencdTpos;
        v64 = 1;

        arg110.priority = 1;
    }
    else
    {
        cellPos = &_primTpos;
        v68 = 1;

        arg110.priority = 0;
    }

    NC_STACK_ypabact *a4 = _world->getYW_userHostStation();

    int v65 = IsParentMyRobo() && _host_station == a4;

    float v62 = (_position.XZ() - cellPos->XZ()).length();

    if ( _clock - _assess_time > 500 || _clock < 500 )
    {
        _assess_time = _clock;

        arg110.tgType = BACT_TGT_TYPE_CELL;
        _atk_ret = TargetAssess(&arg110);
    }

    const bool kamikazeArmed = ypabact_IsKamikazeArmed(this);

    if ( _atk_ret == TA_FIGHT )
    {
        float cellDistance = (cellPos->XZ() - _position.XZ()).length();

        // A Kamikaze assigned to a neutral/enemy sector must close all the way
        // to its fuse point instead of using the vanilla sector standoff orbit.
        // This mirrors the already-authoritative unit-target Kamikaze path.
        if ( kamikazeArmed )
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;
            _status_flg |= BACT_STFLAG_ATTACK;
        }
        else if ( _unifiedAICombatDistance )
        {
            const bool atMovementBoundary =
                _position.x < CurSectrLen || _position.z > -CurSectrLen ||
                _position.x > _wrldSize.x - CurSectrLen ||
                _position.z < _wrldSize.y + CurSectrLen;

            if ( atMovementBoundary )
            {
                _status_flg &= ~BACT_STFLAG_APPROACH;
                if ( cellDistance >= _ai_retreat_range &&
                     cellDistance <= _ai_engage_range )
                    _status_flg |= BACT_STFLAG_ATTACK;
                else
                    _status_flg &= ~BACT_STFLAG_ATTACK;
            }
            else
            {
                bool startedRetreat = false;
                ApplyUnifiedAICombatDistance(cellDistance, &startedRetreat);
                if ( startedRetreat )
                    _target_vec = -_fly_dir;
            }
        }
        else if ( _status_flg & BACT_STFLAG_APPROACH )
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            if ( (_position.x < CurSectrLen || _position.z > -CurSectrLen || _position.x > _wrldSize.x - CurSectrLen || _position.z < _wrldSize.y + CurSectrLen) || _adist_sector < cellDistance )
            {
                _status_flg &= ~BACT_STFLAG_APPROACH;
            }
            else
            {
                _AI_time2 = _clock;
                _AI_time1 = _clock;
            }
        }
        else if ( _sdist_sector <= cellDistance )
        {
            if ( _adist_sector <= cellDistance )
                _status_flg &= ~BACT_STFLAG_ATTACK;
            else
                _status_flg |= BACT_STFLAG_ATTACK;
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_ATTACK;

            {
                _target_vec = -_fly_dir;
            }

            _AI_time2 = _clock;
            _AI_time1 = _clock;

            _status_flg |= BACT_STFLAG_APPROACH;
        }
    }
    else
    {
        _status_flg &= ~(BACT_STFLAG_APPROACH | BACT_STFLAG_ATTACK);
    }

    // The vanilla sector path always stopped FIRE here because MGUNs could not
    // attack sectors. OpenNeoUA can now keep a legacy vehicle MGUN firing at a
    // sector, so only stop the loop immediately when the AI is no longer in
    // the fight state. TA_FIGHT handles its own MGUN stop below.
    if ( _atk_ret != TA_FIGHT && (_status_flg & BACT_STFLAG_FIRE) )
    {
        setState_msg arg78;
        arg78.unsetFlags = BACT_STFLAG_FIRE;
        arg78.setFlags = 0;
        arg78.newStatus = BACT_STATUS_NOPE;

        SetState(&arg78);
    }

    switch(_atk_ret)
    {
        case TA_CANCEL:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( v68 )
            {
                if ( v65 )
                {
                    robo_arg134 arg134;

                    Common::Point tmp = World::PositionToSectorID(_primTpos);

                    arg134.unit = this;
                    arg134.field_4 = 4;
                    arg134.field_8 = tmp.x;
                    arg134.field_C = tmp.y;
                    arg134.field_14 = 18;
                    arg134.field_10 = 0;

                    _host_station->placeMessage(&arg134);
                }

                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_CELL;
                arg67.tgt_pos.x = _position.x;
                arg67.tgt_pos.z = _position.z;
                arg67.priority = 0;

                SetTarget(&arg67);
            }

            if ( v64 )
            {
                if ( v65 )
                {
                    robo_arg134 arg134;

                    Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                    arg134.unit = this;
                    arg134.field_4 = 4;
                    arg134.field_8 = tmp.x;
                    arg134.field_10 = 0;
                    arg134.field_14 = 18;
                    arg134.field_C = tmp.y;

                    _host_station->placeMessage(&arg134);
                }

                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                setTarget_msg arg67;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;
                arg67.priority = 1;
                SetTarget(&arg67);
            }
        }
        break;

        case TA_MOVE:
        {
        }
        break;

        case TA_FIGHT:
        {
            // A continuous AI laser should behave like a held trigger, not like a
            // sequence of unrelated shots.  Once its primary beam is actually
            // contacting a sector, keep that exact sector aim point until the beam
            // breaks.  Non-laser weapons retain the vanilla per-frame best-part
            // selection below.
            const bool keepLaserSectorFocus = ypabact_HasActiveAILaserSectorFocus(this);

            if ( v68 )
            {
                if ( v62 < World::CVSectorLength )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIGHT_P) && v65 && _secndT.pcell != _primT.pcell )
                    {
                        robo_arg134 arg134;

                        Common::Point tmp = World::PositionToSectorID(_primTpos);

                        arg134.field_4 = 3;
                        arg134.field_8 = tmp.x;
                        arg134.field_C = tmp.y;
                        arg134.unit = this;
                        arg134.field_10 = 0;
                        arg134.field_14 = 20;

                        _host_station->placeMessage(&arg134);
                    }

                    _status_flg |= BACT_STFLAG_FIGHT_P;
                }

                if ( !keepLaserSectorFocus )
                    GetBestSectorPart(&_primTpos);

                arg->pos = _primTpos;
            }

            if ( v64 )
            {
                if ( v62 < World::CVSectorLength )
                {
                    if ( v65 && !(_status_flg & BACT_STFLAG_FIGHT_S) )
                    {
                        robo_arg134 arg134;

                        Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                        arg134.field_4 = 3;
                        arg134.field_8 = tmp.x;
                        arg134.field_C = tmp.y;
                        arg134.unit = this;
                        arg134.field_10 = 0;
                        arg134.field_14 = 20;

                        _host_station->placeMessage(&arg134);
                    }

                    _status_flg |= BACT_STFLAG_FIGHT_S;
                }

                if ( !keepLaserSectorFocus )
                    GetBestSectorPart(&_sencdTpos);

                arg->pos = _sencdTpos;
            }

            bact_arg101 arg101;
            arg101.unkn = 1;
            arg101.pos = arg->pos;
            int aiWeaponSourceSlot = -1;
            // Sector/building attacks have no class-specific unit target. Smart
            // therefore follows its documented Random fallback when no continuous
            // laser is already held.  An active laser keeps the same weapon slot
            // until its beam genuinely breaks.
            arg101.weapon = ypabact_SelectAIPrimaryWeaponSlotWithLaserHold(
                this, nullptr, &aiWeaponSourceSlot);

            vec3d aiStartPoint;
            aiStartPoint.x = (arg->g_time & 1) ? _fire_pos.x : -_fire_pos.x;
            aiStartPoint.y = _fire_pos.y;
            aiStartPoint.z = _fire_pos.z;
            arg101.launch_pos =
                _position + _rotation.Transpose().Transform(aiStartPoint);
            arg101.has_launch_pos = true;

            const bool keepContinuousLaser =
                ypabact_ShouldPersistAILaserFire(this, arg101.weapon, &arg101, nullptr);

            if ( CheckFireAI(&arg101) || keepContinuousLaser )
            {
                vec3d tmp = _position + _fire_pos - arg->pos;

                float v60 = tmp.length();

                if ( v60 < 0.01 )
                    v60 = 0.01;

                if ( v64 )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                bact_arg79 arg79;

                arg79.direction = -(_position + _fire_pos - arg->pos) / v60;
                arg79.tgType = BACT_TGT_TYPE_CELL;
                arg79.target.pbact = arg->target.pbact;
                arg79.tgt_pos = arg->pos;
                arg79.weapon = _weapon;
                arg79.weapon_source_slot = aiWeaponSourceSlot;
                arg79.g_time = _clock;

                arg79.start_point = aiStartPoint;
                arg79.flags = BACT_ARG79_FLAG_AI_BALLISTIC_AIM;
                ypabact_AimAtFireTriggerAreaTarget(arg101, &arg79);

                LaunchMissile(&arg79);
            }
            else if ( !kamikazeArmed )
            {
                // No normal Weapon can fire at this sector. A mounted Kamikaze
                // is itself a valid sector attack path, so keep ATTACK latched
                // for ApplyKamikazeRammingGuidance()/UpdateKamikaze().
                _status_flg &= ~BACT_STFLAG_ATTACK;
            }

            // Optional global sector/building minigun attack.
            vec3d minigunDir = arg->pos - _position;
            float minigunDistance = minigunDir.normalise();
            bool fireSectorMinigun = HasMinigun() &&
                                     minigunDistance > 0.01f &&
                                     minigunDistance <= ypabact_GetMinigunRange() &&
                                     minigunDir.dot(_rotation.AxisZ()) > ypabact_GetMinigunAiFireAlignment();

            if ( fireSectorMinigun )
            {
                if ( v64 )
                    _status_flg |= BACT_STFLAG_FIGHT_S;
                else
                    _status_flg |= BACT_STFLAG_FIGHT_P;

                if ( !UsesVehicleMinigunTiming() && !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    setState_msg arg78;
                    arg78.unsetFlags = 0;
                    arg78.newStatus = BACT_STATUS_NOPE;
                    arg78.setFlags = BACT_STFLAG_FIRE;
                    SetState(&arg78);
                }

                bact_arg105 arg105;
                arg105.field_C = arg->fperiod;
                arg105.field_10 = _clock;
                arg105.field_0 = minigunDir;
                FireMinigun(&arg105);
            }
            else if ( !UsesVehicleMinigunTiming() && (_status_flg & BACT_STFLAG_FIRE) )
            {
                setState_msg arg78;
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
        }
        break;

        case TA_IGNORE:
        {
            _status_flg &= ~BACT_STFLAG_APPROACH;

            if ( v64 )
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_S;

                if ( v65 && _secndT.pcell != _primT.pcell )
                {
                    robo_arg134 arg134;

                    Common::Point tmp = World::PositionToSectorID(_sencdTpos);

                    arg134.field_4 = 2;
                    arg134.field_8 = tmp.x;
                    arg134.field_C = tmp.y;
                    arg134.field_10 = 0;
                    arg134.field_14 = 22;
                    arg134.unit = this;

                    _host_station->placeMessage(&arg134);
                }

                setTarget_msg arg67;
                arg67.priority = 1;
                arg67.tgt_type = BACT_TGT_TYPE_NONE;

                SetTarget(&arg67);

                v64 = 0;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_FIGHT_P;

                if ( v65 && _status != BACT_STATUS_IDLE )
                {
                    robo_arg134 arg134;

                    arg134.field_10 = 0;
                    arg134.field_C = 0;
                    arg134.field_8 = 0;
                    arg134.unit = this;
                    arg134.field_4 = 1;
                    arg134.field_14 = 32;

                    _host_station->placeMessage(&arg134);
                }

                _status = BACT_STATUS_IDLE;
            }
        }
        break;

        default:
            break;
    }
}

void NC_STACK_ypabact::CopyWaypointsStuff( NC_STACK_ypabact *bact)
{
    if ( bact->_status_flg & BACT_STFLAG_WAYPOINT )
    {
        for (int i = 0; i < 32; i++)
            _waypoints[i] = bact->_waypoints[i];

        _status_flg |= BACT_STFLAG_WAYPOINT;

        if ( bact->_status_flg & BACT_STFLAG_WAYPOINTCCL )
            _status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            _status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        _waypoints_count = bact->_waypoints_count;
        _current_waypoint = bact->_current_waypoint;
    }
}

bool NC_STACK_ypabact::ApplyAiMaxAltitudeAboveGround()
{
    const float maxAltitudeAboveGround = ypabact_GetAiMaxAltitudeAboveGround();
    if ( !_pSector || maxAltitudeAboveGround <= 0.0f )
        return false;

    const float altitudeAboveGround = _pSector->height - _position.y;
    if ( !isfinite(altitudeAboveGround) ||
         altitudeAboveGround < maxAltitudeAboveGround )
        return false;

    // Y grows downward in Urban Assault. At or above the configured ceiling,
    // replace every upward request with the vanilla mild downward intent.
    if ( _target_dir.y < 0.15f )
        _target_dir.y = 0.15f;

    return true;
}

bool NC_STACK_ypabact::HasLocalPlayerForceVerticalPursuitTarget() const
{
    if ( !_world )
        return false;

    NC_STACK_ypabact *userVehicle = _world->getYW_userVehicle();
    const int playerOwner = _world->_userRobo ? _world->_userRobo->_owner : World::OWNER_0;
    const bool customAiAltitudeEnabled = ypabact_GetAiMaxAltitudeAboveGround() > 0.0f;

    auto isEligible = [userVehicle, playerOwner, customAiAltitudeEnabled](NC_STACK_ypabact *target)
    {
        if ( !target )
            return false;

        // Preserve the vanilla Host Station/current-player exceptions and extend
        // them to the rest of the local player's force. Relinquishing direct
        // control must not make the same target suddenly lose vertical pursuit.
        return target->_bact_type == BACT_TYPES_ROBO ||
               target == userVehicle ||
               (customAiAltitudeEnabled &&
                playerOwner != World::OWNER_0 &&
                target->_owner == playerOwner);
    };

    if ( _secndTtype == BACT_TGT_TYPE_UNIT && isEligible(_secndT.pbact) )
        return true;

    return _primTtype == BACT_TGT_TYPE_UNIT && isEligible(_primT.pbact);
}

float ypabact_AiVerticalFireTriggerRadius(const World::TWeapProto &wproto)
{
    return wproto.vertical_laser_ai_trigger_radius > 0.0f ? wproto.vertical_laser_ai_trigger_radius : 300.0f;
}

bool NC_STACK_ypabact::IsAnyKidWithoutSecondUnitTarget() const
{
    for ( NC_STACK_ypabact* node : _kidList )
    {
        if ( node->_secndTtype != BACT_TGT_TYPE_UNIT )
            return true;
    }
    return false;
}

NC_STACK_ypabact * NC_STACK_ypabact::GetEnemyCandidateInSector(const cellArea &cell, float *radius, char *job) const
{
    NC_STACK_ypabact *lastSelectedUnit = nullptr;
    const TAiTargetRangeConfig &aiTargetRange = ypabact_GetAiTargetRangeConfig();

    uint8_t protoId = _mimic_disguise_vehicleID ? _mimic_disguise_vehicleID : _vehicleID;
    const World::TVhclProto &proto = _world->GetVhclProtos().at( protoId );

    for( NC_STACK_ypabact* cel_unit : cell.unitsList )
    {
        // Do not target missile or dead
        if ( cel_unit->_bact_type == BACT_TYPES_MISSLE ||
             cel_unit->_status == BACT_STATUS_DEAD )
            continue;

        // OpenNeoUA: dummy modules are armor/decoration, never independent AI targets
        if ( cel_unit->_isDummy )
            continue;

        // OpenNeoUA invisible: a still-cloaked stealth unit is never a voluntary AI target.
        // (It can still be caught by blast/crossfire damage, which uses separate predicates.)
        if ( !cel_unit->CanBeSeenByAIOrRadar() )
            continue;

        // Do not target same fraction unit or owner == 0
        if ( cel_unit->_owner == _owner || cel_unit->_owner == World::OWNER_0 )
            continue;

        int jobLevel;

        switch ( cel_unit->_bact_type )
        {
        case BACT_TYPES_BACT:
            jobLevel = proto.job_fighthelicopter;
            break;

        case BACT_TYPES_TANK:
        case BACT_TYPES_CAR:
            jobLevel = proto.job_fighttank;
            break;

        case BACT_TYPES_FLYER:
        case BACT_TYPES_UFO:
            jobLevel = proto.job_fightflyer;
            break;

        case BACT_TYPES_ROBO:
            jobLevel = proto.job_fightrobo;
            break;

        default:
            jobLevel = 5;
            break;
        }

        int specificJob = 0;
        if ( World::TryGetSpecificFightJob(
                 proto, World::ResolveVehicleCombatClass(cel_unit), &specificJob) )
            jobLevel = specificJob;

        // do not target if job for this unit is less of previous
        if ( jobLevel < *job )
            continue;

        float radivs = (_position - cel_unit->_position).length();

        // do not target if distance more than for old selected unit
        if ( radivs > *radius &&
             (!cel_unit->getBACT_viewer() || !aiTargetRange.enforceViewerRange) )
            continue;

        // If own unit is not gun or robo do additional checks for distance
        if ( _bact_type != BACT_TYPES_GUN && _bact_type != BACT_TYPES_ROBO )
        {
            vec3d ownTargetPos;
            const NC_STACK_ypabact *commandUnit = this;
            bool isLeader = true;

            // A detached or level-placed unit without a parent is a valid root.
            // Only real squad members inherit the commander's primary target.
            if ( !IsParentMyRobo() && _parent )
            {
                commandUnit = _parent;
                isLeader = false;
            }

            if ( commandUnit->_primTtype == BACT_TGT_TYPE_CELL )
            {
                ownTargetPos = commandUnit->_primTpos;
            }
            else if ( commandUnit->_primTtype == BACT_TGT_TYPE_UNIT )
            {
                ownTargetPos = commandUnit->_primT.pbact->_position;
            }
            else
            {
                ownTargetPos = _position;
            }

            // if primary/secondary squad target distance is more than 3 sector length
            // do additional check
            if ( (ownTargetPos.XZ() - _position.XZ()).length() > World::CVUnitFarSecDist )
            {
                int countOwnAttackers = 0;

                for ( const TBactAttacker &ainf : cel_unit->_attackersList )
                {
                    if ( ainf.attacker->_secndTtype == BACT_TGT_TYPE_UNIT &&
                         ainf.attacker->_secndT.pbact == cel_unit &&
                         ainf.attacker->_owner == _owner )
                        countOwnAttackers++;

                    if ( countOwnAttackers > 1 ) // if more than 1 do break already
                        break;
                }

                // If current unit already attacked by more than 1 another units - skip it
                if ( countOwnAttackers > 1 )
                    continue;

                // If we is leader and if some of us kids do not has second target unit
                // let's skip this unit and leave it for targeting by kid
                if (isLeader && IsAnyKidWithoutSecondUnitTarget() )
                    continue;
            }
        }

        // If test of sector below of unit is OK then make it current candidate
        // and do tests for next units in this sector
        if ( TestTargetSector(cel_unit) )
        {
            *radius = radivs;
            *job = jobLevel;
            lastSelectedUnit = cel_unit;
        }
    }

    return lastSelectedUnit;
}

NC_STACK_ypabact * NC_STACK_ypabact::GetSectorTarget(Common::Point CellId) const
{
    NC_STACK_ypabact *enemy = nullptr;

    if ( _world->IsSector(CellId) )
    {
        const TAiTargetRangeConfig &aiTargetRange = ypabact_GetAiTargetRangeConfig();
        float rad = aiTargetRange.range;
        char job = 0;

        // Vanilla scans only the current sector and its eight neighbours. That
        // fixed 3x3 window can miss units which are physically inside a custom
        // range when the seeker is close to a sector boundary. Keep the exact
        // vanilla window when the option is disabled; for an explicit positive
        // range, cover every sector that can contain a target inside that radius.
        const int sectorRadius = aiTargetRange.enforceViewerRange
                               ? std::max(1, (int)std::ceil(aiTargetRange.range / World::CVSectorLength))
                               : 1;

        for (int x = -sectorRadius; x <= sectorRadius; x++)
        {
            for (int y = -sectorRadius; y <= sectorRadius; y++)
            {
                Common::Point pt = CellId + Common::Point(x, y);
                if ( !_world->IsSector(pt) )
                    continue;

                NC_STACK_ypabact *unit = GetEnemyCandidateInSector(_world->SectorAt(pt), &rad, &job);

                if ( unit ) enemy = unit;
            }
        }
    }
    return enemy;
}

void NC_STACK_ypabact::GetBestSectorPart(vec3d *arg)
{
    yw_130arg arg130;
    arg130.pos_x = arg->x;
    arg130.pos_z = arg->z;

    _world->GetSectorInfo(&arg130);

    vec2d ttmp = World::SectorIDToCenterPos2( arg130.CellId );

    arg->x = ttmp.x;
    arg->z = ttmp.y;

    if ( arg130.pcell->SectorType != 1 )
    {
        int v7 = 0;

        for (int y = 0; y < 3; y++)
        {
            for (int x = 0; x < 3; x++)
            {
                if ( arg130.pcell->buildings_health.At(x, y) > v7 )
                {
                    arg->z = 300.0 * (-1 + y) + ttmp.y;
                    arg->x = 300.0 * (-1 + x) + ttmp.x;

                    v7 = arg130.pcell->buildings_health.At(x, y);
                }
            }
        }
    }
}

void NC_STACK_ypabact::GetForcesRatio(bact_arg92 *arg)
{
    yw_130arg arg130;

    arg->energ1 = 0;
    arg->energ2 = 0;

    if ( arg->field_14 & 1 )
    {
        arg130.pos_x = _position.x;
        arg130.pos_z = _position.z;
    }
    else
    {
        arg130.pos_x = arg->pos.x;
        arg130.pos_z = arg->pos.z;
    }

    if ( _world->GetSectorInfo(&arg130) )
    {
        cellArea *cell = arg130.pcell;
        Common::Point pt = cell->CellId;

        if ( arg130.CellId.x != 0 && arg130.CellId.y != 0 )
        {
            // left-up
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.y )
        {
            // up
            cellArea &tcell = _world->SectorAt(pt.x, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.x < _wrldSectors.x - 1 && arg130.CellId.y )
        {
            // right-up
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y - 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.x )
        {
            // left
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        // center
        if ( cell->IsCanSee(_owner) )
        {
            for (NC_STACK_ypabact* &cl_unit : cell->unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
        }

        if ( arg130.CellId.x < _wrldSectors.x - 1 )
        {
            // right
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y);

            if ( tcell.IsCanSee(_owner) )
            {
               for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.x != 0 && arg130.CellId.y < _wrldSectors.y - 1 )
        {
            // left-down
            cellArea &tcell = _world->SectorAt(pt.x - 1, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.y < _wrldSectors.y - 1  )
        {
            // down
            cellArea &tcell = _world->SectorAt(pt.x, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( arg130.CellId.x < _wrldSectors.x - 1 && arg130.CellId.y < _wrldSectors.y - 1 )
        {
            // down-right
            cellArea &tcell = _world->SectorAt(pt.x + 1, pt.y + 1);

            if ( tcell.IsCanSee(_owner) )
            {
                for (NC_STACK_ypabact* &cl_unit : tcell.unitsList)
                {
                    if ( cl_unit->_owner && (cl_unit->_owner == _owner || !cl_unit->IsIgnoredByAI()) )
                    {
                        if ( cl_unit->_status != BACT_STATUS_DEAD &&
                            (cl_unit->_bact_type != BACT_TYPES_ROBO || cl_unit->_owner != _owner) &&
                             cl_unit->_bact_type != BACT_TYPES_MISSLE )
                        {
                            if ( cl_unit->_owner == _owner )
                                arg->energ1 += cl_unit->_energy;
                            else
                                arg->energ2 += cl_unit->_energy;
                        }
                    }
                }
            }
        }

        if ( !(arg->field_14 & 2) )
        {
            int v33 = 0;

            if ( cell->SectorType == 1 )
            {
                v33 = cell->buildings_health.At(0, 0);
            }
            else
            {
                for (auto helth : cell->buildings_health)
                    v33 += helth;

                v33 /= cell->buildings_health.size();
            }

            if ( cell->owner == _owner )
            {
                if ( arg->field_14 & 4 )
                    arg->energ1 += v33 * 120;
            }
            else
            {
                arg->energ2 += v33 * 120;
            }
        }
    }
}

void NC_STACK_ypabact::GetFormationPosition(bact_arg94 *arg)
{
    vec3d v2d = _rotation.AxisZ().X0Z();
    v2d.normalise();

    arg->pos1 = _position - v2d * ( (arg->field_0 / 3 + 1) * 150.0 );

    int v6 = arg->field_0 % 3;

    if ( v6 == 0 )
    {
        arg->pos1.x += 100.0 * v2d.z;
        arg->pos1.z += -100.0 * v2d.x;
    }
    else if ( v6 == 2 )
    {
        arg->pos1.x += -100.0 * v2d.z;
        arg->pos1.z += 100.0 * v2d.x;
    }

    // With y = 0
}

size_t NC_STACK_ypabact::CheckFireAI(bact_arg101 *arg)
{
    vec3d tmp;

    if ( arg->unkn == 2 )
        tmp = arg->pos - _position;
    else
        tmp = arg->pos.X0Z() - _position.X0Z() + vec3d::OY(_height);

    float len = tmp.normalise();

    if ( len == 0.0 )
        return 0;

    World::TWeapProto *v8 = nullptr;

    int v36;
    int fireWeapon = arg->weapon >= 0 ? arg->weapon : GetCurrentWeaponId();

    if ( ypabact_IsValidFireWeaponId(this, fireWeapon) )
    {
        v8 = &_world->GetWeaponsProtos().at( fireWeapon );
        v36 = v8->GetFireControlFlags();
    }

    if ( !v8 )
    {
        if ( !HasMinigun() )
            return 0;

        v36 = 2;
    }

    if ( v8 && v8->IsArcGrenade() )
    {
        const vec3d launchPos = arg->has_launch_pos
                                    ? arg->launch_pos
                                    : _position + _rotation.Transpose().Transform(_fire_pos);
        vec3d launchDirection;
        if ( !ypabact_TrySolveArcGrenadeDirection(
                 launchPos, arg->pos, *v8, &launchDirection) )
        {
            // No physical trajectory exists at the authored speed/gravity.
            // Returning through the normal fire-control gate lets the existing
            // AI movement and weapon-selection logic keep repositioning.
            return 0;
        }
    }

    size_t triggerAreaFire = 0;
    if ( v8 && CheckAIFireTriggerArea(*v8, arg, &triggerAreaFire) )
        return triggerAreaFire;

    if ( v8 && v8->IsVerticalLaser() )
    {
        vec3d fireOrigin = _position + _rotation.Transpose().Transform(_fire_pos);
        if ( arg->pos.y < fireOrigin.y )
            return 0;

        return (arg->pos.XZ() - fireOrigin.XZ()).length() <= ypabact_AiVerticalFireTriggerRadius(*v8);
    }

    if ( arg->unkn == 2 )
    {
        float v32;

        if ( v8 )
        {

            float v38 = arg->radius * 0.8 + v8->radius;

            if ( v38 >= 40.0 )
            {
                v32 = v38;
            }
            else
            {
                v32 = 3.0625;
            }
        }
        else
        {
            float v41 = arg->radius * 0.8;

            if ( v41 >= 40.0 )
                v32 = v41;
            else
                v32 = 40.0;
        }

        if ( v36 )
        {
            if ( v36 == 16 )
            {
                if ( len < World::CVSectorLength && tmp.XZ().dot( _rotation.AxisZ().XZ() ) > 0.93 )
                    return 1;
            }
            else
            {
                vec3d tmp2 = tmp * _rotation.AxisZ();

                if ( len < World::CVSectorLength && (tmp.dot( _rotation.AxisZ() ) > 0.0) && v32 / len > tmp2.length() )
                    return 1;
            }
        }
        else
        {
            if ( (arg->pos.XZ() - _position.XZ()).length() < v32 && arg->pos.y > _position.y )
                return 1;
        }
    }
    else if ( v8 )
    {
        if ( v36 )
        {
            if ( v36 == 16 )
            {
                if ( len < World::CVSectorLength && tmp.XZ().dot( _rotation.AxisZ().XZ() ) > 0.91 )
                    return 1;
            }
            else if ( len < World::CVSectorLength && tmp.dot( _rotation.AxisZ() ) > 0.91 )
            {
                return 1;
            }
        }
        else
        {
            if ( (arg->pos.XZ() - _position.XZ()).length() < v8->radius )
                return 1;
        }
    }
    return 0;
}

void NC_STACK_ypabact::StuckFree(update_msg *arg)
{


    if ( _bflags & BACT_OFLAG_BACTCOLL )
    {
        _oflags |= BACT_OFLAG_BACTCOLL;
    }

    if ( _status != BACT_STATUS_NORMAL || _oflags & BACT_OFLAG_USERINPT )
    {
        _mpos = _position;
        _brkfr_time2 = _clock;
    }
    else
    {
        vec3d tmp = _mpos - _position;

        if (tmp.length() >= 12.0)
        {
            _mpos = _position;
            _brkfr_time2 = _clock;
        }
        else
        {
            if ( _oflags & BACT_OFLAG_BACTCOLL )
                _bflags |= BACT_OFLAG_BACTCOLL;

            if ( _clock - _brkfr_time2 > 10000 )
            {
                if ( (_bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR) && !(_status_flg & BACT_STFLAG_ATTACK) )
                {
                    _old_pos = _position;

                    _position += -_rotation.AxisZ() * 10.0;

                    CorrectPositionInLevelBox(nullptr);

                    _rotation = mat3x3::RotateY(0.1) * _rotation;

                    ypaworld_arg136 arg136;
                    arg136.stPos = _old_pos;
                    arg136.vect = _position - _old_pos;
                    arg136.flags = 1;

                    TraceWorldCollision(&arg136);

                    if ( arg136.isect )
                    {
                        _position = arg136.isectPos - vec3d::OY(5.0);
                    }
                }
            }
        }
    }
}

void NC_STACK_ypabact::sub_4843BC(NC_STACK_ypabact *bact2, int a3)
{
    bact_hudi hudi;

    float v23;
    float v24;

    if ( bact2 )
    {
        vec3d v17 = bact2->_position - _position;

        mat3x3 corrected = _rotation;
        GFX::Engine.matrixAspectCorrection(corrected, false);

        vec3d v20 = corrected.Transform(v17);

        if ( v20.z != 0.0 )
        {
            v23 = v20.x / v20.z;
            v24 = v20.y / v20.z;
            GFX::Engine.viewZoomCorrection(v23, v24);
        }
        else
        {
            v24 = 0.0;
            v23 = 0.0;
        }

        hudi.field_18 = bact2;
    }
    else
    {
        v23 = -_gun_leftright;
        v24 = -_gun_angle_user;

        hudi.field_18 = nullptr;
    }

    if ( !HasMinigun() )
    {
        hudi.field_0 = 0;
    }
    else
    {
        hudi.field_0 = 1;
        hudi.field_8 = -_gun_leftright;
        hudi.field_C = -_gun_angle_user;
    }

    int hudWeaponId = GetCurrentWeaponId();
    const bool hasHudWeapon = ypabact_IsValidWeaponId(this, hudWeaponId);
    const int hudWeaponFlags = hasHudWeapon
        ? _world->GetWeaponsProtos().at(hudWeaponId)._weaponFlags : _weapon_flags;
    const bool hudUsesMissileTargeting =
        hasHudWeapon &&
        ypabact_UsesMissileTargeting(_world->GetWeaponsProtos().at(hudWeaponId));

    if ( !hasHudWeapon || a3 )
    {
        hudi.field_4 = 0;
    }
    else
    {
        if ( (hudWeaponFlags & 4) || hudUsesMissileTargeting )
        {
            hudi.field_4 = 4;
            hudi.field_10 = v23;
            hudi.field_14 = v24;
        }
        else
        {
            if ( (hudWeaponFlags & 4) || !(hudWeaponFlags & 2) )
                hudi.field_4 = 2;
            else
                hudi.field_4 = 3;

            hudi.field_10 = -_gun_leftright;
            hudi.field_14 = -_gun_angle_user;
        }
    }

    _world->ypaworld_func153(&hudi);
}

bool NC_STACK_ypabact::GetFightMotivation(float *arg)
{
    if ( _aggr == 100 )
        return true;

    if ( _aggr == 0 )
        return false;

    bact_arg81 arg81;
    arg81.enrg_sum = 0;
    arg81.enrg_type = 1;

    GetSummary(&arg81);

    float v11 = arg81.enrg_sum;

    arg81.enrg_sum = 0;
    arg81.enrg_type = 4;

    GetSummary(&arg81);

    if (arg81.enrg_sum == 0) // Possible devision by zero
        arg81.enrg_sum = 1;

    v11 = v11 / (float)arg81.enrg_sum;

    if ( arg )
        *arg = v11;

    if ( (_status_flg & BACT_STFLAG_ESCAPE) && v11 > 0.5 )
    {
        return true;
    }
    else if ( v11 > 0.2 )
    {
        return true;
    }
    return false;
}

NC_STACK_ypabact *sb_0x493984__sub1(NC_STACK_ypabact *bact)
{
    vec3d v12;

    if ( bact->_primTtype == BACT_TGT_TYPE_CELL )
        v12 = bact->_primTpos;
    else if ( bact->_primTtype == BACT_TGT_TYPE_UNIT )
        v12 = bact->_primT.pbact->_position;
    else
        return nullptr;

    float v14 = 215040.0;

    NC_STACK_ypabact *new_leader = nullptr;

    for ( NC_STACK_ypabact* &kid_unit : bact->_kidList )
    {
        if ( kid_unit->_status != BACT_STATUS_DEAD && !kid_unit->ShouldHideFromStrategicUI() )
        {
            int a4 = kid_unit->getBACT_inputting();

            if ( !a4 )
            {

                float v17 = (v12.XZ() - kid_unit->_position.XZ()).length();

                if ( !new_leader || (kid_unit->_bact_type != BACT_TYPES_UFO && v14 > v17) || (new_leader->_bact_type == BACT_TYPES_UFO && (kid_unit->_bact_type != BACT_TYPES_UFO || v14 > v17 )) )
                {
                    new_leader = kid_unit;
                    v14 = v17;
                }
            }
        }
    }
    return new_leader;
}

NC_STACK_ypabact *sb_0x493984__sub0(NC_STACK_ypabact *bact)
{
    float tmp = 0.0;
    NC_STACK_ypabact *new_leader = nullptr;

    for ( NC_STACK_ypabact* &kid_unit : bact->_kidList )
    {
        if ( kid_unit->_status != BACT_STATUS_DEAD && !kid_unit->ShouldHideFromStrategicUI() )
        {
            float v10;
            if ( kid_unit->_bact_type == BACT_TYPES_UFO )
            {
                v10 = 0.0;
            }
            else
            {
                float v8 = 1.0 - ( (bact->_position.XZ() - kid_unit->_position.XZ()).length() / 110400.0);
                v10 = (float)kid_unit->_energy / (float)kid_unit->_energy_max + v8;
            }

            if ( !new_leader || tmp < v10 )
            {
                new_leader = kid_unit;
                tmp = v10;
            }
        }
    }

    return new_leader;
}

NC_STACK_ypabact *sb_0x493984(NC_STACK_ypabact *bact, int a2)
{
    if ( !bact->_kidList.empty() )
    {
        NC_STACK_ypabact *new_leader = nullptr;

        if (a2)
            new_leader = sb_0x493984__sub1(bact);
        else
            new_leader = sb_0x493984__sub0(bact);

        if (!new_leader)
            return nullptr;

        if (new_leader->_bact_type != BACT_TYPES_UFO || bact->_bact_type == BACT_TYPES_UFO)
        {
            bact->_host_station->AddSubject(new_leader);

            new_leader->CopyTargetOf(bact);

            for ( World::RefBactList::iterator it = bact->_kidList.begin(); it != bact->_kidList.end(); )
            {
                NC_STACK_ypabact *kid_unit = *it;
                it++;

                if ( kid_unit->ShouldHideFromStrategicUI() )
                    continue;

                new_leader->AddSubject(kid_unit);

                kid_unit->CopyTargetOf(new_leader);
            }
            new_leader->_commandID = bact->_commandID;
            return new_leader;
        }

    }
    return nullptr;
}

void NC_STACK_ypabact::sub_493480(NC_STACK_ypabact *bact2, int mode)
{
    if ( _world->_isNetGame )
    {
        static uamessage_reorder ordMsg;

        ordMsg.comm = bact2->_gid;
        ordMsg.num = 0;
        ordMsg.commID = bact2->_commandID;

        for ( NC_STACK_ypabact* &bct : bact2->_kidList )
        {
            if ( bct->ShouldHideFromStrategicUI() )
                continue;

            if ( ordMsg.num < 500 )
            {
                ordMsg.units[ordMsg.num] = bct->_gid;
                ordMsg.num++;
            }
        }

        ordMsg.owner = _owner;
        ordMsg.sz = (char *)&ordMsg.units[ordMsg.num] - (char *)&ordMsg;
        ordMsg.mode = mode;
        ordMsg.msgID = UAMSG_REORDER;

        _world->NetBroadcastMessage(&ordMsg, ordMsg.sz, true);
    }
}

void NC_STACK_ypabact::ReorganizeGroup(bact_arg109 *arg)
{
    if ( arg->field_4 && arg->field_4->ShouldHideFromStrategicUI() )
        return;

    switch ( arg->field_0 )
    {
    case 1:
        if ( arg->field_4 )
        {
            if ( arg->field_4->_status == BACT_STATUS_DEAD )
            {
                ypa_log_out("ORG_NEWCHIEF: Dead master\n");
            }
            else if ( arg->field_4 != _parent && arg->field_4 != this )
            {
                _commandID = arg->field_4->_commandID;
                _aggr = arg->field_4->_aggr;

                arg->field_4->AddSubject(this);

                for ( World::RefBactList::iterator it = _kidList.begin(); it != _kidList.end(); )
                {
                    NC_STACK_ypabact *kid = *it;
                    it++;

                    if ( kid->ShouldHideFromStrategicUI() )
                        continue;

                    kid->_aggr = arg->field_4->_aggr;
                    kid->_commandID = arg->field_4->_commandID;

                    arg->field_4->AddSubject(kid);

                    kid->CopyTargetOf(arg->field_4);
                }

                CopyTargetOf(arg->field_4);
                sub_493480(arg->field_4, 1);
            }
        }
        break;

    case 2:
        if ( _host_station != _parent || arg->field_4 != this )
        {
            if ( _status == BACT_STATUS_DEAD )
            {
                ypa_log_out("ORG_BECOMECHIEF dead vehicle\n");
            }
            else
            {
                if ( _host_station != _parent && _host_station )
                    _host_station->AddSubject(this);

                if ( arg->field_4 )
                {
                    CopyTargetOf(arg->field_4);

                    _aggr = arg->field_4->_aggr;
                    _commandID = arg->field_4->_commandID;

                    AddSubject(arg->field_4);

                    for ( World::RefBactList::iterator it = arg->field_4->_kidList.begin(); it != arg->field_4->_kidList.end(); )
                    {
                        NC_STACK_ypabact *kid = *it;
                        it++;

                        if ( kid->ShouldHideFromStrategicUI() )
                            continue;

                        AddSubject(kid);
                        kid->_aggr = arg->field_4->_aggr;

                        kid->CopyTargetOf(this);
                    }

                    _commandID = arg->field_4->_commandID;
                    sub_493480(this, 2);
                }
                else
                {
                    if ( _host_station != _parent && _host_station )
                    {
                        int a4 = _host_station->getROBO_commCount();

                        _commandID = a4;

                        _host_station->setROBO_commCount(a4 + 1);
                    }
                    sub_493480(this, 2);
                }
            }
        }
        break;

    case 3:
        if ( _status == BACT_STATUS_DEAD )
        {
            ypa_log_out("ORG_NEWCOMMAND: dead vehicle\n");
        }
        else if (_host_station)
        {

            if ( _host_station == _parent )
            {
                NC_STACK_ypabact *v14 = sb_0x493984(this, 0);

                if ( v14 )
                    sub_493480(v14, 13);
            }
            else
            {
                _host_station->AddSubject(this);
            }

            int a4 = _host_station->getROBO_commCount();
            _commandID = a4;

            if (_world->_isNetGame)
                _commandID |= _owner << 24;

            _host_station->setROBO_commCount(a4 + 1);
            sub_493480(this, 3);
        }
        break;

    case 4:
        if ( arg->field_4->ShouldHideFromStrategicUI() )
            break;

        if ( arg->field_4->IsParentMyRobo() )
        {
            NC_STACK_ypabact *v19 = sb_0x493984(arg->field_4, 0);

            if ( v19 )
                sub_493480(v19, 14);
        }

        AddSubject(arg->field_4);

        arg->field_4->_commandID = _commandID;

        arg->field_4->CopyTargetOf(this);
        sub_493480(this, 4);
        break;

    case 6:
    {
        int a4 = getBACT_inputting();

        if ( !a4 )
        {
            NC_STACK_ypabact *v21 = sb_0x493984(this, 1);

            if ( v21 )
            {
                v21->AddSubject(this);
                v21->_commandID = _commandID;

                sub_493480(v21, 6);
            }
        }
    }
    break;

    default:
        break;
    }
}

void NC_STACK_ypabact::DoTargetWaypoint()
{
    if ( ( _position.XZ() - _primTpos.XZ() ).length() >= 300.0 )
        return;

    const int waypointCapacity = sizeof(_waypoints) / sizeof(_waypoints[0]);
    if ( _waypoints_count <= 0 || _waypoints_count > waypointCapacity ||
            _current_waypoint < 0 || _current_waypoint >= _waypoints_count )
    {
        _waypoints_count = 0;
        _current_waypoint = 0;
        _m_owner = 0;
        _m_cmdID = 0;
        _status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
        return;
    }

    if ( !(_status_flg & BACT_STFLAG_WAYPOINTCCL) )
    {
        setTarget_msg arg67;

        if ( _current_waypoint < _waypoints_count - 1 )
        {
            _current_waypoint++;

            arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
            arg67.priority = 0;
            arg67.tgt_pos = _waypoints[ _current_waypoint ];

            SetTarget(&arg67);
        }

        if ( _current_waypoint >= _waypoints_count - 1 )
        {
            if ( _m_cmdID )
            {
                NC_STACK_ypabact *v9 = _world->FindBactByCmdOwn(_m_cmdID, _m_owner);

                if ( v9 )
                {
                    if ( v9->_pSector->IsCanSee(_owner) )
                    {
                        arg67.tgt.pbact = v9;
                        arg67.tgt_type = BACT_TGT_TYPE_UNIT;
                        arg67.priority = 0;

                        SetTarget(&arg67);
                    }
                }
            }

            _m_owner = 0;
            _m_cmdID = 0;
            _status_flg &= ~(BACT_STFLAG_WAYPOINT | BACT_STFLAG_WAYPOINTCCL);
        }
    }
    else
    {

        _current_waypoint++;

        int v5 = _current_waypoint;

        if ( _current_waypoint >= _waypoints_count )
        {
            _current_waypoint = 0;
            v5 = 0;
        }

        setTarget_msg arg67;

        arg67.tgt_type = BACT_TGT_TYPE_CELL_IND;
        arg67.priority = 0;
        arg67.tgt_pos = _waypoints[ v5 ];

        SetTarget(&arg67);
    }
}

size_t NC_STACK_ypabact::TargetAssess(bact_arg110 *arg)
{
    bool primTgtDone = false;
    bool primTgtNear = false;

    if ( arg->tgType == BACT_TGT_TYPE_FRMT
         &&
        (_primTtype == BACT_TGT_TYPE_FRMT || _secndTtype == BACT_TGT_TYPE_FRMT) )
        return TA_MOVE;

    if ( arg->tgType == BACT_TGT_TYPE_NONE)
        return TA_IGNORE;

    if ( _primTtype == BACT_TGT_TYPE_CELL )
    {
        if ( (_position.XZ() - _primTpos.XZ()).length() < 1800.0 )
            primTgtNear = true;

        if ( _owner == _primT.pcell->owner )
            primTgtDone = true;
    }

    if ( _primTtype == BACT_TGT_TYPE_UNIT )
    {
        if ( (_position.XZ() - _primT.pbact->_position.XZ()).length() < 1800.0 )
            primTgtNear = true;

        if ( _owner == _primT.pbact->_owner )
            primTgtDone = true;
    }

    if ( arg->tgType == BACT_TGT_TYPE_UNIT )
    {
        NC_STACK_ypabact *enemy = nullptr;
        bool isSecTgt = false;
        int aggr = 0;

        if ( arg->priority == 1 )
        {
            enemy = _secndT.pbact;
            isSecTgt = true;
            aggr = 50;
        }
        else if ( arg->priority == 0)
        {
            enemy = _primT.pbact;
            isSecTgt = false;
            aggr = 25;
        }

        if ( enemy )
        {
            if ( enemy->_owner != _owner && enemy->IsIgnoredByAI() )
                return TA_CANCEL;

            float enemyDistance = (enemy->_position.XZ() - _position.XZ()).length();

            if ( !enemy->_pSector->IsCanSee(_owner) )
                return TA_CANCEL;

            if ( _aggr >= 100 )
            {
                if ( isSecTgt && enemyDistance > 2160.0 )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( enemy->_owner == 0 || enemy->_owner == _owner )
            {
                if ( enemyDistance < 300.0 )
                    return TA_IGNORE;

                return TA_MOVE;
            }

            if ( _status_flg & BACT_STFLAG_ESCAPE )
            {
                if ( !primTgtNear )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( _aggr < aggr )
            {
                if ( primTgtNear && primTgtDone )
                    return TA_FIGHT;

                return TA_CANCEL;
            }

            if ( !isSecTgt || _bact_type == BACT_TYPES_GUN )
                return TA_FIGHT;

            if ( enemyDistance > 2160.0 )
                return TA_CANCEL;

            vec3d tgtPos;

            if ( IsParentMyRobo() )
            {

                if ( _primTtype == BACT_TGT_TYPE_CELL )
                    tgtPos = _primTpos;
                else if ( _primTtype == BACT_TGT_TYPE_UNIT )
                    tgtPos = _primT.pbact->_position;
                else
                    tgtPos = _position;
            }
            else if ( _parent )
            {
                if ( _parent->_primTtype == BACT_TGT_TYPE_CELL )
                    tgtPos = _parent->_primTpos;
                else if ( _parent->_primTtype == BACT_TGT_TYPE_UNIT )
                    tgtPos = _parent->_primT.pbact->_position;
                else
                    tgtPos = _position;
            }

            if ( (tgtPos.XZ() - _position.XZ()).length() > 3600.0 )
            {
                int v28 = 0;

                for( const TBactAttacker &ainf : _secndT.pbact->_attackersList )
                {
                    if ( ainf.attacker->_secndTtype == BACT_TGT_TYPE_UNIT &&
                         ainf.attacker->_secndT.pbact == _secndT.pbact &&
                         ainf.attacker->_owner == _owner )
                        v28++;

                    if ( v28 > 2 )
                        break;
                }

                if ( v28 > 2 )
                    return TA_CANCEL;
            }
            return TA_FIGHT;
        }
    }
    else if ( arg->tgType == BACT_TGT_TYPE_CELL )
    {
        cellArea *pCell = nullptr;
        vec2d cellPos;
        bool isSecTgt = false;
        int aggr = 0;

        if ( _secndTtype == BACT_TGT_TYPE_CELL )
        {
            pCell = _secndT.pcell;
            cellPos = _sencdTpos.XZ();

            aggr = 75;
            isSecTgt = true;
        }
        else if ( _primTtype == BACT_TGT_TYPE_CELL )
        {
            pCell = _primT.pcell;
            cellPos = _primTpos.XZ();

            aggr = 25;
            isSecTgt = false;
        }

        if ( (_status_flg & BACT_STFLAG_WAYPOINT) && !isSecTgt )
        {
            DoTargetWaypoint();
            return TA_MOVE;
        }

        if ( !pCell )
            return TA_IGNORE;

        int cellEnergy = pCell->GetEnergy();

        float cellDistance = (_position.XZ() - cellPos).length();

        if ( _aggr >= 100 )
        {
            if ( cellEnergy <= 0 && pCell->owner == _owner )
            {
                if ( cellDistance < 300.0 )
                    return TA_IGNORE;

                return TA_MOVE;
            }

            return TA_FIGHT;
        }

        if ( cellDistance >= 300.0 )
        {
            if ( _owner != pCell->owner )
            {
                if ( (_status_flg & BACT_STFLAG_ESCAPE) || _aggr < aggr )
                    return TA_CANCEL;

                return TA_FIGHT;
            }

            if ( isSecTgt )
                return TA_CANCEL;

            return TA_MOVE;
        }

        if ( _owner == pCell->owner )
        {
            if ( isSecTgt )
                return TA_CANCEL;

            return TA_IGNORE;
        }

        if ( (_status_flg & BACT_STFLAG_ESCAPE) || _aggr < aggr )
            return TA_CANCEL;

        return TA_FIGHT;
    }

    return TA_IGNORE;
}

size_t NC_STACK_ypabact::PathFinder(bact_arg124 *arg)
{
    //path find for ground units (tank & car)
    int maxsteps = arg->steps_cnt;

    for (cellArea &cll : _world->_cells)
    {
        cll.pf_flags = 0;
        cll.cost_to_this = 0;
        cll.cost_to_target = 0;
        cll.pf_treeup= nullptr;
    }

    cellArea *target_pcell = _world->GetSector( World::PositionToSectorID( arg->to ) );
    cellArea *start_pcell = _world->GetSector( World::PositionToSectorID( arg->from ) );

    if ( target_pcell == start_pcell )
    {
        arg->steps_cnt = 1;
        arg->waypoints[0].x = arg->to.x;
        arg->waypoints[0].z = arg->to.y;
        return 1;
    }

    std::list<cellArea *> openList;

    start_pcell->pf_flags |= CELL_PFLAGS_IN_CLST;

    cellArea *current_pcell = start_pcell;

    int v23 = Common::ABS(target_pcell->CellId.x - current_pcell->CellId.x);
    int v24 = Common::ABS(target_pcell->CellId.y - current_pcell->CellId.y);

    float sq2 = sqrt(2.0);

    current_pcell->cost_to_target = Common::MIN(v23, v24) * sq2 + Common::ABS(v23 - v24);
    current_pcell->cost_to_this = 0;
    while ( 1 )
    {

        for(int dx = -1; dx <= 1; dx++)
        {
            for(int dz = -1; dz <= 1; dz++)
            {
                if ( dx == 0.0 && dz == 0.0 )
                    continue;

                Common::Point currentSec = current_pcell->CellId;
                Common::Point t = currentSec + Common::Point(dx, dz);

                if ( _world->IsGamePlaySector(t) )
                {
                    cellArea *cell_tzx = _world->GetSector(t);

                    if ( cell_tzx->pf_flags & CELL_PFLAGS_IN_CLST )
                        continue;

                    if ( cell_tzx->addit_cost >= 100 )
                        continue;

                    if (fabs(current_pcell->height - cell_tzx->height) >= 500.0 )
                        continue;

                    if (cell_tzx->SectorType == 1 && cell_tzx != target_pcell)
                    {
                        int32_t hlth = _world->GetLegoBld(cell_tzx, 0, 0);

                        if (_world->_legoArray[hlth].UseCollisionSkelet != _world->_legoArray[hlth].CollisionSkelet)
                            continue;
                    }

                    if ( dx != 0 && dz != 0)
                    {
                        cellArea *cell_tz = _world->GetSector(currentSec.x, t.y);
                        cellArea *cell_tx = _world->GetSector(t.x, currentSec.y);

                        if ( fabs(current_pcell->height - cell_tzx->height) > 300.0
                                || fabs(current_pcell->height - cell_tz->height) > 300.0
                                || fabs(current_pcell->height - cell_tx->height) > 300.0
                                || fabs(cell_tz->height - cell_tx->height) > 300.0
                                || fabs(cell_tzx->height - cell_tz->height) > 300.0
                                || fabs(cell_tzx->height - cell_tx->height) > 300.0)
                            continue;
                    }

                    float new_cost_to_this = sqrt(POW2(dx) + POW2(dz)) + cell_tzx->addit_cost + current_pcell->cost_to_this;

                    int v40 = Common::ABS(target_pcell->CellId.x - t.x);
                    int v41 = Common::ABS(target_pcell->CellId.y - t.y);

                    float new_cost_to_target = Common::MIN(v40, v41) * sq2 + Common::ABS(v40 - v41);

                    if ( (cell_tzx->pf_flags & CELL_PFLAGS_IN_OLST)
                            && new_cost_to_this + new_cost_to_target > cell_tzx->cost_to_this + cell_tzx->cost_to_target )
                        continue;

                    cell_tzx->cost_to_this = new_cost_to_this;
                    cell_tzx->cost_to_target = new_cost_to_target;

                    if ( !(cell_tzx->pf_flags & CELL_PFLAGS_IN_OLST) )
                        openList.push_back(cell_tzx);

                    cell_tzx->pf_treeup = current_pcell;
                    cell_tzx->pf_flags |= CELL_PFLAGS_IN_OLST;
                }
            }
        }



        if ( openList.empty() )
        {
            arg->steps_cnt = 0;
            return 0;
        }

        std::list<cellArea *>::iterator it = openList.begin();

        std::list<cellArea *>::iterator selected = it;
        float selected_value = (*selected)->cost_to_this + (*selected)->cost_to_target;

        for(it++; it != openList.end(); it++)
        {
            float v49 = (*it)->cost_to_this + (*it)->cost_to_target;

            if ( v49 < selected_value )
            {
                selected = it;
                selected_value = v49;
            }
        }

        current_pcell = *selected;

        openList.erase(selected); // Remove OLIST

        current_pcell->pf_flags &= ~CELL_PFLAGS_IN_OLST;
        current_pcell->pf_flags |= CELL_PFLAGS_IN_CLST;

        if ( current_pcell == target_pcell )
            break;
    }

    std::stack<cellArea *> pathCells;

    cellArea *iter_cell = target_pcell;

    while(iter_cell)
    {
        pathCells.push(iter_cell);
        iter_cell = iter_cell->pf_treeup;
    }

    cellArea *curcell = pathCells.top();
    pathCells.pop();

    cellArea *nextcell = pathCells.top();

    int v61 = nextcell->CellId.x - curcell->CellId.x;
    int v62 = nextcell->CellId.y - curcell->CellId.y;

    int step_id = 0;

    while ( !pathCells.empty() )
    {
        if ( maxsteps <= 1 || nextcell == target_pcell)
        {
            arg->waypoints[ step_id ].x = arg->to.x;
            arg->waypoints[ step_id ].z = arg->to.y;
            break;
        }

        curcell = nextcell;

        pathCells.pop();
        nextcell = pathCells.top();

        if ( nextcell->CellId.x - curcell->CellId.x != v61 || nextcell->CellId.y - curcell->CellId.y != v62 )
        {
            float tx, tz;

            if ( Common::ABS(v61) < Common::ABS(v62) )
            {
                if ( v61 > 0 )
                {
                    tz = 0.0;
                    tx = -200.0;
                }
                else
                {
                    tz = 0.0;
                    tx = 200.0;
                }
            }
            else
            {
                if ( v62 > 0 )
                {
                    tz = 200.0;
                    tx = 0.0;
                }
                else
                {
                    tz = -200.0;
                    tx = 0.0;
                }
            }

            v61 = nextcell->CellId.x - curcell->CellId.x;
            v62 = nextcell->CellId.y - curcell->CellId.y;

            arg->waypoints[ step_id ] = World::SectorIDToCenterPos3(curcell->CellId) + vec3d(tx, 0.0, tz);
            maxsteps--;
            step_id++;
        }
    }

    arg->steps_cnt = step_id + 1;
    return 1;
}

void NC_STACK_ypabact::SetKidsPath(int beginWp)
{
    for (NC_STACK_ypabact* &kidunit : _kidList)
    {
        if ( kidunit->ShouldHideFromStrategicUI() )
            continue;

        kidunit->_waypoints_count = _waypoints_count;
        kidunit->_current_waypoint = beginWp;

        kidunit->_status_flg |= BACT_STFLAG_WAYPOINT;

        if ( _status_flg & BACT_STFLAG_WAYPOINTCCL )
            kidunit->_status_flg |= BACT_STFLAG_WAYPOINTCCL;
        else
            kidunit->_status_flg &= ~BACT_STFLAG_WAYPOINTCCL;

        for (int i = 0; i < 32; i++)
        {
            kidunit->_waypoints[i] = _waypoints[i];
        }
    }
}

size_t NC_STACK_ypabact::SetPath(bact_arg124 *arg)
{
    // path find caller for ground squads
    int maxsteps = arg->steps_cnt;

    if ( arg->field_12 >= 2 || arg->field_12 != 1 )
        return 0; //may be 1   CHECK IT

    if ( !PathFinder(arg) )
        return 0;

    setTarget_msg arg67;
    if ( arg->steps_cnt <= 1 )
    {
        arg67.tgt_pos.x = arg->to.x;
        arg67.tgt_pos.z = arg->to.y;
    }
    else
    {
        for (int i = 0; i < arg->steps_cnt; i++)
        {
            _waypoints[i] = arg->waypoints[i];
        }

        _status_flg |= BACT_STFLAG_WAYPOINT;

        _current_waypoint = 0;
        _waypoints_count = arg->steps_cnt;

        SetKidsPath(0);

        arg67.tgt_pos.x = arg->waypoints[0].x;
        arg67.tgt_pos.z = arg->waypoints[0].z;
    }

    arg67.tgt_type = BACT_TGT_TYPE_CELL;
    arg67.priority = 0;
    SetTarget(&arg67);

    for (NC_STACK_ypabact* &kidunit : _kidList)
    {
        if ( kidunit->ShouldHideFromStrategicUI() )
            continue;

        if ( (kidunit->_bact_type == BACT_TYPES_CAR || kidunit->_bact_type == BACT_TYPES_TANK) && _pSector != kidunit->_pSector )
        {
            bact_arg124 arg125;
            arg125.steps_cnt = maxsteps;
            arg125.from = kidunit->_position.XZ();
            arg125.to = arg->to;
            arg125.field_12 = arg->field_12;

            kidunit->SetPath(&arg125);
        }
    }

    return 1;
}

bool NC_STACK_ypabact::IsNeedsWaypoints() const
{
    if (IsGroundUnit())
        return true;

    for (NC_STACK_ypabact* const &unit : _kidList)
    {
        if (unit->IsGroundUnit())
            return true;
    }

    return false;
}

void NC_STACK_ypabact::CleanAttackersTarget()
{
    for(auto it = _attackersList.begin();
        it != _attackersList.end();
        it = _attackersList.erase(it))
    {
        NC_STACK_ypabact *attacker = it->attacker;

        if ( attacker->_primTtype == BACT_TGT_TYPE_UNIT &&
             attacker->_primT.pbact == this )
        {
            attacker->_primT.pbact = nullptr;
            attacker->_primTtype = BACT_TGT_TYPE_NONE;
            attacker->_assess_time = 0;
        }

        if ( attacker->_secndTtype == BACT_TGT_TYPE_UNIT &&
             attacker->_secndT.pbact == this )
        {
            attacker->_secndT.pbact = nullptr;
            attacker->_secndTtype = BACT_TGT_TYPE_NONE;
            attacker->_assess_time = 0;
        }
    }
}

void NC_STACK_ypabact::DeleteAttacker(NC_STACK_ypabact *bact, int tgtType)
{
    _attackersList.remove( TBactAttacker(tgtType, bact) );
}

void NC_STACK_ypabact::AddAttacker(NC_STACK_ypabact *bact, int tgtType)
{
    _attackersList.push_back(TBactAttacker(tgtType, bact));
}

bool NC_STACK_ypabact::IsIgnoredByAI() const
{
    // Attached guns/modules inherit their carrier's debug exclusion.
    if ( (_debugIgnoredByAI && System::IniConf::IsGameNewDebugEnabled()) ||
         (_world && _world->IsSpectatorBact(this)) )
        return true;

    return (_isUnitGunChild || _isDummy || _bact_type == BACT_TYPES_GUN) &&
           _parent && _parent != this &&
           _parent->IsIgnoredByAI();
}
