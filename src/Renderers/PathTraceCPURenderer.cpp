#include "PathTraceCPURenderer.h"
#include "App.h"
#include "imgui.h"
#include "brdf.h"

#include <chrono>

#include "../Swapchain.h"
#include "../ImGuiHelper.h"
#include <iostream>

#define DIFFUSE_TYPE 1
#define SPECULAR_TYPE 2



////////////////////////////////////////////////////////////////////////////////////////

void threadPool::Start()
{
    uint32_t NumThreads = std::thread::hardware_concurrency();
    // NumThreads=1;
    
    Threads.resize(NumThreads);
    Finished.resize(NumThreads, true);
    for(uint32_t i=0; i<NumThreads; i++)
    {
        Threads[i] = std::thread([this, i]()
        {
            while(true)
            {
                std::function<void()> Job;

                {
                    //Wait for the job to not be empty, or for the souldTerminate signal
                    std::unique_lock<std::mutex> Lock(QueueMutex);
                    
                    //Locks the thread until MutexCondition is being signalized, or if the jobs queue is not empty, or if we should terminate
                    MutexCondition.wait(Lock, [this]{
                        return !Jobs.empty() || ShouldTerminate;
                    });

                    if(ShouldTerminate) return;

                    Job = Jobs.front();
                    Jobs.pop();
                    Finished[i] = false;
                }

                Job();
                Finished[i]=true;
            }       
        });
    }

}

void threadPool::AddJob(const std::function<void()>& Job)
{
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        Jobs.push(Job);
    }
    //Notifies one thread waiting on this condition to be signalized
    MutexCondition.notify_one();
}

void threadPool::Stop()
{
    {
        std::unique_lock<std::mutex> lock(QueueMutex);
        ShouldTerminate=true;
    }

    MutexCondition.notify_all();
    for(int i=0; i<Threads.size(); i++)
    {
        Threads[i].join();
    }
    Threads.clear();
}

bool threadPool::Busy()
{
    bool Busy;
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        bool AllFinished=true;
        for(int i=0; i<Finished.size(); i++)
        {
            if(!Finished[i])
            {
                AllFinished=false;
                break;
            }
        }
        Busy = !AllFinished;
    }  

    return Busy;
}

////////////////////////////////////////////////////////////////////////////////////////

