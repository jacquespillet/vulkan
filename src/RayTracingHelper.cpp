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


void shaderBindingTable::Create(vulkanDevice *_VulkanDevice, uint32_t HandleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties)
{
    this->VulkanObjects.Device = _VulkanDevice->Device;

    VK_CALL(vulkanTools::CreateBuffer(
        _VulkanDevice,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        this,
        RayTracingPipelineProperties.shaderGroupHandleSize * HandleCount
    ));

    VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferDeviceAddressInfo.buffer = VulkanObjects.Buffer;
    const uint32_t HandleSizeAligned = vulkanTools::AlignedSize(RayTracingPipelineProperties.shaderGroupHandleSize, RayTracingPipelineProperties.shaderGroupHandleAlignment);
    StrideDeviceAddressRegion.deviceAddress=_VulkanDevice->_vkGetBufferDeviceAddressKHR(VulkanObjects.Device, &BufferDeviceAddressInfo);
    StrideDeviceAddressRegion.stride = HandleSizeAligned;
    StrideDeviceAddressRegion.size = HandleCount * HandleSizeAligned;

    Map();
}

void shaderBindingTable::Destroy()
{
    buffer::Destroy();
}
