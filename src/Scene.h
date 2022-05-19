#pragma once


#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "TextureLoader.h"

class vulkanApp;
struct descriptor;
class textureList;


struct vertex
{
    glm::vec4 Position;
    glm::vec4 Normal;
    glm::vec4 Tangent;
};


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

//Material system:
enum materialFlags
{
    AlphaMode               = 1,
    HasMetallicRoughnessMap = 1 << 1,
    HasEmissiveMap          = 1 << 2,
    HasBaseColorMap         = 1 << 4,
    HasOcclusionMap         = 1 << 5,
    HasClearCoat            = 1 << 6,
    NumFlags                = 6
};

//Build the flags : 
//MatFlag=0;
//if(Mat->HasMtalic) MatFlag |= materialFlags::HasMetallicROughnessMAp
//....
//Store the matFlags in the material struct
//In the resources, store the pipelines with the flags as key of the map

//when building the pipelines :
//For each material, build the flag, check if it already exists in the pipeline map
//If it doesn't build the pipeline accordingly

//At render time :
//For mesh : build the pipeline of its material


struct sceneMesh
{
    // VkBuffer VertexBuffer;
    // VkDeviceMemory VertexMemory;

    // VkBuffer IndexBuffer;
    // VkDeviceMemory IndexMemory;
    buffer VertexBuffer;
    buffer IndexBuffer;
    std::vector<vertex> Vertices;
    std::vector<uint32_t> Indices;

    uint32_t IndexCount;
    uint32_t IndexBase;

    sceneMaterial *Material;

    void Destroy();
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


struct cubemap
{
    vulkanTexture Texture;
    vulkanTexture IrradianceMap;
    vulkanTexture PrefilteredMap;
    vulkanTexture BRDFLUT;
    sceneMesh Mesh;

    struct uniformData 
    {
        glm::mat4 modelMatrix;
    } UniformData;

    buffer UniformBuffer;

    VkDescriptorSet DescriptorSet;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkDescriptorPool DescriptorPool;

    VkPipeline Pipeline;

    void Destroy(vulkanDevice *VulkanDevice);
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
    void Destroy();
    
    VkDescriptorSetLayout MaterialDescriptorSetLayout;
    VkDescriptorSetLayout InstanceDescriptorSetLayout;
    
    buffer VertexBuffer;
    buffer IndexBuffer;
    std::vector<sceneMaterial> Materials;
    std::vector<sceneMesh> Meshes;
    std::vector<instance> Instances;
    cubemap Cubemap;
    
    textureList *Textures;
    
    scene(vulkanApp *App);
    
    void Load(std::string FileName, VkCommandBuffer CopyCommand);
    void CreateDescriptorSets();
};