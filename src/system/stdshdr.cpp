// Built-in GLSL 450 shaders of the renderer (compiled to SPIR-V at startup).
//
// SDL_GPU resource sets: vertex uniforms set 1, fragment samplers set 2, fragment
// uniforms set 3. Clip z is converted from OpenGL's [-w, w] to [0, w], so depth
// values, the projection matrices and the decal depth maths stay as they were.

#include "gfx.h"

namespace GFX
{

// Same std140 block in both stages (GFXEngine::_stdParams).
#define STD_PARAMS(set) \
"layout(set = " #set ", binding = 0, std140) uniform Parameters\n" \
"{\n" \
"    mat4 MProj;\n" \
"    mat4 MView;\n" \
"    vec4 Fog;              // enable, start, length, strength\n" \
"    vec4 AlphaFog;         // mode (1 radial, 2 camera depth), start, length, strength\n" \
"    int Textured;\n" \
"    int Flat;\n" \
"    int ATest;\n" \
"    int Colorize;\n" \
"    vec4 ColorMul;\n" \
"    vec4 FogColor;\n" \
"    vec4 AtmosphereColor;\n" \
"};\n"

const std::string GFXEngine::_stdPShaderText =
"#version 450\n"
STD_PARAMS(3)
"layout(set = 2, binding = 0) uniform sampler2D uTexture;\n"
"layout(location = 0) in vec4 smoothColor;\n"
"layout(location = 1) flat in vec4 flatColor;\n"
"layout(location = 2) in vec2 texCoords;\n"
"layout(location = 3) in vec3 viewPosition;\n"
"layout(location = 0) out vec4 fragColor;\n"
"float horizonFogFactor(float distanceValue, float startValue, float lengthValue, float strengthValue)\n"
"{\n"
"    if (lengthValue <= 0.0 || strengthValue <= 0.0) return 0.0;\n"
"    float t = clamp((distanceValue - startValue) / lengthValue, 0.0, 1.0);\n"
"    return (AlphaFog.x == 2.0 ? t : t * t * (3.0 - 2.0 * t)) * strengthValue;\n"
"}\n"
"void main()\n"
"{\n"
"    vec4 clr = Flat != 0 ? flatColor : smoothColor;\n"
"    vec4 sourceColor = Textured != 0 ? texture(uTexture, texCoords) * clr : clr;\n"
"    if (Colorize != 0)\n"
"    {\n"
"        float intensity = max(sourceColor.r, max(sourceColor.g, sourceColor.b));\n"
"        fragColor = vec4(ColorMul.rgb * intensity, sourceColor.a * ColorMul.a);\n"
"    }\n"
"    else\n"
"        fragColor = sourceColor * ColorMul;\n"
"    if (ATest != 0 && fragColor.a <= 0.0)\n"
"        discard;\n"
"\n"
"    if (AlphaFog.x != 0.0)\n"
"    {\n"
"        float fadeDistance = AlphaFog.x == 2.0 ? viewPosition.z : length(viewPosition.xz);\n"
"        float atmosphere = horizonFogFactor(fadeDistance, AlphaFog.y, AlphaFog.z, AlphaFog.w);\n"
"        fragColor.rgb = mix(fragColor.rgb, AtmosphereColor.rgb, atmosphere);\n"
"        if (Fog.x != 0.0)\n"
"        {\n"
"            float darkness = horizonFogFactor(fadeDistance, Fog.y, Fog.z, Fog.w);\n"
"            fragColor.rgb = mix(fragColor.rgb, FogColor.rgb, darkness);\n"
"        }\n"
"    }\n"
"    else if (Fog.x != 0.0)\n"
"    {\n"
"        // Legacy non-horizon depth fade of ordinary RFLAGS_FOG.\n"
"        float legacyFog = 0.0;\n"
"        if (Fog.z > 0.0)\n"
"            legacyFog = clamp((viewPosition.z - Fog.y) / Fog.z, 0.0, 1.0) * Fog.w;\n"
"        fragColor.rgb = mix(fragColor.rgb, FogColor.rgb, legacyFog);\n"
"    }\n"
"}\n";

const std::string GFXEngine::_stdVShaderText =
"#version 450\n"
STD_PARAMS(1)
"layout(location = 0) in vec3 vPos;\n"
"layout(location = 1) in vec4 vColor;\n"
"layout(location = 2) in vec2 vUV;\n"
"layout(location = 0) out vec4 smoothColor;\n"
"layout(location = 1) flat out vec4 flatColor;\n"
"layout(location = 2) out vec2 texCoords;\n"
"layout(location = 3) out vec3 viewPosition;\n"
"void main()\n"
"{\n"
"    vec4 tformed = MView * vec4(vPos, 1.0);\n"
"    gl_Position = MProj * tformed;\n"
"    gl_Position.z = (gl_Position.z + gl_Position.w) * 0.5;\n"
"    viewPosition = tformed.xyz;\n"
"    // Fog is evaluated per pixel; keep the original vertex colour and alpha.\n"
"    flatColor = vColor;\n"
"    smoothColor = vColor;\n"
"    texCoords = vUV;\n"
"}\n";

#undef STD_PARAMS

// Full-screen passes: the quad already is in clip space. texCoords follow the
// OpenGL renderer (0,0 = bottom-left of the screen), see PostShaderPrelude.
const std::string GFXEngine::_postVShaderText =
"#version 450\n"
"layout(location = 0) in vec3 vPos;\n"
"layout(location = 1) in vec4 vColor;\n"
"layout(location = 2) in vec2 vUV;\n"
"layout(location = 0) out vec2 texCoords;\n"
"void main()\n"
"{\n"
"    gl_Position = vec4(vPos.xy, 0.0, 1.0);\n"
"    texCoords = vec2(vPos.x, vPos.y) * 0.5 + 0.5;\n"
"}\n";

// Declarations every Data/res post shader gets in front of its own text
// (GFXEngine::PostParams on the C++ side). A post shader file holds only its
// functions and main(); it writes fragColor.
const std::string GFXEngine::_postPrelude =
"#version 450\n"
"layout(set = 2, binding = 0) uniform sampler2D uTexture;   // image being processed\n"
"layout(set = 2, binding = 1) uniform sampler2D uAux;       // filter LUT, or the VHS image in the blend\n"
"layout(set = 3, binding = 0, std140) uniform PostParams\n"
"{\n"
"    vec3 normclr;            // palette flash: colour kept\n"
"    float filterStrength;    // visual filter LUT blend, 0 = off\n"
"    vec3 invclr;             // palette flash: colour inverted\n"
"    float vhsStrength;\n"
"    ivec2 screenSize;\n"
"    int randval;             // per-frame random number (game time aware)\n"
"    int millisecs;           // game time aware clock\n"
"    float atmosphereStrength;\n"
"    float atmosphereExposure;\n"
"    float atmosphereContrast;\n"
"    float atmosphereSaturation;\n"
"    float atmosphereVignette;\n"
"};\n"
"layout(location = 0) in vec2 texCoords;   // (0,0) = bottom-left of the screen\n"
"layout(location = 0) out vec4 fragColor;\n"
"// Reads a screen-sized image at a texCoords-style position.\n"
"vec4 sampleScreen(sampler2D image, vec2 uv) { return texture(image, vec2(uv.x, 1.0 - uv.y)); }\n"
"#line 1\n";

}
