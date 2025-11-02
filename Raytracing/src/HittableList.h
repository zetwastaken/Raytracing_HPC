#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H

#include "Hittable.h"

#include <memory>
#include <vector>

/**
 * A collection of objects that can be hit by rays.
 * This represents our entire 3D scene.
 */
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;
    
    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }
    
    /**
     * Remove all objects from the scene.
     */
    void clear() {
        objects.clear();
    }
    
    /**
     * Add an object to the scene.
     */
    void add(std::shared_ptr<Hittable> object) {
        objects.push_back(object);
    }
    
    /**
     * Check if a ray hits any object in the scene.
     * Returns information about the CLOSEST hit.
     * 
     * @param ray The ray to test
     * @param min_distance Ignore hits closer than this
     * @param max_distance Ignore hits farther than this
     * @param record Store hit information here
     * @return true if ray hit something, false if it hit nothing
     */
    bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const override {
        HitRecord temp_record;
        bool hit_anything = false;
        double closest_so_far = max_distance;
        
        // Check every object in the scene
        for (const auto& object : objects) {
            // Did the ray hit this object?
            if (object->hit(ray, min_distance, closest_so_far, temp_record)) {
                hit_anything = true;
                closest_so_far = temp_record.distance_from_ray;
                record = temp_record;
            }
        }
        
        return hit_anything;
    }
};

#endif
