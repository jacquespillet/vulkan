#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Framebuffer.h"
#include "Scene.h"

class vulkanDevice;
class vulkanApp;

class objectPicker
{
public:
    void Initialize(vulkanDevice *Device, vulkanApp *App);
    void Render();
    void FillCommandBuffer();
    void Pick(int MouseX, int MouseY);
    void Destroy();
private:
    vulkanDevice *VulkanDevice;
    vulkanApp *App;
    VkCommandBuffer OffscreenCommandBuffer = VK_NULL_HANDLE;
    
    framebuffer Framebuffer;
    VkPipelineLayout PipelineLayout;
    std::vector<VkShaderModule> ShaderModules;
    VkPipeline Pipeline;

    sceneMesh Quad;

    VkSemaphore OffscreenSemaphore;
};