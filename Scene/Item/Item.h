#pragma once

#include "Scene/GameObject.h"
#include "Utility/SimpleArray.h"
#include "Core/Timer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class ItemType
{
	Weapon,
	GB,
	Key,
	Seed,
	Bullet,
	Interactable,// Interactableの方は手に入れない
	IntSoil,
	IntPSeed,
	IntVines,
	IntAutumnDoorL,
	IntAutumnDoorR,
	IntGate,
	Max
};

//*****************************************************************************
// 構造体定義
//*****************************************************************************
struct ItemAttributes
{
	bool used;
	bool pickedUp;
	bool isThrew;
	bool initialized;
	bool destroy;
	ItemType itemType;

	Transform initTrans;
};


class Item : public GameObject<ModelInstance> {

public:

	Item(ItemType itemType, Transform trans);
	~Item();

	virtual void Initialize(bool enableCollider = true);
	inline const ItemAttributes& GetItemAttributes(void) { return m_itemAttributes; }

	void Update(void) override;
	void Draw(void) override;
	void PlayEffect(HitColliderType type, const XMFLOAT3& hitNormal = XMFLOAT3{});

	virtual void Use(void) { m_itemAttributes.used = true; }
	void Pickup(void);
	bool GetUsed(void) { return m_itemAttributes.used; }
	bool GetPickedUp(void) { return m_itemAttributes.pickedUp; }
	virtual void SetActive(bool enable);
	void SetColliderEnable(bool enable) { instance.collider.enable = enable; }
	void SetIsThrew(bool threw) { m_itemAttributes.isThrew = threw; }
	bool GetIsThrew(void) { return m_itemAttributes.isThrew; }
	void SetDestroy(bool destroy) { m_itemAttributes.destroy = destroy; }
	bool GetDestroy(void) { return m_itemAttributes.destroy; }
	ItemType GetItemType(void) const { return m_itemAttributes.itemType; }

	virtual void Interact() {};
	virtual void ResetStatus() { m_itemAttributes.used = false; m_itemAttributes.pickedUp = false;};

protected:
	// プレイヤーがアイテムを拾ったときのコールバック
	static void OnItemCollisionCallback(const CollisionEvent& event, void* context);
	ItemAttributes m_itemAttributes;

	Collider m_triggerCollider;

};