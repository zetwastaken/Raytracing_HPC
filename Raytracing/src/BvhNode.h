#ifndef BVH_NODE_H
#define BVH_NODE_H

/**
 * @file BvhNode.h
 * @brief Bounding volume hierarchy node for accelerating ray-scene queries.
 */

#include "Aabb.h"
#include "Hittable.h"
#include "Utils.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>

inline bool box_compare(const std::shared_ptr<Hittable>& first,
                        const std::shared_ptr<Hittable>& second,
                        Axis axis) {
    Aabb box_first;
    Aabb box_second;

    if (!first->bounding_box(box_first) || !second->bounding_box(box_second)) {
        throw std::runtime_error("Hittable object missing bounding box for BVH construction.");
    }

    return box_first.min().component(axis) < box_second.min().component(axis);
}

class BvhNode : public Hittable {
public:
    BvhNode() = default;

    explicit BvhNode(std::vector<std::shared_ptr<Hittable>> objects)
        : BvhNode(objects, 0, objects.size()) {}

    BvhNode(std::vector<std::shared_ptr<Hittable>>& objects, std::size_t start, std::size_t end);

    bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const override;
    bool bounding_box(Aabb& output_box) const override {
        output_box = node_box;
        return true;
    }

private:
    std::shared_ptr<Hittable> left_child;
    std::shared_ptr<Hittable> right_child;
    Aabb node_box;
};

inline BvhNode::BvhNode(std::vector<std::shared_ptr<Hittable>>& objects,
                        std::size_t start,
                        std::size_t end) {
    const int axis_choice = static_cast<int>(random_double(0, 3));
    const Axis split_axis = static_cast<Axis>(axis_choice);

    auto comparator = [split_axis](const std::shared_ptr<Hittable>& first,
                                   const std::shared_ptr<Hittable>& second) {
        return box_compare(first, second, split_axis);
    };

    const std::size_t object_span = end - start;

    if (object_span == 1) {
        left_child = right_child = objects[start];
    } else if (object_span == 2) {
        if (comparator(objects[start], objects[start + 1])) {
            left_child = objects[start];
            right_child = objects[start + 1];
        } else {
            left_child = objects[start + 1];
            right_child = objects[start];
        }
    } else {
        std::sort(objects.begin() + static_cast<std::ptrdiff_t>(start),
                  objects.begin() + static_cast<std::ptrdiff_t>(end),
                  comparator);

        const std::size_t midpoint = start + object_span / 2;
        left_child = std::make_shared<BvhNode>(objects, start, midpoint);
        right_child = std::make_shared<BvhNode>(objects, midpoint, end);
    }

    Aabb left_box;
    Aabb right_box;
    if (!left_child->bounding_box(left_box) || !right_child->bounding_box(right_box)) {
        throw std::runtime_error("BVH child missing bounding box during construction.");
    }

    node_box = Aabb::surrounding_box(left_box, right_box);
}

inline bool BvhNode::hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const {
    if (!node_box.hit(ray, min_distance, max_distance)) {
        return false;
    }

    bool hit_left = left_child->hit(ray, min_distance, max_distance, record);
    if (hit_left) {
        max_distance = record.distance_from_ray;
    }

    bool hit_right = right_child->hit(ray, min_distance, max_distance, record);
    return hit_left || hit_right;
}

#endif
