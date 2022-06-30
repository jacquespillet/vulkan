#pragma once
#include "../Renderer.h"
#include <functional>
#include <thread>
#include <mutex>
#include "../Image.h"
#include "Scene.h"


#define NUM_THREADS 8

    
struct rgba8
{
    uint8_t b, g, r, a;
};

struct renderTarget
{
    uint32_t ViewportStartX=0;
    uint32_t ViewportStartY=0;
    uint32_t ViewportWidth;
    uint32_t ViewportHeight;

    uint32_t Width;
    uint32_t Height;

    std::vector<rgba8> *Color;
    std::vector<float> *Depth;
    
    
    void SetPixel(int x, int y, rgba8 Color);
    float SampleDepth(int x, int y);
    void SetDepthPixel(int x, int y, float Depth);
};

struct vertexOutData
{
    glm::vec4 Coord;
    float Intensity;
};

struct vertexOut
{
    vertexOutData Data[3];
};


struct shader
{
    virtual vertexOutData VertexShader(uint32_t Index, uint8_t TriVert)=0;
    virtual bool FragmentShader(glm::vec3 Barycentric, rgba8 &ColorOut)=0;

    std::vector<vertexOut> VertexOut;
    uint32_t NumVertexOut;
    renderTarget Framebuffer;
};

struct gouraudShader : public shader
{
    struct
    {
        float Intensity;
    } Varyings[3];
    struct
    {
        glm::mat4 *ViewProjectionMatrix;
        glm::mat4 *ModelMatrix;
    } Uniforms;
    struct
    {
        std::vector<vertex> *Vertices;
        std::vector<uint32_t> *Indices;
    } Buffers;

    vertexOutData VertexShader(uint32_t Index, uint8_t TriVert) override;
    bool FragmentShader(glm::vec3 Barycentric, rgba8 &ColorOut) override;
};

class rasterizerRenderer : public renderer    
{
public:
    rasterizerRenderer(vulkanApp *App);
    void Render() override;
    void Setup() override;    
    void Destroy() override;    
    void RenderGUI() override;
    void Resize(uint32_t Width, uint32_t Height) override;

    void StartPathTrace();

    void Rasterize();


    struct
    {
        VkCommandBuffer DrawCommandBuffer;
        VkSubmitInfo SubmitInfo;
        buffer ImageStagingBuffer;
    } VulkanObjects;

    std::vector<rgba8> Image; 
    std::vector<float> DepthBuffer;

    // std::vector<vertexOut> VertexOutData;
    std::array<std::vector<vertexOut>, NUM_THREADS> ThreadVertexOutData;

    void UpdateCamera();
private:
    gouraudShader Shader;

    void CreateCommandBuffers();

    glm::vec3  CalculateBarycentric(glm::vec3 A, glm::vec3 B,glm::vec3 C, glm::vec3 P);
    void DrawTriangle(glm::vec3 p0, glm::vec3 p1,glm::vec3 p2, shader &Shader);
};