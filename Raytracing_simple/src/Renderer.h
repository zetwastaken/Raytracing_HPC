#ifndef RENDERER_H
#define RENDERER_H

#include "InlineControl.h"
#include "Camera.h"
#include "Color.h"
#include "Hittable.h"
#include "Material.h"
#include "Ray.h"
#include "RenderConfig.h"
#include "Scene.h"
#include "Utils.h"
#include "Vec3.h"

#include <cmath>
#include <iostream>
#include <vector>

/**
 * Calculate the sky gradient color based on ray direction.
 */
RAYTRACER_INLINE Color calculate_sky_color(const Ray& ray) {
    Vec3 unit_direction = unit_vector(ray.direction());
    
    // Map y-coordinate from [-1, 1] to [0, 1] for blending
    double blend_factor = 0.5 * (unit_direction.y() + 1.0);
    
    // Blend between white (horizon) and sky blue (zenith)
    const Color white(1.0, 1.0, 1.0);
    const Color sky_blue(0.5, 0.7, 1.0);
    
    return (1.0 - blend_factor) * white + blend_factor * sky_blue;
}

RAYTRACER_INLINE Color compute_diffuse_lighting(const Scene& scene, const HitRecord& hit_info) {
    if (scene.lights.empty()) {
        return Color(0.0, 0.0, 0.0);
    }

    Color accumulated_light(0.0, 0.0, 0.0);
    constexpr double shadow_bias = 0.001;

    for (const auto& light : scene.lights) {
        Vec3 to_light = light.position - hit_info.hit_point;
        double distance_squared = to_light.length_squared();
        if (distance_squared <= 0.0) {
            continue;
        }

        Vec3 light_direction = unit_vector(to_light);
        double n_dot_l = dot(hit_info.surface_normal, light_direction);
        if (n_dot_l <= 0.0) {
            continue;
        }

        double distance_to_light = std::sqrt(distance_squared);
        Ray shadow_ray(hit_info.hit_point + shadow_bias * hit_info.surface_normal, light_direction);
        HitRecord shadow_hit;
        if (scene.objects.hit(shadow_ray, shadow_bias, distance_to_light - shadow_bias, shadow_hit)) {
            continue;
        }

        Color light_energy = light.intensity / distance_squared;
        accumulated_light += n_dot_l * light_energy;
    }

    return accumulated_light;
}

/**
 * Calculate the color for a ray with recursive ray tracing.
 * Handles reflections, refractions, and material interactions.
 * Adds direct diffuse lighting from all visible light sources.
 * 
 * @param ray The ray we're tracing
 * @param scene The scene containing objects and lights
 * @param depth Current recursion depth (prevents infinite bounces)
 * @return The color for this ray
 */
RAYTRACER_INLINE Color calculate_ray_color(const Ray& ray, const Scene& scene, int depth) {
    // If we've exceeded the ray bounce limit, no more light is gathered
    if (depth <= 0) {
        return Color(0, 0, 0);
    }
    
    HitRecord hit_info;
    
    // Check if the ray hits any object in the scene
    const double min_hit_distance = 0.001;  // Avoid numerical precision issues (shadow acne)
    const double max_hit_distance = 1000000.0;
    
    if (scene.objects.hit(ray, min_hit_distance, max_hit_distance, hit_info)) {
        const Material& material = *hit_info.material_ptr;
        Color direct_component(0.0, 0.0, 0.0);

        if (material.is_diffuse()) {
            Color incoming_light = compute_diffuse_lighting(scene, hit_info);
            direct_component = material.base_color() * incoming_light;
        }

        ScatterRecord scatter_record;
        
        if (material.scatter(ray, hit_info, scatter_record)) {
            // Material scattered the ray - trace the scattered ray recursively
            return direct_component + scatter_record.attenuation * calculate_ray_color(scatter_record.scattered_ray, scene, depth - 1);
        }
        
        // Material absorbed the ray completely
        return direct_component;
    }
    
    // Ray didn't hit anything - show sky gradient
    return calculate_sky_color(ray);
}

/**
 * Render a single pixel by casting multiple rays through it (antialiasing).
 * Takes multiple samples per pixel and averages them for smoother edges.
 */
RAYTRACER_INLINE Color render_pixel(int col, int row, const RenderConfig& config,
                          const Camera& camera, const Scene& scene, int max_depth) {
    
    Color accumulated_color(0, 0, 0);
    
    // Take multiple samples within this pixel
    for (int sample = 0; sample < config.samples_per_pixel; ++sample) {
        // Add random offset within the pixel for antialiasing
        double horizontal_coord = (col + random_double()) / (config.image_width - 1);
        double vertical_coord = (row + random_double()) / (config.image_height - 1);
        
        // Create ray from camera through this sample point
        Vec3 ray_direction = camera.lower_left_corner + 
                            horizontal_coord * camera.horizontal + 
                            vertical_coord * camera.vertical - 
                            camera.origin;
        Ray ray(camera.origin, ray_direction);
        
        // Accumulate the color from this sample (with recursive ray tracing)
        accumulated_color += calculate_ray_color(ray, scene, max_depth);
    }
    
    // Average all the samples
    double scale = 1.0 / config.samples_per_pixel;
    return scale * accumulated_color;
}

/**
 * Render the entire image with antialiasing and recursive ray tracing.
 */
RAYTRACER_INLINE std::vector<unsigned char> render_image(const RenderConfig& config,
                                               const Camera& camera,
                                               const Scene& scene,
                                               int max_depth) {
    std::vector<unsigned char> image_data;
    const std::size_t total_pixels = 
        static_cast<std::size_t>(config.image_width) * static_cast<std::size_t>(config.image_height);
    image_data.reserve(total_pixels * 3);  // 3 bytes per pixel (RGB)
    
    std::cerr << "Rendering scene with " << scene.object_count() << " objects and "
              << scene.light_count() << " lights...\n";
    std::cerr << "Image size: " << config.image_width << "x" << config.image_height << "\n";
    std::cerr << "Using " << config.samples_per_pixel << " samples per pixel for antialiasing\n";
    std::cerr << "Maximum ray bounce depth: " << max_depth << "\n";
    
    // Iterate through each pixel from top to bottom, left to right
    for (int row = config.image_height - 1; row >= 0; --row) {
        std::cerr << "\rScanlines remaining: " << row << ' ' << std::flush;
        
        for (int col = 0; col < config.image_width; ++col) {
            Color pixel_color = render_pixel(col, row, config, camera, scene, max_depth);
            write_color(image_data, pixel_color);
        }
    }
    
    std::cerr << "\n";
    return image_data;
}

#endif
