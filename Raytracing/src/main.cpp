#include "Camera.h"
#include "PngWriter.h"
#include "RenderConfig.h"
#include "Renderer.h"
#include "Scene.h"
#include "GpuRenderer.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <map>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <tuple>
#include <thread>
#include <cstring>
#include <utility>
#include <vector>
#include <cstdint>

#include <sys/utsname.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

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
                const std::vector<unsigned char>& image_data,
                bool verbose = true) {
    bool success = png_writer::write_rgb(filepath, config.image_width, config.image_height, image_data);
    
    if (verbose) {
        if (success) {
            std::cerr << "Saved image to " << filepath << "\n";
        } else {
            std::cerr << "Failed to write PNG image.\n";
        }
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
              << "  --quiet                      Suppress benchmark progress prints and info logs\n"
              << "  --bench-sizes WxH,[...]      Comma-separated resolutions (e.g., 800x450,1280x720)\n"
              << "  --bench-samples N,[...]      Comma-separated samples-per-pixel values\n"
              << "  --bench-depths N,[...]       Comma-separated max-depth values\n"
              << "  --bench-objects N,[...]      Comma-separated extra object counts to add to scene\n"
              << "  --bench-thread-counts N,[...]   Thread counts for multithreaded mode (0=hardware concurrency)\n"
              << "  --bench-repeats N            Minimum repeats per configuration (default: 5)\n"
              << "  --bench-max-repeats N        Hard cap on repeats per configuration (default: 50)\n"
              << "  --bench-min-runtime-ms M     Keep repeating until cumulative render time >= M ms (default: 200)\n"
              << "  --bench-scale-objects        Multiply extra objects by thread count (weak scaling)\n"
              << "  --bench-csv PATH             CSV output path (default: benchmark_results.csv)\n"
              << "  --bench-warmup N             Unrecorded warmup runs per configuration (default: 1)\n"
              << "  --bench-pin-threads          Pin worker threads to cores (best-effort)\n"
              << "  --bench-gpu-timing           Record GPU command buffer duration (Metal only)\n"
              << "  --bench-run-id ID            Custom run identifier stored in CSV\n"
              << "  --bench-raw-csv PATH         Optional raw-per-run CSV (records every attempt, incl. warmups)\n"
              << "  --gpu-parallelism N          Parallel unit count for GPU efficiency calculations (default: hw threads)\n"
              << "  --help                       Show this message\n";
}

struct CliSettings {
    RenderConfig config;
    int max_depth = 100;
    bool height_override = false;
    int height_value = 0;
    bool output_override = false;
    bool benchmark = false;
    bool quiet = false;
    std::vector<std::pair<int, int>> bench_sizes;
    std::vector<int> bench_samples;
    std::vector<int> bench_depths;
    std::vector<int> bench_extra_objects;
    std::vector<int> bench_thread_counts;
    int bench_repeats = 5;
    int bench_max_repeats = 50;
    double bench_min_runtime_ms = 200.0;
    bool bench_scale_objects = false;
    std::string bench_csv_path = "benchmark_results.csv";
    int gpu_parallelism = 0;
    int bench_warmup_runs = 1;
    bool bench_pin_threads = false;
    bool bench_gpu_timing = false;
    std::string bench_run_id;
    std::string bench_raw_csv_path;
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
        } else if (arg == "--quiet") {
            settings.quiet = true;
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
        } else if (arg == "--bench-thread-counts") {
            const char* value = require_value(arg);
            if (!value) return false;
            std::stringstream ss(value);
            std::string item;
            settings.bench_thread_counts.clear();
            while (std::getline(ss, item, ',')) {
                int v = 0;
                if (!parse_int(item, v) || v < 0) {
                    std::cerr << "Invalid thread count value: " << item << "\n";
                    return false;
                }
                settings.bench_thread_counts.push_back(v);
            }
        } else if (arg == "--bench-repeats") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.bench_repeats) || settings.bench_repeats <= 0) {
                std::cerr << "Invalid bench repeats: " << value << "\n";
                return false;
            }
        } else if (arg == "--bench-max-repeats") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.bench_max_repeats) || settings.bench_max_repeats <= 0) {
                std::cerr << "Invalid bench max repeats: " << value << "\n";
                return false;
            }
        } else if (arg == "--bench-min-runtime-ms") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_double(value, settings.bench_min_runtime_ms) || settings.bench_min_runtime_ms < 0.0) {
                std::cerr << "Invalid bench min runtime: " << value << "\n";
                return false;
            }
        } else if (arg == "--bench-scale-objects") {
            settings.bench_scale_objects = true;
        } else if (arg == "--bench-csv") {
            const char* value = require_value(arg);
            if (!value) return false;
            settings.bench_csv_path = value;
        } else if (arg == "--bench-raw-csv") {
            const char* value = require_value(arg);
            if (!value) return false;
            settings.bench_raw_csv_path = value;
        } else if (arg == "--gpu-parallelism") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.gpu_parallelism) || settings.gpu_parallelism <= 0) {
                std::cerr << "Invalid gpu parallelism: " << value << "\n";
                return false;
            }
        } else if (arg == "--bench-warmup") {
            const char* value = require_value(arg);
            if (!value) return false;
            if (!parse_int(value, settings.bench_warmup_runs) || settings.bench_warmup_runs < 0) {
                std::cerr << "Invalid bench warmup runs: " << value << "\n";
                return false;
            }
        } else if (arg == "--bench-pin-threads") {
            settings.bench_pin_threads = true;
        } else if (arg == "--bench-gpu-timing") {
            settings.bench_gpu_timing = true;
        } else if (arg == "--bench-run-id") {
            const char* value = require_value(arg);
            if (!value) return false;
            settings.bench_run_id = value;
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
    int thread_count;
    bool pin_threads = false;
    bool measure_gpu_timing = false;
};

