#include "Buffer.h"

VkResult buffer::Map(VkDeviceSize _Size, VkDeviceSize Offset)
{
    return vkMapMemory(Device, Memory, Offset, _Size, 0, &Mapped);
}

void buffer::Unmap()
{
    if(Mapped)
    {
        vkUnmapMemory(Device, Memory);
        Mapped=nullptr;
    }
}

void buffer::SetupDescriptor(VkDeviceSize DeviceSize, VkDeviceSize Offset)
{
    Descriptor.offset = Offset;
    Descriptor.buffer = Buffer;
    Descriptor.range = DeviceSize;
}

VkResult buffer::Bind(VkDeviceSize Offset)
{
    return vkBindBufferMemory(Device, Buffer, Memory, Offset);
}

void buffer::CopyTo(void *Data, VkDeviceSize CopySize)
{
    assert(Mapped);
    memcpy(Mapped, Data, CopySize);
}

void buffer::Destroy()
{
    if (Buffer)
    {
        vkDestroyBuffer(Device, Buffer, nullptr);
    }
    if (Memory)
    {
        vkFreeMemory(Device, Memory, nullptr);
    }    
}

VkResult buffer::Flush(VkDeviceSize FlushSize, VkDeviceSize Offset)
{
    VkMappedMemoryRange mappedRange = {};
    mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedRange.memory = Memory;
    mappedRange.offset = Offset;
    mappedRange.size = FlushSize;
    return vkFlushMappedMemoryRanges(Device, 1, &mappedRange);
}