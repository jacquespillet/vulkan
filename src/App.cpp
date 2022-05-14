#include "App.h"

void vulkanApp::InitVulkan()
{
    Instance = vulkanTools::CreateInstance(EnableValidation);

    //Set debug report callback        
    if(EnableValidation)
    {
        VkDebugReportFlagsEXT DebugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
        vulkanDebug::SetupDebugReportCallback(Instance, DebugReportFlags);
    }

    //Pick physical device
    uint32_t GpuCount=0;
    VK_CALL(vkEnumeratePhysicalDevices(Instance, &GpuCount, nullptr));
    assert(GpuCount >0);
    std::vector<VkPhysicalDevice> PhysicalDevices(GpuCount);
    VkResult Error = vkEnumeratePhysicalDevices(Instance, &GpuCount, PhysicalDevices.data());
    if(Error)
    {
        assert(0);
    }
    PhysicalDevice = PhysicalDevices[0];

    //Build logical device
    VulkanDevice = new vulkanDevice(PhysicalDevice);
    VK_CALL(VulkanDevice->CreateDevice(EnabledFeatures));
    Device = VulkanDevice->Device;

    //Get queue
    vkGetDeviceQueue(Device, VulkanDevice->QueueFamilyIndices.Graphics, 0, &Queue);
    
    //Get depth format
    VkBool32 ValidDepthFormat = vulkanTools::GetSupportedDepthFormat(PhysicalDevice, &DepthFormat);
    assert(ValidDepthFormat);

    //Build main semaphores
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.PresentComplete));
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.RenderComplete));

    //Before color output stage, wait for present semaphore to be complete, and signal Render semaphore to be completed
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &Semaphores.PresentComplete;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pSignalSemaphores = &Semaphores.RenderComplete;    
    
    Swapchain.Initialize(Instance, PhysicalDevice, Device);
}

void vulkanApp::SetupWindow()
{
    
}

void vulkanApp::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));
}

void vulkanApp::SetupDepthStencil()
{
    //Image 
    VkImageCreateInfo Image {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    Image.pNext=nullptr;
    Image.imageType = VK_IMAGE_TYPE_2D;
    Image.format = DepthFormat;
    Image.extent = {Width, Height, 1};
    Image.mipLevels=1;
    Image.arrayLayers=1;
    Image.samples = VK_SAMPLE_COUNT_1_BIT;
    Image.tiling=VK_IMAGE_TILING_OPTIMAL;
    Image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    Image.flags=0;

    //Mem allocation
    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext=nullptr;
    MemoryAllocateInfo.allocationSize=0;
    MemoryAllocateInfo.memoryTypeIndex=0;

    //Image view
    VkImageViewCreateInfo DepthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    DepthStencilView.pNext=nullptr;
    DepthStencilView.viewType=VK_IMAGE_VIEW_TYPE_2D;
    DepthStencilView.format=DepthFormat;
    DepthStencilView.flags=0;
    DepthStencilView.subresourceRange={};
    DepthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    DepthStencilView.subresourceRange.baseMipLevel=0;
    DepthStencilView.subresourceRange.levelCount=1;
    DepthStencilView.subresourceRange.baseArrayLayer=0;
    DepthStencilView.subresourceRange.layerCount=1;

    //Create image
    VK_CALL(vkCreateImage(Device, &Image, nullptr, &DepthStencil.Image));
    
    //Get mem requirements for this image
    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(Device, DepthStencil.Image, &MemoryRequirements);
    
    //Allocate memory
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemoryAllocateInfo, nullptr, &DepthStencil.Memory));
    VK_CALL(vkBindImageMemory(Device, DepthStencil.Image, DepthStencil.Memory, 0));

    //Create view
    DepthStencilView.image = DepthStencil.Image;
    VK_CALL(vkCreateImageView(Device, &DepthStencilView, nullptr, &DepthStencil.View));
}


