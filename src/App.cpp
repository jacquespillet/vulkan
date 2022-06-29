#include "App.h"
#include "Renderers/ForwardRenderer.h"
#include "Renderers/DeferredRenderer.h"
#include "Renderers/PathTraceRTXRenderer.h"
#include "Renderers/HybridRenderer.h"
#include "Renderers/PathTraceCPURenderer.h"
#include "Renderers/RasterizerRenderer.h"
#include "imgui.h"
#include "ImGuizmo.h"
void vulkanApp::InitVulkan()
{
    VulkanObjects.Instance = vulkanTools::CreateInstance(EnableValidation);

    //Set debug report callback        
    if(EnableValidation)
    {
        VkDebugReportFlagsEXT DebugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
        vulkanDebug::SetupDebugReportCallback(VulkanObjects.Instance, DebugReportFlags);
    }

    //Pick physical device
    uint32_t GpuCount=0;
    VK_CALL(vkEnumeratePhysicalDevices(VulkanObjects.Instance, &GpuCount, nullptr));
    assert(GpuCount >0);
    std::vector<VkPhysicalDevice> PhysicalDevices(GpuCount);
    VkResult Error = vkEnumeratePhysicalDevices(VulkanObjects.Instance, &GpuCount, PhysicalDevices.data());
    if(Error)
    {
        assert(0);
    }
    VulkanObjects.PhysicalDevice = PhysicalDevices[0];

    //Build logical device
    VulkanObjects.VulkanDevice = new vulkanDevice(VulkanObjects.PhysicalDevice, VulkanObjects.Instance);
    VK_CALL(VulkanObjects.VulkanDevice->CreateDevice(VulkanObjects.EnabledFeatures, RayTracing));
    VulkanObjects.Device = VulkanObjects.VulkanDevice->Device;

    //Get queue
    vkGetDeviceQueue(VulkanObjects.Device, VulkanObjects.VulkanDevice->QueueFamilyIndices.Graphics, 0, &VulkanObjects.Queue);
    
    //Get depth format
    VkBool32 ValidDepthFormat = vulkanTools::GetSupportedDepthFormat(VulkanObjects.PhysicalDevice, &VulkanObjects.DepthFormat);
    assert(ValidDepthFormat);

    //Build main semaphores
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(VulkanObjects.Device, &SemaphoreCreateInfo, nullptr, &VulkanObjects.Semaphores.PresentComplete));
    VK_CALL(vkCreateSemaphore(VulkanObjects.Device, &SemaphoreCreateInfo, nullptr, &VulkanObjects.Semaphores.RenderComplete));
      
    VulkanObjects.Swapchain.Initialize(VulkanObjects.Instance, VulkanObjects.PhysicalDevice, VulkanObjects.Device);
}

void vulkanApp::SetupDepthStencil()
{
    //Image 
    VkImageCreateInfo Image {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    Image.pNext=nullptr;
    Image.imageType = VK_IMAGE_TYPE_2D;
    Image.format = VulkanObjects.DepthFormat;
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
    DepthStencilView.format=VulkanObjects.DepthFormat;
    DepthStencilView.flags=0;
    DepthStencilView.subresourceRange={};
    DepthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    DepthStencilView.subresourceRange.baseMipLevel=0;
    DepthStencilView.subresourceRange.levelCount=1;
    DepthStencilView.subresourceRange.baseArrayLayer=0;
    DepthStencilView.subresourceRange.layerCount=1;

    //Create image
    VK_CALL(vkCreateImage(VulkanObjects.Device, &Image, nullptr, &VulkanObjects.DepthStencil.Image));
    
    //Get mem requirements for this image
    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(VulkanObjects.Device, VulkanObjects.DepthStencil.Image, &MemoryRequirements);
    
    //Allocate memory
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanObjects.VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanObjects.Device, &MemoryAllocateInfo, nullptr, &VulkanObjects.DepthStencil.Memory));
    VK_CALL(vkBindImageMemory(VulkanObjects.Device, VulkanObjects.DepthStencil.Image, VulkanObjects.DepthStencil.Memory, 0));

    //Create view
    DepthStencilView.image = VulkanObjects.DepthStencil.Image;
    VK_CALL(vkCreateImageView(VulkanObjects.Device, &DepthStencilView, nullptr, &VulkanObjects.DepthStencil.View));
}


