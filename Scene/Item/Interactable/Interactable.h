#pragma once

#include "Scene/Item/Item.h"

class Interactable : public Item {

public:

	Interactable(ItemType itemType, Transform transform);
	~Interactable();

	void Update(void) override;
	void Draw(void) override;

	void SetActive(bool active) override;

	virtual void Interact() {};

	virtual char* GetPromptTextureLocation() { return "data/TEXTURE/UI/interactUI.png"; }

private:
	FBXLoader& m_fbxLoader = FBXLoader::get_instance();

	Collider m_triggerCollider;

};