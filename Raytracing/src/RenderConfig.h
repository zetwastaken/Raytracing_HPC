#ifndef RENDER_CONFIG_H
#define RENDER_CONFIG_H

/**
 * @file RenderConfig.h
 * @brief Render resolution and quality controls.
 */

#include <string>

/**
 * Image and quality settings for rendering.
 */
struct RenderConfig {
    double aspect_ratio;
    int image_width;
    int image_height;
    int samples_per_pixel;
    std::string output_path;
    
    /**
     * Create a render configuration.
     * 
     * @param ratio Aspect ratio (width/height), e.g., 16.0/9.0
     * @param width Image width in pixels
     * @param samples Number of samples per pixel (higher = better quality but slower)
     *                Recommended: 10-20 (fast), 50-100 (good), 500+ (excellent)
     */
    RenderConfig(double ratio = 16.0 / 9.0, int width = 800, int samples = 100)
        : aspect_ratio(ratio)
        , image_width(width)
        , image_height(static_cast<int>(width / ratio))
        , samples_per_pixel(samples)
        , output_path("render.png")
    {}
};

#endif
