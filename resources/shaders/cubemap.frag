#version 450

layout (set=1, binding = 0) uniform UBO 
{
    mat4 modelMatrix;
} ubo;
layout (set=1, binding = 1) uniform samplerCube cubemap;

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec4 outColor;

void main() 
{
    // outColor = vec4(inPosition, 1);
	outColor = texture(cubemap, normalize(inPosition));
}