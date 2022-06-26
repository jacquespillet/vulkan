#include "PathTraceCPURenderer.h"
#include "App.h"
#include "imgui.h"

bool pathTraceCPURenderer::bvhNode::IsLeaf()
{
    return TriangleCount > 0;
}

pathTraceCPURenderer::pathTraceCPURenderer(vulkanApp *App) : renderer(App) {}

void pathTraceCPURenderer::Render()
{
    if(ShouldPathTrace)
    {
        PathTrace();    
        ShouldPathTrace=false;
    }

    VK_CALL(App->VulkanObjects.Swapchain.AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    VulkanObjects.ImageStagingBuffer.Map();
    VulkanObjects.ImageStagingBuffer.CopyTo(Image.data(), Image.size() * sizeof(rgba8));
    VulkanObjects.ImageStagingBuffer.Unmap();

    //Fill command buffer
    {
        VkCommandBufferBeginInfo CommandBufferInfo = vulkanTools::BuildCommandBufferBeginInfo();
        std::array<VkClearValue, 2> ClearValues = {};
        ClearValues[0].color = {{0.0f,0.0f,0.0f,0.0f}};
        ClearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo RenderPassBeginInfo = vulkanTools::BuildRenderPassBeginInfo();
        RenderPassBeginInfo.renderPass = App->VulkanObjects.RenderPass;
        RenderPassBeginInfo.renderArea.extent.width = App->Width;
        RenderPassBeginInfo.renderArea.extent.height = App->Height;
        RenderPassBeginInfo.clearValueCount=(uint32_t)ClearValues.size();
        RenderPassBeginInfo.pClearValues=ClearValues.data();

        App->ImGuiHelper->UpdateBuffers();
        
        RenderPassBeginInfo.framebuffer = App->VulkanObjects.AppFramebuffers[App->VulkanObjects.CurrentBuffer];
        VK_CALL(vkBeginCommandBuffer(VulkanObjects.DrawCommandBuffer, &CommandBufferInfo));

        VkImageSubresourceRange SubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain.Images[App->VulkanObjects.CurrentBuffer], 
                                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        
        VkBufferImageCopy Region = {};
        Region.imageExtent.depth=1;
        Region.imageExtent.width = App->Width;
        Region.imageExtent.height = App->Height;
        Region.imageOffset = {0,0,0};
        Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        Region.imageSubresource.baseArrayLayer=0;
        Region.imageSubresource.layerCount=1;
        Region.imageSubresource.mipLevel=0;
        Region.bufferOffset=0;
        Region.bufferRowLength=0;

        vkCmdCopyBufferToImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.ImageStagingBuffer.VulkanObjects.Buffer, 
                                App->VulkanObjects.Swapchain.Images[App->VulkanObjects.CurrentBuffer], 
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);

        vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, App->VulkanObjects.Swapchain.Images[App->VulkanObjects.CurrentBuffer], 
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, SubresourceRange);


        vkCmdBeginRenderPass(VulkanObjects.DrawCommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        App->ImGuiHelper->DrawFrame(VulkanObjects.DrawCommandBuffer);
        vkCmdEndRenderPass(VulkanObjects.DrawCommandBuffer);
        VK_CALL(vkEndCommandBuffer(VulkanObjects.DrawCommandBuffer));
    }

    
    VulkanObjects.SubmitInfo = vulkanTools::BuildSubmitInfo();
    VulkanObjects.SubmitInfo.pWaitDstStageMask = &App->VulkanObjects.SubmitPipelineStages;
    VulkanObjects.SubmitInfo.waitSemaphoreCount = 1;
    VulkanObjects.SubmitInfo.signalSemaphoreCount=1;
    VulkanObjects.SubmitInfo.pWaitSemaphores = &App->VulkanObjects.Semaphores.PresentComplete;
    VulkanObjects.SubmitInfo.pSignalSemaphores = &App->VulkanObjects.Semaphores.RenderComplete;
    VulkanObjects.SubmitInfo.commandBufferCount=1;
    VulkanObjects.SubmitInfo.pCommandBuffers = &VulkanObjects.DrawCommandBuffer;
    VK_CALL(vkQueueSubmit(App->VulkanObjects.Queue, 1, &VulkanObjects.SubmitInfo, VK_NULL_HANDLE));

    VK_CALL(App->VulkanObjects.Swapchain.QueuePresent(App->VulkanObjects.Queue, App->VulkanObjects.CurrentBuffer, App->VulkanObjects.Semaphores.RenderComplete));
    VK_CALL(vkQueueWaitIdle(App->VulkanObjects.Queue));
}

