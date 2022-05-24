layout (set=SCENE_UBO_SET_ID, binding = SCENE_UBO_BINDING) uniform UBO 
{
	mat4 Projection;
	mat4 Model;
	mat4 View;
	mat4 InvView;
	mat4 InvProjection;
	vec3 CameraPosition;
	float Exposure;
} SceneUbo;