float GAMMA = 2.2f;
float INV_GAMMA = 1.0f / GAMMA;

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
glm::mat3 ACESInputMat = glm::mat3
(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

// ACES filmic tone map approximation
// see https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
glm::vec3 RRTAndODTFit(glm::vec3 color)
{
    glm::vec3 a = color * (color + 0.0245786f) - 0.000090537f;
    glm::vec3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    return a / b;
}

// ODT_SAT => XYZ => D60_2_D65 => sRGB
glm::mat3 ACESOutputMat = glm::mat3
(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);
// tone mapping 
glm::vec3 toneMapACES_Hill(glm::vec3 color)
{
    color = ACESInputMat * color;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = ACESOutputMat * color;

    // Clamp to [0, 1]
    color = clamp(color, 0.0f, 1.0f);

    return color;
}

glm::vec3 linearTosRGB(glm::vec3 color)
{
    return pow(color, glm::vec3(INV_GAMMA));
}

glm::vec3 toneMap(glm::vec3 color, float Exposure)
{
    color *= Exposure;
    color /= 0.6;
    color = toneMapACES_Hill(color);
    return linearTosRGB(color);
}


////////////////////////////////////////////////////////////////////////////////////////

pathTraceCPURenderer::pathTraceCPURenderer(vulkanApp *App) : renderer(App) {
    this->UseGizmo=false;
}

void pathTraceCPURenderer::StartPathTrace()
{
    PathTraceFinished=true;
    ShouldPathTrace=true;
    CurrentSampleCount=0;
    for(uint32_t i=0; i<AccumulationImage.size(); i++)
    {
        AccumulationImage[i] = glm::vec3(0);
    }
}

void pathTraceCPURenderer::Render()
{
    if(ShouldPathTrace)
    {
        ProcessingPreview=false;
        if(!ThreadPool.Busy()) PathTrace();    
    }

    if(CurrentSampleCount >= TotalSamples)
    {
        ShouldPathTrace=false;
        PathTraceFinished=true;
        ProcessingPathTrace=false;
    }

    if(App->Scene->Camera.Changed)
    {
        ShouldPathTrace=false;
        ProcessingPathTrace=false;
        Preview();    
    }

    VK_CALL(App->VulkanObjects.Swapchain->AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    if(ProcessingPathTrace || PathTraceFinished)
    {
        VulkanObjects.ImageStagingBuffer.Map();
        VulkanObjects.ImageStagingBuffer.CopyTo(Image.data(), Image.size() * sizeof(rgba8));
        VulkanObjects.ImageStagingBuffer.Unmap();

        if(!ThreadPool.Busy() && !PathTraceFinished)
        {
            stop = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
            float seconds = (float)duration.count() / 1000.0f;
            std::cout << seconds << std::endl;  

            PathTraceFinished=true; 
        }
    }

    if(ProcessingPreview)
    {
        VulkanObjects.previewBuffer.Map();
        VulkanObjects.previewBuffer.CopyTo(PreviewImage.data(), PreviewImage.size() * sizeof(rgba8));
        VulkanObjects.previewBuffer.Unmap();
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

        VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain->Images[App->VulkanObjects.CurrentBuffer], 
                                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);

        if(ProcessingPathTrace)
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

        if(ProcessingPreview)
        {
            VkBufferImageCopy Region = {};
            Region.imageExtent.depth=1;
            Region.imageExtent.width = previewWidth;
            Region.imageExtent.height = previewHeight;
            Region.imageOffset = {0,0,0};
            Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Region.imageSubresource.baseArrayLayer=0;
            Region.imageSubresource.layerCount=1;
            Region.imageSubresource.mipLevel=0;
            Region.bufferOffset=0;
            Region.bufferRowLength=0;

            vkCmdCopyBufferToImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewBuffer.VulkanObjects.Buffer, 
                                    VulkanObjects.previewImage.Image, 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);  

            vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image, 
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);                                    

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


			vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
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

void pathTraceCPURenderer::PreviewTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, uint32_t RenderWidth, uint32_t RenderHeight, std::vector<rgba8>* ImageToWrite)
{
    float StartRatio = App->Scene->ViewportStart / App->Width;
    uint32_t StartPreview = (uint32_t)(StartRatio * (float)previewWidth);
    
    glm::vec4 Origin = App->Scene->Camera.GetModelMatrix() * glm::vec4(0,0,0,1);
    ray Ray = {}; 
    for(uint32_t yy=StartY; yy < StartY+TileHeight; yy++)
    {
        for(uint32_t xx=StartX; xx < StartX+TileWidth; xx++)
        {
            if(xx >= ImageWidth-1) break;

            (*ImageToWrite)[yy * ImageWidth + xx] = { 0, 0, 0, 0 };

            glm::vec2 uv((float)(xx - StartPreview) / (float)RenderWidth, (float)yy / (float)RenderHeight);
            Ray.Origin = Origin;
            glm::vec4 Target = glm::inverse(App->Scene->Camera.GetProjectionMatrix()) * glm::vec4(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f, 0.0f, 1.0f);
            glm::vec4 Direction = App->Scene->Camera.GetModelMatrix() * glm::normalize(glm::vec4(Target.x,Target.y, Target.z, 0.0f));
            Ray.Direction = Direction;
            
            rayPayload RayPayload = {};
            RayPayload.Distance = 1e30f;
            
            TLAS.Intersect(Ray, RayPayload);

            if (RayPayload.Distance < 1e30f)
            {
                
                sceneMaterial *Material = App->Scene->InstancesPointers[RayPayload.InstanceIndex]->Mesh->Material;
                materialData *MatData = &Material->MaterialData;
                vulkanTexture *DiffuseTexture = &Material->Diffuse;
                
                glm::vec2 UV = 
                    Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV1 * RayPayload.U + 
                    Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV2 * RayPayload.V +
                    Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV0 * (1 - RayPayload.U - RayPayload.V);
                

                glm::vec3 FinalColor(0);

                glm::vec3 BaseColor = MatData->BaseColor;
                if(MatData->BaseColorTextureID >=0 && MatData->UseBaseColor>0)
                {
                    glm::vec4 TextureColor = DiffuseTexture->Sample(UV);
                    
                    TextureColor *= glm::pow(TextureColor, glm::vec4(2.2f));
                    BaseColor *= glm::vec3(TextureColor);                    
                }                

                FinalColor += BaseColor;
                FinalColor = toneMap(FinalColor, App->Scene->UBOSceneMatrices.Exposure);
                
                uint8_t r = (uint8_t)(FinalColor.r * 255.0f);
                uint8_t g = (uint8_t)(FinalColor.g * 255.0f);
                uint8_t b = (uint8_t)(FinalColor.b * 255.0f);

                (*ImageToWrite)[yy * ImageWidth + xx] = {b, g, r, 255 };
            }
        }
        if(yy >= ImageHeight-1) break;
    }
}

uint32_t wang_hash(uint32_t &seed)
{
    seed = uint32_t(seed ^ uint32_t(61)) ^ uint32_t(seed >> uint32_t(16));
    seed *= uint32_t(9);
    seed = seed ^ (seed >> 4);
    seed *= uint32_t(0x27d4eb2d);
    seed = seed ^ (seed >> 15);
    return seed;
}

float RandomUnilateral(uint32_t &State)
{
    return float(wang_hash(State)) / 4294967296.0f;
}

float RandomBilateral(uint32_t &State)
{
    return RandomUnilateral(State) * 2.0f - 1.0f;
}

void pathTraceCPURenderer::PathTraceTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, std::vector<rgba8>* ImageToWrite)
{
    uint32_t RayBounces=4;
    ray Ray = {}; 
    glm::vec2 InverseImageSize = 1.0f / glm::vec2(ImageWidth, ImageHeight);
    
    rayPayload RayPayload = {};
                 

    for(uint32_t yy=StartY; yy < StartY+TileHeight; yy++)
    {
        for(uint32_t xx=StartX; xx < StartX+TileWidth; xx++)
        {
            RayPayload.RandomState = (xx * 1973 + yy * 9277 + CurrentSampleCount * 26699) | 1; 

            if(xx >= ImageWidth-1) break;
            (*ImageToWrite)[yy * ImageWidth + xx] = { 0, 0, 0, 0 };

            glm::vec3 SampleColor(0.0f);

            //Origin
            glm::mat4 ModelMatrix = App->Scene->Camera.GetModelMatrix();
            glm::vec3 Origin(ModelMatrix[3][0],ModelMatrix[3][1],ModelMatrix[3][2]);
            
            glm::vec2 uv((float)(xx - App->Scene->ViewportStart) / (float)(ImageWidth-App->Scene->ViewportStart), (float)yy / (float)ImageHeight);
            Ray.Origin = Origin;
            glm::vec4 Target = glm::inverse(App->Scene->Camera.GetProjectionMatrix()) * glm::vec4(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f, 0.0f, 1.0f);
        
            for(uint32_t Sample=0; Sample<SamplesPerFrame; Sample++)
            {
                Ray.Origin = Origin;
                
                RayPayload.Depth=0;

                //Jitter
                glm::vec2 Jitter = (glm::vec2(RandomBilateral(RayPayload.RandomState), RandomBilateral(RayPayload.RandomState))) * InverseImageSize;
                glm::vec3 JitteredTarget = Target + glm::vec4(Jitter,0,0);
                Ray.Direction = App->Scene->Camera.GetModelMatrix() * glm::vec4(glm::normalize(JitteredTarget), 0.0);
                
                glm::vec3 Attenuation(1.0);
                glm::vec3 Radiance(0);
                
                
                RayPayload.Distance = 1e30f;
                for(uint32_t j=0; j<RayBounces; j++)
                {
                    TLAS.Intersect(Ray, RayPayload);
                        
                    //Sky
                    {
                        if(RayPayload.Distance == 1e30f)
                        {
                            // if(SceneUbo.Data.BackgroundType ==BACKGROUND_TYPE_CUBEMAP)
                            // {
                            //     vec3 SkyDirection = Direction.xyz;
                            //     SkyDirection.y *=-1;
                            //     Radiance += Attenuation * SceneUbo.Data.BackgroundIntensity * texture(IrradianceMap, SkyDirection).rgb;
                            // }
                            // else if(SceneUbo.Data.BackgroundType ==BACKGROUND_TYPE_COLOR)
                            // {
                                Radiance += Attenuation * App->Scene->UBOSceneMatrices.BackgroundIntensity * App->Scene->UBOSceneMatrices.BackgroundColor;
                            // }
                            break;
                        }
                    }

                    ////Unpack triangle data
                    sceneMaterial *Material = App->Scene->InstancesPointers[RayPayload.InstanceIndex]->Mesh->Material;
                    materialData *MatData = &Material->MaterialData;
                    vulkanTexture *DiffuseTexture = &Material->Diffuse;
                    vulkanTexture *MetallicRoughnessTexture = &Material->Specular;
                    
                    glm::vec2 UV = 
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV1 * RayPayload.U + 
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV2 * RayPayload.V +
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].UV0 * (1 - RayPayload.U - RayPayload.V);
                    
                    glm::vec3 Normal = 
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].Normal1 * RayPayload.U + 
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].Normal2 * RayPayload.V +
                        Instances[RayPayload.InstanceIndex].BVH->Mesh->TrianglesExtraData[RayPayload.PrimitiveIndex].Normal0 * (1 - RayPayload.U - RayPayload.V);
                    
                    glm::vec3 BaseColor = MatData->BaseColor;
                    if(MatData->BaseColorTextureID >=0 && MatData->UseBaseColor>0)
                    {
                        glm::vec4 TextureColor = DiffuseTexture->Sample(UV);
                        
                        TextureColor *= glm::pow(TextureColor, glm::vec4(2.2f));
                        BaseColor *= glm::vec3(TextureColor);                    
                    }

                    float Roughness = MatData->Roughness;
                    float Metallic = MatData->Metallic;
                    if(MatData->MetallicRoughnessTextureID >=0 && MatData->UseMetallicRoughness>0)
                    {
                        glm::vec2 RoughnessMetallic = MetallicRoughnessTexture->Sample(UV);
                        Metallic *= RoughnessMetallic.r;
                        Roughness *= RoughnessMetallic.g;
                    }    

                    
                    Radiance += Attenuation * RayPayload.Emission;

                    //Sample lights
                    {
                        
                    }


                    if(j == RayBounces-1) break;
                    
                    // Russian roulette
                    {
                        if (j >= 3)
                        {
                            float q = std::min(std::max(Attenuation.x, std::max(Attenuation.y, Attenuation.z)) + 0.001f, 0.95f);
                            if (RandomUnilateral(RayPayload.RandomState) > q)
                                break;
                            Attenuation /= q;
                        }
                    }
                    

                    //Eval brdf
                    {
                        glm::vec3 V = -Ray.Direction;
                        int brdfType = DIFFUSE_TYPE;
                        if (Metallic == 1.0f && Roughness == 0.0f) {
                            brdfType = SPECULAR_TYPE;
                        } else {
                            float brdfProbability = GetBrdfProbability(RayPayload, V, Normal, BaseColor, Metallic);

                            if (RandomUnilateral(RayPayload.RandomState) < brdfProbability) {
                                brdfType = SPECULAR_TYPE;
                                Attenuation /= brdfProbability;
                            } else {
                                brdfType = DIFFUSE_TYPE;
                                Attenuation /= (1.0f - brdfProbability);
                            }
                        }

                        glm::vec3 brdfWeight;
                        glm::vec2 Xi = glm::vec2(RandomUnilateral(RayPayload.RandomState),RandomUnilateral(RayPayload.RandomState));
                        glm::vec3 ScatterDir = glm::vec3(0);
                        
                        if(brdfType == DIFFUSE_TYPE)
                        {
                            if (!SampleDiffuseBRDF(Xi, Normal, V,  ScatterDir, brdfWeight, BaseColor, Metallic)) {
                                break;
                            }
                        }
                        else if(brdfType == SPECULAR_TYPE)
                        {
                            if (!SampleSpecularBRDF(Xi, Normal, V,  ScatterDir, brdfWeight, BaseColor, Roughness, Metallic)) {
                                break;
                            }
                        }

                        //the weights already contains color * brdf * cosine term / pdf
                        Attenuation *= brdfWeight;						

                        Ray.Origin = Ray.Origin + RayPayload.Distance * Ray.Direction;
                        Ray.Direction = glm::vec4(ScatterDir, 0.0f);
                        RayPayload.Distance = 1e30f;
                    }					   
                }

                SampleColor += Radiance;	
            }

			AccumulationImage[yy * ImageWidth + xx] += SampleColor;

			glm::vec3 Color = AccumulationImage[yy * ImageWidth + xx] / CurrentSampleCount;

            Color = toneMap(Color, App->Scene->UBOSceneMatrices.Exposure);
			Color = glm::clamp(Color, glm::vec3(0), glm::vec3(1));
			// Color *= glm::pow(Color, glm::vec3(1.0f / 2.2f));

            uint8_t r = (uint8_t)(Color.r * 255.0f);
            uint8_t g = (uint8_t)(Color.g * 255.0f);
            uint8_t b = (uint8_t)(Color.b * 255.0f);

            (*ImageToWrite)[yy * ImageWidth + xx] = {b, g, r, 255 };
        }
        if(yy >= ImageHeight-1) break;
    }

}

