#pragma once
#include "VulkanDevice.h"
#include "Types.h"
#include <vector>
#include <string>

class RTPipeline
{
public:
	void init(VulkanDevice *device, VkDescriptorSetLayout descriptorSetLayout);
	void cleanup();

	void bind(VkCommandBuffer commandBuffer);
	void bindDescriptorSets(VkCommandBuffer commandBuffer, VkDescriptorSet *descriptorSet);
	void pushConstants(VkCommandBuffer commandBuffer, const PushConstants &pc);
	void traceRays(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height);

private:
	VulkanDevice *vDevice = nullptr;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	AllocatedBuffer raygenShaderBindingTable;
	AllocatedBuffer missShaderBindingTable;
	AllocatedBuffer hitShaderBindingTable;

	VkStridedDeviceAddressRegionKHR raygenRegion{};
	VkStridedDeviceAddressRegionKHR missRegion{};
	VkStridedDeviceAddressRegionKHR hitRegion{};
	VkStridedDeviceAddressRegionKHR callRegion{};

	std::vector<char> readFile(const std::string &filename);
	VkShaderModule createShaderModule(const std::vector<char> &code);
	void createPipeline(VkDescriptorSetLayout descriptorSetLayout);
	void createShaderBindingTable();
	uint32_t alignUp(uint32_t size, uint32_t alignment);
};