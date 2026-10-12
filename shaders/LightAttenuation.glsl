uniform vec4 lightAttenuation;

float getLightAttenuation(vec4 lightPosition, vec3 surfacePosition)
{
    // Directional lights have no distance falloff.
    if(lightPosition.w == 0.0)
        return 1.0;

    float distance = length(lightPosition.xyz - surfacePosition);
    if(distance > lightAttenuation.x)
        return 0.0;

    return 1.0 / dot(lightAttenuation.yzw, vec3(1.0, distance, distance * distance));
}
