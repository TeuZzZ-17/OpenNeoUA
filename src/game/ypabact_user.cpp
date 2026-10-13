// Player control: user layer, user targeting, hand brake and sprint handling.
#include "game/ypabact_internal.h"

float ypabact_ReadHandBrakePower()
{
    return ypabact_ReadNonNegativeFloatIni(System::IniConf::GameHandBrakePower, 1.0f);
}

bool ypabact_IsDirectLocalPlayerHandBrakeActive(NC_STACK_ypabact *bact)
{
    if ( !bact || !bact->_handbrakeHeld )
        return false;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    return world &&
           !world->_isNetGame &&
           world->getYW_userVehicle() == bact &&
           bact->getBACT_inputting();
}

float ypabact_GetHandBrakeRandomSpreadScale(NC_STACK_ypabact *bact)
{
    if ( !ypabact_IsDirectLocalPlayerHandBrakeActive(bact) )
        return 1.0f;

    // game.handbrake_power is the single canonical Hand Brake intensity.
    // Its normalized 0..1 portion reduces both recoil and random spread; values
    // above 1 may strengthen braking but remain capped for weapon modifiers.
    return 1.0f - ypabact_ReadHandBrakeRecoilReduction();
}

bool ypabact_ShouldUsePlayerLaunchShake(NC_STACK_ypabact *bact,
                                                const World::TWeapProto &wproto)
{
    NC_STACK_ypaworld *world = bact ? bact->getBACT_pWorld() : nullptr;

    return world &&
           world->getYW_userVehicle() == bact &&
           bact->getBACT_inputting() &&
           wproto.shk_launch_player.slot > 0 &&
           wproto.shk_launch_player.time > 0;
}

void ypabact_TriggerPlayerLaunchShake(NC_STACK_ypabact *bact,
                                              World::TWeapProto &wproto)
{
    if ( !ypabact_ShouldUsePlayerLaunchShake(bact, wproto) )
        return;

    // Reuse one local carrier so a multi-projectile shot produces one clean
    // event instead of stacking one shake for every spawned projectile.
    ypabact_TriggerLocalShakeCarrier(
        bact,
        &bact->_player_launch_shake_carrier,
        &wproto.shk_launch_player);
}

void NC_STACK_ypabact::ClearPlayerSprintPitchExtra()
{
    if ( !_soundcarrier.Sounds.empty() )
    {
        TSoundSource &sound = _soundcarrier.Sounds[0];
        if ( _playerSprintPitchExtra != 0 )
            sound.Pitch -= _playerSprintPitchExtra;

        // The extended rate is a sprint-only opt-in. Clear it before rebuilding
        // this frame's vanilla movement pitch so release always returns to the
        // historical audio ceiling, even while the vehicle keeps moving.
        sound.AllowExtendedRate = false;
    }

    _playerSprintPitchExtra = 0;
}

void ypabact_RunPlayerUserLayer(NC_STACK_ypabact *bact, update_msg *arg)
{
    if ( !bact )
        return;

    NC_STACK_ypaworld *world = bact->getBACT_pWorld();
    if ( !arg || !arg->inpt || !world )
    {
        bact->User_layer(arg);
        return;
    }

    // A suicide handoff can happen in the middle of the world's unit-update loop.
    // The next squad member may therefore become USERINPT before its own Update()
    // runs and see the same still-held FIRE input that killed its predecessor.
    // Require one real release before allowing fire on the newly controlled unit;
    // otherwise one press can cascade through an entire kill_after_shot squad.
    bool suppressSuicideHandoffFire = false;
    if ( bact->_suicide_handoff_wait_fire_release )
    {
        if ( arg->inpt->Buttons.IsAny({0, 2, 5}) )
            suppressSuicideHandoffFire = true;
        else
            bact->_suicide_handoff_wait_fire_release = false;
    }

    // While stun owns the fire policy, suppress the raw player fire buttons
    // even when stun_unit_fire=1: UpdateActiveDebuffStunFire() has already
    // issued the forced shot for this frame, so passing FIRE through as well
    // could duplicate continuous/laser requests. The suicide-handoff latch shares
    // this existing filtered-input path instead of creating a second User_layer.
    const bool suppressWeapons =
        world->IsNewGemNotificationBlockingPlayerWeapons(bact) ||
        bact->IsActiveDebuffStunning(false) ||
        suppressSuicideHandoffFire;
    if ( !suppressWeapons )
    {
        bact->User_layer(arg);
        return;
    }

    TInputState filteredInput = *arg->inpt;
    filteredInput.Buttons.UnSet({0, 2, 5});

    update_msg filteredArg = *arg;
    filteredArg.inpt = &filteredInput;
    bact->User_layer(&filteredArg);
}

