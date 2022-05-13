#include "Shader.h"
#include <string>
#include <assert.h>


#define VK_CALL(f)\
{\
    VkResult Res = (f); \
    if(Res != VK_SUCCESS) \
    { \
        assert(0); \
    } \
} \


VkShaderModule LoadShaderModule(std::string FileName, VkDevice Device, VkShaderStageFlagBits Stage)
{
    size_t Size;

    FILE *FP = fopen(FileName.c_str(), "rb");
    assert(FP);

    fseek(FP, 0L, SEEK_END);
    Size = ftell(FP);
    fseek(FP, 0L, SEEK_SET);

    char *ShaderCode = new char[Size];
    size_t RetVal = fread(ShaderCode, Size, 1, FP);
    assert(RetVal==1);
    assert(Size>0);

    fclose(FP);

    VkShaderModule ShaderModule;
    VkShaderModuleCreateInfo ModuleCreateInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ModuleCreateInfo.pNext=nullptr;
    ModuleCreateInfo.codeSize=Size;
    ModuleCreateInfo.pCode = (uint32_t*)ShaderCode;
    ModuleCreateInfo.flags = 0;

    VK_CALL(vkCreateShaderModule(Device, &ModuleCreateInfo, nullptr, &ShaderModule));

    delete[] ShaderCode;

    return ShaderModule;

}

VkPipelineShaderStageCreateInfo LoadShader(VkDevice Device, std::string FileName, VkShaderStageFlagBits Stage)
{
    VkPipelineShaderStageCreateInfo Result {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    Result.stage = Stage;
    Result.module = LoadShaderModule(FileName, Device, Stage);
    Result.pName = "main";
    assert(Result.module != nullptr);
    return Result;
}