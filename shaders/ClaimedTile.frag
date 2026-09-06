#version 330  core
#extension GL_ARB_shading_language_include : enable
#include "ShadowMapping.glsl"
#include "LocalLighting.glsl"


uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D crossmap;
uniform sampler2D shadowmap;


uniform vec4 ambientLightColour;
uniform vec4 cameraPosition;
uniform vec4 diffuseSurface;
uniform vec4 seatColor;
uniform bool shadowingEnabled;
in vec2 out_UV0;
in vec2 out_UV1;
in vec2 out_UV2;
in vec3 FragPos;
in vec4 VertexPos; 
in mat3 TBN;
in vec3 tangent; 
out vec4 color;

void main (void)  
{  

    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    vec4 shadow = vec4(1.0, 1.0, 1.0,1.0);
    if(shadowingEnabled)
        shadow = vec4(sampleShadow(shadowmap, VertexPos));
    
    vec3 result;
        
    // precompute the lighting term
    vec3 lightingTerm = getLocalLighting(FragPos, Normal, cameraPosition.xyz, shadow.r) + ambientLightColour.rgb;
    
    vec4 crossMap = texture(crossmap, out_UV2.st);   
    if(crossMap.r < 0.05)
    {
        vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
        result =  lightingTerm * mix(texelColor,diffuseSurface.rgb,0.01);
    }
    else
        result =  lightingTerm * crossMap.rgb * seatColor.rgb;

    color  = vec4( result.xyz,  1.0);
       
}    

