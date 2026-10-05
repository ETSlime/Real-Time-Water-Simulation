#pragma once
//=============================================================================
//
// アイテムたちのまとめ役ちゃん [ItemManager.h]
// Author : 
//
//
//=============================================================================
#include "Core/Graphics/ConstantBufferStructs.h"
#include "Scene/Item/Item.h"
#include "Utility/DoubleLinkedList.h"
#include "Utility/SingletonBase.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define KEY_SIZE			(15.0f)
#define KEY_POS				XMFLOAT3(-5371.0f, -4670.0f, -1295.0f);

#define SEED_SIZE			(0.16f)

// 前方宣言
struct SaveData;

class ItemManager: public SingletonBase<ItemManager>, public IDebugUI 
{

public:
	void Init(void);
	void InitializeItemStatus(void);
	void Draw(void);
	void Update(void);

	Item* SpawnItem(Item* pItem);
	Item* GetItem(ItemType itemType) const;
	Item* GetPickedItem(ItemType itemType) const;
	int GetTotalItemLoadCnt(void) const { return m_totalItemLoadCnt; }
	int GetItemLoadedCnt(void) const { return m_currentItemLoadCnt; }

	void LoadSaveData(const SaveData& saveData);

private:

	HashMap<uint64_t, int, HashUInt64, EqualUInt64> itemLoadMap =
		HashMap<uint64_t, int, HashUInt64, EqualUInt64>(
			static_cast<int>(ItemType::Max),
			HashUInt64(),
			EqualUInt64()
		);

	int m_totalItemLoadCnt = 0;
	int m_currentItemLoadCnt = 0;

	bool m_drawBoundingBox = true;

	DoubleLinkedList<Item*> m_itemList;

	Renderer& m_renderer = Renderer::get_instance();

	virtual void RenderImGui(void) override;


};