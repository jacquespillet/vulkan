#pragma once
#include "../Renderer.h"
#include <functional>
#include <thread>
#include <mutex>
#include "../Image.h"
#include "Scene.h"


class rasterizerRenderer : public renderer    
{
public:
    rasterizerRenderer(vulkanApp *App);
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

    float t=0;

    struct rgba8
    {
        uint8_t b, g, r, a;
    };
    std::vector<rgba8> Image; 
    std::vector<rgba8> PreviewImage; 
    std::vector<glm::vec3> AccumulationImage; 

    uint32_t previewWidth = 128;
    uint32_t previewHeight = 128;

    void UpdateCamera();
private:
    void CreateCommandBuffers();
};