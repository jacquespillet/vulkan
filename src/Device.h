#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <assert.h>

class vulkanDevice
{
public:
    VkPhysicalDevice PhysicalDevice;
    VkInstance Instance;

    VkPhysicalDeviceProperties Properties;
    VkPhysicalDeviceFeatures Features;
    VkPhysicalDeviceMemoryProperties MemoryProperties;

    std::vector<VkQueueFamilyProperties> QueueFamilyProperties;
    std::vector<const char*> SupportedExtensions;

    VkDevice Device;

    struct {
        uint32_t Graphics;
        uint32_t Compute;
        uint32_t Transfer;
    } QueueFamilyIndices;

    bool EnableDebugMarkers=false;
    bool EnableNVDedicatedAllocation=false;

    vulkanDevice(VkPhysicalDevice PhysicalDevice, VkInstance Instance);
    
    uint32_t GetQueueFamilyIndex(VkQueueFlagBits QueueFlags);

    bool ExtensionSupported(std::string Extension);

    VkResult CreateDevice(VkPhysicalDeviceFeatures EnabledFeatures, bool RayTracing);

    uint32_t GetMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags RequestedProperties, VkBool32 *MemoryTypeFound=nullptr);

    
    PFN_vkGetBufferDeviceAddressKHR _vkGetBufferDeviceAddressKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR _vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateAccelerationStructureKHR _vkCreateAccelerationStructureKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR _vkCmdBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR _vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkBuildAccelerationStructuresKHR _vkBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR _vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCmdTraceRaysKHR _vkCmdTraceRaysKHR;
    PFN_vkCreateRayTracingPipelinesKHR _vkCreateRayTracingPipelinesKHR;
    PFN_vkDestroyAccelerationStructureKHR _vkDestroyAccelerationStructureKHR;
    // bool RayTracing=false;

    void *DevicePNextChain=nullptr;
    
	VkPhysicalDeviceDescriptorIndexingFeaturesEXT EnabledDescriptorIndexingFeatures{};
	VkPhysicalDeviceBufferDeviceAddressFeatures EnabledBufferDeviceAddresFeatures{};
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR EnabledRayTracingPipelineFeatures{};
	VkPhysicalDeviceAccelerationStructureFeaturesKHR EnabledAccelerationStructureFeatures{};
    VkPhysicalDeviceRayQueryFeaturesKHR EnabledRayQueryFeatures{};
    void LoadRayTracingFuncs();
};