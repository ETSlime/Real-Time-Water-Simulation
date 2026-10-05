#include "Scene/Item/ItemManager.h"
#include "Scene/Item/Item.h"
#include "Scene/Ground.h"
#include "Scene/Item/Interactable/Interactable.h"
#include "Core/SaveSystem.h"



void ItemManager::Init(void)
{

}

void ItemManager::InitializeItemStatus(void)
{

	Node<Item*>* cur = m_itemList.getHead();
	while (cur != nullptr)
	{
		cur->data->Initialize();
		cur = cur->next;
	}
}

void ItemManager::Draw(void)
{
	// TODO もっと綺麗にドロウしましょう
	// カリング無効
	m_renderer.SetCullingMode(CULL_MODE_NONE);
	
	Node<Item*>* cur = m_itemList.getHead();
	while (cur != nullptr)
	{
		// モデル描画
		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
			m_renderer.SetRenderProgress(cur->data->GetInstance()->renderProgress);

		if (cur->data->GetAttributesConst().use)
			cur->data->Draw();

		if (cur->data->GetInstance()->renderProgress.progress < 1.0f)
		{
			RenderProgressCBuffer defaultRenderProgress;
			defaultRenderProgress.isRandomFade = false;
			defaultRenderProgress.progress = 1.0f;
			m_renderer.SetRenderProgress(defaultRenderProgress);
		}

		cur = cur->next;
	}

	// カリング設定を戻す
	m_renderer.SetCullingMode(CULL_MODE_BACK);
}

void ItemManager::Update(void)
{
	// TODO もっと綺麗にアップデートしましょう
	Node<Item*>* cur = m_itemList.getHead();
	while (cur != nullptr)
	{
		cur->data->Update();

		if (m_currentItemLoadCnt < m_totalItemLoadCnt)
		{

			long id = cur->data->GetID();
			if (itemLoadMap.contains(TO_UINT64(id)))
			{
				if (cur->data->GetLoad() && itemLoadMap[TO_UINT64(id)] == 0)
				{
					itemLoadMap[TO_UINT64(id)] = 1;
					m_currentItemLoadCnt++;
				}
			}
		}

		if (cur->data->GetDestroy())
		{
			Node<Item*>* toDelete = cur;
			cur = cur->next;
			// アイテムを削除
			toDelete->data->Destroy();
			m_itemList.remove(toDelete);
		}
		else
			cur = cur->next;
	}

}

Item* ItemManager::SpawnItem(Item* pItem)
{

	Item* item;
	
	
	// = new Item(itemType, trans);

	/*if (itemType < ItemType::Interactable) {
	
		item = new Item(itemType, trans);
	}
	else {
	
		item = new Interactable(itemType, trans);
	
	}*/

	item = pItem;

	item->Initialize();

	if (!itemLoadMap.contains(TO_UINT64(item->GetID())))
	{
		itemLoadMap[TO_UINT64(item->GetID())] = 0;
		m_totalItemLoadCnt++;
	}

	m_itemList.push_back(item);

	return item;
}

Item* ItemManager::GetItem(ItemType itemType) const
{
	Node<Item*>* cur = m_itemList.getHead();
	while (cur != nullptr)
	{
		if (cur->data->GetItemType() == itemType)
		{
			return cur->data;
		}
		cur = cur->next;
	}
	return nullptr;
}

Item* ItemManager::GetPickedItem(ItemType itemType) const
{
	Node<Item*>* cur = m_itemList.getHead();
	while (cur != nullptr)
	{
		if (cur->data->GetItemType() == itemType && cur->data->GetPickedUp() && !cur->data->GetUsed())
		{
			return cur->data;
		}
		cur = cur->next;
	}
	return nullptr;
}

void ItemManager::LoadSaveData(const SaveData& saveData)
{
	for (int i = 0; i < saveData.seedCount; i++)
	{
		Transform trans;
		trans.scl = XMFLOAT3(SEED_SIZE, SEED_SIZE, SEED_SIZE);
		Item* seed = SpawnItem(new Item(ItemType::Seed, trans));
		seed->Pickup();
	
	}

	for (int i = 0; i < saveData.keyCount; i++)
	{
		Transform trans;
		trans.scl = XMFLOAT3(KEY_SIZE, KEY_SIZE, KEY_SIZE);
		Item* key = SpawnItem(new Item(ItemType::Key, trans));
		key->Pickup();
	}

}

void ItemManager::RenderImGui(void)
{


}
