#include "TextureLoader.h"
#include "Scene.h"
#include "GLTFImporter.h"

glm::vec4 vulkanTexture::Sample(glm::vec2 UV, borderType BorderType)
{
	// UV = glm::abs(UV);
	// UV = glm::clamp(UV, glm::vec2(0), glm::vec2(1));
    if(UV.x > 1)
    {
        float d;
        UV.x = modf(UV.x, &d);
    }
    if(UV.y > 1)
    {
        float d;
        UV.y = modf(UV.y, &d);
    }

    if(UV.x < 0)
    {
        UV.x = 1 + UV.x;
    }
    if(UV.y < 0)
    {
        UV.y = 1 + UV.y;
    }
    //if(BorderType == borderType::Clamp) 
    // if(BorderType==borderType::Repeat) {
    //     glm::vec2 i;
    //     UV = glm::modf(UV, i);
    // }
    
    glm::ivec2 TexCoords = glm::ivec2(UV * glm::vec2(Width, Height));
    uint32_t BaseInx = (uint32_t)(TexCoords.y * Width * 4) + (uint32_t)(TexCoords.x * 4);
    glm::vec4 TextureColor(
        (float)(Data[BaseInx + 0]) / 255.0f,
        (float)(Data[BaseInx + 1]) / 255.0f,
        (float)(Data[BaseInx + 2]) / 255.0f,
        (float)(Data[BaseInx + 3]) / 255.0f
    );       

    return TextureColor;
}

textureLoader::textureLoader(vulkanDevice *VulkanDevice, VkQueue Queue, VkCommandPool CommandPool) :
                  VulkanDevice(VulkanDevice), Queue(Queue), CommandPool(CommandPool)
{
    VkCommandBufferAllocateInfo CommandBufferInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    CommandBufferInfo.commandPool = CommandPool;
    CommandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandBufferInfo.commandBufferCount =1;

    VK_CALL(vkAllocateCommandBuffers(VulkanDevice->Device, &CommandBufferInfo, &CommandBuffer));
}

void textureLoader::LoadTexture2D(std::string Filename, VkFormat Format, vulkanTexture *Texture, VkImageUsageFlags ImageUsageFlags)
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

    Texture->Data.resize(Texture->Width * Texture->Height * 4);
    memcpy(Texture->Data.data(), GliTexture[0].data(), Texture->Data.size());

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
    Texture->ImageLayout = VK_IMAGE_LAYOUT_GENERAL;
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

