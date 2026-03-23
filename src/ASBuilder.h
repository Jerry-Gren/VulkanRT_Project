#pragma once
#include "VulkanDevice.h"
#include <vector>
#include <vulkan/vulkan.h>

// 封装单个加速结构及其关联内存
struct VulkanAS
{
	VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
	AllocatedBuffer buffer;
	VkDeviceAddress deviceAddress = 0;
};

// 描述一个输入给 BLAS 的几何体（目前支持三角形网格）
struct BlasInput
{
	std::vector<VkAccelerationStructureGeometryKHR> asGeometry;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
	VkBuildAccelerationStructureFlagsKHR flags{0};
};

class ASBuilder
{
public:
	void init(VulkanDevice *device);
	void cleanup();

	void buildScene(const SceneData &scene, AllocatedBuffer vertexBuffer, AllocatedBuffer indexBuffer);

	const std::vector<VulkanAS> &getBLAS() const { return blasArray; }
	const VulkanAS &getTLAS() const { return tlas; }

private:
	VulkanDevice *vDevice = nullptr;

	std::vector<VulkanAS> blasArray;
	VulkanAS tlas;

	VulkanAS createAccelerationStructure(VkAccelerationStructureCreateInfoKHR &createInfo);
	AllocatedBuffer createScratchBuffer(VkDeviceSize size);
	void buildBLAS(const std::vector<BlasInput> &input, VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
	void buildTLAS(const std::vector<VkAccelerationStructureInstanceKHR> &instances, VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
};