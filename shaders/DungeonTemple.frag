#version 330 core

uniform sampler2D decalMap;
uniform sampler2D envMap;

uniform vec4 ambientLightColour;
uniform vec4 lightDiffuseColour;
uniform vec4 lightPos;
uniform vec4 cameraPosition;
uniform vec4 surfaceAmbient;
uniform vec4 surfaceDiffuse;
uniform vec4 surfaceEmissive;

in vec3 out_FragPos;
in vec3 out_Normal;
in vec2 out_UV0;

out vec4 color;

void main() {
    vec3 N = normalize(out_Normal);

    // Fixed-function lighting from the pass's surface colours.
    vec3 L = normalize(lightPos.xyz - out_FragPos * lightPos.w);
    float diff = max(dot(L, N), 0.0);
    vec3 lit = surfaceEmissive.rgb
             + surfaceAmbient.rgb * ambientLightColour.rgb
             + surfaceDiffuse.rgb * diff * lightDiffuseColour.rgb;

    // First texture unit used "colour_op alpha_blend": the decal is blended
    // over the lit colour by its own alpha.
    vec4 decal = texture(decalMap, out_UV0);
    vec3 c = mix(lit, decal.rgb, decal.a);

    // Second texture unit was a spherical environment map blended with
    // "colour_op_ex blend_manual src_texture src_current 0.3"; both
    // directives were removed in Ogre 14.x, so reproduce them here (same
    // formula as ReflMetal.frag).
    vec3 V = normalize(cameraPosition.xyz - out_FragPos);
    vec3 R = reflect(-V, N);
    float m = 2.0 * sqrt(R.x * R.x + R.y * R.y + (R.z + 1.0) * (R.z + 1.0));
    vec2 envUV = R.xy / m + 0.5;
    vec3 reflection = texture(envMap, envUV).rgb;

    color = vec4(mix(c, reflection, 0.3), 1.0);
}
