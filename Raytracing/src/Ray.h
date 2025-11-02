#ifndef RAY_H
#define RAY_H

/**
 * @file Ray.h
 * @brief Parametric ray representation used for tracing.
 */

#include "Vec3.h"

/**
 * Represents a ray of light traveling through 3D space.
 * 
 * A ray has:
 * - A starting point (where it begins)
 * - A direction (where it's going)
 * 
 * Mathematical formula: Point(t) = starting_point + t * direction
 * - When t = 0, you're at the starting point
 * - When t = 1, you've traveled one "direction length" forward
 * - When t = 2, you've traveled twice as far, etc.
 */
class Ray {
private:
    Point3 starting_point;    // Where the ray begins (e.g., camera position)
    Vec3 travel_direction;    // Which way the ray is going

public:
    // ========== Constructors ==========
    
    // Create an empty ray (not very useful, but sometimes needed)
    Ray() {}
    
    // Create a ray with a specific starting point and direction
    Ray(const Point3& origin, const Vec3& direction)
        : starting_point(origin), travel_direction(direction)
    {}

    // ========== Get Ray Properties ==========
    
    // Get where this ray starts
    Point3 origin() const { 
        return starting_point; 
    }
    
    // Get which direction this ray is traveling
    Vec3 direction() const { 
        return travel_direction; 
    }

    // ========== Calculate Points Along the Ray ==========
    
    /**
     * Find a point somewhere along this ray.
     * 
     * @param distance How far to travel along the ray
     *                 - distance = 0: returns the starting point
     *                 - distance > 0: point in front of the ray
     *                 - distance < 0: point behind the ray
     * 
     * Example: If ray starts at (0,0,0) going direction (1,0,0)
     *          at(2.5) returns point (2.5, 0, 0)
     */
    Point3 at(double distance) const {
        return starting_point + distance * travel_direction;
    }
};

#endif
