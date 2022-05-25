#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/raypayload.glsl"
#include "Common/ubo.glsl"

layout(location = 0) rayPayloadInEXT rayPayload RayPayload;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 6, set = 0) uniform samplerCube IrradianceMap;
layout(binding = 7, set = 0) uniform samplerCube Cubemap;

#define SCENE_UBO_SET_ID 0
#define SCENE_UBO_BINDING 8
#include "../SceneUBO.glsl"

// The miss shader is used to render a simple sky background gradient (if enabled)

void main()
{
	if(SceneUbo.BackgroundType ==0)
	{
		vec3 Direction = normalize(gl_WorldRayDirectionEXT);
		Direction.y *=-1;
		
		if(RayPayload.Depth == 0) 
		{
			RayPayload.Color = texture(Cubemap, Direction).rgb;
		}
		else
		{
			RayPayload.Color = texture(IrradianceMap, Direction).rgb;
		}
	}
	else
	{
		RayPayload.Color = SceneUbo.BackgroundColor * SceneUbo.BackgroundIntensity;
	}

	RayPayload.Distance = -1.0;
}