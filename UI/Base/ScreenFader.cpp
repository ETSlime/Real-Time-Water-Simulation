//=============================================================================
//
// 画面フェード演出管理ちゃん [ScreenFader.cpp]
// Author : 
//  黒幕（オーバーレイ）による滑らかな画面遷移を演出する、
//  ぷにぷに可愛いフェード管理クラスなのです～っ！
// 
//=============================================================================
#include "UI/Base/ScreenFader.h"
#include "UI/Base/UIManager.h"

void ScreenFader::Initialize(void)
{
    m_overlay.Initialize(UIManager::get_instance().GetSpriteVertexBuffer());
}

void ScreenFader::StartFadeOut(float duration)
{
    m_state = FadeState::FadeOut;
    m_duration = duration;
    m_elapsed = 0.0f;
    m_fadeAlpha = 0.0f;
    m_active = true;
}

void ScreenFader::StartFadeIn(float duration)
{
    m_state = FadeState::FadeIn;
    m_duration = duration;
    m_elapsed = 0.0f;
    m_fadeAlpha = 1.0f;
    m_active = true;
}

void ScreenFader::Update(void)
{
    if (!m_active) return;

    m_elapsed += m_timer.GetDeltaTime();
    float t = m_elapsed / m_duration;

    switch (m_state)
    {
    case FadeState::FadeOut:
        m_fadeAlpha = t;
        if (t >= 1.0f)
        {
            m_fadeAlpha = 1.0f;
            m_state = FadeState::Done;
            m_active = false;
        }
        break;

    case FadeState::FadeIn:
        m_fadeAlpha = 1.0f - t;
        if (t >= 1.0f)
        {
            m_fadeAlpha = 0.0f;
            m_state = FadeState::Done;
            m_active = false;
        }
        break;

    default:
        break;
    }
}

void ScreenFader::Draw(void)
{
    if (m_fadeAlpha <= 0.0f) return; // 不要な描画はスキップ

    m_overlay.Draw(m_renderer.GetDeviceContext(),
        static_cast<float>(SCREEN_CENTER_X),
        static_cast<float>(SCREEN_CENTER_Y),
        static_cast<float>(SCREEN_WIDTH),
        static_cast<float>(SCREEN_HEIGHT),
        { 0, 0, 0, m_fadeAlpha });
}
