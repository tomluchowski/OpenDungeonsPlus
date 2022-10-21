#version 330  core
#extension GL_ARB_separate_shader_objects : enable
uniform mat4 cWorldViewProj;
uniform mat4 cWorldView;

layout (location = 0) in vec4 iPosition;
layout (location = 9) in vec2 iUV;

layout (location = 8) out vec2 oUV;
layout (location = 9) out vec3 oViewPos;

void main()
{
	gl_Position = cWorldViewProj * iPosition;
	oViewPos = (cWorldView * iPosition).xyz;
	oUV = iUV;
}