void textureLoader::LoadCubemap(std::string PanoFileName, vulkanTexture *Output)
{
    if(Output==nullptr) return;

    float *Pixels;
    int Width, Height;
    Pixels = stbi_loadf(PanoFileName.c_str(), &Width, &Height, NULL, 4);
    vulkanTexture PanoTexture;
    CreateTexture(
        Pixels,
        Width * Height * 4 * sizeof(float),
        VK_FORMAT_R32G32B32A32_SFLOAT,
        Width, Height,
        &PanoTexture,
        false
    );
    
    bool DoGenerateMipmaps=true;

    Output->Width = 512;
    Output->Height = 512;
    Output->MipLevels = DoGenerateMipmaps ?  static_cast<uint32_t>(std::floor(std::log2(std::max(Output->Width, Output->Height)))) + 1 : 1;
    
    //Vertex Description
    struct 
    {
        VkPipelineVertexInputStateCreateInfo InputState;
        std::vector<VkVertexInputBindingDescription> BindingDescription;
        std::vector<VkVertexInputAttributeDescription> AttributeDescription;
    } VerticesDescription;
    VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Tangent))
    };
    VerticesDescription.InputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    VerticesDescription.InputState.vertexBindingDescriptionCount = (uint32_t)VerticesDescription.BindingDescription.size();
    VerticesDescription.InputState.pVertexBindingDescriptions = VerticesDescription.BindingDescription.data();
    VerticesDescription.InputState.vertexAttributeDescriptionCount = (uint32_t)VerticesDescription.AttributeDescription.size();
    VerticesDescription.InputState.pVertexAttributeDescriptions = VerticesDescription.AttributeDescription.data();        

    //Framebuffer
    std::vector<framebuffer> Framebuffers(6);
    for(int i=0; i<6; i++)
    {
        Framebuffers[i].SetSize(Output->Width, Output->Height)
                            .SetAttachmentCount(1)
                            .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                            .SetImageFlags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                            .BuildBuffers(VulkanDevice,CommandBuffer);
    }
    //Renderpass


    //Create descriptorPool
    VkDescriptorPool DescriptorPool;
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);
    VK_CALL(vkCreateDescriptorPool(VulkanDevice->Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));                

    //create Cube
    // sceneMesh Cube = vulkanTools::BuildCube(VulkanDevice);
    sceneMesh Cube;
    GLTFImporter::LoadMesh("resources/models/Cube/Cube.gltf", Cube, VulkanDevice, CommandBuffer, Queue);

    //load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
    ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildCubemap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildCubemap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    
    //Create uniform buffers
    glm::mat4 CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    std::vector<glm::mat4> ViewMatrices =
    {
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        CaptureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    
    
    buffer ViewMatricesBuffer;
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &ViewMatricesBuffer,
                                sizeof(glm::mat4)
    );


    //descriptor set layout
    VkDescriptorSetLayout DescriptorSetLayout;
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = {
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 )
    };
    VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings);
    VK_CALL(vkCreateDescriptorSetLayout(VulkanDevice->Device, &DescriptorLayoutCreateInfo, nullptr, &DescriptorSetLayout));

    //Allocate and write descriptor sets
    VkDescriptorSet DescriptorSet;
    VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
    VK_CALL(vkAllocateDescriptorSets(VulkanDevice->Device, &AllocInfo, &DescriptorSet));
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ViewMatricesBuffer.VulkanObjects.Descriptor),
        vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &PanoTexture.Descriptor)
    };
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);

    //Build pipeline layout
    std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
    {
        DescriptorSetLayout
    };
    VkPipelineLayout PipelineLayout;
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
    vkCreatePipelineLayout(VulkanDevice->Device, &pPipelineLayoutCreateInfo, nullptr, &PipelineLayout);
        

    //create graphics pipeline
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = vulkanTools::BuildPipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            0,
            VK_FALSE
        );

    VkPipelineRasterizationStateCreateInfo RasterizationState = vulkanTools::BuildPipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        0
    );

    VkPipelineColorBlendAttachmentState BlendAttachmentState = vulkanTools::BuildPipelineColorBlendAttachmentState(
        0xf,
        VK_FALSE
    );

    VkPipelineColorBlendStateCreateInfo ColorBlendState = vulkanTools::BuildPipelineColorBlendStateCreateInfo(
        1,
        &BlendAttachmentState
    );

    VkPipelineDepthStencilStateCreateInfo DepthStencilState = vulkanTools:: BuildPipelineDepthStencilStateCreateInfo(
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_LESS_OR_EQUAL
    );

    VkPipelineViewportStateCreateInfo ViewportState = vulkanTools::BuildPipelineViewportStateCreateInfo(
        1,1,0
    );

    VkPipelineMultisampleStateCreateInfo MultiSample = vulkanTools::BuildPipelineMultisampleStateCreateInfo(
        VK_SAMPLE_COUNT_1_BIT,
        0
    );

    std::vector<VkDynamicState> DynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo DynamicState = vulkanTools::BuildPipelineDynamicStateCreateInfo(
        DynamicStateEnables.data(),
        (uint32_t)DynamicStateEnables.size(),
        0
    );

    //Shader Stages
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = vulkanTools::BuildGraphicsPipelineCreateInfo();
    PipelineCreateInfo.pVertexInputState = &VerticesDescription.InputState;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyState;
    PipelineCreateInfo.pRasterizationState = &RasterizationState;
    PipelineCreateInfo.pColorBlendState = &ColorBlendState;
    PipelineCreateInfo.pMultisampleState = &MultiSample;
    PipelineCreateInfo.pViewportState = &ViewportState;
    PipelineCreateInfo.pDepthStencilState = &DepthStencilState;
    PipelineCreateInfo.pDynamicState = &DynamicState;
    PipelineCreateInfo.stageCount = (uint32_t)ShaderStages.size();
    PipelineCreateInfo.pStages = ShaderStages.data();
    PipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
    PipelineCreateInfo.layout = PipelineLayout;

    std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
    {
        vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
    };
    ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
    ColorBlendState.pAttachments=BlendAttachmentStates.data();
    
    std::vector<VkPipeline> Pipelines(6);
    for(int i=0; i<6; i++)
    {
        PipelineCreateInfo.renderPass = Framebuffers[i].RenderPass;
        vkCreateGraphicsPipelines(VulkanDevice->Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &Pipelines[i]);
    }
    
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    std::array<VkClearValue, 2> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderArea.extent.width = Output->Width;
    RenderPassBeginInfo.renderArea.extent.height = Output->Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();


    //Copy the 6 faces into a cubemap image
    VkImageCreateInfo imageCreateInfo = vulkanTools::BuildImageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageCreateInfo.mipLevels = Output->MipLevels;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { (uint32_t)Output->Width, (uint32_t)Output->Height, 1 };
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if(DoGenerateMipmaps) imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    // Cube faces count as array layers in Vulkan
    imageCreateInfo.arrayLayers = 6;
    // This flag is required for cube map images
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VK_CALL(vkCreateImage(VulkanDevice->Device, &imageCreateInfo, nullptr, &Output->Image));

    VkMemoryAllocateInfo memAllocInfo = vulkanTools::BuildMemoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(VulkanDevice->Device, Output->Image, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &memAllocInfo, nullptr, &Output->DeviceMemory));
    VK_CALL(vkBindImageMemory(VulkanDevice->Device, Output->Image, Output->DeviceMemory, 0));    

    for(int i=0; i<6; i++)
    {
        VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));
        
        RenderPassBeginInfo.framebuffer = Framebuffers[i].Framebuffer;
        RenderPassBeginInfo.renderPass = Framebuffers[i].RenderPass;
        
        //ViewMatricesBuffer.Flush();
        VK_CALL(ViewMatricesBuffer.Map());
        ViewMatricesBuffer.CopyTo(glm::value_ptr(ViewMatrices[i]), sizeof(glm::mat4));
        ViewMatricesBuffer.Unmap();    


        vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport Viewport = vulkanTools::BuildViewport((float)Output->Width, (float)Output->Height, 0.0f, 1.0f);
        vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
        VkRect2D Scissor = vulkanTools::BuildRect2D(Output->Width,Output->Height,0,0);
        vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);


        VkDeviceSize Offset[1] = {0};
        vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipelines[i]); 
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet, 0, nullptr);

        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Cube.VulkanObjects.VertexBuffer.VulkanObjects.Buffer, Offset);
        vkCmdBindIndexBuffer(CommandBuffer, Cube.VulkanObjects.IndexBuffer.VulkanObjects.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(CommandBuffer, Cube.IndexCount, 1, 0, 0, 0);
    
        vkCmdEndRenderPass(CommandBuffer);
        
    
		vulkanTools::TransitionImageLayout(
			CommandBuffer,
			Framebuffers[i]._Attachments[0].Image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageSubresourceRange CubeFaceSubresourceRange = {};
		CubeFaceSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		CubeFaceSubresourceRange.baseMipLevel = 0;
		CubeFaceSubresourceRange.levelCount = Output->MipLevels;
		CubeFaceSubresourceRange.baseArrayLayer = i;
		CubeFaceSubresourceRange.layerCount = 1;

		vulkanTools::TransitionImageLayout(
			CommandBuffer,
			Output->Image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			CubeFaceSubresourceRange);

		// Copy region for transfer from framebuffer to cube face
		VkImageCopy copyRegion = {};
		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };
		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.baseArrayLayer = i;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };
		copyRegion.extent.width = Output->Width;
		copyRegion.extent.height = Output->Height;
		copyRegion.extent.depth = 1;

		// Copy image
		vkCmdCopyImage(
			CommandBuffer,
			Framebuffers[i]._Attachments[0].Image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Output->Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copyRegion);

		// Change image layout of copied face to shader read
        //If mipmaps are generated, it will take care of transitioning
		if(!DoGenerateMipmaps)
        {
            vulkanTools::TransitionImageLayout(
			CommandBuffer,
			Output->Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			CubeFaceSubresourceRange);
        }

        VK_CALL(vkEndCommandBuffer(CommandBuffer));

        VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
        VK_CALL(vkQueueWaitIdle(Queue));    
    }   

    
    VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));
    if(DoGenerateMipmaps) GenerateCubemapMipmaps(Output->Image, Output->Width, Output->Height, Output->MipLevels);
    VK_CALL(vkEndCommandBuffer(CommandBuffer));
    VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers = &CommandBuffer;
    VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
    VK_CALL(vkQueueWaitIdle(Queue));    

    // Create sampler
    VkSamplerCreateInfo sampler = vulkanTools::BuildSamplerCreateInfo();
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = (float)Output->MipLevels; //Mip levels
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler.maxAnisotropy = 1.0f;
    
    VK_CALL(vkCreateSampler(VulkanDevice->Device, &sampler, nullptr, &Output->Sampler));

    // Create image view
    VkImageViewCreateInfo view = vulkanTools::BuildImageViewCreateInfo();
    // Cube map view type
    view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    // 6 array layers (faces)
    view.subresourceRange.layerCount = 6;
    // Set number of mip levels
    view.subresourceRange.levelCount = Output->MipLevels;//Miplevels
    view.image = Output->Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Output->View));



    
    for(int i=0; i<6; i++)
    {
        vkDestroyPipeline(VulkanDevice->Device, Pipelines[i], nullptr);
    }
    vkDestroyPipelineLayout(VulkanDevice->Device, PipelineLayout, nullptr);
    vkFreeDescriptorSets(VulkanDevice->Device, DescriptorPool, 1, &DescriptorSet);
    vkDestroyDescriptorSetLayout(VulkanDevice->Device, DescriptorSetLayout, nullptr);
    ViewMatricesBuffer.Destroy();
    vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[0].module, nullptr);
    vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[1].module, nullptr);
    Cube.VulkanObjects.IndexBuffer.Destroy();
    Cube.VulkanObjects.VertexBuffer.Destroy();
    vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
    
    for(int i=0; i<6; i++)
    {
        Framebuffers[i].Destroy(VulkanDevice->Device);
    }

    DestroyTexture(PanoTexture);

    
    Output->Descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Output->Descriptor.imageView = Output->View;
    Output->Descriptor.sampler = Output->Sampler;
}

