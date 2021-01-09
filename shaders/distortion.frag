#version 330 core  
#extension GL_ARB_separate_shader_objects: enable
#extension GL_ARB_texture_rectangle: enable

uniform sampler2D myTexture;
in vec2 out_UV;
out vec4 color;

void main (void)  
{  
  color = texture(myTexture, out_UV);        
}    
