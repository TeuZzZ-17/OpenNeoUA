// Smoke rising from ground decals: emitter list and billboard rendering.
#include <algorithm>
#include <cmath>

#include "grounddecal_limit.h"
#include "grounddecal_smoke.h"
#include "../system/inivals.h"
#include "game/yw.h"

namespace
{

constexpr int GROUND_DECAL_SMOKE_DISC_SEGMENTS = 8;

// Global cap of smoking decals; 0 disables smoke everywhere. It can never exceed
// gfx.ground_decal_limit, because only an existing decal can smoke.
static int GroundDecalSmokeLimit()
{
    const int value = System::IniConf::GfxGroundDecalSmokeLimit.Get<int32_t>();
    return std::max(0, std::min(value, World::GroundDecalLimit()));
}

static float GroundDecalSmokeClampUnit(float value)
{
    return std::max(0.0f, std::min(value, 1.0f));
}

static bool GroundDecalSmokeFinite(const vec3d &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

// Both windings are emitted so the mesh is visible from any side.
static void GroundDecalSmokeAddTriangle(GFX::TMesh *mesh, GFX::IndexType a,
                                        GFX::IndexType b, GFX::IndexType c)
{
    mesh->Indixes.push_back(a);
    mesh->Indixes.push_back(b);
    mesh->Indixes.push_back(c);
    mesh->Indixes.push_back(a);
    mesh->Indixes.push_back(c);
    mesh->Indixes.push_back(b);
}

// Soft round disc of diameter 1: opaque in the middle, fully transparent on the
// rim. Colour and opacity come from the render node.
static void GroundDecalSmokeBuildDiscMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    const float ringRadius[] = {0.20f, 0.35f, 0.50f};
    const float ringAlpha[] = {0.85f, 0.45f, 0.0f};
    const int rings = 3;
    const int segments = GROUND_DECAL_SMOKE_DISC_SEGMENTS;

    GFX::TVertex center(vec3f(0.0f, 0.0f, 0.0f));
    center.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->Vertexes.push_back(center);

    for (int ring = 0; ring < rings; ++ring)
    {
        for (int i = 0; i < segments; ++i)
        {
            const float angle = 2.0f * World::GroundDecalSmoke::kPi * (float)i / (float)segments;
            GFX::TVertex vertex(vec3f(std::cos(angle) * ringRadius[ring],
                                      std::sin(angle) * ringRadius[ring], 0.0f));
            vertex.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, ringAlpha[ring]);
            mesh->Vertexes.push_back(vertex);
        }
    }

    const auto ringVertex = [segments](int ring, int i)
    {
        return (GFX::IndexType)(1 + ring * segments + (i % segments));
    };

    for (int i = 0; i < segments; ++i)
        GroundDecalSmokeAddTriangle(mesh, 0, ringVertex(0, i), ringVertex(0, i + 1));

    for (int ring = 0; ring + 1 < rings; ++ring)
    {
        for (int i = 0; i < segments; ++i)
        {
            GroundDecalSmokeAddTriangle(mesh, ringVertex(ring, i), ringVertex(ring + 1, i),
                                        ringVertex(ring + 1, i + 1));
            GroundDecalSmokeAddTriangle(mesh, ringVertex(ring, i), ringVertex(ring + 1, i + 1),
                                        ringVertex(ring, i + 1));
        }
    }

    mesh->Mat = GFX::TRenderParams(GFX::RFLAGS_FOG |
                                   GFX::RFLAGS_DISABLE_ZWRITE |
                                   GFX::RFLAGS_ALPHABLEND);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

}

bool NC_STACK_ypaworld::SpawnGroundDecalSmoke(const World::TGroundDecalSmoke &smoke,
                                              const vec3d &pos, float radius, uint32_t seed)
{
    namespace GDS = World::GroundDecalSmoke;

    const int limit = GroundDecalSmokeLimit();
    if ( !smoke.enabled || limit <= 0 || smoke.chance <= 0 || smoke.count_max <= 0 ||
         !GroundDecalSmokeFinite(pos) || !std::isfinite(radius) || radius <= 0.0f )
        return false;

    // Not every decal smokes: the roll is stable for a given decal.
    if ( GDS::HashUnit(GDS::Hash(seed, 0x43484e43u, 0u)) * 100.0f >= (float)smoke.chance )
        return false;

    GDS::Emitter emitter;
    emitter.params = smoke;
    emitter.pos = pos;
    emitter.radius = radius;
    emitter.startTime = _timeStamp;
    emitter.seed = seed;
    // Every decal rolls its own puff count inside smoke_count = min_max.
    const int span = smoke.count_max - smoke.count_min + 1;
    const int rolled = std::min(smoke.count_min + (int)(GDS::HashUnit(GDS::Hash(seed, 0x434e5452u, 0u)) * (float)span),
                                smoke.count_max);
    emitter.count = GDS::ScaledCount(rolled, radius, GDS::kMaxPuffs);
    emitter.duration = (int32_t)GDS::RangeFrom(GDS::Hash(seed, 0x53445552u, 0u),
                                               (float)smoke.duration_min,
                                               (float)smoke.duration_max);
    if ( emitter.count <= 0 || emitter.duration <= 0 )
        return false;

    _groundDecalSmoke.erase(
        std::remove_if(_groundDecalSmoke.begin(), _groundDecalSmoke.end(),
                       [this](const GDS::Emitter &old)
                       {
                           const int32_t age = _timeStamp - old.startTime;
                           return age < 0 || age >= old.duration;
                       }),
        _groundDecalSmoke.end());

    // The oldest smoking decal stops first when the limit is reached.
    while ( _groundDecalSmoke.size() >= (size_t)limit )
        _groundDecalSmoke.erase(_groundDecalSmoke.begin());

    _groundDecalSmoke.push_back(emitter);
    return true;
}

