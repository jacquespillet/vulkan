#include "PathTraceRTXRenderer.h"
#include "App.h"
#include "imgui.h"
#include "OpenImageDenoise/oidn.hpp"

void accelerationStructure::Create(vulkanDevice *_VulkanDevice, VkAccelerationStructureTypeKHR Type, VkAccelerationStructureBuildSizesInfoKHR BuildSizeInfo)
{
    if(AccelerationStructure != VK_NULL_HANDLE)
    {
        Destroy();
    }

    this->VulkanDevice = _VulkanDevice;

    VkBufferCreateInfo BufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferCreateInfo.size = BuildSizeInfo.accelerationStructureSize;
    BufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &Buffer));

    VkMemoryRequirements MemoryRequirements {};
    vkGetBufferMemoryRequirements(VulkanDevice->Device, Buffer, &MemoryRequirements);

    VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindBufferMemory(VulkanDevice->Device, Buffer, Memory, 0));

    VkAccelerationStructureCreateInfoKHR AccelerationStructureCreateInfo {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    AccelerationStructureCreateInfo.buffer = Buffer;
    AccelerationStructureCreateInfo.size = BuildSizeInfo.accelerationStructureSize;
    AccelerationStructureCreateInfo.type = Type;
    VulkanDevice->_vkCreateAccelerationStructureKHR(VulkanDevice->Device, &AccelerationStructureCreateInfo, nullptr, &AccelerationStructure);

    VkAccelerationStructureDeviceAddressInfoKHR AccelerationStructureDeviceAddressInfo {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    AccelerationStructureDeviceAddressInfo.accelerationStructure = AccelerationStructure;
    DeviceAddress = VulkanDevice->_vkGetAccelerationStructureDeviceAddressKHR(VulkanDevice->Device, &AccelerationStructureDeviceAddressInfo);
}

void accelerationStructure::Destroy()
{
    vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
    vkDestroyBuffer(VulkanDevice->Device, Buffer, nullptr);
    VulkanDevice->_vkDestroyAccelerationStructureKHR(VulkanDevice->Device, AccelerationStructure, nullptr);
}

scratchBuffer::scratchBuffer(vulkanDevice *VulkanDevice, VkDeviceSize Size)
{
    this->VulkanDevice = VulkanDevice;

    VkBufferCreateInfo BufferCreateInfo {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferCreateInfo.size = Size;
    BufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VK_CALL(vkCreateBuffer(VulkanDevice->Device, &BufferCreateInfo, nullptr, &Buffer));

    VkMemoryRequirements MemoryRequirements {};
    vkGetBufferMemoryRequirements(VulkanDevice->Device, Buffer, &MemoryRequirements);

    VkMemoryAllocateFlagsInfo MemoryAllocateFlagsInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
    MemoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    VkMemoryAllocateInfo MemoryAllocateInfo {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    MemoryAllocateInfo.pNext = &MemoryAllocateFlagsInfo;
    MemoryAllocateInfo.allocationSize = MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindBufferMemory(VulkanDevice->Device, Buffer, Memory, 0));
    
    VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferDeviceAddressInfo.buffer = Buffer;
    DeviceAddress = VulkanDevice->_vkGetBufferDeviceAddressKHR(VulkanDevice->Device, &BufferDeviceAddressInfo);
}

void scratchBuffer::Destroy()
{
    if(Memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
    }
    if(Buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(VulkanDevice->Device, Buffer, nullptr);
    }
}

void storageImage::Create(vulkanDevice *_VulkanDevice, VkCommandPool CommandPool, VkQueue Queue, VkFormat _Format, VkExtent3D Extent)
{
    this->VulkanDevice = _VulkanDevice;
    this->Format = _Format;

    if(Image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
        vkDestroyImage(VulkanDevice->Device, Image, nullptr);
        vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
        Image = VK_NULL_HANDLE;
    }

    VkImageCreateInfo ImageCreateInfo = vulkanTools::BuildImageCreateInfo();
    ImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.format = _Format;
    ImageCreateInfo.extent = Extent;
    ImageCreateInfo.mipLevels=1;
    ImageCreateInfo.arrayLayers=1;
    ImageCreateInfo.samples=VK_SAMPLE_COUNT_1_BIT;
    ImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ImageCreateInfo.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CALL(vkCreateImage(VulkanDevice->Device, &ImageCreateInfo, nullptr, &Image));

    VkMemoryRequirements MemoryRequirements;
    vkGetImageMemoryRequirements(VulkanDevice->Device, Image, &MemoryRequirements);
    VkMemoryAllocateInfo MemoryAllocateInfo = vulkanTools::BuildMemoryAllocateInfo();
    MemoryAllocateInfo.allocationSize=MemoryRequirements.size;
    MemoryAllocateInfo.memoryTypeIndex = VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(VulkanDevice->Device, &MemoryAllocateInfo, nullptr, &Memory));
    VK_CALL(vkBindImageMemory(VulkanDevice->Device, Image, Memory, 0));

    VkImageViewCreateInfo ColorImageView = vulkanTools::BuildImageViewCreateInfo();
    ColorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ColorImageView.format = Format;
    ColorImageView.subresourceRange = {};
    ColorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ColorImageView.subresourceRange.baseMipLevel=0;
    ColorImageView.subresourceRange.levelCount=1;
    ColorImageView.subresourceRange.baseArrayLayer=0;
    ColorImageView.subresourceRange.layerCount=1;
    ColorImageView.image = Image;
    VK_CALL(vkCreateImageView(VulkanDevice->Device, &ColorImageView, nullptr, &ImageView));

    VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vulkanTools::TransitionImageLayout(
        CommandBuffer,
        Image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    );
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, CommandPool, CommandBuffer, Queue, true);

}

