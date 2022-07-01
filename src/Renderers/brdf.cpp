#include "brdf.h"
#include "PathTraceCPURenderer.h"

#define ONE_OVER_PI (1.0f / PI)
#define ONE_OVER_TWO_PI (1.0f / TWO_PI)
#define F0_DIELECTRICS 0.04



void ONB(glm::vec3 N, glm::vec3& T,  glm::vec3& B)
{
    glm::vec3 Up = abs(N.z) < 0.999 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    T = glm::normalize(glm::cross(Up, N));
    B = glm::cross(N, T);
}

glm::vec3 SampleHemisphere(glm::vec2 u) {
	float a = sqrt(u.x);
	float b = TWO_PI * u.y;
	return glm::vec3(
		a * cos(b),
		a * sin(b),
		sqrt(1.0f - u.x));
}


glm::vec3 BaseColorToSpecularF0(glm::vec3 BaseColor, float Metalness) {
	return mix(glm::vec3(F0_DIELECTRICS, F0_DIELECTRICS, F0_DIELECTRICS), BaseColor, Metalness);
}

glm::vec3 BaseColorToDiffuseReflectance(glm::vec3 BaseColor, float Metalness)
{
	return BaseColor * (1.0f - Metalness);
}

glm::vec3 EvalFresnelSchlick(glm::vec3 F0, float F90, float NdotS)
{
	return F0 + (F90 - F0) * pow(1.0f - NdotS, 5.0f);
}

