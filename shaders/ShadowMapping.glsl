// One step of the 24-bit shadow depth buffer.
const float SHADOW_DEPTH_STEP = 1.0 / 16777215.0;

// Shadow texture coordinates have biased X/Y and OpenGL clip-space Z.
float sampleShadow(sampler2D shadowMap, vec4 lightPosition)
{
    if(lightPosition.w <= 0.0)
        return 1.0;

    vec3 shadowPosition = lightPosition.xyz / lightPosition.w;
    shadowPosition.z = shadowPosition.z * 0.5 + 0.5;
    if(any(lessThan(shadowPosition, vec3(0.0))) || any(greaterThan(shadowPosition, vec3(1.0))))
        return 1.0;

    // Allow one depth-buffer step for the 24-bit shadow texture.
    return step(shadowPosition.z - SHADOW_DEPTH_STEP, texture(shadowMap, shadowPosition.xy).r);
}