void vulkanApp::SetupRenderPass()
{
    std::array<VkAttachmentDescription, 2> Attachments = {};

    Attachments[0].format = Swapchain.ColorFormat;
    Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Attachments[1].format = DepthFormat;
    Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference ColorReference  = {};
    ColorReference.attachment=0;
    ColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthReference  = {};
    DepthReference.attachment=1;
    DepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription SubpassDescription = {};
    SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDescription.colorAttachmentCount=1;
    SubpassDescription.pColorAttachments = &ColorReference;
    SubpassDescription.pDepthStencilAttachment = &DepthReference;
    SubpassDescription.inputAttachmentCount=0;
    SubpassDescription.pInputAttachments=nullptr;
    SubpassDescription.preserveAttachmentCount=0;
    SubpassDescription.pPreserveAttachments=nullptr;
    SubpassDescription.pResolveAttachments=nullptr;

    std::array<VkSubpassDependency, 2> Dependencies;

    Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependencies[0].dstSubpass=0;
    Dependencies[0].srcStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[0].srcAccessMask=VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[0].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

    Dependencies[1].srcSubpass = 0;
    Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    Dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[1].dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[1].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo RenderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassCreateInfo.attachmentCount=static_cast<uint32_t>(Attachments.size());
    RenderPassCreateInfo.pAttachments=Attachments.data();
    RenderPassCreateInfo.subpassCount=1;
    RenderPassCreateInfo.pSubpasses=&SubpassDescription;
    RenderPassCreateInfo.dependencyCount=static_cast<uint32_t>(Dependencies.size());
    RenderPassCreateInfo.pDependencies = Dependencies.data();

    VK_CALL(vkCreateRenderPass(Device, &RenderPassCreateInfo, nullptr, &RenderPass));
}

void vulkanApp::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo PipelineCacheCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CALL(vkCreatePipelineCache(Device, &PipelineCacheCreateInfo, nullptr, &PipelineCache));
}

void vulkanApp::SetupFramebuffer()
{
    VkImageView Attachments[2];

    Attachments[1] = DepthStencil.View;

    VkFramebufferCreateInfo FramebufferCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferCreateInfo.pNext=nullptr;
    FramebufferCreateInfo.renderPass = RenderPass;
    FramebufferCreateInfo.attachmentCount=2;
    FramebufferCreateInfo.pAttachments=Attachments;
    FramebufferCreateInfo.width=Width;
    FramebufferCreateInfo.height=Height;
    FramebufferCreateInfo.layers=1;

    AppFramebuffers.resize(Swapchain.ImageCount);
    for(uint32_t i=0; i<AppFramebuffers.size(); i++)
    {
        Attachments[0] = Swapchain.Buffers[i].View;
        VK_CALL(vkCreateFramebuffer(Device, &FramebufferCreateInfo, nullptr, &AppFramebuffers[i]));
    }
}


void vulkanApp::CreateGeneralResources()
{
    CommandPool = vulkanTools::CreateCommandPool(Device, Swapchain.QueueNodeIndex);
    Swapchain.Create(&Width, &Height, EnableVSync);
    
    //Create 1 command buffer for each swapchain image
    CreateCommandBuffers();

    SetupDepthStencil();
    
    SetupRenderPass();
    CreatePipelineCache();
    SetupFramebuffer();

    TextureLoader = new textureLoader(VulkanDevice, Queue, CommandPool);
}

 void vulkanApp::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));
}

void vulkanApp::BuildQuads()
{
    struct vertex
    {
        float pos[3];
        float uv[2];
        float col[3];
        float normal[3];
        float tangent[3];
        float bitangent[3];
    };
    std::vector<vertex> VertexBuffer;
    VertexBuffer.push_back({ { 1.0f, 1.0f, 0.0f },{ 1.0f, 1.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0 } });
    VertexBuffer.push_back({ { 0,      1.0f, 0.0f },{ 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0 } });
    VertexBuffer.push_back({ { 0,      0,      0.0f },{ 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0 } });
    VertexBuffer.push_back({ { 1.0f, 0,      0.0f },{ 1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 0 } });    
    
    vulkanTools::CreateBuffer(VulkanDevice,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    VertexBuffer.size() * sizeof(vertex),
                    VertexBuffer.data(),
                    &Meshes.Quad.Vertices.Buffer,
                    &Meshes.Quad.Vertices.DeviceMemory); 

    
    std::vector<uint32_t> IndicesBuffer = {0,1,2,  2,3,0};
    Meshes.Quad.IndexCount = (uint32_t)IndicesBuffer.size();

    vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        IndicesBuffer.size() * sizeof(uint32_t),
        IndicesBuffer.data(),
        &Meshes.Quad.Indices.Buffer,
        &Meshes.Quad.Indices.DeviceMemory
    );
}


