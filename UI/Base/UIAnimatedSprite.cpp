//=============================================================================
//
//  [UIAnimatedSprite.cpp]
// Author : 
//
//=============================================================================
#include "UI/Base/UIAnimatedSprite.h"

void UIAnimatedSprite::Init(ID3D11ShaderResourceView* tex, ID3D11Buffer* sharedVB,
    int columns, int rows, float frameTime, bool loop)
{
    m_sprite = new UISprite(sharedVB, tex);
    m_sprite->SetDrawMode(SpriteDrawMode::CenterWithColor); // 中心を基準に色を指定して描画する
    //m_texture = tex;
    //m_vertexBuffer = vb;
    m_columns = columns;
    m_rows = rows;
    m_frameTime = frameTime;
    m_loop = loop;

    m_startFrame = 0;
    m_endFrame = columns * rows;
    m_currentFrame = m_startFrame;
    m_finished = false;
}

void UIAnimatedSprite::PlayFrames(int start, int end)
{
    m_startFrame = start;
    m_endFrame = end;
    m_currentFrame = start;
    m_finished = false;
}

void UIAnimatedSprite::SetOnFinish(void (*callback)())
{
    m_onFinish = callback;
}

void UIAnimatedSprite::Play(void)
{
    m_finished = false;
    m_currentTime = 0.0f;

    // フレームが終わってたら最初に戻す
    if (m_currentFrame >= m_endFrame || m_currentFrame < m_startFrame)
        m_currentFrame = m_startFrame;
}

void UIAnimatedSprite::Stop(void)
{
    m_currentFrame = m_startFrame;
    m_finished = true;
}

void UIAnimatedSprite::Update(void)
{
    if (m_finished || m_paused || (!m_loop && m_currentFrame >= m_endFrame))
        return;

    m_currentTime += m_timer.GetDeltaTime() * m_playbackSpeed;

    if (m_currentTime >= m_frameTime)
    {
        m_currentTime -= m_frameTime;
        m_currentFrame++;

        if (m_currentFrame >= m_endFrame)
        {
            if (m_loop)
                m_currentFrame = m_startFrame;
            else
            {
                m_currentFrame = m_endFrame;
                m_finished = true;
                if (m_onFinish)
                    m_onFinish();
            }
        }
    }
}

void UIAnimatedSprite::Draw()
{
    //if (!m_texture || !m_vertexBuffer) return;

    int totalFrames = m_columns * m_rows;
    if (m_currentFrame >= totalFrames) return;

    // UV計算
    float uvW = 1.0f / static_cast<float>(m_columns);
    float uvH = 1.0f / static_cast<float>(m_rows);

    int col = m_currentFrame % m_columns;
    int row = m_currentFrame / m_columns;

    float u = col * uvW;
    float v = row * uvH;

    // スプライトを描画
	m_sprite->SetUV(u, v, uvW, uvH);
    m_sprite->Draw();
}

void UIAnimatedSprite::SetFrameDirectly(float ratio)
{
    if (m_loop || m_finished) return; // ループ中や再生終了なら無視

    int totalFrames = m_endFrame - m_startFrame;
    m_currentFrame = m_startFrame + static_cast<int>(totalFrames * ratio);
    if (m_currentFrame >= m_endFrame)
        m_currentFrame = m_endFrame - 1;
}

//追加
bool UIAnimatedSprite::IsAnimationFinished() const
{
    return m_finished;
}