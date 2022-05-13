#pragma once

#include <Device.h>
#include "Tools.h"

#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \



#pragma warning ( disable : 4458; disable : 4996 )
#include <gli/gli.hpp>
#pragma warning ( default : 4458; default : 4996 )

struct vulkanTexture
{
    VkSampler Sampler;
    VkImage Image;
    VkImageLayout ImageLayout;
    VkDeviceMemory DeviceMemory;
    VkImageView View;
    uint32_t Width, Height;
    uint32_t MipLevels;
    uint32_t LayerCount;
    VkDescriptorImageInfo Descriptor;
};

class textureLoader
{
public:
    vulkanDevice *VulkanDevice;
    VkQueue Queue;
    VkCommandPool CommandPool;
    VkCommandBuffer CommandBuffer;
    textureLoader(vulkanDevice *VulkanDevice, VkQueue Queue, VkCommandPool CommandPool) :
                  VulkanDevice(VulkanDevice), Queue(Queue), CommandPool(CommandPool)
    {
        VkCommandBufferAllocateInfo CommandBufferInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        CommandBufferInfo.commandPool = CommandPool;
        CommandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CommandBufferInfo.commandBufferCount =1;

        VK_CALL(vkAllocateCommandBuffers(VulkanDevice->Device, &CommandBufferInfo, &CommandBuffer));
    }