void vulkanApp::BuildOffscreenBuffers()
{
    VkCommandBuffer LayoutCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

                                
    //G buffer
    {
        Framebuffers.Offscreen.SetSize(Width, Height)
                              .SetAttachmentCount(3)
                              .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                              .SetAttachmentFormat(1, VK_FORMAT_R8G8B8A8_UNORM)
                              .SetAttachmentFormat(2, VK_FORMAT_R32G32B32A32_UINT);
        Framebuffers.Offscreen.BuildBuffers(VulkanDevice,LayoutCommand);        
    }    
    //SSAO
    {
        uint32_t SSAOWidth = Width;
        uint32_t SSAOHeight = Height;
        Framebuffers.SSAO.SetSize(SSAOWidth, SSAOHeight)
                         .SetAttachmentCount(1)
                         .SetAttachmentFormat(0, VK_FORMAT_R8_UNORM)
                         .HasDepth=false;
        Framebuffers.SSAO.BuildBuffers(VulkanDevice,LayoutCommand);        
    }


    //SSAO Blur
    {
        Framebuffers.SSAOBlur.SetSize(Width, Height)
                             .SetAttachmentCount(1)
                             .SetAttachmentFormat(0, VK_FORMAT_R8_UNORM)
                             .HasDepth=false;
        Framebuffers.SSAOBlur.BuildBuffers(VulkanDevice,LayoutCommand);  
    }

    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, LayoutCommand, Queue, true);
    
}

 void vulkanApp::UpdateUniformBufferScreen()
{
    UBOVS.Projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    UBOVS.Model = glm::mat4(1.0f);

    VK_CALL(UniformBuffers.FullScreen.Map());
    UniformBuffers.FullScreen.CopyTo(&UBOVS, sizeof(UBOVS));
    UniformBuffers.FullScreen.Unmap();
}

void vulkanApp::UpdateUniformBufferDeferredMatrices()
{
    UBOSceneMatrices.Projection = Camera.GetProjectionMatrix();
    UBOSceneMatrices.View = Camera.GetViewMatrix();
    UBOSceneMatrices.Model = glm::mat4(1);
    
    UBOSceneMatrices.ViewportDim = glm::vec2(t,(float)Height);
    VK_CALL(UniformBuffers.SceneMatrices.Map());
    UniformBuffers.SceneMatrices.CopyTo(&UBOSceneMatrices, sizeof(UBOSceneMatrices));
    UniformBuffers.SceneMatrices.Unmap();
}

void vulkanApp::UpdateUniformBufferSSAOParams()
{
    UBOSceneMatrices.Projection =  glm::perspective(glm::radians(50.0f), 1.0f, 0.01f, 100.0f);
    
    VK_CALL(UniformBuffers.SSAOParams.Map());
    UniformBuffers.SSAOParams.CopyTo(&UBOSSAOParams, sizeof(UBOSSAOParams));
    UniformBuffers.SSAOParams.Unmap();
}

inline float vulkanApp::Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

