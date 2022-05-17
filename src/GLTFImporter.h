#pragma once
#include <string>
#include <vector>

struct vertex;
struct meshBuffer;
struct sceneMesh;
struct sceneMaterial;
struct instance;
class textureList;

namespace GLTFImporter
{
    bool Load(std::string FileName,  std::vector<instance> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures);
    
    bool LoadMesh(std::string FileName, sceneMesh &Mesh);
    
};