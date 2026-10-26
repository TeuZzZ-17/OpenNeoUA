// Transient visuals and effects: spawn/remove transient VPs, chain FX, decoration FX and atmospheric FX.
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "common/env.h"
#include "common/includes.h"
#include "game/yw_internal.h"
#include "game/yw.h"
#include "game/yw_net.h"
#include "platform/windp.h"
#include "nucleus/loaders.h"
#include "game/yparobo.h"
#include "gui/font.h"
#include "gui/uacommon.h"
#include "system/inivals.h"
#include "system/gametime.h"
#include "system/system.h"
#include "world/spin.h"
#include "common/crashdiag.h"
#include "game/yw_game_impl.h"

void yw_ConfigureTransientVPFade(NC_STACK_ypaworld::TTransientVP &effect,
                                        int fadeIn, int fadeOut,
                                        double duration, double elapsed)
{
    effect.fadeIn = fadeIn > 0 ? fadeIn : 0;
    effect.fadeOut = fadeOut > 0 ? fadeOut : 0;
    effect.fadeDuration = std::isfinite(duration) && duration > 0.0 ? duration : 0.0;
    effect.fadeElapsed = std::isfinite(elapsed) && elapsed >= 0.0 ? elapsed : -1.0;
}

NC_STACK_base *NC_STACK_ypaworld::ResolveVisualModel(int32_t modelId,
                                                     const std::string &external3dsPath,
                                                     const std::string &externalBasePath)
{
    if ( !external3dsPath.empty() )
    {
        if ( NC_STACK_base *external = GetSharedExternalMesh(external3dsPath) )
            return external;
    }

    if ( !externalBasePath.empty() )
    {
        if ( NC_STACK_base *external = GetSharedExternalBase(externalBasePath) )
            return external;
    }

    // Preserve the legacy actor lookup exactly when this feature is unused.
    // BASE-only authored visuals need a safe null result when the BASE fails and
    // no numeric fallback exists, but pre-existing VP/3DS data keeps the old at()
    // semantics rather than silently changing unrelated invalid-data behaviour.
    if ( !externalBasePath.empty() &&
         (modelId < 0 || modelId >= (int32_t)_vhclModels.size()) )
        return nullptr;
    return _vhclModels.at(modelId);
}

int32_t NC_STACK_ypaworld::SpawnTransientVisualBase(NC_STACK_base *base, const vec3d &pos, const mat3x3 &rot, int32_t lifeTime, float scale, const World::TVisualTint &tint, const vec3d &axisScale, const vec3d &spin, const TTransientVPParticleControls &particleControls, int32_t fadeIn, int32_t fadeOut)
{
    if ( !base || lifeTime < 0 )
        return 0;

    _transientVPs.emplace_back(base, pos, rot, lifeTime);
    TTransientVP &fx = _transientVPs.back();
    fx.id = _nextTransientVPId++;
    fx.scale = scale > 0.0f ? scale : 1.0f;
    fx.axisScale = vec3d(axisScale.x > 0.0f ? axisScale.x : 1.0f,
                         axisScale.y > 0.0f ? axisScale.y : 1.0f,
                         axisScale.z > 0.0f ? axisScale.z : 1.0f);
    fx.spin = spin;
    fx.tint = tint;
    fx.particleControls = particleControls;
    yw_ConfigureTransientVPFade(fx, fadeIn, fadeOut, (double)lifeTime);
    return fx.id;
}

int32_t NC_STACK_ypaworld::SpawnTransientVisual(int32_t modelId, const std::string &external3dsPath, const vec3d &pos, const mat3x3 &rot, int32_t lifeTime, float scale, const World::TVisualTint &tint, const vec3d &axisScale, const vec3d &spin, const TTransientVPParticleControls &particleControls, int32_t fadeIn, int32_t fadeOut)
{
    return SpawnTransientVisual(modelId, external3dsPath, std::string(), pos, rot,
                                lifeTime, scale, tint, axisScale, spin,
                                particleControls, fadeIn, fadeOut);
}

