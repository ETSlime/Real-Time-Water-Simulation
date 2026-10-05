#pragma once
//=============================================================================
//
//  [UITitleScene.h]
// Author : 
//
//=============================================================================
#include "UI/Base/UIManager.h"
#include "UI/Base/UIAnimatedSprite.h"
#include "UI/Base/UIButton.h"
#include "Core/TextureMgr.h"
#include "Core/SaveSystem.h"
#include "Core/AudioManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define TITLE_ANIM_TEXTURE_PATH                 "data/TEXTURE/bg000.png"
#define TITLE_ANIM_TILE_X                       1
#define TITLE_ANIM_TILE_Y                       1
#define TITLE_ANIM_SPEED                        0.1f

// タイトル画面のボタン設定
#define TITLE_BTN_TEXTURE_SIZE_X                471 * 0.6f
#define TITLE_BTN_TEXTURE_SIZE_Y                471 * 0.6f
#define TITLE_BTN_TEXTURE_SPACE_X               500 // ボタン間のスペース

#define TITLE_BTN_NEWGAME_TEXTURE_PATH          "data/TEXTURE/UI/newgame_xlixk.png"
#define TITLE_BTN_NEWGAME_TEXTURE_POS_X         (SCREEN_CENTER_X * 0.48f)
#define TITLE_BTN_NEWGAME_TEXTURE_POS_Y         (SCREEN_CENTER_Y * 1.6f) // 画面中央の下に配置

#define TITLE_BTN_LOADGAME_TEXTURE_PATH          "data/TEXTURE/UI/loadgame_click.png"
#define TITLE_BTN_EXITGAME_TEXTURE_PATH          "data/TEXTURE/UI/quit_click.png"


class UITitleScene : public UIElement
{
public:
    UITitleScene() {}

    void Init(GameSystem* system)
    {
        // タイトルアニメの生成
        m_titleAnim = new UIAnimatedSprite();

        // テクスチャと情報を設定（例：6列×1行のタイトルアニメ）
        m_titleAnim->Init(
            TextureMgr::get_instance().CreateTexture(TITLE_ANIM_TEXTURE_PATH),
            UIManager::get_instance().GetSpriteVertexBuffer(),
            TITLE_ANIM_TILE_X, TITLE_ANIM_TILE_Y,
            TITLE_ANIM_SPEED, true // TITLE_ANIM_SPEED秒ごと、ループ再生
        );

        // 表示位置とサイズを設定
        m_titleAnim->SetPosition({ static_cast<float>(SCREEN_CENTER_X), static_cast<float>(SCREEN_CENTER_Y) });
        m_titleAnim->SetSize({ static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) });

        // 新規ゲームボタンの初期化
        m_btnNewGame = new UIButton();
        m_btnNewGame->InitWithSingleTexture(
            UIManager::get_instance().GetSpriteVertexBuffer(),
            TITLE_BTN_NEWGAME_TEXTURE_PATH,
            XMFLOAT4(1, 1, 1, 1), // Normal
            XMFLOAT4(0.8f, 0.8f, 0.8f, 1), // Hover
            XMFLOAT4(0.6f, 0.6f, 0.6f, 1), // Pressed
            XMFLOAT4(0.5f, 0.5f, 0.5f, 1) // Disabled
        );
        m_btnNewGame->SetPosition({
            static_cast<float>(TITLE_BTN_NEWGAME_TEXTURE_POS_X),
            static_cast<float>(TITLE_BTN_NEWGAME_TEXTURE_POS_Y)
            });
        m_btnNewGame->SetSize({
            static_cast<float>(TITLE_BTN_TEXTURE_SIZE_X),
            static_cast<float>(TITLE_BTN_TEXTURE_SIZE_Y)
            });
        m_btnNewGame->SetOnClick(OnClick_NewGame, this);



