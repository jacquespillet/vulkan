#include "DeferredHybridRenderer.h"
#include "App.h"


deferredHybridRenderer::deferredHybridRenderer(vulkanApp *App) : renderer(App) {}

void deferredHybridRenderer::Render()
{
    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
    UpdateCamera();

    VK_CALL(App->Swapchain.AcquireNextImage(App->Semaphores.PresentComplete, &App->CurrentBuffer));
    
    //Before color output stage, wait for present semaphore to be complete, and signal Render semaphore to be completed
    
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pWaitSemaphores = &App->Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &OffscreenSemaphore;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &OffscreenCommandBuffer;
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    SubmitInfo.pWaitSemaphores = &OffscreenSemaphore;
    SubmitInfo.pSignalSemaphores = &App->Semaphores.RenderComplete;
    SubmitInfo.pCommandBuffers = &DrawCommandBuffers[App->CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->Swapchain.QueuePresent(App->Queue, App->CurrentBuffer, App->Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->Queue));
}

void deferredHybridRenderer::Setup()
{
    RayTracingPipelineProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceProperties2 DeviceProperties2 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    DeviceProperties2.pNext = &RayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(App->PhysicalDevice, &DeviceProperties2);
    
    AccelerationStructureFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};

    VkPhysicalDeviceFeatures2 DeviceFeatures2 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    DeviceFeatures2.pNext = &AccelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(App->PhysicalDevice, &DeviceFeatures2);
    
    CreateBottomLevelAccelarationStructure(App->Scene);
    FillBLASInstances();
    CreateTopLevelAccelerationStructure();

    CreateCommandBuffers();
    SetupDescriptorPool();
    Resources.Init(VulkanDevice, DescriptorPool, App->TextureLoader);
    BuildQuads();
    BuildOffscreenBuffers();
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    
    VkSemaphoreCreateInfo SemaphoreCreateInfo = vulkanTools::BuildSemaphoreCreateInfo();
    VK_CALL(vkCreateSemaphore(Device, &SemaphoreCreateInfo, nullptr, &OffscreenSemaphore));

    BuildCommandBuffers();
    BuildDeferredCommandBuffers();
}

//


