#include "Scene.h"
#include "App.h"
#include "Resources.h"
#include "AssimpImporter.h"
#include "GLTFImporter.h"

scene::scene(vulkanApp *App) :
            App(App), Device(App->Device), 
            Queue(App->Queue), TextureLoader(App->TextureLoader)
{
    this->Textures = new textureList(Device, TextureLoader);
}


void scene::Load(std::string FileName, VkCommandBuffer CopyCommand)
{
    TextureLoader->LoadCubemap("resources/belfast_farmhouse_4k.hdr",VK_FORMAT_R32G32B32A32_SFLOAT, &Cubemap);

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
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            &Materials[i].MaterialData,
            sizeof(materialData),
            &Materials[i].UniformBuffer,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            CopyCommand,
            Queue
        );     
    }
    
    for (size_t i = 0; i < Instances.size(); i++)
    {
        vulkanTools::CreateAndFillBuffer(
            App->VulkanDevice,
            &Instances[i].InstanceData,
            sizeof(materialData),
            &Instances[i].UniformBuffer,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            CopyCommand,
            Queue
        );     
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



    //create a descriptor setlayout, descriptorSet, and pipelinelayout
    //create a graphicspipeline
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
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0 ));
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
}