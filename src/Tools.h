#pragma once
#include "Device.h"

#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \

#define SizeOfArray(array) \
    sizeof(array) / sizeof(array[0]) \

struct framebufferAttachment;
class buffer;
struct sceneMesh;

namespace vulkanTools
{
    VkBool32 CheckDeviceExtensionPresent(VkPhysicalDevice PhysicalDevice, const char *ExtensionName);

    VkBool32 GetSupportedDepthFormat(VkPhysicalDevice PhysicalDevice, VkFormat *DepthFormat);



    VkSemaphoreCreateInfo BuildSemaphoreCreateInfo();

    VkSubmitInfo BuildSubmitInfo();

    VkInstance CreateInstance(bool EnableValidation);

    VkCommandPool CreateCommandPool(VkDevice Device, uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags CreateFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VkCommandBufferAllocateInfo BuildCommandBufferAllocateInfo(VkCommandPool CommandPool, VkCommandBufferLevel Level, uint32_t BufferCount);

    VkDescriptorPoolSize BuildDescriptorPoolSize(VkDescriptorType Type, uint32_t DescriptorCount);

    VkDescriptorPoolCreateInfo BuildDescriptorPoolCreateInfo(uint32_t PoolSizeCount, VkDescriptorPoolSize *PoolSizes, uint32_t MaxSets);
    VkDescriptorPoolCreateInfo BuildDescriptorPoolCreateInfo(const std::vector<VkDescriptorPoolSize> &PoolSizes, uint32_t MaxSets);

    VkMemoryAllocateInfo BuildMemoryAllocateInfo();

    VkCommandBufferBeginInfo BuildCommandBufferBeginInfo();
    

    VkBufferCreateInfo BuildBufferCreateInfo();

    VkBufferCreateInfo BuildBufferCreateInfo(VkBufferUsageFlags UsageFlags, VkDeviceSize DeviceSize);

    VkImageMemoryBarrier BuildImageMemoryBarrier();

    VkFenceCreateInfo BuildFenceCreateInfo(VkFenceCreateFlags Flags);

    VkImageCreateInfo BuildImageCreateInfo();

    VkVertexInputBindingDescription BuildVertexInputBindingDescription(uint32_t Binding, uint32_t Stride, VkVertexInputRate InputRate);

    VkPipelineVertexInputStateCreateInfo BuildPipelineVertexInputStateCreateInfo();

    VkVertexInputAttributeDescription BuildVertexInputAttributeDescription(uint32_t Binding, uint32_t Location, VkFormat Format, uint32_t Offset);

    VkImageViewCreateInfo BuildImageViewCreateInfo();
    
    VkFramebufferCreateInfo BuildFramebufferCreateInfo();

    VkSamplerCreateInfo BuildSamplerCreateInfo();
    
    VkPipelineLayoutCreateInfo BuildPipelineLayoutCreateInfo(uint32_t SetLayoutCount=1);
    
    VkPipelineLayoutCreateInfo BuildPipelineLayoutCreateInfo(const VkDescriptorSetLayout *SetLayouts, uint32_t SetLayoutCount=1);
    
    VkDescriptorSetAllocateInfo BuildDescriptorSetAllocateInfo(VkDescriptorPool DescriptorPool, const VkDescriptorSetLayout *SetLayouts, uint32_t DescriptorSetCount);

    VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType Type, VkShaderStageFlags StageFlags, uint32_t Binding, uint32_t Count=1);
    
