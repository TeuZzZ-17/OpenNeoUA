#include "../yw.h"
#include "../loaders.h"
#include "../log.h"

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

static void WeaponBeamBuildProceduralMesh(GFX::TMesh *mesh, bool additive)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Procedural beam geometry retained for systems that still author their
    // shape internally (currently Laser). Tracers themselves no longer use
    // this builder: their geometry must come from mesh_tracer_path /
    // mgun_mesh_tracer_path.
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

static mat3x3 WeaponTracerRotationFromDir(const vec3d &direction,
                                             const mat3x3 *orientationHint = NULL)
{
    vec3d z = direction;
    if ( z.normalise() <= 0.001f )
        return mat3x3();

    vec3d x;

    if ( orientationHint )
    {
        // Continuous laser beams are redrawn every frame. Anchor their ribbon
        // basis to the firing actor instead of switching abruptly between world
        // X/Y at |dir.y| = 0.9, which can look like lateral wobble even when
        // laser_mesh_noise_rate is exactly zero. Tracers pass no hint and keep
        // their existing orientation path unchanged.
        x = orientationHint->AxisX();
        x -= z * x.dot(z);

        if ( x.normalise() <= 0.001f )
        {
            x = orientationHint->AxisY();
            x -= z * x.dot(z);
        }
    }
    else
    {
        x = fabs(z.y) < 0.9 ? vec3d::OY(1.0) : vec3d::OX(1.0);
        x = x * z;
    }

    if ( x.normalise() <= 0.001f )
    {
        x = fabs(z.y) < 0.9 ? vec3d::OY(1.0) : vec3d::OX(1.0);
        x = x * z;
        if ( x.normalise() <= 0.001f )
            return mat3x3();
    }

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
                                     float noiseRate,
                                     float factor, uint32_t seed)
{
    if ( !std::isfinite(noiseRate) || noiseRate <= 0.0f )
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
           (noiseRate * 0.70710678f * envelope);
}

static float WeaponTracerPulseFactor(float pulseRate, float pulseSpeed,
                                     int32_t timeStamp)
{
    if ( !std::isfinite(pulseRate) || !std::isfinite(pulseSpeed) ||
         pulseRate <= 0.0f || pulseSpeed <= 0.0f )
        return 1.0f;

    const float amplitude = std::min(0.9f, pulseRate * 0.1f);
    const float phase = (float)timeStamp * 0.001f * pulseSpeed *
                        2.0f * WEAPON_TRACER_PI;
    return std::max(0.1f, 1.0f + std::sin(phase) * amplitude);
}

struct TMeshBeamRenderConfig
{
    float tailSizeX = 0.0f;
    float headSizeX = 0.0f;
    float tailSizeY = 0.0f;
    float headSizeY = 0.0f;
    World::TVisualTint tailTint;
    World::TVisualTint headTint;
    float glowRate = 0.0f;
    float noiseRate = 0.0f;
    float pulseRate = 0.0f;
    float pulseSpeed = 0.0f;
    // Laser-only terminal alpha envelope. The generic tracer path leaves these
    // at their neutral defaults. Factors use the full original beam 0..1 span.
    float impactFadeStartFactor = 1.0f;
    float impactFadeEndAlpha = 1.0f;
    int minSubdivisions = 1;
    bool pulseGeometry = false;
    bool pulseBrightness = false;
};

static World::TVisualTint MeshBeamTintAt(const TMeshBeamRenderConfig &config,
                                         float factor)
{
    return WeaponTracerLerpTint(config.tailTint, config.headTint, factor);
}

static bool MeshBeamHasTintGradient(const TMeshBeamRenderConfig &config)
{
    const float epsilon = 0.0001f;
    return std::fabs(config.tailTint.r - config.headTint.r) > epsilon ||
           std::fabs(config.tailTint.g - config.headTint.g) > epsilon ||
           std::fabs(config.tailTint.b - config.headTint.b) > epsilon ||
           std::fabs(config.tailTint.a - config.headTint.a) > epsilon;
}

