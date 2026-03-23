#pragma once
#include "VulkanDevice.h"
#include <vector>
#include "Types.h"

class BufferManager
{
public:
	void init(VulkanDevice *device);

	// 核心通用上传函数：使用 Staging Buffer 将 CPU 数据推送到 GPU Device Local 内存
	AllocatedBuffer createDeviceLocalBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage);

	// 针对光追几何体的特化上传
	AllocatedBuffer createVertexBuffer(const std::vector<Vertex> &vertices);
	AllocatedBuffer createIndexBuffer(const std::vector<uint32_t> &indices);

private:
	VulkanDevice *vDevice = nullptr;

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
};