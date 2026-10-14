// Movement, terrain/level-box corrections, collisions, crashes and Genesis separation.
#include "game/ypabact_internal.h"

bool ypabact_BeginUnitCollisionEvent(NC_STACK_ypabact *first,
                                             NC_STACK_ypabact *second,
                                             int frameTime)
{
    if ( !first || !second )
        return false;

    NC_STACK_ypaworld *world = first->getBACT_pWorld();
    if ( !world || world != second->getBACT_pWorld() )
        return false;

    static std::map<TUnitCollisionPairKey, int> activeContacts;

    const int low = std::min(first->_gid, second->_gid);
    const int high = std::max(first->_gid, second->_gid);
    const int now = world->_timeStamp;
    const int safeFrameTime = std::max(0, frameTime);
    const int contactGap = std::max(100, safeFrameTime * 3 + 1);
    TUnitCollisionPairKey key = { world, low, high };

    if ( activeContacts.size() > 4096 )
    {
        for (auto it = activeContacts.begin(); it != activeContacts.end(); )
        {
            if ( it->first.world != world || now < it->second || now - it->second > 5000 )
                it = activeContacts.erase(it);
            else
                ++it;
        }
    }

    auto found = activeContacts.find(key);
    const bool isNewContact = found == activeContacts.end() ||
                              now < found->second ||
                              now - found->second > contactGap;
    activeContacts[key] = now;
    return isNewContact;
}

bool ypabact_IsGenesisSeparationVehicle(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    // Apply the safe genesis-only separation to normal buildable vehicle
    // classes. Guns and Host Stations are deliberately excluded, as are
    // missiles and generic helper/dummy BACTs.
    switch ( unit->_bact_type )
    {
    case BACT_TYPES_BACT: // model = heli
    case BACT_TYPES_TANK:
    case BACT_TYPES_ZEPP:
    case BACT_TYPES_FLYER:
    case BACT_TYPES_UFO:
    case BACT_TYPES_CAR:
        return true;
    default:
        return false;
    }
}

bool ypabact_ReadFallDamage(World::TAbsoluteOrPercent *out)
{
    return ypabact_ReadAbsoluteOrPercentIni(System::IniConf::GameFallEnergy, out, 100.0f);
}

float ypabact_ReadPlayerMaxAltitudeAboveGround()
{
    constexpr float DefaultAltitude = 1600.0f;
    float altitude = ypabact_ReadNonNegativeFloatIni(
        System::IniConf::GamePlayerMaxAltitudeAboveGround, DefaultAltitude);
    if ( !isfinite(altitude) || altitude <= 0.0f )
        return DefaultAltitude;

    return altitude;
}

bool ypabact_ReadUnitCollisionDamage(Common::Ini::Key &key,
                                               World::TAbsoluteOrPercent *out)
{
    return ypabact_ReadAbsoluteOrPercentIni(key, out, 100.0f);
}

bool ypabact_IsCustomFallDamageConfigActive()
{
    World::TAbsoluteOrPercent energy;
    return ypabact_ReadFallDamage(&energy);
}

void ypabact_ApplyFallDamage(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->IsInvulnerableToDamage() )
        return;

    World::TAbsoluteOrPercent energy;
    if ( !ypabact_ReadFallDamage(&energy) )
    {
        unit->_energy -= fabs(unit->_fly_dir_length) * 10.0;
        return;
    }

    if ( energy.value <= 0.0f )
        return;

    double rawDamage = energy.percent
        ? (double)std::max(unit->_energy_max, 0) * (double)energy.value / 100.0
        : (double)energy.value;
    if ( rawDamage <= 0.0 )
        return;

    rawDamage = std::min(rawDamage, (double)std::numeric_limits<int>::max());
    const int shieldedDamage = unit->CalcShieldedCustomDamage((int)ceil(rawDamage));
    unit->_energy -= shieldedDamage;
}

bool ypabact_ShouldPlayCustomAiCrashlandSound(const NC_STACK_ypabact *unit)
{
    return unit &&
           unit->_bact_type == BACT_TYPES_TANK &&
           !(unit->_oflags & BACT_OFLAG_USERINPT) &&
           !(unit->_oflags & BACT_OFLAG_VIEWER) &&
           ypabact_IsCustomFallDamageConfigActive();
}

bool ypabact_ShouldPlayCrashlandSound(const NC_STACK_ypabact *unit, float speed, float minSpeed)
{
    if ( fabs(speed) <= minSpeed )
        return false;

    if ( unit->_oflags & BACT_OFLAG_USERINPT )
        return true;

    return ypabact_ShouldPlayCustomAiCrashlandSound(unit);
}

void ypabact_StartSoundOnce(NC_STACK_ypabact *unit, int soundId)
{
    if ( unit && !unit->_soundcarrier.Sounds[soundId].IsEnabled() )
        SFXEngine::SFXe.startSound(&unit->_soundcarrier, soundId);
}

bool ypabact_IsAirVehicle(const NC_STACK_ypabact *unit)
{
    return unit &&
           (unit->_bact_type == BACT_TYPES_BACT ||
            unit->_bact_type == BACT_TYPES_FLYER ||
            unit->_bact_type == BACT_TYPES_UFO);
}

void NC_STACK_ypabact::FixSectorFall()
{
    ypaworld_arg136 arg136;
    arg136.stPos = vec3d(_position.x, -30000.0, _position.z);
    arg136.vect = vec3d(0.0, 50000.0, 0.0);
    arg136.flags = 0;

    _world->ypaworld_func136(&arg136);

    if ( arg136.isect )
        _position.y = arg136.isectPos.y - 50.0;
    else
        _position.y = _pSector->height  - 50.0;
}

void NC_STACK_ypabact::FixBeyondTheWorld()
{
    vec2d mv = World::SectorIDToPos2( _world->GetMapSize() );

    if ( _position.x > mv.x )
        _position.x = mv.x - World::CVSectorHalfLength;

    if ( _position.x < 0.0 )
        _position.x = World::CVSectorHalfLength;

    if ( _position.z < mv.y )
        _position.z = mv.y + World::CVSectorHalfLength;

    if ( _position.z > 0.0 )
        _position.z = -World::CVSectorHalfLength;

    FixSectorFall();
}

bool ypabact_ShouldFlattenAirKnockback(const NC_STACK_ypabact *unit)
{
    // OpenNeoUA recoil/push stability: airborne vehicle controllers fight hard to
    // maintain altitude.  Feeding them vertical recoil / aoe push creates the
    // observed up/down bouncing after attacks.  Keep knockback horizontal while
    // the unit is flying; landed air vehicles still use their normal ground state.
    return ypabact_IsAirVehicle(unit) && !(unit->_status_flg & BACT_STFLAG_LAND);
}

bool ypabact_NormalizeXZ(vec3d *dir)
{
    dir->y = 0.0;

    float dirLen = dir->length();
    if ( !isfinite(dirLen) || dirLen <= 0.001f )
        return false;

    *dir /= dirLen;
    return true;
}

