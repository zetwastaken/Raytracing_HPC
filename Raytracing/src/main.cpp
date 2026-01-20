#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"
#include "GpuRenderer.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
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
              << "  --mode original|bvh|bvh-mt|gpu   Select rendering preset (default: bvh-mt)\n"
              << "  --width N                    Image width (pixels)\n"
              << "  --height N                   Image height (pixels). Overrides aspect ratio.\n"
              << "  --aspect RATIO               Aspect ratio (width/height). Used when height not provided.\n"
              << "  --samples N                  Samples per pixel\n"
              << "  --depth N                    Max ray bounce depth\n"
              << "  --output PATH                Output PNG path (default: auto timestamp name)\n"
              << "  --bvh / --no-bvh             Force enable/disable BVH\n"
              << "  --threads / --no-threads     Force enable/disable multithreading\n"
              << "  --gpu / --no-gpu             Force enable/disable GPU rendering (Metal)\n"
              << "  --benchmark                  Run benchmark sweep (no image write)\n"
              << "  --bench-sizes WxH,[...]      Comma-separated resolutions (e.g., 800x450,1280x720)\n"
              << "  --bench-samples N,[...]      Comma-separated samples-per-pixel values\n"
              << "  --bench-depths N,[...]       Comma-separated max-depth values\n"
              << "  --bench-objects N,[...]      Comma-separated extra object counts to add to scene\n"
              << "  --bench-csv PATH             CSV output path (default: benchmark_results.csv)\n"
              << "  --help                       Show this message\n";
}

struct CliSettings {
    RenderConfig config;
    int max_depth = 100;
    bool height_override = false;
    int height_value = 0;
    bool output_override = false;
    bool benchmark = false;
    std::vector<std::pair<int, int>> bench_sizes;
    std::vector<int> bench_samples;
    std::vector<int> bench_depths;
    std::vector<int> bench_extra_objects;
    std::string bench_csv_path = "benchmark_results.csv";
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
                settings.config.enable_gpu = false;
            } else if (mode == "bvh") {
                settings.config.enable_bvh = true;
                settings.config.enable_multithreading = false;
                settings.config.enable_gpu = false;
            } else if (mode == "bvh-mt") {
                settings.config.enable_bvh = true;
                settings.config.enable_multithreading = true;
                settings.config.enable_gpu = false;
            } else if (mode == "gpu") {
                settings.config.enable_bvh = false;
                settings.config.enable_multithreading = false;
                settings.config.enable_gpu = true;
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
        } else if (arg == "--gpu") {
            settings.config.enable_gpu = true;
            settings.config.enable_bvh = false;
            settings.config.enable_multithreading = false;
        } else if (arg == "--no-gpu") {
            settings.config.enable_gpu = false;
        } else if (arg == "--benchmark") {
            settings.benchmark = true;
        } else if (arg == "--bench-sizes") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::stringstream ss(value);
            std::string item;
            settings.bench_sizes.clear();
            while (std::getline(ss, item, ',')) {
                auto xpos = item.find('x');
                if (xpos == std::string::npos) {
                    std::cerr << "Invalid size: " << item << "\n";
                    return false;
                }
                int w = 0;
                int h = 0;
                if (!parse_int(item.substr(0, xpos), w) || !parse_int(item.substr(xpos + 1), h)) {
                    std::cerr << "Invalid size: " << item << "\n";
                    return false;
                }
                settings.bench_sizes.emplace_back(w, h);
            }
        } else if (arg == "--bench-samples") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::stringstream ss(value);
            std::string item;
            settings.bench_samples.clear();
            while (std::getline(ss, item, ',')) {
                int v = 0;
                if (!parse_int(item, v)) {
                    std::cerr << "Invalid samples value: " << item << "\n";
                    return false;
                }
                settings.bench_samples.push_back(v);
            }
        } else if (arg == "--bench-depths") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::stringstream ss(value);
            std::string item;
            settings.bench_depths.clear();
            while (std::getline(ss, item, ',')) {
                int v = 0;
                if (!parse_int(item, v)) {
                    std::cerr << "Invalid depth value: " << item << "\n";
                    return false;
                }
                settings.bench_depths.push_back(v);
            }
        } else if (arg == "--bench-objects") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::stringstream ss(value);
            std::string item;
            settings.bench_extra_objects.clear();
            while (std::getline(ss, item, ',')) {
                int v = 0;
                if (!parse_int(item, v)) {
                    std::cerr << "Invalid extra objects value: " << item << "\n";
                    return false;
                }
                settings.bench_extra_objects.push_back(v);
            }
        } else if (arg == "--bench-csv") {
            const char* value = require_value(arg);
            if (!value) return false;
            settings.bench_csv_path = value;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}
} // namespace