void storageImage::Destroy()
{
    vkDestroyImageView(VulkanDevice->Device, ImageView, nullptr);
    vkDestroyImage(VulkanDevice->Device, Image, nullptr);
    vkFreeMemory(VulkanDevice->Device, Memory, nullptr);
}

void shaderBindingTable::Create(vulkanDevice *_VulkanDevice, uint32_t HandleCount, VkPhysicalDeviceRayTracingPipelinePropertiesKHR RayTracingPipelineProperties)
{
    this->Device = _VulkanDevice->Device;

    VK_CALL(vulkanTools::CreateBuffer(
        _VulkanDevice,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        this,
        RayTracingPipelineProperties.shaderGroupHandleSize * HandleCount
    ));

    VkBufferDeviceAddressInfoKHR BufferDeviceAddressInfo {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    BufferDeviceAddressInfo.buffer = Buffer;
    const uint32_t HandleSizeAligned = vulkanTools::AlignedSize(RayTracingPipelineProperties.shaderGroupHandleSize, RayTracingPipelineProperties.shaderGroupHandleAlignment);
    StrideDeviceAddressRegion.deviceAddress=_VulkanDevice->_vkGetBufferDeviceAddressKHR(Device, &BufferDeviceAddressInfo);
    StrideDeviceAddressRegion.stride = HandleSizeAligned;
    StrideDeviceAddressRegion.size = HandleCount * HandleSizeAligned;

    Map();
}

void shaderBindingTable::Destroy()
{
    buffer::Destroy();
}

pathTraceRTXRenderer::pathTraceRTXRenderer(vulkanApp *App) : renderer(App) {
    App->VulkanDevice->LoadRayTracingFuncs();
}

void pathTraceRTXRenderer::UpdateMaterial(size_t Index)
{
    UpdateMaterialStagingBuffer.Map();
    UpdateMaterialStagingBuffer.CopyTo(&App->Scene->Materials[Index].MaterialData, sizeof(materialData));
    UpdateMaterialStagingBuffer.Unmap();
    
    VkBufferCopy BufferCopy {};
    BufferCopy.size = sizeof(materialData);
    BufferCopy.dstOffset = Index * sizeof(materialData);


    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(UpdateMaterialCommandBuffer, &CommandBufferInfo));
    vkCmdCopyBuffer(UpdateMaterialCommandBuffer, UpdateMaterialStagingBuffer.Buffer, MaterialBuffer.Buffer, 1, &BufferCopy);
    vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, UpdateMaterialCommandBuffer, App->Queue, false);
}

