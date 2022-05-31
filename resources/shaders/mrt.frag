#version 450



#include "Common/SceneUBO.glsl"
layout (set=0, binding = 0) uniform UBO 
{
    sceneUbo Data;
} SceneUbo;

#include "Common/Material.glsl"
layout (set=1, binding = 0) uniform materialUBO 
{
    material Material;   
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

#include "Common/Functions.glsl"
#include "Common/MaterialForward.glsl"

void main() 
{
    vec4 BaseColor = GetBaseColor();
    materialInfo MaterialInfo;


    //Metallic Roughness
    float Metallic = MaterialUBO.Material.Metallic;
    float PerceptualRoughness = MaterialUBO.Material.Roughness;
    if(HAS_METALLIC_ROUGHNESS_MAP>0 &&  MaterialUBO.Material.UseMetallicRoughnessMap >0)
    {
        vec4 mrSample = texture(samplerSpecular, FragUv);
        PerceptualRoughness *= mrSample.g;
        Metallic *= mrSample.b;
    }

    //Occlusion
    float AmbientOcclusion = 1.0;
    float OcclusionStrength = MaterialUBO.Material.OcclusionStrength;
    if(HAS_OCCLUSION_MAP > 0 &&  MaterialUBO.Material.UseOcclusionMap>0)
    {
        AmbientOcclusion = texture(samplerOcclusion, FragUv).r;
    }    


    vec3 FinalEmissive = MaterialUBO.Material.Emission * MaterialUBO.Material.EmissiveStrength;
    if(HAS_EMISSIVE_MAP > 0 && MaterialUBO.Material.UseEmissionMap>0)
    {
        FinalEmissive *= texture(samplerEmission, FragUv).rgb;
    }

    //Normal
    normalInfo NormalInfo = getNormalInfo();

    if(MaterialUBO.Material.DebugChannel>0 || SceneUbo.Data.DebugChannel>0)
    {
        if(MaterialUBO.Material.DebugChannel == 1 || SceneUbo.Data.DebugChannel == 1)
        {
            BaseColor = vec4(FragUv, 0, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 2)  || (SceneUbo.Data.DebugChannel == 2))
        {
            BaseColor = vec4(NormalInfo.ntex, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 3)  || (SceneUbo.Data.DebugChannel == 3))
        {
            BaseColor = vec4(NormalInfo.ng, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 4)  || (SceneUbo.Data.DebugChannel == 4))
        {
            BaseColor = vec4(NormalInfo.t, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 5)  || (SceneUbo.Data.DebugChannel == 5))
        {
            BaseColor = vec4(NormalInfo.b, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 6)  || (SceneUbo.Data.DebugChannel == 6))
        {
            BaseColor = vec4(NormalInfo.n, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 7)  || (SceneUbo.Data.DebugChannel == 7))
        {
            BaseColor = BaseColor.aaaa;
        }
        else  if((MaterialUBO.Material.DebugChannel == 8)  || (SceneUbo.Data.DebugChannel == 8))
        {
            BaseColor = vec4(vec3(AmbientOcclusion), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 9)  || (SceneUbo.Data.DebugChannel == 9))
        {
            BaseColor = vec4(FinalEmissive, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 10) || (SceneUbo.Data.DebugChannel == 10))
        {
            BaseColor = vec4(vec3(MaterialInfo.Metallic), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 11) || (SceneUbo.Data.DebugChannel == 11))
        {
            BaseColor = vec4(vec3(MaterialInfo.PerceptualRoughness), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 12) || (SceneUbo.Data.DebugChannel == 12))
        {
            BaseColor = vec4(BaseColor.rgb, 1);
        }
    }        
	
    //Change BaseColor based on debug channels
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.r = packHalf2x16(BaseColor.rg);
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.g = packHalf2x16(BaseColor.ba);
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.b = packHalf2x16(vec2(Metallic, PerceptualRoughness));
	outAlbedoMetallicRoughnessOcclusionOcclusionStrength.a = packHalf2x16(vec2(AmbientOcclusion, OcclusionStrength));
    
    outEmission = vec4(FinalEmissive, 1.0);
    outNormal.xyz = NormalInfo.n  * 0.5 + 0.5;
    outPositionDepth = vec4(FragWorldPos, 0);

}