#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 worldMatrix;

layout (location = 0) in vec4 position;


void main(){

vec3 P = (worldMatrix * position).xyz;
gl_Position = projectionMatrix * viewMatrix * vec4(P, 1.0);


}