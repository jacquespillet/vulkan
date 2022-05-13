#pragma once

#include <unordered_map>

template<typename T>
class vulkanResourceList
{
public:
    VkDevice &Device;
    std::unordered_map<std::string, T> Resources;
    vulkanResourceList(VkDevice &Device) : Device(Device){};

    T Get(std::string Name)
    {
        return Resources[Name];
    }

    T *GetPtr(std::string Name)
    {
        return &Resources[Name];
    }

    bool Present(std::string Name)
    {
        return Resources.find(Name) != Resources.end();
    }
};

class pipelineLayoutList : public vulkanResourceList<VkPipelineLayout>
{
public:
    pipelineLayoutList(VkDevice &Device) : vulkanResourceList(Device) {}

    ~pipelineLayoutList()
    {
        for(auto &PipelineLayout : Resources)
        {
            vkDestroyPipelineLayout(Device, PipelineLayout.second, nullptr);
        }
    }

    VkPipelineLayout Add(std::string Name, VkPipelineLayoutCreateInfo &CreateInfo)
    {
        VkPipelineLayout PipelineLayout;
        VK_CALL(vkCreatePipelineLayout(Device, &CreateInfo, nullptr, &PipelineLayout));
        Resources[Name] = PipelineLayout;
        return PipelineLayout;
    }
};

class pipelineList : public vulkanResourceList<VkPipeline>
{
public:
    pipelineList(VkDevice &Device) : vulkanResourceList(Device) {}

    ~pipelineList()
    {
        for(auto &Pipeline : Resources)
        {
            vkDestroyPipeline(Device, Pipeline.second, nullptr);
        }
    }

    VkPipeline Add(std::string Name, VkGraphicsPipelineCreateInfo &CreateInfo, VkPipelineCache &PipelineCache)
    {
        VkPipeline Pipeline;
        VK_CALL(vkCreateGraphicsPipelines(Device, PipelineCache, 1, &CreateInfo, nullptr, &Pipeline));
        Resources[Name] = Pipeline;
        return Pipeline;
    }
};

class descriptorSetLayoutList : public vulkanResourceList<VkDescriptorSetLayout>
{
public:
    descriptorSetLayoutList(VkDevice &Device) : vulkanResourceList(Device) {}

    ~descriptorSetLayoutList()
    {
        for(auto &DescriptorSetLayout : Resources)
        {
            vkDestroyDescriptorSetLayout(Device, DescriptorSetLayout.second, nullptr);
        }
    }

    VkDescriptorSetLayout Add(std::string Name, VkDescriptorSetLayoutCreateInfo &CreateInfo)
    {
        VkDescriptorSetLayout DescriptorSetLayout;
        VK_CALL(vkCreateDescriptorSetLayout(Device, &CreateInfo, nullptr, &DescriptorSetLayout));
        Resources[Name] = DescriptorSetLayout;
        return DescriptorSetLayout;
    }
};

class descriptorSetList : public vulkanResourceList<VkDescriptorSet>
{
public:
    VkDescriptorPool &DescriptorPool;
    descriptorSetList(VkDevice &Device, VkDescriptorPool &DescriptorPool) : vulkanResourceList(Device), DescriptorPool(DescriptorPool) {}

    ~descriptorSetList()
    {
        for(auto &DescriptorSet : Resources)
        {
            vkFreeDescriptorSets(Device, DescriptorPool,1, &DescriptorSet.second);
        }
    }

    VkDescriptorSet Add(std::string Name, VkDescriptorSetAllocateInfo &AllocInfo)
    {
        VkDescriptorSet DescriptorSet;
        VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &DescriptorSet));
        Resources[Name] = DescriptorSet;
        return DescriptorSet;
    }
};

class textureList : public vulkanResourceList<vulkanTexture>
{
public:
    textureLoader *Loader;

    textureList(VkDevice &Device, textureLoader *Loader) : 
                vulkanResourceList(Device),
                Loader(Loader) {}

    ~textureList()
    {
        for(auto &Texture : Resources)
        {
            Loader->DestroyTexture(Texture.second);
        }
    }

    vulkanTexture AddTexture2D(std::string Name, std::string FileName, VkFormat Format)
    {
        vulkanTexture Texture;
        Loader->LoadTexture2D(FileName, Format, &Texture);
        Resources[Name] = Texture;
        return Texture;
    }

    vulkanTexture AddTextureArray(std::string Name, std::string FileName, VkFormat Format)
    {
        vulkanTexture Texture;
        // Loader->LoadTextureArray(FileName, Format, &Texture);
        Resources[Name] = Texture;
        return Texture;
    }

    vulkanTexture AddCubemap(std::string Name, std::string FileName, VkFormat Format)
    {
        vulkanTexture Texture;
        // Loader->LoadCubemap(FileName, Format, &Texture);
        Resources[Name] = Texture;
        return Texture;
    }
};