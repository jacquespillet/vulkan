#include "bvh.h"
#include <glm/ext.hpp>

#define BINS 8

////////////////////////////////////////////////////////////////////////////////////////


bool bvhNode::IsLeaf()
{
    return TriangleCount > 0;
}

////////////////////////////////////////////////////////////////////////////////////////

float aabb::Area()
{
    glm::vec3 e = Max - Min; // box extent
    return e.x * e.y + e.y * e.z + e.z * e.x;
}

void aabb::Grow(glm::vec3 Position)
{
    Min = glm::min(Min, Position);
    Max = glm::max(Max, Position);
}

void aabb::Grow(aabb &AABB)
{
    if(AABB.Min.x != 1e30f)
    {
        Grow(AABB.Min);
        Grow(AABB.Max);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

mesh::mesh(std::vector<uint32_t> &Indices, std::vector<vertex> &Vertices)
{
    uint32_t AddedTriangles=0;

    Triangles.resize(Indices.size()/3);
    TrianglesExtraData.resize(Indices.size()/3);
    for(size_t j=0; j<Indices.size(); j+=3)
    {
        uint32_t i0 = Indices[j+0];
        uint32_t i1 = Indices[j+1];
        uint32_t i2 = Indices[j+2];
        glm::vec4 v0 = Vertices[i0].Position;
        glm::vec4 v1 = Vertices[i1].Position;
        glm::vec4 v2 = Vertices[i2].Position;

        glm::vec4 n0 = Vertices[i0].Normal;
        glm::vec4 n1 = Vertices[i1].Normal;
        glm::vec4 n2 = Vertices[i2].Normal;
        
        Triangles[AddedTriangles].v0=v0;
        Triangles[AddedTriangles].v1=v1;
        Triangles[AddedTriangles].v2=v2;
        
        TrianglesExtraData[AddedTriangles].Normal0=n0;
        TrianglesExtraData[AddedTriangles].Normal1=n1;
        TrianglesExtraData[AddedTriangles].Normal2=n2;

        TrianglesExtraData[AddedTriangles].UV0 = glm::vec2(v0.w, n0.w);
        TrianglesExtraData[AddedTriangles].UV1 = glm::vec2(v1.w, n1.w);
        TrianglesExtraData[AddedTriangles].UV2 = glm::vec2(v2.w, n2.w);
        

        AddedTriangles++;
    }

    BVH = new bvh(this);
}

////////////////////////////////////////////////////////////////////////////////////////

bvh::bvh(mesh *_Mesh)
{
    this->Mesh = _Mesh;
    Build();
}

void bvh::Build()
{
    BVHNodes.resize(Mesh->Triangles.size() * 2 - 1);
    TriangleIndices.resize(Mesh->Triangles.size());

    for(size_t i=0; i<Mesh->Triangles.size(); i++)
    {
        Mesh->Triangles[i].Centroid = (Mesh->Triangles[i].v0 + Mesh->Triangles[i].v1 + Mesh->Triangles[i].v2) * 0.33333f;
        TriangleIndices[i] = (uint32_t)i;
    }

    bvhNode &Root = BVHNodes[RootNodeIndex];
    Root.LeftChildOrFirst = 0;
    Root.TriangleCount = (uint32_t)Mesh->Triangles.size();
    UpdateNodeBounds(RootNodeIndex);
    Subdivide(RootNodeIndex);
}


float bvh::EvaluateSAH(bvhNode &Node, int Axis, float Position)
{
	aabb leftBox, rightBox;
	int leftCount = 0, rightCount = 0;
	for (uint32_t i = 0; i < Node.TriangleCount; i++)
	{
		triangle& Triangle = Mesh->Triangles[TriangleIndices[Node.LeftChildOrFirst + i]];
		if (Triangle.Centroid[Axis] < Position)
		{
			leftCount++;
			leftBox.Grow( Triangle.v0 );
			leftBox.Grow( Triangle.v1 );
			leftBox.Grow( Triangle.v2 );
		}
		else
		{
			rightCount++;
			rightBox.Grow( Triangle.v0 );
			rightBox.Grow( Triangle.v1 );
			rightBox.Grow( Triangle.v2 );
		}
	}
	float cost = leftCount * leftBox.Area() + rightCount * rightBox.Area();
	return cost > 0 ? cost : 1e30f;    
}


float bvh::FindBestSplitPlane(bvhNode &Node, int &Axis, float &SplitPosition)
{
    float BestCost = 1e30f;
    for(int CurrentAxis=0; CurrentAxis<3; CurrentAxis++)
    {
        float BoundsMin = 1e30f;
        float BoundsMax = -1e30f;
        for(uint32_t i=0; i<Node.TriangleCount; i++)
        {
            triangle &Triangle = Mesh->Triangles[TriangleIndices[Node.LeftChildOrFirst + i]];
            BoundsMin = std::min(BoundsMin, Triangle.Centroid[CurrentAxis]);
            BoundsMax = std::max(BoundsMax, Triangle.Centroid[CurrentAxis]);
        }
        if(BoundsMin == BoundsMax) continue;
        
        
        bin Bins[BINS];
        float Scale = BINS / (BoundsMax - BoundsMin);
        for(uint32_t i=0; i<Node.TriangleCount; i++)
        {
            triangle &Triangle = Mesh->Triangles[TriangleIndices[Node.LeftChildOrFirst + i]];
            int BinIndex = std::min(BINS - 1, (int)((Triangle.Centroid[CurrentAxis] - BoundsMin) * Scale));
            Bins[BinIndex].TrianglesCount++;
            Bins[BinIndex].Bounds.Grow(Triangle.v0);
            Bins[BinIndex].Bounds.Grow(Triangle.v1);
            Bins[BinIndex].Bounds.Grow(Triangle.v2);
        }

        float LeftArea[BINS-1], RightArea[BINS-1];
        int LeftCount[BINS-1], RightCount[BINS-1];
        
        aabb LeftBox, RightBox;
        int LeftSum=0, RightSum=0;

        for(int i=0; i<BINS-1; i++)
        {
            //Info from the left to the right
            LeftSum += Bins[i].TrianglesCount;
            LeftCount[i] = LeftSum; //Number of primitives to the right of this plane
            LeftBox.Grow(Bins[i].Bounds);
            LeftArea[i] = LeftBox.Area(); //Area to the right of this plane
            
            //Info from the right to the left
            RightSum += Bins[BINS-1-i].TrianglesCount;
            RightCount[BINS-2-i] = RightSum; //Number of primitives to the left of this plane
            RightBox.Grow(Bins[BINS-1-i].Bounds);
            RightArea[BINS-2-i] = RightBox.Area(); //Area to the left of this plane
        }

        Scale = (BoundsMax - BoundsMin) / BINS;
        for(int i=0; i<BINS-1; i++)
        {
            float PlaneCost = LeftCount[i] * LeftArea[i] + RightCount[i] * RightArea[i];
            if(PlaneCost < BestCost)
            {
                Axis = CurrentAxis;
                SplitPosition = BoundsMin + Scale * (i+1);
                BestCost = PlaneCost;
            }
        }
    }
    return BestCost;

}

void bvh::Subdivide(uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];

    int Axis=-1;
    float SplitPosition = 0;
    float SplitCost = FindBestSplitPlane(Node, Axis, SplitPosition);
    float NoSplitCost = CalculateNodeCost(Node);
    if(SplitCost >= NoSplitCost) return;

    int i=Node.LeftChildOrFirst;
    int j = i + Node.TriangleCount -1;
    while(i <= j)
    {
        if(Mesh->Triangles[TriangleIndices[i]].Centroid[Axis] < SplitPosition)
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


float bvh::CalculateNodeCost(bvhNode &Node)
{
    glm::vec3 e = Node.AABBMax - Node.AABBMin;
    float ParentArea = e.x * e.y + e.x * e.z + e.y * e.z;
    float NodeCost = Node.TriangleCount * ParentArea;
    return NodeCost;
}

void bvh::Refit()
{

}

void bvh::Intersect(ray Ray, rayPayload &RayPayload, uint32_t InstanceIndex)
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
                RayTriangleInteresection(Ray, Mesh->Triangles[TriangleIndices[Node->LeftChildOrFirst + i]], RayPayload, InstanceIndex, TriangleIndices[Node->LeftChildOrFirst + i]);
            }
            if(StackPointer==0) break;
            else Node = Stack[--StackPointer];
            continue;
        }

        bvhNode *Child1 = &BVHNodes[Node->LeftChildOrFirst];
        bvhNode *Child2 = &BVHNodes[Node->LeftChildOrFirst+1];

        float Dist1 = RayAABBIntersection(Ray, Child1->AABBMin, Child1->AABBMax, RayPayload);
        float Dist2 = RayAABBIntersection(Ray, Child2->AABBMin, Child2->AABBMax, RayPayload);
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

void bvh::UpdateNodeBounds(uint32_t NodeIndex)
{
    bvhNode &Node = BVHNodes[NodeIndex];
    Node.AABBMin = glm::vec3(1e30f);
    Node.AABBMax = glm::vec3(-1e30f);
    for(uint32_t First=Node.LeftChildOrFirst, i=0; i<Node.TriangleCount; i++)
    {
        uint32_t TriangleIndex = TriangleIndices[First + i];
        triangle &Triangle = Mesh->Triangles[TriangleIndex];
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v0);
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v1);
        Node.AABBMin = glm::min(Node.AABBMin, Triangle.v2);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v0);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v1);
        Node.AABBMax = glm::max(Node.AABBMax, Triangle.v2);
    }
}

