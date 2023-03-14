#include "MeshModel.h"

MeshModel::MeshModel()
{
}

MeshModel::MeshModel(std::vector<Mesh> newMeshList)
{
    meshList = newMeshList;
    model = glm::mat4(1.0f);
}

size_t MeshModel::getMeshCount()
{
    return meshList.size();
}

Mesh* MeshModel::getMesh(size_t index)
{
    if (index >= meshList.size())
    {
        throw std::runtime_error("Attempted to access invalid Mesh index");
    }
    return &meshList[index];
}

glm::mat4 MeshModel::getModel()
{
    return model;
}

void MeshModel::setModel(glm::mat4 newModel)
{
    model = newModel;
}

void MeshModel::destroyMeshModel()
{
    for (auto& mesh : meshList)
    {
        mesh.destroyBuffers();
    }
}

std::vector<std::string> MeshModel::loadMaterials(const aiScene* scene)
{
    // Create 1:1 sized list of textures
    std::vector<std::string> textureList(scene->mNumMaterials);
    
    for (size_t i = 0; i < scene->mNumMaterials; i++)
    {
        // Get material
        aiMaterial* material = scene->mMaterials[i];

        // Initiate to empty string
        textureList[i] = "";

        if (material->GetTextureCount(aiTextureType_DIFFUSE))
        {
            aiString path;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            {
                // Cut off any directory information from original files
                int idx = std::string(path.data).rfind("\\");
                std::string filename = std::string(path.data).substr(idx + 1);

                textureList[i] = filename;
            }
        }
    }

    return textureList;
}

std::vector<Mesh> MeshModel::loadNode(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, 
    aiNode* node, const aiScene* scene, std::vector<int> matToTex)
{
    std::vector<Mesh> meshList;
    
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
        meshList.push_back(loadMesh(newPhysicalDevice, newDevice, transferQueue, transferCommandPool,
            scene->mMeshes[node->mMeshes[i]], scene, matToTex));
    }

    // Go through each node attached to this node and load it, then append to mesh list
    for (size_t i = 0; i < node->mNumChildren; i++)
    {
        std::vector<Mesh> newList = loadNode(newPhysicalDevice, newDevice, transferQueue, transferCommandPool,
            node->mChildren[i], scene, matToTex);
        meshList.insert(meshList.end(), newList.begin(), newList.end());

    }

    return meshList;
}

Mesh MeshModel::loadMesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, 
    aiMesh* mesh, const aiScene* scene, std::vector<int> matToTex)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Resize vertex list to hold all vertices for mesh
    vertices.resize(mesh->mNumVertices);

    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        // Set position
        vertices[i].pos = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
        vertices[i].normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

        // Set texture coords (does not necessarily exist)
        if (mesh->mTextureCoords[0])
        {
            vertices[i].tex = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
        }
        else
        {
            vertices[i].tex = { 0.0, 0.0 };
        }

        // Set color (just use white for now)
        vertices[i].col = { 1.0f, 1.0f, 1.0f };
    }

    // Iterate over indices through faces and copy accross
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        // Get a gace
        aiFace face = mesh->mFaces[i];

        // Go through faces indices
        for (size_t j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);

        }
    }

    // Create new Mesh with details and return it
    Mesh newMesh = Mesh(newPhysicalDevice, newDevice, transferQueue, transferCommandPool,
        &vertices, &indices, matToTex[mesh->mMaterialIndex]);

    return newMesh;

}

MeshModel::~MeshModel()
{
}
