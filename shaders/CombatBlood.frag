#version 330 core

uniform sampler2D flareMap;
in vec2 impactUV;
in vec4 impactColour;
out vec4 fragColour;

void main()
{
    // Flare's black background is opaque in its alpha channel; its brightness
    // supplies the droplet mask instead of colouring a rectangular billboard.
    float mask = texture(flareMap, impactUV).r;
    fragColour = vec4(impactColour.rgb, impactColour.a * mask);
}
