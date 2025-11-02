#ifndef CAMERA_H
#define CAMERA_H

/**
 * @file Camera.h
 * @brief Pinhole camera implementation for generating primary rays.
 */

#include "Vec3.h"

/**
 * Camera configuration and viewport parameters.
 * Defines the view frustum for rendering.
 */
class Camera {
public:
    Point3 origin;
    Vec3 horizontal;
    Vec3 vertical;
    Vec3 lower_left_corner;
    
    /**
     * Create a camera with the given aspect ratio.
     * 
     * @param aspect_ratio Width/height ratio (e.g., 16/9)
     * @param viewport_height Height of the virtual viewport
     * @param focal_length Distance from camera to viewport plane
     */
    Camera(double aspect_ratio, double viewport_height = 2.0, double focal_length = 1.0) {
        origin = Point3(0, 0, 0);
        
        double viewport_width = aspect_ratio * viewport_height;
        horizontal = Vec3(viewport_width, 0, 0);
        vertical = Vec3(0, viewport_height, 0);
        
        lower_left_corner = origin - horizontal / 2 - vertical / 2 - Vec3(0, 0, focal_length);
    }
};

#endif
