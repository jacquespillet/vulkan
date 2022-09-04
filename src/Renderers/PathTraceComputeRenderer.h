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

    struct
    {
        VkCommandBuffer DrawCommandBuffer;
        VkSubmitInfo SubmitInfo;
        storageImage FinalImage;
        storageImage AccumulationImage;

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
        
        buffer MaterialBuffer;

        buffer UBO;
    } VulkanObjects;

    
    struct indexData
    {
        uint32_t triangleDataStartInx;
        uint32_t IndicesDataStartInx;
        uint32_t BVHNodeDataStartInx;
        uint32_t padding;
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
    

    uint32_t previewWidth = 1024;
    uint32_t previewHeight = 1024;

    struct uniformData
    {
        uint32_t VertexSize;
        uint32_t CurrentSampleCount=0;
        int SamplersPerFrame=4;
        int RayBounces=5;

        int MaxSamples = 8192;
        int ShouldAccumulate=1;
        glm::ivec2 Padding;
    } UniformData;
    bool ResetAccumulation=true;
    

    void UpdateCamera();
    void UpdateTLAS(uint32_t InstanceIndex);
private:

    std::vector<mesh*> Meshes;
    std::vector<bvhInstance> Instances;
    tlas TLAS;

    void CreateCommandBuffers();
    void SetupDescriptorPool();
    void FillCommandBuffer();
    void UpdateUniformBuffers();    

};