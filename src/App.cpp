#include "App.h"

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

    Swapchain.Connect(Instance, PhysicalDevice, Device);

    //Build main semaphores
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.PresentComplete));
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.RenderComplete));
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &Semaphores.TextOverlayComplete));

    //Before color output stage, wait for present semaphore to be complete, and signal Render semaphore to be completed
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.pWaitSemaphores = &Semaphores.PresentComplete;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pSignalSemaphores = &Semaphores.RenderComplete;    
}

void vulkanApp::SetupWindow()
{
    int rc = glfwInit();
    assert(rc);
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    
    Window = glfwCreateWindow(Width, Height, "Gralib", 0, 0);
    assert(Window);
}

void vulkanApp::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));
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


void vulkanApp::CreateGeneralResources()
{
    CommandPool = vulkanTools::CreateCommandPool(Device, Swapchain.QueueNodeIndex);
    Swapchain.Create(&Width, &Height, EnableVSync);
    CreateCommandBuffers();

    SetupDepthStencil();
    SetupRenderPass();
    CreatePipelineCache();
    SetupFramebuffer();

    TextureLoader = new textureLoader(VulkanDevice, Queue, CommandPool);
}

 void vulkanApp::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));
}

void vulkanApp::BuildQuads()
{
    struct vertex
    {
        float pos[3];
        float uv[2];
        float col[3];
        float normal[3];
        float tangent[3];
        float bitangent[3];
    };
    std::vector<vertex> VertexBuffer;
    float x=0;
    float y=0;
    for(uint32_t i=0; i<3; i++)
    {
        VertexBuffer.push_back({ { x + 1.0f, y + 1.0f, 0.0f },{ 1.0f, 1.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, (float)i } });
        VertexBuffer.push_back({ { x,      y + 1.0f, 0.0f },{ 0.0f, 1.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, (float)i } });
        VertexBuffer.push_back({ { x,      y,      0.0f },{ 0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, (float)i } });
        VertexBuffer.push_back({ { x + 1.0f, y,      0.0f },{ 1.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, (float)i } });    

        x += 1.0f;
        if(x > 1.0f)
        {
            x = 0.0f;
            y += 1.0f;
        }  
    }

    vulkanTools::CreateBuffer(VulkanDevice,
                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                    VertexBuffer.size() * sizeof(vertex),
                    VertexBuffer.data(),
                    &Meshes.Quad.Vertices.Buffer,
                    &Meshes.Quad.Vertices.DeviceMemory); 

    std::vector<uint32_t> IndicesBuffer = {0,1,2,  2,3,0};
    for(uint32_t i=0; i<3; i++)
    {
        uint32_t Indices[6] = {0,1,2,  2,3,0};
        for(auto Index : Indices)
        {
            IndicesBuffer.push_back(i * 4 + Index);
        }
    }
    Meshes.Quad.IndexCount = (uint32_t)IndicesBuffer.size();

    vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        IndicesBuffer.size() * sizeof(uint32_t),
        IndicesBuffer.data(),
        &Meshes.Quad.Indices.Buffer,
        &Meshes.Quad.Indices.DeviceMemory
    );
}

 void vulkanApp::BuildOffscreenBuffers()
{
    VkCommandBuffer LayoutCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    uint32_t SSAOWidth = Width;
    uint32_t SSAOHeight = Height;

    Framebuffers.Offscreen.SetSize(Width, Height);
    Framebuffers.SSAO.SetSize(Width, Height);
    Framebuffers.SSAOBlur.SetSize(Width, Height);

    //Create G buffer attachments
    vulkanTools::CreateAttachment(VulkanDevice,
                                    VK_FORMAT_R32G32B32A32_SFLOAT,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    &Framebuffers.Offscreen.Attachments[0],
                                    LayoutCommand,
                                    Width,
                                    Height);
                                    
    vulkanTools::CreateAttachment(VulkanDevice,
                                    VK_FORMAT_R8G8B8A8_UNORM,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    &Framebuffers.Offscreen.Attachments[1],
                                    LayoutCommand,
                                    Width,
                                    Height);

    vulkanTools::CreateAttachment(VulkanDevice,
                                    VK_FORMAT_R32G32B32A32_UINT,
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    &Framebuffers.Offscreen.Attachments[2],
                                    LayoutCommand,
                                    Width,
                                    Height);

    VkFormat AttDepthFormat;
    VkBool32 ValidDepthFormat = vulkanTools::GetSupportedDepthFormat(PhysicalDevice, &AttDepthFormat);
    assert(ValidDepthFormat);

    vulkanTools::CreateAttachment(VulkanDevice,
                                    AttDepthFormat,
                                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                    &Framebuffers.Offscreen.Depth,
                                    LayoutCommand,
                                    Width,
                                    Height);
    
    //Create SSAO attachments
    vulkanTools::CreateAttachment(VulkanDevice,
                                    VK_FORMAT_R8_UNORM, 
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    &Framebuffers.SSAO.Attachments[0],
                                    LayoutCommand,
                                    SSAOWidth,
                                    SSAOHeight);
    vulkanTools::CreateAttachment(VulkanDevice,
                                    VK_FORMAT_R8_UNORM, 
                                    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                    &Framebuffers.SSAOBlur.Attachments[0],
                                    LayoutCommand,
                                    Width,
                                    Height);

    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, LayoutCommand, Queue, true);

    //G buffer
    {
        //Attachment descriptions
        //3 colour buffers + depth
        std::array<VkAttachmentDescription, 4> AttachmentDescriptions = {};
        
        for(uint32_t i=0; i<static_cast<uint32_t>(AttachmentDescriptions.size()); i++)
        {
            AttachmentDescriptions[i].samples = VK_SAMPLE_COUNT_1_BIT;
            AttachmentDescriptions[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            AttachmentDescriptions[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            AttachmentDescriptions[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            AttachmentDescriptions[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            AttachmentDescriptions[i].finalLayout = (i==3) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        AttachmentDescriptions[0].format = Framebuffers.Offscreen.Attachments[0].Format;
        AttachmentDescriptions[1].format = Framebuffers.Offscreen.Attachments[1].Format;
        AttachmentDescriptions[2].format = Framebuffers.Offscreen.Attachments[2].Format;
        AttachmentDescriptions[3].format = Framebuffers.Offscreen.Depth.Format;

        //Attachment references
        std::vector<VkAttachmentReference> ColorReferences;
        ColorReferences.push_back({0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        ColorReferences.push_back({1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        ColorReferences.push_back({2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        VkAttachmentReference DepthReference = {};
        DepthReference.attachment=3;
        DepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        //Subpass
        VkSubpassDescription Subpass = {};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.pColorAttachments = ColorReferences.data();
        Subpass.colorAttachmentCount = (uint32_t)ColorReferences.size();
        Subpass.pDepthStencilAttachment = &DepthReference;

        //Subpass dependencies to transition the attachments from memory read to write, and then back from write to read
        std::array<VkSubpassDependency, 2> Dependencies;

        Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        Dependencies[0].dstSubpass=0;
        Dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


        Dependencies[1].srcSubpass=0;
        Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
        Dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo RenderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        RenderPassInfo.attachmentCount = static_cast<uint32_t>(AttachmentDescriptions.size());
        RenderPassInfo.pAttachments = AttachmentDescriptions.data();
        RenderPassInfo.subpassCount=1;
        RenderPassInfo.pSubpasses = &Subpass;
        RenderPassInfo.dependencyCount=2;
        RenderPassInfo.pDependencies = Dependencies.data();
        VK_CALL(vkCreateRenderPass(Device, &RenderPassInfo, nullptr, &Framebuffers.Offscreen.RenderPass));

        //Create framebuffer objects
        std::array<VkImageView, 4> Attachments;
        Attachments[0] = Framebuffers.Offscreen.Attachments[0].ImageView;
        Attachments[1] = Framebuffers.Offscreen.Attachments[1].ImageView;
        Attachments[2] = Framebuffers.Offscreen.Attachments[2].ImageView;
        Attachments[3] = Framebuffers.Offscreen.Depth.ImageView;

        VkFramebufferCreateInfo FramebufferCreateInfo  = vulkanTools::BuildFramebufferCreateInfo();
        FramebufferCreateInfo.renderPass = Framebuffers.Offscreen.RenderPass;
        FramebufferCreateInfo.pAttachments = Attachments.data();
        FramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(Attachments.size());
        FramebufferCreateInfo.width = Framebuffers.Offscreen.Width;
        FramebufferCreateInfo.height = Framebuffers.Offscreen.Height;
        FramebufferCreateInfo.layers=1;
        VK_CALL(vkCreateFramebuffer(Device, &FramebufferCreateInfo, nullptr, &Framebuffers.Offscreen.Framebuffer));
    }

    //SSAO
    {
        VkAttachmentDescription AttachmentDescription {};
        AttachmentDescription.format = Framebuffers.SSAO.Attachments[0].Format;
        AttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        AttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ColorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        //Subpass
        VkSubpassDescription SubpassDescription = {};
        SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        SubpassDescription.pColorAttachments = &ColorReference;
        SubpassDescription.colorAttachmentCount = 1;

        std::array<VkSubpassDependency, 2> Dependencies;

        Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        Dependencies[0].dstSubpass=0;
        Dependencies[0].srcStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[0].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;
        
        Dependencies[1].srcSubpass = 0;
        Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
        Dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[1].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo RenderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        RenderPassCreateInfo.pAttachments = &AttachmentDescription;
        RenderPassCreateInfo.attachmentCount=1;
        RenderPassCreateInfo.subpassCount=1;
        RenderPassCreateInfo.pSubpasses = &SubpassDescription;
        RenderPassCreateInfo.dependencyCount=2;
        RenderPassCreateInfo.pDependencies = Dependencies.data();
        VK_CALL(vkCreateRenderPass(Device, &RenderPassCreateInfo, nullptr, &Framebuffers.SSAO.RenderPass));

        VkFramebufferCreateInfo FramebufferCreateInfo = vulkanTools::BuildFramebufferCreateInfo();
        FramebufferCreateInfo.renderPass = Framebuffers.SSAO.RenderPass;
        FramebufferCreateInfo.pAttachments = &Framebuffers.SSAO.Attachments[0].ImageView;
        FramebufferCreateInfo.attachmentCount=1;
        FramebufferCreateInfo.width = Framebuffers.SSAO.Width;
        FramebufferCreateInfo.height = Framebuffers.SSAO.Height;
        FramebufferCreateInfo.layers=1;
        VK_CALL(vkCreateFramebuffer(Device, &FramebufferCreateInfo, nullptr, &Framebuffers.SSAO.Framebuffer));
    }

    //SSAO Blur
    {
        VkAttachmentDescription AttachmentDescription {};
        AttachmentDescription.format = Framebuffers.SSAOBlur.Attachments[0].Format;
        AttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        AttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        AttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        AttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        AttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        AttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        AttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ColorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        //Subpass
        VkSubpassDescription SubpassDescription = {};
        SubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        SubpassDescription.pColorAttachments = &ColorReference;
        SubpassDescription.colorAttachmentCount = 1;

        std::array<VkSubpassDependency, 2> Dependencies;

        Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        Dependencies[0].dstSubpass=0;
        Dependencies[0].srcStageMask=VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[0].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;
        
        Dependencies[1].srcSubpass = 0;
        Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
        Dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        Dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        Dependencies[1].dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo RenderPassCreateInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        RenderPassCreateInfo.pAttachments = &AttachmentDescription;
        RenderPassCreateInfo.attachmentCount=1;
        RenderPassCreateInfo.subpassCount=1;
        RenderPassCreateInfo.pSubpasses =&SubpassDescription;
        RenderPassCreateInfo.dependencyCount=2;
        RenderPassCreateInfo.pDependencies = Dependencies.data();
        VK_CALL(vkCreateRenderPass(Device, &RenderPassCreateInfo, nullptr, &Framebuffers.SSAOBlur.RenderPass));

        VkFramebufferCreateInfo FramebufferCreateInfo = vulkanTools::BuildFramebufferCreateInfo();
        FramebufferCreateInfo.renderPass = Framebuffers.SSAOBlur.RenderPass;
        FramebufferCreateInfo.pAttachments = &Framebuffers.SSAOBlur.Attachments[0].ImageView;
        FramebufferCreateInfo.attachmentCount=1;
        FramebufferCreateInfo.width = Framebuffers.SSAOBlur.Width;
        FramebufferCreateInfo.height = Framebuffers.SSAOBlur.Height;
        FramebufferCreateInfo.layers=1;
        VK_CALL(vkCreateFramebuffer(Device, &FramebufferCreateInfo, nullptr, &Framebuffers.SSAOBlur.Framebuffer));
    }

    VkSamplerCreateInfo SamplerCreateInfo = vulkanTools::BuildSamplerCreateInfo();
    SamplerCreateInfo.magFilter=VK_FILTER_LINEAR;
    SamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    SamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.mipLodBias = 0.0f;
    SamplerCreateInfo.maxAnisotropy=0;
    SamplerCreateInfo.minLod=0.0f;
    SamplerCreateInfo.maxLod=1.0f;
    SamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    SamplerCreateInfo.compareEnable=VK_TRUE;
    VK_CALL(vkCreateSampler(Device, &SamplerCreateInfo, nullptr, &ColorSampler));

}

 void vulkanApp::UpdateUniformBufferScreen()
{
    UBOVS.Projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    UBOVS.Model = glm::mat4(1.0f);

    VK_CALL(UniformBuffers.FullScreen.Map());
    UniformBuffers.FullScreen.CopyTo(&UBOVS, sizeof(UBOVS));
    UniformBuffers.FullScreen.Unmap();
}

void vulkanApp::UpdateUniformBufferDeferredMatrices()
{
    UBOSceneMatrices.Projection = glm::perspective(glm::radians(50.0f), 1.0f, 0.01f, 100.0f);
    UBOSceneMatrices.View = glm::mat4(1.0f);
    UBOSceneMatrices.Model = glm::mat4(1.0f);
    UBOSceneMatrices.ViewportDim = glm::vec2((float)Width,(float)Height);

    VK_CALL(UniformBuffers.SceneMatrices.Map());
    UniformBuffers.SceneMatrices.CopyTo(&UBOSceneMatrices, sizeof(UBOSceneMatrices));
    UniformBuffers.SceneMatrices.Unmap();
}

void vulkanApp::UpdateUniformBufferSSAOParams()
{
    UBOSceneMatrices.Projection =  glm::perspective(glm::radians(50.0f), 1.0f, 0.01f, 100.0f);
    
    VK_CALL(UniformBuffers.SSAOParams.Map());
    UniformBuffers.SSAOParams.CopyTo(&UBOSSAOParams, sizeof(UBOSSAOParams));
    UniformBuffers.SSAOParams.Unmap();
}

inline float vulkanApp::Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

void vulkanApp::BuildUniformBuffers()
{
    //Full screen quad shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.FullScreen,
                                sizeof(UBOVS)
    );
        
    //Deferred vertex shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SceneMatrices,
                                sizeof(UBOSceneMatrices)
    );

    //Deferred Frag Shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SceneLights,
                                sizeof(UBOFragmentLights)
    );

    UpdateUniformBufferScreen();
    UpdateUniformBufferDeferredMatrices();

    //Deferred Frag Shader
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SSAOParams,
                                sizeof(UBOSSAOParams)
    );
    UpdateUniformBufferSSAOParams();

    std::uniform_real_distribution<float> RandomDistribution(0.0f, 1.0f);
    std::random_device RandomDevice;
    std::default_random_engine RandomEngine;
    
    std::vector<glm::vec4> SSAOKernel(SSAO_KERNEL_SIZE);
    for(uint32_t i=0; i<SSAO_KERNEL_SIZE; i++)
    {
        glm::vec3 Sample(RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine));
        Sample = glm::normalize(Sample);
        Sample *= RandomDistribution(RandomEngine);
        float Scale = float(i) / float(SSAO_KERNEL_SIZE);
        Scale = Lerp(0.1f, 1.0f, Scale * Scale);
        SSAOKernel[i] = glm::vec4(Sample * Sample, 0.0f);
    }
    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &UniformBuffers.SSAOKernel,
                                SSAOKernel.size() * sizeof(glm::vec4),
                                SSAOKernel.data()
    );

    std::vector<glm::vec4> SSAONoise(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
    for(size_t i=0; i<SSAONoise.size(); i++)
    {
        SSAONoise[i] = glm::vec4(RandomDistribution(RandomEngine) * 2.0f - 1.0f, RandomDistribution(RandomEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
    }

    TextureLoader->CreateTexture(SSAONoise.data(), SSAONoise.size() * sizeof(glm::vec4), VK_FORMAT_R32G32B32A32_SFLOAT, SSAO_NOISE_DIM, SSAO_NOISE_DIM, &Textures.SSAONoise, VK_FILTER_NEAREST);        
}

void vulkanApp::BuildLayoutsAndDescriptors()
{
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
    VkDescriptorSetLayoutCreateInfo SetLayoutCreateInfo;
    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo();
    VkDescriptorSetAllocateInfo DescriptorAllocateInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, nullptr, 1);
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
    std::vector<VkDescriptorImageInfo> ImageDescriptors;
    VkDescriptorSet TargetDescriptorSet;

    //Resolve pass
    SetLayoutBindings = 
    {
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
    };
    SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    Resources.DescriptorSetLayouts->Add("Composition", SetLayoutCreateInfo);
    PipelineLayoutCreateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("Composition");
    Resources.PipelineLayouts->Add("Composition", PipelineLayoutCreateInfo);
    DescriptorAllocateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("Composition");
    TargetDescriptorSet = Resources.DescriptorSets->Add("Composition", DescriptorAllocateInfo);

    ImageDescriptors = {
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.Offscreen.Attachments[0].ImageView, VK_IMAGE_LAYOUT_GENERAL),
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.Offscreen.Attachments[1].ImageView, VK_IMAGE_LAYOUT_GENERAL),
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.Offscreen.Attachments[2].ImageView, VK_IMAGE_LAYOUT_GENERAL),
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.SSAOBlur.Attachments[0].ImageView, VK_IMAGE_LAYOUT_GENERAL),
    };
    WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &UniformBuffers.FullScreen.Descriptor),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &ImageDescriptors[0]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &ImageDescriptors[1]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &ImageDescriptors[2]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &ImageDescriptors[3]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &UniformBuffers.SceneLights.Descriptor)
    };
    vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);

    //SSAO Pass
    SetLayoutBindings = 
    {
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0), //position + depth
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1), //Normals
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2), //noise
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3), //kernel
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 4), //params
    };
    SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    Resources.DescriptorSetLayouts->Add("SSAO.Generate", SetLayoutCreateInfo);
    PipelineLayoutCreateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("SSAO.Generate");
    Resources.PipelineLayouts->Add("SSAO.Generate", PipelineLayoutCreateInfo);
    DescriptorAllocateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("SSAO.Generate");
    TargetDescriptorSet = Resources.DescriptorSets->Add("SSAO.Generate", DescriptorAllocateInfo);      
    ImageDescriptors = 
    {
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.Offscreen.Attachments[0].ImageView, VK_IMAGE_LAYOUT_GENERAL),
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.Offscreen.Attachments[1].ImageView, VK_IMAGE_LAYOUT_GENERAL)
    };
    WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &ImageDescriptors[0]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &ImageDescriptors[1]),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &Textures.SSAONoise.Descriptor),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &UniformBuffers.SSAOKernel.Descriptor),
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4, &UniformBuffers.SSAOParams.Descriptor)
    };
    vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);

    //SSAO Blur
    SetLayoutBindings = 
    {
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
    };
    SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    Resources.DescriptorSetLayouts->Add("SSAO.Blur", SetLayoutCreateInfo);
    PipelineLayoutCreateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("SSAO.Blur");
    Resources.PipelineLayouts->Add("SSAO.Blur", PipelineLayoutCreateInfo);
    DescriptorAllocateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("SSAO.Blur");
    TargetDescriptorSet = Resources.DescriptorSets->Add("SSAO.Blur", DescriptorAllocateInfo);    
    ImageDescriptors = 
    {
        vulkanTools::BuildDescriptorImageInfo(ColorSampler, Framebuffers.SSAO.Attachments[0].ImageView, VK_IMAGE_LAYOUT_GENERAL),
    };
    WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &ImageDescriptors[0]),
    };
    vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);

    //Render scene (Gbuffer)
    SetLayoutBindings = 
    {
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0), //uniforms
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1), //diffuse map
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2), //specular map
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3), //normal map
    };
    SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    Resources.DescriptorSetLayouts->Add("Offscreen", SetLayoutCreateInfo);
    PipelineLayoutCreateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("Offscreen");
    Resources.PipelineLayouts->Add("Offscreen", PipelineLayoutCreateInfo);
    DescriptorAllocateInfo.pSetLayouts=Resources.DescriptorSetLayouts->GetPtr("Offscreen");
    TargetDescriptorSet = Resources.DescriptorSets->Add("Offscreen", DescriptorAllocateInfo);    
    WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &UniformBuffers.SceneMatrices.Descriptor),
    };
    vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
}