struct TimingStats {
    double mean = 0.0;
    double stddev = 0.0;
};

struct SystemInfo {
    std::string cpu_model;
    std::string os_name;
    std::string compiler;
    unsigned int hw_threads = 1;
};

struct SingleRunResult {
    BenchmarkConfig cfg;
    bool bvh = false;
    bool threads = false;
    bool gpu = false;
    std::size_t object_count = 0;
    double scene_ms = 0.0;
    double bvh_ms = 0.0;
    double setup_ms = 0.0;
    double render_ms = 0.0;
    double total_ms = 0.0;
    double thread_launch_ms = 0.0;
    double gpu_ms = 0.0;
    uint64_t ray_count = 0;
    double max_mem_mb = 0.0;
    std::string status;
};

struct BenchmarkResult {
    BenchmarkConfig cfg;
    bool bvh;
    bool threads;
    bool gpu;
    std::size_t object_count;
    TimingStats scene_ms;
    TimingStats bvh_ms;
    TimingStats setup_ms;
    TimingStats render_ms;
    TimingStats total_ms;
    TimingStats thread_launch_ms;
    TimingStats gpu_ms;
    double max_mem_mb;
    int runs;
    int parallel_units;
    double speedup;
    double efficiency;
    double primary_rays;
    double traced_rays;
    double rays_per_sec;
    double rays_per_thread;
    int warmup_runs;
    std::string run_id;
    std::string status;
    SystemInfo sys_info;
    std::string timestamp;
};

struct BenchmarkKey {
    int width;
    int height;
    int samples;
    int depth;
    int extra;

    bool operator<(const BenchmarkKey& other) const {
        return std::tie(width, height, samples, depth, extra)
             < std::tie(other.width, other.height, other.samples, other.depth, other.extra);
    }
};

TimingStats compute_stats(const std::vector<double>& samples) {
    TimingStats stats;
    if (samples.empty()) {
        return stats;
    }
    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    stats.mean = sum / static_cast<double>(samples.size());
    double variance = 0.0;
    for (double v : samples) {
        const double delta = v - stats.mean;
        variance += delta * delta;
    }
    stats.stddev = samples.size() > 1
        ? std::sqrt(variance / static_cast<double>(samples.size() - 1))
        : 0.0;
    return stats;
}

std::string mode_to_string(RenderMode mode) {
    switch (mode) {
    case RenderMode::Original: return "original";
    case RenderMode::Bvh: return "bvh";
    case RenderMode::BvhMultithread: return "bvh-mt";
    case RenderMode::Gpu: return "gpu";
    }
    return "unknown";
}

