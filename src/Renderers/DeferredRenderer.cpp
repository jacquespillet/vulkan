#include "DeferredRenderer.h"
#include "App.h"

renderer::renderer(vulkanApp *App) : App(App), Device(App->VulkanObjects.Device), VulkanDevice(App->VulkanObjects.VulkanDevice)
{

}

deferredRenderer::deferredRenderer(vulkanApp *App) : renderer(App) {}

void deferredRenderer::Render()
{
    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
    UpdateCamera();

    VK_CALL(App->VulkanObjects.Swapchain.AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    //Before color output stage, wait for present semaphore to be complete, and signal Render semaphore to be completed
    
    VulkanObjects.SubmitInfo = vulkanTools::BuildSubmitInfo();
    VulkanObjects.SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    VulkanObjects.SubmitInfo.waitSemaphoreCount = 1;
    VulkanObjects.SubmitInfo.signalSemaphoreCount=1;
    VulkanObjects.SubmitInfo.pWaitSemaphores = &App->VulkanObjects.Semaphores.PresentComplete;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &VulkanObjects.OffscreenSemaphore;
    VulkanObjects.SubmitInfo.commandBufferCount=1;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VulkanObjects.SubmitInfo.pWaitSemaphores = &VulkanObjects.OffscreenSemaphore;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &App->VulkanObjects.Semaphores.RenderComplete;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.DrawCommandBuffers[App->VulkanObjects.CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->VulkanObjects.Swapchain.QueuePresent(App->VulkanObjects.Queue, App->VulkanObjects.CurrentBuffer, App->VulkanObjects.Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));
}

void deferredRenderer::Setup()
{
    CreateCommandBuffers();
    SetupDescriptorPool();
    VulkanObjects.Resources.Init(VulkanDevice, VulkanObjects.DescriptorPool, App->VulkanObjects.TextureLoader);
    BuildUniformBuffers();
    BuildQuads();
    BuildOffscreenBuffers();
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &VulkanObjects.OffscreenSemaphore));

    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
}

//


void deferredRenderer::CreateCommandBuffers()
{
    VulkanObjects.DrawCommandBuffers.resize(App->VulkanObjects.Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(VulkanObjects.DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, VulkanObjects.DrawCommandBuffers.data()));

    VulkanObjects.OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(Device, App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
}

void deferredRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &VulkanObjects.DescriptorPool));    
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

    App->VulkanObjects.TextureLoader->CreateTexture(SSAONoise.data(), SSAONoise.size() * sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT, SSAO_NOISE_DIM, SSAO_NOISE_DIM, &Textures.SSAONoise, VK_FILTER_NEAREST);        
}



void deferredRenderer::UpdateUniformBufferSSAOParams()
{
    VK_CALL(UniformBuffers.SSAOParams.Map());
    UniformBuffers.SSAOParams.CopyTo(&UBOSSAOParams, sizeof(UBOSSAOParams));
    UniformBuffers.SSAOParams.Unmap();
}


void deferredRenderer::BuildQuads()
{
    Quad = vulkanTools::BuildQuad(VulkanDevice);
}



void deferredRenderer::BuildOffscreenBuffers()
{
    VkCommandBuffer LayoutCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);                   
    //G buffer
    {
        Framebuffers.Offscreen.SetSize(App->Width, App->Height)
                              .SetAttachmentCount(4)
                              .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                              .SetAttachmentFormat(1, VK_FORMAT_R8G8B8A8_UNORM)
                              .SetAttachmentFormat(2, VK_FORMAT_R32G32B32A32_UINT)
                              .SetAttachmentFormat(3, VK_FORMAT_R8G8B8A8_UNORM);
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
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, LayoutCommand, App->VulkanObjects.Queue, true);
}




