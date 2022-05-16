#include "GLTFImporter.h"

#include <vector>
#include "Scene.h"
#include <glm/ext.hpp>
#include "Resources.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma warning ( disable : 4996 )
#include <stb_image_write.h>
#pragma warning ( default : 4996 )


#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_RESIZE
#include <tiny_gltf.h>

namespace GLTFImporter
{
    void LoadTextures(tinygltf::Model &GLTFModel, textureList *Textures)
    {
		Textures->AddTexture2D("Dummy.Diffuse", "resources/models/sponza/dummy.dds", VK_FORMAT_BC2_UNORM_BLOCK);
		Textures->AddTexture2D("Dummy.Specular", "resources/models/sponza/dummy_specular.dds", VK_FORMAT_BC2_UNORM_BLOCK);
		Textures->AddTexture2D("Dummy.Bump", "resources/models/sponza/dummy_ddn.dds", VK_FORMAT_BC2_UNORM_BLOCK);

        for (size_t i = 0; i < GLTFModel.textures.size(); i++)
        {
            tinygltf::Texture& GLTFTex = GLTFModel.textures[i];
            tinygltf::Image GLTFImage = GLTFModel.images[GLTFTex.source];
            std::string TexName = GLTFTex.name;
            if(strcmp(GLTFTex.name.c_str(), "") == 0)
            {
                TexName = GLTFImage.uri;
            }
            VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;
            assert(GLTFImage.component==4);
            assert(GLTFImage.bits==8);
            Textures->AddTexture2D(TexName, GLTFImage.image.data(), GLTFImage.image.size(), GLTFImage.width, GLTFImage.height, Format);
        }
    }
    void LoadMaterials(tinygltf::Model &GLTFModel, std::vector<sceneMaterial> &Materials, textureList *Textures)
    {
        
        // AScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), Materials[i].MaterialData.BaseColorTextureID);
        Materials.resize(GLTFModel.materials.size());
        
        for (size_t i = 0; i < GLTFModel.materials.size(); i++)
        {
            const tinygltf::Material GLTFMaterial = GLTFModel.materials[i];
            const tinygltf::PbrMetallicRoughness PBR = GLTFMaterial.pbrMetallicRoughness;

            Materials[i].MaterialData.BaseColor = glm::vec3((float)PBR.baseColorFactor[0], (float)PBR.baseColorFactor[1], (float)PBR.baseColorFactor[2]);
            if(PBR.baseColorTexture.index > -1)
            {
                int TexIndex = PBR.baseColorTexture.index;
                Materials[i].MaterialData.BaseColorTextureID = TexIndex;

                tinygltf::Texture& GLTFTex = GLTFModel.textures[TexIndex];
                tinygltf::Image GLTFImage = GLTFModel.images[GLTFTex.source];
                std::string TexName = GLTFTex.name;
                if(strcmp(GLTFTex.name.c_str(), "") == 0)
                {
                    TexName = GLTFImage.uri;
                    // TexName = "Dummy.Diffuse";
                }
                else
                {
                    TexName = "Dummy.Diffuse";
                }
                Materials[i].Diffuse = Textures->Get(TexName);
            }

            Materials[i].MaterialData.Opacity = (float)PBR.baseColorFactor[3];
            Materials[i].MaterialData.AlphaCutoff = (float) GLTFMaterial.alphaCutoff;
            if (strcmp(GLTFMaterial.alphaMode.c_str(), "OPAQUE") == 0) Materials[i].MaterialData.AlphaMode = alphaMode::Opaque;
            else if (strcmp(GLTFMaterial.alphaMode.c_str(), "BLEND") == 0) Materials[i].MaterialData.AlphaMode = alphaMode::Blend;
            else if (strcmp(GLTFMaterial.alphaMode.c_str(), "MASK") == 0) Materials[i].MaterialData.AlphaMode = alphaMode::Mask;

            Materials[i].MaterialData.Roughness = sqrtf((float)PBR.roughnessFactor);
            Materials[i].MaterialData.Metallic = (float)PBR.metallicFactor;
            if(PBR.metallicRoughnessTexture.index>-1)
            {
                int TexIndex = PBR.baseColorTexture.index;
                Materials[i].MaterialData.MetallicRoughnessTextureID = TexIndex;

                tinygltf::Texture& GLTFTex = GLTFModel.textures[TexIndex];
                tinygltf::Image GLTFImage = GLTFModel.images[GLTFTex.source];
                std::string TexName = GLTFTex.name;
                if(strcmp(GLTFTex.name.c_str(), "") == 0)
                {
                    TexName = GLTFImage.uri;
                    TexName = "Dummy.Specular";
                }
                else
                {
                    TexName = "Dummy.Specular";
                }
                Materials[i].Specular = Textures->Get(TexName);                
            }
            else
            {
                Materials[i].Specular = Textures->Get("Dummy.Specular");         
                Materials[i].MaterialData.UseMetallicRoughness=0;
            }
            
            if(GLTFMaterial.normalTexture.index>-1)
            {
                int TexIndex = GLTFMaterial.normalTexture.index;
                Materials[i].MaterialData.NormalMapTextureID = TexIndex;

                tinygltf::Texture& GLTFTex = GLTFModel.textures[TexIndex];
                tinygltf::Image GLTFImage = GLTFModel.images[GLTFTex.source];
                std::string TexName = GLTFTex.name;
                if(strcmp(GLTFTex.name.c_str(), "") == 0)
                {
                    TexName = GLTFImage.uri;
                    TexName = "Dummy.Bump";
                }
                else
                {
                    TexName = "Dummy.Bump";
                }
                Materials[i].Bump = Textures->Get(TexName);                
            }
            else
            {
                Materials[i].Bump = Textures->Get("Dummy.Bump");                
                Materials[i].MaterialData.UseMetallicRoughness=0;
            }


            if(GLTFMaterial.occlusionTexture.index>-1)
            {
                Materials[i].MaterialData.OcclusionMapTextureID = GLTFMaterial.occlusionTexture.index;
            }
            else
            {
                Materials[i].MaterialData.UseOcclusionMap=0;
            }


            Materials[i].MaterialData.Emission = glm::vec3((float) GLTFMaterial.emissiveFactor[0], (float) GLTFMaterial.emissiveFactor[1], (float) GLTFMaterial.emissiveFactor[2]);
            if(GLTFMaterial.emissiveTexture.index>-1)
            {
                Materials[i].MaterialData.EmissionMapTextureID = GLTFMaterial.emissiveTexture.index;
            }
            else
            {
                Materials[i].MaterialData.UseEmissionMap= 0;
            }

            // if(GLTFMaterial.extensions.find("KHR_materials_transmission") != GLTFMaterial.extensions.end())
            // {
            //     const auto &ext = GLTFMaterial.extensions.find("KHR_materials_transmission")->second;
            //     if(ext.Has("transmissionFactor"))
            //     {
            //         Materials[i].MaterialData.SpecTrans = (float)(ext.Get("transmissionFactor").Get<double>());
            //     }
            // }

            // if (GLTFMaterial.extensions.find("KHR_materials_clearcoat") != GLTFMaterial.extensions.end())
            // {
            //     const auto &ext = GLTFMaterial.extensions.find("KHR_materials_clearcoat")->second;
            //     if (ext.Has("clearcoatFactor"))
            //     {
            //         Materials[i].MaterialData.ClearcoatFactor = (float)(ext.Get("clearcoatFactor").Get<double>());
            //         Materials[i].MaterialData.HasClearcoat=1;
            //     }
            //     if (ext.Has("clearcoatRoughnessFactor"))
            //     {
            //         Materials[i].MaterialData.ClearcoatRoughness = (float)(ext.Get("clearcoatRoughnessFactor").Get<double>());
            //     }
            // }
            // else
            // {
            //     Materials[i].MaterialData.HasClearcoat=0;
            // }
        }
    }    


