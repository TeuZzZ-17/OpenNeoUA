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
           std::to_string(DECAL_SHAPE_SAMPLES) + R"GLSL(
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

    // Up to this height a level surface counts as the hit surface itself: the
    // impact point comes from collision, which never matches the visuals exactly.
    // The mismatch has nothing to do with the decal size, hence the upper limit.
    vec3 offset = world - uOrigin;
    float reach = 0.5 * length(uSize);
    float sameSurface = clamp(reach * 0.25, 8.0, 40.0);

    // Cheap rejection first: most pixels of the rectangle are not in the box.
    float height = dot(offset, uAxisN);
    float heightAbs = abs(height);
    vec2 along = vec2(dot(offset, uAxisT), dot(offset, uAxisB));
    float distanceOnPlane = length(along);
    if (heightAbs > max(reach, sameSurface * 1.5) || distanceOnPlane > reach)
        discard;

    vec3 normal = surfaceNormal(frag, view, invZ);
    float normalLength = length(normal);
    // No usable neighbours (screen border): treat the pixel as the decal plane.
    normal = normalLength > 0.000001 ? normal / normalLength : uAxisN;
    if (dot(normal, world) > 0.0)
        normal = -normal;

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
