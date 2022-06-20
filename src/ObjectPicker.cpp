#include "ObjectPicker.h"
#include "App.h"
#include "Device.h"

void objectPicker::Initialize(vulkanDevice *_VulkanDevice, vulkanApp *_App)
{
    this->VulkanDevice = _VulkanDevice;
    this->App = _App;

    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(VulkanDevice->Device, &SemaphoreCreateInfo, nullptr, &OffscreenSemaphore));


    //Create Command Buffers

    //Build Quad
    Quad = vulkanTools::BuildQuad(VulkanDevice);

    //Create framebuffer attachment
    OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    {
        Framebuffer.SetSize(App->Width, App->Height)
                              .SetAttachmentCount(1)
                              .SetAttachmentImageLayout(0,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                              .SetImageFlags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                              .SetAttachmentFormat(0, VK_FORMAT_R32_UINT);
        Framebuffer.BuildBuffers(VulkanDevice,OffscreenCommandBuffer);
    }    
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, OffscreenCommandBuffer, App->VulkanObjects.Queue, false);    

    //Create descriptor sets and pipeline layout
    {        
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Instances")
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        VK_CALL(vkCreatePipelineLayout(VulkanDevice->Device, &pPipelineLayoutCreateInfo, nullptr, &PipelineLayout));
    }

    //Create render pipeline
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

    ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/ObjectPicker.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/ObjectPicker.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    ShaderModules.push_back(ShaderStages[1].module);            
    ShaderModules.push_back(ShaderStages[0].module);
    
    RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    PipelineCreateInfo.renderPass = Framebuffer.RenderPass;
    PipelineCreateInfo.layout = PipelineLayout;

    VK_CALL(vkCreateGraphicsPipelines(VulkanDevice->Device, nullptr, 1, &PipelineCreateInfo, nullptr, &Pipeline));


	FillCommandBuffer();
}

void objectPicker::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    Framebuffer.Destroy(VulkanDevice->Device);
    
    OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    {
        Framebuffer.SetSize(App->Width, App->Height)
                              .SetAttachmentImageLayout(0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                              .SetImageFlags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                              .SetAttachmentCount(1)
                              .SetAttachmentFormat(0, VK_FORMAT_R32_UINT);
        Framebuffer.BuildBuffers(VulkanDevice,OffscreenCommandBuffer);
    }    
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, OffscreenCommandBuffer, App->VulkanObjects.Queue, false); 
}

void objectPicker::FillCommandBuffer()
{

    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color.uint32[0] = UINT32_MAX;
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = Framebuffer.RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = Framebuffer.Width;
    RenderPassBeginInfo.renderArea.extent.height = Framebuffer.Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;

    RenderPassBeginInfo.framebuffer = Framebuffer.Framebuffer;
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferInfo));
    
    
    VkViewport Viewport = vulkanTools::BuildViewport((float)Framebuffer.Width - App->Scene->ViewportStart, (float)Framebuffer.Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);    
    vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);
    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffer.Width, Framebuffer.Height, 0, 0);
    vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    

    VkDeviceSize Offset[1] = {0};
    
    vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
    
    vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, App->Scene->Resources.DescriptorSets->GetPtr("Scene"), 0, nullptr);
    for(auto &InstanceGroup : App->Scene->Instances)
    {
        vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline); 
        
        for(auto Instance : InstanceGroup.second)
        {
            vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 1, 1, &Instance.VulkanObjects.DescriptorSet, 0, nullptr);
                
            vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VulkanObjects.VertexBuffer.VulkanObjects.Buffer, Offset);
            vkCmdBindIndexBuffer(OffscreenCommandBuffer, Instance.Mesh->VulkanObjects.IndexBuffer.VulkanObjects.Buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, 0, 0);
        }
    }

    vkCmdEndRenderPass(OffscreenCommandBuffer);

    VK_CALL(vkEndCommandBuffer(OffscreenCommandBuffer));
}

void objectPicker::Render()
{
    FillCommandBuffer();

    VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 0;
    SubmitInfo.signalSemaphoreCount=0;
    SubmitInfo.pWaitSemaphores = nullptr;
    SubmitInfo.pSignalSemaphores = nullptr;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));    
}

void objectPicker::Pick(int MouseX, int MouseY)
{
    Render();

    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferInfo));

    //Create a host visible buffer 
    std::vector<uint32_t> Data(Framebuffer.Width * Framebuffer.Height, 456);
    buffer ImageBuffer;
    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &ImageBuffer,
        Framebuffer.Width * Framebuffer.Height * sizeof(uint32_t),
        Data.data()
    ));

    //Copy into the host visible
    VkImageSubresourceLayers ImageSubresource = {};
    ImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ImageSubresource.baseArrayLayer=0;
    ImageSubresource.layerCount=1;
    ImageSubresource.mipLevel=0;

    VkBufferImageCopy BufferImageCopy = {};
    BufferImageCopy.imageExtent.depth=1;
    BufferImageCopy.imageExtent.width = Framebuffer.Width;
    BufferImageCopy.imageExtent.height = Framebuffer.Height;
    BufferImageCopy.imageOffset = {0,0,0};
    BufferImageCopy.imageSubresource = ImageSubresource;

    BufferImageCopy.bufferOffset=0;
    BufferImageCopy.bufferImageHeight = 0;
    BufferImageCopy.bufferRowLength=0;

    VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer, Framebuffer._Attachments[0].Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);
    
    vkCmdCopyImageToBuffer(OffscreenCommandBuffer, Framebuffer._Attachments[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ImageBuffer.VulkanObjects.Buffer,1,  &BufferImageCopy);
    
    VK_CALL(vkEndCommandBuffer(OffscreenCommandBuffer));

    VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 0;
    SubmitInfo.signalSemaphoreCount=0;
    SubmitInfo.pWaitSemaphores = nullptr;
    SubmitInfo.pSignalSemaphores = nullptr;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));

    //Find the object
    ImageBuffer.Map();
    ImageBuffer.CopyFrom(Data.data(), Data.size() * sizeof(uint32_t), 0);
    ImageBuffer.Unmap();
    int MouseIndex = MouseY * Framebuffer.Width + MouseX;
    uint32_t InstanceInx = Data[MouseIndex];
    //Set it selected
    App->SetSelectedItem(InstanceInx, false);
    ImageBuffer.Destroy();
}

void objectPicker::Destroy()
{
    for(size_t i=0; i<ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(VulkanDevice->Device, ShaderModules[i], nullptr);
    }
    vkDestroyPipeline(VulkanDevice->Device, Pipeline, nullptr);
    vkDestroyPipelineLayout(VulkanDevice->Device, PipelineLayout, nullptr);
    Framebuffer.Destroy(VulkanDevice->Device);
    Quad.Destroy();
    vkFreeCommandBuffers(VulkanDevice->Device, App->VulkanObjects.CommandPool, 1, &OffscreenCommandBuffer);
    vkDestroySemaphore(VulkanDevice->Device, OffscreenSemaphore, nullptr);
}