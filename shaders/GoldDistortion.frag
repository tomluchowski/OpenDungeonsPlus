#version 330  core
#extension GL_ARB_shading_language_include : enable
#include "ShadowMapping.glsl"
#include "LocalLighting.glsl"


uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

uniform vec4 ambientLightColour;
uniform vec4 cameraPosition;
uniform bool shadowingEnabled;
uniform float veinTime;
uniform float veinGain;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in vec4 VertexPos;
in mat3 TBN;
 
out vec4 color;

// Cheap 3D hash for the sparkle cells
float goldHash(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

void main (void)  
{  
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    // The texture is dark rock with gold veins. The vein mask tells the two apart.
    float vein = smoothstep(0.10, 0.28, texelColor.r - texelColor.b);

    vec3 result;
    
    // precompute the lighting term
    vec4 shadow = vec4(1.0);
    if(shadowingEnabled)
        shadow = vec4(sampleShadow(shadowmap, VertexPos));
    vec3 lightingTerm = getLocalLighting(FragPos, Normal, cameraPosition.xyz, shadow.r) + ambientLightColour.rgb;
    
    result = lightingTerm * texelColor;

    // Metallic sheen on the veins, strongest where the surface turns away from the viewer
    vec3 viewDir = normalize(cameraPosition.xyz - FragPos);
    float sheen = pow(1.0 - abs(dot(Normal, viewDir)), 3.0);
    result += vein * veinGain * sheen * 0.12 * vec3(1.0, 0.8, 0.4);

    // Sparse glitter points inside the veins that flash up and fade, no area glow
    vec3 cellPos = FragPos * 16.0;
    vec3 cell = floor(cellPos);
    float cellRand = goldHash(cell);
    float phase = fract(cellRand * 7.3 + veinTime * 0.25);
    float flash = pow(sin(phase * 3.14159), 12.0);
    float dotShape = smoothstep(0.30, 0.0, length(fract(cellPos) - 0.5));
    float glitter = step(0.90, cellRand) * flash * dotShape * vein;
    result += glitter * veinGain * vec3(1.0, 0.88, 0.55);

    color = vec4(min(result, vec3(1.0)), 1.0);
}    
