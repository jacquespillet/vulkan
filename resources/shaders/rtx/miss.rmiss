#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../Common/raypayload.glsl"
#include "../Common/ubo.glsl"

layout(location = 0) rayPayloadInEXT rayPayload RayPayload;
layout(location = 1) rayPayloadInEXT bool isShadowed;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 6, set = 0) uniform samplerCube IrradianceMap;
layout(binding = 7, set = 0) uniform samplerCube Cubemap;

#include "../Common/SceneUBO.glsl"
layout (set=0, binding =8) uniform UBO 
{
    sceneUbo Data;
} SceneUbo;
// The miss shader is used to render a simple sky background gradient (if enabled)

void main()
{
	isShadowed=false;
	
	if(SceneUbo.Data.BackgroundType ==0)
	{
		vec3 Direction = normalize(gl_WorldRayDirectionEXT);
		Direction.y *=-1;
		
		if(RayPayload.Depth == 0) 
		{
			RayPayload.Emission = texture(Cubemap, Direction).rgb;
		}
		else
		{
			RayPayload.Emission = texture(IrradianceMap, Direction).rgb;
		}
	}
	else
	{
		RayPayload.Emission = SceneUbo.Data.BackgroundColor;
	}
	RayPayload.Emission *= SceneUbo.Data.BackgroundIntensity;

	RayPayload.Distance = -1.0;
}