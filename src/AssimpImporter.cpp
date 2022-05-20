#include "AssimpImporter.h"
#include <vector>
#include "Scene.h"
#include <glm/ext.hpp>
#include "Resources.h"

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#include <assimp/material.h>


namespace assimpImporter
{
void Load(std::string FileName,  std::unordered_map<int, std::vector<instance>> &Instances, std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures)
{
    Assimp::Importer Importer;
    int Flags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

    const aiScene *AScene = Importer.ReadFile(FileName.c_str(), Flags);
    

	//Materials
	{
		Textures->AddTexture2D("Dummy.Diffuse", "resources/models/sponza/dummy.dds", VK_FORMAT_BC2_UNORM_BLOCK);
		Textures->AddTexture2D("Dummy.Specular", "resources/models/sponza/dummy_specular.dds", VK_FORMAT_BC2_UNORM_BLOCK);
		Textures->AddTexture2D("Dummy.Bump", "resources/models/sponza/dummy_ddn.dds", VK_FORMAT_BC2_UNORM_BLOCK);

		Materials.resize(AScene->mNumMaterials);
		for (uint32_t i = 0; i < Materials.size(); i++)
		{
			Materials[i] = {};

			aiString Name;
			AScene->mMaterials[i]->Get(AI_MATKEY_NAME, Name);
			Materials[i].Name = Name.C_Str();

			aiString TextureFile;
			std::string DiffuseMapFile;

			AScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &TextureFile);
			if (AScene->mMaterials[i]->GetTextureCount(aiTextureType_DIFFUSE) > 0)
			{
				std::string TextureFileName = std::string(TextureFile.C_Str());
				DiffuseMapFile = TextureFileName;
				std::replace(TextureFileName.begin(), TextureFileName.end(), '\\', '/');
				if (!Textures->Present(TextureFileName))
				{
					Materials[i].Diffuse = Textures->AddTexture2D(TextureFileName, "resources/models/sponza/" + TextureFileName, VK_FORMAT_BC2_UNORM_BLOCK);
					AScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), Materials[i].MaterialData.BaseColorTextureID);
				}
				else
				{
					Materials[i].Diffuse = Textures->Get(TextureFileName);
				}
			}
			else
			{
				Materials[i].Diffuse = Textures->Get("Dummy.Diffuse");
			}


