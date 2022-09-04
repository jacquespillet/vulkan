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

// This is an entry point for evaluation of all other BRDFs based on selected configuration (for direct light)
vec3 EvalCombinedBRDF(vec3 N, vec3 L, vec3 V, vec3 Color, float Metallic) {
	vec3 diffuse = BaseColorToDiffuseReflectance(Color, Metallic);
	return diffuse;
}