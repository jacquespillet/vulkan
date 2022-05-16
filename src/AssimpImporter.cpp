#include "AssimpImporter.h"
#include <vector>
#include "Scene.h"
#include <glm/ext.hpp>
#include "Resources.h"


namespace assimpImporter
{
void Load(std::string FileName,  std::vector<sceneMesh> &Meshes, std::vector<sceneMaterial> &Materials,std::vector<vertex> &GVertices, 
            std::vector<uint32_t> &GIndices, textureList *Textures)
{
    Assimp::Importer Importer;
    int Flags = aiProcess_FlipWindingOrder | aiProcess_Triangulate | aiProcess_PreTransformVertices | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;

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
					Materials[i].Bump = Textures->AddTexture2D(TextureFileName, "resources/models/sponza/" + TextureFileName, VK_FORMAT_BC2_UNORM_BLOCK);
					AScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_NORMALS(0), Materials[i].MaterialData.NormalMapTextureID);
				}
				else
				{
					Materials[i].Bump = Textures->Get(TextureFileName);
				}
			}
			else
			{
				Materials[i].Bump = Textures->Get("Dummy.Bump");
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
            
            std::vector<vertex> Vertices(AMesh->mNumVertices);

            bool HasUV = AMesh->HasTextureCoords(0);
            bool HasTangent = AMesh->HasTangentsAndBitangents();
            uint32_t VertexBase = (uint32_t)GVertices.size();

            for(size_t j=0; j<Vertices.size(); j++)
            {
                Vertices[j].Position = glm::make_vec3(&AMesh->mVertices[j].x);
                Vertices[j].Position.y = -Vertices[j].Position.y;
                Vertices[j].UV = (HasUV) ? glm::make_vec2(&AMesh->mTextureCoords[0][j].x) : glm::vec2(0);
                Vertices[j].Normal = glm::make_vec3(&AMesh->mNormals[j].x);
                Vertices[j].Color = glm::vec3(1.0f);
                Vertices[j].Tangent = (HasTangent) ? glm::make_vec3(&AMesh->mTangents[j].x) : glm::vec3(0,1,0);
                Vertices[j].Bitangent = (HasTangent) ? glm::make_vec3(&AMesh->mBitangents[j].x) : glm::vec3(0,1,0);
                GVertices.push_back(Vertices[j]);
            }

            std::vector<uint32_t> Indices;
            Meshes[i].IndexCount = AMesh->mNumFaces * 3;
            Indices.resize(AMesh->mNumFaces * 3);

            for(uint32_t j=0; j<AMesh->mNumFaces; j++)
            {
                Indices[j * 3 + 0] = AMesh->mFaces[j].mIndices[0];
                Indices[j * 3 + 1] = AMesh->mFaces[j].mIndices[1];
                Indices[j * 3 + 2] = AMesh->mFaces[j].mIndices[2];
                GIndices.push_back(Indices[j*3+0] + VertexBase);
                GIndices.push_back(Indices[j*3+1] + VertexBase);
                GIndices.push_back(Indices[j*3+2] + VertexBase);
                GIndexBase+=3;
            }
        }    
    }
}
}