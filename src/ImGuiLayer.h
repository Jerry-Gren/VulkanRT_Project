#pragma once
#include "VulkanDevice.h"
#include "VulkanContext.h"

class ImGuiLayer
{
public:
	void init(VulkanDevice *device, VulkanContext *context, GLFWwindow *window);
	void cleanup();

	void beginFrame();
	void render(VkCommandBuffer commandBuffer);

private:
	void createDescriptorPool();

	VulkanDevice *vDevice = nullptr;
	VkDescriptorPool imguiPool = VK_NULL_HANDLE;
};