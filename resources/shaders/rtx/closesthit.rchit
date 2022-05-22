#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#include "Common/geometryTypes.glsl"

struct ObjBuffers
{
	uint64_t vertices;
	uint64_t indices;
	uint64_t materials;
};

#include "Common/material.glsl"

layout(buffer_reference, scalar) buffer Vertices {vec4 v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {Material m[]; }; // Array of all materials on an object

#include "Common/random.glsl"
#include "Common/raypayload.glsl"
#include "Common/ubo.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 4, set = 0) buffer _scene_desc { ObjBuffers i[]; } scene_desc;
layout(binding = 5, set = 0) uniform sampler2D[] textures;

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec3 attribs;

#include "Common/geometry.glsl"

void main()
{
	triangle Triangle = UnpackTriangle(gl_PrimitiveID, ubo.vertexSize);

	ObjBuffers ObjResource = scene_desc.i[gl_InstanceCustomIndexEXT];
	Materials materials = Materials(ObjResource.materials);
	Material mat = materials.m[Triangle.materialIndex];
	vec4 TextureColor = texture(textures[0], Triangle.uv);

	rayPayload.color=TextureColor.rgb;
}
