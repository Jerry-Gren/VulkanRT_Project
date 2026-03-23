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
	glm::vec3 envColor = {0.5f, 0.7f, 1.0f};
	float envIntensity = 1.0f;

	glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, 1.0f, -0.5f));
	glm::vec3 lightColor = {1.0f, 0.95f, 0.9f};
	float lightIntensity = 5.0f;
	float lightAngleRadius = glm::radians(2.0f);

	// 全局材质调试覆盖参数
	bool overrideMaterial = false;
	glm::vec3 albedo = {1.0f, 1.0f, 1.0f};
	float roughness = 0.05f;
	float metallic = 0.0f;
	float transmission = 1.0f;
	float ior = 1.5f;

	bool isDirty = true;
};

// 3. 全局推送常量 (每帧更新的管线参数)
// 严格控制在 128 字节内 (Vulkan 最低保证标准)
struct PushConstants
{
	glm::vec4 camPos_Fov;	   // xyz: pos, w: fov (tan(fov/2))
	glm::vec4 camDir_Aspect;   // xyz: dir, w: aspect
	glm::vec4 camUp_Frame;	   // xyz: up,  w: frameCount
	glm::vec4 camRight_EnvInt; // xyz: right, w: envIntensity
	glm::vec4 envColor_LgAng;  // xyz: envColor, w: lightAngle
	glm::vec4 lightDir_LgInt;  // xyz: lightDir, w: lightIntensity
	glm::vec4 albedo_Rough;	   // xyz: albedo, w: roughness
	glm::vec4 matParams;	   // x: metallic, y: transmission, z: ior, w: overrideMode
};

struct GPUMaterial
{
	glm::vec4 baseColor;
	float metallic;
	float roughness;
	float transmission; // 0.0: 不透明, 1.0: 全透明玻璃
	float ior;	    // 折射率 (如玻璃 1.5, 水 1.33)
	int textureID;
	int padding[3]; // 保持 16 字节对齐
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