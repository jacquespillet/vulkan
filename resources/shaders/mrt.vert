#version 450


layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inNormal;
layout (location = 2) in vec4 inTangent;


layout (set=0, binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	mat4 view;
	vec2 viewport;
} ubo;

layout (set=2, binding = 0) uniform instance 
{
	mat4 model;
} InstanceUBO;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec3 outWorldPos;
layout (location = 4) out vec3 outTangent;
layout (location = 5) out vec3 outBitangent;

void main() 
{
	vec4 pos = inPos;
	gl_Position = ubo.projection * ubo.view * InstanceUBO.model * pos;
	
	outUV = vec2(inPos.w, inNormal.w);
	outUV.t = 1.0 - outUV.t;

	// Vertex position in world space
	outWorldPos = inPos.xyz;

	outWorldPos = vec3(ubo.view * InstanceUBO.model * inPos);

	// GL to Vulkan coord space
	//outWorldPos.y = -outWorldPos.y;
	
	// Normal in world space
	mat3 mNormal = transpose(inverse(mat3(InstanceUBO.model)));
	outNormal = mNormal * normalize(inNormal.xyz);	

	// Normal in view space
	mat3 normalMatrix = transpose(inverse(mat3(ubo.view * InstanceUBO.model)));
	outNormal = normalMatrix * inNormal.xyz;

	outTangent = mNormal * normalize(inTangent.xyz);
}