void pathTraceCPURenderer::PathTrace()
{
    start = std::chrono::high_resolution_clock::now();
    
    CurrentSampleCount += SamplesPerFrame;
    for(uint32_t y=0; y<App->Height; y+=TileSize)
    {
        for(uint32_t x=(uint32_t)App->Scene->ViewportStart; x<App->Width; x+=TileSize)
        {
            ThreadPool.AddJob([x, y, this]()
            {
               PathTraceTile(x, y, TileSize, TileSize, App->Width, App->Height, &Image); 
            });
        }
    }

    ProcessingPathTrace=true;
}

void pathTraceCPURenderer::Preview()
{
    float StartRatio = App->Scene->ViewportStart / App->Width;
    uint32_t StartPreview = (uint32_t)((float)StartRatio * (float)previewWidth);
    
    uint32_t RenderWidth = previewWidth - StartPreview;
    uint32_t RenderHeight = previewHeight;
    for(uint32_t y=0; y<previewHeight; y+=TileSize)
    {
        for(uint32_t x=StartPreview; x<previewWidth; x+=TileSize)
        {   
            ThreadPool.AddJob([x, y, RenderWidth, RenderHeight, this]()
            {
               PreviewTile(x, y, 
                           TileSize, TileSize, 
                           previewWidth, previewHeight, 
                           RenderWidth,  RenderHeight, 
                           &PreviewImage); 
            });
        }
    }

    ProcessingPreview=true;
}

