struct rayPayload {
	vec3 Color;
	float Distance;
	vec3 ScatterDir;
	bool DoScatter;
	uint RandomState;
	uint Depth;
};