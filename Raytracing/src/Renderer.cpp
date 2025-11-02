#include "Renderer.h"

#include <iostream>

Color calculate_sky_color(const Ray& ray) {
    const Vec3 unit_direction = unit_vector(ray.direction());
    const double blend_factor = 0.5 * (unit_direction.y() + 1.0);
    const Color white(1.0, 1.0, 1.0);
    const Color sky_blue(0.5, 0.7, 1.0);
    return (1.0 - blend_factor) * white + blend_factor * sky_blue;
}

Color compute_diffuse_lighting(const Scene& scene, const HitRecord& hit_info) {
    if (scene.lights.empty()) {
        return Color(0.0, 0.0, 0.0);
    }

    Color accumulated_light(0.0, 0.0, 0.0);
    constexpr double shadow_bias = 0.001;

    for (const auto& light : scene.lights) {
        const Vec3 to_light = light.position - hit_info.hit_point;
        const double distance_squared = to_light.length_squared();
        if (distance_squared <= 0.0) {
            continue;
        }

        const Vec3 light_direction = unit_vector(to_light);
        const double n_dot_l = dot(hit_info.surface_normal, light_direction);
        if (n_dot_l <= 0.0) {
            continue;
        }

        const double distance_to_light = std::sqrt(distance_squared);
        const Ray shadow_ray(hit_info.hit_point + shadow_bias * hit_info.surface_normal, light_direction);
        HitRecord shadow_hit;
        if (scene.objects.hit(shadow_ray, shadow_bias, distance_to_light - shadow_bias, shadow_hit)) {
            continue;
        }

        const Color light_energy = light.intensity / distance_squared;
        accumulated_light += n_dot_l * light_energy;
    }

    return accumulated_light;
}

Color calculate_ray_color(const Ray& ray, const Scene& scene, int depth) {
    if (depth <= 0) {
        return Color(0, 0, 0);
    }

    HitRecord hit_info;
    constexpr double min_hit_distance = 0.001;
    constexpr double max_hit_distance = 1'000'000.0;

    if (scene.objects.hit(ray, min_hit_distance, max_hit_distance, hit_info)) {
        const Material& material = *hit_info.material_ptr;
        Color direct_component(0.0, 0.0, 0.0);

        if (material.is_diffuse()) {
            const Color incoming_light = compute_diffuse_lighting(scene, hit_info);
            direct_component = material.base_color() * incoming_light;
        }

        ScatterRecord scatter_record;
        if (material.scatter(ray, hit_info, scatter_record)) {
            return direct_component
                + scatter_record.attenuation * calculate_ray_color(scatter_record.scattered_ray, scene, depth - 1);
        }

        return direct_component;
    }

    return calculate_sky_color(ray);
}

Color render_pixel(int col, int row, const RenderConfig& config,
                   const Camera& camera, const Scene& scene, int max_depth) {
    Color accumulated_color(0, 0, 0);

    for (int sample = 0; sample < config.samples_per_pixel; ++sample) {
        const double horizontal_coord = (col + random_double()) / (config.image_width - 1);
        const double vertical_coord = (row + random_double()) / (config.image_height - 1);

        const Vec3 ray_direction = camera.lower_left_corner
            + horizontal_coord * camera.horizontal
            + vertical_coord * camera.vertical
            - camera.origin;
        const Ray ray(camera.origin, ray_direction);

        accumulated_color += calculate_ray_color(ray, scene, max_depth);
    }

    const double scale = 1.0 / config.samples_per_pixel;
    return scale * accumulated_color;
}

std::vector<unsigned char> render_image(const RenderConfig& config,
                                        const Camera& camera,
                                        const Scene& scene,
                                        int max_depth) {
    std::vector<unsigned char> image_data;
    const std::size_t total_pixels =
        static_cast<std::size_t>(config.image_width) * static_cast<std::size_t>(config.image_height);
    image_data.reserve(total_pixels * 3);

    std::cerr << "Rendering scene with " << scene.object_count() << " objects and "
              << scene.light_count() << " lights...\n";
    std::cerr << "Image size: " << config.image_width << "x" << config.image_height << "\n";
    std::cerr << "Using " << config.samples_per_pixel << " samples per pixel for antialiasing\n";
    std::cerr << "Maximum ray bounce depth: " << max_depth << "\n";

    for (int row = config.image_height - 1; row >= 0; --row) {
        std::cerr << "\rScanlines remaining: " << row << ' ' << std::flush;

        for (int col = 0; col < config.image_width; ++col) {
            const Color pixel_color = render_pixel(col, row, config, camera, scene, max_depth);
            write_color(image_data, pixel_color);
        }
    }

    std::cerr << "\n";
    return image_data;
}

