#version 450

layout (set=1, binding = 0) uniform UBO 
{
    mat4 modelMatrix;
} ubo;
layout (set=1, binding = 1) uniform samplerCube Cubemap;
layout (set=1, binding = 2) uniform samplerCube IrradianceMap;
layout (set=1, binding = 3) uniform samplerCube PrefilteredEnv;

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec4 outColor;

void main() 
{
	outColor = texture(Cubemap, normalize(inPosition));
}