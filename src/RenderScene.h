#pragma once
#include "Types.h"
#include "VulkanDevice.h"
#include "BufferManager.h"
#include "ASBuilder.h"
#include "ModelLoader.h"
#include <string>

class RenderScene
{
public:
	void init(VulkanDevice *device, BufferManager *bufferMgr, ASBuilder *asBldr);
	void cleanup();

	bool loadScene(const std::string &filepath);

	// 获取底层资源引用以供描述符绑定
	const AllocatedBuffer &getVertexBuffer() const { return vertexBuffer; }
	const AllocatedBuffer &getIndexBuffer() const { return indexBuffer; }
	const AllocatedBuffer &getMaterialBuffer() const { return materialBuffer; }
	const AllocatedBuffer &getSubMeshBuffer() const { return subMeshBuffer; }
	const AllocatedBuffer &getLightBuffer() const { return lightBuffer; }
	const VulkanAS &getTLAS() const { return asBuilder->getTLAS(); }

private:
	void buildGPUResources();
	void generateDefaultScene();
	void extractLightsFromConfig();

	VulkanDevice *vDevice = nullptr;
	BufferManager *bufferManager = nullptr;
	ASBuilder *asBuilder = nullptr;
	ModelLoader modelLoader;

	SceneData sceneData;

	AllocatedBuffer vertexBuffer;
	AllocatedBuffer indexBuffer;
	AllocatedBuffer materialBuffer;
	AllocatedBuffer subMeshBuffer;
	AllocatedBuffer lightBuffer;
};