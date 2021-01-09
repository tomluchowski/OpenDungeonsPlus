#version 330 core  
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

uniform sampler2D myTexture;
uniform vec4 surfaceAmbient; 
in vec2 out_UV;
out vec4 color;

void main (void)  
{  
    vec4 texelColor = texture(myTexture, out_UV); 
    vec4 result = surfaceAmbient * texelColor;
    color = vec4(result.xyz, 1.0);
       
}    
