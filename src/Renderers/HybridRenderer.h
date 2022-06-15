#pragma once

#include "Scene.h"
#include "../Renderer.h"
#include "RayTracingHelper.h"
#include "TextureLoader.h"

class deferredHybridRenderer : public renderer    
{
public:
    deferredHybridRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override{};
    void Resize(uint32_t Width, uint32_t Height) override;


    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    resources Resources;
    
    struct
    {
        glm::mat4 Projection;
    } UBOSSAOParams;

    sceneMesh Quad;
    
    struct 
    {
        struct offscreen : public framebuffer {
        } Offscreen;
    } Framebuffers;

    struct 
    {
        VkDescriptorSetLayout DescriptorSetLayout;
        VkPipeline Pipeline;
        vulkanTexture Texture;
        VkSemaphore Semaphore;
        struct {
            uint32_t FrameCounter=0;
        } UniformData;
        buffer UniformBuffer;
    } ShadowPass;

    struct 
    {
        VkQueue Queue;
        VkCommandPool CommandPool;
        VkCommandBuffer CommandBuffer;
        VkFence Fence;
    } Compute;

    
    VkCommandBuffer OffscreenCommandBuffer = VK_NULL_HANDLE;
    bool Rebuild=false;
    VkSemaphore OffscreenSemaphore;
    bool EnableSSAO=true;
    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;


    void UpdateUniformBufferScreen();
    void UpdateUniformBufferDeferredMatrices();
    void UpdateCamera();
private:

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures{};
        
    buffer InstancesBuffer;
    std::vector<accelerationStructure> BottomLevelAccelerationStructures;
    accelerationStructure TopLevelAccelerationStructure;
    std::vector<VkAccelerationStructureInstanceKHR> BLASInstances{};
    
    void CreateBottomLevelAccelarationStructure(scene *Scene);
    void FillBLASInstances();
    void CreateTopLevelAccelerationStructure();
    VkAccelerationStructureInstanceKHR CreateBottomLevelAccelerationInstance(instance *Instance);

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