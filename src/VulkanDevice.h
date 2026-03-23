#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <string>
#include <glm/glm.hpp>

#include "Types.h"

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

class VulkanDevice
{
public:
	void init(GLFWwindow *window);
	void cleanup();
	void deviceWaitIdle();

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void destroyBuffer(const AllocatedBuffer &allocatedBuffer);
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};

	PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
	PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
	PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
	PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
	PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
	PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

private:
	void createInstance();
	void createSurface(GLFWwindow *window);
	void pickPhysicalDevice();
	void createLogicalDevice();
	void loadExtensionFunctions();
	void createCommandPool();
	bool isDeviceSuitable(VkPhysicalDevice device) const;

	// 清理了 Vulkan 1.2/1.3 已转正的核心扩展，避免驱动合规性报错
	const std::vector<const char *> deviceExtensions = {
	    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
	    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
	    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
};