void NC_STACK_ypabact::Move(move_msg *arg)
{
    _old_pos = _position;
    const mat3x3 shapeOldRotation = _rotation;

    float weight;

    if ( _status == BACT_STATUS_DEAD )
        weight = _mass * 39.2266;
    else
        weight = _mass * 9.80665;

    float thraction = 0.0;
    vec3d v54(0.0, 0.0, 0.0);

    if ( !(arg->flag & 1) )
    {
        v54 = -_rotation.AxisY();

        thraction = _thraction;
        if ( _world && _force > 0.0f )
            thraction *= _world->GetPlayerSprintForce(this) / _force;

        if ( _oflags & BACT_OFLAG_USERINPT )
        {
            v54.x = fSign(v54.x) * sqrt( fabs(v54.x) );
            v54.y = fSign(v54.y) * v54.y * v54.y;
            v54.z = fSign(v54.z) * sqrt( fabs(v54.z) );
        }
    }

    vec3d v41 = vec3d::OY(weight) + v54 * thraction - _fly_dir * (_fly_dir_length * _airconst);

    float len = v41.length();

    if ( _oflags & BACT_OFLAG_USERINPT )
    {
        if ( v41.y >= 0.0 )
            v41.y *= 3.0;
        else
            v41.y *= 5.0;
    }

    if ( len > 0.0 )
    {
        //vec3d v42 = bact->fly_dir * bact->fly_dir_length + (v41 / len) * (len / bact->mass * arg->field_0);
        vec3d v42 = _fly_dir * _fly_dir_length + v41 * (arg->field_0 / _mass);

        _fly_dir_length = v42.length();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = v42 / _fly_dir_length;
    }

    if ( fabs(_fly_dir_length) > 0.1 )
        _position += _fly_dir * (_fly_dir_length * arg->field_0 * 6.0);

    CorrectPositionInLevelBox(nullptr);

    ResolveShapeMovement(_old_pos, shapeOldRotation);

    _soundcarrier.Sounds[0].Pitch = _soundcarrier.Sounds[0].PitchBase;
    _soundcarrier.Sounds[0].Volume = _volume;

    float v50;
    if ( _pitch_max <= -0.8 )
        v50 = 1.2;
    else
        v50 = _pitch_max;

    float v30 = fabs(_fly_dir_length) * v50;
    float v31 = _force * _force - _mass * 100.0 * _mass;

    float v43 = 0.0;
    if ( v31 > 0.0 && isnormal(v31) && _airconst_static != 0.0 )
    {
        float denom = sqrt(v31) / _airconst_static;
        if ( denom > 0.0 && isnormal(denom) )
            v43 = v30 / denom;
    }

    // Heli vehicles use the base BACT movement/audio path. If a debuff lowers
    // force far enough, the old sqrt(_force^2 - mass*100*mass) expression can
    // become NaN and poison Pitch, which makes the heli loop go silent. Treat
    // invalid speed-pitch contribution as no extra movement pitch.
    if ( !isnormal(v43) || v43 < 0.0 )
        v43 = 0.0;

    if ( v43 > v50 )
        v43 = v50;

    if ( _soundcarrier.Sounds[0].PSample )
        _soundcarrier.Sounds[0].Pitch += (_soundcarrier.Sounds[0].PSample->SampleRate + _soundcarrier.Sounds[0].Pitch) * v43;
}

void NC_STACK_ypabact::ResolveShapeMovement(const vec3d &oldPosition, const mat3x3 &oldRotation, int frameTime)
{
    if (_world && _world->_collisionScene)
        _world->_collisionScene->Resolve(this, oldPosition, oldRotation, frameTime);
}

void NC_STACK_ypabact::GetShapeWorldContact(ypaworld_arg137 *contacts)
{
    contacts->coll_count = 0;
    Collision::Contact hit;
    if (!_world || !_world->_collisionScene || contacts->coll_max <= 0 ||
        !_world->_collisionScene->TakeWorldContact(this, &hit)) return;
    contacts->collisions[0].pos1 = hit.point;
    contacts->collisions[0].pos2 = -hit.normal;
    contacts->coll_count = 1;
    // The existing landing/recoil code needs the impact speed before CCD
    // removed its component into the surface.
    _fly_dir_length = hit.incomingVelocity.length();
    if (_fly_dir_length > 1e-8) _fly_dir = hit.incomingVelocity / _fly_dir_length;
}

void NC_STACK_ypabact::HandleShapeWorldCollision(const Collision::Contact &contact)
{
    const float speed = contact.incomingVelocity.length();
    if (speed <= 1e-6) return;
    if ((_status_flg & BACT_STFLAG_LAND) &&
        (_bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR))
    {
        vec3d normal = contact.normal; normal.y = 0;
        if (normal.normalise() < 1e-6) return;
        // Keep the original recoil attenuation, projected onto the tank's
        // signed drive axis, so a wall impact gives a short backward bounce.
        const vec3d reflected = contact.incomingVelocity - normal *
            (2.0 * contact.incomingVelocity.dot(normal));
        _fly_dir = _rotation.AxisZ();
        _fly_dir_length = reflected.dot(_fly_dir) * (25.0 / (speed + 10.0));
        _thraction = 0;
    }
    else
    {
        _fly_dir = contact.incomingVelocity / speed;
        _fly_dir_length = speed;
        bact_arg88 recoil; recoil.pos1 = -contact.normal;
        Recoil(&recoil);
    }
    if (speed > 2.333333333333334f)
        ypabact_StartSoundOnce(this, 5);
}

size_t NC_STACK_ypabact::HandleShapeUnitContact(const Collision::Contact &contact, int frameTime)
{
    if (!contact.actor || contact.actor->IsDestroyed() ||
        contact.incomingVelocity.dot(contact.normal) >= -1e-6)
        return 0;
    if (!(_status_flg & BACT_STFLAG_BCRASH))
    {
        ypabact_StartSoundOnce(this, 6);
        _status_flg |= BACT_STFLAG_BCRASH;
        if (getBACT_viewer())
        {
            yw_arg180 effect;
            effect.field_4 = 1.0;
            effect.field_8 = contact.point.x;
            effect.field_C = contact.point.z;
            effect.effects_type = 5;
            _world->ypaworld_func180(&effect);
        }
    }
    // Reuse the unit rebound before the landing controller consumes the
    // world contact. The accepted shape position remains outside both bodies.
    _fly_dir_length = contact.incomingVelocity.length();
    if (_fly_dir_length > 1e-8) _fly_dir = contact.incomingVelocity / _fly_dir_length;
    if (fabs(_fly_dir_length) < 0.1) _fly_dir_length = 1.0;
    bact_arg88 recoil;
    recoil.pos1 = _bact_type == BACT_TYPES_ROBO ? -contact.normal :
        contact.actor->GetBodyPosition() - GetBodyPosition();
    if (recoil.pos1.normalise() < 1e-6 || recoil.pos1.dot(_fly_dir) < 0)
        recoil.pos1 = -contact.normal;
    const vec3d safe = _position;
    Recoil(&recoil);
    _position = safe; // Mixed sphere/shape contacts also keep the accepted pose.
    _target_vec = _fly_dir;
    _AI_time1 = _clock;
    _AI_time2 = _clock;
    return 1;
}

size_t NC_STACK_ypabact::SetPosition(bact_arg80 *arg)
{
    yw_130arg sect_info;

    sect_info.pos_x = arg->pos.x;
    sect_info.pos_z = arg->pos.z;
    if (!_world->GetSectorInfo(&sect_info))
        return 0;

    if ( _pSector )
        _cellRef.Detach();

    _cellRef = sect_info.pcell->unitsList.push_back(this);

    _pSector = sect_info.pcell;
    _old_pos = arg->pos;
    _position = arg->pos;
    _fallDamageAirborne = false;
    _fallDamageConsumed = false;
    _cellId = sect_info.CellId;

    if ( !(arg->field_C & 2) )
        CorrectPositionInLevelBox(nullptr);

    return 1;
}

