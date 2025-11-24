/// Size of the kernel radius.
static const int BLUR_RADIUS = 7;

/// Generated from https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html with
/// linear filtering turned off. Blur sigma used is 3.
static const float WEIGHTS[BLUR_RADIUS] = {
    0.13649812722755f,
    0.12918787500405665f,
    0.10952340502389689f,
    0.08317258170844943f,
    0.05657737457255749f,
    0.03447396964662017f,
    0.018815730430644367f,
};

