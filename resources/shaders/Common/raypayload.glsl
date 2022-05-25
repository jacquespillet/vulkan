struct rayPayload {
	vec3 Color;
	float Distance;
	vec3 Emission;
	vec3 ScatterDir;
	vec3 Normal;
	bool DoScatter;
	uint RandomState;
	uint Depth;
	float Roughness;
	float Metallic;
};