#pragma once
#include "TextureLoader.h"

namespace IBLHelper
{
    
    void CalculateIrradianceMap(vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue, vulkanTexture *Cubemap, vulkanTexture *Output)
    {
        if(Output==nullptr) return;

        Output->Width = 32;
        Output->Height = 32;
        
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
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        };
        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);
        VK_CALL(vkCreateDescriptorPool(VulkanDevice->Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));                


        //create Cube
        sceneMesh Cube;
        GLTFImporter::LoadMesh("resources/models/Cube/Cube.gltf", Cube, VulkanDevice, CommandBuffer, Queue);
             
        //load shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildIrradianceMap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildIrradianceMap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
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
            vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ViewMatricesBuffer.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Cubemap->Descriptor)
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
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { (uint32_t)Output->Width, (uint32_t)Output->Height, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Cube.VertexBuffer.Buffer, Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Cube.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

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
            CubeFaceSubresourceRange.levelCount = 1;
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
            vulkanTools::TransitionImageLayout(
                CommandBuffer,
                Output->Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                CubeFaceSubresourceRange);

            VK_CALL(vkEndCommandBuffer(CommandBuffer));

            VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
            SubmitInfo.commandBufferCount = 1;
            SubmitInfo.pCommandBuffers = &CommandBuffer;
            VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
            VK_CALL(vkQueueWaitIdle(Queue));    
        }   

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
        sampler.maxLod = 1; //Mip levels
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler.maxAnisotropy = 1.0f;
        
        VK_CALL(vkCreateSampler(VulkanDevice->Device, &sampler, nullptr, &Output->Sampler));

        // Create image view
        VkImageViewCreateInfo view = vulkanTools::BuildImageViewCreateInfo();
        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 6;
        view.subresourceRange.levelCount = 1;//Miplevels
        view.image = Output->Image;
        VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Output->View));
        
        //Cleanup
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
        Cube.IndexBuffer.Destroy();
        Cube.VertexBuffer.Destroy();
        vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
        
        for(int i=0; i<6; i++)
        {
            Framebuffers[i].Destroy(VulkanDevice->Device);
        }
        
        Output->Descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Output->Descriptor.imageView = Output->View;
        Output->Descriptor.sampler = Output->Sampler;        
    }

    void CalculatePrefilteredMap(vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue, vulkanTexture *Cubemap, vulkanTexture *Output)
    {
        if(Output==nullptr) return;

        Output->Width = 128;
        Output->Height = 128;
        Output->MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(Output->Width, Output->Height)))) + 1;
        
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
        std::vector<std::vector<framebuffer>> Framebuffers(Output->MipLevels);
        uint32_t CurrentWidth = Output->Width;
        uint32_t CurrentHeight = Output->Height;
        for(uint32_t i=0; i<Output->MipLevels; i++)
        {
			Framebuffers[i].resize(6);
            for(int j=0; j<6; j++)
            {
                Framebuffers[i][j].SetSize(Output->Width, Output->Height)
                                    .SetAttachmentCount(1)
                                    .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                                    .SetImageFlags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                                    .BuildBuffers(VulkanDevice,CommandBuffer);
            }
            CurrentWidth /=2;
            CurrentHeight /=2;
        }
        //Renderpass


        //Create descriptorPool
        VkDescriptorPool DescriptorPool;
        std::vector<VkDescriptorPoolSize> PoolSizes = 
        {
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        };
        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);
        VK_CALL(vkCreateDescriptorPool(VulkanDevice->Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));                


        //create Cube
        sceneMesh Cube;
        GLTFImporter::LoadMesh("resources/models/Cube/Cube.gltf", Cube, VulkanDevice, CommandBuffer, Queue);  
        
        //load shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildPrefilterEnvMap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildPrefilterEnvMap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
        //Create uniform buffers
        struct uniforms
        {
            glm::mat4 ViewProjection;
            float Roughness;
        } Uniforms;
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
                                    sizeof(uniforms)
        );
        

        //descriptor set layout
        VkDescriptorSetLayout DescriptorSetLayout;
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = {
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0 ),
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
            vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ViewMatricesBuffer.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Cubemap->Descriptor)
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
        
        std::vector<std::vector<VkPipeline>> Pipelines(Output->MipLevels);
        for(size_t i=0; i<Output->MipLevels; i++)
        {
            Pipelines[i].resize(6);
            for(uint32_t j=0; j<6; j++)
            {
                PipelineCreateInfo.renderPass = Framebuffers[i][j].RenderPass;
                vkCreateGraphicsPipelines(VulkanDevice->Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &Pipelines[i][j]);
            }
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


        // Create the cubemap image
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
        imageCreateInfo.arrayLayers = 6;
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        VK_CALL(vkCreateImage(VulkanDevice->Device, &imageCreateInfo, nullptr, &Output->Image));

        VkMemoryAllocateInfo memAllocInfo = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanDevice->Device, Output->Image, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &memAllocInfo, nullptr, &Output->DeviceMemory));
        VK_CALL(vkBindImageMemory(VulkanDevice->Device, Output->Image, Output->DeviceMemory, 0));    


        CurrentWidth = Output->Width;
        CurrentHeight = Output->Height;
        for(size_t i=0; i<Output->MipLevels; i++)
        {
            for(uint32_t j=0; j<6; j++)
            {
                VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));
                
                RenderPassBeginInfo.framebuffer = Framebuffers[i][j].Framebuffer;
                RenderPassBeginInfo.renderPass = Framebuffers[i][j].RenderPass;
                
                Uniforms.ViewProjection = ViewMatrices[j];
                Uniforms.Roughness =  (float)i/ (float)(Output->MipLevels - 1);
                VK_CALL(ViewMatricesBuffer.Map());
                ViewMatricesBuffer.CopyTo(&Uniforms, sizeof(uniforms));
                ViewMatricesBuffer.Unmap();    

                vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                VkViewport Viewport = vulkanTools::BuildViewport((float)CurrentWidth, (float)CurrentHeight, 0.0f, 1.0f);
                vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
                VkRect2D Scissor = vulkanTools::BuildRect2D(CurrentWidth,CurrentHeight,0,0);
                vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

                VkDeviceSize Offset[1] = {0};
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipelines[i][j]); 
                vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet, 0, nullptr);

                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Cube.VertexBuffer.Buffer, Offset);
                vkCmdBindIndexBuffer(CommandBuffer, Cube.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdDrawIndexed(CommandBuffer, Cube.IndexCount, 1, 0, 0, 0);
            
                vkCmdEndRenderPass(CommandBuffer);
                
            
                vulkanTools::TransitionImageLayout(
                    CommandBuffer,
                    Framebuffers[i][j]._Attachments[0].Image,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                VkImageSubresourceRange CubeFaceSubresourceRange = {};
                CubeFaceSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                CubeFaceSubresourceRange.baseMipLevel = (uint32_t)i;
                CubeFaceSubresourceRange.levelCount = 1;
                CubeFaceSubresourceRange.baseArrayLayer = j;
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
                copyRegion.dstSubresource.baseArrayLayer = j;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstSubresource.mipLevel = (uint32_t) i;
                copyRegion.dstOffset = { 0, 0, 0 };
                copyRegion.extent.width = CurrentWidth;
                copyRegion.extent.height = CurrentHeight;
                copyRegion.extent.depth = 1;

            //     // Copy image
                vkCmdCopyImage(
                    CommandBuffer,
                    Framebuffers[i][j]._Attachments[0].Image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    Output->Image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);

                // Change image layout of copied face to shader read
                vulkanTools::TransitionImageLayout(
                    CommandBuffer,
                    Output->Image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    CubeFaceSubresourceRange);

                VK_CALL(vkEndCommandBuffer(CommandBuffer));

                VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
                SubmitInfo.commandBufferCount = 1;
                SubmitInfo.pCommandBuffers = &CommandBuffer;
                VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
                VK_CALL(vkQueueWaitIdle(Queue));    
            }   
            CurrentWidth /=2;
            CurrentHeight /=2;
        }

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
        view.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 6;
        view.subresourceRange.levelCount = Output->MipLevels;//Miplevels
        view.image = Output->Image;
        VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Output->View));
        
        //Cleanup
        for(size_t i=0; i<Pipelines.size(); i++)
        {
            for(size_t j=0; j<Pipelines[i].size(); j++)
            {
                vkDestroyPipeline(VulkanDevice->Device, Pipelines[i][j], nullptr);
            }
        }
        vkDestroyPipelineLayout(VulkanDevice->Device, PipelineLayout, nullptr);
        vkFreeDescriptorSets(VulkanDevice->Device, DescriptorPool, 1, &DescriptorSet);
        vkDestroyDescriptorSetLayout(VulkanDevice->Device, DescriptorSetLayout, nullptr);
        ViewMatricesBuffer.Destroy();
        vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[0].module, nullptr);
        vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[1].module, nullptr);
        Cube.IndexBuffer.Destroy();
        Cube.VertexBuffer.Destroy();
        vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
        
        for(size_t i=0; i<Framebuffers.size(); i++)
        {
            for(size_t j=0; j<Framebuffers[i].size(); j++)
            {
                Framebuffers[i][j].Destroy(VulkanDevice->Device);
            }
        }
        
        Output->Descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Output->Descriptor.imageView = Output->View;
        Output->Descriptor.sampler = Output->Sampler;  
    }

    void CalculateBRDFLUT(vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue, vulkanTexture *Output)
    {
        if(Output==nullptr) return;

        Output->Width = 512;
        Output->Height = 512;
        Output->MipLevels = 1;
        
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
        framebuffer Framebuffer;
        Framebuffer.SetSize(Output->Width, Output->Height)
                            .SetAttachmentCount(1)
                            .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                            .SetImageFlags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                            .BuildBuffers(VulkanDevice,CommandBuffer);
            
        //create Cube
        sceneMesh Quad = vulkanTools::BuildQuad(VulkanDevice);
        
        //load shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildBRDFLUT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/BuildBRDFLUT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
         
    
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = {};
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
        
        VkPipeline Pipeline;
        PipelineCreateInfo.renderPass = Framebuffer.RenderPass;
        vkCreateGraphicsPipelines(VulkanDevice->Device, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &Pipeline);
    
        
        VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
        std::array<VkClearValue, 2> ClearValues = {};
        ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
        ClearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
        RenderPassBeginInfo.renderArea.extent.width = Output->Width;
        RenderPassBeginInfo.renderArea.extent.height = Output->Height;
        RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
        RenderPassBeginInfo.pClearValues=ClearValues.data();


        // Create the cubemap image
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
        imageCreateInfo.arrayLayers = 1;
        VK_CALL(vkCreateImage(VulkanDevice->Device, &imageCreateInfo, nullptr, &Output->Image));

        VkMemoryAllocateInfo memAllocInfo = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanDevice->Device, Output->Image, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(VulkanDevice->Device, &memAllocInfo, nullptr, &Output->DeviceMemory));
        VK_CALL(vkBindImageMemory(VulkanDevice->Device, Output->Image, Output->DeviceMemory, 0));    

        {
            VK_CALL(vkBeginCommandBuffer(CommandBuffer, &CommandBufferInfo));
            
            RenderPassBeginInfo.framebuffer = Framebuffer.Framebuffer;
            RenderPassBeginInfo.renderPass = Framebuffer.RenderPass;
            
            vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport Viewport = vulkanTools::BuildViewport((float)Output->Width, (float)Output->Height, 0.0f, 1.0f);
            vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
            VkRect2D Scissor = vulkanTools::BuildRect2D(Output->Width,Output->Height,0,0);
            vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

            VkDeviceSize Offset[1] = {0};
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline); 
            
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Quad.VertexBuffer.Buffer, Offset);
            vkCmdBindIndexBuffer(CommandBuffer, Quad.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(CommandBuffer, 6, 1, 0, 0, 0);
        
            vkCmdEndRenderPass(CommandBuffer);
            
        
            vulkanTools::TransitionImageLayout(
                CommandBuffer,
                Framebuffer._Attachments[0].Image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            VkImageSubresourceRange TexSubresourceRange = {};
            TexSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            TexSubresourceRange.baseMipLevel = 0;
            TexSubresourceRange.levelCount = 1;
            TexSubresourceRange.baseArrayLayer = 0;
            TexSubresourceRange.layerCount = 1;
            vulkanTools::TransitionImageLayout(
                CommandBuffer,
                Output->Image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                TexSubresourceRange);


            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = { 0, 0, 0 };
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = 0;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = 0;
            copyRegion.dstOffset = { 0, 0, 0 };
            copyRegion.extent.width = Output->Width;
            copyRegion.extent.height = Output->Height;
            copyRegion.extent.depth = 1;

        //     // Copy image
            vkCmdCopyImage(
                CommandBuffer,
                Framebuffer._Attachments[0].Image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                Output->Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion);

            // Change image layout of copied face to shader read
            vulkanTools::TransitionImageLayout(
                CommandBuffer,
                Output->Image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                TexSubresourceRange);

            VK_CALL(vkEndCommandBuffer(CommandBuffer));

            VkSubmitInfo SubmitInfo = vulkanTools::BuildSubmitInfo();
            SubmitInfo.commandBufferCount = 1;
            SubmitInfo.pCommandBuffers = &CommandBuffer;
            VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
            VK_CALL(vkQueueWaitIdle(Queue));    
        }

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
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 1;
        view.subresourceRange.levelCount = Output->MipLevels;//Miplevels
        view.image = Output->Image;
        VK_CALL(vkCreateImageView(VulkanDevice->Device, &view, nullptr, &Output->View));
        
        //Cleanup

        vkDestroyPipeline(VulkanDevice->Device, Pipeline, nullptr);
        vkDestroyPipelineLayout(VulkanDevice->Device, PipelineLayout, nullptr);
        vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[0].module, nullptr);
        vkDestroyShaderModule(VulkanDevice->Device, ShaderStages[1].module, nullptr);
        Quad.IndexBuffer.Destroy();
        Quad.VertexBuffer.Destroy();
        Framebuffer.Destroy(VulkanDevice->Device);
        
        Output->Descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Output->Descriptor.imageView = Output->View;
        Output->Descriptor.sampler = Output->Sampler;  
    }
}