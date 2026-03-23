#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

#include "VulkanDevice.h"
#include "VulkanContext.h"
#include "ImGuiLayer.h"
#include "BufferManager.h"
#include "ASBuilder.h"
#include "ImageManager.h"
#include "DescriptorManager.h"
#include "RTPipeline.h"
#include "Types.h"
#include "Camera.h"
#include "SettingsPanel.h"
#include "RenderScene.h"

class Application
{
public:
	void run();

private:
	GLFWwindow *window = nullptr;
	VulkanDevice vDevice;
	VulkanContext vContext;
	ImGuiLayer imGuiLayer;

	BufferManager bufferManager;
	ASBuilder asBuilder;
	ImageManager imageManager;
	DescriptorManager descriptorManager;
	RTPipeline rtPipeline;

	RenderScene renderScene;

	AllocatedImage storageImage;
	VkDescriptorSetLayout rtDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet rtDescriptorSet = VK_NULL_HANDLE;

	SceneConfig sceneConfig;
	std::vector<std::unique_ptr<UIPanel>> uiPanels;

	uint32_t width = 1280;
	uint32_t height = 720;
	bool framebufferResized = false;
	uint32_t frameCount = 0;

	Camera camera;
	float lastFrameTime = 0.0f;

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

	void initWindow();
	void initVulkan();
	void createStorageImage();
	void createDescriptors();
	void updateDescriptors();
	void handleResize();
	void mainLoop();
	void cleanup();
};