void NC_STACK_ypabact::User_layer(update_msg *arg)
{
    _airconst = _airconst_static;

    UpdateHandBrakeInput(arg->inpt->HandBrakePressed);

    const bool landedAirControl =
        (_oflags & BACT_OFLAG_USERINPT) &&
        (_status_flg & BACT_STFLAG_LAND) &&
        (_status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE) &&
        (_bact_type == BACT_TYPES_BACT || _bact_type == BACT_TYPES_ZEPP);
    const bool takeoffRequested = landedAirControl && Input::Engine.GetKeyState(Input::KC_Q);
    const bool landedMovementLocked = landedAirControl && !takeoffRequested;

    if ( landedAirControl )
    {
        _fly_dir_length = 0.0;

        if ( landedMovementLocked )
        {
            _thraction = _mass * 9.80665;
        }
        else
        {
            _status_flg &= ~BACT_STFLAG_LAND;
            _thraction = std::min(_force, _mass * 9.80665f * 1.2f);

            setState_msg takeoffState;
            takeoffState.newStatus = BACT_STATUS_NORMAL;
            takeoffState.unsetFlags = 0;
            takeoffState.setFlags = 0;
            SetState(&takeoffState);
        }
    }

    float v106 = arg->frameTime / 1000.0;

    if ( _status == BACT_STATUS_NORMAL || _status == BACT_STATUS_IDLE )
    {

        _old_pos = _position;

        if ( _oflags & BACT_OFLAG_BACTCOLL )
        {
            if ( !(_status_flg & BACT_STFLAG_LAND) )
            {
                CollisionWithBact(arg->frameTime);
            }
        }

        float v98;

        if ( _status_flg & BACT_STFLAG_LAND )
            v98 = 0.1;
        else
            v98 = 1.0;

        setState_msg arg78;

        if ( v98 <= fabs(_fly_dir_length) )
        {
            if ( !(_status_flg & BACT_STFLAG_FIRE) )
            {
                arg78.newStatus = BACT_STATUS_NORMAL;
                arg78.unsetFlags = 0;
                arg78.setFlags = 0;
                SetState(&arg78);
            }

            _status_flg &= ~BACT_STFLAG_LAND;
        }
        else
        {
            ypaworld_arg136 arg136;

            arg136.stPos = _position;
            arg136.vect = vec3d(0.0, 0.0, 0.0);

            float v8;

            if ( _viewer_overeof <= _viewer_radius )
                v8 = _viewer_radius;
            else
                v8 = _viewer_overeof;

            arg136.flags = 0;
            arg136.vect.y = v8 * 1.5;

            _world->ypaworld_func136(&arg136);

            if ( arg136.isect && _thraction <= _mass * 9.80665 )
            {
                _fly_dir_length = 0;
                _status_flg |= BACT_STFLAG_LAND;
                float landingY = arg136.isectPos.y - _viewer_overeof;
                if ( _bact_type == BACT_TYPES_BACT )
                    _heliLandingVisualOffsetY += _position.y - landingY;
                _position.y = landingY;
                _thraction = _mass * 9.80665;
            }
            else
            {
                _status_flg &= ~BACT_STFLAG_LAND;
            }

            if ( _primTtype != BACT_TGT_TYPE_CELL || (_primTpos.XZ() - _position.XZ()).length() <= 800.0 )
            {
                if ( _status_flg & BACT_STFLAG_LAND )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        arg78.unsetFlags = 0;
                        arg78.setFlags = 0;
                        arg78.newStatus = BACT_STATUS_IDLE;
                        SetState(&arg78);
                    }
                }

                _status = BACT_STATUS_NORMAL;
            }
            else
            {
                _status = BACT_STATUS_IDLE;

                if ( _status_flg & BACT_STFLAG_LAND )
                {
                    if ( !(_status_flg & BACT_STFLAG_FIRE) )
                    {
                        arg78.newStatus = BACT_STATUS_IDLE;
                        arg78.unsetFlags = 0;
                        arg78.setFlags = 0;
                        SetState(&arg78);
                    }
                }
            }
        }

        float v110 = landedMovementLocked ? 0.0f : arg->inpt->Sliders[1] * _maxrot * v106;
        float v103 = landedMovementLocked ? 0.0f : -arg->inpt->Sliders[0] * _maxrot * v106;

        if ( (fabs(_fly_dir.y) > 0.98 || _fly_dir_length == 0.0) && _rotation.m11 > 0.996 && arg->inpt->Sliders[1] == 0.0 &&
             !(_bact_type == BACT_TYPES_BACT && (_status_flg & BACT_STFLAG_LAND)) )
        {
            // Preserve the legacy auto-level trigger, but use the same
            // frame-rate-independent gradual recovery as Hand Brake and
            // helicopter touchdown instead of snapping upright in one frame.
            SmoothStabilizeUpright(arg->frameTime);
        }

