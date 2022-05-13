#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <assert.h>
#include <cstring>
class buffer
{
public:
    VkDevice Device;
    VkBuffer Buffer;
    
    VkDeviceSize Allignment=0;
    VkDeviceSize Size=0;
    VkBufferUsageFlags UsageFlags;
    VkMemoryPropertyFlags MemoryPropertyFlags;
    VkDescriptorBufferInfo Descriptor;

    VkDeviceMemory Memory;
    void *Mapped = nullptr;

    VkResult Map(VkDeviceSize _Size = VK_WHOLE_SIZE, VkDeviceSize Offset=0);

    void Unmap();

    void SetupDescriptor(VkDeviceSize = VK_WHOLE_SIZE, VkDeviceSize Offset=0);

    VkResult Bind(VkDeviceSize Offset=0);

    void CopyTo(void *Data, VkDeviceSize CopySize);
};