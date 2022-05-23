#pragma once
#include "../Renderer.h"
#include "Scene.h"

enum materialType
{
    Lambertian
};

struct material
{
	glm::vec4 BaseColor;
	int32_t BaseColorTextureIndex;
	int32_t NormalTextureIndex;
	int32_t EmissionTextureIndex;
	int32_t OcclusionTextureIndex;
	int32_t MetallicRoughnessTextureIndex;    
    materialType Type;
};

struct accelerationStructure
{
    VkAccelerationStructureKHR AccelerationStructure;
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


    
    void UpdateCamera();
private:
    std::vector<accelerationStructure> BottomLevelAccelerationStructures;
    accelerationStructure TopLevelAccelerationStructure;
    
    buffer MaterialBuffer;
    buffer SceneDescriptionBuffer;
    storageImage StorageImage;
    storageImage AccumulationImage; 
    struct uniformData
    {
        glm::mat4 ViewInverse;
        glm::mat4 ProjInverse;
        uint32_t VertexSize;
        uint32_t CurrentSampleCount=0;
        uint32_t SamplersPerFrame=4;
        uint32_t RayBounces=8;
    } UniformData;
    buffer UBO;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkPipelineLayout PipelineLayout;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> ShaderGroups;
    VkPipeline Pipeline;
    VkDescriptorSet DescriptorSet;
    bool ResetAccumulation=true;

    struct shaderBindingTables
    {
        shaderBindingTable Raygen;
        shaderBindingTable Miss;
        shaderBindingTable Hit;
    } ShaderBindingTables;
    
    void CreateMaterialBuffer();
    void CreateBottomLevelAccelarationStructure(scene *Scene);
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