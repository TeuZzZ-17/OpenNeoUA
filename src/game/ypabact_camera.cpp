// Cockpit camera and alternative views of a unit.
#include "game/ypabact_internal.h"

bool ypabact_IsCockpitCameraSupportedType(const NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;

    switch ( unit->_bact_type )
    {
        case BACT_TYPES_BACT:
        case BACT_TYPES_TANK:
        case BACT_TYPES_FLYER:
        case BACT_TYPES_UFO:
        case BACT_TYPES_CAR:
            return true;

        case BACT_TYPES_GUN:
        {
            const NC_STACK_ypagun *gun = dynamic_cast<const NC_STACK_ypagun *>(unit);
            return gun && !(gun->_gunFlags & NC_STACK_ypagun::GUN_FLAGS_ROBO);
        }

        default:
            return false;
    }
}

bool NC_STACK_ypabact::IsCockpitCameraAvailable() const
{
    return ypabact_IsCockpitCameraSupportedType(this) &&
           _world &&
           _world->_userUnit == this &&
           _world->_viewerBact == this &&
           (_oflags & BACT_OFLAG_VIEWER) &&
           (_oflags & BACT_OFLAG_USERINPT);
}

bool NC_STACK_ypabact::IsCockpitCameraActive() const
{
    // Alternative View temporarily supersedes the configured first-person camera
    // without changing the Nucleus.ini cockpit setting. Turning Alternative View off
    // therefore returns to the exact camera mode that was active before.
    return !_alternativeViewActive &&
           IsCockpitCameraAvailable() &&
           _world &&
           _world->_GameShell &&
           _world->_GameShell->cockpitCameraRuntimeMode;
}

bool NC_STACK_ypabact::IsPlayerFirstPersonCameraActive() const
{
    return _world &&
           _world->_userUnit == this &&
           _world->_viewerBact == this &&
           (_oflags & BACT_OFLAG_VIEWER) &&
           (_oflags & BACT_OFLAG_USERINPT) &&
           !IsCockpitCameraActive();
}

bool NC_STACK_ypabact::ShouldRenderCockpitCameraBody() const
{
    return IsCockpitCameraActive() && !IsCockpitCameraSilhouetteHidden();
}

bool NC_STACK_ypabact::IsCockpitCameraSilhouetteHidden() const
{
    // Attachments render as separate units, so the owning body is resolved first
    // and the whole vehicle disappears together with its gun children.
    const NC_STACK_ypabact *owner = this;
    if ( (_isUnitGunChild || _isDummy) && _parent && _parent != this )
        owner = _parent;

    return owner->IsCockpitCameraActive() && owner->_cockpit_camera_hide_vehicle;
}

vec3d NC_STACK_ypabact::GetBodyPosition() const
{
    const NC_STACK_ypabact *owner = this;
    if ( (_isUnitGunChild || _isDummy) && _parent && _parent != this )
        owner = _parent;

    vec3d position = _position;
    if ( (owner->_bact_type == BACT_TYPES_TANK || owner->_bact_type == BACT_TYPES_CAR) &&
         owner->IsCockpitCameraActive() )
    {
        // Ground controllers store the viewer origin at vwr_overeof, whereas
        // the authored body (and existing compound collision origin) uses overeof.
        // Camera, body, attachments and muzzle must share this conversion. Keep
        // it while airborne too, so landing does not change their relative pose.
        position.y += owner->_viewer_overeof - owner->_overeof;
    }
    return position;
}

vec3d NC_STACK_ypabact::GetCockpitCameraPosition() const
{
    return GetBodyPosition() + _rotation.Transpose().Transform(_cockpit_camera_offset);
}

vec3d NC_STACK_ypabact::GetCockpitCameraViewPosition() const
{
    vec3d position = GetCockpitCameraPosition();

    if ( !IsCockpitCameraActive() ||
         _bact_type != BACT_TYPES_GUN ||
         _cockpit_gun_camera_recoil <= 0.0f )
        return position;

    // Reuse the original gun first-person recoil displacement without feeding it
    // back into aiming, projectile origins, physics or the generic recoil system.
    return position + _rotation.Transpose().Transform(
        _viewer_position * _cockpit_gun_camera_recoil);
}

void NC_STACK_ypabact::ToggleCockpitCameraMode()
{
    if ( !_world || !_world->_GameShell )
        return;

    // Intentionally retained as an internal legacy POV/cockpit switch. OpenNeoUA no longer
    // exposes or persists this action; Nucleus.ini selects the session default.
    // The two views are intentionally mutually exclusive. Do not leave an
    // Alternative View camera state latched behind a manual cockpit-camera change.
    ResetAlternativeView();
    _world->_GameShell->cockpitCameraRuntimeMode = !_world->_GameShell->cockpitCameraRuntimeMode;
}

