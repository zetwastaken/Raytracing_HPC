#ifndef AXIS_ALIGNED_RECT_H
#define AXIS_ALIGNED_RECT_H

#include "Hittable.h"
#include "Material.h"
#include "Ray.h"
#include "Vec3.h"

#include <cmath>
#include <memory>
#include <utility>

/**
 * Describes how a rectangle sits in 3D space: which axes span it and
 * which axis supplies the outward normal.
 */
struct RectOrientation {
    Axis tangent_u;
    Axis tangent_v;
    Axis normal_axis;
    Vec3 base_normal;

    Vec3 outward_normal(bool flip) const {
        return flip ? base_normal.negate() : base_normal;
    }
};

/**
 * Generic axis-aligned rectangle that supports any orientation.
 */
class AxisAlignedRect : public Hittable {
public:
    AxisAlignedRect(const RectOrientation& orientation_in,
                    double u0_in,
                    double u1_in,
                    double v0_in,
                    double v1_in,
                    double k_in,
                    std::shared_ptr<Material> material,
                    bool flip = false);

    bool hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const override;

protected:
    const RectOrientation orientation;
    const double u0;
    const double u1;
    const double v0;
    const double v1;
    const double k;
    std::shared_ptr<Material> material_ptr;
    bool flip_normal;
};

/**
 * Axis-aligned rectangle lying on the XY plane at constant Z.
 */
class XYRect : public AxisAlignedRect {
public:
    XYRect(double x0_in,
           double x1_in,
           double y0_in,
           double y1_in,
           double k_in,
           std::shared_ptr<Material> material,
           bool flip = false)
        : AxisAlignedRect(kOrientation, x0_in, x1_in, y0_in, y1_in, k_in, std::move(material), flip)
    {}

private:
    static const RectOrientation kOrientation;
};

/**
 * Axis-aligned rectangle lying on the XZ plane at constant Y.
 */
class XZRect : public AxisAlignedRect {
public:
    XZRect(double x0_in,
           double x1_in,
           double z0_in,
           double z1_in,
           double k_in,
           std::shared_ptr<Material> material,
           bool flip = false)
        : AxisAlignedRect(kOrientation, x0_in, x1_in, z0_in, z1_in, k_in, std::move(material), flip)
    {}

private:
    static const RectOrientation kOrientation;
};

/**
 * Axis-aligned rectangle lying on the YZ plane at constant X.
 */
class YZRect : public AxisAlignedRect {
public:
    YZRect(double y0_in,
           double y1_in,
           double z0_in,
           double z1_in,
           double k_in,
           std::shared_ptr<Material> material,
           bool flip = false)
        : AxisAlignedRect(kOrientation, y0_in, y1_in, z0_in, z1_in, k_in, std::move(material), flip)
    {}

private:
    static const RectOrientation kOrientation;
};

#endif
