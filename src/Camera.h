#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class Camera
{
public:
	void init(glm::vec3 startPosition, float fov = 45.0f);

	// 处理输入并更新摄像机状态，如果状态发生变化（需要重置光追累加），返回 true
	bool update(GLFWwindow *window, float deltaTime);

	glm::vec3 getPosition() const { return position; }
	glm::vec3 getForward() const { return forward; }
	glm::vec3 getUp() const { return up; }
	glm::vec3 getRight() const { return right; }
	float getFov() const { return fov; }

private:
	void updateCameraVectors();

	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 up;
	glm::vec3 right;
	glm::vec3 worldUp;

	float yaw;
	float pitch;
	float fov;

	float movementSpeed;
	float mouseSensitivity;

	bool firstMouse;
	double lastMouseX;
	double lastMouseY;
};