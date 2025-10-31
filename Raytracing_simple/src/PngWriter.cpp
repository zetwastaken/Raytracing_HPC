#include "PngWriter.h"

#include <array>
#include <cstdint>
#include <fstream>

namespace png_writer {
namespace detail {

void write_uint32(std::ofstream& out, std::uint32_t value) {
    const unsigned char buffer[4] = {
        static_cast<unsigned char>((value >> 24) & 0xFF),
        static_cast<unsigned char>((value >> 16) & 0xFF),
        static_cast<unsigned char>((value >> 8) & 0xFF),
        static_cast<unsigned char>(value & 0xFF)
    };
    out.write(reinterpret_cast<const char*>(buffer), sizeof(buffer));
}

std::uint32_t crc32(const unsigned char* data, std::size_t length) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            const std::uint32_t mask = static_cast<std::uint32_t>(-(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

std::uint32_t adler32(const unsigned char* data, std::size_t len) {
    constexpr std::uint32_t MOD_ADLER = 65521u;
    std::uint32_t a = 1;
    std::uint32_t b = 0;

    while (len > 0) {
        const std::size_t tlen = len > 5552 ? 5552 : len;
        len -= tlen;
        for (std::size_t i = 0; i < tlen; ++i) {
            a = (a + data[i]) % MOD_ADLER;
            b = (b + a) % MOD_ADLER;
        }
        data += tlen;
    }

    return (b << 16) | a;
}

void write_chunk(std::ofstream& out, const char type[4], const std::vector<unsigned char>& data) {
    const std::array<unsigned char, 4> chunk_type = {
        static_cast<unsigned char>(type[0]),
        static_cast<unsigned char>(type[1]),
        static_cast<unsigned char>(type[2]),
        static_cast<unsigned char>(type[3])
    };

    write_uint32(out, static_cast<std::uint32_t>(data.size()));
    out.write(reinterpret_cast<const char*>(chunk_type.data()), 4);
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }

    std::vector<unsigned char> crc_input;
    crc_input.reserve(chunk_type.size() + data.size());
    crc_input.insert(crc_input.end(), chunk_type.begin(), chunk_type.end());
    crc_input.insert(crc_input.end(), data.begin(), data.end());

    const std::uint32_t crc = crc32(crc_input.data(), crc_input.size());
    write_uint32(out, crc);
}

void append_uncompressed_block(std::vector<unsigned char>& zlib_data,
                               const unsigned char* block_data,
                               std::size_t block_size,
                               bool is_final_block) {
    const unsigned char bfinal = is_final_block ? 1u : 0u;
    zlib_data.push_back(bfinal);

    const std::uint16_t len = static_cast<std::uint16_t>(block_size);
    const std::uint16_t nlen = static_cast<std::uint16_t>(~len);

    zlib_data.push_back(static_cast<unsigned char>(len & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>(nlen & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>((nlen >> 8) & 0xFF));

    zlib_data.insert(zlib_data.end(), block_data, block_data + block_size);
}

std::vector<unsigned char> make_zlib_stream(const std::vector<unsigned char>& raw) {
    std::vector<unsigned char> zlib_data;
    zlib_data.reserve(raw.size() + 6 + (raw.size() / 65535 + 1) * 5);

    zlib_data.push_back(0x78);
    zlib_data.push_back(0x01);

    const std::size_t max_block_size = 65535;
    std::size_t offset = 0;
    while (offset < raw.size()) {
        const std::size_t remaining = raw.size() - offset;
        const std::size_t chunk_size = remaining > max_block_size ? max_block_size : remaining;
        const bool is_final_block = (offset + chunk_size) >= raw.size();
        append_uncompressed_block(zlib_data, raw.data() + offset, chunk_size, is_final_block);
        offset += chunk_size;
    }

    const std::uint32_t adler = adler32(raw.data(), raw.size());
    zlib_data.push_back(static_cast<unsigned char>((adler >> 24) & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>((adler >> 16) & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>((adler >> 8) & 0xFF));
    zlib_data.push_back(static_cast<unsigned char>(adler & 0xFF));

    return zlib_data;
}

} // namespace detail

bool write_rgb(const std::string& filename, int width, int height, const std::vector<unsigned char>& rgb) {
    if (width <= 0 || height <= 0 || static_cast<std::size_t>(width * height * 3) != rgb.size()) {
        return false;
    }

    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        return false;
    }

    const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.write(reinterpret_cast<const char*>(signature), sizeof(signature));

    std::vector<unsigned char> ihdr(13);
    ihdr[0] = static_cast<unsigned char>((width >> 24) & 0xFF);
    ihdr[1] = static_cast<unsigned char>((width >> 16) & 0xFF);
    ihdr[2] = static_cast<unsigned char>((width >> 8) & 0xFF);
    ihdr[3] = static_cast<unsigned char>(width & 0xFF);
    ihdr[4] = static_cast<unsigned char>((height >> 24) & 0xFF);
    ihdr[5] = static_cast<unsigned char>((height >> 16) & 0xFF);
    ihdr[6] = static_cast<unsigned char>((height >> 8) & 0xFF);
    ihdr[7] = static_cast<unsigned char>(height & 0xFF);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    detail::write_chunk(out, "IHDR", ihdr);

    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * 3 + 1));
    const std::size_t row_stride = static_cast<std::size_t>(width) * 3;
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const unsigned char* row_ptr = rgb.data() + static_cast<std::size_t>(y) * row_stride;
        raw.insert(raw.end(), row_ptr, row_ptr + row_stride);
    }

    const std::vector<unsigned char> zlib_data = detail::make_zlib_stream(raw);
    detail::write_chunk(out, "IDAT", zlib_data);
    detail::write_chunk(out, "IEND", {});

    return true;
}

} // namespace png_writer

