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

Vec3 random_cosine_direction(const Vec3& normal) {
    // Generate random point on unit disk using polar coordinates
    const double r = std::sqrt(random_double());
    const double theta = 2 * M_PI * random_double();
    const double x = r * std::cos(theta);
    const double y = r * std::sin(theta);
    const double z = std::sqrt(1 - r * r);  // Height on hemisphere
    
    // Build orthonormal basis around the normal
    // Find a vector not parallel to normal
    Vec3 a = (std::fabs(normal.x()) > 0.9) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
    Vec3 tangent = unit_vector(cross(normal, a));
    Vec3 bitangent = cross(normal, tangent);
    
    // Transform random point to world space
    return unit_vector(tangent * x + bitangent * y + normal * z);
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
