#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;

layout (location = 0) in vec4 position;
layout (location = 2) in vec3 normal;

out vec3 out_FragPos;
out vec3 out_Normal;

void main() {
    vec3 P = (worldMatrix * position).xyz;
    out_Normal = mat3(worldMatrix) * normal;
    gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);
    out_FragPos = P;
}