        // ゲーム終了ボタンの初期化
        m_btnExitGame = new UIButton();
        m_btnExitGame->InitWithSingleTexture(
            UIManager::get_instance().GetSpriteVertexBuffer(),
            TITLE_BTN_EXITGAME_TEXTURE_PATH,
            XMFLOAT4(1, 1, 1, 1), // Normal
            XMFLOAT4(0.8f, 0.8f, 0.8f, 1), // Hover
            XMFLOAT4(0.6f, 0.6f, 0.6f, 1), // Pressed
            XMFLOAT4(0.5f, 0.5f, 0.5f, 1) // Disabled
        );
        m_btnExitGame->SetPosition({
            static_cast<float>(TITLE_BTN_NEWGAME_TEXTURE_POS_X + TITLE_BTN_TEXTURE_SPACE_X * 2),
            static_cast<float>(TITLE_BTN_NEWGAME_TEXTURE_POS_Y)
            });
        m_btnExitGame->SetSize({
            static_cast<float>(TITLE_BTN_TEXTURE_SIZE_X),
            static_cast<float>(TITLE_BTN_TEXTURE_SIZE_Y)
            });
        m_btnExitGame->SetOnClick(OnClickExitGame, nullptr); // ゲーム終了ボタンのコールバック設定

        // 最初はフェードインから始める（必要であれば）
        m_UIManager.GetFader().StartFadeIn(1.0f); // 1秒でフェードイン

        AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_BGM_title);
    }

    ~UITitleScene()
    {
        SAFE_DELETE(m_titleAnim);
        SAFE_DELETE(m_btnNewGame);
		SAFE_DELETE(m_btnExitGame);
    }

    void Update(void) override
    {
        if (m_titleAnim)
            m_titleAnim->Update();

        if (m_btnNewGame)
            m_btnNewGame->Update();

		if (m_btnExitGame)
			m_btnExitGame->Update();

        auto& fader = UIManager::get_instance().GetFader();

        // フェードアウトがまだ始まってない場合、決定入力を待つ
        if (m_fadeOutStarted)
        {
            // フェードアウトが完了したらゲームモードを変更
            if (fader.IsFadeOutComplete())
            {
                GameSystem::get_instance().ChangeMode(GameMode::LOADING, true);
                UnlockClick(); // ← シーン切り替え完了時にロック解除
            }
        }
    }

    void Draw(void) override
    {
        if (m_titleAnim)
            m_titleAnim->Draw();

        // ScreenFader の描画は UIManager 側で一括管理されるのでここでは不要

		if (m_btnNewGame)
			m_btnNewGame->Draw();

		if (m_btnExitGame)
			m_btnExitGame->Draw();
    }

    void RequestStartFadeOut(void)
    {
        auto& fader = UIManager::get_instance().GetFader();
        m_fadeOutStarted = true;
        fader.StartFadeOut(1.0f); // 1秒でフェードアウト開始
    }

    static void OnClick_NewGame(void* userData)
    {
        UITitleScene* scene = static_cast<UITitleScene*>(userData);

        if (scene->IsClickLocked()) return; // 二重クリック防止
        scene->RequestStartFadeOut();
        scene->LockClick(); // ロック！
    }

	static void OnClick_LoadGame(void* userData)
	{
        UITitleScene* scene = static_cast<UITitleScene*>(userData);

        if (scene->IsClickLocked()) return; // 二重クリック防止
		GameSystem::get_instance().SetLoadGame(true); // ロードフラグを立てる
		scene->RequestStartFadeOut();
        scene->LockClick(); // ロック！
	}

    static void OnClickExitGame(void* userData)
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

	// クリックロックの管理
    void LockClick(void) { m_clickLocked = true; }
    void UnlockClick(void) { m_clickLocked = false; }
    bool IsClickLocked(void) const { return m_clickLocked; }

private:

    UIAnimatedSprite* m_titleAnim = nullptr; // タイトルのアニメ
    bool m_clickLocked = false; // クリックロック（多重起動防止）
    bool m_fadeOutStarted = false; // フェードアウト開始フラグ
	UIButton* m_btnNewGame = nullptr; // 新規ゲームボタン
	UIButton* m_btnExitGame = nullptr; // ゲーム終了ボタン

    Timer& m_timer = Timer::get_instance();
    InputManager& m_inputManager = InputManager::get_instance();
    UIManager& m_UIManager = UIManager::get_instance();
};
