// Projectile visual motion (spiral, chaos), fire-X randomness and VP rotation.
#include "game/ypabact_internal.h"

bool ypabact_ShouldApplyVPRotation(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    if ( bact->_vp_rotation.x == 0.0 &&
         bact->_vp_rotation.y == 0.0 &&
         bact->_vp_rotation.z == 0.0 )
        return false;

    return ypabact_IsMainVPBase(bact, base);
}

bool ypabact_ShouldApplyVPSpin(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    if ( !World::Spin::HasStrength(bact->_vp_spin_strength) )
        return false;

    return ypabact_IsMainVPBase(bact, base);
}

bool ypabact_HasProjectileChaos(const NC_STACK_ypabact *bact)
{
    return bact && bact->_chaos_factor > 0.0f && bact->_chaos_radius > 0.0f;
}

bool ypabact_HasProjectileSpiral(const NC_STACK_ypabact *bact)
{
    return bact && bact->_spiral_speed > 0.0f && bact->_spiral_radius > 0.0f;
}

bool ypabact_HasProjectileVisualMotion(const NC_STACK_ypabact *bact)
{
    return ypabact_HasProjectileChaos(bact) || ypabact_HasProjectileSpiral(bact);
}

bool ypabact_ShouldApplyProjectileVisualMotion(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    if ( !ypabact_HasProjectileVisualMotion(bact) && !bact->_projectile_visual_motion_frozen )
        return false;

    if ( ypabact_IsMainVPBase(bact, base) )
        return true;

    // Freeze the final render-only offset for the projectile's short dead/megadeth
    // VP so impact visuals do not snap back to the physical center line.
    return bact->_projectile_visual_motion_frozen &&
           bact->_bact_type == BACT_TYPES_MISSLE &&
           (base == bact->_vp_dead || base == bact->_vp_megadeth);
}

double ypabact_GetProjectileSpiralPhase(float speed, int32_t age)
{
    constexpr double TWO_PI = 6.28318530717958647692;
    const double safeAgeSeconds = (double)std::max<int32_t>(age, 0) * 0.001;
    const double turns = (double)speed * safeAgeSeconds;
    return std::fmod(turns, 1.0) * TWO_PI;
}

mat3x3 ypabact_BuildProjectileSpiralRoll(float speed, int32_t age)
{
    return mat3x3::RotateZ(ypabact_GetProjectileSpiralPhase(speed, age));
}

vec3d ypabact_BuildProjectileSpiralLocalOffset(const NC_STACK_ypabact *bact)
{
    if ( !ypabact_HasProjectileSpiral(bact) )
        return vec3d(0.0, 0.0, 0.0);

    const double phase = ypabact_GetProjectileSpiralPhase(bact->_spiral_speed, bact->_clock);
    const double radius = bact->_spiral_radius;

    // RotateZ() uses this clockwise X/Y convention. Keeping the orbit on the
    // same phase makes the model, its children and its emitter directions sweep
    // together instead of counter-rotating around the central flight line.
    return vec3d(std::cos(phase) * radius,
                -std::sin(phase) * radius,
                 0.0);
}

vec3d ypabact_BuildProjectileSpiralOffset(const NC_STACK_ypabact *bact)
{
    if ( !bact )
        return vec3d(0.0, 0.0, 0.0);

    // Local Z is the projectile's forward axis. Orbit in its perpendicular X/Y
    // plane, then transform that offset with the physical projectile orientation.
    return bact->_rotation.Transpose().Transform(ypabact_BuildProjectileSpiralLocalOffset(bact));
}

mat3x3 ypabact_BuildProjectileSpiralTangent(const NC_STACK_ypabact *bact)
{
    if ( !ypabact_HasProjectileSpiral(bact) )
        return mat3x3::Ident();

    constexpr double TWO_PI = 6.28318530717958647692;
    const double phase = ypabact_GetProjectileSpiralPhase(bact->_spiral_speed, bact->_clock);
    const double angularSpeed = (double)bact->_spiral_speed * TWO_PI;
    const double radius = bact->_spiral_radius;

    const vec3d visualVelocityLocal(-std::sin(phase) * radius * angularSpeed,
                                   -std::cos(phase) * radius * angularSpeed,
                                    0.0);
    return ypabact_BuildProjectileVisualTangent(bact, visualVelocityLocal);
}

