//=============================================================================
//
// [UIButton.cpp]
// Author : 
//
//=============================================================================
#include "UI/Base/UIButton.h"
#include "Core/AudioManager.h"

UIButton::UIButton(void) : m_name(nullptr), m_currentTex(nullptr), m_currentTint({ 1,1,1,1 }), m_isEnabled(true),
m_state(ButtonState::Normal), m_pressedInside(false), m_prevMouseDown(false), m_useTintMode(false) 
{
    // tint 配列を初期化（デフォルト値は白）
    for (auto& c : m_tint) c = { 1,1,1,1 };
    m_sprite = new UISprite();
}

UIButton::UIButton(XMFLOAT2 pos, XMFLOAT2 size, const char* name) : UIButton()
{
    m_name = const_cast<char*>(name);
    SetPosition(pos);
	SetSize(size);
}

UIButton::~UIButton()
{
}

void UIButton::InitWithTextures(ID3D11Buffer* vb, char* normalTex, char* hoverTex, char* pressedTex, char* disabledTex)
{
    m_useTintMode = false;

    m_tex[0] = m_textureMgr.CreateTexture(normalTex);
    m_tex[1] = hoverTex ? m_textureMgr.CreateTexture(hoverTex) : m_tex[0];
    m_tex[2] = pressedTex ? m_textureMgr.CreateTexture(pressedTex) : m_tex[0];
    m_tex[3] = disabledTex ? m_textureMgr.CreateTexture(disabledTex) : m_tex[0];

    // スプライトの初期化
    m_sprite->SetVertexBuffer(vb);
    m_sprite->SetTexture(m_tex[0]);
	m_sprite->SetDrawMode(SpriteDrawMode::CenterWithColor); // 中心を基準に色を指定して描画する

    UpdateVisual();
}

void UIButton::InitWithSingleTexture(ID3D11Buffer* vb, char* texture, XMFLOAT4& tint_normal, XMFLOAT4& tint_hover, XMFLOAT4& tint_pressed, XMFLOAT4& tint_disabled)
{
    m_useTintMode = true;

    m_tex[0] = m_textureMgr.CreateTexture(texture);
    m_tex[1] = m_tex[2] = m_tex[3] = m_tex[0]; // すべて同じテクスチャ

    m_tint[0] = tint_normal;
    m_tint[1] = tint_hover;
    m_tint[2] = tint_pressed;
    m_tint[3] = tint_disabled;

    // スプライトの初期化
    m_sprite->SetVertexBuffer(vb);
    m_sprite->SetTexture(m_tex[0]);
    m_sprite->SetDrawMode(SpriteDrawMode::CenterWithColor); // 中心を基準に色を指定して描画する

    UpdateVisual();
}

void UIButton::Update(void)
{
	HandleMouseInput();
}

void UIButton::Draw(void)
{
    if (!m_currentTex) return;

	m_sprite->SetTexture(m_currentTex);
	m_sprite->SetColor(m_currentTint);
	m_sprite->Draw();
}

void UIButton::HandleMouseInput(void)
{
	int mouseX = GetMousePosX(); // マウスのX座標
	int mouseY = GetMousePosY(); // マウスのY座標
    bool isPressed = m_inputManager.IsMouseLeftPressed();

    if (!m_isEnabled)
    {
        m_state = ButtonState::Disabled;
        UpdateVisual();
        return;
    }

    const bool inside = IsCursorInside(mouseX, mouseY);
    if (!m_prevHovering && inside)
    {
        AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_ui_hover);
    }
	m_prevHovering = inside;

    // 押下開始
    if (isPressed && !m_prevMouseDown && inside)
    {
        m_state = ButtonState::Pressed;
        m_pressedInside = true;
        UpdateVisual();
    }
    // 押下中に外へ出た
    else if (isPressed && m_state == ButtonState::Pressed && !inside)
    {
        m_state = ButtonState::Normal;
        m_pressedInside = false;
        UpdateVisual();
    }
    // ボタン離す
    else if (!isPressed && m_prevMouseDown)
    {
        // 押したまま中にいて離した → クリック成功
        if (m_pressedInside && inside && m_onClick)
        {
            // 効果音
			AudioManager::get_instance().PlaySound(SoundLabel::SOUND_LABEL_SE_ui_select);
            // コールバック
            m_onClick(m_userData);
        }
        m_pressedInside = false;

        // ホバー状態に戻るか通常に戻るか
        m_state = inside ? ButtonState::Hovered : ButtonState::Normal;
        UpdateVisual();
    }
    // ホバー検知
    else if (!isPressed)
    {
        const ButtonState next = inside ? ButtonState::Hovered : ButtonState::Normal;
        if (next != m_state)
        {
            m_state = next;
            UpdateVisual();

            if (m_useTweenScale)
            {
                // ホバー時にTweenで拡大、離れたら元に戻す
                const XMFLOAT2& scale = m_sprite->GetBaseScale();
                const XMFLOAT2& normalScale = { 1.0f * scale.x, 1.0f * scale.y};
                const XMFLOAT2& hoverScale = { 1.3f * scale.x, 1.3f * scale.y}; // 拡大倍率は調整可

                if (m_state == ButtonState::Hovered)
                {
                    m_tweenManager.AddScale(this, hoverScale, 0.2f, EaseType::OutCubic);
                }
                else if (m_state == ButtonState::Normal)
                {
                    m_tweenManager.AddScale(this, normalScale, 0.2f, EaseType::OutCubic);
                }
            }

        }
    }

    m_prevMouseDown = isPressed;
}

void UIButton::SetOnClick(UIButtonCallback callback, void* userData)
{
    m_onClick = callback;
    m_userData = userData;
}

void UIButton::SetEnabled(bool enabled)
{
    if (m_isEnabled == enabled) return;
    m_isEnabled = enabled;
    m_state = enabled ? ButtonState::Normal : ButtonState::Disabled;
    UpdateVisual();
}

void UIButton::SetUseTweenScale(bool useTween)
{
    m_useTweenScale = useTween;

	// Tweenを使用する場合は、スプライトのベーススケールを初期化
    m_sprite->InitBaseScale();
}

bool UIButton::IsCursorInside(int mouseX, int mouseY) const
{
    const auto& pos = GetPosition();
    XMFLOAT2 scaledSize = GetSize();
    scaledSize.x *= GetScale().x; // スケールを考慮
    scaledSize.y *= GetScale().y; // スケールを考慮

    // マージン（判定を広げるピクセル数）
    const float margin = 5.0f;

    // マウス座標がボタンの範囲内にあるかチェック
	// ボタンの中心位置からの相対座標を計算
    const float left = pos.x - scaledSize.x * 0.5f - margin;
    const float right = pos.x + scaledSize.x * 0.5f + margin;
    const float top = pos.y - scaledSize.y * 0.5f - margin;
    const float bottom = pos.y + scaledSize.y * 0.5f + margin;

    return (mouseX >= left && mouseX <= right &&
        mouseY >= top && mouseY <= bottom);
}

void UIButton::UpdateVisual(void)
{
    const int idx = static_cast<int>(m_state);
    m_currentTex = m_tex[idx];
    m_currentTint = m_useTintMode ? m_tint[idx] : XMFLOAT4(1, 1, 1, 1);
}

