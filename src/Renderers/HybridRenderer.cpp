#include "hybridRenderer.h"
#include "App.h"
#include "imgui.h"
hybridRenderer::hybridRenderer(vulkanApp *App) : renderer(App) {}

void hybridRenderer::Render()
{
    BuildCommandBuffers();
    
    VK_CALL(App->Swapchain.AcquireNextImage(App->Semaphores.PresentComplete, &App->CurrentBuffer));
    
    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pWaitSemaphores = &App->Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &App->Semaphores.RenderComplete;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &DrawCommandBuffers[App->CurrentBuffer];
    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->Swapchain.QueuePresent(App->Queue, App->CurrentBuffer, App->Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->Queue));
}



void hybridRenderer::CreateTopLevelAccelerationStructure()
{
    VkDeviceOrHostAddressConstKHR InstanceDataDeviceAddress {};
    InstanceDataDeviceAddress.deviceAddress = vulkanTools::GetBufferDeviceAddress(VulkanDevice, InstancesBuffer.Buffer);

    VkAccelerationStructureGeometryKHR AccelerationStructureGeometry = vulkanTools::BuildAccelerationStructureGeometry();
    AccelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    AccelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
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


void hybridRenderer::CreateBottomLevelAccelarationStructure(scene *Scene)
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
        AccelerationStructureGeometry.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
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

VkAccelerationStructureInstanceKHR hybridRenderer::CreateBottomLevelAccelerationInstance(instance *Instance)
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

void hybridRenderer::FillBLASInstances()
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

void hybridRenderer::Setup()
{
    RayTracingPipelineProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceProperties2 DeviceProperties2 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    DeviceProperties2.pNext = &RayTracingPipelineProperties;
    vkGetPhysicalDeviceProperties2(App->PhysicalDevice, &DeviceProperties2);
    
    AccelerationStructureFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};

    VkPhysicalDeviceFeatures2 DeviceFeatures2 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    DeviceFeatures2.pNext = &AccelerationStructureFeatures;
    vkGetPhysicalDeviceFeatures2(App->PhysicalDevice, &DeviceFeatures2);

    CreateCommandBuffers();

    CreateBottomLevelAccelarationStructure(App->Scene);
    FillBLASInstances();
    CreateTopLevelAccelerationStructure();
    
    
    SetupDescriptorPool();
    Resources.Init(VulkanDevice, DescriptorPool, App->TextureLoader);
    BuildLayoutsAndDescriptors();
    BuildPipelines();
    BuildCommandBuffers();
}

//


void hybridRenderer::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(App->Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));
}

void hybridRenderer::SetupDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1),
    };

    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo((uint32_t)PoolSizes.size(), PoolSizes.data(), 32);

    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));    
}


inline float Lerp(float a, float b, float f)
{
    return a + f * (b - a);
}