void vulkanApp::BuildPipelines()
{
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = vulkanTools::BuildPipelineInputAssemblyStateCreateInfo(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0,
        VK_FALSE
    );

    VkPipelineRasterizationStateCreateInfo RasterizationState = vulkanTools::BuildPipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_CLOCKWISE,
        0
    );

    VkPipelineColorBlendAttachmentState BlendAttachmentState = vulkanTools::BuildPipelineColorBlendAttachmentState(
        0xf,
        VK_FALSE
    );

    VkPipelineColorBlendStateCreateInfo ColorBlendState = vulkanTools::BuildPipelineColorBlendStateCreateInfo(
        1,
        &BlendAttachmentState
    );

    VkPipelineDepthStencilStateCreateInfo DepthStencilState = vulkanTools:: BuildPipelineDepthStencilStateCreateInfo(
        VK_TRUE,
        VK_TRUE,
        VK_COMPARE_OP_LESS_OR_EQUAL
    );

    VkPipelineViewportStateCreateInfo ViewportState = vulkanTools::BuildPipelineViewportStateCreateInfo(
        1,1,0
    );

    VkPipelineMultisampleStateCreateInfo MultiSample = vulkanTools::BuildPipelineMultisampleStateCreateInfo(
        VK_SAMPLE_COUNT_1_BIT,
        0
    );

    std::vector<VkDynamicState> DynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo DynamicState = vulkanTools::BuildPipelineDynamicStateCreateInfo(
        DynamicStateEnables.data(),
        (uint32_t)DynamicStateEnables.size(),
        0
    );

    //Shader Stages
    std::array<VkPipelineShaderStageCreateInfo, 2> ShaderStages;
    VkGraphicsPipelineCreateInfo PipelineCreateInfo = vulkanTools::BuildGraphicsPipelineCreateInfo();
    PipelineCreateInfo.pVertexInputState = &VerticesDescription.InputState;
    PipelineCreateInfo.pInputAssemblyState = &InputAssemblyState;
    PipelineCreateInfo.pRasterizationState = &RasterizationState;
    PipelineCreateInfo.pColorBlendState = &ColorBlendState;
    PipelineCreateInfo.pMultisampleState = &MultiSample;
    PipelineCreateInfo.pViewportState = &ViewportState;
    PipelineCreateInfo.pDepthStencilState = &DepthStencilState;
    PipelineCreateInfo.pDynamicState = &DynamicState;
    PipelineCreateInfo.stageCount = (uint32_t)ShaderStages.size();
    PipelineCreateInfo.pStages = ShaderStages.data();
    PipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

    //Final composition pipeline
    {
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Composition");
        PipelineCreateInfo.renderPass = RenderPass;

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/Composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/Composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);

        struct specializationData
        {
            int32_t EnableSSAO=1;
            float AmbientFactor=0.15f;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries;
        SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, EnableSSAO), sizeof(int32_t)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, AmbientFactor), sizeof(float)),
        };
        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo((uint32_t)SpecializationMapEntries.size(), SpecializationMapEntries.data(), sizeof(SpecializationData), &SpecializationData );
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;

        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, PipelineCache);

        SpecializationData.EnableSSAO = 0;
        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, PipelineCache);
    }

    PipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    PipelineCreateInfo.basePipelineIndex=-1;
    PipelineCreateInfo.basePipelineHandle = Resources.Pipelines->Get("Composition.SSAO.Enabled");

    //Debug
    ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/Debug.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/Debug.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    ShaderModules.push_back(ShaderStages[0].module);
    ShaderModules.push_back(ShaderStages[1].module);
    Resources.Pipelines->Add("DebugDisplay", PipelineCreateInfo, PipelineCache);

    // Fill G buffer
    {
        struct specializationData
        {
            float ZNear;
            float ZFar;
            int32_t Discard=0;
        } SpecializationData;

        SpecializationData.ZNear = 0.01f;
        SpecializationData.ZFar = 100;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, ZNear), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, ZFar), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, Discard), sizeof(int32_t))
        };
        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo(
            (uint32_t)SpecializationMapEntries.size(),
            SpecializationMapEntries.data(),
            sizeof(SpecializationData),
            &SpecializationData
        );
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);            

        PipelineCreateInfo.renderPass = Framebuffers.Offscreen.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Offscreen");

        std::array<VkPipelineColorBlendAttachmentState, 3> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();
        Resources.Pipelines->Add("Scene.Solid", PipelineCreateInfo, PipelineCache);
        
        //Transparents
        DepthStencilState.depthWriteEnable=VK_FALSE;
        RasterizationState.cullMode=VK_CULL_MODE_NONE;
        SpecializationData.Discard=1;
        Resources.Pipelines->Add("Scene.Blend", PipelineCreateInfo, PipelineCache);
    }

    //SSAO
    {
        ColorBlendState.attachmentCount=1;

        VkPipelineVertexInputStateCreateInfo EmptyInputState {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        EmptyInputState.vertexAttributeDescriptionCount=0;
        EmptyInputState.pVertexAttributeDescriptions=nullptr;
        EmptyInputState.vertexBindingDescriptionCount=0;
        EmptyInputState.pVertexBindingDescriptions=nullptr;
        PipelineCreateInfo.pVertexInputState=&EmptyInputState;

        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);

        struct specializationData
        {
            uint32_t KernelSize = SSAO_KERNEL_SIZE;
            float Radius = SSAO_RADIUS;
            float Power = 1.5f;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries;
        SpecializationMapEntries = {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, KernelSize), sizeof(uint32_t)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, Radius), sizeof(float)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, Power), sizeof(float)),
        };

        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo(
            (uint32_t)SpecializationMapEntries.size(),
            SpecializationMapEntries.data(),
            sizeof(SpecializationData),
            &SpecializationData
        );
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        PipelineCreateInfo.renderPass = Framebuffers.SSAO.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("SSAO.Generate");
        Resources.Pipelines->Add("SSAO.Generate", PipelineCreateInfo, PipelineCache);
    }

    //SSAO blur
    {
        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);            
        PipelineCreateInfo.renderPass = Framebuffers.SSAOBlur.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("SSAO.Blur");
        Resources.Pipelines->Add("SSAO.Blur" ,PipelineCreateInfo, PipelineCache);
    }        
}