enum class RenderMode {
    Original,
    Bvh,
    BvhMultithread,
    Gpu
};

struct BenchmarkConfig {
    RenderMode mode;
    int width;
    int height;
    int samples;
    int max_depth;
    int extra_objects;
};

struct BenchmarkResult {
    BenchmarkConfig cfg;
    bool bvh;
    bool threads;
    bool gpu;
    std::size_t object_count;
    double setup_ms;
    double render_ms;
    double total_ms;
    std::string status;
};

Scene create_scene_with_extras(const RoomLayout& layout,
                               bool enable_acceleration,
                               int extra_objects) {
    Scene scene = create_scene(layout, {}, enable_acceleration);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_x(-layout.half_width + 0.5, layout.half_width - 0.5);
    std::uniform_real_distribution<double> dist_z(layout.back_wall_z + 0.5, layout.front_opening_z - 1.0);
    std::uniform_real_distribution<double> dist_r(0.1, 0.4);

    auto matte_material = std::make_shared<Matte>(Color(0.6, 0.6, 0.6));

    for (int i = 0; i < extra_objects; ++i) {
        double radius = dist_r(rng);
        Point3 center(dist_x(rng), layout.floor_y + radius, dist_z(rng));
        scene.objects.add(std::make_shared<Sphere>(center, radius, matte_material));
    }

    if (enable_acceleration) {
        scene.objects.build_bvh();
    }

    return scene;
}

void ensure_csv_header(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::ofstream out(path, std::ios::out);
        out << "mode,width,height,samples,max_depth,extra_objects,object_count,bvh,threads,gpu,setup_ms,render_ms,total_ms,status\n";
    }
}

void append_csv_row(const std::string& path, const BenchmarkResult& r) {
    std::ofstream out(path, std::ios::app);
    auto mode_to_string = [](RenderMode mode) -> std::string {
        switch (mode) {
        case RenderMode::Original: return "original";
        case RenderMode::Bvh: return "bvh";
        case RenderMode::BvhMultithread: return "bvh-mt";
        case RenderMode::Gpu: return "gpu";
        }
        return "unknown";
    };

    out << mode_to_string(r.cfg.mode) << ","
        << r.cfg.width << ","
        << r.cfg.height << ","
        << r.cfg.samples << ","
        << r.cfg.max_depth << ","
        << r.cfg.extra_objects << ","
        << r.object_count << ","
        << (r.bvh ? "1" : "0") << ","
        << (r.threads ? "1" : "0") << ","
        << (r.gpu ? "1" : "0") << ","
        << r.setup_ms << ","
        << r.render_ms << ","
        << r.total_ms << ","
        << r.status << "\n";
}

BenchmarkResult run_single_benchmark(const BenchmarkConfig& cfg, const RoomLayout& layout) {
    BenchmarkResult result{};
    result.cfg = cfg;

    RenderConfig config;
    config.image_width = cfg.width;
    config.image_height = cfg.height;
    config.aspect_ratio = static_cast<double>(cfg.width) / static_cast<double>(cfg.height);
    config.samples_per_pixel = cfg.samples;
    config.enable_bvh = (cfg.mode == RenderMode::Bvh || cfg.mode == RenderMode::BvhMultithread);
    config.enable_multithreading = (cfg.mode == RenderMode::BvhMultithread);
    config.enable_gpu = (cfg.mode == RenderMode::Gpu);

    Camera camera(config.aspect_ratio);

    bool enable_accel_scene = config.enable_bvh && !config.enable_gpu;

    auto t0 = std::chrono::steady_clock::now();
    Scene scene = create_scene_with_extras(layout, enable_accel_scene, cfg.extra_objects);
    auto t1 = std::chrono::steady_clock::now();

    result.object_count = scene.object_count();

    auto duration_setup = std::chrono::duration<double, std::milli>(t1 - t0).count();
    result.setup_ms = duration_setup;
    result.bvh = config.enable_bvh;
    result.threads = config.enable_multithreading;
    result.gpu = config.enable_gpu;

    try {
        auto t_render_start = std::chrono::steady_clock::now();
        if (config.enable_gpu) {
            (void)render_image_gpu(config, camera, scene, cfg.max_depth);
        } else {
            (void)render_image(config, camera, scene, cfg.max_depth);
        }
        auto t_render_end = std::chrono::steady_clock::now();
        result.render_ms = std::chrono::duration<double, std::milli>(t_render_end - t_render_start).count();
        result.total_ms = std::chrono::duration<double, std::milli>(t_render_end - t0).count();
        result.status = "ok";
    } catch (const std::exception& ex) {
        auto t_fail = std::chrono::steady_clock::now();
        result.render_ms = -1.0;
        result.total_ms = std::chrono::duration<double, std::milli>(t_fail - t0).count();
        result.status = std::string("fail: ") + ex.what();
    }

    return result;
}

