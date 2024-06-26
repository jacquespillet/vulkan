#include "rasterizerRenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>
#include "../Swapchain.h"
#include "../ImGuiHelper.h"
#include <random> 
#include <omp.h>
#include <iostream>

vertexOutData gouraudShader::VertexShader(uint32_t Index, uint8_t TriVert) 
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

    return {
        OutPosition,
        std::max(0.0f, 
                glm::dot(
                    glm::vec3(Vertex->Normal), glm::normalize(glm::vec3(1,1,1))
                )
        )
    };
}

bool gouraudShader::FragmentShader(glm::vec3 Barycentric, vertexOut VOut, rgba8 &ColorOut) 
{
    glm::vec3 VaryingIntensity(VOut.Data[0].Intensity, VOut.Data[1].Intensity, VOut.Data[2].Intensity);
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
}

void renderTarget::SetPixel(int x, int y, rgba8 ColorValue)
{
    // x += ViewportStartX;
    // y += ViewportStartY;
    
    if(x > (int)(Width-1) || y > (int)(Height-1) || x < 0 || y < 0)return;
    (*Color)[y * Width + x] = ColorValue;
}

void rasterizerRenderer::DrawTriangle(vertexOut VertexOut, shader &RenderShader)
{
    glm::vec3 p0 = glm::vec3(VertexOut.Data[0].Coord);
    glm::vec3 p1 = glm::vec3(VertexOut.Data[1].Coord);
    glm::vec3 p2 = glm::vec3(VertexOut.Data[2].Coord);
    
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
            RenderShader.FragmentShader(BaryCentric, VertexOut, Color);
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
    std::chrono::steady_clock::time_point start = std::chrono::high_resolution_clock::now();

    glm::mat4 ViewProjectionMatrix = App->Scene->Camera.GetProjectionMatrix() * App->Scene->Camera.GetViewMatrix();
	glm::vec3 ViewDir = glm::vec3(App->Scene->Camera.GetModelMatrix()[2]);
    

    Shader.Uniforms.ViewProjectionMatrix = &ViewProjectionMatrix;
    
    //Viewport
    Shader.Framebuffer.ViewportStartX =(uint32_t) App->Scene->ViewportStart;
    Shader.Framebuffer.ViewportWidth = (uint32_t)(App->Width - App->Scene->ViewportStart);
    Shader.Framebuffer.ViewportHeight = App->Height;
    Shader.Framebuffer.Width = App->Width;
    Shader.Framebuffer.Height = App->Height;
    Shader.Framebuffer.Color = &Image;
    Shader.Framebuffer.Depth = &DepthBuffer;

    if(Multithreaded)
    {
        
        //Clear framebuffer
        #pragma omp parallel for num_threads(NUM_THREADS)
        for(int i=0; i<Image.size(); i++)
        {
            Image[i] = {0, 0, 0, 255};
            DepthBuffer[i] = 1e30f;
        }

        for(int Instance=0; Instance < App->Scene->InstancesPointers.size(); Instance++)
        {
            Shader.Buffers.Indices = &App->Scene->InstancesPointers[Instance]->Mesh->Indices;
            Shader.Buffers.Vertices = &App->Scene->InstancesPointers[Instance]->Mesh->Vertices;

            glm::mat4 ModelMatrix = App->Scene->InstancesPointers[Instance]->InstanceData.Transform;
            Shader.Uniforms.ModelMatrix = &ModelMatrix;
            
            uint32_t TrianglesPerThread = (uint32_t)std::ceil(
                ( (float)(Shader.Buffers.Indices->size()/3) / NUM_THREADS)
            );
            std::array<uint32_t, NUM_THREADS> ThreadCounters;
            for(int i=0; i<NUM_THREADS; i++)
            {
                ThreadVertexOutData[i].resize(TrianglesPerThread);
                ThreadCounters[i]=0;
            }

            #pragma omp parallel num_threads(NUM_THREADS)
            {
                #pragma omp for
                for(int j=0; j<Shader.Buffers.Indices->size()/3; j++)
                {
                    int ThreadID = omp_get_thread_num();
                    
                    int i = j * 3;
                    glm::vec3 v0 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 0)).Position);
                    glm::vec3 v1 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 1)).Position);
                    glm::vec3 v2 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 2)).Position);
                    glm::vec3 Normal = glm::normalize(glm::cross(glm::vec3(v2 - v0), glm::vec3(v1 - v0)));
                    float BackFace = glm::dot(Normal, -ViewDir);

                    if(BackFace > 0)
                    {        
                        vertexOut VOut = {};
                        VOut.Data[0] = Shader.VertexShader(i+0, 0); 
                        VOut.Data[1] =  Shader.VertexShader(i+1, 1); 
                        VOut.Data[2] =  Shader.VertexShader(i+2, 2);
                        
                        ThreadVertexOutData[ThreadID][ThreadCounters[ThreadID]] = VOut;
                        
                        ThreadCounters[ThreadID]++;
                    }
                }
            }
            

            for(int j=0; j<NUM_THREADS; j++)
            {
                for(uint32_t i=0; i<ThreadCounters[j]; i++)
                {
                    vertexOut VertexOut = ThreadVertexOutData[j][i];

                    glm::vec3 p0 = glm::vec3(VertexOut.Data[0].Coord);
                    glm::vec3 p1 = glm::vec3(VertexOut.Data[1].Coord);
                    glm::vec3 p2 = glm::vec3(VertexOut.Data[2].Coord);
                    
                    p0.x = std::floor(p0.x); p0.y = std::floor(p0.y);
                    p1.x = std::floor(p1.x); p1.y = std::floor(p1.y);
                    p2.x = std::floor(p2.x); p2.y = std::floor(p2.y);

                    glm::vec3 BBMin(Shader.Framebuffer.ViewportWidth-1, Shader.Framebuffer.ViewportHeight-1,0 );
                    glm::vec3 BBMax(0, 0,0 );
                    BBMin = glm::min(BBMin, p0); BBMin = glm::min(BBMin, p1); BBMin = glm::min(BBMin, p2);
                    BBMax = glm::max(BBMax, p0); BBMax = glm::max(BBMax, p1); BBMax = glm::max(BBMax, p2);
                    
                    // glm::vec3 P;
                    

                    int Width = (int)(BBMax.x - BBMin.x);
                    int Height = (int)(BBMax.y - BBMin.y);

                    #pragma omp parallel for num_threads(NUM_THREADS)
                    for(int k=0; k<Width * Height; k++)
                    {
                            int xx = k % Width;
                            int yy = k / Width;
                            int x = xx + (int)BBMin.x;
                            int y = yy + (int)BBMin.y;

                            glm::vec3 P((float)x, (float)y, 0);
                            glm::vec3 BaryCentric = CalculateBarycentric(p0, p1, p2, P);
                            if(BaryCentric.x < 0 || BaryCentric.y < 0 || BaryCentric.z < 0) continue;

                            float z = p0.z * BaryCentric.x + p1.z * BaryCentric.y + p2.z * BaryCentric.z;
                            float CurrentDepth = Shader.Framebuffer.SampleDepth((int)x, (int)y);
                            rgba8 Color = {};
                            Shader.FragmentShader(BaryCentric, VertexOut, Color);
                            if(z < CurrentDepth)
                            {
                                Shader.Framebuffer.SetDepthPixel((int)x, (int)y, z);
                                Shader.Framebuffer.SetPixel((int)x, (int)y, Color);
                            }
                    }     
                }
            }
        }
    }
    else
    {
        for(int i=0; i<Image.size(); i++)
        {
            Image[i] = {0, 0, 0, 255};
            DepthBuffer[i] = 1e30f;
        }
        
        for(int Instance=0; Instance < App->Scene->InstancesPointers.size(); Instance++)
        {
            Shader.Buffers.Indices = &App->Scene->InstancesPointers[Instance]->Mesh->Indices;
            Shader.Buffers.Vertices = &App->Scene->InstancesPointers[Instance]->Mesh->Vertices;

            glm::mat4 ModelMatrix = App->Scene->InstancesPointers[Instance]->InstanceData.Transform;
            Shader.Uniforms.ModelMatrix = &ModelMatrix;
        
            VertexOutData.resize(Shader.Buffers.Indices->size()/3);
            uint32_t Counter=0;
            for(int j=0; j<Shader.Buffers.Indices->size()/3; j++)
            {
                int i = j * 3;
                glm::vec3 v0 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 0)).Position);
                glm::vec3 v1 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 1)).Position);
                glm::vec3 v2 = glm::vec3(Shader.Buffers.Vertices->at(Shader.Buffers.Indices->at(i + 2)).Position);
                glm::vec3 Normal = glm::normalize(glm::cross(glm::vec3(v2 - v0), glm::vec3(v1 - v0)));
                float BackFace = glm::dot(Normal, -ViewDir);

                if(BackFace > 0)
                {        
                    vertexOut VOut = {};
                    VOut.Data[0] = Shader.VertexShader(i+0, 0); 
                    VOut.Data[1] =  Shader.VertexShader(i+1, 1); 
                    VOut.Data[2] =  Shader.VertexShader(i+2, 2);
                    
                    VertexOutData[Counter++] = VOut;
                }
            }
        

        
            for(uint32_t i=0; i<Counter; i++)
            {
                vertexOut VertexOut = VertexOutData[i];

                glm::vec3 p0 = glm::vec3(VertexOut.Data[0].Coord);
                glm::vec3 p1 = glm::vec3(VertexOut.Data[1].Coord);
                glm::vec3 p2 = glm::vec3(VertexOut.Data[2].Coord);
                
                p0.x = std::floor(p0.x); p0.y = std::floor(p0.y);
                p1.x = std::floor(p1.x); p1.y = std::floor(p1.y);
                p2.x = std::floor(p2.x); p2.y = std::floor(p2.y);

                glm::vec3 BBMin(Shader.Framebuffer.ViewportWidth-1, Shader.Framebuffer.ViewportHeight-1,0 );
                glm::vec3 BBMax(0, 0,0 );
                BBMin = glm::min(BBMin, p0); BBMin = glm::min(BBMin, p1); BBMin = glm::min(BBMin, p2);
                BBMax = glm::max(BBMax, p0); BBMax = glm::max(BBMax, p1); BBMax = glm::max(BBMax, p2);
                
                // glm::vec3 P;
                

                int Width = (int)(BBMax.x - BBMin.x);
                int Height = (int)(BBMax.y - BBMin.y);

                #pragma omp parallel for num_threads(NUM_THREADS)
                for(int k=0; k<Width * Height; k++)
                {
                        int xx = k % Width;
                        int yy = k / Width;
                        int x = xx + (int)BBMin.x;
                        int y = yy + (int)BBMin.y;

                        glm::vec3 P((float)x, (float)y, 0);
                        glm::vec3 BaryCentric = CalculateBarycentric(p0, p1, p2, P);
                        if(BaryCentric.x < 0 || BaryCentric.y < 0 || BaryCentric.z < 0) continue;

                        float z = p0.z * BaryCentric.x + p1.z * BaryCentric.y + p2.z * BaryCentric.z;
                        float CurrentDepth = Shader.Framebuffer.SampleDepth((int)x, (int)y);
                        rgba8 Color = {};
                        Shader.FragmentShader(BaryCentric, VertexOut, Color);
                        if(z < CurrentDepth)
                        {
                            Shader.Framebuffer.SetDepthPixel((int)x, (int)y, z);
                            Shader.Framebuffer.SetPixel((int)x, (int)y, Color);
                        }
                }     
            }
        }
    }
    

    std::chrono::steady_clock::time_point stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    float seconds = (float)duration.count() / 1000.0f;
    std::cout << seconds << std::endl;  


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
    ImGui::Checkbox("Multithreaded", &Multithreaded);
}

void rasterizerRenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void rasterizerRenderer::Destroy()
{
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);
}
