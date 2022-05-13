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