//=============================================================================
//
// [UIMainHUD.h]
// Author : 
// 
//=============================================================================
#include "UI/UIMainHUD.h"
#include "UI/Base/UIManager.h"
#include "Core/TextureMgr.h"
#include "Core/GameSystem.h"
#include "Scene/Player.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define HUD_EQUIPMENT_SLOT_TEXTURE_PATH			"data/TEXTURE/UI/ui_weapon_2.png"
#define HUD_EQUIPMENT_SLOT_TEXTURE_POS_X		SCREEN_CENTER_X * 1.74f
#define HUD_EQUIPMENT_SLOT_TEXTURE_POS_Y		SCREEN_CENTER_Y * 1.5f
#define HUD_EQUIPMENT_SLOT_TEXTURE_SIZE_X		545.0f
#define HUD_EQUIPMENT_SLOT_TEXTURE_SIZE_Y		435.0f

#define HUD_WEAPON_ICON_TEXTURE_PATH			"data/TEXTURE/UI/weapon.png"
#define HUD_MAIN_WEAPON_ICON_TEXTURE_POS_X		HUD_EQUIPMENT_SLOT_TEXTURE_POS_X * 1.04f
#define HUD_MAIN_WEAPON_ICON_TEXTURE_POS_Y		HUD_EQUIPMENT_SLOT_TEXTURE_POS_Y * 1.05f
#define HUD_MAIN_WEAPON_ICON_TEXTURE_SCALE		0.1f
#define HUD_SUB_WEAPON_ICON_TEXTURE_POS_X		HUD_EQUIPMENT_SLOT_TEXTURE_POS_X * 1.08f
#define HUD_SUB_WEAPON_ICON_TEXTURE_POS_Y		HUD_EQUIPMENT_SLOT_TEXTURE_POS_Y * 0.85f
#define HUD_SUB_WEAPON_ICON_TEXTURE_SCALE		0.04f
#define HUD_WEAPON_ICON_TEXTURE_SIZE_X			765.0f * 2.4f
#define HUD_WEAPON_ICON_TEXTURE_SIZE_Y			572.0f * 2.4f

#define HUD_GUN_ICON_TEXTURE_PATH				"data/TEXTURE/UI/gun.png"
#define HUD_MAIN_GUN_ICON_TEXTURE_POS_X			HUD_EQUIPMENT_SLOT_TEXTURE_POS_X * 1.04f
#define HUD_MAIN_GUN_ICON_TEXTURE_POS_Y			HUD_EQUIPMENT_SLOT_TEXTURE_POS_Y * 1.05f
#define HUD_MAIN_GUN_ICON_TEXTURE_SCALE			0.17f
#define HUD_SUB_GUN_ICON_TEXTURE_POS_X			HUD_EQUIPMENT_SLOT_TEXTURE_POS_X * 1.08f
#define HUD_SUB_GUN_ICON_TEXTURE_POS_Y			HUD_EQUIPMENT_SLOT_TEXTURE_POS_Y * 0.85f
#define HUD_SUB_GUN_ICON_TEXTURE_SCALE			0.08f
#define HUD_GUN_ICON_TEXTURE_SIZE_X				1197.0f
#define HUD_GUN_ICON_TEXTURE_SIZE_Y				658.0f

#define HUD_CROSSHAIR_TEXTURE_PATH				"data/TEXTURE/UI/crosshair.png"
#define HUD_CROSSHAIR_TEXTURE_SIZE_X			48.0f
#define HUD_CROSSHAIR_TEXTURE_SIZE_Y			48.0f


UIMainHUD::~UIMainHUD()
{
	for (UINT i = 0; i < m_sprites.getSize(); ++i)
	{
		if (m_sprites[i])
		{
			SAFE_DELETE(m_sprites[i]);
		}
	}
}

