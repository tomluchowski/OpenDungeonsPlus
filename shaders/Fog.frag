#version 330 core

in vec2 TexCoord; // Texture coordinates from the vertex shader
out vec4 FragColor; // Final color output

uniform sampler2D textureSampler; // The texture sampler

void main()
{
    // Sample the texture at the given texture coordinates
    FragColor = texture(textureSampler, TexCoord.st);
}
