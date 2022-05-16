#pragma once


#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "TextureLoader.h"

class vulkanApp;
struct descriptor;
class textureList;

enum alphaMode
{
    Opaque, Blend, Mask
};

enum mediumType
{
    None,
    Absorb,
    Scatter,
    Emissive
};

struct materialData
{
    int BaseColorTextureID = -1;
    int MetallicRoughnessTextureID = -1;
    int NormalMapTextureID = -1;
    int EmissionMapTextureID = -1;
    int OcclusionMapTextureID = -1;

    int UseBaseColor;
    int UseMetallicRoughness;
    int UseNormalMap;
    int UseEmissionMap;
    int UseOcclusionMap;
    float Roughness;
    float AlphaCutoff;
    float Metallic;
    float OcclusionStrength;
    float EmissiveStrength;
    float ClearcoatFactor;
    float ClearcoatRoughness;
    float Exposure;
    float Opacity;
    alphaMode AlphaMode;
    glm::vec3 BaseColor;
    glm::vec3 Emission;    
};

struct sceneMaterial
{
    std::string Name;
    vulkanTexture Diffuse;
    vulkanTexture Specular;
    vulkanTexture Bump;
    bool HasAlpha=false;
    bool HasBump=false;
    bool HasSpecular=false;

    materialData MaterialData;
    buffer UniformBuffer;

    VkDescriptorSet DescriptorSet;
};

struct sceneMesh
{
    VkBuffer VertexBuffer;
    VkDeviceMemory VertexMemory;

    VkBuffer IndexBuffer;
    VkDeviceMemory IndexMemory;

    uint32_t IndexCount;
    uint32_t IndexBase;

    sceneMaterial *Material;
};

struct instance
{
    sceneMesh *Mesh;
    struct 
    {
        glm::mat4 Transform;
    } InstanceData;
    
    buffer UniformBuffer;
    VkDescriptorSet DescriptorSet;
};

struct vertex
{
    glm::vec3 Position;
    glm::vec2 UV;
    glm::vec3 Color;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

class scene
{
private:
    VkDevice Device;
    VkQueue Queue;
    textureLoader *TextureLoader;
    vulkanApp *App;
    

    VkDescriptorPool MaterialDescriptorPool;
    VkDescriptorPool InstanceDescriptorPool;

    void LoadMaterials(VkCommandBuffer CommandBuffer);
    void LoadMeshes(VkCommandBuffer CommandBuffer);

public:
    VkDescriptorSetLayout MaterialDescriptorSetLayout;
    VkDescriptorSetLayout InstanceDescriptorSetLayout;
    buffer VertexBuffer;
    buffer IndexBuffer;
    std::vector<sceneMaterial> Materials;
    std::vector<sceneMesh> Meshes;
    std::vector<instance> Instances;
    
    textureList *Textures;
    
    scene(vulkanApp *App);
    
    void Load(std::string FileName, VkCommandBuffer CopyCommand);
    void CreateDescriptorSets();
};