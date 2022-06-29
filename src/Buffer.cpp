#include "Buffer.h"
#include <assert.h>

VkResult buffer::Map(VkDeviceSize _Size, VkDeviceSize Offset)
{
    return vkMapMemory(VulkanObjects.Device, VulkanObjects.Memory, Offset, _Size, 0, (void**)&VulkanObjects.Mapped);
}

void buffer::Unmap()
{
    if(VulkanObjects.Mapped)
    {
        vkUnmapMemory(VulkanObjects.Device, VulkanObjects.Memory);
        VulkanObjects.Mapped=nullptr;
    }
}

void buffer::SetupDescriptor(VkDeviceSize DeviceSize, VkDeviceSize Offset)
{
    VulkanObjects.Descriptor.offset = Offset;
    VulkanObjects.Descriptor.buffer = VulkanObjects.Buffer;
    VulkanObjects.Descriptor.range = DeviceSize;
}

VkResult buffer::Bind(VkDeviceSize Offset)
{
    return vkBindBufferMemory(VulkanObjects.Device, VulkanObjects.Buffer, VulkanObjects.Memory, Offset);
}

void buffer::CopyTo(void *Data, VkDeviceSize CopySize, size_t Offset)
{
    assert(VulkanObjects.Mapped);
    memcpy(VulkanObjects.Mapped + Offset, Data, CopySize);
}

void buffer::CopyFrom(void *Data, VkDeviceSize CopySize, size_t Offset)
{
    assert(VulkanObjects.Mapped);
    memcpy(Data, VulkanObjects.Mapped + Offset, CopySize);
}


void buffer::Destroy()
{
    if (VulkanObjects.Buffer)
    {
        vkDestroyBuffer(VulkanObjects.Device, VulkanObjects.Buffer, nullptr);
    }
    if (VulkanObjects.Memory)
    {
        vkFreeMemory(VulkanObjects.Device, VulkanObjects.Memory, nullptr);
    }    
}

VkResult buffer::Flush(VkDeviceSize FlushSize, VkDeviceSize Offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = VulkanObjects.Memory;
    mappedRange.offset = Offset;
    mappedRange.size = FlushSize;
    return vkFlushMappedMemoryRanges(VulkanObjects.Device, 1, &mappedRange);
}