#include "Vec3.h"

/**
 * @file Color.h
 * @brief Helpers for tone mapping and writing RGB values.
 */

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
unsigned char convert_to_byte(double color_value);

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
void write_color(std::vector<unsigned char>& image_buffer, Color pixel_color);
