#include "LightAttenuation.glsl"

// Match Ogre's default maximum number of lights per material pass. The
// "... 8" counts in the light parameters of the materials use the same value.
#define MAX_LOCAL_LIGHTS 8
// Exponent of the specular highlight.
#define SPECULAR_SHININESS 16.0

uniform float lightCount;
uniform vec4 lightDiffuseColour[MAX_LOCAL_LIGHTS];
uniform vec4 lightSpecularColour[MAX_LOCAL_LIGHTS];
uniform vec4 lightPos[MAX_LOCAL_LIGHTS];
uniform vec4 lightAttenuation[MAX_LOCAL_LIGHTS];
uniform float firstLightCastsShadows;

vec3 getLocalLighting(vec3 position, vec3 normal, vec3 camera, float shadow)
{
    vec3 lighting = vec3(0.0);
    vec3 viewDirection = normalize(camera - position);
    for(int i = 0; i < min(int(lightCount), MAX_LOCAL_LIGHTS); ++i)
    {
        vec3 lightDirection = normalize(lightPos[i].xyz - position * lightPos[i].w);
        float diffuse = max(dot(lightDirection, normal), 0.0);
        float specular = pow(max(dot(reflect(-lightDirection, normal), viewDirection), 0.0), SPECULAR_SHININESS);
        // The existing single shadow texture belongs to the first shadow light.
        float visibility = i == 0 && firstLightCastsShadows > 0.5 ? shadow : 1.0;
        lighting += (diffuse * lightDiffuseColour[i].rgb + specular * lightSpecularColour[i].rgb)
            * getLightAttenuation(lightPos[i], position, lightAttenuation[i]) * visibility;
    }
    return lighting;
}
