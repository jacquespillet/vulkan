#pragma once
#include <vulkan/vulkan.h>

namespace vulkanDebug
{
    static PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback;
    static PFN_vkDestroyDebugReportCallbackEXT DestroyDebugReportCallback;
    static PFN_vkDebugReportMessageEXT DebugReportMessage;

    static VkDebugReportCallbackEXT DebugReportCallback;
    
    VkBool32 MessageCallback(VkDebugReportFlagsEXT Flags,
                             VkDebugReportObjectTypeEXT ObjType,
                             uint64_t SourceObject,
                             size_t Location,
                             int32_t MessageCode,
                             const char *LayerPrefix,
                             const char *Message,
                             void *UserData);

    void SetupDebugReportCallback(VkInstance Instance, VkDebugReportFlagsEXT Flags);
};