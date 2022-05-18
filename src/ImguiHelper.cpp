#include "ImguiHelper.h"
#include "Device.h"
#include "Shader.h"
#include "App.h"

#include "imgui.h"

ImGUI::ImGUI(vulkanApp *App) : App(App)
{
    Device = App->VulkanDevice;
    ImGui::CreateContext();
};

ImGUI::~ImGUI()
{
    ImGui::DestroyContext();
    vkDestroyShaderModule(Device->Device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(Device->Device, shaderStages[1].module, nullptr);
    // Release all Vulkan resources required for rendering imGui
    VertexBuffer.Destroy();
    IndexBuffer.Destroy();
    vkDestroyImage(Device->Device, FontImage, nullptr);
    vkDestroyImageView(Device->Device, FontView, nullptr);
    vkFreeMemory(Device->Device, FontMemory, nullptr);
    vkDestroySampler(Device->Device, Sampler, nullptr);
    vkDestroyPipelineCache(Device->Device, PipelineCache, nullptr);
    vkDestroyPipeline(Device->Device, Pipeline, nullptr);
    vkDestroyPipelineLayout(Device->Device, PipelineLayout, nullptr);
    vkDestroyDescriptorPool(Device->Device, DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(Device->Device, DescriptorSetLayout, nullptr);
}

void ImGUI::Init(float Width, float Height)
{
    // Color scheme
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    // Dimensions
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(Width, Height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
}

void ImGUI::InitResources(VkRenderPass RenderPass, VkQueue CopyQueue)
{
    ImGuiIO& io = ImGui::GetIO();

    // Create font texture
    unsigned char* fontData;
    int texWidth, texHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    VkDeviceSize uploadSize = texWidth*texHeight * 4 * sizeof(char);

    // Create target image for copy
    VkImageCreateInfo imageInfo = vulkanTools::BuildImageCreateInfo();
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent.width = texWidth;
    imageInfo.extent.height = texHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CALL(vkCreateImage(Device->Device, &imageInfo, nullptr, &FontImage));
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(Device->Device, FontImage, &memReqs);
    VkMemoryAllocateInfo memAllocInfo = vulkanTools::BuildMemoryAllocateInfo();
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex =  Device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CALL(vkAllocateMemory(Device->Device, &memAllocInfo, nullptr, &FontMemory));
    VK_CALL(vkBindImageMemory(Device->Device, FontImage, FontMemory, 0));

    // Image view
    VkImageViewCreateInfo viewInfo = vulkanTools::BuildImageViewCreateInfo();
    viewInfo.image = FontImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CALL(vkCreateImageView(Device->Device, &viewInfo, nullptr, &FontView));

    // Staging buffers for font data upload
    buffer stagingBuffer;

    VK_CALL(vulkanTools::CreateBuffer(
        Device,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,
        uploadSize));

    stagingBuffer.Map();
    memcpy(stagingBuffer.Mapped, fontData, uploadSize);
    stagingBuffer.Unmap();

    // Copy buffer data to font image
    VkCommandBuffer copyCmd = vulkanTools::CreateCommandBuffer(Device->Device, App->CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Prepare for transfer
    vulkanTools::TransitionImageLayout(
        copyCmd,
        FontImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = texWidth;
    bufferCopyRegion.imageExtent.height = texHeight;
    bufferCopyRegion.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
        copyCmd,
        stagingBuffer.Buffer,
        FontImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &bufferCopyRegion
    );

    // Prepare for shader read
    vulkanTools::TransitionImageLayout(
        copyCmd,
        FontImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vulkanTools::FlushCommandBuffer(Device->Device, App->CommandPool, copyCmd, CopyQueue, true);
    stagingBuffer.Destroy();

    // Font texture Sampler
    VkSamplerCreateInfo samplerInfo = vulkanTools::BuildSamplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CALL(vkCreateSampler(Device->Device, &samplerInfo, nullptr, &Sampler));

    // Descriptor pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(poolSizes, 2);
    VK_CALL(vkCreateDescriptorPool(Device->Device, &descriptorPoolInfo, nullptr, &DescriptorPool));

    // Descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorLayout = vulkanTools::BuildDescriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CALL(vkCreateDescriptorSetLayout(Device->Device, &descriptorLayout, nullptr, &DescriptorSetLayout));

    // Descriptor set
    VkDescriptorSetAllocateInfo allocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &DescriptorSetLayout, 1);
    VK_CALL(vkAllocateDescriptorSets(Device->Device, &allocInfo, &DescriptorSet));
    VkDescriptorImageInfo fontDescriptor = vulkanTools::BuildDescriptorImageInfo(
        Sampler,
        FontView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vulkanTools::BuildWriteDescriptorSet(DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
    };
    vkUpdateDescriptorSets(Device->Device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // Pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CALL(vkCreatePipelineCache(Device->Device, &pipelineCacheCreateInfo, nullptr, &PipelineCache));

    // Pipeline layout
    // Push constants for UI rendering parameters
    VkPushConstantRange pushConstantRange = vulkanTools::BuildPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vulkanTools::BuildPipelineLayoutCreateInfo(&DescriptorSetLayout, 1);
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    VK_CALL(vkCreatePipelineLayout(Device->Device, &pipelineLayoutCreateInfo, nullptr, &PipelineLayout));

    // Setup graphics pipeline for UI rendering
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        vulkanTools::BuildPipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
        vulkanTools::BuildPipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    // Enable blending
    VkPipelineColorBlendAttachmentState blendAttachmentState{};
    blendAttachmentState.blendEnable = VK_TRUE;
    blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vulkanTools::BuildPipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vulkanTools::BuildPipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
        vulkanTools::BuildPipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
        vulkanTools::BuildPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        vulkanTools::BuildPipelineDynamicStateCreateInfo(dynamicStateEnables);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = vulkanTools::BuildGraphicsPipelineCreateInfo(PipelineLayout, RenderPass);

    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();

    // Vertex bindings an attributes based on ImGui vertex definition
    std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
        vulkanTools::BuildVertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        vulkanTools::BuildVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
        vulkanTools::BuildVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
        vulkanTools::BuildVertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
    };
    VkPipelineVertexInputStateCreateInfo vertexInputState = vulkanTools::BuildPipelineVertexInputStateCreateInfo();
    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

    pipelineCreateInfo.pVertexInputState = &vertexInputState;

    shaderStages[0] = LoadShader(Device->Device, "resources/shaders/imgui/ui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(Device->Device, "resources/shaders/imgui/ui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VK_CALL(vkCreateGraphicsPipelines(Device->Device, PipelineCache, 1, &pipelineCreateInfo, nullptr, &Pipeline));
}

// Starts a new imGui frame and sets up windows and ui elements
void ImGUI::NewFrame(vulkanApp *_App, bool updateFrameGraph)
{
        ImGui::NewFrame();

		// Init imGui windows and elements

		ImVec4 clear_color = ImColor(114, 144, 154);
		static float f = 0.0f;
		ImGui::TextUnformatted("HELLOE");
		ImGui::TextUnformatted("HOWIUGF");

		ImGui::Text("Camera");

		ImGui::SetNextWindowSize(ImVec2(200, 200));
		ImGui::Begin("Example settings");
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(650, 20));
		ImGui::ShowDemoWindow();

		// Render to generate draw buffers
		ImGui::Render();
}

void ImGUI::UpdateBuffers()
{
    ImDrawData* imDrawData = ImGui::GetDrawData();

    // Note: Alignment is done inside buffer creation
    VkDeviceSize VertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize IndexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((VertexBufferSize == 0) || (IndexBufferSize == 0)) {
        return;
    }

    // Update buffers only if vertex or index count has been changed compared to current buffer size

    // Vertex buffer
    if ((VertexBuffer.Buffer == VK_NULL_HANDLE) || (VertexCount != imDrawData->TotalVtxCount)) {
        VertexBuffer.Unmap();
        VertexBuffer.Destroy();
        VK_CALL(vulkanTools::CreateBuffer(Device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &VertexBuffer, VertexBufferSize));
        VertexCount = imDrawData->TotalVtxCount;
        VertexBuffer.Map();
    }

    // Index buffer
    if ((IndexBuffer.Buffer == VK_NULL_HANDLE) || (IndexCount < imDrawData->TotalIdxCount)) {
        IndexBuffer.Unmap();
        IndexBuffer.Destroy();
        VK_CALL(vulkanTools::CreateBuffer(Device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &IndexBuffer, IndexBufferSize));
        IndexCount = imDrawData->TotalIdxCount;
        IndexBuffer.Map();
    }

    // Upload data
    ImDrawVert* vtxDst = (ImDrawVert*)VertexBuffer.Mapped;
    ImDrawIdx* idxDst = (ImDrawIdx*)IndexBuffer.Mapped;

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to make writes visible to GPU
    VertexBuffer.Flush();
    IndexBuffer.Flush();
}

void ImGUI::DrawFrame(VkCommandBuffer CommandBuffer)
{
    ImGuiIO& io = ImGui::GetIO();

    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet, 0, nullptr);
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

    VkViewport viewport = vulkanTools::BuildViewport(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);
    vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

    // UI scale and translate via push constants
    PushConstBlock.Scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    PushConstBlock.Translate = glm::vec2(-1.0f);
    vkCmdPushConstants(CommandBuffer, PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &PushConstBlock);

    // Render commands
    ImDrawData* imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    if (imDrawData->CmdListsCount > 0) {

        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer.Buffer, offsets);
        vkCmdBindIndexBuffer(CommandBuffer, IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT16);

        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
        {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(CommandBuffer, 0, 1, &scissorRect);
                vkCmdDrawIndexed(CommandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }
}