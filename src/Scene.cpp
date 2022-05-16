#include "Scene.h"
#include "App.h"
#include "Resources.h"
#include "AssimpImporter.h"

scene::scene(vulkanApp *App) :
            App(App), Device(App->Device), 
            Queue(App->Queue), TextureLoader(App->TextureLoader)
{
    this->Textures = new textureList(Device, TextureLoader);
}


void scene::Load(std::string FileName, VkCommandBuffer CopyCommand)
{
    std::vector<vertex> GVertices;
    std::vector<uint32_t> GIndices;
    assimpImporter::Load(FileName, Meshes, Materials,GVertices, GIndices, Textures);    

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

void scene::CreateDescriptorSets()
{
    //Create descriptor pool
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
    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &DescriptorPool));


    //Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
    SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ));
    SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ));
    SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ));
    SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ));
    VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
    VK_CALL(vkCreateDescriptorSetLayout(Device, &DescriptorLayoutCreateInfo, nullptr, &DescriptorSetLayout));

    //Write descriptor sets
    for(uint32_t i=0; i<Materials.size(); i++)
    {
        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
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