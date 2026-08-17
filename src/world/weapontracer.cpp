#include "../yw.h"

#include <algorithm>
#include <cmath>

namespace
{

const size_t MGUN_TRACER_LIMIT = 512;
const float MGUN_TRACER_VISUAL_SPEED = 6000.0f;
const float WEAPON_TRACER_PI = 3.14159265358979323846f;

static float MinigunTracerTravelTimeMs(float distance)
{
    return distance * 1000.0f / MGUN_TRACER_VISUAL_SPEED;
}

static bool WeaponTracerFinite(const vec3d &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

static bool WeaponTracerFinite(const World::TVisualTint &tint)
{
    return std::isfinite(tint.r) && std::isfinite(tint.g) &&
           std::isfinite(tint.b) && std::isfinite(tint.a);
}

static float WeaponTracerClamp01(float value)
{
    if ( value < 0.0f )
        return 0.0f;
    if ( value > 1.0f )
        return 1.0f;
    return value;
}

static float WeaponTracerLerp(float a, float b, float t)
{
    return a + (b - a) * WeaponTracerClamp01(t);
}

static World::TVisualTint WeaponTracerLerpTint(const World::TVisualTint &a,
                                               const World::TVisualTint &b,
                                               float t)
{
    World::TVisualTint result;
    result.r = WeaponTracerLerp(a.r, b.r, t);
    result.g = WeaponTracerLerp(a.g, b.g, t);
    result.b = WeaponTracerLerp(a.b, b.b, t);
    result.a = WeaponTracerLerp(a.a, b.a, t);
    return result;
}

static uint32_t WeaponTracerHash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float WeaponTracerHashSigned(uint32_t seed, uint32_t key)
{
    const uint32_t hashed = WeaponTracerHash(seed ^ (key * 0x9e3779b9u));
    const float unit = (float)(hashed & 0x00ffffffu) / 16777215.0f;
    return unit * 2.0f - 1.0f;
}

static void WeaponTracerAppendVertex(GFX::TMesh *mesh, const vec3f &pos,
                                     float alpha)
{
    GFX::TVertex vertex(pos);
    vertex.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, alpha);
    mesh->Vertexes.push_back(vertex);
}

static void WeaponTracerAppendRibbon(GFX::TMesh *mesh, bool vertical,
                                     float halfWidth, float alpha)
{
    const GFX::IndexType base = (GFX::IndexType)mesh->Vertexes.size();

    if ( vertical )
    {
        WeaponTracerAppendVertex(mesh, vec3f(0.0f, -halfWidth, -0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f(0.0f,  halfWidth, -0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f(0.0f,  halfWidth,  0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f(0.0f, -halfWidth,  0.5f), alpha);
    }
    else
    {
        WeaponTracerAppendVertex(mesh, vec3f(-halfWidth, 0.0f, -0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f( halfWidth, 0.0f, -0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f( halfWidth, 0.0f,  0.5f), alpha);
        WeaponTracerAppendVertex(mesh, vec3f(-halfWidth, 0.0f,  0.5f), alpha);
    }

    const GFX::IndexType indices[] = {
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 2), (GFX::IndexType)(base + 1),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 3), (GFX::IndexType)(base + 2),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 1), (GFX::IndexType)(base + 2),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 2), (GFX::IndexType)(base + 3)
    };
    mesh->Indixes.insert(mesh->Indixes.end(), indices, indices + 12);
}

static void WeaponTracerBuildLegacyMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Preserve the approved OpenUA tracer exactly when no advanced parameter
    // is authored: translucent outer crossed ribbons plus the fixed bright core.
    WeaponTracerAppendRibbon(mesh, false, 0.5f, 0.22f);
    WeaponTracerAppendRibbon(mesh, true,  0.5f, 0.22f);
    WeaponTracerAppendRibbon(mesh, false, 0.16f, 1.0f);
    WeaponTracerAppendRibbon(mesh, true,  0.16f, 1.0f);

    mesh->Mat = GFX::TRenderParams(GFX::RFLAGS_FOG |
                                   GFX::RFLAGS_DISABLE_ZWRITE |
                                   GFX::RFLAGS_ALPHABLEND);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

static void WeaponTracerBuildAdvancedMesh(GFX::TMesh *mesh, bool additive)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    WeaponTracerAppendRibbon(mesh, false, 0.5f, 1.0f);
    WeaponTracerAppendRibbon(mesh, true,  0.5f, 1.0f);

    uint32_t flags = GFX::RFLAGS_FOG | GFX::RFLAGS_DISABLE_ZWRITE;
    flags |= additive ? GFX::RFLAGS_LUMTRACY : GFX::RFLAGS_ALPHABLEND;
    mesh->Mat = GFX::TRenderParams(flags);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

static void WeaponTracerBuildSmokeMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Small procedural soft disc. It is rendered in view space, so no texture
    // or dedicated VP is required and the smoke always faces the camera.
    const int outerCount = 12;
    WeaponTracerAppendVertex(mesh, vec3f(0.0f, 0.0f, 0.0f), 1.0f);
    for (int i = 0; i < outerCount; i++)
    {
        const float angle = 2.0f * WEAPON_TRACER_PI * (float)i / (float)outerCount;
        WeaponTracerAppendVertex(mesh,
                                 vec3f(std::cos(angle) * 0.5f,
                                       std::sin(angle) * 0.5f, 0.0f),
                                 0.0f);
    }

    for (int i = 0; i < outerCount; i++)
    {
        const GFX::IndexType a = (GFX::IndexType)(1 + i);
        const GFX::IndexType b = (GFX::IndexType)(1 + (i + 1) % outerCount);
        const GFX::IndexType front[] = {0, b, a};
        const GFX::IndexType back[] = {0, a, b};
        mesh->Indixes.insert(mesh->Indixes.end(), front, front + 3);
        mesh->Indixes.insert(mesh->Indixes.end(), back, back + 3);
    }

    mesh->Mat = GFX::TRenderParams(GFX::RFLAGS_FOG |
                                   GFX::RFLAGS_DISABLE_ZWRITE |
                                   GFX::RFLAGS_ALPHABLEND);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

static mat3x3 WeaponTracerRotationFromDir(const vec3d &direction)
{
    vec3d z = direction;
    if ( z.normalise() <= 0.001f )
        return mat3x3();

    vec3d x = fabs(z.y) < 0.9 ? vec3d::OY(1.0) : vec3d::OX(1.0);
    x = x * z;
    if ( x.normalise() <= 0.001f )
        return mat3x3();

    vec3d y = z * x;
    if ( y.normalise() <= 0.001f )
        return mat3x3();

    mat3x3 rotation;
    rotation.SetX(x);
    rotation.SetY(y);
    rotation.SetZ(z);
    return rotation;
}

static vec3d WeaponTracerStableSide(const vec3d &direction, uint32_t seed)
{
    vec3d dir = direction;
    if ( dir.normalise() <= 0.001f )
        return vec3d::OX(1.0);

    vec3d candidate(WeaponTracerHashSigned(seed, 17),
                    WeaponTracerHashSigned(seed, 29),
                    WeaponTracerHashSigned(seed, 43));
    if ( candidate.normalise() <= 0.001f )
        candidate = vec3d::OY(1.0);

    candidate -= dir * candidate.dot(dir);
    if ( candidate.normalise() <= 0.001f )
    {
        candidate = fabs(dir.y) < 0.9 ? vec3d::OY(1.0) : vec3d::OX(1.0);
        candidate -= dir * candidate.dot(dir);
        candidate.normalise();
    }

    return candidate;
}

static vec3d WeaponTracerAdvancedOffset(const vec3d &direction,
                                        const vec3d &basePosition,
                                        const World::TWeaponTracerConfig &config,
                                        float factor, uint32_t seed)
{
    if ( config.wave <= 0.0f && config.noise <= 0.0f )
        return vec3d(0.0, 0.0, 0.0);

    vec3d dir = direction;
    if ( dir.normalise() <= 0.001f )
        return vec3d(0.0, 0.0, 0.0);

    const vec3d side = WeaponTracerStableSide(dir, seed);
    vec3d up = dir * side;
    if ( up.normalise() <= 0.001f )
        up = vec3d::OY(1.0);

    const float u = WeaponTracerClamp01(factor);
    vec3d offset(0.0, 0.0, 0.0);

    if ( config.wave > 0.0f && config.wave_count > 0 )
    {
        const float phase = 2.0f * WEAPON_TRACER_PI *
                            (float)config.wave_count * u;
        offset += side * (std::sin(phase) * config.wave);
    }

    if ( config.noise > 0.0f )
    {
        // Quantized deterministic world-position noise: old trail samples keep
        // the same disturbance while they remain visible instead of flickering
        // when the render framerate changes. The envelope still returns the
        // disturbance to zero at both physical tracer endpoints.
        const int32_t qx = (int32_t)std::floor(basePosition.x * 0.125);
        const int32_t qy = (int32_t)std::floor(basePosition.y * 0.125);
        const int32_t qz = (int32_t)std::floor(basePosition.z * 0.125);
        uint32_t key = WeaponTracerHash((uint32_t)qx);
        key = WeaponTracerHash(key ^ ((uint32_t)qy * 0x9e3779b9u));
        key = WeaponTracerHash(key ^ ((uint32_t)qz * 0x85ebca6bu));
        const float envelope = std::sin(WEAPON_TRACER_PI * u);
        const float n1 = WeaponTracerHashSigned(seed, key * 2u + 1u);
        const float n2 = WeaponTracerHashSigned(seed, key * 2u + 2u);
        offset += (side * n1 + up * n2) *
                  (config.noise * 0.70710678f * envelope);
    }

    return offset;
}

static float WeaponTracerPulseFactor(const World::TWeaponTracerConfig &config,
                                     int32_t timeStamp)
{
    if ( config.pulse <= 0.0f || config.pulse_speed <= 0.0f )
        return 1.0f;

    const float amplitude = std::min(0.9f, config.pulse * 0.1f);
    const float phase = (float)timeStamp * 0.001f * config.pulse_speed *
                        2.0f * WEAPON_TRACER_PI;
    return std::max(0.1f, 1.0f + std::sin(phase) * amplitude);
}

static World::TVisualTint WeaponTracerTintAt(
    const World::TWeaponTracerConfig &config, float factor)
{
    const World::TVisualTint &tail = config.has_tint_tail ?
        config.tint_tail : config.tint;
    const World::TVisualTint &head = config.has_tint_head ?
        config.tint_head : config.tint;
    return WeaponTracerLerpTint(tail, head, factor);
}

static bool WeaponTracerHasTintGradient(
    const World::TWeaponTracerConfig &config)
{
    const World::TVisualTint &tail = config.has_tint_tail ?
        config.tint_tail : config.tint;
    const World::TVisualTint &head = config.has_tint_head ?
        config.tint_head : config.tint;
    const float epsilon = 0.0001f;
    return std::fabs(tail.r - head.r) > epsilon ||
           std::fabs(tail.g - head.g) > epsilon ||
           std::fabs(tail.b - head.b) > epsilon ||
           std::fabs(tail.a - head.a) > epsilon;
}

static void WeaponTracerQueueSegment(baseRender_msg *arg, GFX::TMesh *mesh,
                                     TF::TForm3D *view,
                                     const vec3d &start, const vec3d &end,
                                     float width,
                                     const World::TVisualTint &tint,
                                     float alpha, float vizLimit,
                                     float fadeLength)
{
    if ( !arg || !mesh || !view || mesh->Vertexes.empty() ||
         !WeaponTracerFinite(start) || !WeaponTracerFinite(end) ||
         !WeaponTracerFinite(tint) || !std::isfinite(width) ||
         width <= 0.01f || !std::isfinite(alpha) || alpha <= 0.0f ||
         tint.a <= 0.0f )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    const mat3x3 rotation = WeaponTracerRotationFromDir(direction);
    if ( rotation.AxisZ().length() <= 0.001f )
        return;

    const vec3d center = start + direction * 0.5f;
    mat4x4 worldForm(rotation.Transpose() * mat3x3::Scale(
        vec3d(width, width, segmentLength)));
    worldForm.m03 = center.x;
    worldForm.m13 = center.y;
    worldForm.m23 = center.z;

    mat4x4 transform = view->CalcSclRot;
    transform *= (worldForm - view->CalcPos);
    const float distance = transform.getTranslate().length();
    if ( distance > vizLimit + segmentLength )
        return;

    const float clampedAlpha = WeaponTracerClamp01(alpha);
    GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
    render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
    render.Mesh = mesh;
    render.Flags = mesh->Mat.Flags | arg->flags;
    render.Color = mesh->Mat.Color;
    render.ColorMul = GFX::TGLColor(tint.r, tint.g, tint.b,
                                    tint.a * clampedAlpha);
    if ( mesh->Mat.Flags & GFX::RFLAGS_LUMTRACY )
        render.VPFadeFactor = WeaponTracerClamp01(clampedAlpha * tint.a);
    render.TForm = transform;
    render.Distance = distance;
    render.TimeStamp = arg->globTime;
    render.FrameTime = arg->frameTime;
    render.FogStart = vizLimit - fadeLength;
    render.FogLength = fadeLength;

    arg->adeCount += mesh->Indixes.size() / 3;
    GFX::Engine.QueueRenderMesh(&render);
}

static void WeaponTracerQueueSmoke(baseRender_msg *arg, GFX::TMesh *mesh,
                                   TF::TForm3D *view, const vec3d &worldPos,
                                   float size, float alpha,
                                   const World::TVisualTint &tracerTint,
                                   float vizLimit, float fadeLength)
{
    if ( !arg || !mesh || !view || mesh->Vertexes.empty() ||
         !WeaponTracerFinite(worldPos) || !std::isfinite(size) ||
         size <= 0.01f || !std::isfinite(alpha) || alpha <= 0.0f )
        return;

    const vec3d viewPos = view->CalcSclRot.Transform(worldPos - view->CalcPos);
    const float distance = viewPos.length();
    if ( distance > vizLimit + size )
        return;

    mat4x4 transform(mat3x3::Scale(vec3d(size, size, 1.0f)));
    transform.m03 = viewPos.x;
    transform.m13 = viewPos.y;
    transform.m23 = viewPos.z;

    World::TVisualTint smokeTint;
    smokeTint.r = 0.28f + tracerTint.r * 0.12f;
    smokeTint.g = 0.28f + tracerTint.g * 0.12f;
    smokeTint.b = 0.28f + tracerTint.b * 0.12f;
    smokeTint.a = 1.0f;

    GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
    render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
    render.Mesh = mesh;
    render.Flags = mesh->Mat.Flags | arg->flags;
    render.Color = mesh->Mat.Color;
    render.ColorMul = GFX::TGLColor(smokeTint.r, smokeTint.g, smokeTint.b,
                                    WeaponTracerClamp01(alpha * tracerTint.a));
    render.TForm = transform;
    render.Distance = distance;
    render.TimeStamp = arg->globTime;
    render.FrameTime = arg->frameTime;
    render.FogStart = vizLimit - fadeLength;
    render.FogLength = fadeLength;

    arg->adeCount += mesh->Indixes.size() / 3;
    GFX::Engine.QueueRenderMesh(&render);
}

} // namespace

bool NC_STACK_ypaworld::SpawnMinigunTracer(
    const vec3d &origin, const vec3d &direction, float availableDistance,
    const World::TWeaponTracerConfig &config)
{
    if ( _isNetGame || !config.enabled || !WeaponTracerFinite(origin) ||
         !WeaponTracerFinite(direction) ||
         !std::isfinite(availableDistance) || availableDistance <= 0.01f ||
         !std::isfinite(config.length) || config.length <= 0.01f ||
         !std::isfinite(config.width) || config.width <= 0.01f ||
         config.duration <= 0 || !WeaponTracerFinite(config.tint) ||
         config.tint.a <= 0.0f )
        return false;

    vec3d rayDirection = direction;
    if ( rayDirection.normalise() <= 0.001f )
        return false;

    CleanupExpiredMinigunTracers();

    if ( _mgunTracers.size() >= MGUN_TRACER_LIMIT )
        _mgunTracers.erase(_mgunTracers.begin());

    TMinigunTracer tracer;
    tracer.origin = origin;
    tracer.direction = rayDirection;
    tracer.availableDistance = availableDistance;
    tracer.startTime = _timeStamp;
    tracer.visualSeed = WeaponTracerHash((uint32_t)_timeStamp ^
                                         (uint32_t)(_mgunTracers.size() + 1));
    tracer.config = config;
    tracer.config.tint.Clamp();
    tracer.config.tint_head.Clamp();
    tracer.config.tint_tail.Clamp();
    tracer.config.core_tint.Clamp();
    _mgunTracers.push_back(tracer);
    return true;
}

void NC_STACK_ypaworld::CleanupExpiredMinigunTracers()
{
    _mgunTracers.erase(
        std::remove_if(_mgunTracers.begin(), _mgunTracers.end(),
                       [this](const TMinigunTracer &tracer)
                       {
                           const int32_t age = _timeStamp - tracer.startTime;
                           const float travelTime =
                               MinigunTracerTravelTimeMs(tracer.availableDistance);
                           return age < 0 ||
                                  (float)age >= travelTime + tracer.config.duration;
                       }),
        _mgunTracers.end());
}

void NC_STACK_ypaworld::RenderMinigunTracers(baseRender_msg *arg)
{
    if ( !arg || _isNetGame || _mgunTracers.empty() )
        return;

    CleanupExpiredMinigunTracers();

    for (const TMinigunTracer &tracer : _mgunTracers)
    {
        const int32_t age = std::max(0, _timeStamp - tracer.startTime);
        const float ageSeconds = (float)age * 0.001f;
        const float headDistance = std::min(
            tracer.availableDistance, MGUN_TRACER_VISUAL_SPEED * ageSeconds);
        if ( headDistance <= 0.01f )
            continue;

        // The MGUN hit remains immediate. Only its visual head travels along
        // the already resolved hitscan ray, using the existing internal speed.
        const float oldestVisibleDistance = std::min(
            tracer.availableDistance,
            MGUN_TRACER_VISUAL_SPEED *
                std::max(0.0f, ageSeconds - tracer.config.duration * 0.001f));
        const float tailDistance = std::max(
            oldestVisibleDistance,
            std::max(0.0f, headDistance - tracer.config.length));

        const float travelTime =
            MinigunTracerTravelTimeMs(tracer.availableDistance);
        const float impactAge = std::max(0.0f, (float)age - travelTime);

        float fade = 1.0f;
        if ( tracer.config.advanced && tracer.config.custom_fade )
        {
            if ( tracer.config.fade_in > 0 )
                fade *= World::ComputeVPFadeEnvelope(
                    (double)age, 0.0, (double)tracer.config.fade_in, 0.0);

            if ( impactAge > 0.0f && tracer.config.fade_out > 0 )
                fade *= World::ComputeVPFadeEnvelope(
                    (double)impactAge, (double)tracer.config.fade_out, 0.0,
                    (double)tracer.config.fade_out);
        }
        else
        {
            fade = 1.0f - impactAge / (float)tracer.config.duration;
        }

        const vec3d start = tracer.origin + tracer.direction * tailDistance;
        const vec3d end = tracer.origin + tracer.direction * headDistance;
        RenderWeaponTracerSegment(arg, start, end, tracer.config,
                                  0.0f, 1.0f, fade, tracer.visualSeed);
    }
}

void NC_STACK_ypaworld::ClearMinigunTracers()
{
    _mgunTracers.clear();
}

void NC_STACK_ypaworld::RenderWeaponTracerSegment(
    baseRender_msg *arg, const vec3d &start, const vec3d &end,
    const World::TWeaponTracerConfig &config,
    float tailFactor, float headFactor, float fade, uint32_t visualSeed)
{
    if ( !arg || _isNetGame || !WeaponTracerFinite(start) ||
         !WeaponTracerFinite(end) || !WeaponTracerFinite(config.tint) ||
         !std::isfinite(config.width) || config.width <= 0.01f ||
         !std::isfinite(fade) || fade <= 0.0f || config.tint.a <= 0.0f )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    if ( !config.advanced )
    {
        WeaponTracerBuildLegacyMesh(&_weaponTracerMesh);
        WeaponTracerQueueSegment(arg, &_weaponTracerMesh, view,
                                 start, end, config.width, config.tint, fade,
                                 (float)_normalVizLimit,
                                 (float)_normalFadeLength);
        return;
    }

    WeaponTracerBuildAdvancedMesh(&_weaponTracerAdvancedMesh, false);
    if ( _weaponTracerAdvancedMesh.Vertexes.empty() )
        return;

    if ( config.glow > 0.0f || config.sparks > 0 )
        WeaponTracerBuildAdvancedMesh(&_weaponTracerGlowMesh, true);
    if ( config.smoke > 0 )
        WeaponTracerBuildSmokeMesh(&_weaponTracerSmokeMesh);

    const float u0 = WeaponTracerClamp01(std::min(tailFactor, headFactor));
    const float u1 = WeaponTracerClamp01(std::max(tailFactor, headFactor));
    const float uSpan = std::max(0.0001f, u1 - u0);
    const float pulseFactor = WeaponTracerPulseFactor(config, _timeStamp);

    int desiredSamples = 1;
    if ( std::fabs(config.head_width - config.tail_width) > 0.0001f ||
         WeaponTracerHasTintGradient(config) )
    {
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(12.0f * uSpan));
    }
    if ( config.wave > 0.0f )
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(std::min(48.0f, (float)config.wave_count * 4.0f) * uSpan));
    if ( config.noise > 0.0f )
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(24.0f * uSpan));
    const int subdivisions = std::max(1, std::min(desiredSamples, 8));

    for (int part = 0; part < subdivisions; part++)
    {
        const float local0 = (float)part / (float)subdivisions;
        const float local1 = (float)(part + 1) / (float)subdivisions;
        const float partU0 = WeaponTracerLerp(u0, u1, local0);
        const float partU1 = WeaponTracerLerp(u0, u1, local1);

        vec3d partStart = start + direction * local0;
        vec3d partEnd = start + direction * local1;
        partStart += WeaponTracerAdvancedOffset(direction, partStart, config,
                                                partU0, visualSeed);
        partEnd += WeaponTracerAdvancedOffset(direction, partEnd, config,
                                              partU1, visualSeed);

        const float middle = (partU0 + partU1) * 0.5f;
        const float profileWidth = WeaponTracerLerp(config.tail_width,
                                                    config.head_width,
                                                    middle);
        const float width = config.width * profileWidth * pulseFactor;
        if ( width <= 0.01f )
            continue;

        const World::TVisualTint tint = WeaponTracerTintAt(config, middle);

        if ( config.glow > 0.0f && !_weaponTracerGlowMesh.Vertexes.empty() )
        {
            const float glowAmount = std::min(10.0f, config.glow);
            const float glowWidth = width * (1.0f + glowAmount * 0.35f);
            const float glowAlpha = fade * std::min(1.0f, glowAmount * 0.10f);
            WeaponTracerQueueSegment(arg, &_weaponTracerGlowMesh, view,
                                     partStart, partEnd, glowWidth, tint,
                                     glowAlpha, (float)_normalVizLimit,
                                     (float)_normalFadeLength);
        }

        // The advanced body reproduces the legacy outer ribbon opacity. The
        // configurable core below reproduces the old 0.32 width ratio by default.
        WeaponTracerQueueSegment(arg, &_weaponTracerAdvancedMesh, view,
                                 partStart, partEnd, width, tint, fade * 0.22f,
                                 (float)_normalVizLimit,
                                 (float)_normalFadeLength);

        if ( config.core_enabled && config.core_width > 0.0f )
        {
            const World::TVisualTint coreTint = config.has_core_tint ?
                config.core_tint : tint;
            WeaponTracerQueueSegment(arg, &_weaponTracerAdvancedMesh, view,
                                     partStart, partEnd,
                                     width * config.core_width,
                                     coreTint, fade,
                                     (float)_normalVizLimit,
                                     (float)_normalFadeLength);
        }
    }

    // Sparks and smoke are deterministic slots over the normalized visible
    // tracer, so their requested quantity is not multiplied by path segments.
    if ( config.sparks > 0 && !_weaponTracerGlowMesh.Vertexes.empty() )
    {
        for (int i = 0; i < config.sparks; i++)
        {
            const float slotU = ((float)i + 0.5f) / (float)config.sparks;
            if ( slotU < u0 || (slotU >= u1 && u1 < 0.9999f) )
                continue;

            const float local = WeaponTracerClamp01((slotU - u0) / uSpan);
            vec3d pos = start + direction * local;
            pos += WeaponTracerAdvancedOffset(direction, pos, config, slotU,
                                              visualSeed);

            vec3d dir = direction;
            dir.normalise();
            const vec3d side = WeaponTracerStableSide(dir, visualSeed + (uint32_t)i * 17u);
            vec3d up = dir * side;
            up.normalise();
            vec3d sparkDir = dir * 0.25 +
                side * WeaponTracerHashSigned(visualSeed, (uint32_t)i * 5u + 1u) +
                up * WeaponTracerHashSigned(visualSeed, (uint32_t)i * 5u + 2u);
            if ( sparkDir.normalise() <= 0.001f )
                sparkDir = side;

            const float randomness = std::fabs(
                WeaponTracerHashSigned(visualSeed, (uint32_t)i * 5u + 3u));
            const float sparkLength = std::max(1.0f, config.width *
                                                (1.5f + randomness * 3.0f));
            const float sparkWidth = std::max(0.25f, config.width * 0.12f);
            const World::TVisualTint sparkTint = WeaponTracerTintAt(config, slotU);

            WeaponTracerQueueSegment(arg, &_weaponTracerGlowMesh, view,
                                     pos - sparkDir * (sparkLength * 0.5f),
                                     pos + sparkDir * (sparkLength * 0.5f),
                                     sparkWidth, sparkTint, fade * 0.75f,
                                     (float)_normalVizLimit,
                                     (float)_normalFadeLength);
        }
    }

    if ( config.smoke > 0 && !_weaponTracerSmokeMesh.Vertexes.empty() )
    {
        for (int i = 0; i < config.smoke; i++)
        {
            const float slotU = ((float)i + 0.5f) / (float)config.smoke;
            if ( slotU < u0 || (slotU >= u1 && u1 < 0.9999f) )
                continue;

            const float local = WeaponTracerClamp01((slotU - u0) / uSpan);
            vec3d pos = start + direction * local;
            pos += WeaponTracerAdvancedOffset(direction, pos, config, slotU,
                                              visualSeed);

            const float randomSize = 0.85f + 0.35f * std::fabs(
                WeaponTracerHashSigned(visualSeed, (uint32_t)i * 7u + 5u));
            const float ageExpansion = 1.0f + (1.0f - slotU) * 1.4f;
            const float smokeSize = std::max(1.0f, config.width * 3.0f *
                                              randomSize * ageExpansion);
            const World::TVisualTint smokeSource = WeaponTracerTintAt(config, slotU);
            WeaponTracerQueueSmoke(arg, &_weaponTracerSmokeMesh, view, pos,
                                   smokeSize, fade * 0.24f, smokeSource,
                                   (float)_normalVizLimit,
                                   (float)_normalFadeLength);
        }
    }
}

void NC_STACK_ypaworld::ClearWeaponTracerMesh()
{
    _weaponTracerMesh = GFX::TMesh();
    _weaponTracerAdvancedMesh = GFX::TMesh();
    _weaponTracerGlowMesh = GFX::TMesh();
    _weaponTracerSmokeMesh = GFX::TMesh();
}
