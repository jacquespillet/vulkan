#include "App.h"
#include "Renderers/ForwardRenderer.h"
#include "Renderers/DeferredRenderer.h"
#include "imgui.h"

void vulkanApp::InitVulkan()
{
    Instance = vulkanTools::CreateInstance(EnableValidation);

    //Set debug report callback        
    if(EnableValidation)
    {
        VkDebugReportFlagsEXT DebugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
        vulkanDebug::SetupDebugReportCallback(Instance, DebugReportFlags);
    }

    //Pick physical device
    uint32_t GpuCount=0;
    VK_CALL(vkEnumeratePhysicalDevices(Instance, &GpuCount, nullptr));
    assert(GpuCount >0);
    std::vector<VkPhysicalDevice> PhysicalDevices(GpuCount);
    VkResult Error = vkEnumeratePhysicalDevices(Instance, &GpuCount, PhysicalDevices.data());
    if(Error)
    {
        assert(0);
    }
    PhysicalDevice = PhysicalDevices[0];

    //Build logical device
    VulkanDevice = new vulkanDevice(PhysicalDevice);
    VK_CALL(VulkanDevice->CreateDevice(EnabledFeatures));
    Device = VulkanDevice->Device;

    //Get queue
    vkGetDeviceQueue(Device, VulkanDevice->QueueFamilyIndices.Graphics, 0, &Queue);
    
    //Get depth format
    VkBool32 ValidDepthFormat = vulkanTools::GetSupportedDepthFormat(PhysicalDevice, &DepthFormat);
    assert(ValidDepthFormat);

    //Build main semaphores
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.PresentComplete));
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.RenderComplete));
      
    Swapchain.Initialize(Instance, PhysicalDevice, Device);
}

void vulkanApp::SetupDepthStencil()
{
    //Image 
    VkImageCreateInfo Image {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    Image.pNext=nullptr;
    Image.imageType = VK_IMAGE_TYPE_2D;
    Image.format = DepthFormat;
    Image.extent = {Width, Height, 1};
    Image.mipLevels=1;
    Image.arrayLayers=1;
    Image.samples = VK_SAMPLE_COUNT_1_BIT;
    Image.tiling=VK_IMAGE_TILING_OPTIMAL;
    Image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    Image.flags=0;

    //Mem allocation
    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext=nullptr;
    MemoryAllocateInfo.allocationSize=0;
    MemoryAllocateInfo.memoryTypeIndex=0;

    //Image view
    VkImageViewCreateInfo DepthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    DepthStencilView.pNext=nullptr;
    DepthStencilView.viewType=VK_IMAGE_VIEW_TYPE_2D;
    DepthStencilView.format=DepthFormat;
    DepthStencilView.flags=0;
    DepthStencilView.subresourceRange={};
    DepthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    DepthStencilView.subresourceRange.baseMipLevel=0;
    DepthStencilView.subresourceRange.levelCount=1;
    DepthStencilView.subresourceRange.baseArrayLayer=0;
    DepthStencilView.subresourceRange.layerCount=1;

    //Create image
    VK_CALL(vkCreateImage(Device, &Image, nullptr, &DepthStencil.Image));
    
    //Get mem requirements for this image
    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(Device, DepthStencil.Image, &MemoryRequirements);
    
    //Allocate memory
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemoryAllocateInfo, nullptr, &DepthStencil.Memory));
    VK_CALL(vkBindImageMemory(Device, DepthStencil.Image, DepthStencil.Memory, 0));

    //Create view
    DepthStencilView.image = DepthStencil.Image;
    VK_CALL(vkCreateImageView(Device, &DepthStencilView, nullptr, &DepthStencil.View));
}


void vulkanApp::SetupRenderPass()
{
    std::array<VkAttachmentDescription, 2> Attachments = {};

    Attachments[0].format = Swapchain.ColorFormat;
    Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Attachments[1].format = DepthFormat;
    Attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    Attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference ColorReference  = {};
    ColorReference.attachment=0;
    ColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference DepthReference  = {};
    DepthReference.attachment=1;
    DepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription SubpassDescription = {};
    SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SubpassDescription.colorAttachmentCount=1;
    SubpassDescription.pColorAttachments = &ColorReference;
    SubpassDescription.pDepthStencilAttachment = &DepthReference;
    SubpassDescription.inputAttachmentCount=0;
    SubpassDescription.pInputAttachments=nullptr;
    SubpassDescription.preserveAttachmentCount=0;
    SubpassDescription.pPreserveAttachments=nullptr;
    SubpassDescription.pResolveAttachments=nullptr;

    std::array<VkSubpassDependency, 2> Dependencies;

    Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependencies[0].dstSubpass=0;
    Dependencies[0].srcStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[0].srcAccessMask=VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[0].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

    Dependencies[1].srcSubpass = 0;
    Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    Dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[1].dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[1].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo RenderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassCreateInfo.attachmentCount=static_cast<uint32_t>(Attachments.size());
    RenderPassCreateInfo.pAttachments=Attachments.data();
    RenderPassCreateInfo.subpassCount=1;
    RenderPassCreateInfo.pSubpasses=&SubpassDescription;
    RenderPassCreateInfo.dependencyCount=static_cast<uint32_t>(Dependencies.size());
    RenderPassCreateInfo.pDependencies = Dependencies.data();

    VK_CALL(vkCreateRenderPass(Device, &RenderPassCreateInfo, nullptr, &RenderPass));
}

