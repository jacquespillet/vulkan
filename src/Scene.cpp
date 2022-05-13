#include "Scene.h"
#include "App.h"


scene::scene(vulkanApp *App) :
App(App), Device(App->Device), Queue(App->Queue), TextureLoader(App->TextureLoader), DefaultUBO(&App->UniformBuffers.SceneMatrices) {}

void scene::LoadMaterials()
{
    App->Resources.Textures->AddTexture2D("Dummy.Diffuse", "resources/models/sponza/dummy.dds", VK_FORMAT_BC2_UNORM_BLOCK);
    App->Resources.Textures->AddTexture2D("Dummy.Specular", "resources/models/sponza/dummy_specular.dds", VK_FORMAT_BC2_UNORM_BLOCK);
    App->Resources.Textures->AddTexture2D("Dummy.Bump", "resources/models/sponza/dummy_ddn.dds", VK_FORMAT_BC2_UNORM_BLOCK);

    Materials.resize(AScene->mNumMaterials);
    for(uint32_t i=0; i<Materials.size(); i++)
    {
        Materials[i] = {};
        
        aiString Name;
        AScene->mMaterials[i]->Get(AI_MATKEY_NAME, Name);
        Materials[i].Name = Name.C_Str();

        aiString TextureFile;
        std::string DiffuseMapFile;

        AScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &TextureFile);
        if(AScene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE) > 0)
        {
            std::string FileName = std::string(TextureFile.C_Str());
            DiffuseMapFile = FileName;
            std::replace(FileName.begin(), FileName.end(), '\\', '/');
            if(!App->Resources.Textures->Present(FileName))
            {
                Materials[i].Diffuse = App->Resources.Textures->AddTexture2D(FileName, "resources/models/sponza/" + FileName, VK_FORMAT_BC2_UNORM_BLOCK);
            }
            else
            {
                Materials[i].Diffuse = App->Resources.Textures->Get(FileName);
            }
        }
        else
        {
            Materials[i].Diffuse = App->Resources.Textures->Get("Dummy.Diffuse");
        }


        if(AScene->mMaterials[i]->GetTextureCount(aiTextureType_SPECULAR) > 0)
        {
            AScene->mMaterials[i]->GetTexture(aiTextureType_SPECULAR, 0, &TextureFile);
            std::string FileName = std::string(TextureFile.C_Str());
            std::replace(FileName.begin(), FileName.end(), '\\', '/');
            if(!App->Resources.Textures->Present(FileName))
            {
                Materials[i].Specular = App->Resources.Textures->AddTexture2D(FileName, "resources/models/sponza/" + FileName, VK_FORMAT_BC2_UNORM_BLOCK);
            }
            else
            {
                Materials[i].Specular = App->Resources.Textures->Get(FileName);
            }
        }
        else
        {
            Materials[i].Specular = App->Resources.Textures->Get("Dummy.Specular");
        }

        if(AScene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
        {
            AScene->mMaterials[i]->GetTexture(aiTextureType_NORMALS, 0, &TextureFile);
            std::string FileName = std::string(TextureFile.C_Str());
            std::replace(FileName.begin(), FileName.end(), '\\', '/');
            if(!App->Resources.Textures->Present(FileName))
            {
                Materials[i].Bump = App->Resources.Textures->AddTexture2D(FileName, "resources/models/sponza/" + FileName, VK_FORMAT_BC2_UNORM_BLOCK);
            }
            else
            {
                Materials[i].Bump = App->Resources.Textures->Get(FileName);
            }
        }
        else
        {
            Materials[i].Bump = App->Resources.Textures->Get("Dummy.Bump");
        }

        if(AScene->mMaterials[i]->GetTextureCount(aiTextureType_OPACITY)>0)
        {
            Materials[i].HasAlpha=true;
        }

        Materials[i].Pipeline = App->Resources.Pipelines->Get("Scene.Solid");
    }
}   

