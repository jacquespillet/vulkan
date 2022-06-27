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
#include "Framebuffer.h"
#include "Buffer.h"
#include "Shader.h"

#include "Scene.h"

#include "Camera.h"
#include "ImguiHelper.h"

#include "ObjectPicker.h"

class renderer;
class vulkanApp
{
public:
#if _DEBUG_
    bool EnableValidation=true;
#else
    bool EnableValidation=false;
#endif
    bool EnableVSync=true;

    struct 
    {
        bool Left=false;
        bool Right=false;
        float PosX;
        float PosY;
        float Wheel;
        float MousePressDelta;
        float PosXPress;
        float PosYPress;
    } Mouse;

    //High level objects
    struct {
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

        VkPipelineStageFlags SubmitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkCommandPool CommandPool;
        std::vector<VkFramebuffer> AppFramebuffers;
        struct {
            VkImage Image;
            VkDeviceMemory Memory;
            VkImageView View;
        } DepthStencil;
        VkRenderPass RenderPass;
        VkPipelineCache PipelineCache;
        uint32_t CurrentBuffer=0;
        
        struct 
        {
            VkPipelineVertexInputStateCreateInfo InputState;
            std::vector<VkVertexInputBindingDescription> BindingDescription;
            std::vector<VkVertexInputAttributeDescription> AttributeDescription;
        } VerticesDescription;
        textureLoader *TextureLoader;
    } VulkanObjects;


    scene *Scene;
    uint32_t Width, Height;

    std::vector<renderer*> Renderers;
    int CurrentRenderer=0;

    ImGUI *ImGuiHelper;

    objectPicker ObjectPicker;

    float GuiWidth=200;

    bool RayTracing=true;
    
    void InitVulkan();

    void SetupDepthStencil();

    void SetupRenderPass();

    void CreatePipelineCache();

    void SetupFramebuffer();

    void CreateGeneralResources();

    void BuildVertexDescriptions();

    void BuildScene();
    
    void Render();

    
    uint32_t CurrentSceneItemIndex = UINT32_MAX;
    void RenderGUI();
    void SetSelectedItem(uint32_t Index, bool UnselectIfAlreadySelected=true);

    void Initialize(HWND Window);

    void Destroy();
    void DestroyGeneralResources();

    void MouseMove(float XPosition, float YPosition);
    void MouseAction(int Button, int Action, int Mods);
    void Scroll(float YOffset);
    void Resize(uint32_t Width, uint32_t Height);
    void KeyEvent(int KeyCode, int Action);
};