std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* local_time = std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::setfill('0')
        << (local_time->tm_year + 1900) << "-"
        << std::setw(2) << (local_time->tm_mon + 1) << "-"
        << std::setw(2) << local_time->tm_mday << "T"
        << std::setw(2) << local_time->tm_hour << ":"
        << std::setw(2) << local_time->tm_min << ":"
        << std::setw(2) << local_time->tm_sec;
    return oss.str();
}

std::string generate_run_id() {
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFull);
    std::ostringstream oss;
    oss << iso_timestamp() << "-" << std::hex << std::setw(12) << std::setfill('0') << dist(rng);
    return oss.str();
}

std::string detect_os_name() {
    struct utsname uname_data;
    if (uname(&uname_data) == 0) {
        return std::string(uname_data.sysname) + " " + uname_data.release;
    }
    return "unknown";
}

std::string detect_compiler() {
#if defined(__clang__)
    return "clang " __clang_version__;
#elif defined(__GNUC__)
    return "gcc " __VERSION__;
#else
    return "unknown-compiler";
#endif
}

std::string detect_cpu_model() {
#if defined(__APPLE__)
    char buffer[256];
    size_t buffer_len = sizeof(buffer);
    if (sysctlbyname("machdep.cpu.brand_string", &buffer, &buffer_len, nullptr, 0) == 0) {
        return std::string(buffer, strnlen(buffer, buffer_len));
    }
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = line.substr(colon + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                return value;
            }
        }
    }
#endif
    return "unknown";
}

SystemInfo collect_system_info() {
    SystemInfo info;
    info.cpu_model = detect_cpu_model();
    info.os_name = detect_os_name();
    info.compiler = detect_compiler();
    info.hw_threads = std::max(1u, std::thread::hardware_concurrency());
    return info;
}

