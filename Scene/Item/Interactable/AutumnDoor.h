#pragma once

#include "Interactable.h"

enum class DoorSide
{
	Left,
	Right
};

class AutumnDoor : public Interactable {

public:

	AutumnDoor(DoorSide side, Transform transform) :Interactable(side == DoorSide::Left?ItemType::IntAutumnDoorL:ItemType::IntAutumnDoorR, transform), m_side(side) {};

	void Interact() override;

	void SetLinkedDoor(AutumnDoor* pAutumnDoor) { m_linkedDoor = pAutumnDoor; }

	char* GetPromptTextureLocation() override;

	void ResetStatus() override;

private:

	bool m_opened = false;

	AutumnDoor* m_linkedDoor = nullptr;

	DoorSide m_side;


};