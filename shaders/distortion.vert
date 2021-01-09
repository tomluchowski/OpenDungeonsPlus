#version 330 core
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

layout(location = 0) in vec4 aPos;
layout(location = 8) in vec2 uv_0;
//in vec2 uv_0;
out vec2 out_UV;
 
uniform    mat4 worldMatrix;
uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;


float freq = 3.14;
void main()
{
    vec4 worldPos = worldMatrix * aPos;
    worldPos.z *= (1 + sin((worldPos.x + 2*worldPos.y ) * freq)/8.0);
    gl_Position = projectionMatrix * viewMatrix * vec4(worldPos.xyz, 1.0);
    out_UV = uv_0;
}  
