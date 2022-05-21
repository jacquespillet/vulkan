#version 450



layout (set=0, binding = 0) uniform UBO 
{
	mat4 Projection;
	mat4 model;
	mat4 View;
	vec4 CameraPosition;
} SceneUbo;

layout (set=1, binding = 0) uniform materialUBO 
{
    int BaseColorTextureID;
    int MetallicRoughnessTextureID;
    int NormalMapTextureID;
    int EmissionMapTextureID;
    
    int OcclusionMapTextureID;
    int UseBaseColorMap;
    int UseMetallicRoughnessMap;
    int UseNormalMap;

    int UseEmissionMap;
    int UseOcclusionMap;
    int AlphaMode;
    int DebugChannel;

    float Roughness;
    float AlphaCutoff;
    float ClearcoatRoughness;
    float padding1;
    
    float Metallic;
    float OcclusionStrength;
    float EmissiveStrength;
    float ClearcoatFactor;
    
    vec3 BaseColor;
    float Opacity;
    
    vec3 Emission;
    float Exposure;   
} MaterialUBO;
layout (set=1, binding = 1) uniform sampler2D samplerColor;
layout (set=1, binding = 2) uniform sampler2D samplerSpecular;
layout (set=1, binding = 3) uniform sampler2D samplerNormal;
layout (set=1, binding = 4) uniform sampler2D samplerOcclusion;
layout (set=1, binding = 5) uniform sampler2D samplerEmission;





layout (location = 0) in vec3 FragNormal;
layout (location = 1) in vec2 FragUv;
layout (location = 2) in vec3 FragWorldPos;
layout (location = 3) in mat3 TBN;

layout (location = 0) out vec4 outPositionDepth;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out uvec4 outAlbedoMetallicRoughnessOcclusionOcclusionStrength;
layout (location = 3) out vec4 outEmission;


layout (constant_id = 0) const float MASK = 0;
layout (constant_id = 1) const int HAS_METALLIC_ROUGHNESS_MAP=0;
layout (constant_id = 2) const int HAS_EMISSIVE_MAP=0;
layout (constant_id = 3) const int HAS_BASE_COLOR_MAP=0;
layout (constant_id = 4) const int HAS_OCCLUSION_MAP=0;
layout (constant_id = 5) const int HAS_NORMAL_MAP=0;
layout (constant_id = 6) const int HAS_CLEARCOAT=0;
layout (constant_id = 7) const int HAS_SHEEN=0;

#include "Functions.glsl"
#include "MaterialForward.glsl"

void main() 
{
    vec4 BaseColor = GetBaseColor();
    materialInfo MaterialInfo;

    //Metallic Roughness
    float Metallic = MaterialUBO.Metallic;
    float PerceptualRoughness = MaterialUBO.Roughness;
    if(HAS_METALLIC_ROUGHNESS_MAP>0 &&  MaterialUBO.UseMetallicRoughnessMap >0)
    {
        vec4 mrSample = texture(samplerSpecular, FragUv);
        PerceptualRoughness *= mrSample.g;
        Metallic *= mrSample.b;
    }

    //Occlusion
    float AmbientOcclusion = 1.0;
    float OcclusionStrength = MaterialUBO.OcclusionStrength;
    if(HAS_OCCLUSION_MAP > 0 &&  MaterialUBO.UseOcclusionMap>0)
    {
        AmbientOcclusion = texture(samplerOcclusion, FragUv).r;
    }    
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.r = packHalf2x16(BaseColor.rg);
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.g = packHalf2x16(BaseColor.ba);
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.b = packHalf2x16(vec2(Metallic, PerceptualRoughness));
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.a = packHalf2x16(vec2(AmbientOcclusion, OcclusionStrength));


    // outEmissionEmissionStrength.r = packHalf2x16()
    vec3 FinalEmissive = MaterialUBO.Emission * MaterialUBO.EmissiveStrength;
    if(HAS_EMISSIVE_MAP > 0 && MaterialUBO.UseEmissionMap>0)
    {
        FinalEmissive *= texture(samplerEmission, FragUv).rgb;
    }
    outEmission = vec4(FinalEmissive, 1.0);

    
    normalInfo NormalInfo = getNormalInfo();
    outNormal.xyz = NormalInfo.n  * 0.5 + 0.5;
	
    //Change BaseColor based on debug channels
    outPositionDepth = vec4(FragWorldPos, 0);

}