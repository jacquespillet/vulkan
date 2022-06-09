#include "RayTracingHelper.h"
#include "Tools.h"

void accelerationStructure::Create(vulkanDevice *_VulkanDevice, VkAccelerationStructureTypeKHR Type, VkAccelerationStructureBuildSizesInfoKHR BuildSizeInfo)
{
    if(AccelerationStructure != VK_NULL_HANDLE)
    {
        Destroy();
    }

    this->VulkanDevice = _VulkanDevice;

    VkBufferCreateInfo BufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferCreateInfo.size = BuildSizeInfo.accelerationStructureSize;
    BufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &Buffer));

    VkMemoryRequirements MemoryRequirements {};
    vkGetBufferMemoryRequirements(VulkanDevice->Device, Buffer, &MemoryRequirements);

    VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindBufferMemory(VulkanDevice->Device, Buffer, Memory, 0));

    VkAccelerationStructureCreateInfoKHR AccelerationStructureCreateInfo {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    AccelerationStructureCreateInfo.buffer = Buffer;
    AccelerationStructureCreateInfo.size = BuildSizeInfo.accelerationStructureSize;
    AccelerationStructureCreateInfo.type = Type;
    VulkanDevice->_vkCreateAccelerationStructureKHR(VulkanDevice->Device, &AccelerationStructureCreateInfo, nullptr, &AccelerationStructure);

    VkAccelerationStructureDeviceAddressInfoKHR AccelerationStructureDeviceAddressInfo {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    AccelerationStructureDeviceAddressInfo.accelerationStructure = AccelerationStructure;
    DeviceAddress = VulkanDevice->_vkGetAccelerationStructureDeviceAddressKHR(VulkanDevice->Device, &AccelerationStructureDeviceAddressInfo);
}

void accelerationStructure::Destroy()
{
    vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
    vkDestroyBuffer(VulkanDevice->Device, Buffer, nullptr);
    VulkanDevice->_vkDestroyAccelerationStructureKHR(VulkanDevice->Device, AccelerationStructure, nullptr);
}

scratchBuffer::scratchBuffer(vulkanDevice *VulkanDevice, VkDeviceSize Size)
{
    this->VulkanDevice = VulkanDevice;

    VkBufferCreateInfo BufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferCreateInfo.size = Size;
    BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &Buffer));

    VkMemoryRequirements MemoryRequirements {};
    vkGetBufferMemoryRequirements(VulkanDevice->Device, Buffer, &MemoryRequirements);

    VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindBufferMemory(VulkanDevice->Device, Buffer, Memory, 0));
    
    VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferDeviceAddressInfo.buffer = Buffer;
    DeviceAddress = VulkanDevice->_vkGetBufferDeviceAddressKHR(VulkanDevice->Device, &BufferDeviceAddressInfo);
}

void scratchBuffer::Destroy()
{
    if(Memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
    }
    if(Buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(VulkanDevice->Device, Buffer, nullptr);
    }
}

void storageImage::Create(vulkanDevice *_VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkFormat _Format, VkExtent3D Extent)
{
    this->VulkanDevice = _VulkanDevice;
    this->Format = _Format;

    if(Image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
        vkDestroyImage(VulkanDevice->Device, Image, nullptr);
        vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
        Image = VK_NULL_HANDLE;
    }

    VkImageCreateInfo ImageCreateInfo = vulkanTools::BuildImageCreateInfo();
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = _Format;
    ImageCreateInfo.extent = Extent;
    ImageCreateInfo.mipLevels=1;
    ImageCreateInfo.arrayLayers=1;
    ImageCreateInfo.samples=VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ImageCreateInfo.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CALL(vkCreateImage(VulkanDevice->Device, &ImageCreateInfo, nullptr, &Image));

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(VulkanDevice->Device, Image, &MemoryRequirements);
    VkMemoryAllocateInfo MemoryAllocateInfo = vulkanTools::BuildMemoryAllocateInfo();
    MemoryAllocateInfo.allocationSize=MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindImageMemory(VulkanDevice->Device, Image, Memory, 0));

    VkImageViewCreateInfo ColorImageView = vulkanTools::BuildImageViewCreateInfo();
    ColorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ColorImageView.format = Format;
    ColorImageView.subresourceRange = {};
    ColorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ColorImageView.subresourceRange.baseMipLevel=0;
    ColorImageView.subresourceRange.levelCount=1;
    ColorImageView.subresourceRange.baseArrayLayer=0;
    ColorImageView.subresourceRange.layerCount=1;
    ColorImageView.image = Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &ColorImageView, nullptr, &ImageView));

    VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vulkanTools::TransitionImageLayout(
        CommandBuffer,
        Image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    );
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, CommandBuffer, Queue, true);

}

void storageImage::Destroy()
{
    vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
    vkDestroyImage(VulkanDevice->Device, Image, nullptr);
    vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
}

void shaderBindingTable::Create(vulkanDevice *_VulkanDevice, uint32_t HandleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties)
{
    this->Device = _VulkanDevice->Device;

    VK_CALL(vulkanTools::CreateBuffer(
        _VulkanDevice,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        this,
        RayTracingPipelineProperties.shaderGroupHandleSize * HandleCount
    ));

    VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferDeviceAddressInfo.buffer = Buffer;
    const uint32_t HandleSizeAligned = vulkanTools::AlignedSize(RayTracingPipelineProperties.shaderGroupHandleSize, RayTracingPipelineProperties.shaderGroupHandleAlignment);
    StrideDeviceAddressRegion.deviceAddress=_VulkanDevice->_vkGetBufferDeviceAddressKHR(Device, &BufferDeviceAddressInfo);
    StrideDeviceAddressRegion.stride = HandleSizeAligned;
    StrideDeviceAddressRegion.size = HandleCount * HandleSizeAligned;

    Map();
}

void shaderBindingTable::Destroy()
{
    buffer::Destroy();
}