void pathTraceRTXRenderer::Render()
{
    BuildCommandBuffers();

    if(App->Scene->Camera.Changed)
    {
        ResetAccumulation=true;
        UniformData.ShouldAccumulate=1;
    }

    if(ResetAccumulation)
    {
        UniformData.CurrentSampleCount=0;
        ResetAccumulation=false;
    }  

    if(UniformData.CurrentSampleCount < (uint32_t)UniformData.MaxSamples)
    {
        UniformData.CurrentSampleCount += UniformData.SamplersPerFrame;
    }

    VkResult Result = App->Swapchain.AcquireNextImage(App->Semaphores.PresentComplete, &App->CurrentBuffer);
    VK_CALL(Result);


    SubmitInfo = vulkanTools::BuildSubmitInfo();
    SubmitInfo.pWaitDstStageMask = &App->SubmitPipelineStages;
    SubmitInfo.waitSemaphoreCount = 1;
    SubmitInfo.signalSemaphoreCount=1;
    SubmitInfo.pWaitSemaphores = &App->Semaphores.PresentComplete;
    SubmitInfo.pSignalSemaphores = &App->Semaphores.RenderComplete;
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &DrawCommandBuffers[App->CurrentBuffer];
    

    VK_CALL(vkQueueSubmit(App->Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
    Result = App->Swapchain.QueuePresent(App->Queue, App->CurrentBuffer, App->Semaphores.RenderComplete);
    VK_CALL(vkQueueWaitIdle(App->Queue));

    if(ShouldDenoise)
    {
        //Copy the content of the denoise buffer from gpu to cpu
        DenoiseBuffer.Map();
        memcpy(DenoiserInputUint.data(), DenoiseBuffer.Mapped, DenoiserInputUint.size() * sizeof(rgba));
        DenoiseBuffer.Unmap();
        
        //Denoise
        for(size_t i=0; i<DenoiserInputUint.size(); i++)
        {
            DenoiserInput[i].r = (float)DenoiserInputUint[i].r / 255.0f;
            DenoiserInput[i].g = (float)DenoiserInputUint[i].g / 255.0f;
            DenoiserInput[i].b = (float)DenoiserInputUint[i].b / 255.0f;
        }

        oidn::DeviceRef device = oidn::newDevice();
        device.commit();

        oidn::FilterRef filter = device.newFilter("RT"); // generic ray tracing filter
        filter.setImage("color", DenoiserInput.data(), oidn::Format::Float3, App->Width, App->Height, 0, 0, 0);
        filter.setImage("output", DenoiserOutput.data(), oidn::Format::Float3, App->Width, App->Height, 0, 0, 0);
        filter.set("hdr", true);
        filter.commit();
        filter.execute();

        for(size_t i=0; i<DenoiserInputUint.size(); i++)
        {
            DenoiserInputUint[i].r = (uint8_t)(DenoiserOutput[i].r * 255.0f);
            DenoiserInputUint[i].g = (uint8_t)(DenoiserOutput[i].g * 255.0f);
            DenoiserInputUint[i].b = (uint8_t)(DenoiserOutput[i].b * 255.0f);
        }

        //Copy back to gpu
        DenoiseBuffer.Map();
        memcpy(DenoiseBuffer.Mapped, DenoiserInputUint.data(), DenoiserInputUint.size() * sizeof(rgba));
        DenoiseBuffer.Unmap();
        

        //Copy buffer back into storage buffer
        VkCommandBuffer CommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        VkImageSubresourceLayers ImageSubresource = {};
        ImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageSubresource.baseArrayLayer=0;
        ImageSubresource.layerCount=1;
        ImageSubresource.mipLevel=0;

        VkBufferImageCopy BufferImageCopy = {};
        BufferImageCopy.imageExtent.depth=1;
        BufferImageCopy.imageExtent.width = App->Width;
        BufferImageCopy.imageExtent.height = App->Height;
        BufferImageCopy.imageOffset = {0,0,0};
        BufferImageCopy.imageSubresource = ImageSubresource;

        BufferImageCopy.bufferOffset=0;
        BufferImageCopy.bufferImageHeight = 0;
        BufferImageCopy.bufferRowLength=0;
        

        // vkCmdCopyImageToBuffer(CopyCommand, StorageImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DenoiseBuffer.Buffer,1,  &BufferImageCopy);        
        vulkanTools::TransitionImageLayout(CommandBuffer, StorageImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vkCmdCopyBufferToImage(CommandBuffer, DenoiseBuffer.Buffer, StorageImage.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BufferImageCopy);
        vulkanTools::TransitionImageLayout(CommandBuffer, StorageImage.Image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        
        vulkanTools::FlushCommandBuffer(VulkanDevice->Device, App->CommandPool, CommandBuffer, App->Queue, true);

        VK_CALL(vkQueueWaitIdle(App->Queue));

        ShouldDenoise=false;
        UniformData.ShouldAccumulate=0;
    }

    UpdateUniformBuffers();
}

void pathTraceRTXRenderer::CreateBottomLevelAccelarationStructure(scene *Scene)
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

VkAccelerationStructureInstanceKHR pathTraceRTXRenderer::CreateBottomLevelAccelerationInstance(instance *Instance)
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

void pathTraceRTXRenderer::FillBLASInstances()
{
    std::vector<glm::mat4> TransformMatrices;

    BLASInstances.resize(0);
    for(auto &InstanceGroup : App->Scene->Instances)
    {
        for(size_t i=0; i<InstanceGroup.second.size(); i++)	
        {
            BLASInstances.push_back(CreateBottomLevelAccelerationInstance(&InstanceGroup.second[i]));
            TransformMatrices.push_back(InstanceGroup.second[i].InstanceData.Transform);
        }
    }

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &InstancesBuffer,
        sizeof(VkAccelerationStructureInstanceKHR) * BLASInstances.size(),
        BLASInstances.data()
    ));

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &TransformMatricesBuffer,
        sizeof(glm::mat4) * TransformMatrices.size(),
        TransformMatrices.data()
    ));


}


