#version 330 core

uniform sampler2D envmap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour;
uniform vec4 lightSpecularColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
uniform vec3 ambient;

in vec3 out_FragPos;
in vec3 out_Normal;

out vec4 color;

void main() {
    vec3 N = normalize(out_Normal);

    // Sphere-mapped environment reflection (replaces the legacy
    // fixed-function "env_map spherical" removed in Ogre 14.x).
    vec3 V = normalize(cameraPosition.xyz - out_FragPos);
    vec3 R = reflect(-V, N);
    float m = 2.0 * sqrt(R.x * R.x + R.y * R.y + (R.z + 1.0) * (R.z + 1.0));
    vec2 envUV = R.xy / m + 0.5;
    vec3 reflection = texture(envmap, envUV).rgb;

    // Point light diffuse + specular (same convention as the other OD shaders).
    vec3 L = normalize(lightPos.xyz - out_FragPos * lightPos.w);
    float diff = max(dot(L, N), 0.0);
    vec3 diffuse = diff * lightDiffuseColour.rgb;
    vec3 reflectedLight = normalize(reflect(-L, N));
    float spec = pow(max(dot(reflectedLight, V), 0.0), 16.0);
    vec3 specular = spec * lightSpecularColour.rgb;
    vec3 lighting = diffuse + specular + ambientLightColour.rgb * ambient;

    // 50% blend between lighting and reflection (matches the old
    // "colour_op_ex blend_manual src_texture src_current 0.5").
    color = vec4(mix(lighting, reflection, 0.5), 1.0);
}