void pathTraceCPURenderer::PathTrace()
{
    glm::vec4 Origin = App->Scene->Camera.GetModelMatrix() * glm::vec4(0,0,0,1);
    
    ray Ray = {}; 
    for(uint32_t yy=0; yy<App->Height; yy++)
    {
        for(uint32_t xx=0; xx<App->Width; xx++)
        {
            Image[yy * App->Width + xx] = { 0, 0, 0, 0 };
	
            glm::vec2 uv((float)xx / (float)App->Width, (float)yy / (float)App->Height);
            Ray.Origin = Origin;
            glm::vec4 Target = glm::inverse(App->Scene->Camera.GetProjectionMatrix()) * glm::vec4(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f, 0.0f, 1.0f);
            glm::vec4 Direction = App->Scene->Camera.GetModelMatrix() * glm::normalize(glm::vec4(Target.x,Target.y, Target.z, 0.0f));
            // vec4 Direction = SceneUbo.Data.InvView * vec4(normalize(Target.xyz), 0.0);
            Ray.Direction = Direction;
            Ray.t = 1e30f;
#if 0
            for(int i=0; i<NumTriangles; i++)
            {
                RayTriangleInteresection(Ray, Triangles[i]);
            }
#else
            IntersectBVH(Ray, RootNodeIndex);
#endif
			if (Ray.t < 1e30f)
			{
                uint8_t v = (uint8_t)(std::min(1.0f, (Ray.t / 5.0f)) * 255.0f);
				Image[yy * App->Width + xx] = {v, v, v, 255 };
			}
        }
    }
}

void pathTraceCPURenderer::Setup()
{
    CreateCommandBuffers();

    Image.resize(App->Width * App->Height);
    for(uint32_t yy=0; yy<App->Height; yy++)
    {
        for(uint32_t xx=0; xx<App->Height; xx++)
        {
            Image[yy * App->Width + xx] = {0, 0, 0, 255};
        }
    }

    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
                              &VulkanObjects.ImageStagingBuffer, Image.size() * sizeof(rgba8), Image.data());

    InitGeometry();
}

//


void pathTraceCPURenderer::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo CommandBufferAllocateInfo = vulkanTools::BuildCommandBufferAllocateInfo(App->VulkanObjects.CommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
    VK_CALL(vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &VulkanObjects.DrawCommandBuffer));
}

void pathTraceCPURenderer::RenderGUI()
{
    if(ImGui::Button("PathTrace"))
    {
        ShouldPathTrace=true;
    }
}

void pathTraceCPURenderer::Resize(uint32_t Width, uint32_t Height) 
{
}

void pathTraceCPURenderer::Destroy()
{
    vkFreeCommandBuffers(Device, App->VulkanObjects.CommandPool, 1, &VulkanObjects.DrawCommandBuffer);
}

void pathTraceCPURenderer::InitGeometry()
{
    uint32_t AddedTriangles=0;
    for(size_t i=0; i<App->Scene->InstancesPointers.size(); i++)
    {
        sceneMesh *Mesh = App->Scene->InstancesPointers[i]->Mesh;
        glm::mat4 Transform = App->Scene->InstancesPointers[i]->InstanceData.Transform;
        Triangles.resize(Triangles.size() + Mesh->Indices.size()/3);
        for(size_t j=0; j<Mesh->Indices.size(); j+=3)
        {
            uint32_t i0 = Mesh->Indices[j+0];
            uint32_t i1 = Mesh->Indices[j+1];
            uint32_t i2 = Mesh->Indices[j+2];
            glm::vec4 v0 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i0].Position), 1.0f);
            glm::vec4 v1 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i1].Position), 1.0f);
            glm::vec4 v2 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i2].Position), 1.0f);

            glm::vec4 n0 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i0].Normal), 0.0f);
            glm::vec4 n1 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i1].Normal), 0.0f);
            glm::vec4 n2 = Transform * glm::vec4(glm::vec3(Mesh->Vertices[i2].Normal), 0.0f);
            
            Triangles[AddedTriangles].v0=v0;
            Triangles[AddedTriangles].v1=v1;
            Triangles[AddedTriangles].v2=v2;
            Triangles[AddedTriangles].Normal0=n0;
            Triangles[AddedTriangles].Normal1=n1;
            Triangles[AddedTriangles].Normal2=n2;
			AddedTriangles++;
        }
    }

    NumTriangles = AddedTriangles;

    BuildBVH();
}

void pathTraceCPURenderer::UpdateNodeBounds(uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];
    Node.AABBMin = glm::vec3(1e30f);
    Node.AABBMax = glm::vec3(-1e30f);
    for(uint32_t First=Node.LeftChildOrFirst, i=0; i<Node.TriangleCount; i++)
    {
        uint32_t TriangleIndex = TriangleIndices[First + i];
        triangle &Triangle = Triangles[TriangleIndex];
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v0);
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v1);
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v2);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v0);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v1);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v2);
    }
}

