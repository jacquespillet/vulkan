#include "rasterizerRenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>
#include "../Swapchain.h"
#include "../ImGuiHelper.h"


rasterizerRenderer::rasterizerRenderer(vulkanApp *App) : renderer(App) {
    this->UseGizmo=false;
}


void rasterizerRenderer::Render()
{
    t+=0.1f;

    for(uint32_t i=0; i<Image.size(); i++)
    {
        float r = cos(t) * 0.5f + 0.5f;
        uint8_t r8 = (uint8_t)(r * 255.0f);
        Image[i] = {0, 0, r8, 255};
    }

    VulkanObjects.ImageStagingBuffer.Map();
    VulkanObjects.ImageStagingBuffer.CopyTo(Image.data(), Image.size() * sizeof(rgba8));
    VulkanObjects.ImageStagingBuffer.Unmap();


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

        VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], 
                                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

        {
            VkBufferImageCopy Region = {};
            Region.imageExtent.depth=1;
            Region.imageExtent.width = App->Width;
            Region.imageExtent.height = App->Height;
            Region.imageOffset = {0,0,0};
            Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Region.imageSubresource.baseArrayLayer=0;
            Region.imageSubresource.layerCount=1;
            Region.imageSubresource.mipLevel=0;
            Region.bufferOffset=0;
            Region.bufferRowLength=0;

            vkCmdCopyBufferToImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.ImageStagingBuffer.VulkanObjects.Buffer, 
                                    App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);
        }        

        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], 
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SubresourceRange);


        vkCmdBeginRenderPass(VulkanObjects.DrawCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        App->ImGuiHelper->DrawFrame(VulkanObjects.DrawCommandBuffer);
        vkCmdEndRenderPass(VulkanObjects.DrawCommandBuffer);
        VK_CALL(vkEndCommandBuffer(VulkanObjects.DrawCommandBuffer));
    }

    
    VulkanObjects.SubmitInfo = vulkanTools::BuildSubmitInfo();
    VulkanObjects.SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    VulkanObjects.SubmitInfo.waitSemaphoreCount = 1;
    VulkanObjects.SubmitInfo.signalSemaphoreCount=1;
    VulkanObjects.SubmitInfo.pWaitSemaphores = &App->VulkanObjects.Semaphores.PresentComplete;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &App->VulkanObjects.Semaphores.RenderComplete;
    VulkanObjects.SubmitInfo.commandBufferCount=1;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.DrawCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->VulkanObjects.Swapchain->QueuePresent(App->VulkanObjects.Queue, App->VulkanObjects.CurrentBuffer, App->VulkanObjects.Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));
}


void rasterizerRenderer::Setup()
{
    previewWidth = App->Width / 10;
    previewHeight = App->Height / 10;

    CreateCommandBuffers();

    Image.resize(App->Width * App->Height, {0, 0, 0, 255});
    AccumulationImage.resize(App->Width * App->Height, glm::vec3(0));
    PreviewImage.resize(previewWidth * previewHeight);
    

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
                              &VulkanObjects.ImageStagingBuffer, Image.size() * sizeof(rgba8), Image.data());
    
    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
                              &VulkanObjects.previewBuffer, PreviewImage.size() * sizeof(rgba8), PreviewImage.data());    
    VulkanObjects.previewImage.Create(VulkanDevice, App->VulkanObjects.CommandPool, App->VulkanObjects.Queue, VK_FORMAT_B8G8R8A8_UNORM, {previewWidth, previewHeight, 1});
}

//


void rasterizerRenderer::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &VulkanObjects.DrawCommandBuffer));
}

void rasterizerRenderer::RenderGUI()
{
}

void rasterizerRenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void rasterizerRenderer::Destroy()
{
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);
}
