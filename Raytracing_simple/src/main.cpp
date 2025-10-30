#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"

#include <iostream>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

/**
 * Save the rendered image to a PNG file.
 * 
 * @param filepath Path where the image will be saved
 * @param config Render configuration containing image dimensions
 * @param image_data Raw RGB pixel data
 * @return true if successful, false otherwise
 */
bool save_image(const std::string& filepath, const RenderConfig& config,
                const std::vector<unsigned char>& image_data) {
    bool success = png_writer::write_rgb(filepath, config.image_width, config.image_height, image_data);
    
    if (success) {
        std::cerr << "Saved image to " << filepath << "\n";
    } else {
        std::cerr << "Failed to write PNG image.\n";
    }
    
    return success;
}

int main() {
    // ========== Configuration ==========
    RenderConfig config(16.0 / 9.0, 600, 500);  // aspect_ratio, width, samples_per_pixel
    const int max_depth = 50;  // Maximum number of ray bounces for reflections/refractions
    const int light_count = 3;
    const double light_intensity = 14.0;
    const double light_radius = 6.0;
    const double light_height = 6.0;
    constexpr double pi = 3.1415926535897932385;
    
    // ========== Setup ==========
    Camera camera(config.aspect_ratio);
    
    std::vector<Light> lights;
    lights.reserve(light_count);
    
    // Place lights evenly on a ring so they illuminate the scene from multiple directions
    for (int index = 0; index < light_count; ++index) {
        double angle = 2.0 * pi * (static_cast<double>(index) / light_count);
        Point3 position(
            light_radius * std::cos(angle),
            light_height,
            -2.5 + 1.5 * std::sin(angle)
        );
        
        lights.emplace_back(position, Color(light_intensity, light_intensity, light_intensity));
    }

    Scene scene = create_scene(std::move(lights));
    
    // ========== Render ==========
    std::vector<unsigned char> image_data = render_image(config, camera, scene, max_depth);
    
    // ========== Save ==========
    bool success = save_image(config.output_path, config, image_data);
    
    return success ? 0 : 1;
}