void NC_STACK_ypaworld::RenderGroundDecalSmoke(baseRender_msg *arg)
{
    if ( !arg || _groundDecalSmoke.empty() )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    GroundDecalSmokeBuildDiscMesh(&_groundDecalSmokeMesh);
    if ( _groundDecalSmokeMesh.Vertexes.empty() )
        return;

    namespace GDS = World::GroundDecalSmoke;

    // One soft disc as a camera-facing node, like the procedural energy symbols.
    const auto queueDisc = [&](const vec3d &worldPos, float size, float red, float green,
                               float blue, float alpha, bool flame=false)
    {
        alpha = GroundDecalSmokeClampUnit(alpha);
        if ( size <= 0.01f || alpha <= 0.0f )
            return;

        const vec3d viewPos = view->CalcSclRot.Transform(worldPos - view->CalcPos);
        mat4x4 transform(mat3x3::Scale(flame?vec3d(size*0.45f,size*1.6f,1.0f):vec3d(size,size,1.0f)));
        transform.m03 = viewPos.x;
        transform.m13 = viewPos.y;
        transform.m23 = viewPos.z;

        GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
        render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
        render.Mesh = &_groundDecalSmokeMesh;
        render.Flags = _groundDecalSmokeMesh.Mat.Flags | arg->flags;
        if(flame)render.Flags=(render.Flags&~GFX::RFLAGS_ALPHABLEND)|GFX::RFLAGS_LUMTRACY;
        render.Color = _groundDecalSmokeMesh.Mat.Color;
        render.ColorMul = GFX::TGLColor(GroundDecalSmokeClampUnit(red),
                                        GroundDecalSmokeClampUnit(green),
                                        GroundDecalSmokeClampUnit(blue), alpha);
        render.Colorize = true;
        render.TForm = transform;
        render.Distance = viewPos.length();
        render.TimeStamp = arg->globTime;
        render.FrameTime = arg->frameTime;
        render.FogStart = (float)(_normalVizLimit - _normalFadeLength);
        render.FogLength = (float)_normalFadeLength;

        arg->adeCount += _groundDecalSmokeMesh.Indixes.size() / 3;
        GFX::Engine.QueueRenderMesh(&render);
    };

    for (auto it = _groundDecalSmoke.begin(); it != _groundDecalSmoke.end(); )
    {
        const int32_t age = _timeStamp - it->startTime;
        if ( age < 0 || age >= it->duration )
        {
            it = _groundDecalSmoke.erase(it);
            continue;
        }

        // The plume rises well above the decal, so the visibility margin is
        // wider than the decal radius.
        const vec3d centerView = view->CalcSclRot.Transform(it->pos - view->CalcPos);
        if ( centerView.length() <= (float)_normalVizLimit + it->radius * 4.0f )
        {
            const World::TGroundDecalSmoke &params = it->params;
            const float gain = params.intensity * 0.01f;
            GDS::Puff puff;

            for (int slot = 0; slot < it->count; ++slot)
            {
                if ( !GDS::Evaluate(*it, slot, age, &puff) )
                    continue;

                if(params.flame)
                {
                    const float flicker=0.85f+0.15f*std::sin(age*0.045f+slot*2.37f);
                    const float alpha=params.tint.a*puff.alpha*gain*flicker;
                    queueDisc(puff.pos,puff.size*flicker,params.tint.r,
                              params.tint.g*(1.0f-0.55f*puff.phase),params.tint.b*(1.0f-puff.phase),alpha,true);
                    queueDisc(puff.pos+vec3d(0,puff.size*0.3f,0),puff.size*0.45f,
                              1.0f,0.88f,0.45f,alpha*0.8f,true);
                    continue;
                }
                const float shade = std::min(1.0f, (0.85f + 0.5f * puff.phase) * puff.shade);
                queueDisc(puff.pos, puff.size,
                          params.tint.r * shade, params.tint.g * shade, params.tint.b * shade,
                          params.tint.a * puff.alpha * gain);
            }
        }

        ++it;
    }
}

void NC_STACK_ypaworld::ClearGroundDecalSmoke()
{
    _groundDecalSmoke.clear();
    _groundDecalSmokeMesh = GFX::TMesh();
}
