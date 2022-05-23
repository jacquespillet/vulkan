#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "Common/raypayload.glsl"
#include "Common/ubo.glsl"

layout(location = 0) rayPayloadInEXT rayPayload RayPayload;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };

// The miss shader is used to render a simple sky background gradient (if enabled)

void main()
{
	RayPayload.Color = vec3(1.0);
	RayPayload.Distance = -1.0;
}