void deferredRenderer::BuildLayoutsAndDescriptors()
{
    //Render scene (Gbuffer)
    {        
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Material"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Instances")
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        VulkanObjects.Resources.PipelineLayouts->Add("Offscreen", pPipelineLayoutCreateInfo);
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
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[3].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAOBlur._Attachments[0].ImageView, Framebuffers.SSAOBlur.Sampler),
        };
        std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts = 
        {
            App->Scene->Cubemap.VulkanObjects.DescriptorSetLayout,
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
        };
        VulkanObjects.Resources.AddDescriptorSet(VulkanDevice, "Composition", Descriptors, VulkanObjects.DescriptorPool, AdditionalDescriptorSetLayouts);
    }

    //SSAO Pass
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Textures.SSAONoise.View, Textures.SSAONoise.Sampler),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOKernel.VulkanObjects.Descriptor),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, UniformBuffers.SSAOParams.VulkanObjects.Descriptor),
        };
        VulkanObjects.Resources.AddDescriptorSet(VulkanDevice, "SSAO.Generate", Descriptors, VulkanObjects.DescriptorPool);
    }

    //SSAO Blur
    {
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.SSAO._Attachments[0].ImageView, Framebuffers.SSAO.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        };
        VulkanObjects.Resources.AddDescriptorSet(VulkanDevice, "SSAO.Blur", Descriptors, VulkanObjects.DescriptorPool);            
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
    PipelineCreateInfo.pVertexInputState = &App->VulkanObjects.VerticesDescription.InputState;
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
        PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("Composition");
        PipelineCreateInfo.renderPass = App->VulkanObjects.RenderPass;

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/Composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/Composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
        VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);
        VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);

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

        VulkanObjects.Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, App->VulkanObjects.PipelineCache);

        SpecializationData.EnableSSAO = 0;
        VulkanObjects.Resources.Pipelines->Add("Composition.SSAO.Disabled", PipelineCreateInfo, App->VulkanObjects.PipelineCache);
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

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);            
        VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);
        
        RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        PipelineCreateInfo.renderPass = Framebuffers.Offscreen.RenderPass;
        PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("Offscreen");

        std::array<VkPipelineColorBlendAttachmentState, 4> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();

        for(int i=0; i<App->Scene->Materials.size(); i++)
        {
            int MatFlags = App->Scene->Materials[i].Flags;
            if(!VulkanObjects.Resources.Pipelines->Present(MatFlags))
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
                
                VulkanObjects.Resources.Pipelines->Add(MatFlags, PipelineCreateInfo, App->VulkanObjects.PipelineCache);
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

        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);
        VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);

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
        PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("SSAO.Generate");
        VulkanObjects.Resources.Pipelines->Add("SSAO.Generate", PipelineCreateInfo, App->VulkanObjects.PipelineCache);
    }

    //SSAO blur
    {
        //ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        //ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        //VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);
        //VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);            
        //PipelineCreateInfo.renderPass = Framebuffers.SSAOBlur.RenderPass;
        //PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("SSAO.Blur");
        //VulkanObjects.Resources.Pipelines->Add("SSAO.Blur" ,PipelineCreateInfo, App->VulkanObjects.PipelineCache);
    }        
}


void deferredRenderer::BuildCommandBuffers()
{
    
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->VulkanObjects.RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;


    for(uint32_t i=0; i<VulkanObjects.DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->VulkanObjects.AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(VulkanObjects.DrawCommandBuffers[i], &CommandBufferInfo));
		
		App->ImGuiHelper->UpdateBuffers();
        
        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width, (float)App->Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(VulkanObjects.DrawCommandBuffers[i], 0, 1, &Viewport);
        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width, App->Height, 0, 0);
        vkCmdSetScissor(VulkanObjects.DrawCommandBuffers[i], 0, 1, &Scissor);

        vkCmdBeginRenderPass(VulkanObjects.DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        

        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("Composition"), 0, 1, VulkanObjects.Resources.DescriptorSets->GetPtr("Composition"), 0, nullptr);
        vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("Composition"), 1, 1, &App->Scene->Cubemap.VulkanObjects.DescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("Composition"), 2, 1, App->Scene->Resources.DescriptorSets->GetPtr("Scene"), 0, nullptr);

        vkCmdBindPipeline(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get("Composition.SSAO.Enabled"));
        vkCmdBindVertexBuffers(VulkanObjects.DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Quad.VulkanObjects.VertexBuffer.VulkanObjects.Buffer, Offsets);
        vkCmdBindIndexBuffer(VulkanObjects.DrawCommandBuffers[i], Quad.VulkanObjects.IndexBuffer.VulkanObjects.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(VulkanObjects.DrawCommandBuffers[i], 6, 1, 0, 0, 1);


        App->ImGuiHelper->DrawFrame(VulkanObjects.DrawCommandBuffers[i]);

        vkCmdEndRenderPass(VulkanObjects.DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(VulkanObjects.DrawCommandBuffers[i]));
    }
}