void pathTraceRTXRenderer::UpdateBLASInstance(uint32_t InstanceGroupIndex, uint32_t InstanceIndex, uint32_t FlatIndex)
{
    BLASInstances[FlatIndex] = CreateBottomLevelAccelerationInstance(&App->Scene->Instances[InstanceGroupIndex][InstanceIndex]);

    InstancesBuffer.Map();
    InstancesBuffer.CopyTo(&BLASInstances[FlatIndex], sizeof(VkAccelerationStructureInstanceKHR), FlatIndex * sizeof(VkAccelerationStructureInstanceKHR));
    InstancesBuffer.Unmap();

    CreateTopLevelAccelerationStructure();

    //Update descriptor set
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


void pathTraceRTXRenderer::CreateTopLevelAccelerationStructure()
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

void pathTraceRTXRenderer::CreateImages()
{
    //Contains the final image
    StorageImage.Create(VulkanDevice, App->CommandPool, App->Queue, App->Swapchain.ColorFormat, {App->Width, App->Height, 1});
    
    //Contains the accumulated colors, not divided by the sample count
    AccumulationImage.Create(VulkanDevice, App->CommandPool, App->Queue, VK_FORMAT_R32G32B32A32_SFLOAT, {App->Width, App->Height, 1});

    VK_CALL(vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &DenoiseBuffer, App->Width * App->Height * 4, nullptr));
    DenoiserInput.resize(App->Width * App->Height);
    DenoiserOutput.resize(App->Width * App->Height);
    DenoiserInputUint.resize(App->Width * App->Height);
}

