#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>


struct swapchain
{
    struct swapchainBuffer
    {
        VkImage Image;
        VkImageView View;
    };

    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;

    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR; 

    VkSurfaceKHR Surface;

    uint32_t QueueNodeIndex;

    VkFormat ColorFormat;
    VkColorSpaceKHR ColorSpace;

    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;

    uint32_t ImageCount;
    std::vector<swapchainBuffer> Buffers;
    std::vector<VkImage> Images;

    void Connect(VkInstance _Instance, VkPhysicalDevice _PhysicalDevice, VkDevice _Device)
    {
        this->Instance = _Instance;
        this->PhysicalDevice = _PhysicalDevice;
        this->Device = _Device;

        
        GetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)vkGetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
        GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)vkGetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)vkGetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        GetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)vkGetInstanceProcAddr(Instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    
        CreateSwapchainKHR= (PFN_vkCreateSwapchainKHR)vkGetDeviceProcAddr(Device, "vkCreateSwapchainKHR") ;
        DestroySwapchainKHR= (PFN_vkDestroySwapchainKHR)vkGetDeviceProcAddr(Device, "vkDestroySwapchainKHR") ;
        GetSwapchainImagesKHR= (PFN_vkGetSwapchainImagesKHR)vkGetDeviceProcAddr(Device, "vkGetSwapchainImagesKHR") ;
        AcquireNextImageKHR= (PFN_vkAcquireNextImageKHR)vkGetDeviceProcAddr(Device, "vkAcquireNextImageKHR") ;
        QueuePresentKHR= (PFN_vkQueuePresentKHR)vkGetDeviceProcAddr(Device, "vkQueuePresentKHR") ; 
    }

    void InitSurface(HWND Window)
    {
        VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
        SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        SurfaceCreateInfo.hinstance = GetModuleHandle(nullptr);
        SurfaceCreateInfo.hwnd = Window;
        VkResult Error = vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, nullptr, &Surface);

        uint32_t QueueCount;
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueCount, nullptr);
        assert(QueueCount>=1);
        std::vector<VkQueueFamilyProperties> QueueProperties(QueueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueCount, QueueProperties.data());

        std::vector<VkBool32> SupportsPresent(QueueCount);
        for (uint32_t i = 0; i < QueueCount; i++)
        {   
            GetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, Surface, &SupportsPresent[i]);
        }
        
        uint32_t GraphicsQueueNodeIndex = UINT32_MAX;
        uint32_t PresentQueueNodeIndex = UINT32_MAX;
        for(uint32_t i=0; i<QueueCount; i++)
        {
            if((QueueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                if(GraphicsQueueNodeIndex == UINT32_MAX)
                {
                    GraphicsQueueNodeIndex=i;
                }

                if(SupportsPresent[i] == VK_TRUE)
                {
                    GraphicsQueueNodeIndex=i;
                    PresentQueueNodeIndex=i;
                    break;
                }
            }
        }

        if(PresentQueueNodeIndex == UINT32_MAX)
        {
            for(uint32_t i=0; i<QueueCount; i++)
            {
                if(SupportsPresent[i] == VK_TRUE)
                {
                    PresentQueueNodeIndex = i;
                    break;
                }
            }
        }

        if(GraphicsQueueNodeIndex == UINT32_MAX || PresentQueueNodeIndex == UINT32_MAX)
        {
            assert(0);
        }

        if(GraphicsQueueNodeIndex != PresentQueueNodeIndex)
        {
            assert(0);
        }

        QueueNodeIndex = GraphicsQueueNodeIndex;

        //
        uint32_t FormatCount;
        Error = GetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);
        assert(!Error);
        assert(FormatCount>0);
        std::vector<VkSurfaceFormatKHR> SurfaceFormats(FormatCount);
        Error = GetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, SurfaceFormats.data());
        assert(!Error);

        if((FormatCount==1) && (SurfaceFormats[0].format == VK_FORMAT_UNDEFINED))
        {
            ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        }
        else
        {
            ColorFormat = SurfaceFormats[0].format;
        }

        ColorSpace = SurfaceFormats[0].colorSpace;
    }

    void Create(uint32_t *Width, uint32_t *Height, bool EnableVSync)
    {
        VkResult Error;
        VkSwapchainKHR OldSwapchain = Swapchain;

        VkSurfaceCapabilitiesKHR SurfaceCapabilities;
        Error = GetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCapabilities);
        assert(!Error);

        uint32_t PresentModeCount;
        Error = GetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr);
        assert(!Error);
        assert(PresentModeCount >0);
        std::vector<VkPresentModeKHR> PresentModes(PresentModeCount);
        Error = GetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, PresentModes.data());
        assert(!Error);

        VkExtent2D SwapchainExtent = {};
        if(SurfaceCapabilities.currentExtent.width == (uint32_t)-1)
        {
            SwapchainExtent.width = *Width;
            SwapchainExtent.height = *Height;
        }
        else
        {
            SwapchainExtent = SurfaceCapabilities.currentExtent;
            *Width = SurfaceCapabilities.currentExtent.width;
            *Height = SurfaceCapabilities.currentExtent.height;
        }

        VkPresentModeKHR SwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        //If no vsync, try mailbox, otherwise immediate.
        if(!EnableVSync)
        {
            for(size_t i=0; i<PresentModes.size(); i++)
            {
                if(PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    SwapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if((SwapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (PresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
                {
                    SwapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }

        uint32_t DesiredNumberOfSwapchainImages = SurfaceCapabilities.minImageCount + 1;
        if((SurfaceCapabilities.maxImageCount>0) && (DesiredNumberOfSwapchainImages > SurfaceCapabilities.maxImageCount))
        {
            DesiredNumberOfSwapchainImages = SurfaceCapabilities.maxImageCount;
        }

        VkSurfaceTransformFlagsKHR PreTransform;
        if(SurfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        {
            PreTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        }
        else
        {
            PreTransform = SurfaceCapabilities.currentTransform;
        }

        VkSwapchainCreateInfoKHR SwapchainCreateInfo {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        SwapchainCreateInfo.pNext = nullptr;
        SwapchainCreateInfo.surface = Surface;
        SwapchainCreateInfo.minImageCount = DesiredNumberOfSwapchainImages;
        SwapchainCreateInfo.imageFormat = ColorFormat;
        SwapchainCreateInfo.imageColorSpace = ColorSpace;
        SwapchainCreateInfo.imageExtent = {SwapchainExtent.width, SwapchainExtent.height};
        SwapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        SwapchainCreateInfo.preTransform = (VkSurfaceTransformFlagBitsKHR)PreTransform;
        SwapchainCreateInfo.imageArrayLayers = 1;
        SwapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        SwapchainCreateInfo.queueFamilyIndexCount = 0;
        SwapchainCreateInfo.pQueueFamilyIndices=nullptr;
        SwapchainCreateInfo.presentMode = SwapchainPresentMode;
        SwapchainCreateInfo.oldSwapchain = OldSwapchain;
        SwapchainCreateInfo.clipped = VK_TRUE;
        SwapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        Error = CreateSwapchainKHR(Device, &SwapchainCreateInfo, nullptr, &Swapchain);
        assert(!Error);


        if(OldSwapchain != VK_NULL_HANDLE)
        {
            for(uint32_t i=0; i<ImageCount; i++)
            {
                vkDestroyImageView(Device, Buffers[i].View, nullptr);
            }
            DestroySwapchainKHR(Device, OldSwapchain, nullptr);
        }

        Error = GetSwapchainImagesKHR(Device, Swapchain, &ImageCount, nullptr);
        assert(!Error);

        Images.resize(ImageCount);
        Error = GetSwapchainImagesKHR(Device, Swapchain, &ImageCount, Images.data());


        Buffers.resize(ImageCount);
        for(uint32_t i=0; i<ImageCount; i++)
        {
            VkImageViewCreateInfo ColorAttachmentView {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ColorAttachmentView.pNext = nullptr;
            ColorAttachmentView.format = ColorFormat;
            ColorAttachmentView.components = {
                VK_COMPONENT_SWIZZLE_R,
                VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B,
                VK_COMPONENT_SWIZZLE_A
            };
            ColorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ColorAttachmentView.subresourceRange.baseMipLevel=0;
            ColorAttachmentView.subresourceRange.levelCount=1;
            ColorAttachmentView.subresourceRange.baseArrayLayer=0;
            ColorAttachmentView.subresourceRange.layerCount=1;
            ColorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ColorAttachmentView.flags=0;

            Buffers[i].Image = Images[i];
            
            ColorAttachmentView.image = Buffers[i].Image;
            
            Error = vkCreateImageView(Device, &ColorAttachmentView, nullptr, &Buffers[i].View);
            assert(!Error);
        }


    }

    VkResult AcquireNextImage(VkSemaphore PresentCompleteSemahpore, uint32_t *ImageIndex)
    {
        return AcquireNextImageKHR(Device, Swapchain, UINT64_MAX, PresentCompleteSemahpore, (VkFence)nullptr, ImageIndex);
    }

    VkResult QueuePresent(VkQueue Queue, uint32_t ImageIndex, VkSemaphore WaitSemaphore = VK_NULL_HANDLE)
    {
        VkPresentInfoKHR PresentInfo {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        PresentInfo.pNext=nullptr;
        PresentInfo.swapchainCount=1;
        PresentInfo.pSwapchains=&Swapchain;
        PresentInfo.pImageIndices=&ImageIndex;
        if(WaitSemaphore != VK_NULL_HANDLE)
        {
            PresentInfo.pWaitSemaphores=&WaitSemaphore;
            PresentInfo.waitSemaphoreCount=1;
        }
        return QueuePresentKHR(Queue, &PresentInfo);

    }
};
