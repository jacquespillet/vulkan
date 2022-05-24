#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/raypayload.glsl"
#include "Common/ubo.glsl"

layout(location = 0) rayPayloadInEXT rayPayload RayPayload;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 6, set = 0) uniform samplerCube IrradianceMap;
layout(binding = 7, set = 0) uniform samplerCube Cubemap;
// The miss shader is used to render a simple sky background gradient (if enabled)

void main()
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


	RayPayload.Distance = -1.0;
}