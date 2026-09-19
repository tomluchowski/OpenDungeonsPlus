#version 330 core

layout(location = 0) in vec4 vertex;
layout(location = 8) in vec2 uv0;
out vec2 atmosphereUV;

void main()
{
    gl_Position = vertex;
    atmosphereUV = uv0;
}
