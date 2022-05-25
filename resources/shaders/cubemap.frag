#version 450



#include "Common/SceneUBO.glsl"
layout (set=0, binding = 0) uniform UBO 
{
	sceneUbo Data;
} SceneUbo;


layout (set=1, binding = 1) uniform samplerCube Cubemap;
layout (set=1, binding = 2) uniform samplerCube IrradianceMap;
layout (set=1, binding = 3) uniform samplerCube PrefilteredEnv;

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec4 outColor;

#include "Common/Tonemapping.glsl"

void main() 
{
    if(SceneUbo.Data.BackgroundType==0)
    {
        vec3 Color = texture(Cubemap, normalize(inPosition)).xyz; 
        outColor = vec4(toneMap(Color, SceneUbo.Data.Exposure), 1);
    }
    else
    {
        outColor = vec4(SceneUbo.Data.BackgroundColor, 1) ;
    }
    outColor *= SceneUbo.Data.BackgroundIntensity;
}