void NC_STACK_ypabact::ApplyImpulse(bact_arg83 *arg)
{
    float v81 = 50.0 / _mass;
    float v79 = arg->energ * 0.0004;

    vec3d v60 = _position - arg->pos;

    float distance = v60.length();

    if ( distance <= _radius )
    {
        vec3d v63 = (arg->pos2 * (2.5 * arg->mass * arg->force) + _fly_dir * _mass * _fly_dir_length) / (_mass + arg->mass);

        _fly_dir_length = v63.normalise();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = v63;

        v60 = arg->pos2;

        distance = 1.0;
    }
    else
    {
        v60 /= distance;

        vec3d v63 = _fly_dir * _fly_dir_length + (v60 * v81 * v79) / distance;

        _fly_dir_length = v63.normalise();

        if ( _fly_dir_length > 0.0 )
            _fly_dir = v63;
    }

    CorrectPositionInLevelBox(nullptr);

    _status_flg &= ~BACT_STFLAG_LAND;

    float angle = v81 * 0.01 * v79 / distance;

    float cos_len = v60.dot(_rotation.AxisZ());

    // cos(45) == 0.7071
    if ( fabs(cos_len) > 0.7071 )
    {
        if ( cos_len > 0.7071 )
            _rotation = mat3x3::RotateX(-angle) * _rotation;
        else
            _rotation = mat3x3::RotateX(angle) * _rotation;
    }
    else
    {
        if ( v60.XZ().cross( _rotation.AxisZ().XZ() ) >= 0.0 )
            _rotation = mat3x3::RotateZ(angle) * _rotation;
        else
            _rotation = mat3x3::RotateZ(-angle) * _rotation;
    }
}

bool NC_STACK_ypabact::ypabact_func85(vec3d *arg)
{
    float tmp2 = arg->dot( _fly_dir * _fly_dir_length );

    if ( fabs(tmp2) > 15.0 )
        return true;

    return false;
}

void CrashOrLand__sub1(NC_STACK_ypabact *bact)
{
    if ( bact->_fly_dir.x < 0.0 )
        bact->_fly_dir.x -= 7.0;

    if ( bact->_fly_dir.z < 0.0 )
        bact->_fly_dir.z -= 7.0;

    if ( bact->_fly_dir.x >= 0.0 )
        bact->_fly_dir.x += 7.0;

    if ( bact->_fly_dir.z >= 0.0 )
        bact->_fly_dir.z += 7.0;

    if ( bact->_fly_dir_length < 15.0 )
        bact->_fly_dir_length = 15.0;

    float v4 = bact->_fly_dir.length();

    if ( v4 <= 0.001 )
        bact->_fly_dir = vec3d(0.0, 1.0, 0.0);
    else
        bact->_fly_dir /= v4;
}

void sub_48AB14(NC_STACK_ypabact *bact, const vec3d &vec)
{
    vec3d vaxis = bact->_rotation.AxisY() * vec;

    if ( vaxis.normalise() != 0.0 )
    {
        float angle = clp_acos( vec.dot(bact->_rotation.AxisY()) );

        if ( angle > 0.001 )
            bact->_rotation *= mat3x3::AxisAngle(vaxis, angle);
    }
}

void CrashOrLand__sub0(NC_STACK_ypabact *bact, int a2)
{
    bact->_status_flg |= BACT_STFLAG_SCALE;

    if ( bact->_scale_duration > bact->_scale_pos )
    {
        float v5 = bact->_maxrot * a2 / 1000.0;


        bact->_scale_speed += bact->_scale_accel * a2 / 1000.0;
        bact->_scale_start += bact->_scale_speed * (a2 / 1000.0);

        bact->_scale = vec3d(bact->_scale_start);

        bact->_rotation = mat3x3::RotateY(v5) * bact->_rotation;

        int v14 = 0;
        for (int i = 0; i < 32; i++)
        {
            if ( bact->_vp_fx_models[i] )
                v14++;
        }

        if ( v14 )
        {
            int v15 = bact->_scale_pos * v14 / bact->_scale_duration;

            bact->SetVP(bact->_vp_fx_models[v15]);
        }

        bact->_scale_pos += a2;
    }
    else
    {
        bact->_yls_time = -1;
        bact->Release();
    }
}

void NC_STACK_ypabact::MaintainIdleGroundSupport(int frameTime)
{
    if ( _bact_type == BACT_TYPES_BACT )
    {
        // A helicopter can enter idle before its landing tilt has recovered.
        bact_arg86 settle;
        settle.field_one = 0;
        settle.field_two = frameTime;
        CrashOrLand(&settle);
    }

    // Shape landing already found the hull's safe support pose.
    // A centre-ray snap would push its lower parts back into the ground.
    if ( HasCollisionShape() )
        return;

    ypaworld_arg136 ground;
    ground.stPos = _position;
    ground.vect = vec3d::OY(_overeof + 50.0);
    ground.flags = 0;
    _world->ypaworld_func136(&ground);

    if ( ground.isect )
        _position.y = ground.isectPos.y - _overeof;
}

