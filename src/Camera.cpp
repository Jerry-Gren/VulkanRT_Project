#include "Camera.h"

void Camera::init(glm::vec3 startPosition, float startFov)
{
	position = startPosition;
	worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	yaw = -90.0f;
	pitch = 0.0f;
	fov = startFov;

	movementSpeed = 2.5f;
	mouseSensitivity = 0.1f;

	firstMouse = true;
	lastMouseX = 0.0;
	lastMouseY = 0.0;

	updateCameraVectors();
}

bool Camera::update(GLFWwindow *window, float deltaTime)
{
	bool moved = false;

	// 1. 键盘移动 (WASD + EQ上下)
	float velocity = movementSpeed * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		position += forward * velocity;
		moved = true;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		position -= forward * velocity;
		moved = true;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		position -= right * velocity;
		moved = true;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		position += right * velocity;
		moved = true;
	}
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
	{
		position += up * velocity;
		moved = true;
	}
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
	{
		position -= up * velocity;
		moved = true;
	}

	// 2. 鼠标视角旋转 (按住右键拖动以避免与 ImGui 冲突)
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		if (firstMouse)
		{
			lastMouseX = xpos;
			lastMouseY = ypos;
			firstMouse = false;
		}

		double xoffset = xpos - lastMouseX;
		double yoffset = lastMouseY - ypos;
		lastMouseX = xpos;
		lastMouseY = ypos;

		if (xoffset != 0.0 || yoffset != 0.0)
		{
			xoffset *= mouseSensitivity;
			yoffset *= mouseSensitivity;

			yaw += static_cast<float>(xoffset);
			pitch += static_cast<float>(yoffset);

			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;

			updateCameraVectors();
			moved = true;
		}
	}
	else
	{
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		firstMouse = true;
	}

	return moved;
}

void Camera::updateCameraVectors()
{
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

	forward = glm::normalize(front);
	right = glm::normalize(glm::cross(forward, worldUp));
	up = glm::normalize(glm::cross(right, forward));
}