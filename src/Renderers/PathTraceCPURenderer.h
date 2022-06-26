#pragma once
#include "../Renderer.h"
#include <functional>
#include <thread>
#include <mutex>

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
    } VulkanObjects;

    struct rgba8
    {
        uint8_t b, g, r, a;
    };
    std::vector<rgba8> Image; 

    void UpdateCamera();
private:

    threadPool ThreadPool;

    struct triangle
    {
        glm::vec3 v0, v1, v2; 
        glm::vec3 Normal0, Normal1, Normal2; 
        glm::vec3 Centroid;
    };
    struct ray
    {
        glm::vec3 Origin;
        glm::vec3 Direction;
        glm::vec3 InverseDirection;
        float t = 1e30f;
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
    };

    void RayTriangleInteresection(ray &Ray, triangle &Triangle);
    float RayAABBIntersection(ray &Ray, glm::vec3 AABBMin,glm::vec3 AABBMax);

    void InitGeometry();
    void BuildBVH();
    void UpdateNodeBounds(uint32_t NodeIndex);
    void Subdivide(uint32_t NodeIndex);
    void IntersectBVH(ray &Ray, uint32_t NodeIndex);
    float EvaluateSAH(bvhNode &Node, int Axis, float Position);
    int NumTriangles=30;
    std::vector<triangle> Triangles;
    std::vector<uint32_t> TriangleIndices;
    std::vector<bvhNode> BVHNodes;
    uint32_t RootNodeIndex=0;
    uint32_t NodesUsed=1;

    bool ShouldPathTrace=false;

    int TileSize=32;
    

    void PathTrace();
    void PathTraceTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight);
    void CreateCommandBuffers();

    //Random
    float RandomUnilateral();
};