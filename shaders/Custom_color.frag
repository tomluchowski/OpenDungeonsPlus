#version 330  core

uniform vec4 color;
uniform vec4 ambient;

out vec4 cc;
void main(void)
{


cc = 0.01* ambient + 0.99*color;





}
