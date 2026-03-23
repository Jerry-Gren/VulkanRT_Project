#pragma once
#include "VulkanDevice.h"
#include "Types.h"

class ImageManager
{
public:
	void init(VulkanDevice *device);

	AllocatedImage createStorageImage(uint32_t width, uint32_t height, VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT);
	void destroyImage(const AllocatedImage &allocatedImage);

	// 单次指令屏障 (用于初始化)
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	// 录制到当前帧 CommandBuffer 的屏障与拷贝指令
	void cmdTransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
	void cmdBlitImage(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height);

private:
	VulkanDevice *vDevice = nullptr;
};