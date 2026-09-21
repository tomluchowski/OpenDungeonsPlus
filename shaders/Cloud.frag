#version 330 core

out vec4 FragColor;
// Matches the out_UV0 the cloud vertex shader actually writes; the old
// "TexCoords" name matched nothing and left the coordinates undefined.
in vec2 out_UV0;

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
    vec2 uv = out_UV0 * resolution.xy / resolution.y;
    
    // Animate the clouds by adding time to the UV coordinates
    uv += time_dillation*time * 0.05;
    
    // Generate the cloud pattern
    float cloudPattern = generateCloud(uv);
    
    // Threshold to create clouds and sky
    float threshold = 0.5;
    vec4 cloudColor = vec4(cloud_color,1.0); // White clouds
    vec4 skyColor = vec4(0.4, 0.6, 1.0,0.1); // Light blue sky
    
    // Mix cloud and sky colors based on the cloud pattern
    vec4 color = mix(skyColor, cloudColor, cloudPattern);

    // Each cloud quad is drawn half a tile wider than its tile and faded out
    // over its outer third, so the fades of neighbouring fog tiles overlap
    // into one continuous blanket instead of a grid of separate lumps.
    vec2 border = min(out_UV0, vec2(1.0) - out_UV0);
    float edge = smoothstep(0.0, 1.0 / 3.0, min(border.x, border.y));
    color.a *= edge;

    FragColor = color;
}
