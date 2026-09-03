cmake_minimum_required(VERSION 3.18)

# 如果未设置 NDK 路径，尝试从环境变量获取
if(NOT DEFINED CMAKE_ANDROID_NDK)
    set(CMAKE_ANDROID_NDK $ENV{NDK_HOME})
endif()
if(NOT CMAKE_ANDROID_NDK)
    set(CMAKE_ANDROID_NDK "C:/Sdk/ndk/30.0.15729638")
endif()
if(CMAKE_ANDROID_NDK AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake)
endif()

# Android NDK 设置（必须在 project() 之前）
set(CMAKE_SYSTEM_NAME Android)
set(ANDROID_ABI arm64-v8a)
set(ANDROID_PLATFORM android-26)
set(ANDROID_STL c++_static)

project(dynamic_draw LANGUAGES C CXX)

# 产物输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# 只支持 ARM64
if(ANDROID AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This project only supports arm64-v8a")
endif()

if(NOT ANDROID)
    message(FATAL_ERROR
        "This project must be configured with the Android NDK toolchain. "
        "Check that CMAKE_ANDROID_NDK points to a valid NDK installation."
    )
endif()

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JNI_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

function(import_static_library target_name library_path)
    if(NOT EXISTS "${library_path}")
        message(FATAL_ERROR "Missing prebuilt static library: ${library_path}")
    endif()

    add_library(${target_name} STATIC IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${library_path}"
    )
endfunction()

import_static_library(
    lib_git_freetype
    "${JNI_DIR}/src/ImGui/misc/git_freetype/${ANDROID_ABI}/libfreetype.a"
)

# 壳层源文件：src 显式列出，UI 目录自动收集
set(SOURCES
    src/main.cpp
    src/Android_touch/TouchHelperA.cpp
    src/Android_Graphics/GraphicsManager.cpp
    src/Android_Graphics/OpenGLGraphics.cpp
    src/Android_Graphics/VulkanGraphics.cpp
    src/Android_Graphics/vulkan_wrapper.cpp
    src/Android_my_imgui/AndroidImgui.cpp
    src/Android_my_imgui/my_imgui.cpp
    src/Android_my_imgui/my_imgui_impl_android.cpp
    src/ImGui/imgui.cpp
    src/ImGui/imgui_demo.cpp
    src/ImGui/imgui_draw.cpp
    src/ImGui/imgui_tables.cpp
    src/ImGui/imgui_widgets.cpp
    src/ImGui/backends/imgui_impl_android.cpp
    src/ImGui/backends/imgui_impl_opengl3.cpp
    src/ImGui/backends/imgui_impl_vulkan.cpp
    src/ImGui/misc/freetype/imgui_freetype.cpp
    src/My_Utils/stb_image.cpp
)

file(GLOB UI_SOURCES CONFIGURE_DEPENDS
    "${JNI_DIR}/UI/*.c"
    "${JNI_DIR}/UI/*.cc"
    "${JNI_DIR}/UI/*.cpp"
    "${JNI_DIR}/UI/*.cxx"
)
list(APPEND SOURCES ${UI_SOURCES})

# core 框架层（模板以头文件为主，只有数学实现需要编译）
list(APPEND SOURCES "${JNI_DIR}/core/math/VecMath.cpp")

add_executable(dynamic_draw ${SOURCES})

target_compile_definitions(dynamic_draw PRIVATE
    VK_USE_PLATFORM_ANDROID_KHR
    IMGUI_IMPL_VULKAN_NO_PROTOTYPES
    IMGUI_DISABLE_DEBUG_TOOLS
    IMGUI_ENABLE_FREETYPE
)

target_compile_options(dynamic_draw PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-fvisibility=hidden;-O2>
    $<$<COMPILE_LANGUAGE:CXX>:-fvisibility=hidden;-O2;-fexceptions;-frtti>
)

target_include_directories(dynamic_draw PRIVATE
    "${JNI_DIR}"
    "${JNI_DIR}/core"
    "${JNI_DIR}/diRW"
    "${JNI_DIR}/games"
    "${JNI_DIR}/include"
    "${JNI_DIR}/include/Android_draw"
    "${JNI_DIR}/include/Android_Graphics"
    "${JNI_DIR}/include/Android_my_imgui"
    "${JNI_DIR}/include/Android_touch"
    "${JNI_DIR}/include/My_Utils"
    "${JNI_DIR}/include/ImGui"
    "${JNI_DIR}/include/ImGui/backends"
    "${JNI_DIR}/include/ImGui/misc/freetype"
    "${JNI_DIR}/include/ImGui/misc/git_freetype"
    "${JNI_DIR}/UI"
)

target_link_options(dynamic_draw PRIVATE
    -Wl,--allow-multiple-definition
    -Wl,--gc-sections
)

target_link_libraries(dynamic_draw PRIVATE
    lib_git_freetype
    log
    android
    EGL
    GLESv3
    dl
    m
    atomic
    c
    z
)
