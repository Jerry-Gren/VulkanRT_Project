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

	// 移交场景构建权
	renderScene.init(&vDevice, &bufferManager, &asBuilder);
	renderScene.loadScene("models/test.glb");

	createStorageImage();
	createDescriptors();

	rtPipeline.init(&vDevice, rtDescriptorSetLayout);
	camera.init(glm::vec3(0.0f, 1.0f, -3.0f));
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
	    {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
	    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
	    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
	    {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
	    {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, nullptr}};
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

	VkWriteDescriptorSet vertexWrite = createBufferWrite(rtDescriptorSet, 2, renderScene.getVertexBuffer().buffer);
	VkWriteDescriptorSet indexWrite = createBufferWrite(rtDescriptorSet, 3, renderScene.getIndexBuffer().buffer);
	VkWriteDescriptorSet subMeshWrite = createBufferWrite(rtDescriptorSet, 4, renderScene.getSubMeshBuffer().buffer);
	VkWriteDescriptorSet materialWrite = createBufferWrite(rtDescriptorSet, 5, renderScene.getMaterialBuffer().buffer);
	VkWriteDescriptorSet lightWrite = createBufferWrite(rtDescriptorSet, 6, renderScene.getLightBuffer().buffer);

	descriptorManager.updateSet({asWrite, imageWrite, vertexWrite, indexWrite, subMeshWrite, materialWrite, lightWrite});

	delete vertexWrite.pBufferInfo;
	delete indexWrite.pBufferInfo;
	delete subMeshWrite.pBufferInfo;
	delete materialWrite.pBufferInfo;
	delete lightWrite.pBufferInfo;
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

	renderScene.cleanup();
	asBuilder.cleanup();

	imGuiLayer.cleanup();
	vContext.cleanup();
	vDevice.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}