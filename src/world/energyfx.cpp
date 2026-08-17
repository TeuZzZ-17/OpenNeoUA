#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "energyfx.h"
#include "spin.h"
#include "tools.h"
#include "../log.h"
#include "../system/inivals.h"
#include "../yw.h"

namespace World
{
namespace EnergyFX
{

static Config s_regen;
static Config s_drain;

static bool ParseLongStrict(const std::string &value, long *out)
{
    if ( !out || value.empty() )
        return false;

    errno = 0;
    char *end = NULL;
    const char *begin = value.c_str();
    long parsed = std::strtol(begin, &end, 10);

    if ( end == begin || errno == ERANGE )
        return false;

    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
        end++;

    if ( *end != '\0' )
        return false;

    *out = parsed;
    return true;
}

static bool ParseFloatStrict(const std::string &value, float *out)
{
    if ( !out || value.empty() )
        return false;

    errno = 0;
    char *end = NULL;
    const char *begin = value.c_str();
    float parsed = std::strtof(begin, &end);

    if ( end == begin || errno == ERANGE || !std::isfinite(parsed) )
        return false;

    while ( *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' )
        end++;

    if ( *end != '\0' )
        return false;

    *out = parsed;
    return true;
}

static int ReadInt(Common::Ini::Key &key, int fallback, int minValue, int maxValue)
{
    long parsed = fallback;
    const std::string value = key.Get<std::string>();

    if ( !ParseLongStrict(value, &parsed) )
    {
        if ( key.WasSet )
            ypa_log_out("Warning: invalid %s '%s', using %d\n", key.Name.c_str(), value.c_str(), fallback);
        parsed = fallback;
    }

    if ( parsed < minValue )
        parsed = minValue;
    if ( parsed > maxValue )
        parsed = maxValue;
    return (int)parsed;
}

static float ReadFloat(Common::Ini::Key &key, float fallback)
{
    float parsed = fallback;
    const std::string value = key.Get<std::string>();

    if ( !ParseFloatStrict(value, &parsed) )
    {
        if ( key.WasSet )
            ypa_log_out("Warning: invalid %s '%s', using %.3f\n", key.Name.c_str(), value.c_str(), fallback);
        parsed = fallback;
    }

    return parsed;
}


static bool ParseTintComponent(const char *&pos, int *out)
{
    errno = 0;
    char *end = NULL;
    long value = std::strtol(pos, &end, 10);

    if ( end == pos || errno == ERANGE )
        return false;

    *out = (int)value;
    pos = end;
    return true;
}

static TVisualTint MakeTint(int r, int g, int b, int a)
{
    auto clamp255 = [](int v) -> float
    {
        if ( v < 0 ) v = 0;
        if ( v > 255 ) v = 255;
        return (float)v / 255.0f;
    };

    TVisualTint tint;
    tint.r = clamp255(r);
    tint.g = clamp255(g);
    tint.b = clamp255(b);
    tint.a = clamp255(a);
    return tint;
}

static TVisualTint ReadTint(Common::Ini::Key &key, const TVisualTint &fallback)
{
    if ( !key.WasSet )
        return fallback;

    const std::string value = key.Get<std::string>();
    const char *pos = value.c_str();
    int component[4] = {255, 255, 255, 255};
    bool valid = true;

    for (int i = 0; i < 3; i++)
    {
        if ( !ParseTintComponent(pos, &component[i]) )
        {
            valid = false;
            break;
        }

        if ( i < 2 )
        {
            if ( *pos != '_' )
            {
                valid = false;
                break;
            }
            pos++;
        }
    }

    if ( valid && *pos == '_' )
    {
        pos++;
        valid = ParseTintComponent(pos, &component[3]);
    }

    if ( valid )
        valid = *pos == '\0';

    if ( !valid )
    {
        ypa_log_out("Warning: invalid %s '%s', using profile default tint\n",
                    key.Name.c_str(), value.c_str());
        return fallback;
    }

    return MakeTint(component[0], component[1], component[2], component[3]);
}

static Config BuildConfig(Common::Ini::Key &vp,
                          Common::Ini::Key &scale,
                          Common::Ini::Key &spinX,
                          Common::Ini::Key &spinY,
                          Common::Ini::Key &spinZ,
                          Common::Ini::Key &tint,
                          Common::Ini::Key &duration,
                          Common::Ini::Key &intervalMin,
                          Common::Ini::Key &intervalMax,
                          Common::Ini::Key &countMin,
                          Common::Ini::Key &countMax,
                          Common::Ini::Key &randomOffsetPercent,
                          Common::Ini::Key &size,
                          Common::Ini::Key &thickness,
                          Common::Ini::Key &riseSpeed,
                          Common::Ini::Key &fadeIn,
                          Common::Ini::Key &fadeOut,
                          bool regenProfile)
{
    Config config;
    config.vp = (int16_t)ReadInt(vp, 0, 0, std::numeric_limits<int16_t>::max());

    config.vp_scale = ReadFloat(scale, 1.0f);
    if ( config.vp_scale <= 0.0f )
        config.vp_scale = 1.0f;

    config.vp_spin.x = Spin::ClampStrength(ReadFloat(spinX, 0.0f));
    config.vp_spin.y = Spin::ClampStrength(ReadFloat(spinY, 0.0f));
    config.vp_spin.z = Spin::ClampStrength(ReadFloat(spinZ, 0.0f));

    const TVisualTint whiteTint = MakeTint(255, 255, 255, 255);
    const TVisualTint proceduralTint = regenProfile ?
        MakeTint(80, 255, 120, 230) : MakeTint(255, 80, 80, 230);
    config.vp_tint = ReadTint(tint,
                              config.IsProcedural() ? proceduralTint : whiteTint);

    config.duration = ReadInt(duration, 1000, 0, std::numeric_limits<int>::max() / 4);
    if ( config.duration <= 0 )
        config.duration = 1000;

    config.interval_min = ReadInt(intervalMin, 0, 0, std::numeric_limits<int>::max() / 4);
    config.interval_max = ReadInt(intervalMax, 0, 0, std::numeric_limits<int>::max() / 4);
    config.count_min = ReadInt(countMin, 0, 0, 32);
    config.count_max = ReadInt(countMax, 0, 0, 32);

    config.random_offset_percent = ReadFloat(randomOffsetPercent, 25.0f);
    if ( config.random_offset_percent < 0.0f )
        config.random_offset_percent = 0.0f;
    else if ( config.random_offset_percent > 100.0f )
        config.random_offset_percent = 100.0f;

    config.size = ReadFloat(size, 30.0f);
    if ( config.size <= 0.0f )
        config.size = 30.0f;
    else if ( config.size > 500.0f )
        config.size = 500.0f;

    config.thickness = ReadFloat(thickness, 5.0f);
    if ( config.thickness <= 0.0f )
        config.thickness = 5.0f;
    if ( config.thickness > config.size )
        config.thickness = config.size;

    config.rise_speed = ReadFloat(riseSpeed, 100.0f);
    if ( config.rise_speed < 0.0f )
        config.rise_speed = 0.0f;
    else if ( config.rise_speed > 5000.0f )
        config.rise_speed = 5000.0f;

    config.fade_in = ReadInt(fadeIn, 150, 0, config.duration);
    config.fade_out = ReadInt(fadeOut, 300, 0, config.duration);

    if ( config.interval_max < config.interval_min )
        std::swap(config.interval_min, config.interval_max);
    if ( config.count_max < config.count_min )
        std::swap(config.count_min, config.count_max);

    return config;
}

void Init()
{
    s_regen = BuildConfig(System::IniConf::GfxRegenDecorationFXVP,
                          System::IniConf::GfxRegenDecorationFXVPScale,
                          System::IniConf::GfxRegenDecorationFXVPSpinX,
                          System::IniConf::GfxRegenDecorationFXVPSpinY,
                          System::IniConf::GfxRegenDecorationFXVPSpinZ,
                          System::IniConf::GfxRegenDecorationFXVPTint,
                          System::IniConf::GfxRegenDecorationFXDuration,
                          System::IniConf::GfxRegenDecorationFXIntervalMin,
                          System::IniConf::GfxRegenDecorationFXIntervalMax,
                          System::IniConf::GfxRegenDecorationFXCountMin,
                          System::IniConf::GfxRegenDecorationFXCountMax,
                          System::IniConf::GfxRegenDecorationFXRandomOffsetPercent,
                          System::IniConf::GfxRegenDecorationFXSize,
                          System::IniConf::GfxRegenDecorationFXThickness,
                          System::IniConf::GfxRegenDecorationFXRiseSpeed,
                          System::IniConf::GfxRegenDecorationFXFadeIn,
                          System::IniConf::GfxRegenDecorationFXFadeOut,
                          true);

    s_drain = BuildConfig(System::IniConf::GfxDrainDecorationFXVP,
                          System::IniConf::GfxDrainDecorationFXVPScale,
                          System::IniConf::GfxDrainDecorationFXVPSpinX,
                          System::IniConf::GfxDrainDecorationFXVPSpinY,
                          System::IniConf::GfxDrainDecorationFXVPSpinZ,
                          System::IniConf::GfxDrainDecorationFXVPTint,
                          System::IniConf::GfxDrainDecorationFXDuration,
                          System::IniConf::GfxDrainDecorationFXIntervalMin,
                          System::IniConf::GfxDrainDecorationFXIntervalMax,
                          System::IniConf::GfxDrainDecorationFXCountMin,
                          System::IniConf::GfxDrainDecorationFXCountMax,
                          System::IniConf::GfxDrainDecorationFXRandomOffsetPercent,
                          System::IniConf::GfxDrainDecorationFXSize,
                          System::IniConf::GfxDrainDecorationFXThickness,
                          System::IniConf::GfxDrainDecorationFXRiseSpeed,
                          System::IniConf::GfxDrainDecorationFXFadeIn,
                          System::IniConf::GfxDrainDecorationFXFadeOut,
                          false);
}

const Config &Regen()
{
    return s_regen;
}

const Config &Drain()
{
    return s_drain;
}

}
}

namespace
{

const size_t PROCEDURAL_ENERGY_FX_LIMIT = 512;

static bool ProceduralEnergyFXFinite(const vec3d &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

static bool ProceduralEnergyFXFinite(const World::TVisualTint &tint)
{
    return std::isfinite(tint.r) && std::isfinite(tint.g) &&
           std::isfinite(tint.b) && std::isfinite(tint.a);
}

static void ProceduralEnergyFXBuildQuadMesh(GFX::TMesh *mesh)
{
    if ( !mesh || !mesh->Vertexes.empty() )
        return;

    GFX::TVertex v0(vec3f(-0.5f, -0.5f, 0.0f));
    GFX::TVertex v1(vec3f( 0.5f, -0.5f, 0.0f));
    GFX::TVertex v2(vec3f( 0.5f,  0.5f, 0.0f));
    GFX::TVertex v3(vec3f(-0.5f,  0.5f, 0.0f));
    v0.Color = v1.Color = v2.Color = v3.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->Vertexes.push_back(v0);
    mesh->Vertexes.push_back(v1);
    mesh->Vertexes.push_back(v2);
    mesh->Vertexes.push_back(v3);

    const GFX::IndexType indices[] = {
        0, 2, 1, 0, 3, 2,
        0, 1, 2, 0, 2, 3
    };
    mesh->Indixes.insert(mesh->Indixes.end(), indices, indices + 12);

    mesh->Mat = GFX::TRenderParams(GFX::RFLAGS_FOG |
                                   GFX::RFLAGS_DISABLE_ZWRITE |
                                   GFX::RFLAGS_ALPHABLEND);
    mesh->Mat.Color = GFX::TGLColor(1.0f, 1.0f, 1.0f, 1.0f);
    mesh->RecalcBoundBox();
    GFX::Engine.MeshMakeVBO(mesh);
}

}

bool NC_STACK_ypaworld::SpawnProceduralEnergyFX(const vec3d &pos,
                                                 bool plusSymbol,
                                                 int32_t duration,
                                                 float size,
                                                 float thickness,
                                                 float riseSpeed,
                                                 int32_t fadeIn,
                                                 int32_t fadeOut,
                                                 const World::TVisualTint &tint)
{
    if ( !ProceduralEnergyFXFinite(pos) || !ProceduralEnergyFXFinite(tint) ||
         duration <= 0 || !std::isfinite(size) || size <= 0.0f ||
         !std::isfinite(thickness) || thickness <= 0.0f ||
         !std::isfinite(riseSpeed) || riseSpeed < 0.0f || tint.a <= 0.0f )
        return false;

    _proceduralEnergyFX.erase(
        std::remove_if(_proceduralEnergyFX.begin(), _proceduralEnergyFX.end(),
                       [this](const TProceduralEnergyFX &fx)
                       {
                           const int32_t age = _timeStamp - fx.startTime;
                           return age < 0 || age >= fx.duration;
                       }),
        _proceduralEnergyFX.end());

    if ( _proceduralEnergyFX.size() >= PROCEDURAL_ENERGY_FX_LIMIT )
        _proceduralEnergyFX.erase(_proceduralEnergyFX.begin());

    TProceduralEnergyFX fx;
    fx.pos = pos;
    fx.plusSymbol = plusSymbol;
    fx.startTime = _timeStamp;
    fx.duration = duration;
    fx.size = size;
    fx.thickness = std::min(thickness, size);
    fx.riseSpeed = riseSpeed;
    fx.fadeIn = std::max(0, std::min(fadeIn, duration));
    fx.fadeOut = std::max(0, std::min(fadeOut, duration));
    fx.tint = tint;
    fx.tint.Clamp();
    _proceduralEnergyFX.push_back(fx);
    return true;
}

void NC_STACK_ypaworld::RenderProceduralEnergyFX(baseRender_msg *arg)
{
    if ( !arg || _proceduralEnergyFX.empty() )
        return;

    TF::TForm3D *view = TF::Engine.GetViewPoint();
    if ( !view )
        return;

    ProceduralEnergyFXBuildQuadMesh(&_proceduralEnergyFXQuadMesh);
    if ( _proceduralEnergyFXQuadMesh.Vertexes.empty() )
        return;

    for (auto it = _proceduralEnergyFX.begin(); it != _proceduralEnergyFX.end(); )
    {
        const int32_t age = _timeStamp - it->startTime;
        if ( age < 0 || age >= it->duration )
        {
            it = _proceduralEnergyFX.erase(it);
            continue;
        }

        vec3d worldPos = it->pos;
        worldPos.y -= it->riseSpeed * ((float)age * 0.001f);
        const vec3d viewPos = view->CalcSclRot.Transform(worldPos - view->CalcPos);
        const float distance = viewPos.length();

        if ( distance <= (float)_normalVizLimit + it->size )
        {
            const float fade = World::ComputeVPFadeEnvelope((double)age,
                                                             (double)it->duration,
                                                             (double)it->fadeIn,
                                                             (double)it->fadeOut);

            auto renderBar = [&](float width, float height, float alpha)
            {
                if ( width <= 0.01f || height <= 0.01f || alpha <= 0.0f )
                    return;

                mat4x4 transform(mat3x3::Scale(vec3d(width, height, 1.0f)));
                transform.m03 = viewPos.x;
                transform.m13 = viewPos.y;
                transform.m23 = viewPos.z;

                GFX::TRenderNode &render = GFX::Engine.AllocRenderNode();
                render = GFX::TRenderNode(GFX::TRenderNode::TYPE_MESH);
                render.Mesh = &_proceduralEnergyFXQuadMesh;
                render.Flags = _proceduralEnergyFXQuadMesh.Mat.Flags | arg->flags;
                render.Color = _proceduralEnergyFXQuadMesh.Mat.Color;
                render.ColorMul = GFX::TGLColor(it->tint.r, it->tint.g, it->tint.b,
                                                it->tint.a * fade * alpha);
                render.TForm = transform;
                render.Distance = distance;
                render.TimeStamp = arg->globTime;
                render.FrameTime = arg->frameTime;
                render.FogStart = (float)(_normalVizLimit - _normalFadeLength);
                render.FogLength = (float)_normalFadeLength;

                arg->adeCount += _proceduralEnergyFXQuadMesh.Indixes.size() / 3;
                GFX::Engine.QueueRenderMesh(&render);
            };

            // Same visual idea as the tracer material: a soft translucent body
            // with a narrower bright core. The quad lives in view space, so the
            // symbol always faces the camera without requiring a texture/VP.
            renderBar(it->size, it->thickness, 0.28f);
            renderBar(it->size * 0.72f, it->thickness * 0.45f, 1.0f);

            if ( it->plusSymbol )
            {
                renderBar(it->thickness, it->size, 0.28f);
                renderBar(it->thickness * 0.45f, it->size * 0.72f, 1.0f);
            }
        }

        ++it;
    }
}

void NC_STACK_ypaworld::ClearProceduralEnergyFX()
{
    _proceduralEnergyFX.clear();
    _proceduralEnergyFXQuadMesh = GFX::TMesh();
}
