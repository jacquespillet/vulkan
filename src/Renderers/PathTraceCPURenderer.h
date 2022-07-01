#pragma once
#include "../Renderer.h"
#include "../Image.h"
#include "Scene.h"

#include "../bvh.h"

#include "ThreadPool.h"

class pathTraceCPURenderer : public renderer    
{
public:
    pathTraceCPURenderer(vulkanApp *App);
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

        buffer ImageStagingBuffer;

        storageImage previewImage;
        buffer previewBuffer;
    } VulkanObjects;

    struct rgba8
    {
        uint8_t b, g, r, a;
    };
    std::vector<rgba8> Image; 
    std::vector<rgba8> PreviewImage; 
    std::vector<glm::vec3> AccumulationImage; 

    uint32_t previewWidth = 128;
    uint32_t previewHeight = 128;

    uint32_t SamplesPerFrame=1;
    uint32_t TotalSamples = 16000;
    uint32_t CurrentSampleCount=0;

    void UpdateCamera();
    void UpdateTLAS(uint32_t InstanceIndex);
private:

    threadPool ThreadPool;

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
    void PathTraceTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, std::vector<rgba8>* ImageToWrite);
    void PreviewTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, uint32_t RenderWidth, uint32_t RenderHeight, std::vector<rgba8>* ImageToWrite);
    void CreateCommandBuffers();

};