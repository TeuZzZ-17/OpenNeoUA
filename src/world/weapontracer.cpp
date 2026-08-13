#include "../yw.h"

#include <algorithm>
#include <cmath>

namespace
{

const size_t MGUN_TRACER_LIMIT = 512;
const float MGUN_TRACER_VISUAL_SPEED = 6000.0f;

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

static void WeaponTracerBuildMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Same crossed-ribbon style as the approved earlier tracer: a soft outer
    // body plus a narrow bright core, both readable from every camera angle.
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

}

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
    tracer.config = config;
    tracer.config.tint.Clamp();
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
        // the already resolved hitscan ray, using a fixed internal speed. The
        // time window mirrors Weapon tracer samples: it limits the oldest
        // visible point and fades the final sample after it reaches the impact.
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
        const float fade = 1.0f -
            impactAge / (float)tracer.config.duration;

        const vec3d start = tracer.origin + tracer.direction * tailDistance;
        const vec3d end = tracer.origin + tracer.direction * headDistance;
        RenderWeaponTracerSegment(arg, start, end,
                                  tracer.config.width,
                                  tracer.config.tint, fade);
    }
}

void NC_STACK_ypaworld::ClearMinigunTracers()
{
    _mgunTracers.clear();
}

void NC_STACK_ypaworld::RenderWeaponTracerSegment(
    baseRender_msg *arg, const vec3d &start, const vec3d &end, float width,
    const World::TVisualTint &tint, float fade)
{
    if ( !arg || _isNetGame || !WeaponTracerFinite(start) ||
         !WeaponTracerFinite(end) || !WeaponTracerFinite(tint) ||
         !std::isfinite(width) || width <= 0.01f ||
         !std::isfinite(fade) || fade <= 0.0f || tint.a <= 0.0f )
        return;

    vec3d direction = end - start;
    const float segmentLength = direction.length();
    if ( !std::isfinite(segmentLength) || segmentLength <= 0.01f )
        return;

    mat3x3 rotation = WeaponTracerRotationFromDir(direction);
    if ( rotation.AxisZ().length() <= 0.001f )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    WeaponTracerBuildMesh(&_weaponTracerMesh);
    if ( _weaponTracerMesh.Vertexes.empty() )
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
    if ( distance > (float)_normalVizLimit + segmentLength )
        return;

    GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
    render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
    render.Mesh = &_weaponTracerMesh;
    render.Flags = _weaponTracerMesh.Mat.Flags | arg->flags;
    render.Color = _weaponTracerMesh.Mat.Color;
    render.ColorMul = GFX::TGLColor(tint.r, tint.g, tint.b,
                                    tint.a * std::min(fade, 1.0f));
    render.TForm = transform;
    render.Distance = distance;
    render.TimeStamp = arg->globTime;
    render.FrameTime = arg->frameTime;
    render.FogStart = (float)(_normalVizLimit - _normalFadeLength);
    render.FogLength = (float)_normalFadeLength;

    arg->adeCount += _weaponTracerMesh.Indixes.size() / 3;
    GFX::Engine.QueueRenderMesh(&render);
}

void NC_STACK_ypaworld::ClearWeaponTracerMesh()
{
    _weaponTracerMesh = GFX::TMesh();
}