void vulkanApp::SetupRenderPass()
{
    std::array<VkAttachmentDescription, 2> Attachments = {};

    Attachments[0].format = VulkanObjects.Swapchain.ColorFormat;
    Attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    Attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    Attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    Attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    Attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    Attachments[1].format = VulkanObjects.DepthFormat;
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

    VK_CALL(vkCreateRenderPass(VulkanObjects.Device, &RenderPassCreateInfo, nullptr, &VulkanObjects.RenderPass));
}

void vulkanApp::CreatePipelineCache()
{
    VkPipelineCacheCreateInfo PipelineCacheCreateInfo {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    VK_CALL(vkCreatePipelineCache(VulkanObjects.Device, &PipelineCacheCreateInfo, nullptr, &VulkanObjects.PipelineCache));
}

void vulkanApp::SetupFramebuffer()
{
    VkImageView Attachments[2];

    Attachments[1] = VulkanObjects.DepthStencil.View;

    VkFramebufferCreateInfo FramebufferCreateInfo {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferCreateInfo.pNext=nullptr;
    FramebufferCreateInfo.renderPass = VulkanObjects.RenderPass;
    FramebufferCreateInfo.attachmentCount=2;
    FramebufferCreateInfo.pAttachments=Attachments;
    FramebufferCreateInfo.width=Width;
    FramebufferCreateInfo.height=Height;
    FramebufferCreateInfo.layers=1;

    VulkanObjects.AppFramebuffers.resize(VulkanObjects.Swapchain.ImageCount);
    for(uint32_t i=0; i<VulkanObjects.AppFramebuffers.size(); i++)
    {
        Attachments[0] = VulkanObjects.Swapchain.Buffers[i].View;
        VK_CALL(vkCreateFramebuffer(VulkanObjects.Device, &FramebufferCreateInfo, nullptr, &VulkanObjects.AppFramebuffers[i]));
    }
}

void vulkanApp::BuildScene()
{
    VkCommandBuffer CopyCommand = vulkanTools::CreateCommandBuffer(VulkanObjects.VulkanDevice->Device, VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    Scene = new scene(this);
    //  Scene->Load("resources/models/sponza/sponza.dae", CopyCommand);
    // Scene->Load("D:\\models\\2.0\\Sponza\\glTF\\Sponza.gltf", CopyCommand);
    // Scene->Load("D:/Boulot/Models/Sponza_gltf/glTF/Sponza.gltf", CopyCommand);
    Scene->Load("D:/Boulot/Models/Duck/glTF/Duck.gltf", CopyCommand);
    // Scene->Load("D:/Boulot/Models/Cube/glTF/Cube.gltf", CopyCommand);
    // Scene->Load("D:/Boulot/Models/Pica pica/scene.gltf", CopyCommand);
    // Scene->Load("D:/Boulot/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", CopyCommand);
    // Scene->Load("D:\\Boulot\\Models\\Lantern\\glTF\\Lantern.gltf", CopyCommand);
    // Scene->Load("D:\\Boulot\\Models\\Cube\\glTF\\Cube.gltf", CopyCommand);

    vkFreeCommandBuffers(VulkanObjects.VulkanDevice->Device, VulkanObjects.CommandPool, 1, &CopyCommand);

    Scene->CreateDescriptorSets();
}

void vulkanApp::BuildVertexDescriptions()
{
    VulkanObjects.VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VulkanObjects.VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, Tangent)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex, MatInx))
    };  

    VulkanObjects.VerticesDescription.InputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    VulkanObjects.VerticesDescription.InputState.vertexBindingDescriptionCount = (uint32_t)VulkanObjects.VerticesDescription.BindingDescription.size();
    VulkanObjects.VerticesDescription.InputState.pVertexBindingDescriptions = VulkanObjects.VerticesDescription.BindingDescription.data();
    VulkanObjects.VerticesDescription.InputState.vertexAttributeDescriptionCount = (uint32_t)VulkanObjects.VerticesDescription.AttributeDescription.size();
    VulkanObjects.VerticesDescription.InputState.pVertexAttributeDescriptions = VulkanObjects.VerticesDescription.AttributeDescription.data();
}