void deferredHybridRenderer::CreateTopLevelAccelerationStructure()
{
    VkDeviceOrHostAddressConstKHR InstanceDataDeviceAddress {};
    InstanceDataDeviceAddress.deviceAddress = vulkanTools::GetBufferDeviceAddress(VulkanDevice, InstancesBuffer.Buffer);

    VkAccelerationStructureGeometryKHR AccelerationStructureGeometry = vulkanTools::BuildAccelerationStructureGeometry();
    AccelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    AccelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    AccelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    AccelerationStructureGeometry.geometry.instances.arrayOfPointers =VK_FALSE;
    AccelerationStructureGeometry.geometry.instances.data = InstanceDataDeviceAddress;
    
    //HERE: Put the top level transform ? 

    VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuildGeometryInfo = vulkanTools::BuildAccelerationStructureBuildGeometryInfo();
    AccelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    AccelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    AccelerationStructureBuildGeometryInfo.geometryCount=1;
    AccelerationStructureBuildGeometryInfo.pGeometries = &AccelerationStructureGeometry;

    uint32_t PrimitivesCount = static_cast<uint32_t>(BLASInstances.size());

    VkAccelerationStructureBuildSizesInfoKHR AccelerationStructureBuildSizesInfo = vulkanTools::BuildAccelerationStructureBuildSizesInfo();
    VulkanDevice->_vkGetAccelerationStructureBuildSizesKHR(
        VulkanDevice->Device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &AccelerationStructureBuildGeometryInfo,
        &PrimitivesCount,
        &AccelerationStructureBuildSizesInfo
    );

    TopLevelAccelerationStructure.Create(VulkanDevice, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, AccelerationStructureBuildSizesInfo);

    scratchBuffer ScratchBuffer(VulkanDevice, AccelerationStructureBuildSizesInfo.buildScratchSize);

    VkAccelerationStructureBuildGeometryInfoKHR AccelerationBuildGeometryInfo = vulkanTools::BuildAccelerationStructureBuildGeometryInfo();
    AccelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    AccelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    AccelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    AccelerationBuildGeometryInfo.dstAccelerationStructure = TopLevelAccelerationStructure.AccelerationStructure;
    AccelerationBuildGeometryInfo.geometryCount=1;
    AccelerationBuildGeometryInfo.pGeometries=&AccelerationStructureGeometry;
    AccelerationBuildGeometryInfo.scratchData.deviceAddress = ScratchBuffer.DeviceAddress;
    

    VkAccelerationStructureBuildRangeInfoKHR AccelerationStructureBuildRangeInfo {};
    AccelerationStructureBuildRangeInfo.primitiveCount = PrimitivesCount;
    AccelerationStructureBuildRangeInfo.primitiveOffset = 0;
    AccelerationStructureBuildRangeInfo.firstVertex=0;
    AccelerationStructureBuildRangeInfo.transformOffset=0;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> AccelerationStructureBuildRangeInfos = {&AccelerationStructureBuildRangeInfo};
    
    if(AccelerationStructureFeatures.accelerationStructureHostCommands)
    {
        VulkanDevice->_vkBuildAccelerationStructuresKHR(
            VulkanDevice->Device,
            VK_NULL_HANDLE,
            1,
            &AccelerationBuildGeometryInfo,
            AccelerationStructureBuildRangeInfos.data()
        );
    }
    else
    {
        VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VulkanDevice->_vkCmdBuildAccelerationStructuresKHR(
            CommandBuffer,
            1,
            &AccelerationBuildGeometryInfo,
            AccelerationStructureBuildRangeInfos.data());
        vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, CommandBuffer, App->Queue, true);
    }

    ScratchBuffer.Destroy();
}