double current_memory_mb() {
#if defined(__APPLE__)
    task_vm_info_data_t vm_info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm_info), &count) == KERN_SUCCESS) {
        return static_cast<double>(vm_info.phys_footprint) / (1024.0 * 1024.0);
    }
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("VmHWM:", 0) == 0 || line.rfind("VmRSS:", 0) == 0) {
            std::stringstream ss(line.substr(line.find_first_of("0123456789")));
            double kb = 0.0;
            ss >> kb;
            return kb / 1024.0;
        }
    }
    std::ifstream statm("/proc/self/statm");
    long total_pages = 0;
    long resident_pages = 0;
    if (statm >> total_pages >> resident_pages) {
        const long page_size = sysconf(_SC_PAGESIZE);
        return (static_cast<double>(resident_pages) * static_cast<double>(page_size)) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

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

void ensure_csv_header(const std::string& path, bool quiet) {
    static const std::string header =
        "mode,width,height,samples,max_depth,extra_objects,object_count,thread_count,bvh,threads,gpu,"
        "scene_ms,bvh_ms,setup_ms,render_ms_mean,render_ms_stddev,total_ms_mean,total_ms_stddev,"
        "thread_launch_ms_mean,thread_launch_ms_stddev,gpu_ms_mean,gpu_ms_stddev,max_mem_mb,runs,warmup_runs,parallel_units,"
        "primary_rays,traced_rays,rays_per_sec,rays_per_thread,speedup,efficiency,run_id,"
        "cpu_model,os_name,compiler,hw_threads,timestamp,status\n";

    bool needs_header = true;
    if (std::filesystem::exists(path)) {
        std::ifstream existing(path);
        std::string first_line;
        if (std::getline(existing, first_line)) {
            needs_header = (first_line != header.substr(0, header.size() - 1));
        }
    }

    if (needs_header) {
        if (!quiet && std::filesystem::exists(path)) {
            std::cerr << "[bench] Rewriting CSV header for " << path << " to match new schema\n";
        }
        std::ofstream out(path, std::ios::out);
        out << header;
    }
}

void ensure_raw_csv_header(const std::string& path, bool quiet) {
    static const std::string header =
        "run_id,mode,width,height,samples,max_depth,extra_objects,object_count,thread_count,bvh,threads,gpu,"
        "scene_ms,bvh_ms,setup_ms,render_ms,total_ms,thread_launch_ms,gpu_ms,max_mem_mb,ray_count,"
        "is_warmup,iteration,cpu_model,os_name,compiler,hw_threads,timestamp,status\n";

    bool needs_header = true;
    if (std::filesystem::exists(path)) {
        std::ifstream existing(path);
        std::string first_line;
        if (std::getline(existing, first_line)) {
            needs_header = (first_line != header.substr(0, header.size() - 1));
        }
    }

    if (needs_header) {
        if (!quiet && std::filesystem::exists(path)) {
            std::cerr << "[bench] Rewriting raw CSV header for " << path << " to match new schema\n";
        }
        std::ofstream out(path, std::ios::out);
        out << header;
    }
}

void append_raw_csv_row(const std::string& path,
                        const SingleRunResult& r,
                        const BenchmarkConfig& cfg,
                        const SystemInfo& sys_info,
                        const std::string& run_id,
                        bool is_warmup,
                        int iteration) {
    std::ofstream out(path, std::ios::app);

    out << run_id << ","
        << mode_to_string(cfg.mode) << ","
        << cfg.width << ","
        << cfg.height << ","
        << cfg.samples << ","
        << cfg.max_depth << ","
        << cfg.extra_objects << ","
        << r.object_count << ","
        << cfg.thread_count << ","
        << (r.bvh ? "1" : "0") << ","
        << (r.threads ? "1" : "0") << ","
        << (r.gpu ? "1" : "0") << ","
        << r.scene_ms << ","
        << r.bvh_ms << ","
        << r.setup_ms << ","
        << r.render_ms << ","
        << r.total_ms << ","
        << r.thread_launch_ms << ","
        << r.gpu_ms << ","
        << r.max_mem_mb << ","
        << r.ray_count << ","
        << (is_warmup ? "1" : "0") << ","
        << iteration << ","
        << "\"" << sys_info.cpu_model << "\","
        << "\"" << sys_info.os_name << "\","
        << "\"" << sys_info.compiler << "\","
        << sys_info.hw_threads << ","
        << iso_timestamp() << ","
        << r.status << "\n";
}

void append_csv_row(const std::string& path, const BenchmarkResult& r) {
    std::ofstream out(path, std::ios::app);

    out << mode_to_string(r.cfg.mode) << ","
        << r.cfg.width << ","
        << r.cfg.height << ","
        << r.cfg.samples << ","
        << r.cfg.max_depth << ","
        << r.cfg.extra_objects << ","
        << r.object_count << ","
        << r.cfg.thread_count << ","
        << (r.bvh ? "1" : "0") << ","
        << (r.threads ? "1" : "0") << ","
        << (r.gpu ? "1" : "0") << ","
        << r.scene_ms.mean << ","
        << r.bvh_ms.mean << ","
        << r.setup_ms.mean << ","
        << r.render_ms.mean << ","
        << r.render_ms.stddev << ","
        << r.total_ms.mean << ","
        << r.total_ms.stddev << ","
        << r.thread_launch_ms.mean << ","
        << r.thread_launch_ms.stddev << ","
        << r.gpu_ms.mean << ","
        << r.gpu_ms.stddev << ","
        << r.max_mem_mb << ","
        << r.runs << ","
        << r.warmup_runs << ","
        << r.parallel_units << ","
        << r.primary_rays << ","
        << r.traced_rays << ","
        << r.rays_per_sec << ","
        << r.rays_per_thread << ","
        << r.speedup << ","
        << r.efficiency << ","
        << r.run_id << ","
        << "\"" << r.sys_info.cpu_model << "\","
        << "\"" << r.sys_info.os_name << "\","
        << "\"" << r.sys_info.compiler << "\","
        << r.sys_info.hw_threads << ","
        << r.timestamp << ","
        << r.status << "\n";
}

SingleRunResult execute_benchmark_once(const BenchmarkConfig& cfg, const RoomLayout& layout) {
    SingleRunResult result{};
    result.cfg = cfg;

    RenderConfig config;
    config.image_width = cfg.width;
    config.image_height = cfg.height;
    config.aspect_ratio = static_cast<double>(cfg.width) / static_cast<double>(cfg.height);
    config.samples_per_pixel = cfg.samples;
    config.enable_bvh = (cfg.mode == RenderMode::Bvh || cfg.mode == RenderMode::BvhMultithread);
    config.enable_multithreading = (cfg.mode == RenderMode::BvhMultithread);
    config.enable_gpu = (cfg.mode == RenderMode::Gpu);
    config.thread_count_override = cfg.thread_count;
    config.pin_threads = cfg.pin_threads;

    Camera camera(config.aspect_ratio);

    const bool wants_accel = config.enable_bvh && !config.enable_gpu;

    const auto t_scene_start = std::chrono::steady_clock::now();
    Scene scene = create_scene_with_extras(layout, false, cfg.extra_objects);
    const auto t_scene_end = std::chrono::steady_clock::now();

    result.object_count = scene.object_count();
    result.scene_ms = std::chrono::duration<double, std::milli>(t_scene_end - t_scene_start).count();

    if (wants_accel) {
        scene.objects.set_acceleration_enabled(true);
        const auto t_bvh_start = std::chrono::steady_clock::now();
        scene.objects.build_bvh();
        const auto t_bvh_end = std::chrono::steady_clock::now();
        result.bvh_ms = std::chrono::duration<double, std::milli>(t_bvh_end - t_bvh_start).count();
    } else {
        scene.objects.set_acceleration_enabled(false);
        result.bvh_ms = 0.0;
    }

    result.setup_ms = result.scene_ms + result.bvh_ms;
    result.bvh = config.enable_bvh;
    result.threads = config.enable_multithreading;
    result.gpu = config.enable_gpu;

    try {
        double thread_launch_ms = 0.0;
        uint64_t ray_count = 0;
        const auto t_render_start = std::chrono::steady_clock::now();
        if (config.enable_gpu) {
            double gpu_ms = 0.0;
            (void)render_image_gpu(config, camera, scene, cfg.max_depth, cfg.measure_gpu_timing ? &gpu_ms : nullptr);
            result.gpu_ms = gpu_ms;
            result.ray_count = static_cast<uint64_t>(config.image_width)
                * static_cast<uint64_t>(config.image_height)
                * static_cast<uint64_t>(config.samples_per_pixel);
        } else {
            (void)render_image(config, camera, scene, cfg.max_depth, &thread_launch_ms, &ray_count);
            result.ray_count = ray_count;
        }
        const auto t_render_end = std::chrono::steady_clock::now();
        result.render_ms = std::chrono::duration<double, std::milli>(t_render_end - t_render_start).count();
        result.total_ms = result.render_ms + result.setup_ms;
        result.thread_launch_ms = thread_launch_ms;
        result.max_mem_mb = current_memory_mb();
        result.status = "ok";
    } catch (const std::exception& ex) {
        const auto t_fail = std::chrono::steady_clock::now();
        result.render_ms = -1.0;
        result.total_ms = std::chrono::duration<double, std::milli>(t_fail - t_scene_start).count();
        result.status = std::string("fail: ") + ex.what();
    }

    return result;
}

BenchmarkResult run_repeated_benchmark(const BenchmarkConfig& cfg,
                                       const RoomLayout& layout,
                                       const CliSettings& settings,
                                       const SystemInfo& sys_info) {
    BenchmarkResult aggregate{};
    aggregate.cfg = cfg;
    aggregate.sys_info = sys_info;
    aggregate.timestamp = iso_timestamp();
    aggregate.run_id = settings.bench_run_id;
    aggregate.warmup_runs = settings.bench_warmup_runs;

    std::vector<double> scene_samples;
    std::vector<double> bvh_samples;
    std::vector<double> setup_samples;
    std::vector<double> render_samples;
    std::vector<double> total_samples;
    std::vector<double> thread_launch_samples;
    std::vector<double> gpu_samples;
    std::vector<double> ray_count_samples;
    double max_mem_mb = 0.0;
    std::size_t object_count = 0;

    const int min_repeats = std::max(1, settings.bench_repeats);
    const int max_repeats = std::max(min_repeats, settings.bench_max_repeats);

    double accumulated_render_ms = 0.0;
    int repeats = 0;

    const bool record_raw = !settings.bench_raw_csv_path.empty();
    int iteration = 0;

    // Warmup phase to stabilize caches/allocations; now recorded if requested.
    for (int warm = 0; warm < settings.bench_warmup_runs; ++warm) {
        SingleRunResult warm_run = execute_benchmark_once(cfg, layout);
        if (record_raw) {
            append_raw_csv_row(settings.bench_raw_csv_path, warm_run, cfg, sys_info, settings.bench_run_id, true, iteration++);
        }
        if (warm_run.status != "ok") {
            aggregate.status = std::string("warmup_failed: ") + warm_run.status;
            return aggregate;
        }
    }

    while (repeats < max_repeats) {
        SingleRunResult run = execute_benchmark_once(cfg, layout);
        if (repeats == 0) {
            aggregate.bvh = run.bvh;
            aggregate.threads = run.threads;
            aggregate.gpu = run.gpu;
            object_count = run.object_count;
        }
        if (record_raw) {
            append_raw_csv_row(settings.bench_raw_csv_path, run, cfg, sys_info, settings.bench_run_id, false, iteration++);
        }
        if (run.status != "ok") {
            aggregate.status = run.status;
            break;
        }
        scene_samples.push_back(run.scene_ms);
        bvh_samples.push_back(run.bvh_ms);
        setup_samples.push_back(run.setup_ms);
        render_samples.push_back(run.render_ms);
        total_samples.push_back(run.total_ms);
        thread_launch_samples.push_back(run.thread_launch_ms);
        if (cfg.measure_gpu_timing) {
            gpu_samples.push_back(run.gpu_ms);
        }
        const double fallback_primary = static_cast<double>(cfg.width) * static_cast<double>(cfg.height) * static_cast<double>(cfg.samples);
        const double rays_this_run = run.ray_count > 0 ? static_cast<double>(run.ray_count) : fallback_primary;
        ray_count_samples.push_back(rays_this_run);
        max_mem_mb = std::max(max_mem_mb, run.max_mem_mb);

        ++repeats;
        accumulated_render_ms += std::max(0.0, run.render_ms);
        if (repeats >= min_repeats && accumulated_render_ms >= settings.bench_min_runtime_ms) {
            aggregate.status = "ok";
            break;
        }
    }

    aggregate.object_count = object_count;
    aggregate.scene_ms = compute_stats(scene_samples);
    aggregate.bvh_ms = compute_stats(bvh_samples);
    aggregate.setup_ms = compute_stats(setup_samples);
    aggregate.render_ms = compute_stats(render_samples);
    aggregate.total_ms = compute_stats(total_samples);
    aggregate.thread_launch_ms = compute_stats(thread_launch_samples);
    aggregate.gpu_ms = compute_stats(gpu_samples);
    TimingStats ray_stats = compute_stats(ray_count_samples);
    aggregate.max_mem_mb = max_mem_mb;
    aggregate.runs = repeats;
    aggregate.parallel_units = 1;
    aggregate.speedup = 0.0;
    aggregate.efficiency = 0.0;
    aggregate.primary_rays = static_cast<double>(cfg.width) * static_cast<double>(cfg.height) * static_cast<double>(cfg.samples);
    aggregate.traced_rays = ray_stats.mean > 0.0 ? ray_stats.mean : aggregate.primary_rays;
    if (cfg.mode == RenderMode::BvhMultithread) {
        const int resolved_threads = cfg.thread_count > 0 ? cfg.thread_count : static_cast<int>(sys_info.hw_threads);
        aggregate.parallel_units = std::max(1, resolved_threads);
    } else if (cfg.mode == RenderMode::Gpu) {
        aggregate.parallel_units = settings.gpu_parallelism > 0
            ? settings.gpu_parallelism
            : static_cast<int>(sys_info.hw_threads);
    }
    if (aggregate.render_ms.mean > 0.0) {
        const double seconds = aggregate.render_ms.mean / 1000.0;
        const double rays_for_rate = aggregate.traced_rays > 0.0 ? aggregate.traced_rays : aggregate.primary_rays;
        aggregate.rays_per_sec = rays_for_rate / seconds;
        aggregate.rays_per_thread = aggregate.parallel_units > 0 ? aggregate.rays_per_sec / aggregate.parallel_units : 0.0;
    } else {
        aggregate.rays_per_sec = 0.0;
        aggregate.rays_per_thread = 0.0;
    }
    if (aggregate.status.empty()) {
        aggregate.status = "ok";
    }
    return aggregate;
}

class BenchmarkSweep {
public:
    explicit BenchmarkSweep(const CliSettings& settings)
        : settings(settings)
        , sys_info(collect_system_info())
        , layout(default_room_layout())
        , modes({
            RenderMode::Original,
            RenderMode::Bvh,
            RenderMode::BvhMultithread,
            RenderMode::Gpu
        }) {
        thread_counts = settings.bench_thread_counts;
        if (thread_counts.empty()) {
            thread_counts.push_back(0);
        }
        total_runs = compute_total_runs();
    }

    int run() {
        ensure_csv_header(settings.bench_csv_path, settings.quiet);
        if (!settings.bench_raw_csv_path.empty()) {
            ensure_raw_csv_header(settings.bench_raw_csv_path, settings.quiet);
        }

        for (const auto& size : settings.bench_sizes) {
            const int width = size.first;
            const int height = size.second;
            for (int samples : settings.bench_samples) {
                for (int depth : settings.bench_depths) {
                    for (int extra : settings.bench_extra_objects) {
                        for (int thread_count_value : thread_counts) {
                            const int applied_extra = scaled_extra(extra, thread_count_value);
                            for (RenderMode mode : modes) {
                                const int cfg_threads = (mode == RenderMode::Gpu) ? 0 : thread_count_value;
                                log_progress(mode, width, height, samples, depth, applied_extra, cfg_threads);
                                BenchmarkConfig cfg{mode, width, height, samples, depth, applied_extra, cfg_threads};
                                cfg.pin_threads = settings.bench_pin_threads;
                                cfg.measure_gpu_timing = settings.bench_gpu_timing;
                                BenchmarkResult res = run_repeated_benchmark(cfg, layout, settings, sys_info);
                                apply_baseline_and_speedup(res);
                                append_csv_row(settings.bench_csv_path, res);
                            }
                        }
                    }
                }
            }
        }

        return 0;
    }

private:
    const CliSettings& settings;
    SystemInfo sys_info;
    RoomLayout layout;
    std::vector<RenderMode> modes;
    std::vector<int> thread_counts;
    std::map<BenchmarkKey, double> baselines;
    std::size_t total_runs = 0;
    std::size_t run_index = 0;

    std::size_t compute_total_runs() const {
        return static_cast<std::size_t>(modes.size())
            * settings.bench_sizes.size()
            * settings.bench_samples.size()
            * settings.bench_depths.size()
            * settings.bench_extra_objects.size()
            * thread_counts.size();
    }

    int scaled_extra(int base_extra, int thread_count_value) const {
        if (!settings.bench_scale_objects) {
            return base_extra;
        }
        const int scale = std::max(
            1,
            thread_count_value > 0
                ? thread_count_value
                : static_cast<int>(sys_info.hw_threads)
        );
        return base_extra * scale;
    }

    void log_progress(RenderMode mode,
                      int width,
                      int height,
                      int samples,
                      int depth,
                      int applied_extra,
                      int thread_count_value) {
        ++run_index;
        if (settings.quiet) {
            return;
        }
        std::cerr << "[bench] " << run_index << "/" << total_runs
                  << " mode=" << mode_to_string(mode)
                  << " res=" << width << "x" << height
                  << " spp=" << samples
                  << " depth=" << depth
                  << " extra=" << applied_extra
                  << " threads=" << thread_count_value
                  << std::endl;
    }

    void apply_baseline_and_speedup(BenchmarkResult& res) {
        const BenchmarkKey key{
            res.cfg.width,
            res.cfg.height,
            res.cfg.samples,
            res.cfg.max_depth,
            res.cfg.extra_objects
        };

        if (res.cfg.mode == RenderMode::Original && res.render_ms.mean > 0.0) {
            baselines[key] = res.render_ms.mean;
        }

        const auto it = baselines.find(key);
        const double baseline_ms = (it != baselines.end()) ? it->second : 0.0;
        if (baseline_ms > 0.0 && res.render_ms.mean > 0.0) {
            res.speedup = baseline_ms / res.render_ms.mean;
            res.efficiency = res.parallel_units > 0
                ? res.speedup / static_cast<double>(res.parallel_units)
                : 0.0;
        } else if (res.cfg.mode == RenderMode::Original && res.status == "ok") {
            res.speedup = 1.0;
            res.efficiency = 1.0;
        } else {
            res.speedup = 0.0;
            res.efficiency = 0.0;
            if (res.status == "ok" && baseline_ms == 0.0) {
                res.status = "missing_baseline";
            }
        }
    }
};

int run_benchmarks(const CliSettings& settings) {
    BenchmarkSweep sweep(settings);
    return sweep.run();
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

    if (settings.bench_run_id.empty()) {
        settings.bench_run_id = generate_run_id();
    }

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
    bool success = save_image(output_filename, config, image_data, !settings.quiet);

    return success ? 0 : 1;
}