////////////////////////////////////////////////////////////////////////////////////////

void bvhInstance::SetTransform(glm::mat4 &Transform)
{
    this->InverseTransform = glm::inverse(Transform);
    
    glm::vec3 Min = BVH->BVHNodes[0].AABBMin;
    glm::vec3 Max = BVH->BVHNodes[0].AABBMax;
    Bounds = {};
    for (int i = 0; i < 8; i++)
    {
		Bounds.Grow( Transform *  glm::vec4( 
                                    i & 1 ? Max.x : Min.x,
                                    i & 2 ? Max.y : Min.y, 
                                    i & 4 ? Max.z : Min.z,
                                    1.0f ));
    }    
}

void bvhInstance::Intersect(ray Ray, rayPayload &RayPayload)
{
    Ray.Origin = InverseTransform * glm::vec4(Ray.Origin, 1);
    Ray.Direction = InverseTransform * glm::vec4(Ray.Direction, 0);
    Ray.InverseDirection = 1.0f / Ray.Direction;

    BVH->Intersect(Ray, RayPayload, Index);
}


////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////

tlas::tlas()
{}

tlas::tlas(std::vector<bvhInstance>* BVHList)
{
    BLAS = BVHList;
    Nodes.resize(BLAS->size() * 2);
    NodesUsed=2;
}