size_t NC_STACK_ypabact::CrashOrLand(bact_arg86 *arg)
{
    yw_137col v58[10];

    int v85 = 0;
    const bool customFallDamage = (arg->field_one & 1) &&
                                  ypabact_IsCustomFallDamageConfigActive();
    bool fallDamageAppliedThisCall = false;

    auto applyFallDamageForContact = [&]()
    {
        if ( !(arg->field_one & 1) )
            return;

        // Missing/invalid configuration keeps the original per-contact
        // vanilla calculation. The percentage mode is stateful: a normal
        // ground correction is not a fall, and one airborne episode can
        // consume the configured damage only once.
        if ( !customFallDamage )
        {
            ypabact_ApplyFallDamage(this);
            return;
        }

        if ( !_fallDamageAirborne || _fallDamageConsumed || fallDamageAppliedThisCall )
            return;

        ypabact_ApplyFallDamage(this);
        _fallDamageConsumed = true;
        fallDamageAppliedThisCall = true;
    };

    auto resetFallDamageContact = [&]()
    {
        if ( customFallDamage )
        {
            _fallDamageAirborne = false;
            _fallDamageConsumed = false;
        }
    };

    if ( _status_flg & BACT_STFLAG_SEFFECT )
    {
        CrashOrLand__sub0(this, arg->field_two);
    }
    else
    {
        float v84;
        float v90;

        if ( _oflags & BACT_OFLAG_VIEWER )
        {
            v84 = _viewer_radius;
            v90 = _viewer_overeof;
        }
        else
        {
            v84 = _radius;
            v90 = _overeof;
        }

        if ( _bact_type == BACT_TYPES_ROBO )
            v90 = 60.0;

        vec3d vaxis = vec3d( -_rotation.m12, 0.0, _rotation.m10 );

        float v94 = arg->field_two / 1000.0;

        if ( vaxis.normalise() > 0.001 && !(arg->field_one & 1) )
        {
            float angle = clp_acos( _rotation.m11 );
            float maxrot = _maxrot * v94;

            if ( angle > maxrot )
                angle = maxrot;

            if ( fabs(angle) > BACT_MIN_ANGLE )
            {
                _rotation *= mat3x3::AxisAngle(vaxis, angle);
            }
        }

        if ( arg->field_one & 2 )
        {
            float v18 = fabs(_fly_dir_length) * v94 * 0.08;

            _rotation = mat3x3::RotateZ(v18) * _rotation;
        }

        if ( !(_status_flg & BACT_STFLAG_LAND) )
        {
            if ( _inlinePhysicalFX )
                _airconst = _airconst_static;
            else if ( arg->field_one & 1 )
                _airconst = 0;
            else
                _airconst = 500.0;

            for (int i = 0; i < 3; i++)
            {

                move_msg v66;

                v66.field_0 = v94;
                v66.flag = 1;

                Move(&v66);

                int v20 = 0;

                if ( _oflags & BACT_OFLAG_BACTCOLL )
                {
                    if ( CollisionWithBact(arg->field_two) )
                    {
                        if ( _bact_type == BACT_TYPES_TANK || _bact_type == BACT_TYPES_CAR )
                        {
                            CrashOrLand__sub1(this);
                            return 0;
                        }

                        return 0;
                    }
                }

                if ( (_oflags & BACT_OFLAG_VIEWER) || HasCollisionShape() )
                {
                    ypaworld_arg137 arg137;
                    arg137.pos = _fly_dir * _fly_dir_length * v94 * 6.0 + _position;
                    arg137.pos2 = _fly_dir;
                    arg137.radius = v84;
                    arg137.collisions = v58;
                    arg137.field_30 = 0;
                    arg137.coll_max = 10;

                    if (HasCollisionShape()) GetShapeWorldContact(&arg137);
                    else _world->ypaworld_func137(&arg137);

                    if ( arg137.coll_count )
                    {
                        int v24 = 0;
                        v85 = 1;

                        vec3d v98;

                        for (int j = arg137.coll_count - 1; j >= 0; j--)
                        {
                            yw_137col *v25 = &arg137.collisions[ j ];

                            v98 += v25->pos2;

                            if ( v98.y > 0.6 )
                                v24 = 1;
                        }

                        bact_arg88 arg88;
                        vec3d a2a;

                        float lnn = v98.length();

                        if ( lnn != 0.0 )
                        {
                            arg88.pos1 = v98 / lnn;

                            a2a = arg88.pos1;
                        }
                        else
                        {
                            a2a = _fly_dir;
                            arg88.pos1 = _fly_dir;
                        }

                        if ( arg->field_one & 1 )
                        {
                            applyFallDamageForContact();

                            if ( _inlinePhysicalFX ||
                                 _energy <= 0 ||
                                 (GetVP() == _vp_dead && _status == BACT_STATUS_DEAD) )
                            {
                                setState_msg arg78;
                                arg78.setFlags = BACT_STFLAG_DEATH2;
                                arg78.unsetFlags = 0;
                                arg78.newStatus = BACT_STATUS_NOPE;

                                SetState(&arg78);
                            }

                            bool playCrashlandSound = ypabact_ShouldPlayCrashlandSound(this, _fly_dir_length, 7.0f);
                            if ( playCrashlandSound )
                                ypabact_StartSoundOnce(this, 5);

                            if ( _oflags & BACT_OFLAG_USERINPT )
                            {
                                yw_arg180 arg180_1;

                                arg180_1.effects_type = 5;
                                arg180_1.field_4 = 1.0;
                                arg180_1.field_8 = v98.x * 10.0 + _position.x;
                                arg180_1.field_C = v98.z * 10.0 + _position.z;

                                _world->ypaworld_func180(&arg180_1);
                            }

                            if ( v98.y >= 0.6 && v24 )
                            {
                                if (!HasCollisionShape()) _position.y = _old_pos.y;

                                _status_flg |= BACT_STFLAG_LAND;
                                resetFallDamageContact();

                                _fly_dir_length *= _fly_dir.XZ().length();

                                sub_48AB14(this, a2a);

                                _reb_count = 0;
                            }
                            else
                            {
                                Recoil(&arg88);

                                _reb_count++;

                                v20 = 1;

                                if ( _reb_count > 50 )
                                {
                                    if ( !IsInvulnerableToDamage() )
                                        _energy = -10000;

                                    _status_flg |= BACT_STFLAG_LAND;
                                }
                            }
                        }
                        else if ( v98.y < 0.6 )
                        {
                            Recoil(&arg88);

                            v20 = 1;
                        }
                        else
                        {
                            if (!HasCollisionShape()) _position.y = _old_pos.y;
                            _fly_dir_length = 0;
                            _reb_count = 0;
                            _status_flg |= BACT_STFLAG_LAND;
                        }
                    }
                }

                // Live shaped units land on their hull contact. Falling wrecks
                // are outside the live shape scene and still need the ground ray.
                if ( !v85 && (!HasCollisionShape() || (arg->field_one & 1)) )
                {
                    ypaworld_arg136 arg136;
                    arg136.stPos = _old_pos;
                    arg136.vect = _position - _old_pos + vec3d(0.0, v90, 0.0);
                    arg136.flags = 0;

                    _world->ypaworld_func136(&arg136);

                    if ( arg136.isect )
                    {
                        bact_arg88 arg88;

                        arg88.pos1 = arg136.skel->polygons[arg136.polyID].Normal();

                        vec3d a2a = arg88.pos1;

                        if ( arg->field_one & 1 )
                        {
                            applyFallDamageForContact();

                            if ( _inlinePhysicalFX ||
                                 _energy <= 0 ||
                                 (GetVP() == _vp_dead && _status == BACT_STATUS_DEAD) )
                            {
                                setState_msg arg78;
                                arg78.setFlags = BACT_STFLAG_DEATH2;
                                arg78.unsetFlags = 0;
                                arg78.newStatus = BACT_STATUS_NOPE;

                                SetState(&arg78);
                            }

                            bool playCrashlandSound = ypabact_ShouldPlayCrashlandSound(this, _fly_dir_length, 7.0f);
                            if ( playCrashlandSound )
                                ypabact_StartSoundOnce(this, 5);

                            if ( _oflags & BACT_OFLAG_USERINPT )
                            {
                                yw_arg180 arg180;

                                arg180.effects_type = 5;
                                arg180.field_4 = 1.0;
                                arg180.field_8 = a2a.x * 10.0 + _position.x;
                                arg180.field_C = a2a.z * 10.0 + _position.z;

                                _world->ypaworld_func180(&arg180);
                            }

                            if ( arg136.skel->polygons[arg136.polyID].B < 0.6 )
                            {
                                Recoil(&arg88);

                                _reb_count++;

                                v20 = 1;

                                if ( _reb_count > 50 )
                                {
                                    if ( !_world || !_world->IsDebugGlobalInvulnerabilityEnabled() )
                                        _energy = -10000;
                                    _status_flg |= BACT_STFLAG_LAND;
                                }
                            }
                            else
                            {
                                _position = arg136.isectPos - vec3d(0.0, v90, 0.0);

                                _status_flg |= BACT_STFLAG_LAND;
                                resetFallDamageContact();

                                _fly_dir_length *= _fly_dir.XZ().length();

                                sub_48AB14(this, a2a);

                                _reb_count = 0;
                            }
                        }
                        else if ( arg136.skel->polygons[arg136.polyID].B < 0.6 )
                        {
                            Recoil(&arg88);

                            v20 = 1;
                        }
                        else
                        {
                            _position.y = arg136.isectPos.y - v90;

                            _fly_dir_length = 0;
                            _reb_count = 0;
                            _status_flg |= BACT_STFLAG_LAND;
                            resetFallDamageContact();
                        }
                    }
                    else if ( customFallDamage )
                    {
                        // No terrain contact during this step means that the
                        // next terrain impact belongs to a real airborne
                        // interval, not to a tank's ordinary ground correction.
                        _fallDamageAirborne = true;
                    }
                }

                if ( !v20 ) // Alternative exit from loop
                    break;
            }

        }
        if ( _status_flg & BACT_STFLAG_LAND )
        {
            return 1;
        }
    }
    return 0;
}

