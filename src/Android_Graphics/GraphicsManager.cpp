//
// Created by ITEK on 2024/2/3.
//

#include "GraphicsManager.h"
#include "OpenGLGraphics.h"
#include "VulkanGraphics.h"

std::unique_ptr<AndroidImgui> GraphicsManager::getGraphicsInterface(GraphicsAPI api) {
    switch (api)
    {
    case OPENGL: {
        std::unique_ptr<AndroidImgui> at = std::make_unique<OpenGLGraphics>();
        return at;
    };
    case VULKAN: {
        std::unique_ptr<AndroidImgui> at = std::make_unique<VulkanGraphics>();
        return at;
    };
    }
    return nullptr;
}
