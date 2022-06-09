#include "Device.h"
#include "Tools.h"

vulkanDevice::vulkanDevice(VkPhysicalDevice PhysicalDevice, VkInstance Instance) : 
            PhysicalDevice(PhysicalDevice), Instance(Instance)
{
    assert(PhysicalDevice);

    vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
    vkGetPhysicalDeviceFeatures(PhysicalDevice, &Features);
    vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

    uint32_t QueueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);
    assert(QueueFamilyCount>0);
    QueueFamilyProperties.resize(QueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilyProperties.data());

    uint32_t ExtensionsCount;
    vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionsCount, nullptr);
    std::vector<VkExtensionProperties> Extensions(ExtensionsCount);
    if(ExtensionsCount>0)
    {
        if(vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionsCount, &Extensions.front()) == VK_SUCCESS)
        {
            for(auto Extension : Extensions)
            {
                SupportedExtensions.push_back(Extension.extensionName);
            }
        }
    }
}

VkResult vulkanDevice::CreateDevice(VkPhysicalDeviceFeatures EnabledFeatures, bool RayTracing)
{
    bool UseSwapchain=true;
    VkQueueFlags RequestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    std::vector<VkDeviceQueueCreateInfo> QueueCreateInfos{};
    const float DefaultQueuePriority=0;

    if(RequestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
    {
        QueueFamilyIndices.Graphics = GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        VkDeviceQueueCreateInfo QueueCreateInfo = {};
        QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QueueCreateInfo.queueFamilyIndex = QueueFamilyIndices.Graphics;
        QueueCreateInfo.queueCount=1;
        QueueCreateInfo.pQueuePriorities = &DefaultQueuePriority;
        QueueCreateInfos.push_back(QueueCreateInfo);
    }
    else
    {
        QueueFamilyIndices.Graphics = 0;
    }

    if(RequestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
    {
        QueueFamilyIndices.Compute = GetQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
        if(QueueFamilyIndices.Compute != QueueFamilyIndices.Graphics)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo = {};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = QueueFamilyIndices.Compute;
            QueueCreateInfo.queueCount=1;
            QueueCreateInfo.pQueuePriorities = &DefaultQueuePriority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }
    }
    else
    {
        QueueFamilyIndices.Compute = 0;
    }

    if(RequestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
    {
        QueueFamilyIndices.Transfer = GetQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
        if(QueueFamilyIndices.Transfer != QueueFamilyIndices.Graphics)
        {
            VkDeviceQueueCreateInfo QueueCreateInfo = {};
            QueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfo.queueFamilyIndex = QueueFamilyIndices.Transfer;
            QueueCreateInfo.queueCount=1;
            QueueCreateInfo.pQueuePriorities = &DefaultQueuePriority;
            QueueCreateInfos.push_back(QueueCreateInfo);
        }
    }
    else
    {
        QueueFamilyIndices.Transfer = 0;
    }


    std::vector<const char*> DeviceExtensions;
    if(UseSwapchain)
    {
        DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }


    VkDeviceCreateInfo DeviceCreateInfo = {};
    DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    DeviceCreateInfo.queueCreateInfoCount = (uint32_t)QueueCreateInfos.size();
    DeviceCreateInfo.pQueueCreateInfos = QueueCreateInfos.data();
    DeviceCreateInfo.pEnabledFeatures = &EnabledFeatures;


    if(vulkanTools::CheckDeviceExtensionPresent(PhysicalDevice, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        DeviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        EnableDebugMarkers=true;
    }

    if(ExtensionSupported(VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME))
    {
        DeviceExtensions.push_back(VK_NV_DEDICATED_ALLOCATION_EXTENSION_NAME);
        EnableNVDedicatedAllocation=true;
    }

    if(ExtensionSupported(VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME))
    {
        DeviceExtensions.push_back(VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME);
    }


    if(RayTracing)
    {
        DeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        DeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

        EnabledFeatures.samplerAnisotropy=VK_TRUE;
        EnabledFeatures.shaderInt64 = VK_TRUE;   

        EnabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        EnabledBufferDeviceAddresFeatures.bufferDeviceAddress=VK_TRUE;

        EnabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        EnabledRayTracingPipelineFeatures.rayTracingPipeline=VK_TRUE;
        EnabledRayTracingPipelineFeatures.pNext = &EnabledBufferDeviceAddresFeatures;

        EnabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        EnabledAccelerationStructureFeatures.accelerationStructure=  VK_TRUE;
        EnabledAccelerationStructureFeatures.pNext = &EnabledRayTracingPipelineFeatures;

        EnabledDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
        EnabledDescriptorIndexingFeatures.runtimeDescriptorArray=VK_TRUE;
        EnabledDescriptorIndexingFeatures.pNext = &EnabledAccelerationStructureFeatures;
        DevicePNextChain = &EnabledDescriptorIndexingFeatures;
        // DeviceCreateInfo.pNext = &EnabledDescriptorIndexingFeatures;
    }

    VkPhysicalDeviceFeatures2 PhysicalDeviceFeatures2{};
    if(DevicePNextChain)
    {
        PhysicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        PhysicalDeviceFeatures2.features = EnabledFeatures;
        PhysicalDeviceFeatures2.pNext = DevicePNextChain;
        DeviceCreateInfo.pEnabledFeatures=nullptr;
        DeviceCreateInfo.pNext = &PhysicalDeviceFeatures2;
    }

    if(DeviceExtensions.size() > 0)
    {
        DeviceCreateInfo.enabledExtensionCount = (uint32_t)DeviceExtensions.size();
        DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();
    }
    

    VkResult Result = vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, nullptr, &Device);

    return Result;
}

  
uint32_t vulkanDevice::GetQueueFamilyIndex(VkQueueFlagBits QueueFlags)
{
    if(QueueFlags & VK_QUEUE_COMPUTE_BIT)
    {
        for(size_t i=0; i<QueueFamilyProperties.size(); i++)
        {
            if( (QueueFamilyProperties[i].queueFlags & QueueFlags) &&  //If the queue has compute
                ((QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)==0) //And is not graphics
                )
            {
                return (uint32_t)i;
                break;
            }
        }
    }

    if(QueueFlags & VK_QUEUE_TRANSFER_BIT)
    {
        for(size_t i=0; i<QueueFamilyProperties.size(); i++)
        {
            if( (QueueFamilyProperties[i].queueFlags & QueueFlags) && //If the queue has transfer
                ((QueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)==0) &&  //And is not graphics
                ((QueueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)==0)  //And is not transfer
                )
            {
                return (uint32_t)i;
                break;
            }
        }
    }

    for(size_t i=0; i<QueueFamilyProperties.size(); i++)
    {
        if(QueueFamilyProperties[i].queueFlags & QueueFlags)
        {
            return (uint32_t)i;
            break;
        }
    }
    assert(0);
    return 0;
}

bool vulkanDevice::ExtensionSupported(std::string Extension)
{
    return (std::find(SupportedExtensions.begin(), SupportedExtensions.end(), Extension) != SupportedExtensions.end());
}


uint32_t vulkanDevice::GetMemoryType(uint32_t TypeBits, VkMemoryPropertyFlags RequestedProperties, VkBool32 *MemoryTypeFound)
{
    for(uint32_t i=0; i<MemoryProperties.memoryTypeCount; i++)
    {
        if((TypeBits & 1) == 1)
        {
            if((MemoryProperties.memoryTypes[i].propertyFlags & RequestedProperties) == RequestedProperties)
            {
                if(MemoryTypeFound) 
                {
                    *MemoryTypeFound = true;
                }
                return i;
            }
        }
        TypeBits >>= 1;
    }

    if(MemoryTypeFound)
    {
        *MemoryTypeFound=false;
        return 0;
    }
    else
    {
        assert(0);
    }

    return 0;
}

void vulkanDevice::LoadRayTracingFuncs()
{
    _vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetInstanceProcAddr(Instance, "vkGetBufferDeviceAddressKHR") ;
    _vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(Instance, "vkGetRayTracingShaderGroupHandlesKHR") ;
    _vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(Instance, "vkCreateAccelerationStructureKHR") ;
    _vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(Instance, "vkCmdBuildAccelerationStructuresKHR") ;
    _vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(Instance, "vkGetAccelerationStructureDeviceAddressKHR") ;       
    _vkBuildAccelerationStructuresKHR = (PFN_vkBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(Instance, "vkBuildAccelerationStructuresKHR") ;       
    _vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(Instance, "vkGetAccelerationStructureBuildSizesKHR") ;       
    _vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(Instance, "vkCmdTraceRaysKHR") ;       
    _vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(Instance, "vkCreateRayTracingPipelinesKHR") ;       
    _vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(Instance, "vkDestroyAccelerationStructureKHR") ;       

}