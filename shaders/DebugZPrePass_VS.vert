#version 330  core
#extension GL_ARB_separate_shader_objects : enable
uniform mat4 cWorldViewProj;

layout (location = 0) in vec4 iPosition;
layout (location = 8) in vec2 iUV;
layout (location = 8) out vec2 oUV;


void main()
{
gl_Position = cWorldViewProj * iPosition;
oUV = iUV;

}


