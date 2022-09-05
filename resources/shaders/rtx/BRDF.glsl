#define PI 3.14159265359
#define TWO_PI (2.0f * PI)
#define ONE_OVER_PI (1.0f / PI)
#define ONE_OVER_TWO_PI (1.0f / TWO_PI)
#define F0_DIELECTRICS 0.04

void ONB(in vec3 N, inout vec3 T, inout vec3 B)
{
    vec3 Up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    T = normalize(cross(Up, N));
    B = cross(N, T);
}


vec4 getRotationToZAxis(vec3 vector) {

	// Handle special case when vector is exact or near opposite of (0, 0, 1)
	if (vector.z < -0.99999f) return vec4(1.0f, 0.0f, 0.0f, 0.0f);

	return normalize(vec4(vector.y, -vector.x, 0.0f, 1.0f + vector.z));
}

vec3 rotatePoint(vec4 q, vec3 v) {
	const vec3 qAxis = vec3(q.x, q.y, q.z);
	return 2.0f * dot(qAxis, v) * qAxis + (q.w * q.w - dot(qAxis, qAxis)) * v + 2.0f * q.w * cross(qAxis, v);
}

vec4 invertRotation(vec4 q)
{
	return vec4(-q.x, -q.y, -q.z, q.w);
}

vec3 SampleHemisphere(vec2 u) {
	float a = sqrt(u.x);
	float b = TWO_PI * u.y;
	return vec3(
		a * cos(b),
		a * sin(b),
		sqrt(1.0f - u.x));
}


vec3 BaseColorToSpecularF0(vec3 BaseColor, float Metalness) {
	return mix(vec3(F0_DIELECTRICS, F0_DIELECTRICS, F0_DIELECTRICS), BaseColor, Metalness);
}

vec3 BaseColorToDiffuseReflectance(vec3 BaseColor, float Metalness)
{
	return BaseColor * (1.0f - Metalness);
}

vec3 EvalFresnelSchlick(vec3 F0, float F90, float NdotS)
{
	return F0 + (F90 - F0) * pow(1.0f - NdotS, 5.0f);
}

float Luminance(vec3 RGB)
{
	return dot(RGB, vec3(0.2126f, 0.7152f, 0.0722f));
}
float ShadowedF90(vec3 F0) {
	// This scaler value is somewhat arbitrary, Schuler used 60 in his article. In here, we derive it from F0_DIELECTRICS so
	// that it takes effect for any reflectance lower than least reflective dielectrics
	//const float t = 60.0f;
	const float t = (1.0f / F0_DIELECTRICS);
	return min(1.0f, t * Luminance(F0));
}


vec3 SampleSpecularHalfVector(vec3 Ve, vec2 Alpha2D, vec2 u) {

	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(Alpha2D.x * Ve.x, Alpha2D.y * Ve.y, Ve.z));

	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float LenSq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = LenSq > 0.0f ? vec3(-Vh.y, Vh.x, 0.0f) * inversesqrt(LenSq) : vec3(1.0f, 0.0f, 0.0f);
	vec3 T2 = cross(Vh, T1);

	// Section 4.2: parameterization of the projected area
	float r = sqrt(u.x);
	float Phi = TWO_PI * u.y;
	float t1 = r * cos(Phi);
	float t2 = r * sin(Phi);
	float s = 0.5f * (1.0f + Vh.z);
	t2 = mix(sqrt(1.0f - t1 * t1), t2, s);

	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

	// Section 3.4: transforming the normal back to the ellipsoid configuration
	return normalize(vec3(Alpha2D.x * Nh.x, Alpha2D.y * Nh.y, max(0.0f, Nh.z)));
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

float GGX_D(float alphaSquared, float NdotH) {
	float b = ((alphaSquared - 1.0f) * NdotH * NdotH + 1.0f);
	return alphaSquared / (PI * b * b);
}


vec3 sampleSpecularMicrofacet(vec3 Vlocal, float Alpha, float AlphaSquared, vec3 SpecularF0, vec2 u, inout vec3 Weight) {
	vec3 Hlocal = SampleSpecularHalfVector(Vlocal, vec2(Alpha, Alpha), u);
	vec3 Llocal = reflect(-Vlocal, Hlocal);

	// Note: HdotL is same as HdotV here
	// Clamp dot products here to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
	float HdotL = max(0.00001f, min(1.0f, dot(Hlocal, Llocal)));
	const vec3 Nlocal = vec3(0.0f, 0.0f, 1.0f);
	float NdotL = max(0.00001f, min(1.0f, dot(Nlocal, Llocal)));
	float NdotV = max(0.00001f, min(1.0f, dot(Nlocal, Vlocal)));
	vec3 F = EvalFresnelSchlick(SpecularF0, ShadowedF90(SpecularF0), HdotL);

	// Calculate Weight of the sample specific for selected sampling method 
	// (this is microfacet BRDF divided by PDF of sampling method - notice how most terms cancel out)
	Weight = F * DistributionGGX_GeometrySmith(Alpha, AlphaSquared, NdotL, NdotV);

	return Llocal;
}