void textureLoader::GenerateMipmaps(VkImage Image, uint32_t Width, uint32_t Height, uint32_t MipLevels)
{
    VkImageMemoryBarrier Barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.image = Image;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseArrayLayer=0;
    Barrier.subresourceRange.layerCount=1;
    Barrier.subresourceRange.levelCount=1;

    int32_t MipWidth = Width;
    int32_t MipHeight = Height;
    for(uint32_t i=1; i<MipLevels; i++)
    {
        Barrier.subresourceRange.baseMipLevel=i-1;
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(CommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr, 
        0, nullptr,
        1, &Barrier);

        VkImageBlit Blit {};
        Blit.srcOffsets[0] = {0,0,0};
        Blit.srcOffsets[1] = {MipWidth, MipHeight, 1};
        Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Blit.srcSubresource.mipLevel = i-1;
        Blit.srcSubresource.baseArrayLayer = 0;
        Blit.srcSubresource.layerCount=1;
        Blit.dstOffsets[0] = {0,0,0};
        Blit.dstOffsets[1] = {MipWidth > 1 ? MipWidth / 2 : 1, MipHeight > 1 ? MipHeight/2 : 1, 1};
        Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Blit.dstSubresource.mipLevel = i;
        Blit.dstSubresource.baseArrayLayer = 0;
        Blit.dstSubresource.layerCount=1;

        vkCmdBlitImage(CommandBuffer,
                        Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1, &Blit,
                        VK_FILTER_LINEAR);
        
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(CommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &Barrier);       


        if (MipWidth > 1) MipWidth /= 2;
        if (MipHeight > 1) MipHeight /= 2;                     
    }

    Barrier.subresourceRange.baseMipLevel = MipLevels - 1;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &Barrier);        

}


