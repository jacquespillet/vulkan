
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
