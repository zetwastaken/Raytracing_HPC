#include "Color.h"

unsigned char convert_to_byte(double color_value) {
    // Apply gamma correction (gamma = 2.0, i.e., square root)
    // This fixes the dark/grainy appearance by converting from linear to sRGB-like space
    const double gamma_corrected = std::sqrt(color_value);
    const double clamped_value = std::clamp(gamma_corrected, 0.0, 0.999);
    return static_cast<unsigned char>(clamped_value * 256.0);
}

void write_color(std::vector<unsigned char>& image_buffer, Color pixel_color) {
    image_buffer.push_back(convert_to_byte(pixel_color.x()));
    image_buffer.push_back(convert_to_byte(pixel_color.y()));
    image_buffer.push_back(convert_to_byte(pixel_color.z()));
}

