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