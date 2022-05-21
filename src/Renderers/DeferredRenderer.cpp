#include "DeferredRenderer.h"
#include "App.h"

renderer::renderer(vulkanApp *App) : App(App), Device(App->Device), VulkanDevice(App->VulkanDevice)
{

}

deferredRenderer::deferredRenderer(vulkanApp *App) : renderer(App) {}

void deferredRenderer::Render()
{
    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
    UpdateCamera();

    VK_CALL(App->Swapchain.AcquireNextImage(App->Semaphores.PresentComplete, &App->CurrentBuffer));
    
    //Before color output stage, wait for present semaphore to be complete, and signal Render semaphore to be completed
    
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pWaitSemaphores = &App->Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &OffscreenSemaphore;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    SubmitInfo.pWaitSemaphores = &OffscreenSemaphore;
    SubmitInfo.pSignalSemaphores = &App->Semaphores.RenderComplete;
    SubmitInfo.pCommandBuffers = &DrawCommandBuffers[App->CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->Swapchain.QueuePresent(App->Queue, App->CurrentBuffer, App->Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->Queue));
}

void deferredRenderer::Setup()
{
    CreateCommandBuffers();
    SetupDescriptorPool();
    Resources.Init(VulkanDevice, DescriptorPool, App->TextureLoader);
    BuildUniformBuffers();
    BuildQuads();
    BuildOffscreenBuffers();
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
}

//


void deferredRenderer::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(App->Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));

    OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
}

void deferredRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));    
}


inline float Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}



void deferredRenderer::BuildUniformBuffers()
{
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

    App->TextureLoader->CreateTexture(SSAONoise.data(), SSAONoise.size() * sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT, SSAO_NOISE_DIM, SSAO_NOISE_DIM, &Textures.SSAONoise, VK_FILTER_NEAREST);        
}



void deferredRenderer::UpdateUniformBufferSSAOParams()
{
    VK_CALL(UniformBuffers.SSAOParams.Map());
    UniformBuffers.SSAOParams.CopyTo(&UBOSSAOParams, sizeof(UBOSSAOParams));
    UniformBuffers.SSAOParams.Unmap();
}


void deferredRenderer::BuildQuads()
{
    std::vector<vertex> VertexBuffer;
    VertexBuffer.push_back({ { 1.0f, 1.0f, 0.0f, 1.0f},{ 1.0f, 1.0f, 1.0f , 1.0f},{ 0.0f, 0.0f, 0.0f, 0.0f } });
    VertexBuffer.push_back({ { 0,      1.0f, 0.0f, 0.0f},{ 1.0f, 1.0f, 1.0f , 1.0f},{ 0.0f, 0.0f, 0.0f, 0.0f } });
    VertexBuffer.push_back({ { 0,      0,      0.0f, 0.0f},{ 1.0f, 1.0f, 1.0f , 0.0f},{ 0.0f, 0.0f, 0.0f, 0.0f } });
    VertexBuffer.push_back({ { 1.0f, 0,      0.0f, 1.0f},{ 1.0f, 1.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 0.0f, 0.0f } });    
    
    vulkanTools::CreateBuffer(VulkanDevice,
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             &Meshes.Quad.VertexBuffer,
                             VertexBuffer.size() * sizeof(vertex),
                             VertexBuffer.data());

    
    std::vector<uint32_t> IndexBuffer = {0,1,2,  2,3,0};
    Meshes.Quad.IndexCount = (uint32_t)IndexBuffer.size();

    vulkanTools::CreateBuffer(VulkanDevice,
                             VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                             &Meshes.Quad.IndexBuffer,
                             IndexBuffer.size() * sizeof(uint32_t),
                             IndexBuffer.data());
}



void deferredRenderer::BuildOffscreenBuffers()
{
    VkCommandBuffer LayoutCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);                   
    //G buffer
    {
        Framebuffers.Offscreen.SetSize(App->Width, App->Height)
                              .SetAttachmentCount(3)
                              .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                              .SetAttachmentFormat(1, VK_FORMAT_R8G8B8A8_UNORM)
                              .SetAttachmentFormat(2, VK_FORMAT_R32G32B32A32_UINT);
        Framebuffers.Offscreen.BuildBuffers(VulkanDevice,LayoutCommand);        
    }    
    //SSAO
    {
        uint32_t SSAOWidth = App->Width;
        uint32_t SSAOHeight = App->Height;
        Framebuffers.SSAO.SetSize(SSAOWidth, SSAOHeight)
                         .SetAttachmentCount(1)
                         .SetAttachmentFormat(0, VK_FORMAT_R8_UNORM)
                         .HasDepth=false;
        Framebuffers.SSAO.BuildBuffers(VulkanDevice,LayoutCommand);        
    }


    //SSAO Blur
    {
        Framebuffers.SSAOBlur.SetSize(App->Width, App->Height)
                             .SetAttachmentCount(1)
                             .SetAttachmentFormat(0, VK_FORMAT_R8_UNORM)
                             .HasDepth=false;
        Framebuffers.SSAOBlur.BuildBuffers(VulkanDevice,LayoutCommand);  
    }
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, LayoutCommand, App->Queue, true);
}