static void WeaponTracerQueueSegment(baseRender_msg *arg, GFX::TMesh *mesh,
                                     TF::TForm3D *view,
                                     const vec3d &start, const vec3d &end,
                                     float sizeX, float sizeY,
                                     const World::TVisualTint &tint,
                                     float alpha, float vizLimit,
                                     float fadeLength,
                                     const mat3x3 *orientationHint,
                                     bool additive)
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

    const mat3x3 rotation = WeaponTracerRotationFromDir(direction, orientationHint);
    if ( rotation.AxisZ().length() <= 0.001f )
        return;

    const vec3d center = start + direction * 0.5f;

    // External tracer assets use the normalized local convention expected by
    // the runtime: local Z spans -0.5..+0.5, while X/Y are transverse.
    // Runtime size/taper therefore remains authoritative.
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

    // Geometry/material/texture come from the selected mesh, while beam
    // compositing remains runtime-owned so external meshes preserve the same
    // transparent body + optional additive glow semantics.
    uint32_t flags = mesh->Mat.Flags | GFX::RFLAGS_FOG |
                     GFX::RFLAGS_DISABLE_ZWRITE;
    flags &= ~(GFX::RFLAGS_LUMTRACY | GFX::RFLAGS_ALPHABLEND);
    flags |= additive ? GFX::RFLAGS_LUMTRACY : GFX::RFLAGS_ALPHABLEND;
    render.Flags = flags | arg->flags;
    render.Color = mesh->Mat.Color;
    render.ColorMul = GFX::TGLColor(tint.r, tint.g, tint.b,
                                    tint.a * clampedAlpha);

    if ((mesh->Mat.Flags & GFX::RFLAGS_DYNAMIC_TEXTURE) && mesh->Mat.TexSource)
    {
        mesh->Mat.TexSource->SetTime(arg->globTime, arg->frameTime);
        const uint32_t frameId = mesh->Mat.TexSource->GetCurrentFrameID();
        if ( frameId < mesh->CoordsCache.size() )
        {
            render.Tex = mesh->CoordsCache.at(frameId).Tex;
            render.coordsID = frameId;
        }
    }
    else
        render.Tex = mesh->Mat.Tex;

    if ( additive )
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

static void WeaponTracerQueueGeometry(baseRender_msg *arg,
                                      GFX::TMesh *proceduralMesh,
                                      NC_STACK_base *externalMesh,
                                      TF::TForm3D *view,
                                      const vec3d &start, const vec3d &end,
                                      float sizeX, float sizeY,
                                      const World::TVisualTint &tint,
                                      float alpha, float vizLimit,
                                      float fadeLength,
                                      const mat3x3 *orientationHint,
                                      bool additive)
{
    if ( externalMesh && !externalMesh->Meshes.empty() )
    {
        for (GFX::TMesh &mesh : externalMesh->Meshes)
        {
            WeaponTracerQueueSegment(arg, &mesh, view, start, end,
                                     sizeX, sizeY, tint, alpha,
                                     vizLimit, fadeLength, orientationHint,
                                     additive);
        }
        return;
    }

    WeaponTracerQueueSegment(arg, proceduralMesh, view, start, end,
                             sizeX, sizeY, tint, alpha,
                             vizLimit, fadeLength, orientationHint, additive);
}

