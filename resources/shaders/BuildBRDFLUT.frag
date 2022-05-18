#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

#include "Common.glsl"


float GeometrySchlickGGXPdf(float NdotV, float Roughness)
{
    float Alpha = Roughness;
    float k = (Alpha * Alpha) / 2.0;

    float Numerator   = NdotV;
    float Denominator = NdotV * (1.0 - k) + k;

    return Numerator / Denominator;
}

// ----------------------------------------------------------------------------
float GeometrySmithPdf(vec3 N, vec3 V, vec3 L, float Roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float GGX2 = GeometrySchlickGGXPdf(NdotV, Roughness);
    float GGX1 = GeometrySchlickGGXPdf(NdotL, Roughness);

    return GGX1 * GGX2;
}



vec2 IntegrateBRDF(float NdotV, float Roughness)
{
    vec3 View;
    View.x = sqrt(1.0 - NdotV*NdotV);
    View.y = 0.0;
    View.z = NdotV;

    float Scale = 0.0;
    float Bias = 0.0;

    vec3 Normal = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 Halfway  = ImportanceSampleGGX(Xi, Normal, Roughness);
        vec3 Light  = normalize(2.0 * dot(View, Halfway) * Halfway - View);

        float NdotL = max(Light.z, 0.0);
        float NdotH = max(Halfway.z, 0.0);
        float VdotH = max(dot(View, Halfway), 0.0);

        if(NdotL > 0.0)
        {
            float G = GeometrySmithPdf(Normal, View, Light, Roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            Scale += (1.0 - Fc) * G_Vis;
            Bias += Fc * G_Vis;
        }
    }
    Scale /= float(SAMPLE_COUNT);
    Bias /= float(SAMPLE_COUNT);
    return vec2(Scale, Bias);
}

void main() 
{
    vec2 IntegratedBRDF = IntegrateBRDF(inUV.x, inUV.y);
    outColor = vec4(IntegratedBRDF, 0, 1);    
}