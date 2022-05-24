#version 450


layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;


#define SCENE_UBO_SET_ID 0
#define SCENE_UBO_BINDING 0
#include "SceneUBO.glsl"

layout (set=1, binding = 0) uniform CubemapUBO 
{
    mat4 modelMatrix;
} cubemapUBO;

layout (location = 0) out vec3 outPosition;
void main() 
{
	vec4 pos = vec4(inPos.xyz, 1);
	pos.y *=-1;
	mat4 rotView = mat4(mat3(SceneUbo.View));

	gl_Position = SceneUbo.Projection * rotView * cubemapUBO.modelMatrix * pos;
	
	outPosition = inPos.xyz;
}
