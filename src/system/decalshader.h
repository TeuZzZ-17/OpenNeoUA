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
// Terrain craters and dents change the shape of a surface after (or before) a
// decal lands on it. The decal must stay painted on it, so the dug surfaces of
// the frame are drawn once more into a texture holding, for each pixel, where
// its point was before any hit ("rest" place and normal, see the rest shaders
// below). A dug pixel is then mapped exactly as on the untouched world: no
// guess about the crater shape is made here.
//
// GLSL 450 for SDL_GPU (see stdshdr.cpp for the resource sets). gl_FragCoord and
// the screen textures have their origin at the top-left corner. The file has no
// dependencies so a test can compile the same text.
#ifndef SYSTEM_DECALSHADER_H_INCLUDED
#define SYSTEM_DECALSHADER_H_INCLUDED

#include <string>

namespace GFX
{

// Radii of the jagged decal outline, equally spaced around the centre.
constexpr int DECAL_SHAPE_SAMPLES = 24;

// Uniforms of the decal pass (std140, fragment set 3). Shape holds the
// DECAL_SHAPE_SAMPLES radii four per vec4.
struct TDecalParams
{
    float ScreenInv[2];   // 1 / screen size
    float RestOn;         // 0 = no dug surface in this frame, uRest is not read
    float Textured;
    float Project[4];     // 1/m00, 1/m11, then A and B of z_ndc = A + B / z
    float ViewToWorld[12];// camera axes to world axes, three vec4 columns
    float Fog[4];         // same four values of the standard shader
    float AlphaFog[4];
    float FogColor[4];
    float AtmosphereColor[4];
    float Origin[4];      // impact point, relative to the camera
    float AxisT[4];       // decal plane axes and surface normal
    float AxisB[4];
    float AxisN[4];
    float Size[2];        // decal size along T and B
    float EdgeFade;       // 0 = hard edge, 1 = fades across the whole decal
    float Colorize;       // 1 = Tint.rgb replaces the hue
    float Tint[4];
    float Shape[DECAL_SHAPE_SAMPLES];
};

// Uniforms of the rest pass (std140, same block in both stages).
struct TDecalRestParams
{
    float Proj[16];
    float View[16];
    float ViewToWorld[16];  // upper 3x3 used
    float Cut[4];           // x: 1 = the mesh has holes where its texture is clear
};

// A rest normal travels in the alpha of the rest texture: two numbers of 12
// bits (octahedral map) in one float, plus 1 so that 0 means "not dug".
inline std::string DecalNormalPackText()
{
    return R"GLSL(
float packNormal(vec3 n)
{
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = n.z >= 0.0 ? n.xy
           : (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    vec2 q = floor((e * 0.5 + 0.5) * 4095.0 + 0.5);
    return 1.0 + q.x * 4096.0 + q.y;
}

vec3 unpackNormal(float value)
{
    value -= 1.0;
    float high = floor(value / 4096.0);
    vec2 e = vec2(high, value - high * 4096.0) / 4095.0 * 2.0 - 1.0;
    vec3 n = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    float fold = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -fold : fold;
    n.y += n.y >= 0.0 ? -fold : fold;
    return normalize(n);
}
)GLSL";
}

inline std::string DecalRestParamsText(int set)
{
    return "layout(set = " + std::to_string(set) + R"GLSL(, binding = 0, std140) uniform RestParams
{
    mat4 uProj;
    mat4 uView;
    mat4 uViewToWorld;
    vec4 uCut;
};
)GLSL";
}

// Rest pass: a dug mesh drawn again, writing where each pixel's point was
// before the craters and dents (camera-relative, world axes) and the normal
// the surface had there.
inline std::string DecalRestVertexShaderText()
{
    return "#version 450\n" + DecalRestParamsText(1) + R"GLSL(
layout(location = 0) in vec3 vPos;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vRest;
layout(location = 0) out vec3 vNowView;
layout(location = 1) out vec3 vRestView;
layout(location = 2) out vec2 vTexCoord;
void main()
{
    vec4 tformed = uView * vec4(vPos, 1.0);
    gl_Position = uProj * tformed;
    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;
    vNowView = tformed.xyz;
    vRestView = (uView * vec4(vRest, 1.0)).xyz;
    vTexCoord = vUV;
}
)GLSL";
}

inline std::string DecalRestFragmentShaderText()
{
    return "#version 450\n" + DecalRestParamsText(3) + DecalNormalPackText() + R"GLSL(
layout(set = 2, binding = 0) uniform sampler2D uImage;   // texture of the mesh (read only when cut)
layout(location = 0) in vec3 vNowView;
layout(location = 1) in vec3 vRestView;
layout(location = 2) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

void main()
{
    if (uCut.x != 0.0 && texture(uImage, vTexCoord).a <= 0.0)
        discard;

    // The rest places of a triangle lie on the polygon it was cut from, so
    // this is that polygon's normal, whatever shape the triangle has now.
    mat3 viewToWorld = mat3(uViewToWorld);
    vec3 normal = cross(viewToWorld * dFdx(vRestView), viewToWorld * dFdy(vRestView));
    float size = length(normal);
    if (size <= 0.0)
        discard;
    // The side the camera sees: decided on the surface as it is drawn.
    if (dot(cross(dFdx(vNowView), dFdy(vNowView)), vNowView) > 0.0)
        normal = -normal;

    fragColor = vec4(viewToWorld * vRestView, packNormal(normal / size));
}
)GLSL";
}