float Luminance(glm::vec3 RGB)
{
	return dot(RGB, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}
float ShadowedF90(glm::vec3 F0) {
	// This scaler value is somewhat arbitrary, Schuler used 60 in his article. In here, we derive it from F0_DIELECTRICS so
	// that it takes effect for any reflectance lower than least reflective dielectrics
	//const float t = 60.0f;
	const float t = (1.0f / F0_DIELECTRICS);
	return std::min(1.0f, t * Luminance(F0));
}


glm::vec3 SampleSpecularHalfVector(glm::vec3 Ve, glm::vec2 Alpha2D, glm::vec2 u) {

	// Section 3.2: transforming the view direction to the hemisphere configuration
	glm::vec3 Vh = glm::normalize(glm::vec3(Alpha2D.x * Ve.x, Alpha2D.y * Ve.y, Ve.z));

	// Section 4.1: orthonormal basis (with special case if glm::cross product is zero)
	float LenSq = Vh.x * Vh.x + Vh.y * Vh.y;
	glm::vec3 T1 = LenSq > 0.0f ? glm::vec3(-Vh.y, Vh.x, 0.0f) * glm::inversesqrt(LenSq) : glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 T2 = glm::cross(Vh, T1);

	// Section 4.2: parameterization of the projected area
	float r = sqrt(u.x);
	float Phi = TWO_PI * u.y;
	float t1 = r * cos(Phi);
	float t2 = r * sin(Phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = glm::mix(std::sqrt(1.0f - t1 * t1), t2, s);

	// Section 4.3: reprojection onto hemisphere
	glm::vec3 Nh = t1 * T1 + t2 * T2 + sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

	// Section 3.4: transforming the normal back to the ellipsoid configuration
	return glm::normalize(glm::vec3(Alpha2D.x * Nh.x, Alpha2D.y * Nh.y, std::max(0.0f, Nh.z)));
}

// Smith G1 term (masking function) further optimized for GGX distribution (by substituting G_a into G1_GGX)
float SmithG1GGX(float Alpha, float NdotS, float AlphaSquared, float NdotSSquared) {
	return 2.0f / (sqrt(((AlphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
}

float DistributionGGX_GeometrySmith(float Alpha, float AlphaSquared, float NdotL, float NdotV) {
	float G1V = SmithG1GGX(Alpha, NdotV, AlphaSquared, NdotV * NdotV);
	float G1L = SmithG1GGX(Alpha, NdotL, AlphaSquared, NdotL * NdotL);
	return G1L / (G1V + G1L - G1V * G1L);
}


glm::vec3 sampleSpecularMicrofacet(glm::vec3 Vlocal, float Alpha, float AlphaSquared, glm::vec3 SpecularF0, glm::vec2 u,  glm::vec3& Weight) {
	glm::vec3 Hlocal = SampleSpecularHalfVector(Vlocal, glm::vec2(Alpha, Alpha), u);
	glm::vec3 Llocal = reflect(-Vlocal, Hlocal);

	// Note: HdotL is same as HdotV here
	// Clamp dot products here to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
	float HdotL = std::max(0.00001f, std::min(1.0f, dot(Hlocal, Llocal)));
	const glm::vec3 Nlocal = glm::vec3(0.0f, 0.0f, 1.0f);
	float NdotL = std::max(0.00001f, std::min(1.0f, dot(Nlocal, Llocal)));
	float NdotV = std::max(0.00001f, std::min(1.0f, dot(Nlocal, Vlocal)));
	glm::vec3 F = EvalFresnelSchlick(SpecularF0, ShadowedF90(SpecularF0), HdotL);

	// Calculate Weight of the sample specific for selected sampling method 
	// (this is microfacet BRDF divided by PDF of sampling method - notice how most terms cancel out)
	Weight = F * DistributionGGX_GeometrySmith(Alpha, AlphaSquared, NdotL, NdotV);

	return Llocal;
}

// This is an entry point for evaluation of all other BRDFs based on selected configuration (for indirect light)
bool SampleSpecularBRDF(glm::vec2 u, glm::vec3 N, glm::vec3 V,  glm::vec3& RayDirection,  glm::vec3& SampleWeight, glm::vec3 Color, float Roughness, float Metallic) {
	if (dot(N, V) <= 0.0f) return false;

	glm::vec3 T, B;
	ONB(N, T, B);
	glm::vec3 Vlocal = glm::vec3(dot(V, T), dot(V, B), dot(V, N));
	const glm::vec3 Nlocal = glm::vec3(0.0f, 0.0f, 1.0f);

	glm::vec3 RayDirectionLocal = glm::vec3(0.0f, 0.0f, 0.0f);

	float Alpha = Roughness * Roughness;
	float AlphaSquared = Alpha * Alpha;
	glm::vec3 SpecularF0 = BaseColorToSpecularF0(Color, Metallic);

	//Sample a vector in the specular lobe
	RayDirectionLocal = sampleSpecularMicrofacet(Vlocal, Alpha, AlphaSquared, SpecularF0, u, SampleWeight);

	if (Luminance(SampleWeight) == 0.0f) return false;

	RayDirection =  RayDirectionLocal.x * T + RayDirectionLocal.y * B + RayDirectionLocal.z * N;
	return true;
}

bool SampleDiffuseBRDF(glm::vec2 u, glm::vec3 N, glm::vec3 V,  glm::vec3& RayDirection,  glm::vec3& SampleWeight, glm::vec3 Color, float Metallic) {

	// Ignore incident ray coming from "below" the hemisphere
	if (dot(N, V) <= 0.0f) return false;

	glm::vec3 T, B;
	ONB(N, T, B);
	glm::vec3 Vlocal = glm::vec3(dot(V, T), dot(V, B), dot(V, N));
	
	//Sample a vector in hemisphere
	glm::vec3 RayDirectionLocal = SampleHemisphere(u);
	
	SampleWeight = BaseColorToDiffuseReflectance(Color, Metallic);
	 
	if (Luminance(SampleWeight) == 0.0f) return false;
	
	//Transform back
	RayDirection =  RayDirectionLocal.x * T + RayDirectionLocal.y * B + RayDirectionLocal.z * N;
	
	return true;
}

float GetBrdfProbability(rayPayload RayPayload, glm::vec3 V, glm::vec3 ShadingNormal, glm::vec3 Color, float Metallic) {
	//We compute the fresnel using the shading normal as we don't have H yet.
	float SpecularF0 = Luminance(BaseColorToSpecularF0(Color, Metallic));
	float Fresnel = glm::clamp(Luminance(EvalFresnelSchlick(glm::vec3(SpecularF0), ShadowedF90(glm::vec3(SpecularF0)), std::max(0.0f, dot(V, ShadingNormal)))), 0.0f, 1.0f);

	float DiffuseReflectance = Luminance(BaseColorToDiffuseReflectance(Color, Metallic));
	
	// Approximate relative contribution of BRDFs using the Fresnel term
	float Specular = Fresnel;
	float Diffuse = DiffuseReflectance * (1.0f - Fresnel); 
	// Return probability of selecting Specular BRDF over Diffuse BRDF
	float p = (Specular / std::max(0.0001f, (Specular + Diffuse)));

	// Clamp probability to avoid undersampling of less prominent BRDF
	return glm::clamp(p, 0.1f, 0.9f);
}


// This is an entry point for evaluation of all other BRDFs based on selected configuration (for direct light)
glm::vec3 EvalCombinedBRDF(glm::vec3 N, glm::vec3 L, glm::vec3 V, glm::vec3 Color, float Metallic) {
	glm::vec3 diffuse = BaseColorToDiffuseReflectance(Color, Metallic);
	return diffuse;
}