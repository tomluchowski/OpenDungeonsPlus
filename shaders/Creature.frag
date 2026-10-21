#version 330  core
#extension GL_ARB_shading_language_include : enable
#include "LocalLighting.glsl"
#define epsilon 0.00001

uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

uniform vec4 ambientLightColour;
uniform vec4 cameraPosition;
uniform vec3 ambient;
uniform bool shadowingEnabled;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in vec4 VertexPos; 
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  

    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    vec4 shadow = vec4(1.0, 1.0, 1.0,1.0);
    vec4 tmpVertexPos = VertexPos;
    
    // compute shadowmap
    if(shadowingEnabled){
		if(tmpVertexPos.z > epsilon ){
		    tmpVertexPos /= tmpVertexPos.w;
		    shadow = texture(shadowmap, tmpVertexPos.xy); 
		}
    }
    
    
    
    vec3 result;
        
    // precompute the lighting term
    vec3 lightingTerm = getLocalLighting(FragPos, Normal, cameraPosition.xyz, shadow.r) + ambientLightColour.rgb * ambient;
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    result =  lightingTerm * texelColor;

    color  = vec4(result.xyz,  1.0);

       
}    

