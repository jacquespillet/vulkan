#include "Resources.h"

void resources::AddDescriptorSet(vulkanDevice *VulkanDevice, std::string Name, std::vector<descriptor> &Descriptors, VkDescriptorPool DescriptorPool)
{
    //TODO: Store together descriptor set layout and pipeline layout

    //Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings(Descriptors.size());
    for(uint32_t i=0; i<Descriptors.size(); i++)
    {
        SetLayoutBindings[i] = vulkanTools::DescriptorSetLayoutBinding(Descriptors[i].DescriptorType, Descriptors[i].Stage, i);
    }  
    VkDescriptorSetLayoutCreateInfo SetLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), static_cast<uint32_t>(SetLayoutBindings.size()));
    DescriptorSetLayouts->Add(Name, SetLayoutCreateInfo);
    
    //Create pipeline layout associated with it
    VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo();
    PipelineLayoutCreateInfo.pSetLayouts=DescriptorSetLayouts->GetPtr(Name);
    PipelineLayouts->Add(Name, PipelineLayoutCreateInfo);
    
    
    //Allocate descriptor set
    VkDescriptorSetAllocateInfo DescriptorAllocateInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, nullptr, 1);
    DescriptorAllocateInfo.pSetLayouts=DescriptorSetLayouts->GetPtr(Name);
    VkDescriptorSet TargetDescriptorSet = DescriptorSets->Add(Name, DescriptorAllocateInfo);

    //Write descriptor set
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets(Descriptors.size()); 
    for(uint32_t i=0; i<Descriptors.size(); i++)
    {
        descriptor::type Type = Descriptors[i].Type;
        if(Type == descriptor::Image) { WriteDescriptorSets[i] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[i].DescriptorType, i, &Descriptors[i].DescriptorImageInfo); }
        else 
        if(Type == descriptor::Uniform) { WriteDescriptorSets[i] = vulkanTools::BuildWriteDescriptorSet(TargetDescriptorSet, Descriptors[i].DescriptorType, i, &Descriptors[i].DescriptorBufferInfo);}
    }
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);        
}