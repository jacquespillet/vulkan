#pragma once
#include <string>
#include <vector>
#include <unordered_map>

#include <vulkan/vulkan.h>

struct vertex;
struct meshBuffer;
struct sceneMesh;
struct sceneMaterial;
struct instance;
class textureList;
class vulkanDevice;

namespace GLTFImporter
{
    bool Load(std::string FileName,  std::unordered_map<int, std::vector<instance>> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures, float Size);
    
    bool LoadMesh(std::string FileName, sceneMesh &Mesh, vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue);
    
};