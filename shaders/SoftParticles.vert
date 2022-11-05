#version 330  core
#extension GL_ARB_separate_shader_objects : enable

uniform mat4x4 modelViewProj;
uniform mat4x4 worldView;

layout (location = 0) in vec4 position ;
layout (location = 8) in vec2 iUV ;
layout (location = 3) in vec4 vertexColour;

layout (location = 0 ) out vec4 oPosition ;
layout (location = 8 ) out	vec3 oVertexPos;
layout (location = 9 ) out	vec2 oUV;
layout (location = 10 ) out	vec4 oVertexColour;
layout (location = 11 ) out	vec3 oViewPosition;



void main(void)
{
gl_Position = modelViewProj *position;
oPosition = modelViewProj *position;
oVertexPos = position.xyz;
oUV = iUV;
oViewPosition = (worldView * position).xyz;
oVertexColour = vertexColour;

}
