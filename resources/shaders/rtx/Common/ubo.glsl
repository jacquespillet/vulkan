struct Ubo {
	int vertexSize;
	int currentSamplesCount;
	int samplesPerFrame;
	int rayBounces;
};

struct sceneUBO {
	mat4 Projection;
	mat4 Model;
	mat4 View;
	mat4 InvView;
	mat4 InvProjection;
	vec3 CameraPosition;
	float Exposure;
};