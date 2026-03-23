#pragma once
#include "UIPanel.h"
#include "Types.h"

class SettingsPanel : public UIPanel
{
public:
	// 注入配置指针，面板本身不持有状态
	SettingsPanel(SceneConfig *config);
	void draw() override;

private:
	SceneConfig *sceneConfig;
};