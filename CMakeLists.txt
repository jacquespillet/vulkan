project(vulkanApp)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/install)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

#glm
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm)


# Glfw
set(GLFW_INSTALL FALSE)
add_subdirectory(vendor/glfw)
include_directories(vendor/glfw/include)

# STB
set(STB_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb)
include_directories(${STB_INC_DIR})

# GLI
set(GLI_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/gli)
include_directories(${GLI_INC_DIR})

# imgui
set(IMGUI_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo/)
set(IMGUI_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_demo.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_glfw.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_draw.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_widgets.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/imgui_tables.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends/imgui_impl_opengl3.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ImGuizmo/ImGuizmo.cpp

)
include_directories(${IMGUI_INC_DIR})

# ASSIMP
set(ASSIMP_INSTALL_PDB OFF)
add_subdirectory(vendor/assimp)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/vendor/assimp/include")
include_directories(vendor/assimp/include)

# # TBB
set(TBB_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tbb)
set(TBB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tbb/include)

if(NOT TBB_FOUND)
    add_subdirectory(vendor/tbb)
    set(TBB_FOUND ON)
    add_library(TBB::tbb ALIAS tbb)
    export(TARGETS tbb NAMESPACE TBB:: FILE TBBTargets.cmake)
export(PACKAGE TBB)
endif()

# # # OIDN
add_subdirectory(vendor/oidn)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/oidn/include")

# TinyGLTF
set(TINYGLTF_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinygltf)
include_directories(${TINYGLTF_INC_DIR})

# Vulkan
set(VULKAN_DIR "$ENV{VULKAN_SDK}")
link_directories("${VULKAN_DIR}/Lib")
include_directories("${VULKAN_DIR}/Include")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

set (sourceFiles
    Main.cpp
    src/App.cpp 
    src/Resources.cpp 
    src/Device.cpp 
    src/Tools.cpp 
    src/Scene.cpp 
    src/Debug.cpp 
    src/Buffer.cpp 
    src/Shader.cpp 
    src/Image.cpp 
    src/ThreadPool.cpp 
    src/Camera.cpp 
    src/Renderer.cpp 
    src/Renderers/DeferredRenderer.cpp 
    src/Renderers/ForwardRenderer.cpp 
    src/Renderers/PathTraceRTXRenderer.cpp 
    src/ImGuiHelper.cpp 
    src/AssimpImporter.cpp 
    src/GLTFImporter.cpp 
    src/ObjectPicker.cpp 
    src/Framebuffer.cpp 
    src/bvh.cpp 
    src/TextureLoader.cpp 
    src/RayTracingHelper.cpp 
    src/Renderers/HybridRenderer.cpp 
    src/Renderers/PathTraceCPURenderer.cpp 
    src/Renderers/PathTraceComputeRenderer.cpp 
    src/Renderers/brdf.cpp 
    src/Renderers/RasterizerRenderer.cpp 
    ${IMGUI_SOURCE}
)

add_executable(VulkanApp ${sourceFiles})

target_link_libraries(VulkanApp vulkan-1 glfw OpenImageDenoise assimp)

install(TARGETS VulkanApp RUNTIME)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/ DESTINATION bin/resources)