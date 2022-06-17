#include "Framebuffer.h"
#include "Device.h"
#include "Tools.h"

void framebuffer::BuildBuffers(vulkanDevice *VulkanDevice, VkCommandBuffer LayoutCommand)
{
    uint32_t ColorAttachmentCount = (uint32_t) _Attachments.size();
    uint32_t TotalAttachmentCount = HasDepth ? ColorAttachmentCount + 1 : ColorAttachmentCount;

    for(uint32_t i=0; i<ColorAttachmentCount; i++)
    {
        vulkanTools::CreateAttachment(VulkanDevice,
                                        AttachmentFormats[i],
                                        ImageUsage,
                                        &_Attachments[i],
                                        LayoutCommand,
                                        Width,
                                        Height);
        
    }
    
    
    
    if(HasDepth)
    {
        VkFormat AttDepthFormat;
        VkBool32 ValidDepthFormat = vulkanTools::GetSupportedDepthFormat(VulkanDevice->PhysicalDevice, &AttDepthFormat);
        assert(ValidDepthFormat);
        vulkanTools::CreateAttachment(VulkanDevice,
                                        AttDepthFormat,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        &Depth,
                                        LayoutCommand,
                                        Width,
                                        Height);    
    }
    //Attachment descriptions
    //3 colour buffers + depth
    std::vector<VkAttachmentDescription> AttachmentDescriptions(TotalAttachmentCount);
    std::vector<VkAttachmentReference> ColorReferences;
    VkAttachmentReference DepthReference;
    for(uint32_t i=0; i<AttachmentDescriptions.size(); i++)
    {
        
        AttachmentDescriptions[i].samples = VK_SAMPLE_COUNT_1_BIT;
        AttachmentDescriptions[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        AttachmentDescriptions[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        AttachmentDescriptions[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        
        if(i<ColorAttachmentCount)
        {
            VkImageLayout FinalLayout = AttachmentLayouts[i];
            if(ImageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            {
                FinalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            if(ImageUsage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            {
                FinalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }


            AttachmentDescriptions[i].loadOp = LoadOps[i];
            AttachmentDescriptions[i].format = _Attachments[i].Format;
            AttachmentDescriptions[i].finalLayout = FinalLayout;
            ColorReferences.push_back({i, FinalLayout});
        }
        else if(HasDepth)//Depth
        {
            AttachmentDescriptions[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            AttachmentDescriptions[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            AttachmentDescriptions[i].format = Depth.Format;
            DepthReference = {i, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        }
    }

    //Subpass
    VkSubpassDescription Subpass = {};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.pColorAttachments = ColorReferences.data();
    Subpass.colorAttachmentCount = (uint32_t)ColorReferences.size();
    if(HasDepth) Subpass.pDepthStencilAttachment = &DepthReference;

    //Subpass dependencies to transition the attachments from memory read to write, and then back from write to read
    std::array<VkSubpassDependency, 2> Dependencies;

    Dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependencies[0].dstSubpass=0;
    Dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    Dependencies[1].srcSubpass=0;
    Dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    Dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    Dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    Dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo RenderPassInfo {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.attachmentCount = static_cast<uint32_t>(AttachmentDescriptions.size());
    RenderPassInfo.pAttachments = AttachmentDescriptions.data();
    RenderPassInfo.subpassCount=1;
    RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount=2;
    RenderPassInfo.pDependencies = Dependencies.data();
    VK_CALL(vkCreateRenderPass(VulkanDevice->Device, &RenderPassInfo, nullptr, &RenderPass));

    //Create framebuffer objects
    std::vector<VkImageView> ImageViews(TotalAttachmentCount);
    for(uint32_t i=0; i<ColorAttachmentCount; i++)
    {
        ImageViews[i] = _Attachments[i].ImageView;
    }
    if(HasDepth) ImageViews[ColorAttachmentCount] = Depth.ImageView;

    VkFramebufferCreateInfo FramebufferCreateInfo  = vulkanTools::BuildFramebufferCreateInfo();
    FramebufferCreateInfo.renderPass = RenderPass;
    FramebufferCreateInfo.pAttachments = ImageViews.data();
    FramebufferCreateInfo.attachmentCount = static_cast<uint32_t>(ImageViews.size());
    FramebufferCreateInfo.width = Width;
    FramebufferCreateInfo.height = Height;
    FramebufferCreateInfo.layers=1;
    VK_CALL(vkCreateFramebuffer(VulkanDevice->Device, &FramebufferCreateInfo, nullptr, &Framebuffer));


    VkSamplerCreateInfo SamplerCreateInfo = vulkanTools::BuildSamplerCreateInfo();
    SamplerCreateInfo.magFilter=VK_FILTER_LINEAR;
    SamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    SamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerCreateInfo.mipLodBias = 0.0f;
    SamplerCreateInfo.maxAnisotropy=0;
    SamplerCreateInfo.minLod=0.0f;
    SamplerCreateInfo.maxLod=1.0f;
    SamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    SamplerCreateInfo.compareEnable=VK_TRUE;
    VK_CALL(vkCreateSampler(VulkanDevice->Device, &SamplerCreateInfo, nullptr, &Sampler));        
}