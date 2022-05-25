#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout (set=0, binding = 0) uniform sampler2D samplerPositionDepth;
layout (set=0, binding = 1) uniform sampler2D samplerNormal;
layout (set=0, binding = 2) uniform usampler2D samplerAlbedoMetallicRoughnessOcclusionOcclusionStrength;
layout (set=0, binding = 3) uniform sampler2D samplerEmission;



layout (set=1, binding = 1) uniform samplerCube Cubemap;
layout (set=1, binding = 2) uniform samplerCube IrradianceMap;
layout (set=1, binding = 3) uniform samplerCube PrefilteredEnv;
layout (set=1, binding = 4) uniform sampler2D BRDFLUT;


#include "SceneUBO.glsl"
layout (set=2, binding = 0) uniform UBO 
{
	sceneUbo Data;	
} SceneUbo;


layout (constant_id = 0) const int SSAO_ENABLED = 1;
layout (constant_id = 1) const float AMBIENT_FACTOR = 0.0;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;

#include "Functions.glsl"
#include "MaterialDeferred.glsl"
#include "IBL.glsl"
#include "Tonemapping.glsl"

void main() 
{
	vec4 PositionDepth = texture(samplerPositionDepth, inUV);
	vec3 Position = PositionDepth.xyz;
	float Depth = PositionDepth.w;

	vec3 Normal = texture(samplerNormal, inUV).xyz * 2.0 - 1.0;

	ivec2 texDim = textureSize(samplerAlbedoMetallicRoughnessOcclusionOcclusionStrength, 0);
	uvec4 albedo = texelFetch(samplerAlbedoMetallicRoughnessOcclusionOcclusionStrength, ivec2(inUV.st * texDim ), 0);
	vec4 BaseColor;
	BaseColor.rg = unpackHalf2x16(albedo.r);
	BaseColor.ba = unpackHalf2x16(albedo.g);

	vec2 MetallicRoughness;
	MetallicRoughness = unpackHalf2x16(albedo.b);
	float Metallic = MetallicRoughness.x;
	float Roughness = MetallicRoughness.y;

	vec2 OcclusionOcclusionStrength;
	OcclusionOcclusionStrength = unpackHalf2x16(albedo.a);
	float AmbientOcclusion = OcclusionOcclusionStrength.x;
	float OcclusionStrength = OcclusionOcclusionStrength.y;

	
	vec3 Emission = texture(samplerEmission, inUV).xyz;

	materialInfo MaterialInfo;
    MaterialInfo.BaseColor = BaseColor.rgb;
    
//     // // The default index of refraction of 1.5 yields a dielectric normal incidence Reflectance of 0.04.
    MaterialInfo.ior = 1.5;
    MaterialInfo.f0 = vec3(0.04);
    MaterialInfo.SpecularWeight = 1.0;
    MaterialInfo = GetMetallicRoughnessInfo(MaterialInfo, Roughness, Metallic);

    vec3 View = normalize(SceneUbo.Data.CameraPosition - Position);
	float NdotV = ClampedDot(Normal, View);

	float Reflectance = max(max(MaterialInfo.f0.r, MaterialInfo.f0.g), MaterialInfo.f0.b);

    MaterialInfo.F90 = vec3(1.0);
//     // // LIGHTING
    vec3 FinalSpecular = vec3(0.0);
    vec3 FinalDiffuse = vec3(0.0);
    vec3 FinalEmissive = vec3(0.0);
    vec3 FinalClearcoat = vec3(0.0);
    vec3 FinalSheen = vec3(0.0);
    vec3 FinalTransmission = vec3(0.0);
    
	FinalSpecular += GetIBLRadianceGGX(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.f0, MaterialInfo.SpecularWeight);
    FinalDiffuse += GetIBLRadianceLambertian(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.c_diff, MaterialInfo.f0, MaterialInfo.SpecularWeight);


	// FinalDiffuse = mix(FinalDiffuse, FinalDiffuse * AmbientOcclusion, MaterialUBO.OcclusionStrength);
	// FinalSpecular = mix(FinalSpecular, FinalSpecular * AmbientOcclusion, MaterialUBO.OcclusionStrength);
	// FinalSheen = mix(FinalSheen, FinalSheen * AmbientOcclusion, MaterialUBO.OcclusionStrength);
	// FinalClearcoat = mix(FinalClearcoat, FinalClearcoat * AmbientOcclusion, MaterialUBO.OcclusionStrength);

    FinalEmissive = Emission;
    
//     // // Layer blending

//     //TODO(Jacques): Clearcoat
//     float ClearcoatFactor = 0.0;
//     vec3 ClearcoatFresnel = vec3(0);
// #ifdef MATERIAL_CLEARCOAT
//     ClearcoatFactor = MaterialInfo.ClearcoatFactor;
//     ClearcoatFresnel = FresnelShlick(MaterialInfo.ClearcoatF0, MaterialInfo.ClearcoatF90, ClampedDot(MaterialInfo.ClearcoatNormal, View));
//     FinalClearcoat = FinalClearcoat * ClearcoatFactor;
// #endif    

    vec3 Color = vec3(0);
    Color = FinalEmissive + FinalDiffuse + FinalSpecular;


    outFragcolor = vec4(toneMap(Color, SceneUbo.Data.Exposure), BaseColor.a);   
}