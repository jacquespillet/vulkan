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

        //Vertex
        uint32_t VertexDataSize = (uint32_t)Vertices.size() * sizeof(vertex);
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            Vertices.data(),
            VertexDataSize,
            &Meshes[i].VertexBuffer,
            &Meshes[i].VertexMemory,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            CopyCommandBuffer,
            Queue
        );
    
        //Index
        uint32_t IndexDataSize = (uint32_t)Indices.size() * sizeof(uint32_t);
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            Indices.data(),
            IndexDataSize,
            &Meshes[i].IndexBuffer,
            &Meshes[i].IndexMemory,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            CopyCommandBuffer,
            Queue
        );       
    }
    //Global buffers
    size_t VertexDataSize = GVertices.size() * sizeof(vertex);
    vulkanTools::CreateAndFillBuffer(
        App->VulkanDevice,
        GVertices.data(),
        VertexDataSize,
        &VertexBuffer,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        CopyCommandBuffer,
        Queue
    );
    size_t IndexDataSize = GIndices.size() * sizeof(uint32_t);
    vulkanTools::CreateAndFillBuffer(
        App->VulkanDevice,
        GIndices.data(),
        IndexDataSize,
        &IndexBuffer,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        CopyCommandBuffer,
        Queue
    );        
    

    //Descriptor sets : each mesh has its own descriptor set
    
    //Create descriptor pool
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (uint32_t)Meshes.size()),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (uint32_t)Meshes.size() * 3)
    };
    VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
        (uint32_t)PoolSizes.size(),
        PoolSizes.data(),
        (uint32_t)Meshes.size()
    );
    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &DescriptorPool));


    //Create descriptor set layout and pipeline layout
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = {
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ),
    };
    VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
    VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &DescriptorSetLayout));
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(&DescriptorSetLayout, 1);
    VK_CALL(vkCreatePipelineLayout(Device, &pPipelineLayoutCreateInfo, nullptr, &PipelineLayout));

    //Write descriptor sets
    for(uint32_t i=0; i<Meshes.size(); i++)
    {
        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
        VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Meshes[i].DescriptorSet));

        std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
        {
            vulkanTools::BuildWriteDescriptorSet( Meshes[i].DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &DefaultUBO->Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Meshes[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Meshes[i].Material->Diffuse.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Meshes[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &Meshes[i].Material->Specular.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Meshes[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &Meshes[i].Material->Bump.Descriptor)
        };
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