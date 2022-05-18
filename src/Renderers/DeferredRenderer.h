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

    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    resources Resources;
    struct
    {
        buffer FullScreen;
        buffer SceneMatrices;
        buffer SSAOKernel;
        buffer SSAOParams;
    } UniformBuffers;
    struct 
    {
        glm::mat4 Projection;
        glm::mat4 Model;
        glm::mat4 View;
        glm::vec2 ViewportDim;
    } UBOVS, UBOSceneMatrices;
    
    struct
    {
        glm::mat4 Projection;
        uint32_t SSAO=true;
        uint32_t SSAOOnly=false;
        uint32_t SSAOBlur=true;
    } UBOSSAOParams;
        
    struct 
    {
        vulkanTexture SSAONoise;
    } Textures;
    struct 
    {
        sceneMesh Quad;
    } Meshes;
    
    struct 
    {
        struct offscreen : public framebuffer {
        } Offscreen;
        struct SSAO : public framebuffer {
        } SSAO, SSAOBlur;
    } Framebuffers;

    
    VkCommandBuffer OffscreenCommandBuffer = VK_NULL_HANDLE;
    bool Rebuild=false;
    VkSemaphore OffscreenSemaphore;
    bool EnableSSAO=true;
    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;


    void UpdateUniformBufferScreen();
    void UpdateUniformBufferDeferredMatrices();
    void UpdateUniformBufferSSAOParams();
    void UpdateCamera();
private:
    void CreateCommandBuffers();
    void SetupDescriptorPool();
    void BuildUniformBuffers();
    void BuildQuads();
    void BuildOffscreenBuffers();
    void BuildLayoutsAndDescriptors();
    void BuildPipelines();
    void BuildCommandBuffers();
    void BuildDeferredCommandBuffers();
};