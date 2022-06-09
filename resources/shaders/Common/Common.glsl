const float PI = 3.14159265359;

float RadicalInverse_VdC(uint Bits) 
{
    Bits = (Bits << 16u) | (Bits >> 16u);
    Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
    Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
    Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
    Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
    return float(Bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
} 


vec3 ImportanceSampleGGX(vec2 Xi, vec3 Normal, float Roughness)
{
    float Alpha = Roughness*Roughness;
	
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (Alpha*Alpha - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta*CosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 Halfway;
    Halfway.x = cos(Phi) * SinTheta;
    Halfway.y = sin(Phi) * SinTheta;
    Halfway.z = CosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 Up        = abs(Normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 Tangent   = normalize(cross(Up, Normal));
    vec3 Bitangent = cross(Normal, Tangent);
	
    vec3 SampleVec = Tangent * Halfway.x + Bitangent * Halfway.y + Normal * Halfway.z;
    return normalize(SampleVec);
}  


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   

vec3 GGX(vec3 N, vec3 H, vec3 V, vec3 L, float roughness, float metallic, vec3 albedo)
{    
    vec3 F0 = vec3(0.04); 
    F0      = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);    
    vec3 F    = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);        
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;  

    vec3 kS = F;

    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;  
    
    // return  specular;
    return (kD * albedo / PI + specular);   
}


vec3 FresnelShlick(vec3 f0, vec3 F90, float VdotH)
{
    return f0 + (F90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

float V_GGX(float NdotL, float NdotV, float AlphaRoughness)
{
    float alphaRoughnessSq = AlphaRoughness * AlphaRoughness;

    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

    float GGX = GGXV + GGXL;
    if (GGX > 0.0)
    {
        return 0.5 / GGX;
    }
    return 0.0;
}

float D_GGX(float NdotH, float AlphaRoughness)
{
    float alphaRoughnessSq = AlphaRoughness * AlphaRoughness;
    float f = (NdotH * NdotH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (PI * f * f);
}

vec3 GetBRDFLambertian(vec3 f0, vec3 F90, vec3 diffuseColor, float SpecularWeight, float VdotH)
{
    return (1.0 - SpecularWeight * FresnelShlick(f0, F90, VdotH)) * (diffuseColor / PI);
}

vec3 GetBRDFSpecularGGX(vec3 f0, vec3 F90, float AlphaRoughness, float SpecularWeight, float VdotH, float NdotL, float NdotV, float NdotH)
{
    vec3 F = FresnelShlick(f0, F90, VdotH);
    float Vis = V_GGX(NdotL, NdotV, AlphaRoughness);
    float D = D_GGX(NdotH, AlphaRoughness);

    return SpecularWeight * F * Vis * D;
}