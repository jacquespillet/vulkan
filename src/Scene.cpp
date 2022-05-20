#include "Scene.h"
#include "App.h"
#include "Resources.h"
#include "AssimpImporter.h"
#include "GLTFImporter.h"
#include "IBLHelper.h"



scene::scene(vulkanApp *App) :
            App(App), Device(App->Device), 
            Queue(App->Queue), TextureLoader(App->TextureLoader)
{
    this->Textures = new textureList(Device, TextureLoader);
}


void scene::Load(std::string FileName, VkCommandBuffer CopyCommand)
{
    
    { //Cubemap
        TextureLoader->LoadCubemap("resources/belfast_farmhouse_4k.hdr", &Cubemap.Texture);
        GLTFImporter::LoadMesh("resources/models/Cube/Cube.gltf", Cubemap.Mesh);
        size_t VertexDataSize = Cubemap.Mesh.Vertices.size() * sizeof(vertex);
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            Cubemap.Mesh.Vertices.data(),
            VertexDataSize,
            &Cubemap.Mesh.VertexBuffer,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            CopyCommand,
            Queue
        );
        size_t IndexDataSize = Cubemap.Mesh.Indices.size() * sizeof(uint32_t);
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            Cubemap.Mesh.Indices.data(),
            IndexDataSize,
            &Cubemap.Mesh.IndexBuffer,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            CopyCommand,
            Queue
        );    
        vulkanTools::CreateBuffer(App->VulkanDevice, 
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &Cubemap.UniformBuffer,
                                    sizeof(cubemap::uniformData)
        );        
        Cubemap.UniformData.modelMatrix = glm::scale(glm::mat4(1), glm::vec3(100));
        Cubemap.UniformBuffer.Map();
        Cubemap.UniformBuffer.CopyTo(&Cubemap.UniformData, sizeof(cubemap::uniformData));
        Cubemap.UniformBuffer.Unmap();

        IBLHelper::CalculateIrradianceMap(App->VulkanDevice, CopyCommand, Queue, &Cubemap.Texture, &Cubemap.IrradianceMap);        
        IBLHelper::CalculatePrefilteredMap(App->VulkanDevice, CopyCommand, Queue, &Cubemap.Texture, &Cubemap.PrefilteredMap);        
        IBLHelper::CalculateBRDFLUT(App->VulkanDevice, CopyCommand, Queue, &Cubemap.BRDFLUT);        
    }

    { //Scene

        std::vector<vertex> GVertices;
        std::vector<uint32_t> GIndices;

        std::string Extension = FileName.substr(FileName.find_last_of(".") + 1);
        if(Extension == "gltf" || Extension == "glb")
        {
            GLTFImporter::Load(FileName, Instances, Meshes, Materials,GVertices, GIndices, Textures);    
        }
        else
        {
            assimpImporter::Load(FileName, Instances, Meshes, Materials,GVertices, GIndices, Textures);    
        }


        for (size_t i = 0; i < Materials.size(); i++)
        {
            vulkanTools::CreateBuffer(App->VulkanDevice,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &Materials[i].UniformBuffer,
                                     sizeof(Materials[i].MaterialData),
                                     &Materials[i].MaterialData);             
        }
        
        for (size_t i = 0; i < Instances.size(); i++)
        {
            vulkanTools::CreateBuffer(App->VulkanDevice,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &Instances[i].UniformBuffer,
                                     sizeof(Instances[i].InstanceData),
                                     &Instances[i].InstanceData);   
        }

        for(uint32_t i=0; i<Meshes.size(); i++)
        {
            size_t VertexDataSize = Meshes[i].Vertices.size() * sizeof(vertex);
            vulkanTools::CreateAndFillBuffer(
                App->VulkanDevice,
                Meshes[i].Vertices.data(),
                VertexDataSize,
                &Meshes[i].VertexBuffer,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                CopyCommand,
                Queue
            );
            size_t IndexDataSize = Meshes[i].Indices.size() * sizeof(uint32_t);
            vulkanTools::CreateAndFillBuffer(
                App->VulkanDevice,
                Meshes[i].Indices.data(),
                IndexDataSize,
                &Meshes[i].IndexBuffer,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                CopyCommand,
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
            CopyCommand,
            Queue
        );
        size_t IndexDataSize = GIndices.size() * sizeof(uint32_t);
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            GIndices.data(),
            IndexDataSize,
            &IndexBuffer,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            CopyCommand,
            Queue
        ); 
    }

    //Fill command buffers in the renderers to render it in last
}

