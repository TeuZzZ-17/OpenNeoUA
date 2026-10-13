// Weapon tracer visuals of projectiles.
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "game/yw.h"
#include "game/ypamissile.h"
#include "game/yparobo.h"
#include "game/yw_net.h"
#include "system/inivals.h"
#include "common/log.h"
#include <algorithm>
#include <math.h>
#include "game/ypamissile_internal.h"

bool ypamissile_WeaponTracerFinite(const vec3d &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool ypamissile_WeaponTracerFinite(const World::TVisualTint &tint)
{
    return std::isfinite(tint.r) && std::isfinite(tint.g) &&
           std::isfinite(tint.b) && std::isfinite(tint.a);
}

void NC_STACK_ypamissile::ConfigureWeaponTracer(
    const World::TWeaponTracerConfig &config, bool supported)
{
    _weaponTracer = config;
    _weaponTracer.tint.Clamp();
    _weaponTracer.tint_head.Clamp();
    _weaponTracer.tint_tail.Clamp();
    _weaponTracer.enabled = supported && config.enabled &&
                            std::isfinite(config.size_z) && config.size_z > 0.01f &&
                            std::isfinite(config.size_x) && config.size_x > 0.01f &&
                            std::isfinite(config.ResolveSizeY()) &&
                            config.ResolveSizeY() >= 0.0f &&
                            ypamissile_WeaponTracerFinite(config.pos) &&
                            ypamissile_WeaponTracerFinite(config.tint) &&
                            config.tint.a > 0.0f;
    _weaponTracerStarted = false;
    _weaponTracerVisualSeed = 0;
    _weaponTracerPoints.clear();
}

void NC_STACK_ypamissile::StartWeaponTracer()
{
    _weaponTracerStarted = _weaponTracer.enabled && _world &&
                           !_world->_isNetGame &&
                           _mislType != MISL_INTERNAL;
    _weaponTracerPoints.clear();

    if ( !_weaponTracerStarted )
        return;

    // mesh_tracer_pos_* is a projectile-local visual offset. Apply it to every
    // sampled tracer point, starting here at launch, so the complete trail stays
    // attached to the authored position relative to the projectile transform.
    const vec3d point = GetWeaponTracerPosition();
    if ( !ypamissile_WeaponTracerFinite(point) )
    {
        _weaponTracerStarted = false;
        return;
    }

    _weaponTracerVisualSeed =
        (uint32_t)(_gid ? _gid : (_vehicleID * 2654435761u)) ^
        (uint32_t)_clock;

    TWeaponTracerPoint sample;
    sample.pos = point;
    sample.time = _clock;
    _weaponTracerPoints.push_back(sample);
}

vec3d NC_STACK_ypamissile::GetWeaponTracerPosition() const
{
    // Reuse the same final motion delta as the projectile VP and attached FX.
    // Physics owns the centre trajectory; Chaos/Spiral affect its rendered pose.
    vec3d visualOffset;
    mat3x3 visualRotationDelta;
    GetProjectileVisualMotionDelta(&visualOffset, &visualRotationDelta);
    const mat3x3 renderRotation = _rotation.Transpose() * visualRotationDelta;
    return _position + visualOffset + renderRotation.Transform(_weaponTracer.pos);
}

void NC_STACK_ypamissile::UpdateWeaponTracer()
{
    // Loaded projectiles and other legitimate factory paths may not pass
    // through a launch-specific finalization call. Start from their current
    // authoritative transform on the first update without serializing history.
    if ( !_weaponTracerStarted && _weaponTracer.enabled && _world &&
         !_world->_isNetGame && _status == BACT_STATUS_NORMAL &&
         _mislType != MISL_INTERNAL )
    {
        StartWeaponTracer();
    }

    if ( !_weaponTracerStarted || !_world || _world->_isNetGame )
        return;

    // The tracer has no independent lifetime: it exists only while the physical
    // projectile itself is alive. Do not leave a visual tail after impact/death.
    if ( _status != BACT_STATUS_NORMAL )
    {
        _weaponTracerStarted = false;
        _weaponTracerPoints.clear();
        return;
    }

    const vec3d point = GetWeaponTracerPosition();

    if ( ypamissile_WeaponTracerFinite(point) )
    {
        if ( _weaponTracerPoints.empty() )
        {
            TWeaponTracerPoint sample;
            sample.pos = point;
            sample.time = _clock;
            _weaponTracerPoints.push_back(sample);
        }
        else
        {
            const float moved = (point - _weaponTracerPoints.back().pos).length();
            const bool sampleDue =
                _clock - _weaponTracerPoints.back().time >= 16;

            if ( std::isfinite(moved) && moved > 0.01f && sampleDue )
            {
                TWeaponTracerPoint sample;
                sample.pos = point;
                sample.time = _clock;
                _weaponTracerPoints.push_back(sample);
            }
        }
    }

    // Keep a strict recent-path memory/render bound per projectile. The tracer
    // lifetime itself is still owned exclusively by the physical projectile.
    while ( _weaponTracerPoints.size() > 320 )
        _weaponTracerPoints.pop_front();
}

void NC_STACK_ypamissile::RenderWeaponTracer(baseRender_msg *arg)
{
    if ( !arg || !_weaponTracerStarted || !_world || _world->_isNetGame ||
         _status != BACT_STATUS_NORMAL || _weaponTracerPoints.empty() )
        return;

    struct TVisibleTracerSegment
    {
        vec3d start;
        vec3d end;
        float length = 0.0f;
    };

    std::vector<TVisibleTracerSegment> visible;
    float remainingLength = _weaponTracer.size_z;
    float visibleLength = 0.0f;

    // History is sampled at 16 ms; the head must follow the current pose even
    // between samples or after a later shared movement update in this frame.
    vec3d newer = GetWeaponTracerPosition();
    for (size_t index = _weaponTracerPoints.size();
         index > 0 && remainingLength > 0.01f; index--)
    {
        const TWeaponTracerPoint &older = _weaponTracerPoints[index - 1];

        vec3d segment = newer - older.pos;
        const float segmentLength = segment.length();
        if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
            continue;

        TVisibleTracerSegment item;
        item.start = older.pos;
        item.end = newer;
        item.length = std::min(segmentLength, remainingLength);

        if ( segmentLength > remainingLength )
            item.start = newer - segment * (remainingLength / segmentLength);

        visible.push_back(item);
        visibleLength += item.length;
        remainingLength -= item.length;
        newer = older.pos;
    }

    if ( visible.empty() || visibleLength <= 0.01f )
        return;

    float renderedFromHead = 0.0f;
    for (const TVisibleTracerSegment &item : visible)
    {
        const float headFactor = 1.0f - renderedFromHead / visibleLength;
        const float tailFactor = 1.0f -
            (renderedFromHead + item.length) / visibleLength;
        _world->RenderWeaponTracerSegment(arg, item.start, item.end,
                                           _weaponTracer,
                                           std::max(0.0f, tailFactor),
                                           std::min(1.0f, headFactor),
                                           _weaponTracerVisualSeed);

        renderedFromHead += item.length;
    }
}
