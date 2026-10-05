#pragma once
//=============================================================================
//
// [GameMenuModal.h]
// Author : 
// 
//=============================================================================
#include "UI/Base/UIElement.h"
#include "UI/Base/UIButton.h"
#include "UI/Base/UIManager.h"
#include "UI/Base/TweenManager.h"
#include "Scene/Ground.h"
#include "Core/TextureMgr.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define MENU_BACKGROUND_TEXTURE_PATH		"data/TEXTURE/UI/menu_bg.png"
#define MENU_BACKGROUND_TEXTURE_POS_X		SCREEN_CENTER_X
#define MENU_BACKGROUND_TEXTURE_POS_Y		SCREEN_CENTER_Y * 0.9f // 画面中央の少し上に配置
#define MENU_BACKGROUND_TEXTURE_SIZE_X		553.0f * 2.3f
#define MENU_BACKGROUND_TEXTURE_SIZE_Y		524.0f * 2.3f

#define MENU_BTN_RESUME_TEXTURE_PATH		"data/TEXTURE/UI/back.png"
#define MENU_BTN_RESUME_TEXTURE_POS_X		MENU_BACKGROUND_TEXTURE_POS_X
#define MENU_BTN_RESUME_TEXTURE_POS_Y		MENU_BACKGROUND_TEXTURE_POS_Y
#define MENU_BTN_RESUME_TEXTURE_SIZE_X		490.0f
#define MENU_BTN_RESUME_TEXTURE_SIZE_Y		135.0f


#define MENU_BTN_QUIT_TEXTURE_PATH			"data/TEXTURE/UI/quitgame.png"
#define MENU_BTN_QUIT_TEXTURE_POS_X			MENU_BACKGROUND_TEXTURE_POS_X
#define MENU_BTN_QUIT_TEXTURE_POS_Y			MENU_BACKGROUND_TEXTURE_POS_Y + 120.0f
#define MENU_BTN_QUIT_TEXTURE_SIZE_X		490.0f
#define MENU_BTN_QUIT_TEXTURE_SIZE_Y		135.0f

enum class TimeControlButton
{
	Spring_Day = 0,
	Spring_Night,
	Summer_Day,
	Summer_Night,
	Autumn_Day,
	Autumn_Night,
	Winter_Day,
	Winter_Night,
	NUM
};

class GameSystem;

class GameMenuModal : public UIElement
{
public:
	GameMenuModal(GameSystem* gameSystem, SceneID sceneID);
	~GameMenuModal();

	void OnAppear(void);

	void Update(void) override;
	void Draw(void) override;

	void CollectTweenTargets(SimpleArray<ISpriteTransformable*>& outTargets) const override;

private:
	struct SceneSwitchClickedContext
	{
		GameMenuModal* self;
		TimeControlButton currentTimeControl;
	};

	// ボタンのクリックイベントハンドラー
	static void OnResumeClicked(void* userData);
	static void OnExitClicked(void* userData);

	void ApplyBounceTween(ISpriteTransformable* target, const XMFLOAT2& targetPos, float offset = 300.0f);

	// クリックロックの管理
	void LockClick(void) { m_clickLocked = true; }
	void UnlockClick(void) { m_clickLocked = false; }
	bool IsClickLocked(void) const { return m_clickLocked; }

	bool m_clickLocked = false; // クリックロック（多重起動防止）

	GameSystem* m_gameSystem = nullptr;
    UIButton* m_btnResume = nullptr;
    UIButton* m_btnQuit = nullptr;

	UISprite* m_background = nullptr; // メニュー背景スプライト

	TimeControlButton m_currentTimeControl = TimeControlButton::Spring_Day; // 現在の時間制御状態

    SimpleArray<UIElement*> m_elements;

	TweenManager* m_TweenManager = &TweenManager::get_instance();
};