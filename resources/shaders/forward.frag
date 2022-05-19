#version 450

layout (set=1, binding = 0) uniform sampler2D samplerColor;
layout (set=1, binding = 1) uniform sampler2D samplerSpecular;
layout (set=1, binding = 2) uniform sampler2D samplerNormal;
layout (set=1, binding = 3) uniform materialUBO 
{
    int BaseColorTextureID;
    int MetallicRoughnessTextureID;
    int NormalMapTextureID;
    int EmissionMapTextureID;
    int OcclusionMapTextureID;
    int UseBaseColor;
    int UseMetallicRoughness;
    int UseNormalMap;
    int UseEmissionMap;
    int UseOcclusionMap;
    float Roughness;
    float AlphaCutoff;
    float Metallic;
    float OcclusionStrength;
    float EmissiveStrength;
    float ClearcoatFactor;
    float ClearcoatRoughness;
    float Exposure;
    float Opacity;
    int AlphaMode;
    vec3 BaseColor;
    vec3 Emission;    
} MaterialUBO;


layout (set=0, binding = 0) uniform UBO 
{
	mat4 Projection;
	mat4 model;
	mat4 View;
	vec2 viewport;
	vec3 CameraPosition;
} SceneUbo;

layout(location=0) in vec3 FragPosition;
layout(location=1) in vec3 FragNormal;
layout(location=2) in vec2 FragUv;
layout(location=3) in mat3 TBN;

layout (location = 0) out vec4 outputColor;

layout (constant_id = 0) const float NEAR_PLANE = 0.1f;
layout (constant_id = 1) const float FAR_PLANE = 64.0f;
layout (constant_id = 2) const int ENABLE_DISCARD = 0;

#include "Material.glsl"

void main() 
{
    vec4 BaseColor = GetBaseColor();
    
//     #if ALPHAMODE == ALPHAMODE_OPAQUE
//         BaseColor.a = 1.0;
//     #endif

    vec3 View = normalize(SceneUbo.CameraPosition - FragPosition);
    normalInfo NormalInfo = getNormalInfo(View);
    vec3 Normal = NormalInfo.n;
    vec3 Tangent = NormalInfo.t;
    vec3 Bitangent = NormalInfo.b;

//     float NdotV = ClampedDot(Normal, View);
//     float TdotV = ClampedDot(Tangent, View);
//     float BdotV = ClampedDot(Bitangent, View);


//     materialInfo MaterialInfo;
//     MaterialInfo.BaseColor = BaseColor.rgb;
    
//     // // The default index of refraction of 1.5 yields a dielectric normal incidence Reflectance of 0.04.
//     MaterialInfo.ior = 1.5;
//     MaterialInfo.f0 = vec3(0.04);
//     MaterialInfo.SpecularWeight = 1.0;

//     MaterialInfo = getMetallicRoughnessInfo(MaterialInfo);

// #ifdef MATERIAL_CLEARCOAT
//     MaterialInfo = GetClearCoatInfo(MaterialInfo, NormalInfo);
// #endif

//     MaterialInfo.PerceptualRoughness = clamp(MaterialInfo.PerceptualRoughness, 0.0, 1.0);
//     MaterialInfo.Metallic = clamp(MaterialInfo.Metallic, 0.0, 1.0);

//     MaterialInfo.AlphaRoughness = MaterialInfo.PerceptualRoughness * MaterialInfo.PerceptualRoughness;

//     // // Compute Reflectance.
//     float Reflectance = max(max(MaterialInfo.f0.r, MaterialInfo.f0.g), MaterialInfo.f0.b);

    
//     MaterialInfo.F90 = vec3(1.0);

//     // // LIGHTING
//     vec3 FinalSpecular = vec3(0.0);
//     vec3 FinalDiffuse = vec3(0.0);
//     vec3 FinalEmissive = vec3(0.0);
//     vec3 FinalClearcoat = vec3(0.0);
//     vec3 FinalSheen = vec3(0.0);
//     vec3 FinalTransmission = vec3(0.0);

//     float AlbedoSheenScaling = 1.0;

// #ifdef MATERIAL_CLEARCOAT
//     FinalClearcoat += GetIBLRadianceGGX(MaterialInfo.ClearcoatNormal, View, MaterialInfo.ClearcoatRoughness, MaterialInfo.ClearcoatF0, 1.0);
// #endif

//     // // Calculate lighting contribution from image based lighting source (IBL)
//     FinalSpecular += GetIBLRadianceGGX(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.f0, MaterialInfo.SpecularWeight);
//     FinalDiffuse += GetIBLRadianceLambertian(Normal, View, MaterialInfo.PerceptualRoughness, MaterialInfo.c_diff, MaterialInfo.f0, MaterialInfo.SpecularWeight);


//     float AmbientOcclusion = 1.0;
// #ifdef HAS_OCCLUSION_MAP
//     if(_UseOcclusionMap>0)
//     {
//         AmbientOcclusion = texture(_MapsTextureArray, vec3(FragUv, _OcclusionMapTextureID)).r;
//         FinalDiffuse = mix(FinalDiffuse, FinalDiffuse * AmbientOcclusion, _OcclusionStrength);
//         // apply ambient occlusion to all lighting that is not punctual
//         FinalSpecular = mix(FinalSpecular, FinalSpecular * AmbientOcclusion, _OcclusionStrength);
//         FinalSheen = mix(FinalSheen, FinalSheen * AmbientOcclusion, _OcclusionStrength);
//         FinalClearcoat = mix(FinalClearcoat, FinalClearcoat * AmbientOcclusion, _OcclusionStrength);
//     }
// #endif

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

//     vec3 Diffuse = FinalDiffuse;


//     vec3 Color = vec3(0);
//     Color = FinalEmissive + Diffuse + FinalSpecular;
//     Color = FinalSheen + Color * AlbedoSheenScaling;
//     Color = Color * (1.0 - ClearcoatFactor * ClearcoatFresnel) + FinalClearcoat;

//     //TODO(Jacques): Alpha cutoff
//     // // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
// #if ALPHAMODE == ALPHAMODE_MASK
//     // Late discard to avoid samplig artifacts. See https://github.com/KhronosGroup/glTF-Sample-Viewer/issues/267
//     if (BaseColor.a < _AlphaCutoff)
//     {
//         discard;
//     }
//     BaseColor.a = 1.0;
// #endif

    // outputColor = vec4(toneMap(Color), BaseColor.a);        
    outputColor = vec4(NormalInfo.n, 1);        
}