#include "Scene.h"
#include "App.h"
#include "Resources.h"
#include "AssimpImporter.h"
#include "GLTFImporter.h"
#include "IBLHelper.h"


cubemap::cubemap(scene *Scene) : Scene(Scene){}

void cubemap::CreateDescriptorSet(vulkanDevice *VulkanDevice)
{
    //Create Cubemap descriptor pool
    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  4)
    };
    VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
        (uint32_t)PoolSizes.size(),
        PoolSizes.data(),
        1
    );
    VK_CALL(vkCreateDescriptorPool(VulkanDevice->Device, &DescriptorPoolInfo, nullptr, &VulkanObjects.DescriptorPool));


    //Create descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = 
    {
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ),
        vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4 )
    };
    VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
    VK_CALL(vkCreateDescriptorSetLayout(VulkanDevice->Device, &DescriptorLayoutCreateInfo, nullptr, &VulkanObjects.DescriptorSetLayout));


    VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(VulkanObjects.DescriptorPool, &VulkanObjects.DescriptorSetLayout, 1);
    VK_CALL(vkAllocateDescriptorSets(VulkanDevice->Device, &AllocInfo, &VulkanObjects.DescriptorSet));

    std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
    {
        vulkanTools::BuildWriteDescriptorSet( VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &VulkanObjects.UniformBuffer.VulkanObjects.Descriptor),
        vulkanTools::BuildWriteDescriptorSet( VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &VulkanObjects.Texture.Descriptor),
        vulkanTools::BuildWriteDescriptorSet( VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &VulkanObjects.IrradianceMap.Descriptor),
        vulkanTools::BuildWriteDescriptorSet( VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &VulkanObjects.PrefilteredMap.Descriptor),
        vulkanTools::BuildWriteDescriptorSet( VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &VulkanObjects.BRDFLUT.Descriptor)
    };
    vkUpdateDescriptorSets(VulkanDevice->Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);

}    

void cubemap::Load(std::string FileName, textureLoader *TextureLoader, vulkanDevice *VulkanDevice, VkCommandBuffer CommandBuffer, VkQueue Queue)
{
    TextureLoader->LoadCubemap(FileName, &VulkanObjects.Texture);
    GLTFImporter::LoadMesh("resources/models/Cube/Cube.gltf", Mesh, VulkanDevice, CommandBuffer, Queue);

    vulkanTools::CreateBuffer(VulkanDevice, 
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &VulkanObjects.UniformBuffer,
                                sizeof(cubemap::uniformData)
    );        
    UniformData.modelMatrix = glm::scale(glm::mat4(1), glm::vec3(100));
    VulkanObjects.UniformBuffer.Map();
    VulkanObjects.UniformBuffer.CopyTo(&UniformData, sizeof(cubemap::uniformData));
    VulkanObjects.UniformBuffer.Unmap();

    IBLHelper::CalculateIrradianceMap(VulkanDevice, CommandBuffer, Queue, &VulkanObjects.Texture, &VulkanObjects.IrradianceMap);        
    IBLHelper::CalculatePrefilteredMap(VulkanDevice, CommandBuffer, Queue, &VulkanObjects.Texture, &VulkanObjects.PrefilteredMap);        
    IBLHelper::CalculateBRDFLUT(VulkanDevice, CommandBuffer, Queue, &VulkanObjects.BRDFLUT);        
}

void cubemap::UpdateUniforms()
{
    VulkanObjects.UniformBuffer.Map();
    VulkanObjects.UniformBuffer.CopyTo(&UniformData, sizeof(cubemap::uniformData));
    VulkanObjects.UniformBuffer.Unmap();
}

scene::scene(vulkanApp *App) :
            App(App), Device(App->VulkanObjects.Device), 
            Queue(App->VulkanObjects.Queue), TextureLoader(App->VulkanObjects.TextureLoader), Cubemap(this)
{}


