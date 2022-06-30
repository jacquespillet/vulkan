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
    } VulkanObjects;

    
    struct rgba8
    {
        uint8_t b, g, r, a;
    };
    std::vector<rgba8> Image; 
    std::vector<float> DepthBuffer;
    
    void UpdateCamera();
private:
    void CreateCommandBuffers();

    glm::vec3  CalculateBarycentric(glm::vec3 A, glm::vec3 B,glm::vec3 C, glm::vec3 P);
    void DrawTriangle(glm::vec3 p0, glm::vec3 p1,glm::vec3 p2, rgba8 Color);

    void SetPixel(int x, int y, rgba8 Color);
    
    float SampleDepth(int x, int y);
    void SetDepthPixel(int x, int y, float Depth);
};