int32_t NC_STACK_ypaworld::SpawnTransientVisual(int32_t modelId, const std::string &external3dsPath, const std::string &externalBasePath, const vec3d &pos, const mat3x3 &rot, int32_t lifeTime, float scale, const World::TVisualTint &tint, const vec3d &axisScale, const vec3d &spin, const TTransientVPParticleControls &particleControls, int32_t fadeIn, int32_t fadeOut)
{
    if ( !external3dsPath.empty() )
    {
        if ( NC_STACK_base *external = GetSharedExternalMesh(external3dsPath) )
            return SpawnTransientVisualBase(external, pos, rot, lifeTime, scale, tint, axisScale, spin, particleControls, fadeIn, fadeOut);
    }

    if ( !externalBasePath.empty() )
    {
        if ( NC_STACK_base *external = GetSharedExternalBase(externalBasePath) )
            return SpawnTransientVisualBase(external, pos, rot, lifeTime, scale, tint, axisScale, spin, particleControls, fadeIn, fadeOut);
    }

    return SpawnTransientVP(modelId, pos, rot, lifeTime, scale, tint, axisScale, spin, particleControls, fadeIn, fadeOut);
}

int32_t NC_STACK_ypaworld::SpawnTransientVP(int32_t modelId, const vec3d &pos, const mat3x3 &rot, int32_t lifeTime, float scale, const World::TVisualTint &tint, const vec3d &axisScale, const vec3d &spin, const TTransientVPParticleControls &particleControls, int32_t fadeIn, int32_t fadeOut)
{
    if ( modelId <= 0 || modelId >= (int32_t)_vhclModels.size() )
        return 0;

    return SpawnTransientVisualBase(_vhclModels.at(modelId), pos, rot, lifeTime, scale, tint, axisScale, spin, particleControls, fadeIn, fadeOut);
}

void NC_STACK_ypaworld::SpawnChainFX(const World::TChainFXConfig &config, const vec3d &pos, const mat3x3 &rot)
{
    if ( config.duration <= 0 || config.visuals.empty() )
        return;

    std::vector<NC_STACK_base *> bases;
    bases.reserve(config.visuals.size());
    std::vector<World::TVisualTint> tints;
    tints.reserve(config.visuals.size());

    for (const World::TChainFXVisual &visual : config.visuals)
    {
        NC_STACK_base *base = nullptr;
        if ( !visual.mesh3ds.empty() )
            base = GetSharedExternalMesh(visual.mesh3ds);
        if ( !base && !visual.basePath.empty() )
            base = GetSharedExternalBase(visual.basePath);
        if ( !base && visual.vp > 0 && visual.vp < (int32_t)_vhclModels.size() )
            base = _vhclModels.at(visual.vp);

        if ( base )
        {
            bases.push_back(base);
            tints.push_back(visual.has_tint ? visual.tint : World::TVisualTint());
        }
    }

    if ( bases.empty() )
        return;

    // Offset min_max is rolled fresh for every activation of the Chain FX.
    vec3d spawnPos = pos + rot.Transform(World::RandomVec3RangeInclusive(config.offset_min,
                                                                        config.offset_max));
    _transientVPs.emplace_back(bases.front(), spawnPos,
                              World::RandomFXRotation(rot, config.random_rotation),
                              config.duration);

    TTransientVP &fx = _transientVPs.back();
    fx.chainFX = true;
    fx.spin = config.spin;
    fx.chainBases = std::move(bases);
    fx.chainTints = std::move(tints);
    fx.chainIndex = 0;
    if ( !fx.chainTints.empty() )
        fx.tint = fx.chainTints.front();
    fx.startScale = std::isfinite(config.start_size) && config.start_size >= 0.0f
                  ? config.start_size : 0.0f;
    fx.midScale = std::isfinite(config.mid_size) && config.mid_size >= 0.0f
                ? config.mid_size : 0.0f;
    fx.endScale = std::isfinite(config.end_size) && config.end_size >= 0.0f
                ? config.end_size : 0.0f;
    fx.hasMidScale = config.has_mid_size;
    yw_ConfigureTransientVPFade(fx, config.fade_in, config.fade_out,
                                (double)config.duration);
}

int yw_RandomInRange(int minValue, int maxValue)
{
    if ( maxValue < minValue )
        std::swap(minValue, maxValue);

    if ( minValue == maxValue )
        return minValue;

    double randomPart = (double)rand() / ((double)RAND_MAX + 1.0);
    int64_t range = (int64_t)maxValue - minValue;
    return minValue + (int)((range + 1) * randomPart);
}

bool NC_STACK_ypaworld::UpdateRandomFXTimer(int intervalMin, int intervalMax, int32_t &nextTime)
{
    if ( intervalMin <= 0 || intervalMax <= 0 )
    {
        nextTime = 0;
        return false;
    }

    if ( intervalMax < intervalMin )
        std::swap(intervalMin, intervalMax);

    if ( nextTime <= 0 )
    {
        nextTime = _timeStamp + yw_RandomInRange(intervalMin, intervalMax);
        return false;
    }

    if ( _timeStamp < nextTime )
        return false;

    nextTime = _timeStamp + yw_RandomInRange(intervalMin, intervalMax);
    return true;
}

