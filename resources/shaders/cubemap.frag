#version 450



#include "Common/SceneUBO.glsl"
#include "Common/Defines.glsl"
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
    if(SceneUbo.Data.BackgroundType==BACKGROUND_TYPE_CUBEMAP)
    {
        vec3 Color = texture(Cubemap, normalize(inPosition)).xyz; 
        outColor = vec4(toneMap(Color, SceneUbo.Data.Exposure), 1);
    }
    else if(SceneUbo.Data.BackgroundType==BACKGROUND_TYPE_COLOR)
    {
        outColor = vec4(SceneUbo.Data.BackgroundColor, 1) ;
    }
    else if(SceneUbo.Data.BackgroundType==BACKGROUND_TYPE_DIRLIGHT)
    {
        outColor = vec4(0,0,0,0);
    }
    outColor *= SceneUbo.Data.BackgroundIntensity;
}