void scene::Load(std::string FileName, VkCommandBuffer CopyCommand)
{
    Resources.Init(App->VulkanObjects.VulkanDevice, VK_NULL_HANDLE, TextureLoader);       
    Cubemap.Load("resources/belfast_farmhouse_4k.hdr", TextureLoader, App->VulkanObjects.VulkanDevice, CopyCommand, Queue);
    
    { //Scene

        std::string Extension = FileName.substr(FileName.find_last_of(".") + 1);
        if(Extension == "gltf" || Extension == "glb")
        {
            GLTFImporter::Load(FileName, Instances, Meshes, Materials,GVertices, GIndices, Resources.Textures);    
        }
        else
        {
            assimpImporter::Load(FileName, Instances, Meshes, Materials,GVertices, GIndices, Resources.Textures);    
        }


        for (size_t i = 0; i < Materials.size(); i++)
        {
            vulkanTools::CreateBuffer(App->VulkanObjects.VulkanDevice,
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     &Materials[i].VulkanObjects.UniformBuffer,
                                     sizeof(Materials[i].MaterialData),
                                     &Materials[i].MaterialData);             
        }
        
        NumInstances=0;
        uint32_t InstanceInx=0;
        for(auto &InstanceGroup : Instances)
        {
            for (size_t i = 0; i < InstanceGroup.second.size(); i++)
            {
                InstanceGroup.second[i].InstanceData.InstanceID = (float)InstanceInx;
                InstanceGroup.second[i].InstanceData.Normal = glm::inverseTranspose(InstanceGroup.second[i].InstanceData.Transform);
     
                vulkanTools::CreateBuffer(App->VulkanObjects.VulkanDevice,
                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                        &InstanceGroup.second[i].VulkanObjects.UniformBuffer,
                                        sizeof(InstanceGroup.second[i].InstanceData),
                                        &InstanceGroup.second[i].InstanceData);
                InstancesPointers.push_back(&InstanceGroup.second[i]);
                InstanceInx++;
            }
            NumInstances += InstanceGroup.second.size();
        }

        for(uint32_t i=0; i<Meshes.size(); i++)
        {
            size_t VertexDataSize = Meshes[i].Vertices.size() * sizeof(vertex);
            
            VkBufferUsageFlags Flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            if(App->RayTracing) Flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            vulkanTools::CreateAndFillBuffer(
                App->VulkanObjects.VulkanDevice,
                Meshes[i].Vertices.data(),
                VertexDataSize,
                &Meshes[i].VulkanObjects.VertexBuffer,
                Flags,
                CopyCommand,
                Queue
            );

            size_t IndexDataSize = Meshes[i].Indices.size() * sizeof(uint32_t);
            Flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            if(App->RayTracing) Flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
            vulkanTools::CreateAndFillBuffer(
                App->VulkanObjects.VulkanDevice,
                Meshes[i].Indices.data(),
                IndexDataSize,
                &Meshes[i].VulkanObjects.IndexBuffer,
                Flags,
                CopyCommand,
                Queue
            );         
        }    

        //Global buffers
        size_t VertexDataSize = GVertices.size() * sizeof(vertex);
        VkBufferUsageFlags Flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if(App->RayTracing) Flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        vulkanTools::CreateAndFillBuffer(
            App->VulkanObjects.VulkanDevice,
            GVertices.data(),
            VertexDataSize,
            &VertexBuffer,
            Flags,
            CopyCommand,
            Queue
        );

        size_t IndexDataSize = GIndices.size() * sizeof(uint32_t);
        Flags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if(App->RayTracing) Flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        vulkanTools::CreateAndFillBuffer(
            App->VulkanObjects.VulkanDevice,
            GIndices.data(),
            IndexDataSize,
            &IndexBuffer,
            Flags,
            CopyCommand,
            Queue
        ); 
    }

    std::vector<VkDescriptorPoolSize> PoolSizes = 
    {
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  (uint32_t)Materials.size() * 5),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  (uint32_t)Materials.size()),
        vulkanTools::BuildDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  (uint32_t)NumInstances)
    };
    VkDescriptorPoolCreateInfo DescriptorPoolInfo = vulkanTools::BuildDescriptorPoolCreateInfo(
        (uint32_t)PoolSizes.size(),
        PoolSizes.data(),
        1 + (uint32_t)Materials.size() + (uint32_t)NumInstances
    );
    VK_CALL(vkCreateDescriptorPool(Device, &DescriptorPoolInfo, nullptr, &DescriptorPool));    
    Resources.DescriptorSets->DescriptorPool = DescriptorPool;
}

