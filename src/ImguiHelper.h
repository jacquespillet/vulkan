#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include <glm/vec2.hpp>
#include <string>
#include <array>

class vulkanDevice;
class vulkanApp;

class ImGUI {
private:
	// Vulkan resources for rendering the UI
	VkSampler Sampler;
	buffer VertexBuffer;
	buffer IndexBuffer;
	int32_t VertexCount = 0;
	int32_t IndexCount = 0;
	VkDeviceMemory FontMemory = VK_NULL_HANDLE;
	VkImage FontImage = VK_NULL_HANDLE;
	VkImageView FontView = VK_NULL_HANDLE;
	VkPipelineCache PipelineCache;
	VkPipelineLayout PipelineLayout;
	VkPipeline Pipeline;
	VkDescriptorPool DescriptorPool;
	VkDescriptorSetLayout DescriptorSetLayout;
	VkDescriptorSet DescriptorSet;
	vulkanDevice *Device;
	vulkanApp *App;
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
public:
	// UI params are set via push constants
	struct PushConstBlock {
		glm::vec2 Scale;
		glm::vec2 Translate;
	} PushConstBlock;

	ImGUI(vulkanApp *App);

	~ImGUI();

	// Initialize styles, keys, etc.
	void Init(float Width, float Height);

	// Initialize all Vulkan resources used by the ui
	void InitResources(VkRenderPass RenderPass, VkQueue CopyQueue);

	// Update vertex and index buffer containing the imGui elements when required
	void UpdateBuffers();

	// Draw current imGui frame into a command buffer
	void DrawFrame(VkCommandBuffer CommandBuffer);

	void InitStyle();
};