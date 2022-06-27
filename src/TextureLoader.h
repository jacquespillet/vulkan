#pragma once

#include <Device.h>
#include "Tools.h"
#include <glm/ext.hpp>
#include "Shader.h"
#include "Buffer.h"
#include "Framebuffer.h"

#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \

#include <stb_image.h>

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
    uint32_t Index;

    std::vector<uint8_t> Data;

    void Destroy(vulkanDevice *Device);
};

class textureLoader
{
public:
    vulkanDevice *VulkanDevice;
    VkQueue Queue;
    VkCommandPool CommandPool;
    VkCommandBuffer CommandBuffer;
    textureLoader(vulkanDevice *VulkanDevice, VkQueue Queue, VkCommandPool CommandPool);

    void LoadTexture2D(std::string Filename, VkFormat Format, vulkanTexture *Texture, VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT);

    void GenerateMipmaps(VkImage Image, uint32_t Width, uint32_t Height, uint32_t MipLevels);
    void GenerateCubemapMipmaps(VkImage Image, uint32_t Width, uint32_t Height, uint32_t MipLevels);

    void CreateTexture(void *Buffer, VkDeviceSize BufferSize, VkFormat Format, uint32_t Width, uint32_t Height, vulkanTexture *Texture, bool DoGenerateMipmaps=false, VkFilter Filter = VK_FILTER_LINEAR, VkImageUsageFlags ImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    void CreateEmptyTexture(uint32_t Width, uint32_t Height, VkFormat Format, vulkanTexture *Texture, VkImageUsageFlags ImageUsage = 0);

    void DestroyTexture(vulkanTexture Texture);

    void Destroy();

    void LoadCubemap(std::string FileName, vulkanTexture *Output);
};