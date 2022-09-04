@echo off

set debug=1

REM GLFW
set glfwInclude= ..\ext\glfw\include
set glfwLib= ..\ext\glfw\lib\glfw3.lib
REM --------------------

REM tinygltf
set tinygltfInclude= ..\ext\tinygltf
REM --------------------

REM stb
set stbInclude= ..\ext\stb
REM --------------------

REM oidn
set oidnLib=..\ext\oidn\lib\OpenImageDenoise.lib
set oidnIncludes=..\ext\oidn\include
set oidnBin= ..\ext\oidn\lib\OpenImageDenoise.dll 
set tbbBin= ..\ext\oidn\lib\tbb.dll 
REM --------------------


REM VULKAN
set vulkanInclude=%VULKAN_SDK%\Include
set vulkanLib= %VULKAN_SDK%\Lib\vulkan-1.lib
REM --------------------

REM IMGUI
set imguiInclude=..\ext\imgui
@REM set imguiSrc=%imguiInclude%\imgui_impl_vulkan.cpp %imguiInclude%\imgui_impl_glfw.cpp %imguiInclude%\imgui.cpp %imguiInclude%\imgui_demo.cpp %imguiInclude%\imgui_draw.cpp %imguiInclude%\imgui_widgets.cpp %imguiInclude%\imgui_tables.cpp  %imguiInclude%\ImGuizmo.cpp
set imguiObj=%imguiInclude%\build\imgui_impl_vulkan.obj %imguiInclude%\build\imgui_impl_glfw.obj %imguiInclude%\build\imgui.obj %imguiInclude%\build\imgui_demo.obj %imguiInclude%\build\imgui_draw.obj %imguiInclude%\build\imgui_widgets.obj %imguiInclude%\build\imgui_tables.obj  %imguiInclude%\build\ImGuizmo.obj
REM --------------------

REM ASSIMP
set assimpLibDir= ..\ext\assimp\lib
set assimpLib=..\ext\assimp\lib\assimp-vc141-mt.lib
set assimpIncludes=..\ext\assimp\include
set assimpBin= ..\ext\assimp\bin\assimp-vc141-mt.dll
REM --------------------


%VULKAN_SDK%/Bin/glslc.exe resources/shaders/mrt.vert -o resources/shaders/spv/mrt.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/mrt.frag -o resources/shaders/spv/mrt.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/composition.vert -o resources/shaders/spv/composition.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/composition.frag -o resources/shaders/spv/composition.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/forward.vert -o resources/shaders/spv/forward.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/forward.frag -o resources/shaders/spv/forward.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildCubemap.vert -o resources/shaders/spv/BuildCubemap.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildCubemap.frag -o resources/shaders/spv/BuildCubemap.frag.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildIrradianceMap.vert -o resources/shaders/spv/BuildIrradianceMap.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildIrradianceMap.frag -o resources/shaders/spv/BuildIrradianceMap.frag.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildPrefilterEnvMap.vert -o resources/shaders/spv/BuildPrefilterEnvMap.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildPrefilterEnvMap.frag -o resources/shaders/spv/BuildPrefilterEnvMap.frag.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildBRDFLUT.vert -o resources/shaders/spv/BuildBRDFLUT.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/BuildBRDFLUT.frag -o resources/shaders/spv/BuildBRDFLUT.frag.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/cubemap.vert -o resources/shaders/spv/cubemap.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/cubemap.frag -o resources/shaders/spv/cubemap.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/fullscreen.vert -o resources/shaders/spv/fullscreen.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/ssao.frag -o resources/shaders/spv/ssao.frag.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/blur.frag -o resources/shaders/spv/blur.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/ui.vert -o resources/shaders/spv/ui.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/ui.frag -o resources/shaders/spv/ui.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/ObjectPicker.vert -o resources/shaders/spv/ObjectPicker.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/ObjectPicker.frag -o resources/shaders/spv/ObjectPicker.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rtx/anyhit.rahit -o resources/shaders/spv/anyhit.rahit.spv --target-spv=spv1.4 -g
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rtx/closesthit.rchit -o resources/shaders/spv/closesthit.rchit.spv --target-spv=spv1.4 -g
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rtx/raygen.rgen -o resources/shaders/spv/raygen.rgen.spv --target-spv=spv1.4 -g
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rtx/miss.rmiss -o resources/shaders/spv/miss.rmiss.spv --target-spv=spv1.4 -g
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rtx/missShadow.rmiss -o resources/shaders/spv/missShadow.rmiss.spv --target-spv=spv1.4 -g

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/rayTracedShadows.comp -o resources/shaders/spv/rayTracedShadows.comp.spv --target-spv=spv1.4  --target-env=vulkan1.2 
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/svgfReprojection.comp -o resources/shaders/spv/svgfReprojection.comp.spv --target-spv=spv1.4  --target-env=vulkan1.2 
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/svgfVariance.comp -o resources/shaders/spv/svgfVariance.comp.spv --target-spv=spv1.4  --target-env=vulkan1.2 
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/hybridComposition.frag -o resources/shaders/spv/hybridComposition.frag.spv --target-spv=spv1.4  --target-env=vulkan1.2 
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/hybridGBuffer.frag -o resources/shaders/spv/hybridGBuffer.frag.spv --target-spv=spv1.4  --target-env=vulkan1.2 

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/pathTracePreview.comp -o resources/shaders/spv/pathTracePreview.comp.spv --target-spv=spv1.4  --target-env=vulkan1.2 

