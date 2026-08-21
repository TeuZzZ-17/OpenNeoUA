
#include "gfx.h"

namespace GFX
{

const std::string GFXEngine:: _stdPShaderText =
"\
#version 140\n\
layout(std140) uniform Parameters\
{\
    mat4 MProj;\
    mat4 MView;\
    vec4 Fog;\
    vec4 AlphaFog;\
    bool Textured;\
    bool Flat;\
    bool ATest;\
    bool Colorize;\
    vec4 ColorMul;\
    vec4 FogColor;\
    vec4 AtmosphereColor;\
};\
uniform sampler2D texture;\
in vec4 smoothColor;\
flat in vec4 flatColor;\
in vec2 texCoords;\
in vec3 viewPosition;\
float smoothFogFactor(float distanceValue, float startValue, float lengthValue, float strengthValue)\
{\
    if (lengthValue <= 0.0 || strengthValue <= 0.0) return 0.0;\
    float t = clamp((distanceValue - startValue) / lengthValue, 0.0, 1.0);\
    return (t * t * (3.0 - 2.0 * t)) * strengthValue;\
}\
void main()\
{\
    vec4 clr;\
    if (Flat) clr = flatColor; else clr = smoothColor;\
    vec4 sourceColor;\
    if (Textured)\
        sourceColor = texture2D(texture, texCoords) * clr;\
    else\
        sourceColor = clr;\
    if (Colorize)\
    {\
        float intensity = max(sourceColor.r, max(sourceColor.g, sourceColor.b));\
        gl_FragColor = vec4(ColorMul.rgb * intensity, sourceColor.a * ColorMul.a);\
    }\
    else\
        gl_FragColor = sourceColor * ColorMul;\
    if (ATest && gl_FragColor.w <= 0.0)\
        discard;\
\
    if (AlphaFog.x != 0.0)\
    {\
        float radialDistance = length(viewPosition.xz);\
        float atmosphere = smoothFogFactor(radialDistance, AlphaFog.y, AlphaFog.z, AlphaFog.w);\
        gl_FragColor.rgb = mix(gl_FragColor.rgb, AtmosphereColor.rgb, atmosphere);\
        if (Fog.x != 0.0)\
        {\
            float darkness = smoothFogFactor(radialDistance, Fog.y, Fog.z, Fog.w);\
            gl_FragColor.rgb = mix(gl_FragColor.rgb, FogColor.rgb, darkness);\
        }\
    }\
    else if (Fog.x != 0.0)\
    {\
        /* Preserve the legacy non-horizon depth-fade path for ordinary RFLAGS_FOG. */\
        float legacyFog = 0.0;\
        if (Fog.z > 0.0)\
            legacyFog = clamp((viewPosition.z - Fog.y) / Fog.z, 0.0, 1.0) * Fog.w;\
        gl_FragColor.rgb = mix(gl_FragColor.rgb, FogColor.rgb, legacyFog);\
    }\
}";


const std::string GFXEngine:: _stdVShaderText =
"\
#version 140\n\
layout(std140) uniform Parameters\
{\
    mat4 MProj;\
    mat4 MView;\
    vec4 Fog;\
    vec4 AlphaFog;\
    bool Textured;\
    bool Flat;\
    bool ATest;\
    bool Colorize;\
    vec4 ColorMul;\
    vec4 FogColor;\
    vec4 AtmosphereColor;\
};\
attribute vec3 vPos;\
attribute vec4 vColor;\
attribute vec2 vUV;\
\
out vec4 smoothColor;\
flat out vec4 flatColor;\
out vec2 texCoords;\
out vec3 viewPosition;\
void main()\
{\
    vec4 tformed = MView * vec4(vPos, 1.0);\
    gl_Position = MProj * tformed;\
    viewPosition = tformed.xyz;\
\
    /* Fog is evaluated per pixel; preserve the original vertex colour and alpha. */\
    flatColor = vColor;\
    smoothColor = vColor;\
    if (Textured) texCoords = vUV;\
}";

}
