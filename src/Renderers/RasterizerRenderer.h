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

    void Rasterize();

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
    
    uint32_t previewWidth = 128;
    uint32_t previewHeight = 128;

    void UpdateCamera();
private:
    void CreateCommandBuffers();

    glm::vec3 CalculateBarycentric(glm::ivec2 p0, glm::ivec2 p1,glm::ivec2 p2, int x, int y);

    void DrawLine(glm::ivec2 p0, glm::ivec2 p1, rgba8 Color);
    void DrawTriangle(glm::ivec2 p0, glm::ivec2 p1,glm::ivec2 p2, rgba8 Color);
    void SetPixel(int x, int y, rgba8 Color);
};