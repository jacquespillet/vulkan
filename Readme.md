# Vulkan

This is a project I started to learn more about vulkan API.
I started off just creating a regular glTF model viewer, but kept adding features, and it became more of a playground for exploration on rendering.

## Requirements
Visual Studio (Tested only on Visual Studio 2019)
NVidia GPU (for RTX)

## Build Instructions
```
### Clone the repo and checkout to the latest branch
git clone --recursive https://github.com/jacquespillet/vulkan.git
cd vulkan

### Generate the solution
mkdir build
cd build
cmake ../

### Build
cd ..
Build.bat
```
First build may take a while because it's going to build all the dependencies with the project.

## Usage
```
VulkanApp.exe "path/to/your/model.gltf"
```
you can add another parameter to set the global scale of the scene, like so : 
```
VulkanApp.exe "path/to/your/model.gltf" 0.01
```

## Features

### Renderers
It has 6 different "renderers" available : 

* Forward : 
The simplest one, it just does forward rendering using the vulkan graphics pipeline. It's using a pbr material shader. 
It also supports image based lighting, and directional lighting.

* Deferred : 
Simple Deferred rendering pipeline, just to understand how offscreen render targets work in vulkan
It also supports image based lighting, and directional lighting.

* Path Tracer RTX
A path tracer that uses the hardware acceleration features on new NVidia RTX gpus. 
It can also use Open Image Denoise to denoise the renders.
It's a simple path tracer, but still able to produce some good looking images!

* Path Trace CPU
Well, that's the same path tracer as the previous one, but using the GPU.

As it cannot benefit from hardware accelerated ray tracing, it's using a custom BVH based on the great [BVH series](https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/) by Jacco Bikker.

* Rasterizer CPU

As the title says, it's a rasterizer that runs on the cpu that renders the scene. It really has a limited set of features : Only draws the triangles with a simple lighting algorithm.

It is based on [tinyRenderer](https://www.google.com/search?client=firefox-b-d&q=tiny+rasterizer)

* Path Tracer Compute

That's a path tracer that runs in a compute shader, and that doesn't use the hardware acceleration for ray tracing.

It also uses the same BVH structure as the cpu path tracer, and produces the same results as the other 2 path tracers.

### Material system

The materials can be edited live in the editor

### Object Picking

Pixel perfect object selection, and ability to move objects in the scene using (ImGuizmo)[https://github.com/CedricGuillemet/ImGuizmo]


## Gallery

## List of dependencies
* [Assimp]() for 3D Model loading
* [GLFW]() for windowing system
* [gli]() for image loading and manipulation
* [glm]() for vector maths
* [imgui]() for user inferface
* [oidn]() for denoising path tracing output
* [stb]() for image loading
* [tbb]() needed by oidn
* [tinygltf]() for loading glTF scenes

## Related links
[Vulkan Samples by Khronos Group](https://github.com/KhronosGroup/Vulkan-Samples)
[Vulkan Samples by Sascha Willems](https://github.com/SaschaWillems/Vulkan)
[Vulkan Tutorial](https://vulkan-tutorial.com/)
[Vulkan Guide](https://vkguide.dev/)
[Niagara by Arseny Kapoulkine](https://www.youtube.com/watch?v=BR2my8OE1Sc&list=PL0JVLUVCkk-l7CWCn3-cdftR0oajugYvd&ab_channel=ArsenyKapoulkine)
[glTF Sample Viewer](https://github.com/KhronosGroup/glTF-Sample-Viewer)
[Reference Path Tracer by Jakub Boksansky](https://github.com/boksajak/referencePT)
[pbrt](https://pbr-book.org/)