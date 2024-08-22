#version 460
//-----------------------------------------------------------------------------
//                         PROGRAM DEPENDENCIES
//-----------------------------------------------------------------------------
#define USE_OGRE_FROM_FUTURE
#include <OgreUnifiedShader.h>
#include "FFPLib_Transform.glsl"
#include "SGXLib_PerPixelLighting.glsl"
#include "FFPLib_Texturing.glsl"

//-----------------------------------------------------------------------------
//                         GLOBAL PARAMETERS
//-----------------------------------------------------------------------------

layout(location = 0) uniform	mat4	worldviewproj_matrix;
layout(location = 4) uniform	mat3	normal_matrix;
layout(location = 7) uniform	mat4	worldview_matrix;

//-----------------------------------------------------------------------------
//                         MAIN
//-----------------------------------------------------------------------------
IN(vec4	vertex, POSITION)
IN(vec3	normal, NORMAL)
OUT(vec3	iTexcoord_0, 0)
OUT(vec3	iTexcoord_1, 1)
OUT(vec2	iTexcoord_2, 2)
void main(void) {
	vec4	lColor_0;
	vec4	lColor_1;

	FFP_Transform(worldviewproj_matrix, vertex, gl_Position);
	lColor_0	=	vec4(1.00000,1.00000,1.00000,1.00000);
	lColor_1	=	vec4(0.00000,0.00000,0.00000,0.00000);
	FFP_Transform(normal_matrix, normal, iTexcoord_0);
	FFP_Transform(worldview_matrix, vertex, iTexcoord_1);
	FFP_GenerateTexCoord_EnvMap_Sphere(worldview_matrix, normal_matrix, vertex, normal, iTexcoord_2);
}