void vulkanApp::BuildScene()
{
    VkCommandBuffer CopyCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    Scene = new scene(this);
    Scene->Load("resources/models/sponza/sponza.dae", CopyCommand);
    vkFreeCommandBuffers(VulkanDevice->Device, CommandPool, 1, &CopyCommand);
}

void vulkanApp::BuildVertexDescriptions()
{
    VerticesDescription.BindingDescription = {
        vulkanTools::BuildVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    VerticesDescription.AttributeDescription = {
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Position)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1, VK_FORMAT_R32G32_SFLOAT,    offsetof(vertex, UV)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Color)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Normal)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Tangent)),
        vulkanTools::BuildVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 5, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vertex, Bitangent)),
    };

    VerticesDescription.InputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    VerticesDescription.InputState.vertexBindingDescriptionCount = (uint32_t)VerticesDescription.BindingDescription.size();
    VerticesDescription.InputState.pVertexBindingDescriptions = VerticesDescription.BindingDescription.data();
    VerticesDescription.InputState.vertexAttributeDescriptionCount = (uint32_t)VerticesDescription.AttributeDescription.size();
    VerticesDescription.InputState.pVertexAttributeDescriptions = VerticesDescription.AttributeDescription.data();
}

void vulkanApp::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = Width;
    RenderPassBeginInfo.renderArea.extent.height = Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;


    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));
        
        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport Viewport = vulkanTools::BuildViewport((float)Width, (float)Height, 0.0f, 1.0f);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);

        VkRect2D Scissor = vulkanTools::BuildRect2D(Width, Height, 0, 0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 0, 1, Resources.DescriptorSets->GetPtr("Composition"), 0, nullptr);

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Composition.SSAO.Enabled"));
        vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Meshes.Quad.Vertices.Buffer, Offsets);
        vkCmdBindIndexBuffer(DrawCommandBuffers[i], Meshes.Quad.Indices.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(DrawCommandBuffers[i], 6, 1, 0, 0, 1);

        vkCmdEndRenderPass(DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}

void vulkanApp::BuildDeferredCommandBuffers()
{
    if((OffscreenCommandBuffer == VK_NULL_HANDLE) || Rebuild)
    {
        if(Rebuild)
        {
            vkFreeCommandBuffers(Device, CommandPool, 1, &OffscreenCommandBuffer);
        }
        OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    }

    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &OffscreenSemaphore));

    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferBeginInfo));
    
    //First pass
    std::array<VkClearValue, 4> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[2].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[3].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = Framebuffers.Offscreen.RenderPass;
    RenderPassBeginInfo.framebuffer = Framebuffers.Offscreen.Framebuffer;
    RenderPassBeginInfo.renderArea.extent.width = Framebuffers.Offscreen.Width;
    RenderPassBeginInfo.renderArea.extent.height = Framebuffers.Offscreen.Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();
    
    vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f);
    vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffers.Offscreen.Width,Framebuffers.Offscreen.Height,0,0);
    vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    VkDeviceSize Offset[1] = {0};

    vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Solid"));
    vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Scene->VertexBuffer.Buffer, Offset);
    vkCmdBindIndexBuffer(OffscreenCommandBuffer, Scene->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

    for(auto Mesh : Scene->Meshes)
    {
        if(Mesh.Material->HasAlpha)
        {
            continue;
        }
        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene->PipelineLayout, 0, 1, &Mesh.DescriptorSet, 0, nullptr);
        vkCmdDrawIndexed(OffscreenCommandBuffer, Mesh.IndexCount, 1, 0, Mesh.IndexBase, 0);
    }

    for(auto Mesh : Scene->Meshes)
    {
        if(Mesh.Material->HasAlpha)
        {
            vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Scene->PipelineLayout, 0, 1, &Mesh.DescriptorSet, 0, nullptr);
            vkCmdDrawIndexed(OffscreenCommandBuffer, Mesh.IndexCount, 1, 0, Mesh.IndexBase, 0);
        }
    }

    vkCmdEndRenderPass(OffscreenCommandBuffer);

    // if(EnableSSAO)
    // {
    //     ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    //     ClearValues[1].depthStencil = {1.0f, 0};

    //     RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
    //     RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
    //     RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
    //     RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
    //     RenderPassBeginInfo.clearValueCount=2;
    //     RenderPassBeginInfo.pClearValues = ClearValues.data();

    //     vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
    //     Viewport = vulkanTools::BuildViewport((float)Framebuffers.SSAO.Width, (float)Framebuffers.SSAO.Height, 0.0f, 1.0f);
    //     vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

    //     Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAO.Width,Framebuffers.SSAO.Height,0,0);
    //     vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("SSAO.Generate"), 0, 1, Resources.DescriptorSets->GetPtr("SSAO.Generate"), 0, nullptr);
    //     vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("SSAO.Generate"));
    //     vkCmdDraw(OffscreenCommandBuffer, 3, 1, 0, 0);
    //     vkCmdEndRenderPass(OffscreenCommandBuffer);
        
    //     //ssao blur
    //     RenderPassBeginInfo.framebuffer = Framebuffers.SSAO.Framebuffer;
    //     RenderPassBeginInfo.renderPass = Framebuffers.SSAO.RenderPass;
    //     RenderPassBeginInfo.renderArea.extent.width = Framebuffers.SSAO.Width;
    //     RenderPassBeginInfo.renderArea.extent.height = Framebuffers.SSAO.Height;
    //     vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
    //     Viewport = vulkanTools::BuildViewport((float)Framebuffers.SSAOBlur.Width, (float)Framebuffers.SSAOBlur.Height, 0.0f, 1.0f);
    //     vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

    //     Scissor = vulkanTools::BuildRect2D(Framebuffers.SSAOBlur.Width,Framebuffers.SSAOBlur.Height,0,0);
    //     vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);
        
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("SSAO.Blur"), 0, 1, Resources.DescriptorSets->GetPtr("SSAO.Blur"), 0, nullptr);
    //     vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("SSAO.Generate"));
    //     vkCmdDraw(OffscreenCommandBuffer, 3, 1, 0, 0);
    //     vkCmdEndRenderPass(OffscreenCommandBuffer);
    // }
    VK_CALL(vkEndCommandBuffer(OffscreenCommandBuffer));
}