int tlas::FindBestMatch(std::vector<int>& List, int N, int A)
{
    float Smallest = 1e30f;
    int BestB = -1;
    for(int B=0; B< N; B++)
    {
        if(B != A)
        {
            glm::vec3 BMax = glm::max(Nodes[List[A]].AABBMax, Nodes[List[B]].AABBMax);
            glm::vec3 BMin = glm::min(Nodes[List[A]].AABBMin, Nodes[List[B]].AABBMin);
            glm::vec3 Diff = BMax - BMin;
            float Area = Diff.x * Diff.y + Diff.y * Diff.z + Diff.x * Diff.z;
            if(Area < Smallest) 
            {
                Smallest = Area;
                BestB = B;
            }
        }
    }

    return BestB;
}

void tlas::Build()
{
    std::vector<int> NodeIndex(BLAS->size());
    int NodeIndices = (int)BLAS->size();
    NodesUsed=1;
    for(uint32_t i=0; i<BLAS->size(); i++)
    {
        NodeIndex[i] = NodesUsed;
        Nodes[NodesUsed].AABBMin = (*BLAS)[i].Bounds.Min;
        Nodes[NodesUsed].AABBMax = (*BLAS)[i].Bounds.Max;
        Nodes[NodesUsed].BLAS = i;
        Nodes[NodesUsed].LeftRight = 0; //Makes it a leaf.
        NodesUsed++;
    }


    int A = 0;
    int B= FindBestMatch(NodeIndex, NodeIndices, A); //Best match for A
    while(NodeIndices >1)
    {
        int C = FindBestMatch(NodeIndex, NodeIndices, B); //Best match for B
        if(A == C) //There is no better match --> Create a parent for them
        {
            int NodeIndexA = NodeIndex[A];
            int NodeIndexB = NodeIndex[B];
            tlasNode &NodeA = Nodes[NodeIndexA];
            tlasNode &NodeB = Nodes[NodeIndexB];
            
            tlasNode &NewNode = Nodes[NodesUsed];
            NewNode.LeftRight = NodeIndexA + (NodeIndexB << 16);
            NewNode.AABBMin = glm::min(NodeA.AABBMin, NodeB.AABBMin);
            NewNode.AABBMax = glm::max(NodeA.AABBMax, NodeB.AABBMax);
            
            NodeIndex[A] = NodesUsed++;
            NodeIndex[B] = NodeIndex[NodeIndices-1];
            B = FindBestMatch(NodeIndex, --NodeIndices, A);
        }
        else
        {
            A = B;
            B = C;
        }
    }

    Nodes[0] = Nodes[NodeIndex[A]];




}

