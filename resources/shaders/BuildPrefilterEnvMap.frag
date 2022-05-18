#version 450

layout (set=0, binding = 1) uniform samplerCube Cubemap;

layout (set=0, binding = 0) uniform UBO 
{
	mat4 viewProjectionMatrix;
	float Roughness;
} ubo;

layout (location = 0) in vec3 inUV;
layout (location = 1) in vec3 inPosition;

layout (location = 0) out vec4 outColor;


#include "Common.glsl"

void main() 
{
    vec3 Normal = normalize(inPosition);    

    //We set V to be the normal
    vec3 Reflected = Normal;
    vec3 View = Reflected;

    const uint SAMPLE_COUNT = 1024u;
    float TotalWeight = 0.0;   
    vec3 PrefilteredColor = vec3(0.0);     
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {


        //Sample 2 random vars for sampling the distribution
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        //Find the halfway vector
        vec3 Halfway  = ImportanceSampleGGX(Xi, Normal, ubo.Roughness);
        //Find the light vector
        vec3 Light  = normalize(2.0 * dot(View, Halfway) * Halfway - View);

        float NdotL = max(dot(Normal, Light), 0.0);
        if(NdotL > 0.0)
        {
            float D   = DistributionGGX(Normal, Halfway, ubo.Roughness);
            float NdotH = max(dot(Normal, Halfway), 0.0);
            float HdotV = max(dot(Halfway, View), 0.0);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 
            float resolution = 512.0; // resolution of source cubemap (per face)
            float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mipLevel = ubo.Roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            //Sample the light
            PrefilteredColor += textureLod(Cubemap, Light, mipLevel).rgb * NdotL;
            TotalWeight      += NdotL;
        }
    }
    PrefilteredColor = PrefilteredColor / TotalWeight;

    outColor = vec4(PrefilteredColor, 1.0);
}