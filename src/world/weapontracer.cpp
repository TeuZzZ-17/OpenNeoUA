#include "../yw.h"

#include <algorithm>
#include <cmath>

namespace
{

const size_t MGUN_TRACER_LIMIT = 512;
const float MGUN_TRACER_VISUAL_SPEED = 6000.0f;
const float WEAPON_TRACER_PI = 3.14159265358979323846f;


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

static void WeaponTracerBuildMesh(GFX::TMesh *mesh, bool additive)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Fixed crossed-ribbon topology: both transverse planes are always present.
    // X/Y dimensions remain data-driven, while topology itself has no public
    // switch and therefore no parallel flat/segmented render path.
    WeaponTracerAppendRibbon(mesh, false, 0.5f, 1.0f);
    WeaponTracerAppendRibbon(mesh, true, 0.5f, 1.0f);

    // Glow reuses the exact same topology and dimensions, so it changes
    // brightness only and never expands the authored mesh.
    uint32_t flags = GFX::RFLAGS_FOG | GFX::RFLAGS_DISABLE_ZWRITE;
    flags |= additive ? GFX::RFLAGS_LUMTRACY : GFX::RFLAGS_ALPHABLEND;
    mesh->Mat = GFX::TRenderParams(flags);
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

static vec3d WeaponTracerNoiseOffset(const vec3d &direction,
                                     const vec3d &basePosition,
                                     const World::TWeaponTracerConfig &config,
                                     float factor, uint32_t seed)
{
    if ( config.noise_rate <= 0.0f )
        return vec3d(0.0, 0.0, 0.0);

    vec3d dir = direction;
    if ( dir.normalise() <= 0.001f )
        return vec3d(0.0, 0.0, 0.0);

    const vec3d side = WeaponTracerStableSide(dir, seed);
    vec3d up = dir * side;
    if ( up.normalise() <= 0.001f )
        up = vec3d::OY(1.0);

    const float u = WeaponTracerClamp01(factor);

    // Quantized deterministic world-position noise: existing trail samples
    // keep the same disturbance instead of flickering with render framerate.
    // The sine envelope brings the disturbance back to zero at both ends.
    const int32_t qx = (int32_t)std::floor(basePosition.x * 0.125);
    const int32_t qy = (int32_t)std::floor(basePosition.y * 0.125);
    const int32_t qz = (int32_t)std::floor(basePosition.z * 0.125);
    uint32_t key = WeaponTracerHash((uint32_t)qx);
    key = WeaponTracerHash(key ^ ((uint32_t)qy * 0x9e3779b9u));
    key = WeaponTracerHash(key ^ ((uint32_t)qz * 0x85ebca6bu));

    const float envelope = std::sin(WEAPON_TRACER_PI * u);
    const float n1 = WeaponTracerHashSigned(seed, key * 2u + 1u);
    const float n2 = WeaponTracerHashSigned(seed, key * 2u + 2u);
    return (side * n1 + up * n2) *
           (config.noise_rate * 0.70710678f * envelope);
}

static float WeaponTracerPulseFactor(const World::TWeaponTracerConfig &config,
                                     int32_t timeStamp)
{
    if ( config.pulse_rate <= 0.0f || config.pulse_speed <= 0.0f )
        return 1.0f;

    const float amplitude = std::min(0.9f, config.pulse_rate * 0.1f);
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
                                     float sizeX, float sizeY,
                                     const World::TVisualTint &tint,
                                     float alpha, float vizLimit,
                                     float fadeLength)
{
    if ( !arg || !mesh || !view || mesh->Vertexes.empty() ||
         !WeaponTracerFinite(start) || !WeaponTracerFinite(end) ||
         !WeaponTracerFinite(tint) || !std::isfinite(sizeX) ||
         !std::isfinite(sizeY) ||
         (sizeX <= 0.01f && sizeY <= 0.01f) ||
         !std::isfinite(alpha) || alpha <= 0.0f || tint.a <= 0.0f )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    const mat3x3 rotation = WeaponTracerRotationFromDir(direction);
    if ( rotation.AxisZ().length() <= 0.001f )
        return;

    const vec3d center = start + direction * 0.5f;

    // Always span the full sampled interval. Endpoint taper changes only the
    // transverse X/Y section; shrinking Z per subdivision creates visible gaps.
    mat4x4 worldForm(rotation.Transpose() * mat3x3::Scale(
        vec3d(std::max(0.0f, sizeX), std::max(0.0f, sizeY),
              segmentLength)));
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

} // namespace

bool NC_STACK_ypaworld::SpawnMinigunTracer(
    const vec3d &origin, const vec3d &direction, float availableDistance,
    const World::TWeaponTracerConfig &config)
{
    if ( _isNetGame || !config.enabled || !WeaponTracerFinite(origin) ||
         !WeaponTracerFinite(direction) ||
         !std::isfinite(availableDistance) || availableDistance <= 0.01f ||
         !std::isfinite(config.size_z) || config.size_z <= 0.01f ||
         !std::isfinite(config.size_x) || config.size_x <= 0.01f ||
         !std::isfinite(config.ResolveSizeY()) || config.ResolveSizeY() < 0.0f ||
         !WeaponTracerFinite(config.tint) ||
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
    // MGUN is hitscan and has no physical projectile lifetime. Derive the
    // visual duration internally from the resolved ray plus authored size_z,
    // so the head reaches the endpoint and the tail can follow it out.
    tracer.duration = std::max(1, (int32_t)std::ceil(
        ((double)availableDistance + (double)config.size_z) * 1000.0 /
        (double)MGUN_TRACER_VISUAL_SPEED));
    tracer.visualSeed = WeaponTracerHash((uint32_t)_timeStamp ^
                                         (uint32_t)(_mgunTracers.size() + 1));
    tracer.config = config;
    tracer.config.tint.Clamp();
    tracer.config.tint_head.Clamp();
    tracer.config.tint_tail.Clamp();
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
                           return age < 0 || age >= tracer.duration;
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
        if ( age >= tracer.duration )
            continue;

        const float ageSeconds = (float)age * 0.001f;
        const float travelled = MGUN_TRACER_VISUAL_SPEED * ageSeconds;
        const float headDistance = std::min(tracer.availableDistance, travelled);
        if ( headDistance <= 0.01f )
            continue;

        // MGUN damage stays hitscan/immediate. The visual head travels along the
        // already resolved ray, then the tail follows it out at the impact/end
        // point. Its lifetime is therefore derived internally from ray distance
        // plus size_z, with no public life/fade control.
        const float tailDistance = std::max(0.0f, std::min(
            tracer.availableDistance, travelled - tracer.config.size_z));
        if ( headDistance - tailDistance <= 0.01f )
            continue;
        const vec3d start = tracer.origin + tracer.direction * tailDistance;
        const vec3d end = tracer.origin + tracer.direction * headDistance;
        RenderWeaponTracerSegment(arg, start, end, tracer.config,
                                  0.0f, 1.0f, tracer.visualSeed);
    }
}

void NC_STACK_ypaworld::ClearMinigunTracers()
{
    _mgunTracers.clear();
}

void NC_STACK_ypaworld::RenderWeaponTracerSegment(
    baseRender_msg *arg, const vec3d &start, const vec3d &end,
    const World::TWeaponTracerConfig &config,
    float tailFactor, float headFactor, uint32_t visualSeed)
{
    if ( !arg || _isNetGame || !WeaponTracerFinite(start) ||
         !WeaponTracerFinite(end) || !WeaponTracerFinite(config.tint) ||
         !std::isfinite(config.size_z) || config.size_z <= 0.01f ||
         !std::isfinite(config.size_x) || config.size_x <= 0.01f ||
         !std::isfinite(config.ResolveSizeY()) || config.ResolveSizeY() < 0.0f ||
         config.tint.a <= 0.0f )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    GFX::TMesh *bodyMesh = &_weaponTracerMesh;
    GFX::TMesh *glowMesh = &_weaponTracerGlowMesh;

    WeaponTracerBuildMesh(bodyMesh, false);
    if ( bodyMesh->Vertexes.empty() )
        return;

    if ( config.glow_rate > 0.0f )
        WeaponTracerBuildMesh(glowMesh, true);

    const float u0 = WeaponTracerClamp01(std::min(tailFactor, headFactor));
    const float u1 = WeaponTracerClamp01(std::max(tailFactor, headFactor));
    const float uSpan = std::max(0.0001f, u1 - u0);
    const float pulseFactor = WeaponTracerPulseFactor(config, _timeStamp);
    const float headSizeX = config.ResolveHeadSizeX();
    const float tailSizeX = config.ResolveTailSizeX();
    const float headSizeY = config.ResolveHeadSizeY();
    const float tailSizeY = config.ResolveTailSizeY();

    int desiredSamples = 1;
    if ( std::fabs(headSizeX - tailSizeX) > 0.0001f ||
         std::fabs(headSizeY - tailSizeY) > 0.0001f ||
         WeaponTracerHasTintGradient(config) )
    {
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(12.0f * uSpan));
    }
    if ( config.noise_rate > 0.0f )
    {
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(24.0f * uSpan));
    }
    const int subdivisions = std::max(1, std::min(desiredSamples, 8));

    for (int part = 0; part < subdivisions; part++)
    {
        const float local0 = (float)part / (float)subdivisions;
        const float local1 = (float)(part + 1) / (float)subdivisions;
        const float partU0 = WeaponTracerLerp(u0, u1, local0);
        const float partU1 = WeaponTracerLerp(u0, u1, local1);

        vec3d partStart = start + direction * local0;
        vec3d partEnd = start + direction * local1;
        partStart += WeaponTracerNoiseOffset(direction, partStart, config,
                                             partU0, visualSeed);
        partEnd += WeaponTracerNoiseOffset(direction, partEnd, config,
                                           partU1, visualSeed);

        const float middle = (partU0 + partU1) * 0.5f;
        const float sizeX = WeaponTracerLerp(tailSizeX, headSizeX, middle) *
                            pulseFactor;
        const float sizeY = WeaponTracerLerp(tailSizeY, headSizeY, middle) *
                            pulseFactor;
        if ( sizeX <= 0.01f && sizeY <= 0.01f )
            continue;

        const World::TVisualTint tint = WeaponTracerTintAt(config, middle);

        // Base body opacity comes only from the authored tint alpha. Each
        // subdivision spans its complete interval, so adjacent taper samples
        // remain contiguous instead of turning into a ladder of separated bars.
        WeaponTracerQueueSegment(arg, bodyMesh, view,
                                 partStart, partEnd, sizeX, sizeY,
                                 tint, 1.0f,
                                 (float)_normalVizLimit,
                                 (float)_normalFadeLength);

        if ( config.glow_rate > 0.0f &&
             !glowMesh->Vertexes.empty() )
        {
            // Glow deliberately reuses the exact body dimensions. The 0..10 rate
            // changes additive intensity only, never authored dimensions.
            const float glowAlpha =
                std::min(1.0f, config.glow_rate * 0.1f);
            WeaponTracerQueueSegment(arg, glowMesh, view,
                                     partStart, partEnd, sizeX, sizeY,
                                     tint, glowAlpha,
                                     (float)_normalVizLimit,
                                     (float)_normalFadeLength);
        }
    }
}

void NC_STACK_ypaworld::ClearWeaponTracerMesh()
{
    _weaponTracerMesh = GFX::TMesh();
    _weaponTracerGlowMesh = GFX::TMesh();
}
