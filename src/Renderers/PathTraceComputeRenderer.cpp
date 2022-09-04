#include "PathTraceComputeRenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>

#include "../Swapchain.h"
#include "../ImGuiHelper.h"
#include <iostream>


////////////////////////////////////////////////////////////////////////////////////////

pathTraceComputeRenderer::pathTraceComputeRenderer(vulkanApp *App) : renderer(App) {
    this->UseGizmo=true;
}

void pathTraceComputeRenderer::Render()
{

    if(App->Scene->Camera.Changed)
    {
        ResetAccumulation=true;
        UniformData.ShouldAccumulate=1;
    }

    if(ResetAccumulation)
    {
        UniformData.CurrentSampleCount=0;
        ResetAccumulation=false;
    }  

    if(UniformData.CurrentSampleCount < (uint32_t)UniformData.MaxSamples)
    {
        UniformData.CurrentSampleCount += UniformData.SamplersPerFrame;
    }
    UpdateUniformBuffers();
    
    VK_CALL(App->VulkanObjects.Swapchain->AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    //Fill command buffer
    {
        VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
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
        
        RenderPassBeginInfo.framebuffer = App->VulkanObjects.AppFramebuffers[App->VulkanObjects.CurrentBuffer];
        VK_CALL(vkBeginCommandBuffer(VulkanObjects.DrawCommandBuffer, &CommandBufferInfo));

        // if(ProcessingPreview)
        {
            FillCommandBuffer();
            vkWaitForFences(VulkanDevice->Device, 1, &Compute.Fence, VK_TRUE, UINT64_MAX);
            vkResetFences(VulkanDevice->Device, 1, &Compute.Fence);    
            
            VkPipelineStageFlags SubmitPipelineStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            VkSubmitInfo ComputeSubmitInfo = vulkanTools::BuildSubmitInfo();
            ComputeSubmitInfo.commandBufferCount = 1;
            ComputeSubmitInfo.pCommandBuffers = &Compute.CommandBuffer;
            ComputeSubmitInfo.waitSemaphoreCount=1;
            ComputeSubmitInfo.pWaitSemaphores = &App->VulkanObjects.Semaphores.PresentComplete;
            ComputeSubmitInfo.pWaitDstStageMask = &SubmitPipelineStages;
            ComputeSubmitInfo.signalSemaphoreCount = 1;
            ComputeSubmitInfo.pSignalSemaphores = &VulkanObjects.PreviewSemaphore;

            VK_CALL(vkQueueSubmit(Compute.Queue, 1, &ComputeSubmitInfo, Compute.Fence));  
        }


        vkCmdBeginRenderPass(VulkanObjects.DrawCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        App->ImGuiHelper->DrawFrame(VulkanObjects.DrawCommandBuffer);
        vkCmdEndRenderPass(VulkanObjects.DrawCommandBuffer);
        VK_CALL(vkEndCommandBuffer(VulkanObjects.DrawCommandBuffer));
    }

    
    VulkanObjects.SubmitInfo = vulkanTools::BuildSubmitInfo();
    VulkanObjects.SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    VulkanObjects.SubmitInfo.waitSemaphoreCount = 1;
    VulkanObjects.SubmitInfo.signalSemaphoreCount=1;
    VulkanObjects.SubmitInfo.pWaitSemaphores = &VulkanObjects.PreviewSemaphore;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &App->VulkanObjects.Semaphores.RenderComplete;
    VulkanObjects.SubmitInfo.commandBufferCount=1;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.DrawCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->VulkanObjects.Swapchain->QueuePresent(App->VulkanObjects.Queue, App->VulkanObjects.CurrentBuffer, App->VulkanObjects.Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));

    
}



void pathTraceComputeRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &VulkanObjects.DescriptorPool));    
}


