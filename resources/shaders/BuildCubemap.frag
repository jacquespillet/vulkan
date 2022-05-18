#version 450

layout (set=0, binding = 1) uniform sampler2D Panorama;

layout (location = 0) in vec3 inUV;
layout (location = 1) in vec3 inPosition;

layout (location = 0) out vec4 outColor;


const vec2 InvAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 UV = vec2(atan(v.z, v.x), asin(v.y));
    UV *= InvAtan;
    UV += 0.5;
    return UV;
}

void main() 
{
	// outColor = vec4(1,0, 0, 1);
	vec2 UV = SampleSphericalMap(normalize(inPosition));
    vec3 Color = texture(Panorama, UV).rgb;
    outColor = vec4(Color, 1.0);
}