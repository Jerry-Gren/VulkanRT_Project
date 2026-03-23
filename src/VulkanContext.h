#pragma once
#include "VulkanDevice.h"
#include <vector>

class VulkanContext
{
public:
	static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // 引入多帧并发

	void init(VulkanDevice *device, GLFWwindow *window);
	void cleanup();
	void recreateSwapChain();

	VkCommandBuffer beginFrame(uint32_t &imageIndex);
	bool endFrame(VkCommandBuffer cb, uint32_t imageIndex);

	VkRenderPass getRenderPass() const { return renderPass; }
	VkExtent2D getExtent() const { return swapChainExtent; }
	VkImage getSwapChainImage(uint32_t index) const { return swapChainImages[index]; }
	VkFramebuffer getFramebuffer(uint32_t index) const { return swapChainFramebuffers[index]; }

private:
	void createSwapChain();
	void createImageViews();
	void createRenderPass();
	void createFramebuffers();
	void createCommandBuffers(); // 改为复数
	void createSyncObjects();
	void cleanupSwapChain();

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	VulkanDevice *vDevice;
	GLFWwindow *window;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkRenderPass renderPass;

	// 向量化每帧状态对象
	std::vector<VkCommandBuffer> commandBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;
};