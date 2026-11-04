// Smoke rising from ground decals: emitter list and billboard rendering.
#include <algorithm>
#include <cmath>

#include "grounddecal_limit.h"
#include "grounddecal_smoke.h"
#include "../system/inivals.h"
#include "game/yw.h"

namespace
{

// Cap active decal effects at the global decal limit; 0 disables them.
static int GroundDecalSmokeLimit()
{
    const int value = System::IniConf::GfxGroundDecalEffectLimit.Get<int32_t>();
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

}

bool NC_STACK_ypaworld::SpawnGroundDecalSmoke(const World::TGroundDecalSmoke &smoke,
                                              const vec3d &pos, float radius, uint32_t seed)
{
    namespace GDS = World::GroundDecalSmoke;

    const int limit = GroundDecalSmokeLimit();
    if ( !smoke.enabled || limit <= 0 || smoke.chance <= 0 ||
         smoke.mode == World::TGroundDecalSmoke::MODE_NONE ||
         !GroundDecalSmokeFinite(pos) || !std::isfinite(radius) || radius <= 0.0f )
        return false;

    // The effect always needs an authored model: rise draws only the 3DS,
    // ground_pulse accepts 3DS, BASE or VP. Nothing is drawn without one.
    const bool rise = smoke.mode == World::TGroundDecalSmoke::MODE_RISE;
    const bool hasModel = rise
        ? !smoke.mesh3ds.empty()
        : (!smoke.mesh3ds.empty() || !smoke.base_model.empty() || smoke.pulse_vp > 0);
    if ( !hasModel )
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
    // Every decal rolls its own puff count inside effect_count = min~max (rise only).
    const int span = smoke.count_max - smoke.count_min + 1;
    const int rolled = std::min(smoke.count_min + (int)(GDS::HashUnit(GDS::Hash(seed, 0x434e5452u, 0u)) * (float)span),
                                smoke.count_max);
    emitter.count = GDS::ScaledCount(rolled, radius, GDS::kMaxPuffs);
    emitter.duration = (int32_t)GDS::RangeFrom(GDS::Hash(seed, 0x53445552u, 0u),
                                               (float)smoke.active_time_min,
                                               (float)smoke.active_time_max);
    if ( emitter.duration <= 0 || (rise && emitter.count <= 0) )
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

    namespace GDS = World::GroundDecalSmoke;

    // Render a rise mesh facing the camera, with the puff's tint and opacity.
    const auto queueDisc = [&](GFX::TMesh *mesh, float meshUnit, const vec3d &worldPos,
                               float size, float red, float green, float blue, float alpha)
    {
        alpha = GroundDecalSmokeClampUnit(alpha);
        if ( size <= 0.01f || alpha <= 0.0f )
            return;

        const vec3d viewPos = view->CalcSclRot.Transform(worldPos - view->CalcPos);
        mat4x4 transform(mat3x3::Scale(vec3d(size / meshUnit, size / meshUnit, 1.0f)));
        transform.m03 = viewPos.x;
        transform.m13 = viewPos.y;
        transform.m23 = viewPos.z;

        GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
        render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
        render.Mesh = mesh;
        render.Tex = mesh->Mat.Tex;
        render.Flags = mesh->Mat.Flags | arg->flags | GFX::RFLAGS_DISABLE_ZWRITE | GFX::RFLAGS_ALPHABLEND;
        render.Color = mesh->Mat.Color;
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

        arg->adeCount += mesh->Indixes.size() / 3;
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

        // Rising puffs climb well above the decal, so the visibility margin is
        // wider than the decal radius.
        const vec3d centerView = view->CalcSclRot.Transform(it->pos - view->CalcPos);
        if ( centerView.length() <= (float)_normalVizLimit + it->radius * 4.0f )
        {
            const World::TGroundDecalSmoke &params = it->params;
            const float gain = params.intensity * 0.01f;
            if ( params.mode == World::TGroundDecalSmoke::MODE_GROUND_PULSE )
            {
                // Each pulse starts when the previous interval ends. The length is
                // kept in the emitter because min~max rolls it for every pulse.
                int cycle = std::max(it->meshCycle, 0);
                if ( it->meshCycleLen <= 0 )
                    it->meshCycleLen = GDS::PulseInterval(params, it->seed, cycle);
                while ( age >= it->meshCycleStart + it->meshCycleLen )
                {
                    it->meshCycleStart += it->meshCycleLen;
                    it->meshCycleLen = GDS::PulseInterval(params, it->seed, ++cycle);
                }
                const int interval = it->meshCycleLen;
                if ( cycle != it->meshCycle )
                {
                    it->meshCycle = cycle;
                    const uint32_t hash = GDS::Hash(it->seed, cycle, 0x454c4543u);
                    const float angle = GDS::kPi * 2.0f * GDS::HashUnit(hash);
                    const float spread = params.spread * 0.01f * it->radius;
                    const vec3d pos = it->pos + vec3d(std::cos(angle) * spread,
                                                     -GDS::kGroundLift,
                                                     std::sin(angle) * spread);
                    const float size = params.mesh_scale * it->radius / 50.0f;
                    const int life = std::min(params.pulse_time, interval);
                    SpawnTransientVisual(params.pulse_vp, params.mesh3ds, params.base_model,
                                         pos, mat3x3::RotateZ(angle), life,
                                         std::max(0.01f, size), params.tint,
                                         vec3d(1.0, 1.0, 1.0), vec3d(),
                                         TTransientVPParticleControls(), 10, life / 2);
                }
                ++it;
                continue;
            }
            NC_STACK_base *model = GetSharedExternalMesh(params.mesh3ds);
            if ( !model || model->Meshes.empty() )
            {
                ++it;
                continue;
            }
            GFX::TMesh *plumeMesh = &model->Meshes.front();
            const float meshUnit = 1.0f / params.mesh_scale;
            GDS::Puff puff;

            for (int slot = 0; slot < it->count; ++slot)
            {
                if ( !GDS::Evaluate(*it, slot, age, &puff) )
                    continue;

                const float shade = std::min(1.0f, (0.85f + 0.5f * puff.phase) * puff.shade);
                queueDisc(plumeMesh, meshUnit, puff.pos, puff.size,
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
}
