#pragma once

#include <unordered_map>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "TextureLoader.h"
#include "Camera.h"
#include "Resources.h"

class vulkanApp;
struct descriptor;
class textureList;


struct vertex
{
    glm::vec4 Position;
    glm::vec4 Normal;
    glm::vec4 Tangent;
};


//Material system:
enum materialFlags
{
    Opaque                  = 1,
    Blend                   = 1 << 1,
    Mask                    = 1 << 2,
    HasMetallicRoughnessMap = 1 << 3,
    HasEmissiveMap          = 1 << 4,
    HasBaseColorMap         = 1 << 5,
    HasOcclusionMap         = 1 << 6,
    HasNormalMap            = 1 << 7,
    HasClearCoat            = 1 << 8,
    HasSheen                = 1 << 9,
    NumFlags                = 9
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



enum class alphaMode
{
    Opaque, Blend, Mask
};

enum class mediumType
{
    None,
    Absorb,
    Scatter,
    Emissive
};

enum class debugChannel
{
    None,
    TexCoords,
    NormalTexture,
    Normal,
    Tangent,
    Bitangent,
    ShadingNormal,
    Alpha,
    Occlusion ,
    Emission,
    Metallic ,
    Roughness,
    BaseColor,
    Clearcoat,
    ClearcoatFactor,
    ClearcoatNormal,
    ClearcoatRoughnes,
    Sheen
};

struct materialData
{
    int BaseColorTextureID = -1;
    int MetallicRoughnessTextureID = -1;
    int NormalMapTextureID = -1;
    int EmissionMapTextureID = -1;
    
    int OcclusionMapTextureID = 1;
    int UseBaseColor = 1;
    int UseMetallicRoughness = 1;
    int UseNormalMap = 1;

    int UseEmissionMap=1;
    int UseOcclusionMap=1;
    alphaMode AlphaMode=alphaMode::Opaque;
    debugChannel DebugChannel = debugChannel::None;

    float Roughness=1;
    float AlphaCutoff=1;
    float ClearcoatRoughness=1;
    float padding1;
    
    float Metallic=0;
    float OcclusionStrength=1;
    float EmissiveStrength=1;
    float ClearcoatFactor=0;
    
    glm::vec3 BaseColor = glm::vec3(1,1,1);
    float Opacity=1;
    
    glm::vec3 Emission = glm::vec3(1,1,1);
    float Exposure=1;
};

struct sceneMaterial
{
    std::string Name;
    vulkanTexture Diffuse;
    vulkanTexture Specular;
    vulkanTexture Normal;
    vulkanTexture Emission;
    vulkanTexture Occlusion;
    
    bool HasAlpha=false;
    bool HasBump=false;
    bool HasSpecular=false;

    materialData MaterialData;
    buffer UniformBuffer;

    VkDescriptorSet DescriptorSet;


    int Flags;

    void CalculateFlags()
    {
        Flags=0;
        if(MaterialData.AlphaMode==alphaMode::Opaque) Flags |= materialFlags::Opaque;
        if(MaterialData.AlphaMode==alphaMode::Blend) Flags |= materialFlags::Blend;
        if(MaterialData.AlphaMode==alphaMode::Mask) Flags |= materialFlags::Mask;

        if(MaterialData.MetallicRoughnessTextureID>=0) Flags |= materialFlags::HasMetallicRoughnessMap;
        if(MaterialData.EmissionMapTextureID>=0) Flags |= materialFlags::HasEmissiveMap;
        if(MaterialData.NormalMapTextureID>=0) Flags |= materialFlags::HasNormalMap;
        if(MaterialData.BaseColorTextureID>=0) Flags |= materialFlags::HasBaseColorMap;
        if(MaterialData.OcclusionMapTextureID>=0) Flags |= materialFlags::HasOcclusionMap;
        
        if(MaterialData.ClearcoatFactor>0) Flags |= materialFlags::HasClearCoat;
    }

    void Upload()
    {
        UniformBuffer.Map();
        UniformBuffer.CopyTo(&MaterialData, sizeof(materialData));
        UniformBuffer.Unmap();
    }
};

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
    

    glm::vec3 Position;
    glm::vec3 Rotation;
    glm::vec3 Scale;

    struct 
    {
        glm::mat4 Transform;
        glm::mat4 Normal;
        float Selected=0;
        glm::vec3 padding;
    } InstanceData;
    
    buffer UniformBuffer;
    VkDescriptorSet DescriptorSet;
    
    void RecalculateMatrix()
    {
        glm::mat4 translateM = glm::translate(glm::mat4(1.0f), Position);
        
        glm::mat4 scaleM = glm::scale(glm::mat4(1.0f), Scale);

        glm::mat4 rotxPM = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));//rot x axis
        glm::mat4 rotyPM = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));//rot y axis
        glm::mat4 rotzPM = glm::rotate(glm::mat4(1.0f), glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));//rot z axis

        glm::mat4 rotM = rotyPM * rotxPM * rotzPM; 	
            
        InstanceData.Transform = translateM * rotM * scaleM;   

        InstanceData.Normal = glm::inverseTranspose(InstanceData.Transform);
    }

    void RecalculateModelMatrixAndUpload()
    {
        RecalculateMatrix();
        UploadUniform();
    }

    void UploadUniform()
    {
        UniformBuffer.Map();
        UniformBuffer.CopyTo(&InstanceData, sizeof(InstanceData));
        UniformBuffer.Unmap();     
    }
    

    std::string Name;
};


struct cubemap
{
    vulkanTexture Texture;
    vulkanTexture IrradianceMap;
    vulkanTexture PrefilteredMap;
    vulkanTexture BRDFLUT;
    sceneMesh Mesh;

    VkDescriptorSet DescriptorSet;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkDescriptorPool DescriptorPool;

    VkPipeline Pipeline;

    struct uniformData 
    {
        glm::mat4 modelMatrix;
    } UniformData;

    buffer UniformBuffer;

    void Load(std::string FileName, textureLoader *TextureLoader, vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue);
    void CreateDescriptorSet(vulkanDevice *VulkanDevice);
    void Destroy(vulkanDevice *VulkanDevice);
};

class scene
{
private:
    VkDevice Device;
    VkQueue Queue;
    textureLoader *TextureLoader;
    vulkanApp *App;
    
    VkDescriptorPool DescriptorPool;
    
    void LoadMaterials(VkCommandBuffer CommandBuffer);
    void LoadMeshes(VkCommandBuffer CommandBuffer);


public:
    void Destroy();
    
    buffer VertexBuffer;
    buffer IndexBuffer;
    
    size_t NumInstances=0;
    std::vector<sceneMaterial> Materials;
    std::vector<sceneMesh> Meshes;
    std::unordered_map<int, std::vector<instance>> Instances;
    
    cubemap Cubemap;
    
    buffer SceneMatrices;
    
    struct 
    {
        glm::mat4 Projection;
        glm::mat4 Model;
        glm::mat4 View;
        glm::vec4 CameraPosition;
    } UBOSceneMatrices;    
    
    camera Camera;
    scene(vulkanApp *App);
    
    void Load(std::string FileName, VkCommandBuffer CopyCommand);
    void CreateDescriptorSets();

    
    void UpdateUniformBufferMatrices();
    float ViewportStart;

    resources Resources;
};