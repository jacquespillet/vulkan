#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Resources.h"
#include "Buffer.h"
#include "Framebuffer.h"
#include "Camera.h"
class vulkanApp;
class vulkanDevice;

//This is just :
//Create resources necessary for rendering
//Build the draw command buffers
//Execute them
//Update
class renderer
{
public:
    vulkanApp *App;
    VkDevice Device;
    vulkanDevice *VulkanDevice;

    renderer(vulkanApp *App);

    virtual void Render()=0;
    virtual void Setup()=0;
    virtual void Destroy()=0;
    virtual void RenderGUI()=0;

    virtual void Resize(uint32_t Width, uint32_t Height)=0;

};
