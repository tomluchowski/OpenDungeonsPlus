#version 120 

attribute vec4 vertex;
 in vec4 aPos;

varying vec2 vTexCoord;
 
uniform    mat4 worldMatrix;
uniform    mat4 projectionMatrix;
uniform    mat4 viewMatrix;


float freq = 3.14;
void main()
{
    vec4 worldPos = worldMatrix * aPos;
    worldPos.z += sin((worldPos.x + 2*worldPos.y ) * freq)/4.0;
    gl_Position = projectionMatrix * viewMatrix * vec4(worldPos.xyz, 1.0);
    vTexCoord = gl_MultiTexCoord0.xy;

}  
