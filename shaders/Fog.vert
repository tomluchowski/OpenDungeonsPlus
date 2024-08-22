#version 330 core
#extension GL_ARB_explicit_uniform_location : enable
#extension GL_ARB_shading_language_include : enable


#define USE_OGRE_FROM_FUTURE
#include <OgreUnifiedShader.h>
#include "FFPLib_Transform.glsl" 






layout(location = 0) in vec4 position;
layout(location = 8) in vec2 texCoord;
IN(mat3x4	uv1, TEXCOORD1)
out vec2 TexCoords;

// uniform float time;
uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 worldMatrix;

void main()
{

    // compute world space position, tangent, bitangent
    vec4 local_position = position;
    FFP_Transform(uv1, local_position, local_position.xyz);
    
    TexCoords.st = (worldMatrix *vec4(local_position.xyz,1.0)).xy/4 ;// + vec2(sin(time),cos(time));
    gl_Position = projectionMatrix * viewMatrix * worldMatrix * vec4(local_position.xyz,1.0);
}
