struct Ubo {
	mat4 viewInverse;
	mat4 projInverse;
	int vertexSize;
	int currentSamplesCount;
	int samplesPerFrame;
	int rayBounces;
	float Exposure;
	vec3 padding;
};