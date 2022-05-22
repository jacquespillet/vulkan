#include "App.h"
#include "Renderers/ForwardRenderer.h"
#include "Renderers/DeferredRenderer.h"
#include "Renderers/PathTraceRTXRenderer.h"
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
    VulkanDevice = new vulkanDevice(PhysicalDevice, Instance);
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
    // Scene->Load("D:/Boulot/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", CopyCommand);
    // Scene->Load("D:\\Boulot\\Models\\Lantern\\glTF\\Lantern.gltf", CopyCommand);
    // Scene->Load("D:\\Boulot\\Models\\Cube\\glTF\\Cube.gltf", CopyCommand);

    vkFreeCommandBuffers(VulkanDevice->Device, CommandPool, 1, &CopyCommand);

    Scene->CreateDescriptorSets();
}

void vulkanApp::BuildVertexDescriptions()
{
    VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Tangent)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, MatInx))
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

    Renderers.push_back(new forwardRenderer(this));
    Renderers.push_back(new deferredRenderer(this));
    if(RayTracing) Renderers.push_back(new pathTraceRTXRenderer(this));
    for(int i=0; i<Renderers.size(); i++)
    {
        Renderers[i]->Setup();
    }
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

    renderer *Renderer = Renderers[CurrentRenderer];

    Scene->ViewportStart=0;

    static int CurrentSceneItemIndex = -1;
    static int CurrentSceneGroupIndex=-1;
    if(ImGui::Begin("Parameters"))
    {
        Scene->ViewportStart += GuiWidth;

        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                for (auto& InstanceGroup: Scene->Instances) {
                    for (int i = 0; i < InstanceGroup.second.size(); i++)
                    {
                        const bool IsSelected = (CurrentSceneItemIndex == i);
                        if (ImGui::Selectable(InstanceGroup.second[i].Name.c_str(), IsSelected))
                        {
                            if(IsSelected) CurrentSceneItemIndex=-1; //Unselect if click on selected item
                            else {
                                CurrentSceneGroupIndex = InstanceGroup.first;
                                CurrentSceneItemIndex = i;
                            }
                        }

                        if (IsSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                            InstanceGroup.second[i].InstanceData.Selected=1;
                            InstanceGroup.second[i].UploadUniform();
                        }
                        else //Mesh no longer selected
                        {
                            if(InstanceGroup.second[i].InstanceData.Selected > 0)
                            {
                                InstanceGroup.second[i].InstanceData.Selected=0;
                                InstanceGroup.second[i].UploadUniform();                 
                            }
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
                static int RendererInx = 0;
                ImGui::Combo("Render Mode", &RendererInx, "Forward\0Deferred\0PathTraceRTX\0\0");
                CurrentRenderer = RendererInx;   
                
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
            if(ImGui::Begin(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Name.c_str()))
            {
                Scene->ViewportStart+=GuiWidth;
                if(ImGui::CollapsingHeader("Transform"))
                {
                    bool UpdateTransform=false;
                    UpdateTransform |= ImGui::DragFloat3("Position", glm::value_ptr(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Position), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Rotation", glm::value_ptr(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Rotation), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Scale", glm::value_ptr(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Scale), 0.01f);

                    if(UpdateTransform) Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].RecalculateModelMatrixAndUpload();
                }

                if(ImGui::CollapsingHeader("Material"))
                {
                    //If changing material, disable the colouring
                    if(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].InstanceData.Selected>0)
                    {
                        Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].InstanceData.Selected=0;
                        Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].UploadUniform();   
                    }

                    bool UpdateMaterial=false;
                    materialData *Material = &Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Mesh->Material->MaterialData;

                    UpdateMaterial |= ImGui::Checkbox("Use Metallic / Roughness Map", (bool*)&Material->UseMetallicRoughness);
                    UpdateMaterial |= ImGui::DragFloat("Roughness", &Material->Roughness, 0.01f, 0, 1);
                    UpdateMaterial |= ImGui::DragFloat("Metallic", &Material->Metallic, 0.01f, 0, 1);
                
                    ImGui::Separator();

                    UpdateMaterial |= ImGui::Checkbox("Use Occlusion Map", (bool*)&Material->UseOcclusionMap);
                    UpdateMaterial |= ImGui::DragFloat("Occlusion Strength", &Material->OcclusionStrength, 0.01f, 0, 1);
                
                    ImGui::Separator();

                    UpdateMaterial |= ImGui::Checkbox("Use Color Map", (bool*)&Material->UseBaseColor);
                    UpdateMaterial |= ImGui::ColorPicker3("Base Color", glm::value_ptr(Material->BaseColor));
                    ImGui::Separator();
                    
                    UpdateMaterial |= ImGui::Checkbox("Use Normal Map", (bool*)&Material->UseNormalMap);
                    ImGui::Separator();

                    UpdateMaterial |= ImGui::DragFloat("Opacity", &Material->Opacity, 0.01f, 0, 1);
                    UpdateMaterial |= ImGui::DragFloat("Alpha Cutoff", &Material->AlphaCutoff, 0.01f, 0, 1);
                    ImGui::Separator();

                    UpdateMaterial |= ImGui::ColorPicker3("Emission", &Material->Emission[0]);
                    UpdateMaterial |= ImGui::DragFloat("Emissive Strength", &Material->EmissiveStrength, 0.01f, 0, 5);
                    UpdateMaterial |= ImGui::Checkbox("Use Emission Map", (bool*)&Material->UseEmissionMap);

                    ImGui::Separator();

                    static int DebugChannel = 0;
                    UpdateMaterial |= ImGui::Combo("Render Mode", &DebugChannel, "None\0TexCoords\0NormalTexture\0Normal\0Tangent\0Bitangent\0ShadingNormal\0Alpha\0Occlusion \0Emission\0Metallic \0Roughness\0BaseColor\0Clearcoat\0ClearcoatFactor\0ClearcoatNormal\0ClearcoatRoughnes\0Sheen\0\0");
                    Material->DebugChannel = (debugChannel)DebugChannel;                    


                    if(UpdateMaterial)
                    {
                        Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].Mesh->Material->Upload();
                    }
                }
                else
                {
                    //If not changing material, re-enable the colouring
                    if(Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].InstanceData.Selected==0)
                    {
                        Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].InstanceData.Selected=1;
                        Scene->Instances[CurrentSceneGroupIndex][CurrentSceneItemIndex].UploadUniform();   
                    }                    
                }
            }
            ImGui::End();
        }   

    }

    ImGui::End();
     
    
  



    // ImGui::ShowDemoWindow();

    Renderer->RenderGUI();
    if(ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused()) Scene->Camera.Locked=true;
    else Scene->Camera.Locked=false;
    

    // Render to generate draw buffers
    ImGui::Render();
}


