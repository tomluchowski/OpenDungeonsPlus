#include "LightAttenuation.glsl"

// Match Ogre's default maximum number of lights per material pass.
uniform float lightCount;
uniform vec4 lightDiffuseColour[8];
uniform vec4 lightSpecularColour[8];
uniform vec4 lightPos[8];
uniform vec4 lightAttenuation[8];
uniform float firstLightCastsShadows;

vec3 getLocalLighting(vec3 position, vec3 normal, vec3 camera, float shadow)
{
    vec3 lighting = vec3(0.0);
    vec3 viewDirection = normalize(camera - position);
    for(int i = 0; i < min(int(lightCount), 8); ++i)
    {
        vec3 lightDirection = normalize(lightPos[i].xyz - position * lightPos[i].w);
        float diffuse = max(dot(lightDirection, normal), 0.0);
        float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), 16.0);
        // The existing single shadow texture belongs to the first shadow light.
        float visibility = i == 0 && firstLightCastsShadows > 0.5 ? shadow : 1.0;
        lighting += (diffuse * lightDiffuseColour[i].rgb + specular * lightSpecularColour[i].rgb)
            * getLightAttenuation(lightPos[i], position, lightAttenuation[i]) * visibility;
    }
    return lighting;
}
