#version 450



#define SCENE_UBO_SET_ID 0
#define SCENE_UBO_BINDING 0
#include "SceneUBO.glsl"

layout (set=1, binding = 1) uniform samplerCube Cubemap;
layout (set=1, binding = 2) uniform samplerCube IrradianceMap;
layout (set=1, binding = 3) uniform samplerCube PrefilteredEnv;

layout (location = 0) in vec3 inPosition;

layout (location = 0) out vec4 outColor;

#include "Tonemapping.glsl"

void main() 
{
    if(SceneUbo.BackgroundType==0)
    {
        vec3 Color = texture(Cubemap, normalize(inPosition)).xyz; 
        outColor = vec4(toneMap(Color, SceneUbo.Exposure), 1);
    }
    else
    {
        outColor = vec4(SceneUbo.BackgroundColor, 1);
    }
}