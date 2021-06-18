#version 330  core


uniform sampler2D decalmap;
uniform sampler2D normalmap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour; 
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
uniform vec4 diffuseSurface;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    
    // compute lightDir
    vec3 lightDir =  normalize(lightPos.xyz - FragPos*lightPos.w);
    
    
    // compute Specular
    vec3 viewDirection =  normalize( cameraPosition.xyz - FragPos);
    vec3 reflectedLightDirection =  normalize(reflect(-1.0*lightDir.xyz,Normal));
    float spec =  max(dot(reflectedLightDirection, viewDirection ), 0.0) ;
    spec = pow(spec,16);    
    vec3 specular = spec * lightSpecularColour.rgb; 
    
    
    // compute Diffuse
    float diff = max(dot(lightDir,Normal), 0.0);
    vec3 diffuse = diff * lightDiffuseColour.rgb;
    vec3 result;
    
    
    // precompute the lighting term
    vec3 lightingTerm =  (diffuse + specular + ambientLightColour.rgb/2.0 );
    
    if(diffuseSurface.rgb != vec3(1.0,1.0,1.0))
        result =  lightingTerm * mix(texelColor, diffuseSurface.rgb,0.5);
    else
        result =  lightingTerm * texelColor;
    color = vec4(result.xyz,  1.0);
       
}    
