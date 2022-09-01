#pragma once
#include "../Renderer.h"
#include "../Image.h"
#include "Scene.h"

#include "../bvh.h"
#include <chrono>

class pathTraceComputeRenderer : public renderer    
{
public:
    pathTraceComputeRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;
    void Resize(uint32_t Width, uint32_t Height) override;

    void StartPathTrace();

    struct
    {
        VkCommandBuffer DrawCommandBuffer;
        VkSubmitInfo SubmitInfo;
        storageImage previewImage;

        VkPipeline previewPipeline;
        VkDescriptorPool DescriptorPool;
        VkSemaphore PreviewSemaphore;

        buffer TriangleBuffer;
        buffer TriangleExBuffer;
        buffer InstanceBuffer;
        buffer TLASBuffer;
        buffer BVHBuffer;
        buffer IndicesBuffer;
        // buffer IndicesBuffer;
        // buffer InstanceBuffer;
        // buffer TlasBuffer;
        // buffer bvhBuffer;
    } VulkanObjects;

    
    struct 
    {
        VkQueue Queue;
        VkCommandPool CommandPool;
        VkCommandBuffer CommandBuffer;
        VkFence Fence;
    } Compute;
    resources Resources;
    

    uint32_t previewWidth = 128;
    uint32_t previewHeight = 128;

    uint32_t SamplesPerFrame=1;
    uint32_t TotalSamples = 16000;
    uint32_t CurrentSampleCount=0;

    void UpdateCamera();
    void UpdateTLAS(uint32_t InstanceIndex);
private:

    std::vector<mesh*> Meshes;
    std::vector<bvhInstance> Instances;
    tlas TLAS;

    bool ShouldPathTrace=false;
    
    bool ProcessingPreview=false;
    bool ProcessingPathTrace=false;
    bool PathTraceFinished=false;

    int TileSize=64;

    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point stop;
    

    void PathTrace();
    void Preview();
    void CreateCommandBuffers();
    void SetupDescriptorPool();
    void FillCommandBuffer();

};