static void RenderMeshBeamSegment(
    baseRender_msg *arg, GFX::TMesh *bodyMesh, GFX::TMesh *glowMesh,
    const vec3d &start, const vec3d &end,
    const TMeshBeamRenderConfig &config, int32_t timeStamp,
    float vizLimit, float fadeLength, float tailFactor, float headFactor,
    uint32_t visualSeed, const mat3x3 *orientationHint,
    NC_STACK_base *externalMesh, bool allowProceduralGeometry)
{
    if ( !arg || !bodyMesh || !glowMesh || !WeaponTracerFinite(start) ||
         !WeaponTracerFinite(end) || !WeaponTracerFinite(config.tailTint) ||
         !WeaponTracerFinite(config.headTint) ||
         !std::isfinite(config.tailSizeX) ||
         !std::isfinite(config.headSizeX) ||
         !std::isfinite(config.tailSizeY) ||
         !std::isfinite(config.headSizeY) )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    const bool useExternalMesh = externalMesh && !externalMesh->Meshes.empty();
    if ( !useExternalMesh )
    {
        // Tracers pass allowProceduralGeometry=false: their authored external
        // mesh is mandatory. Procedural geometry is retained only for callers
        // such as Laser that still intentionally own an internal beam shape.
        if ( !allowProceduralGeometry )
            return;

        WeaponBeamBuildProceduralMesh(bodyMesh, false);
        if ( bodyMesh->Vertexes.empty() )
            return;
    }

    const float glowRate = std::isfinite(config.glowRate) &&
                           config.glowRate > 0.0f
                               ? config.glowRate
                               : 0.0f;
    const float noiseRate = std::isfinite(config.noiseRate) &&
                            config.noiseRate > 0.0f
                                ? config.noiseRate
                                : 0.0f;
    const float pulseRate = std::isfinite(config.pulseRate) &&
                            config.pulseRate > 0.0f
                                ? config.pulseRate
                                : 0.0f;
    const float pulseSpeed = std::isfinite(config.pulseSpeed) &&
                             config.pulseSpeed > 0.0f
                                 ? config.pulseSpeed
                                 : 0.0f;

    if ( glowRate > 0.0f && !useExternalMesh )
        WeaponBeamBuildProceduralMesh(glowMesh, true);

    const float u0 = WeaponTracerClamp01(std::min(tailFactor, headFactor));
    const float u1 = WeaponTracerClamp01(std::max(tailFactor, headFactor));
    const float uSpan = std::max(0.0001f, u1 - u0);
    const float pulseValue =
        WeaponTracerPulseFactor(pulseRate, pulseSpeed, timeStamp);
    const float geometryPulse = config.pulseGeometry ? pulseValue : 1.0f;

    int desiredSamples = std::max(1, config.minSubdivisions);
    if ( std::fabs(config.headSizeX - config.tailSizeX) > 0.0001f ||
         std::fabs(config.headSizeY - config.tailSizeY) > 0.0001f ||
         MeshBeamHasTintGradient(config) )
    {
        desiredSamples = std::max(desiredSamples,
            (int)std::ceil(12.0f * uSpan));
    }
    if ( noiseRate > 0.0f )
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
        partStart += WeaponTracerNoiseOffset(direction, partStart,
                                             noiseRate, partU0, visualSeed);
        partEnd += WeaponTracerNoiseOffset(direction, partEnd,
                                           noiseRate, partU1, visualSeed);

        const float middle = (partU0 + partU1) * 0.5f;
        const float sizeX = WeaponTracerLerp(config.tailSizeX,
                                             config.headSizeX, middle) *
                            geometryPulse;
        const float sizeY = WeaponTracerLerp(config.tailSizeY,
                                             config.headSizeY, middle) *
                            geometryPulse;
        if ( sizeX <= 0.01f && sizeY <= 0.01f )
            continue;

        World::TVisualTint tint = MeshBeamTintAt(config, middle);

        if ( config.impactFadeStartFactor < 1.0f &&
             middle > config.impactFadeStartFactor )
        {
            const float denom = std::max(0.0001f,
                                         1.0f - config.impactFadeStartFactor);
            const float fadeU = WeaponTracerClamp01(
                (middle - config.impactFadeStartFactor) / denom);
            const float endAlpha = WeaponTracerClamp01(config.impactFadeEndAlpha);
            tint.a *= WeaponTracerLerp(1.0f, endAlpha, fadeU);
        }

        if ( config.pulseBrightness )
        {
            // Laser pulse is intentionally a luminance multiplier only. The
            // segment dimensions stay on the authored size_x/size_y values.
            tint.r *= pulseValue;
            tint.g *= pulseValue;
            tint.b *= pulseValue;
        }

        WeaponTracerQueueGeometry(arg, bodyMesh, externalMesh, view,
                                  partStart, partEnd, sizeX, sizeY,
                                  tint, 1.0f, vizLimit, fadeLength,
                                  orientationHint, false);

        if ( glowRate > 0.0f &&
             (useExternalMesh || !glowMesh->Vertexes.empty()) )
        {
            float glowAlpha = std::min(1.0f, glowRate * 0.1f);
            if ( config.pulseBrightness )
                glowAlpha *= pulseValue;

            WeaponTracerQueueGeometry(arg, glowMesh, externalMesh, view,
                                      partStart, partEnd, sizeX, sizeY,
                                      tint, glowAlpha, vizLimit, fadeLength,
                                      orientationHint, true);
        }
    }
}

} // namespace