void vulkanApp::BuildUniformBuffers()
{
    //Full screen quad shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.FullScreen,
                                sizeof(UBOVS)
    );
        
    //Deferred vertex shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SceneMatrices,
                                sizeof(UBOSceneMatrices)
    );

    //Deferred Frag Shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SceneLights,
                                sizeof(UBOFragmentLights)
    );

    UpdateUniformBufferScreen();
    UpdateUniformBufferDeferredMatrices();

    //Deferred Frag Shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SSAOParams,
                                sizeof(UBOSSAOParams)
    );
    UpdateUniformBufferSSAOParams();

    std::uniform_real_distribution<float> RandomDistribution(0.0f, 1.0f);
    std::random_device RandomDevice;
    std::default_random_engine RandomEngine;
    
    std::vector<glm::vec4> SSAOKernel(SSAO_KERNEL_SIZE);
    for(uint32_t i=0; i<SSAO_KERNEL_SIZE; i++)
    {
        glm::vec3 Sample(RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine));
        Sample = glm::normalize(Sample);
        Sample *= RandomDistribution(RandomEngine);
        float Scale = float(i) / float(SSAO_KERNEL_SIZE);
        Scale = Lerp(0.1f, 1.0f, Scale * Scale);
        SSAOKernel[i] = glm::vec4(Sample * Sample, 0.0f);
    }
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SSAOKernel,
                                SSAOKernel.size() * sizeof(glm::vec4),
                                SSAOKernel.data()
    );

    std::vector<glm::vec4> SSAONoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
    for(size_t i=0; i<SSAONoise.size(); i++)
    {
        SSAONoise[i] = glm::vec4(RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
    }

    TextureLoader->CreateTexture(SSAONoise.data(), SSAONoise.size() * sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT, SSAO_NOISE_DIM, SSAO_NOISE_DIM, &Textures.SSAONoise, VK_FILTER_NEAREST);        
}

void BuildDescriptorSet(vulkanDevice *VulkanDevice, std::string Name, std::vector<descriptor> &Descriptors, VkDescriptorPool DescriptorPool, resources &Resources)
{   
    //TODO: Store together descriptor set layout and pipeline layout

    //Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings(Descriptors.size());
    for(uint32_t i=0; i<Descriptors.size(); i++)
    {
        SetLayoutBindings[i] = vulkanTools::DescriptorSetLayoutBinding(Descriptors[i].DescriptorType, Descriptors[i].Stage, i);
    }  
    VkDescriptorSetLayoutCreateInfo SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    Resources.DescriptorSetLayouts->Add(Name, SetLayoutCreateInfo);
    
    //Create pipeline layout associated with it
    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo();
    PipelineLayoutCreateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr(Name);
    Resources.PipelineLayouts->Add(Name, PipelineLayoutCreateInfo);
    
    
    //Allocate descriptor set
    VkDescriptorSetAllocateInfo DescriptorAllocateInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, nullptr, 1);
    DescriptorAllocateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr(Name);
    VkDescriptorSet TargetDescriptorSet = Resources.DescriptorSets->Add(Name, DescriptorAllocateInfo);

    //Write descriptor set
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets(Descriptors.size()); 
    for(uint32_t i=0; i<Descriptors.size(); i++)
    {
        descriptor::type Type = Descriptors[i].Type;
        if(Type == descriptor::Image) { WriteDescriptorSets[i] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[i].DescriptorType, i, &Descriptors[i].DescriptorImageInfo); }
        else 
        if(Type == descriptor::Uniform) { WriteDescriptorSets[i] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[i].DescriptorType, i, &Descriptors[i].DescriptorBufferInfo);}
    }
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
}

void vulkanApp::BuildLayoutsAndDescriptors()
{
    //Render scene (Gbuffer)
    {
        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo();
        PipelineLayoutCreateInfo.pSetLayouts=&Scene->DescriptorSetLayout;
        Resources.PipelineLayouts->Add("Offscreen", PipelineLayoutCreateInfo);
    }    

    //Composition
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_VERTEX_BIT, UniformBuffers.FullScreen.Descriptor),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[2].ImageView, Framebuffers.Offscreen.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAOBlur._Attachments[0].ImageView, Framebuffers.SSAOBlur.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SceneLights.Descriptor)
        };
        
        BuildDescriptorSet(VulkanDevice, "Composition", Descriptors, DescriptorPool, Resources);
    }

    //SSAO Pass
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Textures.SSAONoise.View, Textures.SSAONoise.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOKernel.Descriptor),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOParams.Descriptor),
        };
        BuildDescriptorSet(VulkanDevice, "SSAO.Generate", Descriptors, DescriptorPool, Resources);
    }

    //SSAO Blur
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAO._Attachments[0].ImageView, Framebuffers.SSAO.Sampler)
        };
        BuildDescriptorSet(VulkanDevice, "SSAO.Blur", Descriptors, DescriptorPool, Resources);            
    }
}


