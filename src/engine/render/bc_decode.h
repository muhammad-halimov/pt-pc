#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "engine/render/vk.h"

namespace pt {

// BC textures on a GPU that cannot sample them (Apple A-series before the M chips, through MoltenVK): the texture is decoded on
// the CPU into an uncompressed format with the same channels, R8 for BC4, R8G8 for BC5, RGBA16F for BC6H and RGBA8 otherwise

// the format a BC format decodes to, VK_FORMAT_UNDEFINED for a format that is not BC
VkFormat BcDecodedFormat(VkFormat format);
// one level of width x height texels (any size, the blocks cover it in 4x4 steps) into tightly packed BcDecodedFormat texels
bool DecodeBc(VkFormat format, uint32_t width, uint32_t height, std::span<const uint8_t> data, std::vector<uint8_t>& out);

}
