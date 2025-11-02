#ifndef BOX_H
#define BOX_H

/**
 * @file Box.h
 * @brief Axis-aligned box primitive composed of rectangular faces.
 */

#include "AxisAlignedRect.h"
#include "HittableList.h"

#include <memory>

/**
 * Axis-aligned box constructed from six rectangles.
 */
class Box : public Hittable {
public:
    Point3 minimum_corner;
    Point3 maximum_corner;
    HittableList sides;

    Box() = default;

    Box(const Point3& min_point, const Point3& max_point, std::shared_ptr<Material> material)
        : minimum_corner(min_point)
        , maximum_corner(max_point)
    {
        auto shared_material = std::move(material);

        sides.add(std::make_shared<XYRect>(
            minimum_corner.x(), maximum_corner.x(),
            minimum_corner.y(), maximum_corner.y(),
            maximum_corner.z(), shared_material
        ));
        sides.add(std::make_shared<XYRect>(
            minimum_corner.x(), maximum_corner.x(),
            minimum_corner.y(), maximum_corner.y(),
            minimum_corner.z(), shared_material, true
        ));

        sides.add(std::make_shared<XZRect>(
            minimum_corner.x(), maximum_corner.x(),
            minimum_corner.z(), maximum_corner.z(),
            maximum_corner.y(), shared_material
        ));
        sides.add(std::make_shared<XZRect>(
            minimum_corner.x(), maximum_corner.x(),
            minimum_corner.z(), maximum_corner.z(),
            minimum_corner.y(), shared_material, true
        ));

        sides.add(std::make_shared<YZRect>(
            minimum_corner.y(), maximum_corner.y(),
            minimum_corner.z(), maximum_corner.z(),
            maximum_corner.x(), shared_material, true
        ));
        sides.add(std::make_shared<YZRect>(
            minimum_corner.y(), maximum_corner.y(),
            minimum_corner.z(), maximum_corner.z(),
            minimum_corner.x(), shared_material
        ));
    }

    bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const override {
        return sides.hit(ray, min_distance, max_distance, record);
    }
};

#endif