uint32_t ypabact_ProjectileChaosHash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

double ypabact_ProjectileChaosUnit(uint32_t value)
{
    return (double)(ypabact_ProjectileChaosHash(value) & 0x00ffffffu) / 16777215.0;
}

vec3d ypabact_GetProjectileChaosTarget(const NC_STACK_ypabact *bact, uint32_t segment)
{
    constexpr double TWO_PI = 6.28318530717958647692;
    const uint32_t seed = bact->_gid ^ 0x9e3779b9u;
    const uint32_t segmentSeed = seed ^ (segment * 0x85ebca6bu);
    const double angle = ypabact_ProjectileChaosUnit(segmentSeed ^ 0xa341316cu) * TWO_PI;
    const double radiusFactor = std::sqrt(ypabact_ProjectileChaosUnit(segmentSeed ^ 0xc8013ea4u));
    const double radius = (double)bact->_chaos_radius * radiusFactor;

    // Random targets stay inside the configured lateral disc around the physical
    // trajectory. Forward travel remains exclusively owned by projectile physics.
    return vec3d(std::cos(angle) * radius,
                 std::sin(angle) * radius,
                 0.0);
}

vec3d ypabact_BuildProjectileChaosLocalOffsetAtAge(const NC_STACK_ypabact *bact,
                                                          double ageMilliseconds)
{
    if ( !ypabact_HasProjectileChaos(bact) )
        return vec3d(0.0, 0.0, 0.0);

    const double ageSeconds = std::max(ageMilliseconds, 0.0) * 0.001;
    const double motionPhase = ageSeconds * (double)bact->_chaos_factor;
    const double segmentFloor = std::floor(motionPhase);
    const uint32_t segment = segmentFloor > (double)std::numeric_limits<uint32_t>::max()
                           ? std::numeric_limits<uint32_t>::max()
                           : (uint32_t)segmentFloor;
    const double phase = motionPhase - segmentFloor;
    const double smoothPhase = phase * phase * (3.0 - 2.0 * phase);

    const uint32_t nextSegment = segment == std::numeric_limits<uint32_t>::max()
                               ? segment
                               : segment + 1u;
    const vec3d current = ypabact_GetProjectileChaosTarget(bact, segment);
    const vec3d next = ypabact_GetProjectileChaosTarget(bact, nextSegment);
    return current + (next - current) * smoothPhase;
}

vec3d ypabact_BuildProjectileChaosOffset(const NC_STACK_ypabact *bact)
{
    if ( !bact )
        return vec3d(0.0, 0.0, 0.0);

    const vec3d localOffset = ypabact_BuildProjectileChaosLocalOffsetAtAge(bact, bact->_clock);
    return bact->_rotation.Transpose().Transform(localOffset);
}

mat3x3 ypabact_BuildProjectileChaosTangent(const NC_STACK_ypabact *bact)
{
    if ( !ypabact_HasProjectileChaos(bact) )
        return mat3x3::Ident();

    // A 1 ms finite difference is deterministic, cheap and keeps the rendered
    // model aligned to the smoothed erratic path without introducing extra state.
    const double age = (double)std::max<int32_t>(bact->_clock, 0);
    const vec3d current = ypabact_BuildProjectileChaosLocalOffsetAtAge(bact, age);
    const vec3d next = ypabact_BuildProjectileChaosLocalOffsetAtAge(bact, age + 1.0);
    const vec3d visualVelocityLocal = (next - current) * 1000.0;
    return ypabact_BuildProjectileVisualTangent(bact, visualVelocityLocal);
}

