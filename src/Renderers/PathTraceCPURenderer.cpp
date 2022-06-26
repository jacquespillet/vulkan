#include "PathTraceCPURenderer.h"
#include "App.h"
#include "imgui.h"

void threadPool::Start()
{
    uint32_t NumThreads = std::thread::hardware_concurrency();
    
    Threads.resize(NumThreads);
    for(uint32_t i=0; i<NumThreads; i++)
    {
        Threads[i] = std::thread([this]()
        {
            while(true)
            {
                std::function<void()> Job;

                {
                    //Wait for the job to not be empty, or for the souldTerminate signal
                    std::unique_lock<std::mutex> Lock(QueueMutex);
                    
                    //Locks the thread until MutexCondition is being signalized, or if the jobs queue is not empty, or if we should terminate
                    MutexCondition.wait(Lock, [this]{
                        return !Jobs.empty() || ShouldTerminate;
                    });

                    if(ShouldTerminate) return;

                    Job = Jobs.front();
                    Jobs.pop();
                }

                Job();
            }       
        });
    }

}

void threadPool::AddJob(const std::function<void()>& Job)
{
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        Jobs.push(Job);
    }
    //Notifies one thread waiting on this condition to be signalized
    MutexCondition.notify_one();
}

void threadPool::Stop()
{
    {
        std::unique_lock<std::mutex> lock(QueueMutex);
        ShouldTerminate=true;
    }

    MutexCondition.notify_all();
    for(int i=0; i<Threads.size(); i++)
    {
        Threads[i].join();
    }
    Threads.clear();
}

bool threadPool::Busy()
{
    bool Busy;
    {
        std::unique_lock<std::mutex> Lock(QueueMutex);
        Busy = !Jobs.empty();
    }

    return Busy;
}



bool pathTraceCPURenderer::bvhNode::IsLeaf()
{
    return TriangleCount > 0;
}

float pathTraceCPURenderer::aabb::Area()
{
    glm::vec3 e = Max - Min;
    return e.x * e.y + e.y * e.z + e.z * e.x;
}

void pathTraceCPURenderer::aabb::Grow(glm::vec3 Position)
{
    Min = glm::min(Min, Position);
    Max = glm::max(Max, Position);
}

pathTraceCPURenderer::pathTraceCPURenderer(vulkanApp *App) : renderer(App) {}

void pathTraceCPURenderer::Render()
{
    if(ShouldPathTrace)
    {
        HasPreview=false;
        PathTrace();    
        ShouldPathTrace=false;
    }
    if(App->Scene->Camera.Changed && !ThreadPool.Busy())
    {
        HasPathTrace=false;
        Preview();    
    }

    VK_CALL(App->VulkanObjects.Swapchain.AcquireNextImage(App->VulkanObjects.Semaphores.PresentComplete, &App->VulkanObjects.CurrentBuffer));
    
    if(HasPathTrace)
    {
        VulkanObjects.ImageStagingBuffer.Map();
        VulkanObjects.ImageStagingBuffer.CopyTo(Image.data(), Image.size() * sizeof(rgba8));
        VulkanObjects.ImageStagingBuffer.Unmap();
    }

    if(HasPreview)
    {
        VulkanObjects.previewBuffer.Map();
        VulkanObjects.previewBuffer.CopyTo(PreviewImage.data(), PreviewImage.size() * sizeof(rgba8));
        VulkanObjects.previewBuffer.Unmap();
    }


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

        if(HasPathTrace)
        {
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
        }        

        if(HasPreview)
        {
            VkBufferImageCopy Region = {};
            Region.imageExtent.depth=1;
            Region.imageExtent.width = previewSize;
            Region.imageExtent.height = previewSize;
            Region.imageOffset = {0,0,0};
            Region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            Region.imageSubresource.baseArrayLayer=0;
            Region.imageSubresource.layerCount=1;
            Region.imageSubresource.mipLevel=0;
            Region.bufferOffset=0;
            Region.bufferRowLength=0;

            vkCmdCopyBufferToImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewBuffer.VulkanObjects.Buffer, 
                                    VulkanObjects.previewImage.Image, 
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);  

            vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image, 
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SubresourceRange);                                    

            VkImageBlit BlitRegion = {};
            BlitRegion.srcOffsets[0] = {0,0,0};
            BlitRegion.srcOffsets[1] = {(int)previewSize, (int)previewSize, 1};
            BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            BlitRegion.srcSubresource.mipLevel = 0;
            BlitRegion.srcSubresource.baseArrayLayer = 0;
            BlitRegion.srcSubresource.layerCount=1;
            BlitRegion.dstOffsets[0] = {0,0,0};
            BlitRegion.dstOffsets[1] = {(int)App->Width, (int)App->Height, 1};
            BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            BlitRegion.dstSubresource.mipLevel = 0;
            BlitRegion.dstSubresource.baseArrayLayer = 0;
            BlitRegion.dstSubresource.layerCount=1;
            
            vkCmdBlitImage(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                            App->VulkanObjects.Swapchain.Images[App->VulkanObjects.CurrentBuffer], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                            1, &BlitRegion, VK_FILTER_NEAREST);


			vulkanTools::TransitionImageLayout(VulkanObjects.DrawCommandBuffer, VulkanObjects.previewImage.Image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, SubresourceRange);
        }

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