//
//

        float v111 = clp_acos( _rotation.AxisX().XZ().length() / _rotation.AxisX().length() );

        if ( _rotation.m01 < 0.0 )
            v111 = -v111;

        if ( fabs(v111) < 0.01 )
            v111 = 0.0;

        float v36 = fabs(v111);

        float v101 = _heading_speed * v36 + _heading_speed * 0.25;

        float v102 = _maxrot * v106   *  v101;

        if ( v102 > v36 )
            v102 = v36;

        if ( v111 > 0.0 )
            v102 = -v102;

        float v104 = -v103 + v102;

        if ( fabs(v104 + v111) > 1.0 )
        {
            if ( v104 >= 0.0 )
                v104 = 1.0 - fabs(v111);
            else
                v104 = fabs(v111) - 1.0;
        }


        if ( fabs(v104) > _maxrot * 2.0 * v106 * v101 )
        {
            if ( v104 < 0.0 )
                v104 = _maxrot * -2.0 * v106 * v101;

            if ( v104 >= 0.0 )
                v104 = _maxrot * 2.0 * v106 * v101;
        }

        if ( fabs(v104) < 0.001 )
            v104 = 0.0;

        _rotation = mat3x3::RotateX(v110 * 0.5) * _rotation; // local
        _rotation = mat3x3::RotateZ(v104 * 0.5) * _rotation; // local
        _rotation *= mat3x3::RotateY(v103 * 0.5); // global

        if ( !landedMovementLocked )
            _thraction += _force * v106 * 0.5 * arg->inpt->Sliders[2];

        if ( _thraction < 0.0 )
            _thraction = 0;

        if ( _thraction > _force )
            _thraction = _force;

        float v99 = _thraction;

        float v47 = _pSector->height - _position.y;
        float v94 = _player_max_altitude_above_ground * 0.8;

        if ( v47 > v94 )
        {
            float v91 = _mass * 9.80665 - _force;
            float v89 = _player_max_altitude_above_ground * 0.2;
            float v86 = (v47 - v94) * v91 / v89;

            if ( _thraction > v86 )
                _thraction = v86;

            if ( _thraction < 0.0 )
                _thraction = 0;
        }

        bact_arg79 v61;

        v61.tgType = BACT_TGT_TYPE_DRCT;
        v61.tgt_pos = _rotation.AxisZ();

        bact_arg106 v64;
        v64.field_0 = 5;
        v64.field_4 = _rotation.AxisZ();

        if ( UserTargeting(&v64) )
        {
            v61.target.pbact = v64.ret_bact;
            v61.tgType = BACT_TGT_TYPE_UNIT;
        }

        if ( arg->inpt->Buttons.IsAny({0, 5}) )
        {
            v61.direction = vec3d(0.0, 0.0, 0.0);
            v61.weapon = _weapon;
            v61.g_time = _clock;

            if ( v61.g_time % 2 )
                v61.start_point.x = _fire_pos.x;
            else
                v61.start_point.x = -_fire_pos.x;

            v61.start_point.y = _fire_pos.y;
            v61.start_point.z = _fire_pos.z;
            v61.flags = (arg->inpt->Buttons.Is(5) ? 1 : 0) | 2;
            if ( (_oflags & BACT_OFLAG_VIEWER) && arg->inpt->HandBrakePressed )
                v61.flags |= BACT_ARG79_FLAG_RECOIL_BRAKE_HELD;

            LaunchMissile(&v61);
        }

        if ( HasMinigun() && (_status_flg & BACT_STFLAG_LAND) )
        {
            if ( _status_flg & BACT_STFLAG_FIRE )
            {
                arg78.setFlags = 0;
                arg78.newStatus = BACT_STATUS_NOPE;
                arg78.unsetFlags = BACT_STFLAG_FIRE;

                SetState(&arg78);
            }
        }
        else if ( HasMinigun() )
        {
            if ( !UsesVehicleMinigunTiming() && (_status_flg & BACT_STFLAG_FIRE) )
            {
                if ( !(arg->inpt->Buttons.Is(2)) )
                {
                    arg78.setFlags = 0;
                    arg78.newStatus = BACT_STATUS_NOPE;
                    arg78.unsetFlags = BACT_STFLAG_FIRE;

                    SetState(&arg78);
                }
            }

            if ( arg->inpt->Buttons.Is(2) )
            {
                if ( !UsesVehicleMinigunTiming() && !(_status_flg & BACT_STFLAG_FIRE) )
                {
                    arg78.unsetFlags = 0;
                    arg78.newStatus = BACT_STATUS_NOPE;
                    arg78.setFlags = BACT_STFLAG_FIRE;

                    SetState(&arg78);
                }

                bact_arg105 arg105;

                arg105.field_0 = _rotation.AxisZ();
                arg105.field_C = v106;
                arg105.field_10 = _clock;

                FireMinigun(&arg105);
            }
        }

        if ( _bact_type == BACT_TYPES_BACT && (_status_flg & BACT_STFLAG_LAND) )
        {
            // A helicopter may touch down while its nose is still low.  Use
            // the handbrake's gradual rotational recovery, but do not apply
            // its thrust/velocity changes when player control starts.
            SmoothStabilizeUpright(arg->frameTime);
        }
        else if ( !landedMovementLocked && arg->inpt->Buttons.Is(3) )
        {
            HandBrake(arg);

            v99 = _thraction;
        }

        if ( landedMovementLocked )
        {
            // Keep the landed craft planted, including after weapon recoil,
            // while still allowing targeting, weapon HUD updates and firing.
            _fly_dir_length = 0.0;
            _thraction = _mass * 9.80665;
            v99 = _thraction;
        }

        if ( _status_flg & BACT_STFLAG_LAND )
        {
            move_msg arg74;
            arg74.flag = 0;
            arg74.field_0 = v106;

            Move(&arg74);
        }
        else
        {
            vec3d v81(0.0, 0.0, 0.0);

            yw_137col v43[10];

            for (int i = 3; i >= 0; i--)
            {
                move_msg arg74;
                arg74.flag = 0;
                arg74.field_0 = v106;

                Move(&arg74);

                int v50 = 0;

                ypaworld_arg137 arg137;
                arg137.pos = _position;
                arg137.pos2 = _fly_dir;
                arg137.radius = 32.0;
                arg137.collisions = v43;
                arg137.field_30 = 0;
                arg137.coll_max = 10;

                if (HasCollisionShape()) GetShapeWorldContact(&arg137);
                else if ( HasManualCompoundCollision() )
                {
                    // Heli player movement must use the same authored body as
                    // unit/hit collisions, rather than the fixed 32-unit probe.
                    // Keep the existing contact buffer and recoil response.
                    arg137.coll_count = 0;
                    const mat3x3 bodyRotation = _rotation.Transpose();
                    for (const World::TRoboColl &sphere : _collNodes.roboColls)
                    {
                        if ( sphere.robo_coll_radius <= 0.01f ||
                             arg137.coll_count == arg137.coll_max )
                            continue;

                        ypaworld_arg137 bodyProbe = arg137;
                        bodyProbe.pos = _position + bodyRotation.Transform(sphere.coll_pos);
                        bodyProbe.radius = sphere.robo_coll_radius;
                        bodyProbe.collisions = v43 + arg137.coll_count;
                        bodyProbe.coll_max = arg137.coll_max - arg137.coll_count;
                        _world->ypaworld_func137(&bodyProbe);
                        arg137.coll_count += bodyProbe.coll_count;
                    }

                    if ( UsesLegacyRadiusCollision() && arg137.coll_count < arg137.coll_max )
                    {
                        ypaworld_arg137 legacyProbe = arg137;
                        legacyProbe.collisions = v43 + arg137.coll_count;
                        legacyProbe.coll_max = arg137.coll_max - arg137.coll_count;
                        _world->ypaworld_func137(&legacyProbe);
                        arg137.coll_count += legacyProbe.coll_count;
                    }
                }
                else
                    _world->ypaworld_func137(&arg137);

                if ( arg137.coll_count )
                {
                    v81 = vec3d(0.0, 0.0, 0.0);

                    for (int j = arg137.coll_count - 1; j >= 0; j--)
                    {
                        yw_137col *v31 = &arg137.collisions[ j ];

                        v81 += v31->pos2;
                    }

                    bact_arg88 arg88;

                    float ln = v81.length();
                    if ( ln != 0.0 )
                        arg88.pos1 = v81 / ln;
                    else
                        arg88.pos1 = _fly_dir;



                    Recoil(&arg88);

                    v50 = 1;
                }

                if ( !v50 )
                {
                    ypaworld_arg136 arg136;
                    arg136.stPos = _old_pos;
                    arg136.vect = _position - _old_pos;
                    arg136.flags = 0;

                    _world->ypaworld_func136(&arg136);

                    if ( arg136.isect )
                    {
                        bact_arg88 arg88;
                        arg88.pos1 = arg136.skel->polygons[arg136.polyID].Normal();

                        Recoil(&arg88);

                        v50 = 1;
                    }
                }

                if ( !v50 )
                {
                    _status_flg &= ~BACT_STFLAG_LCRASH;
                    break;
                }

                if ( !_soundcarrier.Sounds[5].IsEnabled() )
                {
                    if ( !(_status_flg & BACT_STFLAG_LCRASH) )
                    {
                        _status_flg |= BACT_STFLAG_LCRASH;

                        SFXEngine::SFXe.startSound(&_soundcarrier, 5);

                        yw_arg180 arg180;
                        arg180.effects_type = 5;
                        arg180.field_4 = 1.0;
                        arg180.field_8 = v81.x * 10.0 + _position.x;
                        arg180.field_C = v81.z * 10.0 + _position.z;

                        _world->ypaworld_func180(&arg180);
                    }
                }
            }
        }

        _thraction = v99;
    }
    else if ( _status == BACT_STATUS_DEAD )
    {
        DeadTimeUpdate(arg);
    }
}

