#pragma once
#include <string>

class UIPanel
{
public:
	UIPanel(const std::string &name) : panelName(name) {}
	virtual ~UIPanel() = default;

	virtual void draw() = 0;

	const std::string &getName() const { return panelName; }

protected:
	std::string panelName;
};