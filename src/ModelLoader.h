#pragma once
#include "Types.h"
#include <string>
#include <vector>

namespace tinygltf
{
	class Model;
	class Node;
}

class ModelLoader
{
public:
	// 加载 GLTF/GLB 文件并输出展平后的 SceneData
	bool loadGLTF(const std::string &filepath, SceneData &outScene);

private:
	void processMaterials(const tinygltf::Model &gltfModel, SceneData &scene);
	void processNode(const tinygltf::Model &gltfModel, const tinygltf::Node &node, const glm::mat4 &parentTransform, SceneData &scene);
	void processMesh(const tinygltf::Model &gltfModel, int meshIndex, const glm::mat4 &transform, SceneData &scene);
};