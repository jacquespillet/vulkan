#version 460

#define PI 3.14159265359

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable


layout (binding = 0) uniform sampler2D samplerPositionDepth;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform accelerationStructureEXT topLevelAS;


#include "Common/SceneUBO.glsl"
layout (set=1,binding = 0) uniform UBO 
{
    sceneUbo Data;
} SceneUbo;

layout (location = 0) in vec2 inUV;

layout (location = 0) out float outFragColor;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() 
{	  
	vec3 FragPosition = texture(samplerPositionDepth, inUV).rgb;
	vec3 Normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
	float Shadow=1.0f;

#if 0
	vec3 L = normalize(-SceneUbo.Data.LightDirection);
#else
        vec3 light_dir= normalize(-SceneUbo.Data.LightDirection);;
        vec3 light_tangent   = normalize(cross(light_dir, vec3(0.0f, 1.0f, 0.0f)));
        vec3 light_bitangent = normalize(cross(light_tangent, light_dir));

        // calculate disk point
        float point_radius = 0.5 * rand(inUV.yx);
        float point_angle  = rand(inUV.xy) * 2.0f * PI;
        vec2  disk_point   = vec2(point_radius * cos(point_angle), point_radius * sin(point_angle));    
        vec3 L = normalize(light_dir + disk_point.x * light_tangent + disk_point.y * light_bitangent);
#endif
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery, topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, FragPosition, 0.01, L, 1000.0);
	// Start the ray traversal, rayQueryProceedEXT returns false if the traversal is complete
	while (rayQueryProceedEXT(rayQuery)) { }

	// If the intersection has hit a triangle, the fragment is shadowed
	if (rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT ) {
		Shadow=0.1f;
	}


	outFragColor = Shadow;
}

