#pragma once
#include "../Renderer.h"

class forwardRenderer : public renderer    
{
public:
    forwardRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    

    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    resources Resources;
    struct
    {
        buffer SceneMatrices;
    } UniformBuffers;
    struct 
    {
        glm::mat4 Projection;
        glm::mat4 Model;
        glm::mat4 View;
        glm::vec2 ViewportDim;
    } UBOVS, UBOSceneMatrices;
        
    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;


    void UpdateUniformBufferDeferredMatrices();
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