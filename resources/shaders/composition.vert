#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;


layout (set=2, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
} ubo;

layout (location = 0) out vec2 outUV;

void main() 
{
	outUV = vec2(inPos.w, inNormal.w);
	gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
}
