#version 330  core

// A part painted in the owning seat's colour, with a texture over it: where
// the texture is opaque it shows itself, and where it is not the seat's colour
// shows through. This is what the fixed function pipeline's alpha_blend colour
// operation did for the portal vortex before it was given a shader, with the
// material's own colour now coming from the seat.
// SeatColor.frag is the same thing for a part with no texture at all.

uniform sampler2D decalmap;
uniform vec4 seatColor;

in vec2 out_UV0;

out vec4 color;

void main(void)
{
    vec4 texel = texture(decalmap, out_UV0.st);
    color = vec4(mix(seatColor.rgb, texel.rgb, texel.a), 1.0);
}
