#version 330 core

in vec2 atmosphereUV;
out vec4 fragColour;
uniform vec4 tint;
uniform float time;
uniform float mist;

float noise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec4 h = fract(sin(vec4(dot(cell, vec2(127.1, 311.7)),
        dot(cell + vec2(1, 0), vec2(127.1, 311.7)),
        dot(cell + vec2(0, 1), vec2(127.1, 311.7)),
        dot(cell + vec2(1, 1), vec2(127.1, 311.7)))) * 43758.5453);
    return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y);
}

float wisps(vec2 p)
{
    return 0.57 * noise(p) + 0.28 * noise(p * 2.03 + 4.7)
        + 0.15 * noise(p * 4.11 + 8.2);
}

void main()
{
    vec2 uv = atmosphereUV;
    vec2 centered = uv * 2.0 - 1.0;
    float mask;
    if(mist > 0.5)
    {
        // Continuous advection and domain warping, with a feathered boundary.
        vec2 p = uv * vec2(5.8, 3.0) + vec2(-time * 0.11, time * 0.018);
        float warp = wisps(p * 0.7 + vec2(time * 0.035, 0.0));
        float density = wisps(p + vec2(warp * 1.6, warp * 0.75));
        float edges = smoothstep(0.0, 0.18, uv.x)
            * smoothstep(0.0, 0.18, 1.0 - uv.x)
            * smoothstep(0.0, 0.30, uv.y)
            * smoothstep(0.0, 0.30, 1.0 - uv.y);
        mask = edges * smoothstep(0.26, 0.78, density);
    }
    else
    {
        float radius = dot(centered, centered);
        mask = exp(-radius * 4.5) * (1.0 - smoothstep(0.55, 1.0, radius));
    }
    fragColour = vec4(tint.rgb, tint.a * mask);
}
