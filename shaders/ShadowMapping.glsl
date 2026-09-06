// Shadow texture coordinates have biased X/Y and OpenGL clip-space Z.
float sampleShadow(sampler2D shadowMap, vec4 lightPosition)
{
    if(lightPosition.w <= 0.0)
        return 1.0;

    vec3 shadowPosition = lightPosition.xyz / lightPosition.w;
    shadowPosition.z = shadowPosition.z * 0.5 + 0.5;
    if(any(lessThan(shadowPosition, vec3(0.0))) || any(greaterThan(shadowPosition, vec3(1.0))))
        return 1.0;

    // Allow one depth-buffer step for the 16-bit shadow texture.
    return step(shadowPosition.z - 1.0 / 65535.0, texture(shadowMap, shadowPosition.xy).r);
}
