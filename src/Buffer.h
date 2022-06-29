#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

class buffer
{
public:

    struct
    {
        VkDevice Device;
        VkBuffer Buffer=VK_NULL_HANDLE;
        VkDeviceMemory Memory=VK_NULL_HANDLE;
        
        VkDeviceSize Allignment=0;
        VkDeviceSize Size=0;
        VkBufferUsageFlags UsageFlags;
        VkMemoryPropertyFlags MemoryPropertyFlags;
        VkDescriptorBufferInfo Descriptor;
        uint8_t *Mapped = nullptr;
    } VulkanObjects;


    VkResult Map(VkDeviceSize _Size = VK_WHOLE_SIZE, VkDeviceSize Offset=0);

    void Unmap();

    void SetupDescriptor(VkDeviceSize = VK_WHOLE_SIZE, VkDeviceSize Offset=0);

    VkResult Bind(VkDeviceSize Offset=0);

    void CopyTo(void *Data, VkDeviceSize CopySize, size_t Offset=0);
    void CopyFrom(void *Data, VkDeviceSize CopySize, size_t Offset=0);

    void Destroy();

	VkResult Flush(VkDeviceSize Size = VK_WHOLE_SIZE, VkDeviceSize Offset=0);
};