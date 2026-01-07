#ifndef AABB_H
#define AABB_H

/**
 * @file Aabb.h
 * @brief Axis-aligned bounding box helper used for acceleration structures.
 */

#include "Ray.h"
#include "Vec3.h"

#include <algorithm>

/**
 * Simple axis-aligned bounding box.
 * Provides fast slab-based intersection testing with a ray.
 */
class Aabb {
public:
    Aabb() = default;
    Aabb(const Point3& min_point, const Point3& max_point)
        : minimum_point(min_point)
        , maximum_point(max_point)
    {}

    const Point3& min() const { return minimum_point; }
    const Point3& max() const { return maximum_point; }

    /**
     * Test if a ray hits the box between the provided distances.
     * Uses the optimized slab method to cull misses quickly.
     */
    bool hit(const Ray& ray, double t_min, double t_max) const {
        for (int axis_index = 0; axis_index < 3; ++axis_index) {
            const Axis axis = static_cast<Axis>(axis_index);
            const double inv_direction = 1.0 / ray.direction().component(axis);

            double t0 = (minimum_point.component(axis) - ray.origin().component(axis)) * inv_direction;
            double t1 = (maximum_point.component(axis) - ray.origin().component(axis)) * inv_direction;

            if (inv_direction < 0.0) {
                std::swap(t0, t1);
            }

            t_min = t0 > t_min ? t0 : t_min;
            t_max = t1 < t_max ? t1 : t_max;

            if (t_max <= t_min) {
                return false;
            }
        }

        return true;
    }

    /**
     * Build the smallest box that contains both provided boxes.
     */
    static Aabb surrounding_box(const Aabb& first, const Aabb& second) {
        const Point3 small_point(
            std::min(first.min().x(), second.min().x()),
            std::min(first.min().y(), second.min().y()),
            std::min(first.min().z(), second.min().z())
        );

        const Point3 big_point(
            std::max(first.max().x(), second.max().x()),
            std::max(first.max().y(), second.max().y()),
            std::max(first.max().z(), second.max().z())
        );

        return Aabb(small_point, big_point);
    }

private:
    Point3 minimum_point;
    Point3 maximum_point;
};

#endif
