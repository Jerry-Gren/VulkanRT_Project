#include "SettingsPanel.h"
#include <imgui.h>

SettingsPanel::SettingsPanel(SceneConfig *config)
    : UIPanel("Scene Settings"), sceneConfig(config) {}

void SettingsPanel::draw()
{
	ImGui::Begin(panelName.c_str());

	ImGui::Text("Environment (Sky)");
	if (ImGui::ColorEdit3("Sky Color", &sceneConfig->envColor.x))
		sceneConfig->isDirty = true;
	if (ImGui::SliderFloat("Sky Intensity", &sceneConfig->envIntensity, 0.0f, 10.0f))
		sceneConfig->isDirty = true;

	ImGui::Separator();

	ImGui::Text("Directional Light (Sun)");
	// 太阳方向控制
	if (ImGui::SliderFloat3("Sun Direction", &sceneConfig->lightDirection.x, -1.0f, 1.0f))
	{
		// 防止零向量崩溃
		if (glm::length(sceneConfig->lightDirection) > 0.001f)
		{
			sceneConfig->lightDirection = glm::normalize(sceneConfig->lightDirection);
			sceneConfig->isDirty = true;
		}
	}

	if (ImGui::ColorEdit3("Sun Color", &sceneConfig->lightColor.x))
		sceneConfig->isDirty = true;
	if (ImGui::SliderFloat("Sun Intensity", &sceneConfig->lightIntensity, 0.0f, 50.0f))
		sceneConfig->isDirty = true;

	// 控制太阳的物理体积（角度半径），越大阴影越柔和
	float angleDeg = glm::degrees(sceneConfig->lightAngleRadius);
	if (ImGui::SliderFloat("Sun Size (Deg)", &angleDeg, 0.0f, 10.0f))
	{
		sceneConfig->lightAngleRadius = glm::radians(angleDeg);
		sceneConfig->isDirty = true;
	}

	ImGui::End();
}