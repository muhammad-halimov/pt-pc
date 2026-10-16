# iOS (IOS_PORT.md): the game as an app bundle for iPhone and iPad. Vulkan is MoltenVK on Metal, embedded as the dynamic
# MoltenVK.framework (vk_context.cpp finds it in pt.app/Frameworks); the temporal upscaler is Apple MetalFX (metalfx_backend.mm).
# VULKAN_SDK_IOS (or PT_MOLTENVK_FRAMEWORK_DIR) names a folder with MoltenVK.xcframework/ios-arm64/MoltenVK.framework, such
# as the MoltenVK/MoltenVK/dynamic folder of the official MoltenVK-ios.tar.
find_path(PT_MOLTENVK_FRAMEWORK_DIR MoltenVK.framework
  PATHS
    "$ENV{VULKAN_SDK_IOS}/dynamic/MoltenVK.xcframework/ios-arm64"
    "$ENV{VULKAN_SDK_IOS}/Frameworks/MoltenVK.xcframework/ios-arm64"
    "$ENV{VULKAN_SDK_IOS}/lib/MoltenVK.xcframework/ios-arm64"
    "$ENV{VULKAN_SDK}/../iOS/Frameworks/MoltenVK.xcframework/ios-arm64"
  NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT PT_MOLTENVK_FRAMEWORK_DIR)
  message(FATAL_ERROR "iOS needs the dynamic MoltenVK.framework: set VULKAN_SDK_IOS (or PT_MOLTENVK_FRAMEWORK_DIR) to a folder with "
    "MoltenVK.xcframework/ios-arm64/MoltenVK.framework, for example the dynamic/ folder of MoltenVK-ios.tar")
endif()
set(PT_MOLTENVK_FRAMEWORK "${PT_MOLTENVK_FRAMEWORK_DIR}/MoltenVK.framework")

# the home screen icon: the game's own (assets/pt.icns) in an asset catalog (ios/Assets.xcassets)
target_sources(pt PRIVATE "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets")
set_source_files_properties("${CMAKE_SOURCE_DIR}/ios/Assets.xcassets" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(pt PROPERTIES RESOURCE "${CMAKE_SOURCE_DIR}/ios/Assets.xcassets"
  XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")

target_compile_definitions(pt_engine PUBLIC PT_IOS=1 PT_WITH_METALFX=1)
target_sources(pt_engine PRIVATE src/engine/platform/ios_display.mm src/engine/render/upscale/metalfx_backend.mm)
target_link_libraries(pt_engine PUBLIC whisper "-framework Foundation" "-framework UIKit" "-framework GameController" "-framework AVFoundation"
  "-framework Metal" "-framework MetalFX" "-framework QuartzCore" "-framework IOSurface")

# a signing team needs a bundle identifier of its own (-DPT_IOS_BUNDLE_ID=...)
set(PT_IOS_BUNDLE_ID "com.anch.pt-pc" CACHE STRING "iOS bundle identifier")
set_target_properties(pt PROPERTIES
  MACOSX_BUNDLE_GUI_IDENTIFIER "${PT_IOS_BUNDLE_ID}"
  MACOSX_BUNDLE_BUNDLE_NAME "P.T."
  MACOSX_BUNDLE_BUNDLE_VERSION "${PT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PT_VERSION}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist"
  XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PT_IOS_BUNDLE_ID}"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
  XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
  XCODE_ATTRIBUTE_SUPPORTED_PLATFORMS "iphoneos"
  XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
  XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@executable_path/Frameworks"
  XCODE_EMBED_FRAMEWORKS "${PT_MOLTENVK_FRAMEWORK}"
  XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON
  XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON)

# SDL_GetBasePath() is the top of pt.app on iOS: the shaders, the voice models and the fonts go there (the shaders after
# CMakeLists.txt compiles them; the fonts with every build, as on the desktop)
set(PT_IOS_RESOURCES ON)
