/// Size of the kernel radius.
static const int BLUR_RADIUS = 16;

/// Generated from https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html with
/// linear filtering turned off. Blur sigma used is 3.
static const float WEIGHTS[BLUR_RADIUS] = {
    0.04537448087867176,
    0.04514815367362432,
    0.04447651483296824,
    0.043379357780380726,
    0.041888620540389554,
    0.040046933326962186,
    0.03790554794563106,
    0.035521947684858765,
    0.032957284249529574,
    0.0302737943350784,
    0.02753234224884925,
    0.02479021778025798,
    0.022099292514055516,
    0.019504605870651337,
    0.017043417575355446,
    0.014744729202071892,
};

/// Some of the radiance is too high for a single Gaussian pass to fully blur (think values greater
/// than 64k). For now, we clamp it to a value < 1 so we can get actual blurs.
static const float3 MAX_RADIANCE = float3( 0.9f );


