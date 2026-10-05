#pragma once
//=============================================================================
//
// [UIButton.h]
// Author : 
//
//=============================================================================
#include "UI/Base/UIElement.h"
#include "UI/Base/UISprite.h"
#include "UI/Base/TweenManager.h"
#include "Core/TextureMgr.h"
#include "Utility/InputManager.h"

//*****************************************************************************
// マクロ定義
//*****************************************************************************
enum class ButtonState
{
    Normal,     // 通常
    Hovered,    // マウス乗せ
    Pressed,    // 押下
    Disabled    // 無効
};

// コールバック用ネイティブ関数ポインタ
using UIButtonCallback = void (*)(void* userData);

class UIButton : public UIElement, public ISpriteUI
{
public:
	UIButton(void);
    UIButton(XMFLOAT2 pos, XMFLOAT2 size, const char* name = nullptr);
    ~UIButton(void) override;

    // 複数テクスチャ方式（Normal / Hover / Pressed / Disabled）
    void InitWithTextures(ID3D11Buffer* vb, 
         char* normalTex,
         char* hoverTex = nullptr,
         char* pressedTex = nullptr,
         char* disabledTex = nullptr);


    // 1枚テクスチャ＋色変更方式
    // ※ tint_{state} で色を自由に設定可能
    void InitWithSingleTexture(ID3D11Buffer* vb,
        char* texture,
        XMFLOAT4& tint_normal = XMFLOAT4(1,1,1,1),
        XMFLOAT4& tint_hover = XMFLOAT4(0.15f, 0.15f, 0.15f,1),
        XMFLOAT4& tint_pressed = XMFLOAT4(0.8f,0.8f,0.8f,1),
        XMFLOAT4& tint_disabled = XMFLOAT4(0.5f,0.5f,0.5f,1));

    // 更新・描画
    void Update(void) override;
    void Draw(void) override;

    // マウス入力 (isPressed = 左ボタンが押されているか)
    void HandleMouseInput(void);

    // コールバック設定
    void SetOnClick(UIButtonCallback callback, void* userData = nullptr);

    // 効果音設定（AudioManager::PlaySE に渡す名前）
    void SetClickSE(const char* seName) { m_clickSE = seName; }

    // 有効/無効
    void SetEnabled(bool enabled);
    bool IsEnabled(void) const { return m_isEnabled; }

    void SetUseTweenScale(bool useTween);

	// ISpriteUI インターフェースの実装
    DELEGATE_SPRITE_ALL_INTERFACE(m_sprite);

private:
    //----------------------------------------------------------------------
    // 内部ヘルパー
    //----------------------------------------------------------------------
    bool IsCursorInside(int mouseX, int mouseY) const;
    void UpdateVisual(void);   // 状態に応じたテクスチャ/色を更新

	char* m_name = nullptr; // ボタン名（デバッグ用）

	UISprite* m_sprite = nullptr; // ボタンのスプライト（描画用）

	//--- 状態管理 ---
    ButtonState m_state = ButtonState::Normal;
    bool        m_isEnabled = true;
    bool        m_prevMouseDown = false;
    bool        m_pressedInside = false;
	bool        m_prevHovering = false;

    //--- 描画 ---
    bool m_useTintMode = false; // true=色変更方式
	bool m_useTweenScale = false; // true=Tweenで拡大縮小

    ID3D11ShaderResourceView* m_tex[4] = { nullptr }; // 0:Normal 1:Hover 2:Pressed 3:Disabled
    XMFLOAT4 m_tint[4]; // 各状態の色
    ID3D11ShaderResourceView* m_currentTex = nullptr;
    XMFLOAT4 m_currentTint = { 1,1,1,1 };

    //--- コールバック & 効果音 ---
    UIButtonCallback  m_onClick = nullptr;
    void* m_userData = nullptr;
    const char* m_clickSE = nullptr;

	TextureMgr& m_textureMgr = TextureMgr::get_instance();
	Renderer& m_renderer = Renderer::get_instance();
	InputManager& m_inputManager = InputManager::get_instance();
    ShaderResourceBinder& m_resourceBinder = ShaderResourceBinder::get_instance();
	TweenManager& m_tweenManager = TweenManager::get_instance();
};