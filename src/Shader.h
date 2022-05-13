#pragma once

#include <vulkan/vulkan.h>
#include <string>

VkShaderModule LoadShaderModule(std::string FileName, VkDevice Device, VkShaderStageFlagBits Stage);

VkPipelineShaderStageCreateInfo LoadShader(VkDevice Device, std::string FileName, VkShaderStageFlagBits Stage);