#include "RenderScene.h"
#include <iostream>

void RenderScene::init(VulkanDevice *device, BufferManager *bufferMgr, ASBuilder *asBldr)
{
	vDevice = device;
	bufferManager = bufferMgr;
	asBuilder = asBldr;
}

void RenderScene::cleanup()
{
	vDevice->destroyBuffer(vertexBuffer);
	vDevice->destroyBuffer(indexBuffer);
	vDevice->destroyBuffer(materialBuffer);
	vDevice->destroyBuffer(subMeshBuffer);
	vDevice->destroyBuffer(lightBuffer);
	// asBuilder 的清理仍由 Application 在全局生命周期中控制
}

bool RenderScene::loadScene(const std::string &filepath)
{
	sceneData = SceneData(); // 重置状态

	if (!modelLoader.loadGLTF(filepath, sceneData))
	{
		std::cout << "Warning: Could not load " << filepath << ", generating default scene.\n";
		generateDefaultScene();
	}

	// 防御性编程：如果没有光源，强行注入一个默认方向光，防止 Vulkan 绑定空缓冲报错
	if (sceneData.lights.empty())
	{
		extractLightsFromConfig();
	}

	buildGPUResources();
	return true;
}

void RenderScene::extractLightsFromConfig()
{
	GPULight defaultLight{};
	defaultLight.position = glm::vec4(glm::normalize(glm::vec3(0.5f, 1.0f, -0.5f)), 0.0f); // 0.0 表示方向光
	defaultLight.emission = glm::vec4(1.0f, 0.95f, 0.9f, 5.0f);			       // 颜色与强度
	sceneData.lights.push_back(defaultLight);
}

void RenderScene::generateDefaultScene()
{
	sceneData.vertices = {
	    {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
	    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
	sceneData.indices = {0, 1, 2};
	sceneData.materials.push_back({glm::vec4(0.8f), 0.0f, 0.5f, 0.0f, 1.5f, -1, {0, 0, 0}});
	sceneData.subMeshes.push_back({0, 3, 0, 0});
	sceneData.instances.push_back({glm::mat4(1.0f), 0});
}

void RenderScene::buildGPUResources()
{
	// 销毁可能存在的旧缓冲（支持热重载）
	cleanup();

	// 1. 上传顶点、索引与逻辑元数据
	vertexBuffer = bufferManager->createDeviceLocalBuffer(sceneData.vertices.data(), sizeof(Vertex) * sceneData.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	indexBuffer = bufferManager->createDeviceLocalBuffer(sceneData.indices.data(), sizeof(uint32_t) * sceneData.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	materialBuffer = bufferManager->createDeviceLocalBuffer(sceneData.materials.data(), sizeof(GPUMaterial) * sceneData.materials.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	subMeshBuffer = bufferManager->createDeviceLocalBuffer(sceneData.subMeshes.data(), sizeof(SubMesh) * sceneData.subMeshes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	lightBuffer = bufferManager->createDeviceLocalBuffer(sceneData.lights.data(), sizeof(GPULight) * sceneData.lights.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	// 2. 将控制权交由 ASBuilder 构建加速结构
	asBuilder->buildScene(sceneData, vertexBuffer, indexBuffer);
}