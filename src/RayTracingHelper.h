#pragma once
#include "Device.h"
#include <vulkan/vulkan.h>
#include "Buffer.h"
struct accelerationStructure
{
    VkAccelerationStructureKHR AccelerationStructure= VK_NULL_HANDLE;
    uint64_t DeviceAddress=0;
    VkDeviceMemory Memory;
    VkBuffer Buffer;
    vulkanDevice *VulkanDevice=nullptr;
    void Create(vulkanDevice *VulkanDevice, VkAccelerationStructureTypeKHR Type, VkAccelerationStructureBuildSizesInfoKHR BuildSizeInfo);
    void Destroy();
};

struct scratchBuffer
{
    uint64_t DeviceAddress=0;
    VkBuffer Buffer = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    vulkanDevice *VulkanDevice = nullptr;

    scratchBuffer(vulkanDevice *VulkanDevice, VkDeviceSize Size);
    void Destroy();
};


struct sceneModelInfo
{
    uint64_t Vertices;
    uint64_t Indices;
    uint64_t Materials;
};

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

class shaderBindingTable : public buffer
{
public:
    VkStridedDeviceAddressRegionKHR StrideDeviceAddressRegion{};
    void Create(vulkanDevice *Device, uint32_t HandleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties);
    void Destroy();
};