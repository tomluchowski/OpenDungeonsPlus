#version 330  core


uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour; 
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
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
    if(outputColor.a == 0.0)
	    texelColor = texture(decalmap, out_UV0.st).rgb;
    else
    	texelColor = outputColor.rgb;
    // The fog is flat and unlit on purpose: shading it would show the relief of the unexplored
    // tiles below it. A digging mark (alpha 1.0) is set by the player and keeps its full colour.
    if(outputColor.a == 0.0)
        texelColor *= 0.3;
    color = vec4(texelColor, 1.0);
}    