bool NC_STACK_ypabact::IsAlternativeViewAvailable()
{
    if ( !_world || _world->_isNetGame ||
         _world->_userUnit != this || _world->_viewerBact != this ||
         !(_oflags & BACT_OFLAG_VIEWER) || !(_oflags & BACT_OFLAG_USERINPT) ||
         _status == BACT_STATUS_DEAD || (_status_flg & (BACT_STFLAG_DEATH1 | BACT_STFLAG_DEATH2)) )
        return false;

    // Host Stations keep their existing camera behavior. Missile actors are not
    // normal directly controlled vehicles and must never latch this player view.
    return _bact_type != BACT_TYPES_ROBO && _bact_type != BACT_TYPES_MISSLE;
}

bool NC_STACK_ypabact::UsesDownwardAlternativeView()
{
    if ( !_world )
        return false;

    int weaponId = GetCurrentWeaponId();
    if ( weaponId < 0 || weaponId >= (int)_world->GetWeaponsProtos().size() )
        return false;

    const World::TWeapProto &wproto = _world->GetWeaponsProtos().at(weaponId);
    return wproto.IsBombLike();
}

vec3d NC_STACK_ypabact::GetAlternativeViewAimDirection() const
{
    // In Urban Assault world space +Y points toward the ground (gravity).
    return vec3d::OY(1.0);
}

mat3x3 NC_STACK_ypabact::GetAlternativeViewRotation()
{
    if ( !UsesDownwardAlternativeView() )
    {
        // Exact 180-degree local look-back: reverse right/forward while keeping
        // the vehicle's up axis. Physical orientation and weapon aim are untouched.
        mat3x3 rear = _rotation;
        rear.SetX(-_rotation.AxisX());
        rear.SetY(_rotation.AxisY());
        rear.SetZ(-_rotation.AxisZ());
        return rear;
    }

    const vec3d down = GetAlternativeViewAimDirection();

    // Bomb-like current weapons (incl. Vertical Laser) keep the straight-down bomber view.
    // Keep the vehicle's local right axis horizontal so its nose remains at the
    // top of the screen. The physical _rotation is never touched.
    vec3d right = _rotation.AxisX().X0Z();
    if ( right.normalise() <= 0.001 )
    {
        vec3d forward = _rotation.AxisZ().X0Z();
        if ( forward.normalise() > 0.001 )
            right = down * forward;
    }

    if ( right.normalise() <= 0.001 )
        right = vec3d::OX(1.0);

    vec3d screenDown = down * right;
    if ( screenDown.normalise() <= 0.001 )
        screenDown = vec3d::OZ(-1.0);

    mat3x3 view = mat3x3::Ident();
    view.SetX(right);
    view.SetY(screenDown);
    view.SetZ(down);
    return view;
}

bool NC_STACK_ypabact::SetAlternativeViewActive(bool active)
{
    if ( !active )
    {
        ResetAlternativeView();
        return true;
    }

    if ( !IsAlternativeViewAvailable() )
    {
        ResetAlternativeView();
        return false;
    }

    _alternativeViewActive = true;
    return true;
}

void NC_STACK_ypabact::ResetAlternativeView()
{
    _alternativeViewActive = false;
}

vec3d ypabact_GetCockpitViewDirection(NC_STACK_ypabact *bact, const vec3d &viewDir)
{
    vec3d forward = viewDir;
    if ( forward.normalise() > 0.001 )
        return forward;

    if ( bact->getBACT_extraViewer() )
        forward = bact->_viewer_rotation.AxisZ();
    else
        forward = bact->_rotation.AxisZ();

    if ( forward.normalise() <= 0.001 )
        forward = vec3d::OZ(1.0);

    return forward;
}

vec3d ypabact_GetCockpitAimTarget(NC_STACK_ypabact *bact, const vec3d &viewDir, float range)
{
    vec3d forward = ypabact_GetCockpitViewDirection(bact, viewDir);
    vec3d cameraPos = bact->GetCockpitCameraPosition();
    vec3d farTarget = cameraPos + forward * range;

    ypaworld_arg136 ray;
    ray.stPos = cameraPos;
    ray.vect = forward * range;
    ray.flags = 0;

    bact->getBACT_pWorld()->ypaworld_func136(&ray);

    if ( ray.isect )
        return ray.isectPos;

    return farTarget;
}

vec3d ypabact_GetCockpitAimDirection(NC_STACK_ypabact *bact, const vec3d &origin, const vec3d &viewDir, const vec3d &fallbackDir, float range)
{
    if ( !bact || !bact->IsCockpitCameraActive() || !bact->getBACT_pWorld() )
        return fallbackDir;

    vec3d dir = ypabact_GetCockpitAimTarget(bact, viewDir, range) - origin;
    if ( dir.normalise() > 0.001 )
        return dir;

    return fallbackDir;
}
