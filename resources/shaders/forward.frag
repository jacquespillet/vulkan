#version 450

#include "Common/Defines.glsl"

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



#include "Common/SceneUBO.glsl"
layout (set=0, binding = 0) uniform UBO 
{
    sceneUbo Data;
} SceneUbo;


layout (set=2, binding = 0) uniform instance 
{
	mat4 Model;
	mat4 Normal;
	float Selected;
	vec3 Padding;
} InstanceUBO;


layout (set=3, binding = 1) uniform samplerCube Cubemap;
layout (set=3, binding = 2) uniform samplerCube IrradianceMap;
layout (set=3, binding = 3) uniform samplerCube PrefilteredEnv;
layout (set=3, binding = 4) uniform sampler2D BRDFLUT;


layout(location=0) in vec3 FragPosition;
layout(location=1) in vec3 FragNormal;
layout(location=2) in vec2 FragUv;
layout(location=3) in mat3 TBN;

layout (location = 0) out vec4 outputColor;

layout (constant_id = 0) const float MASK = 0;
layout (constant_id = 1) const int HAS_METALLIC_ROUGHNESS_MAP=0;
layout (constant_id = 2) const int HAS_EMISSIVE_MAP=0;
layout (constant_id = 3) const int HAS_BASE_COLOR_MAP=0;
layout (constant_id = 4) const int HAS_OCCLUSION_MAP=0;
layout (constant_id = 5) const int HAS_NORMAL_MAP=0;
layout (constant_id = 6) const int HAS_CLEARCOAT=0;
layout (constant_id = 7) const int HAS_SHEEN=0;

#include "Common/Functions.glsl"
#include "Common/Tonemapping.glsl"
#include "Common/MaterialForward.glsl"
#include "Common/IBL.glsl"
#include "Common/Common.glsl"

