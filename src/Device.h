#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <vector>
#include <assert.h>

class vulkanDevice
{
public:
    VkPhysicalDevice PhysicalDevice;

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

    vulkanDevice(VkPhysicalDevice PhysicalDevice);
    
    uint32_t GetQueueFamilyIndex(VkQueueFlagBits QueueFlags);

    bool ExtensionSupported(std::string Extension);

    VkResult CreateDevice(VkPhysicalDeviceFeatures EnabledFeatures);

    uint32_t GetMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags RequestedProperties, VkBool32 *MemoryTypeFound=nullptr);
};