#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct vertex
{
    glm::vec3 Position;
    glm::vec2 UV;
    glm::vec3 Color;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};


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