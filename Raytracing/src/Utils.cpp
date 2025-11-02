#include "Utils.h"

#include <cmath>
#include <random>

namespace {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    std::mt19937 generator;
}

double random_double() {
    return distribution(generator);
}

double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

Vec3 random_in_unit_sphere() {
    while (true) {
        const Vec3 point(random_double(-1, 1), random_double(-1, 1), random_double(-1, 1));
        if (point.length_squared() < 1) {
            return point;
        }
    }
}

Vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

bool is_near_zero(const Vec3& vector) {
    constexpr double epsilon = 1e-8;
    return (std::fabs(vector.x()) < epsilon) &&
           (std::fabs(vector.y()) < epsilon) &&
           (std::fabs(vector.z()) < epsilon);
}

Vec3 reflect(const Vec3& incoming, const Vec3& normal) {
    return incoming - 2 * dot(incoming, normal) * normal;
}

Vec3 refract(const Vec3& incoming, const Vec3& normal, double refraction_ratio) {
    const double cos_theta = std::fmin(dot(-incoming, normal), 1.0);
    const Vec3 refracted_perpendicular = refraction_ratio * (incoming + cos_theta * normal);
    const Vec3 refracted_parallel =
        -std::sqrt(std::fabs(1.0 - refracted_perpendicular.length_squared())) * normal;
    return refracted_perpendicular + refracted_parallel;
}
