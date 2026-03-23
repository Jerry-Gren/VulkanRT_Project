#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#include "ModelLoader.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

bool ModelLoader::loadGLTF(const std::string &filepath, SceneData &outScene)
{
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	bool ret = false;
	if (filepath.find(".glb") != std::string::npos)
	{
		ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
	}
	else
	{
		ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
	}

	if (!warn.empty())
		std::cout << "GLTF Warn: " << warn << std::endl;
	if (!err.empty())
		std::cerr << "GLTF Error: " << err << std::endl;
	if (!ret)
		return false;

	processMaterials(gltfModel, outScene);

	const tinygltf::Scene &defaultScene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
	for (int nodeIdx : defaultScene.nodes)
	{
		processNode(gltfModel, gltfModel.nodes[nodeIdx], glm::mat4(1.0f), outScene);
	}

	return true;
}

void ModelLoader::processMaterials(const tinygltf::Model &gltfModel, SceneData &scene)
{
	for (const auto &mat : gltfModel.materials)
	{
		GPUMaterial gpuMat{};
		gpuMat.baseColor = glm::vec4(1.0f);
		gpuMat.metallic = 1.0f;
		gpuMat.roughness = 1.0f;
		gpuMat.transmission = 0.0f;
		gpuMat.ior = 1.5f;
		gpuMat.textureID = -1; // 暂未处理纹理加载

		auto baseColorFactor = mat.pbrMetallicRoughness.baseColorFactor;
		if (baseColorFactor.size() == 4)
		{
			gpuMat.baseColor = glm::make_vec4(baseColorFactor.data());
		}
		gpuMat.metallic = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
		gpuMat.roughness = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);

		scene.materials.push_back(gpuMat);
	}

	// 如果模型没有材质，提供一个默认材质
	if (scene.materials.empty())
	{
		// vec4 baseColor, metallic, roughness, transmission, ior, textureID, padding
		scene.materials.push_back({glm::vec4(0.8f), 0.0f, 0.5f, 0.0f, 1.5f, -1, {0, 0, 0}});
	}
}

void ModelLoader::processNode(const tinygltf::Model &gltfModel, const tinygltf::Node &node, const glm::mat4 &parentTransform, SceneData &scene)
{
	glm::mat4 localTransform(1.0f);

	if (node.matrix.size() == 16)
	{
		localTransform = glm::make_mat4x4(node.matrix.data());
	}
	else
	{
		if (node.translation.size() == 3)
			localTransform = glm::translate(localTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
		if (node.rotation.size() == 4)
		{
			glm::quat q = glm::make_quat(node.rotation.data());
			localTransform *= glm::mat4_cast(q);
		}
		if (node.scale.size() == 3)
			localTransform = glm::scale(localTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
	}

	glm::mat4 globalTransform = parentTransform * localTransform;

	if (node.mesh > -1)
	{
		processMesh(gltfModel, node.mesh, globalTransform, scene);
	}

	for (int childIdx : node.children)
	{
		processNode(gltfModel, gltfModel.nodes[childIdx], globalTransform, scene);
	}
}

void ModelLoader::processMesh(const tinygltf::Model &gltfModel, int meshIndex, const glm::mat4 &transform, SceneData &scene)
{
	const tinygltf::Mesh &mesh = gltfModel.meshes[meshIndex];

	for (const auto &primitive : mesh.primitives)
	{
		if (primitive.mode != 4)
			continue; // 仅支持三角形 (GL_TRIANGLES)

		SubMesh subMesh{};
		subMesh.firstIndex = static_cast<uint32_t>(scene.indices.size());
		subMesh.vertexOffset = static_cast<int32_t>(scene.vertices.size());
		subMesh.materialIndex = primitive.material > -1 ? primitive.material : 0;

		// 提取顶点位置、法线、UV
		const float *positionBuffer = nullptr;
		const float *normalBuffer = nullptr;
		const float *uvBuffer = nullptr;
		size_t vertexCount = 0;

		if (primitive.attributes.find("POSITION") != primitive.attributes.end())
		{
			const tinygltf::Accessor &accessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView &view = gltfModel.bufferViews[accessor.bufferView];
			positionBuffer = reinterpret_cast<const float *>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
			vertexCount = accessor.count;
		}

		if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
		{
			const tinygltf::Accessor &accessor = gltfModel.accessors[primitive.attributes.at("NORMAL")];
			const tinygltf::BufferView &view = gltfModel.bufferViews[accessor.bufferView];
			normalBuffer = reinterpret_cast<const float *>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
		}

		if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
		{
			const tinygltf::Accessor &accessor = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
			const tinygltf::BufferView &view = gltfModel.bufferViews[accessor.bufferView];
			uvBuffer = reinterpret_cast<const float *>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
		}

		for (size_t i = 0; i < vertexCount; ++i)
		{
			Vertex v{};
			v.pos = glm::make_vec3(&positionBuffer[i * 3]);
			v.normal = normalBuffer ? glm::normalize(glm::make_vec3(&normalBuffer[i * 3])) : glm::vec3(0.0f, 1.0f, 0.0f);
			v.uv = uvBuffer ? glm::make_vec2(&uvBuffer[i * 2]) : glm::vec2(0.0f);
			scene.vertices.push_back(v);
		}

		// 提取索引
		if (primitive.indices > -1)
		{
			const tinygltf::Accessor &accessor = gltfModel.accessors[primitive.indices];
			const tinygltf::BufferView &view = gltfModel.bufferViews[accessor.bufferView];
			const void *dataPtr = &(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);

			subMesh.indexCount = static_cast<uint32_t>(accessor.count);

			if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT)
			{
				const uint32_t *buf = static_cast<const uint32_t *>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++)
					scene.indices.push_back(buf[i]);
			}
			else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT)
			{
				const uint16_t *buf = static_cast<const uint16_t *>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++)
					scene.indices.push_back(buf[i]);
			}
			else if (accessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE)
			{
				const uint8_t *buf = static_cast<const uint8_t *>(dataPtr);
				for (size_t i = 0; i < accessor.count; i++)
					scene.indices.push_back(buf[i]);
			}
		}

		// 注册当前 SubMesh 并在场景中实例化它
		uint32_t subMeshIdx = static_cast<uint32_t>(scene.subMeshes.size());
		scene.subMeshes.push_back(subMesh);
		scene.instances.push_back({transform, subMeshIdx});
	}
}