    void LoadMeshes(tinygltf::Model &GLTFModel, std::vector<sceneMesh> &Meshes, std::vector<uint32_t> &GIndices, std::vector<vertex> &GVertices, std::vector<sceneMaterial> &Materials, std::vector<std::vector<sceneMesh*>> &InstanceMapping)
    {
        uint32_t GIndexBase=0;
        InstanceMapping.resize(GLTFModel.meshes.size());
        for(int MeshIndex=0; MeshIndex<GLTFModel.meshes.size(); MeshIndex++)
        {
            tinygltf::Mesh gltfMesh = GLTFModel.meshes[MeshIndex];
            uint32_t BaseIndex = (uint32_t)Meshes.size();
            Meshes.resize(BaseIndex + gltfMesh.primitives.size());
            InstanceMapping[MeshIndex].resize(gltfMesh.primitives.size());
            for(int j=0; j<gltfMesh.primitives.size(); j++)
            {
                tinygltf::Primitive GLTFPrimitive = gltfMesh.primitives[j];

                if(GLTFPrimitive.mode != TINYGLTF_MODE_TRIANGLES)
                    continue;
                
                int IndicesIndex = GLTFPrimitive.indices;
                int PositionIndex = -1;
                int NormalIndex = -1;
                int TangentIndex = -1;
                int UVIndex=-1;

                if(GLTFPrimitive.attributes.count("POSITION") >0)
                    PositionIndex = GLTFPrimitive.attributes["POSITION"];
                if(GLTFPrimitive.attributes.count("NORMAL") >0)
                    NormalIndex = GLTFPrimitive.attributes["NORMAL"];
                if(GLTFPrimitive.attributes.count("TANGENT") >0)
                    TangentIndex = GLTFPrimitive.attributes["TANGENT"];
                if(GLTFPrimitive.attributes.count("TEXCOORD_0") >0)
                    UVIndex = GLTFPrimitive.attributes["TEXCOORD_0"];

                //Positions
                tinygltf::Accessor PositionAccessor = GLTFModel.accessors[PositionIndex];
                tinygltf::BufferView PositionBufferView = GLTFModel.bufferViews[PositionAccessor.bufferView];
                const tinygltf::Buffer &PositionBuffer = GLTFModel.buffers[PositionBufferView.buffer];
                const uint8_t *PositionBufferAddress = PositionBuffer.data.data();
                //3 * float
                int PositionStride = tinygltf::GetComponentSizeInBytes(PositionAccessor.componentType) * tinygltf::GetNumComponentsInType(PositionAccessor.type);
                if(PositionBufferView.byteStride > 0) PositionStride = (int)PositionBufferView.byteStride;

                //Normals
                tinygltf::Accessor NormalAccessor;
                tinygltf::BufferView NormalBufferView;
                const uint8_t *NormalBufferAddress=0;
                int NormalStride=0;
                if(NormalIndex > 0)
                {
                    NormalAccessor = GLTFModel.accessors[NormalIndex];
                    NormalBufferView = GLTFModel.bufferViews[NormalAccessor.bufferView];
                    const tinygltf::Buffer &normalBuffer = GLTFModel.buffers[NormalBufferView.buffer];
                    NormalBufferAddress = normalBuffer.data.data();
                    //3 * float
                    NormalStride = tinygltf::GetComponentSizeInBytes(NormalAccessor.componentType) * tinygltf::GetNumComponentsInType(NormalAccessor.type);
                    if(NormalBufferView.byteStride > 0) NormalStride =(int) NormalBufferView.byteStride;
                }

                //Tangents
                tinygltf::Accessor TangentAccessor;
                tinygltf::BufferView TangentBufferView;
                const uint8_t *TangentBufferAddress=0;
                int TangentStride=0;
                if(TangentIndex > 0)
                {
                    TangentAccessor = GLTFModel.accessors[TangentIndex];
                    TangentBufferView = GLTFModel.bufferViews[TangentAccessor.bufferView];
                    const tinygltf::Buffer &TangentBuffer = GLTFModel.buffers[TangentBufferView.buffer];
                    TangentBufferAddress = TangentBuffer.data.data();
                    //3 * float
                    TangentStride = tinygltf::GetComponentSizeInBytes(TangentAccessor.componentType) * tinygltf::GetNumComponentsInType(TangentAccessor.type);
                    if(TangentBufferView.byteStride > 0) TangentStride =(int) TangentBufferView.byteStride;
                }

                //UV
                tinygltf::Accessor UVAccessor;
                tinygltf::BufferView UVBufferView;
                const uint8_t *UVBufferAddress=0;
                int UVStride=0;
                if(UVIndex > 0)
                {
                    UVAccessor = GLTFModel.accessors[UVIndex];
                    UVBufferView = GLTFModel.bufferViews[UVAccessor.bufferView];
                    const tinygltf::Buffer &uvBuffer = GLTFModel.buffers[UVBufferView.buffer];
                    UVBufferAddress = uvBuffer.data.data();
                    //3 * float
                    UVStride = tinygltf::GetComponentSizeInBytes(UVAccessor.componentType) * tinygltf::GetNumComponentsInType(UVAccessor.type);
                    if(UVBufferView.byteStride > 0) UVStride = (int)UVBufferView.byteStride;
                }

                //Indices
                tinygltf::Accessor IndicesAccessor = GLTFModel.accessors[IndicesIndex];
                tinygltf::BufferView IndicesBufferView = GLTFModel.bufferViews[IndicesAccessor.bufferView];
                const tinygltf::Buffer &IndicesBuffer = GLTFModel.buffers[IndicesBufferView.buffer];
                const uint8_t *IndicesBufferAddress = IndicesBuffer.data.data();
                int IndicesStride = tinygltf::GetComponentSizeInBytes(IndicesAccessor.componentType) * tinygltf::GetNumComponentsInType(IndicesAccessor.type); 

                //Fill geometry buffers
                std::vector<glm::vec3> Positions;
                std::vector<glm::vec3> Normals;
                std::vector<glm::vec3> Tangents;
                std::vector<glm::vec2> UVs;
                for (size_t k = 0; k < PositionAccessor.count; k++)
                {
                    glm::vec3 Position, Normal, Tangent;
                    glm::vec2 UV;

                    {
                        const uint8_t *address = PositionBufferAddress + PositionBufferView.byteOffset + PositionAccessor.byteOffset + (k * PositionStride);
                        memcpy(&Position, address, 12);
                    }

                    if(NormalIndex>0)
                    {
                        const uint8_t *address = NormalBufferAddress + NormalBufferView.byteOffset + NormalAccessor.byteOffset + (k * NormalStride);
                        memcpy(&Normal, address, 12);
                    }

                    if(TangentIndex>0)
                    {
                        const uint8_t *address = TangentBufferAddress + TangentBufferView.byteOffset + TangentAccessor.byteOffset + (k * TangentStride);
                        memcpy(&Tangent, address, 12);
                    }

                    if(UVIndex>0)
                    {
                        const uint8_t *address = UVBufferAddress + UVBufferView.byteOffset + UVAccessor.byteOffset + (k * UVStride);
                        memcpy(&UV, address, 8);
                    }

                    Positions.push_back(Position);
                    Normals.push_back(Normal);
                    Tangents.push_back(Tangent);
                    UVs.push_back(UV);
                }

                //Fill indices buffer
                std::vector<int> indices(IndicesAccessor.count);
                const uint8_t *baseAddress = IndicesBufferAddress + IndicesBufferView.byteOffset + IndicesAccessor.byteOffset;
                if(IndicesStride == 1)
                {
                    std::vector<uint8_t> Quarter;
                    Quarter.resize(IndicesAccessor.count);
                    memcpy(Quarter.data(), baseAddress, (IndicesAccessor.count) * IndicesStride);
                    for(size_t i=0; i<IndicesAccessor.count; i++)
                    {
                        indices[i] = Quarter[i];
                    }
                }
                else if(IndicesStride == 2)
                {
                    std::vector<uint16_t> Half;
                    Half.resize(IndicesAccessor.count);
                    memcpy(Half.data(), baseAddress, (IndicesAccessor.count) * IndicesStride);
                    for(size_t i=0; i<IndicesAccessor.count; i++)
                    {
                        indices[i] = Half[i];
                    }
                }
                else
                {
                    memcpy(indices.data(), baseAddress, (IndicesAccessor.count) * IndicesStride);
                }

                Meshes[BaseIndex + j].IndexBase = GIndexBase;
                Meshes[BaseIndex + j].Material = &Materials[GLTFPrimitive.material];
                InstanceMapping[MeshIndex][j] = &Meshes[BaseIndex + j];

                uint32_t VertexBase = (uint32_t) GVertices.size();
                for (size_t k = 0; k < indices.size(); k++)
                {
                    glm::vec3 Position = Positions[indices[k]];
                    glm::vec3 Normal = Normals[indices[k]];
                    glm::vec3 Tangent = Tangents[indices[k]];
                    glm::vec2 UV = UVs[indices[k]];

                    GVertices.push_back({
                        Position, 
                        UV,
                        glm::vec3(1,1,1),
                        Normal, 
                        Tangent,
                        glm::vec3(0,0,1)
                    });
                }
                
                Meshes[BaseIndex + j].IndexCount = (uint32_t)indices.size();
                for(uint32_t k=0; k<indices.size(); k++)
                {
                    GIndices.push_back(VertexBase + k);
                    GIndexBase++;
                }
            }
        }
    }    