vec3d NC_STACK_ypabact::GetUserWeaponAimDirection() const
{
    float corW, corH;
    GFX::Engine.getAspectCorrection(corW, corH, true);
    vec3d direction = _rotation.AxisZ() - _rotation.AxisY() * (_gun_angle_user * corH);
    if ( _bact_type == BACT_TYPES_TANK )
        direction -= _rotation.AxisX() * (_gun_leftright * corW);
    return direction;
}

void NC_STACK_ypabact::HandBrake(update_msg *arg)
{
    const float brakePower = GetHandBrakePower();
    if ( brakePower <= 0.0f )
        return;

    _thraction = _mass * 9.77665;

    SmoothStabilizeUpright(arg->frameTime);

    const float brakeRetention = std::pow(0.01f, brakePower * arg->frameTime * 0.001f);
    _fly_dir_length *= brakeRetention;

    if ( fabs(_fly_dir_length) < 0.1 )
    {
        _fly_dir = vec3d(0.0, 1.0, 0.0);
        _fly_dir_length = 0.0;
    }
}

float NC_STACK_ypabact::GetHandBrakePower() const
{
    return ypabact_ReadHandBrakePower();
}

void NC_STACK_ypabact::UpdateHandBrakeInput(bool pressed)
{
    if ( !pressed )
    {
        ReleaseHandBrake();
        return;
    }

    if ( _handbrakeHeld )
        return;

    _handbrakeHeld = true;

    const size_t soundId = World::TVhclProto::SND_HANDBRAKE;
    if ( _soundcarrier.Sounds.size() <= soundId )
        return;

    TSoundSource &sound = _soundcarrier.Sounds[soundId];
    if ( sound.PSample && !sound.IsEnabled() )
        SFXEngine::SFXe.startSound(&_soundcarrier, soundId);
}

