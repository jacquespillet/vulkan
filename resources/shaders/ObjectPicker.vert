#version 450

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;

#include "Common/SceneUBO.glsl"
layout (set=0, binding = 0) uniform UBO 
{
	sceneUbo Data;	
} SceneUbo;

layout (set=1, binding = 0) uniform instance 
{
	mat4 Model;
	mat4 Normal;
	float Selected;
	float InstanceID;
    vec2 Padding;
} InstanceUBO;

void main() 
{
	mat4 ModelViewProjection = SceneUbo.Data.Projection * SceneUbo.Data.View * InstanceUBO.Model;
	gl_Position = ModelViewProjection * vec4(inPos.xyz, 1.0);
}
