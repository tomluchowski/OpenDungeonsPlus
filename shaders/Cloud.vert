#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable


#define USE_OGRE_FROM_FUTURE
#include <OgreUnifiedShader.h>
#include "FFPLib_Transform.glsl" 

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
layout (location = 0) in vec4 position;
 
layout (location = 8) in vec2 uv_0;
layout (location = 8) in vec2 uv_1;
IN(mat3x4	uv1, TEXCOORD1)
out vec2 out_UV0;
out vec2 out_UV1;
out vec3 FragPos;
 
float freq = 3.1415; 
 

 
 
void main() {
    // compute world space position, tangent, bitangent
    vec4 local_position = position;
    FFP_Transform(uv1, local_position, local_position.xyz);
    
    
    vec3 P = (worldMatrix * local_position).xyz;
 
    //P = deform(P);
    

 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);
    FragPos = P;
 
    out_UV0 = FragPos.xy;
}  