void deferredHybridRenderer::CreateBottomLevelAccelarationStructure(scene *Scene)
{
    for(size_t i=0; i<App->Scene->Meshes.size(); i++)
    {
        VkTransformMatrixKHR TransformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f, 
            0.0f, 0.0f, 1.0f, 0.0f
        };
        buffer TransformMatrixBuffer;
        VK_CALL(vulkanTools::CreateBuffer(
            App->VulkanDevice,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &TransformMatrixBuffer,
            sizeof(VkTransformMatrixKHR),
            &TransformMatrix
        ));

        uint32_t NumTriangles = (uint32_t)App->Scene->Meshes[i].Indices.size() / 3;

        VkAccelerationStructureGeometryKHR AccelerationStructureGeometry = vulkanTools::BuildAccelerationStructureGeometry();
        AccelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        AccelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        AccelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        AccelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        AccelerationStructureGeometry.geometry.triangles.vertexData.deviceAddress = vulkanTools::GetBufferDeviceAddress(App->VulkanDevice, App->Scene->Meshes[i].VertexBuffer.Buffer);
        AccelerationStructureGeometry.geometry.triangles.maxVertex = (uint32_t)App->Scene->Meshes[i].Vertices.size();
        AccelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vertex);
        AccelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        AccelerationStructureGeometry.geometry.triangles.indexData.deviceAddress = vulkanTools::GetBufferDeviceAddress(App->VulkanDevice, App->Scene->Meshes[i].IndexBuffer.Buffer);
        AccelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = vulkanTools::GetBufferDeviceAddress(App->VulkanDevice, TransformMatrixBuffer.Buffer);
        AccelerationStructureGeometry.geometry.triangles.transformData.hostAddress=nullptr;

        VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuildGeometryInfo = vulkanTools::BuildAccelerationStructureBuildGeometryInfo();
        AccelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        AccelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        AccelerationStructureBuildGeometryInfo.geometryCount=1;
        AccelerationStructureBuildGeometryInfo.pGeometries = &AccelerationStructureGeometry;

        VkAccelerationStructureBuildSizesInfoKHR AccelerationStructureBuildSizeInfo = vulkanTools::BuildAccelerationStructureBuildSizesInfo();
        VulkanDevice->_vkGetAccelerationStructureBuildSizesKHR(
            VulkanDevice->Device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &AccelerationStructureBuildGeometryInfo,
            &NumTriangles,
            &AccelerationStructureBuildSizeInfo
        );

        accelerationStructure BLAS;
        BLAS.Create(VulkanDevice, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, AccelerationStructureBuildSizeInfo);

        scratchBuffer ScratchBuffer(VulkanDevice, AccelerationStructureBuildSizeInfo.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR AccelerationBuildGeometryInfo = vulkanTools::BuildAccelerationStructureBuildGeometryInfo();
        AccelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        AccelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        AccelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        AccelerationBuildGeometryInfo.dstAccelerationStructure = BLAS.AccelerationStructure;
        AccelerationBuildGeometryInfo.geometryCount=1;
        AccelerationBuildGeometryInfo.pGeometries = &AccelerationStructureGeometry;
        AccelerationBuildGeometryInfo.scratchData.deviceAddress = ScratchBuffer.DeviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR AccelerationStructureBuildRangeInfo {};
        AccelerationStructureBuildRangeInfo.primitiveCount = NumTriangles;
        AccelerationStructureBuildRangeInfo.primitiveOffset = 0;
        AccelerationStructureBuildRangeInfo.firstVertex=0;
        AccelerationStructureBuildRangeInfo.transformOffset=0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> AccelerationBuildStructureRangeInfos = {&AccelerationStructureBuildRangeInfo};

        if(AccelerationStructureFeatures.accelerationStructureHostCommands)
        {
            VulkanDevice->_vkBuildAccelerationStructuresKHR(
                VulkanDevice->Device,
                VK_NULL_HANDLE,
                1,
                &AccelerationBuildGeometryInfo,
                AccelerationBuildStructureRangeInfos.data()
            );
        }
        else
        {
            VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            VulkanDevice->_vkCmdBuildAccelerationStructuresKHR(
                CommandBuffer,
                1,
                &AccelerationBuildGeometryInfo,
                AccelerationBuildStructureRangeInfos.data());
            vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, CommandBuffer, App->Queue, true);
        }

        BottomLevelAccelerationStructures.push_back(std::move(BLAS));
        
        ScratchBuffer.Destroy();
        TransformMatrixBuffer.Destroy();
    }
}

VkAccelerationStructureInstanceKHR deferredHybridRenderer::CreateBottomLevelAccelerationInstance(instance *Instance)
{
    glm::mat4& Transform = Instance->InstanceData.Transform;
    //Change this to transform
    VkTransformMatrixKHR TransformMatrix = 
    {
        Transform[0][0], Transform[1][0], Transform[2][0], Transform[3][0],
        Transform[0][1], Transform[1][1], Transform[2][1], Transform[3][1],
        Transform[0][2], Transform[1][2], Transform[2][2], Transform[3][2],
    };
    accelerationStructure &BLAS = BottomLevelAccelerationStructures[Instance->MeshIndex];

    VkAccelerationStructureDeviceAddressInfoKHR AccelerationStructureDeviceAddressInfo {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    AccelerationStructureDeviceAddressInfo.accelerationStructure = BLAS.AccelerationStructure;
    VkDeviceAddress DeviceAddress = VulkanDevice->_vkGetAccelerationStructureDeviceAddressKHR(VulkanDevice->Device, &AccelerationStructureDeviceAddressInfo);

    VkAccelerationStructureInstanceKHR BLASInstance {};
    BLASInstance.transform = TransformMatrix;
    BLASInstance.instanceCustomIndex = Instance->MeshIndex;
    BLASInstance.mask = 0xFF;
    BLASInstance.instanceShaderBindingTableRecordOffset = 0;
    BLASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    BLASInstance.accelerationStructureReference = DeviceAddress;

    return BLASInstance;
}

void deferredHybridRenderer::FillBLASInstances()
{
    BLASInstances.resize(0);
    for(size_t i=0; i<App->Scene->InstancesPointers.size(); i++)
    {	
        BLASInstances.push_back(CreateBottomLevelAccelerationInstance(App->Scene->InstancesPointers[i]));
    }

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &InstancesBuffer,
        sizeof(VkAccelerationStructureInstanceKHR) * BLASInstances.size(),
        BLASInstances.data()
    ));
}


