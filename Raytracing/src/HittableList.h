#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H

/**
 * @file HittableList.h
 * @brief Container for ray-intersectable scene geometry.
 */

#include "Hittable.h"
#include "BvhNode.h"

#include <memory>
#include <vector>

/**
 * A collection of objects that can be hit by rays.
 * This represents our entire 3D scene.
 */
class HittableList : public Hittable {
public:
    std::vector<std::shared_ptr<Hittable>> objects;
    std::unique_ptr<Hittable> accelerator;
    bool use_bvh = false;
    
    HittableList() {}
    HittableList(std::shared_ptr<Hittable> object) { add(object); }
    
    /**
     * Remove all objects from the scene.
     */
    void clear() {
        objects.clear();
        accelerator.reset();
    }
    
    /**
     * Add an object to the scene.
     */
    void add(std::shared_ptr<Hittable> object) {
        objects.push_back(object);
        accelerator.reset();
    }

    /**
     * Enable or disable BVH acceleration. When disabled, the list will use
     * the original linear hit loop.
     */
    void set_acceleration_enabled(bool enabled = true) {
        use_bvh = enabled;
        if (!enabled) {
            accelerator.reset();
        }
    }
    
    /**
     * Build an acceleration structure for faster hit checks.
     */
    void build_bvh() {
        if (!use_bvh || objects.empty()) {
            accelerator.reset();
            return;
        }

        // Copy the list to allow in-place sorting while preserving original ordering.
        std::vector<std::shared_ptr<Hittable>> sortable_objects = objects;
        accelerator = std::make_unique<BvhNode>(sortable_objects);
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
        if (use_bvh && accelerator) {
            return accelerator->hit(ray, min_distance, max_distance, record);
        }

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

    bool bounding_box(Aabb& output_box) const override {
        if (objects.empty()) {
            return false;
        }

        Aabb temp_box;
        bool first_box = true;

        for (const auto& object : objects) {
            if (!object->bounding_box(temp_box)) {
                return false;
            }

            output_box = first_box ? temp_box : Aabb::surrounding_box(output_box, temp_box);
            first_box = false;
        }

        return true;
    }
};

#endif
