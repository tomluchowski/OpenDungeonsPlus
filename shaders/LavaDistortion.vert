#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

#include "PerlinNoise.glsl" 

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
uniform float time1;
layout (location = 0) in vec4 aPos;
 
layout (location = 8) in vec2 uv_0;

out vec2 out_UV0;

out vec3 FragPos;
 
#define PI 3.1415926538

vec3 deform(vec3 pos) {
    pos.x += perlin(pos.x,pos.y);
    pos.y += perlin(pos.y,pos.x);
    pos.z = pos.z + sin(time1/10.0 + (pos.x  ) * PI)/12.0 + cos(2*PI*cos(time1/10.0 + 2*pos.y ))/12.0;
    return pos;
}
 
 
void main() {
    // compute world space position, tangent, bitangent
    vec3 P = (worldMatrix * aPos).xyz;
    
    
    P = deform(P); 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);

    FragPos = P;
 
    out_UV0 = uv_0;
}  
