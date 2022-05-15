@echo off

REM GLFW
set glfwInclude= ..\ext\glfw\include
set glfwLib= ..\ext\glfw\lib\glfw3.lib
REM --------------------


REM VULKAN
set vulkanInclude=%VULKAN_SDK%\Include
set vulkanLib= %VULKAN_SDK%\Lib\vulkan-1.lib
REM --------------------

REM ASSIMP
set assimpLibDir= ..\ext\assimp\lib
set assimpLib=..\ext\assimp\lib\assimp-vc141-mt.lib
set assimpIncludes=..\ext\assimp\include
set assimpBin= ..\ext\assimp\bin\assimp-vc141-mt.dll
REM --------------------


%VULKAN_SDK%/Bin/glslc.exe resources/shaders/mrt.vert -o resources/shaders/mrt.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/mrt.frag -o resources/shaders/mrt.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/composition.vert -o resources/shaders/composition.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/composition.frag -o resources/shaders/composition.frag.spv

%VULKAN_SDK%/Bin/glslc.exe resources/shaders/forward.vert -o resources/shaders/forward.vert.spv
%VULKAN_SDK%/Bin/glslc.exe resources/shaders/forward.frag -o resources/shaders/forward.frag.spv

set compilerFlags=  -MP -MT -nologo -EHa- -Od -Oi -W4 -Z7 -EHsc -wd4201 -wd4310 -wd4100  /I ../src /I ..\ext\glm /I ..\ext\gli  /I %glfwInclude% /I %vulkanInclude% /I %assimpIncludes%
set linkerFlags=  -opt:ref Gdi32.lib Shell32.lib User32.lib opengl32.lib %glfwLib%  %vulkanLib% %assimpLib%

set srcFiles= ..\src\App.cpp ..\src\Resources.cpp ..\src\Device.cpp ..\src\Tools.cpp 
set srcFiles= %srcFiles%  ..\src\Scene.cpp ..\src\Debug.cpp ..\src\Buffer.cpp ..\src\Shader.cpp 
set srcFiles= %srcFiles%  ..\src\Camera.cpp ..\src\Renderer.cpp  ..\src\Renderers\DeferredRenderer.cpp  ..\src\Renderers\ForwardRenderer.cpp 

IF NOT EXIST .\build mkdir .\build
pushd .\build
cl.exe %compilerFlags% ..\Main.cpp  %srcFiles%  /link %linkerFlags%

copy %assimpBin% .\ >nul

@REM IF NOT EXIST .\shaders mkdir .\shaders
@REM xcopy ..\shaders .\shaders\ /s /e /Y  >nul

IF NOT EXIST .\resources mkdir .\resources
xcopy ..\resources .\resources\  /s /e /Y /D >nul

popd