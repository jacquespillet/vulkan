#version 450

const float Exposure=1;

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
    int padding0;

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


layout (set=0, binding = 0) uniform UBO 
{
	mat4 Projection;
	mat4 model;
	mat4 View;
	vec4 CameraPosition;
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

#include "Functions.glsl"
#include "Tonemapping.glsl"
#include "Material.glsl"
#include "IBL.glsl"

void main() 
{
    vec4 BaseColor = GetBaseColor();
    
//     #if ALPHAMODE == ALPHAMODE_OPAQUE
//         BaseColor.a = 1.0;
//     #endif

    vec3 View = normalize(SceneUbo.CameraPosition.xyz - FragPosition);
    normalInfo NormalInfo = getNormalInfo(View);
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
    FinalSpecular += GetIBLRadianceGGX(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.f0, MaterialInfo.SpecularWeight);
    FinalDiffuse += GetIBLRadianceLambertian(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.c_diff, MaterialInfo.f0, MaterialInfo.SpecularWeight);


    float AmbientOcclusion = 1.0;
    if(HAS_OCCLUSION_MAP > 0 &&  MaterialUBO.UseOcclusionMap>0)
    {
        AmbientOcclusion = texture(samplerOcclusion, FragUv).r;
        FinalDiffuse = mix(FinalDiffuse, FinalDiffuse * AmbientOcclusion, MaterialUBO.OcclusionStrength);
        FinalSpecular = mix(FinalSpecular, FinalSpecular * AmbientOcclusion, MaterialUBO.OcclusionStrength);
        FinalSheen = mix(FinalSheen, FinalSheen * AmbientOcclusion, MaterialUBO.OcclusionStrength);
        FinalClearcoat = mix(FinalClearcoat, FinalClearcoat * AmbientOcclusion, MaterialUBO.OcclusionStrength);
    }

//     FinalEmissive = _Emission * _EmissiveStrength;
// #ifdef HAS_EMISSIVE_MAP
//     if(_UseEmissionMap > 0)
//     {
//         FinalEmissive *= texture(_MapsTextureArray, vec3(FragUv, _EmissionMapTextureID)).rgb;
//     }
// #endif

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
        if (BaseColor.a < MaterialUBO.AlphaCutoff)
        {
            discard;
        }
        BaseColor.a = 1.0;
    }

    outputColor = vec4(toneMap(Color), BaseColor.a);   
    if(InstanceUBO.Selected>0) outputColor += vec4(0.5, 0.5, 0, 0);     
}