bool NC_STACK_ypabact::GetProjectileVisualMotionDelta(vec3d *worldOffset, mat3x3 *renderRotationDelta) const
{
    if ( worldOffset )
        *worldOffset = vec3d(0.0, 0.0, 0.0);
    if ( renderRotationDelta )
        *renderRotationDelta = mat3x3::Ident();

    if ( _projectile_visual_motion_frozen )
    {
        if ( worldOffset )
            *worldOffset = _projectile_visual_frozen_offset;
        if ( renderRotationDelta )
            *renderRotationDelta = _projectile_visual_frozen_rotation;
        return true;
    }

    // Chaos is the explicit alternative to Spiral. A valid Chaos configuration
    // therefore takes priority if both modes are present on the same Weapon.
    if ( ypabact_HasProjectileChaos(this) )
    {
        if ( worldOffset )
            *worldOffset = ypabact_BuildProjectileChaosOffset(this);
        if ( renderRotationDelta )
            *renderRotationDelta = ypabact_BuildProjectileChaosTangent(this);
        return true;
    }

    if ( !ypabact_HasProjectileSpiral(this) )
        return false;

    if ( worldOffset )
        *worldOffset = ypabact_BuildProjectileSpiralOffset(this);

    if ( renderRotationDelta )
    {
        *renderRotationDelta = ypabact_BuildProjectileSpiralTangent(this);
        *renderRotationDelta *= ypabact_BuildProjectileSpiralRoll(_spiral_speed, _clock);
    }

    return true;
}

void NC_STACK_ypabact::FreezeProjectileVisualMotion()
{
    if ( _projectile_visual_motion_frozen || !ypabact_HasProjectileVisualMotion(this) )
        return;

    vec3d offset;
    mat3x3 rotation;
    if ( GetProjectileVisualMotionDelta(&offset, &rotation) )
    {
        _projectile_visual_frozen_offset = offset;
        _projectile_visual_frozen_rotation = rotation;
        _projectile_visual_motion_frozen = true;
    }
}

void NC_STACK_ypabact::ResetProjectileVisualMotionFreeze()
{
    _projectile_visual_motion_frozen = false;
    _projectile_visual_frozen_offset = vec3d(0.0, 0.0, 0.0);
    _projectile_visual_frozen_rotation = mat3x3::Ident();
}

void ypabact_ApplyProjectileVisualMotion(NC_STACK_ypabact *bact, NC_STACK_base *base)
{
    if ( !ypabact_ShouldApplyProjectileVisualMotion(bact, base) )
        return;

    vec3d visualOffset;
    mat3x3 visualRotationDelta;
    if ( !bact->GetProjectileVisualMotionDelta(&visualOffset, &visualRotationDelta) )
        return;

    // Move and orient the root VP only for rendering. Child VPs and every embedded
    // particle emitter inherit this transform through NC_STACK_base::Render().
    // Attached decoration FX reuse the exact same delta through the transient-VP
    // follow path. Once emitted, particles remain at their world-space spawn
    // positions, so consecutive emissions trace the selected visual motion instead
    // of moving as one rigid tube. Gameplay position, physics and collision stay central.
    base->TForm().Pos += visualOffset;
    base->TForm().SclRot *= visualRotationDelta;
}

mat3x3 ypabact_BuildVPRotationMatrix(const vec3d &degrees)
{
    vec3d angle = degrees * C_PI_180;
    mat3x3 rot = mat3x3::Ident();

    if ( angle.x != 0.0 )
        rot *= mat3x3::RotateX(angle.x);
    if ( angle.y != 0.0 )
        rot *= mat3x3::RotateY(angle.y);
    if ( angle.z != 0.0 )
        rot *= mat3x3::RotateZ(angle.z);

    return rot;
}

void ypabact_EnsureFireXRandomSeeded(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->_fire_x_random_seeded )
        return;

    uint32_t seed = unit->_gid ^ ((uint32_t)unit->_vehicleID * 2654435761u) ^ 0xA341316Cu;
    unit->_fire_x_random_state = seed ? seed : 0xC8013EA4u;
    unit->_fire_x_random_seeded = true;
}

uint32_t ypabact_PeekFireXRandom(NC_STACK_ypabact *unit)
{
    ypabact_EnsureFireXRandomSeeded(unit);
    return unit->_fire_x_random_state * 1664525u + 1013904223u;
}

uint32_t ypabact_NextFireXRandom(NC_STACK_ypabact *unit)
{
    unit->_fire_x_random_state = ypabact_PeekFireXRandom(unit);
    return unit->_fire_x_random_state;
}