void pathTraceCPURenderer::PathTraceTile(uint32_t StartX, uint32_t StartY, uint32_t TileWidth, uint32_t TileHeight, uint32_t ImageWidth, uint32_t ImageHeight, std::vector<rgba8>* ImageToWrite)
{
    glm::vec4 Origin = App->Scene->Camera.GetModelMatrix() * glm::vec4(0,0,0,1);
    ray Ray = {}; 
    for(uint32_t yy=StartY; yy < StartY+TileHeight; yy++)
    {
        for(uint32_t xx=StartX; xx < StartX+TileWidth; xx++)
        {
            (*ImageToWrite)[yy * ImageWidth + xx] = { 0, 0, 0, 0 };
    
            glm::vec2 uv((float)xx / (float)ImageWidth, (float)yy / (float)ImageHeight);
            Ray.Origin = Origin;
            glm::vec4 Target = glm::inverse(App->Scene->Camera.GetProjectionMatrix()) * glm::vec4(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f, 0.0f, 1.0f);
            glm::vec4 Direction = App->Scene->Camera.GetModelMatrix() * glm::normalize(glm::vec4(Target.x,Target.y, Target.z, 0.0f));
            // vec4 Direction = SceneUbo.Data.InvView * vec4(normalize(Target.xyz), 0.0);
            Ray.Direction = Direction;
            Ray.InverseDirection = 1.0f / Direction;
            Ray.t = 1e30f;
            IntersectBVH(Ray, RootNodeIndex);
            if (Ray.t < 1e30f)
            {
                uint8_t v = (uint8_t)(std::min(1.0f, (Ray.t / 5.0f)) * 255.0f);
                (*ImageToWrite)[yy * ImageWidth + xx] = {v, v, v, 255 };
            }
        }
    }
}

void pathTraceCPURenderer::PathTrace()
{
    for(uint32_t y=0; y<App->Height; y+=TileSize)
    {
        for(uint32_t x=0; x<App->Width; x+=TileSize)
        {
            // 
            ThreadPool.AddJob([x, y, this]()
            {
               PathTraceTile(x, y, TileSize, TileSize, App->Width, App->Height, &Image); 
            });
        }
    }

    HasPathTrace=true;
}

void pathTraceCPURenderer::Preview()
{
    for(uint32_t y=0; y<previewSize; y+=TileSize)
    {
        for(uint32_t x=0; x<previewSize; x+=TileSize)
        {   
            ThreadPool.AddJob([x, y, this]()
            {
               PathTraceTile(x, y, TileSize, TileSize, previewSize,previewSize, &PreviewImage); 
            });
        }
    }

    HasPreview=true;
}

