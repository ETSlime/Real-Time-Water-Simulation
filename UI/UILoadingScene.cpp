//=============================================================================
//
//  [UILoadingScene.cpp]
// Author : 
//
//=============================================================================
#include "UI/UILoadingScene.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define LOADING_ANIM_TILE_X       1
#define LOADING_ANIM_TILE_Y       1
#define LOADING_ANIM_SPD          1.5f
#define LOADING_SMOOTH_DAMP       0.2f // 緩やかな（未完成時）
#define LOADING_FAST_DAMP         0.05f  // 急激な（完成時）
constexpr float ANIM_PROGRESS_EPSILON = 0.09f;

void UILoadingScene::Init(ID3D11ShaderResourceView* tex, bool skipFadeIn)
{
    m_state = skipFadeIn ? LoadingState::WaitForResources : LoadingState::FadeInBlack;

    // ローディングアニメーション用スプライト
    m_loadingAnim = new UIAnimatedSprite();
    m_loadingAnim->Init(tex,
        m_UIManager.GetSpriteVertexBuffer(),
        LOADING_ANIM_TILE_X,
        LOADING_ANIM_TILE_Y,
        LOADING_ANIM_SPD,
        false
    );

    //m_loadingAnim->SetPlaybackSpeed(0.6f);

    // 表示位置とサイズを設定
    m_loadingAnim->SetPosition({ static_cast<float>(SCREEN_CENTER_X), static_cast<float>(SCREEN_CENTER_Y) });
    m_loadingAnim->SetSize({ static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) });
    m_loadingAnim->Play();

    if (!skipFadeIn)
        m_screenFader.StartFadeIn();
}

void UILoadingScene::Update(void)
{
    m_elapsedTime += m_timer.GetDeltaTime();

    // 読み込み完了前までは進行度に合わせて滑らかにアニメバー更新
    if (!m_resourceLoaded)
    {
        float alpha = 1.0f - powf(LOADING_SMOOTH_DAMP, m_timer.GetDeltaTime());
        if (m_loadProgress != 0)
            m_animProgress += (m_loadProgress - m_animProgress) * alpha;
    }
    else
    {
        // 読み込み完了後は一気にゴールへ進める
        float alpha = 1.0f - powf(LOADING_FAST_DAMP, m_timer.GetDeltaTime());
        m_animProgress += (1.0f - m_animProgress) * alpha;

        if ((1.0f - m_animProgress) <= ANIM_PROGRESS_EPSILON)
            m_animProgress = 1.0f;
    }

    // アニメーション更新
    if (m_loadingAnim)
    {
        m_loadingAnim->SetFrameDirectly(m_animProgress);
    }

    m_screenFader.Update();

    // 状態遷移の処理
    switch (m_state)
    {
        // 最初のフェードイン（黒画面へ）
    case LoadingState::FadeInBlack:
        if (m_screenFader.IsFullyHidden())
            m_state = LoadingState::WaitForResources;
        break;

        // リソース読み込み待ち（軽いならアニメなし）
    case LoadingState::WaitForResources:
        if (m_elapsedTime >= m_threshold && !m_showLoadingUI)
        {
            // 一定時間以上かかっているのでアニメを表示する
            m_showLoadingUI = true;
            m_screenFader.StartFadeIn();
            m_state = LoadingState::FadeOutBlackToShowUI;
        }
        if (m_resourceLoaded && !m_showLoadingUI)
        {
            // 読み込み完了、アニメ不要ならそのままゲームへ
            m_screenFader.StartFadeOut();
            m_state = LoadingState::Done;
        }
        break;

        // 黒フェードアウト → アニメ表示
    case LoadingState::FadeOutBlackToShowUI:
        if (m_screenFader.IsFullyVisible())
            m_state = LoadingState::ShowLoadingUI;
        break;

        // アニメ表示中（読み込み完了を待つ）
    case LoadingState::ShowLoadingUI:
        if (m_resourceLoaded && (m_animProgress >= 1.0f))
        {
            m_screenFader.StartFadeOut();
            m_state = LoadingState::FadeInBeforeFinish;
        }
        break;

        // フェードイン（アニメ終了→画面を黒く戻す）
    case LoadingState::FadeInBeforeFinish:
        if (m_screenFader.IsFullyHidden())
            m_state = LoadingState::Done;
        break;

    default:
        break;
    }
}

void UILoadingScene::Draw(void)
{
    if (m_state != LoadingState::Done)
    {
        if (m_loadingAnim)
            m_loadingAnim->Draw();
    }
}