#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \


#define SizeOfArray(array) \
    sizeof(array) / sizeof(array[0]) \

#define VERTEX_BUFFER_BIND_ID 0
#define SSAO_KERNEL_SIZE 32
#define SSAO_RADIUS 2.0f
#define SSAO_NOISE_DIM 4


#include <assert.h>
#include <vector>
#include <array>
#include <string>
#include <iostream>
#include <random>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <glm/ext.hpp>

#include "Debug.h"
#include "Tools.h"
#include "Device.h"
#include "Swapchain.h"
#include "TextureLoader.h"

#include "Resources.h"
#include "Mesh.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Shader.h"

#include "Scene.h"

#include "Camera.h"

struct descriptor
{
    VkShaderStageFlags Stage;
    enum type
    {
        Image,
        Uniform
    } Type;
    VkImageView ImageView;
    VkSampler Sampler;
    VkDescriptorImageInfo DescriptorImageInfo;
    VkDescriptorBufferInfo DescriptorBufferInfo;
    VkDescriptorType DescriptorType;

    descriptor(VkShaderStageFlags Stage, VkImageView ImageView, VkSampler Sampler) :
                Stage(Stage), ImageView(ImageView), DescriptorImageInfo(DescriptorImageInfo), Sampler(Sampler)
    {
        Type = Image;
        DescriptorImageInfo = vulkanTools::BuildDescriptorImageInfo(Sampler, ImageView, VK_IMAGE_LAYOUT_GENERAL);
        DescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    
    descriptor(VkShaderStageFlags Stage, VkDescriptorBufferInfo DescriptorBufferInfo) : 
                Stage(Stage), DescriptorBufferInfo(DescriptorBufferInfo)
    {
        Type = Uniform;
        DescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }    
};

struct resources
{
    descriptorSetLayoutList *DescriptorSetLayouts;
    pipelineLayoutList *PipelineLayouts;
    pipelineList *Pipelines;
    descriptorSetList *DescriptorSets;
    textureList *Textures;
};

class vulkanApp
{
public:
    bool EnableValidation=true;
    bool EnableVSync=true;

    //High level objects
    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    vulkanDevice *VulkanDevice;
    VkDevice Device;
    VkPhysicalDeviceFeatures EnabledFeatures = {};
    VkQueue Queue;
    VkFormat DepthFormat;
    swapchain Swapchain;
    struct {
        VkSemaphore PresentComplete;
        VkSemaphore RenderComplete;
    } Semaphores;
    VkSubmitInfo SubmitInfo;
    VkPipelineStageFlags SubmitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    //General Vulkan resources
    VkCommandPool CommandPool;
    VkCommandBuffer SetupCommandBuffer = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> DrawCommandBuffers;
    std::vector<VkFramebuffer> AppFramebuffers;

    struct {
        VkImage Image;
        VkDeviceMemory Memory;
        VkImageView View;
    } DepthStencil;

    VkRenderPass RenderPass;
    VkPipelineCache PipelineCache;

    textureLoader *TextureLoader;

    uint32_t CurrentBuffer=0;

    bool EnableSSAO=true;

    //Deferred Renderer resources
    struct vertex
    {
        glm::vec3 Position;
        glm::vec2 UV;
        glm::vec3 Color;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
    };

    VkDescriptorPool DescriptorPool;
    resources Resources;

    struct 
    {
        meshBuffer Quad;
        meshBuffer SkySphere;
    } Meshes;

    struct 
    {
        VkPipelineVertexInputStateCreateInfo InputState;
        std::vector<VkVertexInputBindingDescription> BindingDescription;
        std::vector<VkVertexInputAttributeDescription> AttributeDescription;
    } VerticesDescription;


    struct 
    {
        struct offscreen : public framebuffer {
        } Offscreen;
        struct SSAO : public framebuffer {
        } SSAO, SSAOBlur;
    } Framebuffers;


    VkCommandBuffer OffscreenCommandBuffer = VK_NULL_HANDLE;
    bool Rebuild=false;

    VkSemaphore OffscreenSemaphore;

    //Uniforms
    struct
    {
        buffer FullScreen;
        buffer SceneMatrices;
        buffer SceneLights;
        buffer SSAOKernel;
        buffer SSAOParams;
    } UniformBuffers;

    struct light
    {
        glm::vec4 Position;
        glm::vec4 Color;
        float radius;
        float QuadraticFalloff;
        float LinearFalloff;
        float pad;
    };
    struct 
    {
        glm::mat4 Projection;
        glm::mat4 Model;
        glm::mat4 View;
        glm::vec2 ViewportDim;
    } UBOVS, UBOSceneMatrices;

    struct
    {
        light Lights[17];
        glm::vec4 viewPos;
        glm::mat4 View;
        glm::mat4 Model;
    } UBOFragmentLights;

    struct
    {
        glm::mat4 Projection;
        uint32_t SSAO=true;
        uint32_t SSAOOnly=false;
        uint32_t SSAOBlur=true;
    } UBOSSAOParams;

    struct 
    {
        vulkanTexture SSAONoise;
    } Textures;



    std::vector<VkShaderModule> ShaderModules;

    scene *Scene;

    //App
    uint32_t Width, Height;
    
    float t=0;
    
    camera Camera;
    void InitVulkan();

    void SetupWindow();

    //
    
    void CreateCommandBuffers();

    void SetupDepthStencil();

    void SetupRenderPass();

    void CreatePipelineCache();

    void SetupFramebuffer();


    void CreateGeneralResources();


    //
    void SetupDescriptorPool();

    void BuildQuads();

    void BuildVertexDescriptions();

    void BuildOffscreenBuffers();

    void UpdateUniformBufferScreen();

    void UpdateUniformBufferDeferredMatrices();

    void UpdateUniformBufferSSAOParams();

    void UpdateCamera();

    inline float Lerp(float a, float b, float f);

    void BuildUniformBuffers();

    void BuildLayoutsAndDescriptors();

    void BuildPipelines();

    void BuildScene();
    
    void BuildCommandBuffers();
    
    void BuildDeferredCommandBuffers();

    void CreateDeferredRendererResources();

    void Render();

    void Initialize(HWND Window);

    void Destroy();

    void MouseMove(float XPosition, float YPosition);
    void MouseAction(int Button, int Action, int Mods);
    void Scroll(float YOffset);
};