void CollisionWithBact__sub0(NC_STACK_ypabact *bact, NC_STACK_ypabact *a2)
{
    int v2 = a2->GetPlasmaDurationMs();

    int v3 = (float)a2->_scale_time * 0.2 / (float)v2 * (float)a2->_energy_max;

    if ( bact->_energy + v3 > bact->_energy_max )
    {
        NC_STACK_yparobo *robo = bact->_host_station;
        if ( !robo && bact->_bact_type == BACT_TYPES_ROBO )
            robo = static_cast<NC_STACK_yparobo *>(bact);

        int v10 = v3 - (bact->_energy_max - bact->_energy);

        bact->_energy = bact->_energy_max;

        // A Host Station is its own plasma overflow receiver. Detached/orphaned
        // units have no valid reserve destination, so safely keep the local
        // energy clamp and discard only the excess instead of dereferencing NULL.
        if ( !robo )
            return;

        if ( robo->_energy + v10 > robo->_energy_max )
        {
            int v14 = v10 - (robo->_energy_max - robo->_energy);

            robo->_energy = robo->_energy_max;

            if ( robo->_roboEnergyLife + v14 >= robo->_energy_max )
            {
                robo->_roboEnergyMove += v14 - (robo->_energy_max - robo->_roboEnergyLife);

                robo->_roboEnergyLife = robo->_energy_max;

                if ( robo->_roboEnergyMove > robo->_energy_max )
                    robo->_roboEnergyMove = robo->_energy_max;
            }
            else
            {
                robo->_roboEnergyLife += v14;
            }
        }
        else
        {
            robo->_energy += v10;
        }
    }
    else
    {
        bact->_energy += v3;
    }
}

void NC_STACK_ypabact::ApplyCompoundCollision(const World::rbcolls &coll,
                                                  bool radiusDefined)
{
    _collNodes = coll;
    _manualCompoundCollision = !_collNodes.roboColls.empty();
    _legacyRadiusDefined = radiusDefined;

    // Sphere layout is immutable at runtime, so resolve the enclosing extent
    // once instead of rescanning every sphere for each broad-phase query.
    // Padding remains dynamic and is added later.
    _manualCompoundBroadRadius = World::CompoundCollisionExtent(_collNodes);
}

void NC_STACK_ypabact::GetCollisionSpheres(
        std::vector<TCollisionSphereWorld> &out,
        const vec3d &position, const mat3x3 &rotation,
        bool useViewerSemantics)
{
    out.clear();

    World::rbcolls *colls = getBACT_collNodes();
    const bool manual = HasManualCompoundCollision();
    vec3d origin = position;

    if ( useViewerSemantics && colls && getBACT_viewer() )
        origin.y += _viewer_overeof - _overeof;

    const float legacyRadius =
        useViewerSemantics && getBACT_viewer() ? _viewer_radius : _radius;

    size_t reserveCount = colls ? colls->roboColls.size() : 0;
    if ( (manual && UsesLegacyRadiusCollision() && legacyRadius > 0.01f) ||
         (!manual && !colls && legacyRadius > 0.01f) )
    {
        reserveCount++;
    }
    if ( out.capacity() < reserveCount )
        out.reserve(reserveCount);

    if ( manual )
    {
        if ( UsesLegacyRadiusCollision() && legacyRadius > 0.01f )
            out.push_back({origin, legacyRadius, true});
    }
    else if ( !colls )
    {
        if ( legacyRadius > 0.01f )
            out.push_back({origin, legacyRadius, true});
        return;
    }

    if ( !colls )
        return;

    const mat3x3 rotationT = rotation.Transpose();
    for (const World::TRoboColl &sphere : colls->roboColls)
    {
        if ( sphere.robo_coll_radius <= 0.01f ||
             !std::isfinite(sphere.robo_coll_radius) )
            continue;

        out.push_back({
            origin + rotationT.Transform(sphere.coll_pos),
            sphere.robo_coll_radius,
            false
        });
    }
}

bool NC_STACK_ypabact::GetUnitCollisionContact(NC_STACK_ypabact *other,
                                                vec3d *selfCenter,
                                                vec3d *otherCenter,
                                                float *penetration)
{
    if ( !other || other == this )
        return false;
    if (IgnoresGenesisHostCollision(other) || other->IgnoresGenesisHostCollision(this))
        return false;

    if (HasCollisionShape() || other->HasCollisionShape())
    {
        Collision::Contact contact;
        if (!_world || !_world->_collisionScene ||
            !_world->_collisionScene->PairContact(this, other, &contact)) return false;
        if (selfCenter) *selfCenter = contact.point + contact.normal * contact.depth;
        if (otherCenter) *otherCenter = contact.point - contact.normal;
        if (penetration) *penetration = contact.depth;
        return true;
    }

    vec3d selfOrigin = _position;
    vec3d otherOrigin = other->_position;
    if ( getBACT_collNodes() && getBACT_viewer() )
        selfOrigin.y += _viewer_overeof - _overeof;
    if ( other->getBACT_collNodes() && other->getBACT_viewer() )
        otherOrigin.y += other->_viewer_overeof - other->_overeof;

    const float broad = GetCollisionBroadRadius() + other->GetCollisionBroadRadius();
    if ( broad <= 0.01f || (selfOrigin - otherOrigin).square() > broad * broad )
        return false;

    std::vector<TCollisionSphereWorld> selfSpheres;
    std::vector<TCollisionSphereWorld> otherSpheres;
    GetCollisionSpheres(selfSpheres, _position, _rotation, true);
    other->GetCollisionSpheres(otherSpheres, other->_position, other->_rotation, true);

    float bestPenetration = 0.0f;
    vec3d bestSelf;
    vec3d bestOther;

    for (const TCollisionSphereWorld &a : selfSpheres)
    {
        for (const TCollisionSphereWorld &b : otherSpheres)
        {
            const float radiusSum = a.radius + b.radius;
            const vec3d delta = b.center - a.center;
            const float distanceSq = delta.square();
            if ( distanceSq >= radiusSum * radiusSum )
                continue;

            // Only overlapping sphere pairs need the square root required for
            // the exact penetration depth used by the existing response.
            const float overlap = radiusSum - sqrtf(distanceSq);
            if ( overlap > bestPenetration )
            {
                bestPenetration = overlap;
                bestSelf = a.center;
                bestOther = b.center;
            }
        }
    }

    if ( bestPenetration <= 0.0f )
        return false;

    if ( selfCenter )
        *selfCenter = bestSelf;
    if ( otherCenter )
        *otherCenter = bestOther;
    if ( penetration )
        *penetration = bestPenetration;
    return true;
}

