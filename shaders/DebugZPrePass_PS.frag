#version 330  core
#extension GL_ARB_separate_shader_objects : enable

uniform sampler2D ZPrePassMap;
layout (location = 8) in vec2 uv;
out vec4 color;

void main()
{
float val = texture(ZPrePassMap, uv.st).r * 5;
color = vec4(val, val, val, 1.0);
	
}