bool NC_STACK_ypaworld::SpawnMinigunTracer(
    const vec3d &origin, const vec3d &direction, float availableDistance,
    const World::TWeaponTracerConfig &config)
{
    if ( _isNetGame || !config.enabled || config.mesh_path.empty() ||
         !WeaponTracerFinite(origin) || !WeaponTracerFinite(direction) ||
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

    TMeshBeamRenderConfig renderConfig;
    renderConfig.tailSizeX = config.ResolveTailSizeX();
    renderConfig.headSizeX = config.ResolveHeadSizeX();
    renderConfig.tailSizeY = config.ResolveTailSizeY();
    renderConfig.headSizeY = config.ResolveHeadSizeY();
    renderConfig.tailTint = config.has_tint_tail ? config.tint_tail : config.tint;
    renderConfig.headTint = config.has_tint_head ? config.tint_head : config.tint;
    renderConfig.glowRate = config.glow_rate;
    renderConfig.noiseRate = config.noise_rate;
    renderConfig.pulseRate = config.pulse_rate;
    renderConfig.pulseSpeed = config.pulse_speed;
    renderConfig.pulseGeometry = true;

    // Tracer geometry is fully externalized. No path (or an invalid mesh) means
    // no tracer render; the old crossed-ribbon procedural geometry is not used
    // as a hidden fallback.
    if ( config.mesh_path.empty() )
        return;

    NC_STACK_base *externalMesh = GetWeaponTracerExternalMesh(config.mesh_path);
    if ( !externalMesh || externalMesh->Meshes.empty() )
        return;

    RenderMeshBeamSegment(arg, &_weaponTracerMesh, &_weaponTracerGlowMesh,
                          start, end, renderConfig, _timeStamp,
                          (float)_normalVizLimit, (float)_normalFadeLength,
                          tailFactor, headFactor, visualSeed, NULL, externalMesh,
                          false);
}

void NC_STACK_ypaworld::RenderLaserMeshSegment(
    baseRender_msg *arg, const vec3d &start, const vec3d &end,
    const World::TWeapProto::TLaserMeshConfig &config, bool impactContact,
    const mat3x3 &orientationHint, uint32_t visualSeed)
{
    const float sizeY = config.ResolveSizeY();
    if ( !arg || _isNetGame || !config.enabled ||
         !WeaponTracerFinite(start) || !WeaponTracerFinite(end) ||
         !WeaponTracerFinite(config.tint) || config.tint.a <= 0.0f ||
         !std::isfinite(config.size_x) || config.size_x <= 0.01f ||
         !std::isfinite(sizeY) || sizeY <= 0.01f )
        return;

    vec3d beamDirection = end - start;
    const float beamLength = beamDirection.length();
    if ( !std::isfinite(beamLength) || beamLength <= 0.01f )
        return;

    TMeshBeamRenderConfig renderConfig;
    renderConfig.tailSizeX = config.size_x;
    renderConfig.headSizeX = config.size_x;
    renderConfig.tailSizeY = sizeY;
    renderConfig.headSizeY = sizeY;
    renderConfig.tailTint = config.tint;
    renderConfig.headTint = config.tint;
    renderConfig.glowRate = config.glow_rate;
    renderConfig.noiseRate = config.noise_rate;
    renderConfig.pulseRate = config.pulse_rate;
    renderConfig.pulseSpeed = config.pulse_speed;
    // Laser pulse changes only the body/glow luminance; geometry remains fixed.
    renderConfig.pulseBrightness = true;

    const bool useImpactFade = impactContact &&
                               std::isfinite(config.impact_fade_length) &&
                               config.impact_fade_length > 0.01f;

    if ( !useImpactFade )
    {
        RenderMeshBeamSegment(arg, &_weaponTracerMesh, &_weaponTracerGlowMesh,
                              start, end, renderConfig, _timeStamp,
                              (float)_normalVizLimit, (float)_normalFadeLength,
                              0.0f, 1.0f, visualSeed, &orientationHint, NULL,
                              true);
        return;
    }

    const float terminalLength = std::min(config.impact_fade_length, beamLength);
    const float fadeStartFactor = WeaponTracerClamp01(
        1.0f - terminalLength / beamLength);
    const vec3d fadeStart = start + beamDirection * fadeStartFactor;

    if ( fadeStartFactor > 0.0001f )
    {
        RenderMeshBeamSegment(arg, &_weaponTracerMesh, &_weaponTracerGlowMesh,
                              start, fadeStart, renderConfig, _timeStamp,
                              (float)_normalVizLimit, (float)_normalFadeLength,
                              0.0f, fadeStartFactor, visualSeed, &orientationHint, NULL,
                              true);
    }

    TMeshBeamRenderConfig fadeConfig = renderConfig;
    fadeConfig.impactFadeStartFactor = fadeStartFactor;
    fadeConfig.impactFadeEndAlpha = 0.02f;
    // Keep the terminal envelope visually smooth even when it occupies only a
    // tiny fraction of a long beam. This affects laser only; tracer sampling is
    // unchanged.
    fadeConfig.minSubdivisions = 8;

    RenderMeshBeamSegment(arg, &_weaponTracerMesh, &_weaponTracerGlowMesh,
                          fadeStart, end, fadeConfig, _timeStamp,
                          (float)_normalVizLimit, (float)_normalFadeLength,
                          fadeStartFactor, 1.0f, visualSeed, &orientationHint, NULL,
                          true);
}

NC_STACK_base *NC_STACK_ypaworld::GetWeaponTracerExternalMesh(
    const std::string &path)
{
    if ( path.empty() )
        return NULL;

    const auto cached = _weaponTracerExternalMeshes.find(path);
    if ( cached != _weaponTracerExternalMeshes.end() )
        return cached->second;

    NC_STACK_base *meshObject = Utils::ProxyLoadBase(path);
    if ( !meshObject || meshObject->Meshes.empty() )
    {
        if ( meshObject )
            meshObject->Delete();

        // Cache failures too so a bad authored path does not trigger disk I/O
        // for every tracer segment every frame. The tracer remains invisible:
        // there is intentionally no procedural geometry fallback.
        _weaponTracerExternalMeshes[path] = NULL;
        ypa_log_out("Weapon tracer external mesh '%s' could not be loaded; tracer disabled.\n",
                    path.c_str());
        return NULL;
    }

    meshObject->MakeVBO();
    _weaponTracerExternalMeshes[path] = meshObject;
    return meshObject;
}

void NC_STACK_ypaworld::ClearWeaponTracerMesh()
{
    _weaponTracerMesh = GFX::TMesh();
    _weaponTracerGlowMesh = GFX::TMesh();

    for (auto &entry : _weaponTracerExternalMeshes)
    {
        if ( entry.second )
            entry.second->Delete();
    }
    _weaponTracerExternalMeshes.clear();
}
