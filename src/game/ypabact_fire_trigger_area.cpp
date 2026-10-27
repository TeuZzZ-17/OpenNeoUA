// Shared optional AI acquisition volumes. Firing patterns and timing stay in LaunchMissile.
#include "game/ypabact_internal.h"

namespace {
// Distance to an elliptical cone truncated at depth. A single scalar projection
// solves the side; if that projection is past the cap, project onto its ellipse.
// This keeps broad collision spheres touching the rim, without a rectangular cone.
double ConeDistanceSquared(double x, double y, double z, const TFireTriggerArea &box)
{
    const double a = box.coneX, b = box.coneY;
    if ( z <= box.depth && x*x/(a*a) + y*y/(b*b) <= z*z )
        return 0.0;

    double px = x, py = y, pz = z;
    if ( x*x/(a*a) + y*y/(b*b) > z*z )
    {
        double lo = 0.0, hi = 1.0;
        if ( z > 0.0 )
        {
            for (int i = 0; i < 40; ++i)
            {
                const double t = (lo + hi) * 0.5;
                const double cx = x / (1.0 + t/(a*a));
                const double cy = y / (1.0 + t/(b*b));
                const double cz = z / (1.0 - t);
                if ( cx*cx/(a*a) + cy*cy/(b*b) > cz*cz ) lo = t;
                else hi = t;
            }
        }
        const double t = (lo + hi) * 0.5;
        px = x / (1.0 + t/(a*a));
        py = y / (1.0 + t/(b*b));
        // At the apex plane the multiplier is exactly one.
        if ( z == 0.0 )
        {
            px = x / (1.0 + 1.0/(a*a));
            py = y / (1.0 + 1.0/(b*b));
        }
        pz = sqrt(px*px/(a*a) + py*py/(b*b));
    }

    if ( pz > box.depth )
    {
        const double ax = a * box.depth, by = b * box.depth;
        px = x; py = y; pz = box.depth;
        if ( x*x/(ax*ax) + y*y/(by*by) > 1.0 )
        {
            double lo = 0.0, hi = std::hypot(ax*x, by*y);
            for (int i = 0; i < 40; ++i)
            {
                const double t = (lo + hi) * 0.5;
                px = x / (1.0 + t/(ax*ax));
                py = y / (1.0 + t/(by*by));
                if ( px*px/(ax*ax) + py*py/(by*by) > 1.0 ) lo = t;
                else hi = t;
            }
        }
    }
    else if ( pz < box.minDepth )
    {
        const double ax = a * box.minDepth, by = b * box.minDepth;
        pz = box.minDepth;
        if ( ax > 0.0 && by > 0.0 && px*px/(ax*ax) + py*py/(by*by) > 1.0 )
        {
            double lo = 0.0, hi = std::hypot(ax*px, by*py);
            const double x0 = px, y0 = py;
            for (int i = 0; i < 40; ++i)
            {
                const double t = (lo + hi) * 0.5;
                px = x0 / (1.0 + t/(ax*ax));
                py = y0 / (1.0 + t/(by*by));
                if ( px*px/(ax*ax) + py*py/(by*by) > 1.0 ) lo = t;
                else hi = t;
            }
        }
    }
    return (x-px)*(x-px) + (y-py)*(y-py) + (z-pz)*(z-pz);
}

bool CanAcquire(NC_STACK_ypabact *shooter, const World::TWeapProto &wproto,
                const TFireTriggerArea &box, const vec3d &pos, float radius)
{
    if ( !shooter->IsPrimaryWeaponElevationAllowed(pos - shooter->_position, !box.IsBelow()) )
        return false;
    // The configured volume controls acquisition distance. Physical weapon
    // limits remain authoritative; movement thresholds do not shorten it.
    if ( wproto.IsLaser() &&
         (pos - box.top).length() > ypabact_LaserRange(wproto) )
        return false;
    if ( !ypabact_FireTriggerAreaContains(box, pos, radius) )
        return false;
    if ( wproto.IsArcGrenade() )
    {
        float minMuzzleX, maxMuzzleX;
        ypabact_GetFireXSpan(shooter, &minMuzzleX, &maxMuzzleX);
        // Box padding is acquisition tolerance, not an additional muzzle.
        const double x = std::max((double)minMuzzleX, std::min((pos-box.top).dot(box.right), (double)maxMuzzleX));
        vec3d dir;
        return ypabact_TrySolveArcGrenadeDirection(box.top + box.right*x, pos, wproto, &dir);
    }
    return true;
}
}

