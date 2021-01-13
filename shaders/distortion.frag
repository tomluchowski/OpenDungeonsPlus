#version 330  core


uniform sampler2D decalmap;
uniform sampler2D normalmap;

uniform vec4 surfaceAmbient;
uniform vec4 lightColour; 
uniform vec4 lightPos;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;

    Normal.xy = 2 * Normal.xy - (1.0,1.0);
    Normal = normalize(TBN * Normal); 
          
    vec3 lightDir = normalize(lightPos.xyz - FragPos); 
    float diff = max(dot(Normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColour.rgb; 
    vec3 result = ( diffuse + surfaceAmbient.rgb) *texelColor ;
    color = vec4(result.xyz, 1.0);
       
}    
