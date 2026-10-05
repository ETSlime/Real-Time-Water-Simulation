#pragma once
//=============================================================================
//
//  [UILoadingScene.h]
// Author : 
//
//=============================================================================
#include "UI/Base/UIManager.h"
#include "UI/Base/UIAnimatedSprite.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define LOADING_ANIM_TEXTURE_PATH         "data/TEXTURE/LoadingAnimation.png"
#define LOADING_ANIM_THRESHOLD            0.5f

// 内部サブ状態マネージャー
enum class LoadingState
{
    FadeInBlack,          // 最初に黒くフェードイン（外部でやってるなら skip）
    WaitForResources,     // リソース読み込み待ち（短時間ならUI 表示しない）
    FadeOutBlackToShowUI, // 黒からフェードアウトしてアニメUI表示へ
    ShowLoadingUI,        // 読み込みアニメUIを表示中
    FadeInBeforeFinish,   // 読み込み完了後、再度黒にフェードイン
    Done                  // 完了！
};

class UILoadingScene : public UIElement
{
public:
    UILoadingScene(void) {}
    ~UILoadingScene(void) { SAFE_DELETE(m_loadingAnim); }

    // 初期化（アニメーションテクスチャとVB）
    // すでにフェードイン済みなら skipFadeIn を true にする
    void Init(ID3D11ShaderResourceView* tex, bool skipFadeIn = false);
    void Update(void) override;
    void Draw(void) override;

    bool IsFinished(void) const { return m_state == LoadingState::Done; }

    // GameSystem から「読み込み完了」通知
    void NotifyResourceLoadComplete(void) { m_resourceLoaded = true; }

    // 進行度を外部から設定（0.0 ～ 1.0）
    void SetProgress(float progress) { m_loadProgress = progress; }

private:

    float m_elapsedTime = 0.0f;
    bool m_showLoadingUI = false;
    bool m_resourceLoaded = false; // 外部から通知を受けたかどうか
    LoadingState m_state = LoadingState::FadeInBlack;
    const float m_threshold = LOADING_ANIM_THRESHOLD; // これ以上でアニメUI表示

    //変更
    float m_loadProgress = 0.0f; // 現在の読み込み進行 (0.0f～1.0f)
    float m_animProgress = 0.0f; // アニメの内部進行（0～1）

    // アニメーション用スプライト
    UIAnimatedSprite* m_loadingAnim = nullptr;

    Timer& m_timer = Timer::get_instance();
    UIManager& m_UIManager = UIManager::get_instance();
    ScreenFader& m_screenFader = m_UIManager.GetFader();
};