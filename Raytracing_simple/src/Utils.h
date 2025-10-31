#ifndef UTILS_H
#define UTILS_H

#include "Vec3.h"
#include <random>
#include <cmath>

/**
 * Generate a random double in the range [0, 1).
 * Uses a static generator for efficiency.
 */
double random_double();

/**
 * Generate a random double in a specific range.
 * 
 * @param min Minimum value (inclusive)
 * @param max Maximum value (exclusive)
 */
double random_double(double min, double max);

/**
 * Generate a random vector inside a unit sphere.
 * Used for diffuse material scattering.
 */
Vec3 random_in_unit_sphere();

/**
 * Generate a random unit vector (length = 1).
 * Used for random scattering directions.
 */
Vec3 random_unit_vector();

/**
 * Check if a vector is very close to zero in all dimensions.
 * Used to catch degenerate cases.
 */
bool is_near_zero(const Vec3& vector);

/**
 * Reflect a vector around a normal.
 * Like a ball bouncing off a wall.
 * 
 * @param incoming The incoming vector
 * @param normal The surface normal (should be unit length)
 * @return The reflected vector
 */
Vec3 reflect(const Vec3& incoming, const Vec3& normal);

/**
 * Refract a vector through a surface (Snell's law).
 * Like light bending when entering water.
 * 
 * @param incoming The incoming unit vector
 * @param normal The surface normal (unit length)
 * @param refraction_ratio Ratio of refractive indices (n1/n2)
 * @return The refracted vector
 */
Vec3 refract(const Vec3& incoming, const Vec3& normal, double refraction_ratio);

#endif
