#pragma once
#include "../Renderer.h"

class forwardRenderer : public renderer    
{
public:
    forwardRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;
    void Resize(uint32_t Width, uint32_t Height) override;

    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    resources Resources;

        
    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;


    void UpdateCamera();
private:
    void SetupDescriptorPool();
    void BuildUniformBuffers();
    void BuildQuads();
    void BuildLayoutsAndDescriptors();
    void BuildPipelines();
    void BuildCommandBuffers();
    void CreateCommandBuffers();

};