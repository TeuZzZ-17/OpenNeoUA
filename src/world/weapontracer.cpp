#include "../yw.h"

#include <algorithm>
#include <cmath>

namespace
{

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
