#version 330 core

uniform float wingTime;
in vec2 impactUV;
in vec4 impactColour;
out vec4 fragColour;

float ellipse(vec2 p, vec2 center, vec2 radius)
{
    vec2 q = (p - center) / radius;
    return 1.0 - smoothstep(0.65, 1.0, dot(q, q));
}

void main()
{
    vec2 p = impactUV - 0.5;
    float spread = 0.16 + 0.07 * sin(wingTime * 6.2831853 * 24.0);
    float wings = max(ellipse(p, vec2(-spread, 0.03), vec2(0.22, 0.13)),
                      ellipse(p, vec2(spread, 0.03), vec2(0.22, 0.13)));
    float body = max(ellipse(p, vec2(0.0, -0.04), vec2(0.105, 0.26)),
                     ellipse(p, vec2(0.0, 0.22), vec2(0.12, 0.12)));
    vec3 colour = mix(vec3(0.64, 0.61, 0.52), vec3(0.055, 0.04, 0.025), body);
    fragColour = vec4(colour, max(body, wings * 0.75) * impactColour.a);
}
