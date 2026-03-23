#include "Application.h"
#include <imgui.h>
#include <iostream>

void Application::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
	auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

void Application::run()
{
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

void Application::initWindow()
{
	if (!glfwInit())
		throw std::runtime_error("failed to initialize GLFW");

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, "Vulkan RT Engine Test", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void Application::initVulkan()
{
	vDevice.init(window);
	vContext.init(&vDevice, window);
	imGuiLayer.init(&vDevice, &vContext, window);

	bufferManager.init(&vDevice);
	asBuilder.init(&vDevice);
	imageManager.init(&vDevice);
	descriptorManager.init(&vDevice);

	uiPanels.push_back(std::make_unique<SettingsPanel>(&sceneConfig));

	loadScene();
	createStorageImage();
	createDescriptors();

	rtPipeline.init(&vDevice, rtDescriptorSetLayout);
	camera.init(glm::vec3(0.0f, 1.0f, -3.0f));
}

void Application::loadScene()
{
	// 尝试加载外部模型，如果失败则回退到硬编码三角形防止崩溃
	// 请确保可执行文件同级目录下有 models/test.glb 文件
	if (!modelLoader.loadGLTF("models/test.glb", sceneData))
	{
		std::cout << "Warning: Could not load models/test.glb, generating default triangle.\n";

		sceneData.vertices = {
		    {{0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.0f}},
		    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		    {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}};
		sceneData.indices = {0, 1, 2};
		sceneData.materials.push_back({glm::vec4(0.8f, 0.3f, 0.3f, 1.0f), 0.0f, 0.5f, 0.0f, 1.5f, -1, {0, 0, 0}});
		sceneData.subMeshes.push_back({0, 3, 0, 0});
		sceneData.instances.push_back({glm::mat4(1.0f), 0});
	}

	// 1. 上传所有合并后的数组到 GPU
	vertexBuffer = bufferManager.createDeviceLocalBuffer(sceneData.vertices.data(), sizeof(Vertex) * sceneData.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	indexBuffer = bufferManager.createDeviceLocalBuffer(sceneData.indices.data(), sizeof(uint32_t) * sceneData.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	materialBuffer = bufferManager.createDeviceLocalBuffer(sceneData.materials.data(), sizeof(GPUMaterial) * sceneData.materials.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	subMeshBuffer = bufferManager.createDeviceLocalBuffer(sceneData.subMeshes.data(), sizeof(SubMesh) * sceneData.subMeshes.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	// 2. 将数据交由 ASBuilder 进行批量构建
	asBuilder.buildScene(sceneData, vertexBuffer, indexBuffer);
}

void Application::createStorageImage()
{
	VkExtent2D extent = vContext.getExtent();
	storageImage = imageManager.createStorageImage(extent.width, extent.height);
}

void Application::createDescriptors()
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
	    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
	    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
	    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}, // 顶点
	    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}, // 索引
	    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}, // SubMeshes
	    {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}  // Materials
	};
	rtDescriptorSetLayout = descriptorManager.createLayout(bindings);
	rtDescriptorSet = descriptorManager.allocateSet(rtDescriptorSetLayout);

	updateDescriptors();
}

void Application::updateDescriptors()
{
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
	VkAccelerationStructureKHR tlasHandle = asBuilder.getTLAS().handle;
	descASInfo.accelerationStructureCount = 1;
	descASInfo.pAccelerationStructures = &tlasHandle;

	VkWriteDescriptorSet asWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	asWrite.dstSet = rtDescriptorSet;
	asWrite.dstBinding = 0;
	asWrite.descriptorCount = 1;
	asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	asWrite.pNext = &descASInfo;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView = storageImage.view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet imageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	imageWrite.dstSet = rtDescriptorSet;
	imageWrite.dstBinding = 1;
	imageWrite.descriptorCount = 1;
	imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imageWrite.pImageInfo = &imageInfo;

	auto createBufferWrite = [](VkDescriptorSet set, uint32_t binding, VkBuffer buffer)
	{
		VkDescriptorBufferInfo *info = new VkDescriptorBufferInfo{buffer, 0, VK_WHOLE_SIZE};
		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = set;
		write.dstBinding = binding;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = info;
		return write;
	};

	VkWriteDescriptorSet vertexWrite = createBufferWrite(rtDescriptorSet, 2, vertexBuffer.buffer);
	VkWriteDescriptorSet indexWrite = createBufferWrite(rtDescriptorSet, 3, indexBuffer.buffer);
	VkWriteDescriptorSet subMeshWrite = createBufferWrite(rtDescriptorSet, 4, subMeshBuffer.buffer);
	VkWriteDescriptorSet materialWrite = createBufferWrite(rtDescriptorSet, 5, materialBuffer.buffer);

	descriptorManager.updateSet({asWrite, imageWrite, vertexWrite, indexWrite, subMeshWrite, materialWrite});

	// 释放上面动态分配的临时内存
	delete vertexWrite.pBufferInfo;
	delete indexWrite.pBufferInfo;
	delete subMeshWrite.pBufferInfo;
	delete materialWrite.pBufferInfo;
}

void Application::handleResize()
{
	vContext.recreateSwapChain();

	imageManager.destroyImage(storageImage);
	createStorageImage();
	updateDescriptors();

	frameCount = 0;
}

void Application::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float currentFrameTime = static_cast<float>(glfwGetTime());
		float deltaTime = currentFrameTime - lastFrameTime;
		lastFrameTime = currentFrameTime;

		if (camera.update(window, deltaTime) || sceneConfig.isDirty)
		{
			frameCount = 0;
			sceneConfig.isDirty = false;
		}

		uint32_t imageIndex;
		VkCommandBuffer commandBuffer = vContext.beginFrame(imageIndex);

		if (commandBuffer == VK_NULL_HANDLE)
		{
			if (framebufferResized)
			{
				framebufferResized = false;
				handleResize();
			}
			continue;
		}

		PushConstants pc{};
		pc.camPos_Fov = glm::vec4(camera.getPosition(), tan(glm::radians(camera.getFov() * 0.5f)));
		pc.camDir_Aspect = glm::vec4(camera.getForward(), (float)vContext.getExtent().width / vContext.getExtent().height);
		pc.camUp_Frame = glm::vec4(camera.getUp(), static_cast<float>(frameCount++));
		pc.camRight_EnvInt = glm::vec4(camera.getRight(), sceneConfig.envIntensity);

		pc.envColor_LgAng = glm::vec4(sceneConfig.envColor, sceneConfig.lightAngleRadius);
		pc.lightDir_LgInt = glm::vec4(sceneConfig.lightDirection, sceneConfig.lightIntensity);

		pc.albedo_Rough = glm::vec4(sceneConfig.albedo, sceneConfig.roughness);
		pc.matParams = glm::vec4(sceneConfig.metallic, sceneConfig.transmission, sceneConfig.ior, sceneConfig.overrideMaterial ? 1.0f : 0.0f);

		rtPipeline.bind(commandBuffer);
		rtPipeline.bindDescriptorSets(commandBuffer, &rtDescriptorSet);
		rtPipeline.pushConstants(commandBuffer, pc);
		rtPipeline.traceRays(commandBuffer, vContext.getExtent().width, vContext.getExtent().height);

		VkImage swapchainImage = vContext.getSwapChainImage(imageIndex);

		imageManager.cmdTransitionImageLayout(commandBuffer, storageImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		imageManager.cmdTransitionImageLayout(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		imageManager.cmdBlitImage(commandBuffer, storageImage.image, swapchainImage, vContext.getExtent().width, vContext.getExtent().height);

		imageManager.cmdTransitionImageLayout(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		imageManager.cmdTransitionImageLayout(commandBuffer, storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

		imGuiLayer.beginFrame();
		for (auto &panel : uiPanels)
		{
			panel->draw();
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vContext.getRenderPass();
		renderPassInfo.framebuffer = vContext.getFramebuffer(imageIndex);
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = vContext.getExtent();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		imGuiLayer.render(commandBuffer);
		vkCmdEndRenderPass(commandBuffer);

		bool needRecreate = vContext.endFrame(commandBuffer, imageIndex);
		if (needRecreate || framebufferResized)
		{
			framebufferResized = false;
			handleResize();
		}
	}
}

void Application::cleanup()
{
	vDevice.deviceWaitIdle();

	rtPipeline.cleanup();
	imageManager.destroyImage(storageImage);
	descriptorManager.cleanup();

	asBuilder.cleanup();
	vDevice.destroyBuffer(vertexBuffer);
	vDevice.destroyBuffer(indexBuffer);
	vDevice.destroyBuffer(materialBuffer);
	vDevice.destroyBuffer(subMeshBuffer);

	imGuiLayer.cleanup();
	vContext.cleanup();
	vDevice.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}