void vulkanApp::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo PipelineCacheCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CALL(vkCreatePipelineCache(Device, &PipelineCacheCreateInfo, nullptr, &PipelineCache));
}

void vulkanApp::SetupFramebuffer()
{
    VkImageView Attachments[2];

    Attachments[1] = DepthStencil.View;

    VkFramebufferCreateInfo FramebufferCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferCreateInfo.pNext=nullptr;
    FramebufferCreateInfo.renderPass = RenderPass;
    FramebufferCreateInfo.attachmentCount=2;
    FramebufferCreateInfo.pAttachments=Attachments;
    FramebufferCreateInfo.width=Width;
    FramebufferCreateInfo.height=Height;
    FramebufferCreateInfo.layers=1;

    AppFramebuffers.resize(Swapchain.ImageCount);
    for(uint32_t i=0; i<AppFramebuffers.size(); i++)
    {
        Attachments[0] = Swapchain.Buffers[i].View;
        VK_CALL(vkCreateFramebuffer(Device, &FramebufferCreateInfo, nullptr, &AppFramebuffers[i]));
    }
}

void vulkanApp::BuildScene()
{
    VkCommandBuffer CopyCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    Scene = new scene(this);
    //  Scene->Load("resources/models/sponza/sponza.dae", CopyCommand);
    // Scene->Load("D:\\models\\2.0\\Sponza\\glTF\\Sponza.gltf", CopyCommand);
    Scene->Load("D:/Boulot/Models/Sponza_gltf/glTF/Sponza.gltf", CopyCommand);

    vkFreeCommandBuffers(VulkanDevice->Device, CommandPool, 1, &CopyCommand);
}

void vulkanApp::BuildVertexDescriptions()
{
    VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Tangent))
    };  

    VerticesDescription.InputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    VerticesDescription.InputState.vertexBindingDescriptionCount = (uint32_t)VerticesDescription.BindingDescription.size();
    VerticesDescription.InputState.pVertexBindingDescriptions = VerticesDescription.BindingDescription.data();
    VerticesDescription.InputState.vertexAttributeDescriptionCount = (uint32_t)VerticesDescription.AttributeDescription.size();
    VerticesDescription.InputState.pVertexAttributeDescriptions = VerticesDescription.AttributeDescription.data();
}

void vulkanApp::CreateGeneralResources()
{
    CommandPool = vulkanTools::CreateCommandPool(Device, Swapchain.QueueNodeIndex); //Shared
    Swapchain.Create(&Width, &Height, EnableVSync); //Shared
    
    SetupDepthStencil(); //Shared
    
    SetupRenderPass(); //Shared
    CreatePipelineCache(); //Shared
    SetupFramebuffer(); //Shared

    TextureLoader = new textureLoader(VulkanDevice, Queue, CommandPool); //Shared
    
    BuildScene(); //Shared
    BuildVertexDescriptions(); //Shared


	ImGuiHelper = new ImGUI(this);
	ImGuiHelper->Init((float)Width, (float)Height);
	ImGuiHelper->InitResources(RenderPass, Queue);
    ImGuiHelper->InitStyle();

    // Renderer = new deferredRenderer(this);
    Renderer = new forwardRenderer(this);
    Renderer->Setup();
}

void vulkanApp::Initialize(HWND Window)
{
    //Common for any app
    Width = 1920;
    Height = 1080;
    InitVulkan();
    Swapchain.InitSurface(Window);

    CreateGeneralResources();
}

