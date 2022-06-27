#pragma once

#include <unordered_map>
#include <assert.h>
#include<vulkan/vulkan.h>
#include "TextureLoader.h"

#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \

template<typename T>
class vulkanResourceList
{
public:
    VkDevice &Device;
    std::unordered_map<std::string, T> Resources;
    std::unordered_map<int, T> ResourcesInt;
    vulkanResourceList(VkDevice &Device) : Device(Device){};

    T Get(std::string Name)
    {
        return Resources[Name];
    }

    T Get(int Flag)
    {
        return ResourcesInt[Flag];
    }

    T *GetPtr(std::string Name)
    {
        return &Resources[Name];
    }

    bool Present(std::string Name)
    {
        return Resources.find(Name) != Resources.end();
    }

    bool Present(int Flag)
    {
        return ResourcesInt.find(Flag) != ResourcesInt.end();
    }

    size_t Count() 
    {
        return Resources.size();
    }
    
};

class pipelineLayoutList : public vulkanResourceList<VkPipelineLayout>
{
public:
    pipelineLayoutList(VkDevice &Device) : vulkanResourceList(Device) {}

    void Destroy()
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
    
    VkPipelineLayout Add(std::string Name, VkPipelineLayout &PipelineLayout)
    {
        Resources[Name] = PipelineLayout;
        return PipelineLayout;
    }
};

class pipelineList : public vulkanResourceList<VkPipeline>
{
public:
    pipelineList(VkDevice &Device) : vulkanResourceList(Device) {}

    void Destroy()
    {
        for(auto &Pipeline : Resources)
        {
            vkDestroyPipeline(Device, Pipeline.second, nullptr);
        }
        for(auto &Pipeline : ResourcesInt)
        {
            vkDestroyPipeline(Device, Pipeline.second, nullptr);
        }
    }

    VkPipeline Add(int Flags, VkGraphicsPipelineCreateInfo &CreateInfo, VkPipelineCache &PipelineCache)
    {
        VkPipeline Pipeline;
        VK_CALL(vkCreateGraphicsPipelines(Device, PipelineCache, 1, &CreateInfo, nullptr, &Pipeline));
        ResourcesInt[Flags] = Pipeline;
        return Pipeline;
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

    void Destroy()
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
    VkDescriptorPool DescriptorPool;
    descriptorSetList(VkDevice &Device, VkDescriptorPool DescriptorPool) : vulkanResourceList(Device), DescriptorPool(DescriptorPool) {}

    void Destroy()
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
                Loader(Loader) {
        // Loader->LoadTexture2D("resources/models/sponza/dummy.png", VK_FORMAT_R8G8B8A8_UNORM, &DummyDiffuse);
        // Loader->LoadTexture2D("resources/models/sponza/dummy_specular.png", VK_FORMAT_R8G8B8A8_UNORM, &DummyNormal);
        // Loader->LoadTexture2D("resources/models/sponza/dummy_ddn.png", VK_FORMAT_R8G8B8A8_UNORM, &DummySpecular);    
        {
            uint8_t *Pixels;
            int Width, Height;
            Pixels = stbi_load("resources/models/sponza/dummy.png", &Width, &Height, NULL, 4);
            Loader->CreateTexture(
                Pixels,
                Width * Height * 4 * sizeof(float),
                VK_FORMAT_R8G8B8A8_UNORM,
                Width, Height,
                &DummyDiffuse,
                false
            );
            stbi_image_free(Pixels);
        }
        {
            uint8_t *Pixels;
            int Width, Height;
            Pixels = stbi_load("resources/models/sponza/dummy_specular.png", &Width, &Height, NULL, 4);
            Loader->CreateTexture(
                Pixels,
                Width * Height * 4 * sizeof(float),
                VK_FORMAT_R8G8B8A8_UNORM,
                Width, Height,
                &DummySpecular,
                false
            );
            stbi_image_free(Pixels);
        }
        {
            uint8_t *Pixels;
            int Width, Height;
            Pixels = stbi_load("resources/models/sponza/dummy_ddn.png", &Width, &Height, NULL, 4);
            Loader->CreateTexture(
                Pixels,
                Width * Height * 4 * sizeof(float),
                VK_FORMAT_R8G8B8A8_UNORM,
                Width, Height,
                &DummyNormal,
                false
            );
            stbi_image_free(Pixels);
        }

                                    
    }

    void Destroy()
    {
        for(auto &Texture : Resources)
        {
            Loader->DestroyTexture(Texture.second);
        }
        Loader->DestroyTexture(DummyDiffuse);
        Loader->DestroyTexture(DummyNormal);
        Loader->DestroyTexture(DummySpecular);
    }

    vulkanTexture AddTexture2D(std::string Name, std::string FileName, VkFormat Format)
    {
        vulkanTexture Texture;
        Loader->LoadTexture2D(FileName, Format, &Texture);
        Texture.Index = (uint32_t)Resources.size();
        Resources[Name] = Texture;
        return Texture;
    }

    vulkanTexture AddTexture2D(std::string Name, void* Data, size_t Size, uint32_t Width, uint32_t Height, VkFormat Format, bool GenerateMipmaps)
    {
        vulkanTexture Texture;
        Loader->CreateTexture(Data, Size, Format, Width, Height, &Texture, GenerateMipmaps);
        Texture.Index = (uint32_t)Resources.size();
        Resources[Name] = Texture;
        return Texture;
    }

    vulkanTexture DummyDiffuse;
    vulkanTexture DummyNormal;
    vulkanTexture DummySpecular;

};


struct descriptor
{
    VkShaderStageFlags Stage;
    enum type
    {
        Image,
        Uniform,
        accelerationStructure
    } Type;
    VkImageView ImageView;
    VkSampler Sampler;
    VkDescriptorImageInfo DescriptorImageInfo;
    VkDescriptorBufferInfo DescriptorBufferInfo;
    VkWriteDescriptorSetAccelerationStructureKHR DescriptorASInfo;
    VkDescriptorType DescriptorType;