void pathTraceComputeRenderer::Setup()
{
    previewWidth = App->Width;
    previewHeight = App->Height;

    SetupDescriptorPool();
    Resources.Init(VulkanDevice, VulkanObjects.DescriptorPool, App->VulkanObjects.TextureLoader);
    

    CreateCommandBuffers();

    
    VulkanObjects.FinalImage.Create(VulkanDevice, App->VulkanObjects.CommandPool, App->VulkanObjects.Queue, App->VulkanObjects.Swapchain->ColorFormat, {previewWidth, previewHeight, 1});    
    VulkanObjects.AccumulationImage.Create(VulkanDevice, App->VulkanObjects.CommandPool, App->VulkanObjects.Queue, VK_FORMAT_R32G32B32A32_SFLOAT, {previewWidth, previewHeight, 1});

    
    for(size_t i=0; i<App->Scene->Meshes.size(); i++)
    {
        Meshes.push_back(new mesh(App->Scene->Meshes[i].Indices,
                                  App->Scene->Meshes[i].Vertices,
                                  App->Scene->Meshes[i].MaterialIndex));
    }
    for(size_t i=0; i<App->Scene->InstancesPointers.size(); i++)
    {
        Instances.push_back(
            bvhInstance(&Meshes, App->Scene->InstancesPointers[i]->MeshIndex,
                        App->Scene->InstancesPointers[i]->InstanceData.Transform, (uint32_t)i)
        );
    }
    
    TLAS = tlas(&Instances);
    TLAS.Build();

    uint64_t TotalTriangleCount=0;
    uint64_t TotalIndicesCount=0;
    uint64_t TotalBVHNodes=0;
    for(int i=0; i<Meshes.size(); i++)
    {
        TotalTriangleCount += Meshes[i]->Triangles.size();
        TotalIndicesCount += Meshes[i]->BVH->TriangleIndices.size();
        TotalBVHNodes += Meshes[i]->BVH->NodesUsed;
    }
    std::vector<triangle> AllTriangles(TotalTriangleCount);
    std::vector<triangleExtraData> AllTrianglesEx(TotalTriangleCount);
    std::vector<uint32_t> AllTriangleIndices(TotalIndicesCount);
    std::vector<bvhNode> AllBVHNodes(TotalBVHNodes);
    IndexData.resize(Meshes.size());

    uint32_t RunningTriangleCount=0;
    uint32_t RunningIndicesCount=0;
    uint32_t RunningBVHNodeCount=0;
    for(int i=0; i<Meshes.size(); i++)
    {
        memcpy((void*)(AllTriangles.data() + RunningTriangleCount), Meshes[i]->Triangles.data(), Meshes[i]->Triangles.size() * sizeof(triangle));
        memcpy((void*)(AllTrianglesEx.data() + RunningTriangleCount), Meshes[i]->TrianglesExtraData.data(), Meshes[i]->TrianglesExtraData.size() * sizeof(triangleExtraData));
        memcpy((void*)(AllTriangleIndices.data() + RunningIndicesCount), Meshes[i]->BVH->TriangleIndices.data(), Meshes[i]->BVH->TriangleIndices.size() * sizeof(uint32_t));
        memcpy((void*)(AllBVHNodes.data() + RunningBVHNodeCount), Meshes[i]->BVH->BVHNodes.data(), Meshes[i]->BVH->NodesUsed * sizeof(bvhNode));

        IndexData[i] = 
        {
            RunningTriangleCount,
            RunningIndicesCount,
            RunningBVHNodeCount,
            Meshes[i]->MaterialIndex
        };

        RunningTriangleCount += (uint32_t)Meshes[i]->Triangles.size();
        RunningIndicesCount += (uint32_t)Meshes[i]->BVH->TriangleIndices.size();
        RunningBVHNodeCount += (uint32_t)Meshes[i]->BVH->NodesUsed;
    }

    std::vector<materialData> AllMaterials(App->Scene->Materials.size());
    for(int i=0; i<App->Scene->Materials.size(); i++)
    {
        AllMaterials[i] = App->Scene->Materials[i].MaterialData;
    }

    VulkanObjects.CommandPool = vulkanTools::CreateCommandPool(VulkanDevice->Device, App->VulkanObjects.Swapchain->QueueNodeIndex);
    VulkanObjects.CopyCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    
    vulkanTools::CreateAndFillBuffer(VulkanDevice, AllTriangles.data(), AllTriangles.size() * sizeof(triangle), &VulkanObjects.TriangleBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    vulkanTools::CreateAndFillBuffer(VulkanDevice, AllTrianglesEx.data(), AllTrianglesEx.size() * sizeof(triangleExtraData), &VulkanObjects.TriangleExBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    vulkanTools::CreateAndFillBuffer(VulkanDevice, AllBVHNodes.data(), AllBVHNodes.size() * sizeof(bvhNode), &VulkanObjects.BVHBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    vulkanTools::CreateAndFillBuffer(VulkanDevice, AllTriangleIndices.data(), AllTriangleIndices.size() * sizeof(uint32_t), &VulkanObjects.IndicesBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    vulkanTools::CreateAndFillBuffer(VulkanDevice, IndexData.data(), IndexData.size() * sizeof(indexData), &VulkanObjects.IndexDataBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);

    vulkanTools::CreateAndFillBuffer(VulkanDevice, TLAS.BLAS->data(), TLAS.BLAS->size() * sizeof(bvhInstance), &VulkanObjects.TLASInstancesBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    VK_CALL(vulkanTools::CreateBuffer(VulkanDevice,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &VulkanObjects.TLASInstancesStagingBuffer, TLAS.BLAS->size() * sizeof(bvhInstance), nullptr));
    vulkanTools::CreateAndFillBuffer(VulkanDevice, TLAS.Nodes.data(), TLAS.Nodes.size() * sizeof(tlasNode), &VulkanObjects.TLASNodesBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    VK_CALL(vulkanTools::CreateBuffer(VulkanDevice,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &VulkanObjects.TLASNodesStagingBuffer, TLAS.Nodes.size() * sizeof(tlasNode), nullptr));

    vulkanTools::CreateAndFillBuffer(VulkanDevice, AllMaterials.data(), AllMaterials.size() * sizeof(materialData), &VulkanObjects.MaterialBuffer, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VulkanObjects.CopyCommand, App->VulkanObjects.Queue);
    VK_CALL(vulkanTools::CreateBuffer(VulkanDevice,  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &VulkanObjects.MaterialStagingBuffer,AllMaterials.size() * sizeof(materialData), nullptr));

    
    std::vector<VkDescriptorImageInfo> ImageInfos(App->Scene->Resources.Textures->Resources.size()); 
    for(auto &Texture : App->Scene->Resources.Textures->Resources)
    {
		ImageInfos[Texture.second.Index] = Texture.second.Descriptor;
    }

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice, 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &VulkanObjects.UBO,
        sizeof(uniformData),
        &UniformData
    ));
    VK_CALL(VulkanObjects.UBO.Map());
    UpdateUniformBuffers();    

    
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = NULL;
    queueCreateInfo.queueFamilyIndex = VulkanDevice->QueueFamilyIndices.Compute;
    queueCreateInfo.queueCount = 1;
    vkGetDeviceQueue(VulkanDevice->Device, VulkanDevice->QueueFamilyIndices.Compute, 0, &Compute.Queue); 

    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(VulkanDevice->Device, &SemaphoreCreateInfo, nullptr, &VulkanObjects.PreviewSemaphore));    

	//Preview descriptor set
	{
		std::vector<descriptor> Descriptors =
		{
			descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.FinalImage.Descriptor, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TriangleBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TriangleExBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.BVHBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.IndicesBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.IndexDataBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TLASInstancesBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TLASNodesBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.MaterialBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, ImageInfos),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.UBO.VulkanObjects.Descriptor),
			descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.AccumulationImage.Descriptor, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
		};

		std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts =
		{
			App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
		};
		Resources.AddDescriptorSet(VulkanDevice, "Shadows", Descriptors, VulkanObjects.DescriptorPool, AdditionalDescriptorSetLayouts);
	}

    //Preview pipeline
    {
        VkComputePipelineCreateInfo ComputePipelineCreateInfo = vulkanTools::BuildComputePipelineCreateInfo(Resources.PipelineLayouts->Get("Shadows"), 0);

		ComputePipelineCreateInfo.stage = LoadShader(VulkanDevice->Device, "resources/shaders/spv/pathTracePreview.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CALL(vkCreateComputePipelines(VulkanDevice->Device, nullptr, 1, &ComputePipelineCreateInfo, nullptr, &VulkanObjects.previewPipeline));
    }

}

void pathTraceComputeRenderer::UpdateUniformBuffers()
{
    UniformData.VertexSize = sizeof(vertex);
    memcpy(VulkanObjects.UBO.VulkanObjects.Mapped, &UniformData, sizeof(UniformData));
}   


void pathTraceComputeRenderer::FillCommandBuffer()
{
    VkCommandBufferBeginInfo ComputeCommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(Compute.CommandBuffer, &ComputeCommandBufferBeginInfo));

    //Ray traced shadows
    {
        VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");

        vkCmdBindPipeline(Compute.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, VulkanObjects.previewPipeline);
        vkCmdBindDescriptorSets(Compute.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Resources.PipelineLayouts->Get("Shadows"), 0, 1, Resources.DescriptorSets->GetPtr("Shadows"), 0, 0);
        vkCmdBindDescriptorSets(Compute.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Resources.PipelineLayouts->Get("Shadows"), 1, 1, &RendererDescriptorSet, 0, nullptr);			
        
        
        vkCmdDispatch(Compute.CommandBuffer, 
                      (App->Width - (int)App->Scene->ViewportStart) / 16, 
                      App->Height / 16, 
                      1);

        VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer],
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.FinalImage.Image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);

        VkImageBlit BlitRegion = {};
        BlitRegion.srcOffsets[0] = {0,0,0};
        BlitRegion.srcOffsets[1] = {(int)App->Width - (int)App->Scene->ViewportStart, (int)previewHeight, 1};
        BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        BlitRegion.srcSubresource.mipLevel = 0;
        BlitRegion.srcSubresource.baseArrayLayer = 0;
        BlitRegion.srcSubresource.layerCount=1;
        BlitRegion.dstOffsets[0] = {(int)App->Scene->ViewportStart,0,0};
        BlitRegion.dstOffsets[1] = {(int)App->Width, (int)App->Height, 1};
        BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        BlitRegion.dstSubresource.mipLevel = 0;
        BlitRegion.dstSubresource.baseArrayLayer = 0;
        BlitRegion.dstSubresource.layerCount=1;
        
        vkCmdBlitImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.FinalImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                        App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                        1, &BlitRegion, VK_FILTER_NEAREST);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SubresourceRange);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.FinalImage.Image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, SubresourceRange);
    }
    VK_CALL(vkEndCommandBuffer(Compute.CommandBuffer));
}

