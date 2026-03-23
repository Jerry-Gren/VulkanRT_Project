#include "BufferManager.h"
#include <cstring>
#include <stdexcept>

void BufferManager::init(VulkanDevice *device)
{
	vDevice = device;
}

void BufferManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = vDevice->beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vDevice->endSingleTimeCommands(commandBuffer);
}

AllocatedBuffer BufferManager::createDeviceLocalBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage)
{
	// 1. 创建 CPU 可见的暂存缓冲区 (Staging Buffer)
	AllocatedBuffer stagingBuffer = vDevice->createBuffer(
	    size,
	    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// 2. 将数据映射并拷贝到暂存缓冲区
	void *mappedData;
	vkMapMemory(vDevice->device, stagingBuffer.memory, 0, size, 0, &mappedData);
	memcpy(mappedData, data, static_cast<size_t>(size));
	vkUnmapMemory(vDevice->device, stagingBuffer.memory);

	// 3. 创建 GPU 专用的目标缓冲区 (Device Local)
	// 强制附加 SHADER_DEVICE_ADDRESS_BIT，因为光追的 AS 构建和 Shader 都需要直接通过指针访问显存
	AllocatedBuffer deviceBuffer = vDevice->createBuffer(
	    size,
	    usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// 4. 提交拷贝命令
	copyBuffer(stagingBuffer.buffer, deviceBuffer.buffer, size);

	// 5. 释放暂存缓冲区
	vDevice->destroyBuffer(stagingBuffer);

	return deviceBuffer;
}

AllocatedBuffer BufferManager::createVertexBuffer(const std::vector<Vertex> &vertices)
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
	return createDeviceLocalBuffer(
	    vertices.data(),
	    bufferSize,
	    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

AllocatedBuffer BufferManager::createIndexBuffer(const std::vector<uint32_t> &indices)
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
	return createDeviceLocalBuffer(
	    indices.data(),
	    bufferSize,
	    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}