void vulkanApp::BuildPipelines()
{
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = vulkanTools::BuildPipelineInputAssemblyStateCreateInfo(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0,
        VK_FALSE
    );

    VkPipelineRasterizationStateCreateInfo RasterizationState = vulkanTools::BuildPipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_CLOCKWISE,
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
    std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
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

    //Final composition pipeline
    {
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Composition");
        PipelineCreateInfo.renderPass = RenderPass;

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/Composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/Composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);

        struct specializationData
        {
            int32_t EnableSSAO=1;
            float AmbientFactor=0.15f;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries;
        SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, EnableSSAO), sizeof(int32_t)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, AmbientFactor), sizeof(float)),
        };
        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo((uint32_t)SpecializationMapEntries.size(), SpecializationMapEntries.data(), sizeof(SpecializationData), &SpecializationData );
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;

        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, PipelineCache);

        SpecializationData.EnableSSAO = 0;
        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, PipelineCache);
    }

    PipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    PipelineCreateInfo.basePipelineIndex=-1;
    PipelineCreateInfo.basePipelineHandle = Resources.Pipelines->Get("Composition.SSAO.Enabled");

    //Debug
    ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/Debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/Debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    ShaderModules.push_back(ShaderStages[0].module);
    ShaderModules.push_back(ShaderStages[1].module);
    Resources.Pipelines->Add("DebugDisplay", PipelineCreateInfo, PipelineCache);

    // Fill G buffer
    {
        struct specializationData
        {
            float ZNear;
            float ZFar;
            int32_t Discard=0;
        } SpecializationData;

        SpecializationData.ZNear = 0.01f;
        SpecializationData.ZFar = 100;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, ZNear), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, ZFar), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, Discard), sizeof(int32_t))
        };
        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo(
            (uint32_t)SpecializationMapEntries.size(),
            SpecializationMapEntries.data(),
            sizeof(SpecializationData),
            &SpecializationData
        );
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);            

        PipelineCreateInfo.renderPass = Framebuffers.Offscreen.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Offscreen");

        std::array<VkPipelineColorBlendAttachmentState, 3> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();
        Resources.Pipelines->Add("Scene.Solid", PipelineCreateInfo, PipelineCache);
        
        //Transparents
        DepthStencilState.depthWriteEnable=VK_FALSE;
        RasterizationState.cullMode=VK_CULL_MODE_NONE;
        SpecializationData.Discard=1;
        Resources.Pipelines->Add("Scene.Blend", PipelineCreateInfo, PipelineCache);
    }

    //SSAO
    {
        ColorBlendState.attachmentCount=1;

        VkPipelineVertexInputStateCreateInfo EmptyInputState {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        EmptyInputState.vertexAttributeDescriptionCount=0;
        EmptyInputState.pVertexAttributeDescriptions=nullptr;
        EmptyInputState.vertexBindingDescriptionCount=0;
        EmptyInputState.pVertexBindingDescriptions=nullptr;
        PipelineCreateInfo.pVertexInputState=&EmptyInputState;

        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);

        struct specializationData
        {
            uint32_t KernelSize = SSAO_KERNEL_SIZE;
            float Radius = SSAO_RADIUS;
            float Power = 1.5f;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries;
        SpecializationMapEntries = {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, KernelSize), sizeof(uint32_t)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, Radius), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, Power), sizeof(float)),
        };

        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo(
            (uint32_t)SpecializationMapEntries.size(),
            SpecializationMapEntries.data(),
            sizeof(SpecializationData),
            &SpecializationData
        );
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        PipelineCreateInfo.renderPass = Framebuffers.SSAO.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("SSAO.Generate");
        Resources.Pipelines->Add("SSAO.Generate", PipelineCreateInfo, PipelineCache);
    }

    //SSAO blur
    {
        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);            
        PipelineCreateInfo.renderPass = Framebuffers.SSAOBlur.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("SSAO.Blur");
        Resources.Pipelines->Add("SSAO.Blur" ,PipelineCreateInfo, PipelineCache);
    }        
}

