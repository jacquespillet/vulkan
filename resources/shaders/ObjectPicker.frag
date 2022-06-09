#version 450


layout (location = 0) out uint outputColor;

layout (set=1, binding = 0) uniform instance 
{
	mat4 Model;
	mat4 Normal;
	float Selected;
	float InstanceID;
    vec2 Padding;
} InstanceUBO;
void main() 
{
    outputColor = uint(InstanceUBO.InstanceID);
}