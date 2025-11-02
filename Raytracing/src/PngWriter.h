#ifndef PNG_WRITER_H
#define PNG_WRITER_H

/**
 * @file PngWriter.h
 * @brief Interface for writing RGB buffers to PNG files.
 */

#include <string>
#include <vector>

namespace png_writer {

bool write_rgb(const std::string& filename, int width, int height, const std::vector<unsigned char>& rgb);

} // namespace png_writer

#endif
