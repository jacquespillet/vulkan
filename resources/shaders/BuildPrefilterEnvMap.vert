#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;


layout (binding = 0) uniform UBO 
{
	mat4 viewProjectionMatrix;
} ubo;

layout (location = 0) out vec3 outUV;
layout (location = 1) out vec3 outPosition;

void main() 
{
	outUV = vec3(inPos.w, inNormal.w, inNormal.z);
	outPosition = inPos.xyz;
	
	gl_Position = ubo.viewProjectionMatrix * vec4(inPos.xyz, 1.0);
}