bool NC_STACK_ypabact::GetAIFireTriggerArea(const World::TWeapProto &wproto, TFireTriggerArea *out,
                                         const vec3d *aimPos, bool groundTarget)
{
    if ( !out || !wproto.HasAIFireTriggerArea() )
        return false;

    *out = TFireTriggerArea();
    out->shape = wproto.GetAIFireTriggerShape();
    out->top = GetBodyPosition() +
               _rotation.Transpose().Transform(vec3d(0.0, _fire_pos.y, _fire_pos.z));
    ypabact_GetFireXSpan(this, &out->minX, &out->maxX);
    if ( out->IsBelow() )
    {
        // Downward payloads keep their volumes vertical. Only the heading turns
        // their horizontal section; pitch and roll move the muzzle origin.
        out->right = _rotation.AxisX().X0Z();
        if ( out->right.normalise() < 0.001f )
            return false;
        const vec3d heading(-out->right.z, 0.0, out->right.x);
        out->forward = vec3d(0.0, 1.0, 0.0);
        out->up = heading;
    }
    else
    {
        out->right = _rotation.AxisX();
        out->up = _rotation.AxisY();
        out->forward = _rotation.AxisZ();
        const bool aerial = _bact_type == BACT_TYPES_BACT || _bact_type == BACT_TYPES_FLYER ||
                            _bact_type == BACT_TYPES_UFO;
        if ( (IsGroundUnit() || aerial) && !getBACT_inputting() )
        {
            // Debug resolves the same target type and position as the fire query.
            if ( !aimPos )
            {
                NC_STACK_ypabact *unit = nullptr;
                if ( _secndTtype == BACT_TGT_TYPE_UNIT && _secndT.pbact ) unit = _secndT.pbact;
                else if ( _secndTtype == BACT_TGT_TYPE_CELL ) { aimPos = &_sencdTpos; groundTarget = true; }
                else if ( _primTtype == BACT_TGT_TYPE_UNIT && _primT.pbact ) unit = _primT.pbact;
                else if ( _primTtype == BACT_TGT_TYPE_CELL ) { aimPos = &_primTpos; groundTarget = true; }
                if ( unit )
                {
                    aimPos = &unit->_position;
                    groundTarget = ypabact_IsGroundFireTarget(unit);
                }
            }
            if ( aimPos )
            {
                // ai_fire_trigger_forward_max_inclination of the weapon limits how
                // far its forward volume tilts toward the target. Absent, aircraft
                // keep the volume on the hull axes and ground units follow only
                // their native scope limits.
                // With it, a ground target (sector or ground unit) ahead of an
                // aircraft tilts the volume toward it up to that limit.
                const double maxTilt = wproto.GetAIFireTriggerMaxInclination() * C_PI / 180.0;
                const bool aerialGroundAim = aerial && groundTarget && maxTilt > 0.0;
                const vec3d local = _rotation.Transform(*aimPos - out->top);
                if ( local.z >= 0.0f && (IsGroundUnit() || aerialGroundAim) )
                {
                    double pitch = atan2(-local.y, local.z);
                    if ( IsGroundUnit() )
                    {
                        double minPitch, maxPitch;
                        GetPrimaryWeaponElevationLimits(&minPitch, &maxPitch);
                        pitch = std::max(minPitch, std::min(pitch, maxPitch));
                    }
                    if ( maxTilt > 0.0 )
                        pitch = std::max(-maxTilt, std::min(pitch, maxTilt));
                    out->up = _rotation.AxisY()*cos(pitch) + _rotation.AxisZ()*sin(pitch);
                    out->forward = _rotation.AxisZ()*cos(pitch) - _rotation.AxisY()*sin(pitch);
                }
            }
        }
    }

    if ( out->shape == World::TWeapProto::AI_TRIGGER_BOX_BELOW )
    {
        const vec3d &size = wproto.ai_fire_trigger_box_below;
        out->minX -= size.x * 0.5f;
        out->maxX += size.x * 0.5f;
        out->halfY = size.z * 0.5f;
        out->depth = size.y;
    }
    else if ( out->shape == World::TWeapProto::AI_TRIGGER_BOX_FORWARD )
    {
        const vec3d &size = wproto.ai_fire_trigger_box_forward;
        out->minX -= size.x * 0.5f;
        out->maxX += size.x * 0.5f;
        out->halfY = size.y * 0.5f;
        out->depth = size.z;
    }
    else
    {
        const vec3d &size = out->IsBelow() ? wproto.ai_fire_trigger_cone_below : wproto.ai_fire_trigger_cone_forward;
        out->coneX = tan((double)size.x * C_PI / 360.0);
        out->coneY = tan((double)size.y * C_PI / 360.0);
        out->depth = size.z;
    }
    if ( wproto.ai_fire_trigger_min_depth < out->depth )
        out->minDepth = wproto.ai_fire_trigger_min_depth;
    return true;
}