void scene::UpdateUniformBufferMatrices()
{
    Camera.SetAspectRatio((App->Width - ViewportStart) / App->Height);
    
    UBOSceneMatrices.Projection = App->Scene->Camera.GetProjectionMatrix();
    UBOSceneMatrices.View = App->Scene->Camera.GetViewMatrix();
    UBOSceneMatrices.InvProjection = glm::inverse(UBOSceneMatrices.Projection);
    UBOSceneMatrices.InvView = glm::inverse(UBOSceneMatrices.View);
    UBOSceneMatrices.PrevView = App->Scene->Camera.PrevInvModelMatrix;
    UBOSceneMatrices.Model = glm::mat4(1);
    UBOSceneMatrices.CameraPosition = App->Scene->Camera.worldPosition;
    UBOSceneMatrices.RenderSize.x = (float)App->Width;
    UBOSceneMatrices.RenderSize.y = (float)App->Height;

    VK_CALL(SceneMatrices.Map());
    SceneMatrices.CopyTo(&UBOSceneMatrices, sizeof(UBOSceneMatrices));
    SceneMatrices.Unmap();
}


void scene::CreateDescriptorSets()
{
    //Create Camera descriptors
    {
        //Matrices uniform buffer
        vulkanTools::CreateBuffer(App->VulkanObjects.VulkanDevice, 
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                    &SceneMatrices,
                                    sizeof(UBOSceneMatrices)
        );
        UpdateUniformBufferMatrices();

        //Create the scene descriptor set layout
        VkDescriptorSetLayoutBinding SetLayoutBindings = vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0 );
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(&SetLayoutBindings, 1);
        VkDescriptorSetLayout SceneDescriptorSetLayout = Resources.DescriptorSetLayouts->Add("Scene", DescriptorLayoutCreateInfo);

        //Allocate and write descriptor sets
        VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, &SceneDescriptorSetLayout, 1);
        VkDescriptorSet RendererDescriptorSet = Resources.DescriptorSets->Add("Scene", AllocInfo);
        VkWriteDescriptorSet WriteDescriptorSets = vulkanTools::BuildWriteDescriptorSet( RendererDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &SceneMatrices.VulkanObjects.Descriptor);
        vkUpdateDescriptorSets(Device, 1, &WriteDescriptorSets, 0, nullptr);
    }

    {
        //Create descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings = 
        {
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0 ),
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1 ),
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2 ),
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3 ),
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4 ),
            vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5 )
        };
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
        Resources.DescriptorSetLayouts->Add("Material", DescriptorLayoutCreateInfo);
        
        //Write descriptor sets
        for(uint32_t i=0; i<Materials.size(); i++)
        {
            VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, Resources.DescriptorSetLayouts->GetPtr("Material"), 1);
            VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &Materials[i].VulkanObjects.DescriptorSet));

            std::vector<VkWriteDescriptorSet> WriteDescriptorSets = 
            {
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &Materials[i].VulkanObjects.UniformBuffer.VulkanObjects.Descriptor),
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &Materials[i].Diffuse.Descriptor),
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &Materials[i].Specular.Descriptor),
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &Materials[i].Normal.Descriptor),
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &Materials[i].Occlusion.Descriptor),
                vulkanTools::BuildWriteDescriptorSet( Materials[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, &Materials[i].Emission.Descriptor)
            };
            vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
        }
    }

    {
        //Create descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> SetLayoutBindings;
        SetLayoutBindings.push_back(vulkanTools::BuildDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0 ));
        VkDescriptorSetLayoutCreateInfo DescriptorLayoutCreateInfo = vulkanTools::BuildDescriptorSetLayoutCreateInfo(SetLayoutBindings.data(), (uint32_t)SetLayoutBindings.size());
        Resources.DescriptorSetLayouts->Add("Instances", DescriptorLayoutCreateInfo);
        //Write descriptor sets
        for(auto &InstanceGroup : Instances)
        {
            for(uint32_t i=0; i<InstanceGroup.second.size(); i++)
            {
                VkDescriptorSetAllocateInfo AllocInfo = vulkanTools::BuildDescriptorSetAllocateInfo(DescriptorPool, Resources.DescriptorSetLayouts->GetPtr("Instances"), 1);
                VK_CALL(vkAllocateDescriptorSets(Device, &AllocInfo, &InstanceGroup.second[i].VulkanObjects.DescriptorSet));

                std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
                WriteDescriptorSets.push_back(
                    vulkanTools::BuildWriteDescriptorSet( InstanceGroup.second[i].VulkanObjects.DescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &InstanceGroup.second[i].VulkanObjects.UniformBuffer.VulkanObjects.Descriptor)
                );
                vkUpdateDescriptorSets(Device, (uint32_t)WriteDescriptorSets.size(), WriteDescriptorSets.data(), 0, nullptr);
            }
        }
    }    

    Cubemap.CreateDescriptorSet(App->VulkanObjects.VulkanDevice);
}

