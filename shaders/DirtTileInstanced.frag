#version 330  core
#extension GL_ARB_shading_language_include : enable
#include "LocalLighting.glsl"


uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

uniform vec4 ambientLightColour;
uniform vec4 cameraPosition;
uniform vec4 diffuseSurface;
uniform bool shadowingEnabled;
in vec2 out_UV0;
in vec3 FragPos;
in vec4 VertexPos; 
in vec4 outputColor; 
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  
    vec3 texelColor;
    texelColor = texture(decalmap, outputColor.a > 0.0 ? FragPos.xy : out_UV0.st).rgb;
    // The fog is flat and unlit on purpose: shading it would show the relief of the unexplored
    // tiles below it. It shows the same earth texture and the same colour as explored earth
    // without torch light (DirtTile.frag with no local lights: ambient term, then the colour
    // grade), so unexplored and explored earth only differ where torches shine.
    texelColor = enhanceDungeonColour(ambientLightColour.rgb * texelColor);
    // Blend the player's mark over this same neutral fog surface.
    texelColor = mix(texelColor, outputColor.rgb, outputColor.a);
    color = vec4(texelColor, 1.0);
}    

