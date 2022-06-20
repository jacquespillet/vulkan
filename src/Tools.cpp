#include "Tools.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include <iostream>
#include <Scene.h>

namespace vulkanTools
{
    VkBool32 CheckDeviceExtensionPresent(VkPhysicalDevice PhysicalDevice, const char *ExtensionName)
    {
        uint32_t ExtensionCount = 0;
        std::vector<VkExtensionProperties> Extensions;
        vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, nullptr);
        Extensions.resize(ExtensionCount);
        vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &ExtensionCount, Extensions.data());
        for(auto &Extension : Extensions)
        {
			if(!strcmp(ExtensionName, Extension.extensionName))
            {
                return true;
            }
        }
        return false;
    }

    VkBool32 GetSupportedDepthFormat(VkPhysicalDevice PhysicalDevice, VkFormat *DepthFormat)
    {
        std::vector<VkFormat> DepthFormats = {
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D16_UNORM
        };

        for(auto &Format : DepthFormats)
        {
            VkFormatProperties FormatProperties;
            vkGetPhysicalDeviceFormatProperties(PhysicalDevice, Format, &FormatProperties);

            if(FormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                *DepthFormat = Format;
                return true;
            }
        }
        return false;
    }



    VkSemaphoreCreateInfo BuildSemaphoreCreateInfo()
    {
        VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
        SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        SemaphoreCreateInfo.pNext = nullptr;
        SemaphoreCreateInfo.flags=0;
        return SemaphoreCreateInfo;
    }

    VkSubmitInfo BuildSubmitInfo()
    {
        VkSubmitInfo SubmitInfo = {};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.pNext = nullptr;
        return SubmitInfo;
    }

    VkInstance CreateInstance(bool EnableValidation)
    {
        VkApplicationInfo ApplicationInfo = {};
        ApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        ApplicationInfo.apiVersion = VK_API_VERSION_1_2;


        std::vector<const char*> EnabledExtensions = {VK_KHR_SURFACE_EXTENSION_NAME};
        EnabledExtensions.push_back("VK_KHR_win32_surface");
        VkInstanceCreateInfo InstanceCreateInfo = {};
        InstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        InstanceCreateInfo.pNext=nullptr;
        InstanceCreateInfo.pApplicationInfo = &ApplicationInfo;
        if(EnabledExtensions.size() >0)
        {
            if(EnableValidation)
            {
                EnabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            }
        }
        InstanceCreateInfo.enabledExtensionCount = (uint32_t)EnabledExtensions.size();
        InstanceCreateInfo.ppEnabledExtensionNames = EnabledExtensions.data();

        if(EnableValidation)
        {
            const char *ValidationLayersNames[] = 
            {
                "VK_LAYER_KHRONOS_validation",
            };
            InstanceCreateInfo.ppEnabledLayerNames = ValidationLayersNames;
            InstanceCreateInfo.enabledLayerCount = SizeOfArray(ValidationLayersNames);
        }

        VkInstance Instance;
        VkResult Result = vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance);
        assert(!Result);
        return Instance;
    }

    VkCommandPool CreateCommandPool(VkDevice Device, uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags CreateFlags)
    {
        VkCommandPoolCreateInfo CommandPoolCreateInfo = {};
        CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CommandPoolCreateInfo.queueFamilyIndex = QueueFamilyIndex;
        CommandPoolCreateInfo.flags = CreateFlags;
        VkCommandPool CmdPool;
        VK_CALL(vkCreateCommandPool(Device, &CommandPoolCreateInfo, nullptr, &CmdPool));
        return CmdPool;
    }

    VkCommandBufferAllocateInfo BuildCommandBufferAllocateInfo(VkCommandPool CommandPool, VkCommandBufferLevel Level, uint32_t BufferCount)
    {
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {};
        CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CommandBufferAllocateInfo.commandPool = CommandPool;
        CommandBufferAllocateInfo.level = Level;
        CommandBufferAllocateInfo.commandBufferCount = BufferCount;
        return CommandBufferAllocateInfo;
    }

    VkDescriptorPoolSize BuildDescriptorPoolSize(VkDescriptorType Type, uint32_t DescriptorCount)
    {
        VkDescriptorPoolSize DescriptorPoolSize = {};
        DescriptorPoolSize.type = Type;
        DescriptorPoolSize.descriptorCount = DescriptorCount;
        return DescriptorPoolSize;
    }
    VkDescriptorPoolCreateInfo BuildDescriptorPoolCreateInfo(const std::vector<VkDescriptorPoolSize> &PoolSizes, uint32_t MaxSets)
    {
        VkDescriptorPoolCreateInfo result{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        result.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());
        result.pPoolSizes = PoolSizes.data();
        result.maxSets = MaxSets;
        result.flags =  VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        return result;        
    }

    VkDescriptorPoolCreateInfo BuildDescriptorPoolCreateInfo(uint32_t PoolSizeCount, VkDescriptorPoolSize *PoolSizes, uint32_t MaxSets)
    {
        VkDescriptorPoolCreateInfo descriptorPoolInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        descriptorPoolInfo.pNext=nullptr;
        descriptorPoolInfo.poolSizeCount = PoolSizeCount;
        descriptorPoolInfo.pPoolSizes = PoolSizes;
        descriptorPoolInfo.maxSets=MaxSets;
        descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        return descriptorPoolInfo;
    }

    VkMemoryAllocateInfo BuildMemoryAllocateInfo()
    {
        VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        MemoryAllocateInfo.pNext = nullptr;
        MemoryAllocateInfo.allocationSize=0;
        MemoryAllocateInfo.memoryTypeIndex=0;
        return MemoryAllocateInfo;
    }

    VkCommandBufferBeginInfo BuildCommandBufferBeginInfo()
    {
        VkCommandBufferBeginInfo CommandBufferBeginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        CommandBufferBeginInfo.pNext=nullptr;
        return CommandBufferBeginInfo;
    }
    

    VkBufferCreateInfo BuildBufferCreateInfo()
    {
        VkBufferCreateInfo BufferCreatInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        return BufferCreatInfo;
    }

    VkBufferCreateInfo BuildBufferCreateInfo(VkBufferUsageFlags UsageFlags, VkDeviceSize DeviceSize)
    {
        VkBufferCreateInfo BufferCreatInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        BufferCreatInfo.pNext=nullptr;
        BufferCreatInfo.usage = UsageFlags;
        BufferCreatInfo.size = DeviceSize;
        BufferCreatInfo.flags=0;
        return BufferCreatInfo;
    }

    VkImageMemoryBarrier BuildImageMemoryBarrier()
    {
        VkImageMemoryBarrier ImageMemoryBarrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        ImageMemoryBarrier.pNext = nullptr;
        ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        return ImageMemoryBarrier;
    }

    VkFenceCreateInfo BuildFenceCreateInfo(VkFenceCreateFlags Flags)
    {
        VkFenceCreateInfo FenceCreateInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        FenceCreateInfo.flags = Flags;
        return FenceCreateInfo;
    }

    VkImageCreateInfo BuildImageCreateInfo()
    {
        VkImageCreateInfo ImageCreateInfo {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ImageCreateInfo.pNext=nullptr;
        return ImageCreateInfo;
    }

    VkVertexInputBindingDescription BuildVertexInputBindingDescription(uint32_t Binding, uint32_t Stride, VkVertexInputRate InputRate)
    {
        VkVertexInputBindingDescription VertexInputBindingdescription = {};
        VertexInputBindingdescription.binding = Binding;
        VertexInputBindingdescription.stride = Stride;
        VertexInputBindingdescription.inputRate = InputRate;
        return VertexInputBindingdescription;
    }

    VkPipelineVertexInputStateCreateInfo BuildPipelineVertexInputStateCreateInfo()
    {
        VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        PipelineVertexInputStateCreateInfo.pNext=nullptr;
        return PipelineVertexInputStateCreateInfo;
    }

    VkVertexInputAttributeDescription BuildVertexInputAttributeDescription(uint32_t Binding, uint32_t Location, VkFormat Format, uint32_t Offset)
    {
        VkVertexInputAttributeDescription VertexInputAttributeDescription = {};
        VertexInputAttributeDescription.location = Location;
        VertexInputAttributeDescription.binding = Binding;
        VertexInputAttributeDescription.format = Format;
        VertexInputAttributeDescription.offset = Offset;
        return VertexInputAttributeDescription;
    }

    
    VkImageViewCreateInfo BuildImageViewCreateInfo()
    {
        VkImageViewCreateInfo ImageViewCreateInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ImageViewCreateInfo.pNext=nullptr;
        return ImageViewCreateInfo;
    }

    VkFramebufferCreateInfo BuildFramebufferCreateInfo()
    {
        VkFramebufferCreateInfo FramebufferCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        FramebufferCreateInfo.pNext=nullptr;
        return FramebufferCreateInfo;
    }

    VkSamplerCreateInfo BuildSamplerCreateInfo()
    {
        VkSamplerCreateInfo SamplerCreateInfo {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        SamplerCreateInfo.pNext=nullptr;
        return SamplerCreateInfo;
    }

    VkPipelineLayoutCreateInfo BuildPipelineLayoutCreateInfo(uint32_t SetLayoutCount)
    {
        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        PipelineLayoutCreateInfo.setLayoutCount=SetLayoutCount;
        return PipelineLayoutCreateInfo;
    }
    
    VkPipelineLayoutCreateInfo BuildPipelineLayoutCreateInfo(const VkDescriptorSetLayout *SetLayouts, uint32_t SetLayoutCount)
    {
        VkPipelineLayoutCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        Result.pNext=nullptr;
        Result.setLayoutCount=SetLayoutCount;
        Result.pSetLayouts = SetLayouts;
        return Result;
    }

    VkDescriptorSetAllocateInfo BuildDescriptorSetAllocateInfo(VkDescriptorPool DescriptorPool, const VkDescriptorSetLayout *SetLayouts, uint32_t DescriptorSetCount)
    {
        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        DescriptorSetAllocateInfo.pNext=nullptr;
        DescriptorSetAllocateInfo.descriptorPool=DescriptorPool;
        DescriptorSetAllocateInfo.pSetLayouts=SetLayouts;
        DescriptorSetAllocateInfo.descriptorSetCount=DescriptorSetCount;
        return DescriptorSetAllocateInfo;
    }

    VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType Type, VkShaderStageFlags StageFlags, uint32_t Binding, uint32_t Count)
    {
        VkDescriptorSetLayoutBinding SetLayoutBinding= {};
        SetLayoutBinding.descriptorType = Type;
        SetLayoutBinding.stageFlags = StageFlags;
        SetLayoutBinding.binding = Binding;
        SetLayoutBinding.descriptorCount = Count;
        return SetLayoutBinding;

    }

    VkDescriptorSetLayoutCreateInfo BuildDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding *pBindings, uint32_t BindingCount)
    {
        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        DescriptorSetLayoutCreateInfo.pNext=nullptr;
        DescriptorSetLayoutCreateInfo.pBindings=pBindings;
        DescriptorSetLayoutCreateInfo.bindingCount=BindingCount;
        return DescriptorSetLayoutCreateInfo;
    }

    VkDescriptorSetLayoutCreateInfo BuildDescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding> & Bindings)
    {
        VkDescriptorSetLayoutCreateInfo Result{};
        Result.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        Result.pBindings = Bindings.data();
        Result.bindingCount = static_cast<uint32_t>(Bindings.size());
        return Result;        
    }

    VkDescriptorImageInfo BuildDescriptorImageInfo(VkSampler Sampler, VkImageView ImageView, VkImageLayout ImageLayout)
    {
        VkDescriptorImageInfo DescriptorImageInfo = {};
        DescriptorImageInfo.sampler = Sampler;
        DescriptorImageInfo.imageView = ImageView;
        DescriptorImageInfo.imageLayout = ImageLayout;
        return DescriptorImageInfo;
    }

    VkWriteDescriptorSet BuildWriteDescriptorSet(VkDescriptorSet DestSet, VkDescriptorType Type, uint32_t Binding, VkDescriptorBufferInfo *BufferInfo)
    {
        VkWriteDescriptorSet WriteDescriptorSet {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        WriteDescriptorSet.pNext=nullptr;
        WriteDescriptorSet.dstSet = DestSet;
        WriteDescriptorSet.descriptorType = Type;
        WriteDescriptorSet.dstBinding = Binding;
        WriteDescriptorSet.pBufferInfo = BufferInfo;
        WriteDescriptorSet.descriptorCount=1;
        return WriteDescriptorSet;
    }

    VkWriteDescriptorSet BuildWriteDescriptorSet(VkDescriptorSet DestSet, VkDescriptorType Type, uint32_t Binding, VkDescriptorImageInfo *ImageInfo, uint32_t DescriptorCount)
    {
        VkWriteDescriptorSet WriteDescriptorSet {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        WriteDescriptorSet.pNext=nullptr;
        WriteDescriptorSet.dstSet = DestSet;
        WriteDescriptorSet.descriptorType = Type;
        WriteDescriptorSet.dstBinding = Binding;
        WriteDescriptorSet.pImageInfo = ImageInfo;
        WriteDescriptorSet.descriptorCount=DescriptorCount;
        return WriteDescriptorSet;
    }

    VkPipelineInputAssemblyStateCreateInfo BuildPipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology Topology, VkPipelineInputAssemblyStateCreateFlags Flags, VkBool32 PrimitiveRestartEnable)
    {
        VkPipelineInputAssemblyStateCreateInfo Result{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        Result.topology = Topology;
        Result.flags = Flags;
        Result.primitiveRestartEnable = PrimitiveRestartEnable;
        return Result;
    }

    VkPipelineRasterizationStateCreateInfo BuildPipelineRasterizationStateCreateInfo(VkPolygonMode PolygonMode, VkCullModeFlags CullMode, VkFrontFace FrontFace, VkPipelineRasterizationStateCreateFlags Flags)
    {
        VkPipelineRasterizationStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        Result.polygonMode = PolygonMode;
        Result.cullMode = CullMode;
        Result.frontFace = FrontFace;
        Result.flags = Flags;
        Result.depthClampEnable = VK_FALSE;
        Result.lineWidth = 1.0f;
        return Result;
    }

    VkPipelineColorBlendAttachmentState BuildPipelineColorBlendAttachmentState(VkColorComponentFlags ColorWriteMask, VkBool32 BlendEnable)
    {
        VkPipelineColorBlendAttachmentState Result = {};
        Result.colorWriteMask = ColorWriteMask;
        Result.blendEnable = BlendEnable;
        return Result;
    }

    VkPipelineColorBlendStateCreateInfo BuildPipelineColorBlendStateCreateInfo(uint32_t AttachmentCount, const VkPipelineColorBlendAttachmentState *PAttachments)
    {
        VkPipelineColorBlendStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        Result.pNext=nullptr;
        Result.attachmentCount=AttachmentCount;
        Result.pAttachments = PAttachments;
        return Result;
    }

    VkPipelineDepthStencilStateCreateInfo BuildPipelineDepthStencilStateCreateInfo(VkBool32 DepthTestEnable, VkBool32 DepthWriteEnable, VkCompareOp CompareOp)
    {
        VkPipelineDepthStencilStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        Result.depthTestEnable = DepthTestEnable;
        Result.depthWriteEnable = DepthWriteEnable;
        Result.depthCompareOp = CompareOp;
        Result.front = Result.back;
        Result.back.compareOp = VK_COMPARE_OP_ALWAYS;
        return Result;
    }

    VkPipelineViewportStateCreateInfo BuildPipelineViewportStateCreateInfo(uint32_t ViewportCount, uint32_t ScissorCount, VkPipelineViewportStateCreateFlags Flags)
    {
        VkPipelineViewportStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        Result.viewportCount=ViewportCount;
        Result.scissorCount=ScissorCount;
        Result.flags=Flags;
        return Result;
    }

    VkPipelineMultisampleStateCreateInfo BuildPipelineMultisampleStateCreateInfo(VkSampleCountFlagBits RasterizationSamples, VkPipelineMultisampleStateCreateFlags Flags)
    {
        VkPipelineMultisampleStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        Result.rasterizationSamples = RasterizationSamples;
        return Result;
    }

    VkPipelineDynamicStateCreateInfo BuildPipelineDynamicStateCreateInfo(const VkDynamicState *PDynamicStates, uint32_t DynamicStateCount, VkPipelineDynamicStateCreateFlags Flags)
    {
        VkPipelineDynamicStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        Result.pDynamicStates = PDynamicStates;
        Result.dynamicStateCount = DynamicStateCount;
        return Result;
    }

    VkPipelineDynamicStateCreateInfo BuildPipelineDynamicStateCreateInfo(const std::vector<VkDynamicState> &DynamicStates, VkPipelineDynamicStateCreateFlags Flags)
    {
        VkPipelineDynamicStateCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        Result.pDynamicStates = DynamicStates.data();
        Result.dynamicStateCount = (uint32_t)DynamicStates.size();
        return Result;
    }

    VkGraphicsPipelineCreateInfo BuildGraphicsPipelineCreateInfo(VkPipelineLayout Layout, VkRenderPass RenderPass, VkPipelineCreateFlags Flags)
    {
        VkGraphicsPipelineCreateInfo Result {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        Result.pNext=nullptr;
        Result.layout = Layout;
        Result.renderPass = RenderPass;
        Result.flags = Flags;
        return Result;
    }


    VkSpecializationMapEntry BuildSpecializationMapEntry(uint32_t ConstantID, uint32_t Offset, size_t Size)
    {
        VkSpecializationMapEntry Result = {};
        Result.constantID = ConstantID;
        Result.offset = Offset;
        Result.size = Size;
        return Result;
    }

    VkSpecializationInfo BuildSpecializationInfo(uint32_t MapEntryCount, const VkSpecializationMapEntry *MapEntries, size_t DataSize, const void *Data)
    {
        VkSpecializationInfo Result = {};
        Result.mapEntryCount = MapEntryCount;
        Result.pMapEntries = MapEntries;
        Result.dataSize = DataSize;
        Result.pData = Data;
        return Result;
    }

    VkDescriptorSetLayoutBinding BuildDescriptorSetLayoutBinding(VkDescriptorType Type, VkShaderStageFlags Flags, uint32_t Binding, uint32_t Count)
    {
        VkDescriptorSetLayoutBinding Result = {};
        Result.descriptorType = Type;
        Result.stageFlags = Flags;
        Result.binding = Binding;
        Result.descriptorCount = Count;
        return Result;
    }

    VkRenderPassBeginInfo BuildRenderPassBeginInfo()
    {
        VkRenderPassBeginInfo Result {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        Result.pNext=nullptr;
        return Result;
    }
    
    VkViewport BuildViewport(float Width, float Height, float MinDepth, float MaxDepth, float StartX, float StartY)
    {
		VkViewport Result = {};
        Result.width = Width;
        Result.height = Height;
        Result.minDepth = MinDepth;
        Result.maxDepth = MaxDepth;
        Result.x = StartX;
        Result.y = StartY;
        return Result;
    }
    
    VkRect2D BuildRect2D(int32_t Width, int32_t Height, int32_t OffsetX, int32_t OffsetY)
    {
		VkRect2D Result = {};
        Result.extent.width = Width;
        Result.extent.height = Height;
        Result.offset.x = OffsetX;
        Result.offset.y = OffsetY;
        return Result;
    }

    VkPushConstantRange BuildPushConstantRange(VkShaderStageFlags StageFlags, uint32_t Size, uint32_t Offset)
    {
        VkPushConstantRange pushConstantRange {};
        pushConstantRange.stageFlags = StageFlags;
        pushConstantRange.offset = Offset;
        pushConstantRange.size = Size;
        return pushConstantRange;        
    }

    VkAccelerationStructureGeometryKHR BuildAccelerationStructureGeometry()
    {
        VkAccelerationStructureGeometryKHR Result {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        return Result;
    }

    
    VkAccelerationStructureBuildGeometryInfoKHR BuildAccelerationStructureBuildGeometryInfo()
    {
        VkAccelerationStructureBuildGeometryInfoKHR Result {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        return Result;
    }

    VkAccelerationStructureBuildSizesInfoKHR BuildAccelerationStructureBuildSizesInfo()
    {
        VkAccelerationStructureBuildSizesInfoKHR Result {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        return Result;
    }

    VkRayTracingPipelineCreateInfoKHR BuildRayTracingPipelineCreateInfo()
    {
        VkRayTracingPipelineCreateInfoKHR Result {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
        return Result;
    }
    VkWriteDescriptorSetAccelerationStructureKHR BuildWriteDescrpiptorSetAccelerationStructureKHR()
    {
        VkWriteDescriptorSetAccelerationStructureKHR Result {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        return Result;
    }

    VkWriteDescriptorSetAccelerationStructureKHR BuildWriteDescriptorSetAccelerationStructure()
    {
        VkWriteDescriptorSetAccelerationStructureKHR Result {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
        return Result;
    }

    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout, VkImageSubresourceRange SubresourceRange)
    {
        VkImageMemoryBarrier ImageMemoryBarrier = BuildImageMemoryBarrier();
        ImageMemoryBarrier.oldLayout = OldImageLayout;
        ImageMemoryBarrier.newLayout = NewImageLayout;
        ImageMemoryBarrier.image = Image;
        ImageMemoryBarrier.subresourceRange = SubresourceRange;
        
        
        VkPipelineStageFlags SrcStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        VkPipelineStageFlags DstStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

        //Defines actions that need to be finished on the old layout before the image is transitionned to new layout.
        switch (OldImageLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            ImageMemoryBarrier.srcAccessMask=0;
            break;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            //Pre init stage - make sure host finished writing in
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            //Make sure writes have finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            //Make sure writes have finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            //Image is a source of transfer --> make sure reading is finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            //Image is a dest of transfer --> make sure write is finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			SrcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            //Image is read in shaders --> make sure shaders finished reading.
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
            break;
        }

        //Defines actions that need to be finished on the old layout before the image is transitionned to new layout.
        switch (NewImageLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_TRANSFER_WRITE_BIT;
            DstStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            //Image will be used as a source of transfers --> make sure any read / write have finished on the image
            ImageMemoryBarrier.srcAccessMask = ImageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            ImageMemoryBarrier.dstAccessMask =ImageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            if(ImageMemoryBarrier.srcAccessMask==0)
            {
                ImageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			DstStageFlags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            break;
        }


        vkCmdPipelineBarrier(CommandBuffer, 
                             SrcStageFlags, 
                             DstStageFlags,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &ImageMemoryBarrier);
    }

    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageAspectFlags AspectMask, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout, VkPipelineStageFlags SrcStage,  VkPipelineStageFlags DstStage)
    {
        VkImageSubresourceRange SubresourceRange = {};
        SubresourceRange.aspectMask = AspectMask;
        SubresourceRange.baseMipLevel = 0;
        SubresourceRange.levelCount = 1;
        SubresourceRange.layerCount = 1;

        VkImageMemoryBarrier ImageMemoryBarrier = BuildImageMemoryBarrier();
        ImageMemoryBarrier.oldLayout = OldImageLayout;
        ImageMemoryBarrier.newLayout = NewImageLayout;
        ImageMemoryBarrier.image = Image;
        ImageMemoryBarrier.subresourceRange = SubresourceRange;
        
        //Defines actions that need to be finished on the old layout before the image is transitionned to new layout.
        switch (OldImageLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            ImageMemoryBarrier.srcAccessMask=0;
            break;
        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            //Pre init stage - make sure host finished writing in
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            //Make sure writes have finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            //Make sure writes have finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            //Image is a source of transfer --> make sure reading is finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            //Image is a dest of transfer --> make sure write is finished
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            //Image is read in shaders --> make sure shaders finished reading.
            ImageMemoryBarrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
            break;
        }

        //Defines actions that need to be finished on the old layout before the image is transitionned to new layout.
        switch (NewImageLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            //Image will be used as a source of transfers --> make sure any read / write have finished on the image
            ImageMemoryBarrier.srcAccessMask = ImageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_TRANSFER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            ImageMemoryBarrier.dstAccessMask =VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            ImageMemoryBarrier.dstAccessMask =ImageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            if(ImageMemoryBarrier.srcAccessMask==0)
            {
                ImageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
        }


        vkCmdPipelineBarrier(CommandBuffer, 
                             SrcStage, 
                             DstStage,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &ImageMemoryBarrier);
    }    

    void TransitionImageLayout(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout OldImageLayout, VkImageLayout NewImageLayout,  VkImageSubresourceRange SubresourceRange, VkPipelineStageFlags SrcStage,  VkPipelineStageFlags DstStage)
    {
        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier = vulkanTools::BuildImageMemoryBarrier();
        imageMemoryBarrier.oldLayout = OldImageLayout;
        imageMemoryBarrier.newLayout = NewImageLayout;
        imageMemoryBarrier.image = Image;
        imageMemoryBarrier.subresourceRange = SubresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch (OldImageLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            // Image layout is undefined (or does not matter)
            // Only valid as initial layout
            // No flags required, listed only for completeness
            imageMemoryBarrier.srcAccessMask = 0;
            break;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            // Image is preinitialized
            // Only valid as initial layout for linear images, preserves memory contents
            // Make sure host writes have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image is a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image is a depth/stencil attachment
            // Make sure any writes to the depth/stencil buffer have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image is a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image is a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image is read by a shader
            // Make sure any shader reads from the image have been finished
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch (NewImageLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            // Image will be used as a transfer destination
            // Make sure any writes to the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            // Image will be used as a transfer source
            // Make sure any reads from the image have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            // Image will be used as a color attachment
            // Make sure any writes to the color buffer have been finished
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            // Image layout will be used as a depth/stencil attachment
            // Make sure any writes to depth/stencil buffer have been finished
            imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            // Image will be read in a shader (sampler, input attachment)
            // Make sure any writes to the image have been finished
            if (imageMemoryBarrier.srcAccessMask == 0)
            {
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            }
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            // Other source layouts aren't handled (yet)
            break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
            CommandBuffer,
            SrcStage,
            DstStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);        
    }

    VkBool32 CreateBuffer(vulkanDevice *VulkanDevice, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, VkDeviceSize Size, void *Data, VkBuffer *Buffer, VkDeviceMemory *DeviceMemory)
    {
        VkMemoryRequirements MemoryRequirements;
        VkMemoryAllocateInfo MemoryAllocateInfo = BuildMemoryAllocateInfo();
        VkBufferCreateInfo BufferCreateInfo = BuildBufferCreateInfo(UsageFlags, Size);

        VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, Buffer));

        vkGetBufferMemoryRequirements(VulkanDevice->Device, *Buffer, &MemoryRequirements);
        MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
        MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, MemoryPropertyFlags);

        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, DeviceMemory));
        if(Data != nullptr)
        {
            void *Mapped;
            VK_CALL(vkMapMemory(VulkanDevice->Device, *DeviceMemory, 0, Size, 0, &Mapped));
            memcpy(Mapped, Data, Size);
            vkUnmapMemory(VulkanDevice->Device, *DeviceMemory);
        }
        VK_CALL(vkBindBufferMemory(VulkanDevice->Device, *Buffer, *DeviceMemory, 0));

        return true;
    }

    VkResult CreateBuffer(vulkanDevice *VulkanDevice, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags, buffer *Buffer, VkDeviceSize Size, void *Data)
    {
        Buffer->VulkanObjects.Device = VulkanDevice->Device;
        
        VkBufferCreateInfo BufferCreateInfo = BuildBufferCreateInfo(UsageFlags, Size);
        VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &Buffer->VulkanObjects.Buffer));

        VkMemoryRequirements MemoryRequirements;
        VkMemoryAllocateInfo MemoryAllocateInfo = BuildMemoryAllocateInfo();
        vkGetBufferMemoryRequirements(VulkanDevice->Device, Buffer->VulkanObjects.Buffer, &MemoryRequirements);
        MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
        MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, MemoryPropertyFlags);

        VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        if(UsageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
        }

        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Buffer->VulkanObjects.Memory));

        Buffer->VulkanObjects.Allignment = MemoryRequirements.alignment;
        Buffer->VulkanObjects.Size = Size;
        Buffer->VulkanObjects.UsageFlags = UsageFlags;
        Buffer->VulkanObjects.MemoryPropertyFlags = MemoryPropertyFlags;

        if(Data != nullptr)
        {
            VK_CALL(Buffer->Map());
            memcpy(Buffer->VulkanObjects.Mapped, Data, Size);
            Buffer->Unmap();
        }
        
        Buffer->SetupDescriptor();

        return Buffer->Bind();
    }

    VkCommandBuffer CreateCommandBuffer(VkDevice Device, VkCommandPool CommandPool, VkCommandBufferLevel Level, bool Begin)
    {
        VkCommandBuffer CommandBuffer;

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo = BuildCommandBufferAllocateInfo(CommandPool, Level, 1);
        VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &CommandBuffer));

        if(Begin)
        {
            VkCommandBufferBeginInfo CommandBufferInfo = BuildCommandBufferBeginInfo();
            VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));
        }

        return CommandBuffer;
    }
    void CreateAttachment(vulkanDevice *Device, VkFormat Format, VkImageUsageFlags Usage, framebufferAttachment *Attachment, VkCommandBuffer LayoutCommand, uint32_t Width, uint32_t Height)
    {
        VkImageAspectFlags AspectMask=0;
        Attachment->Format = Format;

        AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if(Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        assert(AspectMask>0);

        VkImageCreateInfo ImageCreateInfo = BuildImageCreateInfo();
        ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        ImageCreateInfo.format = Format;
        ImageCreateInfo.extent.width = Width;
        ImageCreateInfo.extent.height = Height;
        ImageCreateInfo.extent.depth = 1;
        ImageCreateInfo.mipLevels=1;
        ImageCreateInfo.arrayLayers=1;
        ImageCreateInfo.samples=VK_SAMPLE_COUNT_1_BIT;
        ImageCreateInfo.tiling=VK_IMAGE_TILING_OPTIMAL;
        ImageCreateInfo.usage = Usage | VK_IMAGE_USAGE_SAMPLED_BIT;
        ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkDedicatedAllocationImageCreateInfoNV DedicatedImageInfo {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_IMAGE_CREATE_INFO_NV};
        if(Device->EnableNVDedicatedAllocation)
        {
            DedicatedImageInfo.dedicatedAllocation=VK_TRUE;
            ImageCreateInfo.pNext = &DedicatedImageInfo;
        }
        VK_CALL(vkCreateImage(Device->Device, &ImageCreateInfo, nullptr, &Attachment->Image));

        VkMemoryAllocateInfo MemoryAllocateInfo = BuildMemoryAllocateInfo();
        VkMemoryRequirements MemoryRequirements;
        vkGetImageMemoryRequirements(Device->Device, Attachment->Image, &MemoryRequirements);
        MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
        MemoryAllocateInfo.memoryTypeIndex = Device->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkDedicatedAllocationMemoryAllocateInfoNV DedicatedAllocationInfo {VK_STRUCTURE_TYPE_DEDICATED_ALLOCATION_MEMORY_ALLOCATE_INFO_NV};
        if(Device->EnableNVDedicatedAllocation)
        {
            DedicatedAllocationInfo.image = Attachment->Image;
            MemoryAllocateInfo.pNext = &DedicatedAllocationInfo;
        }

        VK_CALL(vkAllocateMemory(Device->Device, &MemoryAllocateInfo, nullptr, &Attachment->Memory));
        VK_CALL(vkBindImageMemory(Device->Device, Attachment->Image, Attachment->Memory, 0));

        VkImageViewCreateInfo ImageViewCreateInfo = BuildImageViewCreateInfo();
        ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewCreateInfo.format =Format;
        ImageViewCreateInfo.subresourceRange = {};
        ImageViewCreateInfo.subresourceRange.aspectMask = AspectMask;
        ImageViewCreateInfo.subresourceRange.baseMipLevel=0;
        ImageViewCreateInfo.subresourceRange.levelCount=1;
        ImageViewCreateInfo.subresourceRange.baseArrayLayer=0;
        ImageViewCreateInfo.subresourceRange.layerCount=1;
        ImageViewCreateInfo.image = Attachment->Image;
        VK_CALL(vkCreateImageView(Device->Device, &ImageViewCreateInfo, nullptr, &Attachment->ImageView));
    }

    void FlushCommandBuffer(VkDevice Device, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer, VkQueue Queue, bool Free)
    {
        if(CommandBuffer == VK_NULL_HANDLE)
        {
            return;
        }

        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        SubmitInfo.commandBufferCount=1;
        SubmitInfo.pCommandBuffers=&CommandBuffer;

        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
        VK_CALL(vkQueueWaitIdle(Queue));

        if(Free)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
        }
    }


    VkComputePipelineCreateInfo BuildComputePipelineCreateInfo(VkPipelineLayout Layout, VkPipelineCreateFlags Flags)
    {
        VkComputePipelineCreateInfo Result {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        Result.layout = Layout;
        Result.flags = Flags;
        return Result;        
    }

    
    void CreateAndFillBuffer(vulkanDevice *Device, void *DataToCopy, size_t DataSize, VkBuffer *Buffer, VkDeviceMemory *Memory, VkBufferUsageFlags Flags, VkCommandBuffer CommandBuffer, VkQueue Queue)
    {
        void *Data;
        struct 
        {
            VkDeviceMemory Memory;
            VkBuffer Buffer;
        } Staging;
        VkMemoryAllocateInfo MemAlloc = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements MemoryRequirements;

        VkBufferCreateInfo BufferInfo;
        
        BufferInfo = BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, DataSize);
        VK_CALL(vkCreateBuffer(Device->Device, &BufferInfo, nullptr, &Staging.Buffer));
        
        vkGetBufferMemoryRequirements(Device->Device, Staging.Buffer, &MemoryRequirements);
        MemAlloc.allocationSize = MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = Device->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VK_CALL(vkAllocateMemory(Device->Device, &MemAlloc, nullptr, &Staging.Memory));
        
        VK_CALL(vkMapMemory(Device->Device, Staging.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
        memcpy(Data, DataToCopy, DataSize);
        vkUnmapMemory(Device->Device, Staging.Memory);
        VK_CALL(vkBindBufferMemory(Device->Device, Staging.Buffer, Staging.Memory, 0));

        BufferInfo = vulkanTools::BuildBufferCreateInfo(Flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, DataSize);
        VK_CALL(vkCreateBuffer(Device->Device, &BufferInfo, nullptr, Buffer));
        vkGetBufferMemoryRequirements(Device->Device, *Buffer, &MemoryRequirements);
        MemAlloc.allocationSize=MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = Device->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(Device->Device, &MemAlloc, nullptr, Memory));
        VK_CALL(vkBindBufferMemory(Device->Device, *Buffer, *Memory, 0));      


        VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
        VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));

        VkBufferCopy CopyRegion = {};

        CopyRegion.size = DataSize;
        vkCmdCopyBuffer(
            CommandBuffer,
            Staging.Buffer,
            *Buffer,
            1,
            &CopyRegion
        );

        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        SubmitInfo.commandBufferCount=1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;

        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
        VK_CALL(vkQueueWaitIdle(Queue));

        vkDestroyBuffer(Device->Device, Staging.Buffer, nullptr);
        vkFreeMemory(Device->Device, Staging.Memory, nullptr);
    }

    void CreateAndFillBuffer(vulkanDevice *Device, void *DataToCopy, size_t DataSize, buffer *Buffer, VkBufferUsageFlags Flags, VkCommandBuffer CommandBuffer, VkQueue Queue)
    {
        void *Data;
        struct 
        {
            VkDeviceMemory Memory;
            VkBuffer Buffer;
        } Staging;
        VkMemoryAllocateInfo MemAlloc = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements MemoryRequirements;

        VkBufferCreateInfo BufferInfo;
        
        BufferInfo = BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, DataSize);
        VK_CALL(vkCreateBuffer(Device->Device, &BufferInfo, nullptr, &Staging.Buffer));
        
        vkGetBufferMemoryRequirements(Device->Device, Staging.Buffer, &MemoryRequirements);
        MemAlloc.allocationSize = MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = Device->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        
        VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        if(Flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            MemAlloc.pNext = &MemoryAllocateFlagsInfo;
        }        

        VK_CALL(vkAllocateMemory(Device->Device, &MemAlloc, nullptr, &Staging.Memory));
        
        VK_CALL(vkMapMemory(Device->Device, Staging.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
        memcpy(Data, DataToCopy, DataSize);
        vkUnmapMemory(Device->Device, Staging.Memory);
        VK_CALL(vkBindBufferMemory(Device->Device, Staging.Buffer, Staging.Memory, 0));

        BufferInfo = vulkanTools::BuildBufferCreateInfo(Flags | VK_BUFFER_USAGE_TRANSFER_DST_BIT, DataSize);
        VK_CALL(vkCreateBuffer(Device->Device, &BufferInfo, nullptr, &Buffer->VulkanObjects.Buffer));
        vkGetBufferMemoryRequirements(Device->Device, Buffer->VulkanObjects.Buffer, &MemoryRequirements);
        MemAlloc.allocationSize=MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = Device->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(Device->Device, &MemAlloc, nullptr, &Buffer->VulkanObjects.Memory));
        VK_CALL(vkBindBufferMemory(Device->Device, Buffer->VulkanObjects.Buffer, Buffer->VulkanObjects.Memory, 0));      


        VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
        VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo));

        VkBufferCopy CopyRegion = {};

        CopyRegion.size = DataSize;
        vkCmdCopyBuffer(
            CommandBuffer,
            Staging.Buffer,
            Buffer->VulkanObjects.Buffer,
            1,
            &CopyRegion
        );

        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        SubmitInfo.commandBufferCount=1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;

        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
        VK_CALL(vkQueueWaitIdle(Queue));

        vkDestroyBuffer(Device->Device, Staging.Buffer, nullptr);
        vkFreeMemory(Device->Device, Staging.Memory, nullptr);


        Buffer->VulkanObjects.Allignment = MemoryRequirements.alignment;
        Buffer->VulkanObjects.Size = MemAlloc.allocationSize;
        Buffer->VulkanObjects.UsageFlags = Flags;
        Buffer->VulkanObjects.Device = Device->Device;

        Buffer->SetupDescriptor();
    }

    void CopyBuffer(vulkanDevice *VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, buffer *Source, buffer *Dest)
    { 
        assert(Dest->VulkanObjects.Size <= Source->VulkanObjects.Size);
        assert(Source->VulkanObjects.Buffer);

        VkCommandBuffer CopyCommand = CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy BufferCopy {};
        BufferCopy.size = Source->VulkanObjects.Size;

        vkCmdCopyBuffer(CopyCommand, Source->VulkanObjects.Buffer, Dest->VulkanObjects.Buffer, 1, &BufferCopy);
        FlushCommandBuffer(VulkanDevice->Device, CommandPool, CopyCommand, Queue, true);
    }
    
    void CopyImageToBuffer(vulkanDevice *VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkImage Source, buffer *Dest, uint32_t Width, uint32_t Height)
    {
        VkCommandBuffer CopyCommand = CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        // TransitionImageLayout(
        //     CopyCommand,
        //     Source,
        //     VK_IMAGE_ASPECT_COLOR_BIT,
        //     VK_IMAGE_LAYOUT_GENERAL,
        //     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        // );

        VkImageSubresourceLayers ImageSubresource = {};
        ImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageSubresource.baseArrayLayer=0;
        ImageSubresource.layerCount=1;
        ImageSubresource.mipLevel=0;

        VkBufferImageCopy BufferImageCopy = {};
        BufferImageCopy.imageExtent.depth=1;
        BufferImageCopy.imageExtent.width = Width;
        BufferImageCopy.imageExtent.height = Height;
        BufferImageCopy.imageOffset = {0,0,0};
        BufferImageCopy.imageSubresource = ImageSubresource;

        BufferImageCopy.bufferOffset=0;
        BufferImageCopy.bufferImageHeight = 0;
        BufferImageCopy.bufferRowLength=0;
        
        vkCmdCopyImageToBuffer(CopyCommand, Source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Dest->VulkanObjects.Buffer,1,  &BufferImageCopy);
        
        FlushCommandBuffer(VulkanDevice->Device, CommandPool, CopyCommand, Queue, true);        
    }

    sceneMesh BuildQuad(vulkanDevice *VulkanDevice)
    {
        sceneMesh Quad;

        std::vector<vertex> VertexBuffer;
        VertexBuffer.push_back({ { -1.0f, -1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 0.0f, 0.0f } });
        VertexBuffer.push_back({ {  1.0f,  -1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 0.0f, 0.0f } });
        VertexBuffer.push_back({ {  1.0f,  1.0f, 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f, 0.0f } });
        VertexBuffer.push_back({ { -1.0f,  1.0f, 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0.0f, 0.0f } });    

        vulkanTools::CreateBuffer(VulkanDevice, 
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                 &Quad.VulkanObjects.VertexBuffer,
                                 VertexBuffer.size() * sizeof(vertex),
                                 VertexBuffer.data());

        
        std::vector<uint32_t> IndexBuffer = {0,1,2,  2,3,0};
        Quad.IndexCount = (uint32_t)IndexBuffer.size();
   
        vulkanTools::CreateBuffer(VulkanDevice, 
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                 &Quad.VulkanObjects.IndexBuffer,
                                 IndexBuffer.size() * sizeof(uint32_t),
                                 IndexBuffer.data());          

        return Quad;
    }

    uint64_t GetBufferDeviceAddress(vulkanDevice *VulkanDevice, VkBuffer Buffer)
    {
        VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        BufferDeviceAddressInfo.buffer = Buffer;
        return VulkanDevice->_vkGetBufferDeviceAddressKHR(VulkanDevice->Device, &BufferDeviceAddressInfo);
    }

    uint32_t AlignedSize(uint32_t Value, uint32_t Alignment)
    {
        return (Value + Alignment-1) & ~(Alignment-1);
    }

}