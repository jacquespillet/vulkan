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

        buffer IndexDataBuffer;
        
        //BLAS
        buffer TriangleBuffer;
        buffer TriangleExBuffer;
        buffer InstanceBuffer;
        buffer BVHBuffer;
        buffer IndicesBuffer;
        
        //TLAS
        buffer TLASInstancesBuffer;
        buffer TLASNodesBuffer;
    } VulkanObjects;

    
    struct indexData
    {
        uint32_t triangleDataStartInx;
        uint32_t TriangleDataCount;
        uint32_t IndicesDataStartInx;
        uint32_t IndicesDataCount;
        uint32_t BVHNodeDataStartInx;
        uint32_t BVHNodeDataCount;
    };
    std::vector<indexData> IndexData;

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