void pathTraceCPURenderer::Setup()
{
    ThreadPool.Start();
    CreateCommandBuffers();

    Image.resize(App->Width * App->Height);
    PreviewImage.resize(previewSize * previewSize);
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
    
    vulkanTools::CreateBuffer(VulkanDevice, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 
                              &VulkanObjects.previewBuffer, PreviewImage.size() * sizeof(rgba8), PreviewImage.data());    
    VulkanObjects.previewImage.Create(VulkanDevice, App->VulkanObjects.CommandPool, App->VulkanObjects.Queue, VK_FORMAT_R8G8B8A8_UNORM, {previewSize, previewSize, 1});

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

    ThreadPool.Stop();
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

float pathTraceCPURenderer::EvaluateSAH(bvhNode &Node, int Axis, float Position)
{
    aabb LeftBox, RightBox;
    int LeftCount=0;
    int RightCount=0;

    for(uint32_t i=0; i<Node.TriangleCount; i++)
    {
        triangle &Triangle = Triangles[TriangleIndices[Node.LeftChildOrFirst + i]];
        if(Triangle.Centroid[Axis] < Position)
        {
            LeftCount++;
            LeftBox.Grow(Triangle.v0);
            LeftBox.Grow(Triangle.v1);
            LeftBox.Grow(Triangle.v2);
        }
        else
        {
            RightCount++;
            RightBox.Grow(Triangle.v0);
            RightBox.Grow(Triangle.v1);
            RightBox.Grow(Triangle.v2);
        }
    }

    float Cost = LeftCount * LeftBox.Area() + RightCount * RightBox.Area();
    return Cost > 0 ? Cost : 1e30f;
}

void pathTraceCPURenderer::Subdivide(uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];


#if 0
    if(Node.TriangleCount <=2) return;
    glm::vec3 Extent = Node.AABBMax - Node.AABBMin;
    int Axis=0;
    if(Extent.y > Extent.x) Axis=1;
    if(Extent.z > Extent[Axis]) Axis=2;
    float SplitPosition = Node.AABBMin[Axis] + Extent[Axis] * 0.5f; 
#else
    glm::vec3 e = Node.AABBMax - Node.AABBMin;
    float ParentArea = e.x * e.y + e.x * e.z + e.y * e.z;
    float ParentCost = Node.TriangleCount * ParentArea;

    int BestAxis=-1;
    float BestPosition = 0;
    float BestCost = 1e30f;
    for(int Axis=0; Axis<3; Axis++)
    {
        for(uint32_t i=0; i<Node.TriangleCount; i++)
        {
            triangle &Triangle = Triangles[TriangleIndices[Node.LeftChildOrFirst + i]];
            float CandidatePosition = Triangle.Centroid[Axis];
            float Cost = EvaluateSAH(Node, Axis, CandidatePosition);
            if(Cost < BestCost)
            {
                BestPosition = CandidatePosition;
                BestAxis = Axis;
                BestCost = Cost;
            }
        }
    }
    if(BestCost >= ParentCost) return;

    int Axis = BestAxis;
    float SplitPosition = BestPosition;
#endif

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
    bvhNode *Node = &BVHNodes[RootNodeIndex];
    bvhNode *Stack[64];
    uint32_t StackPointer=0;
    while(true)
    {
        if(Node->IsLeaf())
        {
            for(uint32_t i=0; i<Node->TriangleCount; i++)
            {
                RayTriangleInteresection(Ray, Triangles[TriangleIndices[Node->LeftChildOrFirst + i]]);
            }
            if(StackPointer==0) break;
            else Node = Stack[--StackPointer];
            continue;
        }

        bvhNode *Child1 = &BVHNodes[Node->LeftChildOrFirst];
        bvhNode *Child2 = &BVHNodes[Node->LeftChildOrFirst+1];

        float Dist1 = RayAABBIntersection(Ray, Child1->AABBMin, Child1->AABBMax);
        float Dist2 = RayAABBIntersection(Ray, Child2->AABBMin, Child2->AABBMax);
        if(Dist1 > Dist2) {
            std::swap(Dist1, Dist2);
            std::swap(Child1, Child2);
        }

        if(Dist1 == 1e30f)
        {
            if(StackPointer==0) break;
            else Node = Stack[--StackPointer];
        }
        else
        {
            Node = Child1;
            if(Dist2 != 1e30f)
            {
                Stack[StackPointer++] = Child2;
            }   
        }
    }
}

float pathTraceCPURenderer::RayAABBIntersection(ray &Ray, glm::vec3 AABBMin,glm::vec3 AABBMax)
{
    float tx1 = (AABBMin.x - Ray.Origin.x) * Ray.InverseDirection.x, tx2 = (AABBMax.x - Ray.Origin.x) * Ray.InverseDirection.x;
    float tmin = std::min( tx1, tx2 ), tmax = std::max( tx1, tx2 );
    float ty1 = (AABBMin.y - Ray.Origin.y) * Ray.InverseDirection.y, ty2 = (AABBMax.y - Ray.Origin.y) * Ray.InverseDirection.y;
    tmin = std::max( tmin, std::min( ty1, ty2 ) ), tmax = std::min( tmax, std::max( ty1, ty2 ) );
    float tz1 = (AABBMin.z - Ray.Origin.z) * Ray.InverseDirection.z, tz2 = (AABBMax.z - Ray.Origin.z) * Ray.InverseDirection.z;
    tmin = std::max( tmin, std::min( tz1, tz2 ) ), tmax = std::min( tmax, std::max( tz1, tz2 ) );
    if(tmax >= tmin && tmin < Ray.t && tmax > 0) return tmin;
    else return 1e30f;    
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