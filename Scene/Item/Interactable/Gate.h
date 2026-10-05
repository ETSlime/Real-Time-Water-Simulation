#pragma once

#include "Interactable.h"

class Gate : public Interactable {

public:

	Gate(Transform transform) :Interactable(ItemType::IntGate, transform){};

	void Interact() override;

	char* GetPromptTextureLocation() override;

private:

	bool m_opened = false;


};