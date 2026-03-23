#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// 1. 显存资源分配封装
struct AllocatedBuffer
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDeviceAddress deviceAddress = 0;
};

struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkExtent3D extent = {0, 0, 0};
};

// 2. 场景几何体基础单元
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

// 用于 UI 和主循环共享的状态配置，隔离 UI 渲染与底层逻辑
struct SceneConfig
{
	// 现有环境光（作为 Miss Shader 的天空）
	glm::vec3 envColor = {0.5f, 0.7f, 1.0f};
	float envIntensity = 1.0f;

	// 【新增】：显式面光源/太阳光 (NEE)
	glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, 1.0f, -0.5f));
	glm::vec3 lightColor = {1.0f, 0.95f, 0.9f};
	float lightIntensity = 5.0f;
	float lightAngleRadius = glm::radians(2.0f); // 太阳看起来的大小，控制阴影柔和度

	bool isDirty = true;
};

// 3. 全局推送常量 (每帧更新的管线参数)
// 严格控制在 128 字节内 (Vulkan 最低保证标准)
struct PushConstants
{
	glm::vec4 cameraPos;
	glm::vec4 cameraDir;
	glm::vec4 cameraUp;
	glm::vec4 cameraRight;
	glm::vec4 projParams;
	glm::vec4 envConfig; // xyz: envColor * envIntensity, w: frameCount

	// 【新增】：光源配置数据
	glm::vec4 lightDir;   // xyz: direction, w: angleRadius
	glm::vec4 lightColor; // xyz: color * intensity, w: padding
};

struct GPUMaterial
{
	glm::vec4 baseColor;
	float metallic;
	float roughness;
	int textureID; // -1 表示无纹理
	int padding;   // 满足 std140/std430 对齐要求
};

struct SubMesh
{
	uint32_t firstIndex;
	uint32_t indexCount;
	int32_t vertexOffset;
	uint32_t materialIndex;
};

struct InstanceData
{
	glm::mat4 transform;
	uint32_t subMeshIndex;
};

// 场景的纯数据容器，由 ModelLoader 产出，供管线消费
struct SceneData
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<GPUMaterial> materials;
	std::vector<SubMesh> subMeshes;
	std::vector<InstanceData> instances;
};