void pathTraceRTXRenderer::CreateRayTracingPipeline()
{
    uint32_t ModelsSize = 1; //Number of models !
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = 
    {
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 3),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 4,  (uint32_t)ModelsSize)
    };

    uint32_t TexCount = (uint32_t)App->Scene->Resources.Textures->Count();

    if(TexCount>0)
    {
        SetLayoutBindings.push_back(
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 5,  TexCount)
        );
    }
    
    //Cubemap textures
    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6,  1)
    );
    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 7,  1)
    );

    //Scene uniform
    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 8,  1)
    );
    
    //Transform matrices buffer
    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 9,  1)
    );

    VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings);
    VK_CALL(vkCreateDescriptorSetLayout(VulkanDevice->Device, &DescriptorSetLayoutCreateInfo, nullptr, &DescriptorSetLayout));

    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(&DescriptorSetLayout, 1);
    VK_CALL(vkCreatePipelineLayout(VulkanDevice->Device, &PipelineLayoutCreateInfo, nullptr, &PipelineLayout));

    std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;
    
    {
        ShaderStages.push_back(LoadShader(VulkanDevice->Device, "resources/shaders/spv/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        ShaderModules.push_back(ShaderStages[ShaderStages.size()-1].module);
        VkRayTracingShaderGroupCreateInfoKHR ShaderGroup {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        ShaderGroup.generalShader = static_cast<uint32_t>(ShaderStages.size())-1;
        ShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        
        ShaderGroups.push_back(ShaderGroup);
    }

    {
        ShaderStages.push_back(LoadShader(VulkanDevice->Device, "resources/shaders/spv/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
        ShaderModules.push_back(ShaderStages[ShaderStages.size()-1].module);
        VkRayTracingShaderGroupCreateInfoKHR ShaderGroup {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        ShaderGroup.generalShader = static_cast<uint32_t>(ShaderStages.size())-1;
        ShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        
        ShaderGroups.push_back(ShaderGroup);
    }

    {
        ShaderStages.push_back(LoadShader(VulkanDevice->Device, "resources/shaders/spv/missShadow.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
        ShaderModules.push_back(ShaderStages[ShaderStages.size()-1].module);
        VkRayTracingShaderGroupCreateInfoKHR ShaderGroup {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        ShaderGroup.generalShader = static_cast<uint32_t>(ShaderStages.size())-1;
        ShaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        
        ShaderGroups.push_back(ShaderGroup);
    }

    {
        ShaderStages.push_back(LoadShader(VulkanDevice->Device, "resources/shaders/spv/closestHit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
        ShaderModules.push_back(ShaderStages[ShaderStages.size()-1].module);
        VkRayTracingShaderGroupCreateInfoKHR ShaderGroup {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
        ShaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        ShaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
        ShaderGroup.closestHitShader = static_cast<uint32_t>(ShaderStages.size())-1;
        ShaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        ShaderStages.push_back(LoadShader(VulkanDevice->Device, "resources/shaders/spv/anyHit.rahit.spv", VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
        ShaderModules.push_back(ShaderStages[ShaderStages.size()-1].module);
        ShaderGroup.anyHitShader = static_cast<uint32_t>(ShaderStages.size())-1;
        
        ShaderGroups.push_back(ShaderGroup);
    }

    VkRayTracingPipelineCreateInfoKHR RayTracingPipelineCreateInfo = vulkanTools::BuildRayTracingPipelineCreateInfo();
    RayTracingPipelineCreateInfo.stageCount=static_cast<uint32_t>(ShaderStages.size());
    RayTracingPipelineCreateInfo.pStages = ShaderStages.data();
    RayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(ShaderGroups.size());
    RayTracingPipelineCreateInfo.pGroups = ShaderGroups.data();
    RayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth=2;
    RayTracingPipelineCreateInfo.layout = PipelineLayout;
    VK_CALL(VulkanDevice->_vkCreateRayTracingPipelinesKHR(VulkanDevice->Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &RayTracingPipelineCreateInfo, nullptr, &Pipeline));
}

void pathTraceRTXRenderer::CreateShaderBindingTable()
{
    const uint32_t HandleSize = RayTracingPipelineProperties.shaderGroupHandleSize;
    const uint32_t HandleSizeAligned = vulkanTools::AlignedSize(RayTracingPipelineProperties.shaderGroupHandleSize, RayTracingPipelineProperties.shaderGroupHandleAlignment);
    const uint32_t GroupCount = static_cast<uint32_t>(ShaderGroups.size());
    const uint32_t SBTSize = GroupCount * HandleSizeAligned;

    std::vector<uint8_t> ShaderHandleStorage(SBTSize);
    VK_CALL(VulkanDevice->_vkGetRayTracingShaderGroupHandlesKHR(VulkanDevice->Device, Pipeline, 0, GroupCount, SBTSize, ShaderHandleStorage.data()));

    ShaderBindingTables.Raygen.Create(VulkanDevice, 1, RayTracingPipelineProperties);
    ShaderBindingTables.Miss.Create(VulkanDevice, 2, RayTracingPipelineProperties);
    ShaderBindingTables.Hit.Create(VulkanDevice, 1, RayTracingPipelineProperties);

    memcpy(ShaderBindingTables.Raygen.Mapped, ShaderHandleStorage.data(), HandleSize);
    memcpy(ShaderBindingTables.Miss.Mapped, ShaderHandleStorage.data() + HandleSizeAligned, HandleSize);
    memcpy(ShaderBindingTables.Hit.Mapped, ShaderHandleStorage.data() + HandleSizeAligned*3, HandleSize);
}

void pathTraceRTXRenderer::CreateDescriptorSets()
{
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}
    };
    VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = vulkanTools::BuildDescriptorPoolCreateInfo(PoolSizes, 1);
    VK_CALL(vkCreateDescriptorPool(VulkanDevice->Device, &DescriptorPoolCreateInfo, nullptr, &DescriptorPool));

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

    VkDescriptorImageInfo StorageImageDescriptor {VK_NULL_HANDLE, StorageImage.ImageView, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo AccumImageDescriptor {VK_NULL_HANDLE, AccumulationImage.ImageView, VK_IMAGE_LAYOUT_GENERAL};

    std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
    {
        AccelerationStructureWrite,
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &StorageImageDescriptor),
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &AccumImageDescriptor),
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &UBO.Descriptor),
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &SceneDescriptionBuffer.Descriptor)
    };

    std::vector<VkDescriptorImageInfo> ImageInfos(App->Scene->Resources.Textures->Resources.size()); 
    for(auto &Texture : App->Scene->Resources.Textures->Resources)
    {
		ImageInfos[Texture.second.Index] = Texture.second.Descriptor;
    }
    WriteDescriptorSets.push_back(
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, ImageInfos.data(), static_cast<uint32_t>(ImageInfos.size()))
    );

    WriteDescriptorSets.push_back(
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, &App->Scene->Cubemap.IrradianceMap.Descriptor, 1)
    );
    WriteDescriptorSets.push_back(
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 7, &App->Scene->Cubemap.Texture.Descriptor, 1)
    );
    WriteDescriptorSets.push_back(
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8, &App->Scene->SceneMatrices.Descriptor)
    );
    
    WriteDescriptorSets.push_back(
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9, &TransformMatricesBuffer.Descriptor)
    );

    

    vkUpdateDescriptorSets(VulkanDevice->Device, static_cast<uint32_t>(WriteDescriptorSets.size()), WriteDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void pathTraceRTXRenderer::UpdateUniformBuffers()
{
    UniformData.VertexSize = sizeof(vertex);
    memcpy(UBO.Mapped, &UniformData, sizeof(UniformData));
}   

void pathTraceRTXRenderer::BuildUniformBuffers()
{
    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice, 
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &UBO,
        sizeof(uniformData),
        &UniformData
    ));
    VK_CALL(UBO.Map());
    UpdateUniformBuffers();
}


void pathTraceRTXRenderer::CreateMaterialBuffer()
{
    std::vector<materialData> Materials;
    for(size_t i=0; i<App->Scene->Materials.size(); i++)
    {
        Materials.push_back(App->Scene->Materials[i].MaterialData);
    }

    const VkDeviceSize BufferSize = sizeof(materialData) * Materials.size();
    buffer Staging;

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &Staging,
        BufferSize,
        Materials.data()
    ));
    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &MaterialBuffer,
        BufferSize
    ));

    vulkanTools::CopyBuffer(VulkanDevice, App->CommandPool, App->Queue, &Staging, &MaterialBuffer);

    //Used for updating a material at run time
    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &UpdateMaterialStagingBuffer,
        sizeof(materialData),
        nullptr
    ));
    UpdateMaterialCommandBuffer = vulkanTools::CreateCommandBuffer(VulkanDevice->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
    

    Staging.Destroy();
}