// Tanks, cars and turrets placed in the world. Host Station guns fly with
// their station, so they keep the air-target rules.
bool ypabact_IsGroundFireTarget(NC_STACK_ypabact *unit)
{
    if ( !unit )
        return false;
    if ( unit->IsGroundUnit() )
        return true;
    NC_STACK_ypagun *gun = unit->_bact_type == BACT_TYPES_GUN ? dynamic_cast<NC_STACK_ypagun *>(unit) : nullptr;
    return gun && !gun->IsRoboGun();
}

// Sector queries (unkn 1) and unit queries aimed at a ground unit.
bool ypabact_IsGroundFireQuery(const bact_arg101 &arg)
{
    return arg.unkn != 2 || ypabact_IsGroundFireTarget(arg.target);
}

// All volumes intersect the target broad sphere, including their sides and
// finite end cap; sector points use radius 0.
bool ypabact_FireTriggerAreaContains(const TFireTriggerArea &box, const vec3d &pos, float radius)
{
    const vec3d d = pos - box.top;
    const double r = std::max(radius, 0.0f);
    const double z = d.dot(box.forward);
    // The centre must be on the firing side of the muzzle.
    if ( z < 0.0 || z + r < box.minDepth || z > box.depth + r )
        return false;
    const double x = d.dot(box.right);
    const double y = d.dot(box.up);
    const double dx = x - std::max((double)box.minX, std::min(x, (double)box.maxX));
    if ( box.IsCone() )
    {
        if ( fabs(dx) > box.coneX*box.depth + r || fabs(y) > box.coneY*box.depth + r )
            return false;
        return ConeDistanceSquared(dx, y, z, box) <= r*r + 1e-6;
    }
    const double dy = std::max(fabs(y) - box.halfY, 0.0);
    const double dz = std::max(std::max(z - box.depth, (double)box.minDepth - z), 0.0);
    return dx*dx + dy*dy + dz*dz <= r*r + 1e-6;
}

