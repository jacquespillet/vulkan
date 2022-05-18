#version 450

layout (set=0, binding = 1) uniform samplerCube Cubemap;

layout (location = 0) in vec3 inUV;
layout (location = 1) in vec3 inPosition;

layout (location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void main() 
{
    // the sample direction equals the hemisphere's orientation 
    vec3 Normal = normalize(inPosition);
    
    vec3 Irradiance = vec3(0.0);  

    vec3 Up    = vec3(0.0, 1.0, 0.0);
    vec3 Right = normalize(cross(Up, Normal));
    Up         = normalize(cross(Normal, Right));

    float SampleDelta = 0.025;
    float SampleCount = 0.0; 
    for(float Phi = 0.0; Phi < 2.0 * PI; Phi += SampleDelta)
    {
        for(float Theta = 0.0; Theta < 0.5 * PI; Theta += SampleDelta)
        {
            vec3 TangentSample = vec3(sin(Theta) * cos(Phi),  sin(Theta) * sin(Phi), cos(Theta));
            vec3 WorldSample = TangentSample.x * Right + TangentSample.y * Up + TangentSample.z * Normal; 
            Irradiance += min(vec3(300,300,300), texture(Cubemap, WorldSample).rgb) * cos(Theta) * sin(Theta);
            SampleCount++;
        }
    }
    Irradiance = PI * Irradiance * (1.0 / float(SampleCount));  
  
    outColor = vec4(Irradiance, 1.0);    
}