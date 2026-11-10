// Smoke rising from ground decals: stateless puff math. Parameters live in each begin_fx ground_decal block.
#ifndef WORLD_GROUNDDECAL_SMOKE_H_INCLUDED
#define WORLD_GROUNDDECAL_SMOKE_H_INCLUDED

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "protos.h"
#include "common/vectors.h"

namespace World
{
namespace GroundDecalSmoke
{

// What the world keeps for one smoking decal. Puffs are not stored: each one
// is recomputed from the age every frame (see Evaluate below).
struct Emitter
{
    TGroundDecalSmoke params;   // Copy of the block's smoke settings.
    vec3d pos;
    float radius = 0.0f;
    int32_t startTime = 0;
    int32_t duration = 0;
    uint32_t seed = 0;
    int count = 0;
    int32_t activeDuration = 0; // Rolled effect_active_time, reused by every activation.
    int32_t retrigger = 0;      // Ms between activations; 0 = one activation only.
    int32_t endTime = 0;        // Decal end in world time; 0 = no end (permanent).
    int32_t meshCycle = -1;
    int32_t meshCycleStart = 0; // Start time of the current pulse interval.
    int32_t meshCycleLen = 0;   // Length of the current pulse interval; 0 = not rolled yet.
};

// One drawable smoke puff. pos is a world position (UA height grows towards -Y).
struct Puff
{
    vec3d pos;
    float size = 0.0f;
    float alpha = 0.0f;
    float phase = 0.0f;     // 0 = just born, 1 = about to disappear
    float shade = 1.0f;     // Brightness multiplier, differs from puff to puff.
};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kGroundLift = 2.0f;      // Keeps puffs clear of the decal surface bias.
constexpr float kMinParticleSize = 1.0f;
constexpr int kMaxPuffs = 50;

inline uint32_t Hash(uint32_t seed, uint32_t a, uint32_t b)
{
    uint32_t h = seed ^ 0x9e3779b9u;
    h ^= a + 0x85ebca6bu + (h << 6) + (h >> 2);
    h ^= b + 0xc2b2ae35u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

inline float HashUnit(uint32_t h)
{
    return (float)(h & 0x00ffffffu) / 16777216.0f;
}

// Random value in [minValue, maxValue] taken from a hash.
inline float RangeFrom(uint32_t h, float minValue, float maxValue)
{
    return minValue + (maxValue - minValue) * HashUnit(h);
}

// Length in ms of the interval that starts pulse number `cycle`. A fixed
// effect_pulse_interval gives the same length every time; min~max rolls it per pulse.
inline int PulseInterval(const TGroundDecalSmoke &p, uint32_t seed, int cycle)
{
    if ( p.pulse_interval_max <= p.pulse_interval_min )
        return std::max(200, p.pulse_interval_min);
    return std::max(200, (int)RangeFrom(Hash(seed, (uint32_t)cycle, 0x494e5456u),
                                        (float)p.pulse_interval_min,
                                        (float)p.pulse_interval_max));
}

// Delay between two activations of the whole effect. 0 means one activation.
// A configured range is rolled once per decal, like effect_active_time.
inline int RetriggerInterval(const TGroundDecalSmoke &p, uint32_t seed)
{
    if ( p.retrigger_max <= 0 )
        return 0;
    const int low = std::max(0, p.retrigger_min);
    const int high = std::max(low, p.retrigger_max);
    if ( high <= low )
        return std::max(200, low);
    return std::max(200, (int)RangeFrom(Hash(seed, 0x52545247u, 0u),
                                        (float)low, (float)high));
}

// An emitter is removed only when it cannot activate again. With a retrigger
// interval it stays alive (possibly waiting) until the decal itself ends.
inline bool EmitterExpired(const Emitter &e, int32_t now)
{
    if ( e.retrigger > 0 && e.endTime != 0 && now >= e.endTime )
        return true;

    const int32_t age = now - e.startTime;
    if ( age < 0 )
        return true;
    if ( age < e.duration )
        return false;
    return e.retrigger <= 0;
}

// Life shape: linear fade-in, full strength, then a linear fade-out that ends
// exactly at `duration`. Because the number of active puffs follows this value,
// the smoke thins out piece by piece.
inline float Intensity(int32_t age, int32_t duration, int32_t fadeIn, int32_t fadeOut)
{
    if ( duration <= 0 || age < 0 || age >= duration )
        return 0.0f;

    const float up = fadeIn > 0 ? std::min(1.0f, (float)age / (float)fadeIn) : 1.0f;
    const float down = fadeOut > 0 ? std::min(1.0f, (float)(duration - age) / (float)fadeOut) : 1.0f;
    return up * down;
}

// Count is given for a decal of radius 50; bigger decals get more puffs.
inline int ScaledCount(int base, float radius, int cap)
{
    if ( base <= 0 || !std::isfinite(radius) || radius <= 0.0f )
        return 0;

    const float scale = std::max(0.5f, std::min(radius / 50.0f, 3.0f));
    return std::max(1, std::min((int)std::lround((float)base * scale), cap));
}

// A slot is one puff position that keeps restarting: its period is fixed, the
// place of every new cycle is re-rolled from the cycle number. Slots above the
// current intensity are off, which makes puffs disappear one by one.
// A puff starts near the middle of the decal, rises and drifts sideways with a
// per-decal wind direction, grows and fades out.
inline bool Evaluate(const Emitter &e, int slot, int32_t age, Puff *out)
{
    const TGroundDecalSmoke &p = e.params;
    const float intensity = Intensity(age, e.duration, p.fade_in, p.fade_out);
    const float gate = intensity * (float)e.count - (float)slot;
    if ( gate <= 0.0f )
        return false;

    const uint32_t slotHash = Hash(e.seed, (uint32_t)slot, 0x534d4f4bu);
    const float period = std::max(200.0f, RangeFrom(Hash(slotHash, 1u, 0u),
                                                    (float)p.puff_time_min,
                                                    (float)p.puff_time_max));
    const float t = (float)age + period * HashUnit(Hash(slotHash, 2u, 0u));
    const float cycles = t / period;
    const uint32_t cycle = (uint32_t)cycles;
    const float phase = cycles - (float)cycle;
    const uint32_t cycleHash = Hash(slotHash, cycle, 3u);

    const float angle = 2.0f * kPi * HashUnit(Hash(cycleHash, 4u, 0u));
    const float distance = std::sqrt(HashUnit(Hash(cycleHash, 5u, 0u))) *
                           p.spread * 0.01f * e.radius;
    const float baseSize = RangeFrom(Hash(cycleHash, 6u, 0u), p.size_min, p.size_max) *
                           0.01f * e.radius;

    const float windAngle = 2.0f * kPi * HashUnit(Hash(e.seed, 0x57494e44u, 0u));
    const float rise = p.rise_speed * 0.01f * e.radius * (phase * period * 0.001f);
    const float drift = rise * p.drift * 0.01f;

    const float size = std::max(kMinParticleSize,
                                baseSize * 0.6f * (1.0f + (p.growth - 1.0f) * phase));
    out->pos = e.pos + vec3d(std::cos(angle) * distance + std::cos(windAngle) * drift,
                             -(kGroundLift + 0.05f * e.radius + rise),
                             std::sin(angle) * distance + std::sin(windAngle) * drift);
    out->size = size;
    out->alpha = std::min(1.0f, phase / 0.12f) * (1.0f - phase) * std::min(1.0f, gate);
    out->phase = phase;
    out->shade = 0.8f + 0.4f * HashUnit(Hash(cycleHash, 7u, 0u));
    return true;
}

}
}

#endif // WORLD_GROUNDDECAL_SMOKE_H_INCLUDED