void cubemap::Destroy(vulkanDevice *VulkanDevice)
{
    VulkanObjects.Texture.Destroy(VulkanDevice);
    VulkanObjects.IrradianceMap.Destroy(VulkanDevice);
    VulkanObjects.PrefilteredMap.Destroy(VulkanDevice);
    VulkanObjects.BRDFLUT.Destroy(VulkanDevice);
    Mesh.Destroy();


    VulkanObjects.UniformBuffer.Destroy();

    vkDestroyDescriptorSetLayout(VulkanDevice->Device, VulkanObjects.DescriptorSetLayout, nullptr);
    vkFreeDescriptorSets(VulkanDevice->Device, VulkanObjects.DescriptorPool, 1, &VulkanObjects.DescriptorSet);
    vkDestroyDescriptorPool(VulkanDevice->Device, VulkanObjects.DescriptorPool, nullptr);
    vkDestroyPipeline(VulkanDevice->Device, VulkanObjects.Pipeline, nullptr);
}

void sceneMesh::Destroy()
{
    VulkanObjects.IndexBuffer.Destroy();
    VulkanObjects.VertexBuffer.Destroy();
}

void scene::Update()
{
    
    UpdateUniformBufferMatrices();
    Cubemap.UpdateUniforms();
    Camera.PrevInvModelMatrix = Camera.invModelMatrix;
}

void scene::Destroy()
{
    SceneMatrices.Destroy();
    Resources.Destroy();
    Cubemap.Destroy(App->VulkanObjects.VulkanDevice);

    VertexBuffer.Destroy();
    IndexBuffer.Destroy();

    for(size_t i=0; i<Meshes.size(); i++)
    {
        Meshes[i].Destroy();
    }
    for(size_t i=0; i<Materials.size(); i++)
    {
        Materials[i].VulkanObjects.UniformBuffer.Destroy();
        vkFreeDescriptorSets(Device, DescriptorPool, 1, &Materials[i].VulkanObjects.DescriptorSet);
    }
    for(auto &InstanceGroup : Instances)
    {
        for(size_t i=0; i<InstanceGroup.second.size(); i++)
        {
            InstanceGroup.second[i].VulkanObjects.UniformBuffer.Destroy();
            vkFreeDescriptorSets(Device, DescriptorPool, 1, &InstanceGroup.second[i].VulkanObjects.DescriptorSet);
        }
    }

    vkDestroyDescriptorPool(Device, DescriptorPool, nullptr);
}