void textureLoader::GenerateCubemapMipmaps(VkImage Image, uint32_t Width, uint32_t Height, uint32_t MipLevels)
{
    VkImageMemoryBarrier Barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    Barrier.image = Image;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseArrayLayer=0;
    Barrier.subresourceRange.layerCount=6;
    Barrier.subresourceRange.levelCount=1;

    int32_t MipWidth = Width;
    int32_t MipHeight = Height;
    for(uint32_t i=1; i<MipLevels; i++)
    {
        // for(int j=0; j<6; j++)
        {
            Barrier.subresourceRange.baseMipLevel=i-1;
            Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(CommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 
            0, nullptr,
            1, &Barrier);

            VkImageBlit Blit {};
            Blit.srcOffsets[0] = {0,0,0};
            Blit.srcOffsets[1] = {MipWidth, MipHeight, 1};
            Blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Blit.srcSubresource.mipLevel = i-1;
            Blit.srcSubresource.baseArrayLayer = 0;
            Blit.srcSubresource.layerCount=6;
            Blit.dstOffsets[0] = {0,0,0};
            Blit.dstOffsets[1] = {MipWidth > 1 ? MipWidth / 2 : 1, MipHeight > 1 ? MipHeight/2 : 1, 1};
            Blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Blit.dstSubresource.mipLevel = i;
            Blit.dstSubresource.baseArrayLayer = 0;
            Blit.dstSubresource.layerCount=6;

            vkCmdBlitImage(CommandBuffer,
                            Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1, &Blit,
                            VK_FILTER_LINEAR);
            
            Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(CommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &Barrier);       
        }


        if (MipWidth > 1) MipWidth /= 2;
        if (MipHeight > 1) MipHeight /= 2;                     
    }

    Barrier.subresourceRange.baseMipLevel = MipLevels - 1;
    Barrier.subresourceRange.baseArrayLayer=0;
    Barrier.subresourceRange.layerCount=6;
    Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(CommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &Barrier);        

}

