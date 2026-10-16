#include "engine/platform/ios_display.h"

#import <QuartzCore/CAMetalLayer.h>
#import <UIKit/UIKit.h>

#include <SDL3/SDL.h>

#include <algorithm>

#include "engine/core/log.h"

namespace pt::ios {
namespace {

UIView* FindMetalView(UIView* view) {
    if ([view.layer isKindOfClass:[CAMetalLayer class]]) {
        return view;
    }
    for (UIView* child in view.subviews) {
        if (UIView* found = FindMetalView(child)) {
            return found;
        }
    }
    return nil;
}

}

void ScaleMetalDrawable(SDL_Window* window, float scale, uint32_t* width, uint32_t* height) {
    if (width) *width = 0;
    if (height) *height = 0;
    @autoreleasepool {
        UIWindow* uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
        UIView* view = uiwindow ? FindMetalView(uiwindow) : nil;
        if (!view) {
            LogWarn("display: no Metal view to scale");
            return;
        }
        const CGFloat native = uiwindow.screen.nativeScale;
        // SDL's Metal view sizes its drawable from the layer's contentsScale whenever it lays out
        view.layer.contentsScale = native * std::clamp(scale, 0.33f, 1.0f);
        [view setNeedsLayout];
        [view layoutIfNeeded];
        const CGSize size = ((CAMetalLayer*)view.layer).drawableSize;
        if (width) *width = static_cast<uint32_t>(size.width);
        if (height) *height = static_cast<uint32_t>(size.height);
        static CGSize logged{};
        if (CGSizeEqualToSize(size, logged)) return;
        logged = size;
        const NSProcessInfoThermalState thermal = [NSProcessInfo processInfo].thermalState;
        LogInfo("display: device thermal state {}", thermal == NSProcessInfoThermalStateNominal ? "nominal" : thermal == NSProcessInfoThermalStateFair ? "fair"
                                                     : thermal == NSProcessInfoThermalStateSerious ? "serious (the GPU is throttled)" : "critical (the GPU is throttled)");
        LogInfo("display: drawing at {}x{} ({:.0f}% of the screen's {}x{})", static_cast<int>(size.width), static_cast<int>(size.height),
                100.0 * view.layer.contentsScale / native, static_cast<int>(view.bounds.size.width * native),
                static_cast<int>(view.bounds.size.height * native));
    }
}

}
