#include "ImGuiLayer.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <stdexcept>

void ImGuiLayer::init(VulkanDevice *device, VulkanContext *context, GLFWwindow *window)
{
	vDevice = device;
	createDescriptorPool();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForVulkan(window, true);

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = vDevice->instance;
	initInfo.PhysicalDevice = vDevice->physicalDevice;
	initInfo.Device = vDevice->device;
	initInfo.QueueFamily = vDevice->findQueueFamilies(vDevice->physicalDevice).graphicsFamily.value();
	initInfo.Queue = vDevice->graphicsQueue;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = VulkanContext::MAX_FRAMES_IN_FLIGHT;
	initInfo.ImageCount = VulkanContext::MAX_FRAMES_IN_FLIGHT;
	initInfo.Allocator = nullptr;
	initInfo.CheckVkResultFn = [](VkResult err)
	{
		if (err != VK_SUCCESS)
			throw std::runtime_error("ImGui Vulkan Error");
	};

	// 适配最新版 API 结构：将渲染通道与多重采样设置移入 PipelineInfoMain
	initInfo.PipelineInfoMain.RenderPass = context->getRenderPass();
	initInfo.PipelineInfoMain.Subpass = 0;
	initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	// 适配最新版函数签名：只传入 initInfo 指针
	ImGui_ImplVulkan_Init(&initInfo);

	// uploadFonts() 已被弃用，直接移除即可
}

void ImGuiLayer::createDescriptorPool()
{
	VkDescriptorPoolSize poolSizes[] = {
	    {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
	    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
	    {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
	    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
	    {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
	    {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
	    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
	    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
	    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
	    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
	    {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
	poolInfo.pPoolSizes = poolSizes;

	if (vkCreateDescriptorPool(vDevice->device, &poolInfo, nullptr, &imguiPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create ImGui descriptor pool!");
	}
}

void ImGuiLayer::cleanup()
{
	vkDeviceWaitIdle(vDevice->device);
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(vDevice->device, imguiPool, nullptr);
}

void ImGuiLayer::beginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiLayer::render(VkCommandBuffer commandBuffer)
{
	ImGui::Render();
	ImDrawData *drawData = ImGui::GetDrawData();
	ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
}