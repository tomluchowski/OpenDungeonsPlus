#version 330 core

uniform float fireTime;
in vec2 impactUV;
in vec4 impactColour;
out vec4 fragColour;

float noise(vec2 p)
{
    vec2 cell = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec4 h = fract(sin(vec4(dot(cell, vec2(127.1, 311.7)),
        dot(cell + vec2(1, 0), vec2(127.1, 311.7)),
        dot(cell + vec2(0, 1), vec2(127.1, 311.7)),
        dot(cell + vec2(1, 1), vec2(127.1, 311.7)))) * 43758.5453);
    return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y);
}

void main()
{
    vec2 p = impactUV * 2.0 - 1.0;
    vec2 flow = p * 4.0 + vec2(fireTime * 1.7, -fireTime * 3.0);
    float turbulence = noise(flow) * 0.65 + noise(flow * 2.1) * 0.35;
    float radius = length(p);
    float edge = 0.76 + turbulence * 0.19;
    float mask = 1.0 - smoothstep(edge - 0.18, edge, radius);
    // A shaded hot core and broken orange rim remain coloured when overlapping;
    // alpha blending avoids the old additive stack saturating to white.
    float core = clamp(1.0 - length(p + vec2(0.12, 0.1)) * 1.4, 0.0, 1.0);
    float heat = clamp(core * 0.75 + turbulence * 0.35, 0.0, 1.0);
    vec3 fire = mix(vec3(0.72, 0.035, 0.003), vec3(1.0, 0.7, 0.06), heat);
    fragColour = vec4(fire * impactColour.rgb, mask * impactColour.a);
}
