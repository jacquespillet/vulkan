#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

class vulkanDevice;

struct framebufferAttachment
{
    VkImage Image;
    VkDeviceMemory Memory;
    VkImageView ImageView;
    VkFormat Format;
    void Destroy(VkDevice Device)
    {
        vkDestroyImage(Device, Image, nullptr);
        vkDestroyImageView(Device, ImageView, nullptr);
        vkFreeMemory(Device, Memory, nullptr);
    }
};

struct framebuffer
{
    int32_t Width, Height;
    VkFramebuffer Framebuffer;
    framebufferAttachment Depth;
    VkRenderPass RenderPass;

    std::vector<framebufferAttachment> _Attachments;
    std::vector<VkFormat> AttachmentFormats;

    VkSampler Sampler;

    bool HasDepth=true;

    VkImageUsageFlags ImageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

    framebuffer& SetLayout(VkImageLayout ImageLayout)
    {
        this->Layout = ImageLayout;
        return *this;
    }

    framebuffer& SetSize(uint32_t NewWidth, uint32_t NewHeight)
    {
        this->Width = NewWidth;
        this->Height = NewHeight;
        return *this;
    }
    framebuffer& SetAttachmentFormat(uint32_t AttachmentIndex, VkFormat Format)
    {
        AttachmentFormats[AttachmentIndex] = Format;
        return *this;
    }

    framebuffer& SetAttachmentCount(uint32_t AttachmentCount)
    {
        _Attachments.resize(AttachmentCount);
		AttachmentFormats.resize(AttachmentCount);
        return *this;
    }

    framebuffer& SetImageFlags(VkImageUsageFlagBits Flags)
    {
        ImageUsage = ImageUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        return *this;
    }

    framebuffer& Destroy(VkDevice Device)
    {
        vkDestroyFramebuffer(Device, Framebuffer, nullptr);
        vkDestroyRenderPass(Device, RenderPass, nullptr);
        vkDestroySampler(Device, Sampler, nullptr);
        for(size_t i=0; i<_Attachments.size(); i++)
        {
            _Attachments[i].Destroy(Device);
        }
        if(HasDepth)
        {
            Depth.Destroy(Device);
        }
        return *this;
    }

    void BuildBuffers(vulkanDevice *VulkanDevice, VkCommandBuffer LayoutCommand);
};