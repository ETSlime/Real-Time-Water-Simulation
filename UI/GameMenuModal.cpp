//=============================================================================
//
// [GameMenuModal.cpp]
// Author : 
// 
//=============================================================================
#include "UI/GameMenuModal.h"
#include "Core/GameSystem.h"
#include "Core/SaveSystem.h"
#include "Core/AudioManager.h"

GameMenuModal::GameMenuModal(GameSystem* gameSystem, SceneID sceneID) : m_gameSystem(gameSystem)
{
	m_currentTimeControl = static_cast<TimeControlButton>(sceneID);
	m_elements.reserve(12);

	bool allowSaveAndSwitchScene = m_gameSystem->AllowSaveAndSwitch();

	m_background = new UISprite(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		TextureMgr::get_instance().CreateTexture(MENU_BACKGROUND_TEXTURE_PATH));
	m_background->SetSize(MENU_BACKGROUND_TEXTURE_SIZE_X, MENU_BACKGROUND_TEXTURE_SIZE_Y);
	m_background->SetScale(0.5f, 0.5f);

	m_btnResume = new UIButton(
		{ MENU_BTN_RESUME_TEXTURE_POS_X, MENU_BTN_RESUME_TEXTURE_POS_Y },
		{ MENU_BTN_RESUME_TEXTURE_SIZE_X, MENU_BTN_RESUME_TEXTURE_SIZE_Y }, "Resume");
	m_btnResume->InitWithSingleTexture(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		MENU_BTN_RESUME_TEXTURE_PATH,
		XMFLOAT4(1, 1, 1, 1), // Normal
		XMFLOAT4(0.8f, 0.8f, 0.8f, 1), // Hover
		XMFLOAT4(0.6f, 0.6f, 0.6f, 1), // Pressed
		XMFLOAT4(0.5f, 0.5f, 0.5f, 1) // Disabled
	);
	m_btnResume->SetScale(0.5f, 0.5f);
	m_btnResume->SetOnClick(OnResumeClicked, m_gameSystem);
	m_elements.push_back(m_btnResume);

	m_btnQuit = new UIButton(
		{ MENU_BTN_QUIT_TEXTURE_POS_X, MENU_BTN_QUIT_TEXTURE_POS_Y },
		{ MENU_BTN_QUIT_TEXTURE_SIZE_X, MENU_BTN_QUIT_TEXTURE_SIZE_Y }, "Quit to Title");
	m_btnQuit->InitWithSingleTexture(
		UIManager::get_instance().GetSpriteVertexBuffer(),
		MENU_BTN_QUIT_TEXTURE_PATH,
		XMFLOAT4(1, 1, 1, 1), // Normal
		XMFLOAT4(0.8f, 0.8f, 0.8f, 1), // Hover
		XMFLOAT4(0.6f, 0.6f, 0.6f, 1), // Pressed
		XMFLOAT4(0.5f, 0.5f, 0.5f, 1) // Disabled
	);
	m_btnQuit->SetScale(0.5f, 0.5f);
	m_btnQuit->SetOnClick(OnExitClicked, m_gameSystem); // 終了ボタンにコールバックを設定
	m_elements.push_back(m_btnQuit);

	OnAppear();
}

GameMenuModal::~GameMenuModal()
{
	SAFE_DELETE(m_btnResume);
	SAFE_DELETE(m_btnQuit);

	SAFE_DELETE(m_background);
}

void GameMenuModal::OnAppear(void)
{
	// メニュー背景
	ApplyBounceTween(m_background, { MENU_BACKGROUND_TEXTURE_POS_X, MENU_BACKGROUND_TEXTURE_POS_Y });

	// リジュームボタン
	ApplyBounceTween(m_btnResume, { MENU_BTN_RESUME_TEXTURE_POS_X, MENU_BTN_RESUME_TEXTURE_POS_Y });

	// 終了ボタン
	ApplyBounceTween(m_btnQuit, { MENU_BTN_QUIT_TEXTURE_POS_X, MENU_BTN_QUIT_TEXTURE_POS_Y });
}


void GameMenuModal::Update(void)
{
	for (auto* elem : m_elements)
		elem->Update();
}

void GameMenuModal::Draw(void)
{
	// 背景を描画
	m_background->Draw();

	// ボタンを描画
	for (auto* elem : m_elements)
		elem->Draw();
}

void GameMenuModal::CollectTweenTargets(SimpleArray<ISpriteTransformable*>& outTargets) const
{
	outTargets.push_back(m_background);
	outTargets.push_back(m_btnResume);
	outTargets.push_back(m_btnQuit);
}

void GameMenuModal::ApplyBounceTween(ISpriteTransformable* target, const XMFLOAT2& targetPos, float offset)
{
	// 初期位置を計算（目標位置の上方）
	float startY = targetPos.y - offset;
	target->SetPosition({ targetPos.x, startY });

	// バウンドするアニメーションを追加
	m_TweenManager->AddMove(target, { targetPos.x, targetPos.y + 50 }, 0.3f, EaseType::OutCubic)
		->ThenMove({ targetPos.x, targetPos.y - 20 }, 0.2f, EaseType::InOutCubic)
		->ThenMove({ targetPos.x, targetPos.y + 10 }, 0.15f, EaseType::OutCubic)
		->ThenMove({ targetPos.x, targetPos.y }, 0.1f, EaseType::OutCubic);
}

void GameMenuModal::OnResumeClicked(void* userData)
{
	GameSystem* self = static_cast<GameSystem*>(userData);
	self->SetResumeBtnClick(true); // ゲームシステムにリジュームを通知
}

void GameMenuModal::OnExitClicked(void* userData)
{
	UIManager::get_instance().GetFader().StartFadeOut(); // 画面を黒くフェードイン

	DeferredTaskOptions options{};
	options.async = false; // 同期で実行

	RawDeferredTask::Add(
		nullptr,
		[](void* ptr)
		{
			ExitGame(); // ゲームを終了する
		},
		[](void* ptr)
		{
			return UIManager::get_instance().GetFader().IsFullyHidden(); // 完全に暗くなったら実行する♪
		},
		options
	);
}