void vulkanApp::CreateGeneralResources()
{
    VulkanObjects.CommandPool = vulkanTools::CreateCommandPool(VulkanObjects.Device, VulkanObjects.Swapchain.QueueNodeIndex); //Shared
    VulkanObjects.Swapchain.Create(&Width, &Height, EnableVSync); //Shared
    
    SetupDepthStencil(); //Shared
    
    SetupRenderPass(); //Shared
    CreatePipelineCache(); //Shared
    SetupFramebuffer(); //Shared

    VulkanObjects.TextureLoader = new textureLoader(VulkanObjects.VulkanDevice, VulkanObjects.Queue, VulkanObjects.CommandPool); //Shared
    
    BuildScene(); //Shared
    BuildVertexDescriptions(); //Shared


	ImGuiHelper = new ImGUI(this);
	ImGuiHelper->Init((float)Width, (float)Height);
	ImGuiHelper->InitResources(VulkanObjects.RenderPass, VulkanObjects.Queue);
    ImGuiHelper->InitStyle();

    Renderers.push_back(new forwardRenderer(this));
    Renderers.push_back(new deferredRenderer(this));
    if(RayTracing) 
    {
        Renderers.push_back(new pathTraceRTXRenderer(this));
        // Renderers.push_back(new deferredHybridRenderer(this));
    }
    Renderers.push_back(new pathTraceCPURenderer(this));
    Renderers.push_back(new rasterizerRenderer(this));
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
    VulkanObjects.Swapchain.InitSurface(Window);
    CreateGeneralResources();
    
    ObjectPicker.Initialize(VulkanObjects.VulkanDevice, this);
}

