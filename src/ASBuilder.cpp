#include "ASBuilder.h"
#include <stdexcept>
#include <cstring>

void ASBuilder::init(VulkanDevice *device)
{
	vDevice = device;
}

void ASBuilder::cleanup()
{
	for (auto &blas : blasArray)
	{
		if (blas.handle != VK_NULL_HANDLE)
		{
			vDevice->vkDestroyAccelerationStructureKHR(vDevice->device, blas.handle, nullptr);
			vDevice->destroyBuffer(blas.buffer);
		}
	}
	blasArray.clear();

	if (tlas.handle != VK_NULL_HANDLE)
	{
		vDevice->vkDestroyAccelerationStructureKHR(vDevice->device, tlas.handle, nullptr);
		vDevice->destroyBuffer(tlas.buffer);
	}
}

VulkanAS ASBuilder::createAccelerationStructure(VkAccelerationStructureCreateInfoKHR &createInfo)
{
	VulkanAS as;
	// 为 AS 分配专门的显存，必须使用 ACCELERATION_STRUCTURE_STORAGE 标志
	as.buffer = vDevice->createBuffer(createInfo.size,
					  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	createInfo.buffer = as.buffer.buffer;

	if (vDevice->vkCreateAccelerationStructureKHR(vDevice->device, &createInfo, nullptr, &as.handle) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create acceleration structure!");
	}

	// 获取 AS 的设备地址，供着色器和 TLAS 引用
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
	addressInfo.accelerationStructure = as.handle;
	as.deviceAddress = vDevice->vkGetAccelerationStructureDeviceAddressKHR(vDevice->device, &addressInfo);

	return as;
}

AllocatedBuffer ASBuilder::createScratchBuffer(VkDeviceSize size)
{
	return vDevice->createBuffer(size,
				     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void ASBuilder::buildBLAS(const std::vector<BlasInput> &inputs, VkBuildAccelerationStructureFlagsKHR flags)
{
	uint32_t nbBlas = static_cast<uint32_t>(inputs.size());
	VkDeviceSize maxScratchSize = 0;

	std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(nbBlas);
	std::vector<VkDeviceSize> originalSizes(nbBlas);

	// 1. 获取每个 BLAS 构建所需的显存大小
	for (uint32_t idx = 0; idx < nbBlas; idx++)
	{
		buildInfos[idx].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfos[idx].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfos[idx].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfos[idx].flags = inputs[idx].flags | flags;
		buildInfos[idx].geometryCount = static_cast<uint32_t>(inputs[idx].asGeometry.size());
		buildInfos[idx].pGeometries = inputs[idx].asGeometry.data();

		std::vector<uint32_t> maxPrimCount(inputs[idx].asBuildOffsetInfo.size());
		for (auto tt = 0; tt < inputs[idx].asBuildOffsetInfo.size(); tt++)
		{
			maxPrimCount[tt] = inputs[idx].asBuildOffsetInfo[tt].primitiveCount;
		}

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
		vDevice->vkGetAccelerationStructureBuildSizesKHR(vDevice->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfos[idx], maxPrimCount.data(), &sizeInfo);

		originalSizes[idx] = sizeInfo.accelerationStructureSize;
		maxScratchSize = std::max(maxScratchSize, sizeInfo.buildScratchSize);
	}

	// 2. 分配 Scratch Buffer (临时构建内存)
	AllocatedBuffer scratchBuffer = createScratchBuffer(maxScratchSize);
	VkDeviceAddress scratchAddress = scratchBuffer.deviceAddress;

	// 3. 实际创建 AS 并记录构建命令
	blasArray.resize(nbBlas);
	VkCommandBuffer cmdBuf = vDevice->beginSingleTimeCommands();

	for (uint32_t idx = 0; idx < nbBlas; idx++)
	{
		VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
		createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		createInfo.size = originalSizes[idx];
		blasArray[idx] = createAccelerationStructure(createInfo);

		buildInfos[idx].dstAccelerationStructure = blasArray[idx].handle;
		buildInfos[idx].scratchData.deviceAddress = scratchAddress;

		const VkAccelerationStructureBuildRangeInfoKHR *pBuildOffset = inputs[idx].asBuildOffsetInfo.data();
		vDevice->vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfos[idx], &pBuildOffset);

		// 插入内存屏障，确保当前 BLAS 构建完成才能被后续过程（或 TLAS）使用
		VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
	}

	vDevice->endSingleTimeCommands(cmdBuf);
	vDevice->destroyBuffer(scratchBuffer); // 释放临时内存，防止泄漏
}

void ASBuilder::buildTLAS(const std::vector<VkAccelerationStructureInstanceKHR> &instances, VkBuildAccelerationStructureFlagsKHR flags)
{
	if (instances.empty())
		return;

	// 1. 创建并上传实例缓冲区
	VkDeviceSize instanceBufferSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
	AllocatedBuffer instanceBuffer = vDevice->createBuffer(instanceBufferSize,
							       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
							       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void *data;
	vkMapMemory(vDevice->device, instanceBuffer.memory, 0, instanceBufferSize, 0, &data);
	memcpy(data, instances.data(), instanceBufferSize);
	vkUnmapMemory(vDevice->device, instanceBuffer.memory);

	// 2. 准备 TLAS 构建信息
	VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
	instancesVk.data.deviceAddress = instanceBuffer.deviceAddress;

	VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	topASGeometry.geometry.instances = instancesVk;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	buildInfo.flags = flags;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &topASGeometry;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

	uint32_t count = static_cast<uint32_t>(instances.size());
	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	vDevice->vkGetAccelerationStructureBuildSizesKHR(vDevice->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);

	// 3. 创建 TLAS 对象与 Scratch Buffer
	VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.size = sizeInfo.accelerationStructureSize;
	tlas = createAccelerationStructure(createInfo);

	AllocatedBuffer scratchBuffer = createScratchBuffer(sizeInfo.buildScratchSize);

	buildInfo.dstAccelerationStructure = tlas.handle;
	buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

	// 4. 记录构建命令
	VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{count, 0, 0, 0};
	const VkAccelerationStructureBuildRangeInfoKHR *pBuildOffsetInfo = &buildOffsetInfo;

	VkCommandBuffer cmdBuf = vDevice->beginSingleTimeCommands();
	vDevice->vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);
	vDevice->endSingleTimeCommands(cmdBuf);

	// 5. 清理临时资源
	vDevice->destroyBuffer(scratchBuffer);
	vDevice->destroyBuffer(instanceBuffer);
}

void ASBuilder::buildScene(const SceneData &scene, AllocatedBuffer vertexBuffer, AllocatedBuffer indexBuffer)
{
	if (scene.subMeshes.empty())
		return;

	// 1. 为每个 SubMesh 准备 BLAS 构建信息
	std::vector<BlasInput> blasInputs;
	blasInputs.reserve(scene.subMeshes.size());

	for (const auto &subMesh : scene.subMeshes)
	{
		VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData.deviceAddress = vertexBuffer.deviceAddress; // 使用同一块全局显存
		triangles.vertexStride = sizeof(Vertex);
		triangles.maxVertex = static_cast<uint32_t>(scene.vertices.size() - 1);
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData.deviceAddress = indexBuffer.deviceAddress; // 同上

		VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		geometry.geometry.triangles = triangles;
		geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

		VkAccelerationStructureBuildRangeInfoKHR offset{};
		// 注意：通过偏移定位到这个 SubMesh 在大数组中的精确位置
		offset.primitiveCount = subMesh.indexCount / 3;
		offset.primitiveOffset = subMesh.firstIndex * sizeof(uint32_t);
		offset.firstVertex = subMesh.vertexOffset;
		offset.transformOffset = 0;

		BlasInput input;
		input.asGeometry.push_back(geometry);
		input.asBuildOffsetInfo.push_back(offset);
		input.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		blasInputs.push_back(input);
	}

	// 2. 批量构建 BLAS
	buildBLAS(blasInputs);

	// 3. 将 SceneInstance 映射为 Vulkan 实例数据
	std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
	tlasInstances.reserve(scene.instances.size());

	for (uint32_t i = 0; i < scene.instances.size(); ++i)
	{
		const auto &inst = scene.instances[i];

		// glm::mat4 是列主序，Vulkan AS 实例需要行主序的 3x4 矩阵
		glm::mat4 transposed = glm::transpose(inst.transform);

		VkAccelerationStructureInstanceKHR vkInst{};
		memcpy(&vkInst.transform, &transposed, sizeof(VkTransformMatrixKHR));

		vkInst.instanceCustomIndex = inst.subMeshIndex; // 将 subMeshIndex 编码入自定义索引，着色器解包使用
		vkInst.mask = 0xFF;
		vkInst.instanceShaderBindingTableRecordOffset = 0;
		vkInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		// 绑定对应的 BLAS
		vkInst.accelerationStructureReference = blasArray[inst.subMeshIndex].deviceAddress;

		tlasInstances.push_back(vkInst);
	}

	// 4. 构建 TLAS
	buildTLAS(tlasInstances);
}