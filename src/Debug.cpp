#include "Debug.h"
#include <string>
#include <iostream>
#include <assert.h>


namespace vulkanDebug
{
    

VkBool32 MessageCallback(VkDebugReportFlagsEXT Flags,
                            VkDebugReportObjectTypeEXT ObjType,
                            uint64_t SourceObject,
                            size_t Location,
                            int32_t MessageCode,
                            const char *LayerPrefix,
                            const char *Message,
                            void *UserData)
{
    std::string Prefix("");

    if(Flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        Prefix += "ERROR : ";
    }
    if(Flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        Prefix += "WARNING : ";
    }
    if(Flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        Prefix += "PERFORMANCE : ";
    }
    if(Flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        Prefix += "INFORMATION : ";
    }
    if(Flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        Prefix += "DEBUG : ";
    }
    std::cout << Prefix << " [" << LayerPrefix << " ] Code : " << MessageCode << " : " << Message << std::endl;

    return VK_FALSE;
}

void SetupDebugReportCallback(VkInstance Instance, VkDebugReportFlagsEXT Flags)
{
    CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkCreateDebugReportCallbackEXT");
    DestroyDebugReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(Instance, "vkDestroyDebugReportCallbackEXT");
    DebugReportMessage = (PFN_vkDebugReportMessageEXT)vkGetInstanceProcAddr(Instance, "vkDebugReportMessageEXT");

    VkDebugReportCallbackCreateInfoEXT DebugCreateInfo = {};
    DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    DebugCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)MessageCallback;
    DebugCreateInfo.flags = Flags;

    VkResult Error = CreateDebugReportCallback(Instance, &DebugCreateInfo, nullptr, &DebugReportCallback);
    assert(!Error);
}

}