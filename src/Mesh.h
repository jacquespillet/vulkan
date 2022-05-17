#pragma once

struct meshDescriptor
{
    uint32_t VertexCount;
    uint32_t IndexBase;
    uint32_t IndexCount;
};

struct meshBufferInfo
{
    VkBuffer Buffer = VK_NULL_HANDLE;
    VkDeviceMemory DeviceMemory = VK_NULL_HANDLE;
    size_t Size=0;
};

struct meshBuffer
{
    meshBufferInfo Vertices;
    meshBufferInfo Indices;
    uint32_t IndexCount;
    glm::vec3 Dimension;
};