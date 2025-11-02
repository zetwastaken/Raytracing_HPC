#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

/**
 * Generate a descriptive filename with render parameters and timestamp.
 * 
 * Format: render_WIDTHxHEIGHT_SPPsamples_DEPTHdepth_YYYYMMDD_HHMMSS.png
 * Example: render_1024x576_500samples_100depth_20251102_143027.png
 * 
 * @param config Render configuration containing image dimensions and samples
 * @param max_depth Maximum ray bounce depth
 * @return Generated filename string
 */
std::string generate_filename(const RenderConfig& config, int max_depth) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&time_t_now);
    
    std::ostringstream filename;
    filename << "render_"
             << config.image_width << "x" << config.image_height << "_"
             << config.samples_per_pixel << "samples_"
             << max_depth << "depth_"
             << std::setfill('0')
             << std::setw(4) << (local_time->tm_year + 1900)
             << std::setw(2) << (local_time->tm_mon + 1)
             << std::setw(2) << local_time->tm_mday << "_"
             << std::setw(2) << local_time->tm_hour
             << std::setw(2) << local_time->tm_min
             << std::setw(2) << local_time->tm_sec
             << ".png";
    
    return filename.str();
}

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
    RenderConfig config(16.0 / 9.0, 1024, 500);  // aspect_ratio, width, samples_per_pixel
    const int max_depth = 100;  // Maximum number of ray bounces for reflections/refractions
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
    std::string output_filename = generate_filename(config, max_depth);
    bool success = save_image(output_filename, config, image_data);
    
    return success ? 0 : 1;
}
