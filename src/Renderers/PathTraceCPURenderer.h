#pragma once
#include "../Renderer.h"
#include <functional>
#include <thread>
#include <mutex>
#include "../Image.h"
#include "Scene.h"

struct threadPool
{
    void Start();
    void AddJob(const std::function<void()>& Job);
    void Stop();
    bool Busy();

    bool ShouldTerminate=false;
    std::mutex QueueMutex;
    
    //Mutex for access of the jobs queue
    std::condition_variable MutexCondition;

    std::vector<std::thread> Threads;
    std::queue<std::function<void()>> Jobs;  
};

struct ray
{
    glm::vec3 Origin;
    glm::vec3 Direction;
    glm::vec3 InverseDirection;
};

struct triangle
{
    glm::vec3 v0, v1, v2; 
    glm::vec3 Normal0, Normal1, Normal2; 
    glm::vec3 Centroid;
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
    glm::vec3 Min, Max;
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
};


struct bvh
{
    bvh(std::vector<uint32_t> &Indices, std::vector<vertex> &Vertices, glm::mat4 Transform);
    void Build();
    void Refit();
    void Intersect(ray Ray, rayPayload &RayPayload);

    void Subdivide(uint32_t NodeIndex);
    void UpdateNodeBounds(uint32_t NodeIndex);
    float FindBestSplitPlane(bvhNode &Node, int &Axis, float &SplitPosition);

    float EvaluateSAH(bvhNode &Node, int Axis, float Position);
    float CalculateNodeCost(bvhNode &Node);

    std::vector<bvhNode> BVHNodes;
    std::vector<triangle> Triangles;
    std::vector<uint32_t> TriangleIndices;
    uint32_t NodesUsed=1;
    uint32_t TriangleCount=0;
    uint32_t RootNodeIndex=0;

    glm::mat4 InvTransform;
    aabb Bounds;
};

struct tlasNode
{
    glm::vec3 AABBMin;
    uint32_t LeftRight;
    glm::vec3 AABBMax;
    uint32_t BLAS;
    bool IsLeaf() {return LeftRight==0;}
};

struct tlas
{
    tlas(std::vector<bvh>* Instances);
    tlas();
    void Build();
    void Intersect(ray Ray, rayPayload &RayPayload);

    int FindBestMatch(std::vector<int>& List, int N, int A);

    //Instances
    std::vector<bvh>* BLAS;
    
    std::vector<tlasNode> Nodes;

    uint32_t NodesUsed=0;
};

void RayTriangleInteresection(ray Ray, triangle &Triangle, rayPayload &RayPayload);
float RayAABBIntersection(ray Ray, glm::vec3 AABBMin,glm::vec3 AABBMax, rayPayload &RayPayload);


class pathTraceCPURenderer : public renderer    
{
public:
    pathTraceCPURenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;
    void Resize(uint32_t Width, uint32_t Height) override;

    struct
    {
        VkCommandBuffer DrawCommandBuffer;
        VkSubmitInfo SubmitInfo;

        buffer ImageStagingBuffer;

        storageImage previewImage;
        buffer previewBuffer;
    } VulkanObjects;

    struct rgba8
    {
        uint8_t b, g, r, a;
    };
    std::vector<rgba8> Image; 
    std::vector<rgba8> PreviewImage; 

    uint32_t previewSize = 128;
    void UpdateCamera();
private:

    threadPool ThreadPool;

    std::vector<bvh> BVHs;
    tlas TLAS;

    bool ShouldPathTrace=false;
    
    bool HasPreview=false;
    bool HasPathTrace=false;

    int TileSize=64;
    

    void PathTrace();
    void Preview();
    void PathTraceTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, std::vector<rgba8>* ImageToWrite);
    void CreateCommandBuffers();

    //Random
    float RandomUnilateral();
};