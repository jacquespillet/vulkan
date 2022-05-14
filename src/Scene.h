#pragma once


#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "TextureLoader.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <assimp/material.h>

class vulkanApp;
struct descriptor;

struct sceneMaterial
{
    std::string Name;
    vulkanTexture Diffuse;
    vulkanTexture Specular;
    vulkanTexture Bump;
    bool HasAlpha=false;
    bool HasBump=false;
    bool HasSpecular=false;
    VkPipeline Pipeline;
};

struct sceneMesh
{
    VkBuffer VertexBuffer;
    VkDeviceMemory VertexMemory;

    VkBuffer IndexBuffer;
    VkDeviceMemory IndexMemory;

    uint32_t IndexCount;
    uint32_t IndexBase;

    VkDescriptorSet DescriptorSet;

    sceneMaterial *Material;
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
    buffer *DefaultUBO;
    vulkanApp *App;

    const aiScene *AScene;

    

    VkDescriptorPool DescriptorPool;

    void LoadMaterials();
    void LoadMeshes(VkCommandBuffer CommandBuffer);

public:
    VkDescriptorSetLayout DescriptorSetLayout;
    buffer VertexBuffer;
    buffer IndexBuffer;
    std::vector<sceneMaterial> Materials;
    std::vector<sceneMesh> Meshes;
    VkPipelineLayout PipelineLayout;
    
    scene(vulkanApp *App);
    
    void Load(std::string FileName, VkCommandBuffer CopyCommand);
    void CreateDescriptorSets(std::vector<descriptor>& Descriptors);
};