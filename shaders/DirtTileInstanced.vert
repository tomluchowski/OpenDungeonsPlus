#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

#include <OgreUnifiedShader.h>
#include "PerlinNoise.glsl" 
#include "FFPLib_Transform.glsl"

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
uniform    mat4 lightMatrix;

layout (location = 0) in vec4 position;
layout (location = 2)  in vec3 normal;



/* layout (location = 3)  in mat3x4 uv1; */
layout (location = 8) in vec2 uv_0;
layout (location = 8) in vec2 uv_1;
layout (location = 14) in vec3 tangent;
IN(mat3x4	uv1, TEXCOORD1)
out vec2 out_UV0;
out vec2 out_UV1;

out vec3 FragPos;
out vec4 VertexPos; 
 
out mat3 TBN;
 
float freq = 3.1415; 
 
vec3 deform(vec3 pos) {
    pos.x += perlin(pos.x,pos.y);
    pos.y += perlin(pos.y,pos.x);
    return pos;
}
 
 
void main() {
    vec4 local_position = position;
    FFP_Transform(uv1, local_position, local_position.xyz);
    vec3 local_normal = normal;
    FFP_Transform(uv1, local_normal, local_normal.xyz);

    vec3 local_tangent =tangent;
    FFP_Transform(uv1, local_tangent, local_tangent.xyz);
    // compute world space local_position, local_tangent, bilocal_tangent
    vec3 P = (worldMatrix * local_position).xyz;
    vec3 T = normalize(vec3(worldMatrix * vec4(local_tangent, 0.0)));
    vec3 B = normalize(vec3(worldMatrix * vec4(cross(local_tangent, local_normal), 0.0))); 
 
    // apply deformation
    vec3 PT = deform(P + T);
    vec3 PB = deform(P + B);
    P = deform(P);
    
    // compute local_tangent frame
    T = normalize(PT - P);
    B = normalize(PB - P);
    vec3 N = cross(B, T);
    TBN = mat3(T, B, N);
 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);
    FragPos = P;
 
    out_UV0 = uv_0;
    out_UV1 = uv_1;
    VertexPos = lightMatrix * local_position;    
}  
