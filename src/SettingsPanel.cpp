#include "SettingsPanel.h"
#include <imgui.h>

SettingsPanel::SettingsPanel(SceneConfig *config)
    : UIPanel("Scene Settings"), sceneConfig(config) {}

void SettingsPanel::draw()
{
	ImGui::Begin(panelName.c_str());

	if (ImGui::CollapsingHeader("Environment & Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::ColorEdit3("Sky Color", &sceneConfig->envColor.x))
			sceneConfig->isDirty = true;
		if (ImGui::SliderFloat("Sky Intensity", &sceneConfig->envIntensity, 0.0f, 10.0f))
			sceneConfig->isDirty = true;

		ImGui::Separator();
		if (ImGui::SliderFloat3("Sun Direction", &sceneConfig->lightDirection.x, -1.0f, 1.0f))
		{
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

		float angleDeg = glm::degrees(sceneConfig->lightAngleRadius);
		if (ImGui::SliderFloat("Sun Size (Deg)", &angleDeg, 0.0f, 10.0f))
		{
			sceneConfig->lightAngleRadius = glm::radians(angleDeg);
			sceneConfig->isDirty = true;
		}
	}

	if (ImGui::CollapsingHeader("Global Material Override", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Enable Override", &sceneConfig->overrideMaterial))
			sceneConfig->isDirty = true;

		ImGui::BeginDisabled(!sceneConfig->overrideMaterial);
		if (ImGui::ColorEdit3("Albedo / Color", &sceneConfig->albedo.x))
			sceneConfig->isDirty = true;
		if (ImGui::SliderFloat("Roughness", &sceneConfig->roughness, 0.0f, 1.0f))
			sceneConfig->isDirty = true;
		if (ImGui::SliderFloat("Metallic", &sceneConfig->metallic, 0.0f, 1.0f))
			sceneConfig->isDirty = true;
		if (ImGui::SliderFloat("Transmission", &sceneConfig->transmission, 0.0f, 1.0f))
			sceneConfig->isDirty = true;
		if (ImGui::SliderFloat("IOR (Index of Refraction)", &sceneConfig->ior, 1.0f, 3.0f))
			sceneConfig->isDirty = true;
		ImGui::EndDisabled();
	}

	ImGui::End();
}