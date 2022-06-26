#pragma once

#include "Device.h"
#include <vulkan/vulkan.h>

struct storageImage
{
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    VkFormat Format;
    vulkanDevice *VulkanDevice=nullptr;
    void Create(vulkanDevice *VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkFormat Format, VkExtent3D Extent);
    void Destroy();
};