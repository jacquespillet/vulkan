#version 450

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




layout (location = 0) in vec3 FragNormal;
layout (location = 1) in vec2 FragUv;
layout (location = 2) in vec3 FragWorldPos;
layout (location = 3) in mat3 TBN;

layout (location = 0) out vec4 outPositionDepth;
layout (location = 2) out uvec4 outAlbedoRoughness;
layout (location = 1) out vec4 outNormalMetallic;


layout (constant_id = 0) const float MASK = 0;
layout (constant_id = 1) const int HAS_METALLIC_ROUGHNESS_MAP=0;
layout (constant_id = 2) const int HAS_EMISSIVE_MAP=0;
layout (constant_id = 3) const int HAS_BASE_COLOR_MAP=0;
layout (constant_id = 4) const int HAS_OCCLUSION_MAP=0;
layout (constant_id = 5) const int HAS_NORMAL_MAP=0;
layout (constant_id = 6) const int HAS_CLEARCOAT=0;
layout (constant_id = 7) const int HAS_SHEEN=0;

#include "Functions.glsl"
#include "Material.glsl"

void main() 
{
	// outPosition = vec4(FragWorldPos, linearDepth(gl_FragCoord.z));
	outPositionDepth = vec4(FragWorldPos, 0);
	
	float specular = texture(samplerSpecular, FragUv).r;
	
	vec4 color = texture(samplerColor, FragUv);
	outAlbedoRoughness.r = packHalf2x16(color.rg);
	outAlbedoRoughness.g = packHalf2x16(color.ba);
	outAlbedoRoughness.b = packHalf2x16(vec2(specular, 0.0));
}