void textureLoader::CreateTexture(void *Buffer, VkDeviceSize BufferSize, VkFormat Format, uint32_t Width, uint32_t Height, vulkanTexture *Texture, bool DoGenerateMipmaps, VkFilter Filter, VkImageUsageFlags ImageUsageFlags)
{
    assert(Buffer);
    
    //Save a copy of the data on the cpu (CPU Ray tracer / Rasterizer)
    Texture->Data.resize(Width * Height * 4);
    memcpy(Texture->Data.data(), Buffer, Width * Height * 4);

    Texture->Width = Width;
    Texture->Height = Height;
    Texture->MipLevels = DoGenerateMipmaps ?  static_cast<uint32_t>(std::floor(std::log2(std::max(Width, Height)))) + 1 : 1;

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
    imageCreateInfo.usage = ImageUsageFlags | VK_IMAGE_USAGE_TRANSFER_SRC_BIT ;
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
    
        vkCmdCopyBufferToImage(
            CommandBuffer,
            stagingBuffer,
            Texture->Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );
    
        // Copy mip levels from staging buffer
        if(DoGenerateMipmaps)GenerateMipmaps(Texture->Image, Width, Height, Texture->MipLevels);
        else
        {
            // Change texture image layout to shader read after all mip levels have been copied
            Texture->ImageLayout = VK_IMAGE_LAYOUT_GENERAL;
                vulkanTools::TransitionImageLayout(
                CommandBuffer,
                Texture->Image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                Texture->ImageLayout,
                subresourceRange);
        }


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
    sampler.maxLod = static_cast<float>(Texture->MipLevels);
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
    view.subresourceRange.levelCount = Texture->MipLevels;
    view.image = Texture->Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Texture->View));

    // Fill descriptor image info that can be used for setting up descriptor sets
    Texture->Descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    Texture->Descriptor.imageView = Texture->View;
    Texture->Descriptor.sampler = Texture->Sampler;
}