//

void pathTraceComputeRenderer::UpdateTLAS(uint32_t InstanceIndex)
{
    Instances[InstanceIndex].SetTransform(App->Scene->InstancesPointers[InstanceIndex]->InstanceData.Transform);
    TLAS.Build();

    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(VulkanObjects.CopyCommand, &CommandBufferInfo));
    {
        VulkanObjects.TLASInstancesStagingBuffer.Map();
        VulkanObjects.TLASInstancesStagingBuffer.CopyTo(TLAS.BLAS->data(), TLAS.BLAS->size() * sizeof(bvhInstance));
        VulkanObjects.TLASInstancesStagingBuffer.Unmap();
        
        VkBufferCopy BufferCopy {};
        BufferCopy.size = TLAS.BLAS->size() * sizeof(bvhInstance);

        vkCmdCopyBuffer(VulkanObjects.CopyCommand, VulkanObjects.TLASInstancesStagingBuffer.VulkanObjects.Buffer, VulkanObjects.TLASInstancesBuffer.VulkanObjects.Buffer, 1, &BufferCopy);
    }
    
    {
        VulkanObjects.TLASNodesStagingBuffer.Map();
        VulkanObjects.TLASNodesStagingBuffer.CopyTo(TLAS.Nodes.data(), TLAS.Nodes.size() * sizeof(tlasNode));
        VulkanObjects.TLASNodesStagingBuffer.Unmap();
        
        VkBufferCopy BufferCopy {};
        BufferCopy.size = TLAS.Nodes.size() * sizeof(tlasNode);

        vkCmdCopyBuffer(VulkanObjects.CopyCommand, VulkanObjects.TLASNodesStagingBuffer.VulkanObjects.Buffer, VulkanObjects.TLASNodesBuffer.VulkanObjects.Buffer, 1, &BufferCopy);
    }

    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, VulkanObjects.CopyCommand, App->VulkanObjects.Queue, false);

    ResetAccumulation=true;
}
   