void vulkanApp::BuildScene()
{
    VkCommandBuffer CopyCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    Scene = new scene(this);
    Scene->Load("resources/models/sponza/sponza.dae", CopyCommand);
    vkFreeCommandBuffers(VulkanDevice->Device, CommandPool, 1, &CopyCommand);
}

void vulkanApp::BuildVertexDescriptions()
{
    VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32_SFLOAT,    offsetof(vertex, UV)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Color)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Tangent)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Bitangent)),
    };

    VerticesDescription.InputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    VerticesDescription.InputState.vertexBindingDescriptionCount = (uint32_t)VerticesDescription.BindingDescription.size();
    VerticesDescription.InputState.pVertexBindingDescriptions = VerticesDescription.BindingDescription.data();
    VerticesDescription.InputState.vertexAttributeDescriptionCount = (uint32_t)VerticesDescription.AttributeDescription.size();
    VerticesDescription.InputState.pVertexAttributeDescriptions = VerticesDescription.AttributeDescription.data();
}

void vulkanApp::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = Width;
    RenderPassBeginInfo.renderArea.extent.height = Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;


    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));
        
        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport Viewport = vulkanTools::BuildViewport((float)Width, (float)Height, 0.0f, 1.0f);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);

        VkRect2D Scissor = vulkanTools::BuildRect2D(Width, Height, 0, 0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 0, 1, Resources.DescriptorSets->GetPtr("Composition"), 0, nullptr);

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Composition.SSAO.Enabled"));
        vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Meshes.Quad.Vertices.Buffer, Offsets);
        vkCmdBindIndexBuffer(DrawCommandBuffers[i], Meshes.Quad.Indices.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(DrawCommandBuffers[i], 6, 1, 0, 0, 1);

        vkCmdEndRenderPass(DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}

void vulkanApp::BuildDeferredCommandBuffers()
{
    if((OffscreenCommandBuffer == VK_NULL_HANDLE) || Rebuild)
    {
        if(Rebuild)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &OffscreenCommandBuffer);
        }
        OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    }

    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &OffscreenSemaphore));

    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferBeginInfo));



    
    //First pass
    std::array<VkClearValue, 4> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[2].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[3].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = Framebuffers.Offscreen.RenderPass;
    RenderPassBeginInfo.framebuffer = Framebuffers.Offscreen.Framebuffer;
    RenderPassBeginInfo.renderArea.extent.width = Framebuffers.Offscreen.Width;
    RenderPassBeginInfo.renderArea.extent.height = Framebuffers.Offscreen.Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();
    
    vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f);
    vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffers.Offscreen.Width,Framebuffers.Offscreen.Height,0,0);
    vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    VkDeviceSize Offset[1] = {0};

    vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Solid"));
    vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Scene->VertexBuffer.Buffer, Offset);
    vkCmdBindIndexBuffer(OffscreenCommandBuffer, Scene->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

    for(auto Mesh : Scene->Meshes)
    {
        if(Mesh.Material->HasAlpha)
        {
            continue;
        }
        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene->PipelineLayout, 0, 1, &Mesh.DescriptorSet, 0, nullptr);
        vkCmdDrawIndexed(OffscreenCommandBuffer, Mesh.IndexCount, 1, 0, Mesh.IndexBase, 0);
    }

    for(auto Mesh : Scene->Meshes)
    {
        if(Mesh.Material->HasAlpha)
        {
            vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene->PipelineLayout, 0, 1, &Mesh.DescriptorSet, 0, nullptr);
            vkCmdDrawIndexed(OffscreenCommandBuffer, Mesh.IndexCount, 1, 0, Mesh.IndexBase, 0);
        }
    }

    vkCmdEndRenderPass(OffscreenCommandBuffer);

    if(EnableSSAO)
    {
        ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
        ClearValues[1].depthStencil = {1.0f, 0};

        RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
        RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
        RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
        RenderPassBeginInfo.clearValueCount=2;
        RenderPassBeginInfo.pClearValues = ClearValues.data();

        vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.SSAO.Width, (float)Framebuffers.SSAO.Height, 0.0f, 1.0f);
        vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAO.Width,Framebuffers.SSAO.Height,0,0);
        vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("SSAO.Generate"), 0, 1, Resources.DescriptorSets->GetPtr("SSAO.Generate"), 0, nullptr);
        vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("SSAO.Generate"));
        vkCmdDraw(OffscreenCommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(OffscreenCommandBuffer);
        
        //ssao blur
        VkImageSubresourceRange SubresourceRange = {};
        SubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        SubresourceRange.baseMipLevel = 0;
        SubresourceRange.levelCount = 1;
        SubresourceRange.layerCount = 1;
        vulkanTools::TransitionImageLayout(
            OffscreenCommandBuffer,
            Framebuffers.SSAOBlur._Attachments[0].Image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            SubresourceRange
        );
                
        RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
        RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
        RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
        vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.SSAOBlur.Width, (float)Framebuffers.SSAOBlur.Height, 0.0f, 1.0f);
        vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAOBlur.Width,Framebuffers.SSAOBlur.Height,0,0);
        vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);
        
        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("SSAO.Blur"), 0, 1, Resources.DescriptorSets->GetPtr("SSAO.Blur"), 0, nullptr);
        vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("SSAO.Blur"));
        vkCmdDraw(OffscreenCommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(OffscreenCommandBuffer);
    }
    VK_CALL(vkEndCommandBuffer(OffscreenCommandBuffer));
}