// A conservative horizontal search radius covering every part of the volume.
float ypabact_FireTriggerAreaSearchRadius(const TFireTriggerArea &box)
{
    if ( !box.IsBelow() )
    {
        const double x = std::max(fabs(box.minX), fabs(box.maxX)) + box.coneX*box.depth;
        const double y = box.halfY + box.coneY*box.depth;
        return (float)std::min(sqrt(x*x + y*y + (double)box.depth*box.depth), (double)std::numeric_limits<float>::max());
    }
    const double x = std::max(fabs(box.minX), fabs(box.maxX)) + box.coneX*box.depth;
    const double y = box.halfY + box.coneY*box.depth;
    return (float)std::min(std::hypot(x, y), (double)std::numeric_limits<float>::max());
}

// Nearest valid enemy unit (horizontally from the box top) inside the box.
NC_STACK_ypabact *ypabact_FindFireTriggerAreaEnemy(NC_STACK_ypabact *shooter, const TFireTriggerArea &box,
                                                const World::TWeapProto *wproto)
{
    if ( !shooter || !shooter->getBACT_pWorld() )
        return nullptr;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    const Common::Point mapSize = world->GetMapSize();
    const double searchRadius = ypabact_FireTriggerAreaSearchRadius(box);
    const int sectorRadius = (int)std::min(searchRadius / World::CVSectorLength + 2,
                                         (double)std::max(mapSize.x, mapSize.y));
    const Common::Point center = World::PositionToSectorID(box.top);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = 0.0f;

    for (int y = std::max(0, center.y - sectorRadius); y <= std::min(mapSize.y-1, center.y + sectorRadius); y++)
    {
        for (int x = std::max(0, center.x - sectorRadius); x <= std::min(mapSize.x-1, center.x + sectorRadius); x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            for (NC_STACK_ypabact *bct : world->SectorAt(cellId).unitsList)
            {
                if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, bct, false) )
                    continue;

                if ( wproto ? !CanAcquire(shooter, *wproto, box, bct->_position, bct->GetCollisionBroadRadius())
                            : !ypabact_FireTriggerAreaContains(box, bct->_position, bct->GetCollisionBroadRadius()) )
                    continue;

                const float dist = (bct->_position.XZ() - box.top.XZ()).length();
                if ( !best || dist < bestDist )
                {
                    best = bct;
                    bestDist = dist;
                }
            }
        }
    }

    return best;
}

// AI acquisition gate for a configured volume; absent/incompatible keeps legacy control.
// The current target (unit or sector point) wins; otherwise any enemy unit
// inside the volume makes the weapon fire and is returned in fire_trigger_area_target.
// Returns false when the weapon has no compatible fire volume.
bool NC_STACK_ypabact::CheckAIFireTriggerArea(const World::TWeapProto &wproto, bact_arg101 *arg, size_t *outFire)
{
    TFireTriggerArea box;
    if ( !GetAIFireTriggerArea(wproto, &box, &arg->pos, ypabact_IsGroundFireQuery(*arg)) )
        return false;

    arg->fire_trigger_area_target = nullptr;

    // unkn 2 = unit target with its broad radius, otherwise a sector point.
    const float radius = arg->unkn == 2 ? arg->radius : 0.0f;
    if ( CanAcquire(this, wproto, box, arg->pos, radius) )
    {
        *outFire = 1;
        return true;
    }

    arg->fire_trigger_area_target = ypabact_FindFireTriggerAreaEnemy(this, box, &wproto);
    *outFire = arg->fire_trigger_area_target ? 1 : 0;
    return true;
}

// When another enemy inside the fire trigger area triggered the shot, the AI shot goes to
// that unit instead of the current target (bomb drift, homing and laser grouping).
void ypabact_AimAtFireTriggerAreaTarget(const bact_arg101 &fireTest, bact_arg79 *shot)
{
    NC_STACK_ypabact *unit = fireTest.fire_trigger_area_target;
    if ( !unit )
        return;

    vec3d dir = unit->_position - fireTest.launch_pos;
    if ( dir.normalise() > 0.001f )
        shot->direction = dir;

    shot->tgType = BACT_TGT_TYPE_UNIT;
    shot->target.pbact = unit;
    shot->tgt_pos = unit->_position;
}
