#include "ImageManager.h"
#include <stdexcept>

void ImageManager::init(VulkanDevice *device)
{
	vDevice = device;
}

AllocatedImage ImageManager::createStorageImage(uint32_t width, uint32_t height, VkFormat format)
{
	AllocatedImage result;
	result.format = format;
	result.extent = {width, height, 1};

	VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = result.extent;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(vDevice->device, &imageInfo, nullptr, &result.image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create storage image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vDevice->device, result.image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = vDevice->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(vDevice->device, &allocInfo, nullptr, &result.memory) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to allocate image memory!");
	}
	vkBindImageMemory(vDevice->device, result.image, result.memory, 0);

	VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	viewInfo.image = result.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vDevice->device, &viewInfo, nullptr, &result.view) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image view!");
	}

	transitionImageLayout(result.image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	return result;
}

void ImageManager::destroyImage(const AllocatedImage &allocatedImage)
{
	if (allocatedImage.view != VK_NULL_HANDLE)
	{
		vkDestroyImageView(vDevice->device, allocatedImage.view, nullptr);
	}
	if (allocatedImage.image != VK_NULL_HANDLE)
	{
		vkDestroyImage(vDevice->device, allocatedImage.image, nullptr);
		vkFreeMemory(vDevice->device, allocatedImage.memory, nullptr);
	}
}

void ImageManager::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = vDevice->beginSingleTimeCommands();
	cmdTransitionImageLayout(commandBuffer, image, oldLayout, newLayout);
	vDevice->endSingleTimeCommands(commandBuffer);
}

void ImageManager::cmdTransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition in cmdTransitionImageLayout!");
	}

	vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void ImageManager::cmdBlitImage(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, uint32_t width, uint32_t height)
{
	VkImageBlit blit{};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;

	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;

	vkCmdBlitImage(cmd,
		       srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		       dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		       1, &blit,
		       VK_FILTER_NEAREST);
}