void deferredRenderer::BuildLayoutsAndDescriptors()
{
    //Render scene (Gbuffer) : the pipeline layout contains 2 descriptor sets :
    //- 1 for all the material data (Textures, properties...)
    //- 1 for the global scene variables : Matrices, lights...
    {
        //TODO: This belongs in the scene, we can keep it there (Being created twice for each renderer !)

        
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->MaterialDescriptorSetLayout,
            App->Scene->InstanceDescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        Resources.PipelineLayouts->Add("Offscreen", pPipelineLayoutCreateInfo);
    }    

    //Composition
    {
        //Refactor that :
        //Have a vector of vector of descriptor.
        //First dimension is descriptor set layouts,
        //Second dimension is actual descriptor set information
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[2].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAOBlur._Attachments[0].ImageView, Framebuffers.SSAOBlur.Sampler),
        };
        std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts = 
        {
            App->Scene->Cubemap.DescriptorSetLayout,
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
        };
        Resources.AddDescriptorSet(VulkanDevice, "Composition", Descriptors, DescriptorPool, AdditionalDescriptorSetLayouts);
    }

    //SSAO Pass
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Textures.SSAONoise.View, Textures.SSAONoise.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOKernel.Descriptor),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOParams.Descriptor),
        };
        Resources.AddDescriptorSet(VulkanDevice, "SSAO.Generate", Descriptors, DescriptorPool);
    }

    //SSAO Blur
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAO._Attachments[0].ImageView, Framebuffers.SSAO.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        };
        Resources.AddDescriptorSet(VulkanDevice, "SSAO.Blur", Descriptors, DescriptorPool);            
    }
}


