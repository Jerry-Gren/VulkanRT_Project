#include "RTPipeline.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

void RTPipeline::init(VulkanDevice *device, VkDescriptorSetLayout descriptorSetLayout)
{
	vDevice = device;
	createPipeline(descriptorSetLayout);
	createShaderBindingTable();
}

void RTPipeline::cleanup()
{
	vDevice->destroyBuffer(raygenShaderBindingTable);
	vDevice->destroyBuffer(missShaderBindingTable);
	vDevice->destroyBuffer(hitShaderBindingTable);

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(vDevice->device, pipeline, nullptr);
	}
	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(vDevice->device, pipelineLayout, nullptr);
	}
}

std::vector<char> RTPipeline::readFile(const std::string &filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("failed to open shader file: " + filename);
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

VkShaderModule RTPipeline::createShaderModule(const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(vDevice->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create shader module!");
	}
	return shaderModule;
}

void RTPipeline::createPipeline(VkDescriptorSetLayout descriptorSetLayout)
{
	auto rgenCode = readFile("shaders/raygen.rgen.spv");
	auto rmissCode = readFile("shaders/miss.rmiss.spv");
	auto rmissShadowCode = readFile("shaders/shadow.rmiss.spv");
	auto rchitCode = readFile("shaders/closesthit.rchit.spv");

	VkShaderModule rgenModule = createShaderModule(rgenCode);
	VkShaderModule rmissModule = createShaderModule(rmissCode);
	VkShaderModule rmissShadowModule = createShaderModule(rmissShadowCode);
	VkShaderModule rchitModule = createShaderModule(rchitCode);

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineShaderStageCreateInfo stageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stageInfo.pName = "main";

	stageInfo.module = rgenModule;
	stageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shaderStages.push_back(stageInfo);

	stageInfo.module = rmissModule;
	stageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shaderStages.push_back(stageInfo);

	stageInfo.module = rmissShadowModule;
	stageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shaderStages.push_back(stageInfo);

	stageInfo.module = rchitModule;
	stageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shaderStages.push_back(stageInfo);

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
	VkRayTracingShaderGroupCreateInfoKHR groupInfo{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
	groupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
	groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = 0;
	shaderGroups.push_back(groupInfo);

	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = 1;
	shaderGroups.push_back(groupInfo);

	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groupInfo.generalShader = 2;
	shaderGroups.push_back(groupInfo);

	groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	groupInfo.generalShader = VK_SHADER_UNUSED_KHR;
	groupInfo.closestHitShader = 3;
	shaderGroups.push_back(groupInfo);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(vDevice->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create ray tracing pipeline layout!");
	}

	VkRayTracingPipelineCreateInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
	pipelineInfo.pGroups = shaderGroups.data();
	pipelineInfo.maxPipelineRayRecursionDepth = 2;
	pipelineInfo.layout = pipelineLayout;

	if (vDevice->vkCreateRayTracingPipelinesKHR(vDevice->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create ray tracing pipeline!");
	}

	vkDestroyShaderModule(vDevice->device, rgenModule, nullptr);
	vkDestroyShaderModule(vDevice->device, rmissModule, nullptr);
	vkDestroyShaderModule(vDevice->device, rmissShadowModule, nullptr);
	vkDestroyShaderModule(vDevice->device, rchitModule, nullptr);
}

uint32_t RTPipeline::alignUp(uint32_t size, uint32_t alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

void RTPipeline::createShaderBindingTable()
{
	uint32_t handleSize = vDevice->rtProperties.shaderGroupHandleSize;
	uint32_t handleAlignment = vDevice->rtProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment = vDevice->rtProperties.shaderGroupBaseAlignment;

	uint32_t handleSizeAligned = alignUp(handleSize, handleAlignment);

	raygenRegion.stride = alignUp(handleSizeAligned, baseAlignment);
	raygenRegion.size = raygenRegion.stride;

	missRegion.stride = handleSizeAligned;
	// 分配 2 个 Miss Shader 的空间
	missRegion.size = alignUp(2 * handleSizeAligned, baseAlignment);

	hitRegion.stride = handleSizeAligned;
	hitRegion.size = alignUp(1 * handleSizeAligned, baseAlignment);

	uint32_t groupCount = 4; // Raygen(1) + Miss(2) + Hit(1) = 4
	uint32_t dataSize = groupCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	if (vDevice->vkGetRayTracingShaderGroupHandlesKHR(vDevice->device, pipeline, 0, groupCount, dataSize, handles.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to get ray tracing shader group handles!");
	}

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	raygenShaderBindingTable = vDevice->createBuffer(raygenRegion.size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	missShaderBindingTable = vDevice->createBuffer(missRegion.size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	hitShaderBindingTable = vDevice->createBuffer(hitRegion.size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// 修复拷贝逻辑：严格按照句柄索引和对齐要求写入
	void *mapped;

	// 1. 写入 Group 0: Raygen
	vkMapMemory(vDevice->device, raygenShaderBindingTable.memory, 0, raygenRegion.size, 0, &mapped);
	memcpy(mapped, handles.data() + 0 * handleSize, handleSize);
	vkUnmapMemory(vDevice->device, raygenShaderBindingTable.memory);

	// 2. 写入 Group 1 和 Group 2: 主 Miss 和 Shadow Miss
	vkMapMemory(vDevice->device, missShaderBindingTable.memory, 0, missRegion.size, 0, &mapped);
	uint8_t *missMapped = static_cast<uint8_t *>(mapped);
	memcpy(missMapped + 0 * handleSizeAligned, handles.data() + 1 * handleSize, handleSize); // 主 Miss (Index 1)
	memcpy(missMapped + 1 * handleSizeAligned, handles.data() + 2 * handleSize, handleSize); // Shadow Miss (Index 2)
	vkUnmapMemory(vDevice->device, missShaderBindingTable.memory);

	// 3. 写入 Group 3: Closest Hit
	vkMapMemory(vDevice->device, hitShaderBindingTable.memory, 0, hitRegion.size, 0, &mapped);
	memcpy(mapped, handles.data() + 3 * handleSize, handleSize); // 必须是 3 * handleSize！
	vkUnmapMemory(vDevice->device, hitShaderBindingTable.memory);

	raygenRegion.deviceAddress = raygenShaderBindingTable.deviceAddress;
	missRegion.deviceAddress = missShaderBindingTable.deviceAddress;
	hitRegion.deviceAddress = hitShaderBindingTable.deviceAddress;
}

void RTPipeline::bind(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
}

void RTPipeline::bindDescriptorSets(VkCommandBuffer commandBuffer, VkDescriptorSet *descriptorSet)
{
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, descriptorSet, 0, nullptr);
}

void RTPipeline::pushConstants(VkCommandBuffer commandBuffer, const PushConstants &pc)
{
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, 0, sizeof(PushConstants), &pc);
}

void RTPipeline::traceRays(VkCommandBuffer commandBuffer, uint32_t width, uint32_t height)
{
	vDevice->vkCmdTraceRaysKHR(commandBuffer, &raygenRegion, &missRegion, &hitRegion, &callRegion, width, height, 1);
}