#include "Vec3.h"

#include <ostream>

std::ostream& operator<<(std::ostream& output_stream, const Vec3& vector) {
    output_stream << vector.x_component << ' '
                  << vector.y_component << ' '
                  << vector.z_component;
    return output_stream;
}

Vec3 operator+(const Vec3& first, const Vec3& second) {
    return Vec3(
        first.x_component + second.x_component,
        first.y_component + second.y_component,
        first.z_component + second.z_component
    );
}

Vec3 operator-(const Vec3& first, const Vec3& second) {
    return Vec3(
        first.x_component - second.x_component,
        first.y_component - second.y_component,
        first.z_component - second.z_component
    );
}

Vec3 operator*(const Vec3& first, const Vec3& second) {
    return Vec3(
        first.x_component * second.x_component,
        first.y_component * second.y_component,
        first.z_component * second.z_component
    );
}

Vec3 operator*(double scale_factor, const Vec3& vector) {
    return Vec3(
        scale_factor * vector.x_component,
        scale_factor * vector.y_component,
        scale_factor * vector.z_component
    );
}

Vec3 operator*(const Vec3& vector, double scale_factor) {
    return scale_factor * vector;
}

Vec3 operator/(const Vec3& vector, double divisor) {
    const double scale_factor = 1.0 / divisor;
    return scale_factor * vector;
}

double dot(const Vec3& first, const Vec3& second) {
    return first.x_component * second.x_component
         + first.y_component * second.y_component
         + first.z_component * second.z_component;
}

Vec3 cross(const Vec3& first, const Vec3& second) {
    return Vec3(
        first.y_component * second.z_component - first.z_component * second.y_component,
        first.z_component * second.x_component - first.x_component * second.z_component,
        first.x_component * second.y_component - first.y_component * second.x_component
    );
}

Vec3 unit_vector(const Vec3& vector) {
    return vector / vector.length();
}

