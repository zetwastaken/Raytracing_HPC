#ifndef UTILS_H
#define UTILS_H

#include "Vec3.h"
#include <random>
#include <cmath>

/**
 * Generate a random double in the range [0, 1).
 * Uses a static generator for efficiency.
 */
inline double random_double() {
    static std::uniform_real_distribution<double> distribution(0.0, 1.0);
    static std::mt19937 generator;
    return distribution(generator);
}

/**
 * Generate a random double in a specific range.
 * 
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 */
inline double random_double(double min, double max) {
    return min + (max - min) * random_double();
}

/**
 * Generate a random vector inside a unit sphere.
 * Used for diffuse material scattering.
 */
inline Vec3 random_in_unit_sphere() {
    while (true) {
        Vec3 point = Vec3(random_double(-1, 1), random_double(-1, 1), random_double(-1, 1));
        if (point.length_squared() < 1) {
            return point;
        }
    }
}

/**
 * Generate a random unit vector (length = 1).
 * Used for random scattering directions.
 */
inline Vec3 random_unit_vector() {
    return unit_vector(random_in_unit_sphere());
}

/**
 * Check if a vector is very close to zero in all dimensions.
 * Used to catch degenerate cases.
 */
inline bool is_near_zero(const Vec3& vector) {
    const double epsilon = 1e-8;
    return (std::fabs(vector.x()) < epsilon) && 
           (std::fabs(vector.y()) < epsilon) && 
           (std::fabs(vector.z()) < epsilon);
}

/**
 * Reflect a vector around a normal.
 * Like a ball bouncing off a wall.
 * 
 * @param incoming The incoming vector
 * @param normal The surface normal (should be unit length)
 * @return The reflected vector
 */
inline Vec3 reflect(const Vec3& incoming, const Vec3& normal) {
    return incoming - 2 * dot(incoming, normal) * normal;
}

/**
 * Refract a vector through a surface (Snell's law).
 * Like light bending when entering water.
 * 
 * @param incoming The incoming unit vector
 * @param normal The surface normal (unit length)
 * @param refraction_ratio Ratio of refractive indices (n1/n2)
 * @return The refracted vector
 */
inline Vec3 refract(const Vec3& incoming, const Vec3& normal, double refraction_ratio) {
    double cos_theta = std::fmin(dot(-incoming, normal), 1.0);
    Vec3 refracted_perpendicular = refraction_ratio * (incoming + cos_theta * normal);
    Vec3 refracted_parallel = -std::sqrt(std::fabs(1.0 - refracted_perpendicular.length_squared())) * normal;
    return refracted_perpendicular + refracted_parallel;
}

#endif