bool NC_STACK_ypabact::ResolveGenesisCompoundOverlap(int frameTime)
{
    if (HasCollisionShape()) return false; // The common shape solver includes stationary genesis.
    if ( _status != BACT_STATUS_CREATE || !getBACT_bactCollisions() || !_pSector ||
         IsDestroyed() || !ypabact_IsGenesisSeparationVehicle(this) )
        return false;

    vec3d correction(0.0, 0.0, 0.0);
    vec3d strongestDir(0.0, 0.0, 0.0);
    float strongestPenetration = 0.0f;
    int contacts = 0;

    float frameScale = (float)frameTime / 18.0f;
    frameScale = std::max(0.25f, std::min(frameScale, 2.0f));

    for (NC_STACK_ypabact *other : _world->SnapshotBacts(_pSector->unitsList))
    {
        if (other && other->HasCollisionShape()) continue; // Common solver owns mixed contacts too.
        if ( !other || other == this || !ypabact_IsGenesisSeparationVehicle(other) ||
             other->IsDestroyed() || other->_status == BACT_STATUS_DEAD )
            continue;

        vec3d selfCenter;
        vec3d otherCenter;
        float penetration = 0.0f;
        if ( !GetUnitCollisionContact(other, &selfCenter, &otherCenter, &penetration) ||
             penetration <= 0.75f )
            continue;

        // Genesis separation is deliberately horizontal for every class. It
        // clears overlapping spawn volumes without changing flight altitude,
        // terrain support or the first post-genesis fall/landing behaviour.
        vec3d away = _position - other->_position;
        away.y = 0.0f;

        if ( away.normalise() <= 0.001f )
        {
            away = selfCenter - otherCenter;
            away.y = 0.0f;
        }

        if ( away.normalise() <= 0.001f )
        {
            // Exact same-position spawns need a deterministic opposite pair
            // direction; otherwise both units remain welded indefinitely.
            uintptr_t selfKey = reinterpret_cast<uintptr_t>(this);
            uintptr_t otherKey = reinterpret_cast<uintptr_t>(other);
            uintptr_t lo = std::min(selfKey, otherKey);
            uintptr_t hi = std::max(selfKey, otherKey);
            uint32_t hash = (uint32_t)((lo >> 4) ^ (hi >> 9) ^ (lo * 2654435761u));
            float angle = (float)(hash % 6283u) * 0.001f;
            away = vec3d(cos(angle), 0.0, sin(angle));
            if ( selfKey > otherKey )
                away = -away;
        }

        float pairStep = std::min((penetration - 0.75f) * 0.55f, 14.0f * frameScale);
        correction += away * pairStep;
        contacts++;

        if ( penetration > strongestPenetration )
        {
            strongestPenetration = penetration;
            strongestDir = away;
        }
    }

    if ( contacts == 0 )
        return false;

    if ( correction.XZ().length() <= 0.001f )
        correction = strongestDir * std::min(strongestPenetration * 0.5f, 8.0f * frameScale);

    float correctionLength = correction.XZ().length();
    float maxTotalStep = 18.0f * frameScale;
    if ( correctionLength > maxTotalStep && correctionLength > 0.001f )
        correction *= maxTotalStep / correctionLength;

    vec3d candidate = _position + correction;
    candidate.y = _position.y;

    ypaworld_arg136 worldMove;
    worldMove.stPos = _position;
    worldMove.vect = candidate - _position;
    worldMove.flags = 0;
    _world->ypaworld_func136(&worldMove);

    if ( worldMove.isect )
        return false;

    // Preserve apparent velocity/history while moving only the spawn volume.
    _position = candidate;
    _old_pos += correction;
    CorrectPositionInLevelBox(nullptr);
    return true;
}

float NC_STACK_ypabact::GetCollisionBroadRadius()
{
    if (HasCollisionShape()) return _collisionShape->radius;
    float broadRadius = UsesLegacyRadiusCollision() ? _radius : 0.0f;
    World::rbcolls *colls = getBACT_collNodes();
    if ( !colls )
        return broadRadius;

    const float padding = getBACT_collPadding();
    if ( HasManualCompoundCollision() )
    {
        if ( _manualCompoundBroadRadius >= 0.0f )
        {
            const float compoundRadius = _manualCompoundBroadRadius + padding;
            if ( compoundRadius > broadRadius )
                broadRadius = compoundRadius;
        }
        return broadRadius;
    }

    // Robo keeps its native collision set, so preserve the existing dynamic path.
    for (const World::TRoboColl &sphere : colls->roboColls)
    {
        if ( sphere.robo_coll_radius <= 0.01 )
            continue;

        const float extent = sphere.coll_pos.length() + sphere.robo_coll_radius + padding;
        if ( extent > broadRadius )
            broadRadius = extent;
    }

    return broadRadius;
}

void NC_STACK_ypabact::HandleUnitCollisionContact(NC_STACK_ypabact *other, int frameTime)
{
    if ( !other || other == this || !_world || other->_world != _world )
        return;

    if ( _inlinePhysicalFX )
        ypabact_ApplyInlinePhysicalImpactDamage(this, other);

    if ( IsDestroyed() || other->IsDestroyed() )
        return;

    // Both actors can evaluate the same overlap in the same update. Keep one
    // pair-level contact event, and keep refreshing it while the bodies remain
    // touching so a continuous push never reapplies fixed damage every frame.
    if ( !ypabact_BeginUnitCollisionEvent(this, other, frameTime) )
        return;

    if ( _owner == World::OWNER_0 || other->_owner == World::OWNER_0 )
        return;

    const bool friendlyCollision = _owner == other->_owner;
    Common::Ini::Key &damageKey = friendlyCollision
                                ? System::IniConf::GameUnitFriendlyCollisionEnergy
                                : System::IniConf::GameUnitEnemyCollisionEnergy;
    World::TAbsoluteOrPercent damageConfig;
    if ( !ypabact_ReadUnitCollisionDamage(damageKey, &damageConfig) ||
         damageConfig.value <= 0.0f )
        return;

    const auto resolveRawDamage = [&damageConfig](int maxEnergy) -> int
    {
        double raw = damageConfig.percent
            ? (double)std::max(maxEnergy, 0) * (double)damageConfig.value / 100.0
            : (double)damageConfig.value;
        if ( !std::isfinite(raw) || raw <= 0.0 )
            return 0;
        raw = std::min(raw, (double)std::numeric_limits<int>::max());
        return std::max(1, (int)ceil(raw));
    };

    const int selfRawDamage = resolveRawDamage(_energy_max);
    const int otherRawDamage = resolveRawDamage(other->_energy_max);
    const int selfDamage = CalcShieldedCustomDamage(selfRawDamage);
    const int otherDamage = other->CalcShieldedCustomDamage(otherRawDamage);

    if ( selfDamage > 0 )
    {
        bact_arg84 damage;
        damage.energy = -selfDamage;
        damage.unit = other;
        damage.bypassAttackerDamageModifiers = true;
        ModifyEnergy(&damage);
    }

    if ( otherDamage > 0 )
    {
        bact_arg84 damage;
        damage.energy = -otherDamage;
        damage.unit = this;
        damage.bypassAttackerDamageModifiers = true;
        other->ModifyEnergy(&damage);
    }
}