void vulkanApp::SetSelectedItem(uint32_t Index, bool UnselectIfAlreadySelected)
{
    if(Index==UINT32_MAX) {
        //Clear the selection
        if(CurrentSceneItemIndex != UINT32_MAX)
        {
            Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected=0;
            Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();
            CurrentSceneItemIndex = UINT32_MAX;
			return;
        }
        else //Selection is already clear, do nothing
        {
            return;
        }
    }
    
    if (Index != CurrentSceneItemIndex)
    {
		//Remove old
		if (CurrentSceneItemIndex != UINT32_MAX)
		{
			Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected=0;
			Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();     
		}

        //Set new
        CurrentSceneItemIndex = Index;
        Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected=1;
        Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();
    }
    else if(UnselectIfAlreadySelected)//Mesh no longer selected
    {
        CurrentSceneItemIndex = UINT32_MAX;
        if(Scene->InstancesPointers[Index]->InstanceData.Selected > 0)
        {
            Scene->InstancesPointers[Index]->InstanceData.Selected=0;
            Scene->InstancesPointers[Index]->UploadUniform();                 
        }
    }
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
    ImGuizmo::BeginFrame();
    ImGui::SetNextWindowSize(ImVec2(GuiWidth, (float)Height));
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);

    Scene->ViewportStart=0;

    
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
        if (CurrentSceneItemIndex  < Scene->InstancesPointers.size()-1) SetSelectedItem(CurrentSceneItemIndex+1);
    }
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
        if (CurrentSceneItemIndex > 0) { SetSelectedItem(CurrentSceneItemIndex-1); }
    }

    if(ImGui::Begin("Parameters"))
    {
        Scene->ViewportStart += GuiWidth;

        if (ImGui::BeginTabBar("MyTabBar"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                uint32_t NewSceneItemIndex=UINT32_MAX;
                for (size_t i=0; i<Scene->InstancesPointers.size(); i++) {
                    const bool IsSelected = (CurrentSceneItemIndex == i);
                    if (ImGui::Selectable(Scene->InstancesPointers[i]->Name.c_str(), IsSelected))
                    {
                        NewSceneItemIndex = (uint32_t)i;
                    }
                }
                if(NewSceneItemIndex != UINT32_MAX) SetSelectedItem(NewSceneItemIndex);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Environment"))
            {
                bool UpdateBackground= false;
                static int BackGroundType = 0;
                UpdateBackground |= ImGui::Combo("Background Mode", &BackGroundType, "Cubemap\0Solid Color\0Directional Light\0\0");
                Scene->UBOSceneMatrices.BackgroundType = (float)BackGroundType;
                UpdateBackground |= ImGui::DragFloat("Background Intensity", &Scene->UBOSceneMatrices.BackgroundIntensity, 0.1f, 0, 100);

                if(BackGroundType == 1)
                {
                    UpdateBackground |= ImGui::ColorPicker3("Background Color", &Scene->UBOSceneMatrices.BackgroundColor[0]);
                }
                if(BackGroundType == 2)
                {
                    static glm::vec3 LightDirection = Scene->UBOSceneMatrices.LightDirection;
                    UpdateBackground |= ImGui::SliderFloat3("Direction", &LightDirection[0], -1, 1);
                    Scene->UBOSceneMatrices.LightDirection = glm::normalize(LightDirection);
                }

                if(UpdateBackground && RayTracing)
                {
                    for(size_t i=0; i<Renderers.size(); i++)
                    {
                        pathTraceRTXRenderer *PathTracer = dynamic_cast<pathTraceRTXRenderer*>(Renderers[i]);
                        if(PathTracer)
                        {
                            PathTracer->ResetAccumulation=true;
                        }
                    }
                }                
                
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Renderer"))
            {
                static int RendererInx = 0;
                ImGui::Combo("Render Mode", &RendererInx, "Forward\0Deferred\0PathTraceRTX\0PathTraceCPU\0Rasterizer\0\0");
                CurrentRenderer = RendererInx;   

                ImGui::DragFloat("Exposure", &Scene->UBOSceneMatrices.Exposure, 0.01f);

                Renderers[CurrentRenderer]->RenderGUI();

                ImGui::Separator();

                static int DebugChannel = 0;
                ImGui::Combo("Debug Channels", &DebugChannel, "None\0TexCoords\0NormalTexture\0Normal\0Tangent\0Bitangent\0ShadingNormal\0Alpha\0Occlusion \0Emission\0Metallic \0Roughness\0BaseColor\0Clearcoat\0ClearcoatFactor\0ClearcoatNormal\0ClearcoatRoughnes\0Sheen\0\0");
                Scene->UBOSceneMatrices.DebugChannel = (float)DebugChannel;                    
    
                ImGui::EndTabItem();             
            }
            if (ImGui::BeginTabItem("Lights"))
            {
                ImGui::Text("This is the Broccoli tab!\nblah blah blah blah blah");
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }    


        if(CurrentSceneItemIndex != UINT32_MAX)
        {
            static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
            static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
            
            bool UpdateTransform=false;
             
            ImGui::SetNextWindowSize(ImVec2((float)GuiWidth, (float)Height));
            ImGui::SetNextWindowPos(ImVec2((float)GuiWidth,0), ImGuiCond_Always);
            if(ImGui::Begin(Scene->InstancesPointers[CurrentSceneItemIndex]->Name.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
            {
                Scene->ViewportStart+=GuiWidth;
                if(ImGui::CollapsingHeader("Transform"))
                {
                    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
                        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
                        mCurrentGizmoOperation = ImGuizmo::ROTATE;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
                        mCurrentGizmoOperation = ImGuizmo::SCALE;                    

                    glm::vec3 Translation, Rotation, Scale;
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Transform), glm::value_ptr(Translation), glm::value_ptr(Rotation), glm::value_ptr(Scale));
                    UpdateTransform |= ImGui::DragFloat3("Position", glm::value_ptr(Translation), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Rotation", glm::value_ptr(Rotation), 0.01f);
                    UpdateTransform |= ImGui::DragFloat3("Scale", glm::value_ptr(Scale), 0.01f);
                    ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Transform), glm::value_ptr(Translation), glm::value_ptr(Rotation), glm::value_ptr(Scale));
                }
                
                //ImGuizmo
                if(Renderers[CurrentRenderer]->UseGizmo)
                {
                    glm::mat4 projectionMatrix = Scene->Camera.GetProjectionMatrix();
                    projectionMatrix[1][1] *= -1;
                    
                    ImGuizmo::SetRect(Scene->ViewportStart, 0, Width - Scene->ViewportStart, (float)Height);
                    UpdateTransform |= ImGuizmo::Manipulate(glm::value_ptr(Scene->Camera.GetViewMatrix()), 
                                        glm::value_ptr(projectionMatrix), 
                                        mCurrentGizmoOperation, 
                                        mCurrentGizmoMode, 
                                        glm::value_ptr(Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Transform), 
                                        NULL, 
                                        NULL);  
                    
                }   

                if(UpdateTransform) 
                {
                    Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();
                    if(RayTracing)
                    {
                        for(size_t i=0; i<Renderers.size(); i++)
                        {
                            pathTraceRTXRenderer *PathTracer = dynamic_cast<pathTraceRTXRenderer*>(Renderers[i]);
                            if(PathTracer)
                            {
                                PathTracer->UpdateBLASInstance(CurrentSceneItemIndex);
                                PathTracer->ResetAccumulation=true;
                            }
                        }
                    }                        
                }

                
                if(ImGui::CollapsingHeader("Material"))
                {
                    //If changing material, disable the colouring
                    if(Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected>0)
                    {
                        Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected=0;
                        Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();   
                    }

                    bool UpdateMaterial=false;
                    materialData *Material = &Scene->InstancesPointers[CurrentSceneItemIndex]->Mesh->Material->MaterialData;

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
                    UpdateMaterial |= ImGui::Combo("Debug Channels", &DebugChannel, "None\0TexCoords\0NormalTexture\0Normal\0Tangent\0Bitangent\0ShadingNormal\0Alpha\0Occlusion \0Emission\0Metallic \0Roughness\0BaseColor\0Clearcoat\0ClearcoatFactor\0ClearcoatNormal\0ClearcoatRoughnes\0Sheen\0\0");
                    Material->DebugChannel = (debugChannel)DebugChannel;                    


                    if(UpdateMaterial)
                    {
                        Scene->InstancesPointers[CurrentSceneItemIndex]->Mesh->Material->Upload();
                        if(RayTracing)
                        {
                            for(size_t i=0; i<Renderers.size(); i++)
                            {
                                pathTraceRTXRenderer *PathTracer = dynamic_cast<pathTraceRTXRenderer*>(Renderers[i]);
                                if(PathTracer)
                                {
                                    PathTracer->UpdateMaterial(Scene->InstancesPointers[CurrentSceneItemIndex]->Mesh->Material->Index);
                                    PathTracer->ResetAccumulation=true;
                                }
                            }
                        }
                    }
                }
                else
                {
                    //If not changing material, re-enable the colouring
                    if(Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected==0)
                    {
                        Scene->InstancesPointers[CurrentSceneItemIndex]->InstanceData.Selected=1;
                        Scene->InstancesPointers[CurrentSceneItemIndex]->UploadUniform();   
                    }                    
                }
            }
            ImGui::End();
        }   

    }

    ImGui::End();

    // ImGui::ShowDemoWindow();
    if(ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused() || ImGuizmo::IsUsing()) Scene->Camera.Lock();
    else Scene->Camera.Unlock();
    

    // Render to generate draw buffers
    ImGui::Render();
}


void vulkanApp::Render()
{
    Scene->Update();
    RenderGUI();
    Renderers[CurrentRenderer]->Render();

    if(Scene->Camera.Changed) Scene->Camera.Changed=false;
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
        Mouse.PosXPress = Mouse.PosX;
        Mouse.PosYPress = Mouse.PosY;

        if(Button==0) Mouse.Left=true;
        if(Button==1) Mouse.Right=true;
        if(Mouse.PosX > Scene->ViewportStart && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver()) 
        {
            Scene->Camera.mousePressEvent(Button);

        }
    }
    else if(Action == GLFW_RELEASE)
    {
        float MousePressDelta = glm::length(glm::vec2(Mouse.PosX - Mouse.PosXPress, Mouse.PosY - Mouse.PosYPress));
        
		if (Mouse.PosX > Scene->ViewportStart && !ImGuizmo::IsUsing() && !ImGuizmo::IsOver())
		{
			if(MousePressDelta < 5) ObjectPicker.Pick((int)Mouse.PosX, (int)Mouse.PosY);
		}
        
        if(Button==0) Mouse.Left=false;
        if(Button==1) Mouse.Right=false;
        Scene->Camera.mouseReleaseEvent(Button);
    }
}

