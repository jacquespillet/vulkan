#include "rasterizerRenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>
#include "../Swapchain.h"
#include "../ImGuiHelper.h"
#include <random> 


glm::vec4 gouraudShader::VertexShader(uint32_t Index, uint8_t TriVert) 
{
    uint32_t VertexIndex = Buffers.Indices->at(Index);
    vertex *Vertex = &Buffers.Vertices->at(VertexIndex);
    glm::vec4 Position = glm::vec4(Vertex->Position.x,Vertex->Position.y, Vertex->Position.z, 1.0f);

    glm::vec4 OutPosition = (*Uniforms.ViewProjectionMatrix) * (*Uniforms.ModelMatrix) * Position;
    OutPosition /= OutPosition.w; 
    OutPosition = (OutPosition + 1.0f) / 2.0f;
    OutPosition *= glm::vec4(Framebuffer.ViewportWidth, Framebuffer.ViewportHeight, 1, 0);
    OutPosition.x += Framebuffer.ViewportStartX;
    OutPosition.y += Framebuffer.ViewportStartY;

    Varyings[TriVert].Intensity = std::max(0.0f, 
                                           glm::dot(
                                                glm::vec3(Vertex->Normal), glm::normalize(glm::vec3(1,1,1))
                                           )
                                           );

    return OutPosition;
}

bool gouraudShader::FragmentShader(glm::vec3 Barycentric, rgba8 &ColorOut) 
{
    glm::vec3 VaryingIntensity(Varyings[0].Intensity, Varyings[1].Intensity, Varyings[2].Intensity);
    float Intensity = glm::dot(VaryingIntensity, Barycentric);
    ColorOut = {(uint8_t)(Intensity * 255.0f), (uint8_t)(Intensity * 255.0f), (uint8_t)(Intensity * 255.0f), 255};
    return false;
}


float renderTarget::SampleDepth(int x, int y)
{
    // x -= ViewportStartX;
    // y -= ViewportStartY;
	if (x > (int)(Width - 1) || y > (int)(Height - 1) || x < 0 || y < 0)return 0;
    return (*Depth)[y * Width + x];
}

