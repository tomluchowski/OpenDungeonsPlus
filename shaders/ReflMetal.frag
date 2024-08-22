#version 460
//-----------------------------------------------------------------------------
//                         PROGRAM DEPENDENCIES
//-----------------------------------------------------------------------------
#define USE_OGRE_FROM_FUTURE
#include <OgreUnifiedShader.h>
#include "SGXLib_PerPixelLighting.glsl"
#include "FFPLib_Texturing.glsl"

//-----------------------------------------------------------------------------
//                         GLOBAL PARAMETERS
//-----------------------------------------------------------------------------

SAMPLER2D(gTextureSampler0, 0);
layout(location = 0) uniform	vec4	derived_ambient_light_colour;
layout(location = 1) uniform	vec4	surface_emissive_colour;
layout(location = 2) uniform	vec4	derived_scene_colour;
layout(location = 3) uniform	float	surface_shininess;
layout(location = 4) uniform	vec4	light_position_view_space;
layout(location = 5) uniform	vec4	light_attenuation;
layout(location = 6) uniform	vec4	derived_light_diffuse_colour;
layout(location = 7) uniform	vec4	derived_light_specular_colour;

//-----------------------------------------------------------------------------
//                         MAIN
//-----------------------------------------------------------------------------
uniform vec3 ambient;
IN(vec3	iTexcoord_0, 0)
IN(vec3	iTexcoord_1, 1)
IN(vec2	iTexcoord_2, 2)
void main(void) {
	vec4	lColor_0;
	vec4	lColor_1;
	vec4	texel_0;

	lColor_0	=	vec4(1.00000,1.00000,1.00000,1.00000);
	lColor_1	=	vec4(0.00000,0.00000,0.00000,0.00000);
	gl_FragColor	=	lColor_0;
	gl_FragColor	=	derived_scene_colour;
	SGX_Light_Point_DiffuseSpecular(iTexcoord_0, iTexcoord_1, light_position_view_space.xyz, light_attenuation, derived_light_diffuse_colour.xyz, derived_light_specular_colour.xyz, surface_shininess, gl_FragColor.xyz, lColor_1.xyz);
	lColor_0	=	gl_FragColor;
	texel_0	=	texture2D(gTextureSampler0, iTexcoord_2);
	texel_0	=	texture2D(gTextureSampler0, iTexcoord_2);
	gl_FragColor.xyz	=	mix(gl_FragColor.xyz, texel_0.xyz, 0.500000);
	gl_FragColor.w	=	texel_0.w*gl_FragColor.w;
	gl_FragColor.xyz	=	ambient*(gl_FragColor.xyz+lColor_1.xyz);
}

