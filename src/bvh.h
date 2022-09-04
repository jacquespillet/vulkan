#pragma once

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <vector>
#include "Scene.h"

struct ray
{
    glm::vec3 Origin;
    glm::vec3 Direction;
    glm::vec3 InverseDirection;
};

struct triangle
{
    glm::vec3 v0;
    float padding0;
    glm::vec3 v1;
    float padding1;
    glm::vec3 v2;
    float padding2; 
    glm::vec3 Centroid;
    float padding3; 
};

struct triangleExtraData
{
    glm::vec3 Normal0; 
    float padding0;
    glm::vec3 Normal1; 
    float padding1;
    glm::vec3 Normal2; 
    float padding2;
    glm::vec2 UV0, UV1, UV2; 
    glm::vec2 padding3;
};

struct bvhNode
{
    glm::vec3 AABBMin;
    float padding0;
    glm::vec3 AABBMax;
    float padding1;
    uint32_t LeftChildOrFirst;
    uint32_t TriangleCount;
    glm::uvec2 padding2;
    bool IsLeaf();
};
struct aabb
{
    glm::vec3 Min =glm::vec3(1e30f);
    float pad0;
    glm::vec3 Max =glm::vec3(-1e30f);
    float pad1;
    float Area();
    void Grow(glm::vec3 Position);
    void Grow(aabb &AABB);
};
struct bin
{
    aabb Bounds;
    uint32_t TrianglesCount=0;
};

struct rayPayload
{
    float Distance;
    float U, V;
    glm::vec3 Emission;
    uint32_t InstanceIndex;
    uint32_t PrimitiveIndex;
    uint32_t RandomState;
    uint8_t Depth;
    // uint32_t InstPrim;
};


struct mesh;

struct bvh
{
    bvh(mesh *Mesh);
    void Build();
    void Refit();
    void Intersect(ray Ray, rayPayload &RayPayload, uint32_t InstanceIndex);

    void Subdivide(uint32_t NodeIndex);
    void UpdateNodeBounds(uint32_t NodeIndex);
    float FindBestSplitPlane(bvhNode &Node, int &Axis, float &SplitPosition);

    float EvaluateSAH(bvhNode &Node, int Axis, float Position);
    float CalculateNodeCost(bvhNode &Node);

    mesh *Mesh;
    
    std::vector<uint32_t> TriangleIndices;

    std::vector<bvhNode> BVHNodes;
    uint32_t NodesUsed=1;
    uint32_t RootNodeIndex=0;
};

struct mesh
{
    mesh(std::vector<uint32_t> &Indices, std::vector<vertex> &Vertices, uint32_t MaterialIndex);
    bvh *BVH;
    std::vector<triangle> Triangles;
    std::vector<triangleExtraData> TrianglesExtraData;
    uint32_t MaterialIndex;
};



struct tlasNode
{
    glm::vec3 AABBMin;
    uint32_t LeftRight;
    glm::vec3 AABBMax;
    uint32_t BLAS;
    bool IsLeaf() {return LeftRight==0;}
};

struct bvhInstance
{
    bvhInstance(std::vector<mesh*> *Meshes, uint32_t MeshIndex, glm::mat4 Transform, uint32_t Index) : Meshes(Meshes), MeshIndex(MeshIndex), Index(Index)
    {
        SetTransform(Transform);
    }
    void SetTransform(glm::mat4 &Transform);
    void Intersect(ray Ray, rayPayload &RayPayload);

    //Store the mesh index in the scene instead, and a pointer to the mesh array to access the bvh.
    glm::mat4 InverseTransform;
    aabb Bounds;

    uint32_t MeshIndex;
    uint32_t Index=0;
    glm::ivec2 pad0;
    std::vector<mesh*> *Meshes;
    glm::ivec2 pad1;
    
};

struct tlas
{
    tlas(std::vector<bvhInstance>* Instances);
    tlas();
    void Build();
    void Intersect(ray Ray, rayPayload &RayPayload);

    int FindBestMatch(std::vector<int>& List, int N, int A);

    //Instances
    std::vector<bvhInstance>* BLAS;
    
    std::vector<tlasNode> Nodes;

    uint32_t NodesUsed=0;
};

void RayTriangleInteresection(ray Ray, triangle &Triangle, rayPayload &RayPayload, uint32_t InstanceIndex, uint32_t PrimitiveIndex);
float RayAABBIntersection(ray Ray, glm::vec3 AABBMin,glm::vec3 AABBMax, rayPayload &RayPayload);
