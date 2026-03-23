#pragma once
#include "VulkanDevice.h"
#include <vector>

class DescriptorManager
{
public:
	// maxSets: 预估应用运行期间最大需要的描述符集数量
	void init(VulkanDevice *device, uint32_t maxSets = 100);
	void cleanup();

	// 传入绑定配置，自动生成 Layout（Manager 会接管其生命周期用于销毁）
	VkDescriptorSetLayout createLayout(const std::vector<VkDescriptorSetLayoutBinding> &bindings);

	// 从内部 Pool 分配 Descriptor Set
	VkDescriptorSet allocateSet(VkDescriptorSetLayout layout);

	// 批量更新 Descriptor Set 的内容
	void updateSet(const std::vector<VkWriteDescriptorSet> &writes);

private:
	VulkanDevice *vDevice = nullptr;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

	// 集中管理，防止外部忘记销毁导致泄漏
	std::vector<VkDescriptorSetLayout> trackedLayouts;
};