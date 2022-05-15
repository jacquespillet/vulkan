#version 450

layout (set=1, binding = 0) uniform sampler2D samplerColor;
layout (set=1, binding = 1) uniform sampler2D samplerSpecular;
layout (set=1, binding = 2) uniform sampler2D samplerNormal;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inWorldPos;
layout (location = 4) in vec3 inTangent;
layout (location = 5) in vec3 inBitangent;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const float NEAR_PLANE = 0.1f;
layout (constant_id = 1) const float FAR_PLANE = 64.0f;
layout (constant_id = 2) const int ENABLE_DISCARD = 0;


void main() 
{
	outColor = texture(samplerColor, inUV);
}