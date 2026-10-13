#version 330 core

in vec2 featherUV;
in vec4 featherColour;
out vec4 fragColour;

void main()
{
    float y = featherUV.y;
    float spine = 0.12 * sin(y * 3.141593);
    float distanceFromSpine = abs(featherUV.x * 2.0 - 1.0 - spine);
    float vane = 0.82 * pow(max(0.0, sin(clamp((y - 0.08) / 0.9, 0.0, 1.0) * 3.141593)), 0.7);
    float edge = max(fwidth(distanceFromSpine), 0.012);
    float alpha = 1.0 - smoothstep(vane - edge, vane + edge, distanceFromSpine);
    alpha *= smoothstep(0.02, 0.1, y) * (1.0 - smoothstep(0.96, 1.0, y));
    float quill = (1.0 - smoothstep(0.025, 0.045, distanceFromSpine)) * step(0.03, y) * step(y, 0.9);
    alpha = max(alpha, quill) * featherColour.a;
    float barbs = 0.9 + 0.1 * sin(y * 90.0 + distanceFromSpine * 12.0);
    fragColour = vec4(featherColour.rgb * mix(barbs, 1.0, quill), alpha);
}