void deferredRenderer::BuildPipelines()
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
    PipelineCreateInfo.pVertexInputState = &App->VerticesDescription.InputState;
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
        PipelineCreateInfo.renderPass = App->RenderPass;

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

        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, App->PipelineCache);

        SpecializationData.EnableSSAO = 0;
        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, App->PipelineCache);
    }

    // Fill G buffer
    {
        struct specializationData
        {
            int Mask=0;
            int HasMetallicRoughnessMap = 0;
            int HasEmissiveMap = 0;
            int HasBaseColorMap = 0;
            int HasOcclusionMap = 0;
            int HasNormalMap = 0;
            int HasClearCoat = 0;
            int HasSheen = 0;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, Mask), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, HasMetallicRoughnessMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, HasEmissiveMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(3, offsetof(specializationData, HasBaseColorMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(4, offsetof(specializationData, HasOcclusionMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(5, offsetof(specializationData, HasNormalMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(6, offsetof(specializationData, HasClearCoat), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(7, offsetof(specializationData, HasSheen), sizeof(int))
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
        ShaderModules.push_back(ShaderStages[1].module);            
        ShaderModules.push_back(ShaderStages[0].module);
        
        RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

        for(int i=0; i<App->Scene->Materials.size(); i++)
        {
            int MatFlags = App->Scene->Materials[i].Flags;
            if(!Resources.Pipelines->Present(MatFlags))
            {
                if(MatFlags & materialFlags::Opaque) 
                {
                    SpecializationData.Mask=0;
                    RasterizationState.cullMode=VK_CULL_MODE_BACK_BIT;
                }
                if(MatFlags & materialFlags::Mask) 
                {
                    SpecializationData.Mask=1;
                    RasterizationState.cullMode=VK_CULL_MODE_NONE;
                }

                if(MatFlags & materialFlags::HasBaseColorMap)SpecializationData.HasBaseColorMap=1;
                if(MatFlags & materialFlags::HasEmissiveMap)SpecializationData.HasEmissiveMap=1;
                if(MatFlags & materialFlags::HasMetallicRoughnessMap)SpecializationData.HasMetallicRoughnessMap=1;
                if(MatFlags & materialFlags::HasOcclusionMap)SpecializationData.HasOcclusionMap=1;
                if(MatFlags & materialFlags::HasNormalMap)SpecializationData.HasNormalMap=1;
                if(MatFlags & materialFlags::HasClearCoat)SpecializationData.HasClearCoat=1;
                if(MatFlags & materialFlags::HasSheen)SpecializationData.HasSheen=1;
                
                Resources.Pipelines->Add(MatFlags, PipelineCreateInfo, App->PipelineCache);
            }
        }        

    }

    //SSAO
    {
        RasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        
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
        Resources.Pipelines->Add("SSAO.Generate", PipelineCreateInfo, App->PipelineCache);
    }

    //SSAO blur
    {
        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);            
        PipelineCreateInfo.renderPass = Framebuffers.SSAOBlur.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("SSAO.Blur");
        Resources.Pipelines->Add("SSAO.Blur" ,PipelineCreateInfo, App->PipelineCache);
    }        
}


void deferredRenderer::BuildCommandBuffers()
{
    
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;


    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));
		
		App->ImGuiHelper->UpdateBuffers();
        
        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width, (float)App->Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);
        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width, App->Height, 0, 0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        

        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 0, 1, Resources.DescriptorSets->GetPtr("Composition"), 0, nullptr);
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 1, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 2, 1, App->Scene->Resources.DescriptorSets->GetPtr("Scene"), 0, nullptr);

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Composition.SSAO.Enabled"));
        vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Meshes.Quad.VertexBuffer.Buffer, Offsets);
        vkCmdBindIndexBuffer(DrawCommandBuffers[i], Meshes.Quad.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(DrawCommandBuffers[i], 6, 1, 0, 0, 1);


        App->ImGuiHelper->DrawFrame(DrawCommandBuffers[i]);

        vkCmdEndRenderPass(DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}

void deferredRenderer::BuildDeferredCommandBuffers()
{
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &OffscreenSemaphore));

    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferBeginInfo));
    
    //G-buffer pass
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

        
    VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width - App->Scene->ViewportStart, (float)App->Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);
    vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);


    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffers.Offscreen.Width,Framebuffers.Offscreen.Height,0,0);
    vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    VkDeviceSize Offset[1] = {0};

    VkPipelineLayout RendererPipelineLayout =  Resources.PipelineLayouts->Get("Offscreen");
    VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");


	for (auto &InstanceGroup : App->Scene->Instances)
	{
		int Flag = InstanceGroup.first;
		vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get(Flag));
		vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
		
		for (auto Instance : InstanceGroup.second)
		{
			vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VertexBuffer.Buffer, Offset);
			vkCmdBindIndexBuffer(OffscreenCommandBuffer, Instance.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
			vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
			vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, 0, 0);
		}
	}

	//{ //Cubemap
	//	vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Cubemap"));
	//	vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &App->Scene->Cubemap.Mesh.VertexBuffer.Buffer, Offset);
	//	vkCmdBindIndexBuffer(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
	//	vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 0, 1, &RendererDescriptorSet, 0, nullptr);
	//	vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 1, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
	//	vkCmdDrawIndexed(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexCount, 1, 0, 0, 0);
	//}

    // for(auto Instance : App->Scene->Instances)
    // {
    //     if(Instance.Mesh->Material->HasAlpha)
    //     {
    //         continue;
    //     }
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
    //     vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, Instance.Mesh->IndexBase, 0);
    // }

    // vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Blend"));
    // for(auto Instance : App->Scene->Instances)
    // {
    //     if(Instance.Mesh->Material->HasAlpha)
    //     {
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
    //         vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, Instance.Mesh->IndexBase, 0);
    //     }
    // }

    vkCmdEndRenderPass(OffscreenCommandBuffer);

    //
    //Transition framebuffers from layout color attachment to layout shader read only
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[0].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[1].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[2].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //No ssao for now
    if(false)
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

        
        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAO.Width,Framebuffers.SSAO.Height,0,0);
        vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("SSAO.Generate"), 0, 1, Resources.DescriptorSets->GetPtr("SSAO.Generate"), 0, nullptr);
        vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("SSAO.Generate"));
        vkCmdDraw(OffscreenCommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(OffscreenCommandBuffer);
        
		vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
			Framebuffers.SSAO._Attachments[0].Image,
			VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                
        RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
        RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
        RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
        vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f, 0, 0);
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


void deferredRenderer::UpdateCamera()
{
    App->Scene->UpdateUniformBufferMatrices();
    UpdateUniformBufferSSAOParams();
}

void deferredRenderer::Destroy()
{
    vkDestroySemaphore(VulkanDevice->Device, OffscreenSemaphore, nullptr);
    for (size_t i = 0; i < ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(VulkanDevice->Device, ShaderModules[i], nullptr);
    }

    Textures.SSAONoise.Destroy(VulkanDevice);
    
    Framebuffers.Offscreen.Destroy(VulkanDevice->Device);
    Framebuffers.SSAO.Destroy(VulkanDevice->Device);
    Framebuffers.SSAOBlur.Destroy(VulkanDevice->Device);
    
    Meshes.Quad.Destroy();
    UniformBuffers.SSAOKernel.Destroy();
    UniformBuffers.SSAOParams.Destroy();
    Resources.Destroy();
    vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
    
    vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, (uint32_t)DrawCommandBuffers.size(), DrawCommandBuffers.data());
    vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, 1, &OffscreenCommandBuffer);
}