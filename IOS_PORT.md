# P.T. iOS Port

The native iOS (and iPadOS) build of the P.T. PC port by LoreanXavier, kept on top of
[LoreanXavier/pt-pc](https://github.com/LoreanXavier/pt-pc). It contains no P.T. game data: the extracted CUSA01127 folder
of your own copy goes into the app's Documents (the Files app, or `devicectl device copy to`).

## Status

- Builds and runs on an iPhone 15 (A16, iOS 16.4 or newer), Vulkan through MoltenVK on Metal.
- The game's BC textures are decoded on the CPU for GPUs without BC (A-series), levels over 512 px left out.
- The frame is drawn at a share of the screen (PC settings > Display > Render scale, 67% by default); Apple MetalFX is
  the upscaler. HDR (EDR) works.
- On-screen touch controls (a virtual gamepad); a real gamepad takes over when it is used, a touch hands back.
- The voice part: whisper.cpp linked into the app, the phone's microphone; a Jack button stands in for the word.
- Not done: the iOS Simulator (MoltenVK's iOS release has no simulator slice), signed release packaging.

## Build

On a Mac with Xcode, CMake, Ninja and glslc (`brew install cmake ninja shaderc vulkan-headers`), and the dynamic
MoltenVK.framework for iOS from the official `MoltenVK-ios.tar` (KhronosGroup/MoltenVK releases):

    tar -xf MoltenVK-ios.tar
    VULKAN_SDK_IOS=$PWD/MoltenVK/MoltenVK cmake -S . -B build/ios -G Xcode -DCMAKE_TOOLCHAIN_FILE=cmake/iOS.cmake \
        -DPT_IOS_BUNDLE_ID=<your.bundle.id> -DVulkan_INCLUDE_DIR=/opt/homebrew/include \
        -DVulkan_LIBRARY=$PWD/MoltenVK/MoltenVK/dynamic/MoltenVK.xcframework/ios-arm64/MoltenVK.framework/MoltenVK \
        -DVulkan_GLSLC_EXECUTABLE=/opt/homebrew/bin/glslc
    cmake --build build/ios --config Release --target pt -- -sdk iphoneos -allowProvisioningUpdates \
        CODE_SIGNING_ALLOWED=YES CODE_SIGN_STYLE=Automatic DEVELOPMENT_TEAM=<team>

Without signing (`CODE_SIGNING_ALLOWED=NO`) the CI workflow (.github/workflows/ios.yml) packages an unsigned IPA.

## Controls

In play: MOVE and LOOK sticks appear under the thumbs; Menu top left; Zoom, Hurry, and, where the game takes them, Use
(the picture loop) and Jack (the last loop) top right. In the menus: L1 / Resume / Zoom and Back / Settings / R1 along
the top, the directions (language and brightness on the original page) bottom left, the camera inversion bottom right.
On a gamepad the left stick click (L3) switches Hurry.

## Credits

- LoreanXavier: the P.T. PC port this builds on.
- anch-dev: the first iOS port (toolchain, app metadata, Documents game folder, IPA workflow).
- Felipe de Lima / ignusloki (ignusloki/pt-pc): Fast Walk (Hurry), and the Android launcher's touch layout the iOS
  controls follow.
- ginnfx and ahm3texe: Apple MetalFX and the MoltenVK lighting fix, by way of upstream.
- bcdec by Sergii Kudlai (MIT or Unlicense), MoltenVK (Apache-2.0); their notices are in licenses/.