void textureLoader::CreateEmptyTexture(uint32_t Width, uint32_t Height, VkFormat Format, vulkanTexture *Texture, VkImageUsageFlags ImageUsage)
{
    VkFormatProperties FormatProperties;
    vkGetPhysicalDeviceFormatProperties(VulkanDevice->PhysicalDevice, Format, &FormatProperties);
    assert(FormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

    // Prepare blit target texture
    Texture->Width = Width;
    Texture->Height = Height;

    VkImageCreateInfo ImageCreateInfo = vulkanTools::BuildImageCreateInfo();
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = Format;
    ImageCreateInfo.extent = { Width, Height, 1 };
    ImageCreateInfo.mipLevels = 1;
    ImageCreateInfo.arrayLayers = 1;
    ImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | ImageUsage;
    ImageCreateInfo.flags = 0;

    VkMemoryAllocateInfo MemoryAllocateInfo = vulkanTools::BuildMemoryAllocateInfo();
    VkMemoryRequirements MemoryRequirements;

    VK_CALL(vkCreateImage(VulkanDevice->Device, &ImageCreateInfo, nullptr, &Texture->Image));
    vkGetImageMemoryRequirements(VulkanDevice->Device, Texture->Image, &MemoryRequirements);
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex =  VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Texture->DeviceMemory));
    VK_CALL(vkBindImageMemory(VulkanDevice->Device, Texture->Image, Texture->DeviceMemory, 0));

    VkCommandBuffer LayoutCmd = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Texture->ImageLayout = VK_IMAGE_LAYOUT_GENERAL;
    vulkanTools::TransitionImageLayout(
    	LayoutCmd,
    	Texture->Image,
    	VK_IMAGE_ASPECT_COLOR_BIT,
    	VK_IMAGE_LAYOUT_UNDEFINED,
    	Texture->ImageLayout);

    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, LayoutCmd, Queue, true);

    // Create sampler
    VkSamplerCreateInfo Sampler = vulkanTools::BuildSamplerCreateInfo();
    Sampler.magFilter = VK_FILTER_LINEAR;
    Sampler.minFilter = VK_FILTER_LINEAR;
    Sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    Sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    Sampler.addressModeV = Sampler.addressModeU;
    Sampler.addressModeW = Sampler.addressModeU;
    Sampler.mipLodBias = 0.0f;
    Sampler.maxAnisotropy = 1.0f;
    Sampler.compareOp = VK_COMPARE_OP_NEVER;
    Sampler.minLod = 0.0f;
    Sampler.maxLod = 0.0f;
    Sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CALL(vkCreateSampler(VulkanDevice->Device, &Sampler, nullptr, &Texture->Sampler));

    // Create image view
    VkImageViewCreateInfo view = vulkanTools::BuildImageViewCreateInfo();
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = Format;
    view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    view.image = Texture->Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Texture->View));

    // Initialize a descriptor for later use
    Texture->Descriptor.imageLayout = Texture->ImageLayout;
    Texture->Descriptor.imageView = Texture->View;
    Texture->Descriptor.sampler = Texture->Sampler;    
}


void textureLoader::DestroyTexture(vulkanTexture Texture)
{
    vkDestroyImageView(VulkanDevice->Device, Texture.View, nullptr);
    vkDestroyImage(VulkanDevice->Device, Texture.Image, nullptr);
    vkDestroySampler(VulkanDevice->Device, Texture.Sampler, nullptr);
    vkFreeMemory(VulkanDevice->Device, Texture.DeviceMemory, nullptr);
}

void textureLoader::Destroy()
{
    vkFreeCommandBuffers(VulkanDevice->Device, CommandPool, 1, &CommandBuffer);
}

void vulkanTexture::Destroy(vulkanDevice *VulkanDevice)
{
    vkDestroyImageView(VulkanDevice->Device, View, nullptr);
    vkDestroyImage(VulkanDevice->Device, Image, nullptr);
    vkDestroySampler(VulkanDevice->Device, Sampler, nullptr);
    vkFreeMemory(VulkanDevice->Device, DeviceMemory, nullptr);
}