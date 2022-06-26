#include "Image.h"

#include "Tools.h"
void storageImage::Create(vulkanDevice *_VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkFormat _Format, VkExtent3D Extent)
{
    this->VulkanDevice = _VulkanDevice;
    this->Format = _Format;

    if(Image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
        vkDestroyImage(VulkanDevice->Device, Image, nullptr);
        vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
        Image = VK_NULL_HANDLE;
    }

    VkImageCreateInfo ImageCreateInfo = vulkanTools::BuildImageCreateInfo();
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = _Format;
    ImageCreateInfo.extent = Extent;
    ImageCreateInfo.mipLevels=1;
    ImageCreateInfo.arrayLayers=1;
    ImageCreateInfo.samples=VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ImageCreateInfo.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CALL(vkCreateImage(VulkanDevice->Device, &ImageCreateInfo, nullptr, &Image));

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(VulkanDevice->Device, Image, &MemoryRequirements);
    VkMemoryAllocateInfo MemoryAllocateInfo = vulkanTools::BuildMemoryAllocateInfo();
    MemoryAllocateInfo.allocationSize=MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindImageMemory(VulkanDevice->Device, Image, Memory, 0));

    VkImageViewCreateInfo ColorImageView = vulkanTools::BuildImageViewCreateInfo();
    ColorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ColorImageView.format = Format;
    ColorImageView.subresourceRange = {};
    ColorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ColorImageView.subresourceRange.baseMipLevel=0;
    ColorImageView.subresourceRange.levelCount=1;
    ColorImageView.subresourceRange.baseArrayLayer=0;
    ColorImageView.subresourceRange.layerCount=1;
    ColorImageView.image = Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &ColorImageView, nullptr, &ImageView));

    VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vulkanTools::TransitionImageLayout(
        CommandBuffer,
        Image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    );
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, CommandBuffer, Queue, true);

}

void storageImage::Destroy()
{
    vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
    vkDestroyImage(VulkanDevice->Device, Image, nullptr);
    vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
}