void pathTraceRTXRenderer::Setup()
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
    CreateMaterialBuffer();

    std::vector<sceneModelInfo> SceneModelInfos;
    for(size_t i=0; i<App->Scene->Meshes.size(); i++)
    {
        sceneModelInfo Info {};
        Info.Vertices = vulkanTools::GetBufferDeviceAddress(VulkanDevice, App->Scene->Meshes[i].VertexBuffer.Buffer);
        Info.Indices = vulkanTools::GetBufferDeviceAddress(VulkanDevice, App->Scene->Meshes[i].IndexBuffer.Buffer);
        Info.Materials = vulkanTools::GetBufferDeviceAddress(VulkanDevice, MaterialBuffer.Buffer);
        // MatIndexOffset++;
        SceneModelInfos.emplace_back(Info);
    }

    VK_CALL(vulkanTools::CreateBuffer(
        VulkanDevice, 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &SceneDescriptionBuffer,
        sizeof(sceneModelInfo) * SceneModelInfos.size(),
        SceneModelInfos.data()
    ));


    CreateImages();
    BuildUniformBuffers();
    CreateRayTracingPipeline();
    CreateShaderBindingTable();
    CreateDescriptorSets();
    CreateCommandBuffers();
    BuildCommandBuffers();
    UpdateUniformBuffers();
}