inline std::string DecalVertexShaderText()
{
    return R"GLSL(#version 450
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vUV;
void main() { gl_Position = vec4(vPos.xy, 0.0, 1.0); }
)GLSL";
}

inline std::string DecalFragmentShaderText()
{
    return std::string("#version 450\n#define SHAPE_SAMPLES ") +
           std::to_string(DECAL_SHAPE_SAMPLES) + "\n" + DecalNormalPackText() + R"GLSL(
layout(set = 2, binding = 0) uniform sampler2D uImage;       // decal image (unused when not textured)
layout(set = 2, binding = 1) uniform sampler2D uSceneDepth;  // depth of the world, without any unit
layout(set = 2, binding = 2) uniform sampler2D uRest;        // rest place and normal of the dug pixels
layout(set = 3, binding = 0, std140) uniform DecalParams
{
    vec4 uScreen;            // xy = 1 / screen size, z = rest on, w = textured
    vec4 uProject;
    mat3 uViewToWorld;
    vec4 uFog;
    vec4 uAlphaFog;
    vec4 uFogColor;
    vec4 uAtmosphereColor;
    vec4 uOrigin;
    vec4 uAxisT4;
    vec4 uAxisB4;
    vec4 uAxisN4;
    vec4 uSizeFade;          // xy = size, z = edge fade, w = colorize
    vec4 uTint;
    vec4 uShape[SHAPE_SAMPLES / 4];
};
layout(location = 0) out vec4 fragColor;

float shapeRadius(int i) { return uShape[i / 4][i % 4]; }

// 1/z is the value that changes linearly across a flat polygon on screen.
float inverseDepth(vec2 frag)
{
    float depth = texture(uSceneDepth, frag * uScreen.xy).r;
    return (depth * 2.0 - 1.0 - uProject.z) / uProject.w;
}

vec3 viewPosition(vec2 frag, float invZ)
{
    float z = 1.0 / invZ;
    vec2 uv = frag * uScreen.xy;
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
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
    vec3 uAxisT = uAxisT4.xyz;
    vec3 uAxisB = uAxisB4.xyz;
    vec3 uAxisN = uAxisN4.xyz;
    vec2 uSize = uSizeFade.xy;
    float uEdgeFade = uSizeFade.z;

    vec2 frag = gl_FragCoord.xy;
    float invZ = inverseDepth(frag);
    vec3 view = viewPosition(frag, invZ);
    vec3 world = uViewToWorld * view;

    // A dug pixel is mapped where its point was before the craters and dents:
    // there the world is as it always was, so the rules below need no change.
    vec4 rest = uScreen.z != 0.0 ? texture(uRest, frag * uScreen.xy) : vec4(0.0);
    bool dug = rest.a > 0.5;
    vec3 offset = (dug ? rest.xyz : world) - uOrigin.xyz;

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

    vec3 normal;
    if (dug)
        normal = unpackNormal(rest.a);
    else
    {
        normal = surfaceNormal(frag, view, invZ);
        float normalLength = length(normal);
        // No usable neighbours (screen border): treat the pixel as the decal plane.
        normal = normalLength > 0.000001 ? normal / normalLength : uAxisN;
        if (dot(normal, world) > 0.0)
            normal = -normal;
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
    float boundary = mix(shapeRadius(first), shapeRadius(next), slot - floor(slot));
    if (radius > boundary)
        discard;

    vec4 source;
    float start = 1.0 - uEdgeFade;
    float span = max(uEdgeFade, 0.0001);
    if (uScreen.w != 0.0)
    {
        // Same fade of the image on every surface, measured from the image centre.
        float t = clamp((radius * 2.0 - start) / span, 0.0, 1.0);
        source = texture(uImage, uv);
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
    if (uSizeFade.w != 0.0)
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
        color.rgb = mix(color.rgb, uAtmosphereColor.rgb,
                        horizonFogFactor(fadeDistance, uAlphaFog.y, uAlphaFog.z, uAlphaFog.w));
        if (uFog.x != 0.0)
            color.rgb = mix(color.rgb, uFogColor.rgb,
                            horizonFogFactor(fadeDistance, uFog.y, uFog.z, uFog.w));
    }
    else if (uFog.x != 0.0 && uFog.z > 0.0)
    {
        color.rgb = mix(color.rgb, uFogColor.rgb,
                        clamp((view.z - uFog.y) / uFog.z, 0.0, 1.0) * uFog.w);
    }

    fragColor = color;
}
)GLSL";
}

}

#endif // SYSTEM_DECALSHADER_H_INCLUDED