void deferredRenderer::BuildDeferredCommandBuffers()
{
    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(VulkanObjects.OffscreenCommandBuffer, &CommandBufferBeginInfo));
    
    //G-buffer pass
    std::array<VkClearValue, 5> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[2].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[3].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[4].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = Framebuffers.Offscreen.RenderPass;
    RenderPassBeginInfo.framebuffer = Framebuffers.Offscreen.Framebuffer;
    RenderPassBeginInfo.renderArea.extent.width = Framebuffers.Offscreen.Width;
    RenderPassBeginInfo.renderArea.extent.height = Framebuffers.Offscreen.Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();
    
    vkCmdBeginRenderPass(VulkanObjects.OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
    VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width - App->Scene->ViewportStart, (float)App->Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);
    vkCmdSetViewport(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Viewport);


    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffers.Offscreen.Width,Framebuffers.Offscreen.Height,0,0);
    vkCmdSetScissor(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Scissor);

    VkDeviceSize Offset[1] = {0};

    VkPipelineLayout RendererPipelineLayout =  VulkanObjects.Resources.PipelineLayouts->Get("Offscreen");
    VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");


	for (auto &InstanceGroup : App->Scene->Instances)
	{
		int Flag = InstanceGroup.first;
		vkCmdBindPipeline(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get(Flag));
		vkCmdBindDescriptorSets(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
		
		for (auto Instance : InstanceGroup.second)
		{
			vkCmdBindVertexBuffers(VulkanObjects.OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VulkanObjects.VertexBuffer.VulkanObjects.Buffer, Offset);
			vkCmdBindIndexBuffer(VulkanObjects.OffscreenCommandBuffer, Instance.Mesh->VulkanObjects.IndexBuffer.VulkanObjects.Buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->VulkanObjects.DescriptorSet, 0, nullptr);
			vkCmdBindDescriptorSets(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.VulkanObjects.DescriptorSet, 0, nullptr);
			vkCmdDrawIndexed(VulkanObjects.OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, 0, 0);
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

    vkCmdEndRenderPass(VulkanObjects.OffscreenCommandBuffer);

    //
    //Transition framebuffers from layout color attachment to layout shader read only
    vulkanTools::TransitionImageLayout(VulkanObjects.OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[0].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(VulkanObjects.OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[1].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(VulkanObjects.OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[2].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(VulkanObjects.OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[3].Image, 
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

        vkCmdBeginRenderPass(VulkanObjects.OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAO.Width,Framebuffers.SSAO.Height,0,0);
        vkCmdSetScissor(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Scissor);

        vkCmdBindDescriptorSets(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("SSAO.Generate"), 0, 1, VulkanObjects.Resources.DescriptorSets->GetPtr("SSAO.Generate"), 0, nullptr);
        vkCmdBindPipeline(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get("SSAO.Generate"));
        vkCmdDraw(VulkanObjects.OffscreenCommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(VulkanObjects.OffscreenCommandBuffer);
        
		vulkanTools::TransitionImageLayout(VulkanObjects.OffscreenCommandBuffer,
			Framebuffers.SSAO._Attachments[0].Image,
			VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                
        RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
        RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
        RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
        vkCmdBeginRenderPass(VulkanObjects.OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAOBlur.Width,Framebuffers.SSAOBlur.Height,0,0);
        vkCmdSetScissor(VulkanObjects.OffscreenCommandBuffer, 0, 1, &Scissor);
        
        vkCmdBindDescriptorSets(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("SSAO.Blur"), 0, 1, VulkanObjects.Resources.DescriptorSets->GetPtr("SSAO.Blur"), 0, nullptr);
        vkCmdBindPipeline(VulkanObjects.OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get("SSAO.Blur"));
        vkCmdDraw(VulkanObjects.OffscreenCommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(VulkanObjects.OffscreenCommandBuffer);
    }
    VK_CALL(vkEndCommandBuffer(VulkanObjects.OffscreenCommandBuffer));
}


void deferredRenderer::UpdateCamera()
{
    UpdateUniformBufferSSAOParams();
}

void deferredRenderer::Resize(uint32_t Width, uint32_t Height) 
{
    Framebuffers.Offscreen.Destroy(VulkanDevice->Device);
    Framebuffers.SSAO.Destroy(VulkanDevice->Device);
    Framebuffers.SSAOBlur.Destroy(VulkanDevice->Device);
    BuildOffscreenBuffers();

    VkDescriptorSet TargetDescriptorSet = VulkanObjects.Resources.DescriptorSets->Get("Composition");
    std::vector<descriptor> Descriptors = 
    {
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[2].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[3].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
    };
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets(4);
    WriteDescriptorSets[0] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[0].DescriptorType, 0, &Descriptors[0].DescriptorImageInfo); 
    WriteDescriptorSets[1] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[1].DescriptorType, 1, &Descriptors[1].DescriptorImageInfo); 
    WriteDescriptorSets[2] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[2].DescriptorType, 2, &Descriptors[2].DescriptorImageInfo); 
    WriteDescriptorSets[3] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[3].DescriptorType, 3, &Descriptors[3].DescriptorImageInfo); 
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);        
}

void deferredRenderer::Destroy()
{
    vkDestroySemaphore(VulkanDevice->Device, VulkanObjects.OffscreenSemaphore, nullptr);
    for (size_t i = 0; i < VulkanObjects.ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(VulkanDevice->Device, VulkanObjects.ShaderModules[i], nullptr);
    }

    Textures.SSAONoise.Destroy(VulkanDevice);
    
    Framebuffers.Offscreen.Destroy(VulkanDevice->Device);
    Framebuffers.SSAO.Destroy(VulkanDevice->Device);
    Framebuffers.SSAOBlur.Destroy(VulkanDevice->Device);
    
    Quad.Destroy();
    UniformBuffers.SSAOKernel.Destroy();
    UniformBuffers.SSAOParams.Destroy();
    VulkanObjects.Resources.Destroy();
    vkDestroyDescriptorPool(VulkanDevice->Device, VulkanObjects.DescriptorPool, nullptr);
    
    
    vkFreeCommandBuffers(VulkanDevice->Device, App->VulkanObjects.CommandPool, (uint32_t)VulkanObjects.DrawCommandBuffers.size(), VulkanObjects.DrawCommandBuffers.data());
    vkFreeCommandBuffers(VulkanDevice->Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.OffscreenCommandBuffer);
}