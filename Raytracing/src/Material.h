#ifndef MATERIAL_H
#define MATERIAL_H

/**
 * @file Material.h
 * @brief Surface interaction models for the ray tracer.
 */

#include "Ray.h"
#include "Vec3.h"
#include "Utils.h"
#include "Hittable.h"

/**
 * Information about how a ray scatters after hitting a surface.
 */
struct ScatterRecord {
    Ray scattered_ray;      // The new ray after scattering
    Color attenuation;      // How much color is absorbed (1,1,1 = no absorption)
    bool did_scatter;       // Did the ray scatter or was it absorbed?
};

/**
 * Base class for different material types.
 * Materials define how rays interact with surfaces.
 */
class Material {
public:
    virtual ~Material() = default;
    
    /**
     * Calculate how a ray scatters when it hits this material.
     * 
     * @param ray_in The incoming ray
     * @param hit_info Information about where the ray hit
     * @param scatter_record Output: information about the scattered ray
     * @return true if the ray scattered, false if it was absorbed
     */
    virtual bool scatter(const Ray& ray_in, const HitRecord& hit_info, 
                        ScatterRecord& scatter_record) const = 0;

    /**
     * Base surface color used for direct lighting computations.
     */
    virtual Color base_color() const = 0;

    /**
     * Flag indicating whether the material responds to direct diffuse lighting.
     */
    virtual bool is_diffuse() const { return false; }
};

/**
 * A matte (diffuse) material that scatters light in random directions.
 * This creates a rough, non-shiny surface like chalk or unpolished stone.
 * Uses cosine-weighted hemisphere sampling for reduced noise.
 */
class Matte : public Material {
public:
    Color surface_color;
    
    Matte(const Color& color) : surface_color(color) {}
    
    bool scatter(const Ray& ray_in, const HitRecord& hit_info, 
                ScatterRecord& scatter_record) const override {
        (void)ray_in;  // Not used for matte materials
        
        // Use cosine-weighted hemisphere sampling for better quality
        // This significantly reduces noise compared to random_unit_vector()
        Vec3 scatter_direction = random_cosine_direction(hit_info.surface_normal);
        
        // Catch degenerate scatter direction
        if (is_near_zero(scatter_direction)) {
            scatter_direction = hit_info.surface_normal;
        }
        
        scatter_record.scattered_ray = Ray(hit_info.hit_point, scatter_direction);
        scatter_record.attenuation = surface_color;
        scatter_record.did_scatter = true;
        
        return true;
    }

    Color base_color() const override {
        return surface_color;
    }

    bool is_diffuse() const override {
        return true;
    }
};

/**
 * A reflective (metal) material that reflects rays like a mirror.
 * The fuzziness parameter controls how perfect the reflection is.
 */
class Reflective : public Material {
public:
    Color surface_color;
    double fuzziness;  // 0 = perfect mirror, 1 = very fuzzy reflection
    
    Reflective(const Color& color, double fuzz = 0.0) 
        : surface_color(color), fuzziness(fuzz < 1 ? fuzz : 1) {}
    
    bool scatter(const Ray& ray_in, const HitRecord& hit_info, 
                ScatterRecord& scatter_record) const override {
        // Reflect the ray direction around the surface normal
        Vec3 reflected_direction = reflect(unit_vector(ray_in.direction()), 
                                          hit_info.surface_normal);
        
        // Add fuzziness by randomly perturbing the reflection
        reflected_direction = reflected_direction + fuzziness * random_unit_vector();
        
        scatter_record.scattered_ray = Ray(hit_info.hit_point, reflected_direction);
        scatter_record.attenuation = surface_color;
        scatter_record.did_scatter = dot(reflected_direction, hit_info.surface_normal) > 0;
        
        return scatter_record.did_scatter;
    }

    Color base_color() const override {
        return surface_color;
    }
};

/**
 * A transparent (dielectric) material like glass or water.
 * Can reflect and refract light based on the refractive index.
 */
class Transparent : public Material {
public:
    double refractive_index;  // 1.0 = air, 1.3 = water, 1.5 = glass, 2.4 = diamond
    
    Transparent(double refraction_index) : refractive_index(refraction_index) {}
    
    bool scatter(const Ray& ray_in, const HitRecord& hit_info, 
                ScatterRecord& scatter_record) const override {
        scatter_record.attenuation = Color(1.0, 1.0, 1.0);  // Glass doesn't absorb light
        
        // Calculate the ratio of refractive indices
        double refraction_ratio = hit_info.is_front_face 
            ? (1.0 / refractive_index)  // Air to glass
            : refractive_index;          // Glass to air
        
        Vec3 unit_direction = unit_vector(ray_in.direction());
        
        // Calculate cosine of angle between ray and normal
        double cos_theta = std::fmin(dot(-unit_direction, hit_info.surface_normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
        
        // Check if refraction is possible (or if we should reflect instead)
        bool cannot_refract = refraction_ratio * sin_theta > 1.0;
        Vec3 direction;
        
        if (cannot_refract || reflectance(cos_theta, refraction_ratio) > random_double()) {
            // Must reflect (total internal reflection or Fresnel reflection)
            direction = reflect(unit_direction, hit_info.surface_normal);
        } else {
            // Can refract
            direction = refract(unit_direction, hit_info.surface_normal, refraction_ratio);
        }
        
        scatter_record.scattered_ray = Ray(hit_info.hit_point, direction);
        scatter_record.did_scatter = true;
        
        return true;
    }

private:
    /**
     * Schlick's approximation for reflectance.
     * Calculates how much light is reflected vs refracted at different angles.
     */
    static double reflectance(double cosine, double refraction_index) {
        double r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow((1 - cosine), 5);
    }

public:
    Color base_color() const override {
        return Color(1.0, 1.0, 1.0);
    }
};

#endif
