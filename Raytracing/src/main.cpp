#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
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

namespace {
bool parse_int(const std::string& value, int& out) {
    char* end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (end != value.c_str() + value.size()) {
        return false;
    }
    if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

bool parse_double(const std::string& value, double& out) {
    char* end = nullptr;
    double parsed = std::strtod(value.c_str(), &end);
    if (end != value.c_str() + value.size()) {
        return false;
    }
    out = parsed;
    return true;
}

void print_usage(const char* exe) {
    std::cerr << "Usage: " << exe << " [options]\n"
              << "Options:\n"
              << "  --mode original|bvh|bvh-mt   Select acceleration/threading preset (default: bvh-mt)\n"
              << "  --width N                    Image width (pixels)\n"
              << "  --height N                   Image height (pixels). Overrides aspect ratio.\n"
              << "  --aspect RATIO               Aspect ratio (width/height). Used when height not provided.\n"
              << "  --samples N                  Samples per pixel\n"
              << "  --depth N                    Max ray bounce depth\n"
              << "  --output PATH                Output PNG path (default: auto timestamp name)\n"
              << "  --bvh / --no-bvh             Force enable/disable BVH\n"
              << "  --threads / --no-threads     Force enable/disable multithreading\n"
              << "  --help                       Show this message\n";
}

struct CliSettings {
    RenderConfig config;
    int max_depth = 100;
    bool height_override = false;
    int height_value = 0;
    bool output_override = false;
};

bool parse_arguments(int argc, char* argv[], CliSettings& settings) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        auto require_value = [&](const std::string& name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        } else if (arg == "--mode") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::string mode = value;
            if (mode == "original") {
                settings.config.enable_bvh = false;
                settings.config.enable_multithreading = false;
            } else if (mode == "bvh") {
                settings.config.enable_bvh = true;
                settings.config.enable_multithreading = false;
            } else if (mode == "bvh-mt") {
                settings.config.enable_bvh = true;
                settings.config.enable_multithreading = true;
            } else {
                std::cerr << "Unknown mode: " << mode << "\n";
                return false;
            }
        } else if (arg == "--width") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.config.image_width)) {
                std::cerr << "Invalid width: " << value << "\n";
                return false;
            }
        } else if (arg == "--height") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.height_value)) {
                std::cerr << "Invalid height: " << value << "\n";
                return false;
            }
            settings.height_override = true;
        } else if (arg == "--aspect") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_double(value, settings.config.aspect_ratio)) {
                std::cerr << "Invalid aspect ratio: " << value << "\n";
                return false;
            }
        } else if (arg == "--samples") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.config.samples_per_pixel)) {
                std::cerr << "Invalid samples: " << value << "\n";
                return false;
            }
        } else if (arg == "--depth") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.max_depth)) {
                std::cerr << "Invalid depth: " << value << "\n";
                return false;
            }
        } else if (arg == "--output") {
            const char* value = require_value(arg);
            if (!value) return false;
            settings.config.output_path = value;
            settings.output_override = true;
        } else if (arg == "--bvh") {
            settings.config.enable_bvh = true;
        } else if (arg == "--no-bvh") {
            settings.config.enable_bvh = false;
        } else if (arg == "--threads") {
            settings.config.enable_multithreading = true;
        } else if (arg == "--no-threads") {
            settings.config.enable_multithreading = false;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char* argv[]) {
    // ========== Configuration ==========
    CliSettings settings;
    // Defaults mirror previous behavior (BVH + multithreading on).
    settings.config.enable_bvh = true;
    settings.config.enable_multithreading = true;
    settings.max_depth = 100;

    if (!parse_arguments(argc, argv, settings)) {
        return 1;
    }

    RenderConfig& config = settings.config;
    if (settings.height_override) {
        config.image_height = settings.height_value;
        config.aspect_ratio = static_cast<double>(config.image_width) / static_cast<double>(config.image_height);
    } else {
        config.image_height = static_cast<int>(config.image_width / config.aspect_ratio);
    }
    const int max_depth = settings.max_depth;
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

    Scene scene = create_scene(room_layout, std::move(lights), config.enable_bvh);
    
    // ========== Render ==========
    std::vector<unsigned char> image_data = render_image(config, camera, scene, max_depth);
    
    // ========== Save ==========
    std::string output_filename = generate_filename(config, max_depth);
    if (settings.output_override) {
        output_filename = config.output_path;
    }
    bool success = save_image(output_filename, config, image_data);
    
    return success ? 0 : 1;
}
