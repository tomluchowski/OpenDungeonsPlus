
/* Function to linearly interpolate between a0 and a1
 * Weight w should be in the range [0.0, 1.0]
 */
float interpolate(float a0, float a1, float w) {
    /* // You may want clamping by inserting:*/
    if (0.0 > w) return a0;
    if (1.0 < w) return a1;

    //return smoothstep(a0,a1,w) ;
    /* // Use this cubic interpolation [[Smoothstep]] instead, for a smooth appearance:*/
     return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
     /*
     * // Use [[Smootherstep]] for an even smoother result with a second derivative equal to zero on boundaries:
     * return (a1 - a0) * (x * (w * 6.0 - 15.0) * w * w *w + 10.0) + a0;
     */
}

/* Create random direction vector
 */
vec2 randomGradient(int ix, int iy) {
    // Random float. No precomputed gradients mean this works for any number of grid coordinates
    float random = 2920.0 * sin(ix * 21942.0 + iy * 171324.0 + 8912.0) * cos(ix * 23157.0 * iy * 217832.0 + 9758.0);
    vec2 gradient;
    gradient.x = cos(random);
    gradient.y = sin(random);
    return (gradient/4.0);
}

// Computes the dot product of the distance and gradient vectors.
float dotGridGradient(int ix, int iy, float x, float y) {
    // Get gradient from integer coordinates
    vec2 gradient = randomGradient(ix, iy);
    vec2 dGradient;
    // Compute the distance vector
    dGradient.x = x - float(ix);
    dGradient.y = y - float(iy);

    // Compute the dot-product
    return dot(dGradient,gradient);
}

// Compute Perlin noise at coordinates x, y
float perlin(float x, float y) {
    // Determine grid cell coordinates
    int x0 = int(floor(x));
    int x1 = x0 + 1;
    int y0 = int(floor(y));
    int y1 = y0 + 1;

    // Determine interpolation weights
    // Could also use higher order polynomial/s-curve here
    float sx = x - float(x0);
    float sy = y - float(y0);

    // Interpolate between grid point gradients
    float n0, n1, ix0, ix1, value;

    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = interpolate(n0, n1, sx);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = interpolate(n0, n1, sx);

    value = interpolate(ix0, ix1, sy);
    return value;
}
 
 
