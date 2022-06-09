#pragma once
#include "../Renderer.h"
#include "RayTracingHelper.h"

class scene;
struct instance;

class hybridRenderer : public renderer    
{
public:
    hybridRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;
    void Resize(uint32_t Width, uint32_t Height) override;

    std::vector<VkCommandBuffer> DrawCommandBuffers;

    VkDescriptorPool DescriptorPool;
    VkDescriptorSet DescriptorSet;
    resources Resources;
    // VkPipelineLayout PipelineLayout;


    std::vector<VkShaderModule> ShaderModules;
    VkSubmitInfo SubmitInfo;
    VkDescriptorSetLayout DescriptorSetLayout;

    void UpdateCamera();
private:
    buffer InstancesBuffer;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures{};
        
    std::vector<accelerationStructure> BottomLevelAccelerationStructures;
    accelerationStructure TopLevelAccelerationStructure;
    std::vector<VkAccelerationStructureInstanceKHR> BLASInstances{};

    
    void CreateBottomLevelAccelarationStructure(scene *Scene);
    void FillBLASInstances();
    void CreateTopLevelAccelerationStructure();
    VkAccelerationStructureInstanceKHR CreateBottomLevelAccelerationInstance(instance *Instance);


    void SetupDescriptorPool();
    void BuildUniformBuffers();
    void BuildQuads();
    void BuildLayoutsAndDescriptors();
    void BuildPipelines();
    void BuildCommandBuffers();
    void CreateCommandBuffers();

};