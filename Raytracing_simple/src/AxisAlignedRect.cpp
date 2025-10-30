#include "AxisAlignedRect.h"

#include <cmath>
#include <utility>

AxisAlignedRect::AxisAlignedRect(const RectOrientation& orientation_in,
                                 double u0_in,
                                 double u1_in,
                                 double v0_in,
                                 double v1_in,
                                 double k_in,
                                 std::shared_ptr<Material> material,
                                 bool flip)
    : orientation(orientation_in)
    , u0(u0_in)
    , u1(u1_in)
    , v0(v0_in)
    , v1(v1_in)
    , k(k_in)
    , material_ptr(std::move(material))
    , flip_normal(flip)
{}

#if defined(__clang__)
[[clang::noinline]]
#endif
bool AxisAlignedRect::hit(const Ray& ray, double min_distance, double max_distance, HitRecord& record) const {
    const Point3 origin = ray.origin();
    const Vec3 direction = ray.direction();

    const double denominator = direction.component(orientation.normal_axis);
    if (std::fabs(denominator) < 1e-8) {
        return false;
    }

    const double offset_along_normal = k - origin.component(orientation.normal_axis);
    const double t = offset_along_normal / denominator;
    if (t < min_distance || t > max_distance) {
        return false;
    }

    const double u_coord = origin.component(orientation.tangent_u) + t * direction.component(orientation.tangent_u);
    const double v_coord = origin.component(orientation.tangent_v) + t * direction.component(orientation.tangent_v);
    if (u_coord < u0 || u_coord > u1 || v_coord < v0 || v_coord > v1) {
        return false;
    }

    record.distance_from_ray = t;
    record.hit_point = ray.at(t);
    record.material_ptr = material_ptr;

    const Vec3 outward_normal = orientation.outward_normal(flip_normal);
    record.set_face_normal(ray, outward_normal);
    return true;
}

