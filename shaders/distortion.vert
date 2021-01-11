#version 330 core
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

layout(location = 0) in vec4 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent; 

layout(location = 8) in vec2 uv_0;
//in vec2 uv_0;
layout(location = 9) in vec2 uv_1;
out vec2 out_UV0;
out vec2 out_UV1;
 
out vec3 FragPos;  

uniform    mat4 worldMatrix;
uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;
uniform    mat4 modelMatrix;

out VS_OUT {
    mat3 TBN;
} vs_out;  

float freq = 3.14;
void main()
{
    vec4 worldPos = worldMatrix * aPos;
    worldPos.z *= (1 + sin((worldPos.x + 2*worldPos.y ) * freq)/8.0);
    vec3 T = normalize(vec3(worldMatrix * vec4(aTangent,   0.0)));
    vec3 B = normalize(vec3(worldMatrix * vec4(aBitangent, 0.0)));
    vec3 N = normalize(vec3(worldMatrix * vec4(aNormal,    0.0)));    
    gl_Position = projectionMatrix * viewMatrix * vec4(worldPos.xyz, 1.0);
    FragPos = worldPos.xyz;
    mat3 TBN = mat3(T, B, N);
    out_UV0 = uv_0;
    out_UV1 = uv_1;
}  