void deferredHybridRenderer::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(App->Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));

    OffscreenCommandBuffer = vulkanTools::CreateCommandBuffer(Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
}

void deferredHybridRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16)
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 6);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));    
}


inline float Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}



void deferredHybridRenderer::BuildQuads()
{
    Quad = vulkanTools::BuildQuad(VulkanDevice);
}



void deferredHybridRenderer::BuildOffscreenBuffers()
{
    VkCommandBuffer LayoutCommand = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);                   
    //G buffer
    {
        Framebuffers.Offscreen.SetSize(App->Width, App->Height)
                              .SetAttachmentCount(4)
                              .SetAttachmentFormat(0, VK_FORMAT_R32G32B32A32_SFLOAT)
                              .SetAttachmentFormat(1, VK_FORMAT_R8G8B8A8_UNORM)
                              .SetAttachmentFormat(2, VK_FORMAT_R32G32B32A32_UINT)
                              .SetAttachmentFormat(3, VK_FORMAT_R8G8B8A8_UNORM);
        Framebuffers.Offscreen.BuildBuffers(VulkanDevice,LayoutCommand);        
    }    
    //SSAO
    {
        uint32_t SSAOWidth = App->Width;
        uint32_t SSAOHeight = App->Height;
        ShadowPass.Framebuffer.SetSize(SSAOWidth, SSAOHeight)
                         .SetAttachmentCount(1)
                         .SetAttachmentFormat(0, VK_FORMAT_R8_UNORM)
                         .HasDepth=false;
        ShadowPass.Framebuffer.BuildBuffers(VulkanDevice,LayoutCommand);        
    }

    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, LayoutCommand, App->Queue, true);
}




void deferredHybridRenderer::BuildLayoutsAndDescriptors()
{
    //Render scene (Gbuffer)
    {        
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Material"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Instances")
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        Resources.PipelineLayouts->Add("Offscreen", pPipelineLayoutCreateInfo);
    }    

    //Composition
    {
        //Refactor that :
        //Have a vector of vector of descriptor.
        //First dimension is descriptor set layouts,
        //Second dimension is actual descriptor set information
        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[2].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[3].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, ShadowPass.Framebuffer._Attachments[0].ImageView, ShadowPass.Framebuffer.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        };
        std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts = 
        {
            App->Scene->Cubemap.DescriptorSetLayout,
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
        };
        Resources.AddDescriptorSet(VulkanDevice, "Composition", Descriptors, DescriptorPool, AdditionalDescriptorSetLayouts);
    }

    //Shadow Pass
    {
        VkWriteDescriptorSetAccelerationStructureKHR DescriptorAccelerationStructureInfo = vulkanTools::BuildWriteDescriptorSetAccelerationStructure();
        DescriptorAccelerationStructureInfo.accelerationStructureCount=1;
        DescriptorAccelerationStructureInfo.pAccelerationStructures = &TopLevelAccelerationStructure.AccelerationStructure;        

        std::vector<descriptor> Descriptors = 
        {
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
            descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, DescriptorAccelerationStructureInfo),
        };
        
        std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
        };
        Resources.AddDescriptorSet(VulkanDevice, "Shadows", Descriptors, DescriptorPool, AdditionalDescriptorSetLayouts);
    }
}


