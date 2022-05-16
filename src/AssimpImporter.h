#pragma once
#include <string>
#include <vector>

struct vertex;
struct sceneMesh;
struct sceneMaterial;
struct instance;
class textureList;

namespace assimpImporter
{
    void Load(std::string name, std::vector<instance> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures);
}