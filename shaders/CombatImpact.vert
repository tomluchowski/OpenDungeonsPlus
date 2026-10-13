#version 330 core

uniform mat4 worldViewProj;
layout(location = 0) in vec4 vertex;
layout(location = 3) in vec4 colour;
layout(location = 8) in vec2 uv0;
out vec2 impactUV;
out vec4 impactColour;

void main()
{
    gl_Position = worldViewProj * vertex;
    impactUV = uv0;
    impactColour = colour;
}
