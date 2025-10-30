#ifndef SPHERE_H
#define SPHERE_H

#include "Hittable.h"
#include "Material.h"
#include "Vec3.h"
#include <memory>

/**
 * A sphere in 3D space.
 * Defined by a center point, radius, and material.
 */
class Sphere : public Hittable {
public:
    Point3 center_position;
    double radius;
    std::shared_ptr<Material> material_ptr;
    
    Sphere() {}
    
    Sphere(Point3 center, double sphere_radius, std::shared_ptr<Material> material)
        : center_position(center), radius(sphere_radius), material_ptr(material) {}
    
    /**
     * Check if a ray hits this sphere.
     * Uses the mathematical sphere equation to solve for intersection points.
     * 
     * Math explanation:
     * A sphere is all points at distance 'radius' from 'center'.
     * A ray is: point(t) = origin + t * direction
     * We solve: |point(t) - center|² = radius²
     */
    bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const override {
        // Vector from ray origin to sphere center
        Vec3 origin_to_center = ray.origin() - center_position;
        
        // Solve quadratic equation: at² + bt + c = 0
        // These coefficients come from expanding the sphere equation
        double quadratic_a = ray.direction().length_squared();
        double quadratic_half_b = dot(origin_to_center, ray.direction());
        double quadratic_c = origin_to_center.length_squared() - radius * radius;
        
        // Discriminant tells us if there are solutions (intersections)
        double discriminant = quadratic_half_b * quadratic_half_b - quadratic_a * quadratic_c;
        
        // If discriminant < 0, no intersection (ray misses sphere)
        if (discriminant < 0) {
            return false;
        }
        
        double sqrt_discriminant = std::sqrt(discriminant);
        
        // Try the closer intersection point first
        double intersection_distance = (-quadratic_half_b - sqrt_discriminant) / quadratic_a;
        
        // Check if this intersection is in the valid range
        if (intersection_distance < min_distance || max_distance < intersection_distance) {
            // Try the farther intersection point
            intersection_distance = (-quadratic_half_b + sqrt_discriminant) / quadratic_a;
            
            if (intersection_distance < min_distance || max_distance < intersection_distance) {
                // Both intersections are outside the valid range
                return false;
            }
        }
        
        // We have a valid hit! Record the details
        record.distance_from_ray = intersection_distance;
        record.hit_point = ray.at(intersection_distance);
        record.material_ptr = material_ptr;  // Store the material pointer
        
        // Normal points from center to hit point
        Vec3 outward_normal = (record.hit_point - center_position) / radius;
        record.set_face_normal(ray, outward_normal);
        
        return true;
    }
};

#endif
