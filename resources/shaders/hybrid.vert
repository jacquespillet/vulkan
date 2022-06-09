#version 460

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;

#include "Common/SceneUBO.glsl"
layout (set=0, binding = 0) uniform UBO 
{
	sceneUbo Data;	
} SceneUbo;


layout (set=2, binding = 0) uniform instance 
{
	mat4 Model;
	mat4 Normal;
	float Selected;
	vec3 Padding;
} InstanceUBO;

layout(location=0) out vec3 FragPosition;
layout(location=1) out vec3 FragNormal;
layout(location=2) out vec2 FragUv;
layout(location=3) out mat3 TBN;


void main() 
{
	mat4 ModelViewProjection = SceneUbo.Data.Projection * SceneUbo.Data.View * InstanceUBO.Model;
	gl_Position = ModelViewProjection * vec4(inPos.xyz, 1.0);
	FragPosition = (InstanceUBO.Model * vec4(inPos.xyz, 1.0)).xyz;
	FragUv = vec2(inPos.w, inNormal.w);


	FragNormal = normalize((InstanceUBO.Normal * vec4(inNormal.xyz, 0.0)).xyz);
// #ifdef HAS_TANGENT_VEC3
    vec3 FragTangent = normalize((InstanceUBO.Normal * vec4(inTangent.xyz, 0.0)).xyz);  
    // FragTangent = normalize(FragTangent - dot(FragTangent, FragNormal) * FragNormal);  

    vec3 FragBitangent = normalize(cross(FragNormal, FragTangent.xyz) * inTangent.w); 
	TBN = mat3(FragTangent, FragBitangent, FragNormal);    
// #endif
}
