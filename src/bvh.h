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
    glm::vec3 v0, v1, v2; 
    glm::vec3 Centroid;
};

struct triangleExtraData
{
    glm::vec3 Normal0, Normal1, Normal2; 
    glm::vec2 UV0, UV1, UV2; 
};

struct bvhNode
{
    glm::vec3 AABBMin, AABBMax;
    uint32_t LeftChildOrFirst;
    uint32_t TriangleCount;
    bool IsLeaf();
};
struct aabb
{
    glm::vec3 Min =glm::vec3(1e30f);
    glm::vec3 Max =glm::vec3(-1e30f);
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
    mesh(std::vector<uint32_t> &Indices, std::vector<vertex> &Vertices);
    bvh *BVH;
    std::vector<triangle> Triangles;
    std::vector<triangleExtraData> TrianglesExtraData;
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
    bvhInstance(bvh *Blas, glm::mat4 Transform, uint32_t Index) : BVH(Blas), Index(Index)
    {
        SetTransform(Transform);
    }
    void SetTransform(glm::mat4 &Transform);
    void Intersect(ray Ray, rayPayload &RayPayload);

    uint32_t Index=0;
    bvh *BVH;
    glm::mat4 InverseTransform;
    aabb Bounds;
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