int run_benchmarks(const CliSettings& settings) {
    ensure_csv_header(settings.bench_csv_path);

    RoomLayout layout = default_room_layout();
    std::vector<RenderMode> modes = {
        RenderMode::Original,
        RenderMode::Bvh,
        RenderMode::BvhMultithread,
        RenderMode::Gpu
    };

    // Count total runs for progress.
    std::size_t total_runs = static_cast<std::size_t>(modes.size())
        * settings.bench_sizes.size()
        * settings.bench_samples.size()
        * settings.bench_depths.size()
        * settings.bench_extra_objects.size();
    std::size_t run_index = 0;

    for (const auto& size : settings.bench_sizes) {
        int width = size.first;
        int height = size.second;
        for (int samples : settings.bench_samples) {
            for (int depth : settings.bench_depths) {
                for (int extra : settings.bench_extra_objects) {
                    for (RenderMode mode : modes) {
                        ++run_index;
                        std::cerr << "[bench] " << run_index << "/" << total_runs
                                  << " mode=" << (mode == RenderMode::Original ? "original"
                                              : mode == RenderMode::Bvh ? "bvh"
                                              : mode == RenderMode::BvhMultithread ? "bvh-mt"
                                              : "gpu")
                                  << " res=" << width << "x" << height
                                  << " spp=" << samples
                                  << " depth=" << depth
                                  << " extra=" << extra
                                  << std::endl;
                        BenchmarkConfig cfg{mode, width, height, samples, depth, extra};
                        BenchmarkResult res = run_single_benchmark(cfg, layout);
                        append_csv_row(settings.bench_csv_path, res);
                    }
                }
            }
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    // ========== Configuration ==========
    CliSettings settings;
    // Defaults mirror previous behavior (BVH + multithreading on).
    settings.config.enable_bvh = true;
    settings.config.enable_multithreading = true;
    settings.max_depth = 100;
    settings.bench_sizes = {{800, 450}, {1280, 720}};
    settings.bench_samples = {50};
    settings.bench_depths = {20};
    settings.bench_extra_objects = {0, 50};

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

    if (settings.benchmark) {
        return run_benchmarks(settings);
    }

    // ========== Setup ==========
    const RoomLayout room_layout = default_room_layout();
    const double ceiling_height = room_layout.ceiling_y;
    const double room_center_z = room_layout.back_wall_z + room_layout.half_depth;
    const double lamp_intensity = 10.0;
    const double lamp_drop_from_ceiling = 0.3;
    const double lamp_height = ceiling_height - lamp_drop_from_ceiling;
    const double lamp_z_position = room_center_z;

    Camera camera(config.aspect_ratio);

    std::vector<Light> lights;
    lights.emplace_back(
        Point3(0.0, lamp_height, lamp_z_position),
        Color(lamp_intensity, lamp_intensity, lamp_intensity)
    );

    Scene scene = create_scene(room_layout, std::move(lights), config.enable_bvh && !settings.config.enable_gpu);

    // ========== Render ==========
    std::vector<unsigned char> image_data;
    if (config.enable_gpu) {
        image_data = render_image_gpu(config, camera, scene, max_depth);
    } else {
        image_data = render_image(config, camera, scene, max_depth);
    }

    // ========== Save ==========
    std::string output_filename = generate_filename(config, max_depth);
    if (settings.output_override) {
        output_filename = config.output_path;
    }
    bool success = save_image(output_filename, config, image_data);

    return success ? 0 : 1;
}
