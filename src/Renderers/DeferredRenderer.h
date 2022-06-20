#pragma once

#include "Scene.h"
#include "../Renderer.h"

class deferredRenderer : public renderer    
{
public:
    deferredRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override{};
    void Resize(uint32_t Width, uint32_t Height) override;

    struct 
    {
        std::vector<VkCommandBuffer> DrawCommandBuffers;
        VkDescriptorPool DescriptorPool;
        resources Resources;

        std::vector<VkShaderModule> ShaderModules;
        VkSubmitInfo SubmitInfo;
        VkCommandBuffer OffscreenCommandBuffer = VK_NULL_HANDLE;
        VkSemaphore OffscreenSemaphore;
    } VulkanObjects;


    
    struct
    {
        glm::mat4 Projection;
        uint32_t SSAO=true;
        uint32_t SSAOOnly=false;
        uint32_t SSAOBlur=true;
    } UBOSSAOParams;
        

    sceneMesh Quad;
    
    struct
    {
        buffer SSAOKernel;
        buffer SSAOParams;
    } UniformBuffers;
    
    struct 
    {
        vulkanTexture SSAONoise;
    } Textures;
    
    struct 
    {
        struct offscreen : public framebuffer {
        } Offscreen;
        struct SSAO : public framebuffer {
        } SSAO, SSAOBlur;
    } Framebuffers;

    
    bool Rebuild=false;
    bool EnableSSAO=true;


    void UpdateUniformBufferScreen();
    void UpdateUniformBufferDeferredMatrices();
    void UpdateUniformBufferSSAOParams();
    void UpdateCamera();
private:
    void CreateCommandBuffers();
    void SetupDescriptorPool();
    void BuildUniformBuffers();
    void BuildQuads();
    void BuildCommandBuffers();
    void BuildOffscreenBuffers();
    void BuildLayoutsAndDescriptors();
    void BuildPipelines();
    void BuildDeferredCommandBuffers();
};