float getLightAttenuation(vec4 lightPosition, vec3 surfacePosition, vec4 attenuation)
{
    // Directional lights have no distance falloff.
    if(lightPosition.w == 0.0)
        return 1.0;

    float distance = length(lightPosition.xyz - surfacePosition);
    if(distance > attenuation.x)
        return 0.0;

    return 1.0 / dot(attenuation.yzw, vec3(1.0, distance, distance * distance));
}
