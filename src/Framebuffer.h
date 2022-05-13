#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

struct framebufferAttachment
{
    VkImage Image;
    VkDeviceMemory Memory;
    VkImageView ImageView;
    VkFormat Format;
    void Destroy(VkDevice Device)
    {
        vkDestroyImage(Device, Image, nullptr);
        vkDestroyImageView(Device, ImageView, nullptr);
        vkFreeMemory(Device, Memory, nullptr);
    }
};

struct framebuffer
{
    int32_t Width, Height;
    VkFramebuffer Framebuffer;
    framebufferAttachment Depth;
    VkRenderPass RenderPass;

    void SetSize(uint32_t NewWidth, uint32_t NewHeight)
    {
        this->Width = NewWidth;
        this->Height = NewHeight;
    }

    void Destroy(VkDevice Device)
    {
        vkDestroyFramebuffer(Device, Framebuffer, nullptr);
        vkDestroyRenderPass(Device, RenderPass, nullptr);
    }
};