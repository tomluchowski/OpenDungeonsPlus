#version 330  core
#extension GL_ARB_shading_language_include : enable
#include "LocalLighting.glsl"


uniform sampler2D decalmap;
uniform sampler2D normalmap;

uniform vec4 ambientLightColour;
uniform vec4 cameraPosition;
uniform vec4 diffuseSurface;
uniform bool shadowingEnabled;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    
    vec3 result;
    
    
    // precompute the lighting term
    vec3 lightingTerm = getLocalLighting(FragPos, Normal, cameraPosition.xyz, 1.0) + ambientLightColour.rgb;
    
    if(diffuseSurface.rgb != vec3(1.0,1.0,1.0))
        result =  lightingTerm * mix(texelColor, diffuseSurface.rgb,0.5);
    else
        result =  lightingTerm * texelColor;
    color = vec4(result.xyz,  1.0);
       
}    
