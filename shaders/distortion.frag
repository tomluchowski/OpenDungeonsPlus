#version 330 core  
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

uniform sampler2D myTexture;
uniform sampler2D m_NormalMap;
uniform vec4 surfaceAmbient;
uniform vec3 lightColour; 
uniform vec3 lightPos;
in vec2 out_UV0;
in vec2 out_UV1;
in vec3 FragPos;  
out vec4 color;

in VS_OUT {
    mat3 TBN;
} fs_in;

void main (void)  
{  
    vec4 texelColor = texture(myTexture, out_UV0);
    vec4 Normal = texture(m_NormalMap, out_UV1);
    vec3 norm = Normal.xyz;
    norm.xy = 2 * Normal.xy - (1.0,1.0);
    norm = normalize(fs_in.TBN * norm); 
    
    vec3 lightDir = normalize(lightPos - FragPos); 
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColour; 
    vec3 result = (diffuse /* + surfaceAmbient  */) * texelColor.rgb;
    color = vec4(result.xyz, 1.0);
       
}    