    void TraverseNodes(tinygltf::Model &GLTFModel, uint32_t nodeIndex, glm::mat4 parentTransform, std::vector<sceneMesh> &Meshes, std::vector<instance> &Instances, std::vector<std::vector<sceneMesh*>> &InstanceMapping)
    {
        tinygltf::Node GLTFNode = GLTFModel.nodes[nodeIndex];

        glm::mat4 LocalTransform;
        if(GLTFNode.matrix.size() > 0)
        {
            LocalTransform[0][0] = (float)GLTFNode.matrix[0]; LocalTransform[0][1] = (float)GLTFNode.matrix[1]; LocalTransform[0][2] = (float)GLTFNode.matrix[2]; LocalTransform[0][3] = (float)GLTFNode.matrix[3];
            LocalTransform[1][0] = (float)GLTFNode.matrix[4]; LocalTransform[1][1] = (float)GLTFNode.matrix[5]; LocalTransform[1][2] = (float)GLTFNode.matrix[6]; LocalTransform[1][3] = (float)GLTFNode.matrix[7];
            LocalTransform[2][0] = (float)GLTFNode.matrix[8]; LocalTransform[2][1] = (float)GLTFNode.matrix[9]; LocalTransform[2][2] = (float)GLTFNode.matrix[10]; LocalTransform[2][3] = (float)GLTFNode.matrix[11];
            LocalTransform[3][0] = (float)GLTFNode.matrix[12]; LocalTransform[3][1] = (float)GLTFNode.matrix[13]; LocalTransform[3][2] = (float)GLTFNode.matrix[14]; LocalTransform[3][3] = (float)GLTFNode.matrix[15];
        }
        else
        {
            glm::mat4 translate, rotation, scale;
            if(GLTFNode.translation.size()>0)
            {
                translate[3][0] = (float)GLTFNode.translation[0];
                translate[3][1] = (float)GLTFNode.translation[1];
                translate[3][2] = (float)GLTFNode.translation[2];
            }

            if(GLTFNode.rotation.size() > 0)
            {
                glm::quat q((float)GLTFNode.rotation[3], (float)GLTFNode.rotation[0], (float)GLTFNode.rotation[1], (float)GLTFNode.rotation[2]);
                rotation = glm::toMat4(q);
            }

            if(GLTFNode.scale.size() > 0)
            {
                scale[0][0] = (float)GLTFNode.scale[0];
                scale[1][1] = (float)GLTFNode.scale[1];
                scale[2][2] = (float)GLTFNode.scale[2];
            }

            LocalTransform = scale * rotation * translate;
        }

        glm::mat4 Transform = glm::mat4(1);

        //Leaf node
        if(GLTFNode.children.size() == 0 && GLTFNode.mesh != -1)
        {

            // std::vector<instance> Primitives = Meshes[GLTFNode.mesh];
            // for(s32 i=0; i<Primitives.size(); i++)
            // {
            //     std::string Name = GLTFNode.name;
            //     if(strcmp(Name.c_str(), "") == 0)
            //     {
            //         Name = "Mesh" + std::to_string(GLTFNode.mesh) + " Prim " + std::to_string(Primitives[i].primitiveId);
            //     }
            tinygltf::Mesh Mesh = GLTFModel.meshes[GLTFNode.mesh];
            for(int i=0; i<Mesh.primitives.size(); i++)
            {
                
                instance Instance = {};
                Instance.InstanceData.Transform = Transform;
                Instance.Mesh = InstanceMapping[GLTFNode.mesh][i];
                Instances.push_back(Instance);

                //     meshInstance instance(Name, Primitives[i].primitiveId, Transform, Primitives[i].materialId < 0 ? 0 : Primitives[i].materialId);
                //     // Scene->AddMeshInstance(instance);
                //     Instances.push_back(instance);
                // }
            }
            
        }

        for (size_t i = 0; i < GLTFNode.children.size(); i++)
        {
            TraverseNodes(GLTFModel, GLTFNode.children[i], Transform, Meshes, Instances, InstanceMapping);
        }
        

    }

