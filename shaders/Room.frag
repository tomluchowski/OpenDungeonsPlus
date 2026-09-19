#version 330  core
#define epsilon 0.00001

uniform sampler2D decalmap;
uniform sampler2D normalmap;
uniform sampler2D shadowmap;

#ifdef DORMITORY_FLOOR
uniform sampler2D dormitoryEdge;
uniform sampler2D dormitoryCorner;
// UV boundaries: U=0, V=0, U=1, V=1; one denotes an exposed room edge.
uniform vec4 dormitoryBorders;

vec3 dormitoryFloor(vec2 uv)
{
    bool right = uv.x > 0.5;
    bool top = uv.y < 0.5;
    bool side = (right ? dormitoryBorders.z : dormitoryBorders.x) > 0.5;
    bool end = (top ? dormitoryBorders.y : dormitoryBorders.w) > 0.5;
    if(side && end)
        return texture(dormitoryCorner, vec2(right ? uv.x : 1.0 - uv.x,
            top ? uv.y : 1.0 - uv.y)).rgb;
    if(side)
        return texture(dormitoryEdge, vec2(uv.y, right ? uv.x : 1.0 - uv.x)).rgb;
    if(end)
        return texture(dormitoryEdge, vec2(uv.x, top ? 1.0 - uv.y : uv.y)).rgb;
    return texture(decalmap, uv).rgb;
}
#endif

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour; 
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
uniform bool shadowingEnabled;
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
    vec3 lightingTerm =  (diffuse + specular + ambientLightColour.rgb/2.0 )*shadow.rgb;
    vec3 texelColor = texture(decalmap, out_UV0.st).rgb;
#ifdef DORMITORY_FLOOR
    texelColor = dormitoryFloor(out_UV0.st);
#endif
    result =  lightingTerm * texelColor;

    color  = vec4(result.xyz,  1.0);

       
}    

