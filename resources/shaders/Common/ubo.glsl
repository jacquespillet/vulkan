struct Ubo {
	int vertexSize;
	int currentSamplesCount;
	int samplesPerFrame;
	int rayBounces;

	int MaxSamples;
	ivec3 Padding;
};
