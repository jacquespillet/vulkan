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

void pathTraceComputeRenderer::StartPathTrace()
{
    PathTraceFinished=true;
    ShouldPathTrace=true;
    CurrentSampleCount=0;
}

void pathTraceComputeRenderer::Render()
{
    if(ShouldPathTrace)
    {
        ProcessingPreview=false;
        PathTrace();    
    }

    if(CurrentSampleCount >= TotalSamples)
    {
        ShouldPathTrace=false;
        PathTraceFinished=true;
        ProcessingPathTrace=false;
    }

    if(App->Scene->Camera.Changed || App->Scene->Changed)
    {
        ShouldPathTrace=false;
        ProcessingPathTrace=false;
        Preview();    
    }

    VK_CALL(App->VulkanObjects.Swapchain->AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    if(ProcessingPathTrace || PathTraceFinished)
    {
        if(!PathTraceFinished)
        {
            stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
            float seconds = (float)duration.count() / 1000.0f;
            std::cout << seconds << std::endl;  

            PathTraceFinished=true; 
        }
    }

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


        if(ProcessingPathTrace)
        {
            //COMPUTE COMMAND HERE into swapchain image
        }        

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

void pathTraceComputeRenderer::PathTrace()
{
    start = std::chrono::high_resolution_clock::now();
    
    CurrentSampleCount += SamplesPerFrame;
    for(uint32_t y=0; y<App->Height; y+=TileSize)
    {
        for(uint32_t x=(uint32_t)App->Scene->ViewportStart; x<App->Width; x+=TileSize)
        {
            // ThreadPool.EnqueueJob([x, y, this]()
            //     {
            //     PathTraceTile(x, y, TileSize, TileSize, App->Width, App->Height, &Image); 
            //     }
            // );
        }
    }

    ProcessingPathTrace=true;
}

void pathTraceComputeRenderer::Preview()
{
    float StartRatio = App->Scene->ViewportStart / App->Width;
    uint32_t StartPreview = (uint32_t)((float)StartRatio * (float)previewWidth);
    
    uint32_t RenderWidth = previewWidth - StartPreview;
    uint32_t RenderHeight = previewHeight;
    for(uint32_t y=0; y<previewHeight; y+=TileSize)
    {
        for(uint32_t x=StartPreview; x<previewWidth; x+=TileSize)
        {   
            // ThreadPool.EnqueueJob([x, y, RenderWidth, RenderHeight, this]()
            // {
            //    PreviewTile(x, y, 
            //                TileSize, TileSize, 
            //                previewWidth, previewHeight, 
            //                RenderWidth,  RenderHeight, 
            //                &PreviewImage); 
            // });
        }
    }

    ProcessingPreview=true;
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
    previewWidth = App->Width / 10;
    previewHeight = App->Height / 10;

    SetupDescriptorPool();
    Resources.Init(VulkanDevice, VulkanObjects.DescriptorPool, App->VulkanObjects.TextureLoader);
    

    CreateCommandBuffers();

    VulkanObjects.previewImage.Create(VulkanDevice, App->VulkanObjects.CommandPool, App->VulkanObjects.Queue, VK_FORMAT_B8G8R8A8_UNORM, {previewWidth, previewHeight, 1});

    
    for(size_t i=0; i<App->Scene->Meshes.size(); i++)
    {
        Meshes.push_back(new mesh(App->Scene->Meshes[i].Indices,
                    App->Scene->Meshes[i].Vertices));
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
            (uint32_t)Meshes[i]->Triangles.size(),
            RunningIndicesCount,
            (uint32_t)Meshes[i]->BVH->TriangleIndices.size(),
            RunningBVHNodeCount,
            (uint32_t)Meshes[i]->BVH->BVHNodes.size()
        };

        RunningTriangleCount += (uint32_t)Meshes[i]->Triangles.size();
        RunningIndicesCount += (uint32_t)Meshes[i]->BVH->TriangleIndices.size();
        RunningBVHNodeCount += (uint32_t)Meshes[i]->BVH->NodesUsed;
    }

    

    //Build a big buffer with all the meshes triangle data
    //Build another buffer with info, for each mesh :
    //  Struct indexData:
    //      triangleDataStartInx
    //      TriangleDataCount
    //      IndicesDataStartInx
    //      IndicesDataCount
    //      BVHNodeDataStartInx
    //      BVHNodeDataCount

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.IndexDataBuffer, IndexData.size() * sizeof(indexData), IndexData.data());

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.TriangleBuffer, AllTriangles.size() * sizeof(triangle), AllTriangles.data());
    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.TriangleExBuffer, AllTrianglesEx.size() * sizeof(triangleExtraData), AllTrianglesEx.data());
    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.BVHBuffer, AllBVHNodes.size() * sizeof(bvhNode), AllBVHNodes.data());
    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.IndicesBuffer, AllTriangleIndices.size() * sizeof(uint32_t), AllTriangleIndices.data());

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.TLASInstancesBuffer, TLAS.BLAS->size() * sizeof(bvhInstance), TLAS.BLAS->data());

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                              &VulkanObjects.TLASNodesBuffer, TLAS.Nodes.size() * sizeof(tlasNode), TLAS.Nodes.data());

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
			descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.previewImage.Descriptor, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TriangleBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.TriangleExBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.BVHBuffer.VulkanObjects.Descriptor, true),
            descriptor(VK_SHADER_STAGE_COMPUTE_BIT, VulkanObjects.IndicesBuffer.VulkanObjects.Descriptor, true)
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
        
        vkCmdDispatch(Compute.CommandBuffer, App->Width / 16, App->Height / 16, 1);

        VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer],
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);

        VkImageBlit BlitRegion = {};
        BlitRegion.srcOffsets[0] = {0,0,0};
        BlitRegion.srcOffsets[1] = {(int)previewWidth, (int)previewHeight, 1};
        BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        BlitRegion.srcSubresource.mipLevel = 0;
        BlitRegion.srcSubresource.baseArrayLayer = 0;
        BlitRegion.srcSubresource.layerCount=1;
        BlitRegion.dstOffsets[0] = {0,0,0};
        BlitRegion.dstOffsets[1] = {(int)App->Width, (int)App->Height, 1};
        BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        BlitRegion.dstSubresource.mipLevel = 0;
        BlitRegion.dstSubresource.baseArrayLayer = 0;
        BlitRegion.dstSubresource.layerCount=1;
        
        vkCmdBlitImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                        App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                        1, &BlitRegion, VK_FILTER_NEAREST);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SubresourceRange);
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, SubresourceRange);
    }
    VK_CALL(vkEndCommandBuffer(Compute.CommandBuffer));
}

//

void pathTraceComputeRenderer::UpdateTLAS(uint32_t InstanceIndex)
{
    Instances[InstanceIndex].SetTransform(App->Scene->InstancesPointers[InstanceIndex]->InstanceData.Transform);
    TLAS.Build();
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
    if(ImGui::Button("PathTrace"))
    {
        StartPathTrace();
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
    
    
    VulkanObjects.previewImage.Destroy();
    
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);

}


