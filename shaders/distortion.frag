#version 330 core  
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

uniform sampler2D myTexture;
uniform sampler2D m_NormalMap;
uniform vec4 surfaceAmbient;
uniform vec4 lightColour; 
uniform vec4 lightPos;
in vec2 out_UV0;
in vec2 out_UV1;
in vec4 Normal;
in vec4 FragPos;
out vec4 color;

void main (void)  
{  
    vec4 texelColor = texture(myTexture, out_UV0);
    vec4 Normal = texture(m_NormalMap, out_UV1);

    vec4 norm = normalize(Normal);
    vec4 lightDir = normalize(lightPos - FragPos); 
    float diff = max(dot(norm, lightDir), 0.0);
    vec4 diffuse = diff * lightColour; 
    vec4 result = (diffuse + surfaceAmbient ) * texelColor;
    color = vec4(result.xyz, 1.0);
       
}    