    void LoadInstances(tinygltf::Model &GLTFModel, std::vector<sceneMesh> &Meshes, std::vector<instance> &Instances,  std::vector<std::vector<sceneMesh*>> &InstanceMapping)
    {
        glm::mat4 RootTransform(1.0f);
        const tinygltf::Scene GLTFScene = GLTFModel.scenes[GLTFModel.defaultScene];
        for (size_t i = 0; i < GLTFScene.nodes.size(); i++)
        {
            TraverseNodes(GLTFModel, GLTFScene.nodes[i], RootTransform, Meshes, Instances, InstanceMapping);
        }
    }

    bool Load(std::string FileName,  std::vector<instance> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures)
    {
        tinygltf::Model GLTFModel;
        tinygltf::TinyGLTF ModelLoader;

        std::string Error, Warning;

        //TODO(Jacques): 
        //  Handle binary gltf
        std::string Extension = FileName.substr(FileName.find_last_of(".") + 1);
        bool Result = false;
        if(Extension == "gltf")
        {
            Result = ModelLoader.LoadASCIIFromFile(&GLTFModel, &Error, &Warning, FileName);
        }
        else if(Extension == "glb")
        {
            Result = ModelLoader.LoadBinaryFromFile(&GLTFModel, &Error, &Warning, FileName);
        }
        else Result=false;
            
        if(!Result) 
        {
            std::cout << "Could not load model " << FileName << std::endl;
            return false;
        }

        // InstanceMap->clear();
        std::vector<std::vector<sceneMesh*>> InstanceMapping;
        LoadTextures(GLTFModel, Textures);
        LoadMaterials(GLTFModel, Materials, Textures);
        LoadMeshes(GLTFModel, Meshes, GIndices, GVertices, Materials, InstanceMapping);
        LoadInstances(GLTFModel, Meshes, Instances, InstanceMapping);

        return true;
    }
};