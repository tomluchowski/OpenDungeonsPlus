#version 330  core
#extension GL_ARB_separate_shader_objects : enable

uniform sampler2D DiffuseMap;
uniform sampler2D ZPrePassMap;




uniform float fadeDistance;
//uniform vec3 eyePosition;
uniform float nearClipDistance;
uniform float farClipDistance;
uniform float inverseViewportWidth;
uniform float inverseViewportHeight;
uniform float renderTargetFlipping;


layout (location = 8) in vec3 position;
layout (location = 9) in vec2 uv;
layout (location = 10)	in vec4 vertexColour;
layout (location = 11)	in vec3 viewPosition;
layout (location = 0) in vec4 screenPosition;
				
out vec4 color;
void main(void)
{
	const vec2 renderSystemTexelOffset = vec2(0.5, 0.5);
	vec2 zPrePassUV = vec2((screenPosition.x + renderSystemTexelOffset.x) * inverseViewportWidth,
							   (screenPosition.y + renderSystemTexelOffset.y) * inverseViewportHeight);
	zPrePassUV.y = 1.0 - zPrePassUV.y;
	zPrePassUV.y = (1.0 - clamp(renderTargetFlipping, 0.0, 1.0)) + renderTargetFlipping * zPrePassUV.y;

float pixelPositionDepth = length(viewPosition) - nearClipDistance;

float clipDistance = farClipDistance - nearClipDistance; // Convert it to world space units instead of 0-1
float zPrePassPositionDepth = texture(ZPrePassMap, zPrePassUV).r * clipDistance;

float softMultiplier = 1.0;
float distance = abs(pixelPositionDepth - zPrePassPositionDepth);
if(distance < fadeDistance)
	softMultiplier = distance / fadeDistance;

color.rgba = texture(DiffuseMap, uv.st) * vertexColour;
color.xyz *= softMultiplier;


}