void scene::CreateDescriptorSets()
{
    {
        //Create Material descriptor pool
        std::vector<VkDescriptorPoolSize> PoolSizes = 
        {
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  (uint32_t)Materials.size() * 3),
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  (uint32_t)Materials.size())
        };
        VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
            (uint32_t)PoolSizes.size(),
            PoolSizes.data(),
            (uint32_t)Materials.size()
        );
        VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &MaterialDescriptorPool));


        //Create descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ));
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
        VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &MaterialDescriptorSetLayout));

        //Write descriptor sets
        for(uint32_t i=0; i<Materials.size(); i++)
        {
            VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(MaterialDescriptorPool, &MaterialDescriptorSetLayout, 1);
            VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Materials[i].DescriptorSet));

            std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
            WriteDescriptorSets.push_back(
            vulkanTools::BuildWriteDescriptorSet( Materials[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &Materials[i].Diffuse.Descriptor)
            );
            WriteDescriptorSets.push_back(
                vulkanTools::BuildWriteDescriptorSet( Materials[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Materials[i].Specular.Descriptor)
            );
            WriteDescriptorSets.push_back(
                vulkanTools::BuildWriteDescriptorSet( Materials[i].DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &Materials[i].Bump.Descriptor)
            );
            WriteDescriptorSets.push_back(
                vulkanTools::BuildWriteDescriptorSet( Materials[i].DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &Materials[i].UniformBuffer.Descriptor)
            );
            vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
        }
    }

    {
        //Create Instance descriptor pool
        std::vector<VkDescriptorPoolSize> PoolSizes = 
        {
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  (uint32_t)Instances.size())
        };
        VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
            (uint32_t)PoolSizes.size(),
            PoolSizes.data(),
            (uint32_t)Instances.size()
        );
        VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &InstanceDescriptorPool));


        //Create descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0 ));
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
        VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &InstanceDescriptorSetLayout));

        //Write descriptor sets
        for(uint32_t i=0; i<Instances.size(); i++)
        {
            VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(InstanceDescriptorPool, &InstanceDescriptorSetLayout, 1);
            VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Instances[i].DescriptorSet));

            std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
            WriteDescriptorSets.push_back(
                vulkanTools::BuildWriteDescriptorSet( Instances[i].DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &Instances[i].UniformBuffer.Descriptor)
            );
            vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
        }
    }    

    {
        //Create Instance descriptor pool
        std::vector<VkDescriptorPoolSize> PoolSizes = 
        {
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1),
            vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  4)
        };
        VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
            (uint32_t)PoolSizes.size(),
            PoolSizes.data(),
            1
        );
        VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &Cubemap.DescriptorPool));


        //Create descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ));
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4 ));
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
        VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &Cubemap.DescriptorSetLayout));


        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(Cubemap.DescriptorPool, &Cubemap.DescriptorSetLayout, 1);
        VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Cubemap.DescriptorSet));

        std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
        {
            vulkanTools::BuildWriteDescriptorSet( Cubemap.DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &Cubemap.UniformBuffer.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Cubemap.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Cubemap.Texture.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Cubemap.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &Cubemap.IrradianceMap.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Cubemap.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &Cubemap.PrefilteredMap.Descriptor),
            vulkanTools::BuildWriteDescriptorSet( Cubemap.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &Cubemap.BRDFLUT.Descriptor)
        };
        vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
    
    }    
}

void cubemap::Destroy(vulkanDevice *VulkanDevice)
{
    Texture.Destroy(VulkanDevice);
    IrradianceMap.Destroy(VulkanDevice);
    PrefilteredMap.Destroy(VulkanDevice);
    BRDFLUT.Destroy(VulkanDevice);
    Mesh.Destroy();


    UniformBuffer.Destroy();

    vkDestroyDescriptorSetLayout(VulkanDevice->Device, DescriptorSetLayout, nullptr);
    vkFreeDescriptorSets(VulkanDevice->Device, DescriptorPool, 1, &DescriptorSet);
    vkDestroyDescriptorPool(VulkanDevice->Device, DescriptorPool, nullptr);
    vkDestroyPipeline(VulkanDevice->Device, Pipeline, nullptr);
}

void sceneMesh::Destroy()
{
    IndexBuffer.Destroy();
    VertexBuffer.Destroy();
}

void scene::Destroy()
{
    Cubemap.Destroy(App->VulkanDevice);

    VertexBuffer.Destroy();
    IndexBuffer.Destroy();

    for(size_t i=0; i<Meshes.size(); i++)
    {
        Meshes[i].Destroy();
    }
    for(size_t i=0; i<Materials.size(); i++)
    {
        Materials[i].UniformBuffer.Destroy();
        vkFreeDescriptorSets(Device, MaterialDescriptorPool, 1, &Materials[i].DescriptorSet);
    }
    for(size_t i=0; i<Instances.size(); i++)
    {
        Instances[i].UniformBuffer.Destroy();
        vkFreeDescriptorSets(Device, InstanceDescriptorPool, 1, &Instances[i].DescriptorSet);
    }

    vkDestroyDescriptorSetLayout(Device, InstanceDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(Device, MaterialDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(Device, InstanceDescriptorPool, nullptr);
    vkDestroyDescriptorPool(Device, MaterialDescriptorPool, nullptr);

    Textures->Destroy();
    delete Textures;
}