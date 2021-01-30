#version 330  core


uniform sampler2D decalmap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour; 
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
in vec2 out_UV0;
in vec3 FragPos;
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb; 
    
    vec3 result =   texelColor;
    color = vec4(result.xyz,  1.0);
       
}   