size_t NC_STACK_ypabact::CollisionWithBact(int arg)
{
    Collision::Contact shapeContact;
    if (_world && _world->_collisionScene &&
        _world->_collisionScene->TakeUnitContact(this, &shapeContact))
        return HandleShapeUnitContact(shapeContact, arg);
    if (HasCollisionShape()) return 0; // Swept contacts are resolved by Move/shared Update.
    bool isViewer = getBACT_viewer();
    if ( _fly_dir_length == 0.0 || !_pSector )
        return 0;

    const float trad = isViewer ? _viewer_radius : _radius;
    World::rbcolls *selfColls = getBACT_collNodes();
    const bool selfManual = HasManualCompoundCollision();
    const bool selfHasNativeColls = selfColls && !selfManual;
    bool sawVanillaTargetColls = false;

    vec3d manualCenters(0.0, 0.0, 0.0);
    vec3d vanillaCenters(0.0, 0.0, 0.0);
    int manualCount = 0;
    int vanillaCount = 0;
    std::vector<NC_STACK_ypabact *> manualContacts;
    std::vector<NC_STACK_ypabact *> vanillaContacts;

    for ( NC_STACK_ypabact *bnode : _world->SnapshotBacts(_pSector->unitsList) )
    {
        const bool plasma = CanCollectPlasmaFrom(bnode);

        if (bnode && bnode->HasCollisionShape()) continue;

        if ( !bnode || bnode == this || bnode->_bact_type == BACT_TYPES_MISSLE ||
             (bnode->IsDestroyed() && !plasma) )
            continue;
        if (IgnoresGenesisHostCollision(bnode) || bnode->IgnoresGenesisHostCollision(this))
            continue;

        const bool pairUsesManualCompound = selfManual || bnode->HasManualCompoundCollision();
        if ( pairUsesManualCompound )
        {
            vec3d selfCenter;
            vec3d targetCenter;
            if ( !GetUnitCollisionContact(bnode, &selfCenter, &targetCenter, nullptr) )
                continue;

            if ( plasma )
            {
                CollectPlasmaFrom(bnode);
                continue;
            }

            manualCenters += _position + (targetCenter - selfCenter);
            manualCount++;
            if ( std::find(manualContacts.begin(), manualContacts.end(), bnode) == manualContacts.end() )
                manualContacts.push_back(bnode);
            continue;
        }

        // Vanilla radius / Robo compound path. This intentionally preserves the
        // original asymmetry and the Robo gate: a Robo's native collision nodes
        // do not make it recoil against ordinary single-radius units. Restoring
        // that rule prevents stationary floating Host Stations from being nudged
        // continuously by nearby simple actors or mounted units.
        World::rbcolls *targetColls = bnode->getBACT_collNodes();
        const int targetCount = targetColls ? (int)targetColls->roboColls.size() : 1;
        if ( targetColls )
            sawVanillaTargetColls = true;
        const mat3x3 targetRotationT =
            targetColls ? bnode->_rotation.Transpose() : mat3x3::Ident();

        for (int i = targetCount - 1; i >= 0; i--)
        {
            float targetRadius = trad;
            vec3d targetPosition = bnode->_position;

            if ( targetColls )
            {
                const World::TRoboColl &sphere = targetColls->roboColls[i];
                targetRadius = sphere.robo_coll_radius;
                if ( targetRadius < 0.01f )
                    continue;

                targetPosition += targetRotationT.Transform(sphere.coll_pos);
            }

            const float collisionRadius = trad + targetRadius;
            if ( collisionRadius <= 0.0f ||
                 (_position - targetPosition).square() > collisionRadius * collisionRadius )
            {
                continue;
            }

            if ( plasma )
            {
                CollectPlasmaFrom(bnode);
                break;
            }

            vanillaCenters += targetPosition;
            vanillaCount++;
            if ( std::find(vanillaContacts.begin(), vanillaContacts.end(), bnode) == vanillaContacts.end() )
                vanillaContacts.push_back(bnode);
        }
    }

    const bool useVanillaContacts = vanillaCount > 0 &&
                                    (!selfHasNativeColls || sawVanillaTargetColls);
    const int collisionCount = manualCount + (useVanillaContacts ? vanillaCount : 0);
    if ( collisionCount == 0 )
    {
        _status_flg &= ~BACT_STFLAG_BCRASH;
        return 0;
    }

    for (NC_STACK_ypabact *contact : manualContacts)
        HandleUnitCollisionContact(contact, arg);
    if ( useVanillaContacts )
    {
        for (NC_STACK_ypabact *contact : vanillaContacts)
            HandleUnitCollisionContact(contact, arg);
    }

    vec3d collisionCenters = manualCenters;
    if ( useVanillaContacts )
        collisionCenters += vanillaCenters;
    collisionCenters /= (double)collisionCount;

    vec3d collisionDir = collisionCenters - _position;
    float collisionDirLength = collisionDir.length();
    if ( collisionDirLength < 0.0001 )
        return 0;

    bact_arg88 recoil;
    recoil.pos1 = collisionDir / collisionDirLength;

    if ( clp_acos(collisionDir.dot(_fly_dir)) > C_PI_2 )
        return 0;

    if ( !(_status_flg & BACT_STFLAG_BCRASH) )
    {
        if ( !_soundcarrier.Sounds[6].IsEnabled() )
            SFXEngine::SFXe.startSound(&_soundcarrier, 6);
        _status_flg |= BACT_STFLAG_BCRASH;

        if ( isViewer )
        {
            yw_arg180 effect;
            effect.field_4 = 1.0;
            effect.field_8 = collisionCenters.x;
            effect.field_C = collisionCenters.z;
            effect.effects_type = 5;
            _world->ypaworld_func180(&effect);
        }
    }

    if ( fabs(_fly_dir_length) < 0.1 )
        _fly_dir_length = 1.0;

    Recoil(&recoil);
    _target_vec = _fly_dir;
    _AI_time1 = _clock;
    _AI_time2 = _clock;
    return 1;
}

void NC_STACK_ypabact::SmoothStabilizeUpright(float frameTime)
{
    vec3d vaxis = _rotation.AxisY() * vec3d(0.0, 1.0, 0.0);

    if ( vaxis.normalise() <= 0.001 )
        return;

    float remainingAngle = clp_acos( _rotation.AxisY().dot( vec3d(0.0, 1.0, 0.0) ) );
    float angleStep = std::min(remainingAngle, _maxrot * frameTime * 0.001f);

    // The final frame uses the remaining angular distance too: no axis reset,
    // no visible snap, and no residual tilt.
    if ( angleStep > 0.000001f )
        _rotation *= mat3x3::AxisAngle(vaxis, angleStep);
}

bool NC_STACK_ypabact::IgnoresGenesisHostCollision(const NC_STACK_ypabact *other) const
{
    if (!_host_station || !other || !ypabact_IsGenesisSeparationVehicle(this) ||
        other->_status == BACT_STATUS_DEAD || (other->_status_flg & BACT_STFLAG_DEATH1) ||
        _status == BACT_STATUS_DEAD || (_status_flg & BACT_STFLAG_DEATH1) ||
        (_status != BACT_STATUS_CREATE && !_genesisExitPending)) return false;
    while (other)
    {
        if (other == _host_station) return true;
        const auto *gun=other->_bact_type == BACT_TYPES_GUN
            ? dynamic_cast<const NC_STACK_ypagun *>(other) : nullptr;
        const bool mounted=gun && (gun->_gunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO);
        if (!other->_isUnitGunChild && !other->_isDummy && !mounted) return false;
        const auto *parent=other->_parent ? other->_parent : (mounted ? other->_host_station : nullptr);
        if (parent == other) return false;
        other=parent;
    }
    return false;
}

