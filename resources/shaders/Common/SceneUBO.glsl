struct sceneUbo
{
	mat4 Projection;
	mat4 Model;
	mat4 View;
	mat4 InvView;
	mat4 InvProjection;
	
	vec3 CameraPosition;
	float Exposure;
	
	vec3 BackgroundColor;
	float BackgroundType;
	
	float BackgroundIntensity;
	vec3 Padding;
};