void pathTraceCPURenderer::Setup()
{
    previewWidth = App->Width / 10;
    previewHeight = App->Height / 10;

    ThreadPool.Start();
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

    
    for(size_t i=0; i<App->Scene->Meshes.size(); i++)
    {
        Meshes.push_back(new mesh(App->Scene->Meshes[i].Indices,
                    App->Scene->Meshes[i].Vertices));
    }
    for(size_t i=0; i<App->Scene->InstancesPointers.size(); i++)
    {
        Instances.push_back(
            bvhInstance(Meshes[App->Scene->InstancesPointers[i]->MeshIndex]->BVH,
                        App->Scene->InstancesPointers[i]->InstanceData.Transform, (uint32_t)i)
        );
    }
    
    TLAS = tlas(&Instances);
    TLAS.Build();
}

//


void pathTraceCPURenderer::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &VulkanObjects.DrawCommandBuffer));
}

void pathTraceCPURenderer::RenderGUI()
{
    if(ImGui::Button("PathTrace"))
    {
        StartPathTrace();
    }
}

void pathTraceCPURenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void pathTraceCPURenderer::Destroy()
{
    ThreadPool.Stop();
    for(int i=0; i<Meshes.size(); i++)
    {
        delete Meshes[i]->BVH;
        delete Meshes[i];
    }
    
    VulkanObjects.ImageStagingBuffer.Destroy();

    VulkanObjects.previewImage.Destroy();
    VulkanObjects.previewBuffer.Destroy();

    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);

}