void deferredHybridRenderer::BuildPipelines()
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
    PipelineCreateInfo.pVertexInputState = &App->VerticesDescription.InputState;
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
        PipelineCreateInfo.renderPass = App->RenderPass;

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/Composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/hybridComposition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        
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

        Resources.Pipelines->Add("Composition.SSAO.Enabled", PipelineCreateInfo, App->PipelineCache);

        SpecializationData.EnableSSAO = 0;
        Resources.Pipelines->Add("Composition.SSAO.Disabled", PipelineCreateInfo, App->PipelineCache);
    }

    // Fill G buffer
    {
        struct specializationData
        {
            int Mask=0;
            int HasMetallicRoughnessMap = 0;
            int HasEmissiveMap = 0;
            int HasBaseColorMap = 0;
            int HasOcclusionMap = 0;
            int HasNormalMap = 0;
            int HasClearCoat = 0;
            int HasSheen = 0;
        } SpecializationData;

        std::vector<VkSpecializationMapEntry> SpecializationMapEntries = 
        {
            vulkanTools::BuildSpecializationMapEntry(0, offsetof(specializationData, Mask), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(1, offsetof(specializationData, HasMetallicRoughnessMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(2, offsetof(specializationData, HasEmissiveMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(3, offsetof(specializationData, HasBaseColorMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(4, offsetof(specializationData, HasOcclusionMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(5, offsetof(specializationData, HasNormalMap), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(6, offsetof(specializationData, HasClearCoat), sizeof(int)),
            vulkanTools::BuildSpecializationMapEntry(7, offsetof(specializationData, HasSheen), sizeof(int))
        };
        VkSpecializationInfo SpecializationInfo = vulkanTools::BuildSpecializationInfo(
            (uint32_t)SpecializationMapEntries.size(),
            SpecializationMapEntries.data(),
            sizeof(SpecializationData),
            &SpecializationData
        );

        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        ShaderModules.push_back(ShaderStages[1].module);            
        ShaderModules.push_back(ShaderStages[0].module);
        
        RasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        PipelineCreateInfo.renderPass = Framebuffers.Offscreen.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Offscreen");

        std::array<VkPipelineColorBlendAttachmentState, 4> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE),
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();

        for(int i=0; i<App->Scene->Materials.size(); i++)
        {
            int MatFlags = App->Scene->Materials[i].Flags;
            if(!Resources.Pipelines->Present(MatFlags))
            {
                if(MatFlags & materialFlags::Opaque) 
                {
                    SpecializationData.Mask=0;
                    RasterizationState.cullMode=VK_CULL_MODE_BACK_BIT;
                }
                if(MatFlags & materialFlags::Mask) 
                {
                    SpecializationData.Mask=1;
                    RasterizationState.cullMode=VK_CULL_MODE_NONE;
                }

                if(MatFlags & materialFlags::HasBaseColorMap)SpecializationData.HasBaseColorMap=1;
                if(MatFlags & materialFlags::HasEmissiveMap)SpecializationData.HasEmissiveMap=1;
                if(MatFlags & materialFlags::HasMetallicRoughnessMap)SpecializationData.HasMetallicRoughnessMap=1;
                if(MatFlags & materialFlags::HasOcclusionMap)SpecializationData.HasOcclusionMap=1;
                if(MatFlags & materialFlags::HasNormalMap)SpecializationData.HasNormalMap=1;
                if(MatFlags & materialFlags::HasClearCoat)SpecializationData.HasClearCoat=1;
                if(MatFlags & materialFlags::HasSheen)SpecializationData.HasSheen=1;
                
                Resources.Pipelines->Add(MatFlags, PipelineCreateInfo, App->PipelineCache);
            }
        }        

    }

    //SSAO
    {
        RasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        ColorBlendState.attachmentCount=1;
        ShaderStages[0] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/Composition.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device, "resources/shaders/spv/raytracedShadows.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[0].module);
        ShaderModules.push_back(ShaderStages[1].module);

        PipelineCreateInfo.renderPass = ShadowPass.Framebuffer.RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Shadows");
        Resources.Pipelines->Add("Shadows", PipelineCreateInfo, App->PipelineCache);
    }     
}


void deferredHybridRenderer::BuildCommandBuffers()
{
    
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    VkClearValue ClearValues[2];
    ClearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->RenderPass;
    RenderPassBeginInfo.renderArea.offset.x=0;
    RenderPassBeginInfo.renderArea.offset.y=0;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=2;
    RenderPassBeginInfo.pClearValues = ClearValues;


    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));
		
		App->ImGuiHelper->UpdateBuffers();
        
        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width, (float)App->Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);
        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width, App->Height, 0, 0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        

        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 0, 1, Resources.DescriptorSets->GetPtr("Composition"), 0, nullptr);
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 1, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Composition"), 2, 1, App->Scene->Resources.DescriptorSets->GetPtr("Scene"), 0, nullptr);

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Composition.SSAO.Enabled"));
        vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Quad.VertexBuffer.Buffer, Offsets);
        vkCmdBindIndexBuffer(DrawCommandBuffers[i], Quad.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(DrawCommandBuffers[i], 6, 1, 0, 0, 1);


        App->ImGuiHelper->DrawFrame(DrawCommandBuffers[i]);

        vkCmdEndRenderPass(DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}

void deferredHybridRenderer::BuildDeferredCommandBuffers()
{
    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(OffscreenCommandBuffer, &CommandBufferBeginInfo));
    
    //G-buffer pass
    std::array<VkClearValue, 5> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[2].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[3].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[4].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = Framebuffers.Offscreen.RenderPass;
    RenderPassBeginInfo.framebuffer = Framebuffers.Offscreen.Framebuffer;
    RenderPassBeginInfo.renderArea.extent.width = Framebuffers.Offscreen.Width;
    RenderPassBeginInfo.renderArea.extent.height = Framebuffers.Offscreen.Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();
    
    vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
    VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width - App->Scene->ViewportStart, (float)App->Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);
    vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);


    VkRect2D Scissor = vulkanTools::BuildRect2D(Framebuffers.Offscreen.Width,Framebuffers.Offscreen.Height,0,0);
    vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

    VkDeviceSize Offset[1] = {0};

    VkPipelineLayout RendererPipelineLayout =  Resources.PipelineLayouts->Get("Offscreen");
    VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");


	for (auto &InstanceGroup : App->Scene->Instances)
	{
		int Flag = InstanceGroup.first;
		vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get(Flag));
		vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
		
		for (auto Instance : InstanceGroup.second)
		{
			vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VertexBuffer.Buffer, Offset);
			vkCmdBindIndexBuffer(OffscreenCommandBuffer, Instance.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
			vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
			vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, 0, 0);
		}
	}

	//{ //Cubemap
	//	vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Cubemap"));
	//	vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &App->Scene->Cubemap.Mesh.VertexBuffer.Buffer, Offset);
	//	vkCmdBindIndexBuffer(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
	//	vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 0, 1, &RendererDescriptorSet, 0, nullptr);
	//	vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 1, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
	//	vkCmdDrawIndexed(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexCount, 1, 0, 0, 0);
	//}

    // for(auto Instance : App->Scene->Instances)
    // {
    //     if(Instance.Mesh->Material->HasAlpha)
    //     {
    //         continue;
    //     }
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
    //     vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
    //     vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, Instance.Mesh->IndexBase, 0);
    // }

    // vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Scene.Blend"));
    // for(auto Instance : App->Scene->Instances)
    // {
    //     if(Instance.Mesh->Material->HasAlpha)
    //     {
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
    //         vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
    //         vkCmdDrawIndexed(OffscreenCommandBuffer, Instance.Mesh->IndexCount, 1, 0, Instance.Mesh->IndexBase, 0);
    //     }
    // }

    vkCmdEndRenderPass(OffscreenCommandBuffer);

    //
    //Transition framebuffers from layout color attachment to layout shader read only
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[0].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[1].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[2].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    Framebuffers.Offscreen._Attachments[3].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    if(true)
    {
        ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
        ClearValues[1].depthStencil = {1.0f, 0};

        RenderPassBeginInfo.framebuffer = ShadowPass.Framebuffer.Framebuffer;
        RenderPassBeginInfo.renderPass = ShadowPass.Framebuffer.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = ShadowPass.Framebuffer.Width;
        RenderPassBeginInfo.renderArea.extent.height = ShadowPass.Framebuffer.Height;
        RenderPassBeginInfo.clearValueCount=2;
        RenderPassBeginInfo.pClearValues = ClearValues.data();

        vkCmdBeginRenderPass(OffscreenCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        Viewport = vulkanTools::BuildViewport((float)Framebuffers.Offscreen.Width, (float)Framebuffers.Offscreen.Height, 0.0f, 1.0f, 0, 0);
        vkCmdSetViewport(OffscreenCommandBuffer, 0, 1, &Viewport);

        Scissor = vulkanTools::BuildRect2D(ShadowPass.Framebuffer.Width,ShadowPass.Framebuffer.Height,0,0);
        vkCmdSetScissor(OffscreenCommandBuffer, 0, 1, &Scissor);

        
        VkDeviceSize Offsets[1] = {0};
        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Shadows"), 0, 1, Resources.DescriptorSets->GetPtr("Shadows"), 0, nullptr);
        vkCmdBindDescriptorSets(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Shadows"), 1, 1, App->Scene->Resources.DescriptorSets->GetPtr("Scene"), 0, nullptr);

        vkCmdBindPipeline(OffscreenCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Shadows"));
        vkCmdBindVertexBuffers(OffscreenCommandBuffer, VERTEX_BUFFER_BIND_ID, 1, &Quad.VertexBuffer.Buffer, Offsets);
        vkCmdBindIndexBuffer(OffscreenCommandBuffer, Quad.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(OffscreenCommandBuffer, 6, 1, 0, 0, 1);
        vkCmdEndRenderPass(OffscreenCommandBuffer);
    }
    /*vulkanTools::TransitionImageLayout(OffscreenCommandBuffer,
    ShadowPass.Framebuffer._Attachments[0].Image, 
    VK_IMAGE_ASPECT_COLOR_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);*/

    VK_CALL(vkEndCommandBuffer(OffscreenCommandBuffer));
}


void deferredHybridRenderer::UpdateCamera()
{
}

void deferredHybridRenderer::Resize(uint32_t Width, uint32_t Height) 
{
    Framebuffers.Offscreen.Destroy(VulkanDevice->Device);
    ShadowPass.Framebuffer.Destroy(VulkanDevice->Device);
    BuildOffscreenBuffers();

    VkDescriptorSet TargetDescriptorSet = Resources.DescriptorSets->Get("Composition");
    std::vector<descriptor> Descriptors = 
    {
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[0].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[1].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[2].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        descriptor(VK_SHADER_STAGE_FRAGMENT_BIT, Framebuffers.Offscreen._Attachments[3].ImageView, Framebuffers.Offscreen.Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
    };
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets(4);
    WriteDescriptorSets[0] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[0].DescriptorType, 0, &Descriptors[0].DescriptorImageInfo); 
    WriteDescriptorSets[1] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[1].DescriptorType, 1, &Descriptors[1].DescriptorImageInfo); 
    WriteDescriptorSets[2] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[2].DescriptorType, 2, &Descriptors[2].DescriptorImageInfo); 
    WriteDescriptorSets[3] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[3].DescriptorType, 3, &Descriptors[3].DescriptorImageInfo); 
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);        
}

void deferredHybridRenderer::Destroy()
{
    vkDestroySemaphore(VulkanDevice->Device, OffscreenSemaphore, nullptr);
    for (size_t i = 0; i < ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(VulkanDevice->Device, ShaderModules[i], nullptr);
    }

    Framebuffers.Offscreen.Destroy(VulkanDevice->Device);
    ShadowPass.Framebuffer.Destroy(VulkanDevice->Device);
    
    Quad.Destroy();
    Resources.Destroy();
    vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
    
    
    vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, (uint32_t)DrawCommandBuffers.size(), DrawCommandBuffers.data());
    vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, 1, &OffscreenCommandBuffer);
}