#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "shader_reader.h"

//enum TextureType {TEX_DIFFUSE, TEX_SPECULAR};

struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
};

struct Texture {
	unsigned int id;
	std::string type;
	std::string path;
};

unsigned int TextureFromFile(const char* path, std::string& dir, aiTextureType type);

class Mesh {
	public:
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture> textures;

		Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures);
		void Draw(Shader& shader, unsigned int cubemapID);
		unsigned int VAO;

	private:
		unsigned int VBO, EBO;
		void setupMesh();
};

class Model {
	public:
		Model(const char* path);
		void Draw(Shader& shader, unsigned int cubemapID);
		std::vector<Mesh> meshes;
	private:
		std::vector<Texture> textures_loaded;
		
		std::string directory;

		void loadModel(std::string path);
		void processNode(aiNode* node, const aiScene* scene);
		Mesh processMesh(aiMesh* mesh, const aiScene* scene);
		std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName);
};

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
	this->vertices = vertices;
	this->indices = indices;
	this->textures = textures;

	setupMesh();
	//for (unsigned int i=0; i < this->textures.size(); i++)
		//std::cout << this << " " << this->textures[i].path << " " << this->textures[i].type << " " << this->textures[i].id << std::endl;
}

void Mesh::setupMesh() {
	//create OpenGL objects to store mesh data
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	//activate our OpenGL objects
	glBindVertexArray(VAO);

	//vertex buffer array
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
	//element buffer array
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

	//vertex positions
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	//vertex normals
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
	//vertex texture coordinates
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

	//reset OpenGL state
	glBindVertexArray(0);
}

void Mesh::Draw(Shader& shader, unsigned int cubemapID) {
	//std::cout << "Drawing mesh " << this << std::endl;
	glBindVertexArray(VAO);
	unsigned int diffuseNr = 0;
	unsigned int specularNr = 0;
	
	int i;
	for (i = 0; i < textures.size(); i++) {
		//std::cout << "Binding " << std::format("{} {} {}", textures[i].path, textures[i].id, textures[i].type) << std::endl;
		glActiveTexture(GL_TEXTURE0 + i);
		std::string number;
		std::string name = textures[i].type;
		if (name == "texture_diffuse") number = std::to_string(diffuseNr++);
		else if (name == "texture_specular") number = std::to_string(specularNr++);

		shader.setInt(("material." + name + number).c_str(), i);
		shader.setInt(std::format("{}{}", name, 1), i);
		glBindTexture(GL_TEXTURE_2D, textures[i].id);
	}
	//bind cubemap texture
	glActiveTexture(GL_TEXTURE0 + i);
	shader.setInt("cubemap", cubemapID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);
	//reset OpenGL state
	glActiveTexture(GL_TEXTURE0);

	//draw mesh
	
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

Model::Model(const char* path) {
	loadModel(path);
}

void Model::Draw(Shader& shader, unsigned int cubemapID) {
	for (unsigned int i = 0; i < meshes.size(); i++)
		meshes[i].Draw(shader, cubemapID);
}

void Model::loadModel(std::string path) {
	//std::cout << "Loading " << path << std::endl;
	Assimp::Importer import;
	const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		//std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		return;
	}
	directory = path.substr(0, path.find_last_of('\\')+1);
	processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}
	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene);
	}
}

//convert Assimp mesh to our mesh
Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

	//vertices
	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;
		vertex.Position = {
			mesh->mVertices[i].x,
			mesh->mVertices[i].y,
			mesh->mVertices[i].z
		};
		vertex.Normal = {
			mesh->mNormals[i].x,
			mesh->mNormals[i].y,
			mesh->mNormals[i].z
		};
		if (mesh->mTextureCoords[0]) {
			vertex.TexCoords = {
				mesh->mTextureCoords[0][i].x,
				mesh->mTextureCoords[0][i].y
			};
		}
		else vertex.TexCoords = { 0.0f, 0.0f };
		vertices.push_back(vertex);
	}

	//indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	//materials
	//std::cout << "Mesh material index: " << mesh->mMaterialIndex << std::endl;
	if (mesh->mMaterialIndex >= 0) {
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		//std::cout << "Loading diffuse texture" << std::endl;
		std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

		std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
	}
	return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName) {
	std::vector<Texture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
		aiString str;
		mat->GetTexture(type, i, &str);
		//std::cout << "Looking for texture " << str.C_Str() << std::endl;
		bool skip = false;
		for (unsigned int j = 0; j < textures_loaded.size(); j++) {
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
				textures.push_back(textures_loaded[j]);
				skip = true;
				break;
			}
		}
		if (!skip) {
			Texture texture;
			texture.id = TextureFromFile(str.C_Str(), directory, type);
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture);
		}
	}
	return textures;
}

unsigned int TextureFromFile(const char* path, std::string& dir, aiTextureType type) {
	unsigned int texID;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int width, height, nrChannels;
	std::string localPath = path;
	std::string filepath = dir + localPath;
	//std::cout << "Attempting to load texture at: " << localPath << "  " << dir << std::endl << filepath << std::endl;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);
	std::cout << nrChannels << " channels" << std::endl;
	if (data) {
		GLenum fileFormat = GL_RGB;
		GLenum internalFormat = GL_RGB;
		if (nrChannels == 4) {
			fileFormat = GL_RGBA;
			std::cout << "texture has four channels" << std::endl;
		}
		//format = GL_RGB;
		if (type == aiTextureType_DIFFUSE) internalFormat = GL_SRGB;
		//std::cout << "glTexImage2D attempt" << std::endl;
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, fileFormat, GL_UNSIGNED_BYTE, data);
		//std::cout << "glTexImage2D successful" << std::endl;
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else std::cout << "Failed to load texture " << filepath << std::endl;
	stbi_image_free(data);
	//std::cout << "done loading texture" << std::endl;
	return texID;
}