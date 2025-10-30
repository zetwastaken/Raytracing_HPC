#ifndef HITTABLE_H
#define HITTABLE_H

#include "Ray.h"
#include "Vec3.h"
#include <memory>

// Forward declaration
class Material;

/**
 * Stores information about where a ray hit an object.
 * This tells us everything we need to know about the intersection point.
 */
struct HitRecord {
    Point3 hit_point;           // Where the ray hit the object
    Vec3 surface_normal;        // Direction perpendicular to the surface at hit point
    double distance_from_ray;   // How far along the ray the hit occurred
    bool is_front_face;         // Did we hit the front or back of the object?
    Color object_color;         // The color of the object that was hit
    std::shared_ptr<Material> material_ptr;  // Pointer to the material
    
    /**
     * Determine which side of the surface we hit and set the normal accordingly.
     * Normals always point "outward" from the surface, but we need to know
     * if the ray came from outside or inside the object.
     * 
     * @param ray The ray that hit this surface
     * @param outward_normal The normal pointing away from the object's center
     */
    void set_face_normal(const Ray& ray, const Vec3& outward_normal) {
        // If ray and normal point in same direction, we hit from inside
        is_front_face = dot(ray.direction(), outward_normal) < 0;
        
        // Make the normal always point against the ray direction
        surface_normal = is_front_face ? outward_normal : -outward_normal;
    }
};

/**
 * Base class for anything that can be hit by a ray.
 * This could be a sphere, plane, triangle, etc.
 */
class Hittable {
public:
    virtual ~Hittable() = default;
    
    /**
     * Check if a ray hits this object.
     * 
     * @param ray The ray to test
     * @param min_distance Don't count hits closer than this (avoids self-intersection)
     * @param max_distance Don't count hits farther than this (optimization)
     * @param record Where to store information about the hit (if it happens)
     * @return true if the ray hit this object, false otherwise
     */
    virtual bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const = 0;
};

#endif
