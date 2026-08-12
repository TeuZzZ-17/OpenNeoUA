#include "../yw.h"

#include <algorithm>
#include <cmath>

namespace
{

const size_t MGUN_TRACER_LIMIT = 512;

static bool MgunTracerFinite(const vec3d &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

static bool MgunTracerFinite(const World::TVisualTint &tint)
{
    return std::isfinite(tint.r) && std::isfinite(tint.g) &&
           std::isfinite(tint.b) && std::isfinite(tint.a);
}

static void MgunTracerAppendVertex(GFX::TMesh *mesh, const vec3f &pos, float alpha)
{
    GFX::TVertex vertex(pos);
    vertex.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, alpha);
    mesh->Vertexes.push_back(vertex);
}

static void MgunTracerAppendRibbon(GFX::TMesh *mesh, bool vertical,
                                   float halfWidth, float alpha)
{
    const GFX::IndexType base = (GFX::IndexType)mesh->Vertexes.size();

    if ( vertical )
    {
        MgunTracerAppendVertex(mesh, vec3f(0.0f, -halfWidth, -0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f(0.0f,  halfWidth, -0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f(0.0f,  halfWidth,  0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f(0.0f, -halfWidth,  0.5f), alpha);
    }
    else
    {
        MgunTracerAppendVertex(mesh, vec3f(-halfWidth, 0.0f, -0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f( halfWidth, 0.0f, -0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f( halfWidth, 0.0f,  0.5f), alpha);
        MgunTracerAppendVertex(mesh, vec3f(-halfWidth, 0.0f,  0.5f), alpha);
    }

    const GFX::IndexType indices[] = {
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 2), (GFX::IndexType)(base + 1),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 3), (GFX::IndexType)(base + 2),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 1), (GFX::IndexType)(base + 2),
        (GFX::IndexType)(base + 0), (GFX::IndexType)(base + 2), (GFX::IndexType)(base + 3)
    };
    mesh->Indixes.insert(mesh->Indixes.end(), indices, indices + 12);
}

static void MgunTracerBuildMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    // Two crossed ribbons stay readable from every camera angle. The narrower
    // pair supplies the bright core without a texture, VP or new shader.
    MgunTracerAppendRibbon(mesh, false, 0.5f, 0.22f);
    MgunTracerAppendRibbon(mesh, true,  0.5f, 0.22f);
    MgunTracerAppendRibbon(mesh, false, 0.16f, 1.0f);
    MgunTracerAppendRibbon(mesh, true,  0.16f, 1.0f);

    mesh->Mat = GFX::TRenderParams(GFX::RFLAGS_FOG |
                                   GFX::RFLAGS_DISABLE_ZWRITE |
                                   GFX::RFLAGS_ALPHABLEND);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

}

bool NC_STACK_ypaworld::SpawnMinigunTracer(
    const vec3d &origin, const mat3x3 &rotation, float availableDistance,
    const World::TMgunTracerConfig &config)
{
    if ( _isNetGame || !config.enabled || !MgunTracerFinite(origin) ||
         !std::isfinite(availableDistance) || availableDistance <= 0.01f ||
         !std::isfinite(config.length) || config.length <= 0.01f ||
         !std::isfinite(config.width) || config.width <= 0.01f ||
         config.duration <= 0 || !std::isfinite(config.speed) ||
         config.speed <= 0.01f || !MgunTracerFinite(config.tint) ||
         config.tint.a <= 0.0f )
        return false;

    vec3d direction = rotation.AxisZ();
    if ( direction.normalise() <= 0.001f || !MgunTracerFinite(direction) )
        return false;

    _mgunTracers.erase(
        std::remove_if(_mgunTracers.begin(), _mgunTracers.end(),
                       [this](const TMgunTracer &tracer)
                       {
                           const int32_t age = _timeStamp - tracer.startTime;
                           return age < 0 || age >= tracer.config.duration;
                       }),
        _mgunTracers.end());

    if ( _mgunTracers.size() >= MGUN_TRACER_LIMIT )
        _mgunTracers.erase(_mgunTracers.begin());

    TMgunTracer tracer;
    tracer.origin = origin;
    tracer.rotation = rotation;
    tracer.availableDistance = availableDistance;
    tracer.startTime = _timeStamp;
    tracer.config = config;
    tracer.config.tint.Clamp();
    _mgunTracers.push_back(tracer);
    return true;
}

void NC_STACK_ypaworld::RenderMinigunTracers(baseRender_msg *arg)
{
    if ( !arg || _mgunTracers.empty() )
        return;

    _mgunTracers.erase(
        std::remove_if(_mgunTracers.begin(), _mgunTracers.end(),
                       [this](const TMgunTracer &tracer)
                       {
                           const int32_t age = _timeStamp - tracer.startTime;
                           return age < 0 || age >= tracer.config.duration;
                       }),
        _mgunTracers.end());

    if ( _mgunTracers.empty() )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    MgunTracerBuildMesh(&_mgunTracerMesh);
    if ( _mgunTracerMesh.Vertexes.empty() )
        return;

    for (const TMgunTracer &tracer : _mgunTracers)
    {
        const int32_t age = std::max(0, _timeStamp - tracer.startTime);
        const float travelled = tracer.config.speed * (float)age * 0.001f;
        const float segmentEnd = std::min(tracer.availableDistance,
                                          tracer.config.length + travelled);
        const float segmentStart = std::max(0.0f,
                                            segmentEnd - tracer.config.length);
        const float segmentLength = segmentEnd - segmentStart;
        if ( segmentLength <= 0.01f )
            continue;

        vec3d direction = tracer.rotation.AxisZ();
        if ( direction.normalise() <= 0.001f )
            continue;

        const vec3d center = tracer.origin + direction *
                             (segmentStart + segmentLength * 0.5f);
        // BACT axes are stored as matrix rows, while mesh transforms consume
        // column vectors. Transpose so the mesh Z axis follows AxisZ()/the ray.
        mat4x4 worldForm(tracer.rotation.Transpose() * mat3x3::Scale(
            vec3d(tracer.config.width, tracer.config.width, segmentLength)));
        worldForm.m03 = center.x;
        worldForm.m13 = center.y;
        worldForm.m23 = center.z;

        mat4x4 transform = view->CalcSclRot;
        transform *= (worldForm - view->CalcPos);
        const float distance = transform.getTranslate().length();
        if ( distance > (float)_normalVizLimit + segmentLength )
            continue;

        const float fade = World::ComputeVPFadeEnvelope(
            (double)age, (double)tracer.config.duration,
            0.0, (double)tracer.config.duration);
        if ( fade <= 0.0f )
            continue;

        GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
        render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
        render.Mesh = &_mgunTracerMesh;
        render.Flags = _mgunTracerMesh.Mat.Flags | arg->flags;
        render.Color = _mgunTracerMesh.Mat.Color;
        render.ColorMul = GFX::TGLColor(tracer.config.tint.r,
                                        tracer.config.tint.g,
                                        tracer.config.tint.b,
                                        tracer.config.tint.a * fade);
        render.TForm = transform;
        render.Distance = distance;
        render.TimeStamp = arg->globTime;
        render.FrameTime = arg->frameTime;
        render.FogStart = (float)(_normalVizLimit - _normalFadeLength);
        render.FogLength = (float)_normalFadeLength;

        arg->adeCount += _mgunTracerMesh.Indixes.size() / 3;
        GFX::Engine.QueueRenderMesh(&render);
    }
}

void NC_STACK_ypaworld::ClearMinigunTracers()
{
    _mgunTracers.clear();
}
