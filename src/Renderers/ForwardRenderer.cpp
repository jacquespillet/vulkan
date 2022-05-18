#include "ForwardRenderer.h"
#include "App.h"

#define PER_MESH_BUFFER 1
forwardRenderer::forwardRenderer(vulkanApp *App) : renderer(App) {}

void forwardRenderer::Render()
{
    BuildCommandBuffers();
    UpdateCamera();

    VK_CALL(App->Swapchain.AcquireNextImage(App->Semaphores.PresentComplete, &App->CurrentBuffer));
    
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pWaitSemaphores = &App->Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &App->Semaphores.RenderComplete;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &DrawCommandBuffers[App->CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->Swapchain.QueuePresent(App->Queue, App->CurrentBuffer, App->Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->Queue));
}

void forwardRenderer::Setup()
{
    CreateCommandBuffers();
    SetupDescriptorPool();
    Resources.Init(VulkanDevice, DescriptorPool, App->TextureLoader);
    BuildUniformBuffers();
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    BuildCommandBuffers();
}

//


void forwardRenderer::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(App->Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));
}

void forwardRenderer::SetupDescriptorPool()
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



void forwardRenderer::BuildUniformBuffers()
{    
    //Deferred vertex shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SceneMatrices,
                                sizeof(UBOSceneMatrices)
    );

    UpdateUniformBufferDeferredMatrices();
}

void forwardRenderer::UpdateUniformBufferDeferredMatrices()
{
    UBOSceneMatrices.Projection = Camera.GetProjectionMatrix();
    UBOSceneMatrices.View = Camera.GetViewMatrix();
    UBOSceneMatrices.Model = glm::mat4(1);
    
    UBOSceneMatrices.ViewportDim = glm::vec2((float)App->Width,(float)App->Height);
    VK_CALL(UniformBuffers.SceneMatrices.Map());
    UniformBuffers.SceneMatrices.CopyTo(&UBOSceneMatrices, sizeof(UBOSceneMatrices));
    UniformBuffers.SceneMatrices.Unmap();
}


void forwardRenderer::BuildLayoutsAndDescriptors()
{
    //Render scene (Gbuffer) : the pipeline layout contains 3 descriptor sets :
    //- 1 for the global scene variables : Matrices, lights...
    //- 1 for all the material data (Textures, properties...)
    //- 1 for all the instance specific data (Model matrix)
    {
        //Create the materials descriptor sets
        App->Scene->CreateDescriptorSets();

        //Create the scene descriptor set layout
        VkDescriptorSetLayoutBinding SetLayoutBindings = vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 );
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(&SetLayoutBindings, 1);
        VkDescriptorSetLayout RendererDescriptorSetLayout = Resources.DescriptorSetLayouts->Add("Scene", DescriptorLayoutCreateInfo);

        //Allocate and write descriptor sets
        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &RendererDescriptorSetLayout, 1);
        VkDescriptorSet RendererDescriptorSet = Resources.DescriptorSets->Add("Scene", AllocInfo);
        VkWriteDescriptorSet WriteDescriptorSets = vulkanTools::BuildWriteDescriptorSet( RendererDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &UniformBuffers.SceneMatrices.Descriptor);
        vkUpdateDescriptorSets(Device, 1, &WriteDescriptorSets, 0, nullptr);

        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            RendererDescriptorSetLayout,
            App->Scene->MaterialDescriptorSetLayout,
            App->Scene->InstanceDescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        Resources.PipelineLayouts->Add("Scene", pPipelineLayoutCreateInfo);
    }
}


void forwardRenderer::BuildPipelines()
{
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
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        ShaderModules.push_back(ShaderStages[1].module);            
        ShaderModules.push_back(ShaderStages[0].module);

        PipelineCreateInfo.renderPass = App->RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Scene");

        std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();
        Resources.Pipelines->Add("Scene.Solid", PipelineCreateInfo, App->PipelineCache);
        
        //Transparents
        DepthStencilState.depthWriteEnable=VK_FALSE;
        RasterizationState.cullMode=VK_CULL_MODE_NONE;
        SpecializationData.Discard=1;
        Resources.Pipelines->Add("Scene.Blend", PipelineCreateInfo, App->PipelineCache);
    }   
}



void forwardRenderer::BuildCommandBuffers()
{   
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    //G-buffer pass
    std::array<VkClearValue, 2> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->RenderPass;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();

    
    App->ImGuiHelper->NewFrame(App, (App->CurrentBuffer == 0));
    App->ImGuiHelper->UpdateBuffers();

    //Rendering abstraction : 
    //Foreach command buffer
    //  Scene.Render(CommandBuffer)
    //      For each object in scene
    //          BindPipeline(Mesh.MatType) - Mattype being pbr, non - pbr....
    //          Bind vert/ind buffer, bind descriptor sets
    //          Draw    
    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));

        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width, (float)App->Height, 0.0f, 1.0f);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);

        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width,App->Height,0,0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        VkDeviceSize Offset[1] = {0};

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Solid")); 
        VkPipelineLayout RendererPipelineLayout =  Resources.PipelineLayouts->Get("Scene");
        VkDescriptorSet RendererDescriptorSet = Resources.DescriptorSets->Get("Scene");
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);

        for(auto Instance : App->Scene->Instances)
        {
            if(Instance.Mesh->Material->HasAlpha)
            {
                continue;
            }
            vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VertexBuffer.Buffer, Offset);
            vkCmdBindIndexBuffer(DrawCommandBuffers[i], Instance.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
            vkCmdDrawIndexed(DrawCommandBuffers[i], Instance.Mesh->IndexCount, 1, 0, 0, 0);
        }

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Blend"));
        for(auto Instance : App->Scene->Instances)
        {
            if(Instance.Mesh->Material->HasAlpha)
            {
                vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VertexBuffer.Buffer, Offset);
                vkCmdBindIndexBuffer(DrawCommandBuffers[i], Instance.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
                
                vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
                vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
                vkCmdDrawIndexed(DrawCommandBuffers[i], Instance.Mesh->IndexCount, 1, 0, 0, 0);
            }
        }
        App->ImGuiHelper->DrawFrame(DrawCommandBuffers[i]);
        vkCmdEndRenderPass(DrawCommandBuffers[i]);
        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}


void forwardRenderer::UpdateCamera()
{
    UpdateUniformBufferDeferredMatrices();
}

void forwardRenderer::Destroy()
{
    for(size_t i=0; i<ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(Device, ShaderModules[i], nullptr);
    }
    UniformBuffers.SceneMatrices.Destroy();
    Resources.Destroy();
    vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
    vkFreeCommandBuffers(Device, App->CommandPool, (uint32_t)DrawCommandBuffers.size(), DrawCommandBuffers.data());
}