void vulkanApp::Render()
{
    RenderGUI();
    Renderers[CurrentRenderer]->Render();
}

void vulkanApp::MouseMove(float XPosition, float YPosition)
{
    Scene->Camera.mouseMoveEvent(XPosition, YPosition);
    Mouse.PosX = XPosition;
    Mouse.PosY = YPosition;
}

void vulkanApp::MouseAction(int Button, int Action, int Mods)
{
    if(Action == GLFW_PRESS)
    {
        if(Button==0) Mouse.Left=true;
        if(Button==1) Mouse.Right=true;
        Scene->Camera.mousePressEvent(Button);
    }
    else if(Action == GLFW_RELEASE)
    {
        if(Button==0) Mouse.Left=false;
        if(Button==1) Mouse.Right=false;
        Scene->Camera.mouseReleaseEvent(Button);
    }
}

void vulkanApp::Scroll(float YOffset)
{
    Scene->Camera.Scroll(YOffset);
}

void vulkanApp::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    Width = NewWidth;
    Height = NewHeight;
    Swapchain.Create(&Width, &Height, true);
}

void vulkanApp::DestroyGeneralResources()
{
    for(size_t i=0; i<Renderers.size(); i++)
    {
        Renderers[i]->Destroy(); 
    }

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

    for(size_t i=0; i<Renderers.size(); i++)
    {
        delete Renderers[i];
    }

    delete TextureLoader;
    delete Scene;
    system("pause");
}