void ypabact_BuildFireXRandomOrder(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->_fire_x_slots <= 0 ||
         unit->_fire_x_slots > World::TVhclProto::FIRE_X_MAX_SLOTS )
        return;

    ypabact_EnsureFireXRandomSeeded(unit);

    unit->_fire_x_random_order.resize(unit->_fire_x_slots);
    for (int i = 0; i < unit->_fire_x_slots; i++)
        unit->_fire_x_random_order[i] = i;

    for (int i = unit->_fire_x_slots - 1; i > 0; i--)
    {
        int j = (int)(ypabact_NextFireXRandom(unit) % (uint32_t)(i + 1));
        std::swap(unit->_fire_x_random_order[i], unit->_fire_x_random_order[j]);
    }

    unit->_fire_x_slot_index = 0;
}

// Local X range covered by every spawn point ypabact_GetProjectileFireX() can
// return: +-fire_x for the vanilla/simple/salve modes, the rack for fire_x_advanced.
void ypabact_GetFireXSpan(const NC_STACK_ypabact *unit, float *minX, float *maxX)
{
    const float side = fabs(unit->_fire_pos.x);
    *minX = -side;
    *maxX = side;

    const bool rack = unit->_fire_x_advanced &&
                      (unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SEQUENCE ||
                       unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_RANDOM) &&
                      unit->_fire_x_slots > 0 &&
                      unit->_fire_x_slots <= World::TVhclProto::FIRE_X_MAX_SLOTS;
    if ( !rack )
        return;

    const float first = unit->_fire_x_start;
    const float last = unit->_fire_x_start + (float)(unit->_fire_x_slots - 1) * unit->_fire_x_step;
    *minX = std::min(first, last);
    *maxX = std::max(first, last);
}

float ypabact_GetProjectileFireX(NC_STACK_ypabact *unit, float vanillaFireX,
                                        int projectileIndex, int projectileCount)
{
    if ( !unit || unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_VANILLA )
        return vanillaFireX;

    if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE )
        return (unit->_fire_x_slot_index & 1) ? -unit->_fire_pos.x : unit->_fire_pos.x;

    if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_MIRROR )
    {
        int shotsPerSide = projectileCount / 2;
        if ( projectileIndex < shotsPerSide )
            return unit->_fire_pos.x;
        if ( projectileIndex < shotsPerSide * 2 )
            return -unit->_fire_pos.x;
        return 0.0f;
    }

    if ( !unit->_fire_x_advanced )
    {
        if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SEQUENCE )
            return (unit->_fire_x_slot_index & 1) ? unit->_fire_pos.x : -unit->_fire_pos.x;

        if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_RANDOM )
        {
            // Use a high PRNG bit: the low bit of this LCG alternates every call.
            uint32_t nextRandom = ypabact_PeekFireXRandom(unit);
            return (nextRandom & 0x80000000u) ? unit->_fire_pos.x : -unit->_fire_pos.x;
        }

        return vanillaFireX;
    }

    if ( unit->_fire_x_slots <= 0 ||
         unit->_fire_x_slots > World::TVhclProto::FIRE_X_MAX_SLOTS )
        return vanillaFireX;

    int slot = unit->_fire_x_slot_index;
    if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_RANDOM )
    {
        if ( unit->_fire_x_random_order.size() != (size_t)unit->_fire_x_slots ||
             slot < 0 || slot >= unit->_fire_x_slots )
            ypabact_BuildFireXRandomOrder(unit);

        slot = unit->_fire_x_random_order[unit->_fire_x_slot_index];
    }

    return unit->_fire_x_start + (float)slot * unit->_fire_x_step;
}

void ypabact_ConsumeProjectileFireX(NC_STACK_ypabact *unit)
{
    if ( !unit || unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_VANILLA )
        return;

    if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_SEQUENCE ||
         unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SALVE_MIRROR )
        return;

    if ( !unit->_fire_x_advanced )
    {
        if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_SEQUENCE )
            unit->_fire_x_slot_index = (unit->_fire_x_slot_index + 1) & 1;
        else if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_RANDOM )
            ypabact_NextFireXRandom(unit);
        return;
    }

    if ( unit->_fire_x_slots <= 0 ||
         unit->_fire_x_slots > World::TVhclProto::FIRE_X_MAX_SLOTS )
        return;

    unit->_fire_x_slot_index++;
    if ( unit->_fire_x_slot_index >= unit->_fire_x_slots )
    {
        if ( unit->_fire_x_mode == World::TVhclProto::FIRE_X_MODE_RANDOM )
            ypabact_BuildFireXRandomOrder(unit);
        else
            unit->_fire_x_slot_index = 0;
    }
}
