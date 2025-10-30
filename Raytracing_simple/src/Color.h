#include "InlineControl.h"
#include "Vec3.h"

#include <algorithm>
#include <vector>

/**
 * Convert a floating-point color value to a byte value for image storage.
 * 
 * Colors in calculations use range [0.0, 1.0] for convenience:
 * - 0.0 = no color (black)
 * - 1.0 = full color (brightest)
 * 
 * But images store colors as bytes [0, 255]:
 * - 0 = no color (black)
 * - 255 = full color (brightest)
 * 
 * This function does the conversion and handles values outside the valid range.
 * 
 * @param color_value A color component (red, green, or blue) in range [0.0, 1.0]
 * @return Byte value in range [0, 255] suitable for image storage
 */
RAYTRACER_INLINE unsigned char convert_to_byte(double color_value) {
    // Make sure the value is between 0.0 and 0.999
    // (We use 0.999 instead of 1.0 to ensure we get 255, not 256)
    const double clamped_value = std::clamp(color_value, 0.0, 0.999);
    
    // Convert from [0.0, 0.999] to [0, 255]
    // Example: 0.5 * 256 = 128 (middle brightness)
    return static_cast<unsigned char>(clamped_value * 256.0);
}

/**
 * Add a pixel color to the image data buffer.
 * 
 * Takes a Color (which stores RGB as floating-point x, y, z components)
 * and converts it to three bytes (red, green, blue) that get added to
 * the image data buffer.
 * 
 * @param image_buffer The buffer where we're storing all pixel data
 * @param pixel_color The color to add (x=red, y=green, z=blue)
 */
RAYTRACER_INLINE void write_color(std::vector<unsigned char>& image_buffer, Color pixel_color) {
    // Add red component
    image_buffer.push_back(convert_to_byte(pixel_color.x()));
    
    // Add green component
    image_buffer.push_back(convert_to_byte(pixel_color.y()));
    
    // Add blue component
    image_buffer.push_back(convert_to_byte(pixel_color.z()));
}
