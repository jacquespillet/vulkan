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

    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    resources Resources;

        
    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;


    void UpdateCamera();
private:
    void CreateCommandBuffers();
    void SetupDescriptorPool();
    void BuildUniformBuffers();
    void BuildQuads();
    void BuildLayoutsAndDescriptors();
    void BuildPipelines();
    void BuildCommandBuffers();

};