struct Ubo {
	int vertexSize;
	int currentSamplesCount;
	int samplesPerFrame;
	int rayBounces;

	int MaxSamples;
	int ShouldAccumulate;
	ivec2 Padding;
};
