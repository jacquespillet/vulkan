#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct vertex;
struct sceneMesh;
struct sceneMaterial;
struct instance;
class textureList;

namespace assimpImporter
{
    void Load(std::string name, std::unordered_map<int, std::vector<instance>> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures, float Size);
}