void pathTraceCPURenderer::Subdivide(uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];

    if(Node.TriangleCount <=2) return;

    glm::vec3 Extent = Node.AABBMax - Node.AABBMin;
    int Axis=0;
    if(Extent.y > Extent.x) Axis=1;
    if(Extent.z > Extent[Axis]) Axis=2;
    float SplitPosition = Node.AABBMin[Axis] + Extent[Axis] * 0.5f; 


    int i=Node.LeftChildOrFirst;
    int j = i + Node.TriangleCount -1;
    while(i <= j)
    {
        if(Triangles[TriangleIndices[i]].Centroid[Axis] < SplitPosition)
        {
            i++;
        }
        else
        {
            std::swap(TriangleIndices[i], TriangleIndices[j--]);
        }
    }

    uint32_t LeftCount = i - Node.LeftChildOrFirst;
    if(LeftCount==0 || LeftCount == Node.TriangleCount) return;

    int LeftChildIndex = NodesUsed++;
    int RightChildIndex = NodesUsed++;
    
    BVHNodes[LeftChildIndex].LeftChildOrFirst = Node.LeftChildOrFirst;
    BVHNodes[LeftChildIndex].TriangleCount = LeftCount;
    BVHNodes[RightChildIndex].LeftChildOrFirst = i;
    BVHNodes[RightChildIndex].TriangleCount = Node.TriangleCount - LeftCount;
    Node.LeftChildOrFirst = LeftChildIndex;
    Node.TriangleCount=0;

    UpdateNodeBounds(LeftChildIndex);
    UpdateNodeBounds(RightChildIndex);

    Subdivide(LeftChildIndex);
    Subdivide(RightChildIndex);
}

void pathTraceCPURenderer::BuildBVH()
{
    BVHNodes.resize(NumTriangles * 2 - 1);
    TriangleIndices.resize(Triangles.size());

    for(size_t i=0; i<Triangles.size(); i++)
    {
        Triangles[i].Centroid = (Triangles[i].v0 + Triangles[i].v1 + Triangles[i].v2) * 0.33333f;
        TriangleIndices[i] = (uint32_t)i;
    }

    bvhNode &Root = BVHNodes[RootNodeIndex];
    Root.LeftChildOrFirst = 0;
    Root.TriangleCount = NumTriangles;
    UpdateNodeBounds(RootNodeIndex);
    Subdivide(RootNodeIndex);
}

void pathTraceCPURenderer::IntersectBVH(ray &Ray, uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];
    if(!RayAABBIntersection(Ray, Node.AABBMin, Node.AABBMax)) return;

    if(Node.IsLeaf())
    {
        for(uint32_t i=0; i<Node.TriangleCount; i++)
        {
            RayTriangleInteresection(Ray, Triangles[TriangleIndices[Node.LeftChildOrFirst + i]]);
        }
    }
    else
    {
        IntersectBVH(Ray, Node.LeftChildOrFirst);
        IntersectBVH(Ray, Node.LeftChildOrFirst+1);
    }
}

bool pathTraceCPURenderer::RayAABBIntersection(ray &Ray, glm::vec3 AABBMin,glm::vec3 AABBMax)
{
    float tx1 = (AABBMin.x - Ray.Origin.x) / Ray.Direction.x, tx2 = (AABBMax.x - Ray.Origin.x) / Ray.Direction.x;
    float tmin = std::min( tx1, tx2 ), tmax = std::max( tx1, tx2 );
    float ty1 = (AABBMin.y - Ray.Origin.y) / Ray.Direction.y, ty2 = (AABBMax.y - Ray.Origin.y) / Ray.Direction.y;
    tmin = std::max( tmin, std::min( ty1, ty2 ) ), tmax = std::min( tmax, std::max( ty1, ty2 ) );
    float tz1 = (AABBMin.z - Ray.Origin.z) / Ray.Direction.z, tz2 = (AABBMax.z - Ray.Origin.z) / Ray.Direction.z;
    tmin = std::max( tmin, std::min( tz1, tz2 ) ), tmax = std::min( tmax, std::max( tz1, tz2 ) );
    return tmax >= tmin && tmin < Ray.t && tmax > 0;    
}

void pathTraceCPURenderer::RayTriangleInteresection(ray &Ray, triangle &Triangle)
{
    glm::vec3 Edge1 = Triangle.v1 - Triangle.v0;
    glm::vec3 Edge2 = Triangle.v2 - Triangle.v0;

    glm::vec3 h = glm::cross(Ray.Direction, Edge2);
    float a = glm::dot(Edge1, h);
    if(a > -0.0001f && a < 0.0001f) return; //Ray is parallel to the triangle
    
    float f = 1 / a;
    glm::vec3 s = Ray.Origin - Triangle.v0;
    float u = f * glm::dot(s, h);
    if(u < 0 || u > 1) return;

    glm::vec3 q = glm::cross(s, Edge1);
    float v = f * glm::dot(Ray.Direction, q);
    if(v < 0 || u + v > 1) return;
    
    float t = f * glm::dot(Edge2, q);
    if(t > 0.0001f) Ray.t = std::min(Ray.t, t);
}