// This is an entry point for evaluation of all other BRDFs based on selected configuration (for indirect light)
bool SampleSpecularBRDF(vec2 u, vec3 N, vec3 V, inout vec3 RayDirection, inout vec3 SampleWeight) {
	if (dot(N, V) <= 0.0f) return false;

	vec3 T, B;
	ONB(N, T, B);
	vec3 Vlocal = vec3(dot(V, T), dot(V, B), dot(V, N));
	const vec3 Nlocal = vec3(0.0f, 0.0f, 1.0f);

	vec3 RayDirectionLocal = vec3(0.0f, 0.0f, 0.0f);

	float Alpha = RayPayload.Roughness * RayPayload.Roughness;
	float AlphaSquared = Alpha * Alpha;
	vec3 SpecularF0 = BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic);

	//Sample a vector in the specular lobe
	RayDirectionLocal = sampleSpecularMicrofacet(Vlocal, Alpha, AlphaSquared, SpecularF0, u, SampleWeight);

	if (Luminance(SampleWeight) == 0.0f) return false;

	RayDirection =  RayDirectionLocal.x * T + RayDirectionLocal.y * B + RayDirectionLocal.z * N;
	return true;
}

bool SampleDiffuseBRDF(vec2 u, vec3 N, vec3 V, inout vec3 RayDirection, inout vec3 SampleWeight) {

	// Ignore incident ray coming from "below" the hemisphere
	if (dot(N, V) <= 0.0f) return false;

	vec3 T, B;
	ONB(N, T, B);
	
	//Sample a vector in hemisphere
	vec3 RayDirectionLocal = SampleHemisphere(u);
	
	SampleWeight = BaseColorToDiffuseReflectance(RayPayload.Color, RayPayload.Metallic);

	if (Luminance(SampleWeight) == 0.0f) return false;
	
	//Transform back
	RayDirection =  RayDirectionLocal.x * T + RayDirectionLocal.y * B + RayDirectionLocal.z * N;
	
	return true;
}

float GetBRDFProbability(rayPayload RayPayload, vec3 V, vec3 ShadingNormal) {
	//We compute the fresnel using the shading normal as we don't have H yet.
	float SpecularF0 = Luminance(BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic));
	float Fresnel = clamp(Luminance(EvalFresnelSchlick(vec3(SpecularF0), ShadowedF90(vec3(SpecularF0)), max(0.0f, dot(V, ShadingNormal)))), 0, 1);

	float DiffuseReflectance = Luminance(BaseColorToDiffuseReflectance(RayPayload.Color, RayPayload.Metallic));
	
	// Approximate relative contribution of BRDFs using the Fresnel term
	float Specular = Fresnel;
	float Diffuse = DiffuseReflectance * (1.0f - Fresnel); 
	// Return probability of selecting Specular BRDF over Diffuse BRDF
	float p = (Specular / max(0.0001f, (Specular + Diffuse)));

	// Clamp probability to avoid undersampling of less prominent BRDF
	return clamp(p, 0.1f, 0.9f);
}