void UIMainHUD::Initialize(void)
{
	m_sprites.reserve(12);

	m_player = GameSystem::get_instance().GetPlayer();

	m_weaponIcon = new UISprite(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		TextureMgr::get_instance().CreateTexture(HUD_WEAPON_ICON_TEXTURE_PATH)
	);
	m_weaponIcon->SetSize(HUD_WEAPON_ICON_TEXTURE_SIZE_X, HUD_WEAPON_ICON_TEXTURE_SIZE_Y);
	m_weaponIcon->SetPosition(HUD_MAIN_WEAPON_ICON_TEXTURE_POS_X, HUD_MAIN_WEAPON_ICON_TEXTURE_POS_Y);
	m_weaponIcon->SetScale(HUD_MAIN_WEAPON_ICON_TEXTURE_SCALE, HUD_MAIN_WEAPON_ICON_TEXTURE_SCALE);
	m_sprites.push_back(m_weaponIcon);

	m_gunIcon = new UISprite(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		TextureMgr::get_instance().CreateTexture(HUD_GUN_ICON_TEXTURE_PATH)
	);
	m_gunIcon->SetSize(HUD_GUN_ICON_TEXTURE_SIZE_X, HUD_GUN_ICON_TEXTURE_SIZE_Y);
	m_gunIcon->SetPosition(HUD_SUB_GUN_ICON_TEXTURE_POS_X, HUD_SUB_GUN_ICON_TEXTURE_POS_Y);
	m_gunIcon->SetScale(HUD_SUB_GUN_ICON_TEXTURE_SCALE, HUD_SUB_GUN_ICON_TEXTURE_SCALE);
	m_sprites.push_back(m_gunIcon);

	m_crosshair = new UISprite(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		TextureMgr::get_instance().CreateTexture(HUD_CROSSHAIR_TEXTURE_PATH)
	);
	m_crosshair->SetSize(HUD_CROSSHAIR_TEXTURE_SIZE_X, HUD_CROSSHAIR_TEXTURE_SIZE_Y);
	m_crosshair->SetPosition(SCREEN_CENTER_X, SCREEN_CENTER_Y);
	m_crosshair->SetScale(1.0f, 1.0f);
	m_sprites.push_back(m_crosshair);
}

void UIMainHUD::Update(void)
{
	if (m_player == nullptr)
	{
		m_player = GameSystem::get_instance().GetPlayer();
		if (m_player == nullptr) return; // プレイヤーがまだ初期化されていない場合は何もしない
	}

	PlayerAttributes& attributes = m_player->GetPlayerAttributes();

	m_crosshair->SetVisible(attributes.aimMode);

	// アイコンの位置を更新
	if (m_player->GetMeleeMode())
	{
		m_weaponIcon->SetPosition(HUD_MAIN_WEAPON_ICON_TEXTURE_POS_X, HUD_MAIN_WEAPON_ICON_TEXTURE_POS_Y);
		m_weaponIcon->SetScale(HUD_MAIN_WEAPON_ICON_TEXTURE_SCALE, HUD_MAIN_WEAPON_ICON_TEXTURE_SCALE);
		m_gunIcon->SetPosition(HUD_SUB_GUN_ICON_TEXTURE_POS_X, HUD_SUB_GUN_ICON_TEXTURE_POS_Y);
		m_gunIcon->SetScale(HUD_SUB_GUN_ICON_TEXTURE_SCALE, HUD_SUB_GUN_ICON_TEXTURE_SCALE);
	}
	else
	{
		m_weaponIcon->SetPosition(HUD_SUB_WEAPON_ICON_TEXTURE_POS_X, HUD_SUB_WEAPON_ICON_TEXTURE_POS_Y);
		m_weaponIcon->SetScale(HUD_SUB_WEAPON_ICON_TEXTURE_SCALE, HUD_SUB_WEAPON_ICON_TEXTURE_SCALE);
		m_gunIcon->SetPosition(HUD_MAIN_GUN_ICON_TEXTURE_POS_X, HUD_MAIN_GUN_ICON_TEXTURE_POS_Y);
		m_gunIcon->SetScale(HUD_MAIN_GUN_ICON_TEXTURE_SCALE, HUD_MAIN_GUN_ICON_TEXTURE_SCALE);
	}
}

void UIMainHUD::Draw(void)
{
	for (UINT i = 0; i < m_sprites.getSize(); ++i)
	{
		if (m_sprites[i] && m_sprites[i]->IsVisible())
		{
			m_sprites[i]->Draw();
		}
	}
}

void UIMainHUD::DrawNumers(int number, float posX, float posY)
{
}
