#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#include "../Common/geometryTypes.glsl"

struct ObjBuffers
{
	uint64_t vertices;
	uint64_t indices;
	uint64_t materials;
};

#include "../Common/material.glsl"

layout(buffer_reference, scalar) buffer Vertices {vec4 v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {uint i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {material m[]; }; // Array of all materials on an object

#include "../Common/random.glsl"
#include "../Common/raypayload.glsl"
#include "../Common/ubo.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) uniform UniformData { Ubo ubo; };
layout(binding = 4, set = 0) buffer _scene_desc { ObjBuffers i[]; } scene_desc;
layout(binding = 5, set = 0) uniform sampler2D[] textures;
layout(binding = 9, set = 0) buffer transformMatrices { mat4 Buffer[]; } TransformMatrices;

layout(location = 0) rayPayloadInEXT rayPayload RayPayload;
hitAttributeEXT vec3 attribs;

#include "../Common/geometry.glsl"
#include "../Common/Common.glsl"

void main()
{
	triangle Triangle = UnpackTriangle(gl_PrimitiveID, ubo.vertexSize);

	ObjBuffers ObjResource = scene_desc.i[gl_InstanceCustomIndexEXT];
	Materials materials = Materials(ObjResource.materials);
	material Material = materials.m[Triangle.materialIndex];

	//Base Color
	vec3 BaseColor = Material.BaseColor;
	if(Material.BaseColorTextureID >=0 && Material.UseBaseColorMap>0)
	{
		vec4 TextureColor = texture(textures[Material.BaseColorTextureID], Triangle.uv);
		TextureColor.rgb *= pow(TextureColor.rgb, vec3(2.2));
		BaseColor *= TextureColor.rgb;
	}

	// Normal
	vec3 Normal = Triangle.normal;
	if (Material.NormalMapTextureID >=0 && Material.UseNormalMap > 0) {
		if (length(Triangle.tangent) != 0) {
			vec3 T = normalize(Triangle.tangent.xyz);
			vec3 B = cross(Triangle.normal, Triangle.tangent.xyz) * Triangle.tangent.w;
			vec3 N = normalize(Triangle.normal);
			mat3 TBN = mat3(T, B, N);
			Normal = TBN * normalize(texture(textures[Material.NormalMapTextureID], Triangle.uv).rgb * 2.0 - vec3(1.0));
		}
	}	

	// Roughness / metallic
	RayPayload.Roughness = Material.Roughness;
	if(Material.MetallicRoughnessTextureID >=0 && Material.UseMetallicRoughnessMap>0)
	{
		vec2 RoughnessMetallic = texture(textures[Material.MetallicRoughnessTextureID], Triangle.uv).rg;
		RayPayload.Roughness *= RoughnessMetallic.r;
	}
	
	
	RayPayload.DoScatter=true;
	
	// Emission
	RayPayload.Emission = Material.Emission * Material.EmissiveStrength;
	if(Material.EmissionMapTextureID >=0 && Material.UseEmissionMap>0)
	{
		RayPayload.Emission *= texture(textures[Material.EmissionMapTextureID], Triangle.uv).rgb;
	}
	if(length(RayPayload.Emission)>0) RayPayload.DoScatter=false;
	// Emission = vec3(0);

	RayPayload.Color = BaseColor;
	RayPayload.ScatterDir = reflect(gl_WorldRayDirectionEXT, normalize(Normal + RandomInUnitSphere(RayPayload.RandomState) ));
	RayPayload.Distance = gl_HitTEXT;
	RayPayload.Normal = Normal;
	RayPayload.Depth++;

}
