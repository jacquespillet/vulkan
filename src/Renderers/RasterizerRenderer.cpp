#include "rasterizerRenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>
#include "../Swapchain.h"
#include "../ImGuiHelper.h"
#include <random> 
glm::vec3 rasterizerRenderer::CalculateBarycentric(glm::ivec2 p0, glm::ivec2 p1,glm::ivec2 p2, int x, int y)
{
    glm::vec3 u = glm::cross(
        glm::vec3(p2[0]-p0[0], p1[0]-p0[0], p0[0]-x),
        glm::vec3(p2[1]-p0[1], p1[1]-p0[1], p0[1]-y));
    /* `pts` and `P` has integer value as coordinates
        so `abs(u[2])` < 1 means `u[2]` is 0, that means
        triangle is degenerate, in this case return something with negative coordinates */
    if (std::abs(u.z)<1) return glm::vec3(-1,1,1);
    return glm::vec3(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z); 
}

rasterizerRenderer::rasterizerRenderer(vulkanApp *App) : renderer(App) {
    this->UseGizmo=false;
}

void rasterizerRenderer::SetPixel(int x, int y, rgba8 Color)
{
    if(x > App->Width-1 || y > App->Height-1 || x < 0 || y < 0)return;
    Image[y * App->Width + x] = Color;
}

void rasterizerRenderer::DrawTriangle(glm::ivec2 p0, glm::ivec2 p1,glm::ivec2 p2, rgba8 Color)
{
    glm::ivec2 BBMin(App->Height-1, App->Width-1);
    glm::ivec2 BBMax(0, 0);
    
    BBMin = glm::min(BBMin, p0); BBMin = glm::min(BBMin, p1); BBMin = glm::min(BBMin, p2);
    BBMax = glm::max(BBMax, p0); BBMax = glm::max(BBMax, p1); BBMax = glm::max(BBMax, p2);
    
    
    for(int x=BBMin.x; x <= BBMax.x; x++)
    {
        for(int y=BBMin.y; y <= BBMax.y; y++)
        {
            glm::vec3 BaryCentric = CalculateBarycentric(p0, p1, p2, x, y);
            if(BaryCentric.x < 0 || BaryCentric.y < 0 || BaryCentric.z < 0) continue;
            SetPixel(x, y, Color);
        }
    }

}

void rasterizerRenderer::DrawLine(glm::ivec2 p0, glm::ivec2 p1, rgba8 Color)
{
#if 0
    bool Steep=false;
    if(std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y))
    {
        std::swap(p0, p1);
        Steep=true;
    }
    if(p0.x > p1.x)
    {
        std::swap(p0, p1);
    }

    int dx = p1.x - p0.x;
    int dy = p1.y - p0.y;
    
    int DError = std::abs(dy) * 2;
    int Error=0;
    int y=p0.y;
    
    for(int x=p0.x; x <= p1.x; x++)
    {
        if(Steep)SetPixel(y, x, Color);
        else     SetPixel(x, y, Color);

        Error += DError;
        if(Error > dx)
        {
            y += (p1.y > p0.y) ? 1 : -1;
            Error -= dx * 2;
        }
    }
#else
    bool steep = false; 
    if (std::abs(p0.x-p1.x)<std::abs(p0.y-p1.y)) { 
        std::swap(p0.x, p0.y); 
        std::swap(p1.x, p1.y); 
        steep = true; 
    } 
    if (p0.x>p1.x) { 
        std::swap(p0, p1);
    } 
    glm::ivec2 d = p1 - p0;

    int derror2 = std::abs(d.y)*2; 
    int error2 = 0; 
    int y = p0.y; 
    for (int x=p0.x; x<=p1.x; x++) { 
        if (steep) { 
            SetPixel(y, x, Color); 
        } else { 
            SetPixel(x, y, Color); 
        } 
        error2 += derror2; 
        if (error2 > d.x) { 
            y += (p1.y>p0.y?1:-1); 
            error2 -= d.x*2; 
        } 
    } 

#endif
}

void rasterizerRenderer::Rasterize()
{
    for(uint32_t i=0; i<Image.size(); i++)
    {
        Image[i] = {0, 0, 0, 255};
    }
    glm::vec3 LightDir = glm::normalize(glm::vec3(1,1,1));
    std::vector<vertex> *Vertices = &App->Scene->Meshes[0].Vertices;
    std::vector<uint32_t> *Indices = &App->Scene->Meshes[0].Indices;
    glm::mat4 ViewProjectionMatrix = App->Scene->Camera.GetProjectionMatrix() * App->Scene->Camera.GetViewMatrix();
    for(size_t i=0; i<Indices->size(); i+=3)
    {
        uint32_t i0 = Indices->at(i+0);
        uint32_t i1 = Indices->at(i+1);
        uint32_t i2 = Indices->at(i+2);
        glm::vec4 v0 = Vertices->at(i0).Position * 0.005;
        glm::vec4 v1 = Vertices->at(i1).Position * 0.005;
        glm::vec4 v2 = Vertices->at(i2).Position * 0.005;
        v0.w=1; v1.w = 1; v2.w=1;

        glm::vec4 pv0 = ViewProjectionMatrix * v0;
        pv0 /= pv0.w; pv0 = (pv0 + 1.0f) / 2.0f;
        glm::vec4 pv1 = ViewProjectionMatrix * v1;
        pv1 /= pv1.w; pv1 = (pv1 + 1.0f) / 2.0f;
        glm::vec4 pv2 = ViewProjectionMatrix * v2;
        pv2 /= pv2.w; pv2 = (pv2 + 1.0f) / 2.0f;
        
        glm::ivec2 p0 = pv0 * App->Width;        
        glm::ivec2 p1 = pv1 * App->Width;        
        glm::ivec2 p2 = pv2 * App->Width;        
        
        glm::vec3 Normal = glm::normalize(glm::cross(glm::vec3(v2 - v0), glm::vec3(v1 - v0)));
        float Intensity = glm::dot(Normal, LightDir);
        if(Intensity > 0)
        {
            DrawTriangle(p0, p1, p2, {(uint8_t)(Intensity * 255.0f), (uint8_t)(Intensity * 255.0f), (uint8_t)(Intensity * 255.0f), 255});
        }
        // DrawTriangle(p0, p1, p2, {(uint8_t)(std::rand()%255), (uint8_t)(std::rand()%255), (uint8_t)(std::rand()%255), 255});

    }
    DrawLine(glm::ivec2(600, 500), glm::ivec2(650, 1000), {0, 255, 0, 255});
}


void rasterizerRenderer::Render()
{

    Rasterize();

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