set debugFlag= -O2
if %debug%==1 (
    set debugFlag= -Od /D_DEBUG_
)

set compilerFlags=  -MP -MT -nologo -EHa- %debugFlag% -Oi -W4 -Z7 -EHsc -wd4201 -wd4310 -wd4100 /openmp  /I ../src /I ..\ext\glm /I ..\ext\gli  /I %glfwInclude% /I %vulkanInclude% /I %assimpIncludes% /I %imguiInclude% /I %stbInclude%  /I %tinygltfInclude% /I %oidnIncludes% 
set linkerFlags=  -opt:ref Gdi32.lib Shell32.lib User32.lib opengl32.lib %glfwLib%  %vulkanLib% %assimpLib% %oidnLib% %imguiObj%

set srcFiles= ..\src\App.cpp ..\src\Resources.cpp ..\src\Device.cpp ..\src\Tools.cpp 
set srcFiles= %srcFiles%  ..\src\Scene.cpp ..\src\Debug.cpp ..\src\Buffer.cpp ..\src\Shader.cpp  ..\src\Image.cpp  ..\src\ThreadPool.cpp 
set srcFiles= %srcFiles%  ..\src\Camera.cpp ..\src\Renderer.cpp  ..\src\Renderers\DeferredRenderer.cpp  ..\src\Renderers\ForwardRenderer.cpp ..\src\Renderers\PathTraceRTXRenderer.cpp 
set srcFiles= %srcFiles%   ..\src\ImGuiHelper.cpp ..\src\AssimpImporter.cpp ..\src\GLTFImporter.cpp  ..\src\ObjectPicker.cpp  ..\src\Framebuffer.cpp  ..\src\bvh.cpp 
set srcFiles= %srcFiles%  ..\src\TextureLoader.cpp ..\src\RayTracingHelper.cpp ..\src\Renderers\HybridRenderer.cpp  ..\src\Renderers\PathTraceCPURenderer.cpp  ..\src\Renderers\PathTraceComputeRenderer.cpp  ..\src\Renderers\brdf.cpp  
set srcFiles= %srcFiles%  ..\src\Renderers/RasterizerRenderer.cpp 
@REM set srcFiles= %srcFiles%  %imguiSrc%
 
IF NOT EXIST .\build mkdir .\build
pushd .\build
cl.exe %compilerFlags% ..\Main.cpp  %srcFiles%  /link %linkerFlags%

copy %oidnBin% .\ >nul
copy %tbbBin% .\ >nul
copy %assimpBin% .\ >nul

@REM IF NOT EXIST .\shaders mkdir .\shaders
@REM xcopy ..\shaders .\shaders\ /s /e /Y  >nul

IF NOT EXIST .\resources mkdir .\resources
xcopy ..\resources .\resources\  /s /e /Y /D >nul

popd