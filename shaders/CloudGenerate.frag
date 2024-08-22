#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform float time;
uniform float time_dillation;
uniform vec2 resolution;
uniform vec3 cloud_color;
uniform float persistence;
uniform float lacunarity;
uniform int octaves;


// Include the Perlin noise function
#include "PerlinNoise.glsl"

// Function to generate cloud patterns using Perlin noise
float generateCloud(vec2 uv) {

    
    float noiseValue = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    
    for(int i = 0; i < octaves; i++) {
        noiseValue += amplitude * perlin(uv.x * frequency,uv.y *frequency);
        uv *= lacunarity;
        amplitude *= persistence;
    }
    
    return noiseValue;
}

void main() {
    vec2 uv = TexCoords * resolution.xy / resolution.y;
    
    // Animate the clouds by adding time to the UV coordinates
    uv += time_dillation*time * 0.05;
    
    // Generate the cloud pattern
    float cloudPattern = generateCloud(uv);
    
    // Threshold to create clouds and sky
    float threshold = 0.5;
    vec4 cloudColor = vec4(cloud_color,1.0); // White clouds
    vec4 skyColor = vec4(0.4, 0.5, 0.6,0.3); // Light blue sky
    
    // Mix cloud and sky colors based on the cloud pattern
    vec4 color = mix(skyColor, cloudColor, cloudPattern);
    
    FragColor = color;
}