void main() 
{
    vec4 BaseColor = GetBaseColor();
    
//     #if ALPHAMODE == ALPHAMODE_OPAQUE
//         BaseColor.a = 1.0;
//     #endif

    vec3 View = normalize(SceneUbo.Data.CameraPosition - FragPosition);
    normalInfo NormalInfo = getNormalInfo();
    vec3 Normal = NormalInfo.n;
    vec3 Tangent = NormalInfo.t;
    vec3 Bitangent = NormalInfo.b;

    float NdotV = ClampedDot(Normal, View);
    float TdotV = ClampedDot(Tangent, View);
    float BdotV = ClampedDot(Bitangent, View);


    materialInfo MaterialInfo;
    MaterialInfo.BaseColor = BaseColor.rgb;
    
//     // // The default index of refraction of 1.5 yields a dielectric normal incidence Reflectance of 0.04.
    MaterialInfo.ior = 1.5;
    MaterialInfo.f0 = vec3(0.04);
    MaterialInfo.SpecularWeight = 1.0;

    MaterialInfo = GetMetallicRoughnessInfo(MaterialInfo);

// #ifdef MATERIAL_CLEARCOAT
//     MaterialInfo = GetClearCoatInfo(MaterialInfo, NormalInfo);
// #endif

//     // // Compute Reflectance.
    float Reflectance = max(max(MaterialInfo.f0.r, MaterialInfo.f0.g), MaterialInfo.f0.b);

    MaterialInfo.F90 = vec3(1.0);
//     // // LIGHTING
    vec3 FinalSpecular = vec3(0.0);
    vec3 FinalDiffuse = vec3(0.0);
    vec3 FinalEmissive = vec3(0.0);
    vec3 FinalClearcoat = vec3(0.0);
    vec3 FinalSheen = vec3(0.0);
    vec3 FinalTransmission = vec3(0.0);

    // float AlbedoSheenScaling = 1.0;

// #ifdef MATERIAL_CLEARCOAT
//     FinalClearcoat += GetIBLRadianceGGX(MaterialInfo.ClearcoatNormal, View, MaterialInfo.ClearcoatRoughness, MaterialInfo.ClearcoatF0, 1.0);
// #endif

//     // // Calculate lighting contribution from image based lighting source (IBL)
    if(SceneUbo.Data.BackgroundType == BACKGROUND_TYPE_CUBEMAP || SceneUbo.Data.BackgroundType == BACKGROUND_TYPE_COLOR)
    {
        FinalSpecular += GetIBLRadianceGGX(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.f0, MaterialInfo.SpecularWeight);
        FinalDiffuse += GetIBLRadianceLambertian(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.c_diff, MaterialInfo.f0, MaterialInfo.SpecularWeight);
    }
    else if(SceneUbo.Data.BackgroundType==BACKGROUND_TYPE_DIRLIGHT)
    {
        vec3 H = normalize(-SceneUbo.Data.LightDirection + View);
        float NdotL = ClampedDot(Normal, -SceneUbo.Data.LightDirection);
        float VdotH = ClampedDot(View, H);
        float NdotH = ClampedDot(Normal, H);
        FinalDiffuse += SceneUbo.Data.BackgroundIntensity * NdotL *  GetBRDFLambertian(MaterialInfo.f0, MaterialInfo.F90, MaterialInfo.c_diff, MaterialInfo.SpecularWeight, VdotH);
        FinalSpecular += SceneUbo.Data.BackgroundIntensity * NdotL * GetBRDFSpecularGGX(MaterialInfo.f0, MaterialInfo.F90, MaterialInfo.AlphaRoughness, MaterialInfo.SpecularWeight, VdotH, NdotL, NdotV, NdotH);
    }


    float AmbientOcclusion = 1.0;
    if(HAS_OCCLUSION_MAP > 0 &&  MaterialUBO.Material.UseOcclusionMap>0)
    {
        AmbientOcclusion = texture(samplerOcclusion, FragUv).r;
        FinalDiffuse = mix(FinalDiffuse, FinalDiffuse * AmbientOcclusion, MaterialUBO.Material.OcclusionStrength);
        FinalSpecular = mix(FinalSpecular, FinalSpecular * AmbientOcclusion, MaterialUBO.Material.OcclusionStrength);
        FinalSheen = mix(FinalSheen, FinalSheen * AmbientOcclusion, MaterialUBO.Material.OcclusionStrength);
        FinalClearcoat = mix(FinalClearcoat, FinalClearcoat * AmbientOcclusion, MaterialUBO.Material.OcclusionStrength);
    }

    FinalEmissive = MaterialUBO.Material.Emission * MaterialUBO.Material.EmissiveStrength;
    if(HAS_EMISSIVE_MAP > 0 && MaterialUBO.Material.UseEmissionMap>0)
    {
        FinalEmissive *= texture(samplerEmission, FragUv).rgb;
    }

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
//     Color = FinalSheen + Color * AlbedoSheenScaling;
//     Color = Color * (1.0 - ClearcoatFactor * ClearcoatFresnel) + FinalClearcoat;

//     //TODO(Jacques): Alpha cutoff
//     // // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
    if(MASK>0)
    {
        // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
        if (BaseColor.a < MaterialUBO.Material.AlphaCutoff)
        {
            discard;
        }
        BaseColor.a = 1.0;
    }

    outputColor = vec4(toneMap(Color, SceneUbo.Data.Exposure), BaseColor.a);   
    if(InstanceUBO.Selected>0) outputColor += vec4(0.5, 0.5, 0, 0);     

    if(MaterialUBO.Material.DebugChannel>0 || SceneUbo.Data.DebugChannel>0)
    {
        if(MaterialUBO.Material.DebugChannel == 1 || SceneUbo.Data.DebugChannel == 1)
        {
            outputColor = vec4(FragUv, 0, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 2)  || (SceneUbo.Data.DebugChannel == 2))
        {
            outputColor = vec4(NormalInfo.ntex, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 3)  || (SceneUbo.Data.DebugChannel == 3))
        {
            outputColor = vec4(NormalInfo.ng, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 4)  || (SceneUbo.Data.DebugChannel == 4))
        {
            outputColor = vec4(NormalInfo.t, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 5)  || (SceneUbo.Data.DebugChannel == 5))
        {
            outputColor = vec4(NormalInfo.b, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 6)  || (SceneUbo.Data.DebugChannel == 6))
        {
            outputColor = vec4(NormalInfo.n, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 7)  || (SceneUbo.Data.DebugChannel == 7))
        {
            outputColor = BaseColor.aaaa;
        }
        else  if((MaterialUBO.Material.DebugChannel == 8)  || (SceneUbo.Data.DebugChannel == 8))
        {
            outputColor = vec4(vec3(AmbientOcclusion), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 9)  || (SceneUbo.Data.DebugChannel == 9))
        {
            outputColor = vec4(FinalEmissive, 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 10) || (SceneUbo.Data.DebugChannel == 10))
        {
            outputColor = vec4(vec3(MaterialInfo.Metallic), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 11) || (SceneUbo.Data.DebugChannel == 11))
        {
            outputColor = vec4(vec3(MaterialInfo.PerceptualRoughness), 1);
        }
        else  if((MaterialUBO.Material.DebugChannel == 12) || (SceneUbo.Data.DebugChannel == 12))
        {
            outputColor = vec4(BaseColor.rgb, 1);
        }
        // Clearcoat,
        // ClearcoatFactor,
        // ClearcoatNormal,
        // ClearcoatRoughnes,
        // Sheen
    }
}