void hybridRenderer::BuildLayoutsAndDescriptors()
{
    //Create descriptor set for acceleration structure
    std::vector<VkDescriptorSetLayoutBinding> Bindings = 
    {
        vulkanTools::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
    };

    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(Bindings);
    VK_CALL(vkCreateDescriptorSetLayout(VulkanDevice->Device, &DescriptorSetLayoutCreateInfo, nullptr, &DescriptorSetLayout));
    //Render scene : the pipeline layout contains 4 descriptor sets :
    //- 1 for the global scene variables : Matrices, lights...
    //- 1 for all the material data (Textures, properties...)
    //- 1 for all the instance specific data (Model matrix)
    //- 1 for all cubemap and ibl data
    {
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Material"),
            App->Scene->Resources.DescriptorSetLayouts->Get("Instances"),
            App->Scene->Cubemap.DescriptorSetLayout,
            DescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        Resources.PipelineLayouts->Add("Scene", pPipelineLayoutCreateInfo);
    }

    //Cubemap rendering
    {
        //Build pipeline layout
        std::vector<VkDescriptorSetLayout> RendererSetLayouts = 
        {
            App->Scene->Resources.DescriptorSetLayouts->Get("Scene"),
            App->Scene->Cubemap.DescriptorSetLayout
        };
        VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(RendererSetLayouts.data(), (uint32_t)RendererSetLayouts.size());
        Resources.PipelineLayouts->Add("Cubemap", pPipelineLayoutCreateInfo);
    }

    VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
    VK_CALL(vkAllocateDescriptorSets(VulkanDevice->Device, &DescriptorSetAllocateInfo, &DescriptorSet));

    VkWriteDescriptorSetAccelerationStructureKHR DescriptorAccelerationStructureInfo = vulkanTools::BuildWriteDescriptorSetAccelerationStructure();
    DescriptorAccelerationStructureInfo.accelerationStructureCount=1;
    DescriptorAccelerationStructureInfo.pAccelerationStructures = &TopLevelAccelerationStructure.AccelerationStructure;

    VkWriteDescriptorSet AccelerationStructureWrite {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    AccelerationStructureWrite.pNext = &DescriptorAccelerationStructureInfo;
    AccelerationStructureWrite.dstSet = DescriptorSet;
    AccelerationStructureWrite.dstBinding=0;
    AccelerationStructureWrite.descriptorCount=1;
    AccelerationStructureWrite.descriptorType=VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
    {
        AccelerationStructureWrite,
    };    

    vkUpdateDescriptorSets(VulkanDevice->Device, static_cast<uint32_t>(WriteDescriptorSets.size()), WriteDescriptorSets.data(), 0, VK_NULL_HANDLE);


}


void hybridRenderer::BuildPipelines()
{
    VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = vulkanTools::BuildPipelineInputAssemblyStateCreateInfo(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        0,
        VK_FALSE
    );

    VkPipelineRasterizationStateCreateInfo RasterizationState = vulkanTools::BuildPipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_BACK_BIT,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

    //Loop through materials
    //  Check if a pipeline with this flag already exists, if not :
    //  For each flag
    //      Check the flag, and set a specialization data property
    //      Build the pipeline and add it to the list
    
    //Store the meshes according to their material masks
    //  std::unorderedmap<flag, std::vector<Instance>> 


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
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/hybrid.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/hybrid.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderStages[1].pSpecializationInfo = &SpecializationInfo;
        ShaderModules.push_back(ShaderStages[1].module);            
        ShaderModules.push_back(ShaderStages[0].module);

        PipelineCreateInfo.renderPass = App->RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Scene");

        std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
        {
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
    
    //Cube map
    {
        ShaderStages[0] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/cubemap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        ShaderStages[1] = LoadShader(VulkanDevice->Device,"resources/shaders/spv/cubemap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        ShaderModules.push_back(ShaderStages[1].module);            
        ShaderModules.push_back(ShaderStages[0].module);

        PipelineCreateInfo.renderPass = App->RenderPass;
        PipelineCreateInfo.layout = Resources.PipelineLayouts->Get("Cubemap");

        std::array<VkPipelineColorBlendAttachmentState, 1> BlendAttachmentStates = 
        {
            vulkanTools::BuildPipelineColorBlendAttachmentState(0xf, VK_FALSE)
        };
        ColorBlendState.attachmentCount=(uint32_t)BlendAttachmentStates.size();
        ColorBlendState.pAttachments=BlendAttachmentStates.data();
        Resources.Pipelines->Add("Cubemap", PipelineCreateInfo, App->PipelineCache);
    }   
}

void hybridRenderer::RenderGUI()
{

}




void hybridRenderer::BuildCommandBuffers()
{   
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();

    //G-buffer pass
    std::array<VkClearValue, 2> ClearValues = {};
    ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
    ClearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
    RenderPassBeginInfo.renderPass = App->RenderPass;
    RenderPassBeginInfo.renderArea.extent.width = App->Width;
    RenderPassBeginInfo.renderArea.extent.height = App->Height;
    RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
    RenderPassBeginInfo.pClearValues=ClearValues.data();

    
    App->ImGuiHelper->UpdateBuffers();

    //Rendering abstraction : 
    //Foreach command buffer
    //  Scene.Render(CommandBuffer)
    //      For each object in scene
    //          BindPipeline(Mesh.MatType) - Mattype being pbr, non - pbr....
    //          Bind vert/ind buffer, bind descriptor sets
    //          Draw    
    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        RenderPassBeginInfo.framebuffer = App->AppFramebuffers[i];
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));

        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


        VkViewport Viewport = vulkanTools::BuildViewport((float)App->Width - App->Scene->ViewportStart, (float)App->Height, 0.0f, 1.0f, App->Scene->ViewportStart, 0);
        vkCmdSetViewport(DrawCommandBuffers[i], 0, 1, &Viewport);

        VkRect2D Scissor = vulkanTools::BuildRect2D(App->Width,App->Height,0,0);
        vkCmdSetScissor(DrawCommandBuffers[i], 0, 1, &Scissor);

        VkDeviceSize Offset[1] = {0};

        VkPipelineLayout RendererPipelineLayout =  Resources.PipelineLayouts->Get("Scene");
        VkDescriptorSet RendererDescriptorSet = App->Scene->Resources.DescriptorSets->Get("Scene");
        for(auto &InstanceGroup : App->Scene->Instances)
        {
            int Flag = InstanceGroup.first;
            vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get(Flag)); 
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 0, 1, &RendererDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 3, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 4, 1, &DescriptorSet, 0, nullptr);

            for(auto Instance : InstanceGroup.second)
            {
                  vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &Instance.Mesh->VertexBuffer.Buffer, Offset);
                vkCmdBindIndexBuffer(DrawCommandBuffers[i], Instance.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 1, 1, &Instance.Mesh->Material->DescriptorSet, 0, nullptr);
                vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, RendererPipelineLayout, 2, 1, &Instance.DescriptorSet, 0, nullptr);
                vkCmdDrawIndexed(DrawCommandBuffers[i], Instance.Mesh->IndexCount, 1, 0, 0, 0);
            }
        }

        { //Cubemap
            vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.Pipelines->Get("Cubemap"));
            vkCmdBindVertexBuffers(DrawCommandBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &App->Scene->Cubemap.Mesh.VertexBuffer.Buffer, Offset);
            vkCmdBindIndexBuffer(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 0, 1, &RendererDescriptorSet, 0, nullptr);
            vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, Resources.PipelineLayouts->Get("Cubemap"), 1, 1, &App->Scene->Cubemap.DescriptorSet, 0, nullptr);
            vkCmdDrawIndexed(DrawCommandBuffers[i], App->Scene->Cubemap.Mesh.IndexCount, 1, 0, 0, 0);
        }

        App->ImGuiHelper->DrawFrame(DrawCommandBuffers[i]);
        vkCmdEndRenderPass(DrawCommandBuffers[i]);
        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}

void hybridRenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void hybridRenderer::Destroy()
{
    for(size_t i=0; i<ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(Device, ShaderModules[i], nullptr);
    }
    Resources.Destroy();
    vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
    vkFreeCommandBuffers(Device, App->CommandPool, (uint32_t)DrawCommandBuffers.size(), DrawCommandBuffers.data());
}