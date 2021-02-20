#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

#include "PerlinNoise.glsl" 

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
uniform    mat4 lightMatrix;

layout (location = 0) in vec4 aPos;
layout (location = 2)  in vec3 aNormal;
layout (location = 14) in vec3 aTangent;
 
layout (location = 8) in vec2 uv_0;
out vec2 out_UV0;
out vec2 out_UV1;
out vec2 out_UV2;

out vec3 FragPos;
out vec4 VertexPos; 

out mat3 TBN;
out vec3 tangent;

float freq = 3.1415; 
 
vec3 deform(vec3 pos) {
    pos.x += perlin(pos.x,pos.y);
    pos.y += perlin(pos.y,pos.x);
    return pos;
}
 
 
void main() {
    // compute world space position, tangent, bitangent
    vec4 P4 = (worldMatrix * aPos);
    vec3 P = P4.xyz;
    vec3 T = normalize(vec3(worldMatrix * vec4(aTangent, 0.0)));
    vec3 B = normalize(vec3(worldMatrix * vec4(cross(aTangent, aNormal), 0.0))); 
 
    // apply deformation
    vec3 PT = deform(P + T);
    vec3 PB = deform(P + B);
    P = deform(P);
    
    // compute tangent frame
    T = normalize(PT - P);
    B = normalize(PB - P);
    vec3 N = cross(B, T);
    TBN = mat3(T, B, N);
 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);
    FragPos = P;
    vec4 P_prim = inverse(worldMatrix) * P4;
    VertexPos = lightMatrix * P_prim;    
 
    out_UV0 = uv_0;
    out_UV1 = uv_0;
    out_UV2 = uv_0;
    tangent = aTangent;
    VertexPos = lightMatrix * aPos;
}  
