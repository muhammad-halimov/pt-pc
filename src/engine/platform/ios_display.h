#pragma once

#include <cstdint>

struct SDL_Window;

namespace pt::ios {

// Draws the window's Metal view (the one SDL made for the Vulkan surface) at `scale` of the screen's native pixels per axis;
// Core Animation stretches the smaller drawable over the screen. Called before the swapchain, which takes the drawable's size.
// Returns the drawable's size in pixels (0 x 0 when the window has no Metal view).
void ScaleMetalDrawable(SDL_Window* window, float scale, uint32_t* width = nullptr, uint32_t* height = nullptr);

}
