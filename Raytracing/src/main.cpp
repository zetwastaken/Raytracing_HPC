#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"

#include <iostream>
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
    RenderConfig config(16.0 / 9.0, 300, 100);  // aspect_ratio, width, samples_per_pixel
    const int max_depth = 50;  // Maximum number of ray bounces for reflections/refractions
    const RoomLayout room_layout = default_room_layout();
    const double ceiling_height = room_layout.ceiling_y;
    const double room_center_z = room_layout.back_wall_z + room_layout.half_depth;
    const double lamp_intensity = 10.0;
    const double lamp_drop_from_ceiling = 0.3;
    const double lamp_height = ceiling_height - lamp_drop_from_ceiling;
    const double lamp_z_position = room_center_z;
    
    // ========== Setup ==========
    Camera camera(config.aspect_ratio);
    
    std::vector<Light> lights;
    lights.emplace_back(
        Point3(0.0, lamp_height, lamp_z_position),
        Color(lamp_intensity, lamp_intensity, lamp_intensity)
    );

    Scene scene = create_scene(room_layout, std::move(lights));
    
    // ========== Render ==========
    std::vector<unsigned char> image_data = render_image(config, camera, scene, max_depth);
    
    // ========== Save ==========
    bool success = save_image(config.output_path, config, image_data);
    
    return success ? 0 : 1;
}