void renderTarget::SetDepthPixel(int x, int y, float DepthValue)
{
    // x -= ViewportStartX;
    // y -= ViewportStartY;
	if (x > (int)(Width - 1) || y > (int)(Height - 1) || x < 0 || y < 0)return;
    (*Depth)[y * Width + x] = DepthValue;
}
glm::vec3 rasterizerRenderer::CalculateBarycentric(glm::vec3 A, glm::vec3 B,glm::vec3 C, glm::vec3 P)
{
    glm::vec3 s[2];
    s[0][0] = C[0]-A[0];
    s[0][1] = B[0]-A[0];
    s[0][2] = A[0]-P[0];
    
    s[1][0] = C[1]-A[1];
    s[1][1] = B[1]-A[1];
    s[1][2] = A[1]-P[1];

    glm::vec3 u = cross(s[0], s[1]);
    if (std::abs(u[2])>1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
        return glm::vec3(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return glm::vec3(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator    
}

rasterizerRenderer::rasterizerRenderer(vulkanApp *App) : renderer(App) {
    this->UseGizmo=false;
}

void renderTarget::SetPixel(int x, int y, rgba8 ColorValue)
{
    // x += ViewportStartX;
    // y += ViewportStartY;
    
    if(x > (int)(Width-1) || y > (int)(Height-1) || x < 0 || y < 0)return;
    (*Color)[y * Width + x] = ColorValue;
}

void rasterizerRenderer::DrawTriangle(glm::vec3 p0, glm::vec3 p1,glm::vec3 p2, shader &RenderShader)
{
	p0.x = std::floor(p0.x); p0.y = std::floor(p0.y);
	p1.x = std::floor(p1.x); p1.y = std::floor(p1.y);
	p2.x = std::floor(p2.x); p2.y = std::floor(p2.y);

    glm::vec3 BBMin(RenderShader.Framebuffer.ViewportWidth-1, RenderShader.Framebuffer.ViewportHeight-1,0 );
    glm::vec3 BBMax(0, 0,0 );
    BBMin = glm::min(BBMin, p0); BBMin = glm::min(BBMin, p1); BBMin = glm::min(BBMin, p2);
    BBMax = glm::max(BBMax, p0); BBMax = glm::max(BBMax, p1); BBMax = glm::max(BBMax, p2);
    

    glm::vec3 P;
    for(P.x=BBMin.x; P.x <= BBMax.x; P.x++)
    {
        for(P.y=BBMin.y; P.y <= BBMax.y; P.y++)
        {
            glm::vec3 BaryCentric = CalculateBarycentric(p0, p1, p2, P);
            if(BaryCentric.x < 0 || BaryCentric.y < 0 || BaryCentric.z < 0) continue;

            float z = p0.z * BaryCentric.x + p1.z * BaryCentric.y + p2.z * BaryCentric.z;
            float CurrentDepth = RenderShader.Framebuffer.SampleDepth((int)P.x, (int)P.y);
            rgba8 Color = {};
            RenderShader.FragmentShader(BaryCentric, Color);
            if(z < CurrentDepth)
            {
                RenderShader.Framebuffer.SetDepthPixel((int)P.x, (int)P.y, z);
                RenderShader.Framebuffer.SetPixel((int)P.x, (int)P.y, Color);
            }
        }
    }
}


void rasterizerRenderer::Rasterize()
{
    glm::mat4 ViewProjectionMatrix = App->Scene->Camera.GetProjectionMatrix() * App->Scene->Camera.GetViewMatrix();
    glm::mat4 ModelMatrix = glm::scale(glm::mat4(1), glm::vec3(0.01f));
	glm::vec3 ViewDir = App->Scene->Camera.GetModelMatrix()[2];
    

    Shader.Uniforms.ViewProjectionMatrix = &ViewProjectionMatrix;
    Shader.Uniforms.ModelMatrix = &ModelMatrix;
    Shader.Buffers.Indices = &App->Scene->Meshes[0].Indices;
    Shader.Buffers.Vertices = &App->Scene->Meshes[0].Vertices;
    
    //Viewport
    Shader.Framebuffer.ViewportStartX = App->Scene->ViewportStart;
    Shader.Framebuffer.ViewportWidth = App->Width - App->Scene->ViewportStart;
    Shader.Framebuffer.ViewportHeight = App->Height;
    Shader.Framebuffer.Width = App->Width;
    Shader.Framebuffer.Height = App->Height;
    Shader.Framebuffer.Color = &Image;
    Shader.Framebuffer.Depth = &DepthBuffer;
    
    for(uint32_t i=0; i<Image.size(); i++)
    {
        Image[i] = {0, 0, 0, 255};
        DepthBuffer[i] = 1e30f;
    }
    
    for(uint32_t i=0; i<Shader.Buffers.Indices->size(); i+=3)
    {
        glm::vec4 Coord0 = Shader.VertexShader(i+0, 0); 
        glm::vec4 Coord1 = Shader.VertexShader(i+1, 1); 
        glm::vec4 Coord2 = Shader.VertexShader(i+2, 2); 
        
        {
            DrawTriangle(Coord0, Coord1, Coord2, Shader);
        }
        // DrawTriangle(p0, p1, p2, {(uint8_t)(std::rand()%255), (uint8_t)(std::rand()%255), (uint8_t)(std::rand()%255), 255});

    }
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
    CreateCommandBuffers();

    Image.resize(App->Width * App->Height, {0, 0, 0, 255});
    DepthBuffer.resize(App->Width * App->Height);
    

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
                              &VulkanObjects.ImageStagingBuffer, Image.size() * sizeof(rgba8), Image.data());
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