void tlas::Intersect(ray Ray, rayPayload &RayPayload)
{
    Ray.InverseDirection = 1.0f / Ray.Direction;
    tlasNode *Node = &Nodes[0];
    tlasNode *Stack[64];
    uint32_t StackPtr=0;
    while(1)
    {
        //If we hit the leaf, check intersection with the bvhs
        if(Node->IsLeaf())
        {
            (*BLAS)[Node->BLAS].Intersect(Ray, RayPayload);
            if(StackPtr == 0) break;
            else Node = Stack[--StackPtr];
            continue;
        }

        //Check if hit any of the children
        tlasNode *Child1 = &Nodes[Node->LeftRight & 0xffff];
        tlasNode *Child2 = &Nodes[Node->LeftRight >> 16];
        float Dist1 = RayAABBIntersection(Ray, Child1->AABBMin, Child1->AABBMax, RayPayload);
        float Dist2 = RayAABBIntersection(Ray, Child2->AABBMin, Child2->AABBMax, RayPayload);
        if(Dist1 > Dist2) { //Swap if dist 2 is closer
            std::swap(Dist1, Dist2);
            std::swap(Child1, Child2);
        }
        
        if(Dist1 == 1e30f) //We didn't hit a child
        {
            if(StackPtr == 0) break; //There's no node left to explore
            else Node = Stack[--StackPtr]; //Go to the next node in the stack
        }
        else //We hit a child
        {
            Node = Child1; //Set the current node to the first child
            if(Dist2 != 1e30f) Stack[StackPtr++] = Child2; //If we also hit the other node, add it in the stack
        }

    }
}

////////////////////////////////////////////////////////////////////////////////////////


float RayAABBIntersection(ray Ray, glm::vec3 AABBMin,glm::vec3 AABBMax, rayPayload &RayPayload)
{
    float tx1 = (AABBMin.x - Ray.Origin.x) * Ray.InverseDirection.x, tx2 = (AABBMax.x - Ray.Origin.x) * Ray.InverseDirection.x;
    float tmin = std::min( tx1, tx2 ), tmax = std::max( tx1, tx2 );
    float ty1 = (AABBMin.y - Ray.Origin.y) * Ray.InverseDirection.y, ty2 = (AABBMax.y - Ray.Origin.y) * Ray.InverseDirection.y;
    tmin = std::max( tmin, std::min( ty1, ty2 ) ), tmax = std::min( tmax, std::max( ty1, ty2 ) );
    float tz1 = (AABBMin.z - Ray.Origin.z) * Ray.InverseDirection.z, tz2 = (AABBMax.z - Ray.Origin.z) * Ray.InverseDirection.z;
    tmin = std::max( tmin, std::min( tz1, tz2 ) ), tmax = std::min( tmax, std::max( tz1, tz2 ) );
    if(tmax >= tmin && tmin < RayPayload.Distance && tmax > 0) return tmin;
    else return 1e30f;    
}

void RayTriangleInteresection(ray Ray, triangle &Triangle, rayPayload &RayPayload, uint32_t InstanceIndex, uint32_t PrimitiveIndex)
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
    if(t > 0.0001f && t < RayPayload.Distance) {
        



        RayPayload.U = u;
        RayPayload.V = v;
        RayPayload.InstanceIndex = InstanceIndex;
        RayPayload.Distance = t;
        RayPayload.PrimitiveIndex = PrimitiveIndex;
    }
}