#pragma once
//=============================================================================
//
// 画面フェード演出管理ちゃん [ScreenFader.h]
// Author : 
//  黒幕（オーバーレイ）による滑らかな画面遷移を演出する、
//  ぷにぷに可愛いフェード管理クラスなのです～っ！
//
// 機能概要：
//   - 画面を黒く覆う「フェードアウト」
//   - 黒幕が消えていく「フェードイン」
//   - アルファ値（透明度）の時間補間
//   - 完了チェック (IsFadeOutComplete, IsFullyHiddenなど)
// 
//=============================================================================
#include "Core/Timer.h"
#include "UI/Base/UIOverlayRenderer.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
#define FADE_DURATION_STD           1.0f

// フェード状態の列挙型
enum class FadeState
{
    None,       // 何もしない状態
    FadeIn,     // 黒→透明へのフェードイン
    FadeOut,    // 透明→黒へのフェードアウト
    Done        // 完了状態
};

class ScreenFader
{
public:

    ScreenFader() = default;
    ~ScreenFader() = default;

    void Initialize(void);

    // フェードアウトを開始（透明→黒）
    void StartFadeOut(float duration = FADE_DURATION_STD);

    // フェードインを開始（黒→透明）
    void StartFadeIn(float duration = FADE_DURATION_STD);

    // フェード状態を更新
    void Update(void);

    // フェード描画
    void Draw(void);

    // 現在の状態を取得
    bool IsFading(void) const { return m_active; }
    bool IsFadeOutComplete(void) const { return m_state == FadeState::Done; }
    float GetAlpha(void) const { return m_fadeAlpha; }

    // 画面が完全に明るくなったか（＝完全に見えてる）
    bool ScreenFader::IsFullyVisible() const { return m_fadeAlpha <= 0.0f; }
    // 画面が完全に暗くなったか（＝黒く隠れている）
    bool ScreenFader::IsFullyHidden() const { return m_fadeAlpha >= 1.0f; }

private:
    FadeState m_state = FadeState::None; // 現在のフェード状態
    float m_fadeAlpha = 0.0f;                // 現在のアルファ値（0.0～1.0）
    float m_duration = 1.0f;             // フェードにかける時間（秒）
    float m_elapsed = 0.0f;              // 経過時間
    bool m_active = false;               // 現在フェード中かどうか

    UIOverlayRenderer m_overlay;
    Timer& m_timer = Timer::get_instance();
    Renderer& m_renderer = Renderer::get_instance();
};