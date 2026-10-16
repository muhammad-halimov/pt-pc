#include "engine/render/bc_decode.h"

#define BCDEC_IMPLEMENTATION
#define BCDEC_BC4BC5_PRECISE
#include <bcdec.h>

#include <algorithm>
#include <cstring>

namespace pt {

VkFormat BcDecodedFormat(VkFormat format) {
    switch (format) {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK: return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_BC4_UNORM_BLOCK: return VK_FORMAT_R8_UNORM;
    case VK_FORMAT_BC4_SNORM_BLOCK: return VK_FORMAT_R8_SNORM;
    case VK_FORMAT_BC5_UNORM_BLOCK: return VK_FORMAT_R8G8_UNORM;
    case VK_FORMAT_BC5_SNORM_BLOCK: return VK_FORMAT_R8G8_SNORM;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK: return VK_FORMAT_R16G16B16A16_SFLOAT;
    default: return VK_FORMAT_UNDEFINED;
    }
}

bool DecodeBc(VkFormat format, uint32_t width, uint32_t height, std::span<const uint8_t> data, std::vector<uint8_t>& out) {
    if (BcDecodedFormat(format) == VK_FORMAT_UNDEFINED || width == 0 || height == 0) {
        return false;
    }
    const bool bc6h = format == VK_FORMAT_BC6H_UFLOAT_BLOCK || format == VK_FORMAT_BC6H_SFLOAT_BLOCK;
    const bool bc4 = format == VK_FORMAT_BC4_UNORM_BLOCK || format == VK_FORMAT_BC4_SNORM_BLOCK;
    const bool bc5 = format == VK_FORMAT_BC5_UNORM_BLOCK || format == VK_FORMAT_BC5_SNORM_BLOCK;
    const bool bc1 = format >= VK_FORMAT_BC1_RGB_UNORM_BLOCK && format <= VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    const size_t block_bytes = bc1 || bc4 ? 8 : 16;
    // bytes of one decoded texel; BC6H decodes to RGB halves first and gets its alpha below
    const size_t texel = bc6h ? 8 : bc4 ? 1 : bc5 ? 2 : 4;
    const size_t decoded_texel = bc6h ? 6 : texel;
    const uint32_t blocks_x = (width + 3) / 4;
    const uint32_t blocks_y = (height + 3) / 4;
    if (data.size() < size_t(blocks_x) * blocks_y * block_bytes) {
        return false;
    }
    out.assign(size_t(width) * height * texel, 0);
    uint8_t block[4 * 4 * 8];
    const uint8_t* src = data.data();
    for (uint32_t by = 0; by < blocks_y; ++by) {
        for (uint32_t bx = 0; bx < blocks_x; ++bx, src += block_bytes) {
            const int pitch = static_cast<int>(4 * decoded_texel);
            switch (format) {
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: bcdec_bc1(src, block, pitch); break;
            case VK_FORMAT_BC2_UNORM_BLOCK:
            case VK_FORMAT_BC2_SRGB_BLOCK: bcdec_bc2(src, block, pitch); break;
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK: bcdec_bc3(src, block, pitch); break;
            case VK_FORMAT_BC4_UNORM_BLOCK:
            case VK_FORMAT_BC4_SNORM_BLOCK: bcdec_bc4(src, block, pitch, format == VK_FORMAT_BC4_SNORM_BLOCK); break;
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC5_SNORM_BLOCK: bcdec_bc5(src, block, pitch, format == VK_FORMAT_BC5_SNORM_BLOCK); break;
            // bcdec counts the BC6H pitch in halves
            case VK_FORMAT_BC6H_UFLOAT_BLOCK:
            case VK_FORMAT_BC6H_SFLOAT_BLOCK: bcdec_bc6h_half(src, block, 4 * 3, format == VK_FORMAT_BC6H_SFLOAT_BLOCK); break;
            default: bcdec_bc7(src, block, pitch); break;
            }
            const uint32_t x0 = bx * 4;
            const uint32_t y0 = by * 4;
            const uint32_t w = std::min(4u, width - x0);
            const uint32_t h = std::min(4u, height - y0);
            for (uint32_t y = 0; y < h; ++y) {
                uint8_t* dst = out.data() + ((size_t(y0) + y) * width + x0) * texel;
                const uint8_t* row = block + size_t(y) * 4 * decoded_texel;
                if (!bc6h) {
                    std::memcpy(dst, row, w * texel);
                    continue;
                }
                for (uint32_t x = 0; x < w; ++x) {
                    std::memcpy(dst + x * 8, row + x * 6, 6);
                    const uint16_t one = 0x3C00;
                    std::memcpy(dst + x * 8 + 6, &one, 2);
                }
            }
        }
    }
    if (format == VK_FORMAT_BC1_RGB_UNORM_BLOCK || format == VK_FORMAT_BC1_RGB_SRGB_BLOCK) {
        // the RGB variant has no alpha: its fourth colour is black, not transparent
        for (size_t i = 3; i < out.size(); i += 4) {
            out[i] = 255;
        }
    }
    return true;
}

}