//


void pathTraceRTXRenderer::CreateCommandBuffers()
{
    DrawCommandBuffers.resize(App->Swapchain.ImageCount);
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(DrawCommandBuffers.size()));
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, DrawCommandBuffers.data()));
}


void pathTraceRTXRenderer::Denoise()
{
    ShouldDenoise=true;
    // vulkanTools::CopyImageToBuffer(VulkanDevice, App->CommandPool, App->Queue, StorageImage.Image, &DenoiseBuffer, App->Width, App->Height);
}

void pathTraceRTXRenderer::RenderGUI()
{
    if(ImGui::CollapsingHeader("Path Tracing Options"))
    {
        bool ShouldReset=false;
        ShouldReset |= ImGui::SliderInt("Samples Per Pixel", &UniformData.SamplersPerFrame, 1, 32);
        ShouldReset |= ImGui::SliderInt("Ray Bounces", &UniformData.RayBounces, 1, 32);
        ShouldReset |= ImGui::SliderInt("Max Samples", &UniformData.MaxSamples, 1, 16384);
        ImGui::Text("Num Samples %d", UniformData.CurrentSampleCount);

        if(ImGui::Button("Denoise"))
        {
            Denoise();
        }
        if(ShouldReset)
        {
            ResetAccumulation=true;
        }
    }
}




