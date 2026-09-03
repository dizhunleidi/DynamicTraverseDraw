LOCAL_PATH := $(call my-dir)

# 引入freetype静态库 #
include $(CLEAR_VARS)
LOCAL_MODULE := lib_git_freetype
LOCAL_SRC_FILES := src/ImGui/misc/git_freetype/$(TARGET_ARCH_ABI)/libfreetype.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := dynamic_draw

LOCAL_CFLAGS   := -std=c17
LOCAL_CPPFLAGS := -std=c++17
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES
LOCAL_CPPFLAGS += -DIMGUI_DISABLE_DEBUG_TOOLS
LOCAL_CPPFLAGS += -DIMGUI_ENABLE_FREETYPE
LOCAL_CPPFLAGS += -fexceptions -frtti

LOCAL_CFLAGS += -fvisibility=hidden -O2
LOCAL_CPPFLAGS += -fvisibility=hidden -O2

#引入头文件到全局#
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core
LOCAL_C_INCLUDES += $(LOCAL_PATH)/diRW
LOCAL_C_INCLUDES += $(LOCAL_PATH)/games
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_draw
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_Graphics
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_my_imgui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/Android_touch
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/My_Utils
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/backends
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/misc/freetype
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include/ImGui/misc/git_freetype
LOCAL_C_INCLUDES += $(LOCAL_PATH)/UI

# UI 目录自动收集
FIND_SRC := $(wildcard $(LOCAL_PATH)/UI/*.c*)

LOCAL_SRC_FILES := src/main.cpp
LOCAL_SRC_FILES += core/math/VecMath.cpp
LOCAL_SRC_FILES += src/Android_touch/TouchHelperA.cpp
LOCAL_SRC_FILES += src/Android_Graphics/GraphicsManager.cpp
LOCAL_SRC_FILES += src/Android_Graphics/OpenGLGraphics.cpp
LOCAL_SRC_FILES += src/Android_Graphics/VulkanGraphics.cpp
LOCAL_SRC_FILES += src/Android_my_imgui/AndroidImgui.cpp
LOCAL_SRC_FILES += src/Android_my_imgui/my_imgui.cpp
LOCAL_SRC_FILES += src/Android_my_imgui/my_imgui_impl_android.cpp
LOCAL_SRC_FILES += src/ImGui/imgui.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_demo.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_draw.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_tables.cpp
LOCAL_SRC_FILES += src/ImGui/imgui_widgets.cpp
LOCAL_SRC_FILES += src/ImGui/backends/imgui_impl_android.cpp
LOCAL_SRC_FILES += src/ImGui/backends/imgui_impl_opengl3.cpp
LOCAL_SRC_FILES += src/ImGui/backends/imgui_impl_vulkan.cpp
LOCAL_SRC_FILES += src/ImGui/misc/freetype/imgui_freetype.cpp
LOCAL_SRC_FILES += src/My_Utils/stb_image.cpp
LOCAL_SRC_FILES += $(FIND_SRC:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -ldl -lm -latomic -lc
LOCAL_LDLIBS += -lz #freetype需要

LOCAL_EXPORT_LDFLAGS += -Wl,--allow-multiple-definition
LOCAL_EXPORT_LDFLAGS += -Wl,--gc-sections
LOCAL_STATIC_LIBRARIES := lib_git_freetype

include $(BUILD_EXECUTABLE) #可执行文件
