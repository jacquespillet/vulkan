#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D samplerposition;
layout (binding = 2) uniform sampler2D samplerNormal;
layout (binding = 3) uniform usampler2D samplerAlbedo;
layout (binding = 4) uniform sampler2D samplerSSAO;

layout (constant_id = 0) const int SSAO_ENABLED = 1;
layout (constant_id = 1) const float AMBIENT_FACTOR = 0.0;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragcolor;



void main() 
{
	// Get G-Buffer values
	vec3 fragPos = texture(samplerposition, inUV).rgb;
	vec3 normal = texture(samplerNormal, inUV).rgb * 2.0 - 1.0;

	// unpack
	ivec2 texDim = textureSize(samplerAlbedo, 0);
	//uvec4 albedo = texture(samplerAlbedo, inUV.st, 0);
	uvec4 albedo = texelFetch(samplerAlbedo, ivec2(inUV.st * texDim ), 0);

	vec4 color;
	color.rg = unpackHalf2x16(albedo.r);
	color.ba = unpackHalf2x16(albedo.g);
	vec4 spec;
	spec.rg = unpackHalf2x16(albedo.b);	

	vec3 ambient = color.rgb * AMBIENT_FACTOR;	
	vec3 fragcolor  = ambient;
	
	fragcolor = color.rgb;

	outFragcolor = vec4(fragcolor, 1.0);	
}