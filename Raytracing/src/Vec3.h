#ifndef VEC3_H
#define VEC3_H

#include <cmath>
#include <iostream>

enum class Axis {
    X = 0,
    Y = 1,
    Z = 2
};

/**
 * A 3D vector class for representing positions, directions, and colors.
 * Stores three double values that can represent:
 * - A point in 3D space (x, y, z)
 * - A direction vector
 * - An RGB color (red, green, blue)
 */
class Vec3 {
public:
    // Store the three components
    double x_component;
    double y_component;
    double z_component;

    // ========== Constructors ==========
    
    // Create a zero vector (0, 0, 0)
    Vec3() : x_component(0), y_component(0), z_component(0) {}
    
    // Create a vector with specific values
    Vec3(double x, double y, double z) 
        : x_component(x), y_component(y), z_component(z) {}

    // ========== Get Individual Components ==========
    
    double x() const { return x_component; }
    double y() const { return y_component; }
    double z() const { return z_component; }

    // Fetch a component by axis enum (avoids magic indices when selecting coordinates).
    double component(Axis axis) const {
        switch (axis) {
        case Axis::X:
            return x_component;
        case Axis::Y:
            return y_component;
        default:
            return z_component;
        }
    }

    // ========== Make Vector Negative ==========
    
    // Example: -(1, 2, 3) becomes (-1, -2, -3)
    Vec3 negate() const { 
        return Vec3(-x_component, -y_component, -z_component); 
    }
    
    Vec3 operator-() const { 
        return negate();
    }

    // ========== Modify This Vector ==========
    
    // Add another vector to this one
    // Example: (1, 2, 3) += (4, 5, 6) makes this vector (5, 7, 9)
    Vec3& add(const Vec3& other) {
        x_component += other.x_component;
        y_component += other.y_component;
        z_component += other.z_component;
        return *this;
    }
    
    Vec3& operator+=(const Vec3& other) {
        return add(other);
    }

    // Multiply this vector by a number
    // Example: (1, 2, 3) *= 2 makes this vector (2, 4, 6)
    Vec3& scale(double factor) {
        x_component *= factor;
        y_component *= factor;
        z_component *= factor;
        return *this;
    }
    
    Vec3& operator*=(double factor) {
        return scale(factor);
    }

    // Divide this vector by a number
    // Example: (2, 4, 6) /= 2 makes this vector (1, 2, 3)
    Vec3& divide(double divisor) {
        double scale_factor = 1.0 / divisor;
        return scale(scale_factor);
    }
    
    Vec3& operator/=(double divisor) {
        return divide(divisor);
    }

    // ========== Vector Length ==========
    
    // Get how long this vector is
    // Example: (3, 4, 0) has length 5
    double length() const {
        return std::sqrt(length_squared());
    }

    // Get the squared length (faster - no square root needed)
    double length_squared() const {
        return x_component * x_component + 
               y_component * y_component + 
               z_component * z_component;
    }
};

// ========== Type Aliases ==========

using Point3 = Vec3;   // Represents a 3D point in space
using Color = Vec3;    // Represents RGB color (red=x, green=y, blue=z)


// ==========================================================================
// Vec3 Utility Functions - Work With Vectors
// ==========================================================================

/**
 * Print a vector to the console.
 * Example output: "1.5 2.0 3.5"
 */
std::ostream& operator<<(std::ostream& output_stream, const Vec3& vector);


// ========== Create New Vectors from Two Vectors ==========

/**
 * Add two vectors together.
 * Example: (1, 2, 3) + (4, 5, 6) = (5, 7, 9)
 */
Vec3 operator+(const Vec3& first, const Vec3& second);

/**
 * Subtract one vector from another.
 * Example: (5, 7, 9) - (1, 2, 3) = (4, 5, 6)
 */
Vec3 operator-(const Vec3& first, const Vec3& second);

/**
 * Multiply two vectors component-by-component (Hadamard product).
 * Useful for blending colors together.
 * Example: (2, 3, 4) * (1, 2, 3) = (2, 6, 12)
 */
Vec3 operator*(const Vec3& first, const Vec3& second);


// ========== Scale Vectors by Numbers ==========

/**
 * Multiply a vector by a number (makes it longer/shorter).
 * Example: 3 * (1, 2, 3) = (3, 6, 9)
 */
Vec3 operator*(double scale_factor, const Vec3& vector);

/**
 * Multiply a vector by a number (same as above, just reversed order).
 * Example: (1, 2, 3) * 3 = (3, 6, 9)
 */
Vec3 operator*(const Vec3& vector, double scale_factor);

/**
 * Divide a vector by a number (makes it shorter).
 * Example: (6, 9, 12) / 3 = (2, 3, 4)
 */
Vec3 operator/(const Vec3& vector, double divisor);


// ========== Special Vector Math Operations ==========

/**
 * DOT PRODUCT - How much two vectors point in the same direction.
 * Returns a single number (not a vector).
 * - Positive = vectors point in similar directions
 * - Zero = vectors are perpendicular (90 degrees)
 * - Negative = vectors point in opposite directions
 * 
 * Example: dot((1, 0, 0), (1, 0, 0)) = 1
 *          dot((1, 0, 0), (0, 1, 0)) = 0
 */
double dot(const Vec3& first, const Vec3& second);

/**
 * CROSS PRODUCT - Create a vector perpendicular to two other vectors.
 * The result points "out of the plane" formed by the two input vectors.
 * Used to find normals (perpendicular directions) in 3D graphics.
 */
Vec3 cross(const Vec3& first, const Vec3& second);

/**
 * NORMALIZE - Make a vector have length 1 (but keep the same direction).
 * This is called a "unit vector" and is useful for directions.
 * 
 * Example: unit_vector((3, 4, 0)) = (0.6, 0.8, 0) with length = 1
 */
Vec3 unit_vector(const Vec3& vector);

#endif