    VkDescriptorSetLayoutCreateInfo BuildDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *pBindings, uint32_t BindingCount);
    VkDescriptorSetLayoutCreateInfo BuildDescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding> & Bindings);

    VkDescriptorImageInfo BuildDescriptorImageInfo(VkSampler Sampler, VkImageView ImageView, VkImageLayout ImageLayout);

    VkWriteDescriptorSet BuildWriteDescriptorSet(VkDescriptorSet DestSet, VkDescriptorType Type, uint32_t Binding, VkDescriptorBufferInfo *BufferInfo);
    
    VkWriteDescriptorSet BuildWriteDescriptorSet(VkDescriptorSet DestSet, VkDescriptorType Type, uint32_t Binding, VkDescriptorImageInfo *ImageInfo, uint32_t DescriptorCount=1);

    VkPipelineInputAssemblyStateCreateInfo BuildPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology Topology, VkPipelineInputAssemblyStateCreateFlags Flags, VkBool32 PrimitiveRestartEnable);

    VkPipelineRasterizationStateCreateInfo BuildPipelineRasterizationStateCreateInfo(VkPolygonMode PolygonMode, VkCullModeFlags CullMode, VkFrontFace FrontFace, VkPipelineRasterizationStateCreateFlags Flags=0);

    VkPipelineColorBlendAttachmentState BuildPipelineColorBlendAttachmentState(VkColorComponentFlags ColorWriteMask, VkBool32 BlendEnable);

    VkPipelineColorBlendStateCreateInfo BuildPipelineColorBlendStateCreateInfo(uint32_t AttachmentCount, const VkPipelineColorBlendAttachmentState *PAttachments); 

    VkPipelineDepthStencilStateCreateInfo BuildPipelineDepthStencilStateCreateInfo(VkBool32 DepthTestEnable, VkBool32 DepthWriteEnable, VkCompareOp CompareOp);

    VkPipelineViewportStateCreateInfo BuildPipelineViewportStateCreateInfo(uint32_t ViewportCount, uint32_t ScissorCount, VkPipelineViewportStateCreateFlags Flags);

    VkPipelineMultisampleStateCreateInfo BuildPipelineMultisampleStateCreateInfo(VkSampleCountFlagBits RasterizationSamples, VkPipelineMultisampleStateCreateFlags Flags=0);

    VkPipelineDynamicStateCreateInfo BuildPipelineDynamicStateCreateInfo(const VkDynamicState *PDynamicStates, uint32_t DynamicStateCount, VkPipelineDynamicStateCreateFlags=0);

    VkPipelineDynamicStateCreateInfo BuildPipelineDynamicStateCreateInfo(const std::vector<VkDynamicState> &DynamicStates, VkPipelineDynamicStateCreateFlags Flags=0);

    VkGraphicsPipelineCreateInfo BuildGraphicsPipelineCreateInfo(VkPipelineLayout Layout = VK_NULL_HANDLE, VkRenderPass RenderPass = VK_NULL_HANDLE, VkPipelineCreateFlags Flags =0);
    
    VkSpecializationMapEntry BuildSpecializationMapEntry(uint32_t ConstantID, uint32_t Offset, size_t Size);

    VkSpecializationInfo BuildSpecializationInfo(uint32_t MapEntryCount, const VkSpecializationMapEntry *MapEntries, size_t DataSize, const void *Data);

    VkDescriptorSetLayoutBinding BuildDescriptorSetLayoutBinding(VkDescriptorType Type, VkShaderStageFlags Flags, uint32_t Binding, uint32_t Count=1); 

    VkRenderPassBeginInfo BuildRenderPassBeginInfo();

    VkAccelerationStructureGeometryKHR BuildAccelerationStructureGeometry();
    VkAccelerationStructureBuildGeometryInfoKHR BuildAccelerationStructureBuildGeometryInfo();
    VkAccelerationStructureBuildSizesInfoKHR BuildAccelerationStructureBuildSizesInfo();
    VkRayTracingPipelineCreateInfoKHR BuildRayTracingPipelineCreateInfo(); 
    VkWriteDescriptorSetAccelerationStructureKHR BuildWriteDescriptorSetAccelerationStructure();

    VkViewport BuildViewport(float Width, float Height, float MinDepth, float MaxDepth, float StartX=0, float StartY=0);
    
    VkRect2D BuildRect2D(int32_t Width, int32_t Height, int32_t OffsetX, int32_t OffsetY);

    VkPushConstantRange BuildPushConstantRange(VkShaderStageFlags StageFlags, uint32_t Size, uint32_t Offset);

    void FlushCommandBuffer(VkDevice Device, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer, VkQueue Queue, bool Free);

    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout, VkImageSubresourceRange SubresourceRange);
    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout, VkPipelineStageFlags SrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  VkPipelineStageFlags DstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout,  VkImageSubresourceRange SubresourceRange, VkPipelineStageFlags SrcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,  VkPipelineStageFlags DstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VkBool32 CreateBuffer(vulkanDevice *VulkanDevice, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, VkDeviceSize Size, void *Data, VkBuffer *Buffer, VkDeviceMemory *DeviceMemory);
    VkResult CreateBuffer(vulkanDevice *VulkanDevice, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, buffer *Buffer, VkDeviceSize Size, void *Data=nullptr);
    
    void CopyBuffer(vulkanDevice *VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, buffer *Source, buffer *Dest);

    VkCommandBuffer CreateCommandBuffer(VkDevice Device, VkCommandPool CommandPool,VkCommandBufferLevel Level, bool Begin);


    void CreateAttachment(vulkanDevice *Device, VkFormat Format, VkImageUsageFlags Usage, framebufferAttachment *Attachment, VkCommandBuffer LayoutCommand, uint32_t Width, uint32_t Height);

    void CreateAndFillBuffer(vulkanDevice *Device, void *Data, size_t DataSize, VkBuffer *Buffer, VkDeviceMemory *Memory, VkBufferUsageFlags Flags, VkCommandBuffer CommandBuffer, VkQueue Queue);
    void CreateAndFillBuffer(vulkanDevice *Device, void *Data, size_t DataSize, buffer *Buffer, VkBufferUsageFlags Flags, VkCommandBuffer CommandBuffer, VkQueue Queue);

    uint64_t GetBufferDeviceAddress(vulkanDevice *VulkanDevice, VkBuffer Buffer);

    sceneMesh BuildQuad(vulkanDevice *VulkanDevice);

    uint32_t AlignedSize(uint32_t Value, uint32_t Alignment);
}