			if (AScene->mMaterials[i]->GetTextureCount(aiTextureType_SPECULAR) > 0)
			{
				AScene->mMaterials[i]->GetTexture(aiTextureType_SPECULAR, 0, &TextureFile);
				std::string TextureFileName = std::string(TextureFile.C_Str());
				std::replace(TextureFileName.begin(), TextureFileName.end(), '\\', '/');
				if (!Textures->Present(TextureFileName))
				{
					Materials[i].Specular = Textures->AddTexture2D(TextureFileName, "resources/models/sponza/" + TextureFileName, VK_FORMAT_BC2_UNORM_BLOCK);
					AScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_SPECULAR(0), Materials[i].MaterialData.MetallicRoughnessTextureID);
				}
				else
				{
					Materials[i].Specular = Textures->Get(TextureFileName);
				}
			}
			else
			{
				Materials[i].Specular = Textures->Get("Dummy.Specular");
			}

			if (AScene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
			{
				AScene->mMaterials[i]->GetTexture(aiTextureType_NORMALS, 0, &TextureFile);
				std::string TextureFileName = std::string(TextureFile.C_Str());
				std::replace(TextureFileName.begin(), TextureFileName.end(), '\\', '/');
				if (!Textures->Present(TextureFileName))
				{
					Materials[i].Normal = Textures->AddTexture2D(TextureFileName, "resources/models/sponza/" + TextureFileName, VK_FORMAT_BC2_UNORM_BLOCK);
					AScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_NORMALS(0), Materials[i].MaterialData.NormalMapTextureID);
				}
				else
				{
					Materials[i].Normal = Textures->Get(TextureFileName);
				}
			}
			else
			{
				Materials[i].Normal = Textures->Get("Dummy.Bump");
			}

			if (AScene->mMaterials[i]->GetTextureCount(aiTextureType_OPACITY) > 0)
			{
				Materials[i].HasAlpha = true;
			}

			AScene->mMaterials[i]->Get(AI_MATKEY_OPACITY, Materials[i].MaterialData.Opacity);
			Materials[i].MaterialData.AlphaMode = alphaMode::Opaque;

			AScene->mMaterials[i]->Get(AI_MATKEY_ROUGHNESS_FACTOR, Materials[i].MaterialData.Roughness);
			AScene->mMaterials[i]->Get(AI_MATKEY_METALLIC_FACTOR, Materials[i].MaterialData.Metallic);

			aiColor3D Emission;
			AScene->mMaterials[i]->Get(AI_MATKEY_COLOR_EMISSIVE, Emission);
			Materials[i].MaterialData.Emission = glm::vec3(Emission.r, Emission.g, Emission.b);

			aiColor3D Color;
			AScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, Color);
			Materials[i].MaterialData.BaseColor = glm::vec3(Color.r, Color.g, Color.b);

			Materials[i].CalculateFlags();

		}
	}

    //Meshes
    {
        uint32_t GIndexBase=0;
        Meshes.resize(AScene->mNumMeshes);

        for(uint32_t i=0; i<Meshes.size(); i++)
        {
            aiMesh *AMesh = AScene->mMeshes[i];

            Meshes[i].Material = &Materials[AMesh->mMaterialIndex];
            Meshes[i].IndexBase = GIndexBase;
            
            // std::vector<vertex> Vertices(AMesh->mNumVertices);
			Meshes[i].Vertices.resize(AMesh->mNumVertices);
            bool HasUV = AMesh->HasTextureCoords(0);
            bool HasTangent = AMesh->HasTangentsAndBitangents();
            uint32_t VertexBase = (uint32_t)GVertices.size();

            for(size_t j=0; j<Meshes[i].Vertices.size(); j++)
            {
                glm::vec2 UV = (HasUV) ? glm::make_vec2(&AMesh->mTextureCoords[0][j].x) : glm::vec2(0);
                Meshes[i].Vertices[j].Position = glm::vec4(AMesh->mVertices[j].x, AMesh->mVertices[j].y, AMesh->mVertices[j].z, UV.x);
				Meshes[i].Vertices[j].Position *= 0.01f;
                Meshes[i].Vertices[j].Normal = glm::vec4(AMesh->mNormals[j].x, AMesh->mNormals[j].y, AMesh->mNormals[j].z, UV.y);
                Meshes[i].Vertices[j].Tangent = (HasTangent) ? glm::make_vec4(&AMesh->mTangents[j].x) : glm::vec4(0,1,0,0);
                GVertices.push_back(Meshes[i].Vertices[j]);
            }

            Meshes[i].IndexCount = AMesh->mNumFaces * 3;
            Meshes[i].Indices.resize(AMesh->mNumFaces * 3);

            for(uint32_t j=0; j<AMesh->mNumFaces; j++)
            {
                Meshes[i].Indices[j * 3 + 0] = AMesh->mFaces[j].mIndices[0];
                Meshes[i].Indices[j * 3 + 2] = AMesh->mFaces[j].mIndices[1];
                Meshes[i].Indices[j * 3 + 1] = AMesh->mFaces[j].mIndices[2];
                GIndices.push_back(Meshes[i].Indices[j*3+0] + VertexBase);
                GIndices.push_back(Meshes[i].Indices[j*3+1] + VertexBase);
                GIndices.push_back(Meshes[i].Indices[j*3+2] + VertexBase);
                GIndexBase+=3;
            }
			
			instance Instance = {};
			Instance.InstanceData.Transform = glm::mat4(1.0f);
			Instance.Mesh = &Meshes[i];
			
			int MatFlag = Instance.Mesh->Material->Flags;
			Instances[MatFlag].push_back(Instance);
        }    
    }
}
}