#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable

layout (location = 0) in vec4 position;
layout (location = 2)  in vec4 normal;

uniform    mat4 worldViewProj;
uniform    vec3 center;

void main(void)
{
  vec3 pos =  position.xyz  - center;
  vec3 enlarged = pos*1.05 + center;
  gl_Position =worldViewProj * vec4(enlarged,1.0);
}
