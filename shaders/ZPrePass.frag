#version 330  core
#extension GL_ARB_separate_shader_objects : enable
uniform float cNearClipDistance;
uniform float cFarClipDistance;
in vec2 iUV;
layout (location = 9 )  in vec3 iViewPos;
out vec4 color;

void main()
{
	float clipDistance = cFarClipDistance - cNearClipDistance;
	float intensity = (length(iViewPos) - cNearClipDistance) / clipDistance;
	color = vec4(intensity,intensity,intensity,1.0);
}
