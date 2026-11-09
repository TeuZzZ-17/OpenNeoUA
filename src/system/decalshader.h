// Shader of the projected decals (burn marks, craters, bullet holes).
//
// A decal is not a mesh: it is a box of space around the impact. Each pixel of
// the box reads the depth of what was REALLY drawn there and paints on it, so
// the legacy LEGO surfaces and the collision skeletons do not matter at all.
// Nothing drawn in a pixel means no decal in that pixel.
//
// To avoid stretching, every surface is laid flat on the decal plane first:
//  - a surface is rotated around the line where it meets the decal plane, so
//    walls and slopes keep their real proportions and stay joined to the ground;
//  - a level surface at another height (top of a step) continues the distance
//    already walked on the side of the step;
//  - a steep surface reached only from behind (far side of a wall) gets nothing.
//
// The file has no dependencies so a test can compile the same text on a real GPU.
#ifndef SYSTEM_DECALSHADER_H_INCLUDED
#define SYSTEM_DECALSHADER_H_INCLUDED

#include <string>

namespace GFX
{

// Radii of the jagged decal outline, equally spaced around the centre.
constexpr int DECAL_SHAPE_SAMPLES = 24;
// Craters/dents and building footprints a decal knows about (TDecalField).
constexpr int DECAL_FIELD_SHAPES = 16;
constexpr int DECAL_FIELD_FOOTPRINTS = 16;
// World lattice the engine cuts dug surfaces on (World::Crater::VISUAL_STEP).
constexpr double DECAL_FIELD_STEP = 300.0 / 16.0;
// Band joining a capped box to free ground (World::Crater::FOOTPRINT_BLEND).
constexpr double DECAL_FOOTPRINT_BLEND = 150.0;

// #version 120 is the only level that exists on both GL paths (gfx.vbo on/off).
inline std::string DecalVertexShaderText(bool vbo)
{
    if ( vbo )
        return
            "#version 120\n"
            "attribute vec3 vPos;\n"
            "void main() { gl_Position = vec4(vPos.xy, 0.0, 1.0); }\n";

    return
        "#version 120\n"
        "void main() { gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0); }\n";
}

inline std::string DecalFragmentShaderText()
{
    return std::string("#version 120\n#define SHAPE_SAMPLES ") +
           std::to_string(DECAL_SHAPE_SAMPLES) + "\n#define FIELD_SHAPES " +
           std::to_string(DECAL_FIELD_SHAPES) + "\n#define FIELD_FOOTPRINTS " +
           std::to_string(DECAL_FIELD_FOOTPRINTS) + "\n#define FIELD_STEP " +
           std::to_string(DECAL_FIELD_STEP) + "\n#define FOOTPRINT_BLEND " +
           std::to_string(DECAL_FOOTPRINT_BLEND) + R"GLSL(
uniform sampler2D uImage;       // Decal image (unused when uTextured is 0).
uniform sampler2D uSceneDepth;  // Depth of the world, without any unit.
uniform vec2 uScreenInv;        // 1 / screen size.
uniform vec4 uProject;          // 1/m00, 1/m11, then A and B of z_ndc = A + B / z.
uniform mat3 uViewToWorld;      // Camera axes to world axes.
uniform vec4 uFog;              // Same four values of the standard shader.
uniform vec4 uAlphaFog;
uniform vec3 uFogColor;
uniform vec3 uAtmosphereColor;

uniform vec3 uOrigin;           // Impact point, relative to the camera.
uniform vec3 uAxisT;            // Decal plane axes and surface normal.
uniform vec3 uAxisB;
uniform vec3 uAxisN;
uniform vec2 uSize;             // Decal size along T and B.
// Craters and dents under the decal (world/terraincrater_core.h), positions
// from the decal origin in world axes. The same field the engine dug with, so
// a pixel can be taken back to where its point was before the hits.
uniform int uFieldShapes;
uniform vec4 uFieldPos[FIELD_SHAPES];        // xyz, w: copies (> 0 crater, < 0 dent)
uniform vec4 uFieldSize[FIELD_SHAPES];       // radius, depth, rim, band
uniform vec3 uFieldDir[FIELD_SHAPES];        // dent: direction into the surface
uniform int uFieldFrozen;                    // Map border boxes (nothing moves), first in uFieldFootprint,
uniform int uFieldFootprints;                // then the building boxes.
uniform vec4 uFieldFootprint[FIELD_FOOTPRINTS];   // min x, min z, max x, max z
uniform vec2 uFieldLimits;                   // game.deformation_ground/building_limit (< 0: none)
uniform vec2 uFieldLattice;                  // Decal origin x, z modulo FIELD_STEP.
uniform float uEdgeFade;        // 0 = hard edge, 1 = fades across the whole decal.
uniform float uShape[SHAPE_SAMPLES];
uniform vec4 uTint;
uniform float uColorize;        // 1 = uTint.rgb replaces the hue.
uniform float uTextured;

// 1/z is the value that changes linearly across a flat polygon on screen.
float inverseDepth(vec2 frag)
{
    float depth = texture2D(uSceneDepth, frag * uScreenInv).r;
    return (depth * 2.0 - 1.0 - uProject.z) / uProject.w;
}

vec3 viewPosition(vec2 frag, float invZ)
{
    float z = 1.0 / invZ;
    vec2 ndc = frag * uScreenInv * 2.0 - 1.0;
    return vec3(ndc * z * uProject.xy, z);
}

// Surface normal from depth alone. A pixel on an edge has one neighbour on each
// surface: the side whose depth keeps going straight is the pixel's own surface.
vec3 surfaceNormal(vec2 frag, vec3 view, float invZ)
{
    float left1 = inverseDepth(frag + vec2(-1.0, 0.0));
    float left2 = inverseDepth(frag + vec2(-2.0, 0.0));
    float right1 = inverseDepth(frag + vec2(1.0, 0.0));
    float right2 = inverseDepth(frag + vec2(2.0, 0.0));
    float down1 = inverseDepth(frag + vec2(0.0, -1.0));
    float down2 = inverseDepth(frag + vec2(0.0, -2.0));
    float up1 = inverseDepth(frag + vec2(0.0, 1.0));
    float up2 = inverseDepth(frag + vec2(0.0, 2.0));

    vec3 alongX = abs(2.0 * left1 - left2 - invZ) < abs(2.0 * right1 - right2 - invZ)
                ? view - viewPosition(frag + vec2(-1.0, 0.0), left1)
                : viewPosition(frag + vec2(1.0, 0.0), right1) - view;
    vec3 alongY = abs(2.0 * down1 - down2 - invZ) < abs(2.0 * up1 - up2 - invZ)
                ? view - viewPosition(frag + vec2(0.0, -1.0), down1)
                : viewPosition(frag + vec2(0.0, 1.0), up1) - view;

    return cross(uViewToWorld * alongX, uViewToWorld * alongY);
}

// DeformationLimits::At: the largest move allowed at a place.
float fieldCap(vec2 xz, float uncapped)
{
    float ground = uFieldLimits.x < 0.0 ? uncapped : uFieldLimits.x;
    float building = uFieldLimits.y < 0.0 ? uncapped : uFieldLimits.y;
    float influence = 0.0, frozen = 0.0;
    for (int i = 0; i < FIELD_FOOTPRINTS; ++i)
    {
        if (i >= uFieldFrozen + uFieldFootprints)
            break;
        vec4 box = uFieldFootprint[i];
        vec2 outside = max(vec2(0.0), max(box.xy - xz, xz - box.zw));
        float t = min(1.0, length(outside) / FOOTPRINT_BLEND);
        float near = 1.0 - t * t * (3.0 - 2.0 * t);
        if (i < uFieldFrozen)
            frozen = max(frozen, near);
        else
            influence = max(influence, near);
    }
    return mix(ground, building, influence) * (1.0 - frozen);
}

// How far the craters and dents moved the point that was at p (SampleAt and
// DeformationLimits::Apply, without the dent folds: *dent is their size).
vec3 fieldMove(vec3 p, out float dent, out float reached)
{
    reached = 0.0;      // 1 when a shape reaches p, even where its moves cancel out.
    float drop = 0.0, deepest = 0.0, highestRim = 0.0, strongest = 0.0;
    vec3 push = vec3(0.0);
    for (int i = 0; i < FIELD_SHAPES; ++i)
    {
        if (i >= uFieldShapes)
            break;
        vec4 size = uFieldSize[i];
        vec3 rel = p - uFieldPos[i].xyz;
        // Identical hits come as one shape: they add up just the same.
        float copies = abs(uFieldPos[i].w);
        if (uFieldPos[i].w > 0.0)
        {
            float reachC = size.x * (size.z > 0.0 ? 1.6 : 1.0);
            float distance = length(rel.xz);
            float keep = 1.0 - smoothstep(size.w, size.w * 1.25, abs(rel.y));
            if (distance >= reachC || keep <= 0.0)
                continue;
            float d = distance / size.x;
            float one = 0.0;
            if (d < 1.0)
            {
                float inside = 1.0 - d * d;
                one = size.y * inside * inside;
            }
            if (size.z > 0.0)
            {
                float ridge = (d - 1.0) / 0.22;
                if (ridge > -3.5)
                    one -= size.y * size.z * exp(-ridge * ridge) * (1.0 - smoothstep(1.3, 1.6, d));
            }
            drop += one * keep * copies;
            reached = 1.0;
            deepest = max(deepest, size.y);
            highestRim = max(highestRim, size.y * size.z);
        }
        else
        {
            float distance = length(rel);
            if (distance >= size.x)
                continue;
            float d = distance / size.x;
            float inside = 1.0 - d * d;
            push += uFieldDir[i] * (size.y * inside * inside * copies);
            reached = 1.0;
            strongest = max(strongest, size.y * 1.6);
        }
    }
    drop = clamp(drop, -highestRim, deepest * 1.75);
    float pushed = length(push);
    if (pushed > strongest * 1.75 && pushed > 0.0)
        push *= strongest * 1.75 / pushed;
    if (uFieldLimits.x >= 0.0)
        drop = clamp(drop, -uFieldLimits.x, uFieldLimits.x);
    vec3 move = vec3(0.0, drop, 0.0) + push;     // World Y points down.
    float moved = length(move);
    float cap = fieldCap(p.xz, moved);
    float scale = moved > cap && moved > 0.0 ? cap / moved : 1.0;
    dent = length(push) * scale;
    return move * scale;
}

vec3 fieldMove(vec3 p, out float dent)
{
    float reached;
    return fieldMove(p, dent, reached);
}

// How the dug mesh moved the point that was at p. The engine cut the surface
// on a world lattice (FIELD_STEP) and it is flat between the lattice points,
// each square split in two triangles along one diagonal or the other: the
// move of either split, mixed from the moves of the square's corners.
void latticeMoves(vec3 p, out vec3 splitA, out vec3 splitB)
{
    vec2 grid = (p.xz + uFieldLattice) / FIELD_STEP;
    vec2 square = floor(grid);
    vec2 f = grid - square;
    vec2 corner = square * FIELD_STEP - uFieldLattice;
    float unused;
    vec3 m00 = fieldMove(vec3(corner.x, p.y, corner.y), unused);
    vec3 m10 = fieldMove(vec3(corner.x + FIELD_STEP, p.y, corner.y), unused);
    vec3 m01 = fieldMove(vec3(corner.x, p.y, corner.y + FIELD_STEP), unused);
    vec3 m11 = fieldMove(vec3(corner.x + FIELD_STEP, p.y, corner.y + FIELD_STEP), unused);
    // Diagonal from corner 00 to corner 11.
    splitA = f.x >= f.y ? m00 + f.x * (m10 - m00) + f.y * (m11 - m10)
                        : m00 + f.y * (m01 - m00) + f.x * (m11 - m01);
    // Diagonal from corner 10 to corner 01.
    splitB = f.x + f.y <= 1.0 ? m00 + f.x * (m10 - m00) + f.y * (m01 - m00)
                              : m11 + (1.0 - f.x) * (m01 - m11) + (1.0 - f.y) * (m10 - m11);
}

float horizonFogFactor(float distanceValue, float startValue, float lengthValue, float strengthValue)
{
    if (lengthValue <= 0.0 || strengthValue <= 0.0) return 0.0;
    float t = clamp((distanceValue - startValue) / lengthValue, 0.0, 1.0);
    return (uAlphaFog.x == 2.0 ? t : t * t * (3.0 - 2.0 * t)) * strengthValue;
}

void main()
{
    vec2 frag = gl_FragCoord.xy;
    float invZ = inverseDepth(frag);
    vec3 view = viewPosition(frag, invZ);
    vec3 world = uViewToWorld * view;

    // Craters and dents moved the surface after (or before) the decal: take the
    // pixel back to where its point was before them (fixed point of the same
    // field the engine dug with), then map the decal there.
    vec3 here = world - uOrigin;
    vec3 offset = here;
    float dent = 0.0;
    float reached = 0.0;
    float splitDoubt = 0.0;
    if (uFieldShapes > 0)
    {
        for (int i = 0; i < 3; ++i)
            offset = here - fieldMove(offset, dent, reached);
        // The mesh is flat between lattice points where the field is curved:
        // take the pixel back as the mesh moved it. Of the two ways a square
        // can be split, the one putting it nearer the hit surface (continuous
        // across squares, so no steps along the lattice).
        if (reached > 0.0)
        {
            vec3 splitA, splitB;
            latticeMoves(offset, splitA, splitB);
            vec3 offsetA = here - splitA;
            vec3 offsetB = here - splitB;
            offset = abs(dot(offsetA, uAxisN)) <= abs(dot(offsetB, uAxisN)) ? offsetA : offsetB;
            // Where the two splits disagree (steep walls) the real piece may
            // differ from both: that much more doubt. Zero on the square sides,
            // so it adds no steps.
            splitDoubt = abs(dot(offsetA - offsetB, uAxisN));
        }
    }
    // Inside a crater or dent, also where its moves cancel out (between the
    // dug bowl and the raised rim the surface is back at its height, but tilted).
    bool movedPixel = reached > 0.0;

    // Up to this height a level surface counts as the hit surface itself: the
    // impact point comes from collision, which never matches the visuals exactly.
    // The mismatch has nothing to do with the decal size, hence the upper limit.
    float reach = 0.5 * length(uSize);
    float sameSurface = clamp(reach * 0.25, 8.0, 40.0);
    float height = dot(offset, uAxisN);

    // Cheap rejection first: most pixels of the rectangle are not in the box.
    float heightAbs = abs(height);
    vec2 along = vec2(dot(offset, uAxisT), dot(offset, uAxisB));
    float distanceOnPlane = length(along);
    if (heightAbs > max(reach, sameSurface * 1.5) || distanceOnPlane > reach)
        discard;

    // A moved point that was on the hit surface was painted there: it keeps its
    // place on the image and its cover, whatever shape it has now. The dent
    // folds are not in the field: their size is the doubt on its old place.
    bool painted = movedPixel && heightAbs <= sameSurface + 0.6 * dent + splitDoubt + 2.0;

    vec3 normal = surfaceNormal(frag, view, invZ);
    float normalLength = length(normal);
    // No usable neighbours (screen border): treat the pixel as the decal plane.
    normal = normalLength > 0.000001 ? normal / normalLength : uAxisN;
    if (dot(normal, world) > 0.0)
        normal = -normal;
    if (movedPixel && !painted)
    {
        // Normal the surface had before the hits: its tangents moved with the
        // field, so it goes back through the transpose of the field's change
        // (taken over one lattice step, as the surface is flat between points).
        float unused;
        float halfStep = 0.5 * FIELD_STEP;
        vec3 alongX = (fieldMove(offset + vec3(halfStep, 0.0, 0.0), unused) - fieldMove(offset - vec3(halfStep, 0.0, 0.0), unused)) / FIELD_STEP;
        vec3 alongY = (fieldMove(offset + vec3(0.0, halfStep, 0.0), unused) - fieldMove(offset - vec3(0.0, halfStep, 0.0), unused)) / FIELD_STEP;
        vec3 alongZ = (fieldMove(offset + vec3(0.0, 0.0, halfStep), unused) - fieldMove(offset - vec3(0.0, 0.0, halfStep), unused)) / FIELD_STEP;
        vec3 before = normal + vec3(dot(alongX, normal), dot(alongY, normal), dot(alongZ, normal));
        if (length(before) > 0.000001)
            normal = normalize(before);
    }

    float facing = clamp(dot(normal, uAxisN), -0.95, 1.0);
    vec3 leanWorld = normal - uAxisN * facing;
    vec2 lean = vec2(dot(leanWorld, uAxisT), dot(leanWorld, uAxisB));
    float leanLength = length(lean);
    vec2 outward = distanceOnPlane > 0.0001 ? along / distanceOnPlane : vec2(0.0);

    float level = smoothstep(0.906, 0.985, facing);

    // Lay the surface flat, then add the walk over a step if this pixel is on its
    // top. Near the decal axis the walk shrinks to nothing, so the image keeps a
    // normal centre instead of collapsing into a point.
    vec2 unfolded = along - lean * (height / (1.0 + facing));
    unfolded += outward * level * min(distanceOnPlane, max(heightAbs - sameSurface, 0.0));


    // Undersides and steep surfaces turned away from the impact stay clean.
    float steep = smoothstep(0.35, 0.60, leanLength);
    float away = leanLength > 0.0001 ? dot(lean / leanLength, outward) * sign(height) : 0.0;
    away *= smoothstep(0.0, sameSurface, heightAbs);
    float coverage = smoothstep(-0.3, -0.1, facing) * (1.0 - steep * smoothstep(0.02, 0.20, away));

    // A level surface far above or below is another floor, not a continuation.
    float band = max(reach * 0.6, sameSurface * 1.5);
    coverage *= 1.0 - level * smoothstep(band * 0.6, band, heightAbs);
    if (painted)
    {
        unfolded = along;
        coverage = 1.0;
    }
    if (coverage <= 0.0)
        discard;

    vec2 uv = unfolded / uSize + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        discard;

    // Jagged outline: radius of the shape in this direction.
    vec2 centred = uv - 0.5;
    float radius = length(centred);
    float angle = radius > 0.0001 ? atan(centred.y, centred.x) : 0.0;
    float slot = fract(angle / 6.28318530718) * float(SHAPE_SAMPLES);
    int first = int(floor(slot));
    int next = first + 1 >= SHAPE_SAMPLES ? 0 : first + 1;
    float boundary = mix(uShape[first], uShape[next], slot - floor(slot));
    if (radius > boundary)
        discard;

    vec4 source;
    float start = 1.0 - uEdgeFade;
    float span = max(uEdgeFade, 0.0001);
    if (uTextured != 0.0)
    {
        // Same fade of the image on every surface, measured from the image centre.
        float t = clamp((radius * 2.0 - start) / span, 0.0, 1.0);
        source = texture2D(uImage, uv);
        source.a *= uEdgeFade > 0.0 ? 1.0 - t * t * (3.0 - 2.0 * t) : 1.0;
    }
    else
    {
        // Procedural burn: darker in the middle, edge following the jagged outline.
        float normalized = clamp(radius / boundary, 0.0, 1.0);
        float darkness = 0.10 + normalized * normalized * (3.0 - 2.0 * normalized) * 0.12;
        float t = clamp((normalized - start) / span, 0.0, 1.0);
        source = vec4(darkness, darkness, darkness,
                      uEdgeFade > 0.0 ? 1.0 - t * t * (3.0 - 2.0 * t) : 1.0);
    }

    vec4 color;
    if (uColorize != 0.0)
    {
        float intensity = max(source.r, max(source.g, source.b));
        color = vec4(uTint.rgb * intensity, source.a * uTint.a);
    }
    else
        color = source * uTint;
    color.a *= coverage;

    // Same fog of the surface underneath (see the standard fragment shader).
    if (uAlphaFog.x != 0.0)
    {
        float fadeDistance = uAlphaFog.x == 2.0 ? view.z : length(view.xz);
        color.rgb = mix(color.rgb, uAtmosphereColor,
                        horizonFogFactor(fadeDistance, uAlphaFog.y, uAlphaFog.z, uAlphaFog.w));
        if (uFog.x != 0.0)
            color.rgb = mix(color.rgb, uFogColor,
                            horizonFogFactor(fadeDistance, uFog.y, uFog.z, uFog.w));
    }
    else if (uFog.x != 0.0 && uFog.z > 0.0)
    {
        color.rgb = mix(color.rgb, uFogColor,
                        clamp((view.z - uFog.y) / uFog.z, 0.0, 1.0) * uFog.w);
    }

    gl_FragColor = color;
}
)GLSL";
}

}

#endif // SYSTEM_DECALSHADER_H_INCLUDED
