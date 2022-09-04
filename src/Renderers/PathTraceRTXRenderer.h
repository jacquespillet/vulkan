#pragma once
#include "../Renderer.h"
#include "Scene.h"

#include "RayTracingHelper.h"

#include "Image.h"
class pathTraceRTXRenderer : public renderer    
{
public:
    pathTraceRTXRenderer(vulkanApp *App);
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

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructureFeatures{};


    bool ResetAccumulation=true;
    
    void UpdateCamera();
    void UpdateMaterial(size_t Index);
    void UpdateBLASInstance(uint32_t InstanceIndex);
private:
    //Vulkan Objects
    std::vector<accelerationStructure> BottomLevelAccelerationStructures;
    accelerationStructure TopLevelAccelerationStructure;
    buffer MaterialBuffer;
    buffer UpdateMaterialStagingBuffer;
    VkCommandBuffer UpdateMaterialCommandBuffer;
    buffer SceneDescriptionBuffer;
    buffer TransformMatricesBuffer;
    storageImage StorageImage;
    storageImage AccumulationImage; 
    buffer UBO;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkPipelineLayout PipelineLayout;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> ShaderGroups;
    VkPipeline Pipeline;
    VkDescriptorSet DescriptorSet;
    struct shaderBindingTables
    {
        shaderBindingTable Raygen;
        shaderBindingTable Miss;
        shaderBindingTable Hit;
    } ShaderBindingTables;
    std::vector<VkAccelerationStructureInstanceKHR> BLASInstances{};
    buffer InstancesBuffer;
    
    
    //Ray tracing data
    struct uniformData
    {
        uint32_t VertexSize;
        uint32_t CurrentSampleCount=0;
        int SamplersPerFrame=4;
        int RayBounces=5;

        int MaxSamples = 8192;
        int ShouldAccumulate=1;
        glm::ivec2 Padding;
    } UniformData;
    



    //Denoiser
    bool ShouldDenoise=false;
    buffer DenoiseBuffer;
    std::vector<glm::vec3> DenoiserInput;
    std::vector<glm::vec3> DenoiserOutput;
    struct rgba {uint8_t b, g, r, a;};
    std::vector<rgba> DenoiserInputUint;
    
    void Denoise();

    void CreateMaterialBuffer();
    void CreateBottomLevelAccelarationStructure(scene *Scene);
    void FillBLASInstances();
    void CreateTopLevelAccelerationStructure();
    
    VkAccelerationStructureInstanceKHR CreateBottomLevelAccelerationInstance(instance *Instance);

    void CreateCommandBuffers();
    void CreateImages();
    void CreateRayTracingPipeline();
    void CreateShaderBindingTable();
    void CreateDescriptorSets();
    void UpdateUniformBuffers();    
    void BuildUniformBuffers();
    void BuildCommandBuffers();
};