// Fire trigger area: optional AI fire volume (ai_fire_trigger_area_below_xz) of bomb, homing_bomb and vertical laser.
#include "game/ypabact_internal.h"

bool NC_STACK_ypabact::GetAIFireTriggerArea(const World::TWeapProto &wproto, TFireTriggerArea *out)
{
    if ( !out || !wproto.HasAIFireTriggerArea() )
        return false;

    // Bombs fall straight down and the vertical laser fires along +Y, so the box
    // stays vertical and follows only the heading. Pitch and roll just move its
    // top, which is the real fire point of the model.
    vec3d right = _rotation.AxisX().X0Z();
    if ( right.normalise() < 0.001f )
        return false;

    out->right = right;
    out->forward = vec3d(-right.z, 0.0, right.x);
    out->top = GetBodyPosition() +
               _rotation.Transpose().Transform(vec3d(0.0, _fire_pos.y, _fire_pos.z));

    // Every fire_x mode spawns somewhere inside this local X span.
    float minX = 0.0f;
    float maxX = 0.0f;
    ypabact_GetFireXSpan(this, &minX, &maxX);

    out->minX = minX - wproto.ai_fire_trigger_area_x * 0.5f;
    out->maxX = maxX + wproto.ai_fire_trigger_area_x * 0.5f;
    out->halfZ = wproto.ai_fire_trigger_area_z * 0.5f;
    return true;
}

// True when a unit (centre pos, footprint radius) or a point (radius 0) is inside
// the box: its centre must be below the top and its footprint must touch the
// rectangle seen from above.
bool ypabact_FireTriggerAreaContains(const TFireTriggerArea &box, const vec3d &pos, float radius)
{
    const vec3d d = pos - box.top;
    if ( d.y < 0.0f ) // +Y is down
        return false;

    const float x = d.x * box.right.x + d.z * box.right.z;
    const float z = d.x * box.forward.x + d.z * box.forward.z;
    const float dx = x - std::max(box.minX, std::min(x, box.maxX));
    const float dz = z - std::max(-box.halfZ, std::min(z, box.halfZ));
    const float r = std::max(radius, 0.0f);

    return dx * dx + dz * dz <= r * r;
}

// Horizontal distance from the box top that covers its whole rectangle.
float ypabact_FireTriggerAreaSearchRadius(const TFireTriggerArea &box)
{
    const float x = std::max(fabs(box.minX), fabs(box.maxX));
    return sqrt(x * x + box.halfZ * box.halfZ);
}

// Nearest valid enemy unit (horizontally from the box top) inside the box.
NC_STACK_ypabact *ypabact_FindFireTriggerAreaEnemy(NC_STACK_ypabact *shooter, const TFireTriggerArea &box)
{
    if ( !shooter || !shooter->getBACT_pWorld() )
        return nullptr;

    NC_STACK_ypaworld *world = shooter->getBACT_pWorld();
    const int sectorRadius = (int)(ypabact_FireTriggerAreaSearchRadius(box) / World::CVSectorLength) + 2;
    const Common::Point center = World::PositionToSectorID(box.top);

    NC_STACK_ypabact *best = nullptr;
    float bestDist = 0.0f;

    for (int y = center.y - sectorRadius; y <= center.y + sectorRadius; y++)
    {
        for (int x = center.x - sectorRadius; x <= center.x + sectorRadius; x++)
        {
            Common::Point cellId(x, y);
            if ( !world->IsSector(cellId) )
                continue;

            for (NC_STACK_ypabact *bct : world->SectorAt(cellId).unitsList)
            {
                if ( !ypabact_IsLaserSecondaryTargetCandidate(shooter, bct, false) )
                    continue;

                if ( !ypabact_FireTriggerAreaContains(box, bct->_position, bct->GetCollisionBroadRadius()) )
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

// AI fire gate for a weapon with a fire trigger area; it replaces the legacy bomb test.
// The current target (unit or sector point) wins; otherwise any enemy unit
// inside the box makes the weapon fire and is returned in fire_trigger_area_target.
// Returns false when the weapon has no fire trigger area.
bool NC_STACK_ypabact::CheckAIFireTriggerArea(const World::TWeapProto &wproto, bact_arg101 *arg, size_t *outFire)
{
    TFireTriggerArea box;
    if ( !GetAIFireTriggerArea(wproto, &box) )
        return false;

    arg->fire_trigger_area_target = nullptr;

    // unkn 2 = unit target with its broad radius, otherwise a sector point.
    const float radius = arg->unkn == 2 ? arg->radius : 0.0f;
    if ( IsPrimaryWeaponElevationAllowed(arg->pos - _position) &&
         ypabact_FireTriggerAreaContains(box, arg->pos, radius) )
    {
        *outFire = 1;
        return true;
    }

    arg->fire_trigger_area_target = ypabact_FindFireTriggerAreaEnemy(this, box);
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