void NC_STACK_ypabact::UpdateGenesisHostExit()
{
    if (!_genesisExitPending || _status == BACT_STATUS_CREATE) return;
    if (!_host_station || _host_station->_status == BACT_STATUS_DEAD ||
        (_host_station->_status_flg & BACT_STFLAG_DEATH1) ||
        _status == BACT_STATUS_DEAD || (_status_flg & BACT_STFLAG_DEATH1))
    {
        _genesisExitPending=false;
        return;
    }
    const auto footprint=[&](NC_STACK_ypabact *actor, vec2d &lo, vec2d &hi) {
        if (actor->HasCollisionShape() && _world && _world->_collisionScene)
            return _world->_collisionScene->ActorFootprint(actor, &lo, &hi);
        std::vector<TCollisionSphereWorld> spheres;
        actor->GetCollisionSpheres(spheres, actor->_position, actor->_rotation, true);
        if (spheres.empty()) return false;
        lo=vec2d(1e30,1e30); hi=vec2d(-1e30,-1e30);
        for (const auto &sphere : spheres)
        {
            double radius=sphere.radius;
            // The native single-radius path tests targets with the mover's radius.
            if (actor != this && !HasCollisionShape() && !HasManualCompoundCollision() &&
                !actor->getBACT_collNodes())
                radius=std::max(radius, double(getBACT_viewer() ? _viewer_radius : _radius));
            lo.x=std::min(lo.x,sphere.center.x-radius);
            lo.y=std::min(lo.y,sphere.center.z-radius);
            hi.x=std::max(hi.x,sphere.center.x+radius);
            hi.y=std::max(hi.y,sphere.center.z+radius);
        }
        return true;
    };
    vec2d unitMin, unitMax;
    if (!footprint(this, unitMin, unitMax)) { _genesisExitPending=false; return; }
    vec2d hostMin(1e30,1e30), hostMax(-1e30,-1e30);
    bool hasBody=false;
    const auto include=[&](const auto &visit, NC_STACK_ypabact *part) -> void {
        if (part->_status == BACT_STATUS_DEAD || (part->_status_flg & BACT_STFLAG_DEATH1)) return;
        vec2d lo, hi;
        if (footprint(part, lo, hi))
        {
            hasBody=true;
            hostMin.x=std::min(hostMin.x,lo.x); hostMin.y=std::min(hostMin.y,lo.y);
            hostMax.x=std::max(hostMax.x,hi.x); hostMax.y=std::max(hostMax.y,hi.y);
        }
        for (auto *child : part->GetKidList())
            if (IgnoresGenesisHostCollision(child)) visit(visit, child);
    };
    include(include, _host_station);
    // Keep the first exit open across gaps between the host and its mounts.
    // Once outside this complete footprint, a later re-entry is solid again.
    if (!hasBody || unitMax.x<hostMin.x || unitMin.x>hostMax.x ||
        unitMax.y<hostMin.y || unitMin.y>hostMax.y)
        _genesisExitPending=false;
}

void NC_STACK_ypabact::PlaceGenesisAboveTerrain()
{
    if (!_world) return;
    if (_world->_collisionScene) _world->_collisionScene->PlaceAboveTerrain(this);
    else
    {
        Collision::Scene terrain(*_world);
        terrain.PlaceAboveTerrain(this);
    }
    _genesisGroundChecked = true;
    _tForm.Pos = GetBodyPosition();
    _tForm.SclRot = (_status_flg & BACT_STFLAG_SCALE)
        ? _rotation.Transpose() * mat3x3::Scale(_scale) : _rotation.Transpose();
}

void NC_STACK_ypabact::CorrectPositionOnLand()
{
    float radius;
    if ( _viewer_radius >= 32.0 )
        radius = _viewer_radius;
    else
        radius = 32.0;

    yw_137col coltmp[10];

    ypaworld_arg137 arg137;
    arg137.pos = _position;
    arg137.pos2 = _rotation.AxisX();
    arg137.coll_max = 10;
    arg137.radius = radius;
    arg137.field_30 = 0;
    arg137.collisions = coltmp;

    if (HasCollisionShape()) arg137.coll_count = 0;
                else _world->ypaworld_func137(&arg137);

    vec3d tmp(0.0, 0.0, 0.0);

    float trad = 0.0;

    for (int i = arg137.coll_count - 1; i >= 0; i-- )
    {
        yw_137col *clsn = &arg137.collisions[i];

        if ( clsn->pos2.y < 0.6 )
        {
            vec3d tmp2 = _position - clsn->pos1;

            tmp += clsn->pos2;

            float v36 = radius - tmp2.length();

            if ( trad == 0.0 || trad < v36 )
                trad = v36;
        }
    }

    if ( _viewer_radius >= 32.0 )
        radius = _viewer_radius;
    else
        radius = 32.0;

    arg137.pos = _position;
    arg137.pos2 = -_rotation.AxisX();
    arg137.coll_max = 10;
    arg137.radius = radius;
    arg137.field_30 = 0;
    arg137.collisions = coltmp;

    if (HasCollisionShape()) arg137.coll_count = 0;
                else _world->ypaworld_func137(&arg137);

    for (int i = arg137.coll_count - 1; i >= 0; i-- )
    {
        yw_137col *clsn = &arg137.collisions[i];

        if ( clsn->pos2.y < 0.6 )
        {
            vec3d tmp2 = _position - clsn->pos1;

            tmp += clsn->pos2;

            float v36 = radius - tmp2.length();

            if ( trad == 0.0 || trad < v36 )
                trad = v36;
        }
    }

    float v25 = tmp.length();

    if ( v25 > 0.0001 )
        tmp /= v25;

    _position -= tmp * trad;
}

void NC_STACK_ypabact::CorrectPositionInLevelBox(void *)
{
    int v4 = 0;

    constexpr float CurSectrLen = World::CVSectorLength + 10.0;

    if ( _position.x > _wrldSize.x - CurSectrLen )
    {
        v4 = 1;
        _position.x = _wrldSize.x - CurSectrLen;
    }

    if ( _position.x < CurSectrLen )
    {
        v4 = 1;
        _position.x = CurSectrLen;
    }

    if ( _position.z > -CurSectrLen )
    {
        v4 = 1;
        _position.z = -CurSectrLen;
    }

    if ( _position.z < _wrldSize.y + CurSectrLen )
    {
        v4 = 1;
        _position.z = _wrldSize.y + CurSectrLen;
    }

    if ( _oflags & BACT_OFLAG_VIEWER )
    {
        if ( v4 )
        {
            if ( _bact_type != BACT_TYPES_TANK && _bact_type != BACT_TYPES_CAR )
            {
                ypaworld_arg136 arg136;

                arg136.stPos = _position - vec3d::OY(100.0);

                arg136.vect = vec3d::OY(_viewer_overeof + 100.0);
                arg136.flags = 0;

                _world->ypaworld_func136(&arg136);

                if ( arg136.isect )
                    _position.y = arg136.isectPos.y - _viewer_overeof;
            }
        }
    }
}

void NC_STACK_ypabact::setBACT_exactCollisions(bool col)
{
    if ( col )
        _oflags |= BACT_OFLAG_EXACTCOLL;
    else
        _oflags &= ~BACT_OFLAG_EXACTCOLL;
}

void NC_STACK_ypabact::setBACT_bactCollisions(bool col)
{
    if ( col )
        _oflags |= BACT_OFLAG_BACTCOLL;
    else
        _oflags &= ~BACT_OFLAG_BACTCOLL;
}
