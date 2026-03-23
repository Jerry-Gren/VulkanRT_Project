#include "DescriptorManager.h"
#include <stdexcept>

void DescriptorManager::init(VulkanDevice *device, uint32_t maxSets)
{
	vDevice = device;

	// 为光追场景配置常用的 Descriptor Type 容量比例
	std::vector<VkDescriptorPoolSize> poolSizes = {
	    {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxSets}, // 用于 TLAS
	    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, maxSets},	      // 用于光追输出
	    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets},	      // 用于相机/场景参数
	    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxSets}	      // 用于顶点/索引数据的 SSBO 访问
	};

	VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	if (vkCreateDescriptorPool(vDevice->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void DescriptorManager::cleanup()
{
	for (auto layout : trackedLayouts)
	{
		vkDestroyDescriptorSetLayout(vDevice->device, layout, nullptr);
	}
	trackedLayouts.clear();

	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(vDevice->device, descriptorPool, nullptr);
	}
}

VkDescriptorSetLayout DescriptorManager::createLayout(const std::vector<VkDescriptorSetLayoutBinding> &bindings)
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VkDescriptorSetLayout layout;
	if (vkCreateDescriptorSetLayout(vDevice->device, &layoutInfo, nullptr, &layout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	trackedLayouts.push_back(layout);
	return layout;
}

VkDescriptorSet DescriptorManager::allocateSet(VkDescriptorSetLayout layout)
{
	VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet descriptorSet;
	if (vkAllocateDescriptorSets(vDevice->device, &allocInfo, &descriptorSet) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate descriptor set!");
	}
	return descriptorSet;
}

void DescriptorManager::updateSet(const std::vector<VkWriteDescriptorSet> &writes)
{
	vkUpdateDescriptorSets(vDevice->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}