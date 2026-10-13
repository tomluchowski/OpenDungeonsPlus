#version 330  core
#define epsilon 0.00001

uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour; 
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
uniform vec3 ambient;
uniform bool shadowingEnabled;
uniform float corpseDecay = 0.0;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;
in vec4 VertexPos; 
in mat3 TBN;
 
out vec4 color;

void main (void)  
{  

    // compute Normal
    vec3 Normal = texture(normalmap, out_UV1.st).rgb;
    Normal.xyz = 2 * Normal.xyz - (1.0,1.0,1.0);
    Normal =  normalize(TBN * Normal); 
    
    vec4 shadow = vec4(1.0, 1.0, 1.0,1.0);
    vec4 tmpVertexPos = VertexPos;
    
    // compute shadowmap
    if(shadowingEnabled){
		if(tmpVertexPos.z > epsilon ){
		    tmpVertexPos /= tmpVertexPos.w;
		    shadow = texture(shadowmap, tmpVertexPos.xy); 
		}
    }
    
    
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
    vec3 lightingTerm =  (diffuse + specular + ambientLightColour.rgb * ambient )*shadow.rgb;
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
    if(corpseDecay > 0.0)
    {
        // Stable surface patches change only on corpses with a private material.
        vec2 cell = floor(out_UV0 * 48.0);
        float grain = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
        float rot = smoothstep(grain * 0.35, 0.55 + grain * 0.25, corpseDecay);
        float grey = dot(texelColor, vec3(0.299, 0.587, 0.114));
        texelColor = mix(texelColor, grey * vec3(0.48, 0.43, 0.28), rot);
        float loss = smoothstep(0.45, 1.0, corpseDecay);
        if(loss > grain)
            discard;
    }
    result =  lightingTerm * texelColor;

    color  = vec4(result.xyz,  1.0);

       
}    