void scene::LoadMeshes(VkCommandBuffer CopyCommandBuffer)
{
    std::vector<vertex> GVertices;
    std::vector<uint32_t> GIndices;
    uint32_t GIndexBase=0;
    Meshes.resize(AScene->mNumMeshes);

    for(uint32_t i=0; i<Meshes.size(); i++)
    {
        aiMesh *AMesh = AScene->mMeshes[i];

        Meshes[i].Material = &Materials[AMesh->mMaterialIndex];
        Meshes[i].IndexBase = GIndexBase;
        
        std::vector<vertex> Vertices(AMesh->mNumVertices);

        bool HasUV = AMesh->HasTextureCoords(0);
        bool HasTangent = AMesh->HasTangentsAndBitangents();
        uint32_t VertexBase = (uint32_t)GVertices.size();

        for(size_t j=0; j<Vertices.size(); j++)
        {
            Vertices[j].Position = glm::make_vec3(&AMesh->mVertices[j].x);
            Vertices[j].Position.y = -Vertices[j].Position.y;
            Vertices[j].UV = (HasUV) ? glm::make_vec2(&AMesh->mTextureCoords[0][j].x) : glm::vec2(0);
            Vertices[j].Normal = glm::make_vec3(&AMesh->mNormals[j].x);
            Vertices[j].Color = glm::vec3(1.0f);
            Vertices[j].Tangent = (HasTangent) ? glm::make_vec3(&AMesh->mTangents[j].x) : glm::vec3(0,1,0);
            Vertices[j].Bitangent = (HasTangent) ? glm::make_vec3(&AMesh->mBitangents[j].x) : glm::vec3(0,1,0);
            GVertices.push_back(Vertices[j]);
        }

        std::vector<uint32_t> Indices;
        Meshes[i].IndexCount = AMesh->mNumFaces * 3;
        Indices.resize(AMesh->mNumFaces * 3);

        for(uint32_t j=0; j<AMesh->mNumFaces; j++)
        {
            Indices[j * 3 + 0] = AMesh->mFaces[j].mIndices[0];
            Indices[j * 3 + 1] = AMesh->mFaces[j].mIndices[1];
            Indices[j * 3 + 2] = AMesh->mFaces[j].mIndices[2];
            GIndices.push_back(Indices[j*3+0] + VertexBase);
            GIndices.push_back(Indices[j*3+1] + VertexBase);
            GIndices.push_back(Indices[j*3+2] + VertexBase);
            GIndexBase+=3;
        }

        uint32_t VertexDataSize = (uint32_t)Vertices.size() * sizeof(vertex);
        uint32_t IndexDataSize = (uint32_t)Indices.size() * sizeof(uint32_t);

        VkMemoryAllocateInfo MemAlloc = vulkanTools::BuildMemoryAllocateInfo();
        VkMemoryRequirements MemoryRequirements;

        void *Data;

        struct
        {
            struct 
            {
                VkDeviceMemory Memory;
                VkBuffer Buffer;
            } VBuffer;
            struct 
            {
                VkDeviceMemory Memory;
                VkBuffer Buffer;
            } IBuffer;
        } Staging;

        //Vertex
        VkBufferCreateInfo VBufferInfo;
        
        VBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VertexDataSize);
        VK_CALL(vkCreateBuffer(Device, &VBufferInfo, nullptr, &Staging.VBuffer.Buffer));
        vkGetBufferMemoryRequirements(Device, Staging.VBuffer.Buffer, &MemoryRequirements);
        MemAlloc.allocationSize = MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Staging.VBuffer.Memory));
        VK_CALL(vkMapMemory(Device, Staging.VBuffer.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
        memcpy(Data, Vertices.data(), VertexDataSize);
        vkUnmapMemory(Device, Staging.VBuffer.Memory);
        VK_CALL(vkBindBufferMemory(Device, Staging.VBuffer.Buffer, Staging.VBuffer.Memory, 0));

        VBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VertexDataSize);
        VK_CALL(vkCreateBuffer(Device, &VBufferInfo, nullptr, &Meshes[i].VertexBuffer));
        vkGetBufferMemoryRequirements(Device, Meshes[i].VertexBuffer, &MemoryRequirements);
        MemAlloc.allocationSize=MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Meshes[i].VertexMemory));
        VK_CALL(vkBindBufferMemory(Device, Meshes[i].VertexBuffer, Meshes[i].VertexMemory, 0));

        //Index
        VkBufferCreateInfo IBufferInfo;

        IBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, IndexDataSize);
        VK_CALL(vkCreateBuffer(Device, &IBufferInfo, nullptr, &Staging.IBuffer.Buffer));
        vkGetBufferMemoryRequirements(Device, Staging.IBuffer.Buffer, &MemoryRequirements);
        MemAlloc.allocationSize = MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Staging.IBuffer.Memory));
        VK_CALL(vkMapMemory(Device, Staging.IBuffer.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
        memcpy(Data, Indices.data(), IndexDataSize);
        vkUnmapMemory(Device, Staging.IBuffer.Memory);
        VK_CALL(vkBindBufferMemory(Device, Staging.IBuffer.Buffer, Staging.IBuffer.Memory, 0));

        IBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, IndexDataSize);
        VK_CALL(vkCreateBuffer(Device, &IBufferInfo, nullptr, &Meshes[i].IndexBuffer));
        vkGetBufferMemoryRequirements(Device, Meshes[i].IndexBuffer, &MemoryRequirements);
        MemAlloc.allocationSize=MemoryRequirements.size;
        MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Meshes[i].IndexMemory));
        VK_CALL(vkBindBufferMemory(Device, Meshes[i].IndexBuffer, Meshes[i].IndexMemory, 0));

        //Copy
        VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
        VK_CALL(vkBeginCommandBuffer(CopyCommandBuffer, &CommandBufferBeginInfo));

        VkBufferCopy CopyRegion = {};

        CopyRegion.size = VertexDataSize;
        vkCmdCopyBuffer(
            CopyCommandBuffer,
            Staging.VBuffer.Buffer,
            Meshes[i].VertexBuffer,
            1,
            &CopyRegion
        );

        CopyRegion.size = IndexDataSize;
        vkCmdCopyBuffer(
            CopyCommandBuffer,
            Staging.IBuffer.Buffer,
            Meshes[i].IndexBuffer,
            1,
            &CopyRegion
        );

        VK_CALL(vkEndCommandBuffer(CopyCommandBuffer));

        VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        SubmitInfo.commandBufferCount=1;
        SubmitInfo.pCommandBuffers = &CopyCommandBuffer;

        VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
        VK_CALL(vkQueueWaitIdle(Queue));

        vkDestroyBuffer(Device, Staging.VBuffer.Buffer, nullptr);
        vkFreeMemory(Device, Staging.VBuffer.Memory, nullptr);
        vkDestroyBuffer(Device, Staging.IBuffer.Buffer, nullptr);
        vkFreeMemory(Device, Staging.IBuffer.Memory, nullptr);
    }

    size_t VertexDataSize = GVertices.size() * sizeof(vertex);
    size_t IndexDataSize = GIndices.size() * sizeof(uint32_t);

    VkMemoryAllocateInfo MemAlloc = vulkanTools::BuildMemoryAllocateInfo();
    VkMemoryRequirements MemReqs;

    void *Data;

    struct
    {
        struct 
        {
            VkDeviceMemory Memory;
            VkBuffer Buffer;
        } VBuffer;
        struct 
        {
            VkDeviceMemory Memory;
            VkBuffer Buffer;
        } IBuffer;
    } Staging;

    //Vertex
    VkBufferCreateInfo VBufferInfo;
    
    VBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VertexDataSize);
    VK_CALL(vkCreateBuffer(Device, &VBufferInfo, nullptr, &Staging.VBuffer.Buffer));
    vkGetBufferMemoryRequirements(Device, Staging.VBuffer.Buffer, &MemReqs);
    MemAlloc.allocationSize = MemReqs.size;
    MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Staging.VBuffer.Memory));
    VK_CALL(vkMapMemory(Device, Staging.VBuffer.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
    memcpy(Data, GVertices.data(), VertexDataSize);
    vkUnmapMemory(Device, Staging.VBuffer.Memory);
    VK_CALL(vkBindBufferMemory(Device, Staging.VBuffer.Buffer, Staging.VBuffer.Memory, 0));

    VBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VertexDataSize);
    VK_CALL(vkCreateBuffer(Device, &VBufferInfo, nullptr, &VertexBuffer.Buffer));
    vkGetBufferMemoryRequirements(Device, VertexBuffer.Buffer, &MemReqs);
    MemAlloc.allocationSize=MemReqs.size;
    MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &VertexBuffer.Memory));
    VK_CALL(vkBindBufferMemory(Device, VertexBuffer.Buffer, VertexBuffer.Memory, 0));

    //Index
    VkBufferCreateInfo IBufferInfo;

    IBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, IndexDataSize);
    VK_CALL(vkCreateBuffer(Device, &IBufferInfo, nullptr, &Staging.IBuffer.Buffer));
    vkGetBufferMemoryRequirements(Device, Staging.IBuffer.Buffer, &MemReqs);
    MemAlloc.allocationSize = MemReqs.size;
    MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &Staging.IBuffer.Memory));
    VK_CALL(vkMapMemory(Device, Staging.IBuffer.Memory, 0, VK_WHOLE_SIZE, 0, &Data));
    memcpy(Data, GIndices.data(), IndexDataSize);
    vkUnmapMemory(Device, Staging.IBuffer.Memory);
    VK_CALL(vkBindBufferMemory(Device, Staging.IBuffer.Buffer, Staging.IBuffer.Memory, 0));

    IBufferInfo = vulkanTools::BuildBufferCreateInfo(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, IndexDataSize);
    VK_CALL(vkCreateBuffer(Device, &IBufferInfo, nullptr, &IndexBuffer.Buffer));
    vkGetBufferMemoryRequirements(Device, IndexBuffer.Buffer, &MemReqs);
    MemAlloc.allocationSize=MemReqs.size;
    MemAlloc.memoryTypeIndex = App->VulkanDevice->GetMemoryType(MemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(Device, &MemAlloc, nullptr, &IndexBuffer.Memory));
    VK_CALL(vkBindBufferMemory(Device, IndexBuffer.Buffer, IndexBuffer.Memory, 0));

    //Copy
    VkCommandBufferBeginInfo CommandBufferBeginInfo = vulkanTools::BuildCommandBufferBeginInfo();
    VK_CALL(vkBeginCommandBuffer(CopyCommandBuffer, &CommandBufferBeginInfo));

    VkBufferCopy CopyRegion = {};

    CopyRegion.size = VertexDataSize;
    vkCmdCopyBuffer(
        CopyCommandBuffer,
        Staging.VBuffer.Buffer,
        VertexBuffer.Buffer,
        1,
        &CopyRegion
    );

    CopyRegion.size = IndexDataSize;
    vkCmdCopyBuffer(
        CopyCommandBuffer,
        Staging.IBuffer.Buffer,
        IndexBuffer.Buffer,
        1,
        &CopyRegion
    );

    VK_CALL(vkEndCommandBuffer(CopyCommandBuffer));

    VkSubmitInfo SubmitInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    SubmitInfo.commandBufferCount=1;
    SubmitInfo.pCommandBuffers = &CopyCommandBuffer;

    VK_CALL(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));
    VK_CALL(vkQueueWaitIdle(Queue));

    vkDestroyBuffer(Device, Staging.VBuffer.Buffer, nullptr);
    vkFreeMemory(Device, Staging.VBuffer.Memory, nullptr);
    vkDestroyBuffer(Device, Staging.IBuffer.Buffer, nullptr);
    vkFreeMemory(Device, Staging.IBuffer.Memory, nullptr);



    //Descriptor sets
    std::vector<VkDescriptorPoolSize> PoolSizes;
    PoolSizes.push_back(vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (uint32_t)Meshes.size()));
    PoolSizes.push_back(vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)Meshes.size() * 3));

    VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
        (uint32_t)PoolSizes.size(),
        PoolSizes.data(),
        (uint32_t)Meshes.size()
    );
    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &DescriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;

    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_VERTEX_BIT,
            0
        )
    );

    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            1
        )
    );

    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            2
        )
    );

    SetLayoutBindings.push_back(
        vulkanTools::BuildDescriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            3
        )
    );

    VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
    VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &DescriptorSetLayout));

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(&DescriptorSetLayout, 1);
    VK_CALL(vkCreatePipelineLayout(Device, &pPipelineLayoutCreateInfo, nullptr, &PipelineLayout));

    for(uint32_t i=0; i<Meshes.size(); i++)
    {
        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
        VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Meshes[i].DescriptorSet));

        std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
        WriteDescriptorSets.push_back(vulkanTools::BuildWriteDescriptorSet(
            Meshes[i].DescriptorSet,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            0,
            &DefaultUBO->Descriptor
        ));
        WriteDescriptorSets.push_back(vulkanTools::BuildWriteDescriptorSet(
            Meshes[i].DescriptorSet,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            &Meshes[i].Material->Diffuse.Descriptor
        ));
        WriteDescriptorSets.push_back(vulkanTools::BuildWriteDescriptorSet(
            Meshes[i].DescriptorSet,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            &Meshes[i].Material->Specular.Descriptor
        ));
        WriteDescriptorSets.push_back(vulkanTools::BuildWriteDescriptorSet(
            Meshes[i].DescriptorSet,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            3,
            &Meshes[i].Material->Bump.Descriptor
        ));
        vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
    }




}

void scene::Load(std::string FileName, VkCommandBuffer CopyCommand)
{
    Assimp::Importer Importer;
    int Flags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

    AScene = Importer.ReadFile(FileName.c_str(), Flags);
    if(AScene)
    {
        LoadMaterials();
        LoadMeshes(CopyCommand);
    }
}