void NC_STACK_ypabact::ReleaseHandBrake()
{
    _handbrakeHeld = false;
}

size_t NC_STACK_ypabact::UserTargeting(bact_arg106 *arg)
{
    // Release every primary lock outside the same limits used for firing.
    if ( !IsPlayerPrimaryWeaponElevationAllowed() )
    {
        _userHomingPrimaryTargetGid = 0;
        _userHomingTargetCycleRequested = false;
        if ( _secndTtype != BACT_TGT_TYPE_NONE )
        {
            setTarget_msg clearTarget = {};
            clearTarget.tgt_type = BACT_TGT_TYPE_NONE;
            clearTarget.priority = 1;
            SetTarget(&clearTarget);
        }
        sub_4843BC(nullptr, 0);
        arg->ret_bact = nullptr;
        return 0;
    }

    NC_STACK_ypabact *targeto = 0;
    float v56 = 0.0;

    int targetingWeaponId = GetCurrentWeaponId();
    const World::TWeapProto *targetingProto =
        ypabact_IsValidFireWeaponId(this, targetingWeaponId)
            ? &_world->GetWeaponsProtos().at(targetingWeaponId) : nullptr;

    float v55 = targetingProto ? targetingProto->radius : 0.0;
    int targetingWeaponFlags = targetingProto
        ? targetingProto->_weaponFlags
        : (HasMinigun() ? World::TWeapProto::WEAPON_FLAG_DIRECT : 0);
    const bool usesMissileTargeting =
        targetingProto && ypabact_UsesMissileTargeting(*targetingProto);
    int a3a = !(targetingWeaponFlags & 2) && !(targetingWeaponFlags & 0x10);
    bool searchWeaponTarget = !a3a;

    if ( targetingProto && searchWeaponTarget )
    {
        yw_130arg arg130;
        arg130.pos_x = _position.x;
        arg130.pos_z = _position.z;

        _world->GetSectorInfo(&arg130);

        vec2d tmp = _rotation.AxisZ().XZ() * World::CVSectorLength + _position.XZ();

        cellArea *pCells[3];

        pCells[0] = arg130.pcell;

        arg130.pos_x = tmp.x;
        arg130.pos_z = tmp.y;

        _world->GetSectorInfo(&arg130);

        pCells[2] = arg130.pcell;

        if ( arg130.pcell == pCells[0] )
        {
            pCells[1] = pCells[0];
        }
        else
        {
            vec2d tmp2 = _position.XZ() + (tmp - _position.XZ()) * 0.5;
            arg130.pos_x = tmp2.x;
            arg130.pos_z = tmp2.y;

            _world->GetSectorInfo(&arg130);

            pCells[1] = arg130.pcell;
        }

        for (int i = 0; i < 3; i++)
        {
            if ( i <= 0 || pCells[i] != pCells[i - 1] )
            {
                if ( pCells[i] )
                {
                    for ( NC_STACK_ypabact* &bct : pCells[i]->unitsList )
                    {
                        if ( bct != this )
                        {
                            if ( !IsPrimaryWeaponElevationAllowed(bct->_position - _position) )
                                continue;
                            if ( bct->_bact_type != BACT_TYPES_MISSLE && bct->_status != BACT_STATUS_DEAD )
                            {
                                if ( bct->IsInvisibleUnrevealed() ||
                                     (bct->_owner != _owner && bct->IsIgnoredByAI()) )
                                    continue;

                                int v53 = 0;
                                if (bct->_bact_type == BACT_TYPES_GUN)
                                {
                                    NC_STACK_ypagun *gun = dynamic_cast<NC_STACK_ypagun *>( bct );
                                    v53 = gun->IsRoboGun() && bct->GetEffectiveShield() >= 100.0f;
                                }

                                if ( !v53 )
                                {
                                    if ( arg->field_0 & 2 || bct->_owner != _owner )
                                    {
                                        if ( arg->field_0 & 1 || bct->_owner == _owner || !bct->_owner )
                                        {
                                            if ( arg->field_0 & 4 || bct->_owner )
                                            {
                                                vec3d mv = bct->_position - _old_pos;

                                                if ( mv.dot( _rotation.AxisZ() ) >= 0.0 )
                                                {
                                                    float mv_len = mv.length();

                                                    vec3d mvd = arg->field_4 * mv;

                                                    float v59 = mv_len * 1000.0 * 0.0005 + 20.0;
                                                    float mvd_len = mvd.length();

                                                    if ( ((mvd_len < v59 && ((targetingWeaponFlags & 4) || usesMissileTargeting)) ||
                                                         (bct->_radius + v55 > mvd_len && !(targetingWeaponFlags & 4) && !usesMissileTargeting) )
                                                            && mv_len < 2000.0
                                                            && (v56 > mvd_len || !targeto) )
                                                    {
                                                        targeto = bct;
                                                        v56 = mvd_len;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                            }
                        }
                    }
                }
            }
        }
    }

    // A remappable Cycle Target press is consumed here, where the exact user
    // aim vector is available. Homing bombs keep their automatic launch-time
    // target selection and do not enter this manual missile lock path.
    if ( targetingProto && usesMissileTargeting )
    {
        NC_STACK_ypabact *manualTarget =
            _userHomingPrimaryTargetGid > 0
                ? _world->FindLiveBactByGid(_userHomingPrimaryTargetGid) : nullptr;

        if ( _userHomingTargetCycleRequested )
        {
            NC_STACK_ypabact *currentTarget = manualTarget;
            if ( !currentTarget && _secndTtype == BACT_TGT_TYPE_UNIT )
                currentTarget = _secndT.pbact;
            if ( !currentTarget )
                currentTarget = targeto;

            NC_STACK_ypabact *nextTarget = ypabact_SelectNextHomingCycleTarget(
                this, arg, *targetingProto, currentTarget);

            _userHomingTargetCycleRequested = false;
            if ( nextTarget )
            {
                _userHomingPrimaryTargetGid = nextTarget->_gid;
                manualTarget = nextTarget;
            }
        }

        if ( manualTarget &&
             ypabact_IsHomingCycleTargetLockable(this, manualTarget, *targetingProto, arg->field_4) )
        {
            targeto = manualTarget;
        }
        else if ( _userHomingPrimaryTargetGid > 0 )
        {
            _userHomingPrimaryTargetGid = 0;
        }
    }
    else
    {
        _userHomingPrimaryTargetGid = 0;
        _userHomingTargetCycleRequested = false;
    }

    if ( targeto )
    {
        sub_4843BC(targeto, a3a);

        int previewWeaponId = GetCurrentWeaponId();
        if ( previewWeaponId != -1 && !a3a )
        {
            bact_arg79 previewArg = {};
            previewArg.direction = vec3d(0.0, 0.0, 0.0);
            previewArg.tgType = BACT_TGT_TYPE_UNIT;
            previewArg.target.pbact = targeto;
            previewArg.tgt_pos = targeto->_position;
            previewArg.weapon = previewWeaponId;

            World::TWeapProto &previewProto = _world->GetWeaponsProtos().at(previewWeaponId);
            if ( previewProto.IsLaser() )
            {
                ypabact_UpdateHUDLaserMultiLockTargets(this, &previewArg, previewProto);
            }
            else
            {
                ypabact_UpdateHUDWeaponMultiLockTargets(
                    this, &previewArg, previewProto, GetCurrentWeaponProjectileCount());
            }
        }
        else if ( _oflags & BACT_OFLAG_USERINPT )
        {
            _world->_hudMissileMultiLockTargets.clear();
        }

        setTarget_msg arg67;
        arg67.tgt_type = BACT_TGT_TYPE_UNIT;
        arg67.priority = 1;
        arg67.tgt.pbact = targeto;

        SetTarget(&arg67);

        arg->ret_bact = targeto;
        return 1;
    }

    if ( _secndTtype == BACT_TGT_TYPE_UNIT && _secndT.pbact &&
         !IsPrimaryWeaponElevationAllowed(_secndT.pbact->_position - _position) )
    {
        setTarget_msg clearTarget = {};
        clearTarget.tgt_type = BACT_TGT_TYPE_NONE;
        clearTarget.priority = 1;
        SetTarget(&clearTarget);
    }
    sub_4843BC(nullptr, a3a);
    if ( (_oflags & BACT_OFLAG_USERINPT) && _world )
        _world->_hudMissileMultiLockTargets.clear();
    arg->ret_bact = nullptr;

    return 0;
}

void NC_STACK_ypabact::setBACT_inputting(bool inpt)
{
    if ( !inpt )
        ResetAlternativeView();

    if ( inpt )
    {
        if ( _world && !_world->CanControlUnitInSpectatorMode(this) )
            return;

        // OpenNeoUA custom: artillery shell platforms are artillery used only from the 2D
        // strategic map. Never let the player take first-person control of them.
        if ( _world && IsArtilleryShellPlatform() )
            return;

        _oflags |= BACT_OFLAG_USERINPT;
        _world->setYW_userVehicle(this);

        // Drop the unit's own inherited AI orders when direct control begins.
        // Child squad members keep their already assigned orders because a
        // NONE target is not propagated by SetTarget(). Player aim/lock can
        // immediately assign a fresh secondary target through UserTargeting().
        setTarget_msg clearTarget = {};
        clearTarget.tgt_type = BACT_TGT_TYPE_NONE;

        if ( _primTtype != BACT_TGT_TYPE_NONE )
        {
            clearTarget.priority = 0;
            SetTarget(&clearTarget);
        }

        if ( _secndTtype != BACT_TGT_TYPE_NONE )
        {
            clearTarget.priority = 1;
            SetTarget(&clearTarget);
        }

        _target_vec = vec3d(0.0, 0.0, 0.0);

        const bool landedHelicopter =
            _bact_type == BACT_TYPES_BACT &&
            (_status_flg & BACT_STFLAG_LAND);

        if ( landedHelicopter )
        {
            // AI rest height uses overeof, while player control uses
            // viewer_overeof. Resolve that one-time height difference on the
            // camera cut instead of replaying the landing animation.
            ypaworld_arg136 ground;
            ground.stPos = _position;
            ground.vect = vec3d(0.0, std::max(_viewer_radius, _viewer_overeof) * 1.5f, 0.0);
            ground.flags = 0;

            _world->ypaworld_func136(&ground);

            if ( ground.isect )
            {
                _position.y = ground.isectPos.y - _viewer_overeof;
                _old_pos.y = _position.y;
            }

            _heliLandingVisualOffsetY = 0.0f;
        }
        else if ( _bact_type != BACT_TYPES_GUN )
            CorrectPositionOnLand();
    }
    else
    {
        ClearPlayerSprintPitchExtra();
        _oflags &= ~BACT_OFLAG_USERINPT;
    }
}
