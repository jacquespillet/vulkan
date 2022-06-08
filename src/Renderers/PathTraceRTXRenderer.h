#pragma once
#include "../Renderer.h"
#include "Scene.h"

struct accelerationStructure
{
    VkAccelerationStructureKHR AccelerationStructure= VK_NULL_HANDLE;
    uint64_t DeviceAddress=0;
    VkDeviceMemory Memory;
    VkBuffer Buffer;
    vulkanDevice *VulkanDevice=nullptr;
    void Create(vulkanDevice *VulkanDevice, VkAccelerationStructureTypeKHR Type, VkAccelerationStructureBuildSizesInfoKHR BuildSizeInfo);
    void Destroy();
};

struct scratchBuffer
{
    uint64_t DeviceAddress=0;
    VkBuffer Buffer = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    vulkanDevice *VulkanDevice = nullptr;

    scratchBuffer(vulkanDevice *VulkanDevice, VkDeviceSize Size);
    void Destroy();
};


struct sceneModelInfo
{
    uint64_t Vertices;
    uint64_t Indices;
    uint64_t Materials;
};

struct storageImage
{
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    VkFormat Format;
    vulkanDevice *VulkanDevice=nullptr;
    void Create(vulkanDevice *VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkFormat Format, VkExtent3D Extent);
    void Destroy();
};

class shaderBindingTable : public buffer
{
public:
    VkStridedDeviceAddressRegionKHR StrideDeviceAddressRegion{};
    void Create(vulkanDevice *Device, uint32_t HandleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties);
    void Destroy();
};

class pathTraceRTXRenderer : public renderer    
{
public:
    pathTraceRTXRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;

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
    std::vector<accelerationStructure> BottomLevelAccelerationStructures;
    accelerationStructure TopLevelAccelerationStructure;
    
    buffer MaterialBuffer;
    buffer UpdateMaterialStagingBuffer;
    VkCommandBuffer UpdateMaterialCommandBuffer;
    
    buffer SceneDescriptionBuffer;
    buffer TransformMatricesBuffer;
    storageImage StorageImage;
    storageImage AccumulationImage; 
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

    void CreateImages();
    void CreateRayTracingPipeline();
    void CreateShaderBindingTable();
    void CreateDescriptorSets();
    void UpdateUniformBuffers();    
    void CreateCommandBuffers();
    void BuildUniformBuffers();
    void BuildCommandBuffers();
};