void vulkanApp::CreateDeferredRendererResources()
{

    Resources.PipelineLayouts = new pipelineLayoutList(VulkanDevice->Device);
    Resources.Pipelines = new pipelineList(VulkanDevice->Device);
    Resources.DescriptorSetLayouts = new descriptorSetLayoutList(VulkanDevice->Device);
    Resources.DescriptorSets = new descriptorSetList(VulkanDevice->Device, DescriptorPool);
    Resources.Textures = new textureList(VulkanDevice->Device, TextureLoader);

    BuildQuads();
    BuildVertexDescriptions();
    
    BuildOffscreenBuffers();
    
    BuildUniformBuffers();

    SetupDescriptorPool();
    BuildScene();
    BuildLayoutsAndDescriptors();
    
    BuildPipelines();

    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
}

void vulkanApp::Initialize(HWND Window)
{
    //Common for any app
    Width = 1920;
    Height = 1080;
    InitVulkan();
    SetupWindow();
    Swapchain.InitSurface(Window);
    CreateGeneralResources();
    
    //App specific :
    CreateDeferredRendererResources();
}

void vulkanApp::UpdateCamera()
{

    UpdateUniformBufferDeferredMatrices();
    UpdateUniformBufferSSAOParams();
}

void vulkanApp::Render()
{
    t += 0.1f;
    UpdateCamera();

    VK_CALL(Swapchain.AcquireNextImage(Semaphores.PresentComplete, &CurrentBuffer));
    
    SubmitInfo.pWaitSemaphores = &Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &OffscreenSemaphore;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

     SubmitInfo.pWaitSemaphores = &OffscreenSemaphore;
     SubmitInfo.pSignalSemaphores = &Semaphores.RenderComplete;
     SubmitInfo.pCommandBuffers = &DrawCommandBuffers[CurrentBuffer];
     VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(Swapchain.QueuePresent(Queue, CurrentBuffer, Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(Queue));
}

void vulkanApp::MouseMove(float XPosition, float YPosition)
{
    Camera.mouseMoveEvent(XPosition, YPosition);
}

void vulkanApp::MouseAction(int Button, int Action, int Mods)
{
    if(Action == GLFW_PRESS)
    {
        Camera.mousePressEvent(Button);
    }
    else if(Action == GLFW_RELEASE)
    {
        Camera.mouseReleaseEvent(Button);
    }
}

void vulkanApp::Scroll(float YOffset)
{
    Camera.Scroll(YOffset);
}


void vulkanApp::Destroy()
{

}