/// Size of the kernel radius.
static const int BLUR_RADIUS = 6;

/// Generated from https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html with
/// linear filtering turned off. Blur sigma used is 1.75.
static const float WEIGHTS[BLUR_RADIUS] = {
    0.2252798453592171,
    0.19218719161855763,
    0.11931879617246707,
    0.05390390345302743,
    0.017715680824905423,
    0.004234505251433948,
};

/// Some of the radiance is too high for multiple Gaussian passes to fully blur (think values
/// greater than 64k). We clamp it to a value < 1 so we can get actual blurs.
static const float3 MAX_RADIANCE = float3( 0.9f );


