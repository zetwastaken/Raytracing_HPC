#ifndef LIGHT_H
#define LIGHT_H

#include "Vec3.h"

/**
 * Point light represented by a position and RGB intensity.
 * Intensity is treated as linear energy contribution (larger values shine brighter).
 */
struct Light {
    Point3 position;
    Color intensity;

    Light() = default;
    Light(const Point3& pos, const Color& power) 
        : position(pos)
        , intensity(power) 
    {}
};

#endif