bool SampleIndirectCombinedBRDF(vec2 u, vec3 shadingNormal, vec3 geometryNormal, vec3 V, rayPayload RayPayload, int brdfType, inout vec3 rayDirection, inout vec3 sampleWeight) {

	// Ignore incident ray coming from "below" the hemisphere
	if (dot(shadingNormal, V) <= 0.0f) return false;

	// Transform view direction into local space of our sampling routines 
	// (local space is oriented so that its positive Z axis points along the shading normal)
	vec4 qRotationToZ = getRotationToZAxis(shadingNormal);
	vec3 Vlocal = rotatePoint(qRotationToZ, V);
	const vec3 Nlocal = vec3(0.0f, 0.0f, 1.0f);

	vec3 rayDirectionLocal = vec3(0.0f, 0.0f, 0.0f);

	if (brdfType == DIFFUSE_TYPE) {

		// Sample diffuse ray using cosine-weighted hemisphere sampling 
		rayDirectionLocal = SampleHemisphere(u);
		// const BrdfData data = prepareBRDFData(Nlocal, rayDirectionLocal, Vlocal, material);

        vec3 DiffuseReflectance = BaseColorToDiffuseReflectance(RayPayload.Color, RayPayload.Metallic);
        float Alpha = 	RayPayload.Roughness * RayPayload.Roughness;
	    vec3 SpecularF0 = BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic);

		// Function 'diffuseTerm' is predivided by PDF of sampling the cosine weighted hemisphere
		sampleWeight = DiffuseReflectance;
	
		// Sample a half-vector of specular BRDF. Note that we're reusing random variable 'u' here, but correctly it should be an new independent random number
		vec3 Hspecular = SampleSpecularHalfVector(Vlocal, vec2(Alpha, Alpha), u);

		// Clamp HdotL to small value to prevent numerical instability. Assume that rays incident from below the hemisphere have been filtered
		float VdotH = max(0.00001f, min(1.0f, dot(Vlocal, Hspecular)));
		sampleWeight *= (vec3(1.0f, 1.0f, 1.0f) - EvalFresnelSchlick(SpecularF0, ShadowedF90(SpecularF0), VdotH));

	} else if (brdfType == SPECULAR_TYPE) {
		// const BrdfData data = prepareBRDFData(Nlocal, vec3(0.0f, 0.0f, 1.0f) /* unused L vector */, Vlocal, material);
        float Alpha = 	RayPayload.Roughness * RayPayload.Roughness;
        float AlphaSquared = Alpha * Alpha;
	    vec3 SpecularF0 = BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic);

        rayDirectionLocal = sampleSpecularMicrofacet(Vlocal, Alpha, AlphaSquared, SpecularF0, u, sampleWeight);
	}

	// Prevent tracing direction with no contribution
	if (Luminance(sampleWeight) == 0.0f) return false;

	// Transform sampled direction Llocal back to V vector space
	rayDirection = normalize(rotatePoint(invertRotation(qRotationToZ), rayDirectionLocal));

	// Prevent tracing direction "under" the hemisphere (behind the triangle)
	if (dot(geometryNormal, rayDirection) <= 0.0f) return false;

	return true;
}



float Smith_G2(float alphaSquared, float NdotL, float NdotV) {
	float a = NdotV * sqrt(alphaSquared + NdotL * (NdotL - alphaSquared * NdotL));
	float b = NdotL * sqrt(alphaSquared + NdotV * (NdotV - alphaSquared * NdotV));
	return 0.5f / (a + b);
}

vec3 EvalMicrofacet(vec3 L, vec3 V, rayPayload RayPayload) {
	float Alpha = RayPayload.Roughness * RayPayload.Roughness;
	float AlphaSquared = Alpha * Alpha;
    vec3 H = normalize(L + V);
    float NdotH = clamp(dot(RayPayload.Normal, H), 0, 1);
    float NdotL = dot(RayPayload.Normal, L);
    float NdotV = dot(RayPayload.Normal, V);
    float LdotH = clamp(dot(L, H), 0, 1);
    vec3 SpecularF0 = BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic);
    vec3 F = EvalFresnelSchlick(SpecularF0, ShadowedF90(SpecularF0), LdotH);

	float D = GGX_D(max(0.00001f, AlphaSquared), NdotH);
	float G2 = Smith_G2(AlphaSquared, NdotL, NdotV);
	//vec3 F = evalFresnel(data.specularF0, shadowedF90(data.specularF0), data.VdotH); //< Unused, F is precomputed already

	return F * (G2 * D * NdotL);
}


vec3 EvalDiffuse(rayPayload RayPayload, float NdotL) {
    vec3 DiffuseReflectance = BaseColorToDiffuseReflectance(RayPayload.Color, RayPayload.Metallic);
	return DiffuseReflectance * (ONE_OVER_PI * NdotL);
}

vec3 EvalCombinedBRDF(vec3 N, vec3 L, vec3 V, vec3 Color, float Metallic) {
    float NdotL = dot(N, L);
	float NdotV = dot(N, V);
    vec3 H = normalize(L + V);
    float LdotH = clamp(dot(L, H), 0, 1);
    bool Vbackfacing = (NdotV <= 0.0f);
	bool Lbackfacing = (NdotL <= 0.0f);
    
    vec3 SpecularF0 = BaseColorToSpecularF0(RayPayload.Color, RayPayload.Metallic);
    vec3 F = EvalFresnelSchlick(SpecularF0, ShadowedF90(SpecularF0), LdotH);
    

    // Ignore V and L rays "below" the hemisphere
	if (Vbackfacing || Lbackfacing) return vec3(0.0f, 0.0f, 0.0f);

	// Eval specular and diffuse BRDFs
	vec3 Specular = EvalMicrofacet(L, V, RayPayload);
	vec3 Diffuse = EvalDiffuse(RayPayload, NdotL);

	// Combine Specular and Diffuse layers
	// Specular is already multiplied by F, just attenuate Diffuse
	return (vec3(1.0f, 1.0f, 1.0f) - F) * Diffuse + Specular;
}