    descriptor(VkShaderStageFlags Stage, VkImageView ImageView, VkSampler Sampler, VkImageLayout ImageLayout=VK_IMAGE_LAYOUT_GENERAL) :
                Stage(Stage), ImageView(ImageView), DescriptorImageInfo(DescriptorImageInfo), Sampler(Sampler)
    {
        Type = Image;
        DescriptorImageInfo = vulkanTools::BuildDescriptorImageInfo(Sampler, ImageView, ImageLayout);
        DescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    descriptor(VkShaderStageFlags Stage, VkDescriptorImageInfo Descriptor, VkDescriptorType DescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) :
                Stage(Stage), DescriptorImageInfo(Descriptor), DescriptorType(DescriptorType)
    {
        Type = Image;
    }
    
    descriptor(VkShaderStageFlags Stage, VkDescriptorBufferInfo DescriptorBufferInfo) : 
                Stage(Stage), DescriptorBufferInfo(DescriptorBufferInfo)
    {
        Type = Uniform;
        DescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }    

    descriptor(VkShaderStageFlags Stage, VkWriteDescriptorSetAccelerationStructureKHR DescriptorASInfo) : 
                Stage(Stage), DescriptorASInfo(DescriptorASInfo)
    {
        Type = accelerationStructure;
        DescriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }    
};



struct resources
{
    descriptorSetLayoutList *DescriptorSetLayouts;
    pipelineLayoutList *PipelineLayouts;
    pipelineList *Pipelines;
    descriptorSetList *DescriptorSets;
    textureList *Textures;
    void AddDescriptorSet(vulkanDevice *VulkanDevice, std::string Name, std::vector<descriptor> &Descriptors, VkDescriptorPool DescriptorPool, std::vector<VkDescriptorSetLayout> AdditionalDescriptorSetLayouts=std::vector<VkDescriptorSetLayout>());

    void Init(vulkanDevice *VulkanDevice, VkDescriptorPool DescriptorPool, textureLoader *TextureLoader)
    {
        PipelineLayouts = new pipelineLayoutList(VulkanDevice->Device);
        Pipelines = new pipelineList(VulkanDevice->Device);
        DescriptorSetLayouts = new descriptorSetLayoutList(VulkanDevice->Device);
        DescriptorSets = new descriptorSetList(VulkanDevice->Device, DescriptorPool);
        Textures = new textureList(VulkanDevice->Device, TextureLoader);
    }

    void Destroy()
    {
        DescriptorSets->Destroy();
        DescriptorSetLayouts->Destroy();
        PipelineLayouts->Destroy();
        Pipelines->Destroy();
        Textures->Destroy();
        delete PipelineLayouts;
        delete Pipelines;
        delete DescriptorSetLayouts;
        delete DescriptorSets;
        delete Textures;
    }
};