void vulkanApp::CreateDeferredRendererResources()
{
    SetupDescriptorPool();

    Resources.PipelineLayouts = new pipelineLayoutList(VulkanDevice->Device);
    Resources.Pipelines = new pipelineList(VulkanDevice->Device);
    Resources.DescriptorSetLayouts = new descriptorSetLayoutList(VulkanDevice->Device);
    Resources.DescriptorSets = new descriptorSetList(VulkanDevice->Device, DescriptorPool);
    Resources.Textures = new textureList(VulkanDevice->Device, TextureLoader);

    BuildQuads();
    BuildVertexDescriptions();
    BuildOffscreenBuffers();
    BuildUniformBuffers();

    BuildLayoutsAndDescriptors();
    BuildPipelines();

    BuildScene();
    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
}

vulkanApp::vulkanApp()
{
    Width = 1920;
    Height = 1080;

    InitVulkan();
        SetupWindow();
    Swapchain.InitSurface(Window);

    CreateGeneralResources();
    CreateDeferredRendererResources();

}

void vulkanApp::Render()
{
    VK_CALL(Swapchain.AcquireNextImage(Semaphores.PresentComplete, &CurrentBuffer));
    
    SubmitInfo.pWaitSemaphores = &Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &OffscreenSemaphore;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

     SubmitInfo.pWaitSemaphores = &OffscreenSemaphore;
     SubmitInfo.pSignalSemaphores = &Semaphores.RenderComplete;
     SubmitInfo.pCommandBuffers = &DrawCommandBuffers[CurrentBuffer];
     VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(Swapchain.QueuePresent(Queue, CurrentBuffer, Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(Queue));
}

void vulkanApp::RenderLoop()
{
    while (!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();

        Render();

        glfwSwapBuffers(Window);
    }
}