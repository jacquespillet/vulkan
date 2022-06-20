#include "ForwardRenderer.h"
#include "App.h"
#include "imgui.h"

forwardRenderer::forwardRenderer(vulkanApp *App) : renderer(App) {}

void forwardRenderer::Render()
{
    BuildCommandBuffers();
    
    VK_CALL(App->VulkanObjects.Swapchain.AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    VulkanObjects.SubmitInfo = vulkanTools::BuildSubmitInfo();
    VulkanObjects.SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    VulkanObjects.SubmitInfo.waitSemaphoreCount = 1;
    VulkanObjects.SubmitInfo.signalSemaphoreCount=1;
    VulkanObjects.SubmitInfo.pWaitSemaphores = &App->VulkanObjects.Semaphores.PresentComplete;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &App->VulkanObjects.Semaphores.RenderComplete;
    VulkanObjects.SubmitInfo.commandBufferCount=1;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.DrawCommandBuffers[App->VulkanObjects.CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->VulkanObjects.Swapchain.QueuePresent(App->VulkanObjects.Queue, App->VulkanObjects.CurrentBuffer, App->VulkanObjects.Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));
}

void forwardRenderer::Setup()
{
    CreateCommandBuffers();
    SetupDescriptorPool();
    VulkanObjects.Resources.Init(VulkanDevice, VulkanObjects.DescriptorPool, App->VulkanObjects.TextureLoader);
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    BuildCommandBuffers();
}

//


void forwardRenderer::CreateCommandBuffers()
{
    VulkanObjects.DrawCommandBuffers.resize(App->VulkanObjects.Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(VulkanObjects.DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, VulkanObjects.DrawCommandBuffers.data()));
}

void forwardRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 32);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &VulkanObjects.DescriptorPool));    
}


inline float Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}


void forwardRenderer::BuildLayoutsAndDescriptors()
{
    
    //Render scene : the pipeline layout contains 4 descriptor sets :
    //- 1 for the global scene variables : Matrices, lights...
    //- 1 for all the material data (Textures, properties...)
    //- 1 for all the instance specific data (Model matrix)
    //- 1 for all cubemap and ibl data
    {
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Material"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Instances"),
            App->Scene->Cubemap.VulkanObjects.DescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        VulkanObjects.Resources.PipelineLayouts->Add("Scene", pPipelineLayoutCreateInfo);
    }

    //Cubemap rendering
    {
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Cubemap.VulkanObjects.DescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        VulkanObjects.Resources.PipelineLayouts->Add("Cubemap", pPipelineLayoutCreateInfo);
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

    //Loop through materials
    //  Check if a pipeline with this flag already exists, if not :
    //  For each flag
    //      Check the flag, and set a specialization data property
    //      Build the pipeline and add it to the list
    
    //Store the meshes according to their material masks
    //  std::unorderedmap<flag, std::vector<Instance>> 


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
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);
        VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);            

        PipelineCreateInfo.renderPass = App->VulkanObjects.RenderPass;
        PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("Scene");

        std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
        {
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
    
    //Cube map
    {
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/cubemap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/cubemap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VulkanObjects.ShaderModules.push_back(ShaderStages[1].module);            
        VulkanObjects.ShaderModules.push_back(ShaderStages[0].module);

        PipelineCreateInfo.renderPass = App->VulkanObjects.RenderPass;
        PipelineCreateInfo.layout = VulkanObjects.Resources.PipelineLayouts->Get("Cubemap");

        std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();
        VulkanObjects.Resources.Pipelines->Add("Cubemap", PipelineCreateInfo, App->VulkanObjects.PipelineCache);
    }   
}

void forwardRenderer::RenderGUI()
{

}




void forwardRenderer::BuildCommandBuffers()
{   
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    //G-buffer pass
    std::array<VkClearValue, 2> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->VulkanObjects.RenderPass;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();

    
    App->ImGuiHelper->UpdateBuffers();

    //Rendering abstraction : 
    //Foreach command buffer
    //  Scene.Render(CommandBuffer)
    //      For each object in scene
    //          BindPipeline(Mesh.MatType) - Mattype being pbr, non - pbr....
    //          Bind vert/ind buffer, bind descriptor sets
    //          Draw    
    for(uint32_t i=0; i<VulkanObjects.DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->VulkanObjects.AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(VulkanObjects.DrawCommandBuffers[i], &CommandBufferInfo));

        vkCmdBeginRenderPass(VulkanObjects.DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width - App->Scene->ViewportStart, (float)App->Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);
        vkCmdSetViewport(VulkanObjects.DrawCommandBuffers[i], 0, 1, &Viewport);

        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width,App->Height,0,0);
        vkCmdSetScissor(VulkanObjects.DrawCommandBuffers[i], 0, 1, &Scissor);

        VkDeviceSize Offset[1] = {0};

        VkPipelineLayout RendererPipelineLayout =  VulkanObjects.Resources.PipelineLayouts->Get("Scene");
        VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");
        for(auto &InstanceGroup : App->Scene->Instances)
        {
            int Flag = InstanceGroup.first;
            vkCmdBindPipeline(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get(Flag)); 
            vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 3, 1, &App->Scene->Cubemap.VulkanObjects.DescriptorSet, 0, nullptr);

            for(auto Instance : InstanceGroup.second)
            {
                  vkCmdBindVertexBuffers(VulkanObjects.DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VulkanObjects.VertexBuffer.Buffer, Offset);
                vkCmdBindIndexBuffer(VulkanObjects.DrawCommandBuffers[i], Instance.Mesh->VulkanObjects.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->VulkanObjects.DescriptorSet, 0, nullptr);
                vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.VulkanObjects.DescriptorSet, 0, nullptr);
                vkCmdDrawIndexed(VulkanObjects.DrawCommandBuffers[i], Instance.Mesh->IndexCount, 1, 0, 0, 0);
            }
        }

        { //Cubemap
            vkCmdBindPipeline(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.Pipelines->Get("Cubemap"));
            vkCmdBindVertexBuffers(VulkanObjects.DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &App->Scene->Cubemap.Mesh.VulkanObjects.VertexBuffer.Buffer, Offset);
            vkCmdBindIndexBuffer(VulkanObjects.DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.VulkanObjects.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("Cubemap"), 0, 1, &RendererDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(VulkanObjects.DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanObjects.Resources.PipelineLayouts->Get("Cubemap"), 1, 1, &App->Scene->Cubemap.VulkanObjects.DescriptorSet, 0, nullptr);
            vkCmdDrawIndexed(VulkanObjects.DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexCount, 1, 0, 0, 0);
        }

        App->ImGuiHelper->DrawFrame(VulkanObjects.DrawCommandBuffers[i]);
        vkCmdEndRenderPass(VulkanObjects.DrawCommandBuffers[i]);
        VK_CALL(vkEndCommandBuffer(VulkanObjects.DrawCommandBuffers[i]));
    }
}

void forwardRenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void forwardRenderer::Destroy()
{
    for(size_t i=0; i<VulkanObjects.ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(Device, VulkanObjects.ShaderModules[i], nullptr);
    }
    VulkanObjects.Resources.Destroy();
    vkDestroyDescriptorPool(Device, VulkanObjects.DescriptorPool, nullptr);
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, (uint32_t)VulkanObjects.DrawCommandBuffers.size(), VulkanObjects.DrawCommandBuffers.data());
}