void vulkanApp::RenderGUI()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)Width, (float)Height);
    io.MousePos = ImVec2(Mouse.PosX, Mouse.PosY);
    io.MouseDown[0] = Mouse.Left;
    io.MouseDown[1] = Mouse.Right;    

    //Render scene gui
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(GuiWidth, (float)Height));
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);

    Renderer->ViewportStart=0;

    static int CurrentSceneItemIndex = -1;
    if(ImGui::Begin("Parameters"))
    {
        Renderer->ViewportStart += GuiWidth;

        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                for (int i = 0; i < Scene->Instances.size(); i++)
                {
                    const bool IsSelected = (CurrentSceneItemIndex == i);
                    
                    if (ImGui::Selectable(Scene->Instances[i].Name.c_str(), IsSelected))
                    {
                        CurrentSceneItemIndex = i;
                    }

                    if (IsSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                        Scene->Instances[i].InstanceData.Selected=1;
                        Scene->Instances[i].UploadUniform();
                    }
                    else //Mesh no longer selected
                    {
                        if(Scene->Instances[i].InstanceData.Selected > 0)
                        {
                            Scene->Instances[i].InstanceData.Selected=0;
                            Scene->Instances[i].UploadUniform();                 
                        }
                    }

                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Environment"))
            {
                ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Renderer"))
            {
                ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Lights"))
            {
                ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }    


        if(CurrentSceneItemIndex>=0)
        {
            ImGui::SetNextWindowSize(ImVec2((float)GuiWidth, (float)Height));
            ImGui::SetNextWindowPos(ImVec2((float)GuiWidth,0), ImGuiCond_Always);
            if(ImGui::Begin(Scene->Instances[CurrentSceneItemIndex].Name.c_str()))
            {
                Renderer->ViewportStart+=GuiWidth;
                bool UpdateTransform=false;
                if(ImGui::CollapsingHeader("Transform"))
                {
                    UpdateTransform |= ImGui::DragFloat3("Position", glm::value_ptr(Scene->Instances[CurrentSceneItemIndex].Position), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Rotation", glm::value_ptr(Scene->Instances[CurrentSceneItemIndex].Rotation), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Scale", glm::value_ptr(Scene->Instances[CurrentSceneItemIndex].Scale), 0.01f);

                    if(UpdateTransform) Scene->Instances[CurrentSceneItemIndex].RecalculateModelMatrixAndUpload();
                }

                if(ImGui::CollapsingHeader("Material"))
                {
                }

            }
            ImGui::End();
        }   

    }

    ImGui::End();
     
    
  



    // ImGui::ShowDemoWindow();

    Renderer->RenderGUI();
    std::cout << ImGui::IsAnyItemHovered() << std::endl;
    if(ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused()) Renderer->Camera.Locked=true;
    else Renderer->Camera.Locked=false;
    

    // Render to generate draw buffers
    ImGui::Render();
}


void vulkanApp::Render()
{
    RenderGUI();
    Renderer->Render();
}

void vulkanApp::MouseMove(float XPosition, float YPosition)
{
    Renderer->Camera.mouseMoveEvent(XPosition, YPosition);
    Mouse.PosX = XPosition;
    Mouse.PosY = YPosition;
}

void vulkanApp::MouseAction(int Button, int Action, int Mods)
{
    if(Action == GLFW_PRESS)
    {
        if(Button==0) Mouse.Left=true;
        if(Button==1) Mouse.Right=true;
        Renderer->Camera.mousePressEvent(Button);
    }
    else if(Action == GLFW_RELEASE)
    {
        if(Button==0) Mouse.Left=false;
        if(Button==1) Mouse.Right=false;
        Renderer->Camera.mouseReleaseEvent(Button);
    }
}

void vulkanApp::Scroll(float YOffset)
{
    Renderer->Camera.Scroll(YOffset);
}

void vulkanApp::DestroyGeneralResources()
{
    
    Renderer->Destroy(); 

    Scene->Destroy();
    
    delete ImGuiHelper;

    TextureLoader->Destroy();
    
    for(int i=0; i<AppFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(Device, AppFramebuffers[i], nullptr);
    }
    vkDestroyPipelineCache(Device, PipelineCache, nullptr);
    vkDestroyRenderPass(Device, RenderPass, nullptr);
    
    vkDestroyImageView(Device, DepthStencil.View, nullptr);
    vkDestroyImage(Device, DepthStencil.Image, nullptr);
    vkFreeMemory(Device, DepthStencil.Memory, nullptr);
    
    Swapchain.Destroy();
    
    vkDestroyCommandPool(Device, CommandPool, nullptr);
    

   
}

void vulkanApp::Destroy()
{
    DestroyGeneralResources();
    

    vkDestroySemaphore(Device, Semaphores.PresentComplete, nullptr);
    vkDestroySemaphore(Device, Semaphores.RenderComplete, nullptr);
    vkDestroyDevice(Device, nullptr);
//    vulkanDebug::DestroyDebugReportCallback(Instance, vulkanDebug::DebugReportCallback, nullptr);
    vkDestroyInstance(Instance, nullptr);

    delete Renderer;
    delete TextureLoader;
    delete Scene;
    system("pause");
}