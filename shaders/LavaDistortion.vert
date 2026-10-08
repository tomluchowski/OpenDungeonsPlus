#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

#include "PerlinNoise.glsl" 

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
uniform float time1;
layout (location = 0) in vec4 position;
 
layout (location = 8) in vec2 uv_0;

out vec2 out_UV0;

out vec3 FragPos;
 
#define PI 3.1415926538

vec3 deform(vec3 pos) {
    // The noise displacement fades out towards the tile borders (at n + 0.5) so that shared
    // border edges stay straight and identical for neighbouring tile meshes.
    float fade = tileBorderFade(pos.xy);
    pos.x += fade * perlin(pos.x,pos.y);
    pos.y += fade * perlin(pos.y,pos.x);
    pos.z = pos.z + sin(time1/10.0 + (pos.x  ) * PI)/12.0 + cos(2*PI*cos(time1/10.0 + 2*pos.y ))/12.0;
    return pos;
}
 
 
void main() {
    // compute world space position, tangent, bitangent
    vec3 P = (worldMatrix * position).xyz;
    
    
    P = deform(P); 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);

    FragPos = P;
 
    out_UV0 = uv_0;
}  
