#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;
uniform    mat4 lightMatrix;
layout (location = 0) in vec4 position;
layout (location = 2)  in vec3 normal;
layout (location = 14) in vec3 tangent;
 
layout (location = 8) in vec2 uv0;
layout (location = 8) in vec2 uv1;
out vec2 out_UV0;
out vec2 out_UV1;
out vec3 FragPos;
out vec4 VertexPos; 

out mat3 TBN;
 
 
void main() {
    // compute world space position, tangent, bitangent
    vec3 P = (worldMatrix * position).xyz;
    vec3 T = normalize(vec3(worldMatrix * vec4(tangent, 0.0)));
    vec3 B = normalize(vec3(worldMatrix * vec4(cross(tangent, normal), 0.0))); 
    vec3 N = cross(B, T);
    TBN = mat3(T, B, N);
 
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);
    FragPos = P;
 
    out_UV0 = uv0;
    out_UV1 = uv1;
    VertexPos = lightMatrix * position;
}  