    void LoadTexture2D(std::string Filename, VkFormat Format, vulkanTexture *Texture, VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        gli::texture2D GliTexture(gli::load(Filename.c_str()));
        
        Texture->Width = static_cast<uint32_t>(GliTexture[0].dimensions().x);
        Texture->Height = static_cast<uint32_t>(GliTexture[0].dimensions().y);
        Texture->MipLevels = static_cast<uint32_t>(GliTexture.levels());

        VkFormatProperties FormatProperties;
        vkGetPhysicalDeviceFormatProperties(VulkanDevice->PhysicalDevice, Format, &FormatProperties);


        VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
        VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));

        VkBuffer StagingBuffer;

        VkBufferCreateInfo BufferCreateInfo = vulkanTools::BuildBufferCreateInfo();
        BufferCreateInfo.size = GliTexture.size();
        BufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        BufferCreateInfo.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &StagingBuffer));

        VkMemoryRequirements MemoryRequirements;
        vkGetBufferMemoryRequirements(VulkanDevice->Device, StagingBuffer, &MemoryRequirements);

        VkDeviceMemory StagingMemory;
        VkMemoryAllocateInfo MemoryAllocateInfo = vulkanTools::BuildMemoryAllocateInfo();
        MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
        MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &StagingMemory));
        VK_CALL(vkBindBufferMemory(VulkanDevice->Device, StagingBuffer, StagingMemory, 0));

        uint8_t *Data=nullptr;
        VK_CALL(vkMapMemory(VulkanDevice->Device, StagingMemory, 0, MemoryRequirements.size, 0, (void**)&Data));
        memcpy(Data, GliTexture.data(), GliTexture.size());
        vkUnmapMemory(VulkanDevice->Device, StagingMemory);

        std::vector<VkBufferImageCopy> BufferCopyRegions;
        uint32_t Offset = 0;

        for(uint32_t i=0; i<Texture->MipLevels; i++)
        {
            VkBufferImageCopy BufferCopyRegion = {};
            BufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            BufferCopyRegion.imageSubresource.mipLevel = i;
            BufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            BufferCopyRegion.imageSubresource.layerCount=1;
            BufferCopyRegion.imageExtent.width = static_cast<uint32_t>(GliTexture[i].dimensions().x);
            BufferCopyRegion.imageExtent.height = static_cast<uint32_t>(GliTexture[i].dimensions().y);
            BufferCopyRegion.imageExtent.depth = 1;
            BufferCopyRegion.bufferOffset = Offset;

            BufferCopyRegions.push_back(BufferCopyRegion);

            Offset += static_cast<uint32_t>(GliTexture[i].size());
        }

        VkImageCreateInfo ImageCreateInfo = vulkanTools::BuildImageCreateInfo();
        ImageCreateInfo.imageType=VK_IMAGE_TYPE_2D;
        ImageCreateInfo.format=Format;
        ImageCreateInfo.mipLevels=Texture->MipLevels;
        ImageCreateInfo.arrayLayers=1;
        ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        ImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ImageCreateInfo.extent = {Texture->Width, Texture->Height, 1};
        ImageCreateInfo.usage = ImageUsageFlags;

        //Ensure transfer dst
        if(!(ImageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            ImageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CALL(vkCreateImage(VulkanDevice->Device, &ImageCreateInfo, nullptr, &Texture->Image));

        vkGetImageMemoryRequirements(VulkanDevice->Device, Texture->Image, &MemoryRequirements);
        MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
        MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Texture->DeviceMemory));
        VK_CALL(vkBindImageMemory(VulkanDevice->Device, Texture->Image, Texture->DeviceMemory, 0)); 

        VkImageSubresourceRange SubresourceRange = {};
        SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        SubresourceRange.baseMipLevel=0;
        SubresourceRange.levelCount=Texture->MipLevels;
        SubresourceRange.layerCount=1;

        //Transition to transfer dest
        vulkanTools::TransitionImageLayout(CommandBuffer,
                       Texture->Image,
                       VK_IMAGE_ASPECT_COLOR_BIT,
                       VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       SubresourceRange);
        
        vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer, Texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(BufferCopyRegions.size()), BufferCopyRegions.data());
        
        //Transition to shader read
        Texture->ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vulkanTools::TransitionImageLayout(CommandBuffer, 
                                    Texture->Image, 
                                    VK_IMAGE_ASPECT_COLOR_BIT, 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                                    Texture->ImageLayout, 
                                    SubresourceRange);

        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        VkFence CopyFence;
        VkFenceCreateInfo FenceCreateInfo = vulkanTools::BuildFenceCreateInfo(0);
        VK_CALL(vkCreateFence(VulkanDevice->Device, &FenceCreateInfo, nullptr, &CopyFence));


        VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, CopyFence));

        VK_CALL(vkWaitForFences(VulkanDevice->Device, 1, &CopyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
        
        vkDestroyFence(VulkanDevice->Device, CopyFence, nullptr);

        vkFreeMemory(VulkanDevice->Device, StagingMemory, nullptr);
        vkDestroyBuffer(VulkanDevice->Device, StagingBuffer, nullptr);


        //Create sampler
        VkSamplerCreateInfo SamplerCreateInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        SamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        SamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        SamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        SamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        SamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        SamplerCreateInfo.mipLodBias=0;
        SamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        SamplerCreateInfo.minLod=0;
        SamplerCreateInfo.maxLod=(float)Texture->MipLevels;
        SamplerCreateInfo.maxAnisotropy=8;
        SamplerCreateInfo.anisotropyEnable=VK_FALSE;
        SamplerCreateInfo.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CALL(vkCreateSampler(VulkanDevice->Device, &SamplerCreateInfo, nullptr, &Texture->Sampler));

        //IMage View
        VkImageViewCreateInfo ImageViewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ImageViewCreateInfo.pNext=nullptr;
        ImageViewCreateInfo.image = VK_NULL_HANDLE;
        ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewCreateInfo.format = Format;
        ImageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_R,VK_COMPONENT_SWIZZLE_G,VK_COMPONENT_SWIZZLE_B,VK_COMPONENT_SWIZZLE_A};
        ImageViewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        ImageViewCreateInfo.subresourceRange.levelCount = Texture->MipLevels;
        ImageViewCreateInfo.image = Texture->Image;
        VK_CALL(vkCreateImageView(VulkanDevice->Device, &ImageViewCreateInfo, nullptr, &Texture->View));

        Texture->Descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        Texture->Descriptor.imageView = Texture->View;
        Texture->Descriptor.sampler = Texture->Sampler;
    }

    void CreateTexture(void *Buffer, VkDeviceSize BufferSize, VkFormat Format, uint32_t Width, uint32_t Height, vulkanTexture *Texture, VkFilter Filter = VK_FILTER_LINEAR, VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        assert(Buffer);

        Texture->Width = Width;
        Texture->Height = Height;
        Texture->MipLevels = 1;

        VkMemoryAllocateInfo memAllocInfo = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Use a separate command buffer for texture loading
        VkCommandBufferBeginInfo cmdBufInfo = vulkanTools::BuildCommandBufferBeginInfo();
        VK_CALL(vkBeginCommandBuffer(CommandBuffer, &cmdBufInfo));

        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = vulkanTools::BuildBufferCreateInfo();
        bufferCreateInfo.size = BufferSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CALL(vkCreateBuffer(VulkanDevice->Device, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(VulkanDevice->Device, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &memAllocInfo, nullptr, &stagingMemory));
        VK_CALL(vkBindBufferMemory(VulkanDevice->Device, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CALL(vkMapMemory(VulkanDevice->Device, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, Buffer, BufferSize);
        vkUnmapMemory(VulkanDevice->Device, stagingMemory);

        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = Width;
        bufferCopyRegion.imageExtent.height = Height;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = 0;

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = vulkanTools::BuildImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = Format;
        imageCreateInfo.mipLevels = Texture->MipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { Texture->Width, Texture->Height, 1 };
        imageCreateInfo.usage = ImageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CALL(vkCreateImage(VulkanDevice->Device, &imageCreateInfo, nullptr, &Texture->Image));

        vkGetImageMemoryRequirements(VulkanDevice->Device, Texture->Image, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &memAllocInfo, nullptr, &Texture->DeviceMemory));
        VK_CALL(vkBindImageMemory(VulkanDevice->Device, Texture->Image, Texture->DeviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = Texture->MipLevels;
        subresourceRange.layerCount = 1;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        vulkanTools::TransitionImageLayout(
            CommandBuffer,
            Texture->Image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            CommandBuffer,
            stagingBuffer,
            Texture->Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );

        // Change texture image layout to shader read after all mip levels have been copied
        Texture->ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         vulkanTools::TransitionImageLayout(
            CommandBuffer,
            Texture->Image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            Texture->ImageLayout,
            subresourceRange);

        // Submit command buffer containing copy and image layout commands
        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        // Create a fence to make sure that the copies have finished before continuing
        VkFence copyFence;
        VkFenceCreateInfo fenceCreateInfo = vulkanTools::BuildFenceCreateInfo(0);
        VK_CALL(vkCreateFence(VulkanDevice->Device, &fenceCreateInfo, nullptr, &copyFence));

        VkSubmitInfo submitInfo = vulkanTools::BuildSubmitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &CommandBuffer;

        VK_CALL(vkQueueSubmit(Queue, 1, &submitInfo, copyFence));

        VK_CALL(vkWaitForFences(VulkanDevice->Device, 1, &copyFence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(VulkanDevice->Device, copyFence, nullptr);

        // Clean up staging resources
        vkFreeMemory(VulkanDevice->Device, stagingMemory, nullptr);
        vkDestroyBuffer(VulkanDevice->Device, stagingBuffer, nullptr);

        // Create sampler
        VkSamplerCreateInfo sampler = {};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = Filter;
        sampler.minFilter = Filter;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipLodBias = 0.0f;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = 0.0f;
        VK_CALL(vkCreateSampler(VulkanDevice->Device, &sampler, nullptr, &Texture->Sampler));

        // Create image view
        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.pNext = NULL;
        view.image = VK_NULL_HANDLE;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = Format;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.levelCount = 1;
        view.image = Texture->Image;
        VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Texture->View));

        // Fill descriptor image info that can be used for setting up descriptor sets
        Texture->Descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        Texture->Descriptor.imageView = Texture->View;
        Texture->Descriptor.sampler = Texture->Sampler;
    }

    void DestroyTexture(vulkanTexture Texture)
    {
        vkDestroyImageView(VulkanDevice->Device, Texture.View, nullptr);
        vkDestroyImage(VulkanDevice->Device, Texture.Image, nullptr);
        vkDestroySampler(VulkanDevice->Device, Texture.Sampler, nullptr);
        vkFreeMemory(VulkanDevice->Device, Texture.DeviceMemory, nullptr);
    }
};