void pathTraceRTXRenderer::BuildCommandBuffers()
{   
    VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    
    App->ImGuiHelper->UpdateBuffers();



    for(uint32_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        VK_CALL(vkBeginCommandBuffer(DrawCommandBuffers[i], &CommandBufferInfo));
        
        VkClearValue ClearValues[2];
        ClearValues[0].color = { { 0.0f, 0.0f, 0.2f, 1.0f } };;
        ClearValues[1].depthStencil = { 1.0f, 0 };
        
		VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
		RenderPassBeginInfo.renderPass = App->RenderPass;
		RenderPassBeginInfo.framebuffer = App->AppFramebuffers[i];
		RenderPassBeginInfo.renderArea.extent.width = App->Width;
		RenderPassBeginInfo.renderArea.extent.height = App->Height;
		RenderPassBeginInfo.clearValueCount = 2;
		RenderPassBeginInfo.pClearValues = ClearValues;           
        
        VkStridedDeviceAddressRegionKHR EmptySbtEntry={};

        vkCmdBindPipeline(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, Pipeline);
        vkCmdBindDescriptorSets(DrawCommandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, PipelineLayout, 0, 1, &DescriptorSet, 0, 0);

        //Trace rays, write into the accumulation texture
        VulkanDevice->_vkCmdTraceRaysKHR(
            DrawCommandBuffers[i],
            &ShaderBindingTables.Raygen.StrideDeviceAddressRegion,
            &ShaderBindingTables.Miss.StrideDeviceAddressRegion,
            &ShaderBindingTables.Hit.StrideDeviceAddressRegion,
            &EmptySbtEntry,
            App->Width - (int)App->GuiWidth, App->Height, 1
        );

        vulkanTools::TransitionImageLayout(DrawCommandBuffers[i], App->Swapchain.Images[i], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        vulkanTools::TransitionImageLayout(DrawCommandBuffers[i], StorageImage.Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);
        
        //Copy accumulation texture to swapchain image
        VkImageCopy CopyRegion {};
        CopyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        CopyRegion.srcOffset = {0,0,0};
        CopyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        CopyRegion.dstOffset = {(int)App->GuiWidth,0,0};
        CopyRegion.extent = {App->Width - (int)App->GuiWidth, App->Height, 1};
        vkCmdCopyImage(DrawCommandBuffers[i], StorageImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, App->Swapchain.Images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyRegion);

        if(ShouldDenoise)
        {
            //Copy result to the denoise buffer
            VkImageSubresourceLayers ImageSubresource = {};
            ImageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ImageSubresource.baseArrayLayer=0;
            ImageSubresource.layerCount=1;
            ImageSubresource.mipLevel=0;

            VkBufferImageCopy BufferImageCopy = {};
            BufferImageCopy.imageExtent.depth=1;
            BufferImageCopy.imageExtent.width = App->Width;
            BufferImageCopy.imageExtent.height = App->Height;
            BufferImageCopy.imageOffset = {0,0,0};
            BufferImageCopy.imageSubresource = ImageSubresource;

            BufferImageCopy.bufferOffset=0;
            BufferImageCopy.bufferImageHeight = 0;
            BufferImageCopy.bufferRowLength=0;
            
            vkCmdCopyImageToBuffer(DrawCommandBuffers[i], StorageImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DenoiseBuffer.Buffer,1,  &BufferImageCopy);
        }

        vulkanTools::TransitionImageLayout(DrawCommandBuffers[i], App->Swapchain.Images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SubresourceRange);
        vulkanTools::TransitionImageLayout(DrawCommandBuffers[i], StorageImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, SubresourceRange);
        
        //Imgui
        vkCmdBeginRenderPass(DrawCommandBuffers[i], &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        App->ImGuiHelper->DrawFrame(DrawCommandBuffers[i]);
        vkCmdEndRenderPass(DrawCommandBuffers[i]);

        VK_CALL(vkEndCommandBuffer(DrawCommandBuffers[i]));
    }
}


void pathTraceRTXRenderer::Destroy()
{
    for(size_t i=0; i<BottomLevelAccelerationStructures.size(); i++)
    {
        BottomLevelAccelerationStructures[i].Destroy();
    }
    TopLevelAccelerationStructure.Destroy();
    InstancesBuffer.Destroy();

    MaterialBuffer.Destroy();
    vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, 1, &UpdateMaterialCommandBuffer);
    UpdateMaterialStagingBuffer.Destroy();
    SceneDescriptionBuffer.Destroy();
    TransformMatricesBuffer.Destroy();
    StorageImage.Destroy();
    AccumulationImage.Destroy();
    DenoiseBuffer.Destroy();
    UBO.Destroy();
    
    vkDestroyDescriptorSetLayout(VulkanDevice->Device, DescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(VulkanDevice->Device, PipelineLayout, nullptr);
    vkDestroyPipeline(VulkanDevice->Device, Pipeline, nullptr);
    for(size_t i=0; i<ShaderModules.size(); i++)
    {
        vkDestroyShaderModule(VulkanDevice->Device, ShaderModules[i], nullptr);
    }

    ShaderBindingTables.Hit.Destroy();
    ShaderBindingTables.Miss.Destroy();
    ShaderBindingTables.Raygen.Destroy();

    vkFreeDescriptorSets(VulkanDevice->Device, DescriptorPool, 1, &DescriptorSet);
    vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);

    for(size_t i=0; i<DrawCommandBuffers.size(); i++)
    {
        vkFreeCommandBuffers(VulkanDevice->Device, App->CommandPool, 1, &DrawCommandBuffers[i]);
    }
    // for(size_t i=0; i<ShaderModules.size(); i++)
    // {
    //     vkDestroyShaderModule(Device, ShaderModules[i], nullptr);
    // }
    // //Resources.Destroy();
    // vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
    // vkFreeCommandBuffers(Device, App->CommandPool, (uint32_t)DrawCommandBuffers.size(), DrawCommandBuffers.data());
}