void pathTraceComputeRenderer::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &VulkanObjects.DrawCommandBuffer));

    VkCommandPoolCreateInfo ComputeCommandPoolCreateInfo = {};
    ComputeCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ComputeCommandPoolCreateInfo.queueFamilyIndex = VulkanDevice->QueueFamilyIndices.Compute;
    ComputeCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CALL(vkCreateCommandPool(VulkanDevice->Device, &ComputeCommandPoolCreateInfo, nullptr, &Compute.CommandPool));

    VkCommandBufferAllocateInfo CommandBufferAlllocateInfo =
        vulkanTools::BuildCommandBufferAllocateInfo(
            Compute.CommandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1);

    VK_CALL(vkAllocateCommandBuffers(VulkanDevice->Device, &CommandBufferAlllocateInfo, &Compute.CommandBuffer));

    // Fence for compute CB sync
    VkFenceCreateInfo FenceCreateInfo = vulkanTools::BuildFenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CALL(vkCreateFence(VulkanDevice->Device, &FenceCreateInfo, nullptr, &Compute.Fence));    
}

void pathTraceComputeRenderer::RenderGUI()
{
    if(ImGui::CollapsingHeader("Path Tracing Options"))
    {
        bool ShouldReset=false;
        ShouldReset |= ImGui::SliderInt("Samples Per Pixel", &UniformData.SamplersPerFrame, 1, 32);
        ShouldReset |= ImGui::SliderInt("Ray Bounces", &UniformData.RayBounces, 1, 32);
        ShouldReset |= ImGui::SliderInt("Max Samples", &UniformData.MaxSamples, 1, 16384);
        ImGui::Text("Num Samples %d", UniformData.CurrentSampleCount);

        // if(ImGui::Button("Denoise"))
        // {
        //     Denoise();
        // }
        if(ShouldReset)
        {
            ResetAccumulation=true;
        }
    }
}

void pathTraceComputeRenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void pathTraceComputeRenderer::Destroy()
{
    for(int i=0; i<Meshes.size(); i++)
    {
        delete Meshes[i]->BVH;
        delete Meshes[i];
    }
    
    
    VulkanObjects.FinalImage.Destroy();
    
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);

}

void pathTraceComputeRenderer::UpdateMaterial(size_t Index)
{
    VulkanObjects.MaterialStagingBuffer.Map();
    VulkanObjects.MaterialStagingBuffer.CopyTo(&App->Scene->Materials[Index].MaterialData, sizeof(materialData));
    VulkanObjects.MaterialStagingBuffer.Unmap();
    
    VkBufferCopy BufferCopy {};
    BufferCopy.size = sizeof(materialData);
    BufferCopy.dstOffset = Index * sizeof(materialData);


    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(VulkanObjects.CopyCommand, &CommandBufferInfo));
    vkCmdCopyBuffer(VulkanObjects.CopyCommand, VulkanObjects.MaterialStagingBuffer.VulkanObjects.Buffer, VulkanObjects.MaterialBuffer.VulkanObjects.Buffer, 1, &BufferCopy);
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->VulkanObjects.CommandPool, VulkanObjects.CopyCommand, App->VulkanObjects.Queue, false);
}