void vulkanApp::Scroll(float YOffset)
{
    Scene->Camera.Scroll(YOffset);
    Mouse.Wheel=YOffset;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent(0, Mouse.Wheel);
}


static ImGuiKey ImGui_ImplGlfw_KeyToImGuiKey(int key)
{
    switch (key)
    {
        case GLFW_KEY_TAB: return ImGuiKey_Tab;
        case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
        case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
        case GLFW_KEY_UP: return ImGuiKey_UpArrow;
        case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
        case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
        case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
        case GLFW_KEY_HOME: return ImGuiKey_Home;
        case GLFW_KEY_END: return ImGuiKey_End;
        case GLFW_KEY_INSERT: return ImGuiKey_Insert;
        case GLFW_KEY_DELETE: return ImGuiKey_Delete;
        case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
        case GLFW_KEY_SPACE: return ImGuiKey_Space;
        case GLFW_KEY_ENTER: return ImGuiKey_Enter;
        case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
        case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
        case GLFW_KEY_COMMA: return ImGuiKey_Comma;
        case GLFW_KEY_MINUS: return ImGuiKey_Minus;
        case GLFW_KEY_PERIOD: return ImGuiKey_Period;
        case GLFW_KEY_SLASH: return ImGuiKey_Slash;
        case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
        case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
        case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
        case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
        case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
        case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
        case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
        case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
        case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
        case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
        case GLFW_KEY_KP_0: return ImGuiKey_Keypad0;
        case GLFW_KEY_KP_1: return ImGuiKey_Keypad1;
        case GLFW_KEY_KP_2: return ImGuiKey_Keypad2;
        case GLFW_KEY_KP_3: return ImGuiKey_Keypad3;
        case GLFW_KEY_KP_4: return ImGuiKey_Keypad4;
        case GLFW_KEY_KP_5: return ImGuiKey_Keypad5;
        case GLFW_KEY_KP_6: return ImGuiKey_Keypad6;
        case GLFW_KEY_KP_7: return ImGuiKey_Keypad7;
        case GLFW_KEY_KP_8: return ImGuiKey_Keypad8;
        case GLFW_KEY_KP_9: return ImGuiKey_Keypad9;
        case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
        case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
        case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
        case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
        case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
        case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
        case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
        case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
        case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
        case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
        case GLFW_KEY_MENU: return ImGuiKey_Menu;
        case GLFW_KEY_0: return ImGuiKey_0;
        case GLFW_KEY_1: return ImGuiKey_1;
        case GLFW_KEY_2: return ImGuiKey_2;
        case GLFW_KEY_3: return ImGuiKey_3;
        case GLFW_KEY_4: return ImGuiKey_4;
        case GLFW_KEY_5: return ImGuiKey_5;
        case GLFW_KEY_6: return ImGuiKey_6;
        case GLFW_KEY_7: return ImGuiKey_7;
        case GLFW_KEY_8: return ImGuiKey_8;
        case GLFW_KEY_9: return ImGuiKey_9;
        case GLFW_KEY_A: return ImGuiKey_A;
        case GLFW_KEY_B: return ImGuiKey_B;
        case GLFW_KEY_C: return ImGuiKey_C;
        case GLFW_KEY_D: return ImGuiKey_D;
        case GLFW_KEY_E: return ImGuiKey_E;
        case GLFW_KEY_F: return ImGuiKey_F;
        case GLFW_KEY_G: return ImGuiKey_G;
        case GLFW_KEY_H: return ImGuiKey_H;
        case GLFW_KEY_I: return ImGuiKey_I;
        case GLFW_KEY_J: return ImGuiKey_J;
        case GLFW_KEY_K: return ImGuiKey_K;
        case GLFW_KEY_L: return ImGuiKey_L;
        case GLFW_KEY_M: return ImGuiKey_M;
        case GLFW_KEY_N: return ImGuiKey_N;
        case GLFW_KEY_O: return ImGuiKey_O;
        case GLFW_KEY_P: return ImGuiKey_P;
        case GLFW_KEY_Q: return ImGuiKey_Q;
        case GLFW_KEY_R: return ImGuiKey_R;
        case GLFW_KEY_S: return ImGuiKey_S;
        case GLFW_KEY_T: return ImGuiKey_T;
        case GLFW_KEY_U: return ImGuiKey_U;
        case GLFW_KEY_V: return ImGuiKey_V;
        case GLFW_KEY_W: return ImGuiKey_W;
        case GLFW_KEY_X: return ImGuiKey_X;
        case GLFW_KEY_Y: return ImGuiKey_Y;
        case GLFW_KEY_Z: return ImGuiKey_Z;
        case GLFW_KEY_F1: return ImGuiKey_F1;
        case GLFW_KEY_F2: return ImGuiKey_F2;
        case GLFW_KEY_F3: return ImGuiKey_F3;
        case GLFW_KEY_F4: return ImGuiKey_F4;
        case GLFW_KEY_F5: return ImGuiKey_F5;
        case GLFW_KEY_F6: return ImGuiKey_F6;
        case GLFW_KEY_F7: return ImGuiKey_F7;
        case GLFW_KEY_F8: return ImGuiKey_F8;
        case GLFW_KEY_F9: return ImGuiKey_F9;
        case GLFW_KEY_F10: return ImGuiKey_F10;
        case GLFW_KEY_F11: return ImGuiKey_F11;
        case GLFW_KEY_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

void vulkanApp::KeyEvent(int KeyCode, int Action)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey ImguiKey = ImGui_ImplGlfw_KeyToImGuiKey(KeyCode);
    io.AddKeyEvent(ImguiKey, (Action == GLFW_PRESS));
}

void vulkanApp::Resize(uint32_t NewWidth, uint32_t NewHeight)
{
    Width = NewWidth;
    Height = NewHeight;

    VulkanObjects.Swapchain.Create(&Width, &Height, true);
    vkDestroyImageView(VulkanObjects.Device, VulkanObjects.DepthStencil.View, nullptr);
    vkDestroyImage(VulkanObjects.Device, VulkanObjects.DepthStencil.Image, nullptr);
    vkFreeMemory(VulkanObjects.Device, VulkanObjects.DepthStencil.Memory, nullptr);
    SetupDepthStencil();

    for(size_t i=0; i<VulkanObjects.AppFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(VulkanObjects.Device, VulkanObjects.AppFramebuffers[i], nullptr);
    }
    SetupFramebuffer();

    ObjectPicker.Resize(NewWidth, NewHeight);
    for(uint32_t i=0; i<Renderers.size(); i++)
    {
        Renderers[i]->Resize(NewWidth, NewHeight);
    }

    vkQueueWaitIdle(VulkanObjects.Queue);
    vkDeviceWaitIdle(VulkanObjects.Device);

    Scene->Camera.SetAspectRatio((float)Width / (float)Height);


}

void vulkanApp::DestroyGeneralResources()
{
    ObjectPicker.Destroy();
    
    for(size_t i=0; i<Renderers.size(); i++)
    {
        Renderers[i]->Destroy(); 
    }

    Scene->Destroy();
    
    delete ImGuiHelper;

    VulkanObjects.TextureLoader->Destroy();
    
    for(int i=0; i<VulkanObjects.AppFramebuffers.size(); i++)
    {
        vkDestroyFramebuffer(VulkanObjects.Device, VulkanObjects.AppFramebuffers[i], nullptr);
    }
    vkDestroyPipelineCache(VulkanObjects.Device, VulkanObjects.PipelineCache, nullptr);
    vkDestroyRenderPass(VulkanObjects.Device, VulkanObjects.RenderPass, nullptr);
    
    vkDestroyImageView(VulkanObjects.Device, VulkanObjects.DepthStencil.View, nullptr);
    vkDestroyImage(VulkanObjects.Device, VulkanObjects.DepthStencil.Image, nullptr);
    vkFreeMemory(VulkanObjects.Device, VulkanObjects.DepthStencil.Memory, nullptr);
    
    VulkanObjects.Swapchain.Destroy();
    
    vkDestroyCommandPool(VulkanObjects.Device, VulkanObjects.CommandPool, nullptr);
    

   
}

void vulkanApp::Destroy()
{
    DestroyGeneralResources();
    

    vkDestroySemaphore(VulkanObjects.Device, VulkanObjects.Semaphores.PresentComplete, nullptr);
    vkDestroySemaphore(VulkanObjects.Device, VulkanObjects.Semaphores.RenderComplete, nullptr);
    vkDestroyDevice(VulkanObjects.Device, nullptr);
//    vulkanDebug::DestroyDebugReportCallback(Instance, vulkanDebug::DebugReportCallback, nullptr);
    vkDestroyInstance(VulkanObjects.Instance, nullptr);

    for(size_t i=0; i<Renderers.size(); i++)
    {
        delete Renderers[i];
    }

    delete VulkanObjects.TextureLoader;
    delete Scene;
    system("pause");
}