int32_t NC_STACK_ypaworld::SpawnRandomizedTransientVP(int32_t modelId, const vec3d &ownerPos, float randomPos, const World::TVisualTint &tint, int32_t lifeTime, float scale, const vec3d &offset, const vec3d &axisScale, const vec3d &spin, const TTransientVPParticleControls &particleControls, int32_t fadeIn, int32_t fadeOut, const std::string &external3dsPath, const std::string &externalBasePath)
{
    if ( modelId <= 0 && external3dsPath.empty() && externalBasePath.empty() )
        return 0;

    vec3d pos = ownerPos + offset;
    if ( randomPos > 0.0 )
    {
        pos.x += (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * randomPos;
        pos.y += (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * randomPos;
        pos.z += (((float)rand() / (float)RAND_MAX) * 2.0 - 1.0) * randomPos;
    }

    return SpawnTransientVisual(modelId, external3dsPath, externalBasePath,
                                pos, mat3x3::Ident(), lifeTime, scale, tint,
                                axisScale, spin, particleControls, fadeIn, fadeOut);
}

void NC_STACK_ypaworld::UpdateDecorationFX(const World::TDecorationFXConfig &config, int32_t &nextTime, const vec3d &ownerPos, int32_t *persistentId)
{
    const World::TVisualTint tint = config.tint;

    if ( config.mode == World::DECORATION_FX_PERSISTENT )
    {
        nextTime = 0;

        if ( !persistentId || (config.vp <= 0 && config.mesh3ds.empty() && config.basePath.empty()) )
            return;

        if ( !HasTransientVP(*persistentId) )
            *persistentId = SpawnTransientVisual(config.vp, config.mesh3ds, config.basePath,
                                                 ownerPos + config.offset, mat3x3::Ident(), 0, 1.0, tint,
                                                 config.scale, config.spin,
                                                 TTransientVPParticleControls(config),
                                                 config.fade_in, config.fade_out);

        return;
    }

    if ( persistentId && *persistentId > 0 )
    {
        RemoveTransientVP(*persistentId, config.fade_out, config.vp_trail_fade_out);
        *persistentId = 0;
    }

    int countMin = std::max(0, std::min(config.count_min, 32));
    int countMax = std::max(0, std::min(config.count_max, 32));

    if ( (config.vp <= 0 && config.mesh3ds.empty() && config.basePath.empty()) ||
         countMin <= 0 || countMax <= 0 )
    {
        nextTime = 0;
        return;
    }

    if ( countMax < countMin )
        std::swap(countMin, countMax);

    if ( !UpdateRandomFXTimer(config.interval_min, config.interval_max, nextTime) )
        return;

    int spawnCount = yw_RandomInRange(countMin, countMax);
    for (int i = 0; i < spawnCount; i++)
        SpawnRandomizedTransientVP(config.vp, ownerPos, config.random_pos,
                                   tint,
                                   config.duration > 0 ? config.duration : 1000,
                                   1.0,
                                   config.offset,
                                   config.scale,
                                   config.spin,
                                   TTransientVPParticleControls(config),
                                   config.fade_in,
                                   config.fade_out,
                                   config.mesh3ds,
                                   config.basePath);
}

float yw_RandomFloatSigned(float magnitude)
{
    if ( magnitude <= 0.0f )
        return 0.0f;

    return (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * magnitude;
}

bool yw_IsAtmosphericFXInsideVolume(const NC_STACK_ypaworld::TTransientVP &fx,
                                           const vec3d &viewerPos,
                                           const World::TAtmosphericFXProfile &profile)
{
    vec3d center = viewerPos + profile.spawn_offset;
    const double marginX = std::max(200.0, (double)profile.spawn_radius.x * 2.0 + 200.0);
    const double marginY = std::max(200.0, (double)profile.spawn_radius.y * 2.0 + 200.0);
    const double marginZ = std::max(200.0, (double)profile.spawn_radius.z * 2.0 + 200.0);
    return fabs(fx.pos.x - center.x) <= marginX &&
           fabs(fx.pos.y - center.y) <= marginY &&
           fabs(fx.pos.z - center.z) <= marginZ;
}

void NC_STACK_ypaworld::UpdateAtmosphericFX()
{
    const World::TAtmosphericFXProfile &profile = _atmosphericFXProfile;
    if ( !profile.valid )
        return;

    int32_t activeCount = 0;

    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); )
    {
        if ( !it->atmosphericFX || it->atmosphericSuperItemIndex >= 0 )
        {
            ++it;
            continue;
        }

        if ( !yw_IsAtmosphericFXInsideVolume(*it, _viewerPosition, profile) )
        {
            it = _transientVPs.erase(it);
            continue;
        }

        ++activeCount;
        ++it;
    }

    int32_t spawnNeeded = profile.count - activeCount;
    if ( spawnNeeded <= 0 )
        return;

    vec3d center = _viewerPosition + profile.spawn_offset;
    for (int32_t i = 0; i < spawnNeeded; ++i)
    {
        vec3d pos = center;
        pos.x += yw_RandomFloatSigned(profile.spawn_radius.x);
        pos.y += yw_RandomFloatSigned(profile.spawn_radius.y);
        pos.z += yw_RandomFloatSigned(profile.spawn_radius.z);

        vec3d velocity = profile.velocity;
        velocity.x += yw_RandomFloatSigned(profile.velocity_random.x);
        velocity.y += yw_RandomFloatSigned(profile.velocity_random.y);
        velocity.z += yw_RandomFloatSigned(profile.velocity_random.z);

        const int32_t lifeTime = yw_RandomInRange(profile.lifetime_min, profile.lifetime_max);
        const int32_t id = SpawnTransientVisual(0, profile.mesh3ds, pos, mat3x3::Ident(),
                                                lifeTime, 1.0, profile.tint,
                                                profile.scale, profile.spin,
                                                TTransientVPParticleControls(),
                                                profile.fade_in, profile.fade_out);
        if ( id > 0 && !_transientVPs.empty() && _transientVPs.back().id == id )
        {
            TTransientVP &fx = _transientVPs.back();
            fx.velocity = velocity;
            fx.atmosphericFX = true;
        }
    }
}

int32_t NC_STACK_ypaworld::SpawnAttachedTransientVP(int32_t modelId, NC_STACK_ypabact *owner, const vec3d &localOffset, int32_t lifeTime, float scale, bool useOwnerTransform, const World::TVisualTint &tint, const vec3d &axisScale, const vec3d &spin, bool playerFirstPersonOnly, const vec3d &localRotation, bool hideInOwnerMissileCamera, const TTransientVPParticleControls &particleControls, bool followOwnerVisualTransform, int32_t fadeIn, int32_t fadeOut, const std::string &external3dsPath, const std::string &externalBasePath)
{
    if ( !owner || lifeTime < 0 )
        return 0;

    NC_STACK_base *base = nullptr;
    if ( !external3dsPath.empty() )
        base = GetSharedExternalMesh(external3dsPath);
    if ( !base && !externalBasePath.empty() )
        base = GetSharedExternalBase(externalBasePath);

    if ( !base )
    {
        if ( modelId <= 0 || modelId >= (int32_t)_vhclModels.size() )
            return 0;
        base = _vhclModels.at(modelId);
    }

    if ( !base )
        return 0;

    _transientVPs.emplace_back(base, owner->GetBodyPosition(), owner->_rotation, lifeTime);

    TTransientVP &fx = _transientVPs.back();
    fx.id = _nextTransientVPId++;
    fx.followOwner = true;
    fx.followOwnerGid = owner->_gid;
    fx.followLocalOffset = localOffset;
    fx.followUseOwnerTransform = useOwnerTransform;
    fx.followOwnerVisualTransform = followOwnerVisualTransform;
    fx.playerFirstPersonOnly = playerFirstPersonOnly;
    fx.localRotation = localRotation;
    fx.hideInOwnerMissileCamera = hideInOwnerMissileCamera;
    fx.scale = scale > 0.0 ? scale : 1.0;
    fx.axisScale = vec3d(axisScale.x > 0.0f ? axisScale.x : 1.0f,
                         axisScale.y > 0.0f ? axisScale.y : 1.0f,
                         axisScale.z > 0.0f ? axisScale.z : 1.0f);
    fx.spin = spin;
    fx.tint = tint;
    fx.particleControls = particleControls;
    yw_ConfigureTransientVPFade(fx, fadeIn, fadeOut, (double)lifeTime);
    return fx.id;
}

int32_t NC_STACK_ypaworld::SpawnAttachedStatusTransientVP(int32_t modelId, NC_STACK_ypabact *owner,
                                                          const vec3d &localOffset, int32_t lifeTime,
                                                          bool trailOnly, bool rotateOffsetWithOwner,
                                                          const vec3d &axisScale,
                                                          const World::TVisualTint &tint,
                                                          const World::TVisualTint *trailTint)
{
    int32_t id = SpawnAttachedTransientVP(modelId, owner, localOffset, lifeTime,
                                          1.0, false, tint, axisScale,
                                          vec3d(0.0, 0.0, 0.0), false,
                                          vec3d(0.0, 0.0, 0.0), false);
    if ( id <= 0 || _transientVPs.empty() )
        return id;

    TTransientVP &fx = _transientVPs.back();
    if ( fx.id != id )
        return id;

    fx.followRotateOffset = rotateOffsetWithOwner;
    if ( trailOnly && fx.vp )
        fx.vp->skipGeometry = true;

    if ( trailTint )
    {
        fx.particleControls.enabled = true;
        fx.particleControls.tint = *trailTint;
        fx.particleControls.scale = axisScale;
        fx.particleControls.lifetimeScale = 1.0f;
    }

    return id;
}

int32_t NC_STACK_ypaworld::SpawnAttachedStatusTransientMesh(const std::string &path,
                                                            NC_STACK_ypabact *owner,
                                                            const vec3d &localOffset,
                                                            int32_t lifeTime,
                                                            bool rotateOffsetWithOwner,
                                                            const vec3d &axisScale,
                                                            const World::TVisualTint &tint)
{
    if ( !owner || path.empty() || lifeTime < 0 )
        return 0;

    NC_STACK_base *base = GetSharedExternalMesh(path);
    if ( !base )
        return 0;

    _transientVPs.emplace_back(base, owner->GetBodyPosition(), owner->_rotation, lifeTime);
    TTransientVP &fx = _transientVPs.back();
    fx.id = _nextTransientVPId++;
    fx.followOwner = true;
    fx.followOwnerGid = owner->_gid;
    fx.followLocalOffset = localOffset;
    fx.followRotateOffset = rotateOffsetWithOwner;
    fx.scale = 1.0f;
    fx.axisScale = vec3d(axisScale.x > 0.0f ? axisScale.x : 1.0f,
                         axisScale.y > 0.0f ? axisScale.y : 1.0f,
                         axisScale.z > 0.0f ? axisScale.z : 1.0f);
    fx.tint = tint;
    return fx.id;
}

bool NC_STACK_ypaworld::HasTransientVP(int32_t id) const
{
    if ( id <= 0 )
        return false;

    for (const TTransientVP &fx : _transientVPs)
    {
        if ( fx.id == id )
            return true;
    }

    return false;
}

void NC_STACK_ypaworld::RemoveTransientVP(int32_t id, int32_t fadeOut, int32_t particleFadeOut)
{
    if ( id <= 0 )
        return;

    for (auto it = _transientVPs.begin(); it != _transientVPs.end(); ++it)
    {
        if ( it->id == id )
        {
            const int32_t modelFadeOut = fadeOut > 0 ? fadeOut : 0;
            const int32_t trailFadeOut = it->particleControls.enabled && particleFadeOut > 0
                                       ? particleFadeOut : 0;
            const int32_t endingDuration = std::max(modelFadeOut, trailFadeOut);

            if ( endingDuration > 0 )
            {
                if ( !it->ending )
                {
                    it->ending = true;
                    it->endingAge = it->age;
                    it->endingDuration = endingDuration;
                    it->endingFadeOut = modelFadeOut;
                    it->endingParticleFadeOut = trailFadeOut;

                    // Persistent attached Decoration FX must be able to finish
                    // their visual fade even if the owner dies/disappears.
                    // Keep the last resolved world transform and detach only
                    // for the terminal visual envelope.
                    it->followOwner = false;
                }
                return;
            }

            _transientVPs.erase(it);
            return;
        }
    }
}

mat3x3 yw_BuildTransientVPRotationMatrix(const vec3d &degrees)
{
    const float degToRad = 0.01745329251994329577f;
    vec3d angle = degrees * degToRad;
    mat3x3 rot = mat3x3::Ident();

    if ( angle.x != 0.0 )
        rot *= mat3x3::RotateX(angle.x);
    if ( angle.y != 0.0 )
        rot *= mat3x3::RotateY(angle.y);
    if ( angle.z != 0.0 )
        rot *= mat3x3::RotateZ(angle.z);

    return rot;
}

bool yw_HasTransientVPRotation(const vec3d &rotation)
{
    return rotation.x != 0.0 || rotation.y != 0.0 || rotation.z != 0.0;
}

void yw_RenderTransientVPs(NC_STACK_ypaworld *world, std::list<NC_STACK_ypaworld::TTransientVP> *effects, baseRender_msg *arg)
{
    auto *worldRender=arg;
    for (auto it = effects->begin(); it != effects->end();)
    {
        auto effectRender=*worldRender;
        arg=&effectRender;
        const bool endingFinished = it->ending &&
                                    it->endingDuration > 0 &&
                                    it->age - it->endingAge >= it->endingDuration;
        if ( !it->vp || endingFinished ||
             (!it->ending && it->lifeTime > 0 && it->age >= it->lifeTime) )
        {
            it = effects->erase(it);
            continue;
        }

        if ( it->followOwner )
        {
            NC_STACK_ypabact *owner = yw_FindLiveBactByGidInList(world->_unitsList, it->followOwnerGid);

            if ( !owner )
            {
                it = effects->erase(it);
                continue;
            }
            // Attached flashes and emitters follow their actor's F2 clock.
            // The local copy keeps unrelated world effects in scaled time.
            arg->frameTime=owner->GetDebugFrameTime(arg->frameTime);
            arg->globTime+=owner->_debugVisualTimeOffset;
            arg->debugPlayerParticles=owner->IsPlayerTimeOwner();

            if ( it->playerFirstPersonOnly && (!owner->IsPlayerFirstPersonCameraActive() || world->_viewerBact != owner) )
            {
                it->age += arg->frameTime;
                ++it;
                continue;
            }

            if ( it->hideInOwnerMissileCamera &&
                 owner->_bact_type == BACT_TYPES_MISSLE &&
                 world->_viewerBact == owner )
            {
                it->age += arg->frameTime;
                ++it;
                continue;
            }

            // Status FX (damaged/debuff smoke, sparks, electric effects) should follow
            // the unit position, but must not inherit terrain pitch/roll.
            //
            // Important: do NOT clamp attached status FX against owner->_pSector->height.
            // On sloped sectors that value is only a coarse sector height/average, not
            // the exact ground height under the vehicle. Clamping against it randomly
            // yanks smoke/fire far above the hull when a tank climbs/descends terrain,
            // which is the intermittent "floating FX" bug seen in-game.
            if ( it->followUseOwnerTransform )
            {
                vec3d ownerPos = owner->GetBodyPosition();
                mat3x3 ownerRenderRot = owner->_rotation.Transpose();

                if ( it->followOwnerVisualTransform )
                {
                    vec3d visualOffset;
                    mat3x3 visualRotationDelta;
                    if ( owner->GetProjectileVisualMotionDelta(&visualOffset, &visualRotationDelta) )
                    {
                        ownerPos += visualOffset;
                        ownerRenderRot *= visualRotationDelta;
                    }
                }

                it->pos = ownerPos + ownerRenderRot.Transform(it->followLocalOffset);
                it->rot = ownerRenderRot.Transpose();
            }
            else
            {
                vec3d offset = it->followLocalOffset;
                if ( it->followRotateOffset )
                    offset = owner->_rotation.Transpose().Transform(offset);
                it->pos = owner->GetBodyPosition() + offset;
                it->rot = mat3x3::Ident();
            }
        }

        float scale = it->scale > 0.0 ? it->scale : 1.0;
        if ( it->chainFX )
        {
            float t = 1.0;
            if ( it->lifeTime > 0 )
                t = (float)it->age / (float)it->lifeTime;

            if ( t < 0.0 )
                t = 0.0;
            else if ( t > 1.0 )
                t = 1.0;

            float chainScale = 0.0f;
            if ( it->hasMidScale )
            {
                if ( t < 0.5f )
                {
                    const float firstHalf = t * 2.0f;
                    chainScale = it->startScale +
                                 (it->midScale - it->startScale) * firstHalf;
                }
                else
                {
                    const float secondHalf = (t - 0.5f) * 2.0f;
                    chainScale = it->midScale +
                                 (it->endScale - it->midScale) * secondHalf;
                }
            }
            else
                chainScale = it->startScale +
                             (it->endScale - it->startScale) * t;

            scale *= chainScale;

            if ( !it->chainBases.empty() )
            {
                int32_t index = (int32_t)(t * (float)it->chainBases.size());
                if ( index >= (int32_t)it->chainBases.size() )
                    index = (int32_t)it->chainBases.size() - 1;

                if ( index != it->chainIndex && it->chainBases[index] )
                {
                    it->vp.reset(it->chainBases[index]->GenRenderInstance());
                    it->chainIndex = index;
                    if ( index < (int32_t)it->chainTints.size() )
                        it->tint = it->chainTints[index];
                    else
                        it->tint = World::TVisualTint();
                }
            }
        }

        vec3d renderScale = it->axisScale * scale;
        mat3x3 renderRot = it->rot.Transpose();
        if ( yw_HasTransientVPRotation(it->localRotation) )
            renderRot *= yw_BuildTransientVPRotationMatrix(it->localRotation);
        if ( World::Spin::HasStrength(it->spin) )
            renderRot *= World::Spin::BuildMatrix(it->spin, it->age);

        it->vp->Bas->TForm().Pos = it->pos;
        it->vp->Bas->TForm().SclRot = renderRot * mat3x3::Scale(renderScale);

        World::TVisualTint renderTint = it->tint;
        float fadeFactor = 1.0f;
        if ( it->ending )
        {
            const double endingElapsed = (double)std::max<int32_t>(0, it->age - it->endingAge);
            fadeFactor = it->endingFadeOut > 0
                       ? World::ComputeVPFadeEnvelope(endingElapsed,
                                                      (double)it->endingFadeOut,
                                                      0.0,
                                                      (double)it->endingFadeOut)
                       : 0.0f;
            renderTint.a *= fadeFactor;
        }
        else if ( it->fadeIn > 0 || it->fadeOut > 0 )
        {
            const double fadeElapsed = it->fadeElapsed >= 0.0
                                     ? it->fadeElapsed : (double)it->age;
            const double fadeDuration = it->fadeDuration > 0.0
                                      ? it->fadeDuration : (double)it->lifeTime;
            fadeFactor = World::ComputeVPFadeEnvelope(fadeElapsed, fadeDuration,
                                                       it->fadeIn, it->fadeOut);
            renderTint.a *= fadeFactor;
        }

        // External meshes can lack material fog. Keep every transient effect
        // inside the world draw distance, independently of its lifetime fade.
        // With the sky dome active the visible world ends at the sky horizon.
        // _normalVizLimit is only a CPU-side budget (diamond window plus hidden
        // safety sectors), several times larger than the horizon.
        float vizLimit = (float)world->_normalVizLimit;
        if ( world->_skyRender && world->_skyObject )
            vizLimit = std::min(vizLimit, (float)world->GetSkyHorizonDistance());
        const float distance = arg->particleSystem ? 0.0f
            : (it->pos - TF::Engine.GetViewPoint()->CalcPos).length();
        const float fadeLength = std::max(0, world->_normalFadeLength);
        const float fadeStart = std::max(0.0f, vizLimit - fadeLength);
        const float distanceFade = arg->particleSystem ? 1.0f
            : distance >= vizLimit ? 0.0f
            : World::ComputeSpatialFalloff(distance - fadeStart, fadeLength);
        fadeFactor *= distanceFade;
        renderTint.a *= distanceFade;

        // OpenNeoUA custom VP controls: affect only this transient model and
        // particles emitted by it, then restore defaults for other effects.
        GFX::TGLColor oldTint = arg->tint;
        bool oldColorizeTint = arg->colorizeTint;
        float oldVPFadeFactor = arg->vpFadeFactor;
        GFX::TGLColor oldParticleTint = arg->particleTint;
        bool oldParticleColorizeTint = arg->particleColorizeTint;
        vec3d oldParticleScale = arg->particleScale;
        vec3d oldParticleSpin = arg->particleSpin;
        float oldParticleLifetimeScale = arg->particleLifetimeScale;
        bool oldParticleTintAlphaAffectsAdditive = arg->particleTintAlphaAffectsAdditive;
        if ( it->particleControls.enabled )
        {
            World::TVisualTint particleTint = it->particleControls.tint;
            float particleFadeFactor = 1.0f;
            if ( it->ending )
            {
                const double endingElapsed = (double)std::max<int32_t>(0, it->age - it->endingAge);
                particleFadeFactor = it->endingParticleFadeOut > 0
                                   ? World::ComputeVPFadeEnvelope(endingElapsed,
                                                                  (double)it->endingParticleFadeOut,
                                                                  0.0,
                                                                  (double)it->endingParticleFadeOut)
                                   : 0.0f;
            }
            else if ( it->particleControls.fadeIn > 0 || it->particleControls.fadeOut > 0 )
            {
                const double particleDuration = it->lifeTime > 0
                                              ? (double)it->lifeTime : 0.0;
                particleFadeFactor = World::ComputeVPFadeEnvelope((double)it->age,
                                                                  particleDuration,
                                                                  it->particleControls.fadeIn,
                                                                  it->particleControls.fadeOut);
            }

            particleTint.a *= particleFadeFactor;
            arg->particleTint = GFX::TGLColor(particleTint.r, particleTint.g, particleTint.b, particleTint.a);
            arg->particleColorizeTint = it->particleControls.tint.ColorizesRGB();
            arg->particleScale = it->particleControls.scale;
            arg->particleLifetimeScale = it->particleControls.lifetimeScale;
            arg->particleTintAlphaAffectsAdditive = it->particleControls.tintAlphaAffectsAdditive;
        }
        else
        {
            arg->particleTint = GFX::TGLColor(renderTint.r, renderTint.g, renderTint.b, renderTint.a);
            arg->particleColorizeTint = it->tint.ColorizesRGB();
            arg->particleScale = renderScale;
            arg->particleLifetimeScale = 1.0f;
            arg->particleTintAlphaAffectsAdditive = false;
        }
        arg->particleSpin = it->spin;
        arg->vpFadeFactor = fadeFactor;

        bool tinted = !renderTint.IsNeutral();
        if ( tinted )
            arg->tint = GFX::TGLColor(renderTint.r, renderTint.g, renderTint.b, renderTint.a);
        else
            arg->tint = GFX::TGLColor(1.0, 1.0, 1.0, 1.0);
        // Alpha-only tint/fade must not force grayscale. RGB colorization is enabled
        // only when the authored tint actually changes the target hue.
        arg->colorizeTint = it->tint.ColorizesRGB();

        if ( distanceFade > 0.0f )
            it->vp->Bas->Render(arg, it->vp.get());
        worldRender->adeCount=arg->adeCount;

        arg->tint = oldTint;
        arg->colorizeTint = oldColorizeTint;
        arg->vpFadeFactor = oldVPFadeFactor;
        arg->particleTint = oldParticleTint;
        arg->particleColorizeTint = oldParticleColorizeTint;
        arg->particleScale = oldParticleScale;
        arg->particleSpin = oldParticleSpin;
        arg->particleLifetimeScale = oldParticleLifetimeScale;
        arg->particleTintAlphaAffectsAdditive = oldParticleTintAlphaAffectsAdditive;

        if ( !it->followOwner && (it->velocity.x != 0.0 || it->velocity.y != 0.0 || it->velocity.z != 0.0) )
            it->pos += it->velocity * ((double)arg->frameTime / 1000.0);
        else if ( it->followOwner && it->mobilePowerFX &&
                  (it->velocity.x != 0.0 || it->velocity.y != 0.0 || it->velocity.z != 0.0) )
            it->followLocalOffset += it->velocity * ((double)arg->frameTime / 1000.0);

        it->age += arg->frameTime;
        ++it;
    }
}

void NC_STACK_ypaworld::BuildingDecorationFXUpdate()
{
    for (cellArea &cell : _cells)
    {
        if ( !yw_IsActiveBuildingSpawnerCell(cell) )
        {
            RemoveTransientVP(cell.DecorationFXPersistentId,
                              cell.DecorationFX.fade_out,
                              cell.DecorationFX.vp_trail_fade_out);
            cell.DecorationFXPersistentId = 0;
            cell.DecorationFXNextTime = 0;
            cell.BuildingSpawnLastTime = 0;
            cell.BuildingSpawnedGids.clear();
            continue;
        }

        UpdateDecorationFX(cell.DecorationFX, cell.DecorationFXNextTime, cell.CenterPos, &cell.DecorationFXPersistentId);

        if ( cell.PurposeIndex >= 0 && (size_t)cell.PurposeIndex < _buildProtos.size() )
            yw_UpdateBuildingSpawner(this, cell, _buildProtos[cell.PurposeIndex]);
    }
}

vec3d yw_RandomPointInDisk(float radius)
{
    if ( !std::isfinite(radius) || radius <= 0.0f )
        return vec3d(0.0, 0.0, 0.0);

    const double unit = (double)rand() / (double)RAND_MAX;
    const double angle = ((double)rand() / (double)RAND_MAX) * 6.28318530717958647692;
    const double r = sqrt(unit) * radius;
    return